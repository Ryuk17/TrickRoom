/*
 * @Author: Ryuk
 * @Date: 2026-08-09 23:40:00
 * @LastEditors: Ryuk
 * @LastEditTime: 2026-08-09 23:40:00
 * @Description: Unit test for libAE_Agc — validates Legacy AGC C interface
 */

#include <cstdio>
#include <cstring>
#include <iostream>

#include "interface/audio_engine_agc.h"
#include "utils/dr_wav.h"


#define FRAME_LEN (160)   /* 10ms @ 16kHz */


static void test_normal_flow(const char* input_wav, const char* output_wav)
{
    std::cout << "=== test_normal_flow ===" << std::endl;

    /* Open input WAV */
    DrWavReader wav_reader(input_wav);
    int rate       = wav_reader.sample_rate();
    int channels   = wav_reader.num_channels();
    std::cout << "  input: " << input_wav
              << " rate=" << rate
              << " ch=" << channels
              << " samples=" << wav_reader.num_samples() << std::endl;

    /* Create + Init */
    AgcHandle handle = AudioEngine_Agc_Create();
    if (!handle) {
        std::cerr << "  FAIL: AudioEngine_Agc_Create returned NULL" << std::endl;
        return;
    }

    AgcInitConfig init_cfg;
    init_cfg.sample_rate = rate;
    init_cfg.agc_mode    = 2;   /* kAgcModeAdaptiveDigital, same as internal */
    init_cfg.min_level   = 0;
    init_cfg.max_level   = 255;
    int ret = AudioEngine_Agc_Init(handle, &init_cfg);
    if (ret != AUDIO_ENGINE_SUCCESS) {
        std::cerr << "  FAIL: AudioEngine_Agc_Init returned " << ret << std::endl;
        AudioEngine_Agc_Destroy(handle);
        return;
    }

    /* SetParam — same config as internal test */
    AgcRtConfig rt_cfg;
    rt_cfg.compression_gain_db = 9;
    rt_cfg.limiter_enable      = 1;
    rt_cfg.target_level_dbfs   = 3;
    ret = AudioEngine_Agc_SetParam(handle, &rt_cfg);
    if (ret != AUDIO_ENGINE_SUCCESS) {
        std::cerr << "  FAIL: AudioEngine_Agc_SetParam returned " << ret << std::endl;
        AudioEngine_Agc_Deinit(handle);
        AudioEngine_Agc_Destroy(handle);
        return;
    }

    /* Output WAV writer */
    DrWavWriter wav_writer(output_wav, rate, channels);

    /* Same flow as internal test_agc_legacy: always process FRAME_LEN,
       buffer zero-initialized once, partial final frame keeps leftover tail */
    int total_samples = 0;
    int frames = 0;
    int16_t wav_data[FRAME_LEN] = {0};
    int16_t output[FRAME_LEN]   = {0};

    while (true) {
        int read_samples = wav_reader.ReadSamples(FRAME_LEN, wav_data);

        int out_samples = 0;
        ret = AudioEngine_Agc_Process(handle, wav_data, FRAME_LEN,
                                      output, FRAME_LEN, &out_samples);
        if (ret != AUDIO_ENGINE_SUCCESS) {
            std::cerr << "  FAIL: AudioEngine_Agc_Process returned " << ret
                      << " at frame " << frames << std::endl;
            break;
        }
        if (out_samples != FRAME_LEN) {
            std::cerr << "  FAIL: out_samples=" << out_samples
                      << " expected " << FRAME_LEN << " at frame " << frames << std::endl;
            break;
        }

        wav_writer.WriteSamples(output, read_samples);
        total_samples += read_samples;
        frames++;

        if (read_samples < FRAME_LEN) {
            break;
        }
    }

    std::cout << "  frames=" << frames
              << " total write samples: " << total_samples << std::endl;

    /* Deinit + Destroy */
    AudioEngine_Agc_Deinit(handle);
    AudioEngine_Agc_Destroy(handle);

    std::cout << "  PASS" << std::endl;
}


