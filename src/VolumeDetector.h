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
    // Max window: 1 second at 96 kHz
    static constexpr int kMaxWindowSamples = 96000;

    VolumeDetector() = default;

    void prepare(const juce::dsp::ProcessSpec& spec) {
        sampleRate = static_cast<SampleType>(spec.sampleRate);
        for (int ch = 0; ch < kMaxChannels; ++ch) {
            ringBuf[ch].resize(static_cast<size_t>(kMaxWindowSamples), SampleType(0));
        }
        recomputeWindowSize();
        reset();
    }

    void reset() {
        for (int ch = 0; ch < kMaxChannels; ++ch) {
            std::fill(ringBuf[ch].begin(), ringBuf[ch].end(), SampleType(0));
            writePos[ch] = 0;
            runningSum[ch] = SampleType(0);
            currentMax[ch] = SampleType(0);
            currentLevelDb[ch] = SampleType(-120);
            samplesWritten[ch] = 0;
        }
    }

    void setMode(DetectorMode m) { mode = m; }

    void setWindowMs(SampleType ms) {
        ms = std::max(ms, SampleType(0.1));
        if (ms == windowMs) return;
        windowMs = ms;
        recomputeWindowSize();
    }

    /**
     * Process one sample and return the detector level in dB.
     */
    SampleType processSample(int channel, SampleType input) {
        const SampleType absVal = std::abs(input);
        SampleType levelLinear;

        if (mode == DetectorMode::RMS) {
            levelLinear = processRms(channel, absVal);
        } else {
            levelLinear = processPeakMax(channel, absVal);
        }

        const SampleType db = linearToDb(levelLinear);
        currentLevelDb[channel] = db;
        return db;
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
        int newSize = static_cast<int>(sampleRate * windowMs / SampleType(1000));
        newSize = std::max(1, std::min(newSize, kMaxWindowSamples));
        if (newSize != windowSamples) {
            windowSamples = newSize;
            // Lightweight reset — no buffer zeroing (safe for audio thread)
            for (int ch = 0; ch < kMaxChannels; ++ch) {
                writePos[ch] = 0;
                runningSum[ch] = SampleType(0);
                currentMax[ch] = SampleType(0);
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
    SampleType sampleRate  { SampleType(44100) };
    SampleType windowMs    { SampleType(50) };
    int windowSamples      { 2205 }; // 50ms at 44100
    DetectorMode mode      { DetectorMode::RMS };

    // Per-channel ring buffers
    // RMS mode stores squared values; PeakMax stores absolute values
    std::vector<SampleType> ringBuf[kMaxChannels];
    int writePos[kMaxChannels]         = {};
    SampleType runningSum[kMaxChannels] = {};  // RMS running sum of squares
    SampleType currentMax[kMaxChannels] = {};  // PeakMax current window max
    SampleType currentLevelDb[kMaxChannels] = { SampleType(-120), SampleType(-120) };

    int samplesWritten[kMaxChannels] = {};
};
