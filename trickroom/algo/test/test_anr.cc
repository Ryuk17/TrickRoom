#include <iostream>
#include "common_audio/wav_file.h"
#include "modules/audio_processing/ns/noise_suppressor.h"

#define FRAME_LEN (160)

using namespace webrtc;

int main(int argc, char **argv) 
{
    char wav_file[1024] = "data/voice_engine/audio_long16noise.wav";

    WavReader wav_reader(wav_file);
    std::cout<< "sample_rate: " << wav_reader.sample_rate() << std::endl;
    std::cout<< "num_channels: " << wav_reader.num_channels() << std::endl;
    std::cout<< "read samples: " << wav_reader.num_samples() << std::endl;

    WavWriter wav_writer(
        "data/voice_engine/audio_long16noise_anr_out.wav", 
        wav_reader.sample_rate(),
        wav_reader.num_channels(), 
        WavFile::SampleFormat::kInt16
    );

    auto rate = wav_reader.sample_rate();
    auto num_channels = wav_reader.num_channels();
    const size_t num_bands = rate / 16000;

    AudioBuffer audio(rate, num_channels, rate, num_channels, rate,
                        num_channels);
    NsConfig cfg;
    NoiseSuppressor ns(cfg, rate, num_channels);

    StreamConfig stream_config(rate, num_channels);

    int total_samples = 0;
    int16_t wav_data[FRAME_LEN];
    while(true) 
    {
        int read_samples = wav_reader.ReadSamples(FRAME_LEN, wav_data);
        audio.CopyFrom(wav_data, stream_config);
        if (rate > 16000) 
        {
            audio.SplitIntoFrequencyBands();
        }

        ns.Analyze(audio);
        ns.Process(&audio);

        if (rate > 16000) 
        {
            audio.MergeFrequencyBands();
        }
        audio.CopyTo(stream_config, wav_data);

        wav_writer.WriteSamples(wav_data, read_samples);
        total_samples += read_samples;
        if(read_samples < FRAME_LEN)
        {
            break;
        }
    }
    std::cout<< "total write samples: " << total_samples << std::endl;
    return 0;
}
