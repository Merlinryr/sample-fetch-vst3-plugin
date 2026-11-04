#include "Fetcher.h"

using namespace juce;

namespace
{
struct Tool { const char* file; const char* url; const char* zipEntry; }; // zipEntry == nullptr: the URL is the binary itself

#define GH "https://github.com/"
#if JUCE_MAC && (defined (__aarch64__) || defined (__arm64__))
const Tool tools[] = {
    { "yt-dlp", GH "yt-dlp/yt-dlp/releases/latest/download/yt-dlp_macos", nullptr },
    { "deno",   GH "denoland/deno/releases/latest/download/deno-aarch64-apple-darwin.zip", "deno" },
    { "ffmpeg", "https://ffmpeg.martin-riedl.de/redirect/latest/macos/arm64/release/ffmpeg.zip", "ffmpeg" },
};
#elif JUCE_MAC
const Tool tools[] = {
    { "yt-dlp", GH "yt-dlp/yt-dlp/releases/latest/download/yt-dlp_macos", nullptr },
    { "deno",   GH "denoland/deno/releases/latest/download/deno-x86_64-apple-darwin.zip", "deno" },
    { "ffmpeg", "https://ffmpeg.martin-riedl.de/redirect/latest/macos/amd64/release/ffmpeg.zip", "ffmpeg" },
};
#elif JUCE_WINDOWS
const Tool tools[] = {
    { "yt-dlp.exe", GH "yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe", nullptr },
    { "deno.exe",   GH "denoland/deno/releases/latest/download/deno-x86_64-pc-windows-msvc.zip", "deno.exe" },
    { "ffmpeg.exe", GH "yt-dlp/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl.zip", "bin/ffmpeg.exe" },
};
#else
 #error "Only macOS and Windows are supported"
#endif
#undef GH

File tool (int i) { return Fetcher::toolsDir().getChildFile (tools[i].file); }
} // namespace

Fetcher::~Fetcher()
{
    signalThreadShouldExit();
    {
        const ScopedLock sl (procLock);
        if (current != nullptr)
            current->kill();
    }
    stopThread (10000);
}

File Fetcher::outputDir()
{
    return File::getSpecialLocation (File::userMusicDirectory).getChildFile ("SampleFetch");
}

String Fetcher::formatTitle (const String& title)
{
    String out;
    bool gap = false;
    for (auto p = title.toLowerCase().getCharPointer(); ! p.isEmpty();)
    {
        const auto c = p.getAndAdvance();
        if (c == '\'' || c == 0x2019) // "don't" -> "dont", not "don_t"
            continue;
        if (! CharacterFunctions::isLetterOrDigit (c))
        {
            gap = true; // spaces and punctuation collapse into one underscore
            continue;
        }
        if (gap && out.isNotEmpty())
            out << '_';
        gap = false;
        out << String::charToString (c);
    }
    return out.substring (0, 80);
}

File Fetcher::toolsDir()
{
    auto base = File::getSpecialLocation (File::userApplicationDataDirectory);
   #if JUCE_MAC
    base = base.getChildFile ("Application Support"); // JUCE gives ~/Library on macOS
   #endif
    return base.getChildFile ("SampleFetch/bin");
}

Fetcher::Snapshot Fetcher::snapshot() const
{
    const ScopedLock sl (lock);
    return { progress, status, result, analysis };
}

void Fetcher::set (double p, const String& s)
{
    const ScopedLock sl (lock);
    progress = p;
    status = s;
}

bool Fetcher::renameResult (const String& newName)
{
    auto name = File::createLegalFileName (newName.trim());
    if (name.isEmpty() || isBusy())
        return false;
    if (! name.endsWithIgnoreCase (".wav"))
        name << ".wav";

    const ScopedLock sl (lock);
    auto target = result.getSiblingFile (name);
    if (target == result)
        return true;
    if (! result.existsAsFile() || target.exists() || ! result.moveFileTo (target))
        return false;
    result = target;
    return true;
}

File Fetcher::liveSamplesDir()
{
    // shortcut: Live's default User Library location; parse Live's Library.cfg if a user has moved it
   #if JUCE_MAC
    const auto parent = File::getSpecialLocation (File::userMusicDirectory);
   #else
    const auto parent = File::getSpecialLocation (File::userDocumentsDirectory);
   #endif
    return parent.getChildFile ("Ableton/User Library/Samples/SampleFetch");
}

