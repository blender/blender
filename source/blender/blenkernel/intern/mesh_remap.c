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

/** \file blender/blenkernel/intern/mesh_remap.c
 *  \ingroup bke
 *
 * Functions for mapping data between meshes.
 */

#include <limits.h>

#include "MEM_guardedalloc.h"

#include "DNA_meshdata_types.h"

#include "BLI_utildefines.h"
#include "BLI_alloca.h"
#include "BLI_astar.h"
#include "BLI_bitmap.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_polyfill2d.h"
#include "BLI_rand.h"

#include "BKE_bvhutils.h"
#include "BKE_customdata.h"
#include "BKE_DerivedMesh.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_mesh_remap.h"  /* own include */

#include "BLI_strict_flags.h"


/* -------------------------------------------------------------------- */

/** \name Mesh to mesh mapping
 * \{ */

void BKE_mesh_remap_init(MeshPairRemap *map, const int items_num)
{
	MemArena *mem = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);

	BKE_mesh_remap_free(map);

	map->items = BLI_memarena_alloc(mem, sizeof(*map->items) * (size_t)items_num);
	map->items_num = items_num;

	map->mem = mem;
}

void BKE_mesh_remap_free(MeshPairRemap *map)
{
	if (map->mem) {
		BLI_memarena_free((MemArena *)map->mem);
	}

	map->items_num = 0;
	map->items = NULL;
	map->mem = NULL;
}

static void mesh_remap_item_define(
        MeshPairRemap *map, const int index, const float UNUSED(hit_dist), const int island,
        const int sources_num, const int *indices_src, const float *weights_src)
{
	MeshPairRemapItem *mapit = &map->items[index];
	MemArena *mem = map->mem;

	if (sources_num) {
		mapit->sources_num = sources_num;
		mapit->indices_src = BLI_memarena_alloc(mem, sizeof(*mapit->indices_src) * (size_t)sources_num);
		memcpy(mapit->indices_src, indices_src, sizeof(*mapit->indices_src) * (size_t)sources_num);
		mapit->weights_src = BLI_memarena_alloc(mem, sizeof(*mapit->weights_src) * (size_t)sources_num);
		memcpy(mapit->weights_src, weights_src, sizeof(*mapit->weights_src) * (size_t)sources_num);
	}
	else {
		mapit->sources_num = 0;
		mapit->indices_src = NULL;
		mapit->weights_src = NULL;
	}
	/* UNUSED */
	// mapit->hit_dist = hit_dist;
	mapit->island = island;
}

void BKE_mesh_remap_item_define_invalid(MeshPairRemap *map, const int index)
{
	mesh_remap_item_define(map, index, FLT_MAX, 0, 0, NULL, NULL);
}

static int mesh_remap_interp_poly_data_get(
        const MPoly *mp, MLoop *mloops, const float (*vcos_src)[3], const float point[3],
        size_t *buff_size, float (**vcos)[3], const bool use_loops, int **indices, float **weights,
        const bool do_weights, int *r_closest_index)
{
	MLoop *ml;
	float (*vco)[3];
	float ref_dist_sq = FLT_MAX;
	int *index;
	const int sources_num = mp->totloop;
	int i;

	if ((size_t)sources_num > *buff_size) {
		*buff_size = (size_t)sources_num;
		*vcos = MEM_reallocN(*vcos, sizeof(**vcos) * *buff_size);
		*indices = MEM_reallocN(*indices, sizeof(**indices) * *buff_size);
		if (do_weights) {
			*weights = MEM_reallocN(*weights, sizeof(**weights) * *buff_size);
		}
	}

	for (i = 0, ml = &mloops[mp->loopstart], vco = *vcos, index = *indices; i < sources_num; i++, ml++, vco++, index++) {
		*index = use_loops ? (int)mp->loopstart + i : (int)ml->v;
		copy_v3_v3(*vco, vcos_src[ml->v]);
		if (r_closest_index) {
			/* Find closest vert/loop in this case. */
			const float dist_sq = len_squared_v3v3(point, *vco);
			if (dist_sq < ref_dist_sq) {
				ref_dist_sq = dist_sq;
				*r_closest_index = *index;
			}
		}
	}

	if (do_weights) {
		interp_weights_poly_v3(*weights, *vcos, sources_num, point);
	}

	return sources_num;
}

static bool mesh_remap_bvhtree_query_nearest(
        BVHTreeFromMesh *treedata, BVHTreeNearest *nearest, const SpaceTransform *space_transform,
        const float in_co[3], const float max_dist_sq,
        float *r_hit_dist)
{
	float co[3];

	copy_v3_v3(co, in_co);

	/* Convert the vertex to tree coordinates, if needed. */
	if (space_transform) {
		BLI_space_transform_apply(space_transform, co);
	}

	/* Use local proximity heuristics (to reduce the nearest search). */
	if (nearest->index != -1) {
		nearest->dist_sq = min_ff(len_squared_v3v3(co, nearest->co), max_dist_sq);
	}
	else {
		nearest->dist_sq = max_dist_sq;
	}
	/* Compute and store result. If invalid (-1 index), keep FLT_MAX dist. */
	BLI_bvhtree_find_nearest(treedata->tree, co, nearest, treedata->nearest_callback, treedata);

	if ((nearest->index != -1) && (nearest->dist_sq <= max_dist_sq)) {
		*r_hit_dist = sqrtf(nearest->dist_sq);
		return true;
	}
	else {
		return false;
	}
}

static bool mesh_remap_bvhtree_query_raycast(
        BVHTreeFromMesh *treedata, BVHTreeRayHit *rayhit, const SpaceTransform *space_transform,
        const float in_co[3], const float in_no[3], const float radius, const float max_dist,
        float *r_hit_dist)
{
	BVHTreeRayHit rayhit_tmp;
	float co[3], no[3];

	copy_v3_v3(co, in_co);
	copy_v3_v3(no, in_no);

	/* Convert the vertex to tree coordinates, if needed. */
	if (space_transform) {
		BLI_space_transform_apply(space_transform, co);
		BLI_space_transform_apply_normal(space_transform, no);
	}

	rayhit->index = -1;
	rayhit->dist = max_dist;
	BLI_bvhtree_ray_cast(treedata->tree, co, no, radius, rayhit, treedata->raycast_callback, treedata);

	/* Also cast in the other direction! */
	rayhit_tmp = *rayhit;
	negate_v3(no);
	BLI_bvhtree_ray_cast(treedata->tree, co, no, radius, &rayhit_tmp, treedata->raycast_callback, treedata);
	if (rayhit_tmp.dist < rayhit->dist) {
		*rayhit = rayhit_tmp;
	}

	if ((rayhit->index != -1) && (rayhit->dist <= max_dist)) {
		*r_hit_dist = rayhit->dist;
		return true;
	}
	else {
		return false;
	}
}

/* Little helper when dealing with source islands */
typedef struct IslandResult {
	float factor;           /* A factor, based on which best island for a given set of elements will be selected. */
	int   index_src;        /* Index of the source. */
	float hit_dist;         /* The actual hit distance. */
	float hit_point[3];     /* The hit point, if relevant. */
} IslandResult;

/* Note about all bvh/raycasting stuff below:
 *      * We must use our ray radius as BVH epsilon too, else rays not hitting anything but 'passing near' an item
 *        would be missed (since BVH handling would not detect them, 'refining' callbacks won't be executed,
 *        even though they would return a valid hit).
 *      * However, in 'islands' case where each hit gets a weight, 'precise' hits should have a better weight than
 *        'approximate' hits. To address that, we simplify things with:
 *        ** A first raycast with default, given rayradius;
 *        ** If first one fails, we do more raycasting with bigger radius, but if hit is found
 *           it will get smaller weight.
 *        This only concerns loops, currently (because of islands), and 'sampled' edges/polys norproj.
 */

/* At most n raycasts per 'real' ray. */
#define MREMAP_RAYCAST_APPROXIMATE_NR 3
/* Each approximated raycasts will have n times bigger radius than previous one. */
#define MREMAP_RAYCAST_APPROXIMATE_FAC 5.0f
/* BVH epsilon value we have to give to bvh 'constructor' when doing approximated raycasting. */
#define MREMAP_RAYCAST_APPROXIMATE_BVHEPSILON(_ray_radius) \
	((float)MREMAP_RAYCAST_APPROXIMATE_NR * MREMAP_RAYCAST_APPROXIMATE_FAC * (_ray_radius))

/* min 16 rays/face, max 400. */
#define MREMAP_RAYCAST_TRI_SAMPLES_MIN 4
#define MREMAP_RAYCAST_TRI_SAMPLES_MAX 20

/* Will be enough in 99% of cases. */
#define MREMAP_DEFAULT_BUFSIZE 32

