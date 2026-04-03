#include "PluginEditor.h"
#include "PluginProcessor.h"

static void setupSlider(juce::Slider& slider, juce::Label& label, const juce::String& text,
                        juce::Component* parent) {
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centredLeft);
    label.setFont(juce::FontOptions(11.0f));
    parent->addAndMakeVisible(label);

    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
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
                          upReleaseSlider),
      detectorTypeAttachment(p.getAPVTS(), PhuCompressorAudioProcessor::kParamDetectorType,
                             detectorTypeCombo),
      rmsWindowAttachment(p.getAPVTS(), PhuCompressorAudioProcessor::kParamRmsWindowMs,
                          rmsWindowSlider),
      rmsSyncAttachment(p.getAPVTS(), PhuCompressorAudioProcessor::kParamRmsSyncMode,
                        rmsSyncToggle),
      rmsBeatDivAttachment(p.getAPVTS(), PhuCompressorAudioProcessor::kParamRmsBeatDiv,
                           rmsBeatDivCombo),
      peakWindowAttachment(p.getAPVTS(), PhuCompressorAudioProcessor::kParamPeakWindowMs,
                           peakWindowSlider) {
    setupSlider(downThreshSlider, downThreshLabel, "Thresh (dB)", this);
    setupSlider(downRatioSlider, downRatioLabel, "Ratio", this);
    setupSlider(downAttackSlider, downAttackLabel, "Attack (ms)", this);
    setupSlider(downReleaseSlider, downReleaseLabel, "Release (ms)", this);
    setupSlider(upThreshSlider, upThreshLabel, "Thresh (dB)", this);
    setupSlider(upRatioSlider, upRatioLabel, "Ratio", this);
    setupSlider(upAttackSlider, upAttackLabel, "Attack (ms)", this);
    setupSlider(upReleaseSlider, upReleaseLabel, "Release (ms)", this);

    // Detector type combo
    detectorTypeLabel.setText("Type", juce::dontSendNotification);
    detectorTypeLabel.setJustificationType(juce::Justification::centredLeft);
    detectorTypeLabel.setFont(juce::FontOptions(11.0f));
    addAndMakeVisible(detectorTypeLabel);
    detectorTypeCombo.addItem("RMS", 1);
    detectorTypeCombo.addItem("Peak", 2);
    addAndMakeVisible(detectorTypeCombo);

    // RMS window slider
    setupSlider(rmsWindowSlider, rmsWindowLabel, "Window (ms)", this);

    // RMS sync toggle
    rmsSyncToggle.setButtonText("Sync");
    addAndMakeVisible(rmsSyncToggle);

    // RMS beat division combo
    rmsBeatDivLabel.setText("Beat Div", juce::dontSendNotification);
    rmsBeatDivLabel.setJustificationType(juce::Justification::centredLeft);
    rmsBeatDivLabel.setFont(juce::FontOptions(11.0f));
    addAndMakeVisible(rmsBeatDivLabel);
    rmsBeatDivCombo.addItem("1/32", 1);
    rmsBeatDivCombo.addItem("1/16", 2);
    rmsBeatDivCombo.addItem("1/8", 3);
    rmsBeatDivCombo.addItem("1/4", 4);
    rmsBeatDivCombo.addItem("1/2", 5);
    rmsBeatDivCombo.addItem("1", 6);
    rmsBeatDivCombo.addItem("2", 7);
    rmsBeatDivCombo.addItem("4", 8);
    addAndMakeVisible(rmsBeatDivCombo);

    // Peak window slider
    setupSlider(peakWindowSlider, peakWindowLabel, "Window (ms)", this);

    // Curve visibility toggles
    showDetectorToggle.setButtonText("Detector");
    showDetectorToggle.setToggleState(true, juce::dontSendNotification);
    showDetectorToggle.onClick = [this]() {
        compressorDisplay.setShowDetectorCurve(showDetectorToggle.getToggleState());
    };
    addAndMakeVisible(showDetectorToggle);

    showDownGrToggle.setButtonText("Down GR");
    showDownGrToggle.setToggleState(true, juce::dontSendNotification);
    showDownGrToggle.onClick = [this]() {
        compressorDisplay.setShowDownGr(showDownGrToggle.getToggleState());
    };
    addAndMakeVisible(showDownGrToggle);

    showUpGrToggle.setButtonText("Up GR");
    showUpGrToggle.setToggleState(true, juce::dontSendNotification);
    showUpGrToggle.onClick = [this]() {
        compressorDisplay.setShowUpGr(showUpGrToggle.getToggleState());
    };
    addAndMakeVisible(showUpGrToggle);

    beatSyncToggle.setButtonText("Beat Sync");
    beatSyncToggle.setToggleState(false, juce::dontSendNotification);
    beatSyncToggle.onClick = [this]() {
        compressorDisplay.setBeatSyncMode(beatSyncToggle.getToggleState());
    };
    addAndMakeVisible(beatSyncToggle);

    // Section group frames (like phu-splitter style)
    downwardGroup.setText("Downward");
    downwardGroup.setTextLabelPosition(juce::Justification::centredLeft);
    addAndMakeVisible(downwardGroup);

    upwardGroup.setText("Upward");
    upwardGroup.setTextLabelPosition(juce::Justification::centredLeft);
    addAndMakeVisible(upwardGroup);

    detectorGroup.setText("Detector");
    detectorGroup.setTextLabelPosition(juce::Justification::centredLeft);
    addAndMakeVisible(detectorGroup);

    addAndMakeVisible(compressorDisplay);

    // Wire beat-sync buffer pointers
    compressorDisplay.setBeatSyncBuffers(p.getInputSyncBuffer(),
                                          p.getGRSyncBuffer(),
                                          p.getUpGRSyncBuffer(),
                                          p.getDetectorSyncBuffer());

    updateDetectorControlVisibility();

    setSize(800, 590);
    startTimerHz(60);
}

