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
 * Contributor(s): Blender Foundation, Mike Erwin, Dalai Felinto
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file draw_cache_impl_lattice.c
 *  \ingroup draw
 *
 * \brief Lattice API for render engines
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_math_vector.h"

#include "DNA_curve_types.h"
#include "DNA_lattice_types.h"

#include "BKE_lattice.h"

#include "GPU_batch.h"

#include "draw_cache_impl.h"  /* own include */

#define SELECT   1

/**
 * TODO
 * - 'DispList' is currently not used
 *   (we could avoid using since it will be removed)
 */

static void lattice_batch_cache_clear(Lattice *lt);

/* ---------------------------------------------------------------------- */
/* Lattice Interface, direct access to basic data. */

static int vert_len_calc(int u, int v, int w)
{
	if (u <= 0 || v <= 0 || w <= 0) {
		return 0;
	}
	return u * v * w;
}

static int edge_len_calc(int u, int v, int w)
{
	if (u <= 0 || v <= 0 || w <= 0) {
		return 0;
	}
	return (((((u - 1) * v) +
	          ((v - 1) * u)) * w) +
	        ((w - 1) * (u * v)));
}

static int lattice_render_verts_len_get(Lattice *lt)
{
	if (lt->editlatt) {
		lt = lt->editlatt->latt;
	}

	const int u = lt->pntsu;
	const int v = lt->pntsv;
	const int w = lt->pntsw;

	if ((lt->flag & LT_OUTSIDE) == 0) {
		return vert_len_calc(u, v, w);
	}
	else {
		/* TODO remove internal coords */
		return vert_len_calc(u, v, w);
	}
}

static int lattice_render_edges_len_get(Lattice *lt)
{
	if (lt->editlatt) {
		lt = lt->editlatt->latt;
	}

	const int u = lt->pntsu;
	const int v = lt->pntsv;
	const int w = lt->pntsw;

	if ((lt->flag & LT_OUTSIDE) == 0) {
		return edge_len_calc(u, v, w);
	}
	else {
		/* TODO remove internal coords */
		return edge_len_calc(u, v, w);
	}
}

/* ---------------------------------------------------------------------- */
/* Lattice Interface, indirect, partially cached access to complex data. */

typedef struct LatticeRenderData {
	int types;

	int vert_len;
	int edge_len;

	struct {
		int u_len, v_len, w_len;
	} dims;
	bool show_only_outside;

	struct EditLatt *edit_latt;
	BPoint *bp;

	int actbp;
} LatticeRenderData;

enum {
	LR_DATATYPE_VERT       = 1 << 0,
	LR_DATATYPE_EDGE       = 1 << 1,
	LR_DATATYPE_OVERLAY    = 1 << 2,
};

static LatticeRenderData *lattice_render_data_create(Lattice *lt, const int types)
{
	LatticeRenderData *rdata = MEM_callocN(sizeof(*rdata), __func__);
	rdata->types = types;

	if (lt->editlatt) {
		EditLatt *editlatt = lt->editlatt;
		lt = editlatt->latt;

		rdata->edit_latt = editlatt;

		if (types & (LR_DATATYPE_VERT)) {
			rdata->vert_len = lattice_render_verts_len_get(lt);
		}
		if (types & (LR_DATATYPE_EDGE)) {
			rdata->edge_len = lattice_render_edges_len_get(lt);
		}
		if (types & LR_DATATYPE_OVERLAY) {
			rdata->actbp = lt->actbp;
		}
	}
	else {
		if (types & (LR_DATATYPE_VERT)) {
			rdata->vert_len = lattice_render_verts_len_get(lt);
		}
		if (types & (LR_DATATYPE_EDGE)) {
			rdata->edge_len = lattice_render_edges_len_get(lt);
			/*no edge data */
		}
	}

	rdata->bp = lt->def;

	rdata->dims.u_len = lt->pntsu;
	rdata->dims.v_len = lt->pntsv;
	rdata->dims.w_len = lt->pntsw;

	rdata->show_only_outside = (lt->flag & LT_OUTSIDE) != 0;
	rdata->actbp = lt->actbp;

	return rdata;
}

static void lattice_render_data_free(LatticeRenderData *rdata)
{
#if 0
	if (rdata->loose_verts) {
		MEM_freeN(rdata->loose_verts);
	}
#endif
	MEM_freeN(rdata);
}

static int lattice_render_data_verts_len_get(const LatticeRenderData *rdata)
{
	BLI_assert(rdata->types & LR_DATATYPE_VERT);
	return rdata->vert_len;
}

static int lattice_render_data_edges_len_get(const LatticeRenderData *rdata)
{
	BLI_assert(rdata->types & LR_DATATYPE_EDGE);
	return rdata->edge_len;
}

static const BPoint *lattice_render_data_vert_bpoint(const LatticeRenderData *rdata, const int vert_idx)
{
	BLI_assert(rdata->types & LR_DATATYPE_VERT);
	return &rdata->bp[vert_idx];
}

