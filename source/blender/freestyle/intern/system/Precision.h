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

#ifndef __FREESTYLE_PRECISION_H__
#define __FREESTYLE_PRECISION_H__

/** \file blender/freestyle/intern/system/Precision.h
 *  \ingroup freestyle
 *  \brief Define the float precision used in the program
 *  \author Stephane Grabli
 *  \date 30/07/2002
 */

namespace Freestyle {

typedef double real;

#ifndef SWIG
	static const real M_EPSILON = 0.00000001;
#endif // SWIG

} /* namespace Freestyle */

#endif // __FREESTYLE_PRECISION_H__
