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

/**
 * A single #MaskRasterHandle contains multile #MaskRasterLayer's,
 * each #MaskRasterLayer does its own lookup which contributes to
 * the final pixel with its own blending mode and the final pixel is blended between these.
 */

/* internal use only */
typedef struct MaskRasterLayer {
	/* geometry */
	unsigned int   tri_tot;
	unsigned int (*tri_array)[4];  /* access coords tri/quad */
	float        (*tri_coords)[3]; /* xy, z 0-1 (1.0 == filled) */


	/* 2d bounds (to quickly skip raytree lookup) */
	rctf bounds;


	/* buckets */
	unsigned int **buckets_tri;
	/* cache divide and subtract */
	float buckets_xy_scalar[2]; /* 1.0 / (buckets_width + FLT_EPSILON) */
	unsigned int buckets_x;
	unsigned int buckets_y;


	/* copied direct from #MaskLayer.--- */
	/* blending options */
	float  alpha;
	char   blend;
	char   blend_flag;

} MaskRasterLayer;

static void layer_bucket_init(MaskRasterLayer *layer);

/**
 * opaque local struct for mask pixel lookup, each MaskLayer needs one of these
 */
struct MaskRasterHandle {
	MaskRasterLayer *layers;
	unsigned int     layers_tot;

	/* 2d bounds (to quickly skip raytree lookup) */
	rctf bounds;
};

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
	MaskRasterLayer *raslayers = mr_handle->layers;

	/* raycast vars */
	for (i = 0; i < layers_tot; i++, raslayers++) {

		if (raslayers->tri_array) {
			MEM_freeN(raslayers->tri_array);
		}

		if (raslayers->tri_coords) {
			MEM_freeN(raslayers->tri_coords);
		}

		if (raslayers->buckets_tri) {
			const unsigned int   bucket_tot = raslayers->buckets_x * raslayers->buckets_y;
			unsigned int bucket_index;
			for (bucket_index = 0; bucket_index < bucket_tot; bucket_index++) {
				unsigned int *tri_index = raslayers->buckets_tri[bucket_index];
				if (tri_index) {
					MEM_freeN(tri_index);
				}
			}

			MEM_freeN(raslayers->buckets_tri);
		}
	}

	MEM_freeN(mr_handle->layers);
	MEM_freeN(mr_handle);
}

#define RESOL 32

#define PRINT_MASK_DEBUG printf

#define SF_EDGE_IS_BOUNDARY 0xff

#define SF_KEYINDEX_TEMP_ID ((unsigned int) -1)
#define TRI_TERMINATOR_ID   ((unsigned int) -1)


void maskrasterize_spline_differentiate_point_inset(float (*diff_feather_points)[2], float (*diff_points)[2],
                                                    const int tot_diff_point, const float ofs, const int do_test)
{
	int k_prev = tot_diff_point - 2;
	int k_curr = tot_diff_point - 1;
	int k_next = 0;

	int k;

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

	/* TODO, speedup by only doing one normalize per iter */


	for (k = 0; k < tot_diff_point; k++) {

		co_prev = diff_points[k_prev];
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

		k_prev = k_curr;
		k_curr = k_next;
		k_next++;
	}
}

#define TRI_VERT ((unsigned int) -1)

