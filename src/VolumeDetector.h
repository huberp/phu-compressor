#pragma once

#include <algorithm>
#include <cmath>
#include <vector>
#include <juce_dsp/juce_dsp.h>

enum class DetectorMode { RMS, PeakMax };

/**
 * VolumeDetector — selectable RMS or PeakMax level detector with configurable window.
 *
 * Modes:
 *   - RMS: rolling window of squared samples, output = sqrt(mean) → dB.
 *     Uses running sum for O(1) per sample.
 *   - PeakMax: rolling window of absolute samples, output = max in window → dB.
 *     On eviction of current max, rescans buffer (acceptable for ≤50ms windows).
 *
 * The detector feeds a smooth dB level to downstream CompressorStage instances.
 */
template <typename SampleType>
class VolumeDetector {
  public:
    static constexpr int kMaxChannels = 2;

        struct LevelValues {
                SampleType rms;
                SampleType db;
        };

    /**
     * maxWindowMs — the maximum window length this instance will ever be asked to use.
     * The ring buffer is pre-allocated to this size (in samples at the actual sample rate)
     * during prepare(), so no allocation happens on the audio thread.
     * Caller is responsible for passing a value large enough for the worst-case BPM/beat
     * combination they intend to use. See PluginConstants.h:kDetectorMaxWindowMs.
     */
    explicit VolumeDetector(SampleType maxWindowMs = SampleType(6000))
        : maxWindowMs_(maxWindowMs) {}

    void prepare(const juce::dsp::ProcessSpec& spec) {
        sampleRate = static_cast<SampleType>(spec.sampleRate);
        // Allocate exactly enough for the declared max window at this sample rate.
        // Adding 1 guards against floating-point truncation at the boundary.
        bufCapacity_ = static_cast<int>(sampleRate * maxWindowMs_ / SampleType(1000)) + 1;
        for (int ch = 0; ch < kMaxChannels; ++ch)
            ringBuf[ch].assign(static_cast<size_t>(bufCapacity_), SampleType(0));
        recomputeWindowSize();
        reset();
    }

    void reset() {
        for (int ch = 0; ch < kMaxChannels; ++ch) {
            std::fill(ringBuf[ch].begin(), ringBuf[ch].end(), SampleType(0));
            writePos[ch] = 0;
            runningSum[ch] = SampleType(0);
            currentMax[ch] = SampleType(0);
            currentLevelLinear[ch] = SampleType(0);
            currentLevelDb[ch] = SampleType(-120);
            samplesWritten[ch] = 0;
        }
    }

    void setMode(DetectorMode m) { mode = m; }
    DetectorMode getMode() const { return mode; }

    SampleType getWindowMs() const { return windowMs; }

    void setWindowMs(SampleType ms) {
        ms = std::max(ms, SampleType(0.1));
        if (ms == windowMs) return;
        windowMs = ms;
        recomputeWindowSize();
    }

    /**
     * Process one sample and return both the raw detector value and its dB form.
     */
    LevelValues processSample(int channel, SampleType input) {
        const SampleType absVal = std::abs(input);
        SampleType levelLinear;

        if (mode == DetectorMode::RMS) {
            levelLinear = processRms(channel, absVal);
        } else {
            levelLinear = processPeakMax(channel, absVal);
        }

        const SampleType db = linearToDb(levelLinear);
        currentLevelLinear[channel] = levelLinear;
        currentLevelDb[channel] = db;
        return { levelLinear, db };
    }

    SampleType getCurrentLevelLinear(int channel) const {
        return currentLevelLinear[channel];
    }

    SampleType getCurrentLevelDb(int channel) const {
        return currentLevelDb[channel];
    }

  private:
    SampleType processRms(int channel, SampleType absVal) {
        const SampleType squared = absVal * absVal;
        auto& ring = ringBuf[channel];
        auto& wp = writePos[channel];
        auto& sum = runningSum[channel];
        auto& sw = samplesWritten[channel];

        // Only subtract outgoing sample when ring is full (avoids stale data after window change)
        if (sw >= windowSamples) {
            sum -= ring[static_cast<size_t>(wp)];
        } else {
            ++sw;
        }

        ring[static_cast<size_t>(wp)] = squared;
        sum += squared;
        // Guard against negative drift from floating point
        if (sum < SampleType(0)) sum = SampleType(0);

        wp = (wp + 1) % windowSamples;

        return std::sqrt(sum / static_cast<SampleType>(sw));
    }

    SampleType processPeakMax(int channel, SampleType absVal) {
        auto& ring = ringBuf[channel];
        auto& wp = writePos[channel];
        auto& curMax = currentMax[channel];
        auto& sw = samplesWritten[channel];

        const SampleType evicted = ring[static_cast<size_t>(wp)];
        ring[static_cast<size_t>(wp)] = absVal;
        wp = (wp + 1) % windowSamples;

        // Track fill state
        const bool windowFull = (sw >= windowSamples);
        if (!windowFull) ++sw;

        if (absVal >= curMax) {
            curMax = absVal;
        } else if (windowFull && evicted >= curMax) {
            // Lost the max — rescan the active window
            curMax = SampleType(0);
            for (int i = 0; i < windowSamples; ++i) {
                curMax = std::max(curMax, ring[static_cast<size_t>(i)]);
            }
        }
        return curMax;
    }

    void recomputeWindowSize() {
        // bufCapacity_ is 0 before prepare() — guard so setWindowMs() is safe to call early.
        const int cap = (bufCapacity_ > 0) ? bufCapacity_ : 1;
        int newSize = static_cast<int>(sampleRate * windowMs / SampleType(1000));
        newSize = std::max(1, std::min(newSize, cap));
        if (newSize != windowSamples) {
            windowSamples = newSize;
            // Lightweight reset — no buffer zeroing (safe for audio thread)
            for (int ch = 0; ch < kMaxChannels; ++ch) {
                writePos[ch] = 0;
                runningSum[ch] = SampleType(0);
                currentMax[ch] = SampleType(0);
                currentLevelLinear[ch] = SampleType(0);
                samplesWritten[ch] = 0;
            }
        }
    }

    static SampleType linearToDb(SampleType linear) {
        constexpr SampleType floor = SampleType(-120);
        return (linear > SampleType(0))
                   ? std::max(SampleType(20) * std::log10(linear), floor)
                   : floor;
    }

    // ── State ────────────────────────────────────────────────────────────
    SampleType maxWindowMs_  { SampleType(6000) };  ///< set by constructor, immutable after that
    int        bufCapacity_  { 0 };                 ///< set in prepare(), ring buffer size in samples
    SampleType sampleRate    { SampleType(44100) };
    SampleType windowMs      { SampleType(50) };
    int        windowSamples { 2205 };              ///< 50 ms at 44.1 kHz
    DetectorMode mode        { DetectorMode::RMS };

    // Per-channel ring buffers
    // RMS mode stores squared values; PeakMax stores absolute values
    std::vector<SampleType> ringBuf[kMaxChannels];
    int writePos[kMaxChannels]         = {};
    SampleType runningSum[kMaxChannels] = {};  // RMS running sum of squares
    SampleType currentMax[kMaxChannels] = {};  // PeakMax current window max
    SampleType currentLevelLinear[kMaxChannels] = {};
    SampleType currentLevelDb[kMaxChannels] = { SampleType(-120), SampleType(-120) };

    int samplesWritten[kMaxChannels] = {};
};
