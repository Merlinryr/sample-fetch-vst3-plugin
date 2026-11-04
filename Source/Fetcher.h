#pragma once
#include <juce_core/juce_core.h>
#include "Analysis.h"

// Installs the external tools (yt-dlp, deno, ffmpeg) and turns a URL into a WAV, on its own thread.
// Owned by the processor and polled by the editor, so a download keeps going while the plugin window is closed.
class Fetcher : private juce::Thread
{
public:
    Fetcher() : juce::Thread ("SampleFetch") {}
    ~Fetcher() override;

    void fetch (const juce::String& link) { begin (link, false); }
    void updateTools()                    { begin ({}, true); }
    bool isBusy() const                   { return isThreadRunning(); }

    struct Snapshot { double progress; juce::String status; juce::File result; Analysis analysis; };
    Snapshot snapshot() const;

    // Renames the last result; returns false if the name is invalid or taken.
    bool renameResult (const juce::String& newName);

    // Moves the last result into Live's User Library so it appears in Live's browser. Reports via status.
    void sendToLive();
    static juce::File liveSamplesDir();

    // "Daft Punk - Harder, Better (Official)" -> "daft_punk_harder_better_official"
    static juce::String formatTitle (const juce::String& title);

    static juce::File outputDir();
    static juce::File toolsDir();

private:
    void begin (const juce::String& url, bool reinstall);
    void run() override;
    juce::String installTools();
    juce::String fetchWav();
    int runProcess (const juce::StringArray& cmd, const std::function<void (const juce::String&)>& onLine, juce::String& lastError);
    void set (double progress, const juce::String& status);

    juce::String url;
    bool reinstall = false;

    mutable juce::CriticalSection lock;
    double progress = 0.0;
    juce::String status { "Paste a YouTube link" };
    juce::File result;
    Analysis analysis;

    juce::CriticalSection procLock;
    juce::ChildProcess* current = nullptr; // so the destructor can kill a running tool
};
