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

/** \file draw_cache_impl_curve.c
 *  \ingroup draw
 *
 * \brief Curve API for render engines
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_math_vector.h"

#include "DNA_curve_types.h"

#include "BKE_curve.h"

#include "BKE_font.h"

#include "GPU_batch.h"

#include "UI_resources.h"

#include "draw_cache_impl.h"  /* own include */

#define SELECT            1
#define ACTIVE_NURB       1 << 2
#define EVEN_U_BIT        1 << 3 /* Alternate this bit for every U vert. */

/* Used as values of `color_id` in `edit_curve_overlay_handle_geom.glsl` */
enum {
	COLOR_NURB_ULINE_ID = TH_HANDLE_AUTOCLAMP - TH_HANDLE_FREE + 2,

	TOT_HANDLE_COL,
};

/**
 * TODO
 * - Ensure `CurveCache`, `SEQUENCER_DAG_WORKAROUND`.
 * - Check number of verts/edges to see if cache is valid.
 * - Check if 'overlay.edges' can use single attribute per edge, not 2 (for selection drawing).
 */

static void curve_batch_cache_clear(Curve *cu);

/* ---------------------------------------------------------------------- */
/* Curve Interface, direct access to basic data. */

static void curve_render_overlay_verts_edges_len_get(
        ListBase *lb, int *r_vert_len, int *r_edge_len)
{
	BLI_assert(r_vert_len || r_edge_len);
	int vert_len = 0;
	int edge_len = 0;
	for (Nurb *nu = lb->first; nu; nu = nu->next) {
		if (nu->bezt) {
			vert_len += nu->pntsu * 3;
			/* 2x handles per point*/
			edge_len += 2 * nu->pntsu;
		}
		else if (nu->bp) {
			vert_len += nu->pntsu * nu->pntsv;
			/* segments between points */
			edge_len += (nu->pntsu - 1) * nu->pntsv;
			edge_len += (nu->pntsv - 1) * nu->pntsu;
		}
	}
	if (r_vert_len) {
		*r_vert_len = vert_len;
	}
	if (r_edge_len) {
		*r_edge_len = edge_len;
	}
}

static void curve_render_wire_verts_edges_len_get(
        const CurveCache *ob_curve_cache,
        int *r_vert_len, int *r_edge_len)
{
	BLI_assert(r_vert_len || r_edge_len);
	int vert_len = 0;
	int edge_len = 0;
	for (const BevList *bl = ob_curve_cache->bev.first; bl; bl = bl->next) {
		if (bl->nr > 0) {
			const bool is_cyclic = bl->poly != -1;

			/* verts */
			vert_len += bl->nr;

			/* edges */
			edge_len += bl->nr;
			if (!is_cyclic) {
				edge_len -= 1;
			}
		}
	}
	if (r_vert_len) {
		*r_vert_len = vert_len;
	}
	if (r_edge_len) {
		*r_edge_len = edge_len;
	}
}

static int curve_render_normal_len_get(const ListBase *lb, const CurveCache *ob_curve_cache)
{
	int normal_len = 0;
	const BevList *bl;
	const Nurb *nu;
	for (bl = ob_curve_cache->bev.first, nu = lb->first; nu && bl; bl = bl->next, nu = nu->next) {
		int nr = bl->nr;
		int skip = nu->resolu / 16;
#if 0
		while (nr-- > 0) { /* accounts for empty bevel lists */
			normal_len += 1;
			nr -= skip;
		}
#else
		/* Same as loop above */
		normal_len += (nr / (skip + 1)) + ((nr % (skip + 1)) != 0);
#endif
	}
	return normal_len;
}

/* ---------------------------------------------------------------------- */
/* Curve Interface, indirect, partially cached access to complex data. */

typedef struct CurveRenderData {
	int types;

	struct {
		int vert_len;
		int edge_len;
	} overlay;

	struct {
		int vert_len;
		int edge_len;
	} wire;

	/* edit mode normal's */
	struct {
		/* 'edge_len == len * 2'
		 * 'vert_len == len * 3' */
		int len;
	} normal;

	struct {
		EditFont *edit_font;
	} text;

	/* borrow from 'Object' */
	CurveCache *ob_curve_cache;

	/* borrow from 'Curve' */
	ListBase *nurbs;

	/* edit, index in nurb list */
	int actnu;
	/* edit, index in active nurb (BPoint or BezTriple) */
	int actvert;
} CurveRenderData;

