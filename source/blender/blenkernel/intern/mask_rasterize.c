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
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation,
 *                 Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/mask_rasterize.c
 *  \ingroup bke
 *
 * This module exposes a rasterizer that works as a black box - implementation details are confined to this file,
 *
 * The basic method to access is:
 * - create & initialize a handle from a #Mask datablock.
 * - execute pixel lookups.
 * - free the handle.
 *
 * This file is admittedly a bit confusticated, in quite few areas speed was chosen over readability,
 * though it is commented - so shouldn't be so hard to see whats going on.
 *
 *
 * Implementation:
 *
 * To rasterize the mask its converted into geometry that use a ray-cast for each pixel lookup.
 *
 * Initially 'kdopbvh' was used but this ended up being too slow.
 *
 * To gain some extra speed we take advantage of a few shortcuts that can be made rasterizing masks specifically.
 * - all triangles are known to be completely white - so no depth check is done on triangle intersection.
 * - all quads are known to be feather outlines - the 1 and 0 depths are known by the vertex order in the quad,
 * - there is no color - just a value for each mask pixel.
 * - the mask spacial structure always maps to space 0-1 on X and Y axis.
 * - bucketing is used to speed up lookups for geometry.
 *
 * Other Details:
 * - used unsigned values all over for some extra speed on some arch's.
 * - anti-aliasing is faked, just ensuring at least one pixel feather - avoids oversampling.
 * - initializing the spacial structure doesn't need to be as optimized as pixel lookups are.
 * - mask lookups need not be pixel aligned so any sub-pixel values from x/y (0 - 1), can be found.
 *   (perhaps masks can be used as a vector texture in 3D later on)
 *
 *
 * Currently, to build the spacial structure we have to calculate the total number of faces ahead of time.
 *
 * This is getting a bit complicated with the addition of unfilled splines and end capping -
 * If large changes are needed here we would be better off using an iterable
 * BLI_mempool for triangles and converting to a contiguous array afterwards.
 *
 * - Campbell
 */

#include "MEM_guardedalloc.h"

#include "DNA_vec_types.h"
#include "DNA_mask_types.h"
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"
#include "BLI_scanfill.h"
#include "BLI_memarena.h"

#include "BLI_math.h"
#include "BLI_rect.h"
#include "BLI_listbase.h"
#include "BLI_linklist.h"
#include "BLI_strict_flags.h"

#include "BKE_mask.h"

/* this is rather and annoying hack, use define to isolate it.
 * problem is caused by scanfill removing edges on us. */
#define USE_SCANFILL_EDGE_WORKAROUND

#define SPLINE_RESOL_CAP_PER_PIXEL 2
#define SPLINE_RESOL_CAP_MIN 8
#define SPLINE_RESOL_CAP_MAX 64

/* found this gives best performance for high detail masks, values between 2 and 8 work best */
#define BUCKET_PIXELS_PER_CELL 4

#define SF_EDGE_IS_BOUNDARY 0xff
#define SF_KEYINDEX_TEMP_ID ((unsigned int) -1)

#define TRI_TERMINATOR_ID   ((unsigned int) -1)
#define TRI_VERT            ((unsigned int) -1)

/* for debugging add... */
#ifndef NDEBUG
/* printf("%u %u %u %u\n", _t[0], _t[1], _t[2], _t[3]); \ */
#  define FACE_ASSERT(face, vert_max)                    \
{                                                        \
	unsigned int *_t = face;                             \
	BLI_assert(_t[0] < vert_max);                        \
	BLI_assert(_t[1] < vert_max);                        \
	BLI_assert(_t[2] < vert_max);                        \
	BLI_assert(_t[3] < vert_max || _t[3] == TRI_VERT);   \
} (void)0
#else
   /* do nothing */
#  define FACE_ASSERT(face, vert_max)
#endif

static void rotate_point_v2(float r_p[2], const float p[2], const float cent[2], const float angle, const float asp[2])
{
	const float s = sinf(angle);
	const float c = cosf(angle);
	float p_new[2];

	/* translate point back to origin */
	r_p[0] = (p[0] - cent[0]) / asp[0];
	r_p[1] = (p[1] - cent[1]) / asp[1];

	/* rotate point */
	p_new[0] = ((r_p[0] * c) - (r_p[1] * s)) * asp[0];
	p_new[1] = ((r_p[0] * s) + (r_p[1] * c)) * asp[1];

	/* translate point back */
	r_p[0] = p_new[0] + cent[0];
	r_p[1] = p_new[1] + cent[1];
}

BLI_INLINE unsigned int clampis_uint(const unsigned int v, const unsigned int min, const unsigned int max)
{
	return v < min ? min : (v > max ? max : v);
}

/* --------------------------------------------------------------------- */
/* local structs for mask rasterizeing                                   */
/* --------------------------------------------------------------------- */

/**
 * A single #MaskRasterHandle contains multiple #MaskRasterLayer's,
 * each #MaskRasterLayer does its own lookup which contributes to
 * the final pixel with its own blending mode and the final pixel
 * is blended between these.
 */

/* internal use only */
typedef struct MaskRasterLayer {
	/* geometry */
	unsigned int   face_tot;
	unsigned int (*face_array)[4];  /* access coords tri/quad */
	float        (*face_coords)[3]; /* xy, z 0-1 (1.0 == filled) */


	/* 2d bounds (to quickly skip bucket lookup) */
	rctf bounds;


	/* buckets */
	unsigned int **buckets_face;
	/* cache divide and subtract */
	float buckets_xy_scalar[2]; /* (1.0 / (buckets_width + FLT_EPSILON)) * buckets_x */
	unsigned int buckets_x;
	unsigned int buckets_y;


	/* copied direct from #MaskLayer.--- */
	/* blending options */
	float  alpha;
	char   blend;
	char   blend_flag;
	char   falloff;

} MaskRasterLayer;

typedef struct MaskRasterSplineInfo {
	/* body of the spline */
	unsigned int vertex_offset;
	unsigned int vertex_total;

	/* capping for non-filled, non cyclic splines */
	unsigned int vertex_total_cap_head;
	unsigned int vertex_total_cap_tail;

	bool is_cyclic;
} MaskRasterSplineInfo;

/**
 * opaque local struct for mask pixel lookup, each MaskLayer needs one of these
 */
struct MaskRasterHandle {
	MaskRasterLayer *layers;
	unsigned int     layers_tot;

	/* 2d bounds (to quickly skip bucket lookup) */
	rctf bounds;
};

/* --------------------------------------------------------------------- */
/* alloc / free functions                                                */
/* --------------------------------------------------------------------- */

