#include <iostream>
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include "common_audio/wav_file.h"
#include "common_audio/include/audio_util.h"
#include "modules/audio_processing/agc2/adaptive_digital_gain_controller.h"
#include "modules/audio_processing/agc2/speech_level_estimator.h"
#include "modules/audio_processing/agc2/noise_level_estimator.h"
#include "modules/audio_processing/agc2/vad_wrapper.h"
#include "modules/audio_processing/agc2/cpu_features.h"
#include "modules/audio_processing/agc2/agc2_common.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"

using namespace webrtc;

int main(int argc, char **argv)
{
    char wav_file[1024] = "data/voice_engine/audio_short16.wav";

    WavReader wav_reader(wav_file);
    auto rate = wav_reader.sample_rate();
    int num_channels = wav_reader.num_channels();
    int samples_per_channel = rate * kFrameDurationMs / 1000;

    std::cout << "sample_rate: " << rate << std::endl;
    std::cout << "num_channels: " << num_channels << std::endl;
    std::cout << "read samples: " << wav_reader.num_samples() << std::endl;
    std::cout << "samples_per_channel (10ms): " << samples_per_channel << std::endl;

    WavWriter wav_writer(
        "data/voice_engine/audio_short16_agc2_out.wav",
        rate, num_channels,
        WavFile::SampleFormat::kInt16
    );

    // Setup AGC2 components
    ApmDataDumper apm_data_dumper(0);

    AudioProcessing::Config::GainController2::AdaptiveDigital config;

    AdaptiveDigitalGainController gain_controller(
        &apm_data_dumper, config, kAdjacentSpeechFramesThreshold);

    SpeechLevelEstimator speech_level_estimator(
        &apm_data_dumper, config, kAdjacentSpeechFramesThreshold);

    std::unique_ptr<NoiseLevelEstimator> noise_level_estimator =
        CreateNoiseFloorEstimator(&apm_data_dumper);

    // Use the RNN-based VAD (key AGC2 improvement over legacy AGC).
    AvailableCpuFeatures cpu_features = NoAvailableCpuFeatures();
    VoiceActivityDetectorWrapper vad(cpu_features, rate);

    int total_samples = 0;
    int total_frames = 0;
    int frame_size = samples_per_channel * num_channels;

    std::vector<int16_t> wav_data(frame_size);
    std::vector<float> frame_float(frame_size);

    float initial_speech_level = speech_level_estimator.level_dbfs();
    std::cout << "initial speech level dbfs: " << initial_speech_level << std::endl;
    std::cout << "config headroom_db: " << config.headroom_db << std::endl;
    std::cout << "config max_gain_db: " << config.max_gain_db << std::endl;
    std::cout << "config initial_gain_db: " << config.initial_gain_db << std::endl;

    while(true)
    {
        int read_samples = wav_reader.ReadSamples(frame_size, wav_data.data());

        if(read_samples < samples_per_channel) break;

        int samples_this_frame = read_samples / num_channels;

        // Convert int16 to FloatS16 scale ([-32768.0, 32768.0]).
        S16ToFloatS16(wav_data.data(), read_samples, frame_float.data());

        // Create deinterleaved view for the frame.
        DeinterleavedView<float> view(frame_float.data(), samples_this_frame, num_channels);

        // RNN VAD: analyze the original audio to get speech probability.
        float speech_probability = vad.Analyze(view);

        // Compute RMS and peak levels in dBFS for SpeechLevelEstimator.
        float sum_sq = 0.0f;
        float peak = 0.0f;
        for (int i = 0; i < read_samples; ++i) {
            float val = frame_float[i];
            sum_sq += val * val;
            peak = std::max(peak, std::abs(val));
        }
        float rms = std::sqrt(sum_sq / read_samples);
        float rms_dbfs = FloatS16ToDbfs(rms);
        float peak_dbfs = FloatS16ToDbfs(peak);

        // Update speech level estimator with current frame's RMS, peak, and speech probability.
        speech_level_estimator.Update(rms_dbfs, peak_dbfs, speech_probability);

        // Estimate noise level.
        float noise_rms_dbfs = noise_level_estimator->Analyze(view);

        // Build FrameInfo for the adaptive digital gain controller.
        AdaptiveDigitalGainController::FrameInfo info;
        info.speech_probability = speech_probability;
        info.speech_level_dbfs = speech_level_estimator.level_dbfs();
        info.speech_level_reliable = speech_level_estimator.is_confident();
        info.noise_rms_dbfs = noise_rms_dbfs;
        info.headroom_db = config.headroom_db;
        // Use a reasonable default limiter envelope level (not critical for basic testing).
        info.limiter_envelope_dbfs = -2.0f;

        // Apply adaptive digital gain to the frame.
        gain_controller.Process(info, view);

        // Convert back from FloatS16 to int16.
        FloatS16ToS16(frame_float.data(), read_samples, wav_data.data());

        // Write output.
        wav_writer.WriteSamples(wav_data.data(), read_samples);

        total_samples += read_samples;
        total_frames++;

        if(read_samples < frame_size)
            break;
    }

    std::cout << "total frames processed: " << total_frames << std::endl;
    std::cout << "total write samples: " << total_samples << std::endl;
    std::cout << "final speech level dbfs: " << speech_level_estimator.level_dbfs() << std::endl;
    std::cout << "is confident: " << (speech_level_estimator.is_confident() ? "yes" : "no") << std::endl;

    return 0;
}
