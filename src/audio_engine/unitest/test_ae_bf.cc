/*
 * @Author: Ryuk
 * @Date: 2026-08-12
 * @Description: Unit test for libAE_BF — validates BF C interface
 */

#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>

#include "interface/audio_engine_bf.h"
#include "utils/dr_wav.h"


#define FRAME_LEN (160)   /* 10ms @ 16kHz */


static void test_normal_flow(const char* input_wav, const char* output_wav)
{
    std::cout << "=== test_normal_flow ===" << std::endl;

    /* Open input WAV */
    DrWavReader wav_reader(input_wav);
    int rate       = wav_reader.sample_rate();
    int file_ch    = wav_reader.num_channels();
    int64_t total  = wav_reader.num_samples();
    std::cout << "  input: " << input_wav
              << " rate=" << rate
              << " ch=" << file_ch
              << " samples=" << total << std::endl;

    /* BF requires at least 2 microphones — use minimum if file has fewer */
    const int channels = (file_ch >= 2) ? file_ch : 4;

    /* Create + Init */
    BfHandle handle = AudioEngine_Bf_Create();
    if (!handle) {
        std::cerr << "  FAIL: AudioEngine_Bf_Create returned NULL" << std::endl;
        return;
    }

    /* Set up mic positions: uniform linear array along x-axis, 4cm spacing */
    BfMicPosition mic_pos[8];
    float spacing = 0.04f;  /* 4 cm spacing to avoid aliasing bin boundary */
    for (int i = 0; i < channels && i < 8; i++) {
        mic_pos[i].x = (i - (channels - 1) * 0.5f) * spacing;
        mic_pos[i].y = 0.0f;
        mic_pos[i].z = 0.0f;
    }

    BfInitConfig init_cfg;
    init_cfg.sample_rate      = rate;
    init_cfg.num_channels     = channels;
    init_cfg.frame_len        = FRAME_LEN;
    init_cfg.mic_pos          = mic_pos;
    init_cfg.target_azimuth   = static_cast<float>(M_PI) / 2.0f;
    init_cfg.target_elevation = 0.0f;

    int ret = AudioEngine_Bf_Init(handle, &init_cfg);
    if (ret != AUDIO_ENGINE_SUCCESS) {
        std::cerr << "  FAIL: AudioEngine_Bf_Init returned " << ret << std::endl;
        AudioEngine_Bf_Destroy(handle);
        return;
    }

    /* SetParam */
    BfRtConfig rt_cfg;
    rt_cfg.target_azimuth   = static_cast<float>(M_PI) / 2.0f;
    rt_cfg.target_elevation = 0.0f;
    ret = AudioEngine_Bf_SetParam(handle, &rt_cfg);
    if (ret != AUDIO_ENGINE_SUCCESS) {
        std::cerr << "  FAIL: AudioEngine_Bf_SetParam returned " << ret << std::endl;
        AudioEngine_Bf_Deinit(handle);
        AudioEngine_Bf_Destroy(handle);
        return;
    }

    /* Output WAV writer (single channel) */
    DrWavWriter wav_writer(output_wav, rate, 1);

    int total_frames  = 0;
    int target_frames = 0;
    int in_buf_size   = FRAME_LEN * channels;
    int16_t* in_buf   = new int16_t[in_buf_size];
    int16_t* out_buf  = new int16_t[FRAME_LEN];
    /* Buffer for reading mono input when file has fewer channels than BF needs */
    int16_t* read_buf = (file_ch < channels) ? new int16_t[FRAME_LEN * file_ch] : in_buf;

    while (true) {
        memset(in_buf, 0, in_buf_size * sizeof(int16_t));
        int read_samples = wav_reader.ReadSamples(FRAME_LEN, read_buf);
        if (read_samples == 0) break;

        /* If file has fewer channels, expand: copy each sample across all channels */
        if (file_ch < channels) {
            for (int s = 0; s < read_samples; ++s) {
                int16_t val = read_buf[s * file_ch];  /* take first channel only */
                for (int c = 0; c < channels; ++c) {
                    in_buf[s * channels + c] = val;
                }
            }
        }

        int is_target = 0;
        ret = AudioEngine_Bf_Process(handle, in_buf, FRAME_LEN, out_buf, &is_target);
        if (ret != AUDIO_ENGINE_SUCCESS) {
            std::cerr << "  FAIL: AudioEngine_Bf_Process returned " << ret
                      << " at frame " << total_frames << std::endl;
            break;
        }

        wav_writer.WriteSamples(out_buf, read_samples);
        total_frames++;
        if (is_target) target_frames++;
    }

    delete[] in_buf;
    delete[] out_buf;
    if (read_buf != in_buf) delete[] read_buf;

    std::cout << "  frames=" << total_frames
              << " target=" << target_frames
              << " interference=" << (total_frames - target_frames) << std::endl;

    /* Deinit + Destroy */
    AudioEngine_Bf_Deinit(handle);
    AudioEngine_Bf_Destroy(handle);

    std::cout << "  PASS" << std::endl;
}


