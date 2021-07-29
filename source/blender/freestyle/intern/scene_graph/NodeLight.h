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

#ifndef __FREESTYLE_NODE_LIGHT_H__
#define __FREESTYLE_NODE_LIGHT_H__

/** \file blender/freestyle/intern/scene_graph/NodeLight.h
 *  \ingroup freestyle
 *  \brief Class to represent a light node
 *  \author Stephane Grabli
 *  \date 25/01/2002
 */

#include "Node.h"

#include "../geometry/Geom.h"

#include "../system/FreestyleConfig.h"

namespace Freestyle {

using namespace Geometry;

class NodeLight : public Node
{
public:
	NodeLight(); 
	NodeLight(NodeLight& iBrother);

	virtual ~NodeLight() {}

	/*! Accept the corresponding visitor */
	virtual void accept(SceneVisitor& v);

	/*! Accessors for the light properties */
	inline const float * ambient() const
	{
		return Ambient;
	}

	inline const float * diffuse() const
	{
		return Diffuse;
	}

	inline const float * specular() const
	{
		return Specular;
	}

	inline const float * position() const
	{
		return Position;
	}

	inline bool isOn() const
	{
		return on;
	}

	inline int number() const
	{
		return _number;
	}

private:
	// Data members
	// ============

	/*! on=true, the light is on */
	bool on;

	/*! The color definition */
	float Ambient[4];
	float Diffuse[4];
	float Specular[4];

	/*! Light position. if w = 0, the light is placed at infinite. */
	float Position[4];

	/*! used to manage the number of lights */
	/*! numberOfLights
	 *    the number of lights in the scene.
	 *    Initially, 0.
	 */
	static int numberOfLights;
	/*! The current lignt number */
	int _number;
};

} /* namespace Freestyle */

#endif // __FREESTYLE_NODE_LIGHT_H__
