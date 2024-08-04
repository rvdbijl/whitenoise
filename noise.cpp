#include <iostream>
#include <cstdlib>
#include <ctime>
#include <alsa/asoundlib.h>

#define SAMPLE_RATE 22050
#define CHANNELS 1
#define SAMPLE_SIZE 2 // 16-bit

int main() {
    snd_pcm_t *handle;
    int err;

    // Open PCM device for playback
    if ((err = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        std::cerr << "Unable to open PCM device: " << snd_strerror(err) << std::endl;
        return 1;
    }

    // Set parameters
    snd_pcm_hw_params_t *params;
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(handle, params);
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(handle, params, CHANNELS);
    unsigned int rate = SAMPLE_RATE;
    snd_pcm_hw_params_set_rate_near(handle, params, &rate, 0);
    snd_pcm_hw_params(handle, params);

    // Seed random number generator
    std::srand(std::time(nullptr));

    // Buffer to hold audio data
    const int buffer_size = 1024;
    int16_t buffer[buffer_size];

    std::cout << "Playing white noise. Press Ctrl+C to stop." << std::endl;

    while (true) {
        // Generate white noise
        for (int i = 0; i < buffer_size; ++i) {
            buffer[i] = (std::rand() % 65536) - 32768; // Generate random 16-bit values
        }

        // Write to sound device
        if ((err = snd_pcm_writei(handle, buffer, buffer_size)) != buffer_size) {
            std::cerr << "Write error: " << snd_strerror(err) << std::endl;
            snd_pcm_prepare(handle);
        }
    }

    // Close PCM handle
    snd_pcm_close(handle);
    return 0;
}