PhuCompressorAudioProcessorEditor::~PhuCompressorAudioProcessorEditor() {
    stopTimer();
}

void PhuCompressorAudioProcessorEditor::updateDetectorControlVisibility() {
    const int detType = detectorTypeCombo.getSelectedItemIndex(); // 0=RMS, 1=Peak
    const bool isRms = (detType == 0);
    const bool isSynced = rmsSyncToggle.getToggleState();

    const bool visChanged =
        rmsWindowSlider.isVisible() != (isRms && !isSynced) ||
        rmsSyncToggle.isVisible()   != isRms                ||
        rmsBeatDivCombo.isVisible() != (isRms && isSynced)  ||
        peakWindowSlider.isVisible()!= (!isRms);

    rmsWindowSlider.setVisible(isRms && !isSynced);
    rmsWindowLabel.setVisible(isRms && !isSynced);
    rmsSyncToggle.setVisible(isRms);
    rmsBeatDivCombo.setVisible(isRms && isSynced);
    rmsBeatDivLabel.setVisible(isRms && isSynced);
    peakWindowSlider.setVisible(!isRms);
    peakWindowLabel.setVisible(!isRms);

    if (visChanged)
        resized();
}

void PhuCompressorAudioProcessorEditor::timerCallback() {
    // Update sample rate and BPM
    auto& syncGlobals = audioProcessor.getSyncGlobals();
    compressorDisplay.setSampleRate(syncGlobals.getSampleRate() > 0
                                        ? syncGlobals.getSampleRate()
                                        : 48000.0);
    compressorDisplay.setBPM(syncGlobals.getBPM());

    // Beat-sync: pass PPQ and keep display range in sync with processor
    if (compressorDisplay.isBeatSyncMode()) {
        compressorDisplay.setCurrentPpq(syncGlobals.getPpqEndOfBlock());
        audioProcessor.setDisplayRangeBeats(compressorDisplay.getDisplayRangeBeats());
    }

    // Pull data from FIFOs and repaint
    compressorDisplay.updateFromFifos(audioProcessor.getInputFifo(),
                                       audioProcessor.getGainReductionFifo(),
                                       audioProcessor.getUpGainReductionFifo(),
                                       audioProcessor.getDetectorFifo());
    compressorDisplay.repaint();

    // Update detector control visibility based on current parameter values
    updateDetectorControlVisibility();
}

void PhuCompressorAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(14.0f).withStyle("Bold"));
    g.drawText("PHU COMPRESSOR", getLocalBounds().removeFromTop(28),
               juce::Justification::centred, true);
}

