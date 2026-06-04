# Adding a New (Essentia-based) Timbral Feature

1. Register strings via `STRAXIOMIZE` in StringAxiom.h (feature name, parameter names)
2. Update `Feature_e` enum in Features.h (after BFCCs, before Periodicity)
3. Add `OPAQUE_SETTINGS` definition in ModernSettingsTypes.h
4. Instantiate Essentia algorithm in `calculateTimbres` in TimbreAnalysis.cpp
5. Add frame-by-frame computation in the loop of `calculateTimbres` in TimbreAnalysis.cpp, storing result in timbres[Feature_e::MyFeature]
