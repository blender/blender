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

#include "BLI_listbase.h"
#include "BLI_ghash.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_string_utils.h"

#include "BKE_context.h"

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

	const bool use_lighting = true || (!select && ((U.manipulator_flag & USER_MANIPULATOR_SHADED) != 0));
	Gwn_VertBuf *vbo;
	Gwn_IndexBuf *el;
	Gwn_Batch *batch;
	Gwn_IndexBufBuilder elb = {0};

	Gwn_VertFormat format = {0};
	uint pos_id = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
	uint nor_id;

	if (use_lighting) {
		nor_id = GWN_vertformat_attr_add(&format, "nor", GWN_COMP_I16, 3, GWN_FETCH_INT_TO_FLOAT_UNIT);
	}

	/* Elements */
	GWN_indexbuf_init(&elb, GWN_PRIM_TRIS, info->ntris, info->nverts);
	for (int i = 0; i < info->ntris; ++i) {
		const unsigned short *idx = &info->indices[i * 3];
		GWN_indexbuf_add_tri_verts(&elb, idx[0], idx[1], idx[2]);
	}
	el = GWN_indexbuf_build(&elb);

	vbo = GWN_vertbuf_create_with_format(&format);
	GWN_vertbuf_data_alloc(vbo, info->nverts);

	GWN_vertbuf_attr_fill(vbo, pos_id, info->verts);

	if (use_lighting) {
		/* Normals are expected to be smooth. */
		GWN_vertbuf_attr_fill(vbo, nor_id, info->normals);
	}

	batch = GWN_batch_create_ex(GWN_PRIM_TRIS, vbo, el, GWN_BATCH_OWNS_VBO | GWN_BATCH_OWNS_INDEX);
	GWN_batch_program_set_builtin(batch, GPU_SHADER_3D_UNIFORM_COLOR);

	GWN_batch_uniform_4fv(batch, "color", color);

	/* We may want to re-visit this, for now disable
	 * since it causes issues leaving the GL state modified. */
#if 0
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
#endif

	GWN_batch_draw(batch);

#if 0
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
#endif


	GWN_batch_discard(batch);
}

void wm_manipulator_vec_draw(
        const float color[4], const float (*verts)[3], uint vert_count,
        uint pos, uint primitive_type)
{
	immUniformColor4fv(color);
	immBegin(primitive_type, vert_count);
	for (int i = 0; i < vert_count; i++) {
		immVertex3fv(pos, verts[i]);
	}
	immEnd();
}
