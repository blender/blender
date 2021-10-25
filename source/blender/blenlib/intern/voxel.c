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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Matt Ebb, Raul Fernandez Hernandez (Farsthary).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenlib/intern/voxel.c
 *  \ingroup bli
 */

#include "BLI_utildefines.h"
#include "BLI_voxel.h"

#include "BLI_strict_flags.h"


BLI_INLINE float D(float *data, const int res[3], int x, int y, int z)
{
	CLAMP(x, 0, res[0] - 1);
	CLAMP(y, 0, res[1] - 1);
	CLAMP(z, 0, res[2] - 1);
	return data[BLI_VOXEL_INDEX(x, y, z, res)];
}

/* *** nearest neighbor *** */
/* input coordinates must be in bounding box 0.0 - 1.0 */
float BLI_voxel_sample_nearest(float *data, const int res[3], const float co[3])
{
	int xi, yi, zi;
	
	xi = (int)(co[0] * (float)res[0]);
	yi = (int)(co[1] * (float)res[1]);
	zi = (int)(co[2] * (float)res[2]);
	
	return D(data, res, xi, yi, zi);
}

/* returns highest integer <= x as integer (slightly faster than floor()) */
BLI_INLINE int FLOORI(float x)
{
	const int r = (int)x;
	return ((x >= 0.f) || (float)r == x) ? r : (r - 1);
}

/* clamp function, cannot use the CLAMPIS macro, it sometimes returns unwanted results apparently related to
 * gcc optimization flag -fstrict-overflow which is enabled at -O2
 *
 * this causes the test (x + 2) < 0 with int x == 2147483647 to return false (x being an integer,
 * x + 2 should wrap around to -2147483647 so the test < 0 should return true, which it doesn't) */
BLI_INLINE int64_t _clamp(int a, int b, int c)
{
	return (a < b) ? b : ((a > c) ? c : a);
}

float BLI_voxel_sample_trilinear(float *data, const int res[3], const float co[3])
{
	if (data) {
	
		const float xf = co[0] * (float)res[0] - 0.5f;
		const float yf = co[1] * (float)res[1] - 0.5f;
		const float zf = co[2] * (float)res[2] - 0.5f;
		
		const int x = FLOORI(xf), y = FLOORI(yf), z = FLOORI(zf);
	
		const int64_t xc[2] = {
		    _clamp(x,     0, res[0] - 1),
		    _clamp(x + 1, 0, res[0] - 1),
		};
		const int64_t yc[2] = {
		    _clamp(y,     0, res[1] - 1) * res[0],
		    _clamp(y + 1, 0, res[1] - 1) * res[0],
		};
		const int64_t zc[2] = {
		    _clamp(z,     0, res[2] - 1) * res[0] * res[1],
		    _clamp(z + 1, 0, res[2] - 1) * res[0] * res[1],
		};
	
		const float dx = xf - (float)x;
		const float dy = yf - (float)y;
		const float dz = zf - (float)z;
		
		const float u[2] = {1.f - dx, dx};
		const float v[2] = {1.f - dy, dy};
		const float w[2] = {1.f - dz, dz};
	
		return w[0] * (   v[0] * ( u[0] * data[xc[0] + yc[0] + zc[0]] + u[1] * data[xc[1] + yc[0] + zc[0]] )
		                + v[1] * ( u[0] * data[xc[0] + yc[1] + zc[0]] + u[1] * data[xc[1] + yc[1] + zc[0]] ) )
		     + w[1] * (   v[0] * ( u[0] * data[xc[0] + yc[0] + zc[1]] + u[1] * data[xc[1] + yc[0] + zc[1]] )
		                + v[1] * ( u[0] * data[xc[0] + yc[1] + zc[1]] + u[1] * data[xc[1] + yc[1] + zc[1]] ) );
	
	}
	return 0.f;
}

