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

#ifndef __FREESTYLE_LINE_REP_H__
#define __FREESTYLE_LINE_REP_H__

/** \file blender/freestyle/intern/scene_graph/LineRep.h
 *  \ingroup freestyle
 *  \brief Class to define the representation of 3D Line.
 *  \author Stephane Grabli
 *  \date 26/03/2002
 */

#include <list>
#include <vector>

#include "Rep.h"

#include "../system/FreestyleConfig.h"

using namespace std;

namespace Freestyle {

/*! Base class for all lines objects */
class LIB_SCENE_GRAPH_EXPORT LineRep : public Rep
{
public:
	/*! Line description style */
	enum LINES_STYLE {
		LINES,
		LINE_STRIP,
		LINE_LOOP,
	};

	inline LineRep() : Rep()
	{
		_width = 0.0f;
	}

	/*! Builds a single line from 2 vertices
	 *  v1
	 *    first vertex
	 *  v2
	 *    second vertex
	 */
	inline LineRep(const Vec3r& v1, const Vec3r& v2) : Rep()
	{
		setStyle(LINES);
		AddVertex(v1);
		AddVertex(v2);
		_width = 0.0f;
	}

	/*! Builds a line rep from a vertex chain */
	inline LineRep(const vector<Vec3r>& vertices) : Rep()
	{
		_vertices = vertices;
		setStyle(LINE_STRIP);
		_width = 0.0f;
	}

	/*! Builds a line rep from a vertex chain */
	inline LineRep(const list<Vec3r>& vertices) : Rep()
	{
		for (list<Vec3r>::const_iterator v = vertices.begin(), end = vertices.end(); v != end; ++v) {
			_vertices.push_back(*v);
		}
		setStyle(LINE_STRIP);
		_width = 0.0f;
	}

	virtual ~LineRep()
	{
		_vertices.clear();
	}

	/*! accessors */
	inline const LINES_STYLE style() const
	{
		return _Style;
	}

	inline const vector<Vec3r>& vertices() const
	{
		return _vertices;
	}

	inline float width() const
	{
		return _width;
	}

	/*! modifiers */
	inline void setStyle(const LINES_STYLE iStyle)
	{
		_Style = iStyle;
	}

	inline void AddVertex(const Vec3r& iVertex)
	{
		_vertices.push_back(iVertex);
	}

	inline void setVertices(const vector<Vec3r>& iVertices)
	{
		if (0 != _vertices.size()) {
			_vertices.clear();
		}
		for (vector<Vec3r>::const_iterator v = iVertices.begin(), end = iVertices.end(); v != end; ++v) {
			_vertices.push_back(*v);
		}
	}

	inline void setWidth(float iWidth)
	{
		_width = iWidth;
	}

	/*! Accept the corresponding visitor */
	virtual void accept(SceneVisitor& v)
	{
		Rep::accept(v);
		v.visitLineRep(*this);
	}

	/*! Computes the line bounding box.*/
	virtual void ComputeBBox();

private:
	LINES_STYLE _Style;
	vector<Vec3r> _vertices;
	float _width;

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:LineRep")
#endif

};

} /* namespace Freestyle */

#endif // __FREESTYLE_LINE_REP_H__
