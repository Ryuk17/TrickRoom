#include <AL/al.h>
#include <AL/alc.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef AUDIO_DEBUG_DUMP
#if defined(_WIN32)
#include <direct.h>
#define AE_MKDIR(dir) _mkdir(dir)
#else
#include <sys/stat.h>
#define AE_MKDIR(dir) mkdir(dir, 0755)
#endif
#endif

#include "audio.h"

char *GetALErrorString(ALenum err)
{
    switch(err)
    {
        case AL_NO_ERROR:
            return "AL_NO_ERROR";

        case AL_INVALID_NAME:
            return "AL_INVALID_NAME";

        case AL_INVALID_ENUM:
            return "AL_INVALID_ENUM";

        case AL_INVALID_VALUE:
            return "AL_INVALID_VALUE";

        case AL_INVALID_OPERATION:
            return "AL_INVALID_OPERATION";

        case AL_OUT_OF_MEMORY:
            return "AL_OUT_OF_MEMORY";
    };
	return "?";
}


void Audio::init()
{
	selected_effect = 0;

	printf("Using default audio device: %s\n", alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER));
	device = alcOpenDevice(NULL);
	if (device == NULL)
	{
		printf("No sound device/driver has been found.\n");
		return;
	}

	// step is every 8 ms, 48khz per second, or 48 samples per ms, which is 384 samples per step give or take
	microphone = alcCaptureOpenDevice(NULL, VOICE_SAMPLE_RATE, AL_FORMAT_MONO16, MIC_BUFFER_SIZE);
	if (microphone == NULL)
	{
		printf("No microphone has been found.\n");
		return;
	}

    context = alcCreateContext(device, NULL);

	if (context == NULL)
	{
		printf("alcCreateContext failed.\n");
	}

	if ( alcMakeContextCurrent(context) == ALC_FALSE )
	{
		ALCenum error = alcGetError(device);

		switch (error)
		{
		case ALC_NO_ERROR:
			printf("alcMakeContextCurrent failed: No error.\n");
			break;
		case ALC_INVALID_DEVICE:
			printf("alcMakeContextCurrent failed: Invalid device.\n");
			break;
		case ALC_INVALID_CONTEXT:
			printf("alcMakeContextCurrent failed: Invalid context.\n");
			break;
		case ALC_INVALID_ENUM:
			printf("alcMakeContextCurrent failed: Invalid enum.\n");
			break;
		case ALC_INVALID_VALUE:
			printf("alcMakeContextCurrent failed: Invalid value.\n");
			break;
		case ALC_OUT_OF_MEMORY:
			printf("alcMakeContextCurrent failed: Out of memory.\n");
			break;
		}
		return;
	}

	// AEC → NS → AGC2 → SRC voice pipeline (48kHz mic → 16kHz codec)
	init_pipeline();

	//gain = 	(distance / AL_REFERENCE_DISTANCE) ^ (-AL_ROLLOFF_FACTOR
	alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
//	alListenerf(AL_MAX_DISTANCE, 2000.0f);
//	alListenerf(AL_REFERENCE_DISTANCE, 75.0f);
//	alListenerf(AL_ROLLOFF_FACTOR, 0.0001);
	

	alDopplerFactor(1.0f);
//	alDopplerVelocity(8.0f);
//	alSpeedOfSound(343.3f * UNITS_TO_METERS);
#ifdef WIN32
	// 8 units = 1 foot, 1 foot = 0.3 meters
	// 1 unit = 0.3 / 8 meters
//	alListenerf(AL_METERS_PER_UNIT, 0.375f);

	float zero[3] = { 0.0 };
	listener_position(zero);
#endif
}


void Audio::set_audio_model(int model)
{
	switch (model)
	{
	case 0:
		alDistanceModel(AL_INVERSE_DISTANCE);
		break;
	case 1:
		alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
		break;
	case 2:
		alDistanceModel(AL_LINEAR_DISTANCE);
		break;
	case 3:
		alDistanceModel(AL_LINEAR_DISTANCE_CLAMPED);
		break;
	case 4:
		alDistanceModel(AL_EXPONENT_DISTANCE);
		break;
	case 5:
		alDistanceModel(AL_EXPONENT_DISTANCE_CLAMPED);
		break;
	}
}


