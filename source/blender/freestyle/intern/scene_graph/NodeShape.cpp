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

/** \file blender/freestyle/intern/scene_graph/NodeShape.cpp
 *  \ingroup freestyle
 *  \brief Class to build a shape node. It contains a Rep, which is the shape geometry
 *  \author Stephane Grabli
 *  \date 25/01/2002
 */

#include "NodeShape.h"

namespace Freestyle {

NodeShape::~NodeShape()
{
	vector<Rep *>::iterator rep;

	if (0 != _Shapes.size()) {
		for (rep = _Shapes.begin(); rep != _Shapes.end(); ++rep) {
			int refCount = (*rep)->destroy();
			if (0 == refCount)
				delete (*rep);
		}

		_Shapes.clear();
	}
}

void NodeShape::accept(SceneVisitor& v)
{
	v.visitNodeShape(*this);

	v.visitFrsMaterial(_FrsMaterial);

	v.visitNodeShapeBefore(*this);
	vector<Rep *>::iterator rep;
	for (rep = _Shapes.begin(); rep != _Shapes.end(); ++rep)
		(*rep)->accept(v);
	v.visitNodeShapeAfter(*this);
}

} /* namespace Freestyle */