MaskRasterHandle *BKE_maskrasterize_handle_new(void)
{
	MaskRasterHandle *mr_handle;

	mr_handle = MEM_callocN(sizeof(MaskRasterHandle), "MaskRasterHandle");

	return mr_handle;
}

void BKE_maskrasterize_handle_free(MaskRasterHandle *mr_handle)
{
	const unsigned int layers_tot = mr_handle->layers_tot;
	unsigned int i;
	MaskRasterLayer *layer = mr_handle->layers;

	for (i = 0; i < layers_tot; i++, layer++) {

		if (layer->face_array) {
			MEM_freeN(layer->face_array);
		}

		if (layer->face_coords) {
			MEM_freeN(layer->face_coords);
		}

		if (layer->buckets_face) {
			const unsigned int   bucket_tot = layer->buckets_x * layer->buckets_y;
			unsigned int bucket_index;
			for (bucket_index = 0; bucket_index < bucket_tot; bucket_index++) {
				unsigned int *face_index = layer->buckets_face[bucket_index];
				if (face_index) {
					MEM_freeN(face_index);
				}
			}

			MEM_freeN(layer->buckets_face);
		}
	}

	MEM_freeN(mr_handle->layers);
	MEM_freeN(mr_handle);
}


static void maskrasterize_spline_differentiate_point_outset(float (*diff_feather_points)[2], float (*diff_points)[2],
                                                            const unsigned int tot_diff_point, const float ofs,
                                                            const bool do_test)
{
	unsigned int k_prev = tot_diff_point - 2;
	unsigned int k_curr = tot_diff_point - 1;
	unsigned int k_next = 0;

	unsigned int k;

	float d_prev[2];
	float d_next[2];
	float d[2];

	const float *co_prev;
	const float *co_curr;
	const float *co_next;

	const float ofs_squared = ofs * ofs;

	co_prev = diff_points[k_prev];
	co_curr = diff_points[k_curr];
	co_next = diff_points[k_next];

	/* precalc */
	sub_v2_v2v2(d_prev, co_prev, co_curr);
	normalize_v2(d_prev);

	for (k = 0; k < tot_diff_point; k++) {

		/* co_prev = diff_points[k_prev]; */ /* precalc */
		co_curr = diff_points[k_curr];
		co_next = diff_points[k_next];

		/* sub_v2_v2v2(d_prev, co_prev, co_curr); */ /* precalc */
		sub_v2_v2v2(d_next, co_curr, co_next);

		/* normalize_v2(d_prev); */ /* precalc */
		normalize_v2(d_next);

		if ((do_test == FALSE) ||
		    (len_squared_v2v2(diff_feather_points[k], diff_points[k]) < ofs_squared))
		{

			add_v2_v2v2(d, d_prev, d_next);

			normalize_v2(d);

			diff_feather_points[k][0] = diff_points[k][0] + ( d[1] * ofs);
			diff_feather_points[k][1] = diff_points[k][1] + (-d[0] * ofs);
		}

		/* use next iter */
		copy_v2_v2(d_prev, d_next);

		/* k_prev = k_curr; */ /* precalc */
		k_curr = k_next;
		k_next++;
	}
}

/* this function is not exact, sometimes it returns false positives,
 * the main point of it is to clear out _almost_ all bucket/face non-intersections,
 * returning TRUE in corner cases is ok but missing an intersection is NOT.
 *
 * method used
 * - check if the center of the buckets bounding box is intersecting the face
 * - if not get the max radius to a corner of the bucket and see how close we
 *   are to any of the triangle edges.
 */
static bool layer_bucket_isect_test(
        MaskRasterLayer *layer, unsigned int face_index,
        const unsigned int bucket_x, const unsigned int bucket_y,
        const float bucket_size_x, const float bucket_size_y,
        const float bucket_max_rad_squared)
{
	unsigned int *face = layer->face_array[face_index];
	float (*cos)[3] = layer->face_coords;

	const float xmin = layer->bounds.xmin + (bucket_size_x * (float)bucket_x);
	const float ymin = layer->bounds.ymin + (bucket_size_y * (float)bucket_y);
	const float xmax = xmin + bucket_size_x;
	const float ymax = ymin + bucket_size_y;

	const float cent[2] = {(xmin + xmax) * 0.5f,
	                       (ymin + ymax) * 0.5f};

	if (face[3] == TRI_VERT) {
		const float *v1 = cos[face[0]];
		const float *v2 = cos[face[1]];
		const float *v3 = cos[face[2]];

		if (isect_point_tri_v2(cent, v1, v2, v3)) {
			return TRUE;
		}
		else {
			if ((dist_squared_to_line_segment_v2(cent, v1, v2) < bucket_max_rad_squared) ||
			    (dist_squared_to_line_segment_v2(cent, v2, v3) < bucket_max_rad_squared) ||
			    (dist_squared_to_line_segment_v2(cent, v3, v1) < bucket_max_rad_squared))
			{
				return TRUE;
			}
			else {
				// printf("skip tri\n");
				return FALSE;
			}
		}

	}
	else {
		const float *v1 = cos[face[0]];
		const float *v2 = cos[face[1]];
		const float *v3 = cos[face[2]];
		const float *v4 = cos[face[3]];

		if (isect_point_tri_v2(cent, v1, v2, v3)) {
			return TRUE;
		}
		else if (isect_point_tri_v2(cent, v1, v3, v4)) {
			return TRUE;
		}
		else {
			if ((dist_squared_to_line_segment_v2(cent, v1, v2) < bucket_max_rad_squared) ||
			    (dist_squared_to_line_segment_v2(cent, v2, v3) < bucket_max_rad_squared) ||
			    (dist_squared_to_line_segment_v2(cent, v3, v4) < bucket_max_rad_squared) ||
			    (dist_squared_to_line_segment_v2(cent, v4, v1) < bucket_max_rad_squared))
			{
				return TRUE;
			}
			else {
				// printf("skip quad\n");
				return FALSE;
			}
		}
	}
}

static void layer_bucket_init_dummy(MaskRasterLayer *layer)
{
	layer->face_tot = 0;
	layer->face_coords = NULL;
	layer->face_array  = NULL;

	layer->buckets_x = 0;
	layer->buckets_y = 0;

	layer->buckets_xy_scalar[0] = 0.0f;
	layer->buckets_xy_scalar[1] = 0.0f;

	layer->buckets_face = NULL;

	BLI_rctf_init(&layer->bounds, -1.0f, -1.0f, -1.0f, -1.0f);
}

