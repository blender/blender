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
 * The Original Code is Copyright (C) 2017 by Blender Foundation.
 * All rights reserved.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file draw_cache_impl_displist.c
 *  \ingroup draw
 *
 * \brief DispList API for render engines
 *
 * \note DispList may be removed soon! This is a utility for object types that use render.
 */

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_utildefines.h"
#include "BLI_math_vector.h"

#include "DNA_curve_types.h"

#include "BKE_displist.h"

#include "GPU_batch.h"

#include "draw_cache_impl.h"  /* own include */

static int dl_vert_len(const DispList *dl)
{
	switch (dl->type) {
		case DL_INDEX3:
		case DL_INDEX4:
			return dl->nr;
		case DL_SURF:
			return dl->parts * dl->nr;
	}
	return 0;
}

static int dl_tri_len(const DispList *dl)
{
	switch (dl->type) {
		case DL_INDEX3:
			return dl->parts;
		case DL_INDEX4:
			return dl->parts * 2;
		case DL_SURF:
			return dl->totindex * 2;
	}
	return 0;
}

/* see: displist_get_allverts */
static int curve_render_surface_vert_len_get(const ListBase *lb)
{
	int vert_len = 0;
	for (const DispList *dl = lb->first; dl; dl = dl->next) {
		vert_len += dl_vert_len(dl);
	}
	return vert_len;
}

static int curve_render_surface_tri_len_get(const ListBase *lb)
{
	int tri_len = 0;
	for (const DispList *dl = lb->first; dl; dl = dl->next) {
		tri_len += dl_tri_len(dl);
	}
	return tri_len;
}

typedef void (setTriIndicesFn)(void *thunk, uint v1, uint v2, uint v3);

static void displist_indexbufbuilder_set(
	setTriIndicesFn *set_tri_indices,
	setTriIndicesFn *set_quad_tri_indices, /* meh, find a better solution. */
	void *thunk, const DispList *dl, const int ofs)
{
	if (ELEM(dl->type, DL_INDEX3, DL_INDEX4, DL_SURF)) {
		const int *idx = dl->index;
		if (dl->type == DL_INDEX3) {
			const int i_end = dl->parts;
			for (int i = 0; i < i_end; i++, idx += 3) {
				set_tri_indices(thunk, idx[0] + ofs, idx[2] + ofs, idx[1] + ofs);
			}
		}
		else if (dl->type == DL_SURF) {
			const int i_end = dl->totindex;
			for (int i = 0; i < i_end; i++, idx += 4) {
				set_quad_tri_indices(thunk, idx[0] + ofs, idx[2] + ofs, idx[1] + ofs);
				set_quad_tri_indices(thunk, idx[2] + ofs, idx[0] + ofs, idx[3] + ofs);
			}
		}
		else {
			BLI_assert(dl->type == DL_INDEX4);
			const int i_end = dl->parts;
			for (int i = 0; i < i_end; i++, idx += 4) {
				if (idx[2] != idx[3]) {
					set_quad_tri_indices(thunk, idx[2] + ofs, idx[0] + ofs, idx[1] + ofs);
					set_quad_tri_indices(thunk, idx[0] + ofs, idx[2] + ofs, idx[3] + ofs);
				}
				else {
					set_tri_indices(thunk, idx[2] + ofs, idx[0] + ofs, idx[1] + ofs);
				}
			}
		}
	}
}

static int displist_indexbufbuilder_tess_set(
	setTriIndicesFn *set_tri_indices,
	setTriIndicesFn *set_quad_tri_indices, /* meh, find a better solution. */
	void *thunk, const DispList *dl, const int ofs)
{
	int v_idx = ofs;
	if (ELEM(dl->type, DL_INDEX3, DL_INDEX4, DL_SURF)) {
		if (dl->type == DL_INDEX3) {
			for (int i = 0; i < dl->parts; i++) {
				set_tri_indices(thunk, v_idx + 0, v_idx + 1, v_idx + 2);
				v_idx += 3;
			}
		}
		else if (dl->type == DL_SURF) {
			for (int a = 0; a < dl->parts; a++) {
				if ((dl->flag & DL_CYCL_V) == 0 && a == dl->parts - 1) {
					break;
				}
				int b = (dl->flag & DL_CYCL_U) ? 0 : 1;
				for (; b < dl->nr; b++) {
					set_quad_tri_indices(thunk, v_idx + 0, v_idx + 1, v_idx + 2);
					set_quad_tri_indices(thunk, v_idx + 3, v_idx + 4, v_idx + 5);
					v_idx += 6;
				}
			}
		}
		else {
			BLI_assert(dl->type == DL_INDEX4);
			const int *idx = dl->index;
			for (int i = 0; i < dl->parts; i++, idx += 4) {
				if (idx[2] != idx[3]) {
					set_quad_tri_indices(thunk, v_idx + 0, v_idx + 1, v_idx + 2);
					set_quad_tri_indices(thunk, v_idx + 3, v_idx + 4, v_idx + 5);
					v_idx += 6;
				}
				else {
					set_tri_indices(thunk, v_idx + 0, v_idx + 1, v_idx + 2);
					v_idx += 3;
				}
			}
		}
	}
	return v_idx;
}

