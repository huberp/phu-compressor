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
    for (int i = 0; i < kDetectorNumDivisions; ++i)
        rmsBeatDivCombo.addItem(kDetectorBeatLabels[i], i + 1);
    addAndMakeVisible(rmsBeatDivCombo);

    // Peak window slider
    setupSlider(peakWindowSlider, peakWindowLabel, "Window (ms)", this);

    // RMS info readout label
    rmsInfoLabel.setJustificationType(juce::Justification::centred);
    rmsInfoLabel.setFont(juce::FontOptions(9.0f));
    rmsInfoLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.6f));
    addAndMakeVisible(rmsInfoLabel);

    // Per-stage detector level line toggles
    showDownDetectorToggle.setButtonText("Level");
    showDownDetectorToggle.setToggleState(true, juce::dontSendNotification);
    showDownDetectorToggle.setColour(juce::ToggleButton::textColourId,
                                     juce::Colour{0xFFFFE8D0u});
    showDownDetectorToggle.onClick = [this]() {
        compressorDisplay.setShowDownDetectorCurve(showDownDetectorToggle.getToggleState());
    };
    addAndMakeVisible(showDownDetectorToggle);

    showUpDetectorToggle.setButtonText("Level");
    showUpDetectorToggle.setToggleState(true, juce::dontSendNotification);
    showUpDetectorToggle.setColour(juce::ToggleButton::textColourId,
                                   juce::Colour{0xFFEEFFFFu});
    showUpDetectorToggle.onClick = [this]() {
        compressorDisplay.setShowUpDetectorCurve(showUpDetectorToggle.getToggleState());
    };
    addAndMakeVisible(showUpDetectorToggle);

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
                                          p.getUpGRSyncBuffer());

    updateDetectorControlVisibility();

    setSize(800, 644);
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
        peakWindowSlider.isVisible()!= (!isRms)             ||
        rmsInfoLabel.isVisible()    != isRms;

    rmsWindowSlider.setVisible(isRms && !isSynced);
    rmsWindowLabel.setVisible(isRms && !isSynced);
    rmsSyncToggle.setVisible(isRms);
    rmsBeatDivCombo.setVisible(isRms && isSynced);
    rmsBeatDivLabel.setVisible(isRms && isSynced);
    peakWindowSlider.setVisible(!isRms);
    peakWindowLabel.setVisible(!isRms);
    rmsInfoLabel.setVisible(isRms);

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
                                       audioProcessor.getDetectorPacketFifo(),
                                       audioProcessor.getDownDetectorPacketFifo());
    compressorDisplay.repaint();

    // Update detector control visibility based on current parameter values
    updateDetectorControlVisibility();

    // Live detector info readout
    if (rmsInfoLabel.isVisible()) {
        const auto info = audioProcessor.getDetectorInfo();
        auto modeStr = [](DetectorMode m) -> const char* {
            return m == DetectorMode::RMS ? "RMS" : "Peak";
        };
        rmsInfoLabel.setText(
            juce::String(info.downMs, 1) + "/" + modeStr(info.downMode)
            + " | "
            + juce::String(info.upMs, 1) + "/" + modeStr(info.upMode),
            juce::dontSendNotification);
    }
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

    // ── Downward group (4 sliders + detector toggle = 5 rows) ───────────────
    auto downGroupArea = sliderArea.removeFromTop(computeGroupHeight(5));
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
        // Detector level toggle
        auto toggleRow = content.removeFromTop(kRowHeight);
        showDownDetectorToggle.setBounds(toggleRow);
    }
    sliderArea.removeFromTop(kGroupSpacing);

    // ── Upward group (4 sliders + detector toggle = 5 rows) ─────────────────
    auto upGroupArea = sliderArea.removeFromTop(computeGroupHeight(5));
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
        // Detector level toggle
        auto toggleRow = content.removeFromTop(kRowHeight);
        showUpDetectorToggle.setBounds(toggleRow);
    }
    sliderArea.removeFromTop(kGroupSpacing);

    // ── Detector group (dynamic rows based on selected type) ────────────
    const int detType  = detectorTypeCombo.getSelectedItemIndex(); // 0=RMS, 1=Peak
    const bool isRms   = (detType == 0);
    const bool isSynced = rmsSyncToggle.getToggleState();
    // Peak: Type + Window = 2 rows
    // RMS unsynced: Type + Window + Sync = 3 rows
    // RMS synced:   Type + Sync + Beat Div = 3 rows
    // RMS (any):    +1 row for info label below beat div / sync toggle
    const int detRows  = isRms ? 4 : 2;

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
        if (isRms) {
            auto row = content.removeFromTop(kRowHeight);
            rmsInfoLabel.setBounds(row);
        }
    }
    sliderArea.removeFromTop(kGroupSpacing);

    // ── Curve visibility toggles (Down GR + Up GR) ───────────────────────────
    {
        auto toggleRow = sliderArea.removeFromTop(kRowHeight);
        int toggleWidth = toggleRow.getWidth() / 2;
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
