// Sound cues and a tiny software synthesizer.
//
// The port has no sampled audio: every effect is a short sequence of square-ish
// sine beeps, in the spirit of the PC speaker used by the original game.  This
// module deliberately avoids SDL so the cue table and the sample generation can
// be tested without a device; audio.cpp feeds rendered samples to SDL.
#pragma once

#include <cstdint>
#include <deque>
#include <vector>

namespace buraco::sound {

// Game moments that make a sound.  The list is intentionally small: a cue is
// only worth adding when the player can hear what happened.
enum class Event {
    Shuffle,    // dealing a new hand
    Deal,
    DrawDeck,   // taking a card from the monte
    TakePile,   // buying the whole discard pile
    LayMeld,    // laying or growing a meld
    Discard,
    Canastra,   // a meld just became a canastra
    Morto,      // taking the morto
    Batida,     // a hand was scored (someone went out)
    Error,      // a player action was rejected
    Count,
};

constexpr int kEventCount = int(Event::Count);

// Short lowercase name, used by tests and by --audio-test.
const char* name(Event e);

struct Tone {
    double freqHz = 0.0;  // 0 = silence
    double ms = 0.0;
    double gain = 1.0;    // 0..1 relative to the cue

    friend bool operator==(const Tone&, const Tone&) = default;
};

// Cue for one event, in playback order.  Never empty; an unknown event maps to
// an empty vector so callers can stay total.
const std::vector<Tone>& cue(Event e);

// Mixes queued tones into mono S16 samples.  Tones play one after another with
// a short silence between them; render() is deterministic for the same sample
// rate, volume and cue sequence.
class Synth {
public:
    static constexpr int kDefaultSampleRate = 48000;

    void setSampleRate(int hz);
    int sampleRate() const { return sampleRate_; }

    // Volume is a percentage; values outside 0..100 are clamped.
    void setVolume(int percent);
    int volume() const { return volume_; }
    void setEnabled(bool on) { enabled_ = on; }
    bool enabled() const { return enabled_; }

    void play(Event e);
    void play(const std::vector<Tone>& tones);

    // Drops everything still queued, including the inter-tone gap.
    void clear();

    // Fills `frames` samples.  Silence when muted, disabled or out of cues.
    void render(int16_t* out, int frames);

    // True when nothing is left to play (no tone and no trailing gap).
    bool idle() const { return pending_.empty() && gapLeft_ <= 0.0; }

private:
    struct Pending {
        Tone tone;
        double elapsed = 0.0;  // seconds already rendered
    };

    std::deque<Pending> pending_;
    int sampleRate_ = kDefaultSampleRate;
    int volume_ = 100;
    bool enabled_ = true;
    double gapLeft_ = 0.0;
};

}  // namespace buraco::sound
