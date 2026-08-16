/*
 * @Author: Ryuk
 * @Date: 2026-08-15
 * @Description: Unit test for libAE_DR — validates DR C interface
 *
 * The normal-flow output must be byte-identical to the internal test
 * (test_dr.cc) output for the same input: both feed 64-sample frames
 * (zero-padded tail frame) through the same DeReverberation instance and
 * use the same short<->float conversion.
 */

#include <cstdio>
#include <cstring>
#include <iostream>

#include "interface/audio_engine_dr.h"
#include "utils/dr_wav.h"


/* Default config: 2^ceil(log2(5e-3 * 16000 * sqrt(0.5))) == 64 (4 ms @ 16 kHz) */
#define FRAME_LEN (64)


static void fill_default_config(DrInitConfig* cfg, int sample_rate)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->sample_rate          = sample_rate;
    cfg->overlap_factor       = 6;
    cfg->frame_increment_s    = 5e-3;
    cfg->round_frame_increment = 1;
    cfg->spectral_gain_type   = 1;
    cfg->gain_smoothing       = 0.95;
    cfg->gain_floor           = 1e-5;
    cfg->oversubtraction      = 2.0;
    cfg->num_states           = 6;
    cfg->posterior_mode       = 1;
    cfg->energy_floor_db      = -60.0;
    cfg->clip_reference_db    = -1e300;
}


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
    DrHandle handle = AudioEngine_Dr_Create();
    if (!handle) {
        std::cerr << "  FAIL: AudioEngine_Dr_Create returned NULL" << std::endl;
        return;
    }

    DrInitConfig init_cfg;
    fill_default_config(&init_cfg, rate);
    int ret = AudioEngine_Dr_Init(handle, &init_cfg);
    if (ret != AUDIO_ENGINE_SUCCESS) {
        std::cerr << "  FAIL: AudioEngine_Dr_Init returned " << ret << std::endl;
        AudioEngine_Dr_Destroy(handle);
        return;
    }

    /* Output WAV writer */
    DrWavWriter wav_writer(output_wav, rate, channels);

    /* Frame loop mirrors the internal test: full frames only, tail frame
       zero-padded to FRAME_LEN, write only the samples actually read. */
    int16_t in_buf[FRAME_LEN]  = {0};
    int16_t out_buf[FRAME_LEN] = {0};
    int64_t total_out = 0;
    int frames = 0;
    while (total_out < total) {
        memset(in_buf, 0, sizeof(in_buf));
        int read = wav_reader.ReadSamples(FRAME_LEN, in_buf);

        ret = AudioEngine_Dr_Process(handle, in_buf, FRAME_LEN, out_buf);
        if (ret != AUDIO_ENGINE_SUCCESS) {
            std::cerr << "  FAIL: AudioEngine_Dr_Process returned " << ret
                      << " at frame " << frames << std::endl;
            break;
        }

        int64_t n = (read < total - total_out) ? read : (total - total_out);
        wav_writer.WriteSamples(out_buf, static_cast<int>(n));
        total_out += n;
        frames++;
        if (read < FRAME_LEN) break;
    }

    std::cout << "  frames=" << frames
              << " samples_out=" << total_out << std::endl;

    /* Deinit + Destroy */
    AudioEngine_Dr_Deinit(handle);
    AudioEngine_Dr_Destroy(handle);

    std::cout << "  PASS" << std::endl;
}


