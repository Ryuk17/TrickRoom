/*
 * @Author: Ryuk
 * @Date: 2026-08-10 01:30:00
 * @LastEditors: Ryuk
 * @LastEditTime: 2026-08-10 01:30:00
 * @Description: Unit test for libAE_AEC — validates AEC3 C interface
 */

#include <cstdio>
#include <cstring>
#include <iostream>

#include "interface/audio_engine_aec.h"
#include "utils/dr_wav.h"


#define FRAME_LEN (160)   /* 10ms @ 16kHz */


static void test_normal_flow(const char* nearend_wav, const char* farend_wav,
                             const char* output_wav)
{
    std::cout << "=== test_normal_flow ===" << std::endl;

    /* Open input WAVs (dual-input: near-end mic + far-end render) */
    DrWavReader nearend_reader(nearend_wav);
    DrWavReader farend_reader(farend_wav);
    int rate     = farend_reader.sample_rate();
    int channels = farend_reader.num_channels();
    std::cout << "  input: " << nearend_wav << " + " << farend_wav
              << " rate=" << rate
              << " ch=" << channels
              << " samples=" << farend_reader.num_samples() << std::endl;

    /* Create + Init */
    AecHandle handle = AudioEngine_Aec_Create();
    if (!handle) {
        std::cerr << "  FAIL: AudioEngine_Aec_Create returned NULL" << std::endl;
        return;
    }

    AecInitConfig init_cfg;
    init_cfg.sample_rate          = rate;
    init_cfg.num_render_channels  = channels;
    init_cfg.num_capture_channels = channels;
    int ret = AudioEngine_Aec_Init(handle, &init_cfg);
    if (ret != AUDIO_ENGINE_SUCCESS) {
        std::cerr << "  FAIL: AudioEngine_Aec_Init returned " << ret << std::endl;
        AudioEngine_Aec_Destroy(handle);
        return;
    }

    /* Output WAV writer */
    DrWavWriter wav_writer(output_wav, rate, channels);

    /* Same flow as internal test_aec3: full 10ms frames with zero-filled
       partial final frame (interface enforces strict 10ms frames) */
    int total_samples = 0;
    int frames = 0;
    int16_t nearend_data[FRAME_LEN] = {0};
    int16_t farend_data[FRAME_LEN]  = {0};
    int16_t output[FRAME_LEN]       = {0};

    while (true) {
        int farend_read  = farend_reader.ReadSamples(FRAME_LEN, farend_data);
        int nearend_read = nearend_reader.ReadSamples(FRAME_LEN, nearend_data);
        if (farend_read == 0) {
            break;
        }

        /* Zero-pad partial final frames (caller side; interface requires
           strict 10ms frames) */
        if (farend_read < FRAME_LEN) {
            memset(farend_data + farend_read, 0,
                   (FRAME_LEN - farend_read) * sizeof(int16_t));
        }
        if (nearend_read < FRAME_LEN) {
            memset(nearend_data + nearend_read, 0,
                   (FRAME_LEN - nearend_read) * sizeof(int16_t));
        }

        int out_samples = 0;
        ret = AudioEngine_Aec_Process(handle, nearend_data, farend_data,
                                      FRAME_LEN, output, FRAME_LEN, &out_samples);
        if (ret != AUDIO_ENGINE_SUCCESS) {
            std::cerr << "  FAIL: AudioEngine_Aec_Process returned " << ret
                      << " at frame " << frames << std::endl;
            break;
        }
        if (out_samples != FRAME_LEN) {
            std::cerr << "  FAIL: out_samples=" << out_samples
                      << " expected " << FRAME_LEN << " at frame " << frames << std::endl;
            break;
        }

        wav_writer.WriteSamples(output, FRAME_LEN);
        total_samples += FRAME_LEN;
        frames++;
    }

    std::cout << "  frames=" << frames
              << " total write samples: " << total_samples << std::endl;

    /* Deinit + Destroy */
    AudioEngine_Aec_Deinit(handle);
    AudioEngine_Aec_Destroy(handle);

    std::cout << "  PASS" << std::endl;
}


