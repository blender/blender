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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: some of this file.
 *
 * Contributor(s): Jens Ole Wund (bjornmose), Campbell Barton (ideasman42)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/sculpt_paint/paint_image_proj.c
 *  \ingroup edsculpt
 *  \brief Functions to paint images in 2D and 3D.
 */

#include <float.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#ifdef WIN32
#  include "BLI_winstuff.h"
#endif

#include "BLI_blenlib.h"
#include "BLI_linklist.h"
#include "BLI_math.h"
#include "BLI_math_color_blend.h"
#include "BLI_memarena.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "PIL_time.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"

#include "BKE_camera.h"
#include "BKE_context.h"
#include "BKE_DerivedMesh.h"
#include "BKE_idprop.h"
#include "BKE_brush.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_mesh_mapping.h"
#include "BKE_paint.h"
#include "BKE_report.h"
#include "BKE_scene.h"

#include "UI_view2d.h"

#include "ED_paint.h"
#include "ED_screen.h"
#include "ED_uvedit.h"
#include "ED_view3d.h"

#include "GPU_extensions.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "GPU_draw.h"

#include "IMB_colormanagement.h"

#include "paint_intern.h"

/* Defines and Structs */
/* FTOCHAR as inline function */
BLI_INLINE unsigned char f_to_char(const float val)
{
	return FTOCHAR(val);
}

/* ProjectionPaint defines */

/* approx the number of buckets to have under the brush,
 * used with the brush size to set the ps->buckets_x and ps->buckets_y value.
 *
 * When 3 - a brush should have ~9 buckets under it at once
 * ...this helps for threading while painting as well as
 * avoiding initializing pixels that wont touch the brush */
#define PROJ_BUCKET_BRUSH_DIV 4

#define PROJ_BUCKET_RECT_MIN 4
#define PROJ_BUCKET_RECT_MAX 256

#define PROJ_BOUNDBOX_DIV 8
#define PROJ_BOUNDBOX_SQUARED  (PROJ_BOUNDBOX_DIV * PROJ_BOUNDBOX_DIV)

//#define PROJ_DEBUG_PAINT 1
//#define PROJ_DEBUG_NOSEAMBLEED 1
//#define PROJ_DEBUG_PRINT_CLIP 1
#define PROJ_DEBUG_WINCLIP 1

/* projectFaceSeamFlags options */
//#define PROJ_FACE_IGNORE	(1<<0)	/* When the face is hidden, backfacing or occluded */
//#define PROJ_FACE_INIT	(1<<1)	/* When we have initialized the faces data */
#define PROJ_FACE_SEAM1 (1 << 0)  /* If this face has a seam on any of its edges */
#define PROJ_FACE_SEAM2 (1 << 1)
#define PROJ_FACE_SEAM3 (1 << 2)
#define PROJ_FACE_SEAM4 (1 << 3)

#define PROJ_FACE_NOSEAM1   (1 << 4)
#define PROJ_FACE_NOSEAM2   (1 << 5)
#define PROJ_FACE_NOSEAM3   (1 << 6)
#define PROJ_FACE_NOSEAM4   (1 << 7)

/* face winding */
#define PROJ_FACE_WINDING_INIT 1
#define PROJ_FACE_WINDING_CW 2

#define PROJ_SRC_VIEW       1
#define PROJ_SRC_IMAGE_CAM  2
#define PROJ_SRC_IMAGE_VIEW 3

#define PROJ_VIEW_DATA_ID "view_data"
#define PROJ_VIEW_DATA_SIZE (4 * 4 + 4 * 4 + 3) /* viewmat + winmat + clipsta + clipend + is_ortho */


/* a slightly scaled down face is used to get fake 3D location for edge pixels in the seams
 * as this number approaches  1.0f the likelihood increases of float precision errors where
 * it is occluded by an adjacent face */
#define PROJ_FACE_SCALE_SEAM    0.99f

#define PROJ_BUCKET_NULL        0
#define PROJ_BUCKET_INIT        (1 << 0)
// #define PROJ_BUCKET_CLONE_INIT	(1<<1)

/* used for testing doubles, if a point is on a line etc */
#define PROJ_GEOM_TOLERANCE 0.00075f
#define PROJ_PIXEL_TOLERANCE 0.01f

/* vert flags */
#define PROJ_VERT_CULL 1

/* This is mainly a convenience struct used so we can keep an array of images we use
 * Thir imbufs, etc, in 1 array, When using threads this array is copied for each thread
 * because 'partRedrawRect' and 'touch' values would not be thread safe */
typedef struct ProjPaintImage {
	Image *ima;
	ImBuf *ibuf;
	ImagePaintPartialRedraw *partRedrawRect;
	void **undoRect; /* only used to build undo tiles after painting */
	int touch;
} ProjPaintImage;

/* Main projection painting struct passed to all projection painting functions */
typedef struct ProjPaintState {
	View3D *v3d;
	RegionView3D *rv3d;
	ARegion *ar;
	Scene *scene;
	int source; /* PROJ_SRC_**** */

	Brush *brush;
	short tool, blend, mode;
	int orig_brush_size;
	Object *ob;
	/* end similarities with ImagePaintState */

	DerivedMesh    *dm;
	int dm_totface;
	int dm_totvert;
	int dm_release;

	MVert          *dm_mvert;
	MFace          *dm_mface;
	MTFace         *dm_mtface;
	MTFace         *dm_mtface_clone;    /* other UV map, use for cloning between layers */
	MTFace         *dm_mtface_stencil;

	/* projection painting only */
	MemArena *arena_mt[BLENDER_MAX_THREADS]; /* for multithreading, the first item is sometimes used for non threaded cases too */
	LinkNode **bucketRect;              /* screen sized 2D array, each pixel has a linked list of ProjPixel's */
	LinkNode **bucketFaces;             /* bucketRect aligned array linkList of faces overlapping each bucket */
	unsigned char *bucketFlags;         /* store if the bucks have been initialized  */
#ifndef PROJ_DEBUG_NOSEAMBLEED
	char *faceSeamFlags;                /* store info about faces, if they are initialized etc*/
	char *faceWindingFlags;             /* save the winding of the face in uv space, helps as an extra validation step for seam detection */
	float (*faceSeamUVs)[4][2];         /* expanded UVs for faces to use as seams */
	LinkNode **vertFaces;               /* Only needed for when seam_bleed_px is enabled, use to find UV seams */
#endif
	char *vertFlags;                    /* store options per vert, now only store if the vert is pointing away from the view */
	int buckets_x;                      /* The size of the bucket grid, the grid span's screenMin/screenMax so you can paint outsize the screen or with 2 brushes at once */
	int buckets_y;

	ProjPaintImage *projImages;

	int pixel_sizeof;           /* result of project_paint_pixel_sizeof(), constant per stroke */

	int image_tot;              /* size of projectImages array */

	float (*screenCoords)[4];   /* verts projected into floating point screen space */

	float screenMin[2];         /* 2D bounds for mesh verts on the screen's plane (screenspace) */
	float screenMax[2];
	float screen_width;         /* Calculated from screenMin & screenMax */
	float screen_height;
	int winx, winy;             /* from the carea or from the projection render */

	/* options for projection painting */
	bool  do_layer_clone;
	bool  do_layer_stencil;
	bool  do_layer_stencil_inv;

	bool  do_occlude;               /* Use raytraced occlusion? - ortherwise will paint right through to the back*/
	bool  do_backfacecull;          /* ignore faces with normals pointing away, skips a lot of raycasts if your normals are correctly flipped */
	bool  do_mask_normal;           /* mask out pixels based on their normals */
	bool  do_new_shading_nodes;     /* cache BKE_scene_use_new_shading_nodes value */
	float normal_angle;             /* what angle to mask at*/
	float normal_angle_inner;
	float normal_angle_range;       /* difference between normal_angle and normal_angle_inner, for easy access */

	bool do_face_sel;               /* quick access to (me->editflag & ME_EDIT_PAINT_FACE_SEL) */
	bool is_ortho;
	bool do_masking;              /* use masking during painting. Some operations such as airbrush may disable */
	bool is_texbrush;              /* only to avoid running  */
	bool is_maskbrush;            /* mask brush is applied before masking */
	bool is_maskbrush_tiled;      /* mask brush is applied after masking */
#ifndef PROJ_DEBUG_NOSEAMBLEED
	float seam_bleed_px;
#endif
	/* clone vars */
	float cloneOffset[2];

	float projectMat[4][4];     /* Projection matrix, use for getting screen coords */
	float viewDir[3];           /* View vector, use for do_backfacecull and for ray casting with an ortho viewport  */
	float viewPos[3];           /* View location in object relative 3D space, so can compare to verts  */
	float clipsta, clipend;

	/* reproject vars */
	Image *reproject_image;
	ImBuf *reproject_ibuf;

	/* threads */
	int thread_tot;
	int bucketMin[2];
	int bucketMax[2];
	int context_bucket_x, context_bucket_y; /* must lock threads while accessing these */

	/* redraw */
	bool need_redraw;
} ProjPaintState;

typedef union pixelPointer {
	float *f_pt;            /* float buffer */
	unsigned int *uint_pt; /* 2 ways to access a char buffer */
	unsigned char *ch_pt;
} PixelPointer;

typedef union pixelStore {
	unsigned char ch[4];
	unsigned int uint;
	float f[4];
} PixelStore;

typedef struct ProjPixel {
	float projCoSS[2]; /* the floating point screen projection of this pixel */
	float worldCoSS[3];
	/* Only used when the airbrush is disabled.
	 * Store the max mask value to avoid painting over an area with a lower opacity
	 * with an advantage that we can avoid touching the pixel at all, if the
	 * new mask value is lower then mask_accum */
	unsigned short mask_accum;

	/* for various reasons we may want to mask out painting onto this pixel */
	unsigned short mask;

	short x_px, y_px;

	PixelStore origColor;
	PixelStore newColor;
	PixelPointer pixel;

	short image_index; /* if anyone wants to paint onto more than 32768 images they can bite me */
	unsigned char bb_cell_index;
} ProjPixel;

typedef struct ProjPixelClone {
	struct ProjPixel __pp;
	PixelStore clonepx;
} ProjPixelClone;

/* blur, store surrounding colors */
#define PROJ_PIXEL_SOFTEN_TOT 4
/* blur picking offset (in screenspace) */
#define PROJ_PIXEL_SOFTEN_OFS_PX 1.0f

static const float proj_pixel_soften_v2[PROJ_PIXEL_SOFTEN_TOT][2] = {
	{-PROJ_PIXEL_SOFTEN_OFS_PX,                         0.0f},
	{ 0.0f,                        -PROJ_PIXEL_SOFTEN_OFS_PX},
	{ 0.0f,                         PROJ_PIXEL_SOFTEN_OFS_PX},
	{ PROJ_PIXEL_SOFTEN_OFS_PX,                         0.0f},
};

/* Finish projection painting structs */

static Image *project_paint_face_image(const ProjPaintState *ps, MTFace *dm_mtface, int face_index)
{
	Image *ima;

	if (ps->do_new_shading_nodes) { /* cached BKE_scene_use_new_shading_nodes result */
		MFace *mf = ps->dm_mface + face_index;
		ED_object_get_active_image(ps->ob, mf->mat_nr + 1, &ima, NULL, NULL);
	}
	else {
		ima = dm_mtface[face_index].tpage;
	}

	return ima;
}

/* fast projection bucket array lookup, use the safe version for bound checking  */
static int project_bucket_offset(const ProjPaintState *ps, const float projCoSS[2])
{
	/* If we were not dealing with screenspace 2D coords we could simple do...
	 * ps->bucketRect[x + (y*ps->buckets_y)] */

	/* please explain?
	 * projCoSS[0] - ps->screenMin[0]   : zero origin
	 * ... / ps->screen_width           : range from 0.0 to 1.0
	 * ... * ps->buckets_x              : use as a bucket index
	 *
	 * Second multiplication does similar but for vertical offset
	 */
	return ( (int)(((projCoSS[0] - ps->screenMin[0]) / ps->screen_width)  * ps->buckets_x)) +
	       (((int)(((projCoSS[1] - ps->screenMin[1]) / ps->screen_height) * ps->buckets_y)) * ps->buckets_x);
}

static int project_bucket_offset_safe(const ProjPaintState *ps, const float projCoSS[2])
{
	int bucket_index = project_bucket_offset(ps, projCoSS);

	if (bucket_index < 0 || bucket_index >= ps->buckets_x * ps->buckets_y) {
		return -1;
	}
	else {
		return bucket_index;
	}
}

static float VecZDepthOrtho(const float pt[2],
                            const float v1[3], const float v2[3], const float v3[3],
                            float w[3])
{
	barycentric_weights_v2(v1, v2, v3, pt, w);
	return (v1[2] * w[0]) + (v2[2] * w[1]) + (v3[2] * w[2]);
}

static float VecZDepthPersp(const float pt[2],
                            const float v1[4], const float v2[4], const float v3[4],
                            float w[3])
{
	float wtot_inv, wtot;
	float w_tmp[3];

	barycentric_weights_v2_persp(v1, v2, v3, pt, w);
	/* for the depth we need the weights to match what
	 * barycentric_weights_v2 would return, in this case its easiest just to
	 * undo the 4th axis division and make it unit-sum
	 *
	 * don't call barycentric_weights_v2() because our callers expect 'w'
	 * to be weighted from the perspective */
	w_tmp[0] = w[0] * v1[3];
	w_tmp[1] = w[1] * v2[3];
	w_tmp[2] = w[2] * v3[3];

	wtot = w_tmp[0] + w_tmp[1] + w_tmp[2];

	if (wtot != 0.0f) {
		wtot_inv = 1.0f / wtot;

		w_tmp[0] = w_tmp[0] * wtot_inv;
		w_tmp[1] = w_tmp[1] * wtot_inv;
		w_tmp[2] = w_tmp[2] * wtot_inv;
	}
	else /* dummy values for zero area face */
		w_tmp[0] = w_tmp[1] = w_tmp[2] = 1.0f / 3.0f;
	/* done mimicing barycentric_weights_v2() */

	return (v1[2] * w_tmp[0]) + (v2[2] * w_tmp[1]) + (v3[2] * w_tmp[2]);
}


/* Return the top-most face index that the screen space coord 'pt' touches (or -1) */
static int project_paint_PickFace(const ProjPaintState *ps, const float pt[2], float w[3], int *side)
{
	LinkNode *node;
	float w_tmp[3];
	const float *v1, *v2, *v3, *v4;
	int bucket_index;
	int face_index;
	int best_side = -1;
	int best_face_index = -1;
	float z_depth_best = FLT_MAX, z_depth;
	MFace *mf;

	bucket_index = project_bucket_offset_safe(ps, pt);
	if (bucket_index == -1)
		return -1;



	/* we could return 0 for 1 face buckets, as long as this function assumes
	 * that the point its testing is only every originated from an existing face */

	for (node = ps->bucketFaces[bucket_index]; node; node = node->next) {
		face_index = GET_INT_FROM_POINTER(node->link);
		mf = ps->dm_mface + face_index;

		v1 = ps->screenCoords[mf->v1];
		v2 = ps->screenCoords[mf->v2];
		v3 = ps->screenCoords[mf->v3];

		if (isect_point_tri_v2(pt, v1, v2, v3)) {
			if (ps->is_ortho) z_depth = VecZDepthOrtho(pt, v1, v2, v3, w_tmp);
			else z_depth = VecZDepthPersp(pt, v1, v2, v3, w_tmp);

			if (z_depth < z_depth_best) {
				best_face_index = face_index;
				best_side = 0;
				z_depth_best = z_depth;
				copy_v3_v3(w, w_tmp);
			}
		}
		else if (mf->v4) {
			v4 = ps->screenCoords[mf->v4];

			if (isect_point_tri_v2(pt, v1, v3, v4)) {
				if (ps->is_ortho) z_depth = VecZDepthOrtho(pt, v1, v3, v4, w_tmp);
				else z_depth = VecZDepthPersp(pt, v1, v3, v4, w_tmp);

				if (z_depth < z_depth_best) {
					best_face_index = face_index;
					best_side = 1;
					z_depth_best = z_depth;
					copy_v3_v3(w, w_tmp);
				}
			}
		}
	}

	*side = best_side;
	return best_face_index; /* will be -1 or a valid face */
}

/* Converts a uv coord into a pixel location wrapping if the uv is outside 0-1 range */
static void uvco_to_wrapped_pxco(const float uv[2], int ibuf_x, int ibuf_y, float *x, float *y)
{
	/* use */
	*x = (float)fmodf(uv[0], 1.0f);
	*y = (float)fmodf(uv[1], 1.0f);

	if (*x < 0.0f) *x += 1.0f;
	if (*y < 0.0f) *y += 1.0f;

	*x = *x * ibuf_x - 0.5f;
	*y = *y * ibuf_y - 0.5f;
}

/* Set the top-most face color that the screen space coord 'pt' touches (or return 0 if none touch) */
static bool project_paint_PickColor(const ProjPaintState *ps, const float pt[2],
                                    float *rgba_fp, unsigned char *rgba, const bool interp)
{
	float w[3], uv[2];
	int side;
	int face_index;
	MTFace *tf;
	Image *ima;
	ImBuf *ibuf;
	int xi, yi;


	face_index = project_paint_PickFace(ps, pt, w, &side);

	if (face_index == -1)
		return 0;

	tf = ps->dm_mtface + face_index;

	if (side == 0) {
		interp_v2_v2v2v2(uv, tf->uv[0], tf->uv[1], tf->uv[2], w);
	}
	else { /* QUAD */
		interp_v2_v2v2v2(uv, tf->uv[0], tf->uv[2], tf->uv[3], w);
	}

	ima = project_paint_face_image(ps, ps->dm_mtface, face_index);
	ibuf = BKE_image_get_first_ibuf(ima); /* we must have got the imbuf before getting here */

	if (interp) {
		float x, y;
		uvco_to_wrapped_pxco(uv, ibuf->x, ibuf->y, &x, &y);

		if (ibuf->rect_float) {
			if (rgba_fp) {
				bilinear_interpolation_color_wrap(ibuf, NULL, rgba_fp, x, y);
			}
			else {
				float rgba_tmp_f[4];
				bilinear_interpolation_color_wrap(ibuf, NULL, rgba_tmp_f, x, y);
				premul_float_to_straight_uchar(rgba, rgba_tmp_f);
			}
		}
		else {
			if (rgba) {
				bilinear_interpolation_color_wrap(ibuf, rgba, NULL, x, y);
			}
			else {
				unsigned char rgba_tmp[4];
				bilinear_interpolation_color_wrap(ibuf, rgba_tmp, NULL, x, y);
				straight_uchar_to_premul_float(rgba_fp, rgba_tmp);
			}
		}
	}
	else {
		//xi = (int)((uv[0]*ibuf->x) + 0.5f);
		//yi = (int)((uv[1]*ibuf->y) + 0.5f);
		//if (xi < 0 || xi >= ibuf->x  ||  yi < 0 || yi >= ibuf->y) return 0;

		/* wrap */
		xi = mod_i((int)(uv[0] * ibuf->x), ibuf->x);
		yi = mod_i((int)(uv[1] * ibuf->y), ibuf->y);

		if (rgba) {
			if (ibuf->rect_float) {
				const float *rgba_tmp_fp = ibuf->rect_float + (xi + yi * ibuf->x * 4);
				premul_float_to_straight_uchar(rgba, rgba_tmp_fp);
			}
			else {
				*((unsigned int *)rgba) = *(unsigned int *)(((char *)ibuf->rect) + ((xi + yi * ibuf->x) * 4));
			}
		}

		if (rgba_fp) {
			if (ibuf->rect_float) {
				copy_v4_v4(rgba_fp, (ibuf->rect_float + ((xi + yi * ibuf->x) * 4)));
			}
			else {
				unsigned char *tmp_ch = ((unsigned char *)ibuf->rect) + ((xi + yi * ibuf->x) * 4);
				straight_uchar_to_premul_float(rgba_fp, tmp_ch);
			}
		}
	}
	BKE_image_release_ibuf(ima, ibuf, NULL);
	return 1;
}

/* Check if 'pt' is infront of the 3 verts on the Z axis (used for screenspace occlusuion test)
 * return...
 *  0	: no occlusion
 * -1	: no occlusion but 2D intersection is true (avoid testing the other half of a quad)
 *  1	: occluded
 *  2	: occluded with w[3] weights set (need to know in some cases) */

static int project_paint_occlude_ptv(const float pt[3],
                                     const float v1[4], const float v2[4], const float v3[4],
                                     float w[3], const bool is_ortho)
{
	/* if all are behind us, return false */
	if (v1[2] > pt[2] && v2[2] > pt[2] && v3[2] > pt[2])
		return 0;

	/* do a 2D point in try intersection */
	if (!isect_point_tri_v2(pt, v1, v2, v3))
		return 0;  /* we know there is  */


	/* From here on we know there IS an intersection */
	/* if ALL of the verts are infront of us then we know it intersects ? */
	if (v1[2] < pt[2] && v2[2] < pt[2] && v3[2] < pt[2]) {
		return 1;
	}
	else {
		/* we intersect? - find the exact depth at the point of intersection */
		/* Is this point is occluded by another face? */
		if (is_ortho) {
			if (VecZDepthOrtho(pt, v1, v2, v3, w) < pt[2]) return 2;
		}
		else {
			if (VecZDepthPersp(pt, v1, v2, v3, w) < pt[2]) return 2;
		}
	}
	return -1;
}


static int project_paint_occlude_ptv_clip(const ProjPaintState *ps, const MFace *mf,
                                          const float pt[3], const float v1[4], const float v2[4], const float v3[4],
                                          const int side)
{
	float w[3], wco[3];
	int ret = project_paint_occlude_ptv(pt, v1, v2, v3, w, ps->is_ortho);

	if (ret <= 0)
		return ret;

	if (ret == 1) { /* weights not calculated */
		if (ps->is_ortho) barycentric_weights_v2(v1, v2, v3, pt, w);
		else barycentric_weights_v2_persp(v1, v2, v3, pt, w);
	}

	/* Test if we're in the clipped area, */
	if (side) interp_v3_v3v3v3(wco, ps->dm_mvert[mf->v1].co, ps->dm_mvert[mf->v3].co, ps->dm_mvert[mf->v4].co, w);
	else interp_v3_v3v3v3(wco, ps->dm_mvert[mf->v1].co, ps->dm_mvert[mf->v2].co, ps->dm_mvert[mf->v3].co, w);

	if (!ED_view3d_clipping_test(ps->rv3d, wco, true)) {
		return 1;
	}

	return -1;
}


/* Check if a screenspace location is occluded by any other faces
 * check, pixelScreenCo must be in screenspace, its Z-Depth only needs to be used for comparison
 * and doesn't need to be correct in relation to X and Y coords (this is the case in perspective view) */
