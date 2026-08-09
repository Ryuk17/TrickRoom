/*
 * @Author: Ryuk
 * @Date: 2026-08-09 22:10:00
 * @LastEditors: Ryuk
 * @LastEditTime: 2026-08-09 22:10:00
 * @Description: Unit test for libAE_Resample — validates Resample C interface
 */

#include <cstdio>
#include <cstring>
#include <iostream>

#include "interface/audio_engine_resample.h"
#include "utils/dr_wav.h"


#define FRAME_LEN (160)          /* input frame, 10ms @ 16kHz */
#define RESAMPLED_FRAME_LEN (640) /* FRAME_LEN * 4 for 16k -> 48k */


static void test_normal_flow(const char* input_wav, const char* output_wav)
{
    std::cout << "=== test_normal_flow ===" << std::endl;

    /* Open input WAV */
    DrWavReader wav_reader(input_wav);
    int rate       = wav_reader.sample_rate();
    int channels   = wav_reader.num_channels();
    int dst_rate   = 48000;
    std::cout << "  input: " << input_wav
              << " rate=" << rate
              << " ch=" << channels
              << " samples=" << wav_reader.num_samples() << std::endl;
    std::cout << "  dst rate: " << dst_rate << std::endl;

    /* Create + Init */
    ResampleHandle handle = AudioEngine_Resample_Create();
    if (!handle) {
        std::cerr << "  FAIL: AudioEngine_Resample_Create returned NULL" << std::endl;
        return;
    }

    ResampleInitConfig init_cfg;
    init_cfg.src_sample_rate = rate;
    init_cfg.dst_sample_rate = dst_rate;
    init_cfg.num_channels    = channels;
    int ret = AudioEngine_Resample_Init(handle, &init_cfg);
    if (ret != AUDIO_ENGINE_SUCCESS) {
        std::cerr << "  FAIL: AudioEngine_Resample_Init returned " << ret << std::endl;
        AudioEngine_Resample_Destroy(handle);
        return;
    }

    /* SetParam (no-op placeholder, must accept reserved=0) */
    ResampleRtConfig rt_cfg;
    rt_cfg.reserved = 0;
    ret = AudioEngine_Resample_SetParam(handle, &rt_cfg);
    if (ret != AUDIO_ENGINE_SUCCESS) {
        std::cerr << "  FAIL: AudioEngine_Resample_SetParam returned " << ret << std::endl;
        AudioEngine_Resample_Deinit(handle);
        AudioEngine_Resample_Destroy(handle);
        return;
    }

    /* Output WAV writer */
    DrWavWriter wav_writer(output_wav, dst_rate, channels);

    /* Same flow as internal test_resampler: always push FRAME_LEN,
       partial final frame keeps leftover tail from previous iteration */
    int total_samples = 0;
    int16_t wav_data[FRAME_LEN] = {0};
    int16_t resampled_data[RESAMPLED_FRAME_LEN] = {0};

    while (true) {
        int read_samples = wav_reader.ReadSamples(FRAME_LEN, wav_data);

        int out_samples = 0;
        ret = AudioEngine_Resample_Process(handle, wav_data, FRAME_LEN,
                                           resampled_data, RESAMPLED_FRAME_LEN, &out_samples);
        if (ret != AUDIO_ENGINE_SUCCESS) {
            std::cerr << "  FAIL: AudioEngine_Resample_Process returned " << ret
                      << " at frame " << total_samples / RESAMPLED_FRAME_LEN << std::endl;
            break;
        }

        wav_writer.WriteSamples(resampled_data, out_samples);
        total_samples += out_samples;

        if (read_samples < FRAME_LEN) {
            break;
        }
    }

    std::cout << "  total write samples: " << total_samples << std::endl;

    /* Deinit + Destroy */
    AudioEngine_Resample_Deinit(handle);
    AudioEngine_Resample_Destroy(handle);

    std::cout << "  PASS" << std::endl;
}


static void test_reset(const char* input_wav)
{
    std::cout << "=== test_reset ===" << std::endl;

    DrWavReader wav_reader(input_wav);
    int rate = wav_reader.sample_rate();

    ResampleHandle handle = AudioEngine_Resample_Create();
    ResampleInitConfig init_cfg = { rate, 48000, 1 };
    int ret = AudioEngine_Resample_Init(handle, &init_cfg);
    if (ret != AUDIO_ENGINE_SUCCESS) {
        std::cerr << "  FAIL: Init returned " << ret << std::endl;
        AudioEngine_Resample_Destroy(handle);
        return;
    }

    /* Process a few frames to build up filter state */
    int16_t in_buf[FRAME_LEN] = {0};
    int16_t out_buf[RESAMPLED_FRAME_LEN] = {0};
    int out_samples = 0;
    for (int i = 0; i < 10; i++) {
        int read = wav_reader.ReadSamples(FRAME_LEN, in_buf);
        if (read == 0) break;
        AudioEngine_Resample_Process(handle, in_buf, read, out_buf, RESAMPLED_FRAME_LEN, &out_samples);
    }

    /* Reset */
    ret = AudioEngine_Resample_Reset(handle);
    if (ret != AUDIO_ENGINE_SUCCESS) {
        std::cerr << "  FAIL: AudioEngine_Resample_Reset returned " << ret << std::endl;
    } else {
        std::cout << "  Reset OK, config preserved" << std::endl;
    }

    /* Should still be able to process after reset */
    int read = wav_reader.ReadSamples(FRAME_LEN, in_buf);
    if (read > 0) {
        ret = AudioEngine_Resample_Process(handle, in_buf, read, out_buf, RESAMPLED_FRAME_LEN, &out_samples);
        if (ret != AUDIO_ENGINE_SUCCESS) {
            std::cerr << "  FAIL: Process after Reset returned " << ret << std::endl;
        } else {
            std::cout << "  Process after Reset OK, out_samples=" << out_samples << std::endl;
        }
    }

    AudioEngine_Resample_Deinit(handle);
    AudioEngine_Resample_Destroy(handle);
    std::cout << "  PASS" << std::endl;
}


