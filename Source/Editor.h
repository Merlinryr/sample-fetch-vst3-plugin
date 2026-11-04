#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Processor.h"

class Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit Editor (Processor&);
    ~Editor() override { stopTimer(); }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void applyName();

    Fetcher& fetcher;
    juce::TextEditor url, name;
    juce::TextButton download { "Download" }, openFolder { "Open folder" }, update { "Update tools" }, send { "Send to Live" };
    double progress = 0.0;
    juce::ProgressBar bar { progress };
    juce::Label status, bpmLabel, keyLabel;
    juce::TooltipWindow tooltips { this };
    juce::File shown;
    bool dragged = false; // a drag of the Send button must not also count as a click
};