int Audio::create_source(bool loop, bool global)
{
	ALuint hSource = -1;
	ALenum		al_err;
	bool enable_filter = false;

	alGenSources(1, &hSource);
	al_err = alGetError();
	if (al_err != AL_NO_ERROR)
	{
		printf("Unable to generate audio source: %s\n", GetALErrorString(al_err));
		return hSource;
	}

	if (loop)
		alSourcei(hSource, AL_LOOPING, AL_TRUE);
	else
		alSourcei(hSource, AL_LOOPING, AL_FALSE);

	if (global)
		alSourcei(hSource, AL_SOURCE_RELATIVE, AL_TRUE); 
	else
		alSourcei(hSource, AL_SOURCE_RELATIVE, AL_FALSE);

//	effects(hSource, enable_filter, 0);

	return hSource;
}

void Audio::source_position(int hSource, float *position)
{
	if (hSource == -1)
	{
		//printf("Attempting to position unallocated source\n");
		return;
	}

	alSourcefv(hSource, AL_POSITION, position);
	ALenum al_err = alGetError();
	if (al_err != AL_NO_ERROR)
	{
		printf("Error alSourcefv position : %s\n", GetALErrorString(al_err));
		hSource = -1;
	}
}

void Audio::source_velocity(int hSource, float *velocity)
{

	if (hSource == -1)
	{
		//printf("Attempting to add velocity to unallocated source\n");
		return;
	}

	alSourcefv(hSource, AL_VELOCITY, velocity);
	ALenum al_err = alGetError();
	if (al_err != AL_NO_ERROR)
	{
		printf("Error alSourcefv velocity : %s\n", GetALErrorString(al_err));
		hSource = -1;
	}
}

void Audio::listener_position(float *position)
{
	ALenum al_err;

	alListenerfv(AL_POSITION, position);
	al_err = alGetError();
	if (al_err != AL_NO_ERROR)
	{
		printf("Error alListenerfv : %s\n", GetALErrorString(al_err));
	}
}

void Audio::listener_velocity(float *velocity)
{
	ALenum al_err;

	alListenerfv(AL_VELOCITY, velocity);

	al_err = alGetError();
	if (al_err != AL_NO_ERROR)
	{
		printf("Error alListenerfv velocity: %s\n", GetALErrorString(al_err));
	}
}

void Audio::listener_orientation(float *orientation)
{
	ALenum al_err;

	alListenerfv(AL_ORIENTATION, orientation);

	al_err = alGetError();
	if (al_err != AL_NO_ERROR)
	{
		printf("Error alListenerfv orientation: %s\n", GetALErrorString(al_err));
	}
}

void Audio::delete_source(int hSource)
{
	ALenum		al_err;

    alDeleteSources(1, (ALuint *)&hSource);
	al_err = alGetError();
	if (al_err != AL_NO_ERROR)
	{
		printf("Unable to delete audio source: %s\n", GetALErrorString(al_err));
	}
}

bool Audio::select_buffer(int hSource, int hBuffer)
{
	ALenum		al_err;

	alSourceStop(hSource);
	alSourcei(hSource, AL_BUFFER, hBuffer);
	al_err = alGetError();
	if (al_err != AL_NO_ERROR)
	{
		printf("Unable to add buffer to source: %s\n", GetALErrorString(al_err));
		return true;
	}
	return true;
}

void Audio::delete_buffer(int hBuffer)
{
	ALenum		al_err;

	alDeleteBuffers(1, (ALuint *)&hBuffer);
	al_err = alGetError();
	if (al_err != AL_NO_ERROR)
	{
		printf("Unable to delete buffer: %s\n", GetALErrorString(al_err));
	}
}

void Audio::play(int hSource)
{
	alSourcePlay(hSource);
}

void Audio::stop(int hSource)
{
	alSourceStop(hSource);
}

void Audio::destroy()
{
	destroy_pipeline();
	alcMakeContextCurrent(NULL);
	alcDestroyContext(context);
	alcCloseDevice(device);
	alcCaptureCloseDevice(microphone);
	context = NULL;
	device = NULL;
	microphone = NULL;
}

int Audio::checkFormat(char *data, char *format)
{
	return memcmp(&data[8], format, 4);
}

char *Audio::findChunk(char *chunk, char *id, int *size, char *end)
{
	while (chunk < end)
	{
		*size = *((int *)(chunk + 4));

		if ( memcmp(chunk, id, 4) == 0 )
			return chunk + 8;
		else
			chunk += *size + 8;
	}
	return NULL;
}

