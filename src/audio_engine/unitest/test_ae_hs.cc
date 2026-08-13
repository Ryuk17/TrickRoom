/*
 * @Author: Ryuk
 * @Date: 2026-08-12
 * @Description: Unit test for libAE_HS — validates HS C interface
 */

#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>

#include "interface/audio_engine_hs.h"
#include "utils/dr_wav.h"


/* DETECT_SAMPLES_PER_BLOCK = 205 is the internal Goertzel window size */
#define FRAME_LEN (205)


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
    HsHandle handle = AudioEngine_Hs_Create();
    if (!handle) {
        std::cerr << "  FAIL: AudioEngine_Hs_Create returned NULL" << std::endl;
        return;
    }

    HsInitConfig init_cfg;
    init_cfg.sample_rate        = rate;
    init_cfg.frame_len          = FRAME_LEN;
    init_cfg.detect_threshold   = 13.0f;
    init_cfg.detect_block       = 5;
    init_cfg.detect_freq_min    = 650.0f;
    init_cfg.detect_freq_max    = 3000.0f;
    init_cfg.detect_freq_step   = 25.0f;
    init_cfg.notch_persist_block = 5;
    init_cfg.notch_filter_Q     = 0.7071f;

    int ret = AudioEngine_Hs_Init(handle, &init_cfg);
    if (ret != AUDIO_ENGINE_SUCCESS) {
        std::cerr << "  FAIL: AudioEngine_Hs_Init returned " << ret << std::endl;
        AudioEngine_Hs_Destroy(handle);
        return;
    }

    /* Output WAV writer (mono) */
    const int out_ch = 1;
    DrWavWriter wav_writer(output_wav, rate, out_ch);

    int total_frames = 0;
    int16_t* in_buf  = new int16_t[FRAME_LEN];
    int16_t* out_buf = new int16_t[FRAME_LEN];

    while (true) {
        memset(in_buf, 0, FRAME_LEN * sizeof(int16_t));
        int read_samples = wav_reader.ReadSamples(FRAME_LEN, in_buf);
        if (read_samples == 0) break;

        ret = AudioEngine_Hs_Process(handle, in_buf, read_samples, out_buf);
        if (ret != AUDIO_ENGINE_SUCCESS) {
            std::cerr << "  FAIL: AudioEngine_Hs_Process returned " << ret
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
    AudioEngine_Hs_Deinit(handle);
    AudioEngine_Hs_Destroy(handle);

    std::cout << "  PASS" << std::endl;
}


static void test_reset(const char* input_wav)
{
    std::cout << "=== test_reset ===" << std::endl;

    DrWavReader wav_reader(input_wav);
    int rate = wav_reader.sample_rate();

    HsHandle handle = AudioEngine_Hs_Create();

    HsInitConfig init_cfg;
    init_cfg.sample_rate        = rate;
    init_cfg.frame_len          = FRAME_LEN;
    init_cfg.detect_threshold   = 13.0f;
    init_cfg.detect_block       = 5;
    init_cfg.detect_freq_min    = 650.0f;
    init_cfg.detect_freq_max    = 3000.0f;
    init_cfg.detect_freq_step   = 25.0f;
    init_cfg.notch_persist_block = 5;
    init_cfg.notch_filter_Q     = 0.7071f;

    AudioEngine_Hs_Init(handle, &init_cfg);

    /* Process a few frames to build up state */
    int16_t in_buf[FRAME_LEN]  = {0};
    int16_t out_buf[FRAME_LEN] = {0};
    for (int i = 0; i < 10; i++) {
        int read = wav_reader.ReadSamples(FRAME_LEN, in_buf);
        if (read == 0) break;
        AudioEngine_Hs_Process(handle, in_buf, read, out_buf);
    }

    /* Reset */
    int ret = AudioEngine_Hs_Reset(handle);
    if (ret != AUDIO_ENGINE_SUCCESS) {
        std::cerr << "  FAIL: AudioEngine_Hs_Reset returned " << ret << std::endl;
    } else {
        std::cout << "  Reset OK, config preserved" << std::endl;
    }

    /* Should still be able to process after reset */
    int read = wav_reader.ReadSamples(FRAME_LEN, in_buf);
    if (read > 0) {
        ret = AudioEngine_Hs_Process(handle, in_buf, read, out_buf);
        if (ret != AUDIO_ENGINE_SUCCESS) {
            std::cerr << "  FAIL: Process after Reset returned " << ret << std::endl;
        } else {
            std::cout << "  Process after Reset OK" << std::endl;
        }
    }

    AudioEngine_Hs_Deinit(handle);
    AudioEngine_Hs_Destroy(handle);
    std::cout << "  PASS" << std::endl;
}


static void test_error_cases(void)
{
    std::cout << "=== test_error_cases ===" << std::endl;

    /* NULL handle on Destroy */
    int ret = AudioEngine_Hs_Destroy(NULL);
    std::cout << "  Destroy(NULL) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_HANDLE ? " OK" : " FAIL") << std::endl;

    /* NULL handle on Init */
    ret = AudioEngine_Hs_Init(NULL, NULL);
    std::cout << "  Init(NULL, NULL) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_HANDLE ? " OK" : " FAIL") << std::endl;

    /* Process without Init */
    HsHandle handle = AudioEngine_Hs_Create();
    if (!handle) {
        std::cerr << "  FAIL: AudioEngine_Hs_Create returned NULL" << std::endl;
        return;
    }
    int16_t buf[FRAME_LEN] = {0};
    ret = AudioEngine_Hs_Process(handle, buf, FRAME_LEN, buf);
    std::cout << "  Process without Init = " << ret
              << (ret == AUDIO_ENGINE_ERR_NOT_INITIALIZED ? " OK" : " FAIL") << std::endl;

    /* NULL audio_in */
    HsInitConfig init_cfg;
    init_cfg.sample_rate        = 16000;
    init_cfg.frame_len          = FRAME_LEN;
    init_cfg.detect_threshold   = 13.0f;
    init_cfg.detect_block       = 5;
    init_cfg.detect_freq_min    = 650.0f;
    init_cfg.detect_freq_max    = 3000.0f;
    init_cfg.detect_freq_step   = 25.0f;
    init_cfg.notch_persist_block = 5;
    init_cfg.notch_filter_Q     = 0.7071f;

    ret = AudioEngine_Hs_Init(handle, &init_cfg);
    if (ret != AUDIO_ENGINE_SUCCESS) {
        std::cerr << "  Init failed with " << ret << " — skipping remaining error tests" << std::endl;
        AudioEngine_Hs_Destroy(handle);
        return;
    }

    ret = AudioEngine_Hs_Process(handle, NULL, FRAME_LEN, buf);
    std::cout << "  Process(NULL audio_in) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    /* NULL audio_out */
    ret = AudioEngine_Hs_Process(handle, buf, FRAME_LEN, NULL);
    std::cout << "  Process(NULL audio_out) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    /* Invalid frame length (0 or negative) */
    ret = AudioEngine_Hs_Process(handle, buf, 0, buf);
    std::cout << "  Process(in_samples=0) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    ret = AudioEngine_Hs_Process(handle, buf, -1, buf);
    std::cout << "  Process(in_samples=-1) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* Invalid Init config */
    HsHandle handle2 = AudioEngine_Hs_Create();
    HsInitConfig bad_cfg;
    bad_cfg.sample_rate = -100;
    bad_cfg.frame_len   = 0;
    ret = AudioEngine_Hs_Init(handle2, &bad_cfg);
    std::cout << "  Init(bad sample_rate) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    AudioEngine_Hs_Deinit(handle2);
    AudioEngine_Hs_Destroy(handle2);

    /* NULL init_config */
    HsHandle handle3 = AudioEngine_Hs_Create();
    ret = AudioEngine_Hs_Init(handle3, NULL);
    std::cout << "  Init(NULL config) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    AudioEngine_Hs_Deinit(handle3);
    AudioEngine_Hs_Destroy(handle3);

    AudioEngine_Hs_Deinit(handle);
    AudioEngine_Hs_Destroy(handle);
    std::cout << "  PASS" << std::endl;
}


int main(int argc, char** argv)
{
    const char* input_wav  = "data/audio_short16.wav";
    const char* output_wav = "data/audio_short16_hs_interface_out.wav";

    if (argc >= 2) input_wav = argv[1];

    std::cout << "HS Interface Unit Test" << std::endl;
    std::cout << "======================" << std::endl;

    test_normal_flow(input_wav, output_wav);
    test_reset(input_wav);
    test_error_cases();

    std::cout << "All tests done." << std::endl;
    return 0;
}
