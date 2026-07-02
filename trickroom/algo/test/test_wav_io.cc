#include <iostream>
#include "common_audio/wav_file.h"

using namespace webrtc;

int main(int argc, char **argv) 
{
    char wav_file[1024] = "data/voice_engine/audio_tiny8.wav";

    WavReader wav_reader(wav_file);
    std::cout<< "sample_rate: " << wav_reader.sample_rate() << std::endl;
    std::cout<< "num_channels: " << wav_reader.num_channels() << std::endl;
    std::cout<< "read samples: " << wav_reader.num_samples() << std::endl;

    WavWriter wav_writer(
        "data/voice_engine/audio_tiny8_out.wav", 
        wav_reader.sample_rate(),
        wav_reader.num_channels(), 
        WavFile::SampleFormat::kInt16
    );

    int frame_num = 1024;
    int total_samples = 0;
    int16_t wav_data[1024];
    while(true) 
    {
        int read_samples = wav_reader.ReadSamples(frame_num, wav_data);
        wav_writer.WriteSamples(wav_data, read_samples);
        total_samples += read_samples;
        if(read_samples < frame_num)
        {
            break;
        }
    }
    std::cout<< "total write samples: " << total_samples << std::endl;
    return 0;
}
