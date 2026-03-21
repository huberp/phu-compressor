#pragma once

#include "CompressorStage.h"
#include "VolumeDetector.h"
#include <cmath>
#include <juce_dsp/juce_dsp.h>

/**
 * OttCompressor — single-band upward + downward compressor.
 *
 * Modular 3-component architecture:
 *   1. VolumeDetector  — selectable RMS or PeakMax, configurable window size.
 *                        Converts raw audio into a smooth dB level.
 *   2. CompressorStage — two independent instances (Downward + Upward).
 *                        Each uses: binary trigger → gate ballistics → gain computation.
 *   3. OttCompressor   — orchestrator: detector → stages → sum gains → apply.
 *
 * The two stages are fully independent — no cross-coupling between them.
 */
template <typename SampleType>
class OttCompressor {
  public:
    OttCompressor() {
        downStage.setDirection(StageDirection::Downward);
        upStage.setDirection(StageDirection::Upward);
    }

    void prepare(const juce::dsp::ProcessSpec& spec) {
        detector.prepare(spec);
        downStage.prepare(spec);
        upStage.prepare(spec);
        reset();
    }

    void reset() {
        detector.reset();
        downStage.reset();
        upStage.reset();
    }

    // ── Detector configuration ───────────────────────────────────────────

    void setDetectorMode(DetectorMode mode) { detector.setMode(mode); }
    void setDetectorWindowMs(SampleType ms) { detector.setWindowMs(ms); }

    // ── Stage parameter delegation ───────────────────────────────────────

    void setDownThresholdDb(SampleType dB) { downStage.setThresholdDb(dB); }
    void setDownRatio(SampleType r)        { downStage.setRatio(r); }
    void setDownAttackMs(SampleType ms)    { downStage.setAttackMs(ms); }
    void setDownReleaseMs(SampleType ms)   { downStage.setReleaseMs(ms); }

    void setUpThresholdDb(SampleType dB)   { upStage.setThresholdDb(dB); }
    void setUpRatio(SampleType r)          { upStage.setRatio(r); }
    void setUpAttackMs(SampleType ms)      { upStage.setAttackMs(ms); }
    void setUpReleaseMs(SampleType ms)     { upStage.setReleaseMs(ms); }

    // ── Per-sample processing ────────────────────────────────────────────

    SampleType processSample(int channel, SampleType input) {
        return processSampleWithGR(channel, input).output;
    }

    struct Result {
        SampleType output;
        SampleType totalGain;
    };

    Result processSampleWithGR(int channel, SampleType input) {
        // 1. Volume detector → smooth dB level
        const SampleType envDb = detector.processSample(channel, input);

        // 2. Independent compressor stages
        const auto downResult = downStage.processSample(channel, envDb);
        const auto upResult   = upStage.processSample(channel, envDb);

        // 3. Sum gains and apply
        const SampleType totalGainDb = downResult.gainDb + upResult.gainDb;
        const SampleType totalGain = dbToLinear(totalGainDb);
        return { input * totalGain, totalGain };
    }

    // ── UI accessors ─────────────────────────────────────────────────────

    SampleType getDetectorLevelDb(int channel) const {
        return detector.getCurrentLevelDb(channel);
    }

  private:
    static SampleType dbToLinear(SampleType dB) {
        // 10^(dB/20) = e^(dB * ln(10)/20) — std::exp is faster than std::pow
        constexpr SampleType kLog10Over20 = SampleType(0.11512925464970228);
        return std::exp(dB * kLog10Over20);
    }

    VolumeDetector<SampleType>  detector;
    CompressorStage<SampleType> downStage;
    CompressorStage<SampleType> upStage;
};
