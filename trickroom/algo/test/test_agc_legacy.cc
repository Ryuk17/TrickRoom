#include <iostream>
#include "common_audio/wav_file.h"
#include "modules/audio_processing/agc/legacy/gain_control.h"

#define FRAME_LEN (160)

using namespace webrtc;

int main(int argc, char **argv) 
{
    char wav_file[1024] = "data/voice_engine/audio_short16.wav";

    WavReader wav_reader(wav_file);
    std::cout<< "sample_rate: " << wav_reader.sample_rate() << std::endl;
    std::cout<< "num_channels: " << wav_reader.num_channels() << std::endl;
    std::cout<< "read samples: " << wav_reader.num_samples() << std::endl;

    WavWriter wav_writer(
        "data/voice_engine/audio_short16_agc_legacy_out.wav", 
        wav_reader.sample_rate(),
        wav_reader.num_channels(), 
        WavFile::SampleFormat::kInt16
    );

    auto rate = wav_reader.sample_rate();

    void *agc_handle = WebRtcAgc_Create();
    int ret = WebRtcAgc_Init(agc_handle, 0, 255, kAgcModeAdaptiveDigital, rate);
    if(ret != 0)
    {
        std::cout<< "WebRtcAgc_Init failed" << std::endl;
        return -1;
    }
    
    WebRtcAgcConfig config;
    config.compressionGaindB = 9,
    config.limiterEnable = 1,
    config.targetLevelDbfs = 3;
    ret = WebRtcAgc_set_config(agc_handle, config);
    if(ret != 0)
    {
        std::cout<< "WebRtcAgc_set_config failed" << std::endl;
        return -1;
    }
    
    int total_samples = 0;
    int16_t wav_data[FRAME_LEN] = {0};
    int16_t output[FRAME_LEN] = {0};
    int32_t gains[11] = {0};

    size_t num_bands = 1;
    int inMicLevel = 0, outMicLevel = 0;
    uint8_t saturationWarning = 1;
    int16_t echo = 0;

    while(true) 
    {
        int read_samples = wav_reader.ReadSamples(FRAME_LEN, wav_data);
        const int16_t* bands[] = {wav_data};
        const int16_t* const* audio_input = bands;
        ret = WebRtcAgc_Analyze(agc_handle, audio_input, num_bands, FRAME_LEN,
                inMicLevel, &outMicLevel, echo, &saturationWarning, gains);
        if(ret != 0)
        {
            std::cout<< "WebRtcAgc_set_config failed" << std::endl;
            return -1;
        }

        int16_t* const output_bands[] = {output};
        int16_t* const* audio_output = output_bands;
        ret = WebRtcAgc_Process(agc_handle, gains, audio_input, num_bands, audio_output);
        if(ret != 0)
        {
            std::cout<< "WebRtcAgc_set_config failed" << std::endl;
            return -1;
        }
        wav_writer.WriteSamples(output, read_samples);

        total_samples += read_samples;
        if(read_samples < FRAME_LEN)
        {
            break;
        }
    }
    std::cout<< "total write samples: " << total_samples << std::endl;

    WebRtcAgc_Free(agc_handle);
    return 0;
}
