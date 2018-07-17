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

/** \file blender/gpu/intern/gpu_batch_utils.c
 *  \ingroup gpu
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_rect.h"
#include "BLI_math.h"
#include "BLI_polyfill_2d.h"
#include "BLI_sort_utils.h"

#include "GPU_batch.h"
#include "GPU_batch_utils.h"  /* own include */
#include "gpu_shader_private.h"

/* -------------------------------------------------------------------- */
/** \name Polygon Creation (2D)
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
GPUBatch *GPU_batch_tris_from_poly_2d_encoded(
        const uchar *polys_flat, uint polys_flat_len, const rctf *rect)
{
	const uchar (*polys)[2] = (const void *)polys_flat;
	const uint polys_len = polys_flat_len / 2;
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
			const uint verts_step_len = (&verts[i_vert]) - verts_step;
			BLI_assert(verts_step_len >= 3);
			const uint tris_len = (verts_step_len - 2);
			BLI_polyfill_calc(verts_step, verts_step_len, -1, tris_step);
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
			verts_step += verts_step_len;
			tris_step += tris_len;
			i_poly++;
			/* ignore the duplicate point */
		}
	}

	/* We have vertices and tris, make a batch from this. */
	static GPUVertFormat format = {0};
	static struct { uint pos; } attr_id;
	if (format.attr_len == 0) {
		attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
	}

	const uint verts_len = (verts_step - verts);
	const uint tris_len = (tris_step - tris);
	GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
	GPU_vertbuf_data_alloc(vbo, verts_len);

	GPUVertBufRaw pos_step;
	GPU_vertbuf_attr_get_raw_data(vbo, attr_id.pos, &pos_step);

	for (uint i = 0; i < verts_len; i++) {
		copy_v2_v2(GPU_vertbuf_raw_step(&pos_step), verts[i]);
	}

	GPUIndexBufBuilder elb;
	GPU_indexbuf_init(&elb, GPU_PRIM_TRIS, tris_len, verts_len);
	for (uint i = 0; i < tris_len; i++) {
		GPU_indexbuf_add_tri_verts(&elb, UNPACK3(tris[i]));
	}
	GPUIndexBuf *indexbuf = GPU_indexbuf_build(&elb);

	MEM_freeN(tris);
	MEM_freeN(verts);

	return GPU_batch_create_ex(
	        GPU_PRIM_TRIS, vbo,
	        indexbuf,
	        GPU_BATCH_OWNS_VBO | GPU_BATCH_OWNS_INDEX);
}

GPUBatch *GPU_batch_wire_from_poly_2d_encoded(
        const uchar *polys_flat, uint polys_flat_len, const rctf *rect)
{
	const uchar (*polys)[2] = (const void *)polys_flat;
	const uint polys_len = polys_flat_len / 2;
	BLI_assert(polys_flat_len == polys_len * 2);

	/* Over alloc */
	/* Lines are pairs of (x, y) byte locations packed into an int32_t. */
	int32_t *lines = MEM_mallocN(sizeof(*lines) * polys_len, __func__);
	int32_t *lines_step = lines;

	const float range_uchar[2] = {
		(rect ? (rect->xmax - rect->xmin) : 2.0f) / 255.0f,
		(rect ? (rect->ymax - rect->ymin) : 2.0f) / 255.0f,
	};
	const float min_uchar[2] = {
		(rect ? rect->xmin : -1.0f),
		(rect ? rect->ymin : -1.0f),
	};

	uint i_poly_prev = 0;
	uint i_poly = 0;
	while (i_poly != polys_len) {
		i_poly++;
		if (polys[i_poly - 1][0] == polys[i_poly][0] &&
		    polys[i_poly - 1][1] == polys[i_poly][1])
		{
			const uchar (*polys_step)[2] = polys + i_poly_prev;
			const uint polys_step_len = i_poly - i_poly_prev;
			BLI_assert(polys_step_len >= 2);
			for (uint i_prev = polys_step_len - 1, i = 0; i < polys_step_len; i_prev = i++) {
				union {
					uint8_t  as_u8[4];
					uint16_t as_u16[2];
					uint32_t as_u32;
				} data;
				data.as_u16[0] = *((const uint16_t *)polys_step[i_prev]);
				data.as_u16[1] = *((const uint16_t *)polys_step[i]);
				if (data.as_u16[0] > data.as_u16[1]) {
					SWAP(uint16_t, data.as_u16[0], data.as_u16[1]);
				}
				*lines_step = data.as_u32;
				lines_step++;
			}
			i_poly++;
			i_poly_prev = i_poly;
			/* ignore the duplicate point */
		}
	}

	uint lines_len = lines_step - lines;

	/* Hide Lines (we could make optional) */
	{
		qsort(lines, lines_len, sizeof(int32_t), BLI_sortutil_cmp_int);
		lines_step = lines;
		for (uint i_prev = 0, i = 1; i < lines_len; i_prev = i++) {
			if (lines[i] != lines[i_prev]) {
				*lines_step++ = lines[i_prev];
			}
			else {
				i++;
			}
		}
		lines_len = lines_step - lines;
	}

	/* We have vertices and tris, make a batch from this. */
	static GPUVertFormat format = {0};
	static struct { uint pos; } attr_id;
	if (format.attr_len == 0) {
		attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
	}

	GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
	const uint vbo_len_capacity = lines_len * 2;
	GPU_vertbuf_data_alloc(vbo, vbo_len_capacity);

	GPUVertBufRaw pos_step;
	GPU_vertbuf_attr_get_raw_data(vbo, attr_id.pos, &pos_step);

	for (uint i = 0; i < lines_len; i++) {
		union {
			uint8_t  as_u8_pair[2][2];
			uint32_t as_u32;
		} data;
		data.as_u32 = lines[i];
		for (uint k = 0; k < 2; k++) {
			float *pos_v2 = GPU_vertbuf_raw_step(&pos_step);
			for (uint j = 0; j < 2; j++) {
				pos_v2[j] = min_uchar[j] + ((float)data.as_u8_pair[k][j] * range_uchar[j]);
			}
		}
	}
	BLI_assert(vbo_len_capacity == GPU_vertbuf_raw_used(&pos_step));
	MEM_freeN(lines);
	return GPU_batch_create_ex(
	        GPU_PRIM_LINES, vbo,
	        NULL,
	        GPU_BATCH_OWNS_VBO);
}

/** \} */
