#pragma once

#include <algorithm>
#include <cmath>
#include <juce_dsp/juce_dsp.h>

/**
 * OttCompressor — single-band upward + downward compressor.
 *
 * Standard 4-stage architecture (two parallel gain paths):
 *   1. Level Detector  — fast peak envelope follower gives smooth dB level from audio
 *   2. Gain Computer   — two independent always-positive dB values:
 *        downGrDb   = (envDb - downThresh) * (1 - 1/downRatio)  when envDb > downThresh, else 0
 *        upBoostDb  = (upThresh - envDb)   * (1 - 1/upRatio)    when envDb < upThresh,   else 0
 *   3. Ballistics      — two independent smoothers:
 *        downBallistics: JUCE BallisticsFilter on downGrDb (attack/release = downAttack/downRelease)
 *        upBallistics:   manual one-pole on upBoostDb with a special rule:
 *                          - when envDb > downThresh (compression active), the boost releases at
 *                            the downAttack rate instead of upRelease. This ensures the residual
 *                            boost from a previous quiet section does not reduce effective GR
 *                            when a loud peak arrives.
 *   4. Apply Gain      — totalGainDb = smoothedBoost - smoothedGR
 *                        output = input * dBtoLinear(totalGainDb)
 */
template <typename SampleType>
class OttCompressor {
  public:
    OttCompressor() = default;

    void prepare(const juce::dsp::ProcessSpec& spec) {
        sampleRate = static_cast<SampleType>(spec.sampleRate);
        expFactor = SampleType(-2.0) * SampleType(juce::MathConstants<double>::pi)
                    * SampleType(1000.0) / sampleRate;

        // Level detector: fast peak envelope follower (fixed internal times)
        levelDetector.prepare(spec);
        levelDetector.setAttackTime(SampleType(0.01));  // near-instant peak tracking
        levelDetector.setReleaseTime(SampleType(50.0)); // smooth envelope decay

        downBallistics.prepare(spec);
        updateCoefficients();
        reset();
    }

    void reset() {
        levelDetector.reset();
        downBallistics.reset();
        for (auto& v : smoothedUp) v = SampleType(0);
    }

    // -------------------------------------------------------------------------
    // Parameter setters
    // -------------------------------------------------------------------------

    void setDownThresholdDb(SampleType dB) { downThreshDb = dB; }
    void setDownRatio(SampleType r) { downRatio = std::max(r, SampleType(1)); }
    void setUpThresholdDb(SampleType dB) { upThreshDb = dB; }
    void setUpRatio(SampleType r) { upRatio = std::max(r, SampleType(1)); }

    void setDownAttackMs(SampleType ms) { downAttackMs = ms; downBallistics.setAttackTime(ms); updateCoefficients(); }
    void setDownReleaseMs(SampleType ms) { downReleaseMs = ms; downBallistics.setReleaseTime(ms); }
    void setUpAttackMs(SampleType ms) { upAttackMs = ms; updateCoefficients(); }
    void setUpReleaseMs(SampleType ms) { upReleaseMs = ms; updateCoefficients(); }

    // -------------------------------------------------------------------------
    // Per-sample processing
    // -------------------------------------------------------------------------

    SampleType processSample(int channel, SampleType input) {
        return processSampleWithGR(channel, input).output;
    }

    struct Result {
        SampleType output;
        SampleType totalGain;
    };

    Result processSampleWithGR(int channel, SampleType input) {
        // Stage 1: Level detector — smooth envelope from audio
        const SampleType envelope = levelDetector.processSample(channel, input);
        const SampleType envDb = linearToDb(envelope);

        const bool inCompressionZone = (envDb > downThreshDb);

        // Stage 2: Gain computers — independent, always-positive dB values
        const SampleType downGrDb = inCompressionZone
            ? (envDb - downThreshDb) * (SampleType(1) - SampleType(1) / downRatio)
            : SampleType(0);

        const SampleType upBoostTarget = (!inCompressionZone && envDb < upThreshDb && envelope > SampleType(0))
            ? (upThreshDb - envDb) * (SampleType(1) - SampleType(1) / upRatio)
            : SampleType(0);

        // Stage 3a: Downward ballistics — JUCE BallisticsFilter on always-positive GR dB
        const SampleType smoothedGR = downBallistics.processSample(channel, downGrDb);

        // Stage 3b: Upward ballistics — manual one-pole
        //   Normal: upAttack when rising, upRelease when falling.
        //   Compression zone: use downAttack coeff to release boost in sync with
        //   compression onset, preventing residual boost from offsetting GR.
        {
            const SampleType prev = smoothedUp[channel];
            SampleType coeff;
            if (upBoostTarget > prev) {
                coeff = upAttackCoeff;
            } else if (inCompressionZone) {
                coeff = downAttackCoeff; // fast release — sync with compression attack
            } else {
                coeff = upReleaseCoeff;
            }
            smoothedUp[channel] = upBoostTarget + coeff * (prev - upBoostTarget);
        }

        // Stage 4: Apply combined gain
        const SampleType totalGainDb = smoothedUp[channel] - smoothedGR;
        const SampleType totalGain = dbToLinear(totalGainDb);
        return { input * totalGain, totalGain };
    }

  private:
    static SampleType linearToDb(SampleType linear) {
        constexpr SampleType floor = SampleType(-120);
        return (linear > SampleType(0))
                   ? std::max(SampleType(20) * std::log10(linear), floor)
                   : floor;
    }

    static SampleType dbToLinear(SampleType dB) {
        return std::pow(SampleType(10), dB / SampleType(20));
    }

    // Same one-pole formula as JUCE BallisticsFilter
    SampleType msToCoeff(SampleType timeMs) const {
        return (timeMs < SampleType(0.001))
                   ? SampleType(0)
                   : static_cast<SampleType>(std::exp(expFactor / timeMs));
    }

    void updateCoefficients() {
        downAttackCoeff = msToCoeff(downAttackMs);
        upAttackCoeff   = msToCoeff(upAttackMs);
        upReleaseCoeff  = msToCoeff(upReleaseMs);
    }

    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------

    SampleType sampleRate    { SampleType(44100) };
    SampleType expFactor     { SampleType(0) };

    SampleType downAttackMs  { SampleType(10) };
    SampleType downReleaseMs { SampleType(100) };
    SampleType upAttackMs    { SampleType(10) };
    SampleType upReleaseMs   { SampleType(100) };

    SampleType downThreshDb  { SampleType(0) };
    SampleType downRatio     { SampleType(4) };
    SampleType upThreshDb    { SampleType(-30) };
    SampleType upRatio       { SampleType(4) };

    // Precomputed one-pole coefficients (JUCE BallisticsFilter formula)
    SampleType downAttackCoeff { SampleType(0) };
    SampleType upAttackCoeff   { SampleType(0) };
    SampleType upReleaseCoeff  { SampleType(0) };

    // Internal fast envelope follower (fixed, not user-controlled)
    juce::dsp::BallisticsFilter<SampleType> levelDetector;
    // Downward GR: JUCE BallisticsFilter (always-positive GR dB)
    juce::dsp::BallisticsFilter<SampleType> downBallistics;
    // Upward boost: manual one-pole (releases fast when compression is active)
    SampleType smoothedUp[2] = { SampleType(0), SampleType(0) };
};
