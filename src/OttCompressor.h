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
        // Store sample rate and reset the lookahead delay line
        currentSampleRate_ = static_cast<SampleType>(spec.sampleRate);
        for (int ch = 0; ch < kMaxChannels; ++ch) {
            std::fill(std::begin(lookaheadBuf_[ch]), std::end(lookaheadBuf_[ch]),
                      SampleType(0));
            lookaheadWritePos_[ch] = 0;
        }
        lookaheadDelaySamples_ = 0;
        reset();
    }

    void reset() {
        detectorDown.reset();
        detectorUp.reset();
        downStage.reset();
        upStage.reset();
        for (auto& p : fastPeakUp_)
            p = SampleType(0);
        for (int ch = 0; ch < kMaxChannels; ++ch) {
            std::fill(std::begin(lookaheadBuf_[ch]), std::end(lookaheadBuf_[ch]),
                      SampleType(0));
            lookaheadWritePos_[ch] = 0;
        }
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

    // ── Lookahead control ────────────────────────────────────────────────

    void setLookaheadEnabled(bool enabled) {
        if (enabled != lookaheadEnabled_) {
            lookaheadEnabled_ = enabled;
            // Flush delay line on toggle to avoid clicks
            for (int ch = 0; ch < kMaxChannels; ++ch) {
                std::fill(std::begin(lookaheadBuf_[ch]), std::end(lookaheadBuf_[ch]),
                          SampleType(0));
                lookaheadWritePos_[ch] = 0;
            }
        }
    }

    void setLookaheadMs(SampleType ms) {
        const int newDelay = static_cast<int>(ms * currentSampleRate_ / SampleType(1000));
        lookaheadDelaySamples_ = juce::jlimit(0, kMaxLookaheadSamples - 1, newDelay);
    }

    int getLookaheadSamples() const {
        return lookaheadEnabled_ ? lookaheadDelaySamples_ : 0;
    }

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
        // ── Lookahead delay: push input, read delayed sample ─────────────────
        SampleType audioInput = input; // what goes into the gain stage
        if (lookaheadEnabled_ && lookaheadDelaySamples_ > 0) {
            // Write current sample into ring buffer
            lookaheadBuf_[channel][lookaheadWritePos_[channel]] = input;
            // Read the sample that was written lookaheadDelaySamples_ ago
            const int readPos = (lookaheadWritePos_[channel]
                                 - lookaheadDelaySamples_
                                 + kMaxLookaheadSamples) % kMaxLookaheadSamples;
            audioInput = lookaheadBuf_[channel][readPos];
            lookaheadWritePos_[channel] =
                (lookaheadWritePos_[channel] + 1) % kMaxLookaheadSamples;
        }

        // Stage 1: upward compression.
        // Detector and fast peak-follower run on UNDELAYED 'input' (look-ahead benefit).
        // Gain is applied to DELAYED 'audioInput'.
        const auto envUp = detectorUp.processSample(channel, input);   // undelayed
        const SampleType envDbUp = envUp.db;
        const SampleType absInput = std::abs(input);                    // undelayed
        fastPeakUp_[channel] = std::max(absInput, fastPeakUp_[channel] * fastPeakDecay_);
        const SampleType levelForUp = std::max(envDbUp, linearToDb(fastPeakUp_[channel]));
        const auto upResult         = upStage.processSample(channel, levelForUp);
        const SampleType upGain     = dbToLinear(upResult.gainDb);
        const SampleType intermediate = audioInput * upGain;            // delayed audio

        // Stage 2: downward compression on the upward-boosted signal.
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
        // ln(10)/20 converts dB to nepers: exp(dB * ln(10)/20) = 10^(dB/20)
        constexpr SampleType kLn10Over20 = SampleType(0.11512925464970228);
        return std::exp(dB * kLn10Over20);
    }

    static SampleType linearToDb(SampleType linear) {
        return (linear > SampleType(1e-10))
                   ? SampleType(20) * std::log10(linear)
                   : SampleType(-200);
    }

    static constexpr int kMaxChannels = 2;

    // Lookahead delay-line ring buffers (pre-allocated for kMaxLookaheadMs)
    static constexpr int kMaxLookaheadSamples =
        static_cast<int>(kMaxLookaheadMs * kMaxSampleRate / 1000.0) + 1;

    SampleType lookaheadBuf_[kMaxChannels][kMaxLookaheadSamples] = {};
    int        lookaheadWritePos_[kMaxChannels]                   = {};
    int        lookaheadDelaySamples_                             = 0;
    bool       lookaheadEnabled_                                  = false;
    SampleType currentSampleRate_                                 = SampleType(44100);

    VolumeDetector<SampleType>  detectorDown { SampleType(kDetectorMaxWindowMs) };
    VolumeDetector<SampleType>  detectorUp   { SampleType(kDetectorMaxWindowMs) };
    CompressorStage<SampleType> downStage;
    CompressorStage<SampleType> upStage;

    // Fast peak-follower state (per-channel): instant rise, ~5 ms decay.
    SampleType fastPeakUp_[kMaxChannels] = {};
    SampleType fastPeakDecay_            = SampleType(0);
};
