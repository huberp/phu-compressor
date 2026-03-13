#include "PluginEditor.h"
#include "PluginProcessor.h"

static void setupSlider(juce::Slider& slider, juce::Label& label, const juce::String& text,
                        juce::Component* parent) {
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centredLeft);
    label.setFont(juce::Font(13.0f));
    parent->addAndMakeVisible(label);

    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    parent->addAndMakeVisible(slider);
}

PhuCompressorAudioProcessorEditor::PhuCompressorAudioProcessorEditor(
    PhuCompressorAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p),
      downThreshAttachment(p.getAPVTS(), PhuCompressorAudioProcessor::kParamDownThresh,
                           downThreshSlider),
      downRatioAttachment(p.getAPVTS(), PhuCompressorAudioProcessor::kParamDownRatio,
                          downRatioSlider),
      upThreshAttachment(p.getAPVTS(), PhuCompressorAudioProcessor::kParamUpThresh,
                         upThreshSlider),
      upRatioAttachment(p.getAPVTS(), PhuCompressorAudioProcessor::kParamUpRatio, upRatioSlider),
      attackAttachment(p.getAPVTS(), PhuCompressorAudioProcessor::kParamAttack, attackSlider),
      releaseAttachment(p.getAPVTS(), PhuCompressorAudioProcessor::kParamRelease, releaseSlider) {
    setupSlider(downThreshSlider, downThreshLabel, "Down Threshold (dB)", this);
    setupSlider(downRatioSlider, downRatioLabel, "Down Ratio", this);
    setupSlider(upThreshSlider, upThreshLabel, "Up Threshold (dB)", this);
    setupSlider(upRatioSlider, upRatioLabel, "Up Ratio", this);
    setupSlider(attackSlider, attackLabel, "Attack (ms)", this);
    setupSlider(releaseSlider, releaseLabel, "Release (ms)", this);

    setSize(460, 240);
}

PhuCompressorAudioProcessorEditor::~PhuCompressorAudioProcessorEditor() {
}

void PhuCompressorAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(16.0f, juce::Font::bold));
    g.drawText("PHU COMPRESSOR", getLocalBounds().removeFromTop(30),
               juce::Justification::centred, true);
}

void PhuCompressorAudioProcessorEditor::resized() {
    auto area = getLocalBounds().reduced(10);
    area.removeFromTop(30); // Title

    const int rowHeight = 30;
    const int labelWidth = 160;
    const int gap = 4;

    auto layoutRow = [&](juce::Label& label, juce::Slider& slider) {
        auto row = area.removeFromTop(rowHeight);
        label.setBounds(row.removeFromLeft(labelWidth));
        slider.setBounds(row);
        area.removeFromTop(gap);
    };

    layoutRow(downThreshLabel, downThreshSlider);
    layoutRow(downRatioLabel, downRatioSlider);
    layoutRow(upThreshLabel, upThreshSlider);
    layoutRow(upRatioLabel, upRatioSlider);
    layoutRow(attackLabel, attackSlider);
    layoutRow(releaseLabel, releaseSlider);
}
