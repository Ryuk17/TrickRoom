/*
 * @Author: Ryuk
 * @Date: 2026-07-06 22:23:48
 * @LastEditors: Ryuk
 * @LastEditTime: 2026-07-06 22:35:58
 * @Description: First create
 */
#include <iostream>
#include "common_audio/dr_wav.h"
#include "common_audio/resampler/include/resampler.h"

#define FRAME_LEN (160)
#define RESAMPLED_FRAME_LEN (FRAME_LEN * 4)

using namespace webrtc;

int main(int argc, char **argv) 
{
    char wav_file[1024] = "data/voice_engine/audio_short16.wav";
    int dst_sample_rate = 48000;
    DrWavReader wav_reader(wav_file);
    std::cout<< "src sample_rate: " << wav_reader.sample_rate() << std::endl;
    std::cout<< "num_channels: " << wav_reader.num_channels() << std::endl;
    std::cout<< "read samples: " << wav_reader.num_samples() << std::endl;
    std::cout<< "dst sample_rate: " << dst_sample_rate << std::endl;

    DrWavWriter wav_writer(
        "data/voice_engine/audio_short16_resample_out.wav", 
        dst_sample_rate,
        wav_reader.num_channels()    );

    auto rate = wav_reader.sample_rate();

    Resampler resampler(wav_reader.sample_rate(), dst_sample_rate, wav_reader.num_channels());

    int total_samples = 0;
    int16_t wav_data[FRAME_LEN] = {0};
    int16_t resampled_data[RESAMPLED_FRAME_LEN] = {0};
    while(true) 
    {
        int read_samples = wav_reader.ReadSamples(FRAME_LEN, wav_data);

        size_t resampled_samples = 0;
        resampler.Push(wav_data, FRAME_LEN, resampled_data, RESAMPLED_FRAME_LEN, resampled_samples);

        wav_writer.WriteSamples(resampled_data, resampled_samples);
        total_samples += resampled_samples;
        if(read_samples < FRAME_LEN)
        {
            break;
        }
    }
    std::cout<< "total write samples: " << total_samples << std::endl;
    return 0;
}
