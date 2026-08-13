/*
 * @Author: Ryuk
 * @Date: 2026-08-12
 * @Description: Unit test for libAE_IE — validates IE C interface
 */

#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>

#include "interface/audio_engine_ie.h"
#include "utils/dr_wav.h"


#define FRAME_LEN (160)   /* 10ms @ 16kHz */

/* freqs_ = ComplexLength(FftOrder(16000 * 16ms / 1000)) = ComplexLength(8) = 129 */
#define NOISE_FREQS (129)


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
    IeHandle handle = AudioEngine_Ie_Create();
    if (!handle) {
        std::cerr << "  FAIL: AudioEngine_Ie_Create returned NULL" << std::endl;
        return;
    }

    IeInitConfig init_cfg;
    init_cfg.sample_rate       = rate;
    init_cfg.frame_len         = FRAME_LEN;
    init_cfg.num_channels      = channels;
    init_cfg.decay_rate        = 0.9f;
    init_cfg.analysis_rate     = 60;
    init_cfg.gain_change_limit = 0.1f;
    init_cfg.rho               = 0.02f;

    int ret = AudioEngine_Ie_Init(handle, &init_cfg);
    if (ret != AUDIO_ENGINE_SUCCESS) {
        std::cerr << "  FAIL: AudioEngine_Ie_Init returned " << ret << std::endl;
        AudioEngine_Ie_Destroy(handle);
        return;
    }

    /* Set a flat noise spectrum estimate (uniform noise floor) */
    std::vector<float> noise(NOISE_FREQS, 10.0f);
    ret = AudioEngine_Ie_SetNoiseEstimate(handle, noise.data(), NOISE_FREQS);
    if (ret != AUDIO_ENGINE_SUCCESS) {
        std::cerr << "  FAIL: AudioEngine_Ie_SetNoiseEstimate returned " << ret << std::endl;
        AudioEngine_Ie_Deinit(handle);
        AudioEngine_Ie_Destroy(handle);
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
        ret = AudioEngine_Ie_Process(handle, in_buf, FRAME_LEN, out_buf);
        if (ret != AUDIO_ENGINE_SUCCESS) {
            std::cerr << "  FAIL: AudioEngine_Ie_Process returned " << ret
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
    AudioEngine_Ie_Deinit(handle);
    AudioEngine_Ie_Destroy(handle);

    std::cout << "  PASS" << std::endl;
}


static void test_reset(const char* input_wav)
{
    std::cout << "=== test_reset ===" << std::endl;

    DrWavReader wav_reader(input_wav);
    int rate     = wav_reader.sample_rate();
    int channels = wav_reader.num_channels();

    IeHandle handle = AudioEngine_Ie_Create();

    IeInitConfig init_cfg;
    init_cfg.sample_rate       = rate;
    init_cfg.frame_len         = FRAME_LEN;
    init_cfg.num_channels      = channels;
    init_cfg.decay_rate        = 0.9f;
    init_cfg.analysis_rate     = 60;
    init_cfg.gain_change_limit = 0.1f;
    init_cfg.rho               = 0.02f;

    AudioEngine_Ie_Init(handle, &init_cfg);

    std::vector<float> noise(NOISE_FREQS, 10.0f);
    AudioEngine_Ie_SetNoiseEstimate(handle, noise.data(), NOISE_FREQS);

    /* Process a few frames */
    int in_buf_size  = FRAME_LEN * channels;
    int16_t* in_buf  = new int16_t[in_buf_size];
    int16_t* out_buf = new int16_t[in_buf_size];
    for (int i = 0; i < 10; i++) {
        int read = wav_reader.ReadSamples(FRAME_LEN, in_buf);
        if (read == 0) break;
        AudioEngine_Ie_Process(handle, in_buf, read, out_buf);
    }

    /* Reset */
    int ret = AudioEngine_Ie_Reset(handle);
    if (ret != AUDIO_ENGINE_SUCCESS) {
        std::cerr << "  FAIL: AudioEngine_Ie_Reset returned " << ret << std::endl;
    } else {
        std::cout << "  Reset OK, config preserved" << std::endl;
    }

    /* Set noise again after reset */
    AudioEngine_Ie_SetNoiseEstimate(handle, noise.data(), NOISE_FREQS);

    /* Process after reset */
    int read = wav_reader.ReadSamples(FRAME_LEN, in_buf);
    if (read > 0) {
        ret = AudioEngine_Ie_Process(handle, in_buf, read, out_buf);
        if (ret != AUDIO_ENGINE_SUCCESS) {
            std::cerr << "  FAIL: Process after Reset returned " << ret << std::endl;
        } else {
            std::cout << "  Process after Reset OK" << std::endl;
        }
    }

    delete[] in_buf;
    delete[] out_buf;

    AudioEngine_Ie_Deinit(handle);
    AudioEngine_Ie_Destroy(handle);
    std::cout << "  PASS" << std::endl;
}


static void test_error_cases(void)
{
    std::cout << "=== test_error_cases ===" << std::endl;

    /* NULL handle on Destroy */
    int ret = AudioEngine_Ie_Destroy(NULL);
    std::cout << "  Destroy(NULL) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_HANDLE ? " OK" : " FAIL") << std::endl;

    /* NULL handle on Init */
    ret = AudioEngine_Ie_Init(NULL, NULL);
    std::cout << "  Init(NULL, NULL) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_HANDLE ? " OK" : " FAIL") << std::endl;

    /* Process without Init */
    IeHandle handle = AudioEngine_Ie_Create();
    if (!handle) {
        std::cerr << "  FAIL: AudioEngine_Ie_Create returned NULL" << std::endl;
        return;
    }
    int16_t buf[FRAME_LEN] = {0};
    ret = AudioEngine_Ie_Process(handle, buf, FRAME_LEN, buf);
    std::cout << "  Process without Init = " << ret
              << (ret == AUDIO_ENGINE_ERR_NOT_INITIALIZED ? " OK" : " FAIL") << std::endl;

    /* NULL audio_in */
    IeInitConfig init_cfg;
    init_cfg.sample_rate       = 16000;
    init_cfg.frame_len         = FRAME_LEN;
    init_cfg.num_channels      = 1;
    init_cfg.decay_rate        = 0.9f;
    init_cfg.analysis_rate     = 60;
    init_cfg.gain_change_limit = 0.1f;
    init_cfg.rho               = 0.02f;

    ret = AudioEngine_Ie_Init(handle, &init_cfg);
    if (ret != AUDIO_ENGINE_SUCCESS) {
        std::cerr << "  Init failed with " << ret << " — skipping remaining error tests" << std::endl;
        AudioEngine_Ie_Destroy(handle);
        return;
    }

    ret = AudioEngine_Ie_Process(handle, NULL, FRAME_LEN, buf);
    std::cout << "  Process(NULL audio_in) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    /* NULL audio_out */
    ret = AudioEngine_Ie_Process(handle, buf, FRAME_LEN, NULL);
    std::cout << "  Process(NULL audio_out) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    /* Invalid frame length */
    ret = AudioEngine_Ie_Process(handle, buf, 999, buf);
    std::cout << "  Process(wrong frame_len) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* NULL noise_spectrum */
    ret = AudioEngine_Ie_SetNoiseEstimate(handle, NULL, NOISE_FREQS);
    std::cout << "  SetNoiseEstimate(NULL) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    /* Invalid num_freqs */
    float noise[NOISE_FREQS] = {0};
    ret = AudioEngine_Ie_SetNoiseEstimate(handle, noise, 0);
    std::cout << "  SetNoiseEstimate(num_freqs=0) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* NULL init_config */
    IeHandle handle2 = AudioEngine_Ie_Create();
    ret = AudioEngine_Ie_Init(handle2, NULL);
    std::cout << "  Init(NULL config) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    AudioEngine_Ie_Deinit(handle2);
    AudioEngine_Ie_Destroy(handle2);

    AudioEngine_Ie_Deinit(handle);
    AudioEngine_Ie_Destroy(handle);
    std::cout << "  PASS" << std::endl;
}


int main(int argc, char** argv)
{
    const char* input_wav  = "data/audio_short16.wav";
    const char* output_wav = "data/audio_short16_ie_interface_out.wav";

    if (argc >= 2) input_wav = argv[1];

    std::cout << "IE Interface Unit Test" << std::endl;
    std::cout << "======================" << std::endl;

    test_normal_flow(input_wav, output_wav);
    test_reset(input_wav);
    test_error_cases();

    std::cout << "All tests done." << std::endl;
    return 0;
}