static bool project_bucket_point_occluded(const ProjPaintState *ps, LinkNode *bucketFace,
                                          const int orig_face, const float pixelScreenCo[4])
{
	MFace *mf;
	int face_index;
	int isect_ret;
	float w[3]; /* not needed when clipping */
	const short do_clip = ps->rv3d ? ps->rv3d->rflag & RV3D_CLIPPING : 0;

	/* we could return 0 for 1 face buckets, as long as this function assumes
	 * that the point its testing is only every originated from an existing face */

	for (; bucketFace; bucketFace = bucketFace->next) {
		face_index = GET_INT_FROM_POINTER(bucketFace->link);

		if (orig_face != face_index) {
			mf = ps->dm_mface + face_index;
			if (do_clip)
				isect_ret = project_paint_occlude_ptv_clip(ps, mf, pixelScreenCo, ps->screenCoords[mf->v1], ps->screenCoords[mf->v2], ps->screenCoords[mf->v3], 0);
			else
				isect_ret = project_paint_occlude_ptv(pixelScreenCo, ps->screenCoords[mf->v1], ps->screenCoords[mf->v2], ps->screenCoords[mf->v3], w, ps->is_ortho);

			/* Note, if (isect_ret == -1) then we don't want to test the other side of the quad */
			if (isect_ret == 0 && mf->v4) {
				if (do_clip)
					isect_ret = project_paint_occlude_ptv_clip(ps, mf, pixelScreenCo, ps->screenCoords[mf->v1], ps->screenCoords[mf->v3], ps->screenCoords[mf->v4], 1);
				else
					isect_ret = project_paint_occlude_ptv(pixelScreenCo, ps->screenCoords[mf->v1], ps->screenCoords[mf->v3], ps->screenCoords[mf->v4], w, ps->is_ortho);
			}
			if (isect_ret >= 1) {
				/* TODO - we may want to cache the first hit,
				 * it is not possible to swap the face order in the list anymore */
				return true;
			}
		}
	}
	return false;
}

/* basic line intersection, could move to math_geom.c, 2 points with a horiz line
 * 1 for an intersection, 2 if the first point is aligned, 3 if the second point is aligned */
#define ISECT_TRUE 1
#define ISECT_TRUE_P1 2
#define ISECT_TRUE_P2 3
static int line_isect_y(const float p1[2], const float p2[2], const float y_level, float *x_isect)
{
	float y_diff;

	if (y_level == p1[1]) { /* are we touching the first point? - no interpolation needed */
		*x_isect = p1[0];
		return ISECT_TRUE_P1;
	}
	if (y_level == p2[1]) { /* are we touching the second point? - no interpolation needed */
		*x_isect = p2[0];
		return ISECT_TRUE_P2;
	}

	y_diff = fabsf(p1[1] - p2[1]); /* yuck, horizontal line, we cant do much here */

	if (y_diff < 0.000001f) {
		*x_isect = (p1[0] + p2[0]) * 0.5f;
		return ISECT_TRUE;
	}

	if (p1[1] > y_level && p2[1] < y_level) {
		*x_isect = (p2[0] * (p1[1] - y_level) + p1[0] * (y_level - p2[1])) / y_diff;  /*(p1[1]-p2[1]);*/
		return ISECT_TRUE;
	}
	else if (p1[1] < y_level && p2[1] > y_level) {
		*x_isect = (p2[0] * (y_level - p1[1]) + p1[0] * (p2[1] - y_level)) / y_diff;  /*(p2[1]-p1[1]);*/
		return ISECT_TRUE;
	}
	else {
		return 0;
	}
}

static int line_isect_x(const float p1[2], const float p2[2], const float x_level, float *y_isect)
{
	float x_diff;

	if (x_level == p1[0]) { /* are we touching the first point? - no interpolation needed */
		*y_isect = p1[1];
		return ISECT_TRUE_P1;
	}
	if (x_level == p2[0]) { /* are we touching the second point? - no interpolation needed */
		*y_isect = p2[1];
		return ISECT_TRUE_P2;
	}

	x_diff = fabsf(p1[0] - p2[0]); /* yuck, horizontal line, we cant do much here */

	if (x_diff < 0.000001f) { /* yuck, vertical line, we cant do much here */
		*y_isect = (p1[0] + p2[0]) * 0.5f;
		return ISECT_TRUE;
	}

	if (p1[0] > x_level && p2[0] < x_level) {
		*y_isect = (p2[1] * (p1[0] - x_level) + p1[1] * (x_level - p2[0])) / x_diff; /*(p1[0]-p2[0]);*/
		return ISECT_TRUE;
	}
	else if (p1[0] < x_level && p2[0] > x_level) {
		*y_isect = (p2[1] * (x_level - p1[0]) + p1[1] * (p2[0] - x_level)) / x_diff; /*(p2[0]-p1[0]);*/
		return ISECT_TRUE;
	}
	else {
		return 0;
	}
}

/* simple func use for comparing UV locations to check if there are seams.
 * Its possible this gives incorrect results, when the UVs for 1 face go into the next
 * tile, but do not do this for the adjacent face, it could return a false positive.
 * This is so unlikely that Id not worry about it. */
#ifndef PROJ_DEBUG_NOSEAMBLEED
static bool cmp_uv(const float vec2a[2], const float vec2b[2])
{
	/* if the UV's are not between 0.0 and 1.0 */
	float xa = (float)fmodf(vec2a[0], 1.0f);
	float ya = (float)fmodf(vec2a[1], 1.0f);

	float xb = (float)fmodf(vec2b[0], 1.0f);
	float yb = (float)fmodf(vec2b[1], 1.0f);

	if (xa < 0.0f) xa += 1.0f;
	if (ya < 0.0f) ya += 1.0f;

	if (xb < 0.0f) xb += 1.0f;
	if (yb < 0.0f) yb += 1.0f;

	return ((fabsf(xa - xb) < PROJ_GEOM_TOLERANCE) && (fabsf(ya - yb) < PROJ_GEOM_TOLERANCE)) ? 1 : 0;
}
#endif

/* set min_px and max_px to the image space bounds of the UV coords
 * return zero if there is no area in the returned rectangle */
#ifndef PROJ_DEBUG_NOSEAMBLEED
static bool pixel_bounds_uv(
        const float uv1[2], const float uv2[2], const float uv3[2], const float uv4[2],
        rcti *bounds_px,
        const int ibuf_x, const int ibuf_y,
        const bool is_quad
        )
{
	float min_uv[2], max_uv[2]; /* UV bounds */

	INIT_MINMAX2(min_uv, max_uv);

	minmax_v2v2_v2(min_uv, max_uv, uv1);
	minmax_v2v2_v2(min_uv, max_uv, uv2);
	minmax_v2v2_v2(min_uv, max_uv, uv3);
	if (is_quad)
		minmax_v2v2_v2(min_uv, max_uv, uv4);

	bounds_px->xmin = (int)(ibuf_x * min_uv[0]);
	bounds_px->ymin = (int)(ibuf_y * min_uv[1]);

	bounds_px->xmax = (int)(ibuf_x * max_uv[0]) + 1;
	bounds_px->ymax = (int)(ibuf_y * max_uv[1]) + 1;

	/*printf("%d %d %d %d\n", min_px[0], min_px[1], max_px[0], max_px[1]);*/

	/* face uses no UV area when quantized to pixels? */
	return (bounds_px->xmin == bounds_px->xmax || bounds_px->ymin == bounds_px->ymax) ? 0 : 1;
}
#endif

static bool pixel_bounds_array(float (*uv)[2], rcti *bounds_px, const int ibuf_x, const int ibuf_y, int tot)
{
	float min_uv[2], max_uv[2]; /* UV bounds */

	if (tot == 0) {
		return 0;
	}

	INIT_MINMAX2(min_uv, max_uv);

	while (tot--) {
		minmax_v2v2_v2(min_uv, max_uv, (*uv));
		uv++;
	}

	bounds_px->xmin = (int)(ibuf_x * min_uv[0]);
	bounds_px->ymin = (int)(ibuf_y * min_uv[1]);

	bounds_px->xmax = (int)(ibuf_x * max_uv[0]) + 1;
	bounds_px->ymax = (int)(ibuf_y * max_uv[1]) + 1;

	/*printf("%d %d %d %d\n", min_px[0], min_px[1], max_px[0], max_px[1]);*/

	/* face uses no UV area when quantized to pixels? */
	return (bounds_px->xmin == bounds_px->xmax || bounds_px->ymin == bounds_px->ymax) ? 0 : 1;
}

#ifndef PROJ_DEBUG_NOSEAMBLEED

static void project_face_winding_init(const ProjPaintState *ps, const int face_index)
{
	/* detect the winding of faces in uv space */
	MTFace *tf = ps->dm_mtface + face_index;
	float winding = cross_tri_v2(tf->uv[0], tf->uv[1], tf->uv[2]);

	if (ps->dm_mface[face_index].v4)
		winding += cross_tri_v2(tf->uv[2], tf->uv[3], tf->uv[0]);

	if (winding > 0)
		ps->faceWindingFlags[face_index] |= PROJ_FACE_WINDING_CW;

	ps->faceWindingFlags[face_index] |= PROJ_FACE_WINDING_INIT;
}

/* This function returns 1 if this face has a seam along the 2 face-vert indices
 * 'orig_i1_fidx' and 'orig_i2_fidx' */
static bool check_seam(const ProjPaintState *ps,
                       const int orig_face, const int orig_i1_fidx, const int orig_i2_fidx,
                       int *other_face, int *orig_fidx)
{
	LinkNode *node;
	int face_index;
	unsigned int i1, i2;
	int i1_fidx = -1, i2_fidx = -1; /* index in face */
	MFace *mf;
	MTFace *tf;
	const MFace *orig_mf = ps->dm_mface + orig_face;
	const MTFace *orig_tf = ps->dm_mtface + orig_face;

	/* vert indices from face vert order indices */
	i1 = (*(&orig_mf->v1 + orig_i1_fidx));
	i2 = (*(&orig_mf->v1 + orig_i2_fidx));

	for (node = ps->vertFaces[i1]; node; node = node->next) {
		face_index = GET_INT_FROM_POINTER(node->link);

		if (face_index != orig_face) {
			mf = ps->dm_mface + face_index;
			/* could check if the 2 faces images match here,
			 * but then there wouldn't be a way to return the opposite face's info */


			/* We need to know the order of the verts in the adjacent face
			 * set the i1_fidx and i2_fidx to (0,1,2,3) */
			i1_fidx = BKE_MESH_TESSFACE_VINDEX_ORDER(mf, i1);
			i2_fidx = BKE_MESH_TESSFACE_VINDEX_ORDER(mf, i2);

			/* Only need to check if 'i2_fidx' is valid because we know i1_fidx is the same vert on both faces */
			if (i2_fidx != -1) {
				Image *tpage = project_paint_face_image(ps, ps->dm_mtface, face_index);
				Image *orig_tpage = project_paint_face_image(ps, ps->dm_mtface, orig_face);

				BLI_assert(i1_fidx != -1);

				/* This IS an adjacent face!, now lets check if the UVs are ok */
				tf = ps->dm_mtface + face_index;

				/* set up the other face */
				*other_face = face_index;
				*orig_fidx = (i1_fidx < i2_fidx) ? i1_fidx : i2_fidx;

				/* initialize face winding if needed */
				if ((ps->faceWindingFlags[face_index] & PROJ_FACE_WINDING_INIT) == 0)
					project_face_winding_init(ps, face_index);

				/* first test if they have the same image */
				if ((orig_tpage == tpage) &&
				    cmp_uv(orig_tf->uv[orig_i1_fidx], tf->uv[i1_fidx]) &&
				    cmp_uv(orig_tf->uv[orig_i2_fidx], tf->uv[i2_fidx]) )
				{
					/* if faces don't have the same winding in uv space,
					 * they are on the same side so edge is boundary */
					if ((ps->faceWindingFlags[face_index] & PROJ_FACE_WINDING_CW) !=
					    (ps->faceWindingFlags[orig_face] & PROJ_FACE_WINDING_CW))
					{
						return 1;
					}

					// printf("SEAM (NONE)\n");
					return 0;

				}
				else {
					// printf("SEAM (UV GAP)\n");
					return 1;
				}
			}
		}
	}
	// printf("SEAM (NO FACE)\n");
	*other_face = -1;
	return 1;
}

/* Calculate outset UV's, this is not the same as simply scaling the UVs,
 * since the outset coords are a margin that keep an even distance from the original UV's,
 * note that the image aspect is taken into account */
static void uv_image_outset(float (*orig_uv)[2], float (*outset_uv)[2], const float scaler,
                            const int ibuf_x, const int ibuf_y, const bool is_quad)
{
	float a1, a2, a3, a4 = 0.0f;
	float puv[4][2]; /* pixelspace uv's */
	float no1[2], no2[2], no3[2], no4[2]; /* normals */
	float dir1[2], dir2[2], dir3[2], dir4[2];
	float ibuf_inv[2];

	ibuf_inv[0] = 1.0f / (float)ibuf_x;
	ibuf_inv[1] = 1.0f / (float)ibuf_y;

	/* make UV's in pixel space so we can */
	puv[0][0] = orig_uv[0][0] * ibuf_x;
	puv[0][1] = orig_uv[0][1] * ibuf_y;

	puv[1][0] = orig_uv[1][0] * ibuf_x;
	puv[1][1] = orig_uv[1][1] * ibuf_y;

	puv[2][0] = orig_uv[2][0] * ibuf_x;
	puv[2][1] = orig_uv[2][1] * ibuf_y;

	if (is_quad) {
		puv[3][0] = orig_uv[3][0] * ibuf_x;
		puv[3][1] = orig_uv[3][1] * ibuf_y;
	}

	/* face edge directions */
	sub_v2_v2v2(dir1, puv[1], puv[0]);
	sub_v2_v2v2(dir2, puv[2], puv[1]);
	normalize_v2(dir1);
	normalize_v2(dir2);

	if (is_quad) {
		sub_v2_v2v2(dir3, puv[3], puv[2]);
		sub_v2_v2v2(dir4, puv[0], puv[3]);
		normalize_v2(dir3);
		normalize_v2(dir4);
	}
	else {
		sub_v2_v2v2(dir3, puv[0], puv[2]);
		normalize_v2(dir3);
	}

	if (is_quad) {
		a1 = shell_v2v2_mid_normalized_to_dist(dir4, dir1);
		a2 = shell_v2v2_mid_normalized_to_dist(dir1, dir2);
		a3 = shell_v2v2_mid_normalized_to_dist(dir2, dir3);
		a4 = shell_v2v2_mid_normalized_to_dist(dir3, dir4);
	}
	else {
		a1 = shell_v2v2_mid_normalized_to_dist(dir3, dir1);
		a2 = shell_v2v2_mid_normalized_to_dist(dir1, dir2);
		a3 = shell_v2v2_mid_normalized_to_dist(dir2, dir3);
	}

	if (is_quad) {
		sub_v2_v2v2(no1, dir4, dir1);
		sub_v2_v2v2(no2, dir1, dir2);
		sub_v2_v2v2(no3, dir2, dir3);
		sub_v2_v2v2(no4, dir3, dir4);
		normalize_v2(no1);
		normalize_v2(no2);
		normalize_v2(no3);
		normalize_v2(no4);
		mul_v2_fl(no1, a1 * scaler);
		mul_v2_fl(no2, a2 * scaler);
		mul_v2_fl(no3, a3 * scaler);
		mul_v2_fl(no4, a4 * scaler);
		add_v2_v2v2(outset_uv[0], puv[0], no1);
		add_v2_v2v2(outset_uv[1], puv[1], no2);
		add_v2_v2v2(outset_uv[2], puv[2], no3);
		add_v2_v2v2(outset_uv[3], puv[3], no4);
		mul_v2_v2(outset_uv[0], ibuf_inv);
		mul_v2_v2(outset_uv[1], ibuf_inv);
		mul_v2_v2(outset_uv[2], ibuf_inv);
		mul_v2_v2(outset_uv[3], ibuf_inv);
	}
	else {
		sub_v2_v2v2(no1, dir3, dir1);
		sub_v2_v2v2(no2, dir1, dir2);
		sub_v2_v2v2(no3, dir2, dir3);
		normalize_v2(no1);
		normalize_v2(no2);
		normalize_v2(no3);
		mul_v2_fl(no1, a1 * scaler);
		mul_v2_fl(no2, a2 * scaler);
		mul_v2_fl(no3, a3 * scaler);
		add_v2_v2v2(outset_uv[0], puv[0], no1);
		add_v2_v2v2(outset_uv[1], puv[1], no2);
		add_v2_v2v2(outset_uv[2], puv[2], no3);

		mul_v2_v2(outset_uv[0], ibuf_inv);
		mul_v2_v2(outset_uv[1], ibuf_inv);
		mul_v2_v2(outset_uv[2], ibuf_inv);
	}
}

/*
 * Be tricky with flags, first 4 bits are PROJ_FACE_SEAM1 to 4, last 4 bits are PROJ_FACE_NOSEAM1 to 4
 * 1<<i - where i is (0-3)
 *
 * If we're multithreadng, make sure threads are locked when this is called
 */
static void project_face_seams_init(const ProjPaintState *ps, const int face_index, const int is_quad)
{
	int other_face, other_fidx; /* vars for the other face, we also set its flag */
	int fidx1 = is_quad ? 3 : 2;
	int fidx2 = 0; /* next fidx in the face (0,1,2,3) -> (1,2,3,0) or (0,1,2) -> (1,2,0) for a tri */

	/* initialize face winding if needed */
	if ((ps->faceWindingFlags[face_index] & PROJ_FACE_WINDING_INIT) == 0)
		project_face_winding_init(ps, face_index);

	do {
		if ((ps->faceSeamFlags[face_index] & (1 << fidx1 | 16 << fidx1)) == 0) {
			if (check_seam(ps, face_index, fidx1, fidx2, &other_face, &other_fidx)) {
				ps->faceSeamFlags[face_index] |= 1 << fidx1;
				if (other_face != -1)
					ps->faceSeamFlags[other_face] |= 1 << other_fidx;
			}
			else {
				ps->faceSeamFlags[face_index] |= 16 << fidx1;
				if (other_face != -1)
					ps->faceSeamFlags[other_face] |= 16 << other_fidx;  /* second 4 bits for disabled */
			}
		}

		fidx2 = fidx1;
	} while (fidx1--);
}
#endif // PROJ_DEBUG_NOSEAMBLEED


/* Converts a UV location to a 3D screenspace location
 * Takes a 'uv' and 3 UV coords, and sets the values of pixelScreenCo
 *
 * This is used for finding a pixels location in screenspace for painting */
static void screen_px_from_ortho(
        const float uv[2],
        const float v1co[3], const float v2co[3], const float v3co[3],  /* Screenspace coords */
        const float uv1co[2], const float uv2co[2], const float uv3co[2],
        float pixelScreenCo[4],
        float w[3])
{
	barycentric_weights_v2(uv1co, uv2co, uv3co, uv, w);
	interp_v3_v3v3v3(pixelScreenCo, v1co, v2co, v3co, w);
}

/* same as screen_px_from_ortho except we
 * do perspective correction on the pixel coordinate */
static void screen_px_from_persp(
        const float uv[2],
        const float v1co[4], const float v2co[4], const float v3co[4],  /* screenspace coords */
        const float uv1co[2], const float uv2co[2], const float uv3co[2],
        float pixelScreenCo[4],
        float w[3])
{
	float w_int[3];
	float wtot_inv, wtot;
	barycentric_weights_v2(uv1co, uv2co, uv3co, uv, w);

	/* re-weight from the 4th coord of each screen vert */
	w_int[0] = w[0] * v1co[3];
	w_int[1] = w[1] * v2co[3];
	w_int[2] = w[2] * v3co[3];

	wtot = w_int[0] + w_int[1] + w_int[2];

	if (wtot > 0.0f) {
		wtot_inv = 1.0f / wtot;
		w_int[0] *= wtot_inv;
		w_int[1] *= wtot_inv;
		w_int[2] *= wtot_inv;
	}
	else {
		w[0] = w[1] = w[2] =
		w_int[0] = w_int[1] = w_int[2] = 1.0f / 3.0f;  /* dummy values for zero area face */
	}
	/* done re-weighting */

	/* do interpolation based on projected weight */
	interp_v3_v3v3v3(pixelScreenCo, v1co, v2co, v3co, w_int);
}


static void project_face_pixel(const MTFace *tf_other, ImBuf *ibuf_other, const float w[3],
                               int side, unsigned char rgba_ub[4], float rgba_f[4])
{
	const float *uvCo1, *uvCo2, *uvCo3;
	float uv_other[2], x, y;

	uvCo1 =  (float *)tf_other->uv[0];
	if (side == 1) {
		uvCo2 =  (float *)tf_other->uv[2];
		uvCo3 =  (float *)tf_other->uv[3];
	}
	else {
		uvCo2 =  (float *)tf_other->uv[1];
		uvCo3 =  (float *)tf_other->uv[2];
	}

	interp_v2_v2v2v2(uv_other, uvCo1, uvCo2, uvCo3, (float *)w);

	/* use */
	uvco_to_wrapped_pxco(uv_other, ibuf_other->x, ibuf_other->y, &x, &y);


	if (ibuf_other->rect_float) { /* from float to float */
		bilinear_interpolation_color_wrap(ibuf_other, NULL, rgba_f, x, y);
	}
	else { /* from char to float */
		bilinear_interpolation_color_wrap(ibuf_other, rgba_ub, NULL, x, y);
	}

}

