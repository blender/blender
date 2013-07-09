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

/** \file blender/freestyle/intern/geometry/GridHelpers.cpp
 *  \ingroup freestyle
 *  \brief Class to define a cell grid surrounding the projected image of a scene
 *  \author Alexander Beels
 *  \date 2010-12-21
 */

#include <math.h>

#include "GridHelpers.h"

namespace Freestyle {

void GridHelpers::getDefaultViewProscenium(real viewProscenium[4])
{
	// Get proscenium boundary for culling
	// bufferZone determines the amount by which the area processed should exceed the actual image area.
	// This is intended to avoid visible artifacts generated along the proscenium edge.
	// Perhaps this is no longer needed now that entire view edges are culled at once, since that theoretically
	// should eliminate visible artifacts.
	// To the extent it is still useful, bufferZone should be put into the UI as configurable percentage value
	const real bufferZone = 0.05;
	// borderZone describes a blank border outside the proscenium, but still inside the image area.
	// Only intended for exposing possible artifacts along or outside the proscenium edge during debugging.
	const real borderZone = 0.0;
	viewProscenium[0] = freestyle_viewport[2] * (borderZone - bufferZone);
	viewProscenium[1] = freestyle_viewport[2] * (1.0f - borderZone + bufferZone);
	viewProscenium[2] = freestyle_viewport[3] * (borderZone - bufferZone);
	viewProscenium[3] = freestyle_viewport[3] * (1.0f - borderZone + bufferZone);
}

GridHelpers::Transform::~Transform ()
{
}

} /* namespace Freestyle */
