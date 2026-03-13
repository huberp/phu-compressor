#include "CompressorDisplay.h"
#include <cmath>

// Parameter IDs (must match PluginProcessor)
static constexpr const char* kParamDownThresh = "down_thresh";
static constexpr const char* kParamDownRatio = "down_ratio";
static constexpr const char* kParamUpThresh = "up_thresh";
static constexpr const char* kParamUpRatio = "up_ratio";

// Colours
static const juce::Colour kBgColour{0xFF1A1A1Au};
static const juce::Colour kGridColour{juce::Colours::white.withAlpha(0.12f)};
static const juce::Colour kGridTextColour{juce::Colours::white.withAlpha(0.4f)};
static const juce::Colour kTransferCurveColour{0xFF00CCFFu};   // Cyan
static const juce::Colour kUnityLineColour{juce::Colours::white.withAlpha(0.2f)};
static const juce::Colour kWaveformColour{0xFF00AADDu};        // Blue
static const juce::Colour kGrFillColour{0xFFFF8800u};          // Orange/amber (attenuation)
static const juce::Colour kGrLineColour{0xFFFFAA33u};          // Lighter orange
static const juce::Colour kBoostFillColour{0xFF44CC44u};       // Green (upward boost)
static const juce::Colour kBoostLineColour{0xFF66EE66u};       // Lighter green
static const juce::Colour kDownThreshHandleColour{0xFFFF4444u}; // Red
static const juce::Colour kDownRatioHandleColour{0xFFFF8888u};  // Light red
static const juce::Colour kUpThreshHandleColour{0xFF44FF44u};   // Green
static const juce::Colour kUpRatioHandleColour{0xFF88FF88u};    // Light green

static constexpr float kHandleRadius = 6.0f;
static constexpr float kHandleHitRadius = 10.0f;

// ─────────────────────────────────────────────────────────────────────────
// Construction / Destruction
// ─────────────────────────────────────────────────────────────────────────

CompressorDisplay::CompressorDisplay(juce::AudioProcessorValueTreeState& apvtsRef)
    : apvts(apvtsRef) {
    setOpaque(true);

    // Listen to parameter changes
    apvts.addParameterListener(kParamDownThresh, this);
    apvts.addParameterListener(kParamDownRatio, this);
    apvts.addParameterListener(kParamUpThresh, this);
    apvts.addParameterListener(kParamUpRatio, this);

    // Read initial values
    if (auto* p = apvts.getRawParameterValue(kParamDownThresh))
        downThreshDb = p->load();
    if (auto* p = apvts.getRawParameterValue(kParamDownRatio))
        downRatio = p->load();
    if (auto* p = apvts.getRawParameterValue(kParamUpThresh))
        upThreshDb = p->load();
    if (auto* p = apvts.getRawParameterValue(kParamUpRatio))
        upRatio = p->load();

    // Time selector buttons
    for (int i = 0; i < kNumTimeOptions; ++i) {
        timeButtons[static_cast<size_t>(i)].setButtonText(kBeatLabels[i]);
        timeButtons[static_cast<size_t>(i)].setClickingTogglesState(true);
        timeButtons[static_cast<size_t>(i)].setRadioGroupId(1001);
        timeButtons[static_cast<size_t>(i)].setToggleState(
            i == selectedTimeIndex, juce::dontSendNotification);
        timeButtons[static_cast<size_t>(i)].onClick = [this, i]() {
            selectedTimeIndex = i;
            updateDisplayDurationFromBPM();
        };
        addAndMakeVisible(timeButtons[static_cast<size_t>(i)]);
    }
}

CompressorDisplay::~CompressorDisplay() {
    apvts.removeParameterListener(kParamDownThresh, this);
    apvts.removeParameterListener(kParamDownRatio, this);
    apvts.removeParameterListener(kParamUpThresh, this);
    apvts.removeParameterListener(kParamUpRatio, this);
}

// ─────────────────────────────────────────────────────────────────────────
// Configuration
// ─────────────────────────────────────────────────────────────────────────

void CompressorDisplay::setSampleRate(double sr) {
    if (sr > 0.0)
        sampleRate = sr;
}

void CompressorDisplay::setBPM(double bpm) {
    currentBPM = bpm;
    updateDisplayDurationFromBPM();
}