/* run this outside project_paint_uvpixel_init since pixels with mask 0 don't need init */
static float project_paint_uvpixel_mask(
        const ProjPaintState *ps,
        const int face_index,
        const int side,
        const float w[3])
{
	float mask;

	/* Image Mask */
	if (ps->do_layer_stencil) {
		/* another UV maps image is masking this one's */
		ImBuf *ibuf_other;
		Image *other_tpage = project_paint_face_image(ps, ps->dm_mtface_stencil, face_index);
		const MTFace *tf_other = ps->dm_mtface_stencil + face_index;

		if (other_tpage && (ibuf_other = BKE_image_acquire_ibuf(other_tpage, NULL, NULL))) {
			/* BKE_image_acquire_ibuf - TODO - this may be slow */
			unsigned char rgba_ub[4];
			float rgba_f[4];

			project_face_pixel(tf_other, ibuf_other, w, side, rgba_ub, rgba_f);

			if (ibuf_other->rect_float) { /* from float to float */
				mask = ((rgba_f[0] + rgba_f[1] + rgba_f[2]) * (1.0f / 3.0f)) * rgba_f[3];
			}
			else { /* from char to float */
				mask = ((rgba_ub[0] + rgba_ub[1] + rgba_ub[2]) * (1.0f / (255.0f * 3.0f))) * (rgba_ub[3] * (1.0f / 255.0f));
			}

			BKE_image_release_ibuf(other_tpage, ibuf_other, NULL);

			if (!ps->do_layer_stencil_inv) /* matching the gimps layer mask black/white rules, white==full opacity */
				mask = (1.0f - mask);

			if (mask == 0.0f) {
				return 0.0f;
			}
		}
		else {
			return 0.0f;
		}
	}
	else {
		mask = 1.0f;
	}

	/* calculate mask */
	if (ps->do_mask_normal) {
		MFace *mf = &ps->dm_mface[face_index];
		float no[3], angle;
		if (mf->flag & ME_SMOOTH) {
			const short *no1, *no2, *no3;
			no1 = ps->dm_mvert[mf->v1].no;
			if (side == 1) {
				no2 = ps->dm_mvert[mf->v3].no;
				no3 = ps->dm_mvert[mf->v4].no;
			}
			else {
				no2 = ps->dm_mvert[mf->v2].no;
				no3 = ps->dm_mvert[mf->v3].no;
			}

			no[0] = w[0] * no1[0] + w[1] * no2[0] + w[2] * no3[0];
			no[1] = w[0] * no1[1] + w[1] * no2[1] + w[2] * no3[1];
			no[2] = w[0] * no1[2] + w[1] * no2[2] + w[2] * no3[2];
			normalize_v3(no);
		}
		else {
			/* incase the */
#if 1
			/* normalizing per pixel isn't optimal, we could cache or check ps->*/
			if (mf->v4)
				normal_quad_v3(no,
				               ps->dm_mvert[mf->v1].co,
				               ps->dm_mvert[mf->v2].co,
				               ps->dm_mvert[mf->v3].co,
				               ps->dm_mvert[mf->v4].co);
			else
				normal_tri_v3(no,
				              ps->dm_mvert[mf->v1].co,
				              ps->dm_mvert[mf->v2].co,
				              ps->dm_mvert[mf->v3].co);
#else
			/* don't use because some modifiers dont have normal data (subsurf for eg) */
			copy_v3_v3(no, (float *)ps->dm->getTessFaceData(ps->dm, face_index, CD_NORMAL));
#endif
		}

		/* now we can use the normal as a mask */
		if (ps->is_ortho) {
			angle = angle_normalized_v3v3((float *)ps->viewDir, no);
		}
		else {
			/* Annoying but for the perspective view we need to get the pixels location in 3D space :/ */
			float viewDirPersp[3];
			const float *co1, *co2, *co3;
			co1 = ps->dm_mvert[mf->v1].co;
			if (side == 1) {
				co2 = ps->dm_mvert[mf->v3].co;
				co3 = ps->dm_mvert[mf->v4].co;
			}
			else {
				co2 = ps->dm_mvert[mf->v2].co;
				co3 = ps->dm_mvert[mf->v3].co;
			}

			/* Get the direction from the viewPoint to the pixel and normalize */
			viewDirPersp[0] = (ps->viewPos[0] - (w[0] * co1[0] + w[1] * co2[0] + w[2] * co3[0]));
			viewDirPersp[1] = (ps->viewPos[1] - (w[0] * co1[1] + w[1] * co2[1] + w[2] * co3[1]));
			viewDirPersp[2] = (ps->viewPos[2] - (w[0] * co1[2] + w[1] * co2[2] + w[2] * co3[2]));
			normalize_v3(viewDirPersp);

			angle = angle_normalized_v3v3(viewDirPersp, no);
		}

		if (angle >= ps->normal_angle) {
			return 0.0f; /* outsize the normal limit*/
		}
		else if (angle > ps->normal_angle_inner) {
			mask *= (ps->normal_angle - angle) / ps->normal_angle_range;
		} /* otherwise no mask normal is needed, were within the limit */
	}

	/* This only works when the opacity dosnt change while painting, stylus pressure messes with this
	 * so don't use it. */
	// if (ps->is_airbrush == 0) mask *= BKE_brush_alpha_get(ps->brush);

	return mask;
}

static int project_paint_pixel_sizeof(const short tool)
{
	if ((tool == PAINT_TOOL_CLONE) || (tool == PAINT_TOOL_SMEAR)) {
		return sizeof(ProjPixelClone);
	}
	else {
		return sizeof(ProjPixel);
	}
}


/* run this function when we know a bucket's, face's pixel can be initialized,
 * return the ProjPixel which is added to 'ps->bucketRect[bucket_index]' */
static ProjPixel *project_paint_uvpixel_init(
        const ProjPaintState *ps,
        MemArena *arena,
        const ImBuf *ibuf,
        int x_px, int y_px,
        const float mask,
        const int face_index,
        const int image_index,
        const float pixelScreenCo[4],
        const float world_spaceCo[3],
        const int side,
        const float w[3])
{
	ProjPixel *projPixel;

	/* wrap pixel location */

	x_px = mod_i(x_px, ibuf->x);
	y_px = mod_i(y_px, ibuf->y);

	BLI_assert(ps->pixel_sizeof == project_paint_pixel_sizeof(ps->tool));
	projPixel = (ProjPixel *)BLI_memarena_alloc(arena, ps->pixel_sizeof);
	//memset(projPixel, 0, size);

	if (ibuf->rect_float) {
		projPixel->pixel.f_pt = ibuf->rect_float + ((x_px + y_px * ibuf->x) * 4);
		projPixel->origColor.f[0] = projPixel->pixel.f_pt[0];
		projPixel->origColor.f[1] = projPixel->pixel.f_pt[1];
		projPixel->origColor.f[2] = projPixel->pixel.f_pt[2];
		projPixel->origColor.f[3] = projPixel->pixel.f_pt[3];
		zero_v4(projPixel->newColor.f);
	}
	else {
		projPixel->pixel.ch_pt = ((unsigned char *)ibuf->rect + ((x_px + y_px * ibuf->x) * 4));
		projPixel->origColor.uint = *projPixel->pixel.uint_pt;
		projPixel->newColor.uint = 0;
	}

	/* screenspace unclamped, we could keep its z and w values but don't need them at the moment */
	if (ps->brush->mtex.brush_map_mode == MTEX_MAP_MODE_3D) {
		copy_v3_v3(projPixel->worldCoSS, world_spaceCo);
	}

	copy_v2_v2(projPixel->projCoSS, pixelScreenCo);

	projPixel->x_px = x_px;
	projPixel->y_px = y_px;

	projPixel->mask = (unsigned short)(mask * 65535);
	projPixel->mask_accum = 0;

	/* which bounding box cell are we in?, needed for undo */
	projPixel->bb_cell_index = ((int)(((float)x_px / (float)ibuf->x) * PROJ_BOUNDBOX_DIV)) +
	                           ((int)(((float)y_px / (float)ibuf->y) * PROJ_BOUNDBOX_DIV)) * PROJ_BOUNDBOX_DIV;

	/* done with view3d_project_float inline */
	if (ps->tool == PAINT_TOOL_CLONE) {
		if (ps->dm_mtface_clone) {
			ImBuf *ibuf_other;
			Image *other_tpage = project_paint_face_image(ps, ps->dm_mtface_clone, face_index);
			const MTFace *tf_other = ps->dm_mtface_clone + face_index;

			if (other_tpage && (ibuf_other = BKE_image_acquire_ibuf(other_tpage, NULL, NULL))) {
				/* BKE_image_acquire_ibuf - TODO - this may be slow */

				if (ibuf->rect_float) {
					if (ibuf_other->rect_float) { /* from float to float */
						project_face_pixel(tf_other, ibuf_other, w, side, NULL, ((ProjPixelClone *)projPixel)->clonepx.f);
					}
					else { /* from char to float */
						unsigned char rgba_ub[4];
						float rgba[4];
						project_face_pixel(tf_other, ibuf_other, w, side, rgba_ub, NULL);
						srgb_to_linearrgb_uchar4(rgba, rgba_ub);
						straight_to_premul_v4_v4(((ProjPixelClone *)projPixel)->clonepx.f, rgba);
					}
				}
				else {
					if (ibuf_other->rect_float) { /* float to char */
						float rgba[4];
						project_face_pixel(tf_other, ibuf_other, w, side, NULL, rgba);
						premul_to_straight_v4(rgba);
						linearrgb_to_srgb_uchar3(((ProjPixelClone *)projPixel)->clonepx.ch, rgba);
					}
					else { /* char to char */
						project_face_pixel(tf_other, ibuf_other, w, side, ((ProjPixelClone *)projPixel)->clonepx.ch, NULL);
					}
				}

				BKE_image_release_ibuf(other_tpage, ibuf_other, NULL);
			}
			else {
				if (ibuf->rect_float) {
					((ProjPixelClone *)projPixel)->clonepx.f[3] = 0;
				}
				else {
					((ProjPixelClone *)projPixel)->clonepx.ch[3] = 0;
				}
			}

		}
		else {
			float co[2];
			sub_v2_v2v2(co, projPixel->projCoSS, (float *)ps->cloneOffset);

			/* no need to initialize the bucket, we're only checking buckets faces and for this
			 * the faces are already initialized in project_paint_delayed_face_init(...) */
			if (ibuf->rect_float) {
				if (!project_paint_PickColor(ps, co, ((ProjPixelClone *)projPixel)->clonepx.f, NULL, 1)) {
					((ProjPixelClone *)projPixel)->clonepx.f[3] = 0; /* zero alpha - ignore */
				}
			}
			else {
				if (!project_paint_PickColor(ps, co, NULL, ((ProjPixelClone *)projPixel)->clonepx.ch, 1)) {
					((ProjPixelClone *)projPixel)->clonepx.ch[3] = 0; /* zero alpha - ignore */
				}
			}
		}
	}

#ifdef PROJ_DEBUG_PAINT
	if (ibuf->rect_float) projPixel->pixel.f_pt[0] = 0;
	else                  projPixel->pixel.ch_pt[0] = 0;
#endif
	projPixel->image_index = image_index;

	return projPixel;
}

static bool line_clip_rect2f(
        rctf *rect,
        const float l1[2], const float l2[2],
        float l1_clip[2], float l2_clip[2])
{
	/* first account for horizontal, then vertical lines */
	/* horiz */
	if (fabsf(l1[1] - l2[1]) < PROJ_GEOM_TOLERANCE) {
		/* is the line out of range on its Y axis? */
		if (l1[1] < rect->ymin || l1[1] > rect->ymax) {
			return 0;
		}
		/* line is out of range on its X axis */
		if ((l1[0] < rect->xmin && l2[0] < rect->xmin) || (l1[0] > rect->xmax && l2[0] > rect->xmax)) {
			return 0;
		}


		if (fabsf(l1[0] - l2[0]) < PROJ_GEOM_TOLERANCE) { /* this is a single point  (or close to)*/
			if (BLI_rctf_isect_pt_v(rect, l1)) {
				copy_v2_v2(l1_clip, l1);
				copy_v2_v2(l2_clip, l2);
				return 1;
			}
			else {
				return 0;
			}
		}

		copy_v2_v2(l1_clip, l1);
		copy_v2_v2(l2_clip, l2);
		CLAMP(l1_clip[0], rect->xmin, rect->xmax);
		CLAMP(l2_clip[0], rect->xmin, rect->xmax);
		return 1;
	}
	else if (fabsf(l1[0] - l2[0]) < PROJ_GEOM_TOLERANCE) {
		/* is the line out of range on its X axis? */
		if (l1[0] < rect->xmin || l1[0] > rect->xmax) {
			return 0;
		}

		/* line is out of range on its Y axis */
		if ((l1[1] < rect->ymin && l2[1] < rect->ymin) || (l1[1] > rect->ymax && l2[1] > rect->ymax)) {
			return 0;
		}

		if (fabsf(l1[1] - l2[1]) < PROJ_GEOM_TOLERANCE) { /* this is a single point  (or close to)*/
			if (BLI_rctf_isect_pt_v(rect, l1)) {
				copy_v2_v2(l1_clip, l1);
				copy_v2_v2(l2_clip, l2);
				return 1;
			}
			else {
				return 0;
			}
		}

		copy_v2_v2(l1_clip, l1);
		copy_v2_v2(l2_clip, l2);
		CLAMP(l1_clip[1], rect->ymin, rect->ymax);
		CLAMP(l2_clip[1], rect->ymin, rect->ymax);
		return 1;
	}
	else {
		float isect;
		short ok1 = 0;
		short ok2 = 0;

		/* Done with vertical lines */

		/* are either of the points inside the rectangle ? */
		if (BLI_rctf_isect_pt_v(rect, l1)) {
			copy_v2_v2(l1_clip, l1);
			ok1 = 1;
		}

		if (BLI_rctf_isect_pt_v(rect, l2)) {
			copy_v2_v2(l2_clip, l2);
			ok2 = 1;
		}

		/* line inside rect */
		if (ok1 && ok2) return 1;

		/* top/bottom */
		if (line_isect_y(l1, l2, rect->ymin, &isect) && (isect >= rect->xmin) && (isect <= rect->xmax)) {
			if (l1[1] < l2[1]) { /* line 1 is outside */
				l1_clip[0] = isect;
				l1_clip[1] = rect->ymin;
				ok1 = 1;
			}
			else {
				l2_clip[0] = isect;
				l2_clip[1] = rect->ymin;
				ok2 = 2;
			}
		}

		if (ok1 && ok2) return 1;

		if (line_isect_y(l1, l2, rect->ymax, &isect) && (isect >= rect->xmin) && (isect <= rect->xmax)) {
			if (l1[1] > l2[1]) { /* line 1 is outside */
				l1_clip[0] = isect;
				l1_clip[1] = rect->ymax;
				ok1 = 1;
			}
			else {
				l2_clip[0] = isect;
				l2_clip[1] = rect->ymax;
				ok2 = 2;
			}
		}

		if (ok1 && ok2) return 1;

		/* left/right */
		if (line_isect_x(l1, l2, rect->xmin, &isect) && (isect >= rect->ymin) && (isect <= rect->ymax)) {
			if (l1[0] < l2[0]) { /* line 1 is outside */
				l1_clip[0] = rect->xmin;
				l1_clip[1] = isect;
				ok1 = 1;
			}
			else {
				l2_clip[0] = rect->xmin;
				l2_clip[1] = isect;
				ok2 = 2;
			}
		}

		if (ok1 && ok2) return 1;

		if (line_isect_x(l1, l2, rect->xmax, &isect) && (isect >= rect->ymin) && (isect <= rect->ymax)) {
			if (l1[0] > l2[0]) { /* line 1 is outside */
				l1_clip[0] = rect->xmax;
				l1_clip[1] = isect;
				ok1 = 1;
			}
			else {
				l2_clip[0] = rect->xmax;
				l2_clip[1] = isect;
				ok2 = 2;
			}
		}

		if (ok1 && ok2) {
			return 1;
		}
		else {
			return 0;
		}
	}
}



/* scale the quad & tri about its center
 * scaling by PROJ_FACE_SCALE_SEAM (0.99x) is used for getting fake UV pixel coords that are on the
 * edge of the face but slightly inside it occlusion tests don't return hits on adjacent faces */
#ifndef PROJ_DEBUG_NOSEAMBLEED
static void scale_quad(float insetCos[4][3], float *origCos[4], const float inset)
{
	float cent[3];
	cent[0] = (origCos[0][0] + origCos[1][0] + origCos[2][0] + origCos[3][0]) * (1.0f / 4.0f);
	cent[1] = (origCos[0][1] + origCos[1][1] + origCos[2][1] + origCos[3][1]) * (1.0f / 4.0f);
	cent[2] = (origCos[0][2] + origCos[1][2] + origCos[2][2] + origCos[3][2]) * (1.0f / 4.0f);

	sub_v3_v3v3(insetCos[0], origCos[0], cent);
	sub_v3_v3v3(insetCos[1], origCos[1], cent);
	sub_v3_v3v3(insetCos[2], origCos[2], cent);
	sub_v3_v3v3(insetCos[3], origCos[3], cent);

	mul_v3_fl(insetCos[0], inset);
	mul_v3_fl(insetCos[1], inset);
	mul_v3_fl(insetCos[2], inset);
	mul_v3_fl(insetCos[3], inset);

	add_v3_v3(insetCos[0], cent);
	add_v3_v3(insetCos[1], cent);
	add_v3_v3(insetCos[2], cent);
	add_v3_v3(insetCos[3], cent);
}


static void scale_tri(float insetCos[4][3], float *origCos[4], const float inset)
{
	float cent[3];
	cent[0] = (origCos[0][0] + origCos[1][0] + origCos[2][0]) * (1.0f / 3.0f);
	cent[1] = (origCos[0][1] + origCos[1][1] + origCos[2][1]) * (1.0f / 3.0f);
	cent[2] = (origCos[0][2] + origCos[1][2] + origCos[2][2]) * (1.0f / 3.0f);

	sub_v3_v3v3(insetCos[0], origCos[0], cent);
	sub_v3_v3v3(insetCos[1], origCos[1], cent);
	sub_v3_v3v3(insetCos[2], origCos[2], cent);

	mul_v3_fl(insetCos[0], inset);
	mul_v3_fl(insetCos[1], inset);
	mul_v3_fl(insetCos[2], inset);

	add_v3_v3(insetCos[0], cent);
	add_v3_v3(insetCos[1], cent);
	add_v3_v3(insetCos[2], cent);
}
#endif //PROJ_DEBUG_NOSEAMBLEED

static float len_squared_v2v2_alt(const float v1[2], const float v2_1, const float v2_2)
{
	float x, y;

	x = v1[0] - v2_1;
	y = v1[1] - v2_2;
	return x * x + y * y;
}

/* note, use a squared value so we can use len_squared_v2v2
 * be sure that you have done a bounds check first or this may fail */
/* only give bucket_bounds as an arg because we need it elsewhere */
static bool project_bucket_isect_circle(const float cent[2], const float radius_squared, rctf *bucket_bounds)
{

	/* Would normally to a simple intersection test, however we know the bounds of these 2 already intersect
	 * so we only need to test if the center is inside the vertical or horizontal bounds on either axis,
	 * this is even less work then an intersection test
	 */
#if 0
	if (BLI_rctf_isect_pt_v(bucket_bounds, cent))
		return 1;
#endif

	if ((bucket_bounds->xmin <= cent[0] && bucket_bounds->xmax >= cent[0]) ||
	    (bucket_bounds->ymin <= cent[1] && bucket_bounds->ymax >= cent[1]))
	{
		return 1;
	}

	/* out of bounds left */
	if (cent[0] < bucket_bounds->xmin) {
		/* lower left out of radius test */
		if (cent[1] < bucket_bounds->ymin) {
			return (len_squared_v2v2_alt(cent, bucket_bounds->xmin, bucket_bounds->ymin) < radius_squared) ? 1 : 0;
		}
		/* top left test */
		else if (cent[1] > bucket_bounds->ymax) {
			return (len_squared_v2v2_alt(cent, bucket_bounds->xmin, bucket_bounds->ymax) < radius_squared) ? 1 : 0;
		}
	}
	else if (cent[0] > bucket_bounds->xmax) {
		/* lower right out of radius test */
		if (cent[1] < bucket_bounds->ymin) {
			return (len_squared_v2v2_alt(cent, bucket_bounds->xmax, bucket_bounds->ymin) < radius_squared) ? 1 : 0;
		}
		/* top right test */
		else if (cent[1] > bucket_bounds->ymax) {
			return (len_squared_v2v2_alt(cent, bucket_bounds->xmax, bucket_bounds->ymax) < radius_squared) ? 1 : 0;
		}
	}

	return 0;
}



/* Note for rect_to_uvspace_ortho() and rect_to_uvspace_persp()
 * in ortho view this function gives good results when bucket_bounds are outside the triangle
 * however in some cases, perspective view will mess up with faces that have minimal screenspace area
 * (viewed from the side)
 *
 * for this reason its not reliable in this case so we'll use the Simple Barycentric'
 * funcs that only account for points inside the triangle.
 * however switching back to this for ortho is always an option */

static void rect_to_uvspace_ortho(
        rctf *bucket_bounds,
        const float *v1coSS, const float *v2coSS, const float *v3coSS,
        const float *uv1co, const float *uv2co, const float *uv3co,
        float bucket_bounds_uv[4][2],
        const int flip)
{
	float uv[2];
	float w[3];

	/* get the UV space bounding box */
	uv[0] = bucket_bounds->xmax;
	uv[1] = bucket_bounds->ymin;
	barycentric_weights_v2(v1coSS, v2coSS, v3coSS, uv, w);
	interp_v2_v2v2v2(bucket_bounds_uv[flip ? 3 : 0], uv1co, uv2co, uv3co, w);

	//uv[0] = bucket_bounds->xmax; // set above
	uv[1] = bucket_bounds->ymax;
	barycentric_weights_v2(v1coSS, v2coSS, v3coSS, uv, w);
	interp_v2_v2v2v2(bucket_bounds_uv[flip ? 2 : 1], uv1co, uv2co, uv3co, w);

	uv[0] = bucket_bounds->xmin;
	//uv[1] = bucket_bounds->ymax; // set above
	barycentric_weights_v2(v1coSS, v2coSS, v3coSS, uv, w);
	interp_v2_v2v2v2(bucket_bounds_uv[flip ? 1 : 2], uv1co, uv2co, uv3co, w);

	//uv[0] = bucket_bounds->xmin; // set above
	uv[1] = bucket_bounds->ymin;
	barycentric_weights_v2(v1coSS, v2coSS, v3coSS, uv, w);
	interp_v2_v2v2v2(bucket_bounds_uv[flip ? 0 : 3], uv1co, uv2co, uv3co, w);
}

/* same as above but use barycentric_weights_v2_persp */
static void rect_to_uvspace_persp(
        rctf *bucket_bounds,
        const float *v1coSS, const float *v2coSS, const float *v3coSS,
        const float *uv1co, const float *uv2co, const float *uv3co,
        float bucket_bounds_uv[4][2],
        const int flip
        )
{
	float uv[2];
	float w[3];

	/* get the UV space bounding box */
	uv[0] = bucket_bounds->xmax;
	uv[1] = bucket_bounds->ymin;
	barycentric_weights_v2_persp(v1coSS, v2coSS, v3coSS, uv, w);
	interp_v2_v2v2v2(bucket_bounds_uv[flip ? 3 : 0], uv1co, uv2co, uv3co, w);

	//uv[0] = bucket_bounds->xmax; // set above
	uv[1] = bucket_bounds->ymax;
	barycentric_weights_v2_persp(v1coSS, v2coSS, v3coSS, uv, w);
	interp_v2_v2v2v2(bucket_bounds_uv[flip ? 2 : 1], uv1co, uv2co, uv3co, w);

	uv[0] = bucket_bounds->xmin;
	//uv[1] = bucket_bounds->ymax; // set above
	barycentric_weights_v2_persp(v1coSS, v2coSS, v3coSS, uv, w);
	interp_v2_v2v2v2(bucket_bounds_uv[flip ? 1 : 2], uv1co, uv2co, uv3co, w);

	//uv[0] = bucket_bounds->xmin; // set above
	uv[1] = bucket_bounds->ymin;
	barycentric_weights_v2_persp(v1coSS, v2coSS, v3coSS, uv, w);
	interp_v2_v2v2v2(bucket_bounds_uv[flip ? 0 : 3], uv1co, uv2co, uv3co, w);
}

/* This works as we need it to but we can save a few steps and not use it */

#if 0
static float angle_2d_clockwise(const float p1[2], const float p2[2], const float p3[2])
{
	float v1[2], v2[2];

	v1[0] = p1[0] - p2[0];    v1[1] = p1[1] - p2[1];
	v2[0] = p3[0] - p2[0];    v2[1] = p3[1] - p2[1];

	return -atan2(v1[0] * v2[1] - v1[1] * v2[0], v1[0] * v2[0] + v1[1] * v2[1]);
}
#endif

#define ISECT_1 (1)
#define ISECT_2 (1 << 1)
#define ISECT_3 (1 << 2)
#define ISECT_4 (1 << 3)
#define ISECT_ALL3 ((1 << 3) - 1)
#define ISECT_ALL4 ((1 << 4) - 1)

