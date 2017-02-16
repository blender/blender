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

/** \file blender/blenkernel/intern/mesh_render.c
 *  \ingroup bke
 *
 * \brief Mesh API for render engines
 */

#include "MEM_guardedalloc.h"

#include "BLI_math_vector.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_editmesh.h"
#include "BKE_mesh.h"
#include "BKE_mesh_render.h"

#include "GPU_batch.h"

/* ---------------------------------------------------------------------- */
/* Mesh Interface */

#define MESH_RENDER_FUNCTION(func_name)     \
	if (me->edit_btmesh && me->edit_btmesh->derivedFinal) { \
	    return mesh_bmesh_##func_name(me);  \
	}                                       \
	else {                                  \
	    return mesh_struct_##func_name(me); \
	}


/* Mesh Implementation */

static int mesh_struct_get_num_edges(Mesh *me)
{
	return me->totedge;
}

static int mesh_struct_get_num_verts(Mesh *me)
{
	return me->totvert;
}

static int mesh_struct_get_num_faces(Mesh *me)
{
	BKE_mesh_tessface_ensure(me);
	return me->totface;
}

static int mesh_struct_get_num_polys(Mesh *me)
{
	return me->totpoly;
}

static int mesh_struct_get_num_loops(Mesh *me)
{
	return me->totloop;
}

static MEdge *mesh_struct_get_array_edge(Mesh *me)
{
	return CustomData_get_layer(&me->edata, CD_MEDGE);
}

static MFace *mesh_struct_get_array_face(Mesh *me)
{
	BKE_mesh_tessface_ensure(me);
	return CustomData_get_layer(&me->fdata, CD_MFACE);
}

static MLoop *mesh_struct_get_array_loop(Mesh *me)
{
	return me->mloop;
}

static MPoly *mesh_struct_get_array_poly(Mesh *me)
{
	return me->mpoly;
}

static MVert *mesh_struct_get_array_vert(Mesh *me)
{
	return CustomData_get_layer(&me->vdata, CD_MVERT);
}

/* BMesh Implementation */

/* NOTE: we may want to get rid of Derived Mesh and
 * access BMesh directly */

static int mesh_bmesh_get_num_verts(Mesh *me)
{
	BMEditMesh *bm = me->edit_btmesh;
	DerivedMesh *dm = bm->derivedFinal;
	return dm->getNumVerts(dm);
}

static int mesh_bmesh_get_num_edges(Mesh *me)
{
	BMEditMesh *bm = me->edit_btmesh;
	DerivedMesh *dm = bm->derivedFinal;
	return dm->getNumEdges(dm);
}

static int mesh_bmesh_get_num_faces(Mesh *me)
{
	BMEditMesh *bm = me->edit_btmesh;
	DerivedMesh *dm = bm->derivedFinal;
	return dm->getNumTessFaces(dm);
}

static int mesh_bmesh_get_num_polys(Mesh *me)
{
	BMEditMesh *bm = me->edit_btmesh;
	DerivedMesh *dm = bm->derivedFinal;
	return dm->getNumPolys(dm);
}

static int mesh_bmesh_get_num_loops(Mesh *me)
{
	BMEditMesh *bm = me->edit_btmesh;
	DerivedMesh *dm = bm->derivedFinal;
	return dm->getNumLoops(dm);
}

static MEdge *mesh_bmesh_get_array_edge(Mesh *me)
{
	BMEditMesh *bm = me->edit_btmesh;
	DerivedMesh *dm = bm->derivedFinal;
	return dm->getEdgeArray(dm);
}

static MFace *mesh_bmesh_get_array_face(Mesh *me)
{
	BMEditMesh *bm = me->edit_btmesh;
	DerivedMesh *dm = bm->derivedFinal;
	return dm->getTessFaceArray(dm);
}

static MLoop *mesh_bmesh_get_array_loop(Mesh *me)
{
	BMEditMesh *bm = me->edit_btmesh;
	DerivedMesh *dm = bm->derivedFinal;
	return dm->getLoopArray(dm);
}

