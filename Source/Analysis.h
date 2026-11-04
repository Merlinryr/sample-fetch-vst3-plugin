#pragma once
#include <juce_core/juce_core.h>

struct Analysis
{
    int bpm = 0;          // 0 = not detected
    juce::String key;     // "Am", "F#m", "Eb"... empty = not detected
    juce::String camelot; // "8A", "11A", "5B"...
};

Analysis analyse (const float* mono, int numSamples, double sampleRate);
Analysis analyseFile (const juce::File& wav);

juce::String keyName (int pitchClass, bool minor); // pitch class 0 = C
juce::String camelot (int pitchClass, bool minor);
