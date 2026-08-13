#include "tone_detect.h"


#include <math.h>
#include <stdlib.h>
#include <stdio.h>


#define SAMPLE_RATE             16000


#if !defined(M_PI)
/* C99 systems may not define M_PI */
#define M_PI 3.14159265358979323846264338327
#endif

void make_goertzel_descriptor(goertzel_descriptor_t *t, float freq, int samples)
{
#if defined(SPANDSP_USE_FIXED_POINT)
	t->fac = 16383.0f*2.0f*cosf(2.0f*M_PI*(freq / (float)SAMPLE_RATE));
#else
	t->fac = 2.0f*cosf(2.0f*M_PI*(freq / (float)SAMPLE_RATE));
#endif
	t->samples = samples;
}

goertzel_state_t * goertzel_init(goertzel_state_t *s,
	goertzel_descriptor_t *t)
{
	if (s == NULL)
	{
		if ((s = (goertzel_state_t *)malloc(sizeof(*s))) == NULL)
			return NULL;
	}
#if defined(SPANDSP_USE_FIXED_POINT)
	s->v2 =
		s->v3 = 0;
#else
	s->v2 =
		s->v3 = 0.0f;
#endif
	s->fac = t->fac;
	s->samples = t->samples;
	s->current_sample = 0;
	return s;
}

void goertzel_reset(goertzel_state_t *s)
{
#if defined(SPANDSP_USE_FIXED_POINT)
	s->v2 =
		s->v3 = 0;
#else
	s->v2 =
		s->v3 = 0.0f;
#endif
	s->current_sample = 0;
}

#if defined(SPANDSP_USE_FIXED_POINT)
int32_t goertzel_result(goertzel_state_t *s)
#else
float goertzel_result(goertzel_state_t *s)
#endif
{
#if defined(SPANDSP_USE_FIXED_POINT)
	int16_t v1;
	int32_t x;
	int32_t y;
#else
	float v1;
#endif

	/* Push a zero through the process to finish things off. */
	v1 = s->v2;
	s->v2 = s->v3;
#if defined(SPANDSP_USE_FIXED_POINT)
	x = (((int32_t)s->fac*s->v2) >> 14);
	s->v3 = x - v1;
#else
	s->v3 = s->fac*s->v2 - v1;
#endif
	/* Now calculate the non-recursive side of the filter. */
	/* The result here is not scaled down to allow for the magnification
	effect of the filter (the usual DFT magnification effect). */
#if defined(SPANDSP_USE_FIXED_POINT)
	x = (int32_t)s->v3*s->v3;
	y = (int32_t)s->v2*s->v2;
	x += y;
	y = ((int32_t)s->v3*s->fac) >> 14;
	y *= s->v2;
	x -= y;
	x <<= 1;
	goertzel_reset(s);
	/* The number returned in a floating point build will be 16384^2 times
	as big as for a fixed point build, due to the 14 bit shifts
	(or the square of the 7 bit shifts, depending how you look at it). */
	return x;
#else
	v1 = s->v3*s->v3 + s->v2*s->v2 - s->v2*s->v3*s->fac;
	v1 *= 2.0;
	goertzel_reset(s);
	return v1;
#endif
}

