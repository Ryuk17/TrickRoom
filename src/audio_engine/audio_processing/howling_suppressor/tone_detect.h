#if !defined(_SPANDSP_TONE_DETECT_H_)
#define _SPANDSP_TONE_DETECT_H_

struct goertzel_descriptor_s
{
#if defined(SPANDSP_USE_FIXED_POINT)
	int16_t fac;
#else
	float fac;
#endif
	int samples;
};

struct goertzel_state_s
{
#if defined(SPANDSP_USE_FIXED_POINT)
	int16_t v2;
	int16_t v3;
	int16_t fac;
#else
	float v2;
	float v3;
	float fac;
#endif
	int samples;
	int current_sample;
};

typedef struct goertzel_descriptor_s goertzel_descriptor_t;
typedef struct goertzel_state_s goertzel_state_t;

void make_goertzel_descriptor(goertzel_descriptor_t *t,
	float freq,
	int samples);

goertzel_state_t * goertzel_init(goertzel_state_t *s,
	goertzel_descriptor_t *t);

void goertzel_reset(goertzel_state_t *s);

#if defined(SPANDSP_USE_FIXED_POINT)
int32_t goertzel_result(goertzel_state_t *s);
#else
float goertzel_result(goertzel_state_t *s);
#endif

#if defined(SPANDSP_USE_FIXED_POINT)
#define goertzel_preadjust_amp(amp) (((int16_t) amp) >> 7)
#else
#define goertzel_preadjust_amp(amp) ((float) amp)
#endif

#if defined(SPANDSP_USE_FIXED_POINT)
static inline void goertzel_samplex(goertzel_state_t *s, int16_t amp)
#else
static inline void goertzel_samplex(goertzel_state_t *s, float amp)
#endif
{
#if defined(SPANDSP_USE_FIXED_POINT)
	int16_t x;
	int16_t v1;
#else
	float v1;
#endif

	v1 = s->v2;
	s->v2 = s->v3;
#if defined(SPANDSP_USE_FIXED_POINT)
	x = (((int32_t)s->fac*s->v2) >> 14);
	s->v3 = x - v1 + amp;
#else
	s->v3 = s->fac*s->v2 - v1 + amp;
#endif
}







#endif