static MPoly *mesh_bmesh_get_array_poly(Mesh *me)
{
	BMEditMesh *bm = me->edit_btmesh;
	DerivedMesh *dm = bm->derivedFinal;
	return dm->getPolyArray(dm);
}

static MVert *mesh_bmesh_get_array_vert(Mesh *me)
{
	BMEditMesh *bm = me->edit_btmesh;
	DerivedMesh *dm = bm->derivedFinal;
	return dm->getVertArray(dm);
}

/* Mesh API */

static int mesh_render_get_num_edges(Mesh *me)
{
	MESH_RENDER_FUNCTION(get_num_edges);
}

static int mesh_render_get_num_faces(Mesh *me)
{
	MESH_RENDER_FUNCTION(get_num_faces);
}

static int mesh_render_get_num_loops(Mesh *me)
{
	MESH_RENDER_FUNCTION(get_num_loops);
}

static int mesh_render_get_num_polys(Mesh *me)
{
	MESH_RENDER_FUNCTION(get_num_polys);
}

static int mesh_render_get_num_verts(Mesh *me)
{
	MESH_RENDER_FUNCTION(get_num_verts);
}

static MEdge *mesh_render_get_array_edge(Mesh *me)
{
	MESH_RENDER_FUNCTION(get_array_edge);
}

static MFace *mesh_render_get_array_face(Mesh *me)
{
	MESH_RENDER_FUNCTION(get_array_face);
}

static MLoop *mesh_render_get_array_loop(Mesh *me)
{
	MESH_RENDER_FUNCTION(get_array_loop);
}

static MPoly *mesh_render_get_array_poly(Mesh *me)
{
	MESH_RENDER_FUNCTION(get_array_poly);
}

static MVert *mesh_render_get_array_vert(Mesh *me)
{
	MESH_RENDER_FUNCTION(get_array_vert);
}

/* ---------------------------------------------------------------------- */
/* Mesh Batch Cache */

typedef struct MeshBatchCache {
	VertexBuffer *pos_in_order;
	ElementList *edges_in_order;
	ElementList *triangles_in_order;

	Batch *all_verts;
	Batch *all_edges;
	Batch *all_triangles;

	Batch *triangles_with_normals; /* owns its vertex buffer */
	Batch *fancy_edges; /* owns its vertex buffer (not shared) */
	Batch *overlay_edges; /* owns its vertex buffer */

	/* settings to determine if cache is invalid */
	bool is_dirty;
	int tot_edges;
	int tot_faces;
	int tot_polys;
	int tot_verts;
	bool is_editmode;
} MeshBatchCache;

static bool mesh_batch_cache_valid(Mesh *me)
{
	MeshBatchCache *cache = me->batch_cache;

	if (cache == NULL) {
		return false;
	}

	if (cache->is_editmode != (me->edit_btmesh != NULL)) {
		return false;
	}

	if (cache->is_dirty == false) {
		return true;
	}
	else {
		if (cache->is_editmode) {
			return false;
		}
		else if ((cache->tot_edges != mesh_render_get_num_edges(me)) ||
		    (cache->tot_faces != mesh_render_get_num_faces(me)) ||
		    (cache->tot_polys != mesh_render_get_num_polys(me)) ||
		    (cache->tot_verts != mesh_render_get_num_verts(me)))
		{
			return false;
		}
	}

	return true;
}

static void mesh_batch_cache_init(Mesh *me)
{
	MeshBatchCache *cache = me->batch_cache;
	cache->is_editmode = me->edit_btmesh != NULL;

	if (cache->is_editmode == false) {
		cache->tot_edges = mesh_render_get_num_edges(me);
		cache->tot_faces = mesh_render_get_num_faces(me);
		cache->tot_polys = mesh_render_get_num_polys(me);
		cache->tot_verts = mesh_render_get_num_verts(me);
	}

	cache->is_dirty = false;
}

