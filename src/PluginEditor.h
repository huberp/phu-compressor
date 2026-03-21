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
    void updateDetectorControlVisibility();

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

    // Detector controls
    juce::ComboBox detectorTypeCombo;
    juce::Label detectorTypeLabel;
    juce::Slider rmsWindowSlider;
    juce::Label rmsWindowLabel;
    juce::ToggleButton rmsSyncToggle;
    juce::ComboBox rmsBeatDivCombo;
    juce::Label rmsBeatDivLabel;
    juce::Slider peakWindowSlider;
    juce::Label peakWindowLabel;

    // Curve visibility toggles
    juce::ToggleButton showDetectorToggle;
    juce::ToggleButton showDownGrToggle;
    juce::ToggleButton showUpGrToggle;

    // Section group components (styled frames with titles)
    juce::GroupComponent downwardGroup;
    juce::GroupComponent upwardGroup;
    juce::GroupComponent detectorGroup;

    // APVTS attachments
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    SliderAttachment downThreshAttachment;
    SliderAttachment downRatioAttachment;
    SliderAttachment upThreshAttachment;
    SliderAttachment upRatioAttachment;
    SliderAttachment downAttackAttachment;
    SliderAttachment downReleaseAttachment;
    SliderAttachment upAttackAttachment;
    SliderAttachment upReleaseAttachment;

    ComboBoxAttachment detectorTypeAttachment;
    SliderAttachment rmsWindowAttachment;
    ButtonAttachment rmsSyncAttachment;
    ComboBoxAttachment rmsBeatDivAttachment;
    SliderAttachment peakWindowAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhuCompressorAudioProcessorEditor)
};
