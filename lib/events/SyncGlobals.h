#pragma once

#include "EventSource.h"
#include "SyncGlobalsListener.h"
#include <atomic>
#include <cstddef>
#include <cmath>
#include <juce_audio_processors/juce_audio_processors.h>

namespace phu {
namespace events {

class GlobalsEventSource : public EventSource<GlobalsEventListener> {
  public:
    void fireBPMChanged(const BPMEvent& event) {
        for (size_t i = 0; i < listeners.size(); ++i)
            listeners[i]->onBPMChanged(event);
    }

    void fireIsPlayingChanged(const IsPlayingEvent& event) {
        for (size_t i = 0; i < listeners.size(); ++i)
            listeners[i]->onIsPlayingChanged(event);
    }

    void fireSampleRateChanged(const SampleRateEvent& event) {
        for (size_t i = 0; i < listeners.size(); ++i)
            listeners[i]->onSampleRateChanged(event);
    }
};

class SyncGlobals : public GlobalsEventSource {
  private:
    struct PPQBaseValue {
        double msec = 60000.0;
        double noteNum = 1.0;
        double noteDenom = 4.0;
        double ratio = 0.25;
    } ppqBase;

    long runs = 0;
    long long samplesCount = 0;
    double sampleRate = -1.0;
    double sampleRateByMsec = -1.0;
    bool isPlaying = false;
    double bpm = 0.0;
    double msecPerBeat = 0.0;
    double samplesPerBeat = 0.0;
    int timeSigNumerator = 4;
    int timeSigDenominator = 4;
    double beatLengthPpq = 1.0;
    double barLengthPpq = 4.0;
    double lastBarStartPpq = 0.0;
    double barPhaseOffsetPpq = 0.0;

    // PPQ tracking — blockStartPpq is audio-thread only,
    // ppqEndOfBlock is atomically published for UI-thread reads.
    double blockStartPpq = 0.0;
    std::atomic<double> ppqEndOfBlock{0.0};

  public:
    SyncGlobals() = default;
    SyncGlobals(const SyncGlobals&) = delete;
    SyncGlobals& operator=(const SyncGlobals&) = delete;

    void finishRun(int numSamples) {
        runs++;
        samplesCount += numSamples;
    }

    long getCurrentRun() const { return runs; }
    long long getCurrentSampleCount() const { return samplesCount; }
    double getBPM() const { return bpm; }
    double getSampleRate() const { return sampleRate; }
    bool isDawPlaying() const { return isPlaying; }
    int getTimeSigNumerator() const { return timeSigNumerator; }
    int getTimeSigDenominator() const { return timeSigDenominator; }
    double getBeatLengthPpq() const { return beatLengthPpq; }
    double getBarLengthPpq() const { return barLengthPpq; }
    double getLastBarStartPpq() const { return lastBarStartPpq; }
    double getBarPhaseOffsetPpq() const { return barPhaseOffsetPpq; }

    /** Audio-thread only: block-start PPQ set each processBlock by updateDAWGlobals. */
    double getPpqBlockStart() const { return blockStartPpq; }

    /** UI-thread safe: latest end-of-block PPQ (set by processor after processing). */
    double getPpqEndOfBlock() const { return ppqEndOfBlock.load(std::memory_order_relaxed); }

    /** Audio-thread only: call after processing to publish the end-of-block PPQ. */
    void setPpqEndOfBlock(double ppq) { ppqEndOfBlock.store(ppq, std::memory_order_relaxed); }

    void updateSampleRate(double newSampleRate) {
        if (newSampleRate != sampleRate) {
            double oldSampleRate = sampleRate;
            sampleRate = newSampleRate;
            sampleRateByMsec = newSampleRate / 1000.0;

            if (bpm > 0.0)
                samplesPerBeat = msecPerBeat * sampleRateByMsec;

            SampleRateEvent event;
            event.source = this;
            event.oldRate = oldSampleRate;
            event.newRate = newSampleRate;
            fireSampleRateChanged(event);
        }
    }

    Event::Context
    updateDAWGlobals(const juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midiBuffer,
                     const juce::Optional<juce::AudioPlayHead::PositionInfo>& positionInfo) {
        Event::Context ctx;
        ctx.buffer = &buffer;
        ctx.numberOfSamplesInFrame = buffer.getNumSamples();
        ctx.midiBuffer = &midiBuffer;
        ctx.positionInfo = &positionInfo;
        ctx.epoch = static_cast<int>(runs);

        if (positionInfo.hasValue()) {
            if (auto bpmValue = positionInfo->getBpm()) {
                double newBPM = *bpmValue;
                if (newBPM != bpm && newBPM > 0.0) {
                    BPMEvent event;
                    event.source = this;
                    event.context = ctx;
                    event.oldValues = {bpm, msecPerBeat, samplesPerBeat};

                    bpm = newBPM;
                    msecPerBeat = ppqBase.msec / newBPM;
                    samplesPerBeat = msecPerBeat * sampleRateByMsec;

                    event.newValues = {bpm, msecPerBeat, samplesPerBeat};
                    fireBPMChanged(event);
                }
            }

            if (auto ppqPos = positionInfo->getPpqPosition())
                blockStartPpq = *ppqPos;

            if (auto sig = positionInfo->getTimeSignature()) {
                const int newNumerator = juce::jmax(1, sig->numerator);
                const int newDenominator = juce::jmax(1, sig->denominator);
                timeSigNumerator = newNumerator;
                timeSigDenominator = newDenominator;
                beatLengthPpq = 4.0 / static_cast<double>(timeSigDenominator);
                barLengthPpq = beatLengthPpq * static_cast<double>(timeSigNumerator);
            }

            if (auto lastBarStart = positionInfo->getPpqPositionOfLastBarStart()) {
                lastBarStartPpq = *lastBarStart;
            } else if (auto ppqPos = positionInfo->getPpqPosition()) {
                if (barLengthPpq > 0.0)
                    lastBarStartPpq = std::floor(*ppqPos / barLengthPpq) * barLengthPpq;
            }

            if (barLengthPpq > 0.0) {
                barPhaseOffsetPpq = std::fmod(lastBarStartPpq, barLengthPpq);
                if (barPhaseOffsetPpq < 0.0)
                    barPhaseOffsetPpq += barLengthPpq;
            } else {
                barPhaseOffsetPpq = 0.0;
            }

            bool newIsPlaying = positionInfo->getIsPlaying();
            if (newIsPlaying != isPlaying) {
                IsPlayingEvent event;
                event.source = this;
                event.context = ctx;
                event.oldValue = isPlaying;
                event.newValue = newIsPlaying;
                isPlaying = newIsPlaying;
                fireIsPlayingChanged(event);
            }
        }
        return ctx;
    }
};

} // namespace events
} // namespace phu