void CompressorDisplay::setDisplayDuration(float durationMs) {
    displayDurationMs = juce::jlimit(10.0f, 4000.0f, durationMs);
}

void CompressorDisplay::updateDisplayDurationFromBPM() {
    float beatFraction = kBeatFractions[static_cast<size_t>(selectedTimeIndex)];
    if (currentBPM > 0.0) {
        displayDurationMs = static_cast<float>(
            (static_cast<double>(beatFraction) / currentBPM) * 60000.0);
    } else {
        // Fallback: assume 120 BPM
        displayDurationMs = static_cast<float>(
            (static_cast<double>(beatFraction) / 120.0) * 60000.0);
    }
    displayDurationMs = juce::jlimit(10.0f, 4000.0f, displayDurationMs);
}

// ─────────────────────────────────────────────────────────────────────────
// APVTS Listener
// ─────────────────────────────────────────────────────────────────────────

void CompressorDisplay::parameterChanged(const juce::String& parameterID, float newValue) {
    if (parameterID == kParamDownThresh)
        downThreshDb = newValue;
    else if (parameterID == kParamDownRatio)
        downRatio = newValue;
    else if (parameterID == kParamUpThresh)
        upThreshDb = newValue;
    else if (parameterID == kParamUpRatio)
        upRatio = newValue;

    // Trigger repaint on message thread
    juce::MessageManager::callAsync([safeComp = juce::Component::SafePointer(this)]() {
        if (safeComp != nullptr)
            safeComp->repaint();
    });
}

// ─────────────────────────────────────────────────────────────────────────
// Ring Buffer Helpers
// ─────────────────────────────────────────────────────────────────────────

void CompressorDisplay::appendToRing(RingBuffer& ring, const float* samples, int count) {
    for (int i = 0; i < count; ++i) {
        ring.data[static_cast<size_t>(ring.writePos)] = samples[i];
        ring.writePos = (ring.writePos + 1) % kRingSize;
    }
    ring.samplesWritten = juce::jmin(ring.samplesWritten + count, kRingSize);
}

void CompressorDisplay::readFromRing(const RingBuffer& ring, float* dest, int count) {
    const int available = juce::jmin(ring.samplesWritten, count);
    int readPos = (ring.writePos - available + kRingSize) % kRingSize;
    for (int i = 0; i < available; ++i) {
        dest[i] = ring.data[static_cast<size_t>(readPos)];
        readPos = (readPos + 1) % kRingSize;
    }
    for (int i = available; i < count; ++i)
        dest[i] = kMinDb;
}

// ─────────────────────────────────────────────────────────────────────────
// FIFO → Ring Buffer Transfer (UI thread)
// ─────────────────────────────────────────────────────────────────────────