static MeshBatchCache *mesh_batch_cache_get(Mesh *me)
{
	if (!mesh_batch_cache_valid(me)) {
		BKE_mesh_batch_cache_free(me);
		me->batch_cache = MEM_callocN(sizeof(MeshBatchCache), "MeshBatchCache");
		mesh_batch_cache_init(me);
	}
	return me->batch_cache;
}

static VertexBuffer *mesh_batch_cache_get_pos_in_order(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->pos_in_order == NULL) {
		static VertexFormat format = { 0 };
		static unsigned pos_id;
		if (format.attrib_ct == 0) {
			/* initialize vertex format */
			pos_id = add_attrib(&format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
		}

		const int vertex_ct = mesh_render_get_num_verts(me);
		const MVert *verts = mesh_render_get_array_vert(me);

		cache->pos_in_order = VertexBuffer_create_with_format(&format);
		VertexBuffer_allocate_data(cache->pos_in_order, vertex_ct);
#if 0
		const unsigned stride = (verts + 1) - verts; /* or sizeof(MVert) */
		fillAttribStride(cache->pos_in_order, pos_id, stride, &verts[0].co);
#else
		for (int i = 0; i < vertex_ct; ++i) {
			setAttrib(cache->pos_in_order, pos_id, i, &verts[i].co);
		}
#endif
	}

	return cache->pos_in_order;
}

static ElementList *mesh_batch_cache_get_edges_in_order(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->edges_in_order == NULL) {
		const int vertex_ct = mesh_render_get_num_verts(me);
		const int edge_ct = mesh_render_get_num_edges(me);
		const MEdge *edges = mesh_render_get_array_edge(me);

		ElementListBuilder elb;
		ElementListBuilder_init(&elb, GL_LINES, edge_ct, vertex_ct);
		for (int i = 0; i < edge_ct; ++i) {
			const MEdge *edge = edges + i;
			add_line_vertices(&elb, edge->v1, edge->v2);
		}
		cache->edges_in_order = ElementList_build(&elb);
	}

	return cache->edges_in_order;
}

static ElementList *mesh_batch_cache_get_triangles_in_order(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->triangles_in_order == NULL) {
		const int vertex_ct = mesh_render_get_num_verts(me);
		const int tessface_ct = mesh_render_get_num_faces(me);
		MFace *tessfaces = mesh_render_get_array_face(me);

		ElementListBuilder elb;
		ElementListBuilder_init(&elb, GL_TRIANGLES, tessface_ct * 2, vertex_ct); /* up to 2 triangles per tessface */
		for (int i = 0; i < tessface_ct; ++i) {
			const MFace *tess = tessfaces + i;
			add_triangle_vertices(&elb, tess->v1, tess->v2, tess->v3);

			if (tess->v4) {
				add_triangle_vertices(&elb, tess->v1, tess->v3, tess->v4);
			}
		}
		cache->triangles_in_order = ElementList_build(&elb);
	}

	/* NOTE: we are reallocating, it would be interesting to reallocating the memory once we
	 * know the exactly triangle count (like in BKE_mesh_batch_cache_get_overlay_edges) */

	return cache->triangles_in_order;
}

void BKE_mesh_batch_cache_dirty(struct Mesh *me)
{
	MeshBatchCache *cache = me->batch_cache;
	if (cache) {
		cache->is_dirty = true;
	}
}

void BKE_mesh_batch_cache_free(Mesh *me)
{
	MeshBatchCache *cache = me->batch_cache;
	if (!cache) {
		return;
	}

	if (cache->all_verts) Batch_discard(cache->all_verts);
	if (cache->all_edges) Batch_discard(cache->all_edges);
	if (cache->all_triangles) Batch_discard(cache->all_triangles);

	if (cache->pos_in_order) VertexBuffer_discard(cache->pos_in_order);
	if (cache->edges_in_order) ElementList_discard(cache->edges_in_order);
	if (cache->triangles_in_order) ElementList_discard(cache->triangles_in_order);

	if (cache->triangles_with_normals) {
		Batch_discard_all(cache->triangles_with_normals);
	}

	if (cache->fancy_edges) {
		Batch_discard_all(cache->fancy_edges);
	}

	if (cache->overlay_edges) {
		Batch_discard_all(cache->overlay_edges);
	}

	MEM_freeN(cache);
	me->batch_cache = NULL;
}

