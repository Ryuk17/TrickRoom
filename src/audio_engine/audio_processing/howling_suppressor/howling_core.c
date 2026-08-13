#include "howling_core.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "interface/audio_engine_log.h"
#define THIS_FILE "howling_core.c"
#define LOGV(...) LOG_DEBUG(__VA_ARGS__)


//#define DETECT_TONE_MIN 850
#define DETECT_TONE_MIN 650
#define DETECT_TONE_MAX 3000
#define DETECT_TONE_STEP 25
#define HOWLING_THRESHOLD 13


static howling_suppression_desp_t s_howling_suppression_desp = {
	(float)HOWLING_THRESHOLD,
	5,
	(float)DETECT_TONE_MIN,
	(float)DETECT_TONE_MAX,
	(float)DETECT_TONE_STEP,
	5,
	(float)0.7071f,
};

static int		s_detect_tone_count = (DETECT_TONE_MAX - DETECT_TONE_MIN) / DETECT_TONE_STEP + 1;
static float	s_howling_threshold = (float)HOWLING_THRESHOLD / (((float)DETECT_TONE_MAX - (float)DETECT_TONE_MIN) / (float)DETECT_TONE_STEP + (float)1.0f);



void howling_open(const howling_suppression_desp_t * desp)
{
	if(desp)
		memcpy(&s_howling_suppression_desp, desp, sizeof(howling_suppression_desp_t));
	s_detect_tone_count = (int)(desp->detect_freq_max - desp->detect_freq_min) / desp->detect_freq_interval + 1;
	s_howling_threshold = desp->detect_Threshold / s_detect_tone_count;
}

howling_state_t * howling_init(howling_state_t * state)
{

    LOGV("howling_init: s_detect_tone_count=%d, s_howling_threshold=%f",
         s_detect_tone_count,s_howling_threshold);

	howling_tone_t * tone = NULL;

	state->detect_tones = (howling_tone_t*)malloc(sizeof(howling_tone_t) * s_detect_tone_count);

	state->sample_index = 0;

	for (int i = 0; i < s_detect_tone_count; i++)
	{
		tone = &state->detect_tones[i];

		tone->lasts = 0;
		tone->notch_active = 0;

		tone->freq = s_howling_suppression_desp.detect_freq_min + s_howling_suppression_desp.detect_freq_interval * i;
		make_goertzel_descriptor(&tone->tone_descriptor, tone->freq, DETECT_SAMPLES_PER_BLOCK);
		goertzel_init(&tone->tone_stats, &tone->tone_descriptor);
	}

	return state;
}


static void _DetectOneFrame(howling_state_t * state)
{
	howling_tone_t * tone = NULL;

	for (int i = 0; i < s_detect_tone_count; i++)
	{
		tone = &state->detect_tones[i];

		tone->energy = goertzel_result(&tone->tone_stats);
		state->total_energy += tone->energy;
	}

	for (int i = 1; i < s_detect_tone_count; i++)
	{
		tone = &state->detect_tones[i];

		if (tone->energy / state->total_energy > s_howling_threshold)
		{
			if (tone->notch_active)
				tone->notch_active = s_howling_suppression_desp.notch_last_block;
			else
			{
				tone->lasts++;
				if (tone->lasts >= s_howling_suppression_desp.detect_block)
				{
					tone->lasts = 0;
					tone->notch_active = s_howling_suppression_desp.notch_last_block;
					sf_notch(&tone->notch_state, 16000, tone->freq / 2, s_howling_suppression_desp.notch_filter_Q);
                    LOGV("sf_notch start: freq[%d]", tone->freq);
				}
			}
		}
		else
		{
            if (tone->notch_active > 0) {
                tone->lasts = 0;
                tone->notch_active--;
                if (tone->notch_active <= 0) {
                    tone->notch_active = 0;
                    LOGV("sf_notch stop : freq[%d]", tone->freq);
                }
            }
		}
	}

	state->total_energy = 0.0f;
}

void _do_suppression(howling_state_t * state, short * samples, int len, short * out)
{
	int has_nocth = 0;
	howling_tone_t * tone = NULL;
	for (int i = 1; i < s_detect_tone_count; i++)
	{
		tone = &state->detect_tones[i];
		if (tone->notch_active)
		{
			has_nocth++;
			sf_biquad_process_short(&tone->notch_state, len, samples, out);
		}
	}

	if (!has_nocth)
	{
		for (int sample = 0; sample < len; sample++)
		{
			out[sample] = samples[sample];
		}
	}
}


int howling_suppression(howling_state_t * state, short * samples, int len, short * out)
{
#if defined(SPANDSP_USE_FIXED_POINT)
	int16_t xamp;
#else
	float xamp;
#endif

	//mylog("howling_suppression : %d ms", state->index*DTMF_SAMPLES_PER_BLOCK/16);

	for (int sample = 0; sample < len; sample++)
	{
		xamp = samples[sample];
		xamp = goertzel_preadjust_amp(xamp);

		for (int i = 0; i<s_detect_tone_count; i++)
			goertzel_samplex(&state->detect_tones[i].tone_stats, xamp);

		state->sample_index++;
		if (state->sample_index % DETECT_SAMPLES_PER_BLOCK == 0)
		{
			_DetectOneFrame(state);
		}
	}

	_do_suppression(state, samples, len, out);

	return 0;
}

void howling_free(howling_state_t * state)
{
	free(state->detect_tones);
}
