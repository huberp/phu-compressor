#pragma once

#include "CompressorStage.h"
#include "VolumeDetector.h"
#include <cmath>
#include <juce_dsp/juce_dsp.h>

/**
 * OttCompressor — single-band downward + upward compressor, sequentially chained.
 *
 * Signal path (per sample):
 *   1. detectorDown detects on raw input → envDbDown
 *   2. downStage compresses → downGainDb, applied to input → intermediate
 *   3. detectorUp detects on intermediate (post-downward) signal → envDbUp
 *   4. upStage boosts → upGainDb, applied to intermediate → output
 *
 * Each stage has its own VolumeDetector so it sees its correct input.
 * Gains are returned separately so the UI can display orange (down) and
 * green (up) overlays independently without ambiguity.
 */
template <typename SampleType>
class OttCompressor {
  public:
    OttCompressor() {
        downStage.setDirection(StageDirection::Downward);
        upStage.setDirection(StageDirection::Upward);
    }

    void prepare(const juce::dsp::ProcessSpec& spec) {
        detectorDown.prepare(spec);
        detectorUp.prepare(spec);
        downStage.prepare(spec);
        upStage.prepare(spec);
        reset();
    }

    void reset() {
        detectorDown.reset();
        detectorUp.reset();
        downStage.reset();
        upStage.reset();
    }

    // ── Detector configuration (applied to both detectors) ───────────────

    void setDetectorMode(DetectorMode mode) {
        detectorDown.setMode(mode);
        detectorUp.setMode(mode);
    }
    void setDetectorWindowMs(SampleType ms) {
        detectorDown.setWindowMs(ms);
        detectorUp.setWindowMs(ms);
    }

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
        SampleType downGain; // linear gain from downward stage (≤ 1.0)
        SampleType upGain;   // linear gain from upward stage (≥ 1.0)
    };

    Result processSampleWithGR(int channel, SampleType input) {
        // Stage 1: downward compression on raw input
        const SampleType envDbDown = detectorDown.processSample(channel, input);
        const auto downResult      = downStage.processSample(channel, envDbDown);
        const SampleType downGain  = dbToLinear(downResult.gainDb);
        const SampleType intermediate = input * downGain;

        // Stage 2: upward compression.
        // Detection is against the ORIGINAL input level, not intermediate.
        // This ensures the upward stage never boosts a signal that was originally
        // above the upward threshold — even if downward compression brought it
        // below that threshold in intermediate.
        // The boost is applied to intermediate (post-downward signal).
        const SampleType envDbUp  = detectorUp.processSample(channel, input);
        const auto upResult       = upStage.processSample(channel, envDbUp);
        const SampleType upGain   = dbToLinear(upResult.gainDb);

        return { intermediate * upGain, downGain, upGain };
    }

    // ── UI accessors ─────────────────────────────────────────────────────

    // Returns the down-detector level (i.e. level of the input signal)
    SampleType getDetectorLevelDb(int channel) const {
        return detectorDown.getCurrentLevelDb(channel);
    }

  private:
    static SampleType dbToLinear(SampleType dB) {
        constexpr SampleType kLog10Over20 = SampleType(0.11512925464970228);
        return std::exp(dB * kLog10Over20);
    }

    VolumeDetector<SampleType>  detectorDown;
    VolumeDetector<SampleType>  detectorUp;
    CompressorStage<SampleType> downStage;
    CompressorStage<SampleType> upStage;
};
