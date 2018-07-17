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

static void displist_indexbufbuilder_set(GPUIndexBufBuilder *elb, const DispList *dl, const int ofs)
{
	if (ELEM(dl->type, DL_INDEX3, DL_INDEX4, DL_SURF)) {
		const int *idx = dl->index;
		if (dl->type == DL_INDEX3) {
			const int i_end = dl->parts;
			for (int i = 0; i < i_end; i++, idx += 3) {
				GPU_indexbuf_add_tri_verts(elb, idx[0] + ofs, idx[2] + ofs, idx[1] + ofs);
			}
		}
		else if (dl->type == DL_SURF) {
			const int i_end = dl->totindex;
			for (int i = 0; i < i_end; i++, idx += 4) {
				GPU_indexbuf_add_tri_verts(elb, idx[0] + ofs, idx[2] + ofs, idx[1] + ofs);
				GPU_indexbuf_add_tri_verts(elb, idx[0] + ofs, idx[3] + ofs, idx[2] + ofs);
			}
		}
		else {
			BLI_assert(dl->type == DL_INDEX4);
			const int i_end = dl->parts;
			for (int i = 0; i < i_end; i++, idx += 4) {
				GPU_indexbuf_add_tri_verts(elb, idx[0] + ofs, idx[1] + ofs, idx[2] + ofs);

				if (idx[2] != idx[3]) {
					GPU_indexbuf_add_tri_verts(elb, idx[0] + ofs, idx[2] + ofs, idx[3] + ofs);
				}
			}
		}
	}
}

GPUVertBuf *DRW_displist_vertbuf_calc_pos_with_normals(ListBase *lb)
{
	static GPUVertFormat format = { 0 };
	static struct { uint pos, nor; } attr_id;
	if (format.attr_len == 0) {
		/* initialize vertex format */
		attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		attr_id.nor = GPU_vertformat_attr_add(&format, "nor", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
	}

	GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
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
					GPU_vertbuf_attr_set(vbo, attr_id.nor, vbo_len_used, fp_no);
					if (ndata_is_single == false) {
						fp_no += 3;
					}
				}
				fp_co += 3;
				vbo_len_used += 1;
			}
		}
	}

	return vbo;
}

GPUIndexBuf *DRW_displist_indexbuf_calc_triangles_in_order(ListBase *lb)
{
	const int tri_len = curve_render_surface_tri_len_get(lb);
	const int vert_len = curve_render_surface_vert_len_get(lb);

	GPUIndexBufBuilder elb;
	GPU_indexbuf_init(&elb, GPU_PRIM_TRIS, tri_len, vert_len);

	int ofs = 0;
	for (const DispList *dl = lb->first; dl; dl = dl->next) {
		displist_indexbufbuilder_set(&elb, dl, ofs);
		ofs += dl_vert_len(dl);
	}

	return GPU_indexbuf_build(&elb);
}

GPUIndexBuf **DRW_displist_indexbuf_calc_triangles_in_order_split_by_material(ListBase *lb, uint gpumat_array_len)
{
	GPUIndexBuf **shaded_triangles_in_order = MEM_callocN(
	        sizeof(*shaded_triangles_in_order) * gpumat_array_len, __func__);
	GPUIndexBufBuilder *elb = BLI_array_alloca(elb, gpumat_array_len);

	const int tri_len = curve_render_surface_tri_len_get(lb);
	const int vert_len = curve_render_surface_vert_len_get(lb);
	int i;

	/* Init each index buffer builder */
	for (i = 0; i < gpumat_array_len; i++) {
		GPU_indexbuf_init(&elb[i], GPU_PRIM_TRIS, tri_len, vert_len);
	}

	/* calc each index buffer builder */
	int ofs = 0;
	for (const DispList *dl = lb->first; dl; dl = dl->next) {
		displist_indexbufbuilder_set(&elb[dl->col], dl, ofs);
		ofs += dl_vert_len(dl);
	}

	/* build each indexbuf */
	for (i = 0; i < gpumat_array_len; i++) {
		shaded_triangles_in_order[i] = GPU_indexbuf_build(&elb[i]);
	}

	return shaded_triangles_in_order;
}

static void displist_vertbuf_attr_set_tri_pos_normals_and_uv(
        GPUVertBufRaw *pos_step, GPUVertBufRaw *nor_step, GPUVertBufRaw *uv_step,
        const float v1[3], const float v2[3], const float v3[3],
        const float n1[3], const float n2[3], const float n3[3],
        const float uv1[2], const float uv2[2], const float uv3[2])
{
	copy_v3_v3(GPU_vertbuf_raw_step(pos_step), v1);
	copy_v3_v3(GPU_vertbuf_raw_step(nor_step), n1);
	copy_v2_v2(GPU_vertbuf_raw_step(uv_step), uv1);

	copy_v3_v3(GPU_vertbuf_raw_step(pos_step), v2);
	copy_v3_v3(GPU_vertbuf_raw_step(nor_step), n2);
	copy_v2_v2(GPU_vertbuf_raw_step(uv_step), uv2);

	copy_v3_v3(GPU_vertbuf_raw_step(pos_step), v3);
	copy_v3_v3(GPU_vertbuf_raw_step(nor_step), n3);
	copy_v2_v2(GPU_vertbuf_raw_step(uv_step), uv3);
}

