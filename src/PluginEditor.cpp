#include "PluginEditor.h"
#include "PluginProcessor.h"

// ── Editor layout / style constants ──────────────────────────────────────────
static constexpr int   kEditorWidth          = 800;
static constexpr int   kTimerHz              = 60;
static constexpr int   kTitleHeight          = 28;
static constexpr float kTitleFontSize        = 14.0f;
static constexpr float kLabelFontSize        = 11.0f;
static constexpr float kInfoLabelFontSize    =  9.0f;
static constexpr int   kSliderTextBoxWidth   = 50;
static constexpr int   kSliderTextBoxHeight  = 20;
// ── Layout geometry shared by resized() and height helpers ───────────────────
static constexpr int   kReduceMargin  = 10;
static constexpr int   kRowHeight     = 24;
static constexpr int   kRowGap        =  3;
static constexpr int   kGroupPaddingV = 18;
static constexpr int   kGroupPaddingH = 10;
static constexpr int   kGroupSpacing  =  8;
static constexpr int   kLabelWidth    = 80;
static constexpr int   kSliderColWidth = 250;

static constexpr int kDownwardGroupRows = 5;
static constexpr int kUpwardBaseRows = 6;
static constexpr int kUpwardSnapRows = 1;
static constexpr int kDetectorRmsBaseRows = 4;
static constexpr int kDetectorPeakBaseRows = 2;
static constexpr int kDetectorLookaheadBaseRows = 1;
static constexpr int kDetectorLookaheadExpandedRows = 2;

static constexpr int groupHeight(int n) noexcept {
    return 2 * kGroupPaddingV + n * kRowHeight + (n > 1 ? (n - 1) * kRowGap : 0);
}

