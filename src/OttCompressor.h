#pragma once

#include <cmath>
#include <juce_dsp/juce_dsp.h>

/**
 * OttCompressor — single-band OTT-style (upward + downward) compressor.
 *
 * Design: binary trigger → BallisticsFilter smoothing → gain application
 *
 *   level = |input|
 *
 *   downTrig = (level > downThreshLin) ? 1 : 0
 *   downEnv  = downBallistics.processSample(ch, downTrig)   // 0..1
 *
 *   upTrig   = (level < upThreshLin) ? 1 : 0
 *   upEnv    = upBallistics.processSample(ch, upTrig)       // 0..1
 *
 *   downGain = lerp(1, f_down(level), downEnv)              // <= 1
 *   upGain   = lerp(1, f_up(level),   upEnv)               // >= 1
 *
 *   output = input * downGain * upGain
 *
 * This class is intentionally free of plugin/UI dependencies (only <cmath>
 * and juce_dsp are used) so it can be copied into phu-splitter with minimal
 * edits.  See README for the reuse plan.
 */
template <typename SampleType>
class OttCompressor {
  public:
    OttCompressor() = default;

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    void prepare(const juce::dsp::ProcessSpec& spec) {
        sampleRate = static_cast<SampleType>(spec.sampleRate);
        downBallistics.prepare(spec);
        upBallistics.prepare(spec);
        applyAttackRelease();
        reset();
    }

    void reset() {
        downBallistics.reset();
        upBallistics.reset();
    }

    // -------------------------------------------------------------------------
    // Parameter setters
    // -------------------------------------------------------------------------

    void setDownThresholdDb(SampleType dB) {
        downThreshLin = dbToLinear(dB);
    }

    void setDownRatio(SampleType ratio) {
        downRatio = (ratio > SampleType(1)) ? ratio : SampleType(1);
    }

    void setUpThresholdDb(SampleType dB) {
        upThreshLin = dbToLinear(dB);
    }

    void setUpRatio(SampleType ratio) {
        upRatio = (ratio > SampleType(1)) ? ratio : SampleType(1);
    }

    void setAttackMs(SampleType ms) {
        attackMs = ms;
        applyAttackRelease();
    }

    void setReleaseMs(SampleType ms) {
        releaseMs = ms;
        applyAttackRelease();
    }

    // -------------------------------------------------------------------------
    // Per-sample processing (stereo-capable via channel index)
    // -------------------------------------------------------------------------

    SampleType processSample(int channel, SampleType input) {
        const SampleType level = std::abs(input);

        // --- Downward compression (reduce loud signals above downThreshLin) ---
        const SampleType downTrig = (level > downThreshLin) ? SampleType(1) : SampleType(0);
        const SampleType downEnv = downBallistics.processSample(channel, downTrig);

        // --- Upward compression (boost quiet signals below upThreshLin) ---
        const SampleType upTrig = (level < upThreshLin) ? SampleType(1) : SampleType(0);
        const SampleType upEnv = upBallistics.processSample(channel, upTrig);

        // --- Compute "full effect" target gains ---
        const SampleType downTargetGain = computeDownGain(level);
        const SampleType upTargetGain = computeUpGain(level);

        // --- Blend between unity and target using smoothed envelope ---
        const SampleType downGain = lerp(SampleType(1), downTargetGain, downEnv);
        const SampleType upGain = lerp(SampleType(1), upTargetGain, upEnv);

        return input * downGain * upGain;
    }

  private:
    // -------------------------------------------------------------------------
    // Gain computation helpers
    // -------------------------------------------------------------------------

    /** Downward compressor gain: reduces gain for signals above threshold.
     *  Uses ratio-based power-law mapping; returns a value in (0, 1]. */
    SampleType computeDownGain(SampleType level) const {
        if (level <= downThreshLin || downThreshLin <= SampleType(0))
            return SampleType(1);

        // gain = (threshold / level) ^ (1 - 1/ratio)
        const SampleType exponent = SampleType(1) - SampleType(1) / downRatio;
        return std::pow(downThreshLin / level, exponent);
    }

    /** Upward compressor gain: boosts gain for signals below threshold.
     *  Uses reciprocal ratio-based mapping; returns a value in [1, ∞). */
    SampleType computeUpGain(SampleType level) const {
        if (level >= upThreshLin || level <= SampleType(0))
            return SampleType(1);

        // gain = (threshold / level) ^ (1 - 1/ratio)
        const SampleType exponent = SampleType(1) - SampleType(1) / upRatio;
        return std::pow(upThreshLin / level, exponent);
    }

    // -------------------------------------------------------------------------
    // Utilities
    // -------------------------------------------------------------------------

    static SampleType dbToLinear(SampleType dB) {
        return std::pow(SampleType(10), dB / SampleType(20));
    }

    static SampleType lerp(SampleType a, SampleType b, SampleType t) {
        return a + t * (b - a);
    }

    void applyAttackRelease() {
        downBallistics.setAttackTime(attackMs);
        downBallistics.setReleaseTime(releaseMs);
        upBallistics.setAttackTime(attackMs);
        upBallistics.setReleaseTime(releaseMs);
    }

    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------

    SampleType sampleRate{SampleType(44100)};
    SampleType attackMs{SampleType(10)};
    SampleType releaseMs{SampleType(100)};

    SampleType downThreshLin{SampleType(1)}; // -0 dB default
    SampleType downRatio{SampleType(4)};

    SampleType upThreshLin{SampleType(0.1)}; // ~ -20 dB default
    SampleType upRatio{SampleType(4)};

    juce::dsp::BallisticsFilter<SampleType> downBallistics;
    juce::dsp::BallisticsFilter<SampleType> upBallistics;
};
