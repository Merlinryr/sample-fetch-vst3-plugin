# SampleFetch

A VST3 plugin for Ableton Live (macOS and Windows) that turns a YouTube link into a ready-to-use sample:

1. Paste a link, click **Download**, and follow the progress bar.
2. The audio is converted to a 44.1 kHz / 16-bit WAV.
3. BPM and key are detected. The key is shown in Camelot notation (e.g. `8A`).
4. The file is named `daft_punk_harder_better_faster_stronger_123bpm_F#m.wav`, so you can filter by tempo or key in Live's browser.
5. **Send to Live**: click it to move the WAV into Live's User Library (**Samples → SampleFetch**) so it shows up in Live's browser, or drag the button straight onto a track.

New WAVs are saved to `~/Music/SampleFetch` until you send them. On first use the plugin downloads its tools: yt-dlp, Deno and ffmpeg. If downloads start failing, click **Update tools**.

## Install

Download the zip for your OS from the latest [GitHub Actions run](../../actions).

- **macOS:** unzip **SampleFetch-macOS** and double-click `SampleFetch-macOS-Installer.pkg`. The installer isn't notarized, so the first time macOS blocks it. Go to **System Settings → Privacy & Security**, click **Open Anyway** next to the SampleFetch message, then enter your password. The plugin is installed to `/Library/Audio/Plug-Ins/VST3/`.
- **Windows:** put `SampleFetch.vst3` in `C:\Program Files\Common Files\VST3\`.

Then in Live: **Settings → Plug-Ins → Use VST3 Plug-In System Folders → Rescan**.

## Build

You need CMake ≥ 3.22 and either Visual Studio 2022 or the Xcode command-line tools. JUCE and SoundTouch are fetched automatically.

```bash
cmake -B build && cmake --build build --config Release
```

## Legal

Downloading from YouTube breaks YouTube's Terms of Service. Clearing samples is your responsibility. JUCE is used under AGPLv3, SoundTouch under LGPL, and ffmpeg is GPL.