enum {
	/* Wire center-line */
	CU_DATATYPE_WIRE        = 1 << 0,
	/* Edit-mode verts and optionally handles */
	CU_DATATYPE_OVERLAY     = 1 << 1,
	/* Edit-mode normals */
	CU_DATATYPE_NORMAL      = 1 << 2,
	/* Geometry */
	CU_DATATYPE_SURFACE     = 1 << 3,
	/* Text */
	CU_DATATYPE_TEXT_SELECT = 1 << 4,
};

/*
 * ob_curve_cache can be NULL, only needed for CU_DATATYPE_WIRE
 */
static CurveRenderData *curve_render_data_create(Curve *cu, CurveCache *ob_curve_cache, const int types)
{
	CurveRenderData *rdata = MEM_callocN(sizeof(*rdata), __func__);
	rdata->types = types;
	ListBase *nurbs;

	rdata->actnu = cu->actnu;
	rdata->actvert = cu->actvert;

	rdata->ob_curve_cache = ob_curve_cache;

	if (types & CU_DATATYPE_WIRE) {
		curve_render_wire_verts_edges_len_get(
		        rdata->ob_curve_cache,
		        &rdata->wire.vert_len, &rdata->wire.edge_len);
	}

	if (cu->editnurb) {
		EditNurb *editnurb = cu->editnurb;
		nurbs = &editnurb->nurbs;

		if (types & CU_DATATYPE_OVERLAY) {
			curve_render_overlay_verts_edges_len_get(
			        nurbs,
			        &rdata->overlay.vert_len,
			        &rdata->overlay.edge_len);

			rdata->actnu = cu->actnu;
			rdata->actvert = cu->actvert;
		}
		if (types & CU_DATATYPE_NORMAL) {
			rdata->normal.len = curve_render_normal_len_get(nurbs, rdata->ob_curve_cache);
		}
	}
	else {
		nurbs = &cu->nurb;
	}

	rdata->nurbs = nurbs;

	rdata->text.edit_font = cu->editfont;

	return rdata;
}

static void curve_render_data_free(CurveRenderData *rdata)
{
#if 0
	if (rdata->loose_verts) {
		MEM_freeN(rdata->loose_verts);
	}
#endif
	MEM_freeN(rdata);
}

static int curve_render_data_overlay_verts_len_get(const CurveRenderData *rdata)
{
	BLI_assert(rdata->types & CU_DATATYPE_OVERLAY);
	return rdata->overlay.vert_len;
}

static int curve_render_data_overlay_edges_len_get(const CurveRenderData *rdata)
{
	BLI_assert(rdata->types & CU_DATATYPE_OVERLAY);
	return rdata->overlay.edge_len;
}

static int curve_render_data_wire_verts_len_get(const CurveRenderData *rdata)
{
	BLI_assert(rdata->types & CU_DATATYPE_WIRE);
	return rdata->wire.vert_len;
}

static int curve_render_data_wire_edges_len_get(const CurveRenderData *rdata)
{
	BLI_assert(rdata->types & CU_DATATYPE_WIRE);
	return rdata->wire.edge_len;
}

static int curve_render_data_normal_len_get(const CurveRenderData *rdata)
{
	BLI_assert(rdata->types & CU_DATATYPE_NORMAL);
	return rdata->normal.len;
}


/* ---------------------------------------------------------------------- */
/* Curve GPUBatch Cache */

typedef struct CurveBatchCache {
	/* center-line */
	struct {
		GPUVertBuf *verts;
		GPUVertBuf *edges;
		GPUBatch *batch;
		GPUIndexBuf *elem;
	} wire;

	/* normals */
	struct {
		GPUVertBuf *verts;
		GPUVertBuf *edges;
		GPUBatch *batch;
		GPUIndexBuf *elem;
	} normal;

	/* control handles and vertices */
	struct {
		GPUBatch *edges;
		GPUBatch *verts;
		GPUBatch *verts_no_handles;
	} overlay;

	struct {
		GPUVertBuf *verts;
		GPUIndexBuf *triangles_in_order;
		GPUBatch **shaded_triangles;
		GPUBatch *batch;
		int mat_len;
	} surface;

	/* 3d text */
	struct {
		GPUBatch *select;
		GPUBatch *cursor;
	} text;

	/* settings to determine if cache is invalid */
	bool is_dirty;

	float normal_size;

	bool is_editmode;
} CurveBatchCache;

/* GPUBatch cache management. */

