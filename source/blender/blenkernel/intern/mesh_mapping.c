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
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/mesh_mapping.c
 *  \ingroup bke
 *
 * Functions for accessing mesh connectivity data.
 * eg: polys connected to verts, UV's connected to verts.
 */

#include "MEM_guardedalloc.h"

#include "DNA_meshdata_types.h"
#include "DNA_vec_types.h"

#include "BLI_buffer.h"
#include "BLI_utildefines.h"
#include "BLI_bitmap.h"
#include "BLI_math.h"

#include "BKE_mesh_mapping.h"
#include "BKE_customdata.h"
#include "BLI_memarena.h"

#include "BLI_strict_flags.h"


/* -------------------------------------------------------------------- */

/** \name Mesh Connectivity Mapping
 * \{ */


/* ngon version wip, based on BM_uv_vert_map_create */
/* this replaces the non bmesh function (in trunk) which takes MTFace's, if we ever need it back we could
 * but for now this replaces it because its unused. */

UvVertMap *BKE_mesh_uv_vert_map_create(
        struct MPoly *mpoly, struct MLoop *mloop, struct MLoopUV *mloopuv,
        unsigned int totpoly, unsigned int totvert,
        const float limit[2], const bool selected, const bool use_winding)
{
	UvVertMap *vmap;
	UvMapVert *buf;
	MPoly *mp;
	unsigned int a;
	int i, totuv, nverts;

	bool *winding = NULL;
	BLI_buffer_declare_static(vec2f, tf_uv_buf, BLI_BUFFER_NOP, 32);

	totuv = 0;

	/* generate UvMapVert array */
	mp = mpoly;
	for (a = 0; a < totpoly; a++, mp++)
		if (!selected || (!(mp->flag & ME_HIDE) && (mp->flag & ME_FACE_SEL)))
			totuv += mp->totloop;

	if (totuv == 0)
		return NULL;

	vmap = (UvVertMap *)MEM_callocN(sizeof(*vmap), "UvVertMap");
	buf = vmap->buf = (UvMapVert *)MEM_callocN(sizeof(*vmap->buf) * (size_t)totuv, "UvMapVert");
	vmap->vert = (UvMapVert **)MEM_callocN(sizeof(*vmap->vert) * totvert, "UvMapVert*");
	if (use_winding) {
		winding = MEM_callocN(sizeof(*winding) * totpoly, "winding");
	}

	if (!vmap->vert || !vmap->buf) {
		BKE_mesh_uv_vert_map_free(vmap);
		return NULL;
	}

	mp = mpoly;
	for (a = 0; a < totpoly; a++, mp++) {
		if (!selected || (!(mp->flag & ME_HIDE) && (mp->flag & ME_FACE_SEL))) {
			float (*tf_uv)[2] = NULL;

			if (use_winding) {
				tf_uv = (float (*)[2])BLI_buffer_reinit_data(&tf_uv_buf, vec2f, (size_t)mp->totloop);
			}

			nverts = mp->totloop;

			for (i = 0; i < nverts; i++) {
				buf->loop_of_poly_index = (unsigned short)i;
				buf->poly_index = a;
				buf->separate = 0;
				buf->next = vmap->vert[mloop[mp->loopstart + i].v];
				vmap->vert[mloop[mp->loopstart + i].v] = buf;

				if (use_winding) {
					copy_v2_v2(tf_uv[i], mloopuv[mpoly[a].loopstart + i].uv);
				}

				buf++;
			}

			if (use_winding) {
				winding[a] = cross_poly_v2(tf_uv, (unsigned int)nverts) > 0;
			}
		}
	}

	/* sort individual uvs for each vert */
	for (a = 0; a < totvert; a++) {
		UvMapVert *newvlist = NULL, *vlist = vmap->vert[a];
		UvMapVert *iterv, *v, *lastv, *next;
		float *uv, *uv2, uvdiff[2];

		while (vlist) {
			v = vlist;
			vlist = vlist->next;
			v->next = newvlist;
			newvlist = v;

			uv = mloopuv[mpoly[v->poly_index].loopstart + v->loop_of_poly_index].uv;
			lastv = NULL;
			iterv = vlist;

			while (iterv) {
				next = iterv->next;

				uv2 = mloopuv[mpoly[iterv->poly_index].loopstart + iterv->loop_of_poly_index].uv;
				sub_v2_v2v2(uvdiff, uv2, uv);


				if (fabsf(uv[0] - uv2[0]) < limit[0] && fabsf(uv[1] - uv2[1]) < limit[1] &&
				    (!use_winding || winding[iterv->poly_index] == winding[v->poly_index]))
				{
					if (lastv) lastv->next = next;
					else vlist = next;
					iterv->next = newvlist;
					newvlist = iterv;
				}
				else
					lastv = iterv;

				iterv = next;
			}

			newvlist->separate = 1;
		}

		vmap->vert[a] = newvlist;
	}

	if (use_winding) {
		MEM_freeN(winding);
	}

	BLI_buffer_free(&tf_uv_buf);

	return vmap;
}

