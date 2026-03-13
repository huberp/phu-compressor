#pragma once

#include "PluginProcessor.h"
#include <juce_audio_processors/juce_audio_processors.h>

class PhuCompressorAudioProcessorEditor : public juce::AudioProcessorEditor {
  public:
    PhuCompressorAudioProcessorEditor(PhuCompressorAudioProcessor&);
    ~PhuCompressorAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

  private:
    PhuCompressorAudioProcessor& audioProcessor;

    // Parameter sliders
    juce::Slider downThreshSlider;
    juce::Slider downRatioSlider;
    juce::Slider upThreshSlider;
    juce::Slider upRatioSlider;
    juce::Slider attackSlider;
    juce::Slider releaseSlider;

    // Labels
    juce::Label downThreshLabel;
    juce::Label downRatioLabel;
    juce::Label upThreshLabel;
    juce::Label upRatioLabel;
    juce::Label attackLabel;
    juce::Label releaseLabel;

    // APVTS attachments
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    SliderAttachment downThreshAttachment;
    SliderAttachment downRatioAttachment;
    SliderAttachment upThreshAttachment;
    SliderAttachment upRatioAttachment;
    SliderAttachment attackAttachment;
    SliderAttachment releaseAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhuCompressorAudioProcessorEditor)
};