static bool curve_batch_cache_valid(Curve *cu)
{
	CurveBatchCache *cache = cu->batch_cache;

	if (cache == NULL) {
		return false;
	}

	if (cache->is_dirty) {
		return false;
	}

	if (cache->is_editmode != ((cu->editnurb != NULL) || (cu->editfont != NULL))) {
		return false;
	}

	if (cache->is_editmode) {
		if (cu->editfont) {
			/* TODO */
		}
	}

	return true;
}

static void curve_batch_cache_init(Curve *cu)
{
	CurveBatchCache *cache = cu->batch_cache;

	if (!cache) {
		cache = cu->batch_cache = MEM_callocN(sizeof(*cache), __func__);
	}
	else {
		memset(cache, 0, sizeof(*cache));
	}

#if 0
	ListBase *nurbs;
	if (cu->editnurb) {
		EditNurb *editnurb = cu->editnurb;
		nurbs = &editnurb->nurbs;
	}
	else {
		nurbs = &cu->nurb;
	}
#endif

	cache->is_editmode = (cu->editnurb != NULL) || (cu->editfont != NULL);

	cache->is_dirty = false;
}

static CurveBatchCache *curve_batch_cache_get(Curve *cu)
{
	if (!curve_batch_cache_valid(cu)) {
		curve_batch_cache_clear(cu);
		curve_batch_cache_init(cu);
	}
	return cu->batch_cache;
}

void DRW_curve_batch_cache_dirty_tag(Curve *cu, int mode)
{
	CurveBatchCache *cache = cu->batch_cache;
	if (cache == NULL) {
		return;
	}
	switch (mode) {
		case BKE_CURVE_BATCH_DIRTY_ALL:
			cache->is_dirty = true;
			break;
		case BKE_CURVE_BATCH_DIRTY_SELECT:
			/* editnurb */
			GPU_BATCH_DISCARD_SAFE(cache->overlay.verts_no_handles);
			GPU_BATCH_DISCARD_SAFE(cache->overlay.verts);
			GPU_BATCH_DISCARD_SAFE(cache->overlay.edges);

			/* editfont */
			GPU_BATCH_DISCARD_SAFE(cache->text.select);
			GPU_BATCH_DISCARD_SAFE(cache->text.cursor);
			break;
		default:
			BLI_assert(0);
	}
}

static void curve_batch_cache_clear(Curve *cu)
{
	CurveBatchCache *cache = cu->batch_cache;
	if (!cache) {
		return;
	}

	GPU_BATCH_DISCARD_SAFE(cache->overlay.verts_no_handles);
	GPU_BATCH_DISCARD_SAFE(cache->overlay.verts);
	GPU_BATCH_DISCARD_SAFE(cache->overlay.edges);

	GPU_VERTBUF_DISCARD_SAFE(cache->surface.verts);
	GPU_INDEXBUF_DISCARD_SAFE(cache->surface.triangles_in_order);

	GPU_BATCH_DISCARD_ARRAY_SAFE(cache->surface.shaded_triangles, cache->surface.mat_len);
	GPU_BATCH_DISCARD_SAFE(cache->surface.batch);

	/* don't own vbo & elems */
	GPU_BATCH_DISCARD_SAFE(cache->wire.batch);
	GPU_VERTBUF_DISCARD_SAFE(cache->wire.verts);
	GPU_VERTBUF_DISCARD_SAFE(cache->wire.edges);
	GPU_INDEXBUF_DISCARD_SAFE(cache->wire.elem);

	/* don't own vbo & elems */
	GPU_BATCH_DISCARD_SAFE(cache->normal.batch);
	GPU_VERTBUF_DISCARD_SAFE(cache->normal.verts);
	GPU_VERTBUF_DISCARD_SAFE(cache->normal.edges);
	GPU_INDEXBUF_DISCARD_SAFE(cache->normal.elem);

	/* 3d text */
	GPU_BATCH_DISCARD_SAFE(cache->text.cursor);
	GPU_BATCH_DISCARD_SAFE(cache->text.select);
}

void DRW_curve_batch_cache_free(Curve *cu)
{
	curve_batch_cache_clear(cu);
	MEM_SAFE_FREE(cu->batch_cache);
}

/* -------------------------------------------------------------------- */

/** \name Private Curve Cache API
 * \{ */

