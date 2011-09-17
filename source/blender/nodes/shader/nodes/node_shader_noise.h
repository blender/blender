/*
 * Adapted from Open Shading Language with this license:
 *
 * Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.
 * All Rights Reserved.
 *
 * Modifications Copyright 2011, Blender Foundation.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * * Neither the name of Sony Pictures Imageworks nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef NODE_SHADER_NOISE_H
#define NODE_SHADER_NOISE_H

MINLINE int quick_floor(float x)
{
	return (int)x - ((x < 0) ? 1 : 0);
}

MINLINE float bits_to_01(unsigned int bits)
{
	return bits * (1.0f/(float)0xFFFFFFFF);
}

MINLINE unsigned int hash(unsigned int kx, unsigned int ky, unsigned int kz)
{
	// define some handy macros
#define rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))
#define final(a,b,c) \
{ \
	c ^= b; c -= rot(b,14); \
	a ^= c; a -= rot(c,11); \
	b ^= a; b -= rot(a,25); \
	c ^= b; c -= rot(b,16); \
	a ^= c; a -= rot(c,4);  \
	b ^= a; b -= rot(a,14); \
	c ^= b; c -= rot(b,24); \
}
	// now hash the data!
	unsigned int a, b, c, len = 3;
	a = b = c = 0xdeadbeef + (len << 2) + 13;

	c += kz;
	b += ky;
	a += kx;
	final(a, b, c);

	return c;
	// macros not needed anymore
#undef rot
#undef final
}

MINLINE int imod(int a, int b)
{
	a %= b;
	return a < 0 ? a + b : a;
}

MINLINE unsigned int phash(int kx, int ky, int kz, int p[3]) 
{
	return hash(imod(kx, p[0]), imod(ky, p[1]), imod(kz, p[2]));
}

MINLINE float floorfrac(float x, int* i)
{
    *i = quick_floor(x);
    return x - *i;
}

MINLINE float fade(float t)
{
	return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

MINLINE float nerp(float t, float a, float b)
{
    return (1.0f - t) * a + t * b;
}

MINLINE float grad(int hash, float x, float y, float z)
{
	// use vectors pointing to the edges of the cube
	int h = hash & 15;
	float u = h<8 ? x : y;
	float v = h<4 ? y : h==12||h==14 ? x : z;
	return ((h&1) ? -u : u) + ((h&2) ? -v : v);
}

MINLINE float scale3(float result)
{
	return 0.9820f * result;
}

MINLINE float perlin(float x, float y, float z)
{
	int X; float fx = floorfrac(x, &X);
	int Y; float fy = floorfrac(y, &Y);
	int Z; float fz = floorfrac(z, &Z);

	float u = fade(fx);
	float v = fade(fy);
	float w = fade(fz);

	float result;

	result = nerp (w, nerp (v, nerp (u, grad (hash (X  , Y  , Z  ), fx	 , fy	 , fz	  ),
										grad (hash (X+1, Y  , Z  ), fx-1.0f, fy	 , fz	  )),
							   nerp (u, grad (hash (X  , Y+1, Z  ), fx	 , fy-1.0f, fz	  ),
										grad (hash (X+1, Y+1, Z  ), fx-1.0f, fy-1.0f, fz	  ))),
					  nerp (v, nerp (u, grad (hash (X  , Y  , Z+1), fx	 , fy	 , fz-1.0f ),
										grad (hash (X+1, Y  , Z+1), fx-1.0f, fy	 , fz-1.0f )),
							   nerp (u, grad (hash (X  , Y+1, Z+1), fx	 , fy-1.0f, fz-1.0f ),
										grad (hash (X+1, Y+1, Z+1), fx-1.0f, fy-1.0f, fz-1.0f ))));
	return scale3(result);
}

MINLINE float perlin_periodic(float x, float y, float z, float pperiod[3])
{
	int X; float fx = floorfrac(x, &X);
	int Y; float fy = floorfrac(y, &Y);
	int Z; float fz = floorfrac(z, &Z);

	int p[3] = {
		MAX2(quick_floor(pperiod[0]), 1),
		MAX2(quick_floor(pperiod[1]), 1),
		MAX2(quick_floor(pperiod[2]), 1)};

	float u = fade(fx);
	float v = fade(fy);
	float w = fade(fz);

	float result;

	result = nerp (w, nerp (v, nerp (u, grad (phash (X  , Y  , Z  , p), fx	 , fy	 , fz	  ),
										grad (phash (X+1, Y  , Z  , p), fx-1.0f, fy	 , fz	  )),
							   nerp (u, grad (phash (X  , Y+1, Z  , p), fx	 , fy-1.0f, fz	  ),
										grad (phash (X+1, Y+1, Z  , p), fx-1.0f, fy-1.0f, fz	  ))),
					  nerp (v, nerp (u, grad (phash (X  , Y  , Z+1, p), fx	 , fy	 , fz-1.0f ),
										grad (phash (X+1, Y  , Z+1, p), fx-1.0f, fy	 , fz-1.0f )),
							   nerp (u, grad (phash (X  , Y+1, Z+1, p), fx	 , fy-1.0f, fz-1.0f ),
										grad (phash (X+1, Y+1, Z+1, p), fx-1.0f, fy-1.0f, fz-1.0f ))));
	return scale3(result);
}

/* perlin noise in range 0..1 */
MINLINE float noise(float p[3])
{
	float r = perlin(p[0], p[1], p[2]);
	return 0.5f*r + 0.5f;
}

