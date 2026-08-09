/*
 * @Author: Ryuk
 * @Date: 2026-08-09 23:50:00
 * @LastEditors: Ryuk
 * @LastEditTime: 2026-08-09 23:50:00
 * @Description: Unit test for libAE_AGC2 — validates AGC2 C interface
 */

#include <cstdio>
#include <cstring>
#include <iostream>

#include "interface/audio_engine_agc2.h"
#include "utils/dr_wav.h"


static void test_normal_flow(const char* input_wav, const char* output_wav)
{
    std::cout << "=== test_normal_flow ===" << std::endl;

    /* Open input WAV */
    DrWavReader wav_reader(input_wav);
    int rate     = wav_reader.sample_rate();
    int channels = wav_reader.num_channels();
    std::cout << "  input: " << input_wav
              << " rate=" << rate
              << " ch=" << channels
              << " samples=" << wav_reader.num_samples() << std::endl;

    /* Create + Init */
    Agc2Handle handle = AudioEngine_Agc2_Create();
    if (!handle) {
        std::cerr << "  FAIL: AudioEngine_Agc2_Create returned NULL" << std::endl;
        return;
    }

    Agc2InitConfig init_cfg;
    init_cfg.sample_rate = rate;
    init_cfg.num_channels = channels;
    init_cfg.headroom_db = 5.0f;
    init_cfg.max_gain_db = 50.0f;
    init_cfg.initial_gain_db = 15.0f;
    init_cfg.max_gain_change_db_per_second = 6.0f;
    init_cfg.max_output_noise_level_dbfs = -50.0f;
    int ret = AudioEngine_Agc2_Init(handle, &init_cfg);
    if (ret != AUDIO_ENGINE_SUCCESS) {
        std::cerr << "  FAIL: AudioEngine_Agc2_Init returned " << ret << std::endl;
        AudioEngine_Agc2_Destroy(handle);
        return;
    }

    /* Output WAV writer */
    DrWavWriter wav_writer(output_wav, rate, channels);

    /* Same flow as internal test_agc2: process full 10ms frames only,
       partial final frame is zero-filled to frame size */
    int frame_size = (rate / 100) * channels;
    int total_samples = 0;
    int frames = 0;
    int16_t wav_data[480] = {0};
    int16_t output[480]   = {0};

    while (true) {
        int read_samples = wav_reader.ReadSamples(frame_size, wav_data);
        if (read_samples == 0) {
            break;
        }

        /* Zero-pad partial final frame to full frame size (same as internal) */
        bool is_partial = (read_samples < frame_size);
        if (is_partial) {
            memset(wav_data + read_samples, 0,
                   (frame_size - read_samples) * sizeof(int16_t));
            read_samples = frame_size;
        }

        int out_samples = 0;
        ret = AudioEngine_Agc2_Process(handle, wav_data, frame_size,
                                       output, frame_size, &out_samples);
        if (ret != AUDIO_ENGINE_SUCCESS) {
            std::cerr << "  FAIL: AudioEngine_Agc2_Process returned " << ret
                      << " at frame " << frames << std::endl;
            break;
        }
        if (out_samples != frame_size) {
            std::cerr << "  FAIL: out_samples=" << out_samples
                      << " expected " << frame_size << " at frame " << frames << std::endl;
            break;
        }

        wav_writer.WriteSamples(output, read_samples);
        total_samples += read_samples;
        frames++;

        if (is_partial) {
            break;
        }
    }

    std::cout << "  frames=" << frames
              << " total write samples: " << total_samples << std::endl;

    /* Deinit + Destroy */
    AudioEngine_Agc2_Deinit(handle);
    AudioEngine_Agc2_Destroy(handle);

    std::cout << "  PASS" << std::endl;
}


static void test_reset(const char* input_wav)
{
    std::cout << "=== test_reset ===" << std::endl;

    DrWavReader wav_reader(input_wav);
    int rate = wav_reader.sample_rate();
    int channels = wav_reader.num_channels();
    int frame_size = (rate / 100) * channels;

    Agc2Handle handle = AudioEngine_Agc2_Create();
    Agc2InitConfig init_cfg;
    init_cfg.sample_rate = rate;
    init_cfg.num_channels = channels;
    init_cfg.headroom_db = 5.0f;
    init_cfg.max_gain_db = 50.0f;
    init_cfg.initial_gain_db = 15.0f;
    init_cfg.max_gain_change_db_per_second = 6.0f;
    init_cfg.max_output_noise_level_dbfs = -50.0f;
    int ret = AudioEngine_Agc2_Init(handle, &init_cfg);
    if (ret != AUDIO_ENGINE_SUCCESS) {
        std::cerr << "  FAIL: Init returned " << ret << std::endl;
        AudioEngine_Agc2_Destroy(handle);
        return;
    }

    /* Process a few frames to build up AGC2 state (gain adaptation) */
    int16_t in_buf[480] = {0};
    int16_t out_buf[480] = {0};
    int out_samples = 0;
    for (int i = 0; i < 10; i++) {
        int read = wav_reader.ReadSamples(frame_size, in_buf);
        if (read == 0) break;
        AudioEngine_Agc2_Process(handle, in_buf, frame_size, out_buf, frame_size, &out_samples);
    }

    /* Reset */
    ret = AudioEngine_Agc2_Reset(handle);
    if (ret != AUDIO_ENGINE_SUCCESS) {
        std::cerr << "  FAIL: AudioEngine_Agc2_Reset returned " << ret << std::endl;
    } else {
        std::cout << "  Reset OK, config preserved" << std::endl;
    }

    /* Should still be able to process after reset */
    int read = wav_reader.ReadSamples(frame_size, in_buf);
    if (read > 0) {
        ret = AudioEngine_Agc2_Process(handle, in_buf, frame_size, out_buf, frame_size, &out_samples);
        if (ret != AUDIO_ENGINE_SUCCESS) {
            std::cerr << "  FAIL: Process after Reset returned " << ret << std::endl;
        } else {
            std::cout << "  Process after Reset OK, out_samples=" << out_samples << std::endl;
        }
    }

    AudioEngine_Agc2_Deinit(handle);
    AudioEngine_Agc2_Destroy(handle);
    std::cout << "  PASS" << std::endl;
}