void BKE_mesh_remap_calc_verts_from_dm(
        const int mode, const SpaceTransform *space_transform, const float max_dist, const float ray_radius,
        const MVert *verts_dst, const int numverts_dst, const bool UNUSED(dirty_nors_dst), DerivedMesh *dm_src,
        MeshPairRemap *r_map)
{
	const float full_weight = 1.0f;
	const float max_dist_sq = max_dist * max_dist;
	int i;

	BLI_assert(mode & MREMAP_MODE_VERT);

	BKE_mesh_remap_init(r_map, numverts_dst);

	if (mode == MREMAP_MODE_TOPOLOGY) {
		BLI_assert(numverts_dst == dm_src->getNumVerts(dm_src));
		for (i = 0; i < numverts_dst; i++) {
			mesh_remap_item_define(r_map, i, FLT_MAX, 0, 1, &i, &full_weight);
		}
	}
	else {
		BVHTreeFromMesh treedata = {NULL};
		BVHTreeNearest nearest = {0};
		BVHTreeRayHit rayhit = {0};
		float hit_dist;

		if (mode == MREMAP_MODE_VERT_NEAREST) {
			bvhtree_from_mesh_verts(&treedata, dm_src, 0.0f, 2, 6);
			nearest.index = -1;

			for (i = 0; i < numverts_dst; i++) {
				float tmp_co[3];

				copy_v3_v3(tmp_co, verts_dst[i].co);

				if (mesh_remap_bvhtree_query_nearest(
				        &treedata, &nearest, space_transform,
				        tmp_co, max_dist_sq, &hit_dist))
				{
					mesh_remap_item_define(r_map, i, hit_dist, 0, 1, &nearest.index, &full_weight);
				}
				else {
					/* No source for this dest vertex! */
					BKE_mesh_remap_item_define_invalid(r_map, i);
				}
			}
		}
		else if (ELEM(mode, MREMAP_MODE_VERT_EDGE_NEAREST, MREMAP_MODE_VERT_EDGEINTERP_NEAREST)) {
			MEdge *edges_src = dm_src->getEdgeArray(dm_src);
			float (*vcos_src)[3] = MEM_mallocN(sizeof(*vcos_src) * (size_t)dm_src->getNumVerts(dm_src), __func__);
			dm_src->getVertCos(dm_src, vcos_src);

			bvhtree_from_mesh_edges(&treedata, dm_src, 0.0f, 2, 6);
			nearest.index = -1;

			for (i = 0; i < numverts_dst; i++) {
				float tmp_co[3];

				copy_v3_v3(tmp_co, verts_dst[i].co);

				if (mesh_remap_bvhtree_query_nearest(
				        &treedata, &nearest, space_transform,
				        tmp_co, max_dist_sq, &hit_dist))
				{
					MEdge *me = &edges_src[nearest.index];
					const float *v1cos = vcos_src[me->v1];
					const float *v2cos = vcos_src[me->v2];

					if (mode == MREMAP_MODE_VERT_EDGE_NEAREST) {
						const float dist_v1 = len_squared_v3v3(tmp_co, v1cos);
						const float dist_v2 = len_squared_v3v3(tmp_co, v2cos);
						const int index = (int)((dist_v1 > dist_v2) ? me->v2 : me->v1);
						mesh_remap_item_define(r_map, i, hit_dist, 0, 1, &index, &full_weight);
					}
					else if (mode == MREMAP_MODE_VERT_EDGEINTERP_NEAREST) {
						int indices[2];
						float weights[2];

						indices[0] = (int)me->v1;
						indices[1] = (int)me->v2;

						/* Weight is inverse of point factor here... */
						weights[0] = line_point_factor_v3(tmp_co, v2cos, v1cos);
						CLAMP(weights[0], 0.0f, 1.0f);
						weights[1] = 1.0f - weights[0];

						mesh_remap_item_define(r_map, i, hit_dist, 0, 2, indices, weights);
					}
				}
				else {
					/* No source for this dest vertex! */
					BKE_mesh_remap_item_define_invalid(r_map, i);
				}
			}

			MEM_freeN(vcos_src);
		}
		else if (ELEM(mode, MREMAP_MODE_VERT_POLY_NEAREST, MREMAP_MODE_VERT_POLYINTERP_NEAREST,
		                    MREMAP_MODE_VERT_POLYINTERP_VNORPROJ))
		{
			MPoly *polys_src = dm_src->getPolyArray(dm_src);
			MLoop *loops_src = dm_src->getLoopArray(dm_src);
			float (*vcos_src)[3] = MEM_mallocN(sizeof(*vcos_src) * (size_t)dm_src->getNumVerts(dm_src), __func__);
			int *tessface_to_poly_map_src;

			size_t tmp_buff_size = MREMAP_DEFAULT_BUFSIZE;
			float (*vcos)[3] = MEM_mallocN(sizeof(*vcos) * tmp_buff_size, __func__);
			int *indices = MEM_mallocN(sizeof(*indices) * tmp_buff_size, __func__);
			float *weights = MEM_mallocN(sizeof(*weights) * tmp_buff_size, __func__);

			dm_src->getVertCos(dm_src, vcos_src);
			bvhtree_from_mesh_faces(&treedata, dm_src, (mode & MREMAP_USE_NORPROJ) ? ray_radius : 0.0f, 2, 6);
			/* bvhtree here uses tesselated faces... */
			tessface_to_poly_map_src = dm_src->getTessFaceDataArray(dm_src, CD_ORIGINDEX);

			if (mode == MREMAP_MODE_VERT_POLYINTERP_VNORPROJ) {
				for (i = 0; i < numverts_dst; i++) {
					float tmp_co[3], tmp_no[3];

					copy_v3_v3(tmp_co, verts_dst[i].co);
					normal_short_to_float_v3(tmp_no, verts_dst[i].no);

					if (mesh_remap_bvhtree_query_raycast(
					        &treedata, &rayhit, space_transform,
					        tmp_co, tmp_no, ray_radius, max_dist, &hit_dist))
					{
						MPoly *mp_src = &polys_src[tessface_to_poly_map_src[rayhit.index]];
						const int sources_num = mesh_remap_interp_poly_data_get(
						        mp_src, loops_src, (const float (*)[3])vcos_src, rayhit.co,
						        &tmp_buff_size, &vcos, false, &indices, &weights, true, NULL);

						mesh_remap_item_define(r_map, i, hit_dist, 0, sources_num, indices, weights);
					}
					else {
						/* No source for this dest vertex! */
						BKE_mesh_remap_item_define_invalid(r_map, i);
					}
				}
			}
			else {
				nearest.index = -1;

				for (i = 0; i < numverts_dst; i++) {
					float tmp_co[3];

					/* Convert the vertex to tree coordinates. */
					copy_v3_v3(tmp_co, verts_dst[i].co);

					if (mesh_remap_bvhtree_query_nearest(
					        &treedata, &nearest, space_transform,
					        tmp_co, max_dist_sq, &hit_dist))
					{
						MPoly *mp = &polys_src[tessface_to_poly_map_src[nearest.index]];

						if (mode == MREMAP_MODE_VERT_POLY_NEAREST) {
							int index;
							mesh_remap_interp_poly_data_get(
							        mp, loops_src, (const float (*)[3])vcos_src, nearest.co,
							        &tmp_buff_size, &vcos, false, &indices, &weights, false,
							        &index);

							mesh_remap_item_define(r_map, i, hit_dist, 0, 1, &index, &full_weight);
						}
						else if (mode == MREMAP_MODE_VERT_POLYINTERP_NEAREST) {
							const int sources_num = mesh_remap_interp_poly_data_get(
							        mp, loops_src, (const float (*)[3])vcos_src, nearest.co,
							        &tmp_buff_size, &vcos, false, &indices, &weights, true,
							        NULL);

							mesh_remap_item_define(r_map, i, hit_dist, 0, sources_num, indices, weights);
						}
					}
					else {
						/* No source for this dest vertex! */
						BKE_mesh_remap_item_define_invalid(r_map, i);
					}
				}
			}

			MEM_freeN(vcos_src);
			MEM_freeN(vcos);
			MEM_freeN(indices);
			MEM_freeN(weights);
		}
		else {
			printf("WARNING! Unsupported mesh-to-mesh vertex mapping mode (%d)!\n", mode);
			memset(r_map->items, 0, sizeof(*r_map->items) * (size_t)numverts_dst);
		}

		free_bvhtree_from_mesh(&treedata);
	}
}

