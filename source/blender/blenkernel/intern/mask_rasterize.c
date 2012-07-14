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
 */

#include "MEM_guardedalloc.h"

#include "DNA_vec_types.h"
#include "DNA_mask_types.h"

#include "BLI_utildefines.h"
#include "BLI_scanfill.h"
#include "BLI_memarena.h"

#include "BLI_math.h"
#include "BLI_rect.h"
#include "BLI_listbase.h"
#include "BLI_linklist.h"

#include "BKE_mask.h"

#ifndef USE_RASKTER

#define SPLINE_RESOL 32
#define BUCKET_PIXELS_PER_CELL 8

#define SF_EDGE_IS_BOUNDARY 0xff
#define SF_KEYINDEX_TEMP_ID ((unsigned int) -1)

#define TRI_TERMINATOR_ID   ((unsigned int) -1)
#define TRI_VERT            ((unsigned int) -1)


/* --------------------------------------------------------------------- */
/* local structs for mask rasterizeing                                   */
/* --------------------------------------------------------------------- */

/**
 * A single #MaskRasterHandle contains multile #MaskRasterLayer's,
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

} MaskRasterLayer;

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

MaskRasterHandle *BLI_maskrasterize_handle_new(void)
{
	MaskRasterHandle *mr_handle;

	mr_handle = MEM_callocN(sizeof(MaskRasterHandle), STRINGIFY(MaskRasterHandle));

	return mr_handle;
}

void BLI_maskrasterize_handle_free(MaskRasterHandle *mr_handle)
{
	const unsigned int layers_tot = mr_handle->layers_tot;
	unsigned int i;
	MaskRasterLayer *layer = mr_handle->layers;

	/* raycast vars */
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


void maskrasterize_spline_differentiate_point_outset(float (*diff_feather_points)[2], float (*diff_points)[2],
                                                     const unsigned int tot_diff_point, const float ofs,
                                                     const short do_test)
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