/* perlin noise in range -1..1 */
MINLINE float snoise(float p[3])
{
	return perlin(p[0], p[1], p[2]);
}

/* cell noise */
MINLINE float cellnoise(float p[3])
{
	unsigned int ix = quick_floor(p[0]);
	unsigned int iy = quick_floor(p[1]);
	unsigned int iz = quick_floor(p[2]);

	return bits_to_01(hash(ix, iy, iz));
}

MINLINE void cellnoise_color(float rgb[3], float p[3])
{
	float pg[3] = {p[1], p[0], p[2]};
	float pb[3] = {p[1], p[2], p[0]};

	float r = cellnoise(p);
	float g = cellnoise(pg);
	float b = cellnoise(pb);

	rgb[0]= r;
	rgb[1]= g;
	rgb[2]= b;
}

/* periodic perlin noise in range 0..1 */
MINLINE float pnoise(float p[3], float pperiod[3])
{
	float r = perlin_periodic(p[0], p[1], p[2], pperiod);
	return 0.5f*r + 0.5f;
}

/* periodic perlin noise in range -1..1 */
MINLINE float psnoise(float p[3], float pperiod[3])
{
	return perlin_periodic(p[0], p[1], p[2], pperiod);
}

/*
 * Copyright 2011, Blender Foundation.
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
 */

/* turbulence */
MINLINE float turbulence(float p[3], int oct, int hard)
{
	float amp = 1.0f, fscale = 1.0f, sum = 0.0f;
	int i;

	for(i=0; i<=oct; i++, amp *= 0.5f, fscale *= 2.0f) {
		float pscale[3] = {fscale*p[0], fscale*p[1], fscale*p[2]};
		float t = noise(pscale);
		if(hard) t = fabsf(2.0f*t - 1.0f);
		sum += t * amp;
	}

	sum *= ((float)(1<<oct)/(float)((1<<(oct+1))-1));

	return sum;
}

/* Voronoi Distances */