void BKE_mesh_remap_calc_edges_from_dm(
        const int mode, const SpaceTransform *space_transform, const float max_dist, const float ray_radius,
        const MVert *verts_dst, const int numverts_dst, const MEdge *edges_dst, const int numedges_dst,
        const bool UNUSED(dirty_nors_dst), DerivedMesh *dm_src, MeshPairRemap *r_map)
{
	const float full_weight = 1.0f;
	const float max_dist_sq = max_dist * max_dist;
	int i;

	BLI_assert(mode & MREMAP_MODE_EDGE);

	BKE_mesh_remap_init(r_map, numedges_dst);

	if (mode == MREMAP_MODE_TOPOLOGY) {
		BLI_assert(numedges_dst == dm_src->getNumEdges(dm_src));
		for (i = 0; i < numedges_dst; i++) {
			mesh_remap_item_define(r_map, i, FLT_MAX, 0, 1, &i, &full_weight);
		}
	}
	else {
		BVHTreeFromMesh treedata = {NULL};
		BVHTreeNearest nearest = {0};
		BVHTreeRayHit rayhit = {0};
		float hit_dist;

		if (mode == MREMAP_MODE_EDGE_VERT_NEAREST) {
			const int num_verts_src = dm_src->getNumVerts(dm_src);
			const int num_edges_src = dm_src->getNumEdges(dm_src);
			MEdge *edges_src = dm_src->getEdgeArray(dm_src);
			float (*vcos_src)[3] = MEM_mallocN(sizeof(*vcos_src) * (size_t)dm_src->getNumVerts(dm_src), __func__);

			MeshElemMap *vert_to_edge_src_map;
			int         *vert_to_edge_src_map_mem;

			struct {
				float hit_dist;
				int   index;
			} *v_dst_to_src_map = MEM_mallocN(sizeof(*v_dst_to_src_map) * (size_t)numverts_dst, __func__);

			for (i = 0; i < numverts_dst; i++) {
				v_dst_to_src_map[i].hit_dist = -1.0f;
			}

			BKE_mesh_vert_edge_map_create(&vert_to_edge_src_map, &vert_to_edge_src_map_mem,
			                              edges_src, num_verts_src, num_edges_src);

			dm_src->getVertCos(dm_src, vcos_src);

			bvhtree_from_mesh_verts(&treedata, dm_src, 0.0f, 2, 6);
			nearest.index = -1;

			for (i = 0; i < numedges_dst; i++) {
				const MEdge *e_dst = &edges_dst[i];
				float best_totdist = FLT_MAX;
				int best_eidx_src = -1;
				int j = 2;

				while (j--) {
					const unsigned int vidx_dst = j ? e_dst->v1 : e_dst->v2;

					/* Compute closest verts only once! */
					if (v_dst_to_src_map[vidx_dst].hit_dist == -1.0f) {
						float tmp_co[3];

						copy_v3_v3(tmp_co, verts_dst[vidx_dst].co);

						if (mesh_remap_bvhtree_query_nearest(
						        &treedata, &nearest, space_transform,
						        tmp_co, max_dist_sq, &hit_dist))
						{
							v_dst_to_src_map[vidx_dst].hit_dist = hit_dist;
							v_dst_to_src_map[vidx_dst].index = nearest.index;
						}
						else {
							/* No source for this dest vert! */
							v_dst_to_src_map[vidx_dst].hit_dist = FLT_MAX;
						}
					}
				}

				/* Now, check all source edges of closest sources vertices, and select the one giving the smallest
				 * total verts-to-verts distance. */
				for (j = 2; j--;) {
					const unsigned int vidx_dst = j ? e_dst->v1 : e_dst->v2;
					const float first_dist = v_dst_to_src_map[vidx_dst].hit_dist;
					const int vidx_src = v_dst_to_src_map[vidx_dst].index;
					int *eidx_src, k;

					if (vidx_src < 0) {
						continue;
					}

					eidx_src = vert_to_edge_src_map[vidx_src].indices;
					k = vert_to_edge_src_map[vidx_src].count;

					for (; k--; eidx_src++) {
						MEdge *e_src = &edges_src[*eidx_src];
						const float *other_co_src = vcos_src[BKE_mesh_edge_other_vert(e_src, vidx_src)];
						const float *other_co_dst = verts_dst[BKE_mesh_edge_other_vert(e_dst, (int)vidx_dst)].co;
						const float totdist = first_dist + len_v3v3(other_co_src, other_co_dst);

						if (totdist < best_totdist) {
							best_totdist = totdist;
							best_eidx_src = *eidx_src;
						}
					}
				}

				if (best_eidx_src >= 0) {
					const float *co1_src = vcos_src[edges_src[best_eidx_src].v1];
					const float *co2_src = vcos_src[edges_src[best_eidx_src].v2];
					const float *co1_dst = verts_dst[e_dst->v1].co;
					const float *co2_dst = verts_dst[e_dst->v2].co;
					float co_src[3], co_dst[3];

					/* TODO: would need an isect_seg_seg_v3(), actually! */
					const int isect_type = isect_line_line_v3(co1_src, co2_src, co1_dst, co2_dst, co_src, co_dst);
					if (isect_type != 0) {
						const float fac_src = line_point_factor_v3(co_src, co1_src, co2_src);
						const float fac_dst = line_point_factor_v3(co_dst, co1_dst, co2_dst);
						if (fac_src < 0.0f) {
							copy_v3_v3(co_src, co1_src);
						}
						else if (fac_src > 1.0f) {
							copy_v3_v3(co_src, co2_src);
						}
						if (fac_dst < 0.0f) {
							copy_v3_v3(co_dst, co1_dst);
						}
						else if (fac_dst > 1.0f) {
							copy_v3_v3(co_dst, co2_dst);
						}
					}
					hit_dist = len_v3v3(co_dst, co_src);
					mesh_remap_item_define(r_map, i, hit_dist, 0, 1, &best_eidx_src, &full_weight);
				}
				else {
					/* No source for this dest edge! */
					BKE_mesh_remap_item_define_invalid(r_map, i);
				}
			}

			MEM_freeN(vcos_src);
			MEM_freeN(v_dst_to_src_map);
			MEM_freeN(vert_to_edge_src_map);
			MEM_freeN(vert_to_edge_src_map_mem);
		}
		else if (mode == MREMAP_MODE_EDGE_NEAREST) {
			bvhtree_from_mesh_edges(&treedata, dm_src, 0.0f, 2, 6);
			nearest.index = -1;

			for (i = 0; i < numedges_dst; i++) {
				float tmp_co[3];

				interp_v3_v3v3(tmp_co, verts_dst[edges_dst[i].v1].co, verts_dst[edges_dst[i].v2].co, 0.5f);

				if (mesh_remap_bvhtree_query_nearest(
				        &treedata, &nearest, space_transform,
				        tmp_co, max_dist_sq, &hit_dist))
				{
					mesh_remap_item_define(r_map, i, hit_dist, 0, 1, &nearest.index, &full_weight);
				}
				else {
					/* No source for this dest edge! */
					BKE_mesh_remap_item_define_invalid(r_map, i);
				}
			}
		}
		else if (mode == MREMAP_MODE_EDGE_POLY_NEAREST) {
			MEdge *edges_src = dm_src->getEdgeArray(dm_src);
			MPoly *polys_src = dm_src->getPolyArray(dm_src);
			MLoop *loops_src = dm_src->getLoopArray(dm_src);
			float (*vcos_src)[3] = MEM_mallocN(sizeof(*vcos_src) * (size_t)dm_src->getNumVerts(dm_src), __func__);
			int *tessface_to_poly_map_src;

			dm_src->getVertCos(dm_src, vcos_src);
			bvhtree_from_mesh_faces(&treedata, dm_src, 0.0f, 2, 6);
			/* bvhtree here uses tesselated faces... */
			tessface_to_poly_map_src = dm_src->getTessFaceDataArray(dm_src, CD_ORIGINDEX);

			for (i = 0; i < numedges_dst; i++) {
				float tmp_co[3];

				interp_v3_v3v3(tmp_co, verts_dst[edges_dst[i].v1].co, verts_dst[edges_dst[i].v2].co, 0.5f);

				if (mesh_remap_bvhtree_query_nearest(
				        &treedata, &nearest, space_transform,
				        tmp_co, max_dist_sq, &hit_dist))
				{
					MPoly *mp_src = &polys_src[tessface_to_poly_map_src[nearest.index]];
					MLoop *ml_src = &loops_src[mp_src->loopstart];
					int nloops = mp_src->totloop;
					float best_dist_sq = FLT_MAX;
					int best_eidx_src = -1;

					for (; nloops--; ml_src++) {
						MEdge *me_src = &edges_src[ml_src->e];
						float *co1_src = vcos_src[me_src->v1];
						float *co2_src = vcos_src[me_src->v2];
						float co_src[3];
						float dist_sq;

						interp_v3_v3v3(co_src, co1_src, co2_src, 0.5f);
						dist_sq = len_squared_v3v3(tmp_co, co_src);
						if (dist_sq < best_dist_sq) {
							best_dist_sq = dist_sq;
							best_eidx_src = (int)ml_src->e;
						}
					}
					if (best_eidx_src >= 0) {
						mesh_remap_item_define(r_map, i, hit_dist, 0, 1, &best_eidx_src, &full_weight);
					}
				}
				else {
					/* No source for this dest edge! */
					BKE_mesh_remap_item_define_invalid(r_map, i);
				}
			}

			MEM_freeN(vcos_src);
		}
		else if (mode == MREMAP_MODE_EDGE_EDGEINTERP_VNORPROJ) {
			const int num_rays_min = 5, num_rays_max = 100;
			const int numedges_src = dm_src->getNumEdges(dm_src);

			/* Subtleness - this one we can allocate only max number of cast rays per edges! */
			int *indices = MEM_mallocN(sizeof(*indices) * (size_t)min_ii(numedges_src, num_rays_max), __func__);
			/* Here it's simpler to just allocate for all edges :/ */
			float *weights = MEM_mallocN(sizeof(*weights) * (size_t)numedges_src, __func__);

			bvhtree_from_mesh_edges(&treedata, dm_src, MREMAP_RAYCAST_APPROXIMATE_BVHEPSILON(ray_radius), 2, 6);

			for (i = 0; i < numedges_dst; i++) {
				/* For each dst edge, we sample some rays from it (interpolated from its vertices)
				 * and use their hits to interpolate from source edges. */
				const MEdge *me = &edges_dst[i];
				float tmp_co[3], v1_co[3], v2_co[3];
				float tmp_no[3], v1_no[3], v2_no[3];

				int grid_size;
				float edge_dst_len;
				float grid_step;

				float totweights = 0.0f;
				float hit_dist_accum = 0.0f;
				int sources_num = 0;
				int j;

				copy_v3_v3(v1_co, verts_dst[me->v1].co);
				copy_v3_v3(v2_co, verts_dst[me->v2].co);

				normal_short_to_float_v3(v1_no, verts_dst[me->v1].no);
				normal_short_to_float_v3(v2_no, verts_dst[me->v2].no);

				/* We do our transform here, allows to interpolate from normals already in src space. */
				if (space_transform) {
					BLI_space_transform_apply(space_transform, v1_co);
					BLI_space_transform_apply(space_transform, v2_co);
					BLI_space_transform_apply_normal(space_transform, v1_no);
					BLI_space_transform_apply_normal(space_transform, v2_no);
				}

				fill_vn_fl(weights, (int)numedges_src, 0.0f);

				/* We adjust our ray-casting grid to ray_radius (the smaller, the more rays are cast),
				 * with lower/upper bounds. */
				edge_dst_len = len_v3v3(v1_co, v2_co);

				grid_size = (int)((edge_dst_len / ray_radius) + 0.5f);
				CLAMP(grid_size, num_rays_min, num_rays_max);  /* min 5 rays/edge, max 100. */

				grid_step = 1.0f / (float)grid_size;  /* Not actual distance here, rather an interp fac... */

				/* And now we can cast all our rays, and see what we get! */
				for (j = 0; j < grid_size; j++) {
					const float fac = grid_step * (float)j;

					int n = (ray_radius > 0.0f) ? MREMAP_RAYCAST_APPROXIMATE_NR : 1;
					float w = 1.0f;

					interp_v3_v3v3(tmp_co, v1_co, v2_co, fac);
					interp_v3_v3v3_slerp_safe(tmp_no, v1_no, v2_no, fac);

					while (n--) {
						/* Note we handle dest to src space conversion ourself, here! */
						if (mesh_remap_bvhtree_query_raycast(
						        &treedata, &rayhit, NULL,
						        tmp_co, tmp_no, ray_radius / w, max_dist, &hit_dist))
						{
							weights[rayhit.index] += w;
							totweights += w;
							hit_dist_accum += hit_dist;
							break;
						}
						/* Next iteration will get bigger radius but smaller weight! */
						w /= MREMAP_RAYCAST_APPROXIMATE_FAC;
					}
				}
				/* A sampling is valid (as in, its result can be considered as valid sources) only if at least
				 * half of the rays found a source! */
				if (totweights > ((float)grid_size / 2.0f)) {
					for (j = 0; j < (int)numedges_src; j++) {
						if (!weights[j]) {
							continue;
						}
						/* Note: sources_num is always <= j! */
						weights[sources_num] = weights[j] / totweights;
						indices[sources_num] = j;
						sources_num++;
					}
					mesh_remap_item_define(r_map, i, hit_dist_accum / totweights, 0,
					                                  sources_num, indices, weights);
				}
				else {
					/* No source for this dest edge! */
					BKE_mesh_remap_item_define_invalid(r_map, i);
				}
			}

			MEM_freeN(indices);
			MEM_freeN(weights);
		}
		else {
			printf("WARNING! Unsupported mesh-to-mesh edge mapping mode (%d)!\n", mode);
			memset(r_map->items, 0, sizeof(*r_map->items) * (size_t)numedges_dst);
		}

		free_bvhtree_from_mesh(&treedata);
	}
}

#define POLY_UNSET       0
#define POLY_CENTER_INIT 1
#define POLY_COMPLETE    2

