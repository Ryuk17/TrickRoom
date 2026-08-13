#if !defined(_HOWLING_CORE_H_)
#define _HOWLING_CORE_H_

#include "tone_detect.h"
#include "biquad.h"



#define DETECT_SAMPLES_PER_BLOCK      205

/*
#define DETECT_TONE_COUNT 100
#define DETECT_TONE_START 850
#define DETECT_TONE_END 3000.0
#define HOWLING_THRESHOLD (10.0/(float)DETECT_TONE_COUNT)
#define HOWLING_DETECT_COUNT 6
#define MAX_NOCTH_ACTIVE 30
#define BANDWIDTH_Q 0.7
*/

typedef struct howling_suppression_desp
{
	float detect_Threshold;
	int detect_block;
	float detect_freq_min;
	float detect_freq_max;
	float detect_freq_interval;	
	int notch_last_block;
	float notch_filter_Q;
} howling_suppression_desp_t;

typedef struct howling_tone
{
	float freq;
	goertzel_descriptor_t tone_descriptor;
	goertzel_state_t tone_stats;

#if defined(SPANDSP_USE_FIXED_POINT)
	int32_t energy;
#else
	float energy;
#endif



	int lasts;
	sf_biquad_state_st notch_state;
	int notch_active;
} howling_tone_t;

typedef struct howling_state_s
{
	howling_tone_t * detect_tones;
	int detect_tone_count;

	int sample_index;

	float total_energy;
}howling_state_t;

#if defined(__cplusplus)
extern "C"
{
#endif

void howling_open(const howling_suppression_desp_t * desp);
howling_state_t * howling_init(howling_state_t * state);
int howling_suppression(howling_state_t * state, short * sample, int len, short * output);
void howling_free(howling_state_t * state);

#if defined(__cplusplus)
}
#endif



#endif