/* GPUBatch cache usage. */
static GPUVertBuf *curve_batch_cache_get_wire_verts(CurveRenderData *rdata, CurveBatchCache *cache)
{
	BLI_assert(rdata->types & CU_DATATYPE_WIRE);
	BLI_assert(rdata->ob_curve_cache != NULL);

	if (cache->wire.verts == NULL) {
		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			/* initialize vertex format */
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		}

		const int vert_len = curve_render_data_wire_verts_len_get(rdata);

		GPUVertBuf *vbo = cache->wire.verts = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, vert_len);
		int vbo_len_used = 0;
		for (const BevList *bl = rdata->ob_curve_cache->bev.first; bl; bl = bl->next) {
			if (bl->nr > 0) {
				const int i_end = vbo_len_used + bl->nr;
				for (const BevPoint *bevp = bl->bevpoints; vbo_len_used < i_end; vbo_len_used++, bevp++) {
					GPU_vertbuf_attr_set(vbo, attr_id.pos, vbo_len_used, bevp->vec);
				}
			}
		}
		BLI_assert(vbo_len_used == vert_len);
	}

	return cache->wire.verts;
}

static GPUIndexBuf *curve_batch_cache_get_wire_edges(CurveRenderData *rdata, CurveBatchCache *cache)
{
	BLI_assert(rdata->types & CU_DATATYPE_WIRE);
	BLI_assert(rdata->ob_curve_cache != NULL);

	if (cache->wire.edges == NULL) {
		const int vert_len = curve_render_data_wire_verts_len_get(rdata);
		const int edge_len = curve_render_data_wire_edges_len_get(rdata);
		int edge_len_used = 0;

		GPUIndexBufBuilder elb;
		GPU_indexbuf_init(&elb, GPU_PRIM_LINES, edge_len, vert_len);

		int i = 0;
		for (const BevList *bl = rdata->ob_curve_cache->bev.first; bl; bl = bl->next) {
			if (bl->nr > 0) {
				const bool is_cyclic = bl->poly != -1;
				const int i_end = i + (bl->nr);
				int i_prev;
				if (is_cyclic) {
					i_prev = i + (bl->nr - 1);
				}
				else {
					i_prev = i;
					i += 1;
				}
				for (; i < i_end; i_prev = i++) {
					GPU_indexbuf_add_line_verts(&elb, i_prev, i);
					edge_len_used += 1;
				}
			}
		}
		cache->wire.elem = GPU_indexbuf_build(&elb);
	}

	return cache->wire.elem;
}

static GPUVertBuf *curve_batch_cache_get_normal_verts(CurveRenderData *rdata, CurveBatchCache *cache)
{
	BLI_assert(rdata->types & CU_DATATYPE_NORMAL);
	BLI_assert(rdata->ob_curve_cache != NULL);

	if (cache->normal.verts == NULL) {
		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			/* initialize vertex format */
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		}

		const int normal_len = curve_render_data_normal_len_get(rdata);
		const int vert_len = normal_len * 3;

		GPUVertBuf *vbo = cache->normal.verts = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, vert_len);
		int vbo_len_used = 0;

		const BevList *bl;
		const Nurb *nu;

		for (bl = rdata->ob_curve_cache->bev.first, nu = rdata->nurbs->first;
		     nu && bl;
		     bl = bl->next, nu = nu->next)
		{
			const BevPoint *bevp = bl->bevpoints;
			int nr = bl->nr;
			int skip = nu->resolu / 16;

			while (nr-- > 0) { /* accounts for empty bevel lists */
				const float fac = bevp->radius * cache->normal_size;
				float vec_a[3]; /* Offset perpendicular to the curve */
				float vec_b[3]; /* Delta along the curve */

				vec_a[0] = fac;
				vec_a[1] = 0.0f;
				vec_a[2] = 0.0f;

				mul_qt_v3(bevp->quat, vec_a);
				madd_v3_v3fl(vec_a, bevp->dir, -fac);

				reflect_v3_v3v3(vec_b, vec_a, bevp->dir);
				negate_v3(vec_b);

				add_v3_v3(vec_a, bevp->vec);
				add_v3_v3(vec_b, bevp->vec);

				GPU_vertbuf_attr_set(vbo, attr_id.pos, vbo_len_used++, vec_a);
				GPU_vertbuf_attr_set(vbo, attr_id.pos, vbo_len_used++, bevp->vec);
				GPU_vertbuf_attr_set(vbo, attr_id.pos, vbo_len_used++, vec_b);

				bevp += skip + 1;
				nr -= skip;
			}
		}
		BLI_assert(vbo_len_used == vert_len);
	}

	return cache->normal.verts;
}