static void mesh_island_to_astar_graph_edge_process(
        MeshIslandStore *islands, const int island_index, BLI_AStarGraph *as_graph,
        MVert *verts, MPoly *polys, MLoop *loops,
        const int edge_idx, BLI_bitmap *done_edges, MeshElemMap *edge_to_poly_map, const bool is_edge_innercut,
        int *poly_island_index_map, float (*poly_centers)[3], unsigned char *poly_status)
{
	int *poly_island_indices = BLI_array_alloca(poly_island_indices, (size_t)edge_to_poly_map[edge_idx].count);
	int i, j;

	for (i = 0; i < edge_to_poly_map[edge_idx].count; i++) {
		const int pidx = edge_to_poly_map[edge_idx].indices[i];
		MPoly *mp = &polys[pidx];
		const int pidx_isld = islands ? poly_island_index_map[pidx] : pidx;
		void *custom_data = is_edge_innercut ? SET_INT_IN_POINTER(edge_idx) : SET_INT_IN_POINTER(-1);

		if (UNLIKELY(islands && (islands->items_to_islands[mp->loopstart] != island_index))) {
			/* poly not in current island, happens with border edges... */
			poly_island_indices[i] = -1;
			continue;
		}

		if (poly_status[pidx_isld] == POLY_COMPLETE) {
			poly_island_indices[i] = pidx_isld;
			continue;
		}

		if (poly_status[pidx_isld] == POLY_UNSET) {
			BKE_mesh_calc_poly_center(mp, &loops[mp->loopstart], verts, poly_centers[pidx_isld]);
			BLI_astar_node_init(as_graph, pidx_isld, poly_centers[pidx_isld]);
			poly_status[pidx_isld] = POLY_CENTER_INIT;
		}

		for (j = i; j--;) {
			float dist_cost;
			const int pidx_isld_other = poly_island_indices[j];

			if (pidx_isld_other == -1 || poly_status[pidx_isld_other] == POLY_COMPLETE) {
				/* If the other poly is complete, that link has already been added! */
				continue;
			}
			dist_cost = len_v3v3(poly_centers[pidx_isld_other], poly_centers[pidx_isld]);
			BLI_astar_node_link_add(as_graph, pidx_isld_other, pidx_isld, dist_cost, custom_data);
		}

		poly_island_indices[i] = pidx_isld;
	}

	BLI_BITMAP_ENABLE(done_edges, edge_idx);
}

static void mesh_island_to_astar_graph(
        MeshIslandStore *islands, const int island_index,
        MVert *verts, MeshElemMap *edge_to_poly_map, const int numedges, MLoop *loops, MPoly *polys, const int numpolys,
        BLI_AStarGraph *r_as_graph)
{
	MeshElemMap *island_poly_map = islands ? islands->islands[island_index] : NULL;
	MeshElemMap *island_einnercut_map = islands ? islands->innercuts[island_index] : NULL;

	int *poly_island_index_map = NULL;
	BLI_bitmap *done_edges = BLI_BITMAP_NEW(numedges, __func__);

	const int node_num = islands ? island_poly_map->count : numpolys;
	unsigned char *poly_status = MEM_callocN(sizeof(*poly_status) * (size_t)node_num, __func__);
	float (*poly_centers)[3];

	int pidx_isld;
	int i;

	BLI_astar_graph_init(r_as_graph, node_num, NULL);
	/* poly_centers is owned by graph memarena. */
	poly_centers = BLI_memarena_calloc(r_as_graph->mem, sizeof(*poly_centers) * (size_t)node_num);

	if (islands) {
		/* poly_island_index_map is owned by graph memarena. */
		poly_island_index_map = BLI_memarena_calloc(r_as_graph->mem, sizeof(*poly_island_index_map) * (size_t)numpolys);
		for (i = island_poly_map->count; i--;) {
			poly_island_index_map[island_poly_map->indices[i]] = i;
		}

		r_as_graph->custom_data = poly_island_index_map;

		for (i = island_einnercut_map->count; i--;) {
			mesh_island_to_astar_graph_edge_process(
			        islands, island_index, r_as_graph, verts, polys, loops,
			        island_einnercut_map->indices[i], done_edges, edge_to_poly_map, true,
			        poly_island_index_map, poly_centers, poly_status);
		}
	}

	for (pidx_isld = node_num; pidx_isld--;) {
		const int pidx = islands ? island_poly_map->indices[pidx_isld] : pidx_isld;
		MPoly *mp = &polys[pidx];
		int pl_idx, l_idx;

		if (poly_status[pidx_isld] == POLY_COMPLETE) {
			continue;
		}

		for (pl_idx = 0, l_idx = mp->loopstart; pl_idx < mp->totloop; pl_idx++, l_idx++) {
			MLoop *ml = &loops[l_idx];

			if (BLI_BITMAP_TEST(done_edges, ml->e)) {
				continue;
			}

			mesh_island_to_astar_graph_edge_process(
			        islands, island_index, r_as_graph, verts, polys, loops,
			        (int)ml->e, done_edges, edge_to_poly_map, false,
			        poly_island_index_map, poly_centers, poly_status);
		}
		poly_status[pidx_isld] = POLY_COMPLETE;
	}

	MEM_freeN(done_edges);
	MEM_freeN(poly_status);
}

#undef POLY_UNSET
#undef POLY_CENTER_INIT
#undef POLY_COMPLETE

/* Our 'f_cost' callback func, to find shortest poly-path between two remapped-loops.
 * Note we do not want to make innercuts 'walls' here, just detect when the shortest path goes by those. */
static float mesh_remap_calc_loops_astar_f_cost(
        BLI_AStarGraph *as_graph, BLI_AStarSolution *as_solution, BLI_AStarGNLink *link,
        const int node_idx_curr, const int node_idx_next, const int node_idx_dst)
{
	float *co_next, *co_dest;

	if (link && (GET_INT_FROM_POINTER(link->custom_data) != -1)) {
		/* An innercut edge... We tag our solution as potentially crossing innercuts.
		 * Note it might not be the case in the end (AStar will explore around optimal path), but helps
		 * trimming off some processing later... */
		if (!GET_INT_FROM_POINTER(as_solution->custom_data)) {
			as_solution->custom_data = SET_INT_IN_POINTER(true);
		}
	}

	/* Our heuristic part of current f_cost is distance from next node to destination one.
	 * It is guaranteed to be less than (or equal to) actual shortest poly-path between next node and destination one.
	 */
	co_next = (float *)as_graph->nodes[node_idx_next].custom_data;
	co_dest = (float *)as_graph->nodes[node_idx_dst].custom_data;
	return (link ? (as_solution->g_costs[node_idx_curr] + link->cost) : 0.0f) + len_v3v3(co_next, co_dest);
}

#define ASTAR_STEPS_MAX 64