/* limit must be a fraction over 1.0f */
static bool IsectPT2Df_limit(float pt[2], float v1[2], float v2[2], float v3[2], float limit)
{
	return ((area_tri_v2(pt, v1, v2) +
	         area_tri_v2(pt, v2, v3) +
	         area_tri_v2(pt, v3, v1)) / (area_tri_v2(v1, v2, v3))) < limit;
}

/* Clip the face by a bucket and set the uv-space bucket_bounds_uv
 * so we have the clipped UV's to do pixel intersection tests with
 * */
static int float_z_sort_flip(const void *p1, const void *p2)
{
	return (((float *)p1)[2] < ((float *)p2)[2] ? 1 : -1);
}

static int float_z_sort(const void *p1, const void *p2)
{
	return (((float *)p1)[2] < ((float *)p2)[2] ? -1 : 1);
}

static void project_bucket_clip_face(
        const bool is_ortho,
        rctf *bucket_bounds,
        float *v1coSS, float *v2coSS, float *v3coSS,
        const float *uv1co, const float *uv2co, const float *uv3co,
        float bucket_bounds_uv[8][2],
        int *tot)
{
	int inside_bucket_flag = 0;
	int inside_face_flag = 0;
	const int flip = ((line_point_side_v2(v1coSS, v2coSS, v3coSS) > 0.0f) != (line_point_side_v2(uv1co, uv2co, uv3co) > 0.0f));

	float bucket_bounds_ss[4][2];

	/* get the UV space bounding box */
	inside_bucket_flag |= BLI_rctf_isect_pt_v(bucket_bounds, v1coSS);
	inside_bucket_flag |= BLI_rctf_isect_pt_v(bucket_bounds, v2coSS) << 1;
	inside_bucket_flag |= BLI_rctf_isect_pt_v(bucket_bounds, v3coSS) << 2;

	if (inside_bucket_flag == ISECT_ALL3) {
		/* all screenspace points are inside the bucket bounding box, this means we don't need to clip and can simply return the UVs */
		if (flip) { /* facing the back? */
			copy_v2_v2(bucket_bounds_uv[0], uv3co);
			copy_v2_v2(bucket_bounds_uv[1], uv2co);
			copy_v2_v2(bucket_bounds_uv[2], uv1co);
		}
		else {
			copy_v2_v2(bucket_bounds_uv[0], uv1co);
			copy_v2_v2(bucket_bounds_uv[1], uv2co);
			copy_v2_v2(bucket_bounds_uv[2], uv3co);
		}

		*tot = 3;
		return;
	}

	/* get the UV space bounding box */
	/* use IsectPT2Df_limit here so we catch points are are touching the tri edge (or a small fraction over) */
	bucket_bounds_ss[0][0] = bucket_bounds->xmax;
	bucket_bounds_ss[0][1] = bucket_bounds->ymin;
	inside_face_flag |= (IsectPT2Df_limit(bucket_bounds_ss[0], v1coSS, v2coSS, v3coSS, 1 + PROJ_GEOM_TOLERANCE) ? ISECT_1 : 0);

	bucket_bounds_ss[1][0] = bucket_bounds->xmax;
	bucket_bounds_ss[1][1] = bucket_bounds->ymax;
	inside_face_flag |= (IsectPT2Df_limit(bucket_bounds_ss[1], v1coSS, v2coSS, v3coSS, 1 + PROJ_GEOM_TOLERANCE) ? ISECT_2 : 0);

	bucket_bounds_ss[2][0] = bucket_bounds->xmin;
	bucket_bounds_ss[2][1] = bucket_bounds->ymax;
	inside_face_flag |= (IsectPT2Df_limit(bucket_bounds_ss[2], v1coSS, v2coSS, v3coSS, 1 + PROJ_GEOM_TOLERANCE) ? ISECT_3 : 0);

	bucket_bounds_ss[3][0] = bucket_bounds->xmin;
	bucket_bounds_ss[3][1] = bucket_bounds->ymin;
	inside_face_flag |= (IsectPT2Df_limit(bucket_bounds_ss[3], v1coSS, v2coSS, v3coSS, 1 + PROJ_GEOM_TOLERANCE) ? ISECT_4 : 0);

	if (inside_face_flag == ISECT_ALL4) {
		/* bucket is totally inside the screenspace face, we can safely use weights */

		if (is_ortho) rect_to_uvspace_ortho(bucket_bounds, v1coSS, v2coSS, v3coSS, uv1co, uv2co, uv3co, bucket_bounds_uv, flip);
		else rect_to_uvspace_persp(bucket_bounds, v1coSS, v2coSS, v3coSS, uv1co, uv2co, uv3co, bucket_bounds_uv, flip);

		*tot = 4;
		return;
	}
	else {
		/* The Complicated Case!
		 *
		 * The 2 cases above are where the face is inside the bucket or the bucket is inside the face.
		 *
		 * we need to make a convex polyline from the intersection between the screenspace face
		 * and the bucket bounds.
		 *
		 * There are a number of ways this could be done, currently it just collects all intersecting verts,
		 * and line intersections,  then sorts them clockwise, this is a lot easier then evaluating the geometry to
		 * do a correct clipping on both shapes. */


		/* add a bunch of points, we know must make up the convex hull which is the clipped rect and triangle */



		/* Maximum possible 6 intersections when using a rectangle and triangle */
		float isectVCosSS[8][3]; /* The 3rd float is used to store angle for qsort(), NOT as a Z location */
		float v1_clipSS[2], v2_clipSS[2];
		float w[3];

		/* calc center */
		float cent[2] = {0.0f, 0.0f};
		/*float up[2] = {0.0f, 1.0f};*/
		int i;
		short doubles;

		(*tot) = 0;

		if (inside_face_flag & ISECT_1) { copy_v2_v2(isectVCosSS[*tot], bucket_bounds_ss[0]); (*tot)++; }
		if (inside_face_flag & ISECT_2) { copy_v2_v2(isectVCosSS[*tot], bucket_bounds_ss[1]); (*tot)++; }
		if (inside_face_flag & ISECT_3) { copy_v2_v2(isectVCosSS[*tot], bucket_bounds_ss[2]); (*tot)++; }
		if (inside_face_flag & ISECT_4) { copy_v2_v2(isectVCosSS[*tot], bucket_bounds_ss[3]); (*tot)++; }

		if (inside_bucket_flag & ISECT_1) { copy_v2_v2(isectVCosSS[*tot], v1coSS); (*tot)++; }
		if (inside_bucket_flag & ISECT_2) { copy_v2_v2(isectVCosSS[*tot], v2coSS); (*tot)++; }
		if (inside_bucket_flag & ISECT_3) { copy_v2_v2(isectVCosSS[*tot], v3coSS); (*tot)++; }

		if ((inside_bucket_flag & (ISECT_1 | ISECT_2)) != (ISECT_1 | ISECT_2)) {
			if (line_clip_rect2f(bucket_bounds, v1coSS, v2coSS, v1_clipSS, v2_clipSS)) {
				if ((inside_bucket_flag & ISECT_1) == 0) { copy_v2_v2(isectVCosSS[*tot], v1_clipSS); (*tot)++; }
				if ((inside_bucket_flag & ISECT_2) == 0) { copy_v2_v2(isectVCosSS[*tot], v2_clipSS); (*tot)++; }
			}
		}

		if ((inside_bucket_flag & (ISECT_2 | ISECT_3)) != (ISECT_2 | ISECT_3)) {
			if (line_clip_rect2f(bucket_bounds, v2coSS, v3coSS, v1_clipSS, v2_clipSS)) {
				if ((inside_bucket_flag & ISECT_2) == 0) { copy_v2_v2(isectVCosSS[*tot], v1_clipSS); (*tot)++; }
				if ((inside_bucket_flag & ISECT_3) == 0) { copy_v2_v2(isectVCosSS[*tot], v2_clipSS); (*tot)++; }
			}
		}

		if ((inside_bucket_flag & (ISECT_3 | ISECT_1)) != (ISECT_3 | ISECT_1)) {
			if (line_clip_rect2f(bucket_bounds, v3coSS, v1coSS, v1_clipSS, v2_clipSS)) {
				if ((inside_bucket_flag & ISECT_3) == 0) { copy_v2_v2(isectVCosSS[*tot], v1_clipSS); (*tot)++; }
				if ((inside_bucket_flag & ISECT_1) == 0) { copy_v2_v2(isectVCosSS[*tot], v2_clipSS); (*tot)++; }
			}
		}


		if ((*tot) < 3) { /* no intersections to speak of */
			*tot = 0;
			return;
		}

		/* now we have all points we need, collect their angles and sort them clockwise */

		for (i = 0; i < (*tot); i++) {
			cent[0] += isectVCosSS[i][0];
			cent[1] += isectVCosSS[i][1];
		}
		cent[0] = cent[0] / (float)(*tot);
		cent[1] = cent[1] / (float)(*tot);



		/* Collect angles for every point around the center point */


#if 0   /* uses a few more cycles then the above loop */
		for (i = 0; i < (*tot); i++) {
			isectVCosSS[i][2] = angle_2d_clockwise(up, cent, isectVCosSS[i]);
		}
#endif

		v1_clipSS[0] = cent[0]; /* Abuse this var for the loop below */
		v1_clipSS[1] = cent[1] + 1.0f;

		for (i = 0; i < (*tot); i++) {
			v2_clipSS[0] = isectVCosSS[i][0] - cent[0];
			v2_clipSS[1] = isectVCosSS[i][1] - cent[1];
			isectVCosSS[i][2] = atan2f(v1_clipSS[0] * v2_clipSS[1] - v1_clipSS[1] * v2_clipSS[0], v1_clipSS[0] * v2_clipSS[0] + v1_clipSS[1] * v2_clipSS[1]);
		}

		if (flip) qsort(isectVCosSS, *tot, sizeof(float) * 3, float_z_sort_flip);
		else      qsort(isectVCosSS, *tot, sizeof(float) * 3, float_z_sort);

		/* remove doubles */
		/* first/last check */
		if (fabsf(isectVCosSS[0][0] - isectVCosSS[(*tot) - 1][0]) < PROJ_PIXEL_TOLERANCE &&
		    fabsf(isectVCosSS[0][1] - isectVCosSS[(*tot) - 1][1]) < PROJ_PIXEL_TOLERANCE)
		{
			(*tot)--;
		}

		/* its possible there is only a few left after remove doubles */
		if ((*tot) < 3) {
			// printf("removed too many doubles A\n");
			*tot = 0;
			return;
		}

		doubles = true;
		while (doubles == true) {
			doubles = false;
			for (i = 1; i < (*tot); i++) {
				if (fabsf(isectVCosSS[i - 1][0] - isectVCosSS[i][0]) < PROJ_PIXEL_TOLERANCE &&
				    fabsf(isectVCosSS[i - 1][1] - isectVCosSS[i][1]) < PROJ_PIXEL_TOLERANCE)
				{
					int j;
					for (j = i + 1; j < (*tot); j++) {
						isectVCosSS[j - 1][0] = isectVCosSS[j][0];
						isectVCosSS[j - 1][1] = isectVCosSS[j][1];
					}
					doubles = true; /* keep looking for more doubles */
					(*tot)--;
				}
			}
		}

		/* its possible there is only a few left after remove doubles */
		if ((*tot) < 3) {
			// printf("removed too many doubles B\n");
			*tot = 0;
			return;
		}


		if (is_ortho) {
			for (i = 0; i < (*tot); i++) {
				barycentric_weights_v2(v1coSS, v2coSS, v3coSS, isectVCosSS[i], w);
				interp_v2_v2v2v2(bucket_bounds_uv[i], uv1co, uv2co, uv3co, w);
			}
		}
		else {
			for (i = 0; i < (*tot); i++) {
				barycentric_weights_v2_persp(v1coSS, v2coSS, v3coSS, isectVCosSS[i], w);
				interp_v2_v2v2v2(bucket_bounds_uv[i], uv1co, uv2co, uv3co, w);
			}
		}
	}

#ifdef PROJ_DEBUG_PRINT_CLIP
	/* include this at the bottom of the above function to debug the output */

	{
		/* If there are ever any problems, */
		float test_uv[4][2];
		int i;
		if (is_ortho) rect_to_uvspace_ortho(bucket_bounds, v1coSS, v2coSS, v3coSS, uv1co, uv2co, uv3co, test_uv, flip);
		else          rect_to_uvspace_persp(bucket_bounds, v1coSS, v2coSS, v3coSS, uv1co, uv2co, uv3co, test_uv, flip);
		printf("(  [(%f,%f), (%f,%f), (%f,%f), (%f,%f)], ", test_uv[0][0], test_uv[0][1],   test_uv[1][0], test_uv[1][1],    test_uv[2][0], test_uv[2][1],    test_uv[3][0], test_uv[3][1]);

		printf("  [(%f,%f), (%f,%f), (%f,%f)], ", uv1co[0], uv1co[1],   uv2co[0], uv2co[1],    uv3co[0], uv3co[1]);

		printf("[");
		for (i = 0; i < (*tot); i++) {
			printf("(%f, %f),", bucket_bounds_uv[i][0], bucket_bounds_uv[i][1]);
		}
		printf("]),\\\n");
	}
#endif
}

/*
 * # This script creates faces in a blender scene from printed data above.
 *
 * project_ls = [
 * ...(output from above block)...
 * ]
 *
 * from Blender import Scene, Mesh, Window, sys, Mathutils
 *
 * import bpy
 *
 * V = Mathutils.Vector
 *
 * def main():
 *     sce = bpy.data.scenes.active
 *
 *     for item in project_ls:
 *         bb = item[0]
 *         uv = item[1]
 *         poly = item[2]
 *
 *         me = bpy.data.meshes.new()
 *         ob = sce.objects.new(me)
 *
 *         me.verts.extend([V(bb[0]).xyz, V(bb[1]).xyz, V(bb[2]).xyz, V(bb[3]).xyz])
 *         me.faces.extend([(0,1,2,3),])
 *         me.verts.extend([V(uv[0]).xyz, V(uv[1]).xyz, V(uv[2]).xyz])
 *         me.faces.extend([(4,5,6),])
 *
 *         vs = [V(p).xyz for p in poly]
 *         print len(vs)
 *         l = len(me.verts)
 *         me.verts.extend(vs)
 *
 *         i = l
 *         while i < len(me.verts):
 *             ii = i + 1
 *             if ii == len(me.verts):
 *                 ii = l
 *             me.edges.extend([i, ii])
 *             i += 1
 *
 * if __name__ == '__main__':
 *     main()
 */


#undef ISECT_1
#undef ISECT_2
#undef ISECT_3
#undef ISECT_4
#undef ISECT_ALL3
#undef ISECT_ALL4


/* checks if pt is inside a convex 2D polyline, the polyline must be ordered rotating clockwise
 * otherwise it would have to test for mixed (line_point_side_v2 > 0.0f) cases */
static bool IsectPoly2Df(const float pt[2], float uv[][2], const int tot)
{
	int i;
	if (line_point_side_v2(uv[tot - 1], uv[0], pt) < 0.0f)
		return 0;

	for (i = 1; i < tot; i++) {
		if (line_point_side_v2(uv[i - 1], uv[i], pt) < 0.0f)
			return 0;

	}

	return 1;
}
static bool IsectPoly2Df_twoside(const float pt[2], float uv[][2], const int tot)
{
	int i;
	int side = (line_point_side_v2(uv[tot - 1], uv[0], pt) > 0.0f);

	for (i = 1; i < tot; i++) {
		if ((line_point_side_v2(uv[i - 1], uv[i], pt) > 0.0f) != side)
			return 0;

	}

	return 1;
}

/* One of the most important function for projection painting, since it selects the pixels to be added into each bucket.
 * initialize pixels from this face where it intersects with the bucket_index, optionally initialize pixels for removing seams */