void DRW_displist_vertbuf_create_pos_and_nor(ListBase *lb, GPUVertBuf *vbo)
{
	static GPUVertFormat format = { 0 };
	static struct { uint pos, nor; } attr_id;
	if (format.attr_len == 0) {
		/* initialize vertex format */
		attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		attr_id.nor = GPU_vertformat_attr_add(&format, "nor", GPU_COMP_I16, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
	}

	GPU_vertbuf_init_with_format(vbo, &format);
	GPU_vertbuf_data_alloc(vbo, curve_render_surface_vert_len_get(lb));

	BKE_displist_normals_add(lb);

	int vbo_len_used = 0;
	for (const DispList *dl = lb->first; dl; dl = dl->next) {
		const bool ndata_is_single = dl->type == DL_INDEX3;
		if (ELEM(dl->type, DL_INDEX3, DL_INDEX4, DL_SURF)) {
			const float *fp_co = dl->verts;
			const float *fp_no = dl->nors;
			const int vbo_end = vbo_len_used + dl_vert_len(dl);
			while (vbo_len_used < vbo_end) {
				GPU_vertbuf_attr_set(vbo, attr_id.pos, vbo_len_used, fp_co);
				if (fp_no) {
					static short short_no[4];
					normal_float_to_short_v3(short_no, fp_no);
					GPU_vertbuf_attr_set(vbo, attr_id.nor, vbo_len_used, short_no);
					if (ndata_is_single == false) {
						fp_no += 3;
					}
				}
				fp_co += 3;
				vbo_len_used += 1;
			}
		}
	}
}

void DRW_displist_indexbuf_create_triangles_in_order(ListBase *lb, GPUIndexBuf *ibo)
{
	const int tri_len = curve_render_surface_tri_len_get(lb);
	const int vert_len = curve_render_surface_vert_len_get(lb);

	GPUIndexBufBuilder elb;
	GPU_indexbuf_init(&elb, GPU_PRIM_TRIS, tri_len, vert_len);

	int ofs = 0;
	for (const DispList *dl = lb->first; dl; dl = dl->next) {
		displist_indexbufbuilder_set((setTriIndicesFn *)GPU_indexbuf_add_tri_verts,
		                             (setTriIndicesFn *)GPU_indexbuf_add_tri_verts,
		                             &elb, dl, ofs);
		ofs += dl_vert_len(dl);
	}

	GPU_indexbuf_build_in_place(&elb, ibo);
}

void DRW_displist_indexbuf_create_triangles_tess_split_by_material(
        ListBase *lb,
        GPUIndexBuf **ibo_mats, uint mat_len)
{
	GPUIndexBufBuilder *elb = BLI_array_alloca(elb, mat_len);

	const int tri_len = curve_render_surface_tri_len_get(lb);

	/* Init each index buffer builder */
	for (int i = 0; i < mat_len; i++) {
		GPU_indexbuf_init(&elb[i], GPU_PRIM_TRIS, tri_len * 3, tri_len * 3);
	}

	/* calc each index buffer builder */
	uint v_idx = 0;
	for (const DispList *dl = lb->first; dl; dl = dl->next) {
		v_idx = displist_indexbufbuilder_tess_set((setTriIndicesFn *)GPU_indexbuf_add_tri_verts,
		                                          (setTriIndicesFn *)GPU_indexbuf_add_tri_verts,
		                                          &elb[dl->col], dl, v_idx);
	}

	/* build each indexbuf */
	for (int i = 0; i < mat_len; i++) {
		GPU_indexbuf_build_in_place(&elb[i], ibo_mats[i]);
	}
}

typedef struct DRWDisplistWireThunk {
	uint wd_id, ofs;
	const DispList *dl;
	GPUVertBuf *vbo;
} DRWDisplistWireThunk;

static void set_overlay_wires_tri_indices(void *thunk, uint v1, uint v2, uint v3)
{
	DRWDisplistWireThunk *dwt = (DRWDisplistWireThunk *)thunk;
	uint indices[3] = {v1, v2, v3};

	for (int i = 0; i < 3; ++i) {
		/* TODO: Compute sharpness. For now, only tag real egdes. */
		uchar sharpness = 0xFF;
		GPU_vertbuf_attr_set(dwt->vbo, dwt->wd_id, indices[i], &sharpness);
	}
}

static void set_overlay_wires_quad_tri_indices(void *thunk, uint v1, uint v2, uint v3)
{
	DRWDisplistWireThunk *dwt = (DRWDisplistWireThunk *)thunk;
	uint indices[3] = {v1, v2, v3};

	for (int i = 0; i < 3; ++i) {
		/* TODO: Compute sharpness. For now, only tag real egdes. */
		uchar sharpness = (i == 0) ? 0x00 : 0xFF;
		GPU_vertbuf_attr_set(dwt->vbo, dwt->wd_id, indices[i], &sharpness);
	}
}

/* TODO reuse the position and normals from other tesselation vertbuf. */
void DRW_displist_vertbuf_create_wireframe_data_tess(ListBase *lb, GPUVertBuf *vbo)
{
	static DRWDisplistWireThunk thunk;
	static GPUVertFormat format = {0};
	if (format.attr_len == 0) {
		thunk.wd_id  = GPU_vertformat_attr_add(&format, "wd", GPU_COMP_U8, 1, GPU_FETCH_INT_TO_FLOAT_UNIT);
		GPU_vertformat_triple_load(&format);
	}

	GPU_vertbuf_init_with_format(vbo, &format);
	thunk.vbo = vbo;

	int vert_len = curve_render_surface_tri_len_get(lb) * 3;
	GPU_vertbuf_data_alloc(thunk.vbo, vert_len);

	thunk.ofs = 0;
	for (const DispList *dl = lb->first; dl; dl = dl->next) {
		thunk.dl = dl;
		/* TODO consider non-manifold edges correctly. */
		thunk.ofs = displist_indexbufbuilder_tess_set(set_overlay_wires_tri_indices,
		                                              set_overlay_wires_quad_tri_indices,
		                                              &thunk, dl, thunk.ofs);
	}

	if (thunk.ofs < vert_len) {
		GPU_vertbuf_data_resize(thunk.vbo, thunk.ofs);
	}
}

static void surf_uv_quad(const DispList *dl, const uint quad[4], float r_uv[4][2])
{
	int orco_sizeu = dl->nr - 1;
	int orco_sizev = dl->parts - 1;

	/* exception as handled in convertblender.c too */
	if (dl->flag & DL_CYCL_U) {
		orco_sizeu++;
	}
	if (dl->flag & DL_CYCL_V) {
		orco_sizev++;
	}

	for (int i = 0; i < 4; i++) {
		/* find uv based on vertex index into grid array */
		r_uv[i][0] = (quad[i] / dl->nr) / (float)orco_sizev;
		r_uv[i][1] = (quad[i] % dl->nr) / (float)orco_sizeu;

		/* cyclic correction */
		if ((i == 1 || i == 2) && r_uv[i][0] == 0.0f) {
			r_uv[i][0] = 1.0f;
		}
		if ((i == 0 || i == 1) && r_uv[i][1] == 0.0f) {
			r_uv[i][1] = 1.0f;
		}
	}
}

static void displist_vertbuf_attr_set_tri_pos_nor_uv(
        GPUVertBufRaw *pos_step, GPUVertBufRaw *nor_step, GPUVertBufRaw *uv_step,
        const float v1[3], const float v2[3], const float v3[3],
        const float n1[3], const float n2[3], const float n3[3],
        const float uv1[2], const float uv2[2], const float uv3[2],
        const bool invert_normal)
{
	if (pos_step->size != 0) {
		copy_v3_v3(GPU_vertbuf_raw_step(pos_step), v1);
		copy_v3_v3(GPU_vertbuf_raw_step(pos_step), v2);
		copy_v3_v3(GPU_vertbuf_raw_step(pos_step), v3);

		if (invert_normal) {
			float neg_n1[3], neg_n2[3], neg_n3[3];
			negate_v3_v3(neg_n1, n1);
			negate_v3_v3(neg_n2, n2);
			negate_v3_v3(neg_n3, n3);
			*(GPUPackedNormal *)GPU_vertbuf_raw_step(nor_step) = GPU_normal_convert_i10_v3(neg_n1);
			*(GPUPackedNormal *)GPU_vertbuf_raw_step(nor_step) = GPU_normal_convert_i10_v3(neg_n2);
			*(GPUPackedNormal *)GPU_vertbuf_raw_step(nor_step) = GPU_normal_convert_i10_v3(neg_n3);
		}
		else {
			*(GPUPackedNormal *)GPU_vertbuf_raw_step(nor_step) = GPU_normal_convert_i10_v3(n1);
			*(GPUPackedNormal *)GPU_vertbuf_raw_step(nor_step) = GPU_normal_convert_i10_v3(n2);
			*(GPUPackedNormal *)GPU_vertbuf_raw_step(nor_step) = GPU_normal_convert_i10_v3(n3);
		}
	}

	if (uv_step->size != 0) {
		normal_float_to_short_v2(GPU_vertbuf_raw_step(uv_step), uv1);
		normal_float_to_short_v2(GPU_vertbuf_raw_step(uv_step), uv2);
		normal_float_to_short_v2(GPU_vertbuf_raw_step(uv_step), uv3);
	}
}

void DRW_displist_vertbuf_create_pos_and_nor_and_uv_tess(
        ListBase *lb,
        GPUVertBuf *vbo_pos_nor, GPUVertBuf *vbo_uv)
{
	static GPUVertFormat format_pos_nor = { 0 };
	static GPUVertFormat format_uv = { 0 };
	static struct { uint pos, nor, uv; } attr_id;
	if (format_pos_nor.attr_len == 0) {
		/* initialize vertex format */
		attr_id.pos = GPU_vertformat_attr_add(&format_pos_nor, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		attr_id.nor = GPU_vertformat_attr_add(&format_pos_nor, "nor", GPU_COMP_I10, 3, GPU_FETCH_INT_TO_FLOAT_UNIT);
		GPU_vertformat_triple_load(&format_pos_nor);
		/* UVs are in [0..1] range. We can compress them. */
		attr_id.uv = GPU_vertformat_attr_add(&format_uv, "u", GPU_COMP_I16, 2, GPU_FETCH_INT_TO_FLOAT_UNIT);
	}

	int vbo_len_capacity = curve_render_surface_tri_len_get(lb) * 3;

	GPUVertBufRaw pos_step = {0};
	GPUVertBufRaw nor_step = {0};
	GPUVertBufRaw uv_step = {0};

	if (DRW_TEST_ASSIGN_VBO(vbo_pos_nor)) {
		GPU_vertbuf_init_with_format(vbo_pos_nor, &format_pos_nor);
		GPU_vertbuf_data_alloc(vbo_pos_nor, vbo_len_capacity);
		GPU_vertbuf_attr_get_raw_data(vbo_pos_nor, attr_id.pos, &pos_step);
		GPU_vertbuf_attr_get_raw_data(vbo_pos_nor, attr_id.nor, &nor_step);
	}
	if (DRW_TEST_ASSIGN_VBO(vbo_uv)) {
		GPU_vertbuf_init_with_format(vbo_uv, &format_uv);
		GPU_vertbuf_data_alloc(vbo_uv, vbo_len_capacity);
		GPU_vertbuf_attr_get_raw_data(vbo_uv, attr_id.uv, &uv_step);
	}

	BKE_displist_normals_add(lb);

	for (const DispList *dl = lb->first; dl; dl = dl->next) {
		if (ELEM(dl->type, DL_INDEX3, DL_INDEX4, DL_SURF)) {
			const float(*verts)[3] = (float(*)[3])dl->verts;
			const float(*nors)[3] = (float(*)[3])dl->nors;
			const int *idx = dl->index;
			float uv[4][2];

			if (dl->type == DL_INDEX3) {
				const float x_max = (float)(dl->nr - 1);
				uv[0][1] = uv[1][1] = uv[2][1] = 0.0f;
				const int i_end = dl->parts;
				for (int i = 0; i < i_end; i++, idx += 3) {
					if (vbo_uv) {
						uv[0][0] = idx[0] / x_max;
						uv[1][0] = idx[1] / x_max;
						uv[2][0] = idx[2] / x_max;
					}

					displist_vertbuf_attr_set_tri_pos_nor_uv(
					        &pos_step, &nor_step, &uv_step,
					        verts[idx[0]], verts[idx[2]], verts[idx[1]],
					        dl->nors, dl->nors, dl->nors,
					        uv[0], uv[2], uv[1], false);
				}
			}
			else if (dl->type == DL_SURF) {
				uint quad[4];
				for (int a = 0; a < dl->parts; a++) {
					if ((dl->flag & DL_CYCL_V) == 0 && a == dl->parts - 1) {
						break;
					}

					int b;
					if (dl->flag & DL_CYCL_U) {
						quad[0] = dl->nr * a;
						quad[3] = quad[0] + dl->nr - 1;
						quad[1] = quad[0] + dl->nr;
						quad[2] = quad[3] + dl->nr;
						b = 0;
					}
					else {
						quad[3] = dl->nr * a;
						quad[0] = quad[3] + 1;
						quad[2] = quad[3] + dl->nr;
						quad[1] = quad[0] + dl->nr;
						b = 1;
					}
					if ((dl->flag & DL_CYCL_V) && a == dl->parts - 1) {
						quad[1] -= dl->parts * dl->nr;
						quad[2] -= dl->parts * dl->nr;
					}

					for (; b < dl->nr; b++) {
						if (vbo_uv) {
							surf_uv_quad(dl, quad, uv);
						}

						displist_vertbuf_attr_set_tri_pos_nor_uv(
						        &pos_step, &nor_step, &uv_step,
						        verts[quad[2]], verts[quad[0]], verts[quad[1]],
						        nors[quad[2]], nors[quad[0]], nors[quad[1]],
						        uv[2], uv[0], uv[1], false);

						displist_vertbuf_attr_set_tri_pos_nor_uv(
						        &pos_step, &nor_step, &uv_step,
						        verts[quad[0]], verts[quad[2]], verts[quad[3]],
						        nors[quad[0]], nors[quad[2]], nors[quad[3]],
						        uv[0], uv[2], uv[3], false);

						quad[2] = quad[1];
						quad[1]++;
						quad[3] = quad[0];
						quad[0]++;
					}
				}
			}
			else {
				BLI_assert(dl->type == DL_INDEX4);
				uv[0][0] = uv[0][1] = uv[1][0] = uv[3][1] = 0.0f;
				uv[1][1] = uv[2][0] = uv[2][1] = uv[3][0] = 1.0f;

				const int i_end = dl->parts;
				for (int i = 0; i < i_end; i++, idx += 4) {
					displist_vertbuf_attr_set_tri_pos_nor_uv(
					        &pos_step, &nor_step, &uv_step,
					        verts[idx[0]], verts[idx[2]], verts[idx[1]],
					        nors[idx[0]], nors[idx[2]], nors[idx[1]],
					        uv[0], uv[2], uv[1], true);

					if (idx[2] != idx[3]) {
						displist_vertbuf_attr_set_tri_pos_nor_uv(
						        &pos_step, &nor_step, &uv_step,
						        verts[idx[2]], verts[idx[0]], verts[idx[3]],
						        nors[idx[2]], nors[idx[0]], nors[idx[3]],
						        uv[2], uv[0], uv[3], true);
					}
				}
			}
		}
	}
	/* Resize and finish. */
	if (pos_step.size != 0) {
		int vbo_len_used = GPU_vertbuf_raw_used(&pos_step);
		if (vbo_len_used < vbo_len_capacity) {
			GPU_vertbuf_data_resize(vbo_pos_nor, vbo_len_used);
		}
	}
	if (uv_step.size != 0) {
		int vbo_len_used = GPU_vertbuf_raw_used(&uv_step);
		if (vbo_len_used < vbo_len_capacity) {
			GPU_vertbuf_data_resize(vbo_uv, vbo_len_used);
		}
	}
}
