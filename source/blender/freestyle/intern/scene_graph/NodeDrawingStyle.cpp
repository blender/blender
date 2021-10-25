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

/** \file blender/freestyle/intern/scene_graph/NodeDrawingStyle.cpp
 *  \ingroup freestyle
 *  \brief Class to define a Drawing Style to be applied to the underlying children. Inherits from NodeGroup.
 *  \author Stephane Grabli
 *  \date 06/02/2002
 */

#include "NodeDrawingStyle.h"

namespace Freestyle {

void NodeDrawingStyle::accept(SceneVisitor& v)
{
	v.visitNodeDrawingStyle(*this);

	v.visitNodeDrawingStyleBefore(*this);
	v.visitDrawingStyle(_DrawingStyle);
	for (vector<Node*>::iterator node = _Children.begin(), end = _Children.end(); node != end; ++node)
		(*node)->accept(v);
	v.visitNodeDrawingStyleAfter(*this);
}

} /* namespace Freestyle */
