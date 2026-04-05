#pragma once

#include "PluginConstants.h"
#include "audio/AudioSampleFifo.h"
#include "audio/BeatSyncBuffer.h"
#include "audio/BucketSet.h"
#include "audio/PpqRingBuffer.h"
#include "audio/RmsPacketFifo.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

using phu::audio::AudioSampleFifo;
using phu::audio::BeatSyncBuffer;
using phu::audio::RmsPacketFifo;
using phu::audio::RmsPacket;

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

    // Musical time options live in PluginConstants.h (kDisplay*).
    // kDisplayNumRanges, kDisplayBeatFractions, kDisplayBeatLabels.

    CompressorDisplay(juce::AudioProcessorValueTreeState& apvts);
    ~CompressorDisplay() override;

    void setSampleRate(double sr);
    void setBPM(double bpm);
    void setDisplayDuration(float durationMs);

    /** Toggle visibility of individual overlay curves. */
    void setShowUpDetectorCurve(bool show) { showUpDetectorCurve = show; }
    void setShowDownDetectorCurve(bool show) { showDownDetectorCurve = show; }
    void setShowDownGr(bool show) { showDownGr = show; }
    void setShowUpGr(bool show) { showUpGr = show; }

    /** Beat-sync mode: paint from position-indexed buffers instead of scrolling ring. */
    void setBeatSyncMode(bool enabled);
    bool isBeatSyncMode() const { return beatSyncMode; }

    /** Point to the processor's beat-sync buffers (call once from editor constructor). */
    void setBeatSyncBuffers(const BeatSyncBuffer& input,
                            const BeatSyncBuffer& downGr,
                            const BeatSyncBuffer& upGr);

    /** Set current playhead PPQ for cursor position (call from timerCallback). */
    void setCurrentPpq(double ppq) { currentPpq = ppq; }

    /** Set the display range in beats (synchronised with processor). */
    void setDisplayRangeBeats(double beats) { displayRangeBeats = beats; }
    double getDisplayRangeBeats() const { return displayRangeBeats; }

    /** Pull latest samples from FIFOs (call from editor timerCallback). */
    void updateFromFifos(AudioSampleFifo<2>& inputFifo,
                         AudioSampleFifo<2>& downGrFifo,
                         AudioSampleFifo<2>& upGrFifo,
                         RmsPacketFifo& detectorFifo,
                         RmsPacketFifo& downDetectorFifo);

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
    RingBuffer inputRing;         // input level in dB (mono)
    RingBuffer downGrRing;        // downward GR in dB (mono, always ≤ 0)
    RingBuffer upGrRing;          // upward boost in dB (mono, always ≥ 0)
    RingBuffer detectorRing;      // up-detector level in dB (mono, raw input)
    RingBuffer downDetectorRing;  // down-detector level in dB (mono, post-upward boost)

    // Temp pull buffers
    std::array<float, kMaxPullSamples> tempL{};
    std::array<float, kMaxPullSamples> tempR{};
    std::array<float, phu::audio::kRmsMaxPacketSamples> m_rmsSquaredTemp{};

    // Paint read-out buffers
    std::array<float, kRingSize> paintBufInput{};
    std::array<float, kRingSize> paintBufGR{};
    std::array<float, kRingSize> paintBufUpGR{};
    std::array<float, kRingSize> paintBufDetector{};
    std::array<float, kRingSize> paintBufDownDetector{};
    std::array<float, kMaxDisplayWidth> paintBufAvgDb{};

    // --- Transfer curve cached parameters ---
    float downThreshDb = -12.0f;
    float downRatio = 4.0f;
    float upThreshDb = -30.0f;
    float upRatio = 4.0f;

    // --- Musical time buttons ---
    int selectedTimeIndex = 1; // default: 2 beats (kDisplayBeatFractions[1])
    std::array<juce::TextButton, kDisplayNumRanges> timeButtons;

    // --- Curve visibility flags ---
    bool showUpDetectorCurve = true;
    bool showDownDetectorCurve = true;
    bool showDownGr = true;
    bool showUpGr = true;

    // --- Beat-sync state ---
    bool beatSyncMode = false;
    const BeatSyncBuffer* inputSyncBuf    = nullptr;
    const BeatSyncBuffer* downGrSyncBuf   = nullptr;
    const BeatSyncBuffer* upGrSyncBuf     = nullptr;
    double currentPpq = 0.0;
    double displayRangeBeats = 4.0;

    // --- Detector RMS ring buffer + bucket-set display channels ---
    struct RmsDisplayChannel {
        RmsDisplayChannel()
            : rmsRing(kMinBPM, kMaxSampleRate, kDisplayMaxBeatFraction)
        {}
        phu::audio::PpqRingBufferF rmsRing;    // PPQ-indexed ring of linear power (x^2)
        phu::audio::BucketSet      bucketSet;
        std::vector<float>         paintValues; // one RMS dB value per bucket
    };
    RmsDisplayChannel m_detDisplay;      // up-detector (raw input level)
    RmsDisplayChannel m_downDetDisplay;  // down-detector (post-upward-boost level)

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

    // --- Beat-sync rendering helpers ---
    void resizeDetDisplayChannel(RmsDisplayChannel& ch, double bpm, double sr, double displayBeats);
    void insertPacketToChannel(RmsDisplayChannel& ch, const RmsPacket& packet);
    void computeDirtyBucketMeans(RmsDisplayChannel& ch);
    void paintBeatSyncWaveform(juce::Graphics& g, const juce::Rectangle<int>& area);
    void paintBeatSyncDetector(juce::Graphics& g, const juce::Rectangle<int>& area);
    void paintBeatSyncGainReduction(juce::Graphics& g, const juce::Rectangle<int>& area);
    void paintBeatGrid(juce::Graphics& g, const juce::Rectangle<int>& area);
    void paintPlayheadCursor(juce::Graphics& g, const juce::Rectangle<int>& area);

    // --- Time selector ---
    void updateDisplayDurationFromBPM();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CompressorDisplay)
};
