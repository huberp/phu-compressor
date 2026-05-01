#pragma once

#include "OttCompressor.h"
#include "PluginConstants.h"
#include "audio/AudioSampleFifo.h"
#include "audio/BeatSyncBuffer.h"
#include "audio/RmsPacketFifo.h"
#include "events/SyncGlobals.h"
#include <juce_audio_processors/juce_audio_processors.h>

using phu::audio::AudioSampleFifo;
using phu::audio::BeatSyncBufferF;
using phu::audio::RmsPacketFifo;
using phu::audio::RmsPacket;
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
    AudioSampleFifo<2>& getGainReductionFifo() { return m_gainReductionFifo; }  // downward GR
    AudioSampleFifo<2>& getUpGainReductionFifo() { return m_upGrFifo; }         // upward boost
    RmsPacketFifo& getDetectorPacketFifo()      { return m_detectorPacketFifo; }      // up-detector
    RmsPacketFifo& getDownDetectorPacketFifo()  { return m_downDetectorPacketFifo; }  // down-detector
    SyncGlobals& getSyncGlobals() { return m_syncGlobals; }

    struct DetectorInfo {
        float upMs; float downMs;
        DetectorMode upMode; DetectorMode downMode;
    };
    DetectorInfo getDetectorInfo() const {
        return { compressor.getUpDetectorWindowMs(),
                 compressor.getDownDetectorWindowMs(),
                 compressor.getUpDetectorMode(),
                 compressor.getDownDetectorMode() };
    }

    // Beat-sync buffer access (read-only pointers for UI)
    const BeatSyncBufferF& getInputSyncBuffer() const { return m_inputSyncBuf; }
    const BeatSyncBufferF& getGRSyncBuffer() const { return m_grSyncBuf; }         // downward GR
    const BeatSyncBufferF& getUpGRSyncBuffer() const { return m_upGrSyncBuf; }     // upward boost

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

    // Parameter IDs — snap release (2 new)
    static constexpr const char* kParamUpSnapRelease        = "up_snap_release";
    static constexpr const char* kParamUpSnapReleaseEnabled = "up_snap_release_enabled";

    // Parameter IDs — detector (5 new)
    static constexpr const char* kParamDetectorType = "detector_type";
    static constexpr const char* kParamRmsWindowMs  = "rms_window_ms";
    static constexpr const char* kParamRmsSyncMode  = "rms_sync_mode";
    static constexpr const char* kParamRmsBeatDiv   = "rms_beat_div";
    static constexpr const char* kParamPeakWindowMs  = "peak_window_ms";

    // Beat-division fractions and labels live in PluginConstants.h (kDetector*).
    // kDetectorNumDivisions, kDetectorBeatFractions, kDetectorBeatLabels.

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
    std::atomic<float>* upSnapReleasePtr        {nullptr};
    std::atomic<float>* upSnapReleaseEnabledPtr {nullptr};

    // Detector parameter pointers
    std::atomic<float>* detectorTypePtr{nullptr};
    std::atomic<float>* rmsWindowMsPtr{nullptr};
    std::atomic<float>* rmsSyncModePtr{nullptr};
    std::atomic<float>* rmsBeatDivPtr{nullptr};
    std::atomic<float>* peakWindowMsPtr{nullptr};

    // Lock-free FIFOs for audio→UI data transfer
    AudioSampleFifo<2> m_inputFifo;
    AudioSampleFifo<2> m_gainReductionFifo; // downward GR (linear, ≤ 1.0)
    AudioSampleFifo<2> m_upGrFifo;          // upward boost (linear, ≥ 1.0)

    // Packet FIFOs for detector levels (batched, PPQ-anchored)
    RmsPacketFifo m_detectorPacketFifo;     // up-detector level (raw input)
    RmsPacketFifo m_downDetectorPacketFifo; // down-detector level (post-upward-boost)

    // Accumulation state: collect ~kRmsAccumBlocks blocks before pushing a packet
    static constexpr int kRmsAccumBlocks = 4;
    std::array<float, phu::audio::kRmsMaxPacketSamples> m_detAccumBuf{};
    std::array<float, phu::audio::kRmsMaxPacketSamples> m_downDetAccumBuf{};
    int    m_accumCount      = 0;
    int    m_accumBlockCount = 0;
    double m_accumStartPpq   = 0.0;

    // Temp buffers (reused each processBlock)
    juce::AudioBuffer<float> m_grBuffer;    // downward GR per-sample
    juce::AudioBuffer<float> m_upGrBuffer;  // upward boost per-sample

    // DAW state tracking (BPM, sample rate, transport)
    SyncGlobals m_syncGlobals;

    // Beat-sync position-indexed buffers (written per-sample in processBlock)
    BeatSyncBufferF m_inputSyncBuf;
    BeatSyncBufferF m_grSyncBuf;     // downward GR
    BeatSyncBufferF m_upGrSyncBuf;   // upward boost
    std::atomic<double> m_displayRangeBeats{4.0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhuCompressorAudioProcessor)
};