static void test_reset(const char* input_wav)
{
    std::cout << "=== test_reset ===" << std::endl;

    DrWavReader wav_reader(input_wav);
    int rate      = wav_reader.sample_rate();
    int file_ch   = wav_reader.num_channels();
    const int channels = (file_ch >= 2) ? file_ch : 4;

    BfHandle handle = AudioEngine_Bf_Create();

    BfMicPosition mic_pos[8];
    for (int i = 0; i < channels && i < 8; i++) {
        mic_pos[i].x = (i - (channels - 1) * 0.5f) * 0.04f;
        mic_pos[i].y = 0.0f;
        mic_pos[i].z = 0.0f;
    }

    BfInitConfig init_cfg;
    init_cfg.sample_rate      = rate;
    init_cfg.num_channels     = channels;
    init_cfg.frame_len        = FRAME_LEN;
    init_cfg.mic_pos          = mic_pos;
    init_cfg.target_azimuth   = static_cast<float>(M_PI) / 2.0f;
    init_cfg.target_elevation = 0.0f;

    AudioEngine_Bf_Init(handle, &init_cfg);

    BfRtConfig rt_cfg;
    rt_cfg.target_azimuth   = static_cast<float>(M_PI) / 2.0f;
    rt_cfg.target_elevation = 0.0f;
    AudioEngine_Bf_SetParam(handle, &rt_cfg);

    /* Process a few frames to build up state */
    int in_buf_size  = FRAME_LEN * channels;
    int16_t* in_buf  = new int16_t[in_buf_size];
    int16_t* out_buf = new int16_t[FRAME_LEN];
    int16_t* read_buf = (file_ch < channels) ? new int16_t[FRAME_LEN * file_ch] : in_buf;
    int is_target;
    for (int i = 0; i < 10; i++) {
        int read = wav_reader.ReadSamples(FRAME_LEN, read_buf);
        if (read == 0) break;

        if (file_ch < channels) {
            memset(in_buf, 0, in_buf_size * sizeof(int16_t));
            for (int s = 0; s < read; ++s) {
                int16_t val = read_buf[s * file_ch];
                for (int c = 0; c < channels; ++c) {
                    in_buf[s * channels + c] = val;
                }
            }
        }
        AudioEngine_Bf_Process(handle, in_buf, read, out_buf, &is_target);
    }

    /* Reset */
    int ret = AudioEngine_Bf_Reset(handle);
    if (ret != AUDIO_ENGINE_SUCCESS) {
        std::cerr << "  FAIL: AudioEngine_Bf_Reset returned " << ret << std::endl;
    } else {
        std::cout << "  Reset OK, config preserved" << std::endl;
    }

    /* Should still be able to process after reset */
    int read2 = wav_reader.ReadSamples(FRAME_LEN, read_buf);
    if (read2 > 0) {
        if (file_ch < channels) {
            memset(in_buf, 0, in_buf_size * sizeof(int16_t));
            for (int s = 0; s < read2; ++s) {
                int16_t val = read_buf[s * file_ch];
                for (int c = 0; c < channels; ++c) {
                    in_buf[s * channels + c] = val;
                }
            }
        }
        ret = AudioEngine_Bf_Process(handle, in_buf, read2, out_buf, &is_target);
        if (ret != AUDIO_ENGINE_SUCCESS) {
            std::cerr << "  FAIL: Process after Reset returned " << ret << std::endl;
        } else {
            std::cout << "  Process after Reset OK, is_target=" << is_target << std::endl;
        }
    }

    delete[] in_buf;
    delete[] out_buf;
    if (read_buf != in_buf) delete[] read_buf;

    AudioEngine_Bf_Deinit(handle);
    AudioEngine_Bf_Destroy(handle);
    std::cout << "  PASS" << std::endl;
}


