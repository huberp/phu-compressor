#pragma once

#include "OttCompressor.h"
#include "audio/AudioSampleFifo.h"
#include "audio/BeatSyncBuffer.h"
#include "events/SyncGlobals.h"
#include <juce_audio_processors/juce_audio_processors.h>

using phu::audio::AudioSampleFifo;
using phu::audio::BeatSyncBuffer;
using phu::events::SyncGlobals;

class PhuCompressorAudioProcessor : public juce::AudioProcessor {
  public:
    PhuCompressorAudioProcessor();
    ~PhuCompressorAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

    AudioSampleFifo<2>& getInputFifo() { return m_inputFifo; }
    AudioSampleFifo<2>& getGainReductionFifo() { return m_gainReductionFifo; }
    AudioSampleFifo<2>& getDetectorFifo() { return m_detectorFifo; }
    SyncGlobals& getSyncGlobals() { return m_syncGlobals; }

    // Beat-sync buffer access (read-only pointers for UI)
    const BeatSyncBuffer& getInputSyncBuffer() const { return m_inputSyncBuf; }
    const BeatSyncBuffer& getGRSyncBuffer() const { return m_grSyncBuf; }
    const BeatSyncBuffer& getDetectorSyncBuffer() const { return m_detectorSyncBuf; }

    /** Set the display range in beats (called from UI thread). */
    void setDisplayRangeBeats(double beats) {
        m_displayRangeBeats.store(beats, std::memory_order_relaxed);
    }

    // Parameter IDs — compressor stages (8 existing)
    static constexpr const char* kParamDownThresh = "down_thresh";
    static constexpr const char* kParamDownRatio = "down_ratio";
    static constexpr const char* kParamUpThresh = "up_thresh";
    static constexpr const char* kParamUpRatio = "up_ratio";
    static constexpr const char* kParamDownAttack = "down_attack_ms";
    static constexpr const char* kParamDownRelease = "down_release_ms";
    static constexpr const char* kParamUpAttack = "up_attack_ms";
    static constexpr const char* kParamUpRelease = "up_release_ms";

    // Parameter IDs — detector (5 new)
    static constexpr const char* kParamDetectorType = "detector_type";
    static constexpr const char* kParamRmsWindowMs  = "rms_window_ms";
    static constexpr const char* kParamRmsSyncMode  = "rms_sync_mode";
    static constexpr const char* kParamRmsBeatDiv   = "rms_beat_div";
    static constexpr const char* kParamPeakWindowMs  = "peak_window_ms";

    // Beat-division fractions in beats (must match ComboBox item order)
    // 1/8 = eighth note = 0.5 beats, 1/4 = quarter = 1 beat, etc.
    static constexpr int kNumBeatDivisions = 6;
    static constexpr float kBeatFractions[kNumBeatDivisions] = {
        0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f };

  private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

    // One compressor instance (stereo: channel 0 = L, channel 1 = R)
    OttCompressor<float> compressor;

    // Cached raw parameter pointers for audio-thread access
    std::atomic<float>* downThreshPtr{nullptr};
    std::atomic<float>* downRatioPtr{nullptr};
    std::atomic<float>* upThreshPtr{nullptr};
    std::atomic<float>* upRatioPtr{nullptr};
    std::atomic<float>* downAttackPtr{nullptr};
    std::atomic<float>* downReleasePtr{nullptr};
    std::atomic<float>* upAttackPtr{nullptr};
    std::atomic<float>* upReleasePtr{nullptr};

    // Detector parameter pointers
    std::atomic<float>* detectorTypePtr{nullptr};
    std::atomic<float>* rmsWindowMsPtr{nullptr};
    std::atomic<float>* rmsSyncModePtr{nullptr};
    std::atomic<float>* rmsBeatDivPtr{nullptr};
    std::atomic<float>* peakWindowMsPtr{nullptr};

    // Lock-free FIFOs for audio→UI data transfer
    AudioSampleFifo<2> m_inputFifo;
    AudioSampleFifo<2> m_gainReductionFifo;
    AudioSampleFifo<2> m_detectorFifo;

    // Temp buffers (reused each processBlock)
    juce::AudioBuffer<float> m_grBuffer;
    juce::AudioBuffer<float> m_detectorBuffer;

    // DAW state tracking (BPM, sample rate, transport)
    SyncGlobals m_syncGlobals;

    // Beat-sync position-indexed buffers (written per-sample in processBlock)
    BeatSyncBuffer m_inputSyncBuf;
    BeatSyncBuffer m_grSyncBuf;
    BeatSyncBuffer m_detectorSyncBuf;
    std::atomic<double> m_displayRangeBeats{4.0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhuCompressorAudioProcessor)
};
