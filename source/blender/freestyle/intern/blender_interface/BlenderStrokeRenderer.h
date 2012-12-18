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

#ifndef __BLENDERSTROKERENDERER_H__
#define __BLENDERSTROKERENDERER_H__

/** \file blender/freestyle/intern/blender_interface/BlenderStrokeRenderer.h
 *  \ingroup freestyle
 */

#include "../stroke/StrokeRenderer.h"
#include "../system/FreestyleConfig.h"

// XXX Are those "ifdef __cplusplus" useful here?
#ifdef __cplusplus
extern "C" {
#endif

#include "DNA_material_types.h"
#include "DNA_scene_types.h"

#include "render_types.h"

#ifdef __cplusplus
}
#endif

class LIB_STROKE_EXPORT BlenderStrokeRenderer : public StrokeRenderer
{
public:
	BlenderStrokeRenderer(Render *re, int render_count);
	virtual ~BlenderStrokeRenderer();

	/*! Renders a stroke rep */
	virtual void RenderStrokeRep(StrokeRep *iStrokeRep) const;
	virtual void RenderStrokeRepBasic(StrokeRep *iStrokeRep) const;

	Object *NewMesh() const;

	Render *RenderScene(Render *re);

protected:
	Scene *old_scene;
	Scene *freestyle_scene;
	Material *material;
	float _width, _height;
	float _z, _z_delta;

	float get_stroke_vertex_z(void) const;
};

#endif // __BLENDERSTROKERENDERER_H__
