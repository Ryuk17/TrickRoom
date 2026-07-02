#include <iostream>
#include "common_audio/wav_file.h"
#include "modules/audio_processing/vad/voice_activity_detector.h"

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
        "data/voice_engine/audio_short16_vad_out.wav", 
        wav_reader.sample_rate(),
        wav_reader.num_channels(), 
        WavFile::SampleFormat::kInt16
    );

    auto rate = wav_reader.sample_rate();

    VoiceActivityDetector vad;

    int total_samples = 0;
    float threshold = 0.5f;

    int16_t wav_data[FRAME_LEN] = {0};
    int16_t zeros[FRAME_LEN] = {0};
    int16_t probs[FRAME_LEN] = {0};
    for(int i=0; i<FRAME_LEN;i++)
    {
        probs[i] = 32000;
    }

    while(true) 
    {
        int read_samples = wav_reader.ReadSamples(FRAME_LEN, wav_data);

        vad.ProcessChunk(wav_data, FRAME_LEN, rate);
        float mean_probability = vad.last_voice_probability();
        
        if(mean_probability > threshold)
        {
            wav_writer.WriteSamples(probs, read_samples);
        }else
        {
            wav_writer.WriteSamples(zeros, read_samples);
        }
        
        total_samples += read_samples;
        if(read_samples < FRAME_LEN)
        {
            break;
        }
    }
    std::cout<< "total write samples: " << total_samples << std::endl;
    return 0;
}