static void layer_bucket_init(MaskRasterLayer *layer, const float pixel_size)
{
	MemArena *arena = BLI_memarena_new(MEM_SIZE_OPTIMAL(1 << 16), __func__);

	const float bucket_dim_x = BLI_rctf_size_x(&layer->bounds);
	const float bucket_dim_y = BLI_rctf_size_y(&layer->bounds);

	layer->buckets_x = (unsigned int)((bucket_dim_x / pixel_size) / (float)BUCKET_PIXELS_PER_CELL);
	layer->buckets_y = (unsigned int)((bucket_dim_y / pixel_size) / (float)BUCKET_PIXELS_PER_CELL);

//		printf("bucket size %ux%u\n", layer->buckets_x, layer->buckets_y);

	CLAMP(layer->buckets_x, 8, 512);
	CLAMP(layer->buckets_y, 8, 512);

	layer->buckets_xy_scalar[0] = (1.0f / (bucket_dim_x + FLT_EPSILON)) * (float)layer->buckets_x;
	layer->buckets_xy_scalar[1] = (1.0f / (bucket_dim_y + FLT_EPSILON)) * (float)layer->buckets_y;

	{
		/* width and height of each bucket */
		const float bucket_size_x = (bucket_dim_x + FLT_EPSILON) / (float)layer->buckets_x;
		const float bucket_size_y = (bucket_dim_y + FLT_EPSILON) / (float)layer->buckets_y;
		const float bucket_max_rad = (max_ff(bucket_size_x, bucket_size_y) * (float)M_SQRT2) + FLT_EPSILON;
		const float bucket_max_rad_squared = bucket_max_rad * bucket_max_rad;

		unsigned int *face = &layer->face_array[0][0];
		float (*cos)[3] = layer->face_coords;

		const unsigned int  bucket_tot = layer->buckets_x * layer->buckets_y;
		LinkNode     **bucketstore     = MEM_callocN(bucket_tot * sizeof(LinkNode *),  __func__);
		unsigned int  *bucketstore_tot = MEM_callocN(bucket_tot * sizeof(unsigned int), __func__);

		unsigned int face_index;

		for (face_index = 0; face_index < layer->face_tot; face_index++, face += 4) {
			float xmin;
			float xmax;
			float ymin;
			float ymax;

			if (face[3] == TRI_VERT) {
				const float *v1 = cos[face[0]];
				const float *v2 = cos[face[1]];
				const float *v3 = cos[face[2]];

				xmin = min_ff(v1[0], min_ff(v2[0], v3[0]));
				xmax = max_ff(v1[0], max_ff(v2[0], v3[0]));
				ymin = min_ff(v1[1], min_ff(v2[1], v3[1]));
				ymax = max_ff(v1[1], max_ff(v2[1], v3[1]));
			}
			else {
				const float *v1 = cos[face[0]];
				const float *v2 = cos[face[1]];
				const float *v3 = cos[face[2]];
				const float *v4 = cos[face[3]];

				xmin = min_ff(v1[0], min_ff(v2[0], min_ff(v3[0], v4[0])));
				xmax = max_ff(v1[0], max_ff(v2[0], max_ff(v3[0], v4[0])));
				ymin = min_ff(v1[1], min_ff(v2[1], min_ff(v3[1], v4[1])));
				ymax = max_ff(v1[1], max_ff(v2[1], max_ff(v3[1], v4[1])));
			}


			/* not essential but may as will skip any faces outside the view */
			if (!((xmax < 0.0f) || (ymax < 0.0f) || (xmin > 1.0f) || (ymin > 1.0f))) {

				CLAMP(xmin, 0.0f,  1.0f);
				CLAMP(ymin, 0.0f,  1.0f);
				CLAMP(xmax, 0.0f,  1.0f);
				CLAMP(ymax, 0.0f,  1.0f);

				{
					unsigned int xi_min = (unsigned int) ((xmin - layer->bounds.xmin) * layer->buckets_xy_scalar[0]);
					unsigned int xi_max = (unsigned int) ((xmax - layer->bounds.xmin) * layer->buckets_xy_scalar[0]);
					unsigned int yi_min = (unsigned int) ((ymin - layer->bounds.ymin) * layer->buckets_xy_scalar[1]);
					unsigned int yi_max = (unsigned int) ((ymax - layer->bounds.ymin) * layer->buckets_xy_scalar[1]);
					void *face_index_void = SET_UINT_IN_POINTER(face_index);

					unsigned int xi, yi;

					/* this should _almost_ never happen but since it can in extreme cases,
					 * we have to clamp the values or we overrun the buffer and crash */
					if (xi_min >= layer->buckets_x) xi_min = layer->buckets_x - 1;
					if (xi_max >= layer->buckets_x) xi_max = layer->buckets_x - 1;
					if (yi_min >= layer->buckets_y) yi_min = layer->buckets_y - 1;
					if (yi_max >= layer->buckets_y) yi_max = layer->buckets_y - 1;

					for (yi = yi_min; yi <= yi_max; yi++) {
						unsigned int bucket_index = (layer->buckets_x * yi) + xi_min;
						for (xi = xi_min; xi <= xi_max; xi++, bucket_index++) {
							// unsigned int bucket_index = (layer->buckets_x * yi) + xi; /* correct but do in outer loop */

							BLI_assert(xi < layer->buckets_x);
							BLI_assert(yi < layer->buckets_y);
							BLI_assert(bucket_index < bucket_tot);

							/* check if the bucket intersects with the face */
							/* note: there is a trade off here since checking box/tri intersections isn't
							 * as optimal as it could be, but checking pixels against faces they will never intersect
							 * with is likely the greater slowdown here - so check if the cell intersects the face */
							if (layer_bucket_isect_test(layer, face_index,
							                            xi, yi,
							                            bucket_size_x, bucket_size_y,
							                            bucket_max_rad_squared))
							{
								BLI_linklist_prepend_arena(&bucketstore[bucket_index], face_index_void, arena);
								bucketstore_tot[bucket_index]++;
							}
						}
					}
				}
			}
		}

		if (1) {
			/* now convert linknodes into arrays for faster per pixel access */
			unsigned int  **buckets_face = MEM_mallocN(bucket_tot * sizeof(*buckets_face), __func__);
			unsigned int bucket_index;

			for (bucket_index = 0; bucket_index < bucket_tot; bucket_index++) {
				if (bucketstore_tot[bucket_index]) {
					unsigned int  *bucket = MEM_mallocN((bucketstore_tot[bucket_index] + 1) * sizeof(unsigned int),
					                                    __func__);
					LinkNode *bucket_node;

					buckets_face[bucket_index] = bucket;

					for (bucket_node = bucketstore[bucket_index]; bucket_node; bucket_node = bucket_node->next) {
						*bucket = GET_UINT_FROM_POINTER(bucket_node->link);
						bucket++;
					}
					*bucket = TRI_TERMINATOR_ID;
				}
				else {
					buckets_face[bucket_index] = NULL;
				}
			}

			layer->buckets_face = buckets_face;
		}

		MEM_freeN(bucketstore);
		MEM_freeN(bucketstore_tot);
	}

	BLI_memarena_free(arena);
}