GPUBatch **DRW_displist_batch_calc_tri_pos_normals_and_uv_split_by_material(ListBase *lb, uint gpumat_array_len)
{
	static GPUVertFormat shaded_triangles_format = { 0 };
	static struct { uint pos, nor, uv; } attr_id;

	if (shaded_triangles_format.attr_len == 0) {
		/* initialize vertex format */
		attr_id.pos = GPU_vertformat_attr_add(&shaded_triangles_format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		attr_id.nor = GPU_vertformat_attr_add(&shaded_triangles_format, "nor", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		attr_id.uv = GPU_vertformat_attr_add(&shaded_triangles_format, "u", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
	}

	GPUBatch **shaded_triangles = MEM_mallocN(sizeof(*shaded_triangles) * gpumat_array_len, __func__);

	GPUVertBuf **vbo = BLI_array_alloca(vbo, gpumat_array_len);
	uint *vbo_len_capacity = BLI_array_alloca(vbo_len_capacity, gpumat_array_len);

	GPUVertBufRaw *pos_step, *nor_step, *uv_step;
	pos_step = BLI_array_alloca(pos_step, gpumat_array_len);
	nor_step = BLI_array_alloca(nor_step, gpumat_array_len);
	uv_step = BLI_array_alloca(uv_step, gpumat_array_len);

	/* Create each vertex buffer */
	for (int i = 0; i < gpumat_array_len; i++) {
		vbo[i] = GPU_vertbuf_create_with_format(&shaded_triangles_format);
		vbo_len_capacity[i] = 0;
	}

	/* Calc `vbo_len_capacity` */
	for (const DispList *dl = lb->first; dl; dl = dl->next) {
		vbo_len_capacity[dl->col] += dl_tri_len(dl) * 3;
	}

	/* Alloc each vertex buffer and get each raw data */
	for (int i = 0; i < gpumat_array_len; i++) {
		GPU_vertbuf_data_alloc(vbo[i], vbo_len_capacity[i]);
		GPU_vertbuf_attr_get_raw_data(vbo[i], attr_id.pos, &pos_step[i]);
		GPU_vertbuf_attr_get_raw_data(vbo[i], attr_id.nor, &nor_step[i]);
		GPU_vertbuf_attr_get_raw_data(vbo[i], attr_id.uv, &uv_step[i]);
	}

	BKE_displist_normals_add(lb);

	for (const DispList *dl = lb->first; dl; dl = dl->next) {
		if (ELEM(dl->type, DL_INDEX3, DL_INDEX4, DL_SURF)) {
			const int col = dl->col;
			const float(*verts)[3] = (float(*)[3])dl->verts;
			const float(*nors)[3] = (float(*)[3])dl->nors;
			const int *idx = dl->index;
			float uv[4][2];

			if (dl->type == DL_INDEX3) {
				const float x_max = (float)(dl->nr - 1);
				uv[0][1] = uv[1][1] = uv[2][1] = 0.0f;
				const int i_end = dl->parts;
				for (int i = 0; i < i_end; i++, idx += 3) {
					uv[0][0] = idx[0] / x_max;
					uv[1][0] = idx[2] / x_max;
					uv[2][0] = idx[1] / x_max;

					displist_vertbuf_attr_set_tri_pos_normals_and_uv(
					        &pos_step[col], &nor_step[col], &uv_step[col],
					        verts[idx[0]], verts[idx[2]], verts[idx[1]],
					        dl->nors, dl->nors, dl->nors,
					        uv[0], uv[1], uv[2]);
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
							uv[i][0] = (quad[i] / dl->nr) / (float)orco_sizev;
							uv[i][1] = (quad[i] % dl->nr) / (float)orco_sizeu;

							/* cyclic correction */
							if ((i == 1 || i == 2) && uv[i][0] == 0.0f) {
								uv[i][0] = 1.0f;
							}
							if ((i == 0 || i == 1) && uv[i][1] == 0.0f) {
								uv[i][1] = 1.0f;
							}
						}

						displist_vertbuf_attr_set_tri_pos_normals_and_uv(
						        &pos_step[col], &nor_step[col], &uv_step[col],
						        verts[quad[0]], verts[quad[1]], verts[quad[2]],
						        nors[quad[0]], nors[quad[1]], nors[quad[2]],
						        uv[0], uv[1], uv[2]);

						displist_vertbuf_attr_set_tri_pos_normals_and_uv(
						        &pos_step[col], &nor_step[col], &uv_step[col],
						        verts[quad[0]], verts[quad[2]], verts[quad[3]],
						        nors[quad[0]], nors[quad[2]], nors[quad[3]],
						        uv[0], uv[2], uv[3]);

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
					displist_vertbuf_attr_set_tri_pos_normals_and_uv(
					        &pos_step[col], &nor_step[col], &uv_step[col],
					        verts[idx[0]], verts[idx[1]], verts[idx[2]],
					        nors[idx[0]], nors[idx[1]], nors[idx[2]],
					        uv[0], uv[1], uv[2]);

					if (idx[2] != idx[3]) {
						displist_vertbuf_attr_set_tri_pos_normals_and_uv(
						        &pos_step[col], &nor_step[col], &uv_step[col],
						        verts[idx[0]], verts[idx[2]], verts[idx[3]],
						        nors[idx[0]], nors[idx[2]], nors[idx[3]],
						        uv[0], uv[2], uv[3]);
					}
				}
			}
		}
	}

	for (int i = 0; i < gpumat_array_len; i++) {
		uint vbo_len_used = GPU_vertbuf_raw_used(&pos_step[i]);
		if (vbo_len_capacity[i] != vbo_len_used) {
			GPU_vertbuf_data_resize(vbo[i], vbo_len_used);
		}
		shaded_triangles[i] = GPU_batch_create_ex(GPU_PRIM_TRIS, vbo[i], NULL, GPU_BATCH_OWNS_VBO);
	}

	return shaded_triangles;
}
