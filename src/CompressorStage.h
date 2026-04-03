#pragma once

#include <algorithm>
#include <cmath>
#include <juce_dsp/juce_dsp.h>

enum class StageDirection { Downward, Upward };

/**
 * CompressorStage — one independent compressor stage (downward or upward).
 *
 * Per-sample pipeline:
 *   1. Level ballistics: one-pole smoother applied to the incoming detector level.
 *                        For Downward: attack when level is rising, release when falling.
 *                        For Upward:   attack when level is falling (boost engaging),
 *                                      release when level is rising (boost disengaging).
 *                        This ensures Attack/Release always describe how fast the effect
 *                        turns on/off from the user's perspective regardless of direction.
 *   2. Gain computation: instantaneous gain derived from the smoothed level,
 *                        threshold, and ratio
 *
 * Smoothing the level (not the gain) is the standard compressor design:
 * gain is simply the static characteristic evaluated at the smoothed level,
 * so it tracks the envelope naturally without needing special-case logic.
 *
 * Two instances (one Downward, one Upward) are composed inside OttCompressor.
 */
template <typename SampleType>
class CompressorStage {
  public:
    static constexpr int kMaxChannels = 2;

    CompressorStage() = default;

    void prepare(const juce::dsp::ProcessSpec& spec) {
        sampleRate = static_cast<SampleType>(spec.sampleRate);
        expFactor = SampleType(-2.0) * SampleType(juce::MathConstants<double>::pi)
                    * SampleType(1000.0) / sampleRate;
        updateCoefficients();
        reset();
    }

    void reset() {
        // Downward: init low so the first real signal uses attack (rises correctly).
        // Upward:   init high (0 dB is above any realistic threshold) so the first
        //           real signal does NOT trigger an erroneous burst of gain boost.
        //           If gainEnv started at -200 for an upward stage, smoothed would
        //           take seconds to rise above threshold while applying massive gain.
        const SampleType initLevel = (direction == StageDirection::Downward)
                                         ? SampleType(-200)
                                         : SampleType(0);
        for (int ch = 0; ch < kMaxChannels; ++ch)
            gainEnv[ch] = initLevel;
    }

    // ── Configuration ────────────────────────────────────────────────────

    void setDirection(StageDirection d) { direction = d; reset(); }

    void setThresholdDb(SampleType dB) { threshDb = dB; }

    void setRatio(SampleType r) { ratio = std::max(r, SampleType(1)); }

    void setAttackMs(SampleType ms) {
        attackMs = ms;
        updateCoefficients();
    }

    void setReleaseMs(SampleType ms) {
        releaseMs = ms;
        updateCoefficients();
    }

    // ── Per-sample processing ────────────────────────────────────────────

    struct Result {
        SampleType gainDb;
        SampleType gate;  // abs(gainDb) for UI/metrics
    };

    Result processSample(int channel, SampleType envDb) {
        // Stage 1: one-pole ballistics on the detector level.
        //
        // For Downward compression the compressor ENGAGES when the level rises
        // above threshold, so attack tracks a rising level and release a falling one.
        //
        // For Upward compression the semantics are inverted: the boost ENGAGES
        // when the level FALLS below threshold, so attack must track a falling level
        // and release a rising one — otherwise "Attack" visually controls the wrong
        // side of the boost envelope.
        SampleType& smoothed = gainEnv[channel];
        const bool levelRising = (envDb > smoothed);
        const bool engaging    = (direction == StageDirection::Downward) ? levelRising : !levelRising;
        const SampleType coeff = engaging ? attackCoeff : releaseCoeff;
        smoothed = envDb + coeff * (smoothed - envDb);

        // Stage 2: instantaneous gain from smoothed level
        SampleType gainDb;
        if (direction == StageDirection::Downward) {
            const SampleType excess = std::max(smoothed - threshDb, SampleType(0));
            gainDb = -excess * (SampleType(1) - SampleType(1) / ratio);
        } else {
            const SampleType deficit = std::max(threshDb - smoothed, SampleType(0));
            gainDb = deficit * (SampleType(1) - SampleType(1) / ratio);
        }

        return { gainDb, std::abs(gainDb) };
    }

  private:
    SampleType msToCoeff(SampleType timeMs) const {
        return (timeMs < SampleType(0.001))
                   ? SampleType(0)
                   : static_cast<SampleType>(std::exp(expFactor / timeMs));
    }

    void updateCoefficients() {
        attackCoeff  = msToCoeff(attackMs);
        releaseCoeff = msToCoeff(releaseMs);
    }

    // ── State ────────────────────────────────────────────────────────────

    SampleType sampleRate { SampleType(44100) };
    SampleType expFactor  { SampleType(0) };

    StageDirection direction { StageDirection::Downward };
    SampleType threshDb      { SampleType(0) };
    SampleType ratio         { SampleType(4) };
    SampleType attackMs      { SampleType(10) };
    SampleType releaseMs     { SampleType(100) };

    SampleType attackCoeff   { SampleType(0) };
    SampleType releaseCoeff  { SampleType(0) };

    SampleType gainEnv[kMaxChannels] = {};
};