static void setupSlider(juce::Slider& slider, juce::Label& label, const juce::String& text,
                        juce::Component* parent) {
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centredLeft);
    label.setFont(juce::FontOptions(kLabelFontSize));
    parent->addAndMakeVisible(label);

    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, kSliderTextBoxWidth, kSliderTextBoxHeight);
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
      upSnapReleaseEnabledAttachment(p.getAPVTS(),
                                     PhuCompressorAudioProcessor::kParamUpSnapReleaseEnabled,
                                     upSnapReleaseEnabledToggle),
      upSnapReleaseAttachment(p.getAPVTS(), PhuCompressorAudioProcessor::kParamUpSnapRelease,
                              upSnapReleaseSlider),
      detectorTypeAttachment(p.getAPVTS(), PhuCompressorAudioProcessor::kParamDetectorType,
                             detectorTypeCombo),
      rmsWindowAttachment(p.getAPVTS(), PhuCompressorAudioProcessor::kParamRmsWindowMs,
                          rmsWindowSlider),
      rmsSyncAttachment(p.getAPVTS(), PhuCompressorAudioProcessor::kParamRmsSyncMode,
                        rmsSyncToggle),
      rmsBeatDivAttachment(p.getAPVTS(), PhuCompressorAudioProcessor::kParamRmsBeatDiv,
                           rmsBeatDivCombo),
        peakWindowAttachment(p.getAPVTS(), PhuCompressorAudioProcessor::kParamPeakWindowMs,
                       peakWindowSlider),
        lookaheadEnabledAttachment(p.getAPVTS(),
                           PhuCompressorAudioProcessor::kParamLookaheadEnabled,
                           lookaheadEnabledToggle),
        lookaheadMsAttachment(p.getAPVTS(), PhuCompressorAudioProcessor::kParamLookaheadMs,
                        lookaheadMsSlider) {
    setupSlider(downThreshSlider, downThreshLabel, "Thresh (dB)", this);
    setupSlider(downRatioSlider, downRatioLabel, "Ratio", this);
    setupSlider(downAttackSlider, downAttackLabel, "Attack (ms)", this);
    setupSlider(downReleaseSlider, downReleaseLabel, "Release (ms)", this);
    setupSlider(upThreshSlider, upThreshLabel, "Thresh (dB)", this);
    setupSlider(upRatioSlider, upRatioLabel, "Ratio", this);
    setupSlider(upAttackSlider, upAttackLabel, "Attack (ms)", this);
    setupSlider(upReleaseSlider, upReleaseLabel, "Release (ms)", this);

    // Snap release toggle
    upSnapReleaseEnabledToggle.setButtonText("Snap Rel");
    addAndMakeVisible(upSnapReleaseEnabledToggle);

    // Snap release slider
    setupSlider(upSnapReleaseSlider, upSnapReleaseLabel, "Snap Rel (ms)", this);

    upSnapReleaseEnabledToggle.onClick = [this]() {
        const bool snapOn = upSnapReleaseEnabledToggle.getToggleState();
        upSnapReleaseSlider.setVisible(snapOn);
        upSnapReleaseLabel.setVisible(snapOn);
        resized();
    };
    // Initial visibility: match the loaded parameter state
    {
        const bool snapOn = upSnapReleaseEnabledToggle.getToggleState();
        upSnapReleaseSlider.setVisible(snapOn);
        upSnapReleaseLabel.setVisible(snapOn);
    }

    // Detector type combo
    detectorTypeLabel.setText("Type", juce::dontSendNotification);
    detectorTypeLabel.setJustificationType(juce::Justification::centredLeft);
    detectorTypeLabel.setFont(juce::FontOptions(kLabelFontSize));
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
    rmsBeatDivLabel.setFont(juce::FontOptions(kLabelFontSize));
    addAndMakeVisible(rmsBeatDivLabel);
    for (int i = 0; i < kDetectorNumDivisions; ++i)
        rmsBeatDivCombo.addItem(kDetectorBeatLabels[i], i + 1);
    addAndMakeVisible(rmsBeatDivCombo);

    // Re-sync combo selections: attachments fired before items were populated,
    // so the initial sync had no effect. Restore correct display now.
    detectorTypeCombo.setSelectedItemIndex(
        static_cast<int>(p.getAPVTS().getRawParameterValue(
            PhuCompressorAudioProcessor::kParamDetectorType)->load()),
        juce::dontSendNotification);
    rmsBeatDivCombo.setSelectedItemIndex(
        static_cast<int>(p.getAPVTS().getRawParameterValue(
            PhuCompressorAudioProcessor::kParamRmsBeatDiv)->load()),
        juce::dontSendNotification);

    // Peak window slider
    setupSlider(peakWindowSlider, peakWindowLabel, "Window (ms)", this);

    // Lookahead controls (detector/timing domain)
    lookaheadEnabledToggle.setButtonText("Lookahead");
    addAndMakeVisible(lookaheadEnabledToggle);
    setupSlider(lookaheadMsSlider, lookaheadMsLabel, "Ahead (ms)", this);

    lookaheadLatencyLabel.setJustificationType(juce::Justification::centredLeft);
    lookaheadLatencyLabel.setFont(juce::FontOptions(kInfoLabelFontSize));
    lookaheadLatencyLabel.setColour(juce::Label::textColourId,
                                    juce::Colours::white.withAlpha(0.6f));
    addAndMakeVisible(lookaheadLatencyLabel);

    {
        const bool lookaheadOn = lookaheadEnabledToggle.getToggleState();
        lookaheadMsSlider.setVisible(lookaheadOn);
        lookaheadMsLabel.setVisible(lookaheadOn);
        lookaheadLatencyLabel.setVisible(lookaheadOn);
    }

    lookaheadEnabledToggle.onClick = [this]() {
        const bool lookaheadOn = lookaheadEnabledToggle.getToggleState();
        lookaheadMsSlider.setVisible(lookaheadOn);
        lookaheadMsLabel.setVisible(lookaheadOn);
        lookaheadLatencyLabel.setVisible(lookaheadOn);
        resized();
    };

    // RMS info readout label
    rmsInfoLabel.setJustificationType(juce::Justification::centred);
    rmsInfoLabel.setFont(juce::FontOptions(kInfoLabelFontSize));
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

    setSize(kEditorWidth, computeRequiredEditorHeight());
    startTimerHz(kTimerHz);
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
        rmsInfoLabel.isVisible()    != isRms                ||
        lookaheadMsSlider.isVisible()!= lookaheadEnabledToggle.getToggleState() ||
        lookaheadLatencyLabel.isVisible()!= lookaheadEnabledToggle.getToggleState();

    rmsWindowSlider.setVisible(isRms && !isSynced);
    rmsWindowLabel.setVisible(isRms && !isSynced);
    rmsSyncToggle.setVisible(isRms);
    rmsBeatDivCombo.setVisible(isRms && isSynced);
    rmsBeatDivLabel.setVisible(isRms && isSynced);
    peakWindowSlider.setVisible(!isRms);
    peakWindowLabel.setVisible(!isRms);
    rmsInfoLabel.setVisible(isRms);

    const bool lookaheadOn = lookaheadEnabledToggle.getToggleState();
    lookaheadMsSlider.setVisible(lookaheadOn);
    lookaheadMsLabel.setVisible(lookaheadOn);
    lookaheadLatencyLabel.setVisible(lookaheadOn);

    if (visChanged)
        resized();
}