static void project_paint_face_init(const ProjPaintState *ps, const int thread_index, const int bucket_index, const int face_index, const int image_index, rctf *bucket_bounds, const ImBuf *ibuf, const short clamp_u, const short clamp_v)
{
	/* Projection vars, to get the 3D locations into screen space  */
	MemArena *arena = ps->arena_mt[thread_index];
	LinkNode **bucketPixelNodes = ps->bucketRect + bucket_index;
	LinkNode *bucketFaceNodes = ps->bucketFaces[bucket_index];

	const MFace *mf = ps->dm_mface + face_index;
	const MTFace *tf = ps->dm_mtface + face_index;

	/* UV/pixel seeking data */
	int x; /* Image X-Pixel */
	int y; /* Image Y-Pixel */
	float mask;
	float uv[2]; /* Image floating point UV - same as x, y but from 0.0-1.0 */

	int side;
	float *v1coSS, *v2coSS, *v3coSS; /* vert co screen-space, these will be assigned to mf->v1,2,3 or mf->v1,3,4 */

	float *vCo[4]; /* vertex screenspace coords */

	float w[3], wco[3];

	float *uv1co, *uv2co, *uv3co; /* for convenience only, these will be assigned to tf->uv[0],1,2 or tf->uv[0],2,3 */
	float pixelScreenCo[4];
	bool do_3d_mapping = ps->brush->mtex.brush_map_mode == MTEX_MAP_MODE_3D;

	rcti bounds_px; /* ispace bounds */
	/* vars for getting uvspace bounds */

	float tf_uv_pxoffset[4][2]; /* bucket bounds in UV space so we can init pixels only for this face,  */
	float xhalfpx, yhalfpx;
	const float ibuf_xf = (float)ibuf->x, ibuf_yf = (float)ibuf->y;

	int has_x_isect = 0, has_isect = 0; /* for early loop exit */

	int i1, i2, i3;

	float uv_clip[8][2];
	int uv_clip_tot;
	const bool is_ortho = ps->is_ortho;
	const bool do_backfacecull = ps->do_backfacecull;
	const bool do_clip = ps->rv3d ? ps->rv3d->rflag & RV3D_CLIPPING : 0;

	vCo[0] = ps->dm_mvert[mf->v1].co;
	vCo[1] = ps->dm_mvert[mf->v2].co;
	vCo[2] = ps->dm_mvert[mf->v3].co;


	/* Use tf_uv_pxoffset instead of tf->uv so we can offset the UV half a pixel
	 * this is done so we can avoid offsetting all the pixels by 0.5 which causes
	 * problems when wrapping negative coords */
	xhalfpx = (0.5f + (PROJ_GEOM_TOLERANCE * (1.0f / 3.0f))) / ibuf_xf;
	yhalfpx = (0.5f + (PROJ_GEOM_TOLERANCE * (1.0f / 4.0f))) / ibuf_yf;

	/* Note about (PROJ_GEOM_TOLERANCE/x) above...
	 * Needed to add this offset since UV coords are often quads aligned to pixels.
	 * In this case pixels can be exactly between 2 triangles causing nasty
	 * artifacts.
	 *
	 * This workaround can be removed and painting will still work on most cases
	 * but since the first thing most people try is painting onto a quad- better make it work.
	 */

	tf_uv_pxoffset[0][0] = tf->uv[0][0] - xhalfpx;
	tf_uv_pxoffset[0][1] = tf->uv[0][1] - yhalfpx;

	tf_uv_pxoffset[1][0] = tf->uv[1][0] - xhalfpx;
	tf_uv_pxoffset[1][1] = tf->uv[1][1] - yhalfpx;

	tf_uv_pxoffset[2][0] = tf->uv[2][0] - xhalfpx;
	tf_uv_pxoffset[2][1] = tf->uv[2][1] - yhalfpx;

	if (mf->v4) {
		vCo[3] = ps->dm_mvert[mf->v4].co;

		tf_uv_pxoffset[3][0] = tf->uv[3][0] - xhalfpx;
		tf_uv_pxoffset[3][1] = tf->uv[3][1] - yhalfpx;
		side = 1;
	}
	else {
		side = 0;
	}

	do {
		if (side == 1) {
			i1 = 0; i2 = 2; i3 = 3;
		}
		else {
			i1 = 0; i2 = 1; i3 = 2;
		}

		uv1co = tf_uv_pxoffset[i1]; // was tf->uv[i1];
		uv2co = tf_uv_pxoffset[i2]; // was tf->uv[i2];
		uv3co = tf_uv_pxoffset[i3]; // was tf->uv[i3];

		v1coSS = ps->screenCoords[(*(&mf->v1 + i1))];
		v2coSS = ps->screenCoords[(*(&mf->v1 + i2))];
		v3coSS = ps->screenCoords[(*(&mf->v1 + i3))];

		/* This funtion gives is a concave polyline in UV space from the clipped quad and tri*/
		project_bucket_clip_face(
		        is_ortho, bucket_bounds,
		        v1coSS, v2coSS, v3coSS,
		        uv1co, uv2co, uv3co,
		        uv_clip, &uv_clip_tot
		        );

		/* sometimes this happens, better just allow for 8 intersectiosn even though there should be max 6 */
#if 0
		if (uv_clip_tot > 6) {
			printf("this should never happen! %d\n", uv_clip_tot);
		}
#endif

		if (pixel_bounds_array(uv_clip, &bounds_px, ibuf->x, ibuf->y, uv_clip_tot)) {

			if (clamp_u) {
				CLAMP(bounds_px.xmin, 0, ibuf->x);
				CLAMP(bounds_px.xmax, 0, ibuf->x);
			}

			if (clamp_v) {
				CLAMP(bounds_px.ymin, 0, ibuf->y);
				CLAMP(bounds_px.ymax, 0, ibuf->y);
			}

			/* clip face and */

			has_isect = 0;
			for (y = bounds_px.ymin; y < bounds_px.ymax; y++) {
				//uv[1] = (((float)y) + 0.5f) / (float)ibuf->y;
				uv[1] = (float)y / ibuf_yf; /* use pixel offset UV coords instead */

				has_x_isect = 0;
				for (x = bounds_px.xmin; x < bounds_px.xmax; x++) {
					//uv[0] = (((float)x) + 0.5f) / ibuf->x;
					uv[0] = (float)x / ibuf_xf; /* use pixel offset UV coords instead */

					/* Note about IsectPoly2Df_twoside, checking the face or uv flipping doesnt work,
					 * could check the poly direction but better to do this */
					if ((do_backfacecull == true  && IsectPoly2Df(uv, uv_clip, uv_clip_tot)) ||
					    (do_backfacecull == false && IsectPoly2Df_twoside(uv, uv_clip, uv_clip_tot)))
					{

						has_x_isect = has_isect = 1;

						if (is_ortho) screen_px_from_ortho(uv, v1coSS, v2coSS, v3coSS, uv1co, uv2co, uv3co, pixelScreenCo, w);
						else screen_px_from_persp(uv, v1coSS, v2coSS, v3coSS, uv1co, uv2co, uv3co, pixelScreenCo, w);

						/* a pity we need to get the worldspace pixel location here */
						if (do_clip || do_3d_mapping) {
							interp_v3_v3v3v3(wco, ps->dm_mvert[(*(&mf->v1 + i1))].co, ps->dm_mvert[(*(&mf->v1 + i2))].co, ps->dm_mvert[(*(&mf->v1 + i3))].co, w);
							if (do_clip && ED_view3d_clipping_test(ps->rv3d, wco, true)) {
								continue; /* Watch out that no code below this needs to run */
							}
						}

						/* Is this UV visible from the view? - raytrace */
						/* project_paint_PickFace is less complex, use for testing */
						//if (project_paint_PickFace(ps, pixelScreenCo, w, &side) == face_index) {
						if ((ps->do_occlude == false) ||
						    !project_bucket_point_occluded(ps, bucketFaceNodes, face_index, pixelScreenCo))
						{
							mask = project_paint_uvpixel_mask(ps, face_index, side, w);

							if (mask > 0.0f) {
								BLI_linklist_prepend_arena(
								        bucketPixelNodes,
								        project_paint_uvpixel_init(ps, arena, ibuf, x, y, mask, face_index,
								                                   image_index, pixelScreenCo, wco, side, w),
								        arena
								        );
							}
						}

					}
//#if 0
					else if (has_x_isect) {
						/* assuming the face is not a bow-tie - we know we cant intersect again on the X */
						break;
					}
//#endif
				}


#if 0           /* TODO - investigate why this dosnt work sometimes! it should! */
				/* no intersection for this entire row, after some intersection above means we can quit now */
				if (has_x_isect == 0 && has_isect) {
					break;
				}
#endif
			}
		}
	} while (side--);



#ifndef PROJ_DEBUG_NOSEAMBLEED
	if (ps->seam_bleed_px > 0.0f) {
		int face_seam_flag;

		if (ps->thread_tot > 1)
			BLI_lock_thread(LOCK_CUSTOM1);  /* Other threads could be modifying these vars */

		face_seam_flag = ps->faceSeamFlags[face_index];

		/* are any of our edges un-initialized? */
		if ((face_seam_flag & (PROJ_FACE_SEAM1 | PROJ_FACE_NOSEAM1)) == 0 ||
		    (face_seam_flag & (PROJ_FACE_SEAM2 | PROJ_FACE_NOSEAM2)) == 0 ||
		    (face_seam_flag & (PROJ_FACE_SEAM3 | PROJ_FACE_NOSEAM3)) == 0 ||
		    (face_seam_flag & (PROJ_FACE_SEAM4 | PROJ_FACE_NOSEAM4)) == 0)
		{
			project_face_seams_init(ps, face_index, mf->v4);
			face_seam_flag = ps->faceSeamFlags[face_index];
			//printf("seams - %d %d %d %d\n", flag&PROJ_FACE_SEAM1, flag&PROJ_FACE_SEAM2, flag&PROJ_FACE_SEAM3, flag&PROJ_FACE_SEAM4);
		}

		if ((face_seam_flag & (PROJ_FACE_SEAM1 | PROJ_FACE_SEAM2 | PROJ_FACE_SEAM3 | PROJ_FACE_SEAM4)) == 0) {

			if (ps->thread_tot > 1)
				BLI_unlock_thread(LOCK_CUSTOM1);  /* Other threads could be modifying these vars */

		}
		else {
			/* we have a seam - deal with it! */

			/* Now create new UV's for the seam face */
			float (*outset_uv)[2] = ps->faceSeamUVs[face_index];
			float insetCos[4][3]; /* inset face coords.  NOTE!!! ScreenSace for ortho, Worldspace in prespective view */

			float *vCoSS[4]; /* vertex screenspace coords */

			float bucket_clip_edges[2][2]; /* store the screenspace coords of the face, clipped by the bucket's screen aligned rectangle */
			float edge_verts_inset_clip[2][3];
			int fidx1, fidx2; /* face edge pairs - loop throuh these ((0,1), (1,2), (2,3), (3,0)) or ((0,1), (1,2), (2,0)) for a tri */

			float seam_subsection[4][2];
			float fac1, fac2, ftot;


			if (outset_uv[0][0] == FLT_MAX) /* first time initialize */
				uv_image_outset(tf_uv_pxoffset, outset_uv, ps->seam_bleed_px, ibuf->x, ibuf->y, mf->v4 != 0);

			/* ps->faceSeamUVs cant be modified when threading, now this is done we can unlock */
			if (ps->thread_tot > 1)
				BLI_unlock_thread(LOCK_CUSTOM1);  /* Other threads could be modifying these vars */

			vCoSS[0] = ps->screenCoords[mf->v1];
			vCoSS[1] = ps->screenCoords[mf->v2];
			vCoSS[2] = ps->screenCoords[mf->v3];
			if (mf->v4)
				vCoSS[3] = ps->screenCoords[mf->v4];

			/* PROJ_FACE_SCALE_SEAM must be slightly less then 1.0f */
			if (is_ortho) {
				if (mf->v4) scale_quad(insetCos, vCoSS, PROJ_FACE_SCALE_SEAM);
				else        scale_tri(insetCos, vCoSS, PROJ_FACE_SCALE_SEAM);
			}
			else {
				if (mf->v4) scale_quad(insetCos, vCo, PROJ_FACE_SCALE_SEAM);
				else        scale_tri(insetCos, vCo, PROJ_FACE_SCALE_SEAM);
			}

			side = 0; /* for triangles this wont need to change */

			for (fidx1 = 0; fidx1 < (mf->v4 ? 4 : 3); fidx1++) {
				if (mf->v4) fidx2 = (fidx1 == 3) ? 0 : fidx1 + 1;  /* next fidx in the face (0,1,2,3) -> (1,2,3,0) */
				else        fidx2 = (fidx1 == 2) ? 0 : fidx1 + 1;  /* next fidx in the face (0,1,2) -> (1,2,0) */

				if ((face_seam_flag & (1 << fidx1)) && /* 1<<fidx1 -> PROJ_FACE_SEAM# */
				    line_clip_rect2f(bucket_bounds, vCoSS[fidx1], vCoSS[fidx2], bucket_clip_edges[0], bucket_clip_edges[1]))
				{

					ftot = len_v2v2(vCoSS[fidx1], vCoSS[fidx2]); /* screenspace edge length */

					if (ftot > 0.0f) { /* avoid div by zero */
						if (mf->v4) {
							if (fidx1 == 2 || fidx2 == 2) side = 1;
							else side = 0;
						}

						fac1 = len_v2v2(vCoSS[fidx1], bucket_clip_edges[0]) / ftot;
						fac2 = len_v2v2(vCoSS[fidx1], bucket_clip_edges[1]) / ftot;

						interp_v2_v2v2(seam_subsection[0], tf_uv_pxoffset[fidx1], tf_uv_pxoffset[fidx2], fac1);
						interp_v2_v2v2(seam_subsection[1], tf_uv_pxoffset[fidx1], tf_uv_pxoffset[fidx2], fac2);

						interp_v2_v2v2(seam_subsection[2], outset_uv[fidx1], outset_uv[fidx2], fac2);
						interp_v2_v2v2(seam_subsection[3], outset_uv[fidx1], outset_uv[fidx2], fac1);

						/* if the bucket_clip_edges values Z values was kept we could avoid this
						 * Inset needs to be added so occlusion tests wont hit adjacent faces */
						interp_v3_v3v3(edge_verts_inset_clip[0], insetCos[fidx1], insetCos[fidx2], fac1);
						interp_v3_v3v3(edge_verts_inset_clip[1], insetCos[fidx1], insetCos[fidx2], fac2);


						if (pixel_bounds_uv(seam_subsection[0], seam_subsection[1], seam_subsection[2], seam_subsection[3], &bounds_px, ibuf->x, ibuf->y, true)) {
							/* bounds between the seam rect and the uvspace bucket pixels */

							has_isect = 0;
							for (y = bounds_px.ymin; y < bounds_px.ymax; y++) {
								// uv[1] = (((float)y) + 0.5f) / (float)ibuf->y;
								uv[1] = (float)y / ibuf_yf; /* use offset uvs instead */

								has_x_isect = 0;
								for (x = bounds_px.xmin; x < bounds_px.xmax; x++) {
									//uv[0] = (((float)x) + 0.5f) / (float)ibuf->x;
									uv[0] = (float)x / ibuf_xf; /* use offset uvs instead */

									/* test we're inside uvspace bucket and triangle bounds */
									if (isect_point_quad_v2(uv, seam_subsection[0], seam_subsection[1], seam_subsection[2], seam_subsection[3])) {
										float fac;

										/* We need to find the closest point along the face edge,
										 * getting the screen_px_from_*** wont work because our actual location
										 * is not relevant, since we are outside the face, Use VecLerpf to find
										 * our location on the side of the face's UV */
#if 0
										if (is_ortho) screen_px_from_ortho(ps, uv, v1co, v2co, v3co, uv1co, uv2co, uv3co, pixelScreenCo);
										else          screen_px_from_persp(ps, uv, v1co, v2co, v3co, uv1co, uv2co, uv3co, pixelScreenCo);
#endif

										/* Since this is a seam we need to work out where on the line this pixel is */
										//fac = line_point_factor_v2(uv, uv_seam_quad[0], uv_seam_quad[1]);

										fac = line_point_factor_v2(uv, seam_subsection[0], seam_subsection[1]);
										if      (fac < 0.0f) { copy_v3_v3(pixelScreenCo, edge_verts_inset_clip[0]); }
										else if (fac > 1.0f) { copy_v3_v3(pixelScreenCo, edge_verts_inset_clip[1]); }
										else                 { interp_v3_v3v3(pixelScreenCo, edge_verts_inset_clip[0], edge_verts_inset_clip[1], fac); }

										if (!is_ortho) {
											pixelScreenCo[3] = 1.0f;
											mul_m4_v4((float(*)[4])ps->projectMat, pixelScreenCo); /* cast because of const */
											pixelScreenCo[0] = (float)(ps->winx * 0.5f) + (ps->winx * 0.5f) * pixelScreenCo[0] / pixelScreenCo[3];
											pixelScreenCo[1] = (float)(ps->winy * 0.5f) + (ps->winy * 0.5f) * pixelScreenCo[1] / pixelScreenCo[3];
											pixelScreenCo[2] = pixelScreenCo[2] / pixelScreenCo[3]; /* Use the depth for bucket point occlusion */
										}

										if ((ps->do_occlude == false) ||
										    !project_bucket_point_occluded(ps, bucketFaceNodes, face_index, pixelScreenCo))
										{
											/* Only bother calculating the weights if we intersect */
											if (ps->do_mask_normal || ps->dm_mtface_clone) {
#if 1
												/* get the UV on the line since we want to copy the pixels from there for bleeding */
												float uv_close[2];
												float uv_fac = closest_to_line_v2(uv_close, uv, tf_uv_pxoffset[fidx1], tf_uv_pxoffset[fidx2]);
												if      (uv_fac < 0.0f) copy_v2_v2(uv_close, tf_uv_pxoffset[fidx1]);
												else if (uv_fac > 1.0f) copy_v2_v2(uv_close, tf_uv_pxoffset[fidx2]);

												if (side) {
													barycentric_weights_v2(tf_uv_pxoffset[0], tf_uv_pxoffset[2], tf_uv_pxoffset[3], uv_close, w);
												}
												else {
													barycentric_weights_v2(tf_uv_pxoffset[0], tf_uv_pxoffset[1], tf_uv_pxoffset[2], uv_close, w);
												}
#else											/* this is buggy with quads, don't use for now */

												/* Cheat, we know where we are along the edge so work out the weights from that */
												uv_fac = fac1 + (uv_fac * (fac2 - fac1));

												w[0] = w[1] = w[2] = 0.0;
												if (side) {
													w[fidx1 ? fidx1 - 1 : 0] = 1.0f - uv_fac;
													w[fidx2 ? fidx2 - 1 : 0] = uv_fac;
												}
												else {
													w[fidx1] = 1.0f - uv_fac;
													w[fidx2] = uv_fac;
												}
#endif
											}

											/* a pity we need to get the worldspace pixel location here */
											if (do_clip || do_3d_mapping) {
												if (side) interp_v3_v3v3v3(wco, ps->dm_mvert[mf->v1].co, ps->dm_mvert[mf->v3].co, ps->dm_mvert[mf->v4].co, w);
												else      interp_v3_v3v3v3(wco, ps->dm_mvert[mf->v1].co, ps->dm_mvert[mf->v2].co, ps->dm_mvert[mf->v3].co, w);

												if (do_clip && ED_view3d_clipping_test(ps->rv3d, wco, true)) {
													continue; /* Watch out that no code below this needs to run */
												}
											}

											mask = project_paint_uvpixel_mask(ps, face_index, side, w);

											if (mask > 0.0f) {
												BLI_linklist_prepend_arena(
												        bucketPixelNodes,
												        project_paint_uvpixel_init(ps, arena, ibuf, x, y, mask, face_index, image_index, pixelScreenCo, wco, side, w),
												        arena
												        );
											}

										}
									}
									else if (has_x_isect) {
										/* assuming the face is not a bow-tie - we know we cant intersect again on the X */
										break;
									}
								}

#if 0                           /* TODO - investigate why this dosnt work sometimes! it should! */
								/* no intersection for this entire row, after some intersection above means we can quit now */
								if (has_x_isect == 0 && has_isect) {
									break;
								}
#endif
							}
						}
					}
				}
			}
		}
	}
#endif // PROJ_DEBUG_NOSEAMBLEED
}


/* takes floating point screenspace min/max and returns int min/max to be used as indices for ps->bucketRect, ps->bucketFlags */
static void project_paint_bucket_bounds(const ProjPaintState *ps, const float min[2], const float max[2], int bucketMin[2], int bucketMax[2])
{
	/* divide by bucketWidth & bucketHeight so the bounds are offset in bucket grid units */
	/* XXX: the offset of 0.5 is always truncated to zero and the offset of 1.5f is always truncated to 1, is this really correct?? - jwilkins */
	bucketMin[0] = (int)((int)(((float)(min[0] - ps->screenMin[0]) / ps->screen_width) * ps->buckets_x) + 0.5f); /* these offsets of 0.5 and 1.5 seem odd but they are correct */
	bucketMin[1] = (int)((int)(((float)(min[1] - ps->screenMin[1]) / ps->screen_height) * ps->buckets_y) + 0.5f);

	bucketMax[0] = (int)((int)(((float)(max[0] - ps->screenMin[0]) / ps->screen_width) * ps->buckets_x) + 1.5f);
	bucketMax[1] = (int)((int)(((float)(max[1] - ps->screenMin[1]) / ps->screen_height) * ps->buckets_y) + 1.5f);

	/* in case the rect is outside the mesh 2d bounds */
	CLAMP(bucketMin[0], 0, ps->buckets_x);
	CLAMP(bucketMin[1], 0, ps->buckets_y);

	CLAMP(bucketMax[0], 0, ps->buckets_x);
	CLAMP(bucketMax[1], 0, ps->buckets_y);
}

/* set bucket_bounds to a screen space-aligned floating point bound-box */
static void project_bucket_bounds(const ProjPaintState *ps, const int bucket_x, const int bucket_y, rctf *bucket_bounds)
{
	bucket_bounds->xmin = ps->screenMin[0] + ((bucket_x) * (ps->screen_width / ps->buckets_x));     /* left */
	bucket_bounds->xmax = ps->screenMin[0] + ((bucket_x + 1) * (ps->screen_width / ps->buckets_x)); /* right */

	bucket_bounds->ymin = ps->screenMin[1] + ((bucket_y) * (ps->screen_height / ps->buckets_y));      /* bottom */
	bucket_bounds->ymax = ps->screenMin[1] + ((bucket_y + 1) * (ps->screen_height  / ps->buckets_y)); /* top */
}

/* Fill this bucket with pixels from the faces that intersect it.
 *
 * have bucket_bounds as an argument so we don't need to give bucket_x/y the rect function needs */
static void project_bucket_init(const ProjPaintState *ps, const int thread_index, const int bucket_index, rctf *bucket_bounds)
{
	LinkNode *node;
	int face_index, image_index = 0;
	ImBuf *ibuf = NULL;
	Image *tpage_last = NULL, *tpage;
	Image *ima = NULL;

	if (ps->image_tot == 1) {
		/* Simple loop, no context switching */
		ibuf = ps->projImages[0].ibuf;
		ima = ps->projImages[0].ima;

		for (node = ps->bucketFaces[bucket_index]; node; node = node->next) {
			project_paint_face_init(ps, thread_index, bucket_index, GET_INT_FROM_POINTER(node->link), 0, bucket_bounds, ibuf, ima->tpageflag & IMA_CLAMP_U, ima->tpageflag & IMA_CLAMP_V);
		}
	}
	else {

		/* More complicated loop, switch between images */
		for (node = ps->bucketFaces[bucket_index]; node; node = node->next) {
			face_index = GET_INT_FROM_POINTER(node->link);

			/* Image context switching */
			tpage = project_paint_face_image(ps, ps->dm_mtface, face_index);
			if (tpage_last != tpage) {
				tpage_last = tpage;

				for (image_index = 0; image_index < ps->image_tot; image_index++) {
					if (ps->projImages[image_index].ima == tpage_last) {
						ibuf = ps->projImages[image_index].ibuf;
						ima = ps->projImages[image_index].ima;
						break;
					}
				}
			}
			/* context switching done */

			project_paint_face_init(ps, thread_index, bucket_index, face_index, image_index, bucket_bounds, ibuf, ima->tpageflag & IMA_CLAMP_U, ima->tpageflag & IMA_CLAMP_V);
		}
	}

	ps->bucketFlags[bucket_index] |= PROJ_BUCKET_INIT;
}


/* We want to know if a bucket and a face overlap in screen-space
 *
 * Note, if this ever returns false positives its not that bad, since a face in the bounding area will have its pixels
 * calculated when it might not be needed later, (at the moment at least)
 * obviously it shouldn't have bugs though */

static bool project_bucket_face_isect(ProjPaintState *ps, int bucket_x, int bucket_y, const MFace *mf)
{
	/* TODO - replace this with a tricker method that uses sideofline for all screenCoords's edges against the closest bucket corner */
	rctf bucket_bounds;
	float p1[2], p2[2], p3[2], p4[2];
	const float *v, *v1, *v2, *v3, *v4 = NULL;
	int fidx;

	project_bucket_bounds(ps, bucket_x, bucket_y, &bucket_bounds);

	/* Is one of the faces verts in the bucket bounds? */

	fidx = mf->v4 ? 3 : 2;
	do {
		v = ps->screenCoords[(*(&mf->v1 + fidx))];
		if (BLI_rctf_isect_pt_v(&bucket_bounds, v)) {
			return 1;
		}
	} while (fidx--);

	v1 = ps->screenCoords[mf->v1];
	v2 = ps->screenCoords[mf->v2];
	v3 = ps->screenCoords[mf->v3];
	if (mf->v4) {
		v4 = ps->screenCoords[mf->v4];
	}

	p1[0] = bucket_bounds.xmin; p1[1] = bucket_bounds.ymin;
	p2[0] = bucket_bounds.xmin; p2[1] = bucket_bounds.ymax;
	p3[0] = bucket_bounds.xmax; p3[1] = bucket_bounds.ymax;
	p4[0] = bucket_bounds.xmax; p4[1] = bucket_bounds.ymin;

	if (mf->v4) {
		if (isect_point_quad_v2(p1, v1, v2, v3, v4) ||
		    isect_point_quad_v2(p2, v1, v2, v3, v4) ||
		    isect_point_quad_v2(p3, v1, v2, v3, v4) ||
		    isect_point_quad_v2(p4, v1, v2, v3, v4) ||

		    /* we can avoid testing v3,v1 because another intersection MUST exist if this intersects */
		    (isect_line_line_v2(p1, p2, v1, v2) || isect_line_line_v2(p1, p2, v2, v3) || isect_line_line_v2(p1, p2, v3, v4)) ||
		    (isect_line_line_v2(p2, p3, v1, v2) || isect_line_line_v2(p2, p3, v2, v3) || isect_line_line_v2(p2, p3, v3, v4)) ||
		    (isect_line_line_v2(p3, p4, v1, v2) || isect_line_line_v2(p3, p4, v2, v3) || isect_line_line_v2(p3, p4, v3, v4)) ||
		    (isect_line_line_v2(p4, p1, v1, v2) || isect_line_line_v2(p4, p1, v2, v3) || isect_line_line_v2(p4, p1, v3, v4)))
		{
			return 1;
		}
	}
	else {
		if (isect_point_tri_v2(p1, v1, v2, v3) ||
		    isect_point_tri_v2(p2, v1, v2, v3) ||
		    isect_point_tri_v2(p3, v1, v2, v3) ||
		    isect_point_tri_v2(p4, v1, v2, v3) ||
		    /* we can avoid testing v3,v1 because another intersection MUST exist if this intersects */
		    (isect_line_line_v2(p1, p2, v1, v2) || isect_line_line_v2(p1, p2, v2, v3)) ||
		    (isect_line_line_v2(p2, p3, v1, v2) || isect_line_line_v2(p2, p3, v2, v3)) ||
		    (isect_line_line_v2(p3, p4, v1, v2) || isect_line_line_v2(p3, p4, v2, v3)) ||
		    (isect_line_line_v2(p4, p1, v1, v2) || isect_line_line_v2(p4, p1, v2, v3)))
		{
			return 1;
		}
	}

	return 0;
}

/* Add faces to the bucket but don't initialize its pixels
 * TODO - when painting occluded, sort the faces on their min-Z and only add faces that faces that are not occluded */
static void project_paint_delayed_face_init(ProjPaintState *ps, const MFace *mf, const int face_index)
{
	float min[2], max[2], *vCoSS;
	int bucketMin[2], bucketMax[2]; /* for  ps->bucketRect indexing */
	int fidx, bucket_x, bucket_y;
	int has_x_isect = -1, has_isect = 0; /* for early loop exit */
	MemArena *arena = ps->arena_mt[0]; /* just use the first thread arena since threading has not started yet */

	INIT_MINMAX2(min, max);

	fidx = mf->v4 ? 3 : 2;
	do {
		vCoSS = ps->screenCoords[*(&mf->v1 + fidx)];
		minmax_v2v2_v2(min, max, vCoSS);
	} while (fidx--);

	project_paint_bucket_bounds(ps, min, max, bucketMin, bucketMax);

	for (bucket_y = bucketMin[1]; bucket_y < bucketMax[1]; bucket_y++) {
		has_x_isect = 0;
		for (bucket_x = bucketMin[0]; bucket_x < bucketMax[0]; bucket_x++) {
			if (project_bucket_face_isect(ps, bucket_x, bucket_y, mf)) {
				int bucket_index = bucket_x + (bucket_y * ps->buckets_x);
				BLI_linklist_prepend_arena(
				        &ps->bucketFaces[bucket_index],
				        SET_INT_IN_POINTER(face_index), /* cast to a pointer to shut up the compiler */
				        arena
				        );

				has_x_isect = has_isect = 1;
			}
			else if (has_x_isect) {
				/* assuming the face is not a bow-tie - we know we cant intersect again on the X */
				break;
			}
		}

		/* no intersection for this entire row, after some intersection above means we can quit now */
		if (has_x_isect == 0 && has_isect) {
			break;
		}
	}

#ifndef PROJ_DEBUG_NOSEAMBLEED
	if (ps->seam_bleed_px > 0.0f) {
		if (!mf->v4) {
			ps->faceSeamFlags[face_index] |= PROJ_FACE_NOSEAM4; /* so this wont show up as an untagged edge */
		}
		**ps->faceSeamUVs[face_index] = FLT_MAX; /* set as uninitialized */
	}
#endif
}