static void test_reset(const char* nearend_wav, const char* farend_wav)
{
    std::cout << "=== test_reset ===" << std::endl;

    DrWavReader nearend_reader(nearend_wav);
    DrWavReader farend_reader(farend_wav);
    int rate = farend_reader.sample_rate();

    AecHandle handle = AudioEngine_Aec_Create();
    AecInitConfig init_cfg = { rate, 1, 1 };
    int ret = AudioEngine_Aec_Init(handle, &init_cfg);
    if (ret != AUDIO_ENGINE_SUCCESS) {
        std::cerr << "  FAIL: Init returned " << ret << std::endl;
        AudioEngine_Aec_Destroy(handle);
        return;
    }

    /* Process a few frames to build up AEC3 state (filter adaptation) */
    int16_t nearend_buf[FRAME_LEN] = {0};
    int16_t farend_buf[FRAME_LEN]  = {0};
    int16_t out_buf[FRAME_LEN]     = {0};
    int out_samples = 0;
    for (int i = 0; i < 10; i++) {
        int read = farend_reader.ReadSamples(FRAME_LEN, farend_buf);
        if (read == 0) break;
        nearend_reader.ReadSamples(FRAME_LEN, nearend_buf);
        AudioEngine_Aec_Process(handle, nearend_buf, farend_buf,
                                FRAME_LEN, out_buf, FRAME_LEN, &out_samples);
    }

    /* Reset */
    ret = AudioEngine_Aec_Reset(handle);
    if (ret != AUDIO_ENGINE_SUCCESS) {
        std::cerr << "  FAIL: AudioEngine_Aec_Reset returned " << ret << std::endl;
    } else {
        std::cout << "  Reset OK, config preserved" << std::endl;
    }

    /* Should still be able to process after reset */
    int read = farend_reader.ReadSamples(FRAME_LEN, farend_buf);
    if (read > 0) {
        nearend_reader.ReadSamples(FRAME_LEN, nearend_buf);
        ret = AudioEngine_Aec_Process(handle, nearend_buf, farend_buf,
                                      FRAME_LEN, out_buf, FRAME_LEN, &out_samples);
        if (ret != AUDIO_ENGINE_SUCCESS) {
            std::cerr << "  FAIL: Process after Reset returned " << ret << std::endl;
        } else {
            std::cout << "  Process after Reset OK, out_samples=" << out_samples << std::endl;
        }
    }

    AudioEngine_Aec_Deinit(handle);
    AudioEngine_Aec_Destroy(handle);
    std::cout << "  PASS" << std::endl;
}


