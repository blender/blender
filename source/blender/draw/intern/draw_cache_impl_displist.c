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

static void displist_indexbufbuilder_set(Gwn_IndexBufBuilder *elb, const DispList *dl, const int ofs)
{
	if (ELEM(dl->type, DL_INDEX3, DL_INDEX4, DL_SURF)) {
		const int *idx = dl->index;
		if (dl->type == DL_INDEX3) {
			const int i_end = dl->parts;
			for (int i = 0; i < i_end; i++, idx += 3) {
				GWN_indexbuf_add_tri_verts(elb, idx[0] + ofs, idx[2] + ofs, idx[1] + ofs);
			}
		}
		else if (dl->type == DL_SURF) {
			const int i_end = dl->totindex;
			for (int i = 0; i < i_end; i++, idx += 4) {
				GWN_indexbuf_add_tri_verts(elb, idx[0] + ofs, idx[2] + ofs, idx[1] + ofs);
				GWN_indexbuf_add_tri_verts(elb, idx[0] + ofs, idx[3] + ofs, idx[2] + ofs);
			}
		}
		else {
			BLI_assert(dl->type == DL_INDEX4);
			const int i_end = dl->parts;
			for (int i = 0; i < i_end; i++, idx += 4) {
				GWN_indexbuf_add_tri_verts(elb, idx[0] + ofs, idx[1] + ofs, idx[2] + ofs);

				if (idx[2] != idx[3]) {
					GWN_indexbuf_add_tri_verts(elb, idx[0] + ofs, idx[2] + ofs, idx[3] + ofs);
				}
			}
		}
	}
}

Gwn_VertBuf *DRW_displist_vertbuf_calc_pos_with_normals(ListBase *lb)
{
	static Gwn_VertFormat format = { 0 };
	static struct { uint pos, nor; } attr_id;
	if (format.attrib_ct == 0) {
		/* initialize vertex format */
		attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
		attr_id.nor = GWN_vertformat_attr_add(&format, "nor", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
	}

	Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
	GWN_vertbuf_data_alloc(vbo, curve_render_surface_vert_len_get(lb));

	BKE_displist_normals_add(lb);

	int vbo_len_used = 0;
	for (const DispList *dl = lb->first; dl; dl = dl->next) {
		const bool ndata_is_single = dl->type == DL_INDEX3;
		if (ELEM(dl->type, DL_INDEX3, DL_INDEX4, DL_SURF)) {
			const float *fp_co = dl->verts;
			const float *fp_no = dl->nors;
			const int vbo_end = vbo_len_used + dl_vert_len(dl);
			while (vbo_len_used < vbo_end) {
				GWN_vertbuf_attr_set(vbo, attr_id.pos, vbo_len_used, fp_co);
				if (fp_no) {
					GWN_vertbuf_attr_set(vbo, attr_id.nor, vbo_len_used, fp_no);
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

Gwn_IndexBuf *DRW_displist_indexbuf_calc_triangles_in_order(ListBase *lb)
{
	const int tri_len = curve_render_surface_tri_len_get(lb);
	const int vert_len = curve_render_surface_vert_len_get(lb);

	Gwn_IndexBufBuilder elb;
	GWN_indexbuf_init(&elb, GWN_PRIM_TRIS, tri_len, vert_len);

	int ofs = 0;
	for (const DispList *dl = lb->first; dl; dl = dl->next) {
		displist_indexbufbuilder_set(&elb, dl, ofs);
		ofs += dl_vert_len(dl);
	}

	return GWN_indexbuf_build(&elb);
}

Gwn_IndexBuf **DRW_displist_indexbuf_calc_triangles_in_order_split_by_material(ListBase *lb, uint gpumat_array_len)
{
	Gwn_IndexBuf **shaded_triangles_in_order = MEM_callocN(sizeof(*shaded_triangles_in_order) * gpumat_array_len, __func__);
	Gwn_IndexBufBuilder *elb = BLI_array_alloca(elb, gpumat_array_len);

	const int tri_len = curve_render_surface_tri_len_get(lb);
	const int vert_len = curve_render_surface_vert_len_get(lb);
	int i;

	/* Init each index buffer builder */
	for (i = 0; i < gpumat_array_len; i++) {
		GWN_indexbuf_init(&elb[i], GWN_PRIM_TRIS, tri_len, vert_len);
	}

	/* calc each index buffer builder */
	int ofs = 0;
	for (const DispList *dl = lb->first; dl; dl = dl->next) {
		displist_indexbufbuilder_set(&elb[dl->col], dl, ofs);
		ofs += dl_vert_len(dl);
	}

	/* build each indexbuf */
	for (i = 0; i < gpumat_array_len; i++) {
		shaded_triangles_in_order[i] = GWN_indexbuf_build(&elb[i]);
	}

	return shaded_triangles_in_order;
}
