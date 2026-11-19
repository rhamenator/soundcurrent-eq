// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "spectrogram/spectrogram_core.h"
#include <QByteArray>
#include <QJsonObject>
#include <QLocale>
#include <QVector>
#include <array>
#include <deque>
#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <vector>
namespace soundcurrent::analysis {
enum class View { Waterfall, Spectrum, Waveform, Tuner, Vectorscope, Average, Microphone };
struct Settings {
    int view=1, rate=48000, fft=4096, palette=0, scale=1, layout=0, channel=0, amplitude=0, instrument=0;
    double low=20, high=20000, sensitivity=90, history=15, speed=1, ceiling=0;
    bool grid=true, peaks=false, average=false, autoRange=true, notes=false, transients=false, music=false;
    double upper() const { return high==0?std::min(rate,48000)/2.:high; }
    bool valid() const;
    QJsonObject json() const;
    static std::optional<Settings> parse(const QJsonObject &);
    static std::optional<Settings> parsePreset(const QByteArray &);
};
std::vector<double> frequencyAxisTicks(const Settings &, double pixels, double minimumSpacing);
QString displayNumber(double value, int decimals=3, const QLocale &locale=QLocale());
QString formatFrequency(double hz, const QLocale &locale=QLocale());
std::vector<double> spectrogramColumn(const std::vector<double> &bins, const Settings &, int rows);
struct Pitch { double hz=0, cents=0, confidence=0; int midi=-1; };
// Chord estimated from the pitch-class energy of spectral peaks. quality indexes chordQualitySuffix().
struct Chord { int root=-1, quality=-1; double confidence=0; };
const char *chordQualitySuffix(int quality);
// What the Tuner can say: a note, a chord, sound without a clear pitch (music, noise), or nothing.
enum class TunerState { Silent, Note, Chord, Unclear };
// Notes heard in one analysis frame, for the scrolling note display. level is 0..1.
struct NoteEvent { int midi=0; float level=0; };
struct NoteFrame { double time=0; std::vector<NoteEvent> notes; };
class Engine {
public:
    explicit Engine(Settings settings={});
    void configure(const Settings &);
    void resetAverage();
    bool push(const QByteArray &stereo48k, View view);
    void captureReference();
    std::vector<double> relativeDb() const;
    bool hasReference() const { return !reference.empty(); }
    double frequencyAt(int bin) const { return double(bin)*settings.rate/settings.fft; }
    Settings settings;
    std::vector<double> left,right,dbLeft,dbRight,db,averaged,peak,reference;
    std::uint64_t frames=0, fftCalls=0, waveFrames=0;
    // Wall-clock seconds since the previous frame; the caller sets it so time constants do not depend on the frame rate.
    double frameSeconds=.05;
    double correlation=0, rms=0, fftMs=0;
    bool transient=false;
    Pitch pitch;
    Chord chord;
    std::array<double,12> chroma{};
    TunerState tunerState=TunerState::Silent;
    std::deque<NoteFrame> noteHistory;   // newest at the back; about a minute is kept
    double noteClock=0;
private:
    void analyzeChord();
    void updateTunerState();
    void collectNotes();
    Pitch heldPitch_;Chord heldChord_;double holdSeconds_=0;
    std::unique_ptr<gram::FftPlan> plan_;
    std::array<std::array<double,65>,256> kernel_{};
    bool kernelReady_=false;
    std::vector<double> meanPower_, averagePower_;
    double lastRms_=0;
    void prepareKernel();
    void analyzePitch(const QByteArray &);
};
}
