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
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __NOISE_H__
#define __NOISE_H__

/** \file blender/freestyle/intern/geometry/Noise.h
 *  \ingroup freestyle
 *  \brief Class to define Perlin noise
 *  \author Emmanuel Turquin
 *  \date 12/01/2004
 */

#include "Geom.h"

#include "../system/FreestyleConfig.h"

#define _NOISE_B 0x100

using namespace Geometry;
using namespace std;

/*! Class to provide Perlin Noise functionalities */
class LIB_GEOMETRY_EXPORT Noise
{
public:
	/*! Builds a Noise object */
	Noise(long seed = -1);

	/*! Destructor */
	~Noise() {}

	/*! Returns a noise value for a 1D element */
	float turbulence1(float arg, float freq, float amp, unsigned oct = 4);

	/*! Returns a noise value for a 2D element */
	float turbulence2(Vec2f& v, float freq, float amp, unsigned oct = 4);

	/*! Returns a noise value for a 3D element */
	float turbulence3(Vec3f& v, float freq, float amp, unsigned oct = 4);

	/*! Returns a smooth noise value for a 1D element */
	float smoothNoise1(float arg);

	/*! Returns a smooth noise value for a 2D element */
	float smoothNoise2(Vec2f& vec);

	/*! Returns a smooth noise value for a 3D element */
	float smoothNoise3(Vec3f& vec);

private:
	int p[_NOISE_B + _NOISE_B + 2];
	float g3[_NOISE_B + _NOISE_B + 2][3];
	float g2[_NOISE_B + _NOISE_B + 2][2];
	float g1[_NOISE_B + _NOISE_B + 2];
	int start;
};

#endif // __NOISE_H__