void BKE_maskrasterize_handle_init(MaskRasterHandle *mr_handle, struct Mask *mask,
                                   const int width, const int height,
                                   const bool do_aspect_correct, const bool do_mask_aa,
                                   const bool do_feather)
{
	const rctf default_bounds = {0.0f, 1.0f, 0.0f, 1.0f};
	const float pixel_size = 1.0f / (float)min_ii(width, height);
	const float asp_xy[2] = {(do_aspect_correct && width > height) ? (float)height / (float)width  : 1.0f,
	                         (do_aspect_correct && width < height) ? (float)width  / (float)height : 1.0f};

	const float zvec[3] = {0.0f, 0.0f, 1.0f};
	MaskLayer *masklay;
	unsigned int masklay_index;
	MemArena *sf_arena;

	mr_handle->layers_tot = (unsigned int)BLI_countlist(&mask->masklayers);
	mr_handle->layers = MEM_mallocN(sizeof(MaskRasterLayer) * mr_handle->layers_tot, "MaskRasterLayer");
	BLI_rctf_init_minmax(&mr_handle->bounds);

	sf_arena = BLI_memarena_new(BLI_SCANFILL_ARENA_SIZE, __func__);

	for (masklay = mask->masklayers.first, masklay_index = 0; masklay; masklay = masklay->next, masklay_index++) {

		/* we need to store vertex ranges for open splines for filling */
		unsigned int tot_splines;
		MaskRasterSplineInfo *open_spline_ranges;
		unsigned int   open_spline_index = 0;

		MaskSpline *spline;

		/* scanfill */
		ScanFillContext sf_ctx;
		ScanFillVert *sf_vert = NULL;
		ScanFillVert *sf_vert_next = NULL;
		ScanFillFace *sf_tri;

		unsigned int sf_vert_tot = 0;
		unsigned int tot_feather_quads = 0;

#ifdef USE_SCANFILL_EDGE_WORKAROUND
		unsigned int tot_boundary_used = 0;
		unsigned int tot_boundary_found = 0;
#endif

		if (masklay->restrictflag & MASK_RESTRICT_RENDER) {
			/* skip the layer */
			mr_handle->layers_tot--;
			masklay_index--;
			continue;
		}

		tot_splines = (unsigned int)BLI_countlist(&masklay->splines);
		open_spline_ranges = MEM_callocN(sizeof(*open_spline_ranges) * tot_splines, __func__);

		BLI_scanfill_begin_arena(&sf_ctx, sf_arena);

		for (spline = masklay->splines.first; spline; spline = spline->next) {
			const bool is_cyclic = (spline->flag & MASK_SPLINE_CYCLIC) != 0;
			const bool is_fill = (spline->flag & MASK_SPLINE_NOFILL) == 0;

			float (*diff_points)[2];
			unsigned int tot_diff_point;

			float (*diff_feather_points)[2];
			float (*diff_feather_points_flip)[2];
			unsigned int tot_diff_feather_points;

			const unsigned int resol_a = BKE_mask_spline_resolution(spline, width, height) / 4;
			const unsigned int resol_b = BKE_mask_spline_feather_resolution(spline, width, height) / 4;
			const unsigned int resol = CLAMPIS(MAX2(resol_a, resol_b), 4, 512);

			diff_points = BKE_mask_spline_differentiate_with_resolution(
			                  spline, &tot_diff_point, resol);

			if (do_feather) {
				diff_feather_points = BKE_mask_spline_feather_differentiated_points_with_resolution(
				                          spline, &tot_diff_feather_points, resol, FALSE);
				BLI_assert(diff_feather_points);
			}
			else {
				tot_diff_feather_points = 0;
				diff_feather_points = NULL;
			}

			if (tot_diff_point > 3) {
				ScanFillVert *sf_vert_prev;
				unsigned int j;

				float co[3];
				co[2] = 0.0f;

				if (do_aspect_correct) {
					if (width != height) {
						float *fp;
						float *ffp;
						unsigned int i;
						float asp;

						if (width < height) {
							fp = &diff_points[0][0];
							ffp = tot_diff_feather_points ? &diff_feather_points[0][0] : NULL;
							asp = (float)width / (float)height;
						}
						else {
							fp = &diff_points[0][1];
							ffp = tot_diff_feather_points ? &diff_feather_points[0][1] : NULL;
							asp = (float)height / (float)width;
						}

						for (i = 0; i < tot_diff_point; i++, fp += 2) {
							(*fp) = (((*fp) - 0.5f) / asp) + 0.5f;
						}

						if (tot_diff_feather_points) {
							for (i = 0; i < tot_diff_feather_points; i++, ffp += 2) {
								(*ffp) = (((*ffp) - 0.5f) / asp) + 0.5f;
							}
						}
					}
				}

				/* fake aa, using small feather */
				if (do_mask_aa == TRUE) {
					if (do_feather == FALSE) {
						tot_diff_feather_points = tot_diff_point;
						diff_feather_points = MEM_mallocN(sizeof(*diff_feather_points) *
						                                  (size_t)tot_diff_feather_points,
						                                  __func__);
						/* add single pixel feather */
						maskrasterize_spline_differentiate_point_outset(diff_feather_points, diff_points,
						                                               tot_diff_point, pixel_size, FALSE);
					}
					else {
						/* ensure single pixel feather, on any zero feather areas */
						maskrasterize_spline_differentiate_point_outset(diff_feather_points, diff_points,
						                                               tot_diff_point, pixel_size, TRUE);
					}
				}

				if (is_fill) {
					/* applt intersections depending on fill settings */
					if (spline->flag & MASK_SPLINE_NOINTERSECT) {
						BKE_mask_spline_feather_collapse_inner_loops(spline, diff_feather_points, tot_diff_feather_points);
					}

					copy_v2_v2(co, diff_points[0]);
					sf_vert_prev = BLI_scanfill_vert_add(&sf_ctx, co);
					sf_vert_prev->tmp.u = sf_vert_tot;
					sf_vert_prev->keyindex = sf_vert_tot + tot_diff_point; /* absolute index of feather vert */
					sf_vert_tot++;

					/* TODO, an alternate functions so we can avoid double vector copy! */
					for (j = 1; j < tot_diff_point; j++) {
						copy_v2_v2(co, diff_points[j]);
						sf_vert = BLI_scanfill_vert_add(&sf_ctx, co);
						sf_vert->tmp.u = sf_vert_tot;
						sf_vert->keyindex = sf_vert_tot + tot_diff_point; /* absolute index of feather vert */
						sf_vert_tot++;
					}

					sf_vert = sf_vert_prev;
					sf_vert_prev = sf_ctx.fillvertbase.last;

					for (j = 0; j < tot_diff_point; j++) {
						ScanFillEdge *sf_edge = BLI_scanfill_edge_add(&sf_ctx, sf_vert_prev, sf_vert);

#ifdef USE_SCANFILL_EDGE_WORKAROUND
						if (diff_feather_points) {
							sf_edge->tmp.c = SF_EDGE_IS_BOUNDARY;
							tot_boundary_used++;
						}
#else
						(void)sf_edge;
#endif
						sf_vert_prev = sf_vert;
						sf_vert = sf_vert->next;
					}

					if (diff_feather_points) {
						float co_feather[3];
						co_feather[2] = 1.0f;

						BLI_assert(tot_diff_feather_points == tot_diff_point);

						/* note: only added for convenience, we don't infact use these to scanfill,
						 * only to create feather faces after scanfill */
						for (j = 0; j < tot_diff_feather_points; j++) {
							copy_v2_v2(co_feather, diff_feather_points[j]);
							sf_vert = BLI_scanfill_vert_add(&sf_ctx, co_feather);

							/* no need for these attrs */
#if 0
							sf_vert->tmp.u = sf_vert_tot;
							sf_vert->keyindex = sf_vert_tot + tot_diff_point; /* absolute index of feather vert */
#endif
							sf_vert->keyindex = SF_KEYINDEX_TEMP_ID;
							sf_vert_tot++;
						}

						tot_feather_quads += tot_diff_point;
					}
				}
				else {
					/* unfilled spline */
					if (diff_feather_points) {

						float co_diff[2];

						float co_feather[3];
						co_feather[2] = 1.0f;

						if (spline->flag & MASK_SPLINE_NOINTERSECT) {
							diff_feather_points_flip = MEM_mallocN(sizeof(float) * 2 * tot_diff_feather_points, "diff_feather_points_flip");

							for (j = 0; j < tot_diff_point; j++) {
								sub_v2_v2v2(co_diff, diff_points[j], diff_feather_points[j]);
								add_v2_v2v2(diff_feather_points_flip[j], diff_points[j], co_diff);
							}

							BKE_mask_spline_feather_collapse_inner_loops(spline, diff_feather_points,      tot_diff_feather_points);
							BKE_mask_spline_feather_collapse_inner_loops(spline, diff_feather_points_flip, tot_diff_feather_points);
						}
						else {
							diff_feather_points_flip = NULL;
						}


						open_spline_ranges[open_spline_index].vertex_offset = sf_vert_tot;
						open_spline_ranges[open_spline_index].vertex_total = tot_diff_point;

						/* TODO, an alternate functions so we can avoid double vector copy! */
						for (j = 0; j < tot_diff_point; j++) {

							/* center vert */
							copy_v2_v2(co, diff_points[j]);
							sf_vert = BLI_scanfill_vert_add(&sf_ctx, co);
							sf_vert->tmp.u = sf_vert_tot;
							sf_vert->keyindex = SF_KEYINDEX_TEMP_ID;
							sf_vert_tot++;


							/* feather vert A */
							copy_v2_v2(co_feather, diff_feather_points[j]);
							sf_vert = BLI_scanfill_vert_add(&sf_ctx, co_feather);
							sf_vert->tmp.u = sf_vert_tot;
							sf_vert->keyindex = SF_KEYINDEX_TEMP_ID;
							sf_vert_tot++;


							/* feather vert B */
							if (diff_feather_points_flip) {
								copy_v2_v2(co_feather, diff_feather_points_flip[j]);
							}
							else {
								sub_v2_v2v2(co_diff, co, co_feather);
								add_v2_v2v2(co_feather, co, co_diff);
							}

							sf_vert = BLI_scanfill_vert_add(&sf_ctx, co_feather);
							sf_vert->tmp.u = sf_vert_tot;
							sf_vert->keyindex = SF_KEYINDEX_TEMP_ID;
							sf_vert_tot++;

							tot_feather_quads += 2;
						}

						if (!is_cyclic) {
							tot_feather_quads -= 2;
						}

						if (diff_feather_points_flip) {
							MEM_freeN(diff_feather_points_flip);
							diff_feather_points_flip = NULL;
						}

						/* cap ends */

						/* dummy init value */
						open_spline_ranges[open_spline_index].vertex_total_cap_head = 0;
						open_spline_ranges[open_spline_index].vertex_total_cap_tail = 0;

						if (!is_cyclic) {
							float *fp_cent;
							float *fp_turn;

							unsigned int k;

							fp_cent = diff_points[0];
							fp_turn = diff_feather_points[0];

#define CALC_CAP_RESOL                                                                      \
	clampis_uint((unsigned int )(len_v2v2(fp_cent, fp_turn) /                               \
	                             (pixel_size * SPLINE_RESOL_CAP_PER_PIXEL)),                \
	             SPLINE_RESOL_CAP_MIN, SPLINE_RESOL_CAP_MAX)

							{
								const unsigned int vertex_total_cap = CALC_CAP_RESOL;

								for (k = 1; k < vertex_total_cap; k++) {
									const float angle = (float)k * (1.0f / (float)vertex_total_cap) * (float)M_PI;
									rotate_point_v2(co_feather, fp_turn, fp_cent, angle, asp_xy);

									sf_vert = BLI_scanfill_vert_add(&sf_ctx, co_feather);
									sf_vert->tmp.u = sf_vert_tot;
									sf_vert->keyindex = SF_KEYINDEX_TEMP_ID;
									sf_vert_tot++;
								}
								tot_feather_quads += vertex_total_cap;

								open_spline_ranges[open_spline_index].vertex_total_cap_head = vertex_total_cap;
							}

							fp_cent = diff_points[tot_diff_point - 1];
							fp_turn = diff_feather_points[tot_diff_point - 1];

							{
								const unsigned int vertex_total_cap = CALC_CAP_RESOL;

								for (k = 1; k < vertex_total_cap; k++) {
									const float angle = (float)k * (1.0f / (float)vertex_total_cap) * (float)M_PI;
									rotate_point_v2(co_feather, fp_turn, fp_cent, -angle, asp_xy);

									sf_vert = BLI_scanfill_vert_add(&sf_ctx, co_feather);
									sf_vert->tmp.u = sf_vert_tot;
									sf_vert->keyindex = SF_KEYINDEX_TEMP_ID;
									sf_vert_tot++;
								}
								tot_feather_quads += vertex_total_cap;

								open_spline_ranges[open_spline_index].vertex_total_cap_tail = vertex_total_cap;
							}
						}

						open_spline_ranges[open_spline_index].is_cyclic = is_cyclic;
						open_spline_index++;

#undef CALC_CAP_RESOL
						/* end capping */

					}
				}
			}

			if (diff_points) {
				MEM_freeN(diff_points);
			}

			if (diff_feather_points) {
				MEM_freeN(diff_feather_points);
			}
		}

		{
			unsigned int (*face_array)[4], *face;  /* access coords */
			float        (*face_coords)[3], *cos; /* xy, z 0-1 (1.0 == filled) */
			unsigned int sf_tri_tot;
			rctf bounds;
			unsigned int face_index;

			/* now we have all the splines */
			face_coords = MEM_mallocN((sizeof(float) * 3) * sf_vert_tot, "maskrast_face_coords");

			/* init bounds */
			BLI_rctf_init_minmax(&bounds);

			/* coords */
			cos = (float *)face_coords;
			for (sf_vert = sf_ctx.fillvertbase.first; sf_vert; sf_vert = sf_vert_next) {
				sf_vert_next = sf_vert->next;
				copy_v3_v3(cos, sf_vert->co);

				/* remove so as not to interfere with fill (called after) */
				if (sf_vert->keyindex == SF_KEYINDEX_TEMP_ID) {
					BLI_remlink(&sf_ctx.fillvertbase, sf_vert);
				}

				/* bounds */
				BLI_rctf_do_minmax_v(&bounds, cos);

				cos += 3;
			}

			/* main scan-fill */
			sf_tri_tot = (unsigned int)BLI_scanfill_calc_ex(&sf_ctx, BLI_SCANFILL_CALC_HOLES, zvec);

			face_array = MEM_mallocN(sizeof(*face_array) * ((size_t)sf_tri_tot + (size_t)tot_feather_quads), "maskrast_face_index");
			face_index = 0;

			/* faces */
			face = (unsigned int *)face_array;
			for (sf_tri = sf_ctx.fillfacebase.first; sf_tri; sf_tri = sf_tri->next) {
				*(face++) = sf_tri->v3->tmp.u;
				*(face++) = sf_tri->v2->tmp.u;
				*(face++) = sf_tri->v1->tmp.u;
				*(face++) = TRI_VERT;
				face_index++;
				FACE_ASSERT(face - 4, sf_vert_tot);
			}

			/* start of feather faces... if we have this set,
			 * 'face_index' is kept from loop above */

			BLI_assert(face_index == sf_tri_tot);

			if (tot_feather_quads) {
				ScanFillEdge *sf_edge;

				for (sf_edge = sf_ctx.filledgebase.first; sf_edge; sf_edge = sf_edge->next) {
					if (sf_edge->tmp.c == SF_EDGE_IS_BOUNDARY) {
						*(face++) = sf_edge->v1->tmp.u;
						*(face++) = sf_edge->v2->tmp.u;
						*(face++) = sf_edge->v2->keyindex;
						*(face++) = sf_edge->v1->keyindex;
						face_index++;
						FACE_ASSERT(face - 4, sf_vert_tot);

#ifdef USE_SCANFILL_EDGE_WORKAROUND
						tot_boundary_found++;
#endif
					}
				}
			}

#ifdef USE_SCANFILL_EDGE_WORKAROUND
			if (tot_boundary_found != tot_boundary_used) {
				BLI_assert(tot_boundary_found < tot_boundary_used);
			}
#endif

			/* feather only splines */
			while (open_spline_index > 0) {
				const unsigned int vertex_offset         = open_spline_ranges[--open_spline_index].vertex_offset;
				unsigned int       vertex_total          = open_spline_ranges[  open_spline_index].vertex_total;
				unsigned int       vertex_total_cap_head = open_spline_ranges[  open_spline_index].vertex_total_cap_head;
				unsigned int       vertex_total_cap_tail = open_spline_ranges[  open_spline_index].vertex_total_cap_tail;
				unsigned int k, j;

				j = vertex_offset;

				/* subtract one since we reference next vertex triple */
				for (k = 0; k < vertex_total - 1; k++, j += 3) {

					BLI_assert(j == vertex_offset + (k * 3));

					*(face++) = j + 3; /* next span */ /* z 1 */
					*(face++) = j + 0;                 /* z 1 */
					*(face++) = j + 1;                 /* z 0 */
					*(face++) = j + 4; /* next span */ /* z 0 */
					face_index++;
					FACE_ASSERT(face - 4, sf_vert_tot);

					*(face++) = j + 0;                 /* z 1 */
					*(face++) = j + 3; /* next span */ /* z 1 */
					*(face++) = j + 5; /* next span */ /* z 0 */
					*(face++) = j + 2;                 /* z 0 */
					face_index++;
					FACE_ASSERT(face - 4, sf_vert_tot);
				}

				if (open_spline_ranges[open_spline_index].is_cyclic) {
					*(face++) = vertex_offset + 0; /* next span */ /* z 1 */
					*(face++) = j             + 0;                 /* z 1 */
					*(face++) = j             + 1;                 /* z 0 */
					*(face++) = vertex_offset + 1; /* next span */ /* z 0 */
					face_index++;
					FACE_ASSERT(face - 4, sf_vert_tot);

					*(face++) = j          + 0;                    /* z 1 */
					*(face++) = vertex_offset + 0; /* next span */ /* z 1 */
					*(face++) = vertex_offset + 2; /* next span */ /* z 0 */
					*(face++) = j          + 2;                    /* z 0 */
					face_index++;
					FACE_ASSERT(face - 4, sf_vert_tot);
				}
				else {
					unsigned int midvidx = vertex_offset;

					/***************
					 * cap end 'a' */
					j = midvidx + (vertex_total * 3);

					for (k = 0; k < vertex_total_cap_head - 2; k++, j++) {
						*(face++) = midvidx + 0;  /* z 1 */
						*(face++) = midvidx + 0;  /* z 1 */
						*(face++) = j + 0;        /* z 0 */
						*(face++) = j + 1;        /* z 0 */
						face_index++;
						FACE_ASSERT(face - 4, sf_vert_tot);
					}

					j = vertex_offset + (vertex_total * 3);

					/* 2 tris that join the original */
					*(face++) = midvidx + 0;  /* z 1 */
					*(face++) = midvidx + 0;  /* z 1 */
					*(face++) = midvidx + 1;  /* z 0 */
					*(face++) = j + 0;        /* z 0 */
					face_index++;
					FACE_ASSERT(face - 4, sf_vert_tot);

					*(face++) = midvidx + 0;                    /* z 1 */
					*(face++) = midvidx + 0;                    /* z 1 */
					*(face++) = j + vertex_total_cap_head - 2;  /* z 0 */
					*(face++) = midvidx + 2;                    /* z 0 */
					face_index++;
					FACE_ASSERT(face - 4, sf_vert_tot);


					/***************
					 * cap end 'b' */
					/* ... same as previous but v 2-3 flipped, and different initial offsets */

					j = vertex_offset + (vertex_total * 3) + (vertex_total_cap_head - 1);

					midvidx = vertex_offset + (vertex_total * 3) - 3;

					for (k = 0; k < vertex_total_cap_tail - 2; k++, j++) {
						*(face++) = midvidx;  /* z 1 */
						*(face++) = midvidx;  /* z 1 */
						*(face++) = j + 1;    /* z 0 */
						*(face++) = j + 0;    /* z 0 */
						face_index++;
						FACE_ASSERT(face - 4, sf_vert_tot);
					}

					j = vertex_offset + (vertex_total * 3) + (vertex_total_cap_head - 1);

					/* 2 tris that join the original */
					*(face++) = midvidx + 0;  /* z 1 */
					*(face++) = midvidx + 0;  /* z 1 */
					*(face++) = j + 0;        /* z 0 */
					*(face++) = midvidx + 1;  /* z 0 */
					face_index++;
					FACE_ASSERT(face - 4, sf_vert_tot);

					*(face++) = midvidx + 0;                    /* z 1 */
					*(face++) = midvidx + 0;                    /* z 1 */
					*(face++) = midvidx + 2;                    /* z 0 */
					*(face++) = j + vertex_total_cap_tail - 2;  /* z 0 */
					face_index++;
					FACE_ASSERT(face - 4, sf_vert_tot);

				}
			}

			MEM_freeN(open_spline_ranges);

//			fprintf(stderr, "%u %u (%u %u), %u\n", face_index, sf_tri_tot + tot_feather_quads, sf_tri_tot, tot_feather_quads, tot_boundary_used - tot_boundary_found);

#ifdef USE_SCANFILL_EDGE_WORKAROUND
			BLI_assert(face_index + (tot_boundary_used - tot_boundary_found) == sf_tri_tot + tot_feather_quads);
#else
			BLI_assert(face_index == sf_tri_tot + tot_feather_quads);
#endif
			{
				MaskRasterLayer *layer = &mr_handle->layers[masklay_index];

				if (BLI_rctf_isect(&default_bounds, &bounds, &bounds)) {
#ifdef USE_SCANFILL_EDGE_WORKAROUND
					layer->face_tot = (sf_tri_tot + tot_feather_quads) - (tot_boundary_used - tot_boundary_found);
#else
					layer->face_tot = (sf_tri_tot + tot_feather_quads);
#endif
					layer->face_coords = face_coords;
					layer->face_array  = face_array;
					layer->bounds = bounds;

					layer_bucket_init(layer, pixel_size);

					BLI_rctf_union(&mr_handle->bounds, &bounds);
				}
				else {
					MEM_freeN(face_coords);
					MEM_freeN(face_array);

					layer_bucket_init_dummy(layer);
				}

				/* copy as-is */
				layer->alpha = masklay->alpha;
				layer->blend = masklay->blend;
				layer->blend_flag = masklay->blend_flag;
				layer->falloff = masklay->falloff;
			}

			/* printf("tris %d, feather tris %d\n", sf_tri_tot, tot_feather_quads); */
		}

		/* add trianges */
		BLI_scanfill_end_arena(&sf_ctx, sf_arena);
	}

	BLI_memarena_free(sf_arena);
}