ALenum Audio::alFormat(wave_t *wave)
{
	if (wave->format->channels == 2)
	{
		if (wave->format->sampleSize == 16)
			return AL_FORMAT_STEREO16;
		else
			return AL_FORMAT_STEREO8;
	}
	else
	{
		if (wave->format->sampleSize == 16)
			return AL_FORMAT_MONO16;
		else
			return AL_FORMAT_MONO8;
	}
}

void Audio::capture_start()
{
	if (microphone == NULL)
		return;

	alcCaptureStart(microphone);
}

int Audio::capture_sample(unsigned short *pcm, int &size)
{
	// Samples are not bytes, 2 bytes per sample.
	// Use bytes everywhere but where API's dictate samples
	unsigned int samples = 0;

	if (microphone == NULL)
		return -1;

	static int max_recv = 0;

	alcGetIntegerv(microphone, ALC_CAPTURE_SAMPLES, sizeof(int), (int *)&samples);
	if (samples * 2 > MIC_BUFFER_SIZE)
	{
		// we have more than a buffer, only get one buffer out
		size = MIC_BUFFER_SIZE;
	}
	else if (samples * 2 < MIC_BUFFER_SIZE)
	{
		// if we have less than one buffer, leave it in the buffer
		size = 0;
		return 0;
	}

	// Get one buffer bull of samples
	alcCaptureSamples(microphone, pcm, (MIC_BUFFER_SIZE >> 1));

	if (size > max_recv)
	{
		max_recv = size;
	}
	return size;
}


void Audio::capture_stop()
{
	if (microphone == NULL)
		return;

	alcCaptureStop(microphone);
}


/* ============================================================================
 * AEC → NS → AGC2 → SRC pipeline (48kHz in → 16kHz out)
 * ==========================================================================*/
bool Audio::init_pipeline()
{
	if (pipeline_initialized_)
		return true;

	memset(farend_buf_, 0, sizeof(farend_buf_));
	farend_wr_ = 0;
	farend_rd_ = 0;

	/* Create all handles first */
	aec_		= AudioEngine_Aec_Create();
	ns_			= AudioEngine_Ns_Create();
	agc2_		= AudioEngine_Agc2_Create();
	src_		= AudioEngine_Resample_Create();
	src_farend_	= AudioEngine_Resample_Create();

	if (!aec_ || !ns_ || !agc2_ || !src_ || !src_farend_)
	{
		printf("Audio::init_pipeline: handle creation failed\n");
		destroy_pipeline();
		return false;
	}

	/* AEC @48kHz mono */
	AecInitConfig aec_cfg;
	aec_cfg.sample_rate			= PIPELINE_SAMPLE_RATE;
	aec_cfg.num_render_channels	= 1;
	aec_cfg.num_capture_channels	= 1;
	if (AudioEngine_Aec_Init(aec_, &aec_cfg) != AUDIO_ENGINE_SUCCESS)
	{
		printf("Audio::init_pipeline: AEC init failed\n");
		destroy_pipeline();
		return false;
	}

	/* NS @48kHz mono, default suppression (12dB) */
	NsInitConfig ns_cfg;
	ns_cfg.sample_rate		= PIPELINE_SAMPLE_RATE;
	ns_cfg.num_channels		= 1;
	ns_cfg.suppression_level	= 1;
	if (AudioEngine_Ns_Init(ns_, &ns_cfg) != AUDIO_ENGINE_SUCCESS)
	{
		printf("Audio::init_pipeline: NS init failed\n");
		destroy_pipeline();
		return false;
	}

	/* AGC2 @48kHz mono, defaults */
	Agc2InitConfig agc2_cfg;
	agc2_cfg.sample_rate					= PIPELINE_SAMPLE_RATE;
	agc2_cfg.num_channels					= 1;
	agc2_cfg.headroom_db					= 5.0f;
	agc2_cfg.max_gain_db					= 50.0f;
	agc2_cfg.initial_gain_db				= 15.0f;
	agc2_cfg.max_gain_change_db_per_second	= 6.0f;
	agc2_cfg.max_output_noise_level_dbfs	= -50.0f;
	if (AudioEngine_Agc2_Init(agc2_, &agc2_cfg) != AUDIO_ENGINE_SUCCESS)
	{
		printf("Audio::init_pipeline: AGC2 init failed\n");
		destroy_pipeline();
		return false;
	}

	/* Resamplers: 48k→16k (mic path), 16k→48k (far-end upsample) */
	ResampleInitConfig src_cfg;
	src_cfg.src_sample_rate	= PIPELINE_SAMPLE_RATE;
	src_cfg.dst_sample_rate	= 16000;
	src_cfg.num_channels	= 1;
	if (AudioEngine_Resample_Init(src_, &src_cfg) != AUDIO_ENGINE_SUCCESS)
	{
		printf("Audio::init_pipeline: SRC init failed\n");
		destroy_pipeline();
		return false;
	}

	ResampleInitConfig farend_cfg;
	farend_cfg.src_sample_rate	= 16000;
	farend_cfg.dst_sample_rate	= PIPELINE_SAMPLE_RATE;
	farend_cfg.num_channels		= 1;
	if (AudioEngine_Resample_Init(src_farend_, &farend_cfg) != AUDIO_ENGINE_SUCCESS)
	{
		printf("Audio::init_pipeline: far-end SRC init failed\n");
		destroy_pipeline();
		return false;
	}

#ifdef AUDIO_DEBUG_DUMP
	/* One WAV per stage input, in dump/ (mkdir if needed) */
	AE_MKDIR("dump");
	wav_aec_nearend_ = new DrWavWriter("dump/pipeline_aec_in_nearend.wav", PIPELINE_SAMPLE_RATE, 1);
	wav_aec_farend_  = new DrWavWriter("dump/pipeline_aec_in_farend.wav",  PIPELINE_SAMPLE_RATE, 1);
	wav_ns_in_       = new DrWavWriter("dump/pipeline_ns_in.wav",          PIPELINE_SAMPLE_RATE, 1);
	wav_agc2_in_     = new DrWavWriter("dump/pipeline_agc2_in.wav",        PIPELINE_SAMPLE_RATE, 1);
	wav_src_in_      = new DrWavWriter("dump/pipeline_src_in.wav",         PIPELINE_SAMPLE_RATE, 1);
	wav_src_out_     = new DrWavWriter("dump/pipeline_src_out.wav",        16000, 1);
#endif

	pipeline_initialized_ = true;
	printf("Audio::init_pipeline: AEC→NS→AGC2→SRC OK (48kHz → 16kHz)\n");
	return true;
}

