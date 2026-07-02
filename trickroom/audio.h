#include <AL/al.h>
#include <AL/alc.h>

#define VOICE_HEADER 12
#define VOICE_SAMPLE_RATE 48000

#define MIC_BUFFER_SIZE		3*1920*2
#define MAX_PACKET_SIZE		(2048)



#define MAX_DEPTH 6



#ifndef AUDIO_H
#define AUDIO_H

typedef struct
{
	short	format;
	short	channels;
	int	sampleRate;
	int	avgSampleRate;
	short	align;
	short	sampleSize;
} waveFormat_t;

typedef struct
{
	//	char			file[LINE_SIZE];
	waveFormat_t	*format;
	void			*pcmData;
	int				dataSize;
	int				duration;
	char			*data;
	int				buffer;
} wave_t;


class Audio
{
public:
	void init();
	void play(int hSource);
	void stop(int hSource);
	int create_source(bool loop, bool global);
	void source_position(int hSource, float *position);
	void source_velocity(int hSource, float *velocity);
	void listener_position(float *position);
	void listener_velocity(float *velocity);
	void listener_orientation(float *orientation);
	void delete_source(int hSource);
	bool select_buffer(int hSource, int hBuffer);
	void delete_buffer(int hBuffer);
	void destroy();

	void set_audio_model(int model);
	void capture_start();
	int capture_sample(unsigned short *pcm, int &size);
	void capture_stop();

	ALCdevice		*microphone;

private:
	int checkFormat(char *data, char *format);
	char *findChunk(char *chunk, char *id, int *size, char *end);

    ALenum alFormat(wave_t *wave);

	ALCdevice		*device;
	ALCcontext		*context;


	int selected_effect;

};

#endif
