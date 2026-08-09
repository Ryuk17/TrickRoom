/*
 * @Author: Ryuk
 * @Date: 2026-08-09 21:00:00
 * @LastEditors: Ryuk
 * @LastEditTime: 2026-08-09 21:00:00
 * @Description: Unit test for libAE_VAD — validates VAD C interface
 */

#include <cstdio>
#include <cstring>
#include <iostream>

#include "interface/audio_engine_vad.h"
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
    VadHandle handle = AudioEngine_Vad_Create();
    if (!handle) {
        std::cerr << "  FAIL: AudioEngine_Vad_Create returned NULL" << std::endl;
        return;
    }

    VadInitConfig init_cfg;
    init_cfg.sample_rate = rate;
    init_cfg.frame_len   = FRAME_LEN;
    int ret = AudioEngine_Vad_Init(handle, &init_cfg);
    if (ret != AUDIO_ENGINE_SUCCESS) {
        std::cerr << "  FAIL: AudioEngine_Vad_Init returned " << ret << std::endl;
        AudioEngine_Vad_Destroy(handle);
        return;
    }

    /* SetParam */
    VadRtConfig rt_cfg;
    rt_cfg.threshold = 0.5f;
    ret = AudioEngine_Vad_SetParam(handle, &rt_cfg);
    if (ret != AUDIO_ENGINE_SUCCESS) {
        std::cerr << "  FAIL: AudioEngine_Vad_SetParam returned " << ret << std::endl;
        AudioEngine_Vad_Deinit(handle);
        AudioEngine_Vad_Destroy(handle);
        return;
    }

    /* Output WAV writer */
    DrWavWriter wav_writer(output_wav, rate, channels);

    int16_t in_buf[FRAME_LEN]   = {0};
    int16_t voice_buf[FRAME_LEN];
    int16_t silence_buf[FRAME_LEN] = {0};
    for (int i = 0; i < FRAME_LEN; i++) {
        voice_buf[i] = 32000;   /* visual marker for voice frames */
    }

    int voice_frames = 0;
    int total_frames = 0;

    while (true) {
        memset(in_buf, 0, sizeof(in_buf));
        int read_samples = wav_reader.ReadSamples(FRAME_LEN, in_buf);
        if (read_samples == 0) break;

        int vad_flag = 0;
        /* Always pass FRAME_LEN — zero-padded for partial final frame */
        ret = AudioEngine_Vad_Process(handle, in_buf, FRAME_LEN, &vad_flag);
        if (ret != AUDIO_ENGINE_SUCCESS) {
            std::cerr << "  FAIL: AudioEngine_Vad_Process returned " << ret
                      << " at frame " << total_frames << std::endl;
            break;
        }

        if (vad_flag) {
            wav_writer.WriteSamples(voice_buf, read_samples);
            voice_frames++;
        } else {
            wav_writer.WriteSamples(silence_buf, read_samples);
        }
        total_frames++;
    }

    std::cout << "  frames=" << total_frames
              << " voice=" << voice_frames
              << " silence=" << (total_frames - voice_frames) << std::endl;

    /* Deinit + Destroy */
    AudioEngine_Vad_Deinit(handle);
    AudioEngine_Vad_Destroy(handle);

    std::cout << "  PASS" << std::endl;
}


static void test_reset(const char* input_wav)
{
    std::cout << "=== test_reset ===" << std::endl;

    DrWavReader wav_reader(input_wav);
    int rate = wav_reader.sample_rate();

    VadHandle handle = AudioEngine_Vad_Create();
    VadInitConfig init_cfg = { rate, FRAME_LEN };
    AudioEngine_Vad_Init(handle, &init_cfg);

    /* Process a few frames to build up state */
    int16_t buf[FRAME_LEN] = {0};
    int vad_flag;
    for (int i = 0; i < 10; i++) {
        int read = wav_reader.ReadSamples(FRAME_LEN, buf);
        if (read == 0) break;
        AudioEngine_Vad_Process(handle, buf, read, &vad_flag);
    }

    /* Reset */
    int ret = AudioEngine_Vad_Reset(handle);
    if (ret != AUDIO_ENGINE_SUCCESS) {
        std::cerr << "  FAIL: AudioEngine_Vad_Reset returned " << ret << std::endl;
    } else {
        std::cout << "  Reset OK, threshold=0.5 preserved" << std::endl;
    }

    /* Should still be able to process after reset */
    int read = wav_reader.ReadSamples(FRAME_LEN, buf);
    if (read > 0) {
        ret = AudioEngine_Vad_Process(handle, buf, read, &vad_flag);
        if (ret != AUDIO_ENGINE_SUCCESS) {
            std::cerr << "  FAIL: Process after Reset returned " << ret << std::endl;
        } else {
            std::cout << "  Process after Reset OK, vad_flag=" << vad_flag << std::endl;
        }
    }

    AudioEngine_Vad_Deinit(handle);
    AudioEngine_Vad_Destroy(handle);
    std::cout << "  PASS" << std::endl;
}


static void test_error_cases(void)
{
    std::cout << "=== test_error_cases ===" << std::endl;

    /* NULL handle on Destroy */
    int ret = AudioEngine_Vad_Destroy(NULL);
    std::cout << "  Destroy(NULL) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_HANDLE ? " OK" : " FAIL") << std::endl;

    /* NULL handle on Init */
    ret = AudioEngine_Vad_Init(NULL, NULL);
    std::cout << "  Init(NULL, NULL) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_HANDLE ? " OK" : " FAIL") << std::endl;

    /* Process without Init */
    VadHandle handle = AudioEngine_Vad_Create();
    int vad_flag;
    int16_t buf[FRAME_LEN] = {0};
    ret = AudioEngine_Vad_Process(handle, buf, FRAME_LEN, &vad_flag);
    std::cout << "  Process without Init = " << ret
              << (ret == AUDIO_ENGINE_ERR_NOT_INITIALIZED ? " OK" : " FAIL") << std::endl;

    /* NULL audio_in */
    VadInitConfig init_cfg = { 16000, FRAME_LEN };
    AudioEngine_Vad_Init(handle, &init_cfg);
    ret = AudioEngine_Vad_Process(handle, NULL, FRAME_LEN, &vad_flag);
    std::cout << "  Process(NULL audio_in) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    /* Invalid samples (wrong frame length) */
    ret = AudioEngine_Vad_Process(handle, buf, 999, &vad_flag);
    std::cout << "  Process(wrong samples) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* Invalid threshold */
    VadRtConfig rt_cfg;
    rt_cfg.threshold = 1.5f;
    ret = AudioEngine_Vad_SetParam(handle, &rt_cfg);
    std::cout << "  SetParam(threshold=1.5) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    rt_cfg.threshold = 0.0f;
    ret = AudioEngine_Vad_SetParam(handle, &rt_cfg);
    std::cout << "  SetParam(threshold=0.0) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    AudioEngine_Vad_Deinit(handle);
    AudioEngine_Vad_Destroy(handle);
    std::cout << "  PASS" << std::endl;
}


int main(int argc, char** argv)
{
    const char* input_wav = "data/audio_short16.wav";
    const char* output_wav = "data/audio_short16_vad_interface_out.wav";

    if (argc >= 2) input_wav = argv[1];

    std::cout << "VAD Interface Unit Test" << std::endl;
    std::cout << "========================" << std::endl;

    test_normal_flow(input_wav, output_wav);
    test_reset(input_wav);
    test_error_cases();

    std::cout << "All tests done." << std::endl;
    return 0;
}