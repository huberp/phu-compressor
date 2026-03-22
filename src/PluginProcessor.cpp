#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

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

    detectorTypePtr = apvts.getRawParameterValue(kParamDetectorType);
    rmsWindowMsPtr  = apvts.getRawParameterValue(kParamRmsWindowMs);
    rmsSyncModePtr  = apvts.getRawParameterValue(kParamRmsSyncMode);
    rmsBeatDivPtr   = apvts.getRawParameterValue(kParamRmsBeatDiv);
    peakWindowMsPtr = apvts.getRawParameterValue(kParamPeakWindowMs);
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
    m_detectorFifo.reset();

    // Pre-allocate temp buffers with headroom — never reallocate on the audio thread
    const int bufSize = juce::jmax(samplesPerBlock, 8192);
    m_grBuffer.setSize(2, bufSize);
    m_detectorBuffer.setSize(2, bufSize);
    m_syncGlobals.updateSampleRate(sampleRate);

    // Beat-sync buffers: 4096 bins for position-indexed display
    m_inputSyncBuf.prepare(4096);
    m_grSyncBuf.prepare(4096);
    m_detectorSyncBuf.prepare(4096);
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

    // Detector configuration
    const int detType = static_cast<int>(detectorTypePtr->load());
    compressor.setDetectorMode(detType == 0 ? DetectorMode::RMS : DetectorMode::PeakMax);

    if (detType == 0) {
        // RMS mode: check BPM sync
        const bool rmsSynced = rmsSyncModePtr->load() >= 0.5f;
        if (rmsSynced && m_syncGlobals.getBPM() > 0.0) {
            const int beatIdx = juce::jlimit(0, kNumBeatDivisions - 1,
                                             juce::roundToInt(rmsBeatDivPtr->load()));
            const float beatFrac = kBeatFractions[beatIdx];
            const float windowMs = static_cast<float>(
                (static_cast<double>(beatFrac) / m_syncGlobals.getBPM()) * 60000.0);
            compressor.setDetectorWindowMs(std::max(1.0f, windowMs));
        } else {
            compressor.setDetectorWindowMs(rmsWindowMsPtr->load());
        }
    } else {
        compressor.setDetectorWindowMs(peakWindowMsPtr->load());
    }

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    // Temp buffers pre-allocated in prepareToPlay — no runtime allocation
    jassert(m_grBuffer.getNumSamples() >= numSamples);
    jassert(m_detectorBuffer.getNumSamples() >= numSamples);

    // Process with gain reduction + detector level tracking
    for (int ch = 0; ch < numChannels && ch < 2; ++ch) {
        float* channelData = buffer.getWritePointer(ch);
        float* grData = m_grBuffer.getWritePointer(ch);
        float* detData = m_detectorBuffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i) {
            auto [output, gain] = compressor.processSampleWithGR(ch, channelData[i]);
            channelData[i] = output;
            grData[i] = gain;
            detData[i] = compressor.getDetectorLevelDb(ch);
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

    // Push detector level values to FIFO (for UI detector curve overlay)
    const float* detPtrs[2] = {m_detectorBuffer.getReadPointer(0),
                                numChannels > 1 ? m_detectorBuffer.getReadPointer(1)
                                                : m_detectorBuffer.getReadPointer(0)};
    m_detectorFifo.push(detPtrs, numSamples);

    // Beat-sync buffer writes: per-sample PPQ → normalised position → bin
    {
        const double bpm = m_syncGlobals.getBPM();
        const double blockPpq = m_syncGlobals.getPpqBlockStart();
        const double displayRange = m_displayRangeBeats.load(std::memory_order_relaxed);

        if (bpm > 0.0 && displayRange > 0.0) {
            const double ppqPerSample = bpm / (60.0 * getSampleRate());
            for (int i = 0; i < numSamples; ++i) {
                const double ppq_i = blockPpq + i * ppqPerSample;
                double normPos = std::fmod(ppq_i, displayRange) / displayRange;
                if (normPos < 0.0) normPos += 1.0;

                // Input: stereo → mono → abs → dB
                const float monoIn = (outputPtrs[0][i] + outputPtrs[1][i]) * 0.5f;
                const float absIn = std::abs(monoIn);
                const float inDb = (absIn > 1e-10f) ? 20.0f * std::log10(absIn) : -60.0f;

                // GR: stereo → min → dB
                const float grLin = std::min(grPtrs[0][i], grPtrs[1][i]);
                const float grDb = (grLin > 1e-10f) ? 20.0f * std::log10(grLin) : -24.0f;

                // Detector: stereo → mono (already in dB)
                const float detDb = (detPtrs[0][i] + detPtrs[1][i]) * 0.5f;

                m_inputSyncBuf.write(normPos, inDb);
                m_grSyncBuf.write(normPos, grDb);
                m_detectorSyncBuf.write(normPos, detDb);
            }
            m_syncGlobals.setPpqEndOfBlock(blockPpq + numSamples * ppqPerSample);
        }
    }

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

    // ── Detector parameters ──────────────────────────────────────────────

    // Detector type: 0 = RMS, 1 = Peak
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{kParamDetectorType, 1}, "Detector Type",
        juce::StringArray{"RMS", "Peak"}, 1));

    // RMS window length: 1 ms to 500 ms, default 50 ms
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{kParamRmsWindowMs, 1}, "RMS Window (ms)",
        juce::NormalisableRange<float>(1.0f, 500.0f, 0.1f, 0.4f), 50.0f));

    // RMS BPM sync toggle
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{kParamRmsSyncMode, 1}, "RMS Sync", false));

    // RMS beat division: 0 = 1/8, 1 = 1/4, 2 = 1/2, 3 = 1, 4 = 2, 5 = 4
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{kParamRmsBeatDiv, 1}, "RMS Beat Div",
        juce::StringArray{"1/8", "1/4", "1/2", "1", "2", "4"}, 1));

    // Peak window length: 0.1 ms to 50 ms, default 5 ms
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{kParamPeakWindowMs, 1}, "Peak Window (ms)",
        juce::NormalisableRange<float>(0.1f, 50.0f, 0.01f, 0.5f), 5.0f));

    return layout;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new PhuCompressorAudioProcessor();
}
