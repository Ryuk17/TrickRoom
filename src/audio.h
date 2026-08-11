#include <AL/al.h>
#include <AL/alc.h>

#define VOICE_HEADER 12
#define VOICE_SAMPLE_RATE 48000

#define MIC_BUFFER_SIZE		3*1920*2
#define MAX_PACKET_SIZE		(2048)



#define MAX_DEPTH 6



#ifndef AUDIO_H
#define AUDIO_H

/* ============================================================================
 * libAE voice pipeline (AEC → NS → AGC2 → SRC)
 * ==========================================================================*/

/* Link against the static libAE_*.a archives (no dllimport) */
#ifndef AUDIO_ENGINE_STATIC
#define AUDIO_ENGINE_STATIC
#endif

/* Pipeline debug dump: uncomment to record every stage's input to dump\*.wav */
// #define AUDIO_DEBUG_DUMP 1

#include "interface/audio_engine_aec.h"
#include "interface/audio_engine_ns.h"
#include "interface/audio_engine_agc2.h"
#include "interface/audio_engine_src.h"
#include "utils/dr_wav.h"

/* Pipeline constants: 48kHz in, 16kHz out */
#define PIPELINE_SAMPLE_RATE	48000	/* AEC/NS/AGC2 processing rate */
#define PIPELINE_FRAME_SIZE		480		/* 10ms @ 48kHz                 */
#define PIPELINE_SRC_OUT		160		/* 10ms @ 16kHz (SRC output)    */
#define F_AREND_BLOCK			5760	/* 120ms block @48kHz           */
#define F_AREND_BUF_SIZE		11520	/* far-end ring: 2 blocks       */
#define F_AREND_UP_MAX			5760	/* max upsample out per push    */

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

	/* ---- AEC → NS → AGC2 → SRC voice pipeline (48kHz in → 16kHz out) ---- */
	bool init_pipeline();
	void destroy_pipeline();
	void push_farend(const short *farend, int samples);	/* remote PCM, 16kHz */
	int process_pipeline(const short *in, int in_samples,
	                     short *out, int max_out_samples);	/* 48kHz → 16kHz */

	ALCdevice		*microphone;

private:
	int checkFormat(char *data, char *format);
	char *findChunk(char *chunk, char *id, int *size, char *end);

    ALenum alFormat(wave_t *wave);

	ALCdevice		*device;
	ALCcontext		*context;


	int selected_effect;

	/* ---- AEC → NS → AGC2 → SRC pipeline ---- */
	AecHandle		aec_;
	NsHandle		ns_;
	Agc2Handle		agc2_;
	ResampleHandle	src_;			/* 48k → 16k (mic path)          */
	ResampleHandle	src_farend_;	/* 16k → 48k (far-end upsample)  */
	bool			pipeline_initialized_;

	/* Far-end ring buffer @48kHz: push_farend writes 5760-sample blocks,
	   process_pipeline reads 480-sample frames. Two blocks (11520 = 240ms)
	   so a write and a read never target the same block. */
	short	farend_buf_[F_AREND_BUF_SIZE];
	short	farend_tmp_[F_AREND_UP_MAX];	/* upsample scratch */
	int		farend_wr_;
	int		farend_rd_;

#ifdef AUDIO_DEBUG_DUMP
	/* One WAV per pipeline stage input + final output (dump\*.wav) */
	DrWavWriter	*wav_aec_nearend_;
	DrWavWriter	*wav_aec_farend_;
	DrWavWriter	*wav_ns_in_;
	DrWavWriter	*wav_agc2_in_;
	DrWavWriter	*wav_src_in_;
	DrWavWriter	*wav_src_out_;
#endif

};

#endif
