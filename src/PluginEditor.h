#pragma once

#include "CompressorDisplay.h"
#include "PluginProcessor.h"
#include <juce_audio_processors/juce_audio_processors.h>

class PhuCompressorAudioProcessorEditor : public juce::AudioProcessorEditor,
                                          private juce::Timer {
  public:
    PhuCompressorAudioProcessorEditor(PhuCompressorAudioProcessor&);
    ~PhuCompressorAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

  private:
    void timerCallback() override;

    PhuCompressorAudioProcessor& audioProcessor;

    // Unified display panel
    CompressorDisplay compressorDisplay;

    // Parameter sliders
    juce::Slider downThreshSlider;
    juce::Slider downRatioSlider;
    juce::Slider upThreshSlider;
    juce::Slider upRatioSlider;
    juce::Slider downAttackSlider;
    juce::Slider downReleaseSlider;
    juce::Slider upAttackSlider;
    juce::Slider upReleaseSlider;

    // Labels
    juce::Label downThreshLabel;
    juce::Label downRatioLabel;
    juce::Label upThreshLabel;
    juce::Label upRatioLabel;
    juce::Label downAttackLabel;
    juce::Label downReleaseLabel;
    juce::Label upAttackLabel;
    juce::Label upReleaseLabel;

    // APVTS attachments
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    SliderAttachment downThreshAttachment;
    SliderAttachment downRatioAttachment;
    SliderAttachment upThreshAttachment;
    SliderAttachment upRatioAttachment;
    SliderAttachment downAttackAttachment;
    SliderAttachment downReleaseAttachment;
    SliderAttachment upAttackAttachment;
    SliderAttachment upReleaseAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhuCompressorAudioProcessorEditor)
};