static int layer_bucket_isect_test(MaskRasterLayer *layer, unsigned int face_index,
                                   const unsigned int bucket_x, const unsigned int bucket_y,
                                   const float bucket_x_size, const float bucket_y_size)
{
	const float xmin = layer->bounds.xmin + (bucket_x_size * bucket_x);
	const float ymin = layer->bounds.ymin + (bucket_y_size * bucket_y);
	const float xmax = xmin + bucket_x_size;
	const float ymax = ymin + bucket_y_size;

	const float bucket_quad[4][2] = {{xmin, ymin},
	                                 {xmin, ymax},
	                                 {xmax, ymax},
	                                 {xmax, ymin}};

	unsigned int *face = layer->face_array[face_index];
	float (*cos)[3] = layer->face_coords;

//	float dummy_lambda;

	if (face[3] == TRI_VERT) {
		const float *v1 = cos[face[0]];
		const float *v2 = cos[face[1]];
		const float *v3 = cos[face[2]];

		/* bucket corner in tri? */
		if (isect_point_tri_v2(bucket_quad[0], v1, v2, v3) ||
		    isect_point_tri_v2(bucket_quad[1], v1, v2, v3) ||
		    isect_point_tri_v2(bucket_quad[2], v1, v2, v3) ||
		    isect_point_tri_v2(bucket_quad[3], v1, v2, v3))
		{
			return TRUE;
		}

		/* line intersect */
#if 1
		if (isect_line_line_v2(bucket_quad[0], bucket_quad[1], v1, v2) ||
		    isect_line_line_v2(bucket_quad[0], bucket_quad[1], v2, v3) ||
		    isect_line_line_v2(bucket_quad[0], bucket_quad[1], v3, v1) ||

		    isect_line_line_v2(bucket_quad[1], bucket_quad[2], v1, v2) ||
		    isect_line_line_v2(bucket_quad[1], bucket_quad[2], v2, v3) ||
		    isect_line_line_v2(bucket_quad[1], bucket_quad[2], v3, v1) ||

		    isect_line_line_v2(bucket_quad[2], bucket_quad[3], v1, v2) ||
		    isect_line_line_v2(bucket_quad[2], bucket_quad[3], v2, v3) ||
		    isect_line_line_v2(bucket_quad[2], bucket_quad[3], v3, v1) ||

		    isect_line_line_v2(bucket_quad[3], bucket_quad[0], v1, v2) ||
		    isect_line_line_v2(bucket_quad[3], bucket_quad[0], v2, v3) ||
		    isect_line_line_v2(bucket_quad[3], bucket_quad[0], v3, v1)

		    )
		{
			return TRUE;
		}
#else
		/* line intersect */
		if (isect_line_tri_v3(bucket_quad[0], bucket_quad[1], v1, v2, v3, &dummy_lambda, NULL) ||
		    isect_line_tri_v3(bucket_quad[1], bucket_quad[2], v1, v2, v3, &dummy_lambda, NULL) ||
		    isect_line_tri_v3(bucket_quad[2], bucket_quad[3], v1, v2, v3, &dummy_lambda, NULL) ||
		    isect_line_tri_v3(bucket_quad[3], bucket_quad[0], v1, v2, v3, &dummy_lambda, NULL))
		{
			return TRUE;
		}

#endif
		return FALSE;
	}
	else {
		const float *v1 = cos[face[0]];
		const float *v2 = cos[face[1]];
		const float *v3 = cos[face[2]];
		const float *v4 = cos[face[3]];

		/* bucket corner in tri? */
		if (isect_point_tri_v2(bucket_quad[0], v1, v2, v3) ||
		    isect_point_tri_v2(bucket_quad[1], v1, v2, v3) ||
		    isect_point_tri_v2(bucket_quad[2], v1, v2, v3) ||
		    isect_point_tri_v2(bucket_quad[3], v1, v2, v3))
		{
			return TRUE;
		}
		else if (isect_point_tri_v2(bucket_quad[0], v1, v3, v4) ||
		         isect_point_tri_v2(bucket_quad[1], v1, v3, v4) ||
		         isect_point_tri_v2(bucket_quad[2], v1, v3, v4) ||
		         isect_point_tri_v2(bucket_quad[3], v1, v3, v4))
		{
			return TRUE;
		}

		/* line intersect */
#if 1
		if (isect_line_line_v2(bucket_quad[0], bucket_quad[1], v1, v2) ||
		    isect_line_line_v2(bucket_quad[0], bucket_quad[1], v2, v3) ||
		    isect_line_line_v2(bucket_quad[0], bucket_quad[1], v3, v4) ||
		    isect_line_line_v2(bucket_quad[0], bucket_quad[1], v4, v1) ||

		    isect_line_line_v2(bucket_quad[1], bucket_quad[2], v1, v2) ||
		    isect_line_line_v2(bucket_quad[1], bucket_quad[2], v2, v3) ||
		    isect_line_line_v2(bucket_quad[1], bucket_quad[2], v3, v4) ||
		    isect_line_line_v2(bucket_quad[1], bucket_quad[2], v4, v1) ||

		    isect_line_line_v2(bucket_quad[2], bucket_quad[3], v1, v2) ||
		    isect_line_line_v2(bucket_quad[2], bucket_quad[3], v2, v3) ||
		    isect_line_line_v2(bucket_quad[2], bucket_quad[3], v3, v4) ||
		    isect_line_line_v2(bucket_quad[2], bucket_quad[3], v4, v1) ||

		    isect_line_line_v2(bucket_quad[3], bucket_quad[0], v1, v2) ||
		    isect_line_line_v2(bucket_quad[3], bucket_quad[0], v2, v3) ||
		    isect_line_line_v2(bucket_quad[3], bucket_quad[0], v3, v4) ||
		    isect_line_line_v2(bucket_quad[3], bucket_quad[0], v4, v1)

		    )
		{
			return TRUE;
		}
#else
		if (isect_line_tri_v3(bucket_quad[0], bucket_quad[1], v1, v2, v3, &dummy_lambda, NULL) ||
		    isect_line_tri_v3(bucket_quad[1], bucket_quad[2], v1, v2, v3, &dummy_lambda, NULL) ||
		    isect_line_tri_v3(bucket_quad[2], bucket_quad[3], v1, v2, v3, &dummy_lambda, NULL) ||
		    isect_line_tri_v3(bucket_quad[3], bucket_quad[0], v1, v2, v3, &dummy_lambda, NULL))
		{
			return TRUE;
		}
		else if (isect_line_tri_v3(bucket_quad[0], bucket_quad[1], v1, v3, v4, &dummy_lambda, NULL) ||
		         isect_line_tri_v3(bucket_quad[1], bucket_quad[2], v1, v3, v4, &dummy_lambda, NULL) ||
		         isect_line_tri_v3(bucket_quad[2], bucket_quad[3], v1, v3, v4, &dummy_lambda, NULL) ||
		         isect_line_tri_v3(bucket_quad[3], bucket_quad[0], v1, v3, v4, &dummy_lambda, NULL))
		{
			return TRUE;
		}
#endif

		return FALSE;
	}
}

