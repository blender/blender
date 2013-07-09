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

#ifndef __FREESTYLE_VERTEX_REP_H__
#define __FREESTYLE_VERTEX_REP_H__

/** \file blender/freestyle/intern/scene_graph/VertexRep.h
 *  \ingroup freestyle
 *  \brief Class to define the representation of a vertex for displaying purpose.
 *  \author Stephane Grabli
 *  \date 03/04/2002
 */

#include "Rep.h"

namespace Freestyle {

class LIB_SCENE_GRAPH_EXPORT VertexRep : public Rep
{
public:
	inline VertexRep() : Rep()
	{
		_vid = 0;
		_PointSize = 0.0f;
	}

	inline VertexRep(real x, real y, real z, int id = 0) : Rep()
	{
		_coordinates[0] = x;
		_coordinates[1] = y;
		_coordinates[2] = z;

		_vid = id;
		_PointSize = 0.0f;
	}

	inline ~VertexRep() {}

	/*! Accept the corresponding visitor */
	virtual void accept(SceneVisitor& v)
	{
		Rep::accept(v);
		v.visitVertexRep(*this);
	}

	/*! Computes the rep bounding box. */
	virtual void ComputeBBox();

	/*! accessors */
	inline const int vid() const
	{
		return _vid;
	}

	inline const real * coordinates() const
	{
		return _coordinates;
	}

	inline real x() const
	{
		return _coordinates[0];
	}

	inline real y() const
	{
		return _coordinates[1];
	}

	inline real z() const
	{
		return _coordinates[2];
	}

	inline float pointSize() const
	{
		return _PointSize;
	}

	/*! modifiers */
	inline void setVid(int id)
	{
		_vid = id;
	}

	inline void setX(real x)
	{
		_coordinates[0] = x;
	}

	inline void setY(real y)
	{
		_coordinates[1] = y;
	}

	inline void setZ(real z)
	{
		_coordinates[2] = z;
	}

	inline void setCoordinates(real x, real y, real z)
	{
		_coordinates[0] = x;
		_coordinates[1] = y;
		_coordinates[2] = z;
	}

	inline void setPointSize(float iPointSize)
	{
		_PointSize = iPointSize;
	}

private:
	int _vid; // vertex id
	real _coordinates[3];
	float _PointSize;
};

} /* namespace Freestyle */

#endif // __FREESTYLE_VERTEX_REP_H__
