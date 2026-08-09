/*
 * @Author: Ryuk
 * @Date: 2026-08-09 23:10:00
 * @LastEditors: Ryuk
 * @LastEditTime: 2026-08-09 23:10:00
 * @Description: Unit test for libAE_NS — validates Noise Suppression C interface
 */

#include <cstdio>
#include <cstring>
#include <iostream>

#include "interface/audio_engine_ns.h"
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
    NsHandle handle = AudioEngine_Ns_Create();
    if (!handle) {
        std::cerr << "  FAIL: AudioEngine_Ns_Create returned NULL" << std::endl;
        return;
    }

    NsInitConfig init_cfg;
    init_cfg.sample_rate       = rate;
    init_cfg.num_channels      = channels;
    init_cfg.suppression_level = 1;   /* k12dB, same as internal default */
    int ret = AudioEngine_Ns_Init(handle, &init_cfg);
    if (ret != AUDIO_ENGINE_SUCCESS) {
        std::cerr << "  FAIL: AudioEngine_Ns_Init returned " << ret << std::endl;
        AudioEngine_Ns_Destroy(handle);
        return;
    }

    /* SetParam (no-op placeholder, must accept reserved=0) */
    NsRtConfig rt_cfg;
    rt_cfg.reserved = 0;
    ret = AudioEngine_Ns_SetParam(handle, &rt_cfg);
    if (ret != AUDIO_ENGINE_SUCCESS) {
        std::cerr << "  FAIL: AudioEngine_Ns_SetParam returned " << ret << std::endl;
        AudioEngine_Ns_Deinit(handle);
        AudioEngine_Ns_Destroy(handle);
        return;
    }

    /* Output WAV writer */
    DrWavWriter wav_writer(output_wav, rate, channels);

    /* Same flow as internal test_ns: always process FRAME_LEN,
       partial final frame keeps leftover tail from previous iteration */
    int total_samples = 0;
    int frames = 0;
    int16_t wav_data[FRAME_LEN];          /* NOT cleared — replicates internal test */
    int16_t out_data[FRAME_LEN];

    while (true) {
        int read_samples = wav_reader.ReadSamples(FRAME_LEN, wav_data);

        int out_samples = 0;
        ret = AudioEngine_Ns_Process(handle, wav_data, FRAME_LEN,
                                     out_data, FRAME_LEN, &out_samples);
        if (ret != AUDIO_ENGINE_SUCCESS) {
            std::cerr << "  FAIL: AudioEngine_Ns_Process returned " << ret
                      << " at frame " << frames << std::endl;
            break;
        }
        if (out_samples != FRAME_LEN) {
            std::cerr << "  FAIL: out_samples=" << out_samples
                      << " expected " << FRAME_LEN << " at frame " << frames << std::endl;
            break;
        }

        wav_writer.WriteSamples(out_data, read_samples);
        total_samples += read_samples;
        frames++;

        if (read_samples < FRAME_LEN) {
            break;
        }
    }

    std::cout << "  frames=" << frames
              << " total write samples: " << total_samples << std::endl;

    /* Deinit + Destroy */
    AudioEngine_Ns_Deinit(handle);
    AudioEngine_Ns_Destroy(handle);

    std::cout << "  PASS" << std::endl;
}


static void test_reset(const char* input_wav)
{
    std::cout << "=== test_reset ===" << std::endl;

    DrWavReader wav_reader(input_wav);
    int rate = wav_reader.sample_rate();

    NsHandle handle = AudioEngine_Ns_Create();
    NsInitConfig init_cfg = { rate, 1, 1 };
    int ret = AudioEngine_Ns_Init(handle, &init_cfg);
    if (ret != AUDIO_ENGINE_SUCCESS) {
        std::cerr << "  FAIL: Init returned " << ret << std::endl;
        AudioEngine_Ns_Destroy(handle);
        return;
    }

    /* Process a few frames to build up noise estimate state */
    int16_t in_buf[FRAME_LEN] = {0};
    int16_t out_buf[FRAME_LEN] = {0};
    int out_samples = 0;
    for (int i = 0; i < 10; i++) {
        int read = wav_reader.ReadSamples(FRAME_LEN, in_buf);
        if (read == 0) break;
        AudioEngine_Ns_Process(handle, in_buf, FRAME_LEN, out_buf, FRAME_LEN, &out_samples);
    }

    /* Reset */
    ret = AudioEngine_Ns_Reset(handle);
    if (ret != AUDIO_ENGINE_SUCCESS) {
        std::cerr << "  FAIL: AudioEngine_Ns_Reset returned " << ret << std::endl;
    } else {
        std::cout << "  Reset OK, config preserved" << std::endl;
    }

    /* Should still be able to process after reset */
    int read = wav_reader.ReadSamples(FRAME_LEN, in_buf);
    if (read > 0) {
        ret = AudioEngine_Ns_Process(handle, in_buf, FRAME_LEN, out_buf, FRAME_LEN, &out_samples);
        if (ret != AUDIO_ENGINE_SUCCESS) {
            std::cerr << "  FAIL: Process after Reset returned " << ret << std::endl;
        } else {
            std::cout << "  Process after Reset OK, out_samples=" << out_samples << std::endl;
        }
    }

    AudioEngine_Ns_Deinit(handle);
    AudioEngine_Ns_Destroy(handle);
    std::cout << "  PASS" << std::endl;
}


