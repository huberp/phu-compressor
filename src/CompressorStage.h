#pragma once

#include <algorithm>
#include <cmath>
#include <juce_dsp/juce_dsp.h>

enum class StageDirection { Downward, Upward };

/**
 * CompressorStage — one independent compressor stage (downward or upward).
 *
 * Per-sample pipeline:
 *   1. Compute targetGainDb from the RAW (unsmoothed) detector level:
 *        Downward: targetGainDb = -max(envDb - thresh, 0) * (1 - 1/ratio)   ≤ 0
 *        Upward:   targetGainDb = +max(thresh - envDb, 0) * (1 - 1/ratio)   ≥ 0
 *
 *   2. Smooth gainEnv toward targetGainDb with one-pole ballistics:
 *        Attack  = gain magnitude INCREASING (compressor/boost engaging)
 *        Release = gain magnitude DECREASING (compressor/boost releasing toward 0)
 *
 * Why this is correct:
 *   Target for the upward stage is INSTANTANEOUSLY 0 whenever the raw level is
 *   above threshold. The one-pole smoother can only move toward its target —
 *   it never overshoots. Therefore upward boost is IMPOSSIBLE on above-threshold
 *   content in steady state, regardless of release time.
 *
 *   Contrast with level-smoothing: smoothing the level first means the smoothed
 *   level stays below threshold for [release-time] after a transient arrives,
 *   producing a boost tail on the transient — a fundamental design flaw.
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
        // gainEnv starts at 0 (no compression, no boost) for both stages.
        // This is correct: targetGainDb is also 0 when signal starts at 0,
        // so there is no initial error to smooth away.
        for (int ch = 0; ch < kMaxChannels; ++ch)
            gainEnv[ch] = SampleType(0);
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

    void setSnapReleaseMs(SampleType ms) {
        snapReleaseMs    = ms;
        snapReleaseCoeff = msToCoeff(snapReleaseMs);
    }

    void setSnapReleaseEnabled(bool enabled) {
        snapReleaseEnabled = enabled;
    }

    // ── Per-sample processing ────────────────────────────────────────────

    struct Result {
        SampleType gainDb;
        SampleType gate;  // abs(gainDb) for UI/metrics
    };

    Result processSample(int channel, SampleType envDb) {
        // Step 1: instantaneous target gain from the raw detector level.
        // Target is IMMEDIATELY 0 when the signal is in the "wrong" zone —
        // above threshold for Downward, or below threshold for Upward.
        // This is the key property that prevents above-threshold boosting.
        SampleType targetGainDb;
        if (direction == StageDirection::Downward) {
            const SampleType excess = std::max(envDb - threshDb, SampleType(0));
            targetGainDb = -excess * (SampleType(1) - SampleType(1) / ratio); // ≤ 0
        } else {
            // Clamp env to the noise floor before computing deficit.
            // Below kUpwardFloorDb the signal is effectively noise — boosting it
            // further would amplify the noise floor rather than musical content,
            // and would cause the max boost to become independent of threshold.
            const SampleType clampedEnv = std::max(envDb, kUpwardFloorDb);
            const SampleType deficit = std::max(threshDb - clampedEnv, SampleType(0));
            targetGainDb = deficit * (SampleType(1) - SampleType(1) / ratio); // ≥ 0
        }

        // Step 2: one-pole ballistics on gainEnv toward targetGainDb.
        //   Attack  = gain magnitude increasing (effect engaging)
        //   Release = gain magnitude decreasing (effect releasing toward 0)
        //
        // For Downward: magnitude grows when gain goes more negative → target < gainEnv
        // For Upward:   magnitude grows when gain goes more positive → target > gainEnv
        SampleType& gainEnvelope = gainEnv[channel];
        const bool attacking = (direction == StageDirection::Downward)
                                   ? (targetGainDb < gainEnvelope)
                                   : (targetGainDb > gainEnvelope);

        SampleType coeff;
        if (!attacking) {
            // Releasing — choose snap or normal
            if (direction == StageDirection::Upward
                && snapReleaseEnabled
                && (gainEnvelope - targetGainDb) > kSnapReleaseThresholdDb)
            {
                coeff = snapReleaseCoeff;
            } else {
                coeff = releaseCoeff;
            }
        } else {
            coeff = attackCoeff;
        }
        gainEnvelope = targetGainDb + coeff * (gainEnvelope - targetGainDb);

        return { gainEnvelope, std::abs(gainEnvelope) };
    }

  private:
    SampleType msToCoeff(SampleType timeMs) const {
        return (timeMs < SampleType(0.001))
                   ? SampleType(0)
                   : static_cast<SampleType>(std::exp(expFactor / timeMs));
    }

    void updateCoefficients() {
        attackCoeff      = msToCoeff(attackMs);
        releaseCoeff     = msToCoeff(releaseMs);
        snapReleaseCoeff = msToCoeff(snapReleaseMs);
    }

    // ── Constants ────────────────────────────────────────────────────────

    // Noise floor for the upward stage: signals below this are treated as silence.
    // Prevents boosting quantization/circuit noise and keeps max boost proportional
    // to threshold, making the display and audio behaviour predictable.
    static constexpr SampleType kUpwardFloorDb = SampleType(-80);

    // Minimum gain-envelope drop (dB) required to trigger snap release.
    // Detects a silence-to-transient event: the upward boost built up during silence
    // collapses instantly when the target drops by more than this amount.
    static constexpr SampleType kSnapReleaseThresholdDb = SampleType(6.0);

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
    SampleType snapReleaseMs    { SampleType(5) };
    SampleType snapReleaseCoeff { SampleType(0) };
    bool       snapReleaseEnabled { false };

    SampleType gainEnv[kMaxChannels] = {};
};
