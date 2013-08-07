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

#ifndef __FREESTYLE_PSEUDO_NOISE_H__
#define __FREESTYLE_PSEUDO_NOISE_H__

/** \file blender/freestyle/intern/system/PseudoNoise.h
 *  \ingroup freestyle
 *  \brief Class to define a pseudo Perlin noise
 *  \author Fredo Durand
 *  \date 16/06/2003
 */

#include "FreestyleConfig.h"
#include "Precision.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

class LIB_SYSTEM_EXPORT PseudoNoise
{
public:
	PseudoNoise();

	virtual ~PseudoNoise() {}

	real smoothNoise(real x);
	real linearNoise(real x);

	real turbulenceSmooth(real x, unsigned nbOctave = 8);
	real turbulenceLinear(real x, unsigned nbOctave = 8);

	static void init(long seed);

protected:
	static const unsigned NB_VALUE_NOISE = 512;
	static real _values[NB_VALUE_NOISE];

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:PseudoNoise")
#endif
};

} /* namespace Freestyle */

#endif // __FREESTYLE_PSEUDO_NOISE_H__