enum {
	VFLAG_VERTEX_SELECTED = 1 << 0,
	VFLAG_VERTEX_ACTIVE   = 1 << 1,
};

/* ---------------------------------------------------------------------- */
/* Lattice Batch Cache */

typedef struct LatticeBatchCache {
	VertexBuffer *pos;
	ElementList *edges;

	Batch *all_verts;
	Batch *all_edges;

	Batch *overlay_verts;

	/* settings to determine if cache is invalid */
	bool is_dirty;

	struct {
		int u_len, v_len, w_len;
	} dims;
	bool show_only_outside;

	bool is_editmode;
} LatticeBatchCache;

/* Batch cache management. */

static bool lattice_batch_cache_valid(Lattice *lt)
{
	LatticeBatchCache *cache = lt->batch_cache;

	if (cache == NULL) {
		return false;
	}

	if (cache->is_editmode != (lt->editlatt != NULL)) {
		return false;
	}

	if (cache->is_dirty == false) {
		return true;
	}
	else {
		if (cache->is_editmode) {
			return false;
		}
		else if ((cache->dims.u_len != lt->pntsu) ||
		         (cache->dims.v_len != lt->pntsv) ||
		         (cache->dims.w_len != lt->pntsw) ||
		         ((cache->show_only_outside != ((lt->flag & LT_OUTSIDE) != 0))))
		{
			return false;
		}
	}

	return true;
}

static void lattice_batch_cache_init(Lattice *lt)
{
	LatticeBatchCache *cache = lt->batch_cache;

	if (!cache) {
		cache = lt->batch_cache = MEM_callocN(sizeof(*cache), __func__);
	}
	else {
		memset(cache, 0, sizeof(*cache));
	}

	cache->dims.u_len = lt->pntsu;
	cache->dims.v_len = lt->pntsv;
	cache->dims.w_len = lt->pntsw;
	cache->show_only_outside = (lt->flag & LT_OUTSIDE) != 0;

	cache->is_editmode = lt->editlatt != NULL;

	cache->is_dirty = false;
}

static LatticeBatchCache *lattice_batch_cache_get(Lattice *lt)
{
	if (!lattice_batch_cache_valid(lt)) {
		lattice_batch_cache_clear(lt);
		lattice_batch_cache_init(lt);
	}
	return lt->batch_cache;
}

void DRW_lattice_batch_cache_dirty(Lattice *lt, int mode)
{
	LatticeBatchCache *cache = lt->batch_cache;
	if (cache == NULL) {
		return;
	}
	switch (mode) {
		case BKE_LATTICE_BATCH_DIRTY_ALL:
			cache->is_dirty = true;
			break;
		case BKE_LATTICE_BATCH_DIRTY_SELECT:
			/* TODO Separate Flag vbo */
			BATCH_DISCARD_ALL_SAFE(cache->overlay_verts);
			break;
		default:
			BLI_assert(0);
	}
}

static void lattice_batch_cache_clear(Lattice *lt)
{
	LatticeBatchCache *cache = lt->batch_cache;
	if (!cache) {
		return;
	}

	BATCH_DISCARD_SAFE(cache->all_verts);
	BATCH_DISCARD_SAFE(cache->all_edges);
	BATCH_DISCARD_ALL_SAFE(cache->overlay_verts);

	VERTEXBUFFER_DISCARD_SAFE(cache->pos);
	ELEMENTLIST_DISCARD_SAFE(cache->edges);
}

void DRW_lattice_batch_cache_free(Lattice *lt)
{
	lattice_batch_cache_clear(lt);
	MEM_SAFE_FREE(lt->batch_cache);
}

/* Batch cache usage. */
static VertexBuffer *lattice_batch_cache_get_pos(LatticeRenderData *rdata, LatticeBatchCache *cache)
{
	BLI_assert(rdata->types & LR_DATATYPE_VERT);

	if (cache->pos == NULL) {
		static VertexFormat format = { 0 };
		static unsigned pos_id;
		if (format.attrib_ct == 0) {
			/* initialize vertex format */
			pos_id = VertexFormat_add_attrib(&format, "pos", COMP_F32, 3, KEEP_FLOAT);
		}

		const int vert_len = lattice_render_data_verts_len_get(rdata);

		cache->pos = VertexBuffer_create_with_format(&format);
		VertexBuffer_allocate_data(cache->pos, vert_len);
		for (int i = 0; i < vert_len; ++i) {
			const BPoint *bp = lattice_render_data_vert_bpoint(rdata, i);
			VertexBuffer_set_attrib(cache->pos, pos_id, i, bp->vec);
		}
	}

	return cache->pos;
}