void Fetcher::sendToLive()
{
    if (isBusy())
        return;

    const auto dir = liveSamplesDir();
    const auto library = dir.getParentDirectory().getParentDirectory();
    const ScopedLock sl (lock);

    if (! result.existsAsFile())
        status = "Error: no file to send";
    else if (result.isAChildOf (dir))
        status = "Already in Live's browser";
    else if (! library.isDirectory())
        status = "Error: Live's User Library not found at " + library.getFullPathName();
    else if (! dir.createDirectory())
        status = "Error: can't create " + dir.getFullPathName();
    else
    {
        const auto target = dir.getNonexistentChildFile (result.getFileNameWithoutExtension(), ".wav", false);
        if (result.moveFileTo (target))
        {
            result = target;
            status = "In Live's browser: User Library > Samples > SampleFetch";
        }
        else
            status = "Error: can't move to " + target.getFullPathName();
    }
}

void Fetcher::begin (const String& u, bool reinstallTools)
{
    if (isThreadRunning())
        return;
    url = u.trim();
    reinstall = reinstallTools;
    if (! reinstall)
    {
        const ScopedLock sl (lock);
        result = File();
        analysis = {};
    }
    set (0.0, "Starting...");
    startThread();
}

void Fetcher::run()
{
    if (reinstall)
        toolsDir().deleteRecursively();

    auto error = installTools();
    if (error.isEmpty() && ! reinstall)
        error = fetchWav();

    if (error.isNotEmpty())
        set (0.0, "Error: " + error);
    else if (reinstall)
        set (1.0, "Tools updated");
}

String Fetcher::installTools()
{
    if (! toolsDir().createDirectory())
        return "can't create " + toolsDir().getFullPathName();

    const int count = (int) std::size (tools);
    for (int i = 0; i < count; ++i)
    {
        const auto& t = tools[i];
        auto dest = tool (i);
        if (dest.existsAsFile())
            continue;

        const String label = "Installing " + String (t.file) + " (" + String (i + 1) + "/" + String (count) + ")";
        set (0.0, label);

        auto part = dest.withFileExtension (".part");
        auto in = URL (t.url).createInputStream (URL::InputStreamOptions (URL::ParameterHandling::inAddress)
                                                   .withConnectionTimeoutMs (20000)
                                                   .withNumRedirectsToFollow (10));
        if (in == nullptr)
            return "can't reach " + String (t.url);

        const auto total = in->getTotalLength();
        int64 done = 0;
        {
            part.deleteFile();
            FileOutputStream out (part);
            if (out.failedToOpen())
                return "can't write " + part.getFullPathName();

            HeapBlock<char> buf (1 << 16);
            for (;;)
            {
                if (threadShouldExit())
                    return "cancelled";
                const auto n = in->read (buf, 1 << 16);
                if (n <= 0)
                    break;
                out.write (buf, (size_t) n);
                done += n;
                if (total > 0)
                    set ((double) done / (double) total, label);
            }
            out.flush();
            if (out.getStatus().failed())
                return out.getStatus().getErrorMessage();
        }
        if (done == 0 || (total > 0 && done != total))
            return "download of " + String (t.file) + " was incomplete";

        if (t.zipEntry == nullptr)
        {
            if (! part.moveFileTo (dest))
                return "can't install " + dest.getFullPathName();
        }
        else
        {
            bool extracted = false;
            {
                ZipFile zip (part);
                for (int e = 0; e < zip.getNumEntries() && ! extracted; ++e)
                {
                    if (! zip.getEntry (e)->filename.endsWith (t.zipEntry))
                        continue;
                    std::unique_ptr<InputStream> entry (zip.createStreamForEntry (e));
                    auto tmp = dest.withFileExtension (".new");
                    tmp.deleteFile();
                    {
                        FileOutputStream out (tmp);
                        extracted = entry != nullptr && ! out.failedToOpen()
                                    && out.writeFromInputStream (*entry, -1) > 0 && out.getStatus().wasOk();
                    }
                    extracted = extracted && tmp.moveFileTo (dest);
                }
            }
            part.deleteFile();
            if (! extracted)
                return "couldn't find " + String (t.zipEntry) + " in " + String (t.url);
        }

       #if JUCE_MAC
        dest.setExecutePermission (true);
        ChildProcess xattr; // clear quarantine flags so Gatekeeper doesn't block the unsigned tool
        if (xattr.start (StringArray { "/usr/bin/xattr", "-c", dest.getFullPathName() }))
            xattr.waitForProcessToFinish (5000);
       #endif
    }
    return {};
}