UvMapVert *BKE_mesh_uv_vert_map_get_vert(UvVertMap *vmap, unsigned int v)
{
	return vmap->vert[v];
}

void BKE_mesh_uv_vert_map_free(UvVertMap *vmap)
{
	if (vmap) {
		if (vmap->vert) MEM_freeN(vmap->vert);
		if (vmap->buf) MEM_freeN(vmap->buf);
		MEM_freeN(vmap);
	}
}

/**
 * Generates a map where the key is the vertex and the value is a list
 * of polys or loops that use that vertex as a corner. The lists are allocated
 * from one memory pool.
 *
 * Wrapped by #BKE_mesh_vert_poly_map_create & BKE_mesh_vert_loop_map_create
 */
static void mesh_vert_poly_or_loop_map_create(
        MeshElemMap **r_map, int **r_mem,
        const MPoly *mpoly, const MLoop *mloop,
        int totvert, int totpoly, int totloop, const bool do_loops)
{
	MeshElemMap *map = MEM_callocN(sizeof(MeshElemMap) * (size_t)totvert, __func__);
	int *indices, *index_iter;
	int i, j;

	indices = index_iter = MEM_mallocN(sizeof(int) * (size_t)totloop, __func__);

	/* Count number of polys for each vertex */
	for (i = 0; i < totpoly; i++) {
		const MPoly *p = &mpoly[i];

		for (j = 0; j < p->totloop; j++)
			map[mloop[p->loopstart + j].v].count++;
	}

	/* Assign indices mem */
	for (i = 0; i < totvert; i++) {
		map[i].indices = index_iter;
		index_iter += map[i].count;

		/* Reset 'count' for use as index in last loop */
		map[i].count = 0;
	}

	/* Find the users */
	for (i = 0; i < totpoly; i++) {
		const MPoly *p = &mpoly[i];

		for (j = 0; j < p->totloop; j++) {
			unsigned int v = mloop[p->loopstart + j].v;

			map[v].indices[map[v].count] = do_loops ? p->loopstart + j : i;
			map[v].count++;
		}
	}

	*r_map = map;
	*r_mem = indices;
}

/**
 * Generates a map where the key is the vertex and the value is a list of polys that use that vertex as a corner.
 * The lists are allocated from one memory pool.
 */
void BKE_mesh_vert_poly_map_create(
        MeshElemMap **r_map, int **r_mem,
        const MPoly *mpoly, const MLoop *mloop,
        int totvert, int totpoly, int totloop)
{
	mesh_vert_poly_or_loop_map_create(r_map, r_mem, mpoly, mloop, totvert, totpoly, totloop, false);
}

/**
 * Generates a map where the key is the vertex and the value is a list of loops that use that vertex as a corner.
 * The lists are allocated from one memory pool.
 */
void BKE_mesh_vert_loop_map_create(
        MeshElemMap **r_map, int **r_mem,
        const MPoly *mpoly, const MLoop *mloop,
        int totvert, int totpoly, int totloop)
{
	mesh_vert_poly_or_loop_map_create(r_map, r_mem, mpoly, mloop, totvert, totpoly, totloop, true);
}

/**
 * Generates a map where the key is the edge and the value is a list of looptris that use that edge.
 * The lists are allocated from one memory pool.
 */
void BKE_mesh_vert_looptri_map_create(
        MeshElemMap **r_map, int **r_mem,
        const MVert *UNUSED(mvert), const int totvert,
        const MLoopTri *mlooptri, const int totlooptri,
        const MLoop *mloop, const int UNUSED(totloop))
{
	MeshElemMap *map = MEM_callocN(sizeof(MeshElemMap) * (size_t)totvert, __func__);
	int *indices = MEM_mallocN(sizeof(int) * (size_t)totlooptri * 3, __func__);
	int *index_step;
	const MLoopTri *mlt;
	int i;

	/* count face users */
	for (i = 0, mlt = mlooptri; i < totlooptri; mlt++, i++) {
		for (int j = 3; j--;) {
			map[mloop[mlt->tri[j]].v].count++;
		}
	}

	/* create offsets */
	index_step = indices;
	for (i = 0; i < totvert; i++) {
		map[i].indices = index_step;
		index_step += map[i].count;

		/* re-count, using this as an index below */
		map[i].count = 0;
	}

	/* assign looptri-edge users */
	for (i = 0, mlt = mlooptri; i < totlooptri; mlt++, i++) {
		for (int j = 3; j--;) {
			MeshElemMap *map_ele = &map[mloop[mlt->tri[j]].v];
			map_ele->indices[map_ele->count++] = i;
		}
	}

	*r_map = map;
	*r_mem = indices;
}

/**
 * Generates a map where the key is the vertex and the value is a list of edges that use that vertex as an endpoint.
 * The lists are allocated from one memory pool.
 */
