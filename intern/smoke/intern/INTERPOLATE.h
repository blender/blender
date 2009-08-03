//////////////////////////////////////////////////////////////////////
// This file is part of Wavelet Turbulence.
// 
// Wavelet Turbulence is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// Wavelet Turbulence is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with Wavelet Turbulence.  If not, see <http://www.gnu.org/licenses/>.
// 
// Copyright 2008 Theodore Kim and Nils Thuerey
// 
//////////////////////////////////////////////////////////////////////
#ifndef INTERPOLATE_H
#define INTERPOLATE_H

#include <iostream>
#include <VEC3.h>

namespace INTERPOLATE {

//////////////////////////////////////////////////////////////////////
// linear interpolators
//////////////////////////////////////////////////////////////////////
static inline float lerp(float t, float a, float b) {
	return ( a + t * (b - a) );
}

static inline float lerp(float* field, float x, float y, int res) {
	// clamp backtrace to grid boundaries
	if (x < 0.5f) x = 0.5f;
	if (x > res - 1.5f) x = res - 1.5f;
	if (y < 0.5f) y = 0.5f;
	if (y > res - 1.5f) y = res - 1.5f;

	const int x0 = (int)x;
	const int y0 = (int)y;
	x -= x0;
	y -= y0;
	float d00, d10, d01, d11;

	// lerp the velocities
	d00 = field[x0 + y0 * res];
	d10 = field[(x0 + 1) + y0 * res];
	d01 = field[x0 + (y0 + 1) * res];
	d11 = field[(x0 + 1) + (y0 + 1) * res];
	return lerp(y, lerp(x, d00, d10),
			lerp(x, d01, d11));
}

//////////////////////////////////////////////////////////////////////////////////////////
// 3d linear interpolation
////////////////////////////////////////////////////////////////////////////////////////// 
static inline float lerp3d(float* field, float x, float y, float z,  int xres, int yres, int zres) {
	// clamp pos to grid boundaries
	if (x < 0.5) x = 0.5;
	if (x > xres - 1.5) x = xres - 1.5;
	if (y < 0.5) y = 0.5;
	if (y > yres - 1.5) y = yres - 1.5;
	if (z < 0.5) z = 0.5;
	if (z > zres - 1.5) z = zres - 1.5;

	// locate neighbors to interpolate
	const int x0 = (int)x;
	const int x1 = x0 + 1;
	const int y0 = (int)y;
	const int y1 = y0 + 1;
	const int z0 = (int)z;
	const int z1 = z0 + 1;

	// get interpolation weights
	const float s1 = x - (float)x0;
	const float s0 = 1.0f - s1;
	const float t1 = y - (float)y0;
	const float t0 = 1.0f - t1;
	const float u1 = z - (float)z0;
	const float u0 = 1.0f - u1;

	const int slabSize = xres*yres;
	const int i000 = x0 + y0 * xres + z0 * slabSize;
	const int i010 = x0 + y1 * xres + z0 * slabSize;
	const int i100 = x1 + y0 * xres + z0 * slabSize;
	const int i110 = x1 + y1 * xres + z0 * slabSize;
	const int i001 = x0 + y0 * xres + z1 * slabSize;
	const int i011 = x0 + y1 * xres + z1 * slabSize;
	const int i101 = x1 + y0 * xres + z1 * slabSize;
	const int i111 = x1 + y1 * xres + z1 * slabSize;

	// interpolate (indices could be computed once)
	return ( u0 * (s0 * (t0 * field[i000] +
		t1 * field[i010]) +
		s1 * (t0 * field[i100] +
		t1 * field[i110])) +
		u1 * (s0 * (t0 * field[i001] +
		t1 * field[i011]) +
		s1 * (t0 * field[i101] +
		t1 * field[i111])) );
}

//////////////////////////////////////////////////////////////////////////////////////////
// convert field entries of type T to floats, then interpolate
//////////////////////////////////////////////////////////////////////////////////////////
template <class T> 
static inline float lerp3dToFloat(T* field1,
		float x, float y, float z,  int xres, int yres, int zres) {
	// clamp pos to grid boundaries
	if (x < 0.5) x = 0.5;
	if (x > xres - 1.5) x = xres - 1.5;
	if (y < 0.5) y = 0.5;
	if (y > yres - 1.5) y = yres - 1.5;
	if (z < 0.5) z = 0.5;
	if (z > zres - 1.5) z = zres - 1.5;

	// locate neighbors to interpolate
	const int x0 = (int)x;
	const int x1 = x0 + 1;
	const int y0 = (int)y;
	const int y1 = y0 + 1;
	const int z0 = (int)z;
	const int z1 = z0 + 1;

	// get interpolation weights
	const float s1 = x - (float)x0;
	const float s0 = 1.0f - s1;
	const float t1 = y - (float)y0;
	const float t0 = 1.0f - t1;
	const float u1 = z - (float)z0;
	const float u0 = 1.0f - u1;

	const int slabSize = xres*yres;
	const int i000 = x0 + y0 * xres + z0 * slabSize;
	const int i010 = x0 + y1 * xres + z0 * slabSize;
	const int i100 = x1 + y0 * xres + z0 * slabSize;
	const int i110 = x1 + y1 * xres + z0 * slabSize;
	const int i001 = x0 + y0 * xres + z1 * slabSize;
	const int i011 = x0 + y1 * xres + z1 * slabSize;
	const int i101 = x1 + y0 * xres + z1 * slabSize;
	const int i111 = x1 + y1 * xres + z1 * slabSize;

	// interpolate (indices could be computed once)
	return (float)(
			( u0 * (s0 * (t0 * (float)field1[i000] +
				t1 * (float)field1[i010]) +
				s1 * (t0 * (float)field1[i100] +
				t1 * (float)field1[i110])) +
				u1 * (s0 * (t0 * (float)field1[i001] +
				t1 * (float)field1[i011]) +
				s1 * (t0 * (float)field1[i101] +
				t1 * (float)field1[i111])) ) );
}

//////////////////////////////////////////////////////////////////////////////////////////
// interpolate a vector from 3 fields
//////////////////////////////////////////////////////////////////////////////////////////
static inline Vec3 lerp3dVec(float* field1, float* field2, float* field3, 
		float x, float y, float z,  int xres, int yres, int zres) {
	// clamp pos to grid boundaries
	if (x < 0.5) x = 0.5;
	if (x > xres - 1.5) x = xres - 1.5;
	if (y < 0.5) y = 0.5;
	if (y > yres - 1.5) y = yres - 1.5;
	if (z < 0.5) z = 0.5;
	if (z > zres - 1.5) z = zres - 1.5;

	// locate neighbors to interpolate
	const int x0 = (int)x;
	const int x1 = x0 + 1;
	const int y0 = (int)y;
	const int y1 = y0 + 1;
	const int z0 = (int)z;
	const int z1 = z0 + 1;

	// get interpolation weights
	const float s1 = x - (float)x0;
	const float s0 = 1.0f - s1;
	const float t1 = y - (float)y0;
	const float t0 = 1.0f - t1;
	const float u1 = z - (float)z0;
	const float u0 = 1.0f - u1;

	const int slabSize = xres*yres;
	const int i000 = x0 + y0 * xres + z0 * slabSize;
	const int i010 = x0 + y1 * xres + z0 * slabSize;
	const int i100 = x1 + y0 * xres + z0 * slabSize;
	const int i110 = x1 + y1 * xres + z0 * slabSize;
	const int i001 = x0 + y0 * xres + z1 * slabSize;
	const int i011 = x0 + y1 * xres + z1 * slabSize;
	const int i101 = x1 + y0 * xres + z1 * slabSize;
	const int i111 = x1 + y1 * xres + z1 * slabSize;

	// interpolate (indices could be computed once)
	return Vec3(
			( u0 * (s0 * (t0 * field1[i000] +
				t1 * field1[i010]) +
				s1 * (t0 * field1[i100] +
				t1 * field1[i110])) +
				u1 * (s0 * (t0 * field1[i001] +
				t1 * field1[i011]) +
				s1 * (t0 * field1[i101] +
				t1 * field1[i111])) ) , 
			( u0 * (s0 * (t0 * field2[i000] +
				t1 * field2[i010]) +
				s1 * (t0 * field2[i100] +
				t1 * field2[i110])) +
				u1 * (s0 * (t0 * field2[i001] +
				t1 * field2[i011]) +
				s1 * (t0 * field2[i101] +
				t1 * field2[i111])) ) , 
			( u0 * (s0 * (t0 * field3[i000] +
				t1 * field3[i010]) +
				s1 * (t0 * field3[i100] +
				t1 * field3[i110])) +
				u1 * (s0 * (t0 * field3[i001] +
				t1 * field3[i011]) +
				s1 * (t0 * field3[i101] +
				t1 * field3[i111])) ) 
			);
}

};
#endif
