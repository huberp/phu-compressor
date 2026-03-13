#include "PluginProcessor.h"
#include "PluginEditor.h"

PhuCompressorAudioProcessor::PhuCompressorAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout()) {
    downThreshPtr = apvts.getRawParameterValue(kParamDownThresh);
    downRatioPtr = apvts.getRawParameterValue(kParamDownRatio);
    upThreshPtr = apvts.getRawParameterValue(kParamUpThresh);
    upRatioPtr = apvts.getRawParameterValue(kParamUpRatio);
    downAttackPtr = apvts.getRawParameterValue(kParamDownAttack);
    downReleasePtr = apvts.getRawParameterValue(kParamDownRelease);
    upAttackPtr = apvts.getRawParameterValue(kParamUpAttack);
    upReleasePtr = apvts.getRawParameterValue(kParamUpRelease);
}

PhuCompressorAudioProcessor::~PhuCompressorAudioProcessor() {
}

void PhuCompressorAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = 2;
    compressor.prepare(spec);

    m_inputFifo.reset();
    m_gainReductionFifo.reset();
    m_grBuffer.setSize(2, samplesPerBlock);
    m_syncGlobals.updateSampleRate(sampleRate);
}

void PhuCompressorAudioProcessor::releaseResources() {
}

void PhuCompressorAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;

    // Track DAW state (BPM, transport)
    auto positionInfo = getPlayHead() ? getPlayHead()->getPosition()
                                      : juce::Optional<juce::AudioPlayHead::PositionInfo>();
    m_syncGlobals.updateDAWGlobals(buffer, midiMessages, positionInfo);

    // Update compressor parameters from APVTS (per-block, not per-sample)
    compressor.setDownThresholdDb(downThreshPtr->load());
    compressor.setDownRatio(downRatioPtr->load());
    compressor.setUpThresholdDb(upThreshPtr->load());
    compressor.setUpRatio(upRatioPtr->load());
    compressor.setDownAttackMs(downAttackPtr->load());
    compressor.setDownReleaseMs(downReleasePtr->load());
    compressor.setUpAttackMs(upAttackPtr->load());
    compressor.setUpReleaseMs(upReleasePtr->load());

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    // Ensure GR buffer is large enough
    if (m_grBuffer.getNumSamples() < numSamples)
        m_grBuffer.setSize(2, numSamples, false, false, true);

    // Process with gain reduction tracking
    for (int ch = 0; ch < numChannels && ch < 2; ++ch) {
        float* channelData = buffer.getWritePointer(ch);
        float* grData = m_grBuffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i) {
            auto [output, gain] = compressor.processSampleWithGR(ch, channelData[i]);
            channelData[i] = output;
            grData[i] = gain;
        }
    }

    // Push post-compression output to FIFO (for UI waveform display)
    const float* outputPtrs[2] = {buffer.getReadPointer(0),
                                   numChannels > 1 ? buffer.getReadPointer(1)
                                                   : buffer.getReadPointer(0)};
    m_inputFifo.push(outputPtrs, numSamples);

    // Push gain reduction values to FIFO (for UI GR display)
    const float* grPtrs[2] = {m_grBuffer.getReadPointer(0),
                               numChannels > 1 ? m_grBuffer.getReadPointer(1)
                                               : m_grBuffer.getReadPointer(0)};
    m_gainReductionFifo.push(grPtrs, numSamples);

    m_syncGlobals.finishRun(numSamples);
}

juce::AudioProcessorEditor* PhuCompressorAudioProcessor::createEditor() {
    return new PhuCompressorAudioProcessorEditor(*this);
}

bool PhuCompressorAudioProcessor::hasEditor() const {
    return true;
}

const juce::String PhuCompressorAudioProcessor::getName() const {
    return "PhuCompressor";
}
bool PhuCompressorAudioProcessor::acceptsMidi() const {
    return false;
}
bool PhuCompressorAudioProcessor::producesMidi() const {
    return false;
}
bool PhuCompressorAudioProcessor::isMidiEffect() const {
    return false;
}
double PhuCompressorAudioProcessor::getTailLengthSeconds() const {
    return 0.0;
}

bool PhuCompressorAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

int PhuCompressorAudioProcessor::getNumPrograms() {
    return 1;
}
int PhuCompressorAudioProcessor::getCurrentProgram() {
    return 0;
}
void PhuCompressorAudioProcessor::setCurrentProgram(int) {
}
const juce::String PhuCompressorAudioProcessor::getProgramName(int) {
    return "Default";
}
void PhuCompressorAudioProcessor::changeProgramName(int, const juce::String&) {
}

void PhuCompressorAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void PhuCompressorAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorValueTreeState::ParameterLayout
PhuCompressorAudioProcessor::createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Downward compression threshold: -60 dB to 0 dB, default -12 dB
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{kParamDownThresh, 1}, "Down Threshold",
        juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f), -12.0f));

    // Downward compression ratio: 1:1 to 20:1, default 4:1
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{kParamDownRatio, 1}, "Down Ratio",
        juce::NormalisableRange<float>(1.0f, 100.0f, 0.1f), 4.0f));

    // Upward compression threshold: -60 dB to 0 dB, default -30 dB
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{kParamUpThresh, 1}, "Up Threshold",
        juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f), -30.0f));

    // Upward compression ratio: 1:1 to 20:1, default 4:1
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{kParamUpRatio, 1}, "Up Ratio",
        juce::NormalisableRange<float>(1.0f, 100.0f, 0.1f), 4.0f));

    // Attack: 0.01 ms to 1000 ms, default 10 ms
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{kParamDownAttack, 1}, "Down Attack (ms)",
        juce::NormalisableRange<float>(0.01f, 1000.0f, 0.01f, 0.35f), 10.0f));

    // Release: 0.1 ms to 2000 ms, default 100 ms
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{kParamDownRelease, 1}, "Down Release (ms)",
        juce::NormalisableRange<float>(0.1f, 2000.0f, 0.01f, 0.5f), 100.0f));

    // Up Attack: 0.01 ms to 1000 ms, default 10 ms
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{kParamUpAttack, 1}, "Up Attack (ms)",
        juce::NormalisableRange<float>(0.01f, 1000.0f, 0.01f, 0.35f), 10.0f));

    // Up Release: 0.1 ms to 2000 ms, default 100 ms
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{kParamUpRelease, 1}, "Up Release (ms)",
        juce::NormalisableRange<float>(0.1f, 2000.0f, 0.01f, 0.5f), 100.0f));

    return layout;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new PhuCompressorAudioProcessor();
}