MINLINE float voronoi_distance(int distance_metric, float d[3], float e)
{
	if(distance_metric == SHD_VORONOI_DISTANCE_SQUARED)
		return dot_v3v3(d, d);
	if(distance_metric == SHD_VORONOI_ACTUAL_DISTANCE)
		return len_v3(d);
	if(distance_metric == SHD_VORONOI_MANHATTAN)
		return fabsf(d[0]) + fabsf(d[1]) + fabsf(d[2]);
	if(distance_metric == SHD_VORONOI_CHEBYCHEV)
		return MAX2(fabsf(d[0]), MAX2(fabsf(d[1]), fabsf(d[2])));
	if(distance_metric == SHD_VORONOI_MINKOVSKY_H)
		return sqrtf(fabsf(d[0])) + sqrtf(fabsf(d[1])) + sqrtf(fabsf(d[1]));
	if(distance_metric == SHD_VORONOI_MINKOVSKY_4) {
		float dsq[3] = {d[0]*d[0], d[1]*d[1], d[2]*d[2]};
		return sqrtf(sqrtf(dot_v3v3(dsq, dsq)));
	}
	if(distance_metric == SHD_VORONOI_MINKOVSKY)
		return powf(powf(fabsf(d[0]), e) + powf(fabsf(d[1]), e) + powf(fabsf(d[2]), e), 1.0f/e);
	
	return 0.0f;
}

/* Voronoi / Worley like */

MINLINE void voronoi_generic(float p[3], int distance_metric, float e, float da[4], float pa[4][3])
{
	/* returns distances in da and point coords in pa */
	int xx, yy, zz, xi, yi, zi;

	xi = (int)floorf(p[0]);
	yi = (int)floorf(p[1]);
	zi = (int)floorf(p[2]);

	da[0] = 1e10f;
	da[1] = 1e10f;
	da[2] = 1e10f;
	da[3] = 1e10f;

	zero_v3(pa[0]);
	zero_v3(pa[1]);
	zero_v3(pa[2]);
	zero_v3(pa[3]);

	for(xx = xi-1; xx <= xi+1; xx++) {
		for(yy = yi-1; yy <= yi+1; yy++) {
			for(zz = zi-1; zz <= zi+1; zz++) {
				float ip[3] = {(float)xx, (float)yy, (float)zz};
				float vp[3], pd[3], d;

				cellnoise_color(vp, ip);
				add_v3_v3v3(pd, vp, ip);
				sub_v3_v3v3(pd, p, pd);

				d = voronoi_distance(distance_metric, pd, e);

				add_v3_v3(vp, ip);

				if(d < da[0]) {
					da[3] = da[2];
					da[2] = da[1];
					da[1] = da[0];
					da[0] = d;

					copy_v3_v3(pa[3], pa[2]);
					copy_v3_v3(pa[2], pa[1]);
					copy_v3_v3(pa[1], pa[0]);
					copy_v3_v3(pa[0], vp);
				}
				else if(d < da[1]) {
					da[3] = da[2];
					da[2] = da[1];
					da[1] = d;

					copy_v3_v3(pa[3], pa[2]);
					copy_v3_v3(pa[2], pa[1]);
					copy_v3_v3(pa[1], vp);
				}
				else if(d < da[2]) {
					da[3] = da[2];
					da[2] = d;

					copy_v3_v3(pa[3], pa[2]);
					copy_v3_v3(pa[2], vp);
				}
				else if(d < da[3]) {
					da[3] = d;
					copy_v3_v3(pa[3], vp);
				}
			}
		}
	}
}

MINLINE float voronoi_Fn(float p[3], int n)
{
	float da[4];
	float pa[4][3];

	voronoi_generic(p, SHD_VORONOI_DISTANCE_SQUARED, 0, da, pa);

	return da[n];
}

MINLINE float voronoi_FnFn(float p[3], int n1, int n2)
{
	float da[4];
	float pa[4][3];

	voronoi_generic(p, SHD_VORONOI_DISTANCE_SQUARED, 0, da, pa);

	return da[n2] - da[n1];
}

