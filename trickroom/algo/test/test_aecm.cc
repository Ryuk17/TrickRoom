#include <iostream>
#include "common_audio/wav_file.h"
#include "modules/audio_processing/aecm/echo_control_mobile.h"

#define FRAME_LEN (160)

using namespace webrtc;

int main(int argc, char **argv) 
{
    char farend_file[1024] = "data/voice_engine/audio_farend16k.wav";
    char nearend_file[1024] = "data/voice_engine/audio_nearend16k.wav";



    WavReader farend_wav_reader(farend_file);
    WavReader nearend_wav_reader(nearend_file);
    assert(farend_wav_reader.num_channels() == nearend_wav_reader.num_channels());
    assert(farend_wav_reader.sample_rate() == nearend_wav_reader.sample_rate());

    std::cout<< "sample_rate: " << farend_wav_reader.sample_rate() << std::endl;
    std::cout<< "num_channels: " << farend_wav_reader.num_channels() << std::endl;
    std::cout<< "read samples: " << farend_wav_reader.num_samples() << std::endl;


    WavWriter wav_writer(
        "data/voice_engine/audio_nearend16k_aecm_out.wav", 
        farend_wav_reader.sample_rate(),
        farend_wav_reader.num_channels(), 
        WavFile::SampleFormat::kInt16
    );

    int ret = 0;
    void* aecmInst = WebRtcAecm_Create();
    ret = WebRtcAecm_Init(aecmInst, farend_wav_reader.sample_rate());
    if (ret != 0)
    {
        std::cout<< "WebRtcAecm_Init error: " << ret << std::endl;
    }else
    {
        std::cout<< "WebRtcAecm_Init success" << std::endl;
    }

    AecmConfig config;
    config.cngMode = 1;
    config.echoMode = 3;
    ret = WebRtcAecm_set_config(aecmInst, config);
    if (ret != 0)
    {
        std::cout<< "WebRtcAecm_set_config error: " << ret << std::endl;
    }else
    {
        std::cout<< "WebRtcAecm_set_config success" << std::endl;
    }

    int total_samples = 0;
    int16_t farend_wav_data[FRAME_LEN];
    int16_t nearend_wav_data[FRAME_LEN];
    int16_t out_wav_data[FRAME_LEN];
    while(true) 
    {
        int farend_read_samples = farend_wav_reader.ReadSamples(FRAME_LEN, farend_wav_data);
        int nearend_read_samples = nearend_wav_reader.ReadSamples(FRAME_LEN, nearend_wav_data);

        ret = WebRtcAecm_BufferFarend(aecmInst, farend_wav_data, farend_read_samples);
        if (ret != 0)
        {
            std::cout<< "WebRtcAecm_BufferFarend error: " << ret << std::endl;
        }

        ret = WebRtcAecm_Process(aecmInst, nearend_wav_data, nullptr, out_wav_data, farend_read_samples, 0);
        if (ret != 0)
        {
            std::cout<< "WebRtcAecm_Process error: " << ret << std::endl;
        }

        wav_writer.WriteSamples(out_wav_data, farend_read_samples);
        total_samples += farend_read_samples;
        if(farend_read_samples < FRAME_LEN)
        {
            break;
        }
    }
    std::cout<< "total write samples: " << total_samples << std::endl;
    return 0;
}
