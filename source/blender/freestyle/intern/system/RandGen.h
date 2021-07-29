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

#ifndef __FREESTYLE_RAND_GEN_H__
#define __FREESTYLE_RAND_GEN_H__

/** \file blender/freestyle/intern/system/RandGen.h
 *  \ingroup freestyle
 *  \brief Pseudo-random number generator
 *  \author Fredo Durand
 *  \date 20/05/2003
 */

// TODO Check whether we could replace this with BLI rand stuff...

#include "../system/Precision.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

class RandGen
{
public:
	static real drand48();
	static void srand48(long value);

private:
	static void next();

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:RandGen")
#endif
};

} /* namespace Freestyle */

#endif // __FREESTYLE_RAND_GEN_H__
