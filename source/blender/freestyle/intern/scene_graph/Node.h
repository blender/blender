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
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __FREESTYLE_NODE_H__
#define __FREESTYLE_NODE_H__

/** \file blender/freestyle/intern/scene_graph/Node.h
 *  \ingroup freestyle
 *  \brief Abstract class for scene graph nodes. Inherits from BaseObject which defines the addRef release mechanism.
 *  \author Stephane Grabli
 *  \date 24/01/2002
 */

#include "SceneVisitor.h"

#include "../system/BaseObject.h"
#include "../system/FreestyleConfig.h"
#include "../system/Precision.h"

#include "../geometry/BBox.h"
#include "../geometry/Geom.h"

using namespace std;
using namespace Geometry;

class LIB_SCENE_GRAPH_EXPORT Node : public BaseObject
{
public:
	inline Node() : BaseObject() {}

	inline Node(const Node& iBrother) : BaseObject()
	{
		_BBox = iBrother.bbox();
	}

	virtual ~Node(){}

	/*! Accept the corresponding visitor
	 *  Each inherited node must overload this method
	 */
	virtual void accept(SceneVisitor& v)
	{
		v.visitNode(*this);
	}

	/*! bounding box management */
	/*! Returns the node bounding box
	 *  If no bounding box exists, an empty bbox is returned
	 */
	virtual const BBox<Vec3r>& bbox() const
	{
		return _BBox;
	}

	/*! Sets the Node bounding box */
	virtual void setBBox(const BBox<Vec3r>& iBox)
	{
		_BBox = iBox;
	}

	/*! Makes the union of _BBox and iBox */
	virtual void AddBBox(const BBox<Vec3r>& iBox)
	{
		if(iBox.empty())
			return;

		if(_BBox.empty())
			_BBox = iBox;
		else
			_BBox += iBox;
	}

	/*! Updates the BBox */
	virtual const BBox<Vec3r>& UpdateBBox()
	{
		return _BBox;
	}

	/*! Clears the bounding box */
	virtual void clearBBox()
	{
		_BBox.clear();
	}

protected:

private: 
	BBox<Vec3r> _BBox;
};

#endif // __FREESTYLE_NODE_H__
