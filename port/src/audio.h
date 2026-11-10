// SDL audio output for the synthesized sound effects.
//
// The device is opened without a callback: the main loop renders a few
// milliseconds ahead and hands the samples to SDL_QueueAudio.  That keeps the
// mixing single-threaded and the game loop in control of the latency.
#pragma once

#include "sound.h"

#include <cstdint>
#include <string>
#include <vector>

namespace audio {

class Output {
public:
    Output() = default;
    ~Output();

    Output(const Output&) = delete;
    Output& operator=(const Output&) = delete;

    // Opens the default audio device (S16 stereo, 48 kHz when available).
    // Failure is not fatal: the caller keeps running without sound.
    bool init(std::string* error = nullptr);
    void shutdown();

    bool active() const { return device_ != 0; }
    int sampleRate() const { return sampleRate_; }
    int channels() const { return channels_; }

    // Renders from the synth until roughly `targetMs` of audio is queued.
    // No-op when the device is closed or the synth has nothing to play.
    void pump(buraco::sound::Synth& synth, int targetMs = 60);

    // Milliseconds of audio still waiting in the device queue.
    int queuedMs() const;

private:
    uint32_t device_ = 0;  // SDL_AudioDeviceID
    int sampleRate_ = buraco::sound::Synth::kDefaultSampleRate;
    int channels_ = 2;
    std::vector<int16_t> mono_;
    std::vector<int16_t> interleaved_;
};

}  // namespace audio
