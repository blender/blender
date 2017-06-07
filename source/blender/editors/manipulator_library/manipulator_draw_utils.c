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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file manipulator_draw_utils.c
 *  \ingroup wm
 */

#include "BKE_context.h"

#include "BLI_listbase.h"
#include "BLI_ghash.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_string_utils.h"

#include "DNA_manipulator_types.h"

#include "ED_screen.h"
#include "ED_view3d.h"

#include "GPU_batch.h"
#include "GPU_glew.h"
#include "GPU_immediate.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

/* only for own init/exit calls (wm_manipulatortype_init/wm_manipulatortype_free) */
#include "wm.h"

/* own includes */
#include "manipulator_library_intern.h"

/**
 * Main draw call for ManipulatorGeomInfo data
 */
void wm_manipulator_geometryinfo_draw(const ManipulatorGeomInfo *info, const bool select, const float color[4])
{
	/* TODO store the Batches inside the ManipulatorGeomInfo and updated it when geom changes
	 * So we don't need to re-created and discard it every time */

	const bool use_lighting = true || (!select && ((U.manipulator_flag & V3D_SHADED_MANIPULATORS) != 0));
	VertexBuffer *vbo;
	ElementList *el;
	Batch *batch;
	ElementListBuilder elb = {0};

	VertexFormat format = {0};
	unsigned int pos_id = VertexFormat_add_attrib(&format, "pos", COMP_F32, 3, KEEP_FLOAT);
	unsigned int nor_id;

	if (use_lighting) {
		nor_id = VertexFormat_add_attrib(&format, "nor", COMP_I16, 3, NORMALIZE_INT_TO_FLOAT);
	}

	/* Elements */
	ElementListBuilder_init(&elb, PRIM_TRIANGLES, info->ntris, info->nverts);
	for (int i = 0; i < info->ntris; ++i) {
		const unsigned short *idx = &info->indices[i * 3];
		add_triangle_vertices(&elb, idx[0], idx[1], idx[2]);
	}
	el = ElementList_build(&elb);

	vbo = VertexBuffer_create_with_format(&format);
	VertexBuffer_allocate_data(vbo, info->nverts);

	VertexBuffer_fill_attrib(vbo, pos_id, info->verts);

	if (use_lighting) {
		/* Normals are expected to be smooth. */
		VertexBuffer_fill_attrib(vbo, nor_id, info->normals);
	}

	batch = Batch_create(PRIM_TRIANGLES, vbo, el);
	Batch_set_builtin_program(batch, GPU_SHADER_3D_UNIFORM_COLOR);

	Batch_Uniform4fv(batch, "color", color);

	glEnable(GL_CULL_FACE);
	// glEnable(GL_DEPTH_TEST);

	Batch_draw(batch);

	glDisable(GL_DEPTH_TEST);
	// glDisable(GL_CULL_FACE);


	Batch_discard_all(batch);
}

void wm_manipulator_vec_draw(
        const float color[4], const float (*verts)[3], unsigned int vert_count,
        unsigned int pos, unsigned int primitive_type)
{
	immUniformColor4fv(color);
	immBegin(primitive_type, vert_count);
	for (int i = 0; i < vert_count; i++) {
		immVertex3fv(pos, verts[i]);
	}
	immEnd();
}