void Audio::destroy_pipeline()
{
	if (aec_)			{ AudioEngine_Aec_Deinit(aec_);		AudioEngine_Aec_Destroy(aec_);		aec_ = NULL; }
	if (ns_)			{ AudioEngine_Ns_Deinit(ns_);		AudioEngine_Ns_Destroy(ns_);		ns_ = NULL; }
	if (agc2_)			{ AudioEngine_Agc2_Deinit(agc2_);	AudioEngine_Agc2_Destroy(agc2_);	agc2_ = NULL; }
	if (src_)			{ AudioEngine_Resample_Deinit(src_);	AudioEngine_Resample_Destroy(src_);	src_ = NULL; }
	if (src_farend_)	{ AudioEngine_Resample_Deinit(src_farend_); AudioEngine_Resample_Destroy(src_farend_); src_farend_ = NULL; }

#ifdef AUDIO_DEBUG_DUMP
	delete wav_aec_nearend_;	wav_aec_nearend_ = NULL;
	delete wav_aec_farend_;		wav_aec_farend_ = NULL;
	delete wav_ns_in_;			wav_ns_in_ = NULL;
	delete wav_agc2_in_;		wav_agc2_in_ = NULL;
	delete wav_src_in_;			wav_src_in_ = NULL;
	delete wav_src_out_;		wav_src_out_ = NULL;
#endif

	pipeline_initialized_ = false;
}

void Audio::push_farend(const short *farend, int samples)
{
	if (!pipeline_initialized_ || farend == NULL || samples <= 0)
		return;

	/* Upsample remote PCM 16k → 48k (AEC requires render at 48k) */
	int up_sz = 0;
	if (AudioEngine_Resample_Process(src_farend_, farend, samples,
	                                 farend_tmp_, F_AREND_UP_MAX, &up_sz) != AUDIO_ENGINE_SUCCESS
	    || up_sz <= 0)
		return;

	/* Ring write with wrap */
	int pos = farend_wr_ % F_AREND_BUF_SIZE;
	if (pos + up_sz <= F_AREND_BUF_SIZE)
	{
		memcpy(farend_buf_ + pos, farend_tmp_, up_sz * sizeof(short));
	}
	else
	{
		int first = F_AREND_BUF_SIZE - pos;
		memcpy(farend_buf_ + pos, farend_tmp_, first * sizeof(short));
		memcpy(farend_buf_, farend_tmp_ + first, (up_sz - first) * sizeof(short));
	}
	farend_wr_ += up_sz;
}