static void test_reset(const char* input_wav)
{
    std::cout << "=== test_reset ===" << std::endl;

    DrWavReader wav_reader(input_wav);
    int rate = wav_reader.sample_rate();

    DrHandle handle = AudioEngine_Dr_Create();
    DrInitConfig init_cfg;
    fill_default_config(&init_cfg, rate);
    AudioEngine_Dr_Init(handle, &init_cfg);

    /* Process a few frames to build up state */
    int16_t in_buf[FRAME_LEN]  = {0};
    int16_t out_buf[FRAME_LEN] = {0};
    for (int i = 0; i < 10; i++) {
        memset(in_buf, 0, sizeof(in_buf));
        int read = wav_reader.ReadSamples(FRAME_LEN, in_buf);
        if (read == 0) break;
        AudioEngine_Dr_Process(handle, in_buf, FRAME_LEN, out_buf);
    }

    /* Reset */
    int ret = AudioEngine_Dr_Reset(handle);
    if (ret != AUDIO_ENGINE_SUCCESS) {
        std::cerr << "  FAIL: AudioEngine_Dr_Reset returned " << ret << std::endl;
    } else {
        std::cout << "  Reset OK, config preserved" << std::endl;
    }

    /* Should still be able to process after reset */
    memset(in_buf, 0, sizeof(in_buf));
    int read = wav_reader.ReadSamples(FRAME_LEN, in_buf);
    if (read > 0) {
        ret = AudioEngine_Dr_Process(handle, in_buf, FRAME_LEN, out_buf);
        if (ret != AUDIO_ENGINE_SUCCESS) {
            std::cerr << "  FAIL: Process after Reset returned " << ret << std::endl;
        } else {
            std::cout << "  Process after Reset OK" << std::endl;
        }
    }

    AudioEngine_Dr_Deinit(handle);
    AudioEngine_Dr_Destroy(handle);
    std::cout << "  PASS" << std::endl;
}


static void test_error_cases(void)
{
    std::cout << "=== test_error_cases ===" << std::endl;

    /* NULL handle on Destroy */
    int ret = AudioEngine_Dr_Destroy(NULL);
    std::cout << "  Destroy(NULL) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_HANDLE ? " OK" : " FAIL") << std::endl;

    /* NULL handle on Init */
    ret = AudioEngine_Dr_Init(NULL, NULL);
    std::cout << "  Init(NULL, NULL) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_HANDLE ? " OK" : " FAIL") << std::endl;

    /* Process without Init */
    DrHandle handle = AudioEngine_Dr_Create();
    int16_t in_buf[FRAME_LEN]  = {0};
    int16_t out_buf[FRAME_LEN] = {0};
    ret = AudioEngine_Dr_Process(handle, in_buf, FRAME_LEN, out_buf);
    std::cout << "  Process without Init = " << ret
              << (ret == AUDIO_ENGINE_ERR_NOT_INITIALIZED ? " OK" : " FAIL") << std::endl;

    /* NULL init_config */
    ret = AudioEngine_Dr_Init(handle, NULL);
    std::cout << "  Init(handle, NULL) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    /* Unsupported sample rate */
    DrInitConfig init_cfg;
    fill_default_config(&init_cfg, 8000);
    ret = AudioEngine_Dr_Init(handle, &init_cfg);
    std::cout << "  Init(sample_rate=8000) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* Zeroed config with valid sample rate -> all defaults applied */
    memset(&init_cfg, 0, sizeof(init_cfg));
    init_cfg.sample_rate = 16000;
    ret = AudioEngine_Dr_Init(handle, &init_cfg);
    std::cout << "  Init(zeroed config) = " << ret
              << (ret == AUDIO_ENGINE_SUCCESS ? " OK" : " FAIL") << std::endl;

    /* NULL audio_in */
    ret = AudioEngine_Dr_Process(handle, NULL, FRAME_LEN, out_buf);
    std::cout << "  Process(NULL audio_in) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    /* NULL audio_out */
    ret = AudioEngine_Dr_Process(handle, in_buf, FRAME_LEN, NULL);
    std::cout << "  Process(NULL audio_out) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    /* Wrong frame length */
    ret = AudioEngine_Dr_Process(handle, in_buf, 160, out_buf);
    std::cout << "  Process(wrong samples=160) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* Process after Deinit */
    AudioEngine_Dr_Deinit(handle);
    ret = AudioEngine_Dr_Process(handle, in_buf, FRAME_LEN, out_buf);
    std::cout << "  Process after Deinit = " << ret
              << (ret == AUDIO_ENGINE_ERR_NOT_INITIALIZED ? " OK" : " FAIL") << std::endl;

    AudioEngine_Dr_Destroy(handle);
    std::cout << "  PASS" << std::endl;
}


int main(int argc, char** argv)
{
    const char* input_wav = "data/audio_reverb16.wav";
    const char* output_wav = "data/audio_reverb16_dr_interface_out.wav";

    if (argc >= 2) input_wav = argv[1];

    std::cout << "DR Interface Unit Test" << std::endl;
    std::cout << "======================" << std::endl;

    test_normal_flow(input_wav, output_wav);
    test_reset(input_wav);
    test_error_cases();

    std::cout << "All tests done." << std::endl;
    return 0;
}
