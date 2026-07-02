#include <iostream>
#include "common_audio/wav_file.h"
#include "modules/audio_processing/aec3/echo_canceller3.h"
#include "api/environment/environment_factory.h"

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
        "data/voice_engine/audio_nearend16k_aec3_out.wav", 
        farend_wav_reader.sample_rate(),
        farend_wav_reader.num_channels(), 
        WavFile::SampleFormat::kInt16
    );

    int ret = 0;
    EchoCanceller3 aec3(CreateEnvironment(), EchoCanceller3Config(),
                        /*multichannel_config=*/std::nullopt,
                        /*neural_residual_echo_estimator=*/nullptr,
                        farend_wav_reader.sample_rate(), 1, 1);

    int total_samples = 0;
    int16_t farend_wav_data[FRAME_LEN];
    int16_t nearend_wav_data[FRAME_LEN];
    int16_t out_wav_data[FRAME_LEN];

    auto rate = farend_wav_reader.sample_rate();
    auto num_channels = farend_wav_reader.num_channels();

    StreamConfig stream_config(rate, num_channels);
    AudioBuffer capture_buffer(rate, num_channels, rate, num_channels, rate, num_channels);
    AudioBuffer render_buffer(rate, num_channels, rate, num_channels, rate, num_channels);

    while(true) 
    {
        int farend_read_samples = farend_wav_reader.ReadSamples(FRAME_LEN, farend_wav_data);
        int nearend_read_samples = nearend_wav_reader.ReadSamples(FRAME_LEN, nearend_wav_data);
        
        aec3.AnalyzeCapture(&capture_buffer);

        render_buffer.CopyFrom(farend_wav_data, stream_config);
        capture_buffer.CopyFrom(nearend_wav_data, stream_config);
        if (rate > 16000) 
        {
            render_buffer.SplitIntoFrequencyBands();
            capture_buffer.SplitIntoFrequencyBands();
        }

        aec3.AnalyzeRender(&render_buffer);
        aec3.ProcessCapture(&capture_buffer, false);

        capture_buffer.CopyTo(stream_config, out_wav_data);

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