void BKE_mesh_remap_calc_loops_from_dm(
        const int mode, const SpaceTransform *space_transform, const float max_dist, const float ray_radius,
        MVert *verts_dst, const int numverts_dst, MEdge *edges_dst, const int numedges_dst,
        MLoop *loops_dst, const int numloops_dst, MPoly *polys_dst, const int numpolys_dst,
        CustomData *ldata_dst, CustomData *pdata_dst, const float split_angle_dst, const bool dirty_nors_dst,
        DerivedMesh *dm_src,
        MeshRemapIslandsCalc gen_islands_src, const float islands_precision_src, MeshPairRemap *r_map)
{
	const float full_weight = 1.0f;
	const float max_dist_sq = max_dist * max_dist;

	int i;

	BLI_assert(mode & MREMAP_MODE_LOOP);
	BLI_assert((islands_precision_src >= 0.0f) && (islands_precision_src <= 1.0f));

	BKE_mesh_remap_init(r_map, numloops_dst);

	if (mode == MREMAP_MODE_TOPOLOGY) {
		/* In topology mapping, we assume meshes are identical, islands included! */
		BLI_assert(numloops_dst == dm_src->getNumLoops(dm_src));
		for (i = 0; i < numloops_dst; i++) {
			mesh_remap_item_define(r_map, i, FLT_MAX, 0, 1, &i, &full_weight);
		}
	}
	else {
		BVHTreeFromMesh *treedata = NULL;
		BVHTreeNearest nearest = {0};
		BVHTreeRayHit rayhit = {0};
		int num_trees = 0;
		float hit_dist;

		const bool use_from_vert = (mode & MREMAP_USE_VERT);

		MeshIslandStore island_store = {0};
		bool use_islands = false;

		BLI_AStarGraph *as_graphdata = NULL;
		BLI_AStarSolution as_solution = {0};
		const int isld_steps_src = islands_precision_src ?
		                           max_ii((int)(ASTAR_STEPS_MAX * islands_precision_src + 0.499f), 1) : 0;

		float (*poly_nors_src)[3] = NULL;
		float (*loop_nors_src)[3] = NULL;
		float (*poly_nors_dst)[3] = NULL;
		float (*loop_nors_dst)[3] = NULL;

		MeshElemMap *vert_to_loop_map_src = NULL;
		int *vert_to_loop_map_src_buff = NULL;
		MeshElemMap *vert_to_poly_map_src = NULL;
		int *vert_to_poly_map_src_buff = NULL;
		MeshElemMap *edge_to_poly_map_src = NULL;
		int *edge_to_poly_map_src_buff = NULL;
		MeshElemMap *poly_to_tessface_map_src = NULL;
		int *poly_to_tessface_map_src_buff = NULL;

		/* Unlike above, those are one-to-one mappings, simpler! */
		int *loop_to_poly_map_src = NULL;
		int *tessface_to_poly_map_src = NULL;

		bool verts_allocated_src;
		MVert *verts_src = DM_get_vert_array(dm_src, &verts_allocated_src);
		const int num_verts_src = dm_src->getNumVerts(dm_src);
		float (*vcos_src)[3] = NULL;
		bool edges_allocated_src;
		MEdge *edges_src = DM_get_edge_array(dm_src, &edges_allocated_src);
		const int num_edges_src = dm_src->getNumEdges(dm_src);
		bool loops_allocated_src;
		MLoop *loops_src = DM_get_loop_array(dm_src, &loops_allocated_src);
		const int num_loops_src = dm_src->getNumLoops(dm_src);
		bool polys_allocated_src;
		MPoly *polys_src = DM_get_poly_array(dm_src, &polys_allocated_src);
		const int num_polys_src = dm_src->getNumPolys(dm_src);
		bool faces_allocated_src = false;
		MFace *faces_src = NULL;
		int num_faces_src = 0;

		size_t buff_size_interp = MREMAP_DEFAULT_BUFSIZE;
		float (*vcos_interp)[3] = NULL;
		int *indices_interp = NULL;
		float *weights_interp = NULL;

		int tindex, pidx_dst, lidx_dst, plidx_dst, pidx_src, lidx_src, plidx_src;

		IslandResult **islands_res;
		size_t islands_res_buff_size = MREMAP_DEFAULT_BUFSIZE;

		const float bvh_epsilon = (mode & MREMAP_USE_NORPROJ) ? MREMAP_RAYCAST_APPROXIMATE_BVHEPSILON(ray_radius) : 0.0f;

		if (!use_from_vert) {
			vcos_src = MEM_mallocN(sizeof(*vcos_src) * (size_t)num_verts_src, __func__);
			dm_src->getVertCos(dm_src, vcos_src);

			vcos_interp = MEM_mallocN(sizeof(*vcos_interp) * buff_size_interp, __func__);
			indices_interp = MEM_mallocN(sizeof(*indices_interp) * buff_size_interp, __func__);
			weights_interp = MEM_mallocN(sizeof(*weights_interp) * buff_size_interp, __func__);
		}

		{
			const bool need_lnors_src = (mode & MREMAP_USE_LOOP) && (mode & MREMAP_USE_NORMAL);
			const bool need_lnors_dst = need_lnors_src || (mode & MREMAP_USE_NORPROJ);
			const bool need_pnors_src = need_lnors_src || ((mode & MREMAP_USE_POLY) && (mode & MREMAP_USE_NORMAL));
			const bool need_pnors_dst = need_lnors_dst || need_pnors_src;

			if (need_pnors_dst) {
				/* Cache poly nors into a temp CDLayer. */
				poly_nors_dst = CustomData_get_layer(pdata_dst, CD_NORMAL);
				if (!poly_nors_dst) {
					poly_nors_dst = CustomData_add_layer(pdata_dst, CD_NORMAL, CD_CALLOC, NULL, numpolys_dst);
					CustomData_set_layer_flag(pdata_dst, CD_NORMAL, CD_FLAG_TEMPORARY);
				}
				if (dirty_nors_dst) {
					BKE_mesh_calc_normals_poly(verts_dst, numverts_dst, loops_dst, polys_dst,
					                           numloops_dst, numpolys_dst, poly_nors_dst, true);
				}
			}
			if (need_lnors_dst) {
				/* Cache poly nors into a temp CDLayer. */
				loop_nors_dst = CustomData_get_layer(ldata_dst, CD_NORMAL);
				if (!loop_nors_dst) {
					loop_nors_dst = CustomData_add_layer(ldata_dst, CD_NORMAL, CD_CALLOC, NULL, numloops_dst);
					CustomData_set_layer_flag(ldata_dst, CD_NORMAL, CD_FLAG_TEMPORARY);
				}
				if (dirty_nors_dst) {
					BKE_mesh_normals_loop_split(verts_dst, numverts_dst, edges_dst, numedges_dst,
					                            loops_dst, loop_nors_dst, numloops_dst,
					                            polys_dst, poly_nors_dst, numpolys_dst, split_angle_dst);
				}
			}
			if (need_pnors_src || need_lnors_src) {
				/* Simpler for now, calcNormals never stores pnors :( */
				dm_src->calcLoopNormals(dm_src, /* TODO */ (float)M_PI);

				if (need_pnors_src) {
					poly_nors_src = dm_src->getPolyDataArray(dm_src, CD_NORMAL);
				}
				if (need_lnors_src) {
					loop_nors_src = dm_src->getLoopDataArray(dm_src, CD_NORMAL);
				}
			}
		}

		if (use_from_vert) {
			BKE_mesh_vert_loop_map_create(&vert_to_loop_map_src, &vert_to_loop_map_src_buff,
			                              polys_src, loops_src, num_verts_src, num_polys_src, num_loops_src);
			if (mode & MREMAP_USE_POLY) {
				BKE_mesh_vert_poly_map_create(&vert_to_poly_map_src, &vert_to_poly_map_src_buff,
				                              polys_src, loops_src, num_verts_src, num_polys_src, num_loops_src);
			}
		}

		/* Needed for islands (or plain mesh) to AStar graph conversion. */
		BKE_mesh_edge_poly_map_create(&edge_to_poly_map_src, &edge_to_poly_map_src_buff,
		                              edges_src, num_edges_src, polys_src, num_polys_src, loops_src, num_loops_src);
		if (use_from_vert) {
			loop_to_poly_map_src = MEM_mallocN(sizeof(*loop_to_poly_map_src) * (size_t)num_loops_src, __func__);
			for (pidx_src = 0; pidx_src < num_polys_src; pidx_src++) {
				MPoly *mp = &polys_src[pidx_src];
				for (plidx_src = 0, lidx_src = mp->loopstart; plidx_src < mp->totloop; plidx_src++, lidx_src++) {
					loop_to_poly_map_src[lidx_src] = pidx_src;
				}
			}
		}

		/* Island makes things slightly more complex here.
		 * Basically, we:
		 *     * Make one treedata for each island's elements.
		 *     * Check all loops of a same dest poly against all treedata.
		 *     * Choose the island's elements giving the best results.
		 */

		/* First, generate the islands, if possible. */
		if (gen_islands_src) {
			use_islands = gen_islands_src(
			        verts_src, num_verts_src,
			        edges_src, num_edges_src,
			        polys_src, num_polys_src,
			        loops_src, num_loops_src,
			        &island_store);

			num_trees = use_islands ? island_store.islands_num : 1;
			treedata = MEM_callocN(sizeof(*treedata) * (size_t)num_trees, __func__);
			if (isld_steps_src) {
				as_graphdata = MEM_callocN(sizeof(*as_graphdata) * (size_t)num_trees, __func__);
			}

			if (use_islands) {
				/* We expect our islands to contain poly indices, with edge indices of 'inner cuts',
				 * and a mapping loops -> islands indices.
				 * This implies all loops of a same poly are in the same island. */
				BLI_assert((island_store.item_type == MISLAND_TYPE_LOOP) &&
				           (island_store.island_type == MISLAND_TYPE_POLY) &&
				           (island_store.innercut_type == MISLAND_TYPE_EDGE));
			}
		}
		else {
			num_trees = 1;
			treedata = MEM_callocN(sizeof(*treedata), __func__);
			if (isld_steps_src) {
				as_graphdata = MEM_callocN(sizeof(*as_graphdata), __func__);
			}
		}

		/* Build our AStar graphs. */
		if (isld_steps_src) {
			for (tindex = 0; tindex < num_trees; tindex++) {
				mesh_island_to_astar_graph(
				        use_islands ? &island_store : NULL, tindex, verts_src, edge_to_poly_map_src, num_edges_src,
				        loops_src, polys_src, num_polys_src, &as_graphdata[tindex]);
			}
		}

		/* Build our BVHtrees, either from verts or tessfaces. */
		if (use_from_vert) {
			if (use_islands) {
				BLI_bitmap *verts_active = BLI_BITMAP_NEW((size_t)num_verts_src, __func__);

				for (tindex = 0; tindex < num_trees; tindex++) {
					MeshElemMap *isld = island_store.islands[tindex];
					int num_verts_active = 0;
					BLI_BITMAP_SET_ALL(verts_active, false, (size_t)num_verts_src);
					for (i = 0; i < isld->count; i++) {
						MPoly *mp_src = &polys_src[isld->indices[i]];
						for (lidx_src = mp_src->loopstart; lidx_src < mp_src->loopstart + mp_src->totloop; lidx_src++) {
							BLI_BITMAP_ENABLE(verts_active, loops_src[lidx_src].v);
							num_verts_active++;
						}
					}
					/* verts 'ownership' is transfered to treedata here, which will handle its freeing. */
					bvhtree_from_mesh_verts_ex(&treedata[tindex], verts_src, num_verts_src, verts_allocated_src,
					                           verts_active, num_verts_active, bvh_epsilon, 2, 6);
					if (verts_allocated_src) {
						verts_allocated_src = false;  /* Only 'give' our verts once, to first tree! */
					}
				}

				MEM_freeN(verts_active);
			}
			else {
				BLI_assert(num_trees == 1);
				bvhtree_from_mesh_verts(&treedata[0], dm_src, bvh_epsilon, 2, 6);
			}
		}
		else {  /* We use polygons. */
			if (use_islands) {
				/* bvhtree here uses tesselated faces... */
				const unsigned int dirty_tess_flag = dm_src->dirty & DM_DIRTY_TESS_CDLAYERS;
				BLI_bitmap *faces_active;

				/* We do not care about tessellated data here, only geometry itself is important. */
				if (dirty_tess_flag) {
					dm_src->dirty &= ~dirty_tess_flag;
				}
				DM_ensure_tessface(dm_src);
				if (dirty_tess_flag) {
					dm_src->dirty |= dirty_tess_flag;
				}
				faces_src = DM_get_tessface_array(dm_src, &faces_allocated_src);
				num_faces_src = dm_src->getNumTessFaces(dm_src);
				tessface_to_poly_map_src = dm_src->getTessFaceDataArray(dm_src, CD_ORIGINDEX);
				faces_active = BLI_BITMAP_NEW((size_t)num_faces_src, __func__);

				for (tindex = 0; tindex < num_trees; tindex++) {
					int num_faces_active = 0;
					BLI_BITMAP_SET_ALL(faces_active, false, (size_t)num_faces_src);
					for (i = 0; i < num_faces_src; i++) {
						MPoly *mp_src = &polys_src[tessface_to_poly_map_src[i]];
						if (island_store.items_to_islands[mp_src->loopstart] == tindex) {
							BLI_BITMAP_ENABLE(faces_active, i);
							num_faces_active++;
						}
					}
					/* verts 'ownership' is transfered to treedata here, which will handle its freeing. */
					bvhtree_from_mesh_faces_ex(
					        &treedata[tindex], verts_src, verts_allocated_src,
					        faces_src, num_faces_src, faces_allocated_src,
					        faces_active, num_faces_active, bvh_epsilon, 2, 6);
					if (verts_allocated_src) {
						verts_allocated_src = false;  /* Only 'give' our verts once, to first tree! */
					}
					if (faces_allocated_src) {
						faces_allocated_src = false;  /* Only 'give' our faces once, to first tree! */
					}
				}

				MEM_freeN(faces_active);
			}
			else {
				BLI_assert(num_trees == 1);
				bvhtree_from_mesh_faces(&treedata[0], dm_src, bvh_epsilon, 2, 6);
				tessface_to_poly_map_src = dm_src->getTessFaceDataArray(dm_src, CD_ORIGINDEX);
			}
		}

		/* And check each dest poly! */
		islands_res = MEM_mallocN(sizeof(*islands_res) * (size_t)num_trees, __func__);
		for (tindex = 0; tindex < num_trees; tindex++) {
			islands_res[tindex] = MEM_mallocN(sizeof(**islands_res) * islands_res_buff_size, __func__);
		}

		for (pidx_dst = 0; pidx_dst < numpolys_dst; pidx_dst++) {
			MPoly *mp_dst = &polys_dst[pidx_dst];
			float (*pnor_dst)[3] = &poly_nors_dst[pidx_dst];

			if ((size_t)mp_dst->totloop > islands_res_buff_size) {
				islands_res_buff_size = (size_t)mp_dst->totloop;
				for (tindex = 0; tindex < num_trees; tindex++) {
					islands_res[tindex] = MEM_reallocN(islands_res[tindex], sizeof(**islands_res) * islands_res_buff_size);
				}
			}

			for (tindex = 0; tindex < num_trees; tindex++) {
				BVHTreeFromMesh *tdata = &treedata[tindex];
				MLoop *ml_dst = &loops_dst[mp_dst->loopstart];

				for (plidx_dst = 0; plidx_dst < mp_dst->totloop; plidx_dst++, ml_dst++) {
					if (use_from_vert) {
						float tmp_co[3];
						MeshElemMap *vert_to_refelem_map_src = NULL;

						copy_v3_v3(tmp_co, verts_dst[ml_dst->v].co);
						nearest.index = -1;

						if (mesh_remap_bvhtree_query_nearest(
						        tdata, &nearest, space_transform,
						        tmp_co, max_dist_sq, &hit_dist))
						{
							float (*nor_dst)[3];
							float (*nors_src)[3];
							float best_nor_dot = -2.0f;
							int best_index_src = -1;

							if (mode == MREMAP_MODE_LOOP_NEAREST_LOOPNOR) {
								nor_dst = &loop_nors_dst[plidx_dst + mp_dst->loopstart];
								nors_src = loop_nors_src;
								vert_to_refelem_map_src = vert_to_loop_map_src;
							}
							else {  /* if (mode == MREMAP_MODE_LOOP_NEAREST_POLYNOR) { */
								nor_dst = pnor_dst;
								nors_src = poly_nors_src;
								vert_to_refelem_map_src = vert_to_poly_map_src;
							}

							for (i = vert_to_refelem_map_src[nearest.index].count; i--;) {
								const int index_src = vert_to_refelem_map_src[nearest.index].indices[i];
								const float dot = dot_v3v3(nors_src[index_src], *nor_dst);
								if (dot > best_nor_dot) {
									best_nor_dot = dot;
									best_index_src = index_src;
								}
							}
							if (mode == MREMAP_MODE_LOOP_NEAREST_POLYNOR) {
								/* Our best_index_src is a poly one for now!
								 * Have to find its loop matching our closest vertex. */
								MPoly *mp_src = &polys_src[best_index_src];
								MLoop *ml_src = &loops_src[mp_src->loopstart];
								for (plidx_src = 0; plidx_src < mp_src->totloop; plidx_src++, ml_src++) {
									if ((int)ml_src->v == nearest.index) {
										best_index_src = plidx_src + mp_src->loopstart;
										break;
									}
								}
							}
							islands_res[tindex][plidx_dst].factor = hit_dist ? (1.0f / (hit_dist * best_nor_dot)) : 1e18f;
							islands_res[tindex][plidx_dst].hit_dist = hit_dist;
							islands_res[tindex][plidx_dst].index_src = best_index_src;
						}
						else {
							/* No source for this dest loop! */
							islands_res[tindex][plidx_dst].factor = 0.0f;
							islands_res[tindex][plidx_dst].hit_dist = FLT_MAX;
							islands_res[tindex][plidx_dst].index_src = -1;
						}
					}
					else if (mode & MREMAP_USE_NORPROJ) {
						float tmp_co[3], tmp_no[3];

						int n = (ray_radius > 0.0f) ? MREMAP_RAYCAST_APPROXIMATE_NR : 1;
						float w = 1.0f;

						copy_v3_v3(tmp_co, verts_dst[ml_dst->v].co);
						copy_v3_v3(tmp_no, loop_nors_dst[plidx_dst + mp_dst->loopstart]);

						/* We do our transform here, since we may do several raycast/nearest queries. */
						if (space_transform) {
							BLI_space_transform_apply(space_transform, tmp_co);
							BLI_space_transform_apply_normal(space_transform, tmp_no);
						}

						while (n--) {
							/* Note we handle dest to src space conversion ourself, here! */
							if (mesh_remap_bvhtree_query_raycast(
							        tdata, &rayhit, NULL,
							        tmp_co, tmp_no, ray_radius / w, max_dist, &hit_dist))
							{
								islands_res[tindex][plidx_dst].factor = (hit_dist ? (1.0f / hit_dist) : 1e18f) * w;
								islands_res[tindex][plidx_dst].hit_dist = hit_dist;
								islands_res[tindex][plidx_dst].index_src = tessface_to_poly_map_src[rayhit.index];
								copy_v3_v3(islands_res[tindex][plidx_dst].hit_point, rayhit.co);
								break;
							}
							/* Next iteration will get bigger radius but smaller weight! */
							w /= MREMAP_RAYCAST_APPROXIMATE_FAC;
						}
						if (n == -1) {
							/* Fallback to 'nearest' hit here, loops usually comes in 'face group', not good to
							 * have only part of one dest face's loops to map to source.
							 * Note that since we give this a null weight, if whole weight for a given face
							 * is null, it means none of its loop mapped to this source island, hence we can skip it
							 * later.
							 */
							copy_v3_v3(tmp_co, verts_dst[ml_dst->v].co);
							nearest.index = -1;

							/* In any case, this fallback nearest hit should have no weight at all
							 * in 'best island' decision! */
							islands_res[tindex][plidx_dst].factor = 0.0f;

							/* Note we handle dest to src space conversion ourself, here! */
							if (mesh_remap_bvhtree_query_nearest(
							        tdata, &nearest, NULL,
							        tmp_co, max_dist_sq, &hit_dist))
							{
								islands_res[tindex][plidx_dst].hit_dist = hit_dist;
								islands_res[tindex][plidx_dst].index_src = tessface_to_poly_map_src[nearest.index];
								copy_v3_v3(islands_res[tindex][plidx_dst].hit_point, nearest.co);
							}
							else {
								/* No source for this dest loop! */
								islands_res[tindex][plidx_dst].hit_dist = FLT_MAX;
								islands_res[tindex][plidx_dst].index_src = -1;
							}
						}
					}
					else {  /* Nearest poly either to use all its loops/verts or just closest one. */
						float tmp_co[3];

						copy_v3_v3(tmp_co, verts_dst[ml_dst->v].co);
						nearest.index = -1;

						if (mesh_remap_bvhtree_query_nearest(
						        tdata, &nearest, space_transform,
						        tmp_co, max_dist_sq, &hit_dist))
						{
							islands_res[tindex][plidx_dst].factor = hit_dist ? (1.0f / hit_dist) : 1e18f;
							islands_res[tindex][plidx_dst].hit_dist = hit_dist;
							islands_res[tindex][plidx_dst].index_src = tessface_to_poly_map_src[nearest.index];
							copy_v3_v3(islands_res[tindex][plidx_dst].hit_point, nearest.co);
						}
						else {
							/* No source for this dest loop! */
							islands_res[tindex][plidx_dst].factor = 0.0f;
							islands_res[tindex][plidx_dst].hit_dist = FLT_MAX;
							islands_res[tindex][plidx_dst].index_src = -1;
						}
					}
				}
			}

			/* And now, find best island to use! */
			/* We have to first select the 'best source island' for given dst poly and its loops.
			 * Then, we have to check that poly does not 'spread' across some island's limits
			 * (like inner seams for UVs, etc.).
			 * Note we only still partially support that kind of situation here, i.e. polys spreading over actual cracks
			 * (like a narrow space without faces on src, splitting a 'tube-like' geometry). That kind of situation
			 * should be relatively rare, though.
			 */
			/* XXX This block in itself is big and complex enough to be a separate function but... it uses a bunch
			 *     of locale vars. Not worth sending all that through parameters (for now at least). */
			{
				BLI_AStarGraph *as_graph = NULL;
				int *poly_island_index_map = NULL;
				int pidx_src_prev = -1;

				MeshElemMap *best_island = NULL;
				float best_island_fac = 0.0f;
				int best_island_index = -1;

				for (tindex = 0; tindex < num_trees; tindex++) {
					float island_fac = 0.0f;

					for (plidx_dst = 0; plidx_dst < mp_dst->totloop; plidx_dst++) {
						island_fac += islands_res[tindex][plidx_dst].factor;
					}
					island_fac /= (float)mp_dst->totloop;

					if (island_fac > best_island_fac) {
						best_island_fac = island_fac;
						best_island_index = tindex;
					}
				}

				if (best_island_index != -1 && isld_steps_src) {
					best_island = island_store.islands[best_island_index];
					as_graph = &as_graphdata[best_island_index];
					poly_island_index_map = (int *)as_graph->custom_data;
					BLI_astar_solution_init(as_graph, &as_solution, false);
				}

				for (plidx_dst = 0; plidx_dst < mp_dst->totloop; plidx_dst++) {
					IslandResult *isld_res;
					lidx_dst = plidx_dst + mp_dst->loopstart;

					if (best_island_index == -1) {
						/* No source for any loops of our dest poly in any source islands. */
						BKE_mesh_remap_item_define_invalid(r_map, lidx_dst);
						continue;
					}

					as_solution.custom_data = SET_INT_IN_POINTER(false);

					isld_res = &islands_res[best_island_index][plidx_dst];
					if (use_from_vert) {
						/* Indices stored in islands_res are those of loops, one per dest loop. */
						lidx_src = isld_res->index_src;
						if (lidx_src >= 0) {
							pidx_src = loop_to_poly_map_src[lidx_src];
							/* If prev and curr poly are the same, no need to do anything more!!! */
							if (!ELEM(pidx_src_prev, -1, pidx_src) && isld_steps_src) {
								BLI_astar_graph_solve(
								        as_graph, poly_island_index_map[pidx_src_prev], poly_island_index_map[pidx_src],
								        mesh_remap_calc_loops_astar_f_cost, &as_solution, isld_steps_src);
								if (GET_INT_FROM_POINTER(as_solution.custom_data) && (as_solution.steps > 0)) {
									/* Find first 'cutting edge' on path, and bring back lidx_src on poly just
									 * before that edge.
									 * Note we could try to be much smarter (like e.g. storing a whole poly's indices,
									 * and making decision (on which side of cutting edge(s!) to be) on the end,
									 * but this is one more level of complexity, better to first see if
									 * simple solution works!
									 */
									int pidx_isld_src = poly_island_index_map[pidx_src];
									int last_valid_pidx_isld_src = -1;
									/* Note we go backward here, from dest to src poly. */
									for (i = as_solution.steps - 1; i--;) {
										BLI_AStarGNLink *as_link = as_solution.prev_links[pidx_isld_src];
										const int eidx = GET_INT_FROM_POINTER(as_link->custom_data);
										pidx_isld_src = as_solution.prev_nodes[pidx_isld_src];
										BLI_assert(pidx_isld_src != -1);
										if (eidx != -1) {
											/* we are 'crossing' a cutting edge. */
											last_valid_pidx_isld_src = pidx_isld_src;
										}
									}
									if (last_valid_pidx_isld_src != -1) {
										/* Find a new valid loop in that new poly (nearest one for now).
										 * Note we could be much more subtle here, again that's for later... */
										MPoly *mp_src;
										MLoop *ml_src, *ml_dst = &loops_dst[lidx_dst];
										int j;
										float best_dist_sq = FLT_MAX;
										float tmp_co[3];

										copy_v3_v3(tmp_co, verts_dst[ml_dst->v].co);

										/* We do our transform here, since we may do several raycast/nearest queries. */
										if (space_transform) {
											BLI_space_transform_apply(space_transform, tmp_co);
										}

										pidx_src = best_island->indices[last_valid_pidx_isld_src];
										mp_src = &polys_src[pidx_src];
										ml_src = &loops_src[mp_src->loopstart];
										for (j = 0; j < mp_src->totloop; j++, ml_src++) {
											const float dist_sq = len_squared_v3v3(verts_src[ml_src->v].co, tmp_co);
											if (dist_sq < best_dist_sq) {
												best_dist_sq = dist_sq;
												lidx_src = mp_src->loopstart + j;
											}
										}
									}
								}
							}
							mesh_remap_item_define(
							        r_map, lidx_dst, isld_res->hit_dist,
							        best_island_index, 1, &lidx_src, &full_weight);
							pidx_src_prev = pidx_src;
						}
						else {
							/* No source for this loop in this island. */
							/* TODO: would probably be better to get a source at all cost in best island anyway? */
							mesh_remap_item_define(
							        r_map, lidx_dst, FLT_MAX,
							        best_island_index, 0, NULL, NULL);
						}
					}
					else {
						/* Else, we use source poly, indices stored in islands_res are those of polygons. */
						pidx_src = isld_res->index_src;
						if (pidx_src >= 0) {
							MPoly *mp_src = &polys_src[pidx_src];
							float *hit_co = isld_res->hit_point;
							int best_loop_index_src;

							/* If prev and curr poly are the same, no need to do anything more!!! */
							if (!ELEM(pidx_src_prev, -1, pidx_src) && isld_steps_src) {
								BLI_astar_graph_solve(
								        as_graph, poly_island_index_map[pidx_src_prev], poly_island_index_map[pidx_src],
								        mesh_remap_calc_loops_astar_f_cost, &as_solution, isld_steps_src);
								if (GET_INT_FROM_POINTER(as_solution.custom_data) && (as_solution.steps > 0)) {
									/* Find first 'cutting edge' on path, and bring back lidx_src on poly just
									 * before that edge.
									 * Note we could try to be much smarter (like e.g. storing a whole poly's indices,
									 * and making decision (one which side of cutting edge(s!) to be on the end,
									 * but this is one more level of complexity, better to first see if
									 * simple solution works!
									 */
									int pidx_isld_src = poly_island_index_map[pidx_src];
									int last_valid_pidx_isld_src = -1;
									/* Note we go backward here, from dest to src poly. */
									for (i = as_solution.steps - 1; i--;) {
										BLI_AStarGNLink *as_link = as_solution.prev_links[pidx_isld_src];
										int eidx = GET_INT_FROM_POINTER(as_link->custom_data);

										pidx_isld_src = as_solution.prev_nodes[pidx_isld_src];
										BLI_assert(pidx_isld_src != -1);
										if (eidx != -1) {
											/* we are 'crossing' a cutting edge. */
											last_valid_pidx_isld_src = pidx_isld_src;
										}
									}
									if (last_valid_pidx_isld_src != -1) {
										/* Find a new valid loop in that new poly (nearest point on poly for now).
										 * Note we could be much more subtle here, again that's for later... */
										MLoop *ml_dst = &loops_dst[lidx_dst];
										float best_dist_sq = FLT_MAX;
										float tmp_co[3];
										int j;

										copy_v3_v3(tmp_co, verts_dst[ml_dst->v].co);

										/* We do our transform here, since we may do several raycast/nearest queries. */
										if (space_transform) {
											BLI_space_transform_apply(space_transform, tmp_co);
										}

										pidx_src = best_island->indices[last_valid_pidx_isld_src];
										mp_src = &polys_src[pidx_src];

										/* Create that one on demand. */
										if (poly_to_tessface_map_src == NULL) {
											BKE_mesh_origindex_map_create(
											        &poly_to_tessface_map_src, &poly_to_tessface_map_src_buff,
											        num_faces_src, tessface_to_poly_map_src, num_polys_src);
										}

										for (j = poly_to_tessface_map_src[pidx_src].count; j--;) {
											float h[3];
											MFace *mf = &faces_src[poly_to_tessface_map_src[pidx_src].indices[j]];
											float dist_sq;

											closest_on_tri_to_point_v3(
											        h, tmp_co,
											        vcos_src[mf->v1], vcos_src[mf->v2], vcos_src[mf->v3]);
											dist_sq = len_squared_v3v3(tmp_co, h);
											if (dist_sq < best_dist_sq) {
												copy_v3_v3(hit_co, h);
												best_dist_sq = dist_sq;
											}
											if (mf->v4) {
												closest_on_tri_to_point_v3(
												        h, tmp_co,
												        vcos_src[mf->v1], vcos_src[mf->v3], vcos_src[mf->v4]);
												dist_sq = len_squared_v3v3(tmp_co, h);
												if (dist_sq < best_dist_sq) {
													copy_v3_v3(hit_co, h);
													best_dist_sq = dist_sq;
												}
											}
										}
									}
								}
							}

							if (mode == MREMAP_MODE_LOOP_POLY_NEAREST) {
								mesh_remap_interp_poly_data_get(
								        mp_src, loops_src, (const float (*)[3])vcos_src, hit_co,
								        &buff_size_interp, &vcos_interp, true, &indices_interp,
								        &weights_interp, false, &best_loop_index_src);

								mesh_remap_item_define(
								        r_map, lidx_dst, isld_res->hit_dist,
								        best_island_index, 1, &best_loop_index_src, &full_weight);
							}
							else {
								const int sources_num = mesh_remap_interp_poly_data_get(
								        mp_src, loops_src, (const float (*)[3])vcos_src, hit_co,
								        &buff_size_interp, &vcos_interp, true, &indices_interp,
								        &weights_interp, true, NULL);

								mesh_remap_item_define(
								        r_map, lidx_dst,
								        isld_res->hit_dist, best_island_index,
								        sources_num, indices_interp, weights_interp);
							}

							pidx_src_prev = pidx_src;
						}
						else {
							/* No source for this loop in this island. */
							/* TODO: would probably be better to get a source at all cost in best island anyway? */
							mesh_remap_item_define(r_map, lidx_dst, FLT_MAX, best_island_index, 0, NULL, NULL);
						}
					}
				}

				BLI_astar_solution_clear(&as_solution);
			}
		}

		for (tindex = 0; tindex < num_trees; tindex++) {
			MEM_freeN(islands_res[tindex]);
			free_bvhtree_from_mesh(&treedata[tindex]);
			if (isld_steps_src) {
				BLI_astar_graph_free(&as_graphdata[tindex]);
			}
		}
		MEM_freeN(islands_res);
		BKE_mesh_loop_islands_free(&island_store);
		MEM_freeN(treedata);
		if (isld_steps_src) {
			MEM_freeN(as_graphdata);
			BLI_astar_solution_free(&as_solution);
		}

		if (verts_allocated_src) {
			MEM_freeN(verts_src);
		}
		if (vcos_src) {
			MEM_freeN(vcos_src);
		}
		if (edges_allocated_src) {
			MEM_freeN(edges_src);
		}
		if (loops_allocated_src) {
			MEM_freeN(loops_src);
		}
		if (polys_allocated_src) {
			MEM_freeN(polys_src);
		}
		if (faces_allocated_src) {
			MEM_freeN(faces_src);
		}
		if (vert_to_loop_map_src_buff) {
			MEM_freeN(vert_to_loop_map_src_buff);
		}
		if (vert_to_poly_map_src_buff) {
			MEM_freeN(vert_to_poly_map_src_buff);
		}
		if (edge_to_poly_map_src_buff) {
			MEM_freeN(edge_to_poly_map_src_buff);
		}
		if (poly_to_tessface_map_src_buff) {
			MEM_freeN(poly_to_tessface_map_src_buff);
		}
		if (loop_to_poly_map_src) {
			MEM_freeN(loop_to_poly_map_src);
		}
		if (vcos_interp) {
			MEM_freeN(vcos_interp);
		}
		if (indices_interp) {
			MEM_freeN(indices_interp);
		}
		if (weights_interp) {
			MEM_freeN(weights_interp);
		}
	}
}