/* run once per stroke before projection painting */
static void project_paint_begin(ProjPaintState *ps)
{
	/* Viewport vars */
	float mat[3][3];

	float no[3];

	float *projScreenCo; /* Note, we could have 4D vectors are only needed for */
	float projMargin;

	/* Image Vars - keep track of images we have used */
	LinkNode *image_LinkList = NULL;
	LinkNode *node;

	ProjPaintImage *projIma;
	Image *tpage_last = NULL, *tpage;

	/* Face vars */
	MPoly *mpoly_orig;
	MFace *mf;
	MTFace *tf;

	int a, i; /* generic looping vars */
	int image_index = -1, face_index;

	/* double lookup */
	const int *index_mf_to_mpoly = NULL;
	const int *index_mp_to_orig  = NULL;

	MVert *mv;

	MemArena *arena; /* at the moment this is just ps->arena_mt[0], but use this to show were not multithreading */

	const int diameter = 2 * BKE_brush_size_get(ps->scene, ps->brush);

	bool reset_threads = false;

	/* ---- end defines ---- */

	if (ps->source == PROJ_SRC_VIEW)
		ED_view3d_clipping_local(ps->rv3d, ps->ob->obmat);  /* faster clipping lookups */

	ps->do_face_sel = ((((Mesh *)ps->ob->data)->editflag & ME_EDIT_PAINT_FACE_SEL) != 0);

	/* paint onto the derived mesh */

	/* Workaround for subsurf selection, try the display mesh first */
	if (ps->source == PROJ_SRC_IMAGE_CAM) {
		/* using render mesh, assume only camera was rendered from */
		ps->dm = mesh_create_derived_render(ps->scene, ps->ob, ps->scene->customdata_mask | CD_MASK_MTFACE);
		ps->dm_release = true;
	}
	else if (ps->ob->derivedFinal &&
	         CustomData_has_layer(&ps->ob->derivedFinal->faceData, CD_MTFACE) &&
	         (ps->do_face_sel == false || CustomData_has_layer(&ps->ob->derivedFinal->polyData, CD_ORIGINDEX)))
	{
		ps->dm = ps->ob->derivedFinal;
		ps->dm_release = false;
	}
	else {
		ps->dm = mesh_get_derived_final(
		             ps->scene, ps->ob,
		             ps->scene->customdata_mask | CD_MASK_MTFACE | (ps->do_face_sel ? CD_ORIGINDEX : 0));
		ps->dm_release = true;
	}

	if (!CustomData_has_layer(&ps->dm->faceData, CD_MTFACE) ) {

		if (ps->dm_release)
			ps->dm->release(ps->dm);

		ps->dm = NULL;
		return;
	}

	ps->dm_mvert = ps->dm->getVertArray(ps->dm);
	ps->dm_mface = ps->dm->getTessFaceArray(ps->dm);
	ps->dm_mtface = ps->dm->getTessFaceDataArray(ps->dm, CD_MTFACE);

	ps->dm_totvert = ps->dm->getNumVerts(ps->dm);
	ps->dm_totface = ps->dm->getNumTessFaces(ps->dm);

	if (ps->do_face_sel) {
		index_mf_to_mpoly = ps->dm->getTessFaceDataArray(ps->dm, CD_ORIGINDEX);
		index_mp_to_orig  = ps->dm->getPolyDataArray(ps->dm, CD_ORIGINDEX);
		if (index_mf_to_mpoly == NULL) {
			index_mp_to_orig = NULL;
		}
		else {
			mpoly_orig = ((Mesh *)ps->ob->data)->mpoly;
		}
	}
	else {
		mpoly_orig = NULL;
	}

	/* use clone mtface? */


	/* Note, use the original mesh for getting the clone and mask layer index
	 * this avoids re-generating the derived mesh just to get the new index */
	if (ps->do_layer_clone) {
		//int layer_num = CustomData_get_clone_layer(&ps->dm->faceData, CD_MTFACE);
		int layer_num = CustomData_get_clone_layer(&((Mesh *)ps->ob->data)->pdata, CD_MTEXPOLY);
		if (layer_num != -1)
			ps->dm_mtface_clone = CustomData_get_layer_n(&ps->dm->faceData, CD_MTFACE, layer_num);

		if (ps->dm_mtface_clone == NULL || ps->dm_mtface_clone == ps->dm_mtface) {
			ps->do_layer_clone = false;
			ps->dm_mtface_clone = NULL;
		}
	}

	if (ps->do_layer_stencil) {
		//int layer_num = CustomData_get_stencil_layer(&ps->dm->faceData, CD_MTFACE);
		int layer_num = CustomData_get_stencil_layer(&((Mesh *)ps->ob->data)->pdata, CD_MTEXPOLY);
		if (layer_num != -1)
			ps->dm_mtface_stencil = CustomData_get_layer_n(&ps->dm->faceData, CD_MTFACE, layer_num);

		if (ps->dm_mtface_stencil == NULL || ps->dm_mtface_stencil == ps->dm_mtface) {
			ps->do_layer_stencil = false;
			ps->dm_mtface_stencil = NULL;
		}
	}

	/* when using subsurf or multires, mface arrays are thrown away, we need to keep a copy */
	if (ps->dm->type != DM_TYPE_CDDM) {
		ps->dm_mvert = MEM_dupallocN(ps->dm_mvert);
		ps->dm_mface = MEM_dupallocN(ps->dm_mface);
		/* looks like these are ok for now.*/
#if 0
		ps->dm_mtface = MEM_dupallocN(ps->dm_mtface);
		ps->dm_mtface_clone = MEM_dupallocN(ps->dm_mtface_clone);
		ps->dm_mtface_stencil = MEM_dupallocN(ps->dm_mtface_stencil);
#endif
	}

	ps->viewDir[0] = 0.0f;
	ps->viewDir[1] = 0.0f;
	ps->viewDir[2] = 1.0f;

	{
		float viewmat[4][4];
		float viewinv[4][4];

		invert_m4_m4(ps->ob->imat, ps->ob->obmat);

		if (ps->source == PROJ_SRC_VIEW) {
			/* normal drawing */
			ps->winx = ps->ar->winx;
			ps->winy = ps->ar->winy;

			copy_m4_m4(viewmat, ps->rv3d->viewmat);
			copy_m4_m4(viewinv, ps->rv3d->viewinv);

			ED_view3d_ob_project_mat_get(ps->rv3d, ps->ob, ps->projectMat);

			ps->is_ortho = ED_view3d_clip_range_get(ps->v3d, ps->rv3d, &ps->clipsta, &ps->clipend, true);
		}
		else {
			/* re-projection */
			float winmat[4][4];
			float vmat[4][4];

			ps->winx = ps->reproject_ibuf->x;
			ps->winy = ps->reproject_ibuf->y;

			if (ps->source == PROJ_SRC_IMAGE_VIEW) {
				/* image stores camera data, tricky */
				IDProperty *idgroup = IDP_GetProperties(&ps->reproject_image->id, 0);
				IDProperty *view_data = IDP_GetPropertyFromGroup(idgroup, PROJ_VIEW_DATA_ID);

				const float *array = (float *)IDP_Array(view_data);

				/* use image array, written when creating image */
				memcpy(winmat, array, sizeof(winmat)); array += sizeof(winmat) / sizeof(float);
				memcpy(viewmat, array, sizeof(viewmat)); array += sizeof(viewmat) / sizeof(float);
				ps->clipsta = array[0];
				ps->clipend = array[1];
				ps->is_ortho = array[2] ? 1 : 0;

				invert_m4_m4(viewinv, viewmat);
			}
			else if (ps->source == PROJ_SRC_IMAGE_CAM) {
				Object *cam_ob = ps->scene->camera;
				CameraParams params;

				/* viewmat & viewinv */
				copy_m4_m4(viewinv, cam_ob->obmat);
				normalize_m4(viewinv);
				invert_m4_m4(viewmat, viewinv);

				/* window matrix, clipping and ortho */
				BKE_camera_params_init(&params);
				BKE_camera_params_from_object(&params, cam_ob);
				BKE_camera_params_compute_viewplane(&params, ps->winx, ps->winy, 1.0f, 1.0f);
				BKE_camera_params_compute_matrix(&params);

				copy_m4_m4(winmat, params.winmat);
				ps->clipsta = params.clipsta;
				ps->clipend = params.clipend;
				ps->is_ortho = params.is_ortho;
			}

			/* same as #ED_view3d_ob_project_mat_get */
			mul_m4_m4m4(vmat, viewmat, ps->ob->obmat);
			mul_m4_m4m4(ps->projectMat, winmat, vmat);
		}


		/* viewDir - object relative */
		invert_m4_m4(ps->ob->imat, ps->ob->obmat);
		copy_m3_m4(mat, viewinv);
		mul_m3_v3(mat, ps->viewDir);
		copy_m3_m4(mat, ps->ob->imat);
		mul_m3_v3(mat, ps->viewDir);
		normalize_v3(ps->viewDir);

		/* viewPos - object relative */
		copy_v3_v3(ps->viewPos, viewinv[3]);
		copy_m3_m4(mat, ps->ob->imat);
		mul_m3_v3(mat, ps->viewPos);
		add_v3_v3(ps->viewPos, ps->ob->imat[3]);
	}

	/* calculate vert screen coords
	 * run this early so we can calculate the x/y resolution of our bucket rect */
	INIT_MINMAX2(ps->screenMin, ps->screenMax);

	ps->screenCoords = MEM_mallocN(sizeof(float) * ps->dm_totvert * 4, "ProjectPaint ScreenVerts");
	projScreenCo = *ps->screenCoords;

	if (ps->is_ortho) {
		for (a = 0, mv = ps->dm_mvert; a < ps->dm_totvert; a++, mv++, projScreenCo += 4) {
			mul_v3_m4v3(projScreenCo, ps->projectMat, mv->co);

			/* screen space, not clamped */
			projScreenCo[0] = (float)(ps->winx * 0.5f) + (ps->winx * 0.5f) * projScreenCo[0];
			projScreenCo[1] = (float)(ps->winy * 0.5f) + (ps->winy * 0.5f) * projScreenCo[1];
			minmax_v2v2_v2(ps->screenMin, ps->screenMax, projScreenCo);
		}
	}
	else {
		for (a = 0, mv = ps->dm_mvert; a < ps->dm_totvert; a++, mv++, projScreenCo += 4) {
			copy_v3_v3(projScreenCo, mv->co);
			projScreenCo[3] = 1.0f;

			mul_m4_v4(ps->projectMat, projScreenCo);

			if (projScreenCo[3] > ps->clipsta) {
				/* screen space, not clamped */
				projScreenCo[0] = (float)(ps->winx * 0.5f) + (ps->winx * 0.5f) * projScreenCo[0] / projScreenCo[3];
				projScreenCo[1] = (float)(ps->winy * 0.5f) + (ps->winy * 0.5f) * projScreenCo[1] / projScreenCo[3];
				projScreenCo[2] = projScreenCo[2] / projScreenCo[3]; /* Use the depth for bucket point occlusion */
				minmax_v2v2_v2(ps->screenMin, ps->screenMax, projScreenCo);
			}
			else {
				/* TODO - deal with cases where 1 side of a face goes behind the view ?
				 *
				 * After some research this is actually very tricky, only option is to
				 * clip the derived mesh before painting, which is a Pain */
				projScreenCo[0] = FLT_MAX;
			}
		}
	}

	/* If this border is not added we get artifacts for faces that
	 * have a parallel edge and at the bounds of the the 2D projected verts eg
	 * - a single screen aligned quad */
	projMargin = (ps->screenMax[0] - ps->screenMin[0]) * 0.000001f;
	ps->screenMax[0] += projMargin;
	ps->screenMin[0] -= projMargin;
	projMargin = (ps->screenMax[1] - ps->screenMin[1]) * 0.000001f;
	ps->screenMax[1] += projMargin;
	ps->screenMin[1] -= projMargin;

	if (ps->source == PROJ_SRC_VIEW) {
#ifdef PROJ_DEBUG_WINCLIP
		CLAMP(ps->screenMin[0], (float)(-diameter), (float)(ps->winx + diameter));
		CLAMP(ps->screenMax[0], (float)(-diameter), (float)(ps->winx + diameter));

		CLAMP(ps->screenMin[1], (float)(-diameter), (float)(ps->winy + diameter));
		CLAMP(ps->screenMax[1], (float)(-diameter), (float)(ps->winy + diameter));
#endif
	}
	else { /* re-projection, use bounds */
		ps->screenMin[0] = 0;
		ps->screenMax[0] = (float)(ps->winx);

		ps->screenMin[1] = 0;
		ps->screenMax[1] = (float)(ps->winy);
	}

	/* only for convenience */
	ps->screen_width  = ps->screenMax[0] - ps->screenMin[0];
	ps->screen_height = ps->screenMax[1] - ps->screenMin[1];

	ps->buckets_x = (int)(ps->screen_width / (((float)diameter) / PROJ_BUCKET_BRUSH_DIV));
	ps->buckets_y = (int)(ps->screen_height / (((float)diameter) / PROJ_BUCKET_BRUSH_DIV));

	/* printf("\tscreenspace bucket division x:%d y:%d\n", ps->buckets_x, ps->buckets_y); */

	if (ps->buckets_x > PROJ_BUCKET_RECT_MAX || ps->buckets_y > PROJ_BUCKET_RECT_MAX) {
		reset_threads = true;
	}

	/* really high values could cause problems since it has to allocate a few
	 * (ps->buckets_x*ps->buckets_y) sized arrays  */
	CLAMP(ps->buckets_x, PROJ_BUCKET_RECT_MIN, PROJ_BUCKET_RECT_MAX);
	CLAMP(ps->buckets_y, PROJ_BUCKET_RECT_MIN, PROJ_BUCKET_RECT_MAX);

	ps->bucketRect = (LinkNode **)MEM_callocN(sizeof(LinkNode *) * ps->buckets_x * ps->buckets_y, "paint-bucketRect");
	ps->bucketFaces = (LinkNode **)MEM_callocN(sizeof(LinkNode *) * ps->buckets_x * ps->buckets_y, "paint-bucketFaces");

	ps->bucketFlags = (unsigned char *)MEM_callocN(sizeof(char) * ps->buckets_x * ps->buckets_y, "paint-bucketFaces");
#ifndef PROJ_DEBUG_NOSEAMBLEED
	if (ps->seam_bleed_px > 0.0f) {
		ps->vertFaces = (LinkNode **)MEM_callocN(sizeof(LinkNode *) * ps->dm_totvert, "paint-vertFaces");
		ps->faceSeamFlags = (char *)MEM_callocN(sizeof(char) * ps->dm_totface, "paint-faceSeamFlags");
		ps->faceWindingFlags = (char *)MEM_callocN(sizeof(char) * ps->dm_totface, "paint-faceWindindFlags");
		ps->faceSeamUVs = MEM_mallocN(sizeof(float) * ps->dm_totface * 8, "paint-faceSeamUVs");
	}
#endif

	/* Thread stuff
	 *
	 * very small brushes run a lot slower multithreaded since the advantage with
	 * threads is being able to fill in multiple buckets at once.
	 * Only use threads for bigger brushes. */

	ps->thread_tot = BKE_scene_num_threads(ps->scene);

	/* workaround for #35057, disable threading if diameter is less than is possible for
	 * optimum bucket number generation */
	if (reset_threads)
		ps->thread_tot = 1;

	for (a = 0; a < ps->thread_tot; a++) {
		ps->arena_mt[a] = BLI_memarena_new(MEM_SIZE_OPTIMAL(1 << 16), "project paint arena");
	}

	arena = ps->arena_mt[0];

	if (ps->do_backfacecull && ps->do_mask_normal) {
		float viewDirPersp[3];

		ps->vertFlags = MEM_callocN(sizeof(char) * ps->dm_totvert, "paint-vertFlags");

		for (a = 0, mv = ps->dm_mvert; a < ps->dm_totvert; a++, mv++) {
			normal_short_to_float_v3(no, mv->no);

			if (ps->is_ortho) {
				if (angle_normalized_v3v3(ps->viewDir, no) >= ps->normal_angle) { /* 1 vert of this face is towards us */
					ps->vertFlags[a] |= PROJ_VERT_CULL;
				}
			}
			else {
				sub_v3_v3v3(viewDirPersp, ps->viewPos, mv->co);
				normalize_v3(viewDirPersp);
				if (angle_normalized_v3v3(viewDirPersp, no) >= ps->normal_angle) { /* 1 vert of this face is towards us */
					ps->vertFlags[a] |= PROJ_VERT_CULL;
				}
			}
		}
	}

	for (face_index = 0, tf = ps->dm_mtface, mf = ps->dm_mface; face_index < ps->dm_totface; mf++, tf++, face_index++) {
		bool is_face_sel;

#ifndef PROJ_DEBUG_NOSEAMBLEED
		/* add face user if we have bleed enabled, set the UV seam flags later */
		/* annoying but we need to add all faces even ones we never use elsewhere */
		if (ps->seam_bleed_px > 0.0f) {
			BLI_linklist_prepend_arena(&ps->vertFaces[mf->v1], SET_INT_IN_POINTER(face_index), arena);
			BLI_linklist_prepend_arena(&ps->vertFaces[mf->v2], SET_INT_IN_POINTER(face_index), arena);
			BLI_linklist_prepend_arena(&ps->vertFaces[mf->v3], SET_INT_IN_POINTER(face_index), arena);
			if (mf->v4) {
				BLI_linklist_prepend_arena(&ps->vertFaces[mf->v4], SET_INT_IN_POINTER(face_index), arena);
			}
		}
#endif

		if (ps->do_face_sel) {
			int orig_index;
			if (index_mp_to_orig && ((orig_index = DM_origindex_mface_mpoly(index_mf_to_mpoly, index_mp_to_orig,
			                                                                face_index))) != ORIGINDEX_NONE)
			{
				MPoly *mp = &mpoly_orig[orig_index];
				is_face_sel = ((mp->flag & ME_FACE_SEL) != 0);
			}
			else {
				is_face_sel = ((mf->flag & ME_FACE_SEL) != 0);
			}
		}
		else {
			is_face_sel = true;
		}

		if (is_face_sel && (tpage = project_paint_face_image(ps, ps->dm_mtface, face_index))) {
			const float *v1coSS, *v2coSS, *v3coSS, *v4coSS = NULL;

			v1coSS = ps->screenCoords[mf->v1];
			v2coSS = ps->screenCoords[mf->v2];
			v3coSS = ps->screenCoords[mf->v3];
			if (mf->v4) {
				v4coSS = ps->screenCoords[mf->v4];
			}


			if (!ps->is_ortho) {
				if (v1coSS[0] == FLT_MAX ||
				    v2coSS[0] == FLT_MAX ||
				    v3coSS[0] == FLT_MAX ||
				    (mf->v4 && v4coSS[0] == FLT_MAX))
				{
					continue;
				}
			}

#ifdef PROJ_DEBUG_WINCLIP
			/* ignore faces outside the view */
			if (
			    (v1coSS[0] < ps->screenMin[0] &&
			     v2coSS[0] < ps->screenMin[0] &&
			     v3coSS[0] < ps->screenMin[0] &&
			     (mf->v4 && v4coSS[0] < ps->screenMin[0])) ||

			    (v1coSS[0] > ps->screenMax[0] &&
			     v2coSS[0] > ps->screenMax[0] &&
			     v3coSS[0] > ps->screenMax[0] &&
			     (mf->v4 && v4coSS[0] > ps->screenMax[0])) ||

			    (v1coSS[1] < ps->screenMin[1] &&
			     v2coSS[1] < ps->screenMin[1] &&
			     v3coSS[1] < ps->screenMin[1] &&
			     (mf->v4 && v4coSS[1] < ps->screenMin[1])) ||

			    (v1coSS[1] > ps->screenMax[1] &&
			     v2coSS[1] > ps->screenMax[1] &&
			     v3coSS[1] > ps->screenMax[1] &&
			     (mf->v4 && v4coSS[1] > ps->screenMax[1]))
			    )
			{
				continue;
			}

#endif //PROJ_DEBUG_WINCLIP


			if (ps->do_backfacecull) {
				if (ps->do_mask_normal) {
					/* Since we are interpolating the normals of faces, we want to make
					 * sure all the verts are pointing away from the view,
					 * not just the face */
					if ((ps->vertFlags[mf->v1] & PROJ_VERT_CULL) &&
					    (ps->vertFlags[mf->v2] & PROJ_VERT_CULL) &&
					    (ps->vertFlags[mf->v3] & PROJ_VERT_CULL) &&
					    (mf->v4 == 0 || ps->vertFlags[mf->v4] & PROJ_VERT_CULL)
					    )
					{
						continue;
					}
				}
				else {
					if (line_point_side_v2(v1coSS, v2coSS, v3coSS) < 0.0f) {
						continue;
					}

				}
			}

			if (tpage_last != tpage) {

				image_index = BLI_linklist_index(image_LinkList, tpage);

				if (image_index == -1 && BKE_image_has_ibuf(tpage, NULL)) { /* MemArena dosnt have an append func */
					BLI_linklist_append(&image_LinkList, tpage);
					image_index = ps->image_tot;
					ps->image_tot++;
				}

				tpage_last = tpage;
			}

			if (image_index != -1) {
				/* Initialize the faces screen pixels */
				/* Add this to a list to initialize later */
				project_paint_delayed_face_init(ps, mf, face_index);
			}
		}
	}

	/* build an array of images we use*/
	projIma = ps->projImages = (ProjPaintImage *)BLI_memarena_alloc(arena, sizeof(ProjPaintImage) * ps->image_tot);

	for (node = image_LinkList, i = 0; node; node = node->next, i++, projIma++) {
		projIma->ima = node->link;
		projIma->touch = 0;
		projIma->ibuf = BKE_image_acquire_ibuf(projIma->ima, NULL, NULL);
		projIma->partRedrawRect =  BLI_memarena_calloc(arena, sizeof(ImagePaintPartialRedraw) * PROJ_BOUNDBOX_SQUARED);
	}

	/* we have built the array, discard the linked list */
	BLI_linklist_free(image_LinkList, NULL);
}

static void paint_proj_begin_clone(ProjPaintState *ps, const float mouse[2])
{
	/* setup clone offset */
	if (ps->tool == PAINT_TOOL_CLONE) {
		float projCo[4];
		copy_v3_v3(projCo, ED_view3d_cursor3d_get(ps->scene, ps->v3d));
		mul_m4_v3(ps->ob->imat, projCo);

		projCo[3] = 1.0f;
		mul_m4_v4(ps->projectMat, projCo);
		ps->cloneOffset[0] = mouse[0] - ((float)(ps->winx * 0.5f) + (ps->winx * 0.5f) * projCo[0] / projCo[3]);
		ps->cloneOffset[1] = mouse[1] - ((float)(ps->winy * 0.5f) + (ps->winy * 0.5f) * projCo[1] / projCo[3]);
	}
}