/* --------------------------------------------------------------------- */
/* functions that run inside the sampling thread (keep fast!)            */
/* --------------------------------------------------------------------- */

/* 2D ray test */
#if 0
static float maskrasterize_layer_z_depth_tri(const float pt[2],
                                             const float v1[3], const float v2[3], const float v3[3])
{
	float w[3];
	barycentric_weights_v2(v1, v2, v3, pt, w);
	return (v1[2] * w[0]) + (v2[2] * w[1]) + (v3[2] * w[2]);
}
#endif

#if 1
static float maskrasterize_layer_z_depth_quad(const float pt[2],
                                              const float v1[3], const float v2[3], const float v3[3], const float v4[3])
{
	float w[4];
	barycentric_weights_v2_quad(v1, v2, v3, v4, pt, w);
	//return (v1[2] * w[0]) + (v2[2] * w[1]) + (v3[2] * w[2]) + (v4[2] * w[3]);
	return w[2] + w[3];  /* we can make this assumption for small speedup */
}
#endif

static float maskrasterize_layer_isect(unsigned int *face, float (*cos)[3], const float dist_orig, const float xy[2])
{
	/* we always cast from same place only need xy */
	if (face[3] == TRI_VERT) {
		/* --- tri --- */

#if 0
		/* not essential but avoids unneeded extra lookups */
		if ((cos[0][2] < dist_orig) ||
		    (cos[1][2] < dist_orig) ||
		    (cos[2][2] < dist_orig))
		{
			if (isect_point_tri_v2_cw(xy, cos[face[0]], cos[face[1]], cos[face[2]])) {
				/* we know all tris are close for now */
				return maskrasterize_layer_z_depth_tri(xy, cos[face[0]], cos[face[1]], cos[face[2]]);
			}
		}
#else
		/* we know all tris are close for now */
		if (1) {
			if (isect_point_tri_v2_cw(xy, cos[face[0]], cos[face[1]], cos[face[2]])) {
				return 0.0f;
			}
		}
#endif
	}
	else {
		/* --- quad --- */

		/* not essential but avoids unneeded extra lookups */
		if ((cos[0][2] < dist_orig) ||
		    (cos[1][2] < dist_orig) ||
		    (cos[2][2] < dist_orig) ||
		    (cos[3][2] < dist_orig))
		{

			/* needs work */
#if 1
			/* quad check fails for bow-tie, so keep using 2 tri checks */
			//if (isect_point_quad_v2(xy, cos[face[0]], cos[face[1]], cos[face[2]], cos[face[3]]))
			if (isect_point_tri_v2(xy, cos[face[0]], cos[face[1]], cos[face[2]]) ||
			    isect_point_tri_v2(xy, cos[face[0]], cos[face[2]], cos[face[3]]))
			{
				return maskrasterize_layer_z_depth_quad(xy, cos[face[0]], cos[face[1]], cos[face[2]], cos[face[3]]);
			}
#elif 1
			/* don't use isect_point_tri_v2_cw because we could have bow-tie quads */

			if (isect_point_tri_v2(xy, cos[face[0]], cos[face[1]], cos[face[2]])) {
				return maskrasterize_layer_z_depth_tri(xy, cos[face[0]], cos[face[1]], cos[face[2]]);
			}
			else if (isect_point_tri_v2(xy, cos[face[0]], cos[face[2]], cos[face[3]])) {
				return maskrasterize_layer_z_depth_tri(xy, cos[face[0]], cos[face[2]], cos[face[3]]);
			}
#else
			/* cheat - we know first 2 verts are z0.0f and second 2 are z 1.0f */
			/* ... worth looking into */
#endif
		}
	}

	return 1.0f;
}

