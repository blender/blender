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

#ifndef __FREESTYLE_ORIENTED_LINE_REP_H__
#define __FREESTYLE_ORIENTED_LINE_REP_H__

/** \file blender/freestyle/intern/scene_graph/OrientedLineRep.h
 *  \ingroup freestyle
 *  \brief Class to display an oriented line representation.
 *  \author Stephane Grabli
 *  \date 24/10/2002
 */

#include "LineRep.h"

#include "../system/FreestyleConfig.h"

namespace Freestyle {

class LIB_SCENE_GRAPH_EXPORT OrientedLineRep : public LineRep
{
public:
	OrientedLineRep() : LineRep() {}
	/*! Builds a single line from 2 vertices
	 *  v1
	 *    first vertex
	 *  v2
	 *    second vertex
	 */
	inline OrientedLineRep(const Vec3r& v1, const Vec3r& v2) : LineRep(v1, v2) {}

	/*! Builds a line rep from a vertex chain */
	inline OrientedLineRep(const vector<Vec3r>& vertices) : LineRep(vertices) {}

	/*! Builds a line rep from a vertex chain */
	inline OrientedLineRep(const list<Vec3r>& vertices) : LineRep(vertices) {}

	virtual ~OrientedLineRep() {}

	/*! Accept the corresponding visitor */
	virtual void accept(SceneVisitor& v);
};

} /* namespace Freestyle */

#endif // __FREESTYLE_ORIENTED_LINE_REP_H__
