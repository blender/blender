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

/** \file blender/freestyle/intern/system/PseudoNoise.cpp
 *  \ingroup freestyle
 *  \brief Class to define a pseudo Perlin noise
 *  \author Fredo Durand
 *  \date 16/06/2003
 */

#include "BLI_math.h"

#include "PseudoNoise.h"
#include "RandGen.h"

static int modf_to_index(Freestyle::real x, unsigned int range)
{
	if (finite(x)) {
		Freestyle::real tmp;
		int i = abs((int)(modf(x, &tmp) * range));
		BLI_assert(i >= 0 && i < range);
		return i;
	}
	else {
		return 0;
	}
}

namespace Freestyle {

real PseudoNoise::_values[];

PseudoNoise::PseudoNoise() {}

void PseudoNoise::init(long seed)
{
	RandGen::srand48(seed);
	for (unsigned int i = 0; i < NB_VALUE_NOISE; i++)
		_values[i] = -1.0 + 2.0 * RandGen::drand48();
}

real PseudoNoise::linearNoise(real x)
{
	real tmp;
	int i = modf_to_index(x, NB_VALUE_NOISE);
	real x1 = _values[i], x2 = _values[(i + 1) % NB_VALUE_NOISE];
	real t = modf(x * NB_VALUE_NOISE, &tmp);
	return x1 * (1 - t) + x2 * t;
}

static real LanczosWindowed(real t)
{
	if (fabs(t) > 2)
		return 0;
	if (fabs(t) < M_EPSILON)
		return 1.0;
	return sin(M_PI * t) / (M_PI * t) * sin(M_PI * t / 2.0) / (M_PI * t / 2.0);
}

real PseudoNoise::smoothNoise(real x)
{
	real tmp;
	int i = modf_to_index(x, NB_VALUE_NOISE);
	int h = i - 1;
	if (UNLIKELY(h < 0)) {
		h = NB_VALUE_NOISE + h;
	}

	real x1 = _values[i], x2 = _values[(i + 1) % NB_VALUE_NOISE];
	real x0 = _values[h], x3 = _values[(i + 2) % NB_VALUE_NOISE];

	real t = modf(x * NB_VALUE_NOISE, &tmp);
	real y0 = LanczosWindowed(-1 -t);
	real y1 = LanczosWindowed(-t);
	real y2 = LanczosWindowed(1 - t);
	real y3 = LanczosWindowed(2 - t);
#if 0
	cerr << "x0=" << x0 << "  x1=" << x1 << "  x2=" << x2 << "  x3=" << x3 << endl;
	cerr << "y0=" << y0 << "  y1=" << y1 << "  y2=" << y2 << "  y3=" << y3 << "  :" << endl;
#endif
	return (x0 * y0 + x1 * y1 + x2 * y2 + x3 * y3) / (y0 + y1 + y2 + y3);
}

real PseudoNoise::turbulenceSmooth(real x, unsigned nbOctave)
{
	real y = 0;
	real k = 1.0;
	for (unsigned int i = 0; i < nbOctave; i++) {
		y = y + k * smoothNoise(x * k);
		k = k / 2.0;
	}
	return y;
}

real PseudoNoise::turbulenceLinear(real x, unsigned nbOctave)
{
	real y = 0;
	real k = 1.0;
	for (unsigned int i = 0; i < nbOctave; i++) {
		y = y + k * linearNoise(x * k);
		k = k / 2.0;
	}
	return y;
}

} /* namespace Freestyle */