String Fetcher::fetchWav()
{
    if (! (url.startsWithIgnoreCase ("https://") || url.startsWithIgnoreCase ("http://")))
        return "paste a full link starting with https://";

    auto work = File::getSpecialLocation (File::tempDirectory).getChildFile ("SampleFetch");
    work.deleteRecursively();
    if (! work.createDirectory() || ! outputDir().createDirectory())
        return "can't create work folders";

    set (-1.0, "Looking up video..."); // yt-dlp takes a few seconds before the first progress line
    String downloaded, title, error;
    const StringArray ytdlp {
        tool (0).getFullPathName(),
        "--newline", "--progress", "--no-playlist", "--no-part",
        "--color", "never", "--encoding", "utf-8",
        "-f", "bestaudio/best",
        "--js-runtimes", "deno:" + tool (1).getFullPathName(),
        "--ffmpeg-location", tool (2).getFullPathName(),
        "--progress-template", "download:PROGRESS %(progress._percent_str)s",
        "-O", "after_move:FILE %(filepath)s",
        "-O", "after_move:TITLE %(title)s",
        "-o", work.getChildFile ("%(id)s.%(ext)s").getFullPathName(),
        "--", url // "--" so a link can never be read as an option
    };
    const auto ytCode = runProcess (ytdlp, [&] (const String& line)
    {
        if (line.startsWith ("PROGRESS "))
            set (line.substring (9).trim().getDoubleValue() / 100.0, "Downloading...");
        else if (line.startsWith ("FILE "))
            downloaded = line.substring (5).trim();
        else if (line.startsWith ("TITLE "))
            title = line.substring (6).trim();
    }, error);

    if (ytCode != 0 || ! File (downloaded).existsAsFile())
        return ytCode == -1 ? "cancelled" : (error.isNotEmpty() ? error : "yt-dlp failed (code " + String (ytCode) + ")");

    set (-1.0, "Converting to WAV...");
    const auto converted = work.getChildFile ("audio.wav");
    const StringArray ffmpeg {
        tool (2).getFullPathName(), "-y", "-hide_banner", "-loglevel", "error",
        "-i", downloaded, "-vn", "-ar", "44100", "-c:a", "pcm_s16le", converted.getFullPathName()
    };
    const auto ffCode = runProcess (ffmpeg, [] (const String&) {}, error);
    if (ffCode != 0 || ! converted.existsAsFile())
        return ffCode == -1 ? "cancelled" : (error.isNotEmpty() ? error : "ffmpeg failed (code " + String (ffCode) + ")");

    set (-1.0, "Detecting BPM and key...");
    const auto a = analyseFile (converted);

    // title_128bpm_Am.wav, so Live's browser search can filter by "128bpm" or "_Am"
    auto name = formatTitle (title);
    if (name.isEmpty())
        name = "sample";
    if (a.bpm > 0)
        name << "_" << a.bpm << "bpm";
    if (a.key.isNotEmpty())
        name << "_" << a.key;

    const auto wav = outputDir().getNonexistentChildFile (name, ".wav", false);
    const bool moved = converted.moveFileTo (wav);
    work.deleteRecursively();
    if (! moved)
        return "can't write " + wav.getFullPathName();

    const ScopedLock sl (lock);
    result = wav;
    analysis = a;
    progress = 1.0;
    status = "Ready - send or drag it to Live";
    return {};
}

int Fetcher::runProcess (const StringArray& cmd, const std::function<void (const String&)>& onLine, String& lastError)
{
    ChildProcess p;
    if (! p.start (cmd, ChildProcess::wantStdOut | ChildProcess::wantStdErr))
    {
        lastError = "can't start " + File (cmd[0]).getFileName();
        return -2;
    }
    {
        const ScopedLock sl (procLock);
        current = &p;
    }

    std::string pending;
    char buf[4096];
    auto emit = [&]
    {
        auto line = String::fromUTF8 (pending.data(), (int) pending.size()).trim();
        pending.clear();
        if (line.isEmpty())
            return;
        if (line.containsIgnoreCase ("error"))
            lastError = line;
        onLine (line);
    };

    for (;;)
    {
        const int n = p.readProcessOutput (buf, sizeof (buf));
        if (n <= 0)
            break;
        for (int i = 0; i < n; ++i)
        {
            if (buf[i] == '\n' || buf[i] == '\r')
                emit();
            else
                pending += buf[i];
        }
        if (threadShouldExit())
            p.kill();
    }
    emit();
    p.waitForProcessToFinish (5000);

    {
        const ScopedLock sl (procLock);
        current = nullptr;
    }
    return threadShouldExit() ? -1 : (int) p.getExitCode();
}