float BLI_voxel_sample_triquadratic(float *data, const int res[3], const float co[3])
{
	if (data) {

		const float xf = co[0] * (float)res[0];
		const float yf = co[1] * (float)res[1];
		const float zf = co[2] * (float)res[2];
		const int x = FLOORI(xf), y = FLOORI(yf), z = FLOORI(zf);

		const int64_t xc[3] = {
		    _clamp(x - 1, 0, res[0] - 1),
		    _clamp(x,     0, res[0] - 1),
		    _clamp(x + 1, 0, res[0] - 1),
		};
		const int64_t yc[3] = {
		    _clamp(y - 1, 0, res[1] - 1) * res[0],
		    _clamp(y,     0, res[1] - 1) * res[0],
		    _clamp(y + 1, 0, res[1] - 1) * res[0],
		};
		const int64_t zc[3] = {
		    _clamp(z - 1, 0, res[2] - 1) * res[0] * res[1],
		    _clamp(z,     0, res[2] - 1) * res[0] * res[1],
		    _clamp(z + 1, 0, res[2] - 1) * res[0] * res[1],
		};

		const float dx = xf - (float)x, dy = yf - (float)y, dz = zf - (float)z;
		const float u[3] = {dx * (0.5f * dx - 1.f) + 0.5f, dx * (1.0f - dx) + 0.5f, 0.5f * dx * dx};
		const float v[3] = {dy * (0.5f * dy - 1.f) + 0.5f, dy * (1.0f - dy) + 0.5f, 0.5f * dy * dy};
		const float w[3] = {dz * (0.5f * dz - 1.f) + 0.5f, dz * (1.0f - dz) + 0.5f, 0.5f * dz * dz};

		return w[0] * (   v[0] * ( u[0] * data[xc[0] + yc[0] + zc[0]] + u[1] * data[xc[1] + yc[0] + zc[0]] + u[2] * data[xc[2] + yc[0] + zc[0]] )
		                + v[1] * ( u[0] * data[xc[0] + yc[1] + zc[0]] + u[1] * data[xc[1] + yc[1] + zc[0]] + u[2] * data[xc[2] + yc[1] + zc[0]] )
		                + v[2] * ( u[0] * data[xc[0] + yc[2] + zc[0]] + u[1] * data[xc[1] + yc[2] + zc[0]] + u[2] * data[xc[2] + yc[2] + zc[0]] ) )
		     + w[1] * (   v[0] * ( u[0] * data[xc[0] + yc[0] + zc[1]] + u[1] * data[xc[1] + yc[0] + zc[1]] + u[2] * data[xc[2] + yc[0] + zc[1]] )
		                + v[1] * ( u[0] * data[xc[0] + yc[1] + zc[1]] + u[1] * data[xc[1] + yc[1] + zc[1]] + u[2] * data[xc[2] + yc[1] + zc[1]] )
		                + v[2] * ( u[0] * data[xc[0] + yc[2] + zc[1]] + u[1] * data[xc[1] + yc[2] + zc[1]] + u[2] * data[xc[2] + yc[2] + zc[1]] ) )
		     + w[2] * (   v[0] * ( u[0] * data[xc[0] + yc[0] + zc[2]] + u[1] * data[xc[1] + yc[0] + zc[2]] + u[2] * data[xc[2] + yc[0] + zc[2]] )
		                + v[1] * ( u[0] * data[xc[0] + yc[1] + zc[2]] + u[1] * data[xc[1] + yc[1] + zc[2]] + u[2] * data[xc[2] + yc[1] + zc[2]] )
		                + v[2] * ( u[0] * data[xc[0] + yc[2] + zc[2]] + u[1] * data[xc[1] + yc[2] + zc[2]] + u[2] * data[xc[2] + yc[2] + zc[2]] ) );

	}
	return 0.f;
}