BLI_INLINE unsigned int layer_bucket_index_from_xy(MaskRasterLayer *layer, const float xy[2])
{
	BLI_assert(BLI_rctf_isect_pt_v(&layer->bounds, xy));

	return ( (unsigned int)((xy[0] - layer->bounds.xmin) * layer->buckets_xy_scalar[0])) +
	       (((unsigned int)((xy[1] - layer->bounds.ymin) * layer->buckets_xy_scalar[1])) * layer->buckets_x);
}

static float layer_bucket_depth_from_xy(MaskRasterLayer *layer, const float xy[2])
{
	unsigned int index = layer_bucket_index_from_xy(layer, xy);
	unsigned int *face_index = layer->buckets_face[index];

	if (face_index) {
		unsigned int (*face_array)[4] = layer->face_array;
		float        (*cos)[3]        = layer->face_coords;
		float best_dist = 1.0f;
		while (*face_index != TRI_TERMINATOR_ID) {
			const float test_dist = maskrasterize_layer_isect(face_array[*face_index], cos, best_dist, xy);
			if (test_dist < best_dist) {
				best_dist = test_dist;
				/* comparing with 0.0f is OK here because triangles are always zero depth */
				if (best_dist == 0.0f) {
					/* bail early, we're as close as possible */
					return 0.0f;
				}
			}
			face_index++;
		}
		return best_dist;
	}
	else {
		return 1.0f;
	}
}