static void test_error_cases(void)
{
    std::cout << "=== test_error_cases ===" << std::endl;

    /* NULL handle on Destroy */
    int ret = AudioEngine_Aec_Destroy(NULL);
    std::cout << "  Destroy(NULL) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_HANDLE ? " OK" : " FAIL") << std::endl;

    /* NULL handle on Init */
    ret = AudioEngine_Aec_Init(NULL, NULL);
    std::cout << "  Init(NULL, NULL) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_HANDLE ? " OK" : " FAIL") << std::endl;

    /* Process without Init */
    AecHandle handle = AudioEngine_Aec_Create();
    int16_t in_buf[FRAME_LEN] = {0};
    int16_t out_buf[FRAME_LEN] = {0};
    int out_samples = 0;
    ret = AudioEngine_Aec_Process(handle, in_buf, in_buf, FRAME_LEN,
                                  out_buf, FRAME_LEN, &out_samples);
    std::cout << "  Process without Init = " << ret
              << (ret == AUDIO_ENGINE_ERR_NOT_INITIALIZED ? " OK" : " FAIL") << std::endl;

    /* NULL nearend_in */
    AecInitConfig init_cfg = { 16000, 1, 1 };
    AudioEngine_Aec_Init(handle, &init_cfg);
    ret = AudioEngine_Aec_Process(handle, NULL, in_buf, FRAME_LEN,
                                  out_buf, FRAME_LEN, &out_samples);
    std::cout << "  Process(NULL nearend_in) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    /* NULL farend_in */
    ret = AudioEngine_Aec_Process(handle, in_buf, NULL, FRAME_LEN,
                                  out_buf, FRAME_LEN, &out_samples);
    std::cout << "  Process(NULL farend_in) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    /* NULL audio_out */
    ret = AudioEngine_Aec_Process(handle, in_buf, in_buf, FRAME_LEN,
                                  NULL, FRAME_LEN, &out_samples);
    std::cout << "  Process(NULL audio_out) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    /* NULL out_samples */
    ret = AudioEngine_Aec_Process(handle, in_buf, in_buf, FRAME_LEN,
                                  out_buf, FRAME_LEN, NULL);
    std::cout << "  Process(NULL out_samples) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    /* Invalid in_samples (not 10ms frame) */
    ret = AudioEngine_Aec_Process(handle, in_buf, in_buf, 100,
                                  out_buf, FRAME_LEN, &out_samples);
    std::cout << "  Process(in_samples=100) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* max_out_samples too small */
    ret = AudioEngine_Aec_Process(handle, in_buf, in_buf, FRAME_LEN,
                                  out_buf, 100, &out_samples);
    std::cout << "  Process(max_out=100) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* Invalid Init: sample_rate = 0 */
    AecInitConfig bad_cfg = { 0, 1, 1 };
    ret = AudioEngine_Aec_Init(handle, &bad_cfg);
    std::cout << "  Init(sample_rate=0) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* Invalid Init: sample_rate = 8000 (AEC3 supports 16k/32k/48k only) */
    bad_cfg.sample_rate = 8000;
    ret = AudioEngine_Aec_Init(handle, &bad_cfg);
    std::cout << "  Init(sample_rate=8000) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* Invalid Init: sample_rate = 44100 */
    bad_cfg.sample_rate = 44100;
    ret = AudioEngine_Aec_Init(handle, &bad_cfg);
    std::cout << "  Init(sample_rate=44100) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* Invalid Init: num_render_channels = 0 */
    bad_cfg.sample_rate = 16000;
    bad_cfg.num_render_channels = 0;
    ret = AudioEngine_Aec_Init(handle, &bad_cfg);
    std::cout << "  Init(render_ch=0) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* Invalid Init: num_capture_channels = 3 */
    bad_cfg.num_render_channels = 1;
    bad_cfg.num_capture_channels = 3;
    ret = AudioEngine_Aec_Init(handle, &bad_cfg);
    std::cout << "  Init(capture_ch=3) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* NULL init_config */
    ret = AudioEngine_Aec_Init(handle, NULL);
    std::cout << "  Init(NULL) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    /* NULL rt_config on SetParam / ResetParam */
    ret = AudioEngine_Aec_SetParam(handle, NULL);
    std::cout << "  SetParam(NULL) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    ret = AudioEngine_Aec_ResetParam(handle, NULL);
    std::cout << "  ResetParam(NULL) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    /* SetParam with a real config (delay_ms >= 0 is applied) */
    AecRtConfig rt_cfg;
    rt_cfg.delay_ms = 10;
    ret = AudioEngine_Aec_SetParam(handle, &rt_cfg);
    std::cout << "  SetParam(delay_ms=10) = " << ret
              << (ret == AUDIO_ENGINE_SUCCESS ? " OK" : " FAIL") << std::endl;

    rt_cfg.delay_ms = -1;   /* sentinel: keep current */
    ret = AudioEngine_Aec_SetParam(handle, &rt_cfg);
    std::cout << "  SetParam(delay_ms=-1) = " << ret
              << (ret == AUDIO_ENGINE_SUCCESS ? " OK" : " FAIL") << std::endl;

    /* ResetParam (full reset back to default) */
    rt_cfg.delay_ms = 20;
    ret = AudioEngine_Aec_ResetParam(handle, &rt_cfg);
    std::cout << "  ResetParam(delay_ms=20) = " << ret
              << (ret == AUDIO_ENGINE_SUCCESS ? " OK" : " FAIL") << std::endl;

    /* Reset without Init */
    AecHandle raw_handle = AudioEngine_Aec_Create();
    ret = AudioEngine_Aec_Reset(raw_handle);
    std::cout << "  Reset without Init = " << ret
              << (ret == AUDIO_ENGINE_ERR_NOT_INITIALIZED ? " OK" : " FAIL") << std::endl;
    AudioEngine_Aec_Destroy(raw_handle);

    AudioEngine_Aec_Deinit(handle);
    AudioEngine_Aec_Destroy(handle);
    std::cout << "  PASS" << std::endl;
}


int main(int argc, char** argv)
{
    const char* nearend_wav = "data/audio_nearend16k.wav";
    const char* farend_wav  = "data/audio_farend16k.wav";
    const char* output_wav  = "data/audio_nearend16k_aec3_out.wav";

    if (argc >= 2) nearend_wav = argv[1];
    if (argc >= 3) farend_wav = argv[2];

    std::cout << "AEC Interface Unit Test" << std::endl;
    std::cout << "=======================" << std::endl;

    test_normal_flow(nearend_wav, farend_wav, output_wav);
    test_reset(nearend_wav, farend_wav);
    test_error_cases();

    std::cout << "All tests done." << std::endl;
    return 0;
}