static void test_error_cases(void)
{
    std::cout << "=== test_error_cases ===" << std::endl;

    /* NULL handle on Destroy */
    int ret = AudioEngine_Ns_Destroy(NULL);
    std::cout << "  Destroy(NULL) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_HANDLE ? " OK" : " FAIL") << std::endl;

    /* NULL handle on Init */
    ret = AudioEngine_Ns_Init(NULL, NULL);
    std::cout << "  Init(NULL, NULL) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_HANDLE ? " OK" : " FAIL") << std::endl;

    /* Process without Init */
    NsHandle handle = AudioEngine_Ns_Create();
    int16_t in_buf[FRAME_LEN] = {0};
    int16_t out_buf[FRAME_LEN] = {0};
    int out_samples = 0;
    ret = AudioEngine_Ns_Process(handle, in_buf, FRAME_LEN, out_buf, FRAME_LEN, &out_samples);
    std::cout << "  Process without Init = " << ret
              << (ret == AUDIO_ENGINE_ERR_NOT_INITIALIZED ? " OK" : " FAIL") << std::endl;

    /* NULL audio_in */
    NsInitConfig init_cfg = { 16000, 1, 1 };
    AudioEngine_Ns_Init(handle, &init_cfg);
    ret = AudioEngine_Ns_Process(handle, NULL, FRAME_LEN, out_buf, FRAME_LEN, &out_samples);
    std::cout << "  Process(NULL audio_in) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    /* NULL audio_out */
    ret = AudioEngine_Ns_Process(handle, in_buf, FRAME_LEN, NULL, FRAME_LEN, &out_samples);
    std::cout << "  Process(NULL audio_out) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    /* NULL out_samples */
    ret = AudioEngine_Ns_Process(handle, in_buf, FRAME_LEN, out_buf, FRAME_LEN, NULL);
    std::cout << "  Process(NULL out_samples) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    /* Invalid in_samples (not 10ms frame) */
    ret = AudioEngine_Ns_Process(handle, in_buf, 999, out_buf, FRAME_LEN, &out_samples);
    std::cout << "  Process(in_samples=999) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* max_out_samples too small */
    ret = AudioEngine_Ns_Process(handle, in_buf, FRAME_LEN, out_buf, 100, &out_samples);
    std::cout << "  Process(max_out=100) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* Invalid Init: sample_rate = 0 */
    NsInitConfig bad_cfg = { 0, 1, 1 };
    ret = AudioEngine_Ns_Init(handle, &bad_cfg);
    std::cout << "  Init(sample_rate=0) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* Invalid Init: channels = 3 */
    bad_cfg.sample_rate = 16000;
    bad_cfg.num_channels = 3;
    ret = AudioEngine_Ns_Init(handle, &bad_cfg);
    std::cout << "  Init(channels=3) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* Invalid Init: suppression_level = 5 */
    bad_cfg.num_channels = 1;
    bad_cfg.suppression_level = 5;
    ret = AudioEngine_Ns_Init(handle, &bad_cfg);
    std::cout << "  Init(level=5) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* Invalid SetParam reserved */
    NsRtConfig bad_rt;
    bad_rt.reserved = 1;
    ret = AudioEngine_Ns_SetParam(handle, &bad_rt);
    std::cout << "  SetParam(reserved=1) = " << ret
              << (ret == AUDIO_ENGINE_ERR_SET_PARAM_FAILED ? " OK" : " FAIL") << std::endl;

    /* Reset without Init */
    NsHandle raw_handle = AudioEngine_Ns_Create();
    ret = AudioEngine_Ns_Reset(raw_handle);
    std::cout << "  Reset without Init = " << ret
              << (ret == AUDIO_ENGINE_ERR_NOT_INITIALIZED ? " OK" : " FAIL") << std::endl;
    AudioEngine_Ns_Destroy(raw_handle);

    AudioEngine_Ns_Deinit(handle);
    AudioEngine_Ns_Destroy(handle);
    std::cout << "  PASS" << std::endl;
}


int main(int argc, char** argv)
{
    const char* input_wav = "data/audio_long16noise.wav";
    const char* output_wav = "data/audio_long16noise_anr_out.wav";

    if (argc >= 2) input_wav = argv[1];

    std::cout << "NS Interface Unit Test" << std::endl;
    std::cout << "=======================" << std::endl;

    test_normal_flow(input_wav, output_wav);
    test_reset(input_wav);
    test_error_cases();

    std::cout << "All tests done." << std::endl;
    return 0;
}