void BLI_maskrasterize_handle_init(MaskRasterHandle *mr_handle, struct Mask *mask,
                                   const int width, const int height,
                                   const short do_aspect_correct, const short do_mask_aa,
                                   const short do_feather)
{
	/* TODO: real size */
	const int resol = RESOL;
	const float aa_filter_size = 1.0f / MIN2(width, height);

	const float zvec[3] = {0.0f, 0.0f, 1.0f};
	MaskLayer *masklay;
	int masklay_index;

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

			diff_points = BKE_mask_spline_differentiate_with_resolution_ex(spline, resol, &tot_diff_point);

			/* dont ch*/
			if (do_feather) {
				diff_feather_points = BKE_mask_spline_feather_differentiated_points_with_resolution_ex(spline, resol, &tot_diff_feather_points);
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
						diff_feather_points = MEM_mallocN(sizeof(*diff_feather_points) * tot_diff_feather_points, __func__);
						/* add single pixel feather */
						maskrasterize_spline_differentiate_point_inset(diff_feather_points, diff_points,
						                                               tot_diff_point, aa_filter_size, FALSE);
					}
					else {
						/* ensure single pixel feather, on any zero feather areas */
						maskrasterize_spline_differentiate_point_inset(diff_feather_points, diff_points,
						                                               tot_diff_point, aa_filter_size, TRUE);
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
			unsigned int (*tri_array)[4], *tri;  /* access coords */
			float        (*tri_coords)[3], *cos; /* xy, z 0-1 (1.0 == filled) */
			int sf_tri_tot;
			rctf bounds;
			int tri_index;

			float bvhcos[4][3];

			/* now we have all the splines */
			tri_coords = MEM_mallocN((sizeof(float) * 3) * sf_vert_tot, "maskrast_tri_coords");

			/* init bounds */
			BLI_rctf_init_minmax(&bounds);

			/* coords */
			cos = (float *)tri_coords;
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

			tri_array = MEM_mallocN(sizeof(*tri_array) * (sf_tri_tot + tot_feather_quads), "maskrast_tri_index");

			/* tri's */
			tri = (unsigned int *)tri_array;
			for (sf_tri = sf_ctx.fillfacebase.first, tri_index = 0; sf_tri; sf_tri = sf_tri->next, tri_index++) {
				*(tri++) = sf_tri->v1->tmp.u;
				*(tri++) = sf_tri->v2->tmp.u;
				*(tri++) = sf_tri->v3->tmp.u;
				*(tri++) = TRI_VERT;
			}

			/* start of feather faces... if we have this set,
			 * 'tri_index' is kept from loop above */

			BLI_assert(tri_index == sf_tri_tot);

			if (tot_feather_quads) {
				ScanFillEdge *sf_edge;

				for (sf_edge = sf_ctx.filledgebase.first; sf_edge; sf_edge = sf_edge->next) {
					if (sf_edge->tmp.c == SF_EDGE_IS_BOUNDARY) {
						*(tri++) = sf_edge->v1->tmp.u;
						*(tri++) = sf_edge->v2->tmp.u;
						*(tri++) = sf_edge->v2->keyindex;
						*(tri++) = sf_edge->v1->keyindex;

						copy_v3_v3(bvhcos[0], tri_coords[*(tri - 4)]);
						copy_v3_v3(bvhcos[1], tri_coords[*(tri - 3)]);
						copy_v3_v3(bvhcos[2], tri_coords[*(tri - 2)]);
						copy_v3_v3(bvhcos[3], tri_coords[*(tri - 1)]);

						tri_index++;
					}
				}
			}

			fprintf(stderr, "%d %d\n", tri_index, sf_tri_tot + tot_feather_quads);

			BLI_assert(tri_index == sf_tri_tot + tot_feather_quads);

			{
				MaskRasterLayer *raslayer = &mr_handle->layers[masklay_index];

				raslayer->tri_tot = sf_tri_tot + tot_feather_quads;
				raslayer->tri_coords = tri_coords;
				raslayer->tri_array  = tri_array;
				raslayer->bounds  = bounds;

				/* copy as-is */
				raslayer->alpha = masklay->alpha;
				raslayer->blend = masklay->blend;
				raslayer->blend_flag = masklay->blend_flag;

				layer_bucket_init(raslayer);

				BLI_union_rctf(&mr_handle->bounds, &bounds);
			}

			PRINT_MASK_DEBUG("tris %d, feather tris %d\n", sf_tri_tot, tot_feather_quads);
		}

		/* add trianges */
		BLI_scanfill_end(&sf_ctx);
	}
}

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

static float maskrasterize_layer_isect(unsigned int *tri, float (*cos)[3], const float dist_orig, const float xy[2])
{
	/* we always cast from same place only need xy */
	if (tri[3] == TRI_VERT) {
		/* --- tri --- */

		/* not essential but avoids unneeded extra lookups */
		if ((cos[0][2] < dist_orig) ||
		    (cos[1][2] < dist_orig) ||
		    (cos[2][2] < dist_orig))
		{
			if (isect_point_tri_v2(xy, cos[tri[0]], cos[tri[1]], cos[tri[2]])) {
				/* we know all tris are close for now */
#if 0
				return maskrasterize_layer_z_depth_tri(xy, cos[tri[0]], cos[tri[1]], cos[tri[2]]);
#else
				return 0.0f;
#endif
			}
		}
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
			if (isect_point_quad_v2(xy, cos[tri[0]], cos[tri[1]], cos[tri[2]], cos[tri[3]])) {
				return maskrasterize_layer_z_depth_quad(xy, cos[tri[0]], cos[tri[1]], cos[tri[2]], cos[tri[3]]);
			}
#elif 1
			if (isect_point_tri_v2(xy, cos[tri[0]], cos[tri[1]], cos[tri[2]])) {
				return maskrasterize_layer_z_depth_tri(xy, cos[tri[0]], cos[tri[1]], cos[tri[2]]);
			}
			else if (isect_point_tri_v2(xy, cos[tri[0]], cos[tri[2]], cos[tri[3]])) {
				return maskrasterize_layer_z_depth_tri(xy, cos[tri[0]], cos[tri[2]], cos[tri[3]]);
			}
#else
			/* cheat - we know first 2 verts are z0.0f and second 2 are z 1.0f */
			/* ... worth looking into */
#endif
		}
	}

	return 1.0f;
}

