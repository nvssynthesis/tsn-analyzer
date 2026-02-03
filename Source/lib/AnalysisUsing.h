/*
  ==============================================================================

    AnalysisUsing.h
    Created: 30 Oct 2023 2:35:10pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include "EssentiaSetup.h"

#include "essentia/streaming/algorithms/vectorinput.h"
#include "essentia/streaming/algorithms/vectoroutput.h"

#include "juce_core/juce_core.h"
#include "juce_data_structures/juce_data_structures.h"

namespace nvs::analysis {

//===================================================================================
using namespace essentia;
using namespace essentia::streaming;
using namespace essentia::scheduler;
//===================================================================================
using Real = float;
using vecReal = std::vector<Real>;
using vecVecReal = std::vector<vecReal>;
using vectorInput = VectorInput<Real> ;
using vectorInputCumulative = VectorInput<vecReal> ;
using vectorOutput = VectorOutput<Real>;
using vectorOutputCumulative = VectorOutput<vecReal> ;
using startAndEndTimesVec = std::pair<vecReal, vecReal> ;

using streamingFactory = streaming::AlgorithmFactory;
using standardFactory = standard::AlgorithmFactory;

using array2dReal = TNT::Array2D<Real>;

using File = juce::File;
using String = juce::String;
using StringArray = juce::StringArray;
using Logger = juce::Logger;
using ValueTree = juce::ValueTree;
using var = juce::var;

}   // namespace nvs::analysis
