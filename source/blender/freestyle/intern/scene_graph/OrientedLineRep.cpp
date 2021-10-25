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

/** \file blender/freestyle/intern/scene_graph/OrientedLineRep.cpp
 *  \ingroup freestyle
 *  \brief Class to display an oriented line representation.
 *  \author Stephane Grabli
 *  \date 24/10/2002
 */

#include "OrientedLineRep.h"

#include "../system/BaseObject.h"

namespace Freestyle {

void OrientedLineRep::accept(SceneVisitor& v)
{
	Rep::accept(v);
	if (!frs_material())
		v.visitOrientedLineRep(*this);
	else
		v.visitLineRep(*this);
}

} /* namespace Freestyle */