void CompressorDisplay::updateFromFifos(AudioSampleFifo<2>& inputFifo,
                                         AudioSampleFifo<2>& grFifo) {
    // Pull input samples (stereo → mono → dB → ring)
    {
        const int avail = inputFifo.getNumAvailable();
        const int toPull = juce::jmin(avail, kMaxPullSamples);
        if (toPull > 0) {
            float* ch[2] = {tempL.data(), tempR.data()};
            int got = inputFifo.pull(ch, toPull);
            // Stereo → mono, then convert to dB
            for (int i = 0; i < got; ++i) {
                float mono = (tempL[static_cast<size_t>(i)] + tempR[static_cast<size_t>(i)]) * 0.5f;
                float absMono = std::abs(mono);
                float db = (absMono > 1e-10f)
                               ? 20.0f * std::log10(absMono)
                               : kMinDb;
                tempL[static_cast<size_t>(i)] = juce::jlimit(kMinDb, kMaxDb, db);
            }
            appendToRing(inputRing, tempL.data(), got);
        }
    }

    // Pull gain reduction (stereo → min(L,R) → dB → ring)
    {
        const int avail = grFifo.getNumAvailable();
        const int toPull = juce::jmin(avail, kMaxPullSamples);
        if (toPull > 0) {
            float* ch[2] = {tempL.data(), tempR.data()};
            int got = grFifo.pull(ch, toPull);
            for (int i = 0; i < got; ++i) {
                float gain = juce::jmin(tempL[static_cast<size_t>(i)],
                                        tempR[static_cast<size_t>(i)]);
                float db = (gain > 1e-10f)
                               ? 20.0f * std::log10(gain)
                               : -kGrMaxDb;
                tempL[static_cast<size_t>(i)] = juce::jlimit(-kGrMaxDb, 20.0f, db);
            }
            appendToRing(grRing, tempL.data(), got);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────
// Layout
// ─────────────────────────────────────────────────────────────────────────

juce::Rectangle<int> CompressorDisplay::getTransferCurveArea() const {
    auto bounds = getLocalBounds();
    int tcWidth = static_cast<int>(bounds.getWidth() * 0.35f);
    // Make it square, limited by height
    int side = juce::jmin(tcWidth, bounds.getHeight() - 24); // 24px for time bar
    return bounds.removeFromLeft(side).withTrimmedBottom(24);
}

juce::Rectangle<int> CompressorDisplay::getWaveformArea() const {
    auto bounds = getLocalBounds();
    int tcWidth = static_cast<int>(bounds.getWidth() * 0.35f);
    int side = juce::jmin(tcWidth, bounds.getHeight() - 24);
    return bounds.withTrimmedLeft(side + 2).withTrimmedBottom(24); // 2px separator
}

juce::Rectangle<int> CompressorDisplay::getTimeBarArea() const {
    auto bounds = getLocalBounds();
    int tcWidth = static_cast<int>(bounds.getWidth() * 0.35f);
    int side = juce::jmin(tcWidth, bounds.getHeight() - 24);
    return bounds.withTrimmedLeft(side + 2).removeFromBottom(24);
}

void CompressorDisplay::resized() {
    auto timeBar = getTimeBarArea();
    int btnWidth = timeBar.getWidth() / kNumTimeOptions;
    for (int i = 0; i < kNumTimeOptions; ++i) {
        timeButtons[static_cast<size_t>(i)].setBounds(
            timeBar.getX() + i * btnWidth, timeBar.getY(),
            btnWidth, timeBar.getHeight());
    }
}

// ─────────────────────────────────────────────────────────────────────────
// Paint
// ─────────────────────────────────────────────────────────────────────────

void CompressorDisplay::paint(juce::Graphics& g) {
    g.fillAll(kBgColour);

    auto tcArea = getTransferCurveArea();
    auto wfArea = getWaveformArea();

    // Separator line
    g.setColour(kGridColour);
    g.drawVerticalLine(tcArea.getRight() + 1, static_cast<float>(tcArea.getY()),
                       static_cast<float>(tcArea.getBottom()));

    paintTransferCurve(g, tcArea);
    paintWaveform(g, wfArea);
    paintGainReduction(g, wfArea);
}

// ─────────────────────────────────────────────────────────────────────────
// Transfer Curve
// ─────────────────────────────────────────────────────────────────────────

float CompressorDisplay::computeTransferOutput(float inputDb) const {
    float outputDb = inputDb;

    // Downward compression: above downThreshDb
    if (inputDb > downThreshDb && downRatio > 1.0f) {
        outputDb = downThreshDb + (inputDb - downThreshDb) / downRatio;
    }

    // Upward compression: below upThreshDb
    if (inputDb < upThreshDb && upRatio > 1.0f) {
        float upOutput = upThreshDb + (inputDb - upThreshDb) / upRatio;
        // If both regions overlap somehow, take the more compressed result
        if (inputDb > downThreshDb)
            outputDb = juce::jmin(outputDb, upOutput);
        else
            outputDb = upOutput;
    }

    return juce::jlimit(kMinDb, kMaxDb, outputDb);
}

juce::Point<float> CompressorDisplay::dbToTransferPoint(
    const juce::Rectangle<int>& area, float inputDb, float outputDb) const {
    // Map dB to pixel coordinates
    float normX = (inputDb - kMinDb) / (kMaxDb - kMinDb);
    float normY = (outputDb - kMinDb) / (kMaxDb - kMinDb);
    float x = area.getX() + normX * area.getWidth();
    float y = area.getBottom() - normY * area.getHeight(); // Y inverted
    return {x, y};
}

juce::Point<float> CompressorDisplay::getHandlePosition(
    const juce::Rectangle<int>& area, DragTarget target) const {
    switch (target) {
    case DragTarget::DownThresh: {
        // On the transfer curve at the downward threshold knee
        float outDb = computeTransferOutput(downThreshDb);
        return dbToTransferPoint(area, downThreshDb, outDb);
    }
    case DragTarget::DownRatio: {
        // Right edge of the downward compression line (0 dB input)
        float outDb = computeTransferOutput(kMaxDb);
        return dbToTransferPoint(area, kMaxDb, outDb);
    }
    case DragTarget::UpThresh: {
        float outDb = computeTransferOutput(upThreshDb);
        return dbToTransferPoint(area, upThreshDb, outDb);
    }
    case DragTarget::UpRatio: {
        // Left edge of the upward compression line (-60 dB input)
        float outDb = computeTransferOutput(kMinDb);
        return dbToTransferPoint(area, kMinDb, outDb);
    }
    default:
        return {};
    }
}

void CompressorDisplay::paintTransferCurve(juce::Graphics& g,
                                            const juce::Rectangle<int>& area) {
    // Background
    g.setColour(kBgColour.brighter(0.05f));
    g.fillRect(area);

    // dB grid
    paintDbGrid(g, area, true);

    // Unity diagonal line
    g.setColour(kUnityLineColour);
    auto topLeft = dbToTransferPoint(area, kMinDb, kMinDb);
    auto bottomRight = dbToTransferPoint(area, kMaxDb, kMaxDb);
    g.drawLine(topLeft.x, topLeft.y, bottomRight.x, bottomRight.y, 1.0f);

    // Transfer curve
    juce::Path curvePath;
    bool first = true;
    for (float db = kMinDb; db <= kMaxDb; db += 0.5f) {
        float outDb = computeTransferOutput(db);
        auto pt = dbToTransferPoint(area, db, outDb);
        if (first) {
            curvePath.startNewSubPath(pt);
            first = false;
        } else {
            curvePath.lineTo(pt);
        }
    }
    g.setColour(kTransferCurveColour);
    g.strokePath(curvePath, juce::PathStrokeType(2.0f));

    // Threshold lines (subtle)
    {
        float normDown = (downThreshDb - kMinDb) / (kMaxDb - kMinDb);
        float xDown = area.getX() + normDown * area.getWidth();
        g.setColour(kDownThreshHandleColour.withAlpha(0.25f));
        g.drawVerticalLine(static_cast<int>(xDown),
                           static_cast<float>(area.getY()),
                           static_cast<float>(area.getBottom()));

        float normUp = (upThreshDb - kMinDb) / (kMaxDb - kMinDb);
        float xUp = area.getX() + normUp * area.getWidth();
        g.setColour(kUpThreshHandleColour.withAlpha(0.25f));
        g.drawVerticalLine(static_cast<int>(xUp),
                           static_cast<float>(area.getY()),
                           static_cast<float>(area.getBottom()));
    }

    // Draggable handles
    auto drawHandle = [&](DragTarget target, juce::Colour colour, bool isDiamond) {
        auto pos = getHandlePosition(area, target);
        g.setColour(colour);
        if (isDiamond) {
            juce::Path diamond;
            diamond.addTriangle(pos.x, pos.y - kHandleRadius,
                                pos.x + kHandleRadius, pos.y,
                                pos.x, pos.y + kHandleRadius);
            diamond.addTriangle(pos.x, pos.y - kHandleRadius,
                                pos.x - kHandleRadius, pos.y,
                                pos.x, pos.y + kHandleRadius);
            g.fillPath(diamond);
            g.setColour(colour.brighter(0.3f));
            g.strokePath(diamond, juce::PathStrokeType(1.0f));
        } else {
            g.fillEllipse(pos.x - kHandleRadius, pos.y - kHandleRadius,
                          kHandleRadius * 2, kHandleRadius * 2);
            g.setColour(colour.brighter(0.3f));
            g.drawEllipse(pos.x - kHandleRadius, pos.y - kHandleRadius,
                          kHandleRadius * 2, kHandleRadius * 2, 1.0f);
        }
    };

    drawHandle(DragTarget::DownThresh, kDownThreshHandleColour, false);  // Circle
    drawHandle(DragTarget::DownRatio, kDownRatioHandleColour, true);     // Diamond
    drawHandle(DragTarget::UpThresh, kUpThreshHandleColour, false);      // Circle
    drawHandle(DragTarget::UpRatio, kUpRatioHandleColour, true);         // Diamond

    // Labels
    g.setColour(kGridTextColour);
    g.setFont(juce::Font(9.0f));
    auto labelArea = area;
    g.drawText("IN (dB)", labelArea.removeFromBottom(12), juce::Justification::centred);
}

// ─────────────────────────────────────────────────────────────────────────
// dB Grid
// ─────────────────────────────────────────────────────────────────────────

void CompressorDisplay::paintDbGrid(juce::Graphics& g,
                                     const juce::Rectangle<int>& area,
                                     bool isTransferCurve) {
    const float dbSteps[] = {-6.0f, -12.0f, -18.0f, -24.0f, -36.0f, -48.0f};
    g.setFont(juce::Font(8.0f));

    for (float db : dbSteps) {
        float norm = (db - kMinDb) / (kMaxDb - kMinDb);

        if (isTransferCurve) {
            // Horizontal + vertical grid
            float y = area.getBottom() - norm * area.getHeight();
            float x = area.getX() + norm * area.getWidth();
            g.setColour(kGridColour);
            g.drawHorizontalLine(static_cast<int>(y),
                                 static_cast<float>(area.getX()),
                                 static_cast<float>(area.getRight()));
            g.drawVerticalLine(static_cast<int>(x),
                               static_cast<float>(area.getY()),
                               static_cast<float>(area.getBottom()));
        } else {
            // Horizontal grid only (waveform area)
            float y = area.getBottom() - norm * area.getHeight();
            g.setColour(kGridColour);
            g.drawHorizontalLine(static_cast<int>(y),
                                 static_cast<float>(area.getX()),
                                 static_cast<float>(area.getRight()));
            g.setColour(kGridTextColour);
            g.drawText(juce::String(static_cast<int>(db)),
                       area.getX() + 2, static_cast<int>(y) - 6, 24, 12,
                       juce::Justification::centredLeft);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────
// Waveform
// ─────────────────────────────────────────────────────────────────────────

void CompressorDisplay::paintWaveform(juce::Graphics& g,
                                       const juce::Rectangle<int>& area) {
    // Background
    g.setColour(kBgColour);
    g.fillRect(area);

    paintDbGrid(g, area, false);

    // How many samples to display
    const int displaySamples = juce::jmin(
        static_cast<int>(sampleRate * static_cast<double>(displayDurationMs) / 1000.0),
        kRingSize);

    readFromRing(inputRing, paintBufInput.data(), displaySamples);

    const int w = area.getWidth();
    if (w <= 0 || displaySamples <= 0)
        return;

    const float samplesPerPixel = static_cast<float>(displaySamples) / static_cast<float>(w);

    g.setColour(kWaveformColour);

    for (int px = 0; px < w; ++px) {
        const int startSamp = static_cast<int>(static_cast<float>(px) * samplesPerPixel);
        int endSamp = static_cast<int>(static_cast<float>(px + 1) * samplesPerPixel);
        endSamp = juce::jmin(endSamp, displaySamples);

        float maxDb = kMinDb;
        for (int s = startSamp; s < endSamp; ++s) {
            maxDb = juce::jmax(maxDb, paintBufInput[static_cast<size_t>(s)]);
        }

        // Map dB to pixel Y: 0 dB at top, -60 dB at bottom
        float norm = (maxDb - kMinDb) / (kMaxDb - kMinDb);
        norm = juce::jlimit(0.0f, 1.0f, norm);
        float barTop = area.getBottom() - norm * area.getHeight();
        float barBottom = static_cast<float>(area.getBottom());

        g.drawVerticalLine(area.getX() + px, barTop, barBottom);
    }
}

// ─────────────────────────────────────────────────────────────────────────
// Gain Reduction
// ─────────────────────────────────────────────────────────────────────────

void CompressorDisplay::paintGainReduction(juce::Graphics& g,
                                            const juce::Rectangle<int>& area) {
    const int displaySamples = juce::jmin(
        static_cast<int>(sampleRate * static_cast<double>(displayDurationMs) / 1000.0),
        kRingSize);

    readFromRing(grRing, paintBufGR.data(), displaySamples);

    const int w = area.getWidth();
    if (w <= 0 || displaySamples <= 0)
        return;

    const float samplesPerPixel = static_cast<float>(displaySamples) / static_cast<float>(w);

    // Attenuation region: top 30% of graph (0 dB at top edge, -kGrMaxDb going down)
    const float attenAreaHeight = area.getHeight() * 0.3f;
    // Boost region: bottom 30% of graph (0 dB at bottom edge, +kGrMaxDb going up)
    const float boostAreaHeight = area.getHeight() * 0.3f;

    // Pre-compute average dB per pixel column
    std::vector<float> avgDbPerPx(static_cast<size_t>(w), 0.0f);
    for (int px = 0; px < w; ++px) {
        const int startSamp = static_cast<int>(static_cast<float>(px) * samplesPerPixel);
        int endSamp = static_cast<int>(static_cast<float>(px + 1) * samplesPerPixel);
        endSamp = juce::jmin(endSamp, displaySamples);
        float sum = 0.0f;
        int count = 0;
        for (int s = startSamp; s < endSamp; ++s) {
            sum += paintBufGR[static_cast<size_t>(s)];
            ++count;
        }
        avgDbPerPx[static_cast<size_t>(px)] = (count > 0) ? sum / static_cast<float>(count) : 0.0f;
    }

    // --- Attenuation (orange, from top) — negative dB values ---
    {
        juce::Path attenPath;
        attenPath.startNewSubPath(static_cast<float>(area.getX()), static_cast<float>(area.getY()));
        for (int px = 0; px < w; ++px) {
            float db = avgDbPerPx[static_cast<size_t>(px)];
            float attenDb = juce::jmin(db, 0.0f); // only negative part
            float norm = juce::jlimit(0.0f, 1.0f, -attenDb / kGrMaxDb);
            float y = area.getY() + norm * attenAreaHeight;
            attenPath.lineTo(static_cast<float>(area.getX() + px), y);
        }
        attenPath.lineTo(static_cast<float>(area.getRight()), static_cast<float>(area.getY()));
        attenPath.closeSubPath();

        g.setColour(kGrFillColour.withAlpha(0.35f));
        g.fillPath(attenPath);

        // Stroke the bottom edge
        juce::Path attenLine;
        for (int px = 0; px < w; ++px) {
            float db = avgDbPerPx[static_cast<size_t>(px)];
            float attenDb = juce::jmin(db, 0.0f);
            float norm = juce::jlimit(0.0f, 1.0f, -attenDb / kGrMaxDb);
            float y = area.getY() + norm * attenAreaHeight;
            if (px == 0)
                attenLine.startNewSubPath(static_cast<float>(area.getX()), y);
            else
                attenLine.lineTo(static_cast<float>(area.getX() + px), y);
        }
        g.setColour(kGrLineColour);
        g.strokePath(attenLine, juce::PathStrokeType(1.5f));
    }

    // --- Boost (green, from bottom) — positive dB values ---
    {
        const float bottom = static_cast<float>(area.getBottom());
        juce::Path boostPath;
        boostPath.startNewSubPath(static_cast<float>(area.getX()), bottom);
        for (int px = 0; px < w; ++px) {
            float db = avgDbPerPx[static_cast<size_t>(px)];
            float boostDb = juce::jmax(db, 0.0f); // only positive part
            float norm = juce::jlimit(0.0f, 1.0f, boostDb / kGrMaxDb);
            float y = bottom - norm * boostAreaHeight;
            boostPath.lineTo(static_cast<float>(area.getX() + px), y);
        }
        boostPath.lineTo(static_cast<float>(area.getRight()), bottom);
        boostPath.closeSubPath();

        g.setColour(kBoostFillColour.withAlpha(0.3f));
        g.fillPath(boostPath);

        // Stroke the top edge
        juce::Path boostLine;
        for (int px = 0; px < w; ++px) {
            float db = avgDbPerPx[static_cast<size_t>(px)];
            float boostDb = juce::jmax(db, 0.0f);
            float norm = juce::jlimit(0.0f, 1.0f, boostDb / kGrMaxDb);
            float y = bottom - norm * boostAreaHeight;
            if (px == 0)
                boostLine.startNewSubPath(static_cast<float>(area.getX()), y);
            else
                boostLine.lineTo(static_cast<float>(area.getX() + px), y);
        }
        g.setColour(kBoostLineColour);
        g.strokePath(boostLine, juce::PathStrokeType(1.5f));
    }

    // Labels
    g.setFont(juce::Font(8.0f));
    g.setColour(kGrLineColour.withAlpha(0.6f));
    g.drawText("GR", area.getX() + area.getWidth() - 20, area.getY() + 2, 18, 10,
               juce::Justification::centredRight);
    g.setColour(kBoostLineColour.withAlpha(0.6f));
    g.drawText("UP", area.getX() + area.getWidth() - 20, area.getBottom() - 12, 18, 10,
               juce::Justification::centredRight);
}

// ─────────────────────────────────────────────────────────────────────────
// Mouse Interaction (Transfer Curve Handles)
// ─────────────────────────────────────────────────────────────────────────

void CompressorDisplay::mouseDown(const juce::MouseEvent& e) {
    auto tcArea = getTransferCurveArea();
    auto pos = e.position;

    if (!tcArea.toFloat().contains(pos))
        return;

    // Check which handle is closest
    auto checkHandle = [&](DragTarget target) -> float {
        auto hp = getHandlePosition(tcArea, target);
        return pos.getDistanceFrom(hp);
    };

    struct { DragTarget target; float dist; } candidates[] = {
        {DragTarget::DownThresh, checkHandle(DragTarget::DownThresh)},
        {DragTarget::DownRatio, checkHandle(DragTarget::DownRatio)},
        {DragTarget::UpThresh, checkHandle(DragTarget::UpThresh)},
        {DragTarget::UpRatio, checkHandle(DragTarget::UpRatio)},
    };

    float bestDist = kHandleHitRadius;
    currentDrag = DragTarget::None;
    for (auto& c : candidates) {
        if (c.dist < bestDist) {
            bestDist = c.dist;
            currentDrag = c.target;
        }
    }
}

void CompressorDisplay::mouseDrag(const juce::MouseEvent& e) {
    if (currentDrag == DragTarget::None)
        return;

    auto tcArea = getTransferCurveArea();
    auto pos = e.position;

    // Convert pixel to dB
    float normX = (pos.x - tcArea.getX()) / static_cast<float>(tcArea.getWidth());
    float normY = 1.0f - (pos.y - tcArea.getY()) / static_cast<float>(tcArea.getHeight());
    float inputDb = kMinDb + normX * (kMaxDb - kMinDb);
    float outputDb = kMinDb + normY * (kMaxDb - kMinDb);
    inputDb = juce::jlimit(kMinDb, kMaxDb, inputDb);
    outputDb = juce::jlimit(kMinDb, kMaxDb, outputDb);

    switch (currentDrag) {
    case DragTarget::DownThresh: {
        // Threshold handle: X position sets threshold dB
        if (auto* param = apvts.getParameter(kParamDownThresh))
            param->setValueNotifyingHost(
                param->getNormalisableRange().convertTo0to1(inputDb));
        break;
    }
    case DragTarget::DownRatio: {
        // Ratio handle at right edge (0 dB input):
        //   outputDb = threshDb + (kMaxDb - threshDb) / ratio
        //   ratio = (kMaxDb - threshDb) / (outputDb - threshDb)
        float denom = outputDb - downThreshDb;
        float newRatio = (denom > 0.1f) ? (kMaxDb - downThreshDb) / denom : 20.0f;
        newRatio = juce::jlimit(1.0f, 20.0f, newRatio);
        if (auto* param = apvts.getParameter(kParamDownRatio))
            param->setValueNotifyingHost(
                param->getNormalisableRange().convertTo0to1(newRatio));
        break;
    }
    case DragTarget::UpThresh: {
        if (auto* param = apvts.getParameter(kParamUpThresh))
            param->setValueNotifyingHost(
                param->getNormalisableRange().convertTo0to1(inputDb));
        break;
    }
    case DragTarget::UpRatio: {
        // Ratio handle at left edge (-60 dB input):
        //   outputDb = threshDb + (kMinDb - threshDb) / ratio
        //   ratio = (kMinDb - threshDb) / (outputDb - threshDb)
        float denom = outputDb - upThreshDb;
        float newRatio = (denom < -0.1f) ? (kMinDb - upThreshDb) / denom : 20.0f;
        newRatio = juce::jlimit(1.0f, 20.0f, newRatio);
        if (auto* param = apvts.getParameter(kParamUpRatio))
            param->setValueNotifyingHost(
                param->getNormalisableRange().convertTo0to1(newRatio));
        break;
    }
    default:
        break;
    }
}

void CompressorDisplay::mouseUp(const juce::MouseEvent&) {
    currentDrag = DragTarget::None;
}
