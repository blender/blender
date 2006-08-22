/*
 * Utility functions.
*/

#include <stdio.h>

#include "verse_header.h"
#include "v_network.h"
#include "v_util.h"

/* Safe string copy. Copies from <src> to <dst>, not using more than <size>
 * bytes of destination space. Always 0-terminates the destination. Returns
 * the beginning of the destination string.
*/
char * v_strlcpy(char *dst, const char *src, size_t size)
{
	char	*base = dst;

	if(size == 0)
		return NULL;
	for(size--; size > 0 && *src != '\0'; size--)
		*dst++ = *src++;
	*dst = '\0';

	return base;
}

void v_timer_start(VUtilTimer *timer)
{
	v_n_get_current_time(&timer->seconds, &timer->fractions);
}

void v_timer_advance(VUtilTimer *timer, double seconds)
{
	if(timer == NULL)
		return;
	timer->seconds   += (uint32) seconds;
	timer->fractions += (uint32) ((seconds - (int) seconds) * (double) 0xffffffff);
}

double v_timer_elapsed(const VUtilTimer *timer)
{
	uint32 cur_seconds, cur_fractions;

	v_n_get_current_time(&cur_seconds, &cur_fractions);
	return (double)(cur_seconds - timer->seconds) + ((double)cur_fractions - (double)timer->fractions) / (double) 0xffffffff;
}

void v_timer_print(const VUtilTimer *timer)
{
	uint32 cur_seconds, cur_fractions;

	v_n_get_current_time(&cur_seconds, &cur_fractions);
	printf("%f", (double)(cur_seconds - timer->seconds) + ((double)cur_fractions - (double)timer->fractions) / (double) 0xffffffff);
}

/* Compare |x| against built-in semi-magical constant, and return 1 if it's larger, 0 if not. */
static int quat_valid(real64 x)
{
	const real64	EPSILON = 0.0000001;
	return x > 0.0 ? x > EPSILON : x < -EPSILON;
}

int v_quat32_valid(const VNQuat32 *q)
{
	if(q == NULL)
		return 0;
	return quat_valid(q->x) && quat_valid(q->y) && quat_valid(q->z) && quat_valid(q->w);
}

int v_quat64_valid(const VNQuat64 *q)
{
	if(q == NULL)
		return 0;
	return quat_valid(q->x) && quat_valid(q->y) && quat_valid(q->z) && quat_valid(q->w);
}

VNQuat32 * v_quat32_from_quat64(VNQuat32 *dst, const VNQuat64 *src)
{
	if(dst == NULL || src == NULL)
		return NULL;
	dst->x = (real32) src->x;
	dst->y = (real32) src->y;
	dst->z = (real32) src->z;	
	dst->w = (real32) src->w;
	return dst;
}

VNQuat64 * v_quat64_from_quat32(VNQuat64 *dst, const VNQuat32 *src)
{
	if(dst == NULL || src == NULL)
		return NULL;
	dst->x = src->x;	
	dst->y = src->y;
	dst->z = src->z;	
	dst->w = src->w;
	return dst;
}
