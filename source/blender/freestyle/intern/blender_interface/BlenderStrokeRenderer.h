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

#ifndef __BLENDER_STROKE_RENDERER_H__
#define __BLENDER_STROKE_RENDERER_H__

/** \file blender/freestyle/intern/blender_interface/BlenderStrokeRenderer.h
 *  \ingroup freestyle
 */

#include "../stroke/StrokeRenderer.h"
#include "../system/FreestyleConfig.h"

extern "C" {
struct GHash;
struct Main;
struct Material;
struct Object;
struct Render;
struct Scene;
struct bContext;
struct bNodeTree;
}

namespace Freestyle {

class BlenderStrokeRenderer : public StrokeRenderer
{
public:
	BlenderStrokeRenderer(Render *re, int render_count);
	virtual ~BlenderStrokeRenderer();

	/*! Renders a stroke rep */
	virtual void RenderStrokeRep(StrokeRep *iStrokeRep) const;
	virtual void RenderStrokeRepBasic(StrokeRep *iStrokeRep) const;

	Object *NewMesh() const;

	Render *RenderScene(Render *re, bool render);

	static Material* GetStrokeShader(Main *bmain, bNodeTree *iNodeTree, bool do_id_user);

protected:
	Main *freestyle_bmain;
	Scene *old_scene;
	Scene *freestyle_scene;
	bContext *_context;
	float _width, _height;
	float _z, _z_delta;
	unsigned int _mesh_id;
	bool _use_shading_nodes;
	struct GHash *_nodetree_hash;

	float get_stroke_vertex_z(void) const;
	unsigned int get_stroke_mesh_id(void) const;
	bool test_triangle_visibility(StrokeVertexRep *svRep[3]) const;
	void test_strip_visibility(Strip::vertex_container& strip_vertices,
		int *visible_faces, int *visible_segments) const;

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:BlenderStrokeRenderer")
#endif

};

} /* namespace Freestyle */

#endif // __BLENDER_STROKE_RENDERER_H__
