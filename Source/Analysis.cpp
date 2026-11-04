#include "Analysis.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <BPMDetect.h>

using namespace juce;

String keyName (int pc, bool minor)
{
    static const char* const names[] = { "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    return String (names[pc % 12]) + (minor ? "m" : "");
}

String camelot (int pc, bool minor)
{
    // Camelot steps by fifths (x7 semitones); C major = 8B, and a minor key shares its number with its relative major (+3).
    return String (((minor ? pc + 3 : pc) * 7 + 7) % 12 + 1) + (minor ? "A" : "B");
}

static int detectBpm (const float* x, int n, double sr)
{
    soundtouch::BPMDetect detector (1, (int) sr);
    for (int i = 0; i < n; i += 65536)
        detector.inputSamples (x + i, jmin (65536, n - i));
    return roundToInt (detector.getBpm()); // 0 when no beat was found
}

static double correlation (const double* a, const double* b)
{
    double ma = 0, mb = 0;
    for (int i = 0; i < 12; ++i) { ma += a[i] / 12; mb += b[i] / 12; }
    double ab = 0, aa = 0, bb = 0;
    for (int i = 0; i < 12; ++i)
    {
        ab += (a[i] - ma) * (b[i] - mb);
        aa += (a[i] - ma) * (a[i] - ma);
        bb += (b[i] - mb) * (b[i] - mb);
    }
    return aa > 0 && bb > 0 ? ab / std::sqrt (aa * bb) : 0;
}

// Chromagram (energy per pitch class) correlated against the Krumhansl-Kessler key profiles.
static void detectKey (const float* x, int n, double sr, Analysis& out)
{
    constexpr int order = 15, size = 1 << order; // ~0.74 s at 44.1 kHz: ~1.3 Hz bins, fine enough for bass notes
    if (n < size)
        return;

    std::vector<int> pcOfBin (size / 2, -1);
    for (int b = 1; b < size / 2; ++b)
    {
        const auto f = b * sr / size;
        if (f < 55.0 || f > 2000.0)
            continue;
        const auto pitch = 69.0 + 12.0 * std::log2 (f / 440.0);
        const auto note = std::round (pitch);
        if (std::abs (pitch - note) < 0.25) // skip bins between notes
            pcOfBin[(size_t) b] = ((int) note % 12 + 12) % 12;
    }

    dsp::FFT fft (order);
    dsp::WindowingFunction<float> window ((size_t) size, dsp::WindowingFunction<float>::hann, false);
    std::vector<float> buf (2 * size);
    double chroma[12] {};

    for (int start = 0; start + size <= n; start += size / 2)
    {
        std::copy (x + start, x + start + size, buf.begin());
        std::fill (buf.begin() + size, buf.end(), 0.0f);
        window.multiplyWithWindowingTable (buf.data(), (size_t) size);
        fft.performFrequencyOnlyForwardTransform (buf.data(), true);

        double frame[12] {}, sum = 0;
        for (int b = 1; b < size / 2; ++b)
            if (pcOfBin[(size_t) b] >= 0)
                frame[pcOfBin[(size_t) b]] += buf[(size_t) b];
        for (auto v : frame)
            sum += v;
        if (sum > 1e-3) // skip silence; normalise so loud sections don't outvote the rest
            for (int i = 0; i < 12; ++i)
                chroma[i] += frame[i] / sum;
    }

    static const double profiles[2][12] = {
        { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 }, // major
        { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 }, // minor
    };
    double best = 0;
    for (int minor = 0; minor < 2; ++minor)
        for (int tonic = 0; tonic < 12; ++tonic)
        {
            double rotated[12];
            for (int i = 0; i < 12; ++i)
                rotated[i] = chroma[(tonic + i) % 12];
            const auto r = correlation (rotated, profiles[minor]);
            if (r > best)
            {
                best = r;
                out.key = keyName (tonic, minor);
                out.camelot = camelot (tonic, minor);
            }
        }
}

Analysis analyse (const float* mono, int n, double sr)
{
    Analysis a;
    a.bpm = detectBpm (mono, n, sr);
    detectKey (mono, n, sr, a);
    return a;
}

Analysis analyseFile (const File& wav)
{
    WavAudioFormat format;
    std::unique_ptr<AudioFormatReader> reader (format.createReaderFor (new FileInputStream (wav), true));
    if (reader == nullptr || reader->lengthInSamples <= 0)
        return {};

    // shortcut: only the first 6 minutes are analysed to bound memory; raise it if long DJ mixes matter
    const auto n = (int) jmin (reader->lengthInSamples, (int64) (reader->sampleRate * 360));
    AudioBuffer<float> buf ((int) reader->numChannels, n);
    reader->read (&buf, 0, n, 0, true, true);
    for (int ch = 1; ch < buf.getNumChannels(); ++ch)
        buf.addFrom (0, 0, buf, ch, 0, n);
    buf.applyGain (0, 0, n, 1.0f / (float) buf.getNumChannels());

    return analyse (buf.getReadPointer (0), n, reader->sampleRate);
}