static void layer_bucket_init(MaskRasterLayer *layer, const float pixel_size)
{
	MemArena *arena = BLI_memarena_new(1 << 16, __func__);

	const float bucket_dim_x = layer->bounds.xmax - layer->bounds.xmin;
	const float bucket_dim_y = layer->bounds.ymax - layer->bounds.ymin;

	layer->buckets_x = (bucket_dim_x / pixel_size) / (float)BUCKET_PIXELS_PER_CELL;
	layer->buckets_y = (bucket_dim_y / pixel_size) / (float)BUCKET_PIXELS_PER_CELL;

//		printf("bucket size %ux%u\n", layer->buckets_x, layer->buckets_y);

	CLAMP(layer->buckets_x, 8, 512);
	CLAMP(layer->buckets_y, 8, 512);

	layer->buckets_xy_scalar[0] = (1.0f / (bucket_dim_x + FLT_EPSILON)) * layer->buckets_x;
	layer->buckets_xy_scalar[1] = (1.0f / (bucket_dim_y + FLT_EPSILON)) * layer->buckets_y;

	{
		/* width and height of each bucket */
		const float bucket_size_x = bucket_dim_x / layer->buckets_x;
		const float bucket_size_y = bucket_dim_y / layer->buckets_y;

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

				xmin = fminf(v1[0], fminf(v2[0], v3[0]));
				xmax = fmaxf(v1[0], fmaxf(v2[0], v3[0]));
				ymin = fminf(v1[1], fminf(v2[1], v3[1]));
				ymax = fmaxf(v1[1], fmaxf(v2[1], v3[1]));
			}
			else {
				const float *v1 = cos[face[0]];
				const float *v2 = cos[face[1]];
				const float *v3 = cos[face[2]];
				const float *v4 = cos[face[3]];

				xmin = fminf(v1[0], fminf(v2[0], fminf(v3[0], v4[0])));
				xmax = fmaxf(v1[0], fmaxf(v2[0], fmaxf(v3[0], v4[0])));
				ymin = fminf(v1[1], fminf(v2[1], fminf(v3[1], v4[1])));
				ymax = fmaxf(v1[1], fmaxf(v2[1], fmaxf(v3[1], v4[1])));
			}


			/* not essential but may as will skip any faces outside the view */
			if (!((xmax < 0.0f) || (ymax < 0.0f) || (xmin > 1.0f) || (ymin > 1.0f))) {
				const unsigned int xi_min = (unsigned int) ((xmin - layer->bounds.xmin) * layer->buckets_xy_scalar[0]);
				const unsigned int xi_max = (unsigned int) ((xmax - layer->bounds.xmin) * layer->buckets_xy_scalar[0]);
				const unsigned int yi_min = (unsigned int) ((ymin - layer->bounds.ymin) * layer->buckets_xy_scalar[1]);
				const unsigned int yi_max = (unsigned int) ((ymax - layer->bounds.ymin) * layer->buckets_xy_scalar[1]);
				void *face_index_void = SET_UINT_IN_POINTER(face_index);

				unsigned int xi, yi;

				for (yi = yi_min; yi <= yi_max; yi++) {
					unsigned int bucket_index = (layer->buckets_x * yi) + xi_min;
					for (xi = xi_min; xi <= xi_max; xi++, bucket_index++) {
						// unsigned int bucket_index = (layer->buckets_x * yi) + xi; /* correct but do in outer loop */

						BLI_assert(xi < layer->buckets_x);
						BLI_assert(yi < layer->buckets_y);
						BLI_assert(bucket_index < bucket_tot);

						/* check if the bucket intersects with the face */
						/* note: there is a tradeoff here since checking box/tri intersections isn't
						 * as optimal as it could be, but checking pixels against faces they will never intersect
						 * with is likely the greater slowdown here - so check if the cell intersects the face */
						if (layer_bucket_isect_test(layer, face_index, xi, yi, bucket_size_x, bucket_size_y)) {
							BLI_linklist_prepend_arena(&bucketstore[bucket_index], face_index_void, arena);
							bucketstore_tot[bucket_index]++;
						}
					}
				}
			}
		}

		if (1) {
			/* now convert linknodes into arrays for faster per pixel access */
			unsigned int  **buckets_face = MEM_mallocN(bucket_tot * sizeof(unsigned int **), __func__);
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

void BLI_maskrasterize_handle_init(MaskRasterHandle *mr_handle, struct Mask *mask,
                                   const int width, const int height,
                                   const short do_aspect_correct, const short do_mask_aa,
                                   const short do_feather)
{
	const int resol = SPLINE_RESOL;  /* TODO: real size */
	const float pixel_size = 1.0f / MIN2(width, height);

	const float zvec[3] = {0.0f, 0.0f, 1.0f};
	MaskLayer *masklay;
	unsigned int masklay_index;

	mr_handle->layers_tot = BLI_countlist(&mask->masklayers);
	mr_handle->layers = MEM_mallocN(sizeof(MaskRasterLayer) * mr_handle->layers_tot, STRINGIFY(MaskRasterLayer));
	BLI_rctf_init_minmax(&mr_handle->bounds);

	for (masklay = mask->masklayers.first, masklay_index = 0; masklay; masklay = masklay->next, masklay_index++) {

		MaskSpline *spline;

		/* scanfill */
		ScanFillContext sf_ctx;
		ScanFillVert *sf_vert = NULL;
		ScanFillVert *sf_vert_next = NULL;
		ScanFillFace *sf_tri;

		unsigned int sf_vert_tot = 0;
		unsigned int tot_feather_quads = 0;

		if (masklay->restrictflag & MASK_RESTRICT_RENDER) {
			continue;
		}

		BLI_scanfill_begin(&sf_ctx);

		for (spline = masklay->splines.first; spline; spline = spline->next) {

			float (*diff_points)[2];
			int tot_diff_point;

			float (*diff_feather_points)[2];
			int tot_diff_feather_points;

			diff_points = BKE_mask_spline_differentiate_with_resolution_ex(
			                  spline, resol, &tot_diff_point);

			/* dont ch*/
			if (do_feather) {
				diff_feather_points = BKE_mask_spline_feather_differentiated_points_with_resolution_ex(
				                          spline, resol, &tot_diff_feather_points);
			}
			else {
				tot_diff_feather_points = 0;
				diff_feather_points = NULL;
			}

			if (tot_diff_point > 3) {
				ScanFillVert *sf_vert_prev;
				int j;

				float co[3];
				co[2] = 0.0f;

				if (do_aspect_correct) {
					if (width != height) {
						float *fp;
						float *ffp;
						int i;
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
						diff_feather_points = MEM_mallocN(sizeof(*diff_feather_points) * tot_diff_feather_points,
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
					sf_edge->tmp.c = SF_EDGE_IS_BOUNDARY;

					sf_vert_prev = sf_vert;
					sf_vert = sf_vert->next;
				}

				if (diff_feather_points) {
					float co_feather[3];
					co_feather[2] = 1.0f;

					BLI_assert(tot_diff_feather_points == tot_diff_point);

					/* note: only added for convenience, we dont infact use these to scanfill,
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

					if (diff_feather_points) {
						MEM_freeN(diff_feather_points);
					}

					tot_feather_quads += tot_diff_point;
				}
			}

			if (diff_points) {
				MEM_freeN(diff_points);
			}
		}

		if (sf_ctx.fillvertbase.first) {
			unsigned int (*face_array)[4], *face;  /* access coords */
			float        (*face_coords)[3], *cos; /* xy, z 0-1 (1.0 == filled) */
			int sf_tri_tot;
			rctf bounds;
			int face_index;

			/* now we have all the splines */
			face_coords = MEM_mallocN((sizeof(float) * 3) * sf_vert_tot, "maskrast_face_coords");

			/* init bounds */
			BLI_rctf_init_minmax(&bounds);

			/* coords */
			cos = (float *)face_coords;
			for (sf_vert = sf_ctx.fillvertbase.first; sf_vert; sf_vert = sf_vert_next) {
				sf_vert_next = sf_vert->next;
				copy_v3_v3(cos, sf_vert->co);

				/* remove so as not to interfear with fill (called after) */
				if (sf_vert->keyindex == SF_KEYINDEX_TEMP_ID) {
					BLI_remlink(&sf_ctx.fillvertbase, sf_vert);
				}

				/* bounds */
				BLI_rctf_do_minmax_v(&bounds, cos);

				cos += 3;
			}

			/* main scanfill */
			sf_tri_tot = BLI_scanfill_calc_ex(&sf_ctx, FALSE, zvec);

			face_array = MEM_mallocN(sizeof(*face_array) * (sf_tri_tot + tot_feather_quads), "maskrast_face_index");

			/* tri's */
			face = (unsigned int *)face_array;
			for (sf_tri = sf_ctx.fillfacebase.first, face_index = 0; sf_tri; sf_tri = sf_tri->next, face_index++) {
				*(face++) = sf_tri->v1->tmp.u;
				*(face++) = sf_tri->v2->tmp.u;
				*(face++) = sf_tri->v3->tmp.u;
				*(face++) = TRI_VERT;
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
					}
				}
			}

			// fprintf(stderr, "%d %d\n", face_index, sf_face_tot + tot_feather_quads);

			BLI_assert(face_index == sf_tri_tot + tot_feather_quads);

			{
				MaskRasterLayer *layer = &mr_handle->layers[masklay_index];

				layer->face_tot = sf_tri_tot + tot_feather_quads;
				layer->face_coords = face_coords;
				layer->face_array  = face_array;
				layer->bounds  = bounds;

				/* copy as-is */
				layer->alpha = masklay->alpha;
				layer->blend = masklay->blend;
				layer->blend_flag = masklay->blend_flag;

				layer_bucket_init(layer, pixel_size);

				BLI_union_rctf(&mr_handle->bounds, &bounds);
			}

			/* printf("tris %d, feather tris %d\n", sf_tri_tot, tot_feather_quads); */
		}

		/* add trianges */
		BLI_scanfill_end(&sf_ctx);
	}
}


/* --------------------------------------------------------------------- */
/* functions that run inside the sampling thread (keep fast!)            */
/* --------------------------------------------------------------------- */

/* 2D ray test */
static float maskrasterize_layer_z_depth_tri(const float pt[2],
                                             const float v1[3], const float v2[3], const float v3[3])
{
	float w[3];
	barycentric_weights_v2(v1, v2, v3, pt, w);
	return (v1[2] * w[0]) + (v2[2] * w[1]) + (v3[2] * w[2]);
}

#if 0
static float maskrasterize_layer_z_depth_quad(const float pt[2],
                                              const float v1[3], const float v2[3], const float v3[3], const float v4[3])
{
	float w[4];
	barycentric_weights_v2_quad(v1, v2, v3, v4, pt, w);
	return (v1[2] * w[0]) + (v2[2] * w[1]) + (v3[2] * w[2]) + (v4[2] * w[3]);
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
			if (isect_point_tri_v2(xy, cos[face[0]], cos[face[1]], cos[face[2]])) {
				/* we know all tris are close for now */
				return maskrasterize_layer_z_depth_tri(xy, cos[face[0]], cos[face[1]], cos[face[2]]);
			}
		}
#else
		/* we know all tris are close for now */
		if (1) {
			if (isect_point_tri_v2(xy, cos[face[0]], cos[face[1]], cos[face[2]])) {
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
#if 0
			if (isect_point_quad_v2(xy, cos[face[0]], cos[face[1]], cos[face[2]], cos[face[3]])) {
				return maskrasterize_layer_z_depth_quad(xy, cos[face[0]], cos[face[1]], cos[face[2]], cos[face[3]]);
			}
#elif 1
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
	BLI_assert(BLI_in_rctf_v(&layer->bounds, xy));

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

float BLI_maskrasterize_handle_sample(MaskRasterHandle *mr_handle, const float xy[2])
{
	if (BLI_in_rctf_v(&mr_handle->bounds, xy)) {
		const unsigned int layers_tot = mr_handle->layers_tot;
		unsigned int i;
		MaskRasterLayer *layer = mr_handle->layers;

		/* raycast vars*/

		/* return */
		float value = 0.0f;

		for (i = 0; i < layers_tot; i++, layer++) {
			if (BLI_in_rctf_v(&layer->bounds, xy)) {
				/* --- hit (start) --- */
				const float dist = 1.0f - layer_bucket_depth_from_xy(layer, xy);
				const float dist_ease = (3.0f * dist * dist - 2.0f * dist * dist * dist);

				float v;
				/* apply alpha */
				v = dist_ease * layer->alpha;

				if (layer->blend_flag & MASK_BLENDFLAG_INVERT) {
					v = 1.0f - v;
				}

				switch (layer->blend) {
					case MASK_BLEND_SUBTRACT:
					{
						value -= v;
						break;
					}
					case MASK_BLEND_ADD:
					default:
					{
						value += v;
						break;
					}
				}
				/* --- hit (end) --- */

			}
		}

		return CLAMPIS(value, 0.0f, 1.0f);
	}
	else {
		return 0.0f;
	}
}

#endif /* USE_RASKTER */