float BKE_maskrasterize_handle_sample(MaskRasterHandle *mr_handle, const float xy[2])
{
	/* can't do this because some layers may invert */
	/* if (BLI_rctf_isect_pt_v(&mr_handle->bounds, xy)) */

	const unsigned int layers_tot = mr_handle->layers_tot;
	unsigned int i;
	MaskRasterLayer *layer = mr_handle->layers;

	/* return value */
	float value = 0.0f;

	for (i = 0; i < layers_tot; i++, layer++) {
		float value_layer;

		/* also used as signal for unused layer (when render is disabled) */
		if (layer->alpha != 0.0f && BLI_rctf_isect_pt_v(&layer->bounds, xy)) {
			value_layer = 1.0f - layer_bucket_depth_from_xy(layer, xy);

			switch (layer->falloff) {
				case PROP_SMOOTH:
					/* ease - gives less hard lines for dilate/erode feather */
					value_layer = (3.0f * value_layer * value_layer - 2.0f * value_layer * value_layer * value_layer);
					break;
				case PROP_SPHERE:
					value_layer = sqrtf(2.0f * value_layer - value_layer * value_layer);
					break;
				case PROP_ROOT:
					value_layer = sqrtf(value_layer);
					break;
				case PROP_SHARP:
					value_layer = value_layer * value_layer;
					break;
				case PROP_LIN:
				default:
					/* nothing */
					break;
			}

			if (layer->blend != MASK_BLEND_REPLACE) {
				value_layer *= layer->alpha;
			}
		}
		else {
			value_layer = 0.0f;
		}

		if (layer->blend_flag & MASK_BLENDFLAG_INVERT) {
			value_layer = 1.0f - value_layer;
		}

		switch (layer->blend) {
			case MASK_BLEND_MERGE_ADD:
				value += value_layer * (1.0f - value);
				break;
			case MASK_BLEND_MERGE_SUBTRACT:
				value -= value_layer * value;
				break;
			case MASK_BLEND_ADD:
				value += value_layer;
				break;
			case MASK_BLEND_SUBTRACT:
				value -= value_layer;
				break;
			case MASK_BLEND_LIGHTEN:
				value = max_ff(value, value_layer);
				break;
			case MASK_BLEND_DARKEN:
				value = min_ff(value, value_layer);
				break;
			case MASK_BLEND_MUL:
				value *= value_layer;
				break;
			case MASK_BLEND_REPLACE:
				value = (value * (1.0f - layer->alpha)) + (value_layer * layer->alpha);
				break;
			case MASK_BLEND_DIFFERENCE:
				value = fabsf(value - value_layer);
				break;
			default: /* same as add */
				BLI_assert(0);
				value += value_layer;
				break;
		}

		/* clamp after applying each layer so we don't get
		 * issues subtracting after accumulating over 1.0f */
		CLAMP(value, 0.0f, 1.0f);
	}

	return value;
}

/**
 * \brief Rasterize a buffer from a single mask
 *
 * We could get some speedup by inlining #BKE_maskrasterize_handle_sample
 * and calculating each layer then blending buffers, but this function is only
 * used by the sequencer - so better have the caller thread.
 *
 * Since #BKE_maskrasterize_handle_sample is used threaded elsewhere,
 * we can simply use openmp here for some speedup.
 */
void BKE_maskrasterize_buffer(MaskRasterHandle *mr_handle,
                              const unsigned int width, const unsigned int height,
                              float *buffer)
{
#ifdef _MSC_VER
	int y;  /* msvc requires signed for some reason */
#else
	unsigned int y;
#endif

#pragma omp parallel for private(y)
	for (y = 0; y < height; y++) {
		unsigned int i = y * width;
		unsigned int x;
		float xy[2];
		xy[1] = (float)y / (float)height;
		for (x = 0; x < width; x++, i++) {
			xy[0] = (float)x / (float)width;

			buffer[i] = BKE_maskrasterize_handle_sample(mr_handle, xy);
		}
	}
}
