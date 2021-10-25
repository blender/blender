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

#ifndef __FREESTYLE_NODE_SCENE_RENDER_LAYER_H__
#define __FREESTYLE_NODE_SCENE_RENDER_LAYER_H__

/** \file blender/freestyle/intern/scene_graph/NodeSceneRenderLayer.h
 *  \ingroup freestyle
 *  \brief Class to represent a scene render layer in Blender.
 */

#include "Node.h"

extern "C" {
#include "DNA_scene_types.h" /* for Scene and SceneRenderLayer */
}

using namespace std;

namespace Freestyle {

class NodeSceneRenderLayer : public Node
{
public:
	inline NodeSceneRenderLayer(Scene& scene, SceneRenderLayer& srl) : Node(), _Scene(scene), _SceneRenderLayer(srl) {}
	virtual ~NodeSceneRenderLayer() {}

	inline struct Scene& scene() const
	{
		return _Scene;
	}

	inline struct SceneRenderLayer& sceneRenderLayer() const
	{
		return _SceneRenderLayer;
	}

	/*! Accept the corresponding visitor */
	virtual void accept(SceneVisitor& v);

protected:

	Scene& _Scene;
	SceneRenderLayer& _SceneRenderLayer;
};

} /* namespace Freestyle */

#endif // __FREESTYLE_NODE_SCENE_RENDER_LAYER_H__