static GPUIndexBuf *curve_batch_cache_get_normal_edges(CurveRenderData *rdata, CurveBatchCache *cache)
{
	BLI_assert(rdata->types & CU_DATATYPE_NORMAL);
	BLI_assert(rdata->ob_curve_cache != NULL);

	if (cache->normal.edges == NULL) {
		const int normal_len = curve_render_data_normal_len_get(rdata);
		const int vert_len = normal_len * 3;
		const int edge_len = normal_len * 2;

		GPUIndexBufBuilder elb;
		GPU_indexbuf_init(&elb, GPU_PRIM_LINES, edge_len, vert_len);

		int vbo_len_used = 0;
		for (int i = 0; i < normal_len; i++) {
			GPU_indexbuf_add_line_verts(&elb, vbo_len_used + 0, vbo_len_used + 1);
			GPU_indexbuf_add_line_verts(&elb, vbo_len_used + 1, vbo_len_used + 2);
			vbo_len_used += 3;
		}

		BLI_assert(vbo_len_used == vert_len);

		cache->normal.elem = GPU_indexbuf_build(&elb);
	}

	return cache->normal.elem;
}

static void curve_batch_cache_create_overlay_batches(Curve *cu)
{
	/* Since CU_DATATYPE_OVERLAY is slow to generate, generate them all at once */
	int options = CU_DATATYPE_OVERLAY;

	CurveBatchCache *cache = curve_batch_cache_get(cu);
	CurveRenderData *rdata = curve_render_data_create(cu, NULL, options);

	if (cache->overlay.verts == NULL) {
		static GPUVertFormat format = { 0 };
		static struct { uint pos, data; } attr_id;
		if (format.attr_len == 0) {
			/* initialize vertex format */
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
			attr_id.data = GPU_vertformat_attr_add(&format, "data", GPU_COMP_U8, 1, GPU_FETCH_INT);
		}

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		const int vbo_len_capacity = curve_render_data_overlay_verts_len_get(rdata);
		GPUIndexBufBuilder elb;
		GPU_indexbuf_init(&elb, GPU_PRIM_POINTS, vbo_len_capacity, vbo_len_capacity);
		int vbo_len_used = 0;
		GPU_vertbuf_data_alloc(vbo, vbo_len_capacity);
		int i = 0, nu_id = 0;
		for (Nurb *nu = rdata->nurbs->first; nu; nu = nu->next, nu_id++) {
			const bool is_active_nurb = (nu_id == cu->actnu);
			if (nu->bezt) {
				int a = 0;
				for (const BezTriple *bezt = nu->bezt; a < nu->pntsu; a++, bezt++) {
					if (bezt->hide == false) {
						const bool is_active = (i == rdata->actvert);
						GPU_indexbuf_add_point_vert(&elb, vbo_len_used + 1);
						for (int j = 0; j < 3; j++) {
							char vflag = ((&bezt->f1)[j] & SELECT) ? VFLAG_VERTEX_SELECTED : 0;
							vflag |= (is_active) ? VFLAG_VERTEX_ACTIVE : 0;
							vflag |= (is_active_nurb) ? ACTIVE_NURB : 0;
							/* handle color id */
							char col_id = (&bezt->h1)[j / 2];
							vflag |= col_id << 4; /* << 4 because of EVEN_U_BIT */
							GPU_vertbuf_attr_set(vbo, attr_id.pos, vbo_len_used, bezt->vec[j]);
							GPU_vertbuf_attr_set(vbo, attr_id.data, vbo_len_used, &vflag);
							vbo_len_used += 1;
						}
					}
					i += 1;
				}
			}
			else if (nu->bp) {
				int a = 0;
				int pt_len = nu->pntsu * nu->pntsv;
				for (const BPoint *bp = nu->bp; a < pt_len; a++, bp++) {
					if (bp->hide == false) {
						const bool is_active = (i == rdata->actvert);
						char vflag = (bp->f1 & SELECT) ? VFLAG_VERTEX_SELECTED : 0;
						vflag |= (is_active) ? VFLAG_VERTEX_ACTIVE : 0;
						vflag |= (is_active_nurb) ? ACTIVE_NURB : 0;
						vflag |= (((a % nu->pntsu) % 2) == 0) ? EVEN_U_BIT : 0;
						vflag |= COLOR_NURB_ULINE_ID << 4; /* << 4 because of EVEN_U_BIT */
						GPU_indexbuf_add_point_vert(&elb, vbo_len_used);
						GPU_vertbuf_attr_set(vbo, attr_id.pos, vbo_len_used, bp->vec);
						GPU_vertbuf_attr_set(vbo, attr_id.data, vbo_len_used, &vflag);
						vbo_len_used += 1;
					}
					i += 1;
				}
			}
			i += nu->pntsu;
		}
		if (vbo_len_capacity != vbo_len_used) {
			GPU_vertbuf_data_resize(vbo, vbo_len_used);
		}

		GPUIndexBuf *ibo = GPU_indexbuf_build(&elb);

		cache->overlay.verts = GPU_batch_create_ex(GPU_PRIM_POINTS, vbo, NULL, GPU_BATCH_OWNS_VBO);
		cache->overlay.verts_no_handles = GPU_batch_create_ex(GPU_PRIM_POINTS, vbo, ibo, GPU_BATCH_OWNS_INDEX);
	}

	if (cache->overlay.edges == NULL) {
		GPUVertBuf *vbo = cache->overlay.verts->verts[0];

		const int edge_len =  curve_render_data_overlay_edges_len_get(rdata);
		const int vbo_len_capacity = edge_len * 2;

		GPUIndexBufBuilder elb;
		GPU_indexbuf_init(&elb, GPU_PRIM_LINES, vbo_len_capacity, vbo->vertex_len);

		int curr_index = 0;
		int i = 0;
		for (Nurb *nu = rdata->nurbs->first; nu; nu = nu->next, i++) {
			if (nu->bezt) {
				int a = 0;
				for (const BezTriple *bezt = nu->bezt; a < nu->pntsu; a++, bezt++) {
					if (bezt->hide == false) {
						GPU_indexbuf_add_line_verts(&elb, curr_index + 1, curr_index + 0);
						GPU_indexbuf_add_line_verts(&elb, curr_index + 1, curr_index + 2);
						curr_index += 3;
					}
				}
			}
			else if (nu->bp) {
				int a = 0;
				int next_v_index = curr_index;
				for (const BPoint *bp = nu->bp; a < nu->pntsu; a++, bp++) {
					if (bp->hide == false) {
						next_v_index += 1;
					}
				}

				int pt_len = nu->pntsu * nu->pntsv;
				for (a = 0; a < pt_len; a++) {
					const BPoint *bp_curr = &nu->bp[a];
					const BPoint *bp_next_u = ((a % nu->pntsu) < (nu->pntsu - 1)) ? &nu->bp[a + 1] : NULL;
					const BPoint *bp_next_v = (a < (pt_len - nu->pntsu)) ? &nu->bp[a + nu->pntsu] : NULL;
					if (bp_curr->hide == false) {
						if (bp_next_u && (bp_next_u->hide == false)) {
							GPU_indexbuf_add_line_verts(&elb, curr_index, curr_index + 1);
						}
						if (bp_next_v && (bp_next_v->hide == false)) {
							GPU_indexbuf_add_line_verts(&elb, curr_index, next_v_index);
						}
						curr_index += 1;
					}
					if (bp_next_v && (bp_next_v->hide == false)) {
						next_v_index += 1;
					}
				}
			}
		}

		GPUIndexBuf *ibo = GPU_indexbuf_build(&elb);
		cache->overlay.edges = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, ibo, GPU_BATCH_OWNS_INDEX);
	}

	curve_render_data_free(rdata);
}

