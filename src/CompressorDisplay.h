#pragma once

#include "audio/AudioSampleFifo.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

using phu::audio::AudioSampleFifo;

/**
 * CompressorDisplay — unified dB-based panel containing:
 *   - Transfer curve (left zone): input dB vs output dB with draggable handles
 *   - Rolling waveform (right zone): input level in dB, scrolling right-to-left
 *   - Gain reduction line (right zone, overlaid on top)
 *   - Musical time selector (bottom-right)
 *
 * All rendering happens on the UI thread. The editor's timerCallback calls
 * updateFromFifos() to pull data from lock-free FIFOs, then repaint().
 */
class CompressorDisplay : public juce::Component,
                          public juce::AudioProcessorValueTreeState::Listener {
  public:
    // Ring buffer capacity: supports up to 4000ms at 96kHz
    static constexpr int kRingSize = 384000;
    static constexpr int kMaxPullSamples = 8192;
    static constexpr int kMaxDisplayWidth = 4096;

    // dB range for all displays
    static constexpr float kMinDb = -60.0f;
    static constexpr float kMaxDb = 0.0f;
    static constexpr float kGrMaxDb = 24.0f; // max GR/boost display depth in dB

    // Musical time options (in beats)
    static constexpr int kNumTimeOptions = 5;
    static constexpr float kBeatFractions[kNumTimeOptions] = {
        0.5f, 1.0f, 2.0f, 4.0f, 8.0f};
    static constexpr const char* kBeatLabels[kNumTimeOptions] = {
        "1/2", "1", "2", "4", "8"};

    CompressorDisplay(juce::AudioProcessorValueTreeState& apvts);
    ~CompressorDisplay() override;

    void setSampleRate(double sr);
    void setBPM(double bpm);
    void setDisplayDuration(float durationMs);

    /** Toggle visibility of individual overlay curves. */
    void setShowDetectorCurve(bool show) { showDetectorCurve = show; }
    void setShowDownGr(bool show) { showDownGr = show; }
    void setShowUpGr(bool show) { showUpGr = show; }

    /** Pull latest samples from FIFOs (call from editor timerCallback). */
    void updateFromFifos(AudioSampleFifo<2>& inputFifo,
                         AudioSampleFifo<2>& grFifo,
                         AudioSampleFifo<2>& detectorFifo);

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Mouse interaction for transfer curve handles
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

    // APVTS listener
    void parameterChanged(const juce::String& parameterID, float newValue) override;

  private:
    juce::AudioProcessorValueTreeState& apvts;

    double sampleRate = 48000.0;
    double currentBPM = 0.0;
    float displayDurationMs = 500.0f;

    // --- Ring buffers for waveform & GR (mono, in dB) ---
    struct RingBuffer {
        std::array<float, kRingSize> data{};
        int writePos = 0;
        int samplesWritten = 0;
    };
    RingBuffer inputRing;    // input level in dB (mono)
    RingBuffer grRing;       // gain reduction in dB (mono, negative values = compression)
    RingBuffer detectorRing; // detector level in dB (mono)

    // Temp pull buffers
    std::array<float, kMaxPullSamples> tempL{};
    std::array<float, kMaxPullSamples> tempR{};

    // Paint read-out buffers
    std::array<float, kRingSize> paintBufInput{};
    std::array<float, kRingSize> paintBufGR{};
    std::array<float, kRingSize> paintBufDetector{};
    std::array<float, kMaxDisplayWidth> paintBufAvgDb{};

    // --- Transfer curve cached parameters ---
    float downThreshDb = -12.0f;
    float downRatio = 4.0f;
    float upThreshDb = -30.0f;
    float upRatio = 4.0f;

    // --- Musical time buttons ---
    int selectedTimeIndex = 1; // default: 1 beat
    std::array<juce::TextButton, kNumTimeOptions> timeButtons;

    // --- Curve visibility flags ---
    bool showDetectorCurve = true;
    bool showDownGr = true;
    bool showUpGr = true;

    // --- Draggable handle state ---
    enum class DragTarget { None, DownThresh, DownRatio, UpThresh, UpRatio };
    DragTarget currentDrag = DragTarget::None;

    // --- Layout zones ---
    juce::Rectangle<int> getTransferCurveArea() const;
    juce::Rectangle<int> getWaveformArea() const;
    juce::Rectangle<int> getTimeBarArea() const;

    // --- Ring buffer helpers ---
    static void appendToRing(RingBuffer& ring, const float* samples, int count);
    static void readFromRing(const RingBuffer& ring, float* dest, int count);

    // --- Transfer curve rendering ---
    void paintTransferCurve(juce::Graphics& g, const juce::Rectangle<int>& area);
    float computeTransferOutput(float inputDb) const;
    juce::Point<float> dbToTransferPoint(const juce::Rectangle<int>& area,
                                          float inputDb, float outputDb) const;
    juce::Point<float> getHandlePosition(const juce::Rectangle<int>& area,
                                          DragTarget target) const;

    // --- Waveform + GR rendering ---
    void paintWaveform(juce::Graphics& g, const juce::Rectangle<int>& area);
    void paintDetectorCurve(juce::Graphics& g, const juce::Rectangle<int>& area);
    void paintGainReduction(juce::Graphics& g, const juce::Rectangle<int>& area);
    void paintDbGrid(juce::Graphics& g, const juce::Rectangle<int>& area, bool isTransferCurve);

    // --- Time selector ---
    void updateDisplayDurationFromBPM();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CompressorDisplay)
};
