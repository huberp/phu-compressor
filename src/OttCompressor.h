#pragma once

#include "CompressorStage.h"
#include "PluginConstants.h"
#include "VolumeDetector.h"
#include <cmath>
#include <juce_dsp/juce_dsp.h>

/**
 * OttCompressor — single-band downward + upward compressor, sequentially chained.
 *
 * Signal path (per sample, classic OTT order):
 *   1. detectorUp detects on raw input (windowed + instantaneous max) → upward boost
 *      applied to raw input → intermediate
 *   2. detectorDown detects on intermediate → downward compression applied → output
 *
 * Upward-first is the standard OTT signal flow: quiet parts get lifted, then the
 * downward stage acts as a natural ceiling on the result, making the chain
 * self-regulating.
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
        // Fast peak-follower: instant rise, 5 ms decay.
        // Provides transient protection without per-sample carrier-frequency noise.
        constexpr SampleType kFastPeakMs = SampleType(5);
        fastPeakDecay_ = std::exp(SampleType(-1)
                                  / (static_cast<SampleType>(spec.sampleRate)
                                     * kFastPeakMs * SampleType(0.001)));
        reset();
    }

    void reset() {
        detectorDown.reset();
        detectorUp.reset();
        downStage.reset();
        upStage.reset();
        for (auto& p : fastPeakUp_)
            p = SampleType(0);
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
    void setUpSnapReleaseMs(SampleType ms)     { upStage.setSnapReleaseMs(ms); }
    void setUpSnapReleaseEnabled(bool enabled) { upStage.setSnapReleaseEnabled(enabled); }

    // ── Per-sample processing ────────────────────────────────────────────

    SampleType processSample(int channel, SampleType input) {
        return processSampleWithGR(channel, input).output;
    }

    struct Result {
        SampleType output;
        SampleType downGain; // linear gain from downward stage (≤ 1.0)
        SampleType upGain;   // linear gain from upward stage (≥ 1.0)
        SampleType envRmsUp;
        SampleType envRmsDown;
        SampleType envDbUp;  // detectorUp RMS/peak value for this sample
        SampleType envDbDown; // detectorDown RMS/peak value for this sample
    };

    Result processSampleWithGR(int channel, SampleType input) {
        // Stage 1: upward compression on raw input (classic OTT order).
        // Quiet signals get boosted first.
        //
        // Transient protection via a fast peak-follower (5 ms decay, instant rise):
        // the peak envelope tracks any above-threshold transient immediately but
        // decays smoothly — unlike a raw per-sample abs(), which collapses to -inf
        // at every zero crossing and causes the boost envelope to chase carrier-
        // frequency noise, producing audible "wiggle" and display spikes.
        const auto envUp = detectorUp.processSample(channel, input);
        const SampleType envDbUp = envUp.db;
        const SampleType absInput = std::abs(input);
        fastPeakUp_[channel] = std::max(absInput, fastPeakUp_[channel] * fastPeakDecay_);
        const SampleType levelForUp = std::max(envDbUp, linearToDb(fastPeakUp_[channel]));
        const auto upResult          = upStage.processSample(channel, levelForUp);
        const SampleType upGain      = dbToLinear(upResult.gainDb);
        const SampleType intermediate = input * upGain;

        // Stage 2: downward compression on the upward-boosted signal.
        // Acts as a natural ceiling — tames anything the upward stage pushed up,
        // making the chain self-regulating.
        const auto envDown = detectorDown.processSample(channel, intermediate);
        const SampleType envDbDown = envDown.db;
        const auto downResult      = downStage.processSample(channel, envDbDown);
        const SampleType downGain  = dbToLinear(downResult.gainDb);

        return { intermediate * downGain, downGain, upGain,
                 envUp.rms, envDown.rms, envDbUp, envDbDown };
    }

    // ── UI accessors ─────────────────────────────────────────────────────

    // Returns the raw input level from detectorUp.
    // This is what the UI should display: the level of the original signal
    // before any compression, so it lines up correctly against both thresholds
    // on the transfer curve and waveform overlay.
    SampleType getDetectorLevelDb(int channel) const {
        return detectorUp.getCurrentLevelDb(channel);
    }

    // Returns the intermediate level from detectorDown (post-upward-boost signal).
    // Used by the downward-detector overlay line in the UI.
    SampleType getDownDetectorLevelDb(int channel) const {
        return detectorDown.getCurrentLevelDb(channel);
    }

    SampleType   getUpDetectorWindowMs()   const { return detectorUp.getWindowMs();   }
    SampleType   getDownDetectorWindowMs() const { return detectorDown.getWindowMs(); }
    DetectorMode getUpDetectorMode()       const { return detectorUp.getMode();       }
    DetectorMode getDownDetectorMode()     const { return detectorDown.getMode();     }

  private:
    static SampleType dbToLinear(SampleType dB) {
        constexpr SampleType kLog10Over20 = SampleType(0.11512925464970228);
        return std::exp(dB * kLog10Over20);
    }

    static SampleType linearToDb(SampleType linear) {
        return (linear > SampleType(1e-10))
                   ? SampleType(20) * std::log10(linear)
                   : SampleType(-200);
    }

    static constexpr int kMaxChannels = 2;

    VolumeDetector<SampleType>  detectorDown { SampleType(kDetectorMaxWindowMs) };
    VolumeDetector<SampleType>  detectorUp   { SampleType(kDetectorMaxWindowMs) };
    CompressorStage<SampleType> downStage;
    CompressorStage<SampleType> upStage;

    // Fast peak-follower state (per-channel): instant rise, ~5 ms decay.
    SampleType fastPeakUp_[kMaxChannels] = {};
    SampleType fastPeakDecay_            = SampleType(0);
};
