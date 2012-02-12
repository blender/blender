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

CCL_NAMESPACE_BEGIN

__device int quick_floor(float x)
{
	return (int)x - ((x < 0) ? 1 : 0);
}

__device float bits_to_01(uint bits)
{
	return bits * (1.0f/(float)0xFFFFFFFF);
}

__device uint hash(uint kx, uint ky, uint kz)
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
	uint a, b, c, len = 3;
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

__device int imod(int a, int b)
{
	a %= b;
	return a < 0 ? a + b : a;
}

__device uint phash(int kx, int ky, int kz, int3 p) 
{
	return hash(imod(kx, p.x), imod(ky, p.y), imod(kz, p.z));
}

__device float floorfrac(float x, int* i)
{
    float f = floorf(x);
    *i = (int)f;
    return x - f;
}

__device float fade(float t)
{
	return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

__device float nerp(float t, float a, float b)
{
    return (1.0f - t) * a + t * b;
}

__device float grad(int hash, float x, float y, float z)
{
	// use vectors pointing to the edges of the cube
	int h = hash & 15;
	float u = h<8 ? x : y;
	float v = h<4 ? y : h==12||h==14 ? x : z;
	return ((h&1) ? -u : u) + ((h&2) ? -v : v);
}

__device float scale3(float result)
{
	return 0.9820f * result;
}

__device_noinline float perlin(float x, float y, float z)
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

__device_noinline float perlin_periodic(float x, float y, float z, float3 pperiod)
{
	int X; float fx = floorfrac(x, &X);
	int Y; float fy = floorfrac(y, &Y);
	int Z; float fz = floorfrac(z, &Z);

	int3 p;

	p.x = max(quick_floor(pperiod.x), 1);
	p.y = max(quick_floor(pperiod.y), 1);
	p.z = max(quick_floor(pperiod.z), 1);

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
__device float noise(float3 p)
{
	float r = perlin(p.x, p.y, p.z);
	return 0.5f*r + 0.5f;
}

/* perlin noise in range -1..1 */
__device float snoise(float3 p)
{
	return perlin(p.x, p.y, p.z);
}

/* cell noise */
__device_noinline float cellnoise(float3 p)
{
	uint ix = quick_floor(p.x);
	uint iy = quick_floor(p.y);
	uint iz = quick_floor(p.z);

	return bits_to_01(hash(ix, iy, iz));
}

__device float3 cellnoise_color(float3 p)
{
	float r = cellnoise(p);
	float g = cellnoise(make_float3(p.y, p.x, p.z));
	float b = cellnoise(make_float3(p.y, p.z, p.x));

	return make_float3(r, g, b);
}

/* periodic perlin noise in range 0..1 */
__device float pnoise(float3 p, float3 pperiod)
{
	float r = perlin_periodic(p.x, p.y, p.z, pperiod);
	return 0.5f*r + 0.5f;
}

/* periodic perlin noise in range -1..1 */
__device float psnoise(float3 p, float3 pperiod)
{
	return perlin_periodic(p.x, p.y, p.z, pperiod);
}

CCL_NAMESPACE_END

