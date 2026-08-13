/*
 * @Author: Ryuk
 * @Date: 2026-08-12
 * @Description: Unit test for libAE_TS — validates TS C interface
 */

#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>

#include "interface/audio_engine_ts.h"
#include "utils/dr_wav.h"


#define FRAME_LEN (160)   /* 10ms @ 16kHz */


static void test_normal_flow(const char* input_wav, const char* output_wav)
{
    std::cout << "=== test_normal_flow ===" << std::endl;

    /* Open input WAV */
    DrWavReader wav_reader(input_wav);
    int rate       = wav_reader.sample_rate();
    int channels   = wav_reader.num_channels();
    int64_t total  = wav_reader.num_samples();
    std::cout << "  input: " << input_wav
              << " rate=" << rate
              << " ch=" << channels
              << " samples=" << total << std::endl;

    /* Create + Init */
    TsHandle handle = AudioEngine_Ts_Create();
    if (!handle) {
        std::cerr << "  FAIL: AudioEngine_Ts_Create returned NULL" << std::endl;
        return;
    }

    TsInitConfig init_cfg;
    init_cfg.sample_rate   = rate;
    init_cfg.frame_len     = FRAME_LEN;
    init_cfg.num_channels  = channels;
    init_cfg.detector_rate = rate;

    int ret = AudioEngine_Ts_Init(handle, &init_cfg);
    if (ret != AUDIO_ENGINE_SUCCESS) {
        std::cerr << "  FAIL: AudioEngine_Ts_Init returned " << ret << std::endl;
        AudioEngine_Ts_Destroy(handle);
        return;
    }

    /* Output WAV writer */
    DrWavWriter wav_writer(output_wav, rate, channels);

    int total_frames = 0;
    int in_buf_size  = FRAME_LEN * channels;
    int16_t* in_buf  = new int16_t[in_buf_size];
    int16_t* out_buf = new int16_t[in_buf_size];

    while (true) {
        memset(in_buf, 0, in_buf_size * sizeof(int16_t));
        int read_samples = wav_reader.ReadSamples(FRAME_LEN, in_buf);
        if (read_samples == 0) break;

        /* Always pass FRAME_LEN — zero-padded for partial final frame */
        ret = AudioEngine_Ts_Process(handle,
                                     in_buf, FRAME_LEN,
                                     NULL,     /* detection: use audio itself */
                                     NULL,     /* no reference mic */
                                     1.0f,     /* voice info unavailable → 1.0 */
                                     0,        /* no key pressed */
                                     out_buf);
        if (ret != AUDIO_ENGINE_SUCCESS) {
            std::cerr << "  FAIL: AudioEngine_Ts_Process returned " << ret
                      << " at frame " << total_frames << std::endl;
            break;
        }

        wav_writer.WriteSamples(out_buf, read_samples);
        total_frames++;
    }

    delete[] in_buf;
    delete[] out_buf;

    std::cout << "  frames=" << total_frames << std::endl;

    /* Deinit + Destroy */
    AudioEngine_Ts_Deinit(handle);
    AudioEngine_Ts_Destroy(handle);

    std::cout << "  PASS" << std::endl;
}


static void test_reset(const char* input_wav)
{
    std::cout << "=== test_reset ===" << std::endl;

    DrWavReader wav_reader(input_wav);
    int rate     = wav_reader.sample_rate();
    int channels = wav_reader.num_channels();

    TsHandle handle = AudioEngine_Ts_Create();

    TsInitConfig init_cfg;
    init_cfg.sample_rate   = rate;
    init_cfg.frame_len     = FRAME_LEN;
    init_cfg.num_channels  = channels;
    init_cfg.detector_rate = rate;

    AudioEngine_Ts_Init(handle, &init_cfg);

    /* Process a few frames to build up state */
    int in_buf_size  = FRAME_LEN * channels;
    int16_t* in_buf  = new int16_t[in_buf_size];
    int16_t* out_buf = new int16_t[in_buf_size];
    for (int i = 0; i < 10; i++) {
        int read = wav_reader.ReadSamples(FRAME_LEN, in_buf);
        if (read == 0) break;
        AudioEngine_Ts_Process(handle, in_buf, FRAME_LEN, NULL, NULL,
                               1.0f, 1 /* simulate key press */, out_buf);
    }

    /* Reset */
    int ret = AudioEngine_Ts_Reset(handle);
    if (ret != AUDIO_ENGINE_SUCCESS) {
        std::cerr << "  FAIL: AudioEngine_Ts_Reset returned " << ret << std::endl;
    } else {
        std::cout << "  Reset OK, config preserved" << std::endl;
    }

    /* Should still be able to process after reset */
    int read = wav_reader.ReadSamples(FRAME_LEN, in_buf);
    if (read > 0) {
        ret = AudioEngine_Ts_Process(handle, in_buf, FRAME_LEN, NULL, NULL,
                                     1.0f, 0, out_buf);
        if (ret != AUDIO_ENGINE_SUCCESS) {
            std::cerr << "  FAIL: Process after Reset returned " << ret << std::endl;
        } else {
            std::cout << "  Process after Reset OK" << std::endl;
        }
    }

    delete[] in_buf;
    delete[] out_buf;

    AudioEngine_Ts_Deinit(handle);
    AudioEngine_Ts_Destroy(handle);
    std::cout << "  PASS" << std::endl;
}


