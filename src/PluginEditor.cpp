#include "PluginEditor.h"
#include "PluginProcessor.h"

static void setupSlider(juce::Slider& slider, juce::Label& label, const juce::String& text,
                        juce::Component* parent) {
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centredLeft);
    label.setFont(juce::Font(11.0f));
    parent->addAndMakeVisible(label);

    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 18);
    parent->addAndMakeVisible(slider);
}

PhuCompressorAudioProcessorEditor::PhuCompressorAudioProcessorEditor(
    PhuCompressorAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p),
      compressorDisplay(p.getAPVTS()),
      downThreshAttachment(p.getAPVTS(), PhuCompressorAudioProcessor::kParamDownThresh,
                           downThreshSlider),
      downRatioAttachment(p.getAPVTS(), PhuCompressorAudioProcessor::kParamDownRatio,
                          downRatioSlider),
      upThreshAttachment(p.getAPVTS(), PhuCompressorAudioProcessor::kParamUpThresh,
                         upThreshSlider),
      upRatioAttachment(p.getAPVTS(), PhuCompressorAudioProcessor::kParamUpRatio, upRatioSlider),
      downAttackAttachment(p.getAPVTS(), PhuCompressorAudioProcessor::kParamDownAttack,
                           downAttackSlider),
      downReleaseAttachment(p.getAPVTS(), PhuCompressorAudioProcessor::kParamDownRelease,
                            downReleaseSlider),
      upAttackAttachment(p.getAPVTS(), PhuCompressorAudioProcessor::kParamUpAttack,
                         upAttackSlider),
      upReleaseAttachment(p.getAPVTS(), PhuCompressorAudioProcessor::kParamUpRelease,
                          upReleaseSlider) {
    setupSlider(downThreshSlider, downThreshLabel, "Down Thresh (dB)", this);
    setupSlider(downRatioSlider, downRatioLabel, "Down Ratio", this);
    setupSlider(downAttackSlider, downAttackLabel, "Down Attack (ms)", this);
    setupSlider(downReleaseSlider, downReleaseLabel, "Down Release (ms)", this);
    setupSlider(upThreshSlider, upThreshLabel, "Up Thresh (dB)", this);
    setupSlider(upRatioSlider, upRatioLabel, "Up Ratio", this);
    setupSlider(upAttackSlider, upAttackLabel, "Up Attack (ms)", this);
    setupSlider(upReleaseSlider, upReleaseLabel, "Up Release (ms)", this);

    addAndMakeVisible(compressorDisplay);

    setSize(720, 460);
    startTimerHz(60);
}

PhuCompressorAudioProcessorEditor::~PhuCompressorAudioProcessorEditor() {
    stopTimer();
}

void PhuCompressorAudioProcessorEditor::timerCallback() {
    // Update sample rate and BPM
    auto& syncGlobals = audioProcessor.getSyncGlobals();
    compressorDisplay.setSampleRate(syncGlobals.getSampleRate() > 0
                                        ? syncGlobals.getSampleRate()
                                        : 48000.0);
    compressorDisplay.setBPM(syncGlobals.getBPM());

    // Pull data from FIFOs and repaint
    compressorDisplay.updateFromFifos(audioProcessor.getInputFifo(),
                                       audioProcessor.getGainReductionFifo());
    compressorDisplay.repaint();
}

void PhuCompressorAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(16.0f, juce::Font::bold));
    g.drawText("PHU COMPRESSOR", getLocalBounds().removeFromTop(28),
               juce::Justification::centred, true);
}

void PhuCompressorAudioProcessorEditor::resized() {
    auto area = getLocalBounds().reduced(8);
    area.removeFromTop(28); // Title

    const int sliderColumnWidth = 220;
    auto sliderArea = area.removeFromLeft(sliderColumnWidth);

    const int rowHeight = 26;
    const int labelWidth = 110;
    const int gap = 3;

    auto layoutRow = [&](juce::Label& label, juce::Slider& slider) {
        auto row = sliderArea.removeFromTop(rowHeight);
        label.setBounds(row.removeFromLeft(labelWidth));
        slider.setBounds(row);
        sliderArea.removeFromTop(gap);
    };

    layoutRow(downThreshLabel, downThreshSlider);
    layoutRow(downRatioLabel, downRatioSlider);
    layoutRow(downAttackLabel, downAttackSlider);
    layoutRow(downReleaseLabel, downReleaseSlider);
    layoutRow(upThreshLabel, upThreshSlider);
    layoutRow(upRatioLabel, upRatioSlider);
    layoutRow(upAttackLabel, upAttackSlider);
    layoutRow(upReleaseLabel, upReleaseSlider);

    // Display panel fills remaining space
    auto displayArea = area.withTrimmedLeft(8);
    compressorDisplay.setBounds(displayArea);
}
