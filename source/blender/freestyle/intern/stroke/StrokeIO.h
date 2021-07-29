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

#ifndef __FREESTYLE_STROKE_IO_H__
#define __FREESTYLE_STROKE_IO_H__

/** \file blender/freestyle/intern/stroke/StrokeIO.h
 *  \ingroup freestyle
 *  \brief Functions to manage I/O for the stroke
 *  \author Stephane Grabli
 *  \date 03/02/2004
 */

#include <iostream>

#include "Stroke.h"

#include "../system/FreestyleConfig.h"

namespace Freestyle {

ostream& operator<<(ostream& out, const StrokeAttribute& iStrokeAttribute);

ostream& operator<<(ostream& out, const StrokeVertex& iStrokeVertex);

ostream& operator<<(ostream& out, const Stroke& iStroke);

} /* namespace Freestyle */

#endif // __FREESTYLE_STROKE_IO_H__
