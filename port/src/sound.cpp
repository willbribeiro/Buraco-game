#include "sound.h"

#include <algorithm>
#include <cmath>

namespace buraco::sound {
namespace {

constexpr double kTwoPi = 6.28318530717958647692;
constexpr double kBaseAmplitude = 0.6;   // comfortable but with plenty of headroom
constexpr double kGapSeconds = 0.006;    // silence between two tones

// Cue table, indexed by Event.  Frequencies stay in the range a PC speaker
// could produce and no cue is longer than a fraction of a second.
const std::vector<std::vector<Tone>>& cueTable() {
    static const std::vector<std::vector<Tone>> table = [] {
        std::vector<std::vector<Tone>> t;
        t.resize(size_t(kEventCount));
        t[int(Event::Shuffle)] = {{330, 45, 0.5}, {294, 45, 0.5}, {262, 65, 0.5}};
        t[int(Event::Deal)] = {{660, 35, 0.55}, {880, 35, 0.55}};
        t[int(Event::DrawDeck)] = {{880, 40, 0.5}};
        t[int(Event::TakePile)] = {{740, 40, 0.55}, {988, 55, 0.55}};
        t[int(Event::LayMeld)] = {{659, 40, 0.55}, {880, 55, 0.55}};
        t[int(Event::Discard)] = {{523, 45, 0.5}};
        t[int(Event::Canastra)] = {{659, 55, 0.55}, {880, 55, 0.55}, {1109, 75, 0.55}};
        t[int(Event::Morto)] = {{440, 50, 0.55}, {659, 50, 0.55}, {880, 65, 0.55}};
        t[int(Event::Batida)] = {
            {523, 60, 0.55}, {659, 60, 0.55}, {784, 60, 0.55}, {1047, 90, 0.55}};
        t[int(Event::Error)] = {{196, 140, 0.45}, {165, 160, 0.45}};
        return t;
    }();
    return table;
}

const std::vector<Tone>& emptyCue() {
    static const std::vector<Tone> empty;
    return empty;
}

// Short attack and release so a tone never starts or ends with a click.
double envelope(double t, double duration) {
    const double attack = std::min(0.003, duration * 0.25);
    const double release = std::min(0.010, duration * 0.25);
    if (attack > 0.0 && t < attack) return t / attack;
    const double left = duration - t;
    if (release > 0.0 && left < release) return std::max(0.0, left / release);
    return 1.0;
}

}  // namespace

const char* name(Event e) {
    switch (e) {
        case Event::Shuffle: return "shuffle";
        case Event::Deal: return "deal";
        case Event::DrawDeck: return "draw";
        case Event::TakePile: return "take-pile";
        case Event::LayMeld: return "meld";
        case Event::Discard: return "discard";
        case Event::Canastra: return "canastra";
        case Event::Morto: return "morto";
        case Event::Batida: return "batida";
        case Event::Error: return "error";
        case Event::Count: break;
    }
    return "?";
}

const std::vector<Tone>& cue(Event e) {
    const int index = int(e);
    if (index < 0 || index >= kEventCount) return emptyCue();
    return cueTable()[size_t(index)];
}

void Synth::setSampleRate(int hz) {
    if (hz > 0) sampleRate_ = hz;
}

void Synth::setVolume(int percent) {
    volume_ = std::clamp(percent, 0, 100);
}

void Synth::play(const std::vector<Tone>& tones) {
    for (const Tone& tone : tones) {
        if (tone.ms <= 0.0) continue;
        pending_.push_back(Pending{tone, 0.0});
    }
}

void Synth::play(Event e) { play(cue(e)); }

void Synth::clear() {
    pending_.clear();
    gapLeft_ = 0.0;
}

void Synth::render(int16_t* out, int frames) {
    if (!out || frames <= 0) return;

    const double step = 1.0 / double(sampleRate_);
    const double scale =
        enabled_ ? (double(volume_) / 100.0) * kBaseAmplitude * 32767.0 : 0.0;

    for (int i = 0; i < frames; ++i) {
        double sample = 0.0;

        if (gapLeft_ > 0.0) {
            gapLeft_ -= step;
        } else if (!pending_.empty()) {
            Pending& p = pending_.front();
            const double duration = std::max(0.001, p.tone.ms / 1000.0);
            if (p.tone.freqHz > 0.0 && p.elapsed < duration) {
                const double env = envelope(p.elapsed, duration);
                sample = std::sin(kTwoPi * p.tone.freqHz * p.elapsed) * env * p.tone.gain;
            }
            p.elapsed += step;
            if (p.elapsed >= duration) {
                pending_.pop_front();
                gapLeft_ = kGapSeconds;
            }
        }

        const double value = sample * scale;
        out[i] = int16_t(std::lround(std::clamp(value, -32768.0, 32767.0)));
    }
}

}  // namespace buraco::sound
