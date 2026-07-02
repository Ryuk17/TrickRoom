#include <AL/al.h>
#include <AL/alc.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