static void test_reset(const char* input_wav)
{
    std::cout << "=== test_reset ===" << std::endl;

    DrWavReader wav_reader(input_wav);
    int rate = wav_reader.sample_rate();

    AgcHandle handle = AudioEngine_Agc_Create();
    AgcInitConfig init_cfg = { rate, 2, 0, 255 };
    int ret = AudioEngine_Agc_Init(handle, &init_cfg);
    if (ret != AUDIO_ENGINE_SUCCESS) {
        std::cerr << "  FAIL: Init returned " << ret << std::endl;
        AudioEngine_Agc_Destroy(handle);
        return;
    }

    /* Process a few frames to build up AGC state (mic level adaptation) */
    int16_t in_buf[FRAME_LEN] = {0};
    int16_t out_buf[FRAME_LEN] = {0};
    int out_samples = 0;
    for (int i = 0; i < 10; i++) {
        int read = wav_reader.ReadSamples(FRAME_LEN, in_buf);
        if (read == 0) break;
        AudioEngine_Agc_Process(handle, in_buf, FRAME_LEN, out_buf, FRAME_LEN, &out_samples);
    }

    /* Reset */
    ret = AudioEngine_Agc_Reset(handle);
    if (ret != AUDIO_ENGINE_SUCCESS) {
        std::cerr << "  FAIL: AudioEngine_Agc_Reset returned " << ret << std::endl;
    } else {
        std::cout << "  Reset OK, config preserved" << std::endl;
    }

    /* Should still be able to process after reset */
    int read = wav_reader.ReadSamples(FRAME_LEN, in_buf);
    if (read > 0) {
        ret = AudioEngine_Agc_Process(handle, in_buf, FRAME_LEN, out_buf, FRAME_LEN, &out_samples);
        if (ret != AUDIO_ENGINE_SUCCESS) {
            std::cerr << "  FAIL: Process after Reset returned " << ret << std::endl;
        } else {
            std::cout << "  Process after Reset OK, out_samples=" << out_samples << std::endl;
        }
    }

    AudioEngine_Agc_Deinit(handle);
    AudioEngine_Agc_Destroy(handle);
    std::cout << "  PASS" << std::endl;
}


