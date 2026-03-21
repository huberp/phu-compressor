#pragma once

#include <algorithm>
#include <cmath>
#include <juce_dsp/juce_dsp.h>

enum class StageDirection { Downward, Upward };

/**
 * CompressorStage — one independent compressor stage (downward or upward).
 *
 * Per-sample pipeline:
 *   1. Trigger:   binary 0/1 based on whether envDb crosses the threshold
 *   2. Gate:      one-pole ballistics smoother on the trigger (attack/release)
 *   3. Gain:      smoothedGate * level-proportional dB value
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
        for (int ch = 0; ch < kMaxChannels; ++ch)
            smoothedGate[ch] = SampleType(0);
    }

    // ── Configuration ────────────────────────────────────────────────────

    void setDirection(StageDirection d) { direction = d; }

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
        SampleType gate;  // smoothed gate value [0..1] for UI/metrics
    };

    Result processSample(int channel, SampleType envDb) {
        // Stage 1: binary trigger
        const SampleType trigger = computeTrigger(envDb);

        // Stage 2: gate ballistics (one-pole smoother)
        SampleType& gate = smoothedGate[channel];
        const SampleType coeff = (trigger > gate) ? attackCoeff : releaseCoeff;
        gate = trigger + coeff * (gate - trigger);

        // Stage 3: gain computation (gate modulates level-proportional dB)
        SampleType gainDb;
        if (direction == StageDirection::Downward) {
            // Reduce gain when above threshold
            const SampleType excess = envDb - threshDb;
            gainDb = -gate * std::max(excess, SampleType(0))
                     * (SampleType(1) - SampleType(1) / ratio);
        } else {
            // Boost gain when below threshold
            const SampleType deficit = threshDb - envDb;
            gainDb = gate * std::max(deficit, SampleType(0))
                     * (SampleType(1) - SampleType(1) / ratio);
        }

        return { gainDb, gate };
    }

  private:
    SampleType computeTrigger(SampleType envDb) const {
        if (direction == StageDirection::Downward)
            return (envDb > threshDb) ? SampleType(1) : SampleType(0);
        else
            return (envDb < threshDb) ? SampleType(1) : SampleType(0);
    }

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

    SampleType smoothedGate[kMaxChannels] = {};
};