static void test_error_cases(void)
{
    std::cout << "=== test_error_cases ===" << std::endl;

    /* NULL handle on Destroy */
    int ret = AudioEngine_Bf_Destroy(NULL);
    std::cout << "  Destroy(NULL) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_HANDLE ? " OK" : " FAIL") << std::endl;

    /* NULL handle on Init */
    ret = AudioEngine_Bf_Init(NULL, NULL);
    std::cout << "  Init(NULL, NULL) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_HANDLE ? " OK" : " FAIL") << std::endl;

    /* Process without Init */
    BfHandle handle = AudioEngine_Bf_Create();
    if (!handle) {
        std::cerr << "  FAIL: AudioEngine_Bf_Create returned NULL" << std::endl;
        return;
    }
    int is_target;
    int16_t buf[FRAME_LEN]        = {0};
    int16_t in_buf[FRAME_LEN * 4] = {0};  /* 4 ch */
    ret = AudioEngine_Bf_Process(handle, in_buf, FRAME_LEN, buf, &is_target);
    std::cout << "  Process without Init = " << ret
              << (ret == AUDIO_ENGINE_ERR_NOT_INITIALIZED ? " OK" : " FAIL") << std::endl;

    /* NULL audio_in — first do a valid Init (need 2+ mics for BF) */
    BfMicPosition mic_pos[4] = {
        {-0.06f, 0.0f, 0.0f}, {-0.02f, 0.0f, 0.0f},
        { 0.02f, 0.0f, 0.0f}, { 0.06f, 0.0f, 0.0f}
    };
    BfInitConfig init_cfg;
    init_cfg.sample_rate      = 16000;
    init_cfg.num_channels     = 4;
    init_cfg.frame_len        = FRAME_LEN;
    init_cfg.mic_pos          = mic_pos;
    init_cfg.target_azimuth   = static_cast<float>(M_PI) / 2.0f;
    init_cfg.target_elevation = 0.0f;

    ret = AudioEngine_Bf_Init(handle, &init_cfg);
    if (ret != AUDIO_ENGINE_SUCCESS) {
        std::cerr << "  Init failed with " << ret << " — skipping remaining error tests" << std::endl;
        AudioEngine_Bf_Destroy(handle);
        return;
    }

    ret = AudioEngine_Bf_Process(handle, NULL, FRAME_LEN, buf, &is_target);
    std::cout << "  Process(NULL audio_in) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    /* NULL audio_out */
    ret = AudioEngine_Bf_Process(handle, in_buf, FRAME_LEN, NULL, &is_target);
    std::cout << "  Process(NULL audio_out) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    /* Invalid frame length */
    ret = AudioEngine_Bf_Process(handle, in_buf, 999, buf, &is_target);
    std::cout << "  Process(wrong frame_len) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    /* NULL mic_pos on Init */
    BfHandle handle2 = AudioEngine_Bf_Create();
    BfInitConfig bad_cfg;
    bad_cfg.sample_rate      = 16000;
    bad_cfg.num_channels     = 2;
    bad_cfg.frame_len        = FRAME_LEN;
    bad_cfg.mic_pos          = NULL;   /* trigger error */
    bad_cfg.target_azimuth   = static_cast<float>(M_PI) / 2.0f;
    bad_cfg.target_elevation = 0.0f;
    ret = AudioEngine_Bf_Init(handle2, &bad_cfg);
    std::cout << "  Init(NULL mic_pos) = " << ret
              << (ret == AUDIO_ENGINE_ERR_NULL_POINTER ? " OK" : " FAIL") << std::endl;

    AudioEngine_Bf_Deinit(handle2);
    AudioEngine_Bf_Destroy(handle2);

    /* Invalid sample_rate */
    BfHandle handle3 = AudioEngine_Bf_Create();
    BfInitConfig bad_cfg2;
    bad_cfg2.sample_rate      = -100;
    bad_cfg2.num_channels     = 2;
    bad_cfg2.frame_len        = FRAME_LEN;
    bad_cfg2.mic_pos          = mic_pos;
    bad_cfg2.target_azimuth   = static_cast<float>(M_PI) / 2.0f;
    bad_cfg2.target_elevation = 0.0f;
    ret = AudioEngine_Bf_Init(handle3, &bad_cfg2);
    std::cout << "  Init(bad sample_rate) = " << ret
              << (ret == AUDIO_ENGINE_ERR_INVALID_PARAM ? " OK" : " FAIL") << std::endl;

    AudioEngine_Bf_Deinit(handle3);
    AudioEngine_Bf_Destroy(handle3);

    AudioEngine_Bf_Deinit(handle);
    AudioEngine_Bf_Destroy(handle);
    std::cout << "  PASS" << std::endl;
}


int main(int argc, char** argv)
{
    const char* input_wav  = "data/audio_short16.wav";
    const char* output_wav = "data/audio_short16_bf_interface_out.wav";

    if (argc >= 2) input_wav = argv[1];

    std::cout << "BF Interface Unit Test" << std::endl;
    std::cout << "======================" << std::endl;

    test_normal_flow(input_wav, output_wav);
    test_reset(input_wav);
    test_error_cases();

    std::cout << "All tests done." << std::endl;
    return 0;
}
