// SampleFetchTests              offline checks: Camelot table + BPM/key on a synthetic loop
// SampleFetchTests --network    also runs the real pipeline on a 19 s YouTube video
// SampleFetchTests file.wav     prints the analysis of a WAV
#include "../Source/Fetcher.h"
#include <juce_events/juce_events.h>
#include <iostream>

using namespace juce;

static int failures = 0;
static void check (bool ok, const String& what)
{
    std::cout << (ok ? "  ok   " : "  FAIL ") << what << "\n";
    failures += ok ? 0 : 1;
}

static void offline()
{
    check (camelot (0, false) == "8B", "C = 8B");
    check (camelot (9, true) == "8A", "Am = 8A");
    check (camelot (6, true) == "11A", "F#m = 11A");
    check (camelot (1, false) == "3B", "Db = 3B");
    check (camelot (4, true) == "9A", "Em = 9A");
    check (camelot (5, false) == "7B", "F = 7B");
    check (keyName (9, true) == "Am", "keyName Am");

    check (Fetcher::formatTitle ("Daft Punk - Harder, Better (Official Video)") == "daft_punk_harder_better_official_video", "title: spaces/punctuation");
    check (Fetcher::formatTitle ("  Don't Stop  ") == "dont_stop", "title: apostrophe, trim");
    check (Fetcher::formatTitle (String::fromUTF8 ("Caf\xc3\xa9 Del Mar")) == String::fromUTF8 ("caf\xc3\xa9_del_mar"), "title: accents kept");
    check (Fetcher::formatTitle ("!!!").isEmpty(), "title: nothing usable");

    // 30 s loop: kick on every beat at 128 BPM over a sustained A minor chord
    const double sr = 44100, beat = sr * 60.0 / 128.0;
    std::vector<float> x ((size_t) (sr * 30));
    const double notes[] = { 110.0, 220.0, 261.63, 329.63 }; // A2, A3, C4, E4
    for (size_t i = 0; i < x.size(); ++i)
    {
        const double t = (double) i / sr, sinceBeat = std::fmod ((double) i, beat) / sr;
        double v = 0.8 * std::sin (2 * MathConstants<double>::pi * 60 * sinceBeat) * std::exp (-sinceBeat * 25);
        for (auto f : notes)
            v += 0.08 * std::sin (2 * MathConstants<double>::pi * f * t) + 0.03 * std::sin (2 * MathConstants<double>::pi * 2 * f * t);
        x[i] = (float) v;
    }
    const auto a = analyse (x.data(), (int) x.size(), sr);
    check (std::abs (a.bpm - 128) <= 1, "synthetic loop BPM = " + String (a.bpm) + " (want 128)");
    check (a.key == "Am" && a.camelot == "8A", "synthetic loop key = " + a.key + " " + a.camelot + " (want Am 8A)");
}

static Fetcher::Snapshot waitFor (Fetcher& f)
{
    Thread::sleep (200);
    while (f.isBusy())
        Thread::sleep (200);
    return f.snapshot();
}

static void network()
{
    Fetcher f;
    f.fetch ("-o /tmp/evil"); // not a link: must be rejected before yt-dlp sees it
    check (waitFor (f).status.startsWith ("Error"), "non-link input rejected");

    f.fetch ("https://www.youtube.com/watch?v=jNQXAC9IVRw");
    auto s = waitFor (f);
    std::cout << "  " << s.status << " -> " << s.result.getFileName() << "\n";
    check (s.result.existsAsFile(), "WAV written");
    check (s.result.getFileName().startsWith ("me_at_the_zoo"), "file named after the formatted title");
    check (s.analysis.key.isEmpty() || s.result.getFileName().contains ("_" + s.analysis.key + "."), "key in file name");

    check (f.renameResult ("test_120bpm_8A"), "rename");
    s = f.snapshot();
    check (s.result.getFileName() == "test_120bpm_8A.wav" && s.result.existsAsFile(), "renamed file exists");

    // Without Live installed the library is missing: the file must stay put and the user be told.
    const bool hasLive = Fetcher::liveSamplesDir().getParentDirectory().getParentDirectory().isDirectory();
    f.sendToLive();
    s = f.snapshot();
    std::cout << "  " << s.status << "\n";
    if (hasLive)
        check (s.result.isAChildOf (Fetcher::liveSamplesDir()) && s.result.existsAsFile(), "sent to Live's User Library");
    else
        check (s.status.contains ("User Library not found") && s.result.existsAsFile(), "missing User Library reported, file kept");
    s.result.deleteFile();
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init; // URL streams want the message manager on some platforms

    if (argc > 1 && String (argv[1]) != "--network")
    {
        const auto a = analyseFile (File::getCurrentWorkingDirectory().getChildFile (String::fromUTF8 (argv[1])));
        std::cout << a.bpm << " BPM, " << a.key << " (" << a.camelot << ")\n";
        return 0;
    }

    offline();
    if (argc > 1)
        network();

    std::cout << (failures == 0 ? "PASS" : "FAILED") << "\n";
    return failures == 0 ? 0 : 1;
}
