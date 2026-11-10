#include "audio.h"

#include <SDL.h>

#include <algorithm>

namespace audio {
namespace {

constexpr int kPreferredRate = 48000;
constexpr int kPreferredChannels = 2;
constexpr int kPreferredSamples = 1024;

}  // namespace

Output::~Output() { shutdown(); }

bool Output::init(std::string* error) {
    shutdown();

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        if (error) *error = SDL_GetError();
        return false;
    }

    SDL_AudioSpec want{};
    want.freq = kPreferredRate;
    want.format = AUDIO_S16SYS;
    want.channels = Uint8(kPreferredChannels);
    want.samples = Uint16(kPreferredSamples);
    want.callback = nullptr;

    SDL_AudioSpec have{};
    device_ = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (device_ == 0) {
        if (error) *error = SDL_GetError();
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }

    // We asked for no format changes, but if a driver still hands back
    // something else the queue would play garbage.  Refuse it instead.
    if (have.format != AUDIO_S16SYS || have.channels <= 0 || have.freq <= 0) {
        if (error)
            *error = "unsupported audio format: " + std::to_string(have.freq) + " Hz, " +
                     std::to_string(int(have.channels)) + " channels, format " +
                     std::to_string(int(have.format));
        shutdown();
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }

    sampleRate_ = have.freq;
    channels_ = have.channels;
    SDL_PauseAudioDevice(device_, 0);
    return true;
}

void Output::shutdown() {
    if (device_ != 0) {
        SDL_CloseAudioDevice(device_);
        device_ = 0;
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
}

int Output::queuedMs() const {
    if (device_ == 0) return 0;
    const int frameBytes = channels_ * int(sizeof(int16_t));
    if (frameBytes <= 0 || sampleRate_ <= 0) return 0;
    const Uint32 bytes = SDL_GetQueuedAudioSize(device_);
    return int(bytes / Uint32(frameBytes) * 1000u / Uint32(sampleRate_));
}

void Output::pump(buraco::sound::Synth& synth, int targetMs) {
    if (device_ == 0 || synth.idle()) return;

    const int frameBytes = channels_ * int(sizeof(int16_t));
    const int queuedFrames = int(SDL_GetQueuedAudioSize(device_) / Uint32(frameBytes));
    const int targetFrames = std::max(0, targetMs) * sampleRate_ / 1000;
    const int frames = targetFrames - queuedFrames;
    if (frames <= 0) return;

    mono_.resize(size_t(frames));
    synth.render(mono_.data(), frames);

    interleaved_.resize(size_t(frames) * size_t(channels_));
    for (int i = 0; i < frames; ++i) {
        const int16_t sample = mono_[size_t(i)];
        for (int c = 0; c < channels_; ++c)
            interleaved_[size_t(i) * size_t(channels_) + size_t(c)] = sample;
    }
    SDL_QueueAudio(device_, interleaved_.data(),
                   Uint32(interleaved_.size() * sizeof(int16_t)));
}

}  // namespace audio
