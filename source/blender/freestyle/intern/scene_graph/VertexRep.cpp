/*
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
 */

/** \file \ingroup freestyle
 *  \brief Class to define the representation of a vertex for displaying purpose.
 */

#include "VertexRep.h"

namespace Freestyle {

void VertexRep::ComputeBBox()
{
	setBBox(BBox<Vec3r>(Vec3r(_coordinates[0], _coordinates[1], _coordinates[2]),
	                    Vec3r(_coordinates[0], _coordinates[1], _coordinates[2])));
}

} /* namespace Freestyle */