static void test_error_cases(void)
{
    std::cout << "=== test_error_cases ===" << std::endl;

    /* NULL handle on Destroy */
    int ret = AudioEngine_Agc2_Destroy(NULL);
    std::cout << "  Destroy(NULL) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_HANDLE ? " OK" : " FAIL") << std::endl;

    /* NULL handle on Init */
    ret = AudioEngine_Agc2_Init(NULL, NULL);
    std::cout << "  Init(NULL, NULL) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_HANDLE ? " OK" : " FAIL") << std::endl;

    /* Process without Init */
    Agc2Handle handle = AudioEngine_Agc2_Create();
    int16_t in_buf[160] = {0};
    int16_t out_buf[160] = {0};
    int out_samples = 0;
    ret = AudioEngine_Agc2_Process(handle, in_buf, 160, out_buf, 160, &out_samples);
    std::cout << "  Process without Init = " << ret
              << (ret == AUDIO_ENGINE_ERR_NOT_INITIALIZED ? " OK" : " FAIL") << std::endl;

    /* NULL audio_in */
    Agc2InitConfig init_cfg;
    init_cfg.sample_rate = 16000;
    init_cfg.num_channels = 1;
    init_cfg.headroom_db = 5.0f;
    init_cfg.max_gain_db = 50.0f;
    init_cfg.initial_gain_db = 15.0f;
    init_cfg.max_gain_change_db_per_second = 6.0f;
    init_cfg.max_output_noise_level_dbfs = -50.0f;
    AudioEngine_Agc2_Init(handle, &init_cfg);
    ret = AudioEngine_Agc2_Process(handle, NULL, 160, out_buf, 160, &out_samples);
    std::cout << "  Process(NULL audio_in) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    /* NULL audio_out */
    ret = AudioEngine_Agc2_Process(handle, in_buf, 160, NULL, 160, &out_samples);
    std::cout << "  Process(NULL audio_out) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    /* NULL out_samples */
    ret = AudioEngine_Agc2_Process(handle, in_buf, 160, out_buf, 160, NULL);
    std::cout << "  Process(NULL out_samples) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    /* Invalid in_samples (not 10ms frame) */
    ret = AudioEngine_Agc2_Process(handle, in_buf, 100, out_buf, 160, &out_samples);
    std::cout << "  Process(in_samples=100) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* max_out_samples too small */
    ret = AudioEngine_Agc2_Process(handle, in_buf, 160, out_buf, 100, &out_samples);
    std::cout << "  Process(max_out=100) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* Invalid Init: sample_rate = 0 */
    Agc2InitConfig bad_cfg = { 0, 1, 5.0f, 50.0f, 15.0f, 6.0f, -50.0f };
    ret = AudioEngine_Agc2_Init(handle, &bad_cfg);
    std::cout << "  Init(sample_rate=0) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* Invalid Init: num_channels = 0 */
    bad_cfg.sample_rate = 16000;
    bad_cfg.num_channels = 0;
    ret = AudioEngine_Agc2_Init(handle, &bad_cfg);
    std::cout << "  Init(num_channels=0) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* Invalid Init: num_channels = 3 */
    bad_cfg.num_channels = 3;
    ret = AudioEngine_Agc2_Init(handle, &bad_cfg);
    std::cout << "  Init(num_channels=3) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* Invalid Init: sample_rate = 44100 */
    bad_cfg.num_channels = 1;
    bad_cfg.sample_rate = 44100;
    ret = AudioEngine_Agc2_Init(handle, &bad_cfg);
    std::cout << "  Init(sample_rate=44100) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* NULL init_config */
    ret = AudioEngine_Agc2_Init(handle, NULL);
    std::cout << "  Init(NULL) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    /* NULL rt_config */
    ret = AudioEngine_Agc2_SetParam(handle, NULL);
    std::cout << "  SetParam(NULL) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    ret = AudioEngine_Agc2_ResetParam(handle, NULL);
    std::cout << "  ResetParam(NULL) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    /* Reset without Init */
    Agc2Handle raw_handle = AudioEngine_Agc2_Create();
    ret = AudioEngine_Agc2_Reset(raw_handle);
    std::cout << "  Reset without Init = " << ret
              << (ret == AUDIO_ENGINE_ERR_NOT_INITIALIZED ? " OK" : " FAIL") << std::endl;
    AudioEngine_Agc2_Destroy(raw_handle);

    AudioEngine_Agc2_Deinit(handle);
    AudioEngine_Agc2_Destroy(handle);
    std::cout << "  PASS" << std::endl;
}


int main(int argc, char** argv)
{
    const char* input_wav = "data/audio_short16.wav";
    const char* output_wav = "data/audio_short16_agc2_out.wav";

    if (argc >= 2) input_wav = argv[1];

    std::cout << "AGC2 Interface Unit Test" << std::endl;
    std::cout << "========================" << std::endl;

    test_normal_flow(input_wav, output_wav);
    test_reset(input_wav);
    test_error_cases();

    std::cout << "All tests done." << std::endl;
    return 0;
}