Batch *BKE_mesh_batch_cache_get_all_edges(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->all_edges == NULL) {
		/* create batch from Mesh */
		cache->all_edges = Batch_create(GL_LINES, mesh_batch_cache_get_pos_in_order(me), mesh_batch_cache_get_edges_in_order(me));
	}

	return cache->all_edges;
}

Batch *BKE_mesh_batch_cache_get_all_triangles(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->all_triangles == NULL) {
		/* create batch from DM */
		cache->all_triangles = Batch_create(GL_TRIANGLES, mesh_batch_cache_get_pos_in_order(me), mesh_batch_cache_get_triangles_in_order(me));
	}

	return cache->all_triangles;
}

Batch *BKE_mesh_batch_cache_get_triangles_with_normals(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->triangles_with_normals == NULL) {
		unsigned int vidx = 0, nidx = 0;
		float face_no[3];
		short face_no_short[3];
		unsigned int mpoly_prev = UINT_MAX;

		static VertexFormat format = { 0 };
		static unsigned pos_id, nor_id;
		if (format.attrib_ct == 0) {
			/* initialize vertex format */
			pos_id = add_attrib(&format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
			nor_id = add_attrib(&format, "nor", GL_SHORT, 3, NORMALIZE_INT_TO_FLOAT);
		}
		const MVert *verts = mesh_render_get_array_vert(me);
		const MLoop *loops = mesh_render_get_array_loop(me);
		const MPoly *polys = mesh_render_get_array_poly(me);
		const int totpoly = mesh_render_get_num_polys(me);
		const int totloop = mesh_render_get_num_loops(me);

		const int tottri = poly_to_tri_count(totpoly,totloop);
		MLoopTri *looptri = MEM_mallocN(sizeof(*looptri) * tottri, __func__);

		BKE_mesh_recalc_looptri(loops, polys, verts, totloop, totpoly, looptri);

		VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
		VertexBuffer_allocate_data(vbo, tottri * 3);

		for (int i = 0; i < tottri; i++) {
			const MLoopTri *lt = &looptri[i];
			const MPoly *mp = &polys[lt->poly];

			/* Calc Normal */
			if (lt->poly != mpoly_prev) {
				BKE_mesh_calc_poly_normal(mp, &loops[mp->loopstart], verts, face_no);
				mpoly_prev = lt->poly;
			}

			if ((mp->flag & ME_SMOOTH) == 0) {
				normal_float_to_short_v3(face_no_short, face_no);
				setAttrib(vbo, nor_id, nidx++, face_no_short);
				setAttrib(vbo, nor_id, nidx++, face_no_short);
				setAttrib(vbo, nor_id, nidx++, face_no_short);
			}
			else {
				setAttrib(vbo, nor_id, nidx++, &verts[loops[lt->tri[0]].v].no);
				setAttrib(vbo, nor_id, nidx++, &verts[loops[lt->tri[1]].v].no);
				setAttrib(vbo, nor_id, nidx++, &verts[loops[lt->tri[2]].v].no);
			}

			setAttrib(vbo, pos_id, vidx++, &verts[loops[lt->tri[0]].v].co);
			setAttrib(vbo, pos_id, vidx++, &verts[loops[lt->tri[1]].v].co);
			setAttrib(vbo, pos_id, vidx++, &verts[loops[lt->tri[2]].v].co);
		}

		cache->triangles_with_normals = Batch_create(GL_TRIANGLES, vbo, NULL);

		MEM_freeN(looptri);
	}

	return cache->triangles_with_normals;
}

Batch *BKE_mesh_batch_cache_get_all_verts(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->all_verts == NULL) {
		/* create batch from DM */
		cache->all_verts = Batch_create(GL_POINTS, mesh_batch_cache_get_pos_in_order(me), NULL);
		Batch_set_builtin_program(cache->all_verts, GPU_SHADER_3D_POINT_FIXED_SIZE_UNIFORM_COLOR);
	}

	return cache->all_verts;
}