float BLI_voxel_sample_tricubic(float *data, const int res[3], const float co[3], int bspline)
{
	if (data) {

		const float xf = co[0] * (float)res[0] - 0.5f;
		const float yf = co[1] * (float)res[1] - 0.5f;
		const float zf = co[2] * (float)res[2] - 0.5f;
		const int x = FLOORI(xf), y = FLOORI(yf), z = FLOORI(zf);

		const int64_t xc[4] = {
		    _clamp(x - 1, 0, res[0] - 1),
		    _clamp(x,     0, res[0] - 1),
		    _clamp(x + 1, 0, res[0] - 1),
		    _clamp(x + 2, 0, res[0] - 1),
		};
		const int64_t yc[4] = {
		    _clamp(y - 1, 0, res[1] - 1) * res[0],
		    _clamp(y,     0, res[1] - 1) * res[0],
		    _clamp(y + 1, 0, res[1] - 1) * res[0],
		    _clamp(y + 2, 0, res[1] - 1) * res[0],
		};
		const int64_t zc[4] = {
		    _clamp(z - 1, 0, res[2] - 1) * res[0] * res[1],
		    _clamp(z,     0, res[2] - 1) * res[0] * res[1],
		    _clamp(z + 1, 0, res[2] - 1) * res[0] * res[1],
		    _clamp(z + 2, 0, res[2] - 1) * res[0] * res[1],
		};
		const float dx = xf - (float)x, dy = yf - (float)y, dz = zf - (float)z;

		float u[4], v[4], w[4];
		if (bspline) {	// B-Spline
			u[0] = (((-1.f/6.f)*dx + 0.5f)*dx - 0.5f)*dx + (1.f/6.f);
			u[1] =  ((     0.5f*dx - 1.f )*dx       )*dx + (2.f/3.f);
			u[2] =  ((    -0.5f*dx + 0.5f)*dx + 0.5f)*dx + (1.f/6.f);
			u[3] =   ( 1.f/6.f)*dx*dx*dx;
			v[0] = (((-1.f/6.f)*dy + 0.5f)*dy - 0.5f)*dy + (1.f/6.f);
			v[1] =  ((     0.5f*dy - 1.f )*dy       )*dy + (2.f/3.f);
			v[2] =  ((    -0.5f*dy + 0.5f)*dy + 0.5f)*dy + (1.f/6.f);
			v[3] =  ( 1.f/6.f)*dy*dy*dy;
			w[0] = (((-1.f/6.f)*dz + 0.5f)*dz - 0.5f)*dz + (1.f/6.f);
			w[1] =  ((     0.5f*dz - 1.f )*dz       )*dz + (2.f/3.f);
			w[2] =  ((    -0.5f*dz + 0.5f)*dz + 0.5f)*dz + (1.f/6.f);
			w[3] =   ( 1.f/6.f)*dz*dz*dz;
		}
		else {	// Catmull-Rom
			u[0] = ((-0.5f*dx + 1.0f)*dx - 0.5f)*dx;
			u[1] = (( 1.5f*dx - 2.5f)*dx       )*dx + 1.0f;
			u[2] = ((-1.5f*dx + 2.0f)*dx + 0.5f)*dx;
			u[3] = (( 0.5f*dx - 0.5f)*dx       )*dx;
			v[0] = ((-0.5f*dy + 1.0f)*dy - 0.5f)*dy;
			v[1] = (( 1.5f*dy - 2.5f)*dy       )*dy + 1.0f;
			v[2] = ((-1.5f*dy + 2.0f)*dy + 0.5f)*dy;
			v[3] = (( 0.5f*dy - 0.5f)*dy       )*dy;
			w[0] = ((-0.5f*dz + 1.0f)*dz - 0.5f)*dz;
			w[1] = (( 1.5f*dz - 2.5f)*dz       )*dz + 1.0f;
			w[2] = ((-1.5f*dz + 2.0f)*dz + 0.5f)*dz;
			w[3] = (( 0.5f*dz - 0.5f)*dz       )*dz;
		}

		return w[0] * (   v[0] * ( u[0] * data[xc[0] + yc[0] + zc[0]] + u[1] * data[xc[1] + yc[0] + zc[0]] + u[2] * data[xc[2] + yc[0] + zc[0]] + u[3] * data[xc[3] + yc[0] + zc[0]] )
		                + v[1] * ( u[0] * data[xc[0] + yc[1] + zc[0]] + u[1] * data[xc[1] + yc[1] + zc[0]] + u[2] * data[xc[2] + yc[1] + zc[0]] + u[3] * data[xc[3] + yc[1] + zc[0]] )
		                + v[2] * ( u[0] * data[xc[0] + yc[2] + zc[0]] + u[1] * data[xc[1] + yc[2] + zc[0]] + u[2] * data[xc[2] + yc[2] + zc[0]] + u[3] * data[xc[3] + yc[2] + zc[0]] )
		                + v[3] * ( u[0] * data[xc[0] + yc[3] + zc[0]] + u[1] * data[xc[1] + yc[3] + zc[0]] + u[2] * data[xc[2] + yc[3] + zc[0]] + u[3] * data[xc[3] + yc[3] + zc[0]] ) )
		     + w[1] * (   v[0] * ( u[0] * data[xc[0] + yc[0] + zc[1]] + u[1] * data[xc[1] + yc[0] + zc[1]] + u[2] * data[xc[2] + yc[0] + zc[1]] + u[3] * data[xc[3] + yc[0] + zc[1]] )
		                + v[1] * ( u[0] * data[xc[0] + yc[1] + zc[1]] + u[1] * data[xc[1] + yc[1] + zc[1]] + u[2] * data[xc[2] + yc[1] + zc[1]] + u[3] * data[xc[3] + yc[1] + zc[1]] )
		                + v[2] * ( u[0] * data[xc[0] + yc[2] + zc[1]] + u[1] * data[xc[1] + yc[2] + zc[1]] + u[2] * data[xc[2] + yc[2] + zc[1]] + u[3] * data[xc[3] + yc[2] + zc[1]] )
		                + v[3] * ( u[0] * data[xc[0] + yc[3] + zc[1]] + u[1] * data[xc[1] + yc[3] + zc[1]] + u[2] * data[xc[2] + yc[3] + zc[1]] + u[3] * data[xc[3] + yc[3] + zc[1]] ) )
		     + w[2] * (   v[0] * ( u[0] * data[xc[0] + yc[0] + zc[2]] + u[1] * data[xc[1] + yc[0] + zc[2]] + u[2] * data[xc[2] + yc[0] + zc[2]] + u[3] * data[xc[3] + yc[0] + zc[2]] )
		                + v[1] * ( u[0] * data[xc[0] + yc[1] + zc[2]] + u[1] * data[xc[1] + yc[1] + zc[2]] + u[2] * data[xc[2] + yc[1] + zc[2]] + u[3] * data[xc[3] + yc[1] + zc[2]] )
		                + v[2] * ( u[0] * data[xc[0] + yc[2] + zc[2]] + u[1] * data[xc[1] + yc[2] + zc[2]] + u[2] * data[xc[2] + yc[2] + zc[2]] + u[3] * data[xc[3] + yc[2] + zc[2]] )
		                + v[3] * ( u[0] * data[xc[0] + yc[3] + zc[2]] + u[1] * data[xc[1] + yc[3] + zc[2]] + u[2] * data[xc[2] + yc[3] + zc[2]] + u[3] * data[xc[3] + yc[3] + zc[2]] ) )
		     + w[3] * (   v[0] * ( u[0] * data[xc[0] + yc[0] + zc[3]] + u[1] * data[xc[1] + yc[0] + zc[3]] + u[2] * data[xc[2] + yc[0] + zc[3]] + u[3] * data[xc[3] + yc[0] + zc[3]] )
		                + v[1] * ( u[0] * data[xc[0] + yc[1] + zc[3]] + u[1] * data[xc[1] + yc[1] + zc[3]] + u[2] * data[xc[2] + yc[1] + zc[3]] + u[3] * data[xc[3] + yc[1] + zc[3]] )
		                + v[2] * ( u[0] * data[xc[0] + yc[2] + zc[3]] + u[1] * data[xc[1] + yc[2] + zc[3]] + u[2] * data[xc[2] + yc[2] + zc[3]] + u[3] * data[xc[3] + yc[2] + zc[3]] )
		                + v[3] * ( u[0] * data[xc[0] + yc[3] + zc[3]] + u[1] * data[xc[1] + yc[3] + zc[3]] + u[2] * data[xc[2] + yc[3] + zc[3]] + u[3] * data[xc[3] + yc[3] + zc[3]] ) );

	}
	return 0.f;
}