void BKE_mesh_remap_calc_polys_from_dm(
        const int mode, const SpaceTransform *space_transform, const float max_dist, const float ray_radius,
        MVert *verts_dst, const int numverts_dst, MLoop *loops_dst, const int numloops_dst,
        MPoly *polys_dst, const int numpolys_dst, CustomData *pdata_dst, const bool dirty_nors_dst,
        DerivedMesh *dm_src, MeshPairRemap *r_map)
{
	const float full_weight = 1.0f;
	const float max_dist_sq = max_dist * max_dist;
	float (*poly_nors_dst)[3] = NULL;
	int i;

	BLI_assert(mode & MREMAP_MODE_POLY);

	if (mode & (MREMAP_USE_NORMAL | MREMAP_USE_NORPROJ)) {
		/* Cache poly nors into a temp CDLayer. */
		poly_nors_dst = CustomData_get_layer(pdata_dst, CD_NORMAL);
		if (!poly_nors_dst) {
			poly_nors_dst = CustomData_add_layer(pdata_dst, CD_NORMAL, CD_CALLOC, NULL, numpolys_dst);
			CustomData_set_layer_flag(pdata_dst, CD_NORMAL, CD_FLAG_TEMPORARY);
		}
		if (dirty_nors_dst) {
			BKE_mesh_calc_normals_poly(verts_dst, numverts_dst, loops_dst, polys_dst, numloops_dst, numpolys_dst,
			                           poly_nors_dst, true);
		}
	}

	BKE_mesh_remap_init(r_map, numpolys_dst);

	if (mode == MREMAP_MODE_TOPOLOGY) {
		BLI_assert(numpolys_dst == dm_src->getNumPolys(dm_src));
		for (i = 0; i < numpolys_dst; i++) {
			mesh_remap_item_define(r_map, i, FLT_MAX, 0, 1, &i, &full_weight);
		}
	}
	else {
		BVHTreeFromMesh treedata = {NULL};
		BVHTreeNearest nearest = {0};
		BVHTreeRayHit rayhit = {0};
		float hit_dist;

		int *tessface_to_poly_map_src;

		bvhtree_from_mesh_faces(
		        &treedata, dm_src,
		        (mode & MREMAP_USE_NORPROJ) ? MREMAP_RAYCAST_APPROXIMATE_BVHEPSILON(ray_radius) : 0.0f,
		        2, 6);
		/* bvhtree here uses tesselated faces... */
		tessface_to_poly_map_src = dm_src->getTessFaceDataArray(dm_src, CD_ORIGINDEX);

		if (mode == MREMAP_MODE_POLY_NEAREST) {
			nearest.index = -1;

			for (i = 0; i < numpolys_dst; i++) {
				MPoly *mp = &polys_dst[i];
				float tmp_co[3];

				BKE_mesh_calc_poly_center(mp, &loops_dst[mp->loopstart], verts_dst, tmp_co);

				if (mesh_remap_bvhtree_query_nearest(
				        &treedata, &nearest, space_transform,
				        tmp_co, max_dist_sq, &hit_dist))
				{
					mesh_remap_item_define(
					        r_map, i, hit_dist, 0,
					        1, &tessface_to_poly_map_src[nearest.index], &full_weight);
				}
				else {
					/* No source for this dest poly! */
					BKE_mesh_remap_item_define_invalid(r_map, i);
				}
			}
		}
		else if (mode == MREMAP_MODE_POLY_NOR) {
			BLI_assert(poly_nors_dst);

			for (i = 0; i < numpolys_dst; i++) {
				MPoly *mp = &polys_dst[i];
				float tmp_co[3], tmp_no[3];

				BKE_mesh_calc_poly_center(mp, &loops_dst[mp->loopstart], verts_dst, tmp_co);
				copy_v3_v3(tmp_no, poly_nors_dst[i]);

				if (mesh_remap_bvhtree_query_raycast(
				        &treedata, &rayhit, space_transform,
				        tmp_co, tmp_no, ray_radius, max_dist, &hit_dist))
				{
					mesh_remap_item_define(
					        r_map, i, hit_dist, 0,
					        1, &tessface_to_poly_map_src[rayhit.index], &full_weight);
				}
				else {
					/* No source for this dest poly! */
					BKE_mesh_remap_item_define_invalid(r_map, i);
				}
			}
		}
		else if (mode == MREMAP_MODE_POLY_POLYINTERP_PNORPROJ) {
			/* We cast our rays randomly, with a pseudo-even distribution (since we spread across tessellated tris,
			 * with additional weighting based on each tri's relative area).
			 */
			RNG *rng = BLI_rng_new(0);

			const size_t numpolys_src = (size_t)dm_src->getNumPolys(dm_src);

			/* Here it's simpler to just allocate for all polys :/ */
			int *indices = MEM_mallocN(sizeof(*indices) * numpolys_src, __func__);
			float *weights = MEM_mallocN(sizeof(*weights) * numpolys_src, __func__);

			size_t tmp_poly_size = MREMAP_DEFAULT_BUFSIZE;
			float (*poly_vcos_2d)[2] = MEM_mallocN(sizeof(*poly_vcos_2d) * tmp_poly_size, __func__);
			/* Tessellated 2D poly, always (num_loops - 2) triangles. */
			int (*tri_vidx_2d)[3] = MEM_mallocN(sizeof(*tri_vidx_2d) * (tmp_poly_size - 2), __func__);

			for (i = 0; i < numpolys_dst; i++) {
				/* For each dst poly, we sample some rays from it (2D grid in pnor space)
				 * and use their hits to interpolate from source polys. */
				/* Note: dst poly is early-converted into src space! */
				MPoly *mp = &polys_dst[i];
				float tmp_co[3], tmp_no[3];

				int tot_rays, done_rays = 0;
				float poly_area_2d_inv, done_area = 0.0f;

				const float zvec[3] = {0.0f, 0.0f, 1.0f};
				float pcent_dst[3];
				float to_pnor_2d_mat[3][3], from_pnor_2d_mat[3][3];
				float poly_dst_2d_min[2], poly_dst_2d_max[2], poly_dst_2d_z;
				float poly_dst_2d_size[2];

				float totweights = 0.0f;
				float hit_dist_accum = 0.0f;
				int sources_num = 0;
				const int tris_num = mp->totloop - 2;
				int j;

				BKE_mesh_calc_poly_center(mp, &loops_dst[mp->loopstart], verts_dst, pcent_dst);
				copy_v3_v3(tmp_no, poly_nors_dst[i]);
				/* We do our transform here, else it'd be redone by raycast helper for each ray, ugh! */
				if (space_transform) {
					BLI_space_transform_apply(space_transform, pcent_dst);
					BLI_space_transform_apply_normal(space_transform, tmp_no);
				}

				fill_vn_fl(weights, (int)numpolys_src, 0.0f);

				if (UNLIKELY((size_t)mp->totloop > tmp_poly_size)) {
					tmp_poly_size = (size_t)mp->totloop;
					poly_vcos_2d = MEM_reallocN(poly_vcos_2d, sizeof(*poly_vcos_2d) * tmp_poly_size);
					tri_vidx_2d = MEM_reallocN(tri_vidx_2d, sizeof(*tri_vidx_2d) * (tmp_poly_size - 2));
				}

				rotation_between_vecs_to_mat3(to_pnor_2d_mat, tmp_no, zvec);
				invert_m3_m3(from_pnor_2d_mat, to_pnor_2d_mat);

				mul_m3_v3(to_pnor_2d_mat, pcent_dst);
				poly_dst_2d_z = pcent_dst[2];

				/* Get (2D) bounding square of our poly. */
				INIT_MINMAX2(poly_dst_2d_min, poly_dst_2d_max);

				for (j = 0; j < mp->totloop; j++) {
					MLoop *ml = &loops_dst[j + mp->loopstart];
					copy_v3_v3(tmp_co, verts_dst[ml->v].co);
					if (space_transform) {
						BLI_space_transform_apply(space_transform, tmp_co);
					}
					mul_v2_m3v3(poly_vcos_2d[j], to_pnor_2d_mat, tmp_co);
					minmax_v2v2_v2(poly_dst_2d_min, poly_dst_2d_max, poly_vcos_2d[j]);
				}

				/* We adjust our ray-casting grid to ray_radius (the smaller, the more rays are cast),
				 * with lower/upper bounds. */
				sub_v2_v2v2(poly_dst_2d_size, poly_dst_2d_max, poly_dst_2d_min);

				if (ray_radius) {
					tot_rays = (int)((max_ff(poly_dst_2d_size[0], poly_dst_2d_size[1]) / ray_radius) + 0.5f);
					CLAMP(tot_rays, MREMAP_RAYCAST_TRI_SAMPLES_MIN, MREMAP_RAYCAST_TRI_SAMPLES_MAX);
				}
				else {
					/* If no radius (pure rays), give max number of rays! */
					tot_rays = MREMAP_RAYCAST_TRI_SAMPLES_MIN;
				}
				tot_rays *= tot_rays;

				poly_area_2d_inv = 1.0f / area_poly_v2((const float(*)[2])poly_vcos_2d, (unsigned int)mp->totloop);

				/* Tessellate our poly. */
				if (mp->totloop == 3) {
					tri_vidx_2d[0][0] = 0;
					tri_vidx_2d[0][1] = 1;
					tri_vidx_2d[0][2] = 2;
				}
				if (mp->totloop == 4) {
					tri_vidx_2d[0][0] = 0;
					tri_vidx_2d[0][1] = 1;
					tri_vidx_2d[0][2] = 2;
					tri_vidx_2d[1][0] = 0;
					tri_vidx_2d[1][1] = 2;
					tri_vidx_2d[1][2] = 3;
				}
				else {
					BLI_polyfill_calc((const float(*)[2])poly_vcos_2d, (unsigned int)mp->totloop, -1,
					                  (unsigned int (*)[3])tri_vidx_2d);
				}

				for (j = 0; j < tris_num; j++) {
					float *v1 = poly_vcos_2d[tri_vidx_2d[j][0]];
					float *v2 = poly_vcos_2d[tri_vidx_2d[j][1]];
					float *v3 = poly_vcos_2d[tri_vidx_2d[j][2]];
					int rays_num;

					/* All this allows us to get 'absolute' number of rays for each tri, avoiding accumulating
					 * errors over iterations, and helping better even distribution. */
					done_area += area_tri_v2(v1, v2, v3);
					rays_num = (int)((float)tot_rays * done_area * poly_area_2d_inv + 0.5f) - done_rays;
					done_rays += rays_num;

					while (rays_num--) {
						int n = (ray_radius > 0.0f) ? MREMAP_RAYCAST_APPROXIMATE_NR : 1;
						float w = 1.0f;

						BLI_rng_get_tri_sample_float_v2(rng, v1, v2, v3, tmp_co);

						tmp_co[2] = poly_dst_2d_z;
						mul_m3_v3(from_pnor_2d_mat, tmp_co);

						/* At this point, tmp_co is a point on our poly surface, in mesh_src space! */
						while (n--) {
							/* Note we handle dest to src space conversion ourself, here! */

							if (mesh_remap_bvhtree_query_raycast(
							        &treedata, &rayhit, NULL,
							        tmp_co, tmp_no, ray_radius / w, max_dist, &hit_dist))
							{
								weights[tessface_to_poly_map_src[rayhit.index]] += w;
								totweights += w;
								hit_dist_accum += hit_dist;
								break;
							}
							/* Next iteration will get bigger radius but smaller weight! */
							w /= MREMAP_RAYCAST_APPROXIMATE_FAC;
						}
					}
				}

				if (totweights > 0.0f) {
					for (j = 0; j < (int)numpolys_src; j++) {
						if (!weights[j]) {
							continue;
						}
						/* Note: sources_num is always <= j! */
						weights[sources_num] = weights[j] / totweights;
						indices[sources_num] = j;
						sources_num++;
					}
					mesh_remap_item_define(r_map, i, hit_dist_accum / totweights, 0, sources_num, indices, weights);
				}
				else {
					/* No source for this dest poly! */
					BKE_mesh_remap_item_define_invalid(r_map, i);
				}
			}

			MEM_freeN(tri_vidx_2d);
			MEM_freeN(poly_vcos_2d);
			MEM_freeN(indices);
			MEM_freeN(weights);
			BLI_rng_free(rng);
		}
		else {
			printf("WARNING! Unsupported mesh-to-mesh poly mapping mode (%d)!\n", mode);
			memset(r_map->items, 0, sizeof(*r_map->items) * (size_t)numpolys_dst);
		}

		free_bvhtree_from_mesh(&treedata);
	}
}

#undef MREMAP_RAYCAST_APPROXIMATE_NR
#undef MREMAP_RAYCAST_APPROXIMATE_FAC
#undef MREMAP_RAYCAST_APPROXIMATE_BVHEPSILON
#undef MREMAP_RAYCAST_TRI_SAMPLES_MIN
#undef MREMAP_RAYCAST_TRI_SAMPLES_MAX
#undef MREMAP_DEFAULT_BUFSIZE

/** \} */