Batch *BKE_mesh_batch_cache_get_fancy_edges(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->fancy_edges == NULL) {
		/* create batch from DM */
		static VertexFormat format = { 0 };
		static unsigned pos_id, n1_id, n2_id;
		if (format.attrib_ct == 0) {
			/* initialize vertex format */
			pos_id = add_attrib(&format, "pos", COMP_F32, 3, KEEP_FLOAT);

#if USE_10_10_10 /* takes 1/3 the space */
			n1_id = add_attrib(&format, "N1", COMP_I10, 3, NORMALIZE_INT_TO_FLOAT);
			n2_id = add_attrib(&format, "N2", COMP_I10, 3, NORMALIZE_INT_TO_FLOAT);
#else
			n1_id = add_attrib(&format, "N1", COMP_F32, 3, KEEP_FLOAT);
			n2_id = add_attrib(&format, "N2", COMP_F32, 3, KEEP_FLOAT);
#endif
		}
		VertexBuffer *vbo = VertexBuffer_create_with_format(&format);

		const MVert *verts = mesh_render_get_array_vert(me);
		const MEdge *edges = mesh_render_get_array_edge(me);
		const MPoly *polys = mesh_render_get_array_poly(me);
		const MLoop *loops = mesh_render_get_array_loop(me);
		const int edge_ct = mesh_render_get_num_edges(me);
		const int poly_ct = mesh_render_get_num_polys(me);

		/* need normal of each face, and which faces are adjacent to each edge */
		typedef struct {
			int count;
			int face_index[2];
		} AdjacentFaces;

		float (*face_normal)[3] = MEM_mallocN(poly_ct * 3 * sizeof(float), "face_normal");
		AdjacentFaces *adj_faces = MEM_callocN(edge_ct * sizeof(AdjacentFaces), "adj_faces");

		for (int i = 0; i < poly_ct; ++i) {
			const MPoly *poly = polys + i;

			BKE_mesh_calc_poly_normal(poly, loops + poly->loopstart, verts, face_normal[i]);

			for (int j = poly->loopstart; j < (poly->loopstart + poly->totloop); ++j) {
				AdjacentFaces *adj = adj_faces + loops[j].e;
				if (adj->count < 2)
					adj->face_index[adj->count] = i;
				adj->count++;
			}
		}

		const int vertex_ct = edge_ct * 2; /* these are GL_LINE verts, not mesh verts */
		VertexBuffer_allocate_data(vbo, vertex_ct);
		for (int i = 0; i < edge_ct; ++i) {
			const MEdge *edge = edges + i;
			const AdjacentFaces *adj = adj_faces + i;

#if USE_10_10_10
			PackedNormal n1value = { .x = 0, .y = 0, .z = +511 };
			PackedNormal n2value = { .x = 0, .y = 0, .z = -511 };

			if (adj->count == 2) {
				n1value = convert_i10_v3(face_normal[adj->face_index[0]]);
				n2value = convert_i10_v3(face_normal[adj->face_index[1]]);
			}

			const PackedNormal *n1 = &n1value;
			const PackedNormal *n2 = &n2value;
#else
			const float dummy1[3] = { 0.0f, 0.0f, +1.0f };
			const float dummy2[3] = { 0.0f, 0.0f, -1.0f };

			const float *n1 = (adj->count == 2) ? face_normal[adj->face_index[0]] : dummy1;
			const float *n2 = (adj->count == 2) ? face_normal[adj->face_index[1]] : dummy2;
#endif

			setAttrib(vbo, pos_id, 2 * i, &verts[edge->v1].co);
			setAttrib(vbo, n1_id, 2 * i, n1);
			setAttrib(vbo, n2_id, 2 * i, n2);

			setAttrib(vbo, pos_id, 2 * i + 1, &verts[edge->v2].co);
			setAttrib(vbo, n1_id, 2 * i + 1, n1);
			setAttrib(vbo, n2_id, 2 * i + 1, n2);
		}

		MEM_freeN(adj_faces);
		MEM_freeN(face_normal);

		cache->fancy_edges = Batch_create(GL_LINES, vbo, NULL);
	}

	return cache->fancy_edges;
}