static void project_paint_end(ProjPaintState *ps)
{
	int a;
	ProjPaintImage *projIma;

	/* build undo data from original pixel colors */
	if (U.uiflag & USER_GLOBALUNDO) {
		ProjPixel *projPixel;
		ImBuf *tmpibuf = NULL, *tmpibuf_float = NULL;
		LinkNode *pixel_node;
		void *tilerect;
		MemArena *arena = ps->arena_mt[0]; /* threaded arena re-used for non threaded case */

		int bucket_tot = (ps->buckets_x * ps->buckets_y); /* we could get an X/Y but easier to loop through all possible buckets */
		int bucket_index;
		int tile_index;
		int x_round, y_round;
		int x_tile, y_tile;
		int is_float = -1;

		/* context */
		ProjPaintImage *last_projIma;
		int last_image_index = -1;
		int last_tile_width = 0;

		for (a = 0, last_projIma = ps->projImages; a < ps->image_tot; a++, last_projIma++) {
			int size = sizeof(void **) * IMAPAINT_TILE_NUMBER(last_projIma->ibuf->x) * IMAPAINT_TILE_NUMBER(last_projIma->ibuf->y);
			last_projIma->undoRect = (void **) BLI_memarena_calloc(arena, size);
			last_projIma->ibuf->userflags |= IB_BITMAPDIRTY;
		}

		for (bucket_index = 0; bucket_index < bucket_tot; bucket_index++) {
			/* loop through all pixels */
			for (pixel_node = ps->bucketRect[bucket_index]; pixel_node; pixel_node = pixel_node->next) {

				/* ok we have a pixel, was it modified? */
				projPixel = (ProjPixel *)pixel_node->link;

				if (last_image_index != projPixel->image_index) {
					/* set the context */
					last_image_index = projPixel->image_index;
					last_projIma =     ps->projImages + last_image_index;
					last_tile_width =  IMAPAINT_TILE_NUMBER(last_projIma->ibuf->x);
					is_float =         last_projIma->ibuf->rect_float ? 1 : 0;
				}


				if ((is_float == 0 && projPixel->origColor.uint != *projPixel->pixel.uint_pt) ||
				    (is_float == 1 &&
				     (projPixel->origColor.f[0] != projPixel->pixel.f_pt[0] ||
				      projPixel->origColor.f[1] != projPixel->pixel.f_pt[1] ||
				      projPixel->origColor.f[2] != projPixel->pixel.f_pt[2] ||
				      projPixel->origColor.f[3] != projPixel->pixel.f_pt[3]))
				    )
				{

					x_tile =  projPixel->x_px >> IMAPAINT_TILE_BITS;
					y_tile =  projPixel->y_px >> IMAPAINT_TILE_BITS;

					x_round = x_tile * IMAPAINT_TILE_SIZE;
					y_round = y_tile * IMAPAINT_TILE_SIZE;

					tile_index = x_tile + y_tile * last_tile_width;

					if (last_projIma->undoRect[tile_index] == NULL) {
						/* add the undo tile from the modified image, then write the original colors back into it */
						tilerect = last_projIma->undoRect[tile_index] = image_undo_push_tile(last_projIma->ima, last_projIma->ibuf, is_float ? (&tmpibuf_float) : (&tmpibuf), x_tile, y_tile);
					}
					else {
						tilerect = last_projIma->undoRect[tile_index];
					}

					/* This is a BIT ODD, but overwrite the undo tiles image info with this pixels original color
					 * because allocating the tiles along the way slows down painting */

					if (is_float) {
						float *rgba_fp = (float *)tilerect + (((projPixel->x_px - x_round) + (projPixel->y_px - y_round) * IMAPAINT_TILE_SIZE)) * 4;
						copy_v4_v4(rgba_fp, projPixel->origColor.f);
					}
					else {
						((unsigned int *)tilerect)[(projPixel->x_px - x_round) + (projPixel->y_px - y_round) * IMAPAINT_TILE_SIZE] = projPixel->origColor.uint;
					}
				}
			}
		}

		if (tmpibuf) IMB_freeImBuf(tmpibuf);
		if (tmpibuf_float) IMB_freeImBuf(tmpibuf_float);
	}
	/* done calculating undo data */

	/* dereference used image buffers */
	for (a = 0, projIma = ps->projImages; a < ps->image_tot; a++, projIma++) {
		BKE_image_release_ibuf(projIma->ima, projIma->ibuf, NULL);
	}

	BKE_image_release_ibuf(ps->reproject_image, ps->reproject_ibuf, NULL);

	MEM_freeN(ps->screenCoords);
	MEM_freeN(ps->bucketRect);
	MEM_freeN(ps->bucketFaces);
	MEM_freeN(ps->bucketFlags);

#ifndef PROJ_DEBUG_NOSEAMBLEED
	if (ps->seam_bleed_px > 0.0f) {
		MEM_freeN(ps->vertFaces);
		MEM_freeN(ps->faceSeamFlags);
		MEM_freeN(ps->faceWindingFlags);
		MEM_freeN(ps->faceSeamUVs);
	}
#endif

	if (ps->vertFlags) MEM_freeN(ps->vertFlags);

	for (a = 0; a < ps->thread_tot; a++) {
		BLI_memarena_free(ps->arena_mt[a]);
	}

	/* copy for subsurf/multires, so throw away */
	if (ps->dm->type != DM_TYPE_CDDM) {
		if (ps->dm_mvert) MEM_freeN(ps->dm_mvert);
		if (ps->dm_mface) MEM_freeN(ps->dm_mface);
		/* looks like these don't need copying */
#if 0
		if (ps->dm_mtface) MEM_freeN(ps->dm_mtface);
		if (ps->dm_mtface_clone) MEM_freeN(ps->dm_mtface_clone);
		if (ps->dm_mtface_stencil) MEM_freeN(ps->dm_mtface_stencil);
#endif
	}

	if (ps->dm_release)
		ps->dm->release(ps->dm);
}

/* 1 = an undo, -1 is a redo. */
static void partial_redraw_array_init(ImagePaintPartialRedraw *pr)
{
	int tot = PROJ_BOUNDBOX_SQUARED;
	while (tot--) {
		pr->x1 = 10000000;
		pr->y1 = 10000000;

		pr->x2 = -1;
		pr->y2 = -1;

		pr->enabled = 1;

		pr++;
	}
}


static bool partial_redraw_array_merge(ImagePaintPartialRedraw *pr, ImagePaintPartialRedraw *pr_other, int tot)
{
	bool touch = 0;
	while (tot--) {
		pr->x1 = min_ii(pr->x1, pr_other->x1);
		pr->y1 = min_ii(pr->y1, pr_other->y1);

		pr->x2 = max_ii(pr->x2, pr_other->x2);
		pr->y2 = max_ii(pr->y2, pr_other->y2);

		if (pr->x2 != -1)
			touch = 1;

		pr++; pr_other++;
	}

	return touch;
}

/* Loop over all images on this mesh and update any we have touched */
static bool project_image_refresh_tagged(ProjPaintState *ps)
{
	ImagePaintPartialRedraw *pr;
	ProjPaintImage *projIma;
	int a, i;
	bool redraw = false;


	for (a = 0, projIma = ps->projImages; a < ps->image_tot; a++, projIma++) {
		if (projIma->touch) {
			/* look over each bound cell */
			for (i = 0; i < PROJ_BOUNDBOX_SQUARED; i++) {
				pr = &(projIma->partRedrawRect[i]);
				if (pr->x2 != -1) { /* TODO - use 'enabled' ? */
					set_imapaintpartial(pr);
					imapaint_image_update(NULL, projIma->ima, projIma->ibuf, true);
					redraw = 1;
				}
			}

			projIma->touch = 0; /* clear for reuse */
		}
	}

	return redraw;
}

/* run this per painting onto each mouse location */
static bool project_bucket_iter_init(ProjPaintState *ps, const float mval_f[2])
{
	if (ps->source == PROJ_SRC_VIEW) {
		float min_brush[2], max_brush[2];
		const float radius = (float)BKE_brush_size_get(ps->scene, ps->brush);

		/* so we don't have a bucket bounds that is way too small to paint into */
		// if (radius < 1.0f) radius = 1.0f; // this doesn't work yet :/

		min_brush[0] = mval_f[0] - radius;
		min_brush[1] = mval_f[1] - radius;

		max_brush[0] = mval_f[0] + radius;
		max_brush[1] = mval_f[1] + radius;

		/* offset to make this a valid bucket index */
		project_paint_bucket_bounds(ps, min_brush, max_brush, ps->bucketMin, ps->bucketMax);

		/* mouse outside the model areas? */
		if (ps->bucketMin[0] == ps->bucketMax[0] || ps->bucketMin[1] == ps->bucketMax[1]) {
			return 0;
		}

		ps->context_bucket_x = ps->bucketMin[0];
		ps->context_bucket_y = ps->bucketMin[1];
	}
	else { /* reproject: PROJ_SRC_* */
		ps->bucketMin[0] = 0;
		ps->bucketMin[1] = 0;

		ps->bucketMax[0] = ps->buckets_x;
		ps->bucketMax[1] = ps->buckets_y;

		ps->context_bucket_x = 0;
		ps->context_bucket_y = 0;
	}
	return 1;
}


static bool project_bucket_iter_next(ProjPaintState *ps, int *bucket_index, rctf *bucket_bounds, const float mval[2])
{
	const int diameter = 2 * BKE_brush_size_get(ps->scene, ps->brush);

	if (ps->thread_tot > 1)
		BLI_lock_thread(LOCK_CUSTOM1);

	//printf("%d %d\n", ps->context_bucket_x, ps->context_bucket_y);

	for (; ps->context_bucket_y < ps->bucketMax[1]; ps->context_bucket_y++) {
		for (; ps->context_bucket_x < ps->bucketMax[0]; ps->context_bucket_x++) {

			/* use bucket_bounds for project_bucket_isect_circle and project_bucket_init*/
			project_bucket_bounds(ps, ps->context_bucket_x, ps->context_bucket_y, bucket_bounds);

			if ((ps->source != PROJ_SRC_VIEW) ||
			    project_bucket_isect_circle(mval, (float)(diameter * diameter), bucket_bounds))
			{
				*bucket_index = ps->context_bucket_x + (ps->context_bucket_y * ps->buckets_x);
				ps->context_bucket_x++;

				if (ps->thread_tot > 1)
					BLI_unlock_thread(LOCK_CUSTOM1);

				return 1;
			}
		}
		ps->context_bucket_x = ps->bucketMin[0];
	}

	if (ps->thread_tot > 1)
		BLI_unlock_thread(LOCK_CUSTOM1);
	return 0;
}

/* Each thread gets one of these, also used as an argument to pass to project_paint_op */
typedef struct ProjectHandle {
	/* args */
	ProjPaintState *ps;
	float prevmval[2];
	float mval[2];

	/* annoying but we need to have image bounds per thread, then merge into ps->projectPartialRedraws */
	ProjPaintImage *projImages; /* array of partial redraws */

	/* thread settings */
	int thread_index;

	struct ImagePool *pool;
} ProjectHandle;

static void do_projectpaint_clone(ProjPaintState *ps, ProjPixel *projPixel, float mask)
{
	const unsigned char *clone_pt = ((ProjPixelClone *)projPixel)->clonepx.ch;

	if (clone_pt[3]) {
		unsigned char clone_rgba[4];

		clone_rgba[0] = clone_pt[0];
		clone_rgba[1] = clone_pt[1];
		clone_rgba[2] = clone_pt[2];
		clone_rgba[3] = (unsigned char)(clone_pt[3] * mask);

		if (ps->do_masking) {
			IMB_blend_color_byte(projPixel->pixel.ch_pt, projPixel->origColor.ch, clone_rgba, ps->blend);
		}
		else {
			IMB_blend_color_byte(projPixel->pixel.ch_pt, projPixel->pixel.ch_pt, clone_rgba, ps->blend);
		}
	}
}

static void do_projectpaint_clone_f(ProjPaintState *ps, ProjPixel *projPixel, float mask)
{
	const float *clone_pt = ((ProjPixelClone *)projPixel)->clonepx.f;

	if (clone_pt[3]) {
		float clone_rgba[4];

		mul_v4_v4fl(clone_rgba, clone_pt, mask);

		if (ps->do_masking) {
			IMB_blend_color_float(projPixel->pixel.f_pt, projPixel->origColor.f, clone_rgba, ps->blend);
		}
		else {
			IMB_blend_color_float(projPixel->pixel.f_pt, projPixel->pixel.f_pt, clone_rgba, ps->blend);
		}
	}
}

/* do_projectpaint_smear*
 *
 * note, mask is used to modify the alpha here, this is not correct since it allows
 * accumulation of color greater then 'projPixel->mask' however in the case of smear its not
 * really that important to be correct as it is with clone and painting
 */
static void do_projectpaint_smear(ProjPaintState *ps, ProjPixel *projPixel, float mask,
                                  MemArena *smearArena, LinkNode **smearPixels, const float co[2])
{
	unsigned char rgba_ub[4];

	if (project_paint_PickColor(ps, co, NULL, rgba_ub, 1) == 0)
		return;

	blend_color_interpolate_byte(((ProjPixelClone *)projPixel)->clonepx.ch, projPixel->pixel.ch_pt, rgba_ub, mask);
	BLI_linklist_prepend_arena(smearPixels, (void *)projPixel, smearArena);
}

static void do_projectpaint_smear_f(ProjPaintState *ps, ProjPixel *projPixel, float mask,
                                    MemArena *smearArena, LinkNode **smearPixels_f, const float co[2])
{
	float rgba[4];

	if (project_paint_PickColor(ps, co, rgba, NULL, 1) == 0)
		return;

	blend_color_interpolate_float(((ProjPixelClone *)projPixel)->clonepx.f, projPixel->pixel.f_pt, rgba, mask);
	BLI_linklist_prepend_arena(smearPixels_f, (void *)projPixel, smearArena);
}

/* do_projectpaint_soften for float & byte
 */
static float inv_pow2(float f)
{
	f = 1.0f - f;
	f = f * f;
	return 1.0f - f;
}

static void do_projectpaint_soften_f(ProjPaintState *ps, ProjPixel *projPixel, float mask,
                                     MemArena *softenArena, LinkNode **softenPixels)
{
	unsigned int accum_tot = 0;
	unsigned int i;

	float *rgba = projPixel->newColor.f;

	/* sigh, mask values tend to need to be a _lot_ stronger with blur */
	mask  = inv_pow2(mask);

	/* rather then painting, accumulate surrounding colors */
	zero_v4(rgba);

	for (i = 0; i < PROJ_PIXEL_SOFTEN_TOT; i++) {
		float co_ofs[2];
		float rgba_tmp[4];
		sub_v2_v2v2(co_ofs, projPixel->projCoSS, proj_pixel_soften_v2[i]);
		if (project_paint_PickColor(ps, co_ofs, rgba_tmp, NULL, true)) {
			add_v4_v4(rgba, rgba_tmp);
			accum_tot++;
		}
	}

	if (LIKELY(accum_tot != 0)) {
		mul_v4_fl(rgba, 1.0f / (float)accum_tot);
		blend_color_interpolate_float(rgba, rgba, projPixel->pixel.f_pt, mask);
		BLI_linklist_prepend_arena(softenPixels, (void *)projPixel, softenArena);
	}
}

static void do_projectpaint_soften(ProjPaintState *ps, ProjPixel *projPixel, float mask,
                                   MemArena *softenArena, LinkNode **softenPixels)
{
	unsigned int accum_tot = 0;
	unsigned int i;

	float rgba[4];  /* convert to byte after */

	/* sigh, mask values tend to need to be a _lot_ stronger with blur */
	mask  = inv_pow2(mask);

	/* rather then painting, accumulate surrounding colors */
	zero_v4(rgba);

	for (i = 0; i < PROJ_PIXEL_SOFTEN_TOT; i++) {
		float co_ofs[2];
		float rgba_tmp[4];
		sub_v2_v2v2(co_ofs, projPixel->projCoSS, proj_pixel_soften_v2[i]);
		if (project_paint_PickColor(ps, co_ofs, rgba_tmp, NULL, true)) {
			add_v4_v4(rgba, rgba_tmp);
			accum_tot++;
		}
	}

	if (LIKELY(accum_tot != 0)) {
		unsigned char *rgba_ub = projPixel->newColor.ch;

		mul_v4_fl(rgba, 1.0f / (float)accum_tot);
		premul_float_to_straight_uchar(rgba_ub, rgba);

		blend_color_interpolate_byte(rgba_ub, rgba_ub, projPixel->pixel.ch_pt, mask);
		BLI_linklist_prepend_arena(softenPixels, (void *)projPixel, softenArena);
	}
}

static void do_projectpaint_draw(ProjPaintState *ps, ProjPixel *projPixel, const float texrgb[3], float mask)
{
	float rgb[3];
	unsigned char rgba_ub[4];

	copy_v3_v3(rgb, ps->brush->rgb);

	if (ps->is_texbrush) {
		mul_v3_v3(rgb, texrgb);
		/* TODO(sergey): Support texture paint color space. */
		linearrgb_to_srgb_v3_v3(rgb, rgb);
	}

	rgb_float_to_uchar(rgba_ub, rgb);
	rgba_ub[3] = f_to_char(mask);

	if (ps->do_masking) {
		IMB_blend_color_byte(projPixel->pixel.ch_pt, projPixel->origColor.ch, rgba_ub, ps->blend);
	}
	else {
		IMB_blend_color_byte(projPixel->pixel.ch_pt, projPixel->pixel.ch_pt, rgba_ub, ps->blend);
	}
}

static void do_projectpaint_draw_f(ProjPaintState *ps, ProjPixel *projPixel, const float texrgb[3], float mask)
{
	float rgba[4];

	srgb_to_linearrgb_v3_v3(rgba, ps->brush->rgb);

	if (ps->is_texbrush)
		mul_v3_v3(rgba, texrgb);
	
	mul_v3_fl(rgba, mask);
	rgba[3] = mask;

	if (ps->do_masking) {
		IMB_blend_color_float(projPixel->pixel.f_pt, projPixel->origColor.f, rgba, ps->blend);
	}
	else {
		IMB_blend_color_float(projPixel->pixel.f_pt, projPixel->pixel.f_pt, rgba, ps->blend);
	}
}