void BKE_mesh_vert_edge_map_create(
        MeshElemMap **r_map, int **r_mem,
        const MEdge *medge, int totvert, int totedge)
{
	MeshElemMap *map = MEM_callocN(sizeof(MeshElemMap) * (size_t)totvert, "vert-edge map");
	int *indices = MEM_mallocN(sizeof(int[2]) * (size_t)totedge, "vert-edge map mem");
	int *i_pt = indices;

	int i;

	/* Count number of edges for each vertex */
	for (i = 0; i < totedge; i++) {
		map[medge[i].v1].count++;
		map[medge[i].v2].count++;
	}

	/* Assign indices mem */
	for (i = 0; i < totvert; i++) {
		map[i].indices = i_pt;
		i_pt += map[i].count;

		/* Reset 'count' for use as index in last loop */
		map[i].count = 0;
	}

	/* Find the users */
	for (i = 0; i < totedge; i++) {
		const unsigned int v[2] = {medge[i].v1, medge[i].v2};

		map[v[0]].indices[map[v[0]].count] = i;
		map[v[1]].indices[map[v[1]].count] = i;

		map[v[0]].count++;
		map[v[1]].count++;
	}

	*r_map = map;
	*r_mem = indices;
}

/**
 * A version of #BKE_mesh_vert_edge_map_create that references connected vertices directly (not their edges).
 */
void BKE_mesh_vert_edge_vert_map_create(
        MeshElemMap **r_map, int **r_mem,
        const MEdge *medge, int totvert, int totedge)
{
	MeshElemMap *map = MEM_callocN(sizeof(MeshElemMap) * (size_t)totvert, "vert-edge map");
	int *indices = MEM_mallocN(sizeof(int[2]) * (size_t)totedge, "vert-edge map mem");
	int *i_pt = indices;

	int i;

	/* Count number of edges for each vertex */
	for (i = 0; i < totedge; i++) {
		map[medge[i].v1].count++;
		map[medge[i].v2].count++;
	}

	/* Assign indices mem */
	for (i = 0; i < totvert; i++) {
		map[i].indices = i_pt;
		i_pt += map[i].count;

		/* Reset 'count' for use as index in last loop */
		map[i].count = 0;
	}

	/* Find the users */
	for (i = 0; i < totedge; i++) {
		const unsigned int v[2] = {medge[i].v1, medge[i].v2};

		map[v[0]].indices[map[v[0]].count] = (int)v[1];
		map[v[1]].indices[map[v[1]].count] = (int)v[0];

		map[v[0]].count++;
		map[v[1]].count++;
	}

	*r_map = map;
	*r_mem = indices;
}

/**
 * Generates a map where the key is the edge and the value is a list of loops that use that edge.
 * Loops indices of a same poly are contiguous and in winding order.
 * The lists are allocated from one memory pool.
 */
void BKE_mesh_edge_loop_map_create(
        MeshElemMap **r_map, int **r_mem,
        const MEdge *UNUSED(medge), const int totedge,
        const MPoly *mpoly, const int totpoly,
        const MLoop *mloop, const int totloop)
{
	MeshElemMap *map = MEM_callocN(sizeof(MeshElemMap) * (size_t)totedge, "edge-poly map");
	int *indices = MEM_mallocN(sizeof(int) * (size_t)totloop * 2, "edge-poly map mem");
	int *index_step;
	const MPoly *mp;
	int i;

	/* count face users */
	for (i = 0, mp = mpoly; i < totpoly; mp++, i++) {
		const MLoop *ml;
		int j = mp->totloop;
		for (ml = &mloop[mp->loopstart]; j--; ml++) {
			map[ml->e].count += 2;
		}
	}

	/* create offsets */
	index_step = indices;
	for (i = 0; i < totedge; i++) {
		map[i].indices = index_step;
		index_step += map[i].count;

		/* re-count, using this as an index below */
		map[i].count = 0;
	}

	/* assign loop-edge users */
	for (i = 0, mp = mpoly; i < totpoly; mp++, i++) {
		const MLoop *ml;
		MeshElemMap *map_ele;
		const int max_loop = mp->loopstart + mp->totloop;
		int j = mp->loopstart;
		for (ml = &mloop[j]; j < max_loop; j++, ml++) {
			map_ele = &map[ml->e];
			map_ele->indices[map_ele->count++] = j;
			map_ele->indices[map_ele->count++] = j + 1;
		}
		/* last edge/loop of poly, must point back to first loop! */
		map_ele->indices[map_ele->count - 1] = mp->loopstart;
	}

	*r_map = map;
	*r_mem = indices;
}

/**
 * Generates a map where the key is the edge and the value is a list of polygons that use that edge.
 * The lists are allocated from one memory pool.
 */