static GPUBatch *curve_batch_cache_get_pos_and_normals(CurveRenderData *rdata, CurveBatchCache *cache)
{
	BLI_assert(rdata->types & CU_DATATYPE_SURFACE);
	if (cache->surface.batch == NULL) {
		ListBase *lb = &rdata->ob_curve_cache->disp;

		if (cache->surface.verts == NULL) {
			cache->surface.verts = DRW_displist_vertbuf_calc_pos_with_normals(lb);
		}
		if (cache->surface.triangles_in_order == NULL) {
			cache->surface.triangles_in_order = DRW_displist_indexbuf_calc_triangles_in_order(lb);
		}
		cache->surface.batch = GPU_batch_create(
		        GPU_PRIM_TRIS, cache->surface.verts, cache->surface.triangles_in_order);
	}

	return cache->surface.batch;
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Private Object/Font Cache API
 * \{ */


static GPUBatch *curve_batch_cache_get_overlay_select(CurveRenderData *rdata, CurveBatchCache *cache)
{
	BLI_assert(rdata->types & CU_DATATYPE_TEXT_SELECT);
	if (cache->text.select == NULL) {
		EditFont *ef = rdata->text.edit_font;
		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		}

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		const int vbo_len_capacity = ef->selboxes_len * 6;
		int vbo_len_used = 0;
		GPU_vertbuf_data_alloc(vbo, vbo_len_capacity);

		float box[4][3];

		/* fill in xy below */
		box[0][2] = box[1][2] = box[2][2] = box[3][2] = 0.001;

		for (int i = 0; i < ef->selboxes_len; i++) {
			EditFontSelBox *sb = &ef->selboxes[i];

			float selboxw;
			if (i + 1 != ef->selboxes_len) {
				if (ef->selboxes[i + 1].y == sb->y)
					selboxw = ef->selboxes[i + 1].x - sb->x;
				else
					selboxw = sb->w;
			}
			else {
				selboxw = sb->w;
			}

			if (sb->rot == 0.0f) {
				copy_v2_fl2(box[0], sb->x, sb->y);
				copy_v2_fl2(box[1], sb->x + selboxw, sb->y);
				copy_v2_fl2(box[2], sb->x + selboxw, sb->y + sb->h);
				copy_v2_fl2(box[3], sb->x, sb->y + sb->h);
			}
			else {
				float mat[2][2];

				angle_to_mat2(mat, sb->rot);

				copy_v2_fl2(box[0], sb->x, sb->y);

				copy_v2_fl2(box[1], selboxw, 0.0f);
				mul_m2v2(mat, box[1]);
				add_v2_v2(box[1], &sb->x);

				copy_v2_fl2(box[2], selboxw, sb->h);
				mul_m2v2(mat, box[2]);
				add_v2_v2(box[2], &sb->x);

				copy_v2_fl2(box[3], 0.0f, sb->h);
				mul_m2v2(mat, box[3]);
				add_v2_v2(box[3], &sb->x);
			}

			GPU_vertbuf_attr_set(vbo, attr_id.pos, vbo_len_used++, box[0]);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, vbo_len_used++, box[1]);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, vbo_len_used++, box[2]);

			GPU_vertbuf_attr_set(vbo, attr_id.pos, vbo_len_used++, box[0]);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, vbo_len_used++, box[2]);
			GPU_vertbuf_attr_set(vbo, attr_id.pos, vbo_len_used++, box[3]);
		}
		BLI_assert(vbo_len_used == vbo_len_capacity);
		cache->text.select = GPU_batch_create_ex(GPU_PRIM_TRIS, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return cache->text.select;
}

