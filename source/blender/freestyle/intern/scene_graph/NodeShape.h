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

#ifndef __FREESTYLE_NODE_SHAPE_H__
#define __FREESTYLE_NODE_SHAPE_H__

/** \file blender/freestyle/intern/scene_graph/NodeShape.h
 *  \ingroup freestyle
 *  \brief Class to build a shape node. It contains a Rep, which is the shape geometry
 *  \author Stephane Grabli
 *  \date 25/01/2002
 */

#include <vector>

#include "../geometry/BBox.h"
#include "../geometry/Geom.h"

#include "../system/FreestyleConfig.h"

#include "FrsMaterial.h"
#include "Node.h"
#include "Rep.h"

using namespace std;

namespace Freestyle {

using namespace Geometry;

class NodeShape : public Node
{
public:
	inline NodeShape() : Node() {}

	virtual ~NodeShape();

	/*! Adds a Rep to the _Shapes list
	 *  The delete of the rep is done when it is not used any more by the Scene Manager.
	 *  So, it must not be deleted by the caller
	 */
	virtual void AddRep(Rep *iRep)
	{
		if (NULL == iRep)
			return;
		_Shapes.push_back(iRep);
		iRep->addRef();

		// updates bbox:
		AddBBox(iRep->bbox());
	}

	/*! Accept the corresponding visitor */
	virtual void accept(SceneVisitor& v);

	/*! Sets the shape material */
	inline void setFrsMaterial(const FrsMaterial& iMaterial)
	{
		_FrsMaterial = iMaterial;
	}

	/*! accessors */
	/*! returns the shape's material */
	inline FrsMaterial& frs_material()
	{
		return _FrsMaterial;
	}

	inline const vector<Rep*>& shapes()
	{
		return _Shapes;
	}

private:
	/*! list of shapes */
	vector<Rep*> _Shapes;

	/*! Shape Material */
	FrsMaterial _FrsMaterial;
};

} /* namespace Freestyle */

#endif // __FREESTYLE_NODE_SHAPE_H__