static void test_error_cases(void)
{
    std::cout << "=== test_error_cases ===" << std::endl;

    /* NULL handle on Destroy */
    int ret = AudioEngine_Resample_Destroy(NULL);
    std::cout << "  Destroy(NULL) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_HANDLE ? " OK" : " FAIL") << std::endl;

    /* NULL handle on Init */
    ret = AudioEngine_Resample_Init(NULL, NULL);
    std::cout << "  Init(NULL, NULL) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_HANDLE ? " OK" : " FAIL") << std::endl;

    /* Process without Init */
    ResampleHandle handle = AudioEngine_Resample_Create();
    int16_t in_buf[FRAME_LEN] = {0};
    int16_t out_buf[RESAMPLED_FRAME_LEN] = {0};
    int out_samples = 0;
    ret = AudioEngine_Resample_Process(handle, in_buf, FRAME_LEN, out_buf, RESAMPLED_FRAME_LEN, &out_samples);
    std::cout << "  Process without Init = " << ret
              << (ret == AUDIO_ENGINE_ERR_NOT_INITIALIZED ? " OK" : " FAIL") << std::endl;

    /* NULL in */
    ResampleInitConfig init_cfg = { 16000, 48000, 1 };
    AudioEngine_Resample_Init(handle, &init_cfg);
    ret = AudioEngine_Resample_Process(handle, NULL, FRAME_LEN, out_buf, RESAMPLED_FRAME_LEN, &out_samples);
    std::cout << "  Process(NULL in) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    /* NULL out */
    ret = AudioEngine_Resample_Process(handle, in_buf, FRAME_LEN, NULL, RESAMPLED_FRAME_LEN, &out_samples);
    std::cout << "  Process(NULL out) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    /* NULL out_samples */
    ret = AudioEngine_Resample_Process(handle, in_buf, FRAME_LEN, out_buf, RESAMPLED_FRAME_LEN, NULL);
    std::cout << "  Process(NULL out_samples) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    /* Invalid in_samples */
    ret = AudioEngine_Resample_Process(handle, in_buf, 0, out_buf, RESAMPLED_FRAME_LEN, &out_samples);
    std::cout << "  Process(in_samples=0) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* Invalid max_out_samples */
    ret = AudioEngine_Resample_Process(handle, in_buf, FRAME_LEN, out_buf, 0, &out_samples);
    std::cout << "  Process(max_out_samples=0) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* Invalid Init rates */
    ResampleInitConfig bad_cfg = { 0, 48000, 1 };
    ret = AudioEngine_Resample_Init(handle, &bad_cfg);
    std::cout << "  Init(src_rate=0) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    bad_cfg.src_sample_rate = 16000;
    bad_cfg.dst_sample_rate = -1;
    ret = AudioEngine_Resample_Init(handle, &bad_cfg);
    std::cout << "  Init(dst_rate=-1) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* Invalid num_channels */
    bad_cfg.src_sample_rate = 16000;
    bad_cfg.dst_sample_rate = 48000;
    bad_cfg.num_channels = 3;
    ret = AudioEngine_Resample_Init(handle, &bad_cfg);
    std::cout << "  Init(channels=3) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* Invalid SetParam reserved */
    ResampleRtConfig bad_rt;
    bad_rt.reserved = 1;
    ret = AudioEngine_Resample_SetParam(handle, &bad_rt);
    std::cout << "  SetParam(reserved=1) = " << ret
              << (ret == AUDIO_ENGINE_ERR_SET_PARAM_FAILED ? " OK" : " FAIL") << std::endl;

    /* Reset without Init */
    ResampleHandle raw_handle = AudioEngine_Resample_Create();
    ret = AudioEngine_Resample_Reset(raw_handle);
    std::cout << "  Reset without Init = " << ret
              << (ret == AUDIO_ENGINE_ERR_NOT_INITIALIZED ? " OK" : " FAIL") << std::endl;
    AudioEngine_Resample_Destroy(raw_handle);

    AudioEngine_Resample_Deinit(handle);
    AudioEngine_Resample_Destroy(handle);
    std::cout << "  PASS" << std::endl;
}


int main(int argc, char** argv)
{
    const char* input_wav = "data/audio_short16.wav";
    const char* output_wav = "data/audio_short16_resample_out.wav";

    if (argc >= 2) input_wav = argv[1];

    std::cout << "Resample Interface Unit Test" << std::endl;
    std::cout << "==============================" << std::endl;

    test_normal_flow(input_wav, output_wav);
    test_reset(input_wav);
    test_error_cases();

    std::cout << "All tests done." << std::endl;
    return 0;
}
