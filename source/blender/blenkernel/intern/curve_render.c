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

/** \file blender/blenkernel/intern/curve_render.c
 *  \ingroup bke
 *
 * \brief Curve API for render engines
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_math_vector.h"

#include "DNA_curve_types.h"

#include "BKE_curve.h"
#include "BKE_curve_render.h"

#include "GPU_batch.h"

#define SELECT   1

/**
 * TODO
 * - Ensure `CurveCache`, `SEQUENCER_DAG_WORKAROUND`.
 * - Check number of verts/edges to see if cache is valid.
 * - Check if 'overlay_edges' can use single attribyte per edge, not 2 (for selection drawing).
 */

/* ---------------------------------------------------------------------- */
/* Curve Interface, direct access to basic data. */

static void curve_render_overlay_verts_edges_len_get(
        ListBase *lb, bool hide_handles,
        int *r_vert_len, int *r_edge_len)
{
	BLI_assert(r_vert_len || r_edge_len);
	int vert_len = 0;
	int edge_len = 0;
	for (Nurb *nu = lb->first; nu; nu = nu->next) {
		if (nu->bezt) {
			vert_len += hide_handles ? nu->pntsu : (nu->pntsu * 3);
			/* 2x handles per point*/
			edge_len += 2 * nu->pntsu;
		}
		else if (nu->bp) {
			vert_len += nu->pntsu;
			/* segments between points */
			edge_len += nu->pntsu - 1;
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

	bool hide_handles;

	/* borrow from 'Object' */
	CurveCache *ob_curve_cache;

	/* borrow from 'Curve' */
	struct EditNurb *edit_latt;
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
};

/*
 * ob_curve_cache can be NULL, only needed for CU_DATATYPE_WIRE
 */
static CurveRenderData *curve_render_data_create(Curve *cu, CurveCache *ob_curve_cache, const int types)
{
	CurveRenderData *lrdata = MEM_callocN(sizeof(*lrdata), __func__);
	lrdata->types = types;
	ListBase *nurbs;

	lrdata->hide_handles = (cu->drawflag & CU_HIDE_HANDLES) != 0;
	lrdata->actnu = cu->actnu;
	lrdata->actvert = cu->actvert;

	lrdata->ob_curve_cache = ob_curve_cache;

	if (types & CU_DATATYPE_WIRE) {
		curve_render_wire_verts_edges_len_get(
		        lrdata->ob_curve_cache,
		        &lrdata->wire.vert_len, &lrdata->wire.edge_len);
	}

	if (cu->editnurb) {
		EditNurb *editnurb = cu->editnurb;
		nurbs = &editnurb->nurbs;

		lrdata->edit_latt = editnurb;

		if (types & CU_DATATYPE_OVERLAY) {
			curve_render_overlay_verts_edges_len_get(
			        nurbs, lrdata->hide_handles,
			        &lrdata->overlay.vert_len,
			        lrdata->hide_handles ? NULL : &lrdata->overlay.edge_len);

			lrdata->actnu = cu->actnu;
			lrdata->actvert = cu->actvert;
		}
	}
	else {
		nurbs = &cu->nurb;
	}

	lrdata->nurbs = nurbs;

	return lrdata;
}

static void curve_render_data_free(CurveRenderData *lrdata)
{
#if 0
	if (lrdata->loose_verts) {
		MEM_freeN(lrdata->loose_verts);
	}
#endif
	MEM_freeN(lrdata);
}

static int curve_render_data_overlay_verts_len_get(const CurveRenderData *lrdata)
{
	BLI_assert(lrdata->types & CU_DATATYPE_OVERLAY);
	return lrdata->overlay.vert_len;
}

static int curve_render_data_overlay_edges_len_get(const CurveRenderData *lrdata)
{
	BLI_assert(lrdata->types & CU_DATATYPE_OVERLAY);
	return lrdata->overlay.edge_len;
}

static int curve_render_data_wire_verts_len_get(const CurveRenderData *lrdata)
{
	BLI_assert(lrdata->types & CU_DATATYPE_WIRE);
	return lrdata->wire.vert_len;
}

static int curve_render_data_wire_edges_len_get(const CurveRenderData *lrdata)
{
	BLI_assert(lrdata->types & CU_DATATYPE_WIRE);
	return lrdata->wire.edge_len;
}

#if 0
static const BPoint *curve_render_data_vert_bpoint(const CurveRenderData *lrdata, const int vert_idx)
{
	BLI_assert(lrdata->types & CU_DATATYPE_VERT);
	return &lrdata->bp[vert_idx];
}
#endif

enum {
	VFLAG_VERTEX_SELECTED = 1 << 0,
	VFLAG_VERTEX_ACTIVE   = 1 << 1,
};

/* ---------------------------------------------------------------------- */
/* Curve Batch Cache */

typedef struct CurveBatchCache {
	VertexBuffer *wire_verts;
	VertexBuffer *wire_edges;
	Batch *wire_batch;

	ElementList *wire_elem;

	/* control handles and vertices */
	Batch *overlay_edges;
	Batch *overlay_verts;

	/* settings to determine if cache is invalid */
	bool is_dirty;

	bool hide_handles;

	bool is_editmode;
} CurveBatchCache;

/* Batch cache management. */

static bool curve_batch_cache_valid(Curve *cu)
{
	CurveBatchCache *cache = cu->batch_cache;

	if (cache == NULL) {
		return false;
	}

	if (cache->is_editmode != (cu->editnurb != NULL)) {
		return false;
	}

	if (cache->is_dirty == false) {
		return true;
	}
	else {
		/* TODO: check number of vertices/edges? */
		if (cache->is_editmode) {
			return false;
		}
		else if ((cache->hide_handles != ((cu->drawflag & CU_HIDE_HANDLES) != 0))) {
			return false;
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

	cache->hide_handles = (cu->flag & CU_HIDE_HANDLES) != 0;

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

	cache->is_editmode = cu->editnurb != NULL;

	cache->is_dirty = false;
}

static CurveBatchCache *curve_batch_cache_get(Curve *cu)
{
	if (!curve_batch_cache_valid(cu)) {
		BKE_curve_batch_cache_clear(cu);
		curve_batch_cache_init(cu);
	}
	return cu->batch_cache;
}

void BKE_curve_batch_cache_dirty(Curve *cu)
{
	CurveBatchCache *cache = cu->batch_cache;
	if (cache) {
		cache->is_dirty = true;
	}
}

void BKE_curve_batch_selection_dirty(Curve *cu)
{
	CurveBatchCache *cache = cu->batch_cache;
	if (cache) {
		BATCH_DISCARD_ALL_SAFE(cache->overlay_verts);
		BATCH_DISCARD_ALL_SAFE(cache->overlay_edges);
	}
}

void BKE_curve_batch_cache_clear(Curve *cu)
{
	CurveBatchCache *cache = cu->batch_cache;
	if (!cache) {
		return;
	}

	BATCH_DISCARD_ALL_SAFE(cache->overlay_verts);
	BATCH_DISCARD_ALL_SAFE(cache->overlay_edges);

	if (cache->wire_batch) {
		/* Also handles: 'cache->wire_verts', 'cache->wire_edges', 'cache->wire_elem' */
		BATCH_DISCARD_ALL_SAFE(cache->wire_batch);
	}
	else {
		VERTEXBUFFER_DISCARD_SAFE(cache->wire_verts);
		VERTEXBUFFER_DISCARD_SAFE(cache->wire_edges);
		ELEMENTLIST_DISCARD_SAFE(cache->wire_elem);
	}
}

void BKE_curve_batch_cache_free(Curve *cu)
{
	BKE_curve_batch_cache_clear(cu);
	MEM_SAFE_FREE(cu->batch_cache);
}

/* Batch cache usage. */
static VertexBuffer *curve_batch_cache_get_wire_verts(CurveRenderData *lrdata, CurveBatchCache *cache)
{
	BLI_assert(lrdata->types & CU_DATATYPE_WIRE);
	BLI_assert(lrdata->ob_curve_cache != NULL);

	if (cache->wire_verts == NULL) {
		static VertexFormat format = { 0 };
		static unsigned pos_id;
		if (format.attrib_ct == 0) {
			/* initialize vertex format */
			pos_id = VertexFormat_add_attrib(&format, "pos", COMP_F32, 3, KEEP_FLOAT);
		}

		const int vert_len = curve_render_data_wire_verts_len_get(lrdata);

		VertexBuffer *vbo = cache->wire_verts = VertexBuffer_create_with_format(&format);
		VertexBuffer_allocate_data(vbo, vert_len);
		int vbo_len_used = 0;
		for (const BevList *bl = lrdata->ob_curve_cache->bev.first; bl; bl = bl->next) {
			if (bl->nr > 0) {
				const int i_end = vbo_len_used + bl->nr;
				for (const BevPoint *bevp = bl->bevpoints; vbo_len_used < i_end; vbo_len_used++, bevp++) {
					VertexBuffer_set_attrib(vbo, pos_id, vbo_len_used, bevp->vec);
				}
			}
		}
		BLI_assert(vbo_len_used == vert_len);
	}

	return cache->wire_verts;
}

static ElementList *curve_batch_cache_get_wire_edges(CurveRenderData *lrdata, CurveBatchCache *cache)
{
	BLI_assert(lrdata->types & CU_DATATYPE_WIRE);
	BLI_assert(lrdata->ob_curve_cache != NULL);

	if (cache->wire_edges == NULL) {
		const int vert_len = curve_render_data_wire_verts_len_get(lrdata);
		const int edge_len = curve_render_data_wire_edges_len_get(lrdata);
		int edge_len_real = 0;

		ElementListBuilder elb;
		ElementListBuilder_init(&elb, PRIM_LINES, edge_len, vert_len);

		int i = 0;
		for (const BevList *bl = lrdata->ob_curve_cache->bev.first; bl; bl = bl->next) {
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
					add_line_vertices(&elb, i_prev, i);
					edge_len_real += 1;
				}
			}
		}

		if (lrdata->hide_handles) {
			BLI_assert(edge_len_real <= edge_len);
		}
		else {
			BLI_assert(edge_len_real == edge_len);
		}

		cache->wire_elem = ElementList_build(&elb);
	}

	return cache->wire_elem;
}

static void curve_batch_cache_create_overlay_batches(Curve *cu)
{
	/* Since CU_DATATYPE_OVERLAY is slow to generate, generate them all at once */
	int options = CU_DATATYPE_OVERLAY;

	CurveBatchCache *cache = curve_batch_cache_get(cu);
	CurveRenderData *lrdata = curve_render_data_create(cu, NULL, options);

	if (cache->overlay_verts == NULL) {
		static VertexFormat format = { 0 };
		static unsigned pos_id, data_id;
		if (format.attrib_ct == 0) {
			/* initialize vertex format */
			pos_id = VertexFormat_add_attrib(&format, "pos", COMP_F32, 3, KEEP_FLOAT);
			data_id = VertexFormat_add_attrib(&format, "data", COMP_U8, 1, KEEP_INT);
		}

		VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
		const int vbo_len_capacity = curve_render_data_overlay_verts_len_get(lrdata);
		int vbo_len_used = 0;
		VertexBuffer_allocate_data(vbo, vbo_len_capacity);
		int i = 0;
		for (Nurb *nu = lrdata->nurbs->first; nu; nu = nu->next) {
			if (nu->bezt) {
				int a = 0;
				for (const BezTriple *bezt = nu->bezt; a < nu->pntsu; a++, bezt++) {
					if (bezt->hide == false) {
						const bool is_active = (i == lrdata->actvert);
						char vflag;

						if (lrdata->hide_handles) {
							vflag = (bezt->f2 & SELECT) ?
							        (is_active ? VFLAG_VERTEX_ACTIVE : VFLAG_VERTEX_SELECTED) : 0;
							VertexBuffer_set_attrib(vbo, pos_id, vbo_len_used, bezt->vec[1]);
							VertexBuffer_set_attrib(vbo, data_id, vbo_len_used, &vflag);
							vbo_len_used += 1;
						}
						else {
							for (int j = 0; j < 3; j++) {
								vflag = ((&bezt->f1)[j] & SELECT) ?
								        (is_active ? VFLAG_VERTEX_ACTIVE : VFLAG_VERTEX_SELECTED) : 0;
								VertexBuffer_set_attrib(vbo, pos_id, vbo_len_used, bezt->vec[j]);
								VertexBuffer_set_attrib(vbo, data_id, vbo_len_used, &vflag);
								vbo_len_used += 1;
							}
						}
					}
					i += 1;
				}
			}
			else if (nu->bp) {
				int a = 0;
				for (const BPoint *bp = nu->bp; a < nu->pntsu; a++, bp++) {
					if (bp->hide == false) {
						const bool is_active = (i == lrdata->actvert);
						char vflag;
						vflag = (bp->f1 & SELECT) ? (is_active ? VFLAG_VERTEX_ACTIVE : VFLAG_VERTEX_SELECTED) : 0;
						VertexBuffer_set_attrib(vbo, pos_id, vbo_len_used, bp->vec);
						VertexBuffer_set_attrib(vbo, data_id, vbo_len_used, &vflag);
						vbo_len_used += 1;
					}
					i += 1;
				}
			}
			i += nu->pntsu;
		}
		if (vbo_len_capacity != vbo_len_used) {
			VertexBuffer_resize_data(vbo, vbo_len_used);
		}

		cache->overlay_verts = Batch_create(PRIM_POINTS, vbo, NULL);
	}


	if ((cache->overlay_edges == NULL) && (lrdata->hide_handles == false)) {
		/* Note: we could reference indices to vertices (above) */

		static VertexFormat format = { 0 };
		static unsigned pos_id, data_id;
		if (format.attrib_ct == 0) {
			/* initialize vertex format */
			pos_id = VertexFormat_add_attrib(&format, "pos", COMP_F32, 3, KEEP_FLOAT);
			data_id = VertexFormat_add_attrib(&format, "data", COMP_U8, 1, KEEP_INT);
		}

		VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
		const int edge_len =  curve_render_data_overlay_edges_len_get(lrdata);
		const int vbo_len_capacity = edge_len * 2;
		int vbo_len_used = 0;
		VertexBuffer_allocate_data(vbo, vbo_len_capacity);
		int i = 0;
		for (Nurb *nu = lrdata->nurbs->first; nu; nu = nu->next) {
			if (nu->bezt) {
				int a = 0;
				for (const BezTriple *bezt = nu->bezt; a < nu->pntsu; a++, bezt++) {
					if (bezt->hide == false) {
						const bool is_active = (i == lrdata->actvert);
						char vflag;

						vflag = (bezt->f1 & SELECT) ? (is_active ? VFLAG_VERTEX_ACTIVE : VFLAG_VERTEX_SELECTED) : 0;
						VertexBuffer_set_attrib(vbo, pos_id, vbo_len_used, bezt->vec[0]);
						VertexBuffer_set_attrib(vbo, data_id, vbo_len_used, &vflag);
						vbo_len_used += 1;

						/* same vertex twice, only check different selection */
						for (int j = 0; j < 2; j++) {
							vflag = ((j ? bezt->f3 : bezt->f1) & SELECT) ?
							        (is_active ? VFLAG_VERTEX_ACTIVE : VFLAG_VERTEX_SELECTED) : 0;
							VertexBuffer_set_attrib(vbo, pos_id, vbo_len_used, bezt->vec[1]);
							VertexBuffer_set_attrib(vbo, data_id, vbo_len_used, &vflag);
							vbo_len_used += 1;
						}

						vflag = (bezt->f3 & SELECT) ? (is_active ? VFLAG_VERTEX_ACTIVE : VFLAG_VERTEX_SELECTED) : 0;
						VertexBuffer_set_attrib(vbo, pos_id, vbo_len_used, bezt->vec[2]);
						VertexBuffer_set_attrib(vbo, data_id, vbo_len_used, &vflag);
						vbo_len_used += 1;
					}
					i += 1;
				}
			}
			else if (nu->bp) {
				int a = 1;
				for (const BPoint *bp_prev = nu->bp, *bp_curr = &nu->bp[1]; a < nu->pntsu; a++, bp_prev = bp_curr++) {
					if ((bp_prev->hide == false) && (bp_curr->hide == false)) {
						char vflag;
						vflag = ((bp_prev->f1 & SELECT) && (bp_curr->f1 & SELECT)) ? VFLAG_VERTEX_SELECTED : 0;
						VertexBuffer_set_attrib(vbo, pos_id, vbo_len_used, bp_prev->vec);
						VertexBuffer_set_attrib(vbo, data_id, vbo_len_used, &vflag);
						vbo_len_used += 1;
						VertexBuffer_set_attrib(vbo, pos_id, vbo_len_used, bp_curr->vec);
						VertexBuffer_set_attrib(vbo, data_id, vbo_len_used, &vflag);
						vbo_len_used += 1;

					}
				}
			}
		}
		if (vbo_len_capacity != vbo_len_used) {
			VertexBuffer_resize_data(vbo, vbo_len_used);
		}

		cache->overlay_edges = Batch_create(PRIM_LINES, vbo, NULL);
	}

	curve_render_data_free(lrdata);
}

Batch *BKE_curve_batch_cache_get_wire_edge(Curve *cu, CurveCache *ob_curve_cache)
{
	CurveBatchCache *cache = curve_batch_cache_get(cu);

	if (cache->wire_batch == NULL) {
		/* create batch from Curve */
		CurveRenderData *lrdata = curve_render_data_create(cu, ob_curve_cache, CU_DATATYPE_WIRE);

		cache->wire_batch = Batch_create(
		        PRIM_LINES,
		        curve_batch_cache_get_wire_verts(lrdata, cache),
		        curve_batch_cache_get_wire_edges(lrdata, cache));

		curve_render_data_free(lrdata);
	}

	return cache->wire_batch;
}

Batch *BKE_curve_batch_cache_get_overlay_edges(Curve *cu)
{
	CurveBatchCache *cache = curve_batch_cache_get(cu);

	if (cache->overlay_edges == NULL) {
		curve_batch_cache_create_overlay_batches(cu);
	}

	return cache->overlay_edges;
}

Batch *BKE_curve_batch_cache_get_overlay_verts(Curve *cu)
{
	CurveBatchCache *cache = curve_batch_cache_get(cu);

	if (cache->overlay_verts == NULL) {
		curve_batch_cache_create_overlay_batches(cu);
	}

	return cache->overlay_verts;
}