void PhuCompressorAudioProcessorEditor::resized() {
    auto area = getLocalBounds().reduced(10);
    area.removeFromTop(28); // Title

    // ── Layout constants (aligned with phu-splitter style) ────────────────
    constexpr int kRowHeight = 24;
    constexpr int kRowGap = 3;
    constexpr int kGroupPaddingV = 18;
    constexpr int kGroupPaddingH = 10;
    constexpr int kGroupSpacing = 8;
    constexpr int kLabelWidth = 80;

    const int sliderColumnWidth = 250;
    auto sliderArea = area.removeFromLeft(sliderColumnWidth);

    // Helper: compute group height for N rows of controls
    auto computeGroupHeight = [&](int numRows) {
        int contentHeight = numRows * kRowHeight + (numRows > 1 ? (numRows - 1) * kRowGap : 0);
        return 2 * kGroupPaddingV + contentHeight;
    };

    // ── Downward group (4 rows) ──────────────────────────────────────────
    auto downGroupArea = sliderArea.removeFromTop(computeGroupHeight(4));
    downwardGroup.setBounds(downGroupArea);
    {
        auto content = downGroupArea.reduced(kGroupPaddingH, kGroupPaddingV);
        auto layoutRow = [&](juce::Label& label, juce::Slider& slider) {
            auto row = content.removeFromTop(kRowHeight);
            label.setBounds(row.removeFromLeft(kLabelWidth));
            slider.setBounds(row);
            content.removeFromTop(kRowGap);
        };
        layoutRow(downThreshLabel, downThreshSlider);
        layoutRow(downRatioLabel, downRatioSlider);
        layoutRow(downAttackLabel, downAttackSlider);
        layoutRow(downReleaseLabel, downReleaseSlider);
    }
    sliderArea.removeFromTop(kGroupSpacing);

    // ── Upward group (4 rows) ────────────────────────────────────────────
    auto upGroupArea = sliderArea.removeFromTop(computeGroupHeight(4));
    upwardGroup.setBounds(upGroupArea);
    {
        auto content = upGroupArea.reduced(kGroupPaddingH, kGroupPaddingV);
        auto layoutRow = [&](juce::Label& label, juce::Slider& slider) {
            auto row = content.removeFromTop(kRowHeight);
            label.setBounds(row.removeFromLeft(kLabelWidth));
            slider.setBounds(row);
            content.removeFromTop(kRowGap);
        };
        layoutRow(upThreshLabel, upThreshSlider);
        layoutRow(upRatioLabel, upRatioSlider);
        layoutRow(upAttackLabel, upAttackSlider);
        layoutRow(upReleaseLabel, upReleaseSlider);
    }
    sliderArea.removeFromTop(kGroupSpacing);

    // ── Detector group (dynamic rows based on selected type) ────────────
    const int detType  = detectorTypeCombo.getSelectedItemIndex(); // 0=RMS, 1=Peak
    const bool isRms   = (detType == 0);
    const bool isSynced = rmsSyncToggle.getToggleState();
    // Peak: Type + Window = 2 rows
    // RMS unsynced: Type + Window + Sync = 3 rows
    // RMS synced:   Type + Sync + Beat Div = 3 rows
    const int detRows  = isRms ? 3 : 2;

    auto detGroupArea = sliderArea.removeFromTop(computeGroupHeight(detRows));
    detectorGroup.setBounds(detGroupArea);
    {
        auto content = detGroupArea.reduced(kGroupPaddingH, kGroupPaddingV);
        auto layoutSliderRow = [&](juce::Label& label, juce::Slider& slider) {
            auto row = content.removeFromTop(kRowHeight);
            label.setBounds(row.removeFromLeft(kLabelWidth));
            slider.setBounds(row);
            content.removeFromTop(kRowGap);
        };
        auto layoutComboRow = [&](juce::Label& label, juce::ComboBox& combo) {
            auto row = content.removeFromTop(kRowHeight);
            label.setBounds(row.removeFromLeft(kLabelWidth));
            combo.setBounds(row);
            content.removeFromTop(kRowGap);
        };
        layoutComboRow(detectorTypeLabel, detectorTypeCombo);
        if (isRms && !isSynced)
            layoutSliderRow(rmsWindowLabel, rmsWindowSlider);
        if (isRms) {
            auto row = content.removeFromTop(kRowHeight);
            rmsSyncToggle.setBounds(row.removeFromLeft(kLabelWidth));
            content.removeFromTop(kRowGap);
        }
        if (isRms && isSynced)
            layoutComboRow(rmsBeatDivLabel, rmsBeatDivCombo);
        if (!isRms)
            layoutSliderRow(peakWindowLabel, peakWindowSlider);
    }
    sliderArea.removeFromTop(kGroupSpacing);

    // ── Curve visibility toggles ─────────────────────────────────────────
    {
        auto toggleRow = sliderArea.removeFromTop(kRowHeight);
        int toggleWidth = toggleRow.getWidth() / 3;
        showDetectorToggle.setBounds(toggleRow.removeFromLeft(toggleWidth));
        showDownGrToggle.setBounds(toggleRow.removeFromLeft(toggleWidth));
        showUpGrToggle.setBounds(toggleRow);
    }
    sliderArea.removeFromTop(kRowGap);

    // ── Beat sync toggle ─────────────────────────────────────────────────
    {
        auto toggleRow = sliderArea.removeFromTop(kRowHeight);
        beatSyncToggle.setBounds(toggleRow);
    }

    // ── Display panel fills remaining space ──────────────────────────────
    auto displayArea = area.withTrimmedLeft(8);
    compressorDisplay.setBounds(displayArea);
}