MINLINE float voronoi_F1(float p[3]) { return voronoi_Fn(p, 0); }
MINLINE float voronoi_F2(float p[3]) { return voronoi_Fn(p, 1); }
MINLINE float voronoi_F3(float p[3]) { return voronoi_Fn(p, 2); }
MINLINE float voronoi_F4(float p[3]) { return voronoi_Fn(p, 3); }
MINLINE float voronoi_F1F2(float p[3]) { return voronoi_FnFn(p, 0, 1); }

MINLINE float voronoi_Cr(float p[3])
{
	/* crackle type pattern, just a scale/clamp of F2-F1 */
	float t = 10.0f*voronoi_F1F2(p);
	return (t > 1.0f)? 1.0f: t;
}

MINLINE float voronoi_F1S(float p[3]) { return 2.0f*voronoi_F1(p) - 1.0f; }
MINLINE float voronoi_F2S(float p[3]) { return 2.0f*voronoi_F2(p) - 1.0f; }
MINLINE float voronoi_F3S(float p[3]) { return 2.0f*voronoi_F3(p) - 1.0f; }
MINLINE float voronoi_F4S(float p[3]) { return 2.0f*voronoi_F4(p) - 1.0f; }
MINLINE float voronoi_F1F2S(float p[3]) { return 2.0f*voronoi_F1F2(p) - 1.0f; }
MINLINE float voronoi_CrS(float p[3]) { return 2.0f*voronoi_Cr(p) - 1.0f; }

/* Noise Bases */

MINLINE float noise_basis(float p[3], int basis)
{
	if(basis == SHD_NOISE_PERLIN)
		return noise(p);
	if(basis == SHD_NOISE_VORONOI_F1)
		return voronoi_F1S(p);
	if(basis == SHD_NOISE_VORONOI_F2)
		return voronoi_F2S(p);
	if(basis == SHD_NOISE_VORONOI_F3)
		return voronoi_F3S(p);
	if(basis == SHD_NOISE_VORONOI_F4)
		return voronoi_F4S(p);
	if(basis == SHD_NOISE_VORONOI_F2_F1)
		return voronoi_F1F2S(p);
	if(basis == SHD_NOISE_VORONOI_CRACKLE)
		return voronoi_CrS(p);
	if(basis == SHD_NOISE_CELL_NOISE)
		return cellnoise(p);
	
	return 0.0f;
}

/* Soft/Hard Noise */

MINLINE float noise_basis_hard(float p[3], int basis, int hard)
{
	float t = noise_basis(p, basis);
	return (hard)? fabsf(2.0f*t - 1.0f): t;
}

/* Waves */

MINLINE float noise_wave(int wave, float a)
{
	if(wave == SHD_WAVE_SINE) {
    	return 0.5f + 0.5f*sin(a);
	}
	else if(wave == SHD_WAVE_SAW) {
		float b = 2*M_PI;
		int n = (int)(a / b);
		a -= n*b;
		if(a < 0) a += b;

		return a / b;
	}
	else if(wave == SHD_WAVE_TRI) {
		float b = 2*M_PI;
		float rmax = 1.0f;

		return rmax - 2.0f*fabsf(floorf((a*(1.0f/b))+0.5f) - (a*(1.0f/b)));
	}

	return 0.0f;
}

/* Turbulence */

MINLINE float noise_turbulence(float p[3], int basis, int octaves, int hard)
{
	float fscale = 1.0f;
	float amp = 1.0f;
	float sum = 0.0f;
	int i;

	for(i = 0; i <= octaves; i++) {
		float pscale[3] = {fscale*p[0], fscale*p[1], fscale*p[2]};
		float t = noise_basis(pscale, basis);

		if(hard)
			t = fabsf(2.0f*t - 1.0f);

		sum += t*amp;
		amp *= 0.5f;
		fscale *= 2.0f;
	}

	sum *= ((float)(1 << octaves)/(float)((1 << (octaves+1)) - 1));

	return sum;
}

#endif /* NODE_SHADER_NOISE_H */