static GPUBatch *curve_batch_cache_get_overlay_cursor(CurveRenderData *rdata, CurveBatchCache *cache)
{
	BLI_assert(rdata->types & CU_DATATYPE_TEXT_SELECT);
	if (cache->text.cursor == NULL) {
		static GPUVertFormat format = { 0 };
		static struct { uint pos; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
		}

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		const int vbo_len_capacity = 4;
		GPU_vertbuf_data_alloc(vbo, vbo_len_capacity);
		for (int i = 0; i < 4; i++) {
			GPU_vertbuf_attr_set(vbo, attr_id.pos, i, rdata->text.edit_font->textcurs[i]);
		}
		cache->text.cursor = GPU_batch_create_ex(GPU_PRIM_TRI_FAN, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return cache->text.cursor;
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Public Object/Curve API
 * \{ */

GPUBatch *DRW_curve_batch_cache_get_wire_edge(Curve *cu, CurveCache *ob_curve_cache)
{
	CurveBatchCache *cache = curve_batch_cache_get(cu);

	if (cache->wire.batch == NULL) {
		/* create batch from Curve */
		CurveRenderData *rdata = curve_render_data_create(cu, ob_curve_cache, CU_DATATYPE_WIRE);

		cache->wire.batch = GPU_batch_create(
		        GPU_PRIM_LINES,
		        curve_batch_cache_get_wire_verts(rdata, cache),
		        curve_batch_cache_get_wire_edges(rdata, cache));

		curve_render_data_free(rdata);
	}
	return cache->wire.batch;
}

GPUBatch *DRW_curve_batch_cache_get_normal_edge(Curve *cu, CurveCache *ob_curve_cache, float normal_size)
{
	CurveBatchCache *cache = curve_batch_cache_get(cu);

	if (cache->normal.batch != NULL) {
		cache->normal_size = normal_size;
		if (cache->normal_size != normal_size) {
			GPU_BATCH_DISCARD_SAFE(cache->normal.batch);
			GPU_VERTBUF_DISCARD_SAFE(cache->normal.edges);
		}
	}
	cache->normal_size = normal_size;

	if (cache->normal.batch == NULL) {
		/* create batch from Curve */
		CurveRenderData *rdata = curve_render_data_create(cu, ob_curve_cache, CU_DATATYPE_NORMAL);

		cache->normal.batch = GPU_batch_create(
		        GPU_PRIM_LINES,
		        curve_batch_cache_get_normal_verts(rdata, cache),
		        curve_batch_cache_get_normal_edges(rdata, cache));

		curve_render_data_free(rdata);
		cache->normal_size = normal_size;
	}
	return cache->normal.batch;
}

GPUBatch *DRW_curve_batch_cache_get_overlay_edges(Curve *cu)
{
	CurveBatchCache *cache = curve_batch_cache_get(cu);

	if (cache->overlay.edges == NULL) {
		curve_batch_cache_create_overlay_batches(cu);
	}

	return cache->overlay.edges;
}

GPUBatch *DRW_curve_batch_cache_get_overlay_verts(Curve *cu, bool handles)
{
	CurveBatchCache *cache = curve_batch_cache_get(cu);

	if (cache->overlay.verts == NULL || cache->overlay.verts_no_handles == NULL) {
		curve_batch_cache_create_overlay_batches(cu);
	}

	return (handles) ? cache->overlay.verts : cache->overlay.verts_no_handles;
}

GPUBatch *DRW_curve_batch_cache_get_triangles_with_normals(
        struct Curve *cu, struct CurveCache *ob_curve_cache)
{
	CurveBatchCache *cache = curve_batch_cache_get(cu);

	if (cache->surface.batch == NULL) {
		CurveRenderData *rdata = curve_render_data_create(cu, ob_curve_cache, CU_DATATYPE_SURFACE);

		curve_batch_cache_get_pos_and_normals(rdata, cache);

		curve_render_data_free(rdata);
	}

	return cache->surface.batch;
}

GPUBatch **DRW_curve_batch_cache_get_surface_shaded(
        struct Curve *cu, struct CurveCache *ob_curve_cache,
        struct GPUMaterial **UNUSED(gpumat_array), uint gpumat_array_len)
{
	CurveBatchCache *cache = curve_batch_cache_get(cu);

	if (cache->surface.mat_len != gpumat_array_len) {
		GPU_BATCH_DISCARD_ARRAY_SAFE(cache->surface.shaded_triangles, cache->surface.mat_len);
	}

	if (cache->surface.shaded_triangles == NULL) {
		CurveRenderData *rdata = curve_render_data_create(cu, ob_curve_cache, CU_DATATYPE_SURFACE);
		ListBase *lb = &rdata->ob_curve_cache->disp;

		cache->surface.mat_len = gpumat_array_len;
		if (cu->flag & CU_UV_ORCO) {
			cache->surface.shaded_triangles = DRW_displist_batch_calc_tri_pos_normals_and_uv_split_by_material(
			        lb, gpumat_array_len);
		}
		else {
			cache->surface.shaded_triangles = MEM_mallocN(
			        sizeof(*cache->surface.shaded_triangles) * gpumat_array_len, __func__);
			GPUIndexBuf **el = DRW_displist_indexbuf_calc_triangles_in_order_split_by_material(
			        lb, gpumat_array_len);

			if (cache->surface.verts == NULL) {
				cache->surface.verts = DRW_displist_vertbuf_calc_pos_with_normals(lb);
			}

			for (int i = 0; i < gpumat_array_len; ++i) {
				cache->surface.shaded_triangles[i] = GPU_batch_create_ex(
				        GPU_PRIM_TRIS, cache->surface.verts, el[i], GPU_BATCH_OWNS_INDEX);
			}

			MEM_freeN(el); /* Save `el` in cache? */
		}

		curve_render_data_free(rdata);
	}

	return cache->surface.shaded_triangles;
}


/* -------------------------------------------------------------------- */

/** \name Public Object/Font API
 * \{ */

GPUBatch *DRW_curve_batch_cache_get_overlay_select(Curve *cu)
{
	CurveBatchCache *cache = curve_batch_cache_get(cu);

	if (cache->text.select == NULL) {
		CurveRenderData *rdata = curve_render_data_create(cu, NULL, CU_DATATYPE_TEXT_SELECT);

		curve_batch_cache_get_overlay_select(rdata, cache);

		curve_render_data_free(rdata);
	}

	return cache->text.select;
}

GPUBatch *DRW_curve_batch_cache_get_overlay_cursor(Curve *cu)
{
	CurveBatchCache *cache = curve_batch_cache_get(cu);

	if (cache->text.cursor == NULL) {
		CurveRenderData *rdata = curve_render_data_create(cu, NULL, CU_DATATYPE_TEXT_SELECT);

		curve_batch_cache_get_overlay_cursor(rdata, cache);

		curve_render_data_free(rdata);
	}

	return cache->text.cursor;
}

/** \} */