void BKE_mesh_edge_poly_map_create(
        MeshElemMap **r_map, int **r_mem,
        const MEdge *UNUSED(medge), const int totedge,
        const MPoly *mpoly, const int totpoly,
        const MLoop *mloop, const int totloop)
{
	MeshElemMap *map = MEM_callocN(sizeof(MeshElemMap) * (size_t)totedge, "edge-poly map");
	int *indices = MEM_mallocN(sizeof(int) * (size_t)totloop, "edge-poly map mem");
	int *index_step;
	const MPoly *mp;
	int i;

	/* count face users */
	for (i = 0, mp = mpoly; i < totpoly; mp++, i++) {
		const MLoop *ml;
		int j = mp->totloop;
		for (ml = &mloop[mp->loopstart]; j--; ml++) {
			map[ml->e].count++;
		}
	}

	/* create offsets */
	index_step = indices;
	for (i = 0; i < totedge; i++) {
		map[i].indices = index_step;
		index_step += map[i].count;

		/* re-count, using this as an index below */
		map[i].count = 0;

	}

	/* assign poly-edge users */
	for (i = 0, mp = mpoly; i < totpoly; mp++, i++) {
		const MLoop *ml;
		int j = mp->totloop;
		for (ml = &mloop[mp->loopstart]; j--; ml++) {
			MeshElemMap *map_ele = &map[ml->e];
			map_ele->indices[map_ele->count++] = i;
		}
	}

	*r_map = map;
	*r_mem = indices;
}

/**
 * This function creates a map so the source-data (vert/edge/loop/poly)
 * can loop over the destination data (using the destination arrays origindex).
 *
 * This has the advantage that it can operate on any data-types.
 *
 * \param totsource  The total number of elements the that \a final_origindex points to.
 * \param totfinal  The size of \a final_origindex
 * \param final_origindex  The size of the final array.
 *
 * \note ``totsource`` could be ``totpoly``,
 *       ``totfinal`` could be ``tottessface`` and ``final_origindex`` its ORIGINDEX customdata.
 *       This would allow an MPoly to loop over its tessfaces.
 */
void BKE_mesh_origindex_map_create(
        MeshElemMap **r_map, int **r_mem,
        const int totsource,
        const int *final_origindex, const int totfinal)
{
	MeshElemMap *map = MEM_callocN(sizeof(MeshElemMap) * (size_t)totsource, "poly-tessface map");
	int *indices = MEM_mallocN(sizeof(int) * (size_t)totfinal, "poly-tessface map mem");
	int *index_step;
	int i;

	/* count face users */
	for (i = 0; i < totfinal; i++) {
		if (final_origindex[i] != ORIGINDEX_NONE) {
			BLI_assert(final_origindex[i] < totsource);
			map[final_origindex[i]].count++;
		}
	}

	/* create offsets */
	index_step = indices;
	for (i = 0; i < totsource; i++) {
		map[i].indices = index_step;
		index_step += map[i].count;

		/* re-count, using this as an index below */
		map[i].count = 0;
	}

	/* assign poly-tessface users */
	for (i = 0; i < totfinal; i++) {
		if (final_origindex[i] != ORIGINDEX_NONE) {
			MeshElemMap *map_ele = &map[final_origindex[i]];
			map_ele->indices[map_ele->count++] = i;
		}
	}

	*r_map = map;
	*r_mem = indices;
}

/**
 * A version of #BKE_mesh_origindex_map_create that takes a looptri array.
 * Making a poly -> looptri map.
 */
