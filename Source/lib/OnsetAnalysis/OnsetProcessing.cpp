//
// Created by Nicholas Solem on 10/12/25.
//

#include "OnsetProcessing.h"
#include <ranges>
#include "essentia/essentiamath.h"

namespace nvs::analysis {
void filterOnsetsOutsideBounds(std::vector<float> &onsetsInSeconds, const double lengthInSeconds, const float minimumProximityToEndAllowedSeconds)
{	// filter out onsets that exceed the file length (taking into account minimum onset delta)
    assert( std::ranges::is_sorted(onsetsInSeconds) );

    // starting at end, count onsets exceeding lengthInSeconds
    size_t numProperOnsets = onsetsInSeconds.size();
    for (auto it = onsetsInSeconds.rbegin(); it != onsetsInSeconds.rend(); ++it){
        if (*it > (lengthInSeconds - minimumProximityToEndAllowedSeconds)){
            --numProperOnsets;
        }
        else {	// since the vector is sorted, we know that there are no more exceeding the proper length
            break;
        }
    }
    onsetsInSeconds.resize(numProperOnsets);
}
void filterRedundantOnsets(std::vector<float> &onsetsInSeconds, const float minimumOnsetDeltaSeconds)
{
    assert( std::ranges::is_sorted(onsetsInSeconds) );
    {   // filter out redundant onsets (can be defined by onsets which are too close together)
        const auto new_end = std::ranges::unique(
            onsetsInSeconds,
            [minimumOnsetDeltaSeconds](const float a, const float b){
            return (b - a) < minimumOnsetDeltaSeconds;
        }).begin();
        onsetsInSeconds.erase(new_end, onsetsInSeconds.end());	// erase from new end to original end
    }
}

void subdivideGap(std::vector<float> &onsets,
                         const size_t startIdx,
                         const size_t endIdx,
                         const int numOnsetsToInsert,
                         const double lengthInSeconds) {
    const float start = onsets[startIdx];
    const float end = [onsets, endIdx, lengthInSeconds]() {
        if (endIdx < onsets.size())
            return onsets[endIdx];
        assert (endIdx == onsets.size()); // why would we possibly get an end index even greater than the length of the vector?
        return static_cast<float>(lengthInSeconds);
    }();

    assert(start <= end);

    const auto newOnsets = std::views::iota(1, numOnsetsToInsert + 1)
                   | std::views::transform([=](const int i) {
                         return start + (end - start) * static_cast<float>(i) / (numOnsetsToInsert + 1);
                     });

    onsets.insert(onsets.begin() + startIdx + 1,
                  newOnsets.begin(),
                  newOnsets.end());
}

void forceMinimumOnsets(std::vector<float> &onsets, const int minOnsets, const double lengthInSeconds) {
    // Handle empty case
    if (onsets.empty()) {
        // Create evenly spaced onsets from 0.0 to 1.0 (assuming normalized)
        for (int i = 0; i < minOnsets; ++i) {
            onsets.push_back(static_cast<float>(i) / (minOnsets - 1));
        }
        return;
    }

    // If we already have the minimum number of onsets, just return
    if (onsets.size() >= minOnsets) {
        return;
    }

    // Find the largest gap and subdivide it repeatedly until we have minOnsets
    while (onsets.size() < minOnsets) {
        // Find the largest gap
        size_t largestGapIdx = 0;
        float largestGapSize = 0.0f;

        for (size_t i = 0; i < onsets.size() - 1; ++i) {
            if (const float gapSize = onsets[i + 1] - onsets[i];
                gapSize > largestGapSize)
            {
                largestGapSize = gapSize;
                largestGapIdx = i;
            }
        }

        // Insert one onset in the middle of the largest gap
        subdivideGap(onsets, largestGapIdx, largestGapIdx + 1, 1, lengthInSeconds);
    }
}

void equalizeOnsetDensity(std::vector<float> &onsets, double lengthInSeconds) {
    /// -get onsetDiffs (a.k.a. the event lengths)
    const std::vector<float> onsetDiffs = [onsets]() {
        std::vector<float> result;
        result.reserve(onsets.size());
        for (size_t i = 0; i < onsets.size() - 1; ++i) {
            result.push_back(onsets[i+1] - onsets[i]);
        }
        return result;
    }();

    // detect length outliers. method for now: IQR?
    const auto [lowerBound, upperBound] = [onsets]() {
        /// -take 25th percentile (Q1) and 75th percentile (Q3) of lengths
        const float Q1 = essentia::percentile(onsets, 25.f);
        const float Q3 = essentia::percentile(onsets, 75.f);
        /// -set IQR = Q3 - Q1
        const float IQR = Q3 - Q1;
        /// -set lowerBound = Q1 - 1.5*IQR
        /// --this could actually be unused; or, we could possibly use it to REDUCE the density of certain portions
        const float LB = Q1 - 1.5*IQR;
        /// -set upperBound = Q3 + 1.5*IQR
        const float UB = Q3 + 1.5*IQR;
        return std::make_pair(LB, UB);
    }();

    /// -take 50th percentile (median); set this to the targetDensity for too-sparse sections
    const float medianDiff = essentia::median(onsetDiffs);

    for (size_t i = 0; i < onsets.size() - 1; /* increment depends on current onset */) {
    /// -if the corresponding onsetDiff (length) exceeds upperBound:
        if (const float currentDiff = onsets[i+1] - onsets[i];
            currentDiff > upperBound)
        {
    /// --set numOnsetsToInsert = int(currentDiff / medianDiff) - 1
            const int numOnsetsToInsert = static_cast<int>(currentDiff / medianDiff) - 1;
    /// --use subdivideGap to insert numOnsetsToInsert - 1 new onsets between the current and next onset
            subdivideGap(onsets, i, i+1, numOnsetsToInsert, lengthInSeconds);

    /// --skip over newly inserted onsets
            i += numOnsetsToInsert + 1;
        } else {
            ++i;
        }
    }
}

void normalizeOnsets(std::vector<float> &onsetsInSeconds, const double lengthInSeconds) {
    std::ranges::transform(onsetsInSeconds,	// formerly terminated via begin() + numProperOnsets
                           onsetsInSeconds.begin(),
                           [lengthInSeconds](const double f)
                           {
                               const double res = f / lengthInSeconds;
                               assert(res <= 1.0);
                               return res;
                           });
}

void denormalizeOnsets(std::vector<float> &normalizedOnsets, const double lengthInSeconds) {
    std::ranges::transform(normalizedOnsets, normalizedOnsets.begin(),
                          [lengthInSeconds](const double f) {
                              return f * lengthInSeconds;
                          });
}

namespace {
float rmsEnergy(const std::span<const float> wave, const int centerSample, const int halfWindow) {
    const int start = std::max(0, centerSample - halfWindow);
    const int end   = std::min(static_cast<int>(wave.size()) - 1, centerSample + halfWindow);
    float sum = 0.0f;
    for (int i = start; i <= end; ++i) {
        sum += wave[i] * wave[i];
    }
    return std::sqrt(sum / (end - start + 1));
}
std::vector<float> movingAverage(const std::span<const float>  energy, const int smoothingFrames) {
    const int numFrames = static_cast<int>(energy.size());
    const int halfWindow = smoothingFrames / 2;
    std::vector<float> smoothed(numFrames, 0.0f);
    for (int f = 0; f < numFrames; ++f) {
        const int start = std::max(0, f - halfWindow);
        const int end   = std::min(numFrames - 1, f + halfWindow);
        float sum = 0.0f;
        for (int k = start; k <= end; ++k) {
            sum += energy[k];
        }
        smoothed[f] = sum / static_cast<float>(end - start + 1);
    }
    return smoothed;
}
}


void improveOnsetsInSeconds(
    std::vector<float>& onsetsInSeconds,
    const std::span<const float> wave,
    const float          sampleRate,
    const float          searchBackMs,  // how far back to look for pre-onset silence
    const float          rmsWindowMs,   // RMS analysis window size
    const bool           giveChanceBeforeStart
)
{
    if (wave.empty() || onsetsInSeconds.empty()) return;

    std::vector<float> improved = onsetsInSeconds;

    const size_t numSamples  = wave.size();
    const int rmsHalfWin     = static_cast<int>(sampleRate * rmsWindowMs   / 1000.0f / 2.0f);
    const int searchBackSmp  = static_cast<int>(sampleRate * searchBackMs  / 1000.0f);

    for (int i = 0; i < static_cast<int>(onsetsInSeconds.size()); ++i) {
        const int onsetSample = static_cast<int>(onsetsInSeconds[i] * sampleRate);

        if (onsetSample < 0 || onsetSample >= numSamples) {
            continue;
        }

        // ------------------ Determine SEARCH REGION ------------------
        // avoid searching back past the previous onset
        int searchStart = onsetSample - searchBackSmp;
        if (i > 0) {
            int prevOnsetSample = static_cast<int>(onsetsInSeconds[i - 1] * sampleRate);
            searchStart = std::max(searchStart, prevOnsetSample);
        }
        searchStart = std::max(searchStart, 0);

        if (searchStart >= onsetSample) {
            continue; // no room to search
        }

        // --- FORWARD SCAN from searchStart to find where signal crosses threshold ---
        int correctedSample = onsetSample; // fallback: keep original
        [[maybe_unused]] const float originalEnergy = rmsEnergy(wave, onsetSample, rmsHalfWin);
        float minEnergy = std::numeric_limits<float>::max();

        for (int s = searchStart; s < onsetSample; s += rmsHalfWin) {
            if (const float e = rmsEnergy(wave, s, rmsHalfWin);
                e < minEnergy && e < originalEnergy)
            {
                minEnergy = e;
                correctedSample = s;
            }
        }
        if (i == 0 && giveChanceBeforeStart) {
            if (correctedSample == onsetSample) { // then no improvement has been made to first onset
                if (0.0 < minEnergy) { // 0.0 would be the idealized energy level before start of file
                    correctedSample = 0;    // move first onset to start of file
                }
            }
        }

        improved[i] = static_cast<float>(correctedSample) / sampleRate;
    }

    onsetsInSeconds = improved;
}

void subdivideOnsetsNaive(std::vector<float>& onsetsInSeconds, const std::span<const float>  wave, float sampleRate, unsigned int numSubsections) {
    if (numSubsections <= 1) return;

    const std::vector<float> original = onsetsInSeconds;
    std::vector<float> result;
    result.reserve(original.size() * numSubsections);

    const int totalSamples = static_cast<int>(wave.size());

    for (int i = 0; i < static_cast<int>(original.size()); ++i) {
        result.push_back(original[i]);

        const float segmentStart = original[i];
        const float segmentEnd   = (i + 1 < static_cast<int>(original.size()))
                                    ? original[i + 1]
                                    : static_cast<float>(totalSamples) / sampleRate;

        const float segmentDuration = segmentEnd - segmentStart;
        const float subDuration     = segmentDuration / static_cast<float>(numSubsections);

        for (unsigned int j = 1; j < numSubsections; ++j) {
            result.push_back(segmentStart + static_cast<float>(j) * subDuration);
        }
    }

    onsetsInSeconds = result;
}

void subdivideOnsetsEnergy(std::vector<float>& onsetsInSeconds, const std::span<const float>  wave,
    const float sampleRate, const unsigned int numSubsections, const float rmsHopProportion,
    const float minimumSubdivisionLengthMs)
{
    if (numSubsections <= 1) return;

    const std::vector<float> originalOnsets = onsetsInSeconds;
    const int totalSamples = static_cast<int>(wave.size());

    std::vector<float> resultOnsets;
    resultOnsets.reserve(originalOnsets.size() * numSubsections);

    const float minSubdivLengthSamples = (minimumSubdivisionLengthMs / 1000.f) * sampleRate;

    for (int i = 0; i < static_cast<int>(originalOnsets.size()); ++i) {
        const auto currentOnsetSeconds = originalOnsets[i];
        resultOnsets.push_back(currentOnsetSeconds);

        const int segStartSample = static_cast<int>(currentOnsetSeconds * sampleRate);
        const int segEndSample   = (i + 1 < static_cast<int>(originalOnsets.size()))
                                     ? static_cast<int>(originalOnsets[i + 1] * sampleRate)
                                     : totalSamples;

        if (segEndSample <= segStartSample) continue;

        const int segLengthSamples = segEndSample - segStartSample;
        if (segLengthSamples < minSubdivLengthSamples) {
            continue;
        }
        const int rmsHopSamples    = std::max(1, static_cast<int>(segLengthSamples * rmsHopProportion));

        // build RMS energy envelope over segment
        const int rmsHalfWin = rmsHopSamples / 2;

        std::vector<float> energy;
        for (int s = segStartSample; s < segEndSample; s += rmsHopSamples) {
            energy.push_back(rmsEnergy(wave, s, rmsHalfWin));
        }
        if (energy.size() < 2) { continue; }

        // --- build cumulative energy ---
        std::vector<float> cumulative(energy.size());
        cumulative[0] = energy[0];
        for (int j = 1; j < static_cast<int>(energy.size()); ++j) {
            cumulative[j] = cumulative[j - 1] + energy[j];
        }

        const float totalEnergy = cumulative.back();
        if (totalEnergy <= 0.0f) { continue; }

        // place boundaries at equal cumulative energy percentiles
        float lastOnsetSeconds = currentOnsetSeconds;
        for (unsigned int cut = 1; cut < numSubsections; ++cut) {

            const int cutSample = [totalEnergy, cut, numSubsections, &cumulative, segStartSample, rmsHopSamples]() {
                const float targetEnergy = totalEnergy * (static_cast<float>(cut) / static_cast<float>(numSubsections));

                const auto it = std::ranges::lower_bound(cumulative, targetEnergy);
                const int frameIndex = static_cast<int>(std::distance(cumulative.begin(), it));

                return segStartSample + frameIndex * rmsHopSamples;
            }();
            const float cutTimeSeconds = static_cast<float>(cutSample) / sampleRate;

            {
                const float ostensibleCutLengthSeconds = cutTimeSeconds - lastOnsetSeconds;

                if (ostensibleCutLengthSeconds < (minimumSubdivisionLengthMs * 0.001f)) {
                    continue;
                }
            }
            if constexpr (false) {  // never apparently reach this condition anyway
                const auto nextOnsetSeconds = i + 1 < originalOnsets.size() ?
                                                            originalOnsets[i + 1] :
                                                                totalSamples / sampleRate;
                if (const float nextOstensibleLenthSeconds = nextOnsetSeconds - cutTimeSeconds;
                    nextOstensibleLenthSeconds < (minimumSubdivisionLengthMs * 0.001f))
                {
                    continue;
                }
            }

            if (cutTimeSeconds > currentOnsetSeconds &&
                cutSample < segEndSample )
            {
                resultOnsets.push_back(cutTimeSeconds);
                lastOnsetSeconds = cutTimeSeconds;
            }
        }
    }

    assert( std::ranges::is_sorted(resultOnsets) );
    onsetsInSeconds = resultOnsets;
}

SilenceTimings detectSilences(const std::span<const float>  wave, const float sampleRate,
    const float silenceThresholdDb,
    const float minSilenceDurationMs,
    const float minEventDurationMs,
    const float rmsHopMs,
    const float rmsAveragingWindowLength)
{
    const float silenceThresholdLinear = std::pow(10.0f, silenceThresholdDb / 20.0f);
    const float exitThreshold          = silenceThresholdLinear * 2.0f;
    const int   totalSamples           = static_cast<int>(wave.size());
    const int   rmsHopSamples          = std::max(1, static_cast<int>(rmsHopMs / 1000.0f * sampleRate));
    const int   rmsHalfWin             = rmsHopSamples / 2;
    const int   minSilenceFrames       = std::max(1, static_cast<int>((minSilenceDurationMs / 1000.0f * sampleRate) / rmsHopSamples));
    const int   minEventFrames         = std::max(1, static_cast<int>((minEventDurationMs   / 1000.0f * sampleRate) / rmsHopSamples));

    std::vector<float> energy;
    energy.reserve(totalSamples / rmsHopSamples + 1);
    for (int s = 0; s < totalSamples; s += rmsHopSamples) {
        energy.push_back(rmsEnergy(wave, s, rmsHalfWin));
    }
    const auto numFramesToAverage = static_cast<int>(std::max(1.f, std::round(rmsAveragingWindowLength / rmsHopMs)));
    energy = movingAverage(energy, numFramesToAverage);

    const int numFrames = static_cast<int>(energy.size());
    int frameIdx = 0;
    int lastSilenceEndFrame = -minEventFrames;

    SilenceTimings silenceTimings;

    silenceTimings.silenceOnsets.reserve(numFrames);
    silenceTimings.silenceOffsets.reserve(numFrames);

    while (frameIdx < numFrames) {
        if (energy[frameIdx] >= silenceThresholdLinear) {
            ++frameIdx;
            continue;
        }

        const int silenceStartFrame = frameIdx;
        while (frameIdx < numFrames && energy[frameIdx] < exitThreshold)
            ++frameIdx;
        const int silenceEndFrame = frameIdx - 1;   // minus 1 to try to prevent delayed reaction

        if (silenceEndFrame - silenceStartFrame < minSilenceFrames) continue;
        if (silenceStartFrame - lastSilenceEndFrame < minEventFrames) continue;

        const float silenceStartTime = static_cast<float>(silenceStartFrame * rmsHopSamples) / sampleRate;
        const float silenceEndTime   = static_cast<float>((silenceEndFrame - 1)   * rmsHopSamples) / sampleRate;

        silenceTimings.silenceOnsets.push_back(silenceStartTime);
        silenceTimings.silenceOffsets.push_back(silenceEndTime);

        lastSilenceEndFrame = silenceEndFrame;
    }

    return silenceTimings;
}

void combineOnsetsAndSilenceTimings(std::vector<float>& onsetsInSeconds, const SilenceTimings& silenceTimings,
    const float minDeltaOnsetToSilenceOnsetSeconds, const float minDeltaSilenceOnsetToOnsetSeconds,
    const float minDeltaOnsetToSilenceOffsetSeconds, const float minDeltaSilenceOffsetToOnsetSeconds)
{
    std::vector<float> result;
    result.reserve(onsetsInSeconds.size() + silenceTimings.silenceOnsets.size() * 2);

    const auto& so = silenceTimings.silenceOnsets;
    const auto& sf = silenceTimings.silenceOffsets;

    // insert all silence onsets and offsets in [timeA, timeB)
    const auto insertSilencesInRange = [&](const float timeA, const float timeB) {
        const auto soBegin = std::ranges::lower_bound(so, timeA);
        const auto soEnd   = std::ranges::lower_bound(so.begin(), so.end(), timeB);
        const auto sfBegin = std::ranges::lower_bound(sf.begin(), sf.end(), timeA);
        const auto sfEnd   = std::ranges::lower_bound(sf.begin(), sf.end(), timeB);

        // Merge the two ranges in order
        auto soIt = soBegin;
        auto sfIt = sfBegin;
        while (soIt != soEnd || sfIt != sfEnd) {
            const bool takeSilenceOnset = (sfIt == sfEnd) || (soIt != soEnd && *soIt <= *sfIt);
            if (takeSilenceOnset) {
                const auto silenceOnset = *soIt++;
                if (std::abs(timeA - silenceOnset) >=  minDeltaOnsetToSilenceOnsetSeconds &&
                     std::abs(silenceOnset - timeB) >= minDeltaSilenceOnsetToOnsetSeconds)
                {
                    result.push_back(silenceOnset);
                }
            }
            else {
                const auto silenceOffset = *sfIt++;
                if (std::abs(timeA - silenceOffset) >= minDeltaOnsetToSilenceOffsetSeconds &&
                    std::abs(silenceOffset - timeB) >= minDeltaSilenceOffsetToOnsetSeconds )
                {
                    result.push_back(silenceOffset);
                }
            }
        }
    };

    // check for silences before the first onset
    if (!onsetsInSeconds.empty())
        insertSilencesInRange(0.0f, onsetsInSeconds.front());

    // iterate through onsets
    for (int i = 0; i < static_cast<int>(onsetsInSeconds.size()); ++i) {
        result.push_back(onsetsInSeconds[i]);

        const float rangeStart = onsetsInSeconds[i];
        const float rangeEnd   = (i + 1 < static_cast<int>(onsetsInSeconds.size()))
                                   ? onsetsInSeconds[i + 1]
                                   : std::numeric_limits<float>::max();

        insertSilencesInRange(rangeStart, rangeEnd);
    }

    onsetsInSeconds = result;
}

}
