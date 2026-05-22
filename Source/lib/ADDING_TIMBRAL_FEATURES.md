# Adding a New (Essentia-based) Timbral Feature

1. Register strings in StringAxiom.h if needed (parameter names, feature name)
2. Add feature to Feature_e enum in Features.h (after BFCCs, before Periodicity)
3. Update NumTimbralFeatures assertion in Features.h accordingly
4. Add settings struct to AnalyzerSettings in Settings.h (e.g., struct MyFeature { ... })
5. Create spec map in Settings.cpp (e.g., const std::map<juce::String, AnySpec> myFeatureSpecs)
6. Register spec map in specsByBranch in Settings.cpp
7. Add ValueTree node in createParentTreeFromSettings in Settings.cpp
8. Add settings extraction in updateSettingsFromValueTree in Settings.cpp
9. Update extern declarations in Settings.h to include new spec
10. Instantiate Essentia algorithm in calculateTimbres in TimbreAnalysis.cpp
11. Add frame-by-frame computation in the loop in TimbreAnalysis.cpp, storing result in timbres[Feature_e::MyFeature]