static bool edge_is_real(const MEdge *edges, int edge_ct, int v1, int v2)
{
	/* TODO: same thing, except not ridiculously slow */

	for (int e = 0; e < edge_ct; ++e) {
		const MEdge *edge = edges + e;
		if ((edge->v1 == v1 && edge->v2 == v2) || (edge->v1 == v2 && edge->v2 == v1)) {
			return true;
		}
	}

	return false;
}

static void add_overlay_tri(
        VertexBuffer *vbo, unsigned pos_id, unsigned edgeMod_id, const MVert *verts,
        const MEdge *edges, int edge_ct, int v1, int v2, int v3, int base_vert_idx)
{
	const float edgeMods[2] = { 0.0f, 1.0f };

	const float *pos = verts[v1].co;
	setAttrib(vbo, pos_id, base_vert_idx + 0, pos);
	setAttrib(vbo, edgeMod_id, base_vert_idx + 0, edgeMods + (edge_is_real(edges, edge_ct, v2, v3) ? 1 : 0));

	pos = verts[v2].co;
	setAttrib(vbo, pos_id, base_vert_idx + 1, pos);
	setAttrib(vbo, edgeMod_id, base_vert_idx + 1, edgeMods + (edge_is_real(edges, edge_ct, v3, v1) ? 1 : 0));

	pos = verts[v3].co;
	setAttrib(vbo, pos_id, base_vert_idx + 2, pos);
	setAttrib(vbo, edgeMod_id, base_vert_idx + 2, edgeMods + (edge_is_real(edges, edge_ct, v1, v2) ? 1 : 0));
}

Batch *BKE_mesh_batch_cache_get_overlay_edges(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->overlay_edges == NULL) {
		/* create batch from DM */
		static VertexFormat format = { 0 };
		static unsigned pos_id, edgeMod_id;

		if (format.attrib_ct == 0) {
			/* initialize vertex format */
			pos_id = add_attrib(&format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
			edgeMod_id = add_attrib(&format, "edgeWidthModulator", GL_FLOAT, 1, KEEP_FLOAT);
		}
		VertexBuffer *vbo = VertexBuffer_create_with_format(&format);

		const int edge_ct = mesh_render_get_num_edges(me);
		const int tessface_ct = mesh_render_get_num_faces(me);
		const MVert *verts = mesh_render_get_array_vert(me);
		const MEdge *edges = mesh_render_get_array_edge(me);
		const MFace *tessfaces = mesh_render_get_array_face(me);

		VertexBuffer_allocate_data(vbo, tessface_ct * 6); /* up to 2 triangles per tessface */

		int gpu_vert_idx = 0;
		for (int i = 0; i < tessface_ct; ++i) {
			const MFace *tess = tessfaces + i;
			add_overlay_tri(vbo, pos_id, edgeMod_id, verts, edges, edge_ct, tess->v1, tess->v2, tess->v3, gpu_vert_idx);
			gpu_vert_idx += 3;
			/* tessface can be triangle or quad */
			if (tess->v4) {
				add_overlay_tri(vbo, pos_id, edgeMod_id, verts, edges, edge_ct, tess->v3, tess->v2, tess->v4, gpu_vert_idx);
				gpu_vert_idx += 3;
			}
		}

		/* in some cases all the faces are quad, so no need to reallocate */
		if (vbo->vertex_ct != gpu_vert_idx) {
			VertexBuffer_resize_data(vbo, gpu_vert_idx);
		}

		cache->overlay_edges = Batch_create(GL_TRIANGLES, vbo, NULL);
	}

	return cache->overlay_edges;
}

#undef MESH_RENDER_FUNCTION
