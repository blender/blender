/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/freestyle/intern/geometry/Noise.cpp
 *  \ingroup freestyle
 *  \brief Class to define Perlin noise
 *  \author Emmanuel Turquin
 *  \date 12/01/2004
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "Noise.h"

namespace Freestyle {

#define SCURVE(a) ((a) * (a) * (3.0 - 2.0 * (a)))

#if 0  // XXX Unused
#define REALSCALE (2.0 / 65536.0)
#define NREALSCALE (2.0 / 4096.0)

#define HASH3D(a, b, c) hashTable[hashTable[hashTable[(a) & 0xfff] ^ ((b) & 0xfff)] ^ ((c) & 0xfff)]
#define HASH(a, b, c) (xtab[(xtab[(xtab[(a) & 0xff] ^ (b)) & 0xff] ^ (c)) & 0xff] & 0xff)
#define INCRSUM(m, s, x, y, z) \
	((s) * (RTable[m] * 0.5 + RTable[m + 1] * (x) + RTable[m + 2] * (y) + RTable[m + 3] * (z)))

#define MAXSIZE 500
#define NRAND() ((float)rand() / (float)RAND_MAX)
#endif
#define SEEDNRAND(x) (srand(x * RAND_MAX))

#define BM 0xff
#define N  0x1000
#if 0  // XXX Unused
#define NP 12  /* 2^N */
#define NM 0xfff
#endif

#define LERP(t, a, b) ((a) + (t) * ((b) - (a)))

#define SETUP(i, b0, b1, r0, r1) \
	{                            \
		(t) = (i) + (N);         \
		(r0) = modff((t), &(u));  \
		(r1) = (r0) - 1.0;       \
		(b0) = ((int)(u)) & BM;  \
		(b1) = ((b0) + 1) & BM;  \
	} (void)0

static void normalize2(float v[2])
{
	float s;

	s = sqrt(v[0] * v[0] + v[1] * v[1]);
	v[0] = v[0] / s;
	v[1] = v[1] / s;
}

static void normalize3(float v[3])
{
	float s;

	s = sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
	v[0] = v[0] / s;
	v[1] = v[1] / s;
	v[2] = v[2] / s;
}

float Noise::turbulence1(float arg, float freq, float amp, unsigned oct)
{
	float t;
	float vec;

	for (t = 0; oct > 0 && freq > 0; freq *= 2, amp /= 2, --oct) {
		vec = freq * arg;
		t += smoothNoise1(vec) * amp;
	}
	return t;
}

float Noise::turbulence2(Vec2f& v, float freq, float amp, unsigned oct)
{
	float t;
	Vec2f vec;

	for (t = 0; oct > 0 && freq > 0; freq *= 2, amp /= 2, --oct) {
		vec.x() = freq * v.x();
		vec.y() = freq * v.y();
		t += smoothNoise2(vec) * amp;
	}
	return t;
}

float Noise::turbulence3(Vec3f& v, float freq, float amp, unsigned oct)
{
	float t;
	Vec3f vec;

	for (t = 0; oct > 0 && freq > 0; freq *= 2, amp /= 2, --oct) {
		vec.x() = freq * v.x();
		vec.y() = freq * v.y();
		vec.z() = freq * v.z();
		t += smoothNoise3(vec) * amp;
	}
	return t;
}

// Noise functions over 1, 2, and 3 dimensions
float Noise::smoothNoise1(float arg)
{
	int bx0, bx1;
	float rx0, rx1, sx, t, u, v, vec;

	vec = arg;
	SETUP(vec, bx0, bx1, rx0, rx1);

	sx = SCURVE(rx0);

	u = rx0 * g1[p[bx0]];
	v = rx1 * g1[p[bx1]];

	return LERP(sx, u, v);
}

float Noise::smoothNoise2(Vec2f& vec)
{
	int bx0, bx1, by0, by1, b00, b10, b01, b11;
	float rx0, rx1, ry0, ry1, *q, sx, sy, a, b, t, u, v;
	register int i, j;

	SETUP(vec.x(), bx0, bx1, rx0, rx1);
	SETUP(vec.y(), by0, by1, ry0, ry1);

	i = p[bx0];
	j = p[bx1];

	b00 = p[i + by0];
	b10 = p[j + by0];
	b01 = p[i + by1];
	b11 = p[j + by1];

	sx = SCURVE(rx0);
	sy = SCURVE(ry0);

#define AT2(rx, ry) ((rx) * q[0] + (ry) * q[1])

	q = g2[b00];
	u = AT2(rx0, ry0);
	q = g2[b10];
	v = AT2(rx1, ry0);
	a = LERP(sx, u, v);

	q = g2[b01];
	u = AT2(rx0, ry1);
	q = g2[b11];
	v = AT2(rx1, ry1);
	b = LERP(sx, u, v);

#undef AT2

	return LERP(sy, a, b);
}

float Noise::smoothNoise3(Vec3f& vec)
{
	int bx0, bx1, by0, by1, bz0, bz1, b00, b10, b01, b11;
	float rx0, rx1, ry0, ry1, rz0, rz1, *q, sy, sz, a, b, c, d, t, u, v;
	register int i, j;

	SETUP(vec.x(), bx0, bx1, rx0, rx1);
	SETUP(vec.y(), by0, by1, ry0, ry1);
	SETUP(vec.z(), bz0, bz1, rz0, rz1);

	i = p[bx0];
	j = p[bx1];

	b00 = p[i + by0];
	b10 = p[j + by0];
	b01 = p[i + by1];
	b11 = p[j + by1];

	t  = SCURVE(rx0);
	sy = SCURVE(ry0);
	sz = SCURVE(rz0);

#define AT3(rx, ry, rz) ((rx) * q[0] + (ry) * q[1] + (rz) * q[2])

	q = g3[b00 + bz0];
	u = AT3(rx0, ry0, rz0);
	q = g3[b10 + bz0];
	v = AT3(rx1, ry0, rz0);
	a = LERP(t, u, v);

	q = g3[b01 + bz0];
	u = AT3(rx0, ry1, rz0);
	q = g3[b11 + bz0];
	v = AT3(rx1, ry1, rz0);
	b = LERP(t, u, v);

	c = LERP(sy, a, b);

	q = g3[b00 + bz1];
	u = AT3(rx0, ry0, rz1);
	q = g3[b10 + bz1];
	v = AT3(rx1, ry0, rz1);
	a = LERP(t, u, v);

	q = g3[b01 + bz1];
	u = AT3(rx0, ry1, rz1);
	q = g3[b11 + bz1];
	v = AT3(rx1, ry1, rz1);
	b = LERP(t, u, v);

	d = LERP(sy, a, b);

#undef AT3

	return LERP(sz, c, d);
}

Noise::Noise(long seed)
{
	int i, j, k;

	SEEDNRAND((seed < 0) ? time(NULL) : seed);
	for (i = 0 ; i < _NOISE_B ; i++) {
		p[i] = i;
		g1[i] = (float)((rand() % (_NOISE_B + _NOISE_B)) - _NOISE_B) / _NOISE_B;

		for (j = 0 ; j < 2 ; j++)
			g2[i][j] = (float)((rand() % (_NOISE_B + _NOISE_B)) - _NOISE_B) / _NOISE_B;
		normalize2(g2[i]);

		for (j = 0 ; j < 3 ; j++)
			g3[i][j] = (float)((rand() % (_NOISE_B + _NOISE_B)) - _NOISE_B) / _NOISE_B;
		normalize3(g3[i]);
	}

	while (--i) {
		k = p[i];
		p[i] = p[j = rand() % _NOISE_B];
		p[j] = k;
	}

	for (i = 0 ; i < _NOISE_B + 2 ; i++) {
		p[_NOISE_B + i] = p[i];
		g1[_NOISE_B + i] = g1[i];

		for (j = 0 ; j < 2 ; j++)
			g2[_NOISE_B + i][j] = g2[i][j];

		for (j = 0 ; j < 3 ; j++)
			g3[_NOISE_B + i][j] = g3[i][j];
	}
}

} /* namespace Freestyle */