void BKE_mesh_origindex_map_create_looptri(
        MeshElemMap **r_map, int **r_mem,
        const MPoly *mpoly, const int mpoly_num,
        const MLoopTri *looptri, const int looptri_num)
{
	MeshElemMap *map = MEM_callocN(sizeof(MeshElemMap) * (size_t)mpoly_num, "poly-tessface map");
	int *indices = MEM_mallocN(sizeof(int) * (size_t)looptri_num, "poly-tessface map mem");
	int *index_step;
	int i;

	/* create offsets */
	index_step = indices;
	for (i = 0; i < mpoly_num; i++) {
		map[i].indices = index_step;
		index_step += ME_POLY_TRI_TOT(&mpoly[i]);
	}

	/* assign poly-tessface users */
	for (i = 0; i < looptri_num; i++) {
		MeshElemMap *map_ele = &map[looptri[i].poly];
		map_ele->indices[map_ele->count++] = i;
	}

	*r_map = map;
	*r_mem = indices;
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Mesh loops/poly islands.
 * Used currently for UVs and 'smooth groups'.
 * \{ */

/**
 * Callback deciding whether the given poly/loop/edge define an island boundary or not.
 */
typedef bool (*MeshRemap_CheckIslandBoundary)(
        const struct MPoly *mpoly, const struct MLoop *mloop, const struct MEdge *medge,
        const int nbr_egde_users, void *user_data);

static void poly_edge_loop_islands_calc(
        const MEdge *medge, const int totedge, const MPoly *mpoly, const int totpoly,
        const MLoop *mloop, const int totloop, MeshElemMap *edge_poly_map,
        const bool use_bitflags, MeshRemap_CheckIslandBoundary edge_boundary_check, void *edge_boundary_check_data,
        int **r_poly_groups, int *r_totgroup, BLI_bitmap **r_edge_borders, int *r_totedgeborder)
{
	int *poly_groups;
	int *poly_stack;

	BLI_bitmap *edge_borders = NULL;
	int num_edgeborders = 0;

	int poly_prev = 0;
	const int temp_poly_group_id = 3;  /* Placeholder value. */
	const int poly_group_id_overflowed = 5;  /* Group we could not find any available bit, will be reset to 0 at end */
	int tot_group = 0;
	bool group_id_overflow = false;

	/* map vars */
	int *edge_poly_mem = NULL;

	if (totpoly == 0) {
		*r_totgroup = 0;
		*r_poly_groups = NULL;
		if (r_edge_borders) {
			*r_edge_borders = NULL;
			*r_totedgeborder = 0;
		}
		return;
	}

	if (r_edge_borders) {
		edge_borders = BLI_BITMAP_NEW(totedge, __func__);
		*r_totedgeborder = 0;
	}

	if (!edge_poly_map) {
		BKE_mesh_edge_poly_map_create(
		        &edge_poly_map, &edge_poly_mem,
		        medge, totedge, mpoly, totpoly, mloop, totloop);
	}

	poly_groups = MEM_callocN(sizeof(int) * (size_t)totpoly, __func__);
	poly_stack  = MEM_mallocN(sizeof(int) * (size_t)totpoly, __func__);

	while (true) {
		int poly;
		int bit_poly_group_mask = 0;
		int poly_group_id;
		int ps_curr_idx = 0, ps_end_idx = 0;  /* stack indices */

		for (poly = poly_prev; poly < totpoly; poly++) {
			if (poly_groups[poly] == 0) {
				break;
			}
		}

		if (poly == totpoly) {
			/* all done */
			break;
		}

		poly_group_id = use_bitflags ? temp_poly_group_id : ++tot_group;

		/* start searching from here next time */
		poly_prev = poly + 1;

		poly_groups[poly] = poly_group_id;
		poly_stack[ps_end_idx++] = poly;

		while (ps_curr_idx != ps_end_idx) {
			const MPoly *mp;
			const MLoop *ml;
			int j;

			poly = poly_stack[ps_curr_idx++];
			BLI_assert(poly_groups[poly] == poly_group_id);

			mp = &mpoly[poly];
			for (ml = &mloop[mp->loopstart], j = mp->totloop; j--; ml++) {
				/* loop over poly users */
				const int me_idx = (int)ml->e;
				const MEdge *me = &medge[me_idx];
				const MeshElemMap *map_ele = &edge_poly_map[me_idx];
				const int *p = map_ele->indices;
				int i = map_ele->count;
				if (!edge_boundary_check(mp, ml, me, i, edge_boundary_check_data)) {
					for (; i--; p++) {
						/* if we meet other non initialized its a bug */
						BLI_assert(ELEM(poly_groups[*p], 0, poly_group_id));

						if (poly_groups[*p] == 0) {
							poly_groups[*p] = poly_group_id;
							poly_stack[ps_end_idx++] = *p;
						}
					}
				}
				else {
					if (edge_borders && !BLI_BITMAP_TEST(edge_borders, me_idx)) {
						BLI_BITMAP_ENABLE(edge_borders, me_idx);
						num_edgeborders++;
					}
					if (use_bitflags) {
						/* Find contiguous smooth groups already assigned, these are the values we can't reuse! */
						for (; i--; p++) {
							int bit = poly_groups[*p];
							if (!ELEM(bit, 0, poly_group_id, poly_group_id_overflowed) &&
							    !(bit_poly_group_mask & bit))
							{
								bit_poly_group_mask |= bit;
							}
						}
					}
				}
			}
		}
		/* And now, we have all our poly from current group in poly_stack (from 0 to (ps_end_idx - 1)), as well as
		 * all smoothgroups bits we can't use in bit_poly_group_mask.
		 */
		if (use_bitflags) {
			int i, *p, gid_bit = 0;
			poly_group_id = 1;

			/* Find first bit available! */
			for (; (poly_group_id & bit_poly_group_mask) && (gid_bit < 32); gid_bit++) {
				poly_group_id <<= 1;  /* will 'overflow' on last possible iteration. */
			}
			if (UNLIKELY(gid_bit > 31)) {
				/* All bits used in contiguous smooth groups, we can't do much!
				 * Note: this is *very* unlikely - theoretically, four groups are enough, I don't think we can reach
				 *       this goal with such a simple algo, but I don't think either we'll never need all 32 groups!
				 */
				printf("Warning, could not find an available id for current smooth group, faces will me marked "
				       "as out of any smooth group...\n");
				poly_group_id = poly_group_id_overflowed; /* Can't use 0, will have to set them to this value later. */
				group_id_overflow = true;
			}
			if (gid_bit > tot_group) {
				tot_group = gid_bit;
			}
			/* And assign the final smooth group id to that poly group! */
			for (i = ps_end_idx, p = poly_stack; i--; p++) {
				poly_groups[*p] = poly_group_id;
			}
		}
	}

	if (use_bitflags) {
		/* used bits are zero-based. */
		tot_group++;
	}

	if (UNLIKELY(group_id_overflow)) {
		int i = totpoly, *gid = poly_groups;
		for (; i--; gid++) {
			if (*gid == poly_group_id_overflowed) {
				*gid = 0;
			}
		}
		/* Using 0 as group id adds one more group! */
		tot_group++;
	}

	if (edge_poly_mem) {
		MEM_freeN(edge_poly_map);
		MEM_freeN(edge_poly_mem);
	}
	MEM_freeN(poly_stack);

	*r_totgroup = tot_group;
	*r_poly_groups = poly_groups;
	if (r_edge_borders) {
		*r_edge_borders = edge_borders;
		*r_totedgeborder = num_edgeborders;
	}
}

static bool poly_is_island_boundary_smooth_cb(
        const MPoly *mp, const MLoop *UNUSED(ml), const MEdge *me, const int nbr_egde_users, void *UNUSED(user_data))
{
	/* Edge is sharp if its poly is sharp, or edge itself is sharp, or edge is not used by exactly two polygons. */
	return (!(mp->flag & ME_SMOOTH) || (me->flag & ME_SHARP) || (nbr_egde_users != 2));
}

/**
 * Calculate smooth groups from sharp edges.
 *
 * \param r_totgroup The total number of groups, 1 or more.
 * \return Polygon aligned array of group index values (bitflags if use_bitflags is true), starting at 1
 *         (0 being used as 'invalid' flag).
 *         Note it's callers's responsibility to MEM_freeN returned array.
 */
int *BKE_mesh_calc_smoothgroups(
        const MEdge *medge, const int totedge,
        const MPoly *mpoly, const int totpoly,
        const MLoop *mloop, const int totloop,
        int *r_totgroup, const bool use_bitflags)
{
	int *poly_groups = NULL;

	poly_edge_loop_islands_calc(
	        medge, totedge, mpoly, totpoly, mloop, totloop, NULL, use_bitflags,
	        poly_is_island_boundary_smooth_cb, NULL, &poly_groups, r_totgroup, NULL, NULL);

	return poly_groups;
}

#define MISLAND_DEFAULT_BUFSIZE 64

void BKE_mesh_loop_islands_init(
        MeshIslandStore *island_store,
        const short item_type, const int items_num, const short island_type, const short innercut_type)
{
	MemArena *mem = island_store->mem;

	if (mem == NULL) {
		mem = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
		island_store->mem = mem;
	}
	/* else memarena should be cleared */

	BLI_assert(ELEM(item_type, MISLAND_TYPE_VERT, MISLAND_TYPE_EDGE, MISLAND_TYPE_POLY, MISLAND_TYPE_LOOP));
	BLI_assert(ELEM(island_type, MISLAND_TYPE_VERT, MISLAND_TYPE_EDGE, MISLAND_TYPE_POLY, MISLAND_TYPE_LOOP));

	island_store->item_type = item_type;
	island_store->items_to_islands_num = items_num;
	island_store->items_to_islands = BLI_memarena_alloc(mem, sizeof(*island_store->items_to_islands) * (size_t)items_num);

	island_store->island_type = island_type;
	island_store->islands_num_alloc = MISLAND_DEFAULT_BUFSIZE;
	island_store->islands = BLI_memarena_alloc(mem, sizeof(*island_store->islands) * island_store->islands_num_alloc);

	island_store->innercut_type = innercut_type;
	island_store->innercuts = BLI_memarena_alloc(mem, sizeof(*island_store->innercuts) * island_store->islands_num_alloc);
}

void BKE_mesh_loop_islands_clear(MeshIslandStore *island_store)
{
	island_store->item_type = MISLAND_TYPE_NONE;
	island_store->items_to_islands_num = 0;
	island_store->items_to_islands = NULL;

	island_store->island_type = MISLAND_TYPE_NONE;
	island_store->islands_num = 0;
	island_store->islands = NULL;

	island_store->innercut_type = MISLAND_TYPE_NONE;
	island_store->innercuts = NULL;

	if (island_store->mem) {
		BLI_memarena_clear(island_store->mem);
	}

	island_store->islands_num_alloc = 0;
}

void BKE_mesh_loop_islands_free(MeshIslandStore *island_store)
{
	if (island_store->mem) {
		BLI_memarena_free(island_store->mem);
		island_store->mem = NULL;
	}
}

void BKE_mesh_loop_islands_add(
        MeshIslandStore *island_store, const int item_num, int *items_indices,
        const int num_island_items, int *island_item_indices,
        const int num_innercut_items, int *innercut_item_indices)
{
	MemArena *mem = island_store->mem;

	MeshElemMap *isld, *innrcut;
	const int curr_island_idx = island_store->islands_num++;
	const size_t curr_num_islands = (size_t)island_store->islands_num;
	int i = item_num;

	while (i--) {
		island_store->items_to_islands[items_indices[i]] = curr_island_idx;
	}

	if (UNLIKELY(curr_num_islands > island_store->islands_num_alloc)) {
		MeshElemMap **islds, **innrcuts;

		island_store->islands_num_alloc *= 2;
		islds = BLI_memarena_alloc(mem, sizeof(*islds) * island_store->islands_num_alloc);
		memcpy(islds, island_store->islands, sizeof(*islds) * (curr_num_islands - 1));
		island_store->islands = islds;

		innrcuts = BLI_memarena_alloc(mem, sizeof(*innrcuts) * island_store->islands_num_alloc);
		memcpy(innrcuts, island_store->innercuts, sizeof(*innrcuts) * (curr_num_islands - 1));
		island_store->innercuts = innrcuts;
	}

	island_store->islands[curr_island_idx] = isld = BLI_memarena_alloc(mem, sizeof(*isld));
	isld->count = num_island_items;
	isld->indices = BLI_memarena_alloc(mem, sizeof(*isld->indices) * (size_t)num_island_items);
	memcpy(isld->indices, island_item_indices, sizeof(*isld->indices) * (size_t)num_island_items);

	island_store->innercuts[curr_island_idx] = innrcut = BLI_memarena_alloc(mem, sizeof(*innrcut));
	innrcut->count = num_innercut_items;
	innrcut->indices = BLI_memarena_alloc(mem, sizeof(*innrcut->indices) * (size_t)num_innercut_items);
	memcpy(innrcut->indices, innercut_item_indices, sizeof(*innrcut->indices) * (size_t)num_innercut_items);
}

/* TODO: I'm not sure edge seam flag is enough to define UV islands? Maybe we should also consider UVmaps values
 *       themselves (i.e. different UV-edges for a same mesh-edge => boundary edge too?).
 *       Would make things much more complex though, and each UVMap would then need its own mesh mapping,
 *       not sure we want that at all!
 */
typedef struct MeshCheckIslandBoundaryUv {
	const MLoop *loops;
	const MLoopUV *luvs;
	const MeshElemMap *edge_loop_map;
} MeshCheckIslandBoundaryUv;

static bool mesh_check_island_boundary_uv(
        const MPoly *UNUSED(mp), const MLoop *ml, const MEdge *me,
        const int UNUSED(nbr_egde_users), void *user_data)
{
	if (user_data) {
		const MeshCheckIslandBoundaryUv *data = user_data;
		const MLoop *loops = data->loops;
		const MLoopUV *luvs = data->luvs;
		const MeshElemMap *edge_to_loops = &data->edge_loop_map[ml->e];

		BLI_assert(edge_to_loops->count >= 2 && (edge_to_loops->count % 2) == 0);

		const unsigned int v1 = loops[edge_to_loops->indices[0]].v;
		const unsigned int v2 = loops[edge_to_loops->indices[1]].v;
		const float *uvco_v1 = luvs[edge_to_loops->indices[0]].uv;
		const float *uvco_v2 = luvs[edge_to_loops->indices[1]].uv;
		for (int i = 2; i < edge_to_loops->count; i += 2) {
			if (loops[edge_to_loops->indices[i]].v == v1) {
				if (!equals_v2v2(uvco_v1, luvs[edge_to_loops->indices[i]].uv) ||
				    !equals_v2v2(uvco_v2, luvs[edge_to_loops->indices[i + 1]].uv))
				{
					return true;
				}
			}
			else {
				BLI_assert(loops[edge_to_loops->indices[i]].v == v2);
				UNUSED_VARS_NDEBUG(v2);
				if (!equals_v2v2(uvco_v2, luvs[edge_to_loops->indices[i]].uv) ||
				    !equals_v2v2(uvco_v1, luvs[edge_to_loops->indices[i + 1]].uv))
				{
					return true;
				}
			}
		}
		return false;
	}
	else {
		/* Edge is UV boundary if tagged as seam. */
		return (me->flag & ME_SEAM) != 0;
	}
}

static bool mesh_calc_islands_loop_poly_uv(
        MVert *UNUSED(verts), const int UNUSED(totvert),
        MEdge *edges, const int totedge,
        MPoly *polys, const int totpoly,
        MLoop *loops, const int totloop,
        const MLoopUV *luvs,
        MeshIslandStore *r_island_store)
{
	int *poly_groups = NULL;
	int num_poly_groups;

	/* map vars */
	MeshElemMap *edge_poly_map;
	int *edge_poly_mem;

	MeshElemMap *edge_loop_map;
	int *edge_loop_mem;

	MeshCheckIslandBoundaryUv edge_boundary_check_data;

	int *poly_indices;
	int *loop_indices;
	int num_pidx, num_lidx;

	/* Those are used to detect 'inner cuts', i.e. edges that are borders, and yet have two or more polys of
	 * a same group using them (typical case: seam used to unwrap properly a cylinder). */
	BLI_bitmap *edge_borders = NULL;
	int num_edge_borders = 0;
	char *edge_border_count = NULL;
	int *edge_innercut_indices = NULL;
	int num_einnercuts = 0;

	int grp_idx, p_idx, pl_idx, l_idx;

	BKE_mesh_loop_islands_clear(r_island_store);
	BKE_mesh_loop_islands_init(r_island_store, MISLAND_TYPE_LOOP, totloop, MISLAND_TYPE_POLY, MISLAND_TYPE_EDGE);

	BKE_mesh_edge_poly_map_create(
	        &edge_poly_map, &edge_poly_mem,
	        edges, totedge, polys, totpoly, loops, totloop);

	if (luvs) {
		BKE_mesh_edge_loop_map_create(
		        &edge_loop_map, &edge_loop_mem,
		        edges, totedge, polys, totpoly, loops, totloop);
		edge_boundary_check_data.loops = loops;
		edge_boundary_check_data.luvs = luvs;
		edge_boundary_check_data.edge_loop_map = edge_loop_map;
	}

	poly_edge_loop_islands_calc(
	        edges, totedge, polys, totpoly, loops, totloop, edge_poly_map, false,
	        mesh_check_island_boundary_uv, luvs ? &edge_boundary_check_data : NULL,
	        &poly_groups, &num_poly_groups, &edge_borders, &num_edge_borders);

	if (!num_poly_groups) {
		/* Should never happen... */
		MEM_freeN(edge_poly_map);
		MEM_freeN(edge_poly_mem);

		if (edge_borders) {
			MEM_freeN(edge_borders);
		}
		return false;
	}

	if (num_edge_borders) {
		edge_border_count = MEM_mallocN(sizeof(*edge_border_count) * (size_t)totedge, __func__);
		edge_innercut_indices = MEM_mallocN(sizeof(*edge_innercut_indices) * (size_t)num_edge_borders, __func__);
	}

	poly_indices = MEM_mallocN(sizeof(*poly_indices) * (size_t)totpoly, __func__);
	loop_indices = MEM_mallocN(sizeof(*loop_indices) * (size_t)totloop, __func__);

	/* Note: here we ignore '0' invalid group - this should *never* happen in this case anyway? */
	for (grp_idx = 1; grp_idx <= num_poly_groups; grp_idx++) {
		num_pidx = num_lidx = 0;
		if (num_edge_borders) {
			num_einnercuts = 0;
			memset(edge_border_count, 0, sizeof(*edge_border_count) * (size_t)totedge);
		}

		for (p_idx = 0; p_idx < totpoly; p_idx++) {
			MPoly *mp;

			if (poly_groups[p_idx] != grp_idx) {
				continue;
			}

			mp = &polys[p_idx];
			poly_indices[num_pidx++] = p_idx;
			for (l_idx = mp->loopstart, pl_idx = 0; pl_idx < mp->totloop; l_idx++, pl_idx++) {
				MLoop *ml = &loops[l_idx];
				loop_indices[num_lidx++] = l_idx;
				if (num_edge_borders && BLI_BITMAP_TEST(edge_borders, ml->e) && (edge_border_count[ml->e] < 2)) {
					edge_border_count[ml->e]++;
					if (edge_border_count[ml->e] == 2) {
						edge_innercut_indices[num_einnercuts++] = (int)ml->e;
					}
				}
			}
		}

		BKE_mesh_loop_islands_add(
		        r_island_store, num_lidx, loop_indices, num_pidx, poly_indices,
		        num_einnercuts, edge_innercut_indices);
	}

	MEM_freeN(edge_poly_map);
	MEM_freeN(edge_poly_mem);

	if (luvs) {
		MEM_freeN(edge_loop_map);
		MEM_freeN(edge_loop_mem);
	}

	MEM_freeN(poly_indices);
	MEM_freeN(loop_indices);
	MEM_freeN(poly_groups);

	if (edge_borders) {
		MEM_freeN(edge_borders);
	}

	if (num_edge_borders) {
		MEM_freeN(edge_border_count);
		MEM_freeN(edge_innercut_indices);
	}
	return true;
}

/**
 * Calculate 'generic' UV islands, i.e. based only on actual geometry data (edge seams), not some UV layers coordinates.
 */
bool BKE_mesh_calc_islands_loop_poly_edgeseam(
        MVert *verts, const int totvert,
        MEdge *edges, const int totedge,
        MPoly *polys, const int totpoly,
        MLoop *loops, const int totloop,
        MeshIslandStore *r_island_store)
{
	return mesh_calc_islands_loop_poly_uv(
	        verts, totvert, edges, totedge, polys, totpoly, loops, totloop, NULL, r_island_store);
}

/**
 * Calculate UV islands.
 *
 * \note If no MLoopUV layer is passed, we only consider edges tagged as seams as UV boundaries.
 *     This has the advantages of simplicity, and being valid/common to all UV maps.
 *     However, it means actual UV islands whithout matching UV seams will not be handled correctly...
 *     If a valid UV layer is passed as \a luvs parameter, UV coordinates are also used to detect islands boundaries.
 *
 * \note All this could be optimized...
 *     Not sure it would be worth the more complex code, though, those loops are supposed to be really quick to do...
 */
bool BKE_mesh_calc_islands_loop_poly_uvmap(
        MVert *verts, const int totvert,
        MEdge *edges, const int totedge,
        MPoly *polys, const int totpoly,
        MLoop *loops, const int totloop,
        const MLoopUV *luvs,
        MeshIslandStore *r_island_store)
{
	BLI_assert(luvs != NULL);
	return mesh_calc_islands_loop_poly_uv(
	        verts, totvert, edges, totedge, polys, totpoly, loops, totloop, luvs, r_island_store);
}

/** \} */