void PhuCompressorAudioProcessorEditor::timerCallback() {
    // Update sample rate and BPM
    auto& syncGlobals = audioProcessor.getSyncGlobals();
    compressorDisplay.setSampleRate(syncGlobals.getSampleRate() > 0
                                        ? syncGlobals.getSampleRate()
                                        : kFallbackSampleRate);
    const double newBpm = syncGlobals.getBPM();
    if (phu::compressor::bpmChangeExceedsTolerance(newBpm, m_lastBpm)) {
        m_lastBpm = newBpm;
        compressorDisplay.setBPM(newBpm);
    }

    // Always update PPQ — required by both the beat-sync positional and scrolling temporal renderers.
    compressorDisplay.setCurrentPpq(syncGlobals.getPpqEndOfBlock());

    // Keep processor display range in sync with the time selector when beat-sync is active.
    if (compressorDisplay.isBeatSyncMode())
        audioProcessor.setDisplayRangeBeats(compressorDisplay.getDisplayRangeBeats());

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

    // Live lookahead latency readout
    if (lookaheadEnabledToggle.getToggleState()) {
        const int latSamples = audioProcessor.getLatencySamples();
        const double sr = audioProcessor.getSyncGlobals().getSampleRate();
        const float latMs = (sr > 0.0) ? static_cast<float>(latSamples / sr * 1000.0) : 0.0f;
        lookaheadLatencyLabel.setText("Latency: " + juce::String(latMs, 1) + " ms",
                                      juce::dontSendNotification);
    }
}

// ── Group-height helpers ──────────────────────────────────────────────────────

int PhuCompressorAudioProcessorEditor::computeDownwardGroupHeight() const {
    return groupHeight(kDownwardGroupRows);
}

int PhuCompressorAudioProcessorEditor::computeUpwardGroupHeight() const {
    int rows = kUpwardBaseRows;
    if (upSnapReleaseEnabledToggle.getToggleState()) rows += kUpwardSnapRows;
    return groupHeight(rows);
}

int PhuCompressorAudioProcessorEditor::computeDetectorGroupHeight() const {
    const bool isRms = detectorTypeCombo.getSelectedItemIndex() == 0;
    int rows = isRms ? kDetectorRmsBaseRows : kDetectorPeakBaseRows;
    rows += kDetectorLookaheadBaseRows;
    if (lookaheadEnabledToggle.getToggleState()) rows += kDetectorLookaheadExpandedRows;
    return groupHeight(rows);
}

int PhuCompressorAudioProcessorEditor::computeRequiredEditorHeight() const {
    return 2 * kReduceMargin + kTitleHeight
        + computeDownwardGroupHeight() + kGroupSpacing
        + computeUpwardGroupHeight()   + kGroupSpacing
        + computeDetectorGroupHeight() + kGroupSpacing
        + kRowHeight + kRowGap   // GR curve toggles row
        + kRowHeight;            // Beat Sync toggle
}

void PhuCompressorAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(kTitleFontSize).withStyle("Bold"));
    g.drawText("PHU COMPRESSOR", getLocalBounds().removeFromTop(kTitleHeight),
               juce::Justification::centred, true);
}

void PhuCompressorAudioProcessorEditor::resized() {
    if (const int req = computeRequiredEditorHeight(); getHeight() != req)
        { setSize(kEditorWidth, req); return; }

    auto area = getLocalBounds().reduced(kReduceMargin);
    area.removeFromTop(kTitleHeight);

    auto sliderArea = area.removeFromLeft(kSliderColWidth);

    // ── Downward group (4 sliders + detector toggle = 5 rows) ───────────────
    auto downGroupArea = sliderArea.removeFromTop(computeDownwardGroupHeight());
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

    // ── Upward group: thresh + ratio + attack + release + snapToggle + [snapSlider] + detectorToggle ──
    const bool snapEnabled = upSnapReleaseEnabledToggle.getToggleState();
    auto upGroupArea = sliderArea.removeFromTop(computeUpwardGroupHeight());
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
        // Snap release toggle (always visible inside the group)
        auto snapToggleRow = content.removeFromTop(kRowHeight);
        upSnapReleaseEnabledToggle.setBounds(snapToggleRow);
        content.removeFromTop(kRowGap);
        // Snap release slider (visible only when enabled)
        if (snapEnabled)
            layoutRow(upSnapReleaseLabel, upSnapReleaseSlider);
        // Detector level toggle
        auto toggleRow = content.removeFromTop(kRowHeight);
        showUpDetectorToggle.setBounds(toggleRow);
    }
    sliderArea.removeFromTop(kGroupSpacing);

    // ── Detector group (dynamic rows based on selected type) ────────────
    const int detType   = detectorTypeCombo.getSelectedItemIndex();
    const bool isRms    = (detType == 0);
    const bool isSynced = rmsSyncToggle.getToggleState();
    const bool lookaheadOn = lookaheadEnabledToggle.getToggleState();
    auto detGroupArea   = sliderArea.removeFromTop(computeDetectorGroupHeight());
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
        auto lookaheadToggleRow = content.removeFromTop(kRowHeight);
        lookaheadEnabledToggle.setBounds(lookaheadToggleRow);
        content.removeFromTop(kRowGap);
        if (lookaheadOn) {
            layoutSliderRow(lookaheadMsLabel, lookaheadMsSlider);
            auto latRow = content.removeFromTop(kRowHeight);
            lookaheadLatencyLabel.setBounds(latRow);
            content.removeFromTop(kRowGap);
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