int Audio::process_pipeline(const short *in, int in_samples,
                            short *out, int max_out_samples)
{
	if (!pipeline_initialized_ || in == NULL || out == NULL)
		return 0;
	if (in_samples <= 0 || in_samples % PIPELINE_FRAME_SIZE != 0)
		return 0;
	if (max_out_samples < (in_samples / PIPELINE_FRAME_SIZE) * PIPELINE_SRC_OUT)
		return 0;

	short farend[PIPELINE_FRAME_SIZE];
	short tmp_aec[PIPELINE_FRAME_SIZE];
	short tmp_ns[PIPELINE_FRAME_SIZE];
	short tmp_agc2[PIPELINE_FRAME_SIZE];

	int total_out = 0;
	int n_frames = in_samples / PIPELINE_FRAME_SIZE;

	for (int f = 0; f < n_frames; f++)
	{
		const short *nearend = in + f * PIPELINE_FRAME_SIZE;
		int out_sz = 0;

		/* Pull one 480-sample far-end frame from the ring (wrap at end) */
		int pos = farend_rd_ % F_AREND_BUF_SIZE;
		if (pos + PIPELINE_FRAME_SIZE <= F_AREND_BUF_SIZE)
		{
			memcpy(farend, farend_buf_ + pos, PIPELINE_FRAME_SIZE * sizeof(short));
		}
		else
		{
			int first = F_AREND_BUF_SIZE - pos;
			memcpy(farend, farend_buf_ + pos, first * sizeof(short));
			memcpy(farend + first, farend_buf_, (PIPELINE_FRAME_SIZE - first) * sizeof(short));
		}
		farend_rd_ += PIPELINE_FRAME_SIZE;

		/* Stage 1: AEC (nearend + farend) */
#ifdef AUDIO_DEBUG_DUMP
		wav_aec_nearend_->WriteSamples(nearend, PIPELINE_FRAME_SIZE);
		wav_aec_farend_->WriteSamples(farend, PIPELINE_FRAME_SIZE);
#endif
		if (AudioEngine_Aec_Process(aec_, nearend, farend, PIPELINE_FRAME_SIZE,
		                            tmp_aec, PIPELINE_FRAME_SIZE, &out_sz) != AUDIO_ENGINE_SUCCESS
		    || out_sz != PIPELINE_FRAME_SIZE)
			break;

		/* Stage 2: NS */
#ifdef AUDIO_DEBUG_DUMP
		wav_ns_in_->WriteSamples(tmp_aec, PIPELINE_FRAME_SIZE);
#endif
		if (AudioEngine_Ns_Process(ns_, tmp_aec, PIPELINE_FRAME_SIZE,
		                           tmp_ns, PIPELINE_FRAME_SIZE, &out_sz) != AUDIO_ENGINE_SUCCESS
		    || out_sz != PIPELINE_FRAME_SIZE)
			break;

		/* Stage 3: AGC2 */
#ifdef AUDIO_DEBUG_DUMP
		wav_agc2_in_->WriteSamples(tmp_ns, PIPELINE_FRAME_SIZE);
#endif
		if (AudioEngine_Agc2_Process(agc2_, tmp_ns, PIPELINE_FRAME_SIZE,
		                             tmp_agc2, PIPELINE_FRAME_SIZE, &out_sz) != AUDIO_ENGINE_SUCCESS
		    || out_sz != PIPELINE_FRAME_SIZE)
			break;

		/* Stage 4: SRC 48k → 16k */
#ifdef AUDIO_DEBUG_DUMP
		wav_src_in_->WriteSamples(tmp_agc2, PIPELINE_FRAME_SIZE);
#endif
		if (AudioEngine_Resample_Process(src_, tmp_agc2, PIPELINE_FRAME_SIZE,
		                                 out + total_out, max_out_samples - total_out,
		                                 &out_sz) != AUDIO_ENGINE_SUCCESS
		    || out_sz != PIPELINE_SRC_OUT)
			break;
#ifdef AUDIO_DEBUG_DUMP
		wav_src_out_->WriteSamples(out + total_out, out_sz);
#endif

		total_out += out_sz;
	}

	return total_out;
}