static void layer_bucket_init(MaskRasterLayer *layer)
{
	MemArena *arena = BLI_memarena_new(1 << 16, __func__);

	/* TODO - calculate best bucket size */
	layer->buckets_x = 128;
	layer->buckets_y = 128;

	layer->buckets_xy_scalar[0] = (1.0f / ((layer->bounds.xmax - layer->bounds.xmin) + FLT_EPSILON)) * layer->buckets_x;
	layer->buckets_xy_scalar[1] = (1.0f / ((layer->bounds.ymax - layer->bounds.ymin) + FLT_EPSILON)) * layer->buckets_y;

	{
		unsigned int *tri = &layer->tri_array[0][0];
		float (*cos)[3] = layer->tri_coords;

		const unsigned int   bucket_tot = layer->buckets_x * layer->buckets_y;
		LinkNode     **bucketstore     = MEM_callocN(bucket_tot * sizeof(LinkNode *),  __func__);
		unsigned int  *bucketstore_tot = MEM_callocN(bucket_tot * sizeof(unsigned int), __func__);

		unsigned int tri_index;

		for (tri_index = 0; tri_index < layer->tri_tot; tri_index++, tri += 4) {
			float xmin;
			float xmax;
			float ymin;
			float ymax;

			if (tri[3] == TRI_VERT) {
				const float *v1 = cos[tri[0]];
				const float *v2 = cos[tri[1]];
				const float *v3 = cos[tri[2]];

				xmin = fminf(v1[0], fminf(v2[0], v3[0]));
				xmax = fmaxf(v1[0], fmaxf(v2[0], v3[0]));
				ymin = fminf(v1[1], fminf(v2[1], v3[1]));
				ymax = fmaxf(v1[1], fmaxf(v2[1], v3[1]));
			}
			else {
				const float *v1 = cos[tri[0]];
				const float *v2 = cos[tri[1]];
				const float *v3 = cos[tri[2]];
				const float *v4 = cos[tri[3]];

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

				unsigned int xi, yi;

				for (xi = xi_min; xi <= xi_max; xi++) {
					for (yi = yi_min; yi <= yi_max; yi++) {
						unsigned int bucket_index = (layer->buckets_x * yi) + xi;

						BLI_assert(xi < layer->buckets_x);
						BLI_assert(yi < layer->buckets_y);
						BLI_assert(bucket_index < bucket_tot);

						BLI_linklist_prepend_arena(&bucketstore[bucket_index],
                                                   SET_UINT_IN_POINTER(tri_index),
                                                   arena);

						bucketstore_tot[bucket_index]++;
					}
				}
			}
		}

		if (1) {
            /* now convert linknodes into arrays for faster per pixel access */
			unsigned int  **buckets_tri = MEM_mallocN(bucket_tot * sizeof(unsigned int **), __func__);
			unsigned int bucket_index;

			for (bucket_index = 0; bucket_index < bucket_tot; bucket_index++) {
				if (bucketstore_tot[bucket_index]) {
					unsigned int  *bucket = MEM_mallocN((bucketstore_tot[bucket_index] + 1) * sizeof(unsigned int), __func__);
					LinkNode *bucket_node;

					buckets_tri[bucket_index] = bucket;

					for (bucket_node = bucketstore[bucket_index]; bucket_node; bucket_node = bucket_node->next) {
						*bucket = GET_UINT_FROM_POINTER(bucket_node->link);
						bucket++;
					}
					*bucket = TRI_TERMINATOR_ID;
				}
				else {
					buckets_tri[bucket_index] = NULL;
				}
			}

			layer->buckets_tri = buckets_tri;
		}

		MEM_freeN(bucketstore);
		MEM_freeN(bucketstore_tot);
	}

	BLI_memarena_free(arena);
}

static unsigned int layer_bucket_index_from_xy(MaskRasterLayer *layer, const float xy[2])
{
	BLI_assert(BLI_in_rctf_v(&layer->bounds, xy));

	return ( (unsigned int)((xy[0] - layer->bounds.xmin) * layer->buckets_xy_scalar[0])) +
	       (((unsigned int)((xy[1] - layer->bounds.ymin) * layer->buckets_xy_scalar[1])) * layer->buckets_x);
}

static float layer_bucket_depth_from_xy(MaskRasterLayer *layer, const float xy[2])
{
	unsigned int index = layer_bucket_index_from_xy(layer, xy);
	unsigned int *tri_index = layer->buckets_tri[index];

	if (tri_index) {
		float (*cos)[3] = layer->tri_coords;
		float best_dist = 1.0f;
		float test_dist;
		while (*tri_index != TRI_TERMINATOR_ID) {
			unsigned int *tri = layer->tri_array[*tri_index];
			if ((test_dist = maskrasterize_layer_isect(tri, cos, best_dist, xy)) < best_dist) {
				best_dist = test_dist;
				/* bail early */
				if (best_dist <= 0.0f) {
					return 0.0f;
				}
			}
			tri_index++;
		}
		return best_dist;
	}
	else {
		return 1.0f;
	}
}


float BLI_maskrasterize_handle_sample(MaskRasterHandle *mr_handle, const float xy[2])
{
	/* TODO - AA jitter */

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
