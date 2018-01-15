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
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Mike Erwin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpu/intern/gpu_basic_shader.c
 *  \ingroup gpu
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_rect.h"
#include "BLI_math.h"
#include "BLI_polyfill2d.h"


#include "GPU_batch.h"  /* own include */
#include "gpu_shader_private.h"

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

void GWN_batch_program_set_builtin(Gwn_Batch *batch, GPUBuiltinShader shader_id)
{
	GPUShader *shader = GPU_shader_get_builtin_shader(shader_id);
	GWN_batch_program_set(batch, shader->program, shader->interface);
}

/** \} */



/* -------------------------------------------------------------------- */
/** \name Batch Creation
 * \{ */

/**
 * Creates triangles from a byte-array of polygons.
 *
 * See 'make_shape_2d_from_blend.py' utility to create data to pass to this function.
 *
 * \param polys_flat: Pairs of X, Y coordinates (repeating to signify closing the polygon).
 * \param polys_flat_len: Length of the array (must be an even number).
 * \param rect: Optional region to map the byte 0..255 coords to. When not set use -1..1.
 */
Gwn_Batch *GPU_batch_from_poly_2d_encoded(
        const uchar *polys_flat, uint polys_flat_len, const rctf *rect)
{
	uchar (*polys)[2] = (void *)polys_flat;
	uint polys_len = polys_flat_len / 2;
	BLI_assert(polys_flat_len == polys_len * 2);

	/* Over alloc in both cases */
	float (*verts)[2] = MEM_mallocN(sizeof(*verts) * polys_len, __func__);
	float (*verts_step)[2] = verts;
	uint (*tris)[3] = MEM_mallocN(sizeof(*tris) * polys_len, __func__);
	uint (*tris_step)[3] = tris;

	const float range_uchar[2] = {
		(rect ? (rect->xmax - rect->xmin) : 2.0f) / 255.0f,
		(rect ? (rect->ymax - rect->ymin) : 2.0f) / 255.0f,
	};
	const float min_uchar[2] = {
		(rect ? rect->xmin : -1.0f),
		(rect ? rect->ymin : -1.0f),
	};

	uint i_poly = 0;
	uint i_vert = 0;
	while (i_poly != polys_len) {
		for (uint j = 0; j < 2; j++) {
			verts[i_vert][j] = min_uchar[j] + ((float)polys[i_poly][j] * range_uchar[j]);
		}
		i_vert++;
		i_poly++;
		if (polys[i_poly - 1][0] == polys[i_poly][0] &&
		    polys[i_poly - 1][1] == polys[i_poly][1])
		{
			const uint verts_len = (&verts[i_vert]) - verts_step;
			BLI_assert(verts_len >= 3);
			const uint tris_len = (verts_len - 2);
			BLI_polyfill_calc(verts_step, verts_len, -1, tris_step);
			/* offset indices */
			if (verts_step != verts) {
				uint *t = tris_step[0];
				const uint offset = (verts_step - verts);
				uint tot = tris_len * 3;
				while (tot--) {
					*t += offset;
					t++;
				}
				BLI_assert(t == tris_step[tris_len]);
			}
			verts_step += verts_len;
			tris_step += (verts_len - 2);
			i_poly++;
			/* ignore the duplicate point */
		}
	}

	/* We have vertices and tris, make a batch from this. */
	static Gwn_VertFormat format = {0};
	static struct { uint pos; } attr_id;
	if (format.attrib_ct == 0) {
		attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);
	}

	const uint verts_len = (verts_step - verts);
	const uint tris_len = (tris_step - tris);
	Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
	GWN_vertbuf_data_alloc(vbo, verts_len);

	Gwn_VertBufRaw pos_step;
	GWN_vertbuf_attr_get_raw_data(vbo, attr_id.pos, &pos_step);

	for (uint i = 0; i < verts_len; i++) {
		copy_v2_v2(GWN_vertbuf_raw_step(&pos_step), verts[i]);
	}

	Gwn_IndexBufBuilder elb;
	GWN_indexbuf_init(&elb, GWN_PRIM_TRIS, tris_len, verts_len);
	for (uint i = 0; i < tris_len; i++) {
		GWN_indexbuf_add_tri_verts(&elb, UNPACK3(tris[i]));
	}
	Gwn_IndexBuf *indexbuf = GWN_indexbuf_build(&elb);

	MEM_freeN(tris);
	MEM_freeN(verts);

	return GWN_batch_create_ex(
	        GWN_PRIM_TRIS, vbo,
	        indexbuf,
	        GWN_BATCH_OWNS_VBO | GWN_BATCH_OWNS_INDEX);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Init/Exit
 * \{ */

void gpu_batch_init(void)
{
	gpu_batch_presets_init();
}

void gpu_batch_exit(void)
{
	gpu_batch_presets_exit();
}

/** \} */
