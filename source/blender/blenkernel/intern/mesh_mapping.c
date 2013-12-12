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

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BKE_mesh_mapping.h"
#include "BKE_customdata.h"

#include "BLI_strict_flags.h"


/* -------------------------------------------------------------------- */

/** \name Mesh Connectivity Mapping
 * \{ */


/* ngon version wip, based on BM_uv_vert_map_create */
/* this replaces the non bmesh function (in trunk) which takes MTFace's, if we ever need it back we could
 * but for now this replaces it because its unused. */

UvVertMap *BKE_mesh_uv_vert_map_create(struct MPoly *mpoly, struct MLoop *mloop, struct MLoopUV *mloopuv,
                                       unsigned int totpoly, unsigned int totvert, int selected, float *limit)
{
	UvVertMap *vmap;
	UvMapVert *buf;
	MPoly *mp;
	unsigned int a;
	int i, totuv, nverts;

	totuv = 0;

	/* generate UvMapVert array */
	mp = mpoly;
	for (a = 0; a < totpoly; a++, mp++)
		if (!selected || (!(mp->flag & ME_HIDE) && (mp->flag & ME_FACE_SEL)))
			totuv += mp->totloop;

	if (totuv == 0)
		return NULL;

	vmap = (UvVertMap *)MEM_callocN(sizeof(*vmap), "UvVertMap");
	if (!vmap)
		return NULL;

	vmap->vert = (UvMapVert **)MEM_callocN(sizeof(*vmap->vert) * totvert, "UvMapVert*");
	buf = vmap->buf = (UvMapVert *)MEM_callocN(sizeof(*vmap->buf) * (size_t)totuv, "UvMapVert");

	if (!vmap->vert || !vmap->buf) {
		BKE_mesh_uv_vert_map_free(vmap);
		return NULL;
	}

	mp = mpoly;
	for (a = 0; a < totpoly; a++, mp++) {
		if (!selected || (!(mp->flag & ME_HIDE) && (mp->flag & ME_FACE_SEL))) {
			nverts = mp->totloop;

			for (i = 0; i < nverts; i++) {
				buf->tfindex = (unsigned char)i;
				buf->f = a;
				buf->separate = 0;
				buf->next = vmap->vert[mloop[mp->loopstart + i].v];
				vmap->vert[mloop[mp->loopstart + i].v] = buf;
				buf++;
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

			uv = mloopuv[mpoly[v->f].loopstart + v->tfindex].uv;
			lastv = NULL;
			iterv = vlist;

			while (iterv) {
				next = iterv->next;

				uv2 = mloopuv[mpoly[iterv->f].loopstart + iterv->tfindex].uv;
				sub_v2_v2v2(uvdiff, uv2, uv);


				if (fabsf(uv[0] - uv2[0]) < limit[0] && fabsf(uv[1] - uv2[1]) < limit[1]) {
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

/* Generates a map where the key is the vertex and the value is a list
 * of polys that use that vertex as a corner. The lists are allocated
 * from one memory pool. */
void BKE_mesh_vert_poly_map_create(MeshElemMap **r_map, int **r_mem,
                                   const MPoly *mpoly, const MLoop *mloop,
                                   int totvert, int totpoly, int totloop)
{
	MeshElemMap *map = MEM_callocN(sizeof(MeshElemMap) * (size_t)totvert, "vert poly map");
	int *indices, *index_iter;
	int i, j;

	indices = index_iter = MEM_mallocN(sizeof(int) * (size_t)totloop, "vert poly map mem");

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

			map[v].indices[map[v].count] = i;
			map[v].count++;
		}
	}

	*r_map = map;
	*r_mem = indices;
}

/* Generates a map where the key is the vertex and the value is a list
 * of edges that use that vertex as an endpoint. The lists are allocated
 * from one memory pool. */
void BKE_mesh_vert_edge_map_create(MeshElemMap **r_map, int **r_mem,
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

void BKE_mesh_edge_poly_map_create(MeshElemMap **r_map, int **r_mem,
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
void BKE_mesh_origindex_map_create(MeshElemMap **r_map, int **r_mem,
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

/** \} */



/* -------------------------------------------------------------------- */

/** \name Mesh Smooth Groups
 * \{ */

/**
 * Calculate smooth groups from sharp edges.
 *
 * \param r_totgroup The total number of groups, 1 or more.
 * \return Polygon aligned array of group index values (bitflags if use_bitflags is true), starting at 1.
 */
int *BKE_mesh_calc_smoothgroups(const MEdge *medge, const int totedge,
                                const MPoly *mpoly, const int totpoly,
                                const MLoop *mloop, const int totloop,
                                int *r_totgroup, const bool use_bitflags)
{
	int *poly_groups;
	int *poly_stack;

	int poly_prev = 0;
	const int temp_poly_group_id = 3;  /* Placeholder value. */
	const int poly_group_id_overflowed = 5;  /* Group we could not find any available bit, will be reset to 0 at end */
	int tot_group = 0;
	bool group_id_overflow = false;

	/* map vars */
	MeshElemMap *edge_poly_map;
	int *edge_poly_mem;

	if (totpoly == 0) {
		*r_totgroup = 0;
		return NULL;
	}

	BKE_mesh_edge_poly_map_create(&edge_poly_map, &edge_poly_mem,
	                              medge, totedge,
	                              mpoly, totpoly,
	                              mloop, totloop);

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
				const MeshElemMap *map_ele = &edge_poly_map[ml->e];
				int *p = map_ele->indices;
				int i = map_ele->count;
				if (!(medge[ml->e].flag & ME_SHARP)) {
					for (; i--; p++) {
						/* if we meet other non initialized its a bug */
						BLI_assert(ELEM(poly_groups[*p], 0, poly_group_id));

						if (poly_groups[*p] == 0) {
							poly_groups[*p] = poly_group_id;
							poly_stack[ps_end_idx++] = *p;
						}
					}
				}
				else if (use_bitflags) {
					/* Find contiguous smooth groups already assigned, these are the values we can't reuse! */
					for (; i--; p++) {
						int bit = poly_groups[*p];
						if (!ELEM3(bit, 0, poly_group_id, poly_group_id_overflowed) &&
						    !(bit_poly_group_mask & bit))
						{
							bit_poly_group_mask |= bit;
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

	if (UNLIKELY(group_id_overflow)) {
		int i = totpoly, *gid = poly_groups;
		for (; i--; gid++) {
			if (*gid == poly_group_id_overflowed) {
				*gid = 0;
			}
		}
	}

	MEM_freeN(edge_poly_map);
	MEM_freeN(edge_poly_mem);
	MEM_freeN(poly_stack);

	*r_totgroup = tot_group + 1;

	return poly_groups;
}
/** \} */
