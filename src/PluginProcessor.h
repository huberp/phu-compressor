#pragma once

#include "OttCompressor.h"
#include "audio/AudioSampleFifo.h"
#include "events/SyncGlobals.h"
#include <juce_audio_processors/juce_audio_processors.h>

using phu::audio::AudioSampleFifo;
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
    SyncGlobals& getSyncGlobals() { return m_syncGlobals; }

    // Parameter IDs
    static constexpr const char* kParamDownThresh = "down_thresh";
    static constexpr const char* kParamDownRatio = "down_ratio";
    static constexpr const char* kParamUpThresh = "up_thresh";
    static constexpr const char* kParamUpRatio = "up_ratio";
    static constexpr const char* kParamDownAttack = "down_attack_ms";
    static constexpr const char* kParamDownRelease = "down_release_ms";
    static constexpr const char* kParamUpAttack = "up_attack_ms";
    static constexpr const char* kParamUpRelease = "up_release_ms";

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

    // Lock-free FIFOs for audio→UI data transfer
    AudioSampleFifo<2> m_inputFifo;
    AudioSampleFifo<2> m_gainReductionFifo;

    // Temp buffer for gain reduction values (reused each processBlock)
    juce::AudioBuffer<float> m_grBuffer;

    // DAW state tracking (BPM, sample rate, transport)
    SyncGlobals m_syncGlobals;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhuCompressorAudioProcessor)
};
