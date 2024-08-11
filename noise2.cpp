// Build with 
// g++ -o white_noise white_noise.cpp -lasound -lpthread

#include <iostream>
#include <cstdlib>
#include <ctime>
#include <alsa/asoundlib.h>
#include <thread>
#include <atomic>
#include <vector>
#include <pthread.h>

#define SAMPLE_RATE 22050
#define CHANNELS 1
#define SAMPLE_SIZE 2 // 16-bit

const int BUFFER_SIZE = 16384;
std::vector<int16_t> ring_buffer(BUFFER_SIZE * 4);
std::atomic<size_t> write_pos{0};
std::atomic<size_t> read_pos{0};
std::atomic<bool> keep_running{true};

void generate_white_noise(std::vector<int16_t>& buffer, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        buffer[i] = (std::rand() % 65536) - 32768;
    }
}

void audio_callback(snd_pcm_t* handle) {
    int err;
    const snd_pcm_channel_area_t* areas;
    snd_pcm_uframes_t offset, frames = BUFFER_SIZE;

    while (keep_running.load()) {
        err = snd_pcm_mmap_begin(handle, &areas, &offset, &frames);
        if (err < 0) {
            std::cerr << "snd_pcm_mmap_begin error: " << snd_strerror(err) << std::endl;
            break;
        }

        int16_t* buf = (int16_t*)((char*)areas[0].addr + (areas[0].first + areas[0].step * offset) / 8);
        
        size_t bytes_to_write = frames * sizeof(int16_t);
        size_t bytes_available = (write_pos - read_pos + ring_buffer.size()) % ring_buffer.size();
        
        if (bytes_available < bytes_to_write) {
            // Fill the remaining space with silence
            size_t silence_bytes = bytes_to_write - bytes_available;
            memset(buf + bytes_available / sizeof(int16_t), 0, silence_bytes);
        }

        for (size_t i = 0; i < frames; ++i) {
            buf[i] = ring_buffer[read_pos];
            read_pos = (read_pos + 1) % ring_buffer.size();
        }

        err = snd_pcm_mmap_commit(handle, offset, frames);
        if (err < 0 || (snd_pcm_uframes_t)err != frames) {
            std::cerr << "snd_pcm_mmap_commit error: " << snd_strerror(err) << std::endl;
            break;
        }
    }
}

int main() {
    snd_pcm_t *handle;
    int err;

    if ((err = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        std::cerr << "Unable to open PCM device: " << snd_strerror(err) << std::endl;
        return 1;
    }

    snd_pcm_hw_params_t *params;
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(handle, params);
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_MMAP_INTERLEAVED);
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(handle, params, CHANNELS);

    unsigned int rate = SAMPLE_RATE;
    snd_pcm_hw_params_set_rate_near(handle, params, &rate, 0);

    snd_pcm_uframes_t buffer_size = BUFFER_SIZE * 2;
    snd_pcm_hw_params_set_buffer_size_near(handle, params, &buffer_size);

    snd_pcm_hw_params(handle, params);

    std::srand(std::time(nullptr));

    std::thread audio_thread(audio_callback, handle);

    // Set real-time priority for the audio thread
    sched_param sch_params;
    sch_params.sched_priority = sched_get_priority_max(SCHED_FIFO);
    pthread_setschedparam(audio_thread.native_handle(), SCHED_FIFO, &sch_params);

    std::cout << "Playing white noise. Press Enter to stop." << std::endl;

    while (keep_running.load()) {
        generate_white_noise(ring_buffer, BUFFER_SIZE);
        write_pos = (write_pos + BUFFER_SIZE) % ring_buffer.size();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    keep_running.store(false);
    audio_thread.join();

    snd_pcm_close(handle);
    return 0;
}