static ElementList *lattice_batch_cache_get_edges(LatticeRenderData *rdata, LatticeBatchCache *cache)
{
	BLI_assert(rdata->types & (LR_DATATYPE_VERT | LR_DATATYPE_EDGE));

	if (cache->edges == NULL) {
		const int vert_len = lattice_render_data_verts_len_get(rdata);
		const int edge_len = lattice_render_data_edges_len_get(rdata);
		int edge_len_real = 0;

		ElementListBuilder elb;
		ElementListBuilder_init(&elb, PRIM_LINES, edge_len, vert_len);

#define LATT_INDEX(u, v, w) \
	((((w) * rdata->dims.v_len + (v)) * rdata->dims.u_len) + (u))

		for (int w = 0; w < rdata->dims.w_len; w++) {
			int wxt = (w == 0 || w == rdata->dims.w_len - 1);
			for (int v = 0; v < rdata->dims.v_len; v++) {
				int vxt = (v == 0 || v == rdata->dims.v_len - 1);
				for (int u = 0; u < rdata->dims.u_len; u++) {
					int uxt = (u == 0 || u == rdata->dims.u_len - 1);

					if (w && ((uxt || vxt) || !rdata->show_only_outside)) {
						add_line_vertices(&elb, LATT_INDEX(u, v, w - 1), LATT_INDEX(u, v, w));
						BLI_assert(edge_len_real <= edge_len);
						edge_len_real++;
					}
					if (v && ((uxt || wxt) || !rdata->show_only_outside)) {
						add_line_vertices(&elb, LATT_INDEX(u, v - 1, w), LATT_INDEX(u, v, w));
						BLI_assert(edge_len_real <= edge_len);
						edge_len_real++;
					}
					if (u && ((vxt || wxt) || !rdata->show_only_outside)) {
						add_line_vertices(&elb, LATT_INDEX(u - 1, v, w), LATT_INDEX(u, v, w));
						BLI_assert(edge_len_real <= edge_len);
						edge_len_real++;
					}
				}
			}
		}

#undef LATT_INDEX

		if (rdata->show_only_outside) {
			BLI_assert(edge_len_real <= edge_len);
		}
		else {
			BLI_assert(edge_len_real == edge_len);
		}

		cache->edges = ElementList_build(&elb);
	}

	return cache->edges;
}

static void lattice_batch_cache_create_overlay_batches(Lattice *lt)
{
	/* Since LR_DATATYPE_OVERLAY is slow to generate, generate them all at once */
	int options = LR_DATATYPE_VERT | LR_DATATYPE_OVERLAY;

	LatticeBatchCache *cache = lattice_batch_cache_get(lt);
	LatticeRenderData *rdata = lattice_render_data_create(lt, options);

	if (cache->overlay_verts == NULL) {
		static VertexFormat format = { 0 };
		static unsigned pos_id, data_id;
		if (format.attrib_ct == 0) {
			/* initialize vertex format */
			pos_id = VertexFormat_add_attrib(&format, "pos", COMP_F32, 3, KEEP_FLOAT);
			data_id = VertexFormat_add_attrib(&format, "data", COMP_U8, 1, KEEP_INT);
		}

		const int vert_len = lattice_render_data_verts_len_get(rdata);

		VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
		VertexBuffer_allocate_data(vbo, vert_len);
		for (int i = 0; i < vert_len; ++i) {
			const BPoint *bp = lattice_render_data_vert_bpoint(rdata, i);

			char vflag = 0;
			if (bp->f1 & SELECT) {
				if (i == rdata->actbp) {
					vflag |= VFLAG_VERTEX_ACTIVE;
				}
				else {
					vflag |= VFLAG_VERTEX_SELECTED;
				}
			}

			VertexBuffer_set_attrib(vbo, pos_id, i, bp->vec);
			VertexBuffer_set_attrib(vbo, data_id, i, &vflag);
		}

		cache->overlay_verts = Batch_create(PRIM_POINTS, vbo, NULL);
	}	

	lattice_render_data_free(rdata);
}

Batch *DRW_lattice_batch_cache_get_all_edges(Lattice *lt)
{
	LatticeBatchCache *cache = lattice_batch_cache_get(lt);

	if (cache->all_edges == NULL) {
		/* create batch from Lattice */
		LatticeRenderData *rdata = lattice_render_data_create(lt, LR_DATATYPE_VERT | LR_DATATYPE_EDGE);

		cache->all_edges = Batch_create(PRIM_LINES, lattice_batch_cache_get_pos(rdata, cache),
		                                lattice_batch_cache_get_edges(rdata, cache));

		lattice_render_data_free(rdata);
	}

	return cache->all_edges;
}

Batch *DRW_lattice_batch_cache_get_all_verts(Lattice *lt)
{
	LatticeBatchCache *cache = lattice_batch_cache_get(lt);

	if (cache->all_verts == NULL) {
		LatticeRenderData *rdata = lattice_render_data_create(lt, LR_DATATYPE_VERT);

		cache->all_verts = Batch_create(PRIM_POINTS, lattice_batch_cache_get_pos(rdata, cache), NULL);

		lattice_render_data_free(rdata);
	}

	return cache->all_verts;
}

Batch *DRW_lattice_batch_cache_get_overlay_verts(Lattice *lt)
{
	LatticeBatchCache *cache = lattice_batch_cache_get(lt);

	if (cache->overlay_verts == NULL) {
		lattice_batch_cache_create_overlay_batches(lt);
	}

	return cache->overlay_verts;
}
