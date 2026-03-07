/*
  ==============================================================================

    Statistics.h
    Created: 2 Jul 2025 8:35:27pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include "util.h"

namespace nvs::analysis {

enum class Statistic {
	Mean,
	Median,
	Variance,
	Skewness,
	Kurtosis,
	NumStatistics
};
typedef nvs::util::Iterator<Statistic, Statistic::Mean, Statistic::Kurtosis> statisticIterator;

template <typename T>
struct EventwiseStatistics {
	T mean		{};
	T median	{};
	T variance	{};
	T skewness	{};
	T kurtosis	{};
};

using EventwiseStatisticsF = EventwiseStatistics<float>;

template <typename T>
T getStatVal(const EventwiseStatistics<T> &stats, const Statistic stat) {
    switch (stat) {
        case Statistic::Mean: {
            return stats.mean;
        }
        case Statistic::Median: {
            return stats.median;
        }
        case Statistic::Variance: {
            return stats.variance;
        }
        case Statistic::Skewness: {
            return stats.skewness;
        }
        case Statistic::Kurtosis: {
            return stats.kurtosis;
        }
        [[unlikely]]
        default: {
            assert(false);
            return T{};
        }
    }
}

}