/* run this for single and multithreaded painting */
static void *do_projectpaint_thread(void *ph_v)
{
	/* First unpack args from the struct */
	ProjPaintState *ps =         ((ProjectHandle *)ph_v)->ps;
	ProjPaintImage *projImages = ((ProjectHandle *)ph_v)->projImages;
	const float *lastpos =       ((ProjectHandle *)ph_v)->prevmval;
	const float *pos =           ((ProjectHandle *)ph_v)->mval;
	const int thread_index =     ((ProjectHandle *)ph_v)->thread_index;
	struct ImagePool *pool =     ((ProjectHandle *)ph_v)->pool;
	/* Done with args from ProjectHandle */

	LinkNode *node;
	ProjPixel *projPixel;
	Brush *brush = ps->brush;

	int last_index = -1;
	ProjPaintImage *last_projIma = NULL;
	ImagePaintPartialRedraw *last_partial_redraw_cell;

	float dist_sq, dist;

	float falloff;
	int bucket_index;
	bool is_floatbuf = false;
	const short tool =  ps->tool;
	rctf bucket_bounds;

	/* for smear only */
	float pos_ofs[2] = {0};
	float co[2];
	unsigned short mask_short;
	const float brush_alpha = BKE_brush_alpha_get(ps->scene, brush);
	const float brush_radius = (float)BKE_brush_size_get(ps->scene, brush);
	const float brush_radius_sq = brush_radius * brush_radius; /* avoid a square root with every dist comparison */

	short lock_alpha = ELEM(brush->blend, IMB_BLEND_ERASE_ALPHA, IMB_BLEND_ADD_ALPHA) ? 0 : brush->flag & BRUSH_LOCK_ALPHA;

	LinkNode *smearPixels = NULL;
	LinkNode *smearPixels_f = NULL;
	MemArena *smearArena = NULL; /* mem arena for this brush projection only */

	LinkNode *softenPixels = NULL;
	LinkNode *softenPixels_f = NULL;
	MemArena *softenArena = NULL; /* mem arena for this brush projection only */

	if (tool == PAINT_TOOL_SMEAR) {
		pos_ofs[0] = pos[0] - lastpos[0];
		pos_ofs[1] = pos[1] - lastpos[1];

		smearArena = BLI_memarena_new(MEM_SIZE_OPTIMAL(1 << 16), "paint smear arena");
	}
	else if (tool == PAINT_TOOL_SOFTEN) {
		softenArena = BLI_memarena_new(MEM_SIZE_OPTIMAL(1 << 16), "paint soften arena");
	}

	/* printf("brush bounds %d %d %d %d\n", bucketMin[0], bucketMin[1], bucketMax[0], bucketMax[1]); */

	while (project_bucket_iter_next(ps, &bucket_index, &bucket_bounds, pos)) {

		/* Check this bucket and its faces are initialized */
		if (ps->bucketFlags[bucket_index] == PROJ_BUCKET_NULL) {
			/* No pixels initialized */
			project_bucket_init(ps, thread_index, bucket_index, &bucket_bounds);
		}

		if (ps->source != PROJ_SRC_VIEW) {

			/* Re-Projection, simple, no brushes! */

			for (node = ps->bucketRect[bucket_index]; node; node = node->next) {
				projPixel = (ProjPixel *)node->link;

				/* copy of code below */
				if (last_index != projPixel->image_index) {
					last_index = projPixel->image_index;
					last_projIma = projImages + last_index;

					last_projIma->touch = 1;
					is_floatbuf = (last_projIma->ibuf->rect_float != NULL);
				}
				/* end copy */

				if (is_floatbuf) {
					/* re-project buffer is assumed byte - TODO, allow float */
					bicubic_interpolation_color(ps->reproject_ibuf, projPixel->newColor.ch, NULL,
					                            projPixel->projCoSS[0], projPixel->projCoSS[1]);
					if (projPixel->newColor.ch[3]) {
						float newColor_f[4];
						float mask = ((float)projPixel->mask) * (1.0f / 65535.0f);

						straight_uchar_to_premul_float(newColor_f, projPixel->newColor.ch);
						IMB_colormanagement_colorspace_to_scene_linear_v4(newColor_f, true, ps->reproject_ibuf->rect_colorspace);
						mul_v4_v4fl(newColor_f, newColor_f, mask);

						blend_color_mix_float(projPixel->pixel.f_pt,  projPixel->origColor.f,
						                      newColor_f);
					}
				}
				else {
					/* re-project buffer is assumed byte - TODO, allow float */
					bicubic_interpolation_color(ps->reproject_ibuf, projPixel->newColor.ch, NULL,
					                            projPixel->projCoSS[0], projPixel->projCoSS[1]);
					if (projPixel->newColor.ch[3]) {
						float mask = ((float)projPixel->mask) * (1.0f / 65535.0f);
						projPixel->newColor.ch[3] *= mask;

						blend_color_mix_byte(projPixel->pixel.ch_pt,  projPixel->origColor.ch,
						                     projPixel->newColor.ch);
					}
				}
			}
		}
		else {
			/* Normal brush painting */

			for (node = ps->bucketRect[bucket_index]; node; node = node->next) {

				projPixel = (ProjPixel *)node->link;

				dist_sq = len_squared_v2v2(projPixel->projCoSS, pos);

				/*if (dist < radius) {*/ /* correct but uses a sqrtf */
				if (dist_sq <= brush_radius_sq) {
					dist = sqrtf(dist_sq);

					falloff = BKE_brush_curve_strength_clamp(ps->brush, dist, brush_radius);

					if (falloff > 0.0f) {
						float texrgb[3];
						float mask = falloff;

						if (ps->do_masking) {
							/* masking to keep brush contribution to a pixel limited. note we do not do
							 * a simple max(mask, mask_accum), as this is very sensitive to spacing and
							 * gives poor results for strokes crossing themselves.
							 * 
							 * Instead we use a formula that adds up but approaches brush_alpha slowly
							 * and never exceeds it, which gives nice smooth results. */
							float mask_accum = projPixel->mask_accum;

							if (ps->is_maskbrush) {
								float texmask = BKE_brush_sample_masktex(ps->scene, ps->brush, projPixel->projCoSS, thread_index, pool);
								CLAMP(texmask, 0.0f, 1.0f);
								mask = mask_accum + (brush_alpha * texmask * 65535.0f - mask_accum) * mask;
							}
							else {
								mask = mask_accum + (brush_alpha * 65535.0f - mask_accum) * mask;
							}
							mask_short = (unsigned short)mask;

							if (mask_short > projPixel->mask_accum) {
								projPixel->mask_accum = mask_short;
								mask = mask_short * (1.0f / 65535.0f);
							}
							else {
								/* Go onto the next pixel */
								continue;
							}
						}
						else {
							mask *= brush_alpha;
							if (ps->is_maskbrush) {
								float texmask = BKE_brush_sample_masktex(ps->scene, ps->brush, projPixel->projCoSS, thread_index, pool);
								CLAMP(texmask, 0.0f, 1.0f);
								mask *= texmask;
							}
						}

						if (ps->is_texbrush) {
							MTex *mtex = &brush->mtex;
							float samplecos[3];
							float texrgba[4];

							/* taking 3d copy to account for 3D mapping too. It gets concatenated during sampling */
							if (mtex->brush_map_mode == MTEX_MAP_MODE_3D) {
								copy_v3_v3(samplecos, projPixel->worldCoSS);
							}
							else {
								copy_v2_v2(samplecos, projPixel->projCoSS);
								samplecos[2] = 0.0f;
							}

							/* note, for clone and smear, we only use the alpha, could be a special function */
							BKE_brush_sample_tex_3D(ps->scene, brush, samplecos, texrgba, thread_index, pool);

							copy_v3_v3(texrgb, texrgba);
							mask *= texrgba[3];
						}

						if (ps->is_maskbrush_tiled) {
							mask *= BKE_brush_sample_masktex(ps->scene, ps->brush, projPixel->projCoSS, thread_index, pool);
						}

						/* extra mask for normal, layer stencil, .. */
						mask *= ((float)projPixel->mask) * (1.0f / 65535.0f);

						if (mask > 0.0f) {

							/* copy of code above */
							if (last_index != projPixel->image_index) {
								last_index = projPixel->image_index;
								last_projIma = projImages + last_index;

								last_projIma->touch = 1;
								is_floatbuf = (last_projIma->ibuf->rect_float != NULL);
							}
							/* end copy */

							last_partial_redraw_cell = last_projIma->partRedrawRect + projPixel->bb_cell_index;
							last_partial_redraw_cell->x1 = min_ii(last_partial_redraw_cell->x1, (int)projPixel->x_px);
							last_partial_redraw_cell->y1 = min_ii(last_partial_redraw_cell->y1, (int)projPixel->y_px);

							last_partial_redraw_cell->x2 = max_ii(last_partial_redraw_cell->x2, (int)projPixel->x_px + 1);
							last_partial_redraw_cell->y2 = max_ii(last_partial_redraw_cell->y2, (int)projPixel->y_px + 1);

							/* texrgb is not used for clone, smear or soften */
							switch (tool) {
								case PAINT_TOOL_CLONE:
									if (is_floatbuf) do_projectpaint_clone_f(ps, projPixel, mask);
									else             do_projectpaint_clone(ps, projPixel, mask);
									break;
								case PAINT_TOOL_SMEAR:
									sub_v2_v2v2(co, projPixel->projCoSS, pos_ofs);

									if (is_floatbuf) do_projectpaint_smear_f(ps, projPixel, mask, smearArena, &smearPixels_f, co);
									else             do_projectpaint_smear(ps, projPixel, mask, smearArena, &smearPixels, co);
									break;
								case PAINT_TOOL_SOFTEN:
									if (is_floatbuf) do_projectpaint_soften_f(ps, projPixel, mask, softenArena, &softenPixels_f);
									else             do_projectpaint_soften(ps, projPixel, mask, softenArena, &softenPixels);
									break;
								default:
									if (is_floatbuf) do_projectpaint_draw_f(ps, projPixel, texrgb, mask);
									else             do_projectpaint_draw(ps, projPixel, texrgb, mask);
									break;
							}
						}

						if (lock_alpha) {
							if (is_floatbuf) projPixel->pixel.f_pt[3] = projPixel->origColor.f[3];
							else projPixel->pixel.ch_pt[3] = projPixel->origColor.ch[3];
						}

						/* done painting */
					}
				}
			}
		}
	}


	if (tool == PAINT_TOOL_SMEAR) {

		for (node = smearPixels; node; node = node->next) { /* this wont run for a float image */
			projPixel = node->link;
			*projPixel->pixel.uint_pt = ((ProjPixelClone *)projPixel)->clonepx.uint;
		}

		for (node = smearPixels_f; node; node = node->next) {
			projPixel = node->link;
			copy_v4_v4(projPixel->pixel.f_pt, ((ProjPixelClone *)projPixel)->clonepx.f);
		}

		BLI_memarena_free(smearArena);
	}
	else if (tool == PAINT_TOOL_SOFTEN) {

		for (node = softenPixels; node; node = node->next) { /* this wont run for a float image */
			projPixel = node->link;
			*projPixel->pixel.uint_pt = projPixel->newColor.uint;
		}

		for (node = softenPixels_f; node; node = node->next) {
			projPixel = node->link;
			copy_v4_v4(projPixel->pixel.f_pt, projPixel->newColor.f);
		}

		BLI_memarena_free(softenArena);
	}

	return NULL;
}

static bool project_paint_op(void *state, const float lastpos[2], const float pos[2])
{
	/* First unpack args from the struct */
	ProjPaintState *ps = (ProjPaintState *)state;
	bool touch_any = false;

	ProjectHandle handles[BLENDER_MAX_THREADS];
	ListBase threads;
	int a, i;

	struct ImagePool *pool;

	if (!project_bucket_iter_init(ps, pos)) {
		return 0;
	}

	if (ps->thread_tot > 1)
		BLI_init_threads(&threads, do_projectpaint_thread, ps->thread_tot);

	pool = BKE_image_pool_new();

	/* get the threads running */
	for (a = 0; a < ps->thread_tot; a++) {

		/* set defaults in handles */
		//memset(&handles[a], 0, sizeof(BakeShade));

		handles[a].ps = ps;
		copy_v2_v2(handles[a].mval, pos);
		copy_v2_v2(handles[a].prevmval, lastpos);

		/* thread specific */
		handles[a].thread_index = a;

		handles[a].projImages = (ProjPaintImage *)BLI_memarena_alloc(ps->arena_mt[a], ps->image_tot * sizeof(ProjPaintImage));

		memcpy(handles[a].projImages, ps->projImages, ps->image_tot * sizeof(ProjPaintImage));

		/* image bounds */
		for (i = 0; i < ps->image_tot; i++) {
			handles[a].projImages[i].partRedrawRect = (ImagePaintPartialRedraw *)BLI_memarena_alloc(ps->arena_mt[a], sizeof(ImagePaintPartialRedraw) * PROJ_BOUNDBOX_SQUARED);
			memcpy(handles[a].projImages[i].partRedrawRect, ps->projImages[i].partRedrawRect, sizeof(ImagePaintPartialRedraw) * PROJ_BOUNDBOX_SQUARED);
		}

		handles[a].pool = pool;

		if (ps->thread_tot > 1)
			BLI_insert_thread(&threads, &handles[a]);
	}

	if (ps->thread_tot > 1) /* wait for everything to be done */
		BLI_end_threads(&threads);
	else
		do_projectpaint_thread(&handles[0]);


	BKE_image_pool_free(pool);

	/* move threaded bounds back into ps->projectPartialRedraws */
	for (i = 0; i < ps->image_tot; i++) {
		int touch = 0;
		for (a = 0; a < ps->thread_tot; a++) {
			touch |= partial_redraw_array_merge(ps->projImages[i].partRedrawRect, handles[a].projImages[i].partRedrawRect, PROJ_BOUNDBOX_SQUARED);
		}

		if (touch) {
			ps->projImages[i].touch = 1;
			touch_any = 1;
		}
	}

	return touch_any;
}


void paint_proj_stroke(bContext *C, void *pps, const float prev_pos[2], const float pos[2])
{
	ProjPaintState *ps = pps;
	int a;

	/* clone gets special treatment here to avoid going through image initialization */
	if (ps->tool == PAINT_TOOL_CLONE && ps->mode == BRUSH_STROKE_INVERT) {
		Scene *scene = ps->scene;
		View3D *v3d = ps->v3d;
		float *cursor = ED_view3d_cursor3d_get(scene, v3d);
		int mval_i[2] = {(int)pos[0], (int)pos[1]};

		view3d_operator_needs_opengl(C);

		if (!ED_view3d_autodist(scene, ps->ar, v3d, mval_i, cursor, false, NULL))
			return;

		ED_region_tag_redraw(ps->ar);

		return;
	}

	/* continue adding to existing partial redraw rects until redraw */
	if (!ps->need_redraw) {
		for (a = 0; a < ps->image_tot; a++)
			partial_redraw_array_init(ps->projImages[a].partRedrawRect);
	}

	if (project_paint_op(ps, prev_pos, pos))
		ps->need_redraw = true;
}


/* initialize project paint settings from context */
static void project_state_init(bContext *C, Object *ob, ProjPaintState *ps, int mode)
{
	Scene *scene = CTX_data_scene(C);
	ToolSettings *settings = scene->toolsettings;

	/* brush */
	ps->mode = mode;
	ps->brush = BKE_paint_brush(&settings->imapaint.paint);
	if (ps->brush) {
		Brush *brush = ps->brush;
		ps->tool = brush->imagepaint_tool;
		ps->blend = brush->blend;

		/* disable for 3d mapping also because painting on mirrored mesh can create "stripes" */
		ps->do_masking = (brush->flag & BRUSH_AIRBRUSH ||
		                  (brush->imagepaint_tool == PAINT_TOOL_SMEAR) ||
		                  (brush->mtex.tex && !ELEM(brush->mtex.brush_map_mode, MTEX_MAP_MODE_TILED, MTEX_MAP_MODE_STENCIL, MTEX_MAP_MODE_3D)))
		                 ? false : true;
		ps->is_texbrush = (brush->mtex.tex && brush->imagepaint_tool == PAINT_TOOL_DRAW) ? true : false;
		ps->is_maskbrush = false;
		ps->is_maskbrush_tiled = false;
		if (brush->mask_mtex.tex) {
			if (ELEM(brush->mask_mtex.brush_map_mode, MTEX_MAP_MODE_STENCIL, MTEX_MAP_MODE_TILED)) {
				ps->is_maskbrush_tiled = true;
			}
			else {
				ps->is_maskbrush = true;
			}
		}
	}
	else {
		/* brush may be NULL*/
		ps->do_masking = false;
		ps->is_texbrush = false;
		ps->is_maskbrush = false;
		ps->is_maskbrush_tiled = false;
	}

	/* sizeof(ProjPixel), since we alloc this a _lot_ */
	ps->pixel_sizeof = project_paint_pixel_sizeof(ps->tool);
	BLI_assert(ps->pixel_sizeof >= sizeof(ProjPixel));

	/* these can be NULL */
	ps->v3d = CTX_wm_view3d(C);
	ps->rv3d = CTX_wm_region_view3d(C);
	ps->ar = CTX_wm_region(C);

	ps->scene = scene;
	ps->ob = ob; /* allow override of active object */

	/* setup projection painting data */
	ps->do_backfacecull = (settings->imapaint.flag & IMAGEPAINT_PROJECT_BACKFACE) ? 0 : 1;
	ps->do_occlude = (settings->imapaint.flag & IMAGEPAINT_PROJECT_XRAY) ? 0 : 1;
	ps->do_mask_normal = (settings->imapaint.flag & IMAGEPAINT_PROJECT_FLAT) ? 0 : 1;
	ps->do_new_shading_nodes = BKE_scene_use_new_shading_nodes(scene); /* only cache the value */

	if (ps->tool == PAINT_TOOL_CLONE)
		ps->do_layer_clone = (settings->imapaint.flag & IMAGEPAINT_PROJECT_LAYER_CLONE) ? 1 : 0;

	ps->do_layer_stencil = (settings->imapaint.flag & IMAGEPAINT_PROJECT_LAYER_STENCIL) ? 1 : 0;
	ps->do_layer_stencil_inv = (settings->imapaint.flag & IMAGEPAINT_PROJECT_LAYER_STENCIL_INV) ? 1 : 0;


#ifndef PROJ_DEBUG_NOSEAMBLEED
	ps->seam_bleed_px = settings->imapaint.seam_bleed; /* pixel num to bleed */
#endif

	if (ps->do_mask_normal) {
		ps->normal_angle_inner = settings->imapaint.normal_angle;
		ps->normal_angle = (ps->normal_angle_inner + 90.0f) * 0.5f;
	}
	else {
		ps->normal_angle_inner = ps->normal_angle = settings->imapaint.normal_angle;
	}

	ps->normal_angle_inner *=   (float)(M_PI_2 / 90);
	ps->normal_angle *=         (float)(M_PI_2 / 90);
	ps->normal_angle_range = ps->normal_angle - ps->normal_angle_inner;

	if (ps->normal_angle_range <= 0.0f)
		ps->do_mask_normal = false;  /* no need to do blending */

	return;
}

void *paint_proj_new_stroke(bContext *C, Object *ob, const float mouse[2], int mode)
{
	ProjPaintState *ps = MEM_callocN(sizeof(ProjPaintState), "ProjectionPaintState");
	project_state_init(C, ob, ps, mode);

	if (ps->tool == PAINT_TOOL_CLONE && mode == BRUSH_STROKE_INVERT) {
		view3d_operator_needs_opengl(C);
		return ps;
	}

	paint_brush_init_tex(ps->brush);

	ps->source = PROJ_SRC_VIEW;

	if (ps->ob == NULL || !(ps->ob->lay & ps->v3d->lay)) {
		MEM_freeN(ps);
		return NULL;
	}

	ps->orig_brush_size = BKE_brush_size_get(ps->scene, ps->brush);

	/* Don't allow brush size below 2 */
	if (BKE_brush_size_get(ps->scene, ps->brush) < 2)
		BKE_brush_size_set(ps->scene, ps->brush, 2);

	/* allocate and initialize spatial data structures */
	project_paint_begin(ps);

	if (ps->dm == NULL) {
		MEM_freeN(ps);
		return NULL;
	}

	paint_proj_begin_clone(ps, mouse);

	return ps;
}

void paint_proj_redraw(const bContext *C, void *pps, bool final)
{
	ProjPaintState *ps = pps;

	if (ps->need_redraw) {
		project_image_refresh_tagged(ps);

		ps->need_redraw = false;
	}
	else if (!final) {
		return;
	}

	if (final) {
		/* compositor listener deals with updating */
		WM_event_add_notifier(C, NC_IMAGE | NA_EDITED, NULL);
	}
	else {
		ED_region_tag_redraw(CTX_wm_region(C));
	}
}

void paint_proj_stroke_done(void *pps)
{
	ProjPaintState *ps = pps;
	if (ps->tool == PAINT_TOOL_CLONE && ps->mode == BRUSH_STROKE_INVERT) {
		MEM_freeN(ps);
		return;
	}
	BKE_brush_size_set(ps->scene, ps->brush, ps->orig_brush_size);

	paint_brush_exit_tex(ps->brush);

	project_paint_end(ps);
	MEM_freeN(ps);
}
/* use project paint to re-apply an image */
static int texture_paint_camera_project_exec(bContext *C, wmOperator *op)
{
	Image *image = BLI_findlink(&CTX_data_main(C)->image, RNA_enum_get(op->ptr, "image"));
	Scene *scene = CTX_data_scene(C);
	ProjPaintState ps = {NULL};
	int orig_brush_size;
	IDProperty *idgroup;
	IDProperty *view_data = NULL;

	project_state_init(C, OBACT, &ps, BRUSH_STROKE_NORMAL);

	if (ps.ob == NULL || ps.ob->type != OB_MESH) {
		BKE_report(op->reports, RPT_ERROR, "No active mesh object");
		return OPERATOR_CANCELLED;
	}

	if (image == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Image could not be found");
		return OPERATOR_CANCELLED;
	}

	ps.reproject_image = image;
	ps.reproject_ibuf = BKE_image_acquire_ibuf(image, NULL, NULL);

	if (ps.reproject_ibuf == NULL || ps.reproject_ibuf->rect == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Image data could not be found");
		return OPERATOR_CANCELLED;
	}

	idgroup = IDP_GetProperties(&image->id, 0);

	if (idgroup) {
		view_data = IDP_GetPropertyTypeFromGroup(idgroup, PROJ_VIEW_DATA_ID, IDP_ARRAY);

		/* type check to make sure its ok */
		if (view_data->len != PROJ_VIEW_DATA_SIZE || view_data->subtype != IDP_FLOAT) {
			BKE_report(op->reports, RPT_ERROR, "Image project data invalid");
			return OPERATOR_CANCELLED;
		}
	}

	if (view_data) {
		/* image has stored view projection info */
		ps.source = PROJ_SRC_IMAGE_VIEW;
	}
	else {
		ps.source = PROJ_SRC_IMAGE_CAM;

		if (scene->camera == NULL) {
			BKE_report(op->reports, RPT_ERROR, "No active camera set");
			return OPERATOR_CANCELLED;
		}
	}

	/* override */
	ps.is_texbrush = false;
	ps.is_maskbrush = false;
	ps.is_maskbrush_tiled = false;
	ps.do_masking = false;
	orig_brush_size = BKE_brush_size_get(scene, ps.brush);
	BKE_brush_size_set(scene, ps.brush, 32); /* cover the whole image */

	ps.tool = PAINT_TOOL_DRAW; /* so pixels are initialized with minimal info */

	scene->toolsettings->imapaint.flag |= IMAGEPAINT_DRAWING;

	ED_undo_paint_push_begin(UNDO_PAINT_IMAGE, op->type->name,
	                         ED_image_undo_restore, ED_image_undo_free);

	/* allocate and initialize spatial data structures */
	project_paint_begin(&ps);

	if (ps.dm == NULL) {
		BKE_brush_size_set(scene, ps.brush, orig_brush_size);
		return OPERATOR_CANCELLED;
	}
	else {
		float pos[2] = {0.0, 0.0};
		float lastpos[2] = {0.0, 0.0};
		int a;

		for (a = 0; a < ps.image_tot; a++)
			partial_redraw_array_init(ps.projImages[a].partRedrawRect);

		project_paint_op(&ps, lastpos, pos);

		project_image_refresh_tagged(&ps);

		for (a = 0; a < ps.image_tot; a++) {
			GPU_free_image(ps.projImages[a].ima);
			WM_event_add_notifier(C, NC_IMAGE | NA_EDITED, ps.projImages[a].ima);
		}
	}

	project_paint_end(&ps);

	scene->toolsettings->imapaint.flag &= ~IMAGEPAINT_DRAWING;
	BKE_brush_size_set(scene, ps.brush, orig_brush_size);

	return OPERATOR_FINISHED;
}

void PAINT_OT_project_image(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Project Image";
	ot->idname = "PAINT_OT_project_image";
	ot->description = "Project an edited render from the active camera back onto the object";

	/* api callbacks */
	ot->invoke = WM_enum_search_invoke;
	ot->exec = texture_paint_camera_project_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	prop = RNA_def_enum(ot->srna, "image", DummyRNA_NULL_items, 0, "Image", "");
	RNA_def_enum_funcs(prop, RNA_image_itemf);
	RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
	ot->prop = prop;
}

static int texture_paint_image_from_view_exec(bContext *C, wmOperator *op)
{
	Image *image;
	ImBuf *ibuf;
	char filename[FILE_MAX];

	Scene *scene = CTX_data_scene(C);
	ToolSettings *settings = scene->toolsettings;
	int w = settings->imapaint.screen_grab_size[0];
	int h = settings->imapaint.screen_grab_size[1];
	int maxsize;
	char err_out[256] = "unknown";

	RNA_string_get(op->ptr, "filepath", filename);

	maxsize = GPU_max_texture_size();

	if (w > maxsize) w = maxsize;
	if (h > maxsize) h = maxsize;

	ibuf = ED_view3d_draw_offscreen_imbuf(scene, CTX_wm_view3d(C), CTX_wm_region(C), w, h, IB_rect, false, R_ALPHAPREMUL, err_out);
	if (!ibuf) {
		/* Mostly happens when OpenGL offscreen buffer was failed to create, */
		/* but could be other reasons. Should be handled in the future. nazgul */
		BKE_reportf(op->reports, RPT_ERROR, "Failed to create OpenGL off-screen buffer: %s", err_out);
		return OPERATOR_CANCELLED;
	}

	image = BKE_image_add_from_imbuf(ibuf);

	if (image) {
		/* now for the trickyness. store the view projection here!
		 * re-projection will reuse this */
		View3D *v3d = CTX_wm_view3d(C);
		RegionView3D *rv3d = CTX_wm_region_view3d(C);

		IDPropertyTemplate val;
		IDProperty *idgroup = IDP_GetProperties(&image->id, 1);
		IDProperty *view_data;
		bool is_ortho;
		float *array;

		val.array.len = PROJ_VIEW_DATA_SIZE;
		val.array.type = IDP_FLOAT;
		view_data = IDP_New(IDP_ARRAY, &val, PROJ_VIEW_DATA_ID);

		array = (float *)IDP_Array(view_data);
		memcpy(array, rv3d->winmat, sizeof(rv3d->winmat)); array += sizeof(rv3d->winmat) / sizeof(float);
		memcpy(array, rv3d->viewmat, sizeof(rv3d->viewmat)); array += sizeof(rv3d->viewmat) / sizeof(float);
		is_ortho = ED_view3d_clip_range_get(v3d, rv3d, &array[0], &array[1], true);
		array[2] = is_ortho ? 1.0f : 0.0f; /* using float for a bool is dodgy but since its an extra member in the array... easier then adding a single bool prop */

		IDP_AddToGroup(idgroup, view_data);

		rename_id(&image->id, "image_view");
	}

	return OPERATOR_FINISHED;
}

void PAINT_OT_image_from_view(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Image from View";
	ot->idname = "PAINT_OT_image_from_view";
	ot->description = "Make an image from the current 3D view for re-projection";

	/* api callbacks */
	ot->exec = texture_paint_image_from_view_exec;
	ot->poll = ED_operator_region_view3d_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER;

	RNA_def_string_file_name(ot->srna, "filepath", NULL, FILE_MAX, "File Path", "Name of the file");
}
