# Adding a New Timbral Feature (Essentia-based)

1. Register strings in StringAxiom.h if needed (parameter names, feature name)
2. Add feature to Feature_e enum in Features.h (after BFCCs, before Periodicity)
3. Update NumTimbralFeatures assertion in Features.h
4. Add case to toString function in FeatureOperations.h
5. Add settings struct to AnalyzerSettings in Settings.h (e.g., struct MyFeature { ... })
6. Create spec map in Settings.cpp (e.g., const std::map<juce::String, AnySpec> myFeatureSpecs)
7. Register spec map in specsByBranch in Settings.cpp
8. Add ValueTree node in createParentTreeFromSettings in Settings.cpp
9. Add settings extraction in updateSettingsFromValueTree in Settings.cpp
10. Update extern declarations in Settings.h to include new spec
11. Instantiate Essentia algorithm in calculateTimbres in TimbreAnalysis.cpp
12. Add frame-by-frame computation in the loop in TimbreAnalysis.cpp, storing result in timbres[Feature_e::MyFeature]