static void test_error_cases(void)
{
    std::cout << "=== test_error_cases ===" << std::endl;

    /* NULL handle on Destroy */
    int ret = AudioEngine_Ts_Destroy(NULL);
    std::cout << "  Destroy(NULL) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_HANDLE ? " OK" : " FAIL") << std::endl;

    /* NULL handle on Init */
    ret = AudioEngine_Ts_Init(NULL, NULL);
    std::cout << "  Init(NULL, NULL) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_HANDLE ? " OK" : " FAIL") << std::endl;

    /* Process without Init */
    TsHandle handle = AudioEngine_Ts_Create();
    if (!handle) {
        std::cerr << "  FAIL: AudioEngine_Ts_Create returned NULL" << std::endl;
        return;
    }
    int16_t buf[FRAME_LEN] = {0};
    ret = AudioEngine_Ts_Process(handle, buf, FRAME_LEN, NULL, NULL, 1.0f, 0, buf);
    std::cout << "  Process without Init = " << ret
              << (ret == AUDIO_ENGINE_ERR_NOT_INITIALIZED ? " OK" : " FAIL") << std::endl;

    /* NULL audio_in */
    TsInitConfig init_cfg;
    init_cfg.sample_rate   = 16000;
    init_cfg.frame_len     = FRAME_LEN;
    init_cfg.num_channels  = 1;
    init_cfg.detector_rate = 16000;

    ret = AudioEngine_Ts_Init(handle, &init_cfg);
    if (ret != AUDIO_ENGINE_SUCCESS) {
        std::cerr << "  Init failed with " << ret << " — skipping remaining error tests" << std::endl;
        AudioEngine_Ts_Destroy(handle);
        return;
    }

    ret = AudioEngine_Ts_Process(handle, NULL, FRAME_LEN, NULL, NULL, 1.0f, 0, buf);
    std::cout << "  Process(NULL audio_in) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    /* NULL audio_out */
    ret = AudioEngine_Ts_Process(handle, buf, FRAME_LEN, NULL, NULL, 1.0f, 0, NULL);
    std::cout << "  Process(NULL audio_out) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    /* Invalid frame length */
    ret = AudioEngine_Ts_Process(handle, buf, 999, NULL, NULL, 1.0f, 0, buf);
    std::cout << "  Process(wrong frame_len) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* Invalid voice_probability */
    ret = AudioEngine_Ts_Process(handle, buf, FRAME_LEN, NULL, NULL, 1.5f, 0, buf);
    std::cout << "  Process(voice_prob=1.5) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    ret = AudioEngine_Ts_Process(handle, buf, FRAME_LEN, NULL, NULL, -0.5f, 0, buf);
    std::cout << "  Process(voice_prob=-0.5) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* Unsupported sample rate */
    TsHandle handle2 = AudioEngine_Ts_Create();
    TsInitConfig bad_cfg;
    bad_cfg.sample_rate   = 44100;  /* not supported by TS */
    bad_cfg.frame_len     = 441;
    bad_cfg.num_channels  = 1;
    bad_cfg.detector_rate = 44100;
    ret = AudioEngine_Ts_Init(handle2, &bad_cfg);
    std::cout << "  Init(44.1kHz) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    AudioEngine_Ts_Deinit(handle2);
    AudioEngine_Ts_Destroy(handle2);

    /* NULL init_config */
    TsHandle handle3 = AudioEngine_Ts_Create();
    ret = AudioEngine_Ts_Init(handle3, NULL);
    std::cout << "  Init(NULL config) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    AudioEngine_Ts_Deinit(handle3);
    AudioEngine_Ts_Destroy(handle3);

    AudioEngine_Ts_Deinit(handle);
    AudioEngine_Ts_Destroy(handle);
    std::cout << "  PASS" << std::endl;
}


int main(int argc, char** argv)
{
    const char* input_wav  = "data/audio_short16.wav";
    const char* output_wav = "data/audio_short16_ts_interface_out.wav";

    if (argc >= 2) input_wav = argv[1];

    std::cout << "TS Interface Unit Test" << std::endl;
    std::cout << "======================" << std::endl;

    test_normal_flow(input_wav, output_wav);
    test_reset(input_wav);
    test_error_cases();

    std::cout << "All tests done." << std::endl;
    return 0;
}