static void test_error_cases(void)
{
    std::cout << "=== test_error_cases ===" << std::endl;

    /* NULL handle on Destroy */
    int ret = AudioEngine_Agc_Destroy(NULL);
    std::cout << "  Destroy(NULL) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_HANDLE ? " OK" : " FAIL") << std::endl;

    /* NULL handle on Init */
    ret = AudioEngine_Agc_Init(NULL, NULL);
    std::cout << "  Init(NULL, NULL) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_HANDLE ? " OK" : " FAIL") << std::endl;

    /* Process without Init */
    AgcHandle handle = AudioEngine_Agc_Create();
    int16_t in_buf[FRAME_LEN] = {0};
    int16_t out_buf[FRAME_LEN] = {0};
    int out_samples = 0;
    ret = AudioEngine_Agc_Process(handle, in_buf, FRAME_LEN, out_buf, FRAME_LEN, &out_samples);
    std::cout << "  Process without Init = " << ret
              << (ret == AUDIO_ENGINE_ERR_NOT_INITIALIZED ? " OK" : " FAIL") << std::endl;

    /* NULL audio_in */
    AgcInitConfig init_cfg = { 16000, 2, 0, 255 };
    AudioEngine_Agc_Init(handle, &init_cfg);
    ret = AudioEngine_Agc_Process(handle, NULL, FRAME_LEN, out_buf, FRAME_LEN, &out_samples);
    std::cout << "  Process(NULL audio_in) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    /* NULL audio_out */
    ret = AudioEngine_Agc_Process(handle, in_buf, FRAME_LEN, NULL, FRAME_LEN, &out_samples);
    std::cout << "  Process(NULL audio_out) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    /* NULL out_samples */
    ret = AudioEngine_Agc_Process(handle, in_buf, FRAME_LEN, out_buf, FRAME_LEN, NULL);
    std::cout << "  Process(NULL out_samples) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    /* Invalid in_samples (not 10ms frame) */
    ret = AudioEngine_Agc_Process(handle, in_buf, 999, out_buf, FRAME_LEN, &out_samples);
    std::cout << "  Process(in_samples=999) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* max_out_samples too small */
    ret = AudioEngine_Agc_Process(handle, in_buf, FRAME_LEN, out_buf, 100, &out_samples);
    std::cout << "  Process(max_out=100) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* Invalid Init: sample_rate = 0 */
    AgcInitConfig bad_cfg = { 0, 2, 0, 255 };
    ret = AudioEngine_Agc_Init(handle, &bad_cfg);
    std::cout << "  Init(sample_rate=0) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* Invalid Init: agc_mode = 5 */
    bad_cfg.sample_rate = 16000;
    bad_cfg.agc_mode = 5;
    ret = AudioEngine_Agc_Init(handle, &bad_cfg);
    std::cout << "  Init(agc_mode=5) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* Invalid Init: min_level < 0 */
    bad_cfg.agc_mode = 2;
    bad_cfg.min_level = -1;
    ret = AudioEngine_Agc_Init(handle, &bad_cfg);
    std::cout << "  Init(min_level=-1) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* Invalid Init: max_level < min_level */
    bad_cfg.min_level = 100;
    bad_cfg.max_level = 50;
    ret = AudioEngine_Agc_Init(handle, &bad_cfg);
    std::cout << "  Init(max_level<min_level) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* Invalid SetParam: compression_gain_db = -2 (not sentinel, out of range) */
    AgcRtConfig bad_rt = { -2, 1, 3 };
    ret = AudioEngine_Agc_SetParam(handle, &bad_rt);
    std::cout << "  SetParam(compression=-2) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* Invalid SetParam: compression_gain_db = 100 */
    bad_rt.compression_gain_db = 100;
    ret = AudioEngine_Agc_SetParam(handle, &bad_rt);
    std::cout << "  SetParam(compression=100) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* Invalid SetParam: limiter_enable = 2 */
    bad_rt.compression_gain_db = 9;
    bad_rt.limiter_enable = 2;
    ret = AudioEngine_Agc_SetParam(handle, &bad_rt);
    std::cout << "  SetParam(limiter=2) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* Invalid SetParam: target_level_dbfs = 32 */
    bad_rt.limiter_enable = 1;
    bad_rt.target_level_dbfs = 32;
    ret = AudioEngine_Agc_SetParam(handle, &bad_rt);
    std::cout << "  SetParam(target=32) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* NULL rt_config */
    ret = AudioEngine_Agc_SetParam(handle, NULL);
    std::cout << "  SetParam(NULL) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    ret = AudioEngine_Agc_ResetParam(handle, NULL);
    std::cout << "  ResetParam(NULL) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    /* Reset without Init */
    AgcHandle raw_handle = AudioEngine_Agc_Create();
    ret = AudioEngine_Agc_Reset(raw_handle);
    std::cout << "  Reset without Init = " << ret
              << (ret == AUDIO_ENGINE_ERR_NOT_INITIALIZED ? " OK" : " FAIL") << std::endl;
    AudioEngine_Agc_Destroy(raw_handle);

    AudioEngine_Agc_Deinit(handle);
    AudioEngine_Agc_Destroy(handle);
    std::cout << "  PASS" << std::endl;
}


int main(int argc, char** argv)
{
    const char* input_wav = "data/audio_short16.wav";
    const char* output_wav = "data/audio_short16_agc_legacy_out.wav";

    if (argc >= 2) input_wav = argv[1];

    std::cout << "AGC Interface Unit Test" << std::endl;
    std::cout << "========================" << std::endl;

    test_normal_flow(input_wav, output_wav);
    test_reset(input_wav);
    test_error_cases();

    std::cout << "All tests done." << std::endl;
    return 0;
}
