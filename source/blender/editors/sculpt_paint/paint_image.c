/*
 * imagepaint.c
 *
 * Functions to paint images in 2D and 3D.
 * 
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

/** \file blender/editors/sculpt_paint/paint_image.c
 *  \ingroup edsculpt
 */


#include <float.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#ifdef WIN32
#include "BLI_winstuff.h"
#endif
#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_dynstr.h"
#include "BLI_linklist.h"
#include "BLI_memarena.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "PIL_time.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "DNA_brush_types.h"
#include "DNA_camera_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"

#include "BKE_camera.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_idprop.h"
#include "BKE_brush.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_global.h"
#include "BKE_deform.h"

#include "BKE_tessmesh.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "UI_view2d.h"

#include "ED_image.h"
#include "ED_screen.h"
#include "ED_sculpt.h"
#include "ED_uvedit.h"
#include "ED_view3d.h"
#include "ED_mesh.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "GPU_draw.h"
#include "GPU_extensions.h"

#include "paint_intern.h"

/* Defines and Structs */

#define IMAPAINT_CHAR_TO_FLOAT(c) ((c)/255.0f)

#define IMAPAINT_FLOAT_RGB_TO_CHAR(c, f)  {                                   \
	(c)[0]= FTOCHAR((f)[0]);                                                  \
	(c)[1]= FTOCHAR((f)[1]);                                                  \
	(c)[2]= FTOCHAR((f)[2]);                                                  \
}
#define IMAPAINT_FLOAT_RGBA_TO_CHAR(c, f)  {                                  \
	(c)[0]= FTOCHAR((f)[0]);                                                  \
	(c)[1]= FTOCHAR((f)[1]);                                                  \
	(c)[2]= FTOCHAR((f)[2]);                                                  \
	(c)[3]= FTOCHAR((f)[3]);                                                  \
}
#define IMAPAINT_CHAR_RGB_TO_FLOAT(f, c)  {                                   \
	(f)[0]= IMAPAINT_CHAR_TO_FLOAT((c)[0]);                                   \
	(f)[1]= IMAPAINT_CHAR_TO_FLOAT((c)[1]);                                   \
	(f)[2]= IMAPAINT_CHAR_TO_FLOAT((c)[2]);                                   \
}
#define IMAPAINT_CHAR_RGBA_TO_FLOAT(f, c)  {                                  \
	(f)[0]= IMAPAINT_CHAR_TO_FLOAT((c)[0]);                                   \
	(f)[1]= IMAPAINT_CHAR_TO_FLOAT((c)[1]);                                   \
	(f)[2]= IMAPAINT_CHAR_TO_FLOAT((c)[2]);                                   \
	(f)[3]= IMAPAINT_CHAR_TO_FLOAT((c)[3]);                                   \
}

#define IMAPAINT_FLOAT_RGB_COPY(a, b) copy_v3_v3(a, b)

#define IMAPAINT_TILE_BITS			6
#define IMAPAINT_TILE_SIZE			(1 << IMAPAINT_TILE_BITS)
#define IMAPAINT_TILE_NUMBER(size)	(((size)+IMAPAINT_TILE_SIZE-1) >> IMAPAINT_TILE_BITS)

static void imapaint_image_update(SpaceImage *sima, Image *image, ImBuf *ibuf, short texpaint);


typedef struct ImagePaintState {
	SpaceImage *sima;
	View2D *v2d;
	Scene *scene;
	bScreen *screen;

	Brush *brush;
	short tool, blend;
	Image *image;
	ImBuf *canvas;
	ImBuf *clonecanvas;
	short clonefreefloat;
	char *warnpackedfile;
	char *warnmultifile;

	/* texture paint only */
	Object *ob;
	Mesh *me;
	int faceindex;
	float uv[2];
} ImagePaintState;

typedef struct ImagePaintPartialRedraw {
	int x1, y1, x2, y2;
	int enabled;
} ImagePaintPartialRedraw;

typedef struct ImagePaintRegion {
	int destx, desty;
	int srcx, srcy;
	int width, height;
} ImagePaintRegion;

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
#define PROJ_FACE_SEAM1	(1<<0)	/* If this face has a seam on any of its edges */
#define PROJ_FACE_SEAM2	(1<<1)
#define PROJ_FACE_SEAM3	(1<<2)
#define PROJ_FACE_SEAM4	(1<<3)

#define PROJ_FACE_NOSEAM1	(1<<4)
#define PROJ_FACE_NOSEAM2	(1<<5)
#define PROJ_FACE_NOSEAM3	(1<<6)
#define PROJ_FACE_NOSEAM4	(1<<7)

#define PROJ_SRC_VIEW		1
#define PROJ_SRC_IMAGE_CAM	2
#define PROJ_SRC_IMAGE_VIEW	3

#define PROJ_VIEW_DATA_ID "view_data"
#define PROJ_VIEW_DATA_SIZE (4*4 + 4*4 + 3) /* viewmat + winmat + clipsta + clipend + is_ortho */


/* a slightly scaled down face is used to get fake 3D location for edge pixels in the seams
 * as this number approaches  1.0f the likelihood increases of float precision errors where
 * it is occluded by an adjacent face */
#define PROJ_FACE_SCALE_SEAM	0.99f

#define PROJ_BUCKET_NULL		0
#define PROJ_BUCKET_INIT		(1<<0)
// #define PROJ_BUCKET_CLONE_INIT	(1<<1)

/* used for testing doubles, if a point is on a line etc */
#define PROJ_GEOM_TOLERANCE 0.00075f

/* vert flags */
#define PROJ_VERT_CULL 1

#define PI_80_DEG ((M_PI_2 / 9) * 8)

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
	short tool, blend;
	Object *ob;
	/* end similarities with ImagePaintState */
	
	DerivedMesh    *dm;
	int 			dm_totface;
	int 			dm_totvert;
	int				dm_release;
	
	MVert 		   *dm_mvert;
	MFace 		   *dm_mface;
	MTFace 		   *dm_mtface;
	MTFace 		   *dm_mtface_clone;	/* other UV map, use for cloning between layers */
	MTFace 		   *dm_mtface_stencil;
	
	/* projection painting only */
	MemArena *arena_mt[BLENDER_MAX_THREADS];/* for multithreading, the first item is sometimes used for non threaded cases too */
	LinkNode **bucketRect;				/* screen sized 2D array, each pixel has a linked list of ProjPixel's */
	LinkNode **bucketFaces;				/* bucketRect aligned array linkList of faces overlapping each bucket */
	unsigned char *bucketFlags;					/* store if the bucks have been initialized  */
#ifndef PROJ_DEBUG_NOSEAMBLEED
	char *faceSeamFlags;				/* store info about faces, if they are initialized etc*/
	float (*faceSeamUVs)[4][2];			/* expanded UVs for faces to use as seams */
	LinkNode **vertFaces;				/* Only needed for when seam_bleed_px is enabled, use to find UV seams */
#endif
	char *vertFlags;					/* store options per vert, now only store if the vert is pointing away from the view */
	int buckets_x;						/* The size of the bucket grid, the grid span's screenMin/screenMax so you can paint outsize the screen or with 2 brushes at once */
	int buckets_y;
	
	ProjPaintImage *projImages;
	
	int image_tot;				/* size of projectImages array */
	
	float (*screenCoords)[4];	/* verts projected into floating point screen space */
	
	float screenMin[2];			/* 2D bounds for mesh verts on the screen's plane (screenspace) */
	float screenMax[2]; 
	float screen_width;			/* Calculated from screenMin & screenMax */
	float screen_height;
	int winx, winy;				/* from the carea or from the projection render */
	
	/* options for projection painting */
	int do_layer_clone;
	int do_layer_stencil;
	int do_layer_stencil_inv;
	
	short do_occlude;			/* Use raytraced occlusion? - ortherwise will paint right through to the back*/
	short do_backfacecull;	/* ignore faces with normals pointing away, skips a lot of raycasts if your normals are correctly flipped */
	short do_mask_normal;			/* mask out pixels based on their normals */
	short do_new_shading_nodes;     /* cache scene_use_new_shading_nodes value */
	float normal_angle;				/* what angle to mask at*/
	float normal_angle_inner;
	float normal_angle_range;		/* difference between normal_angle and normal_angle_inner, for easy access */
	
	short is_ortho;
	short is_airbrush;					/* only to avoid using (ps.brush->flag & BRUSH_AIRBRUSH) */
	short is_texbrush;					/* only to avoid running  */
#ifndef PROJ_DEBUG_NOSEAMBLEED
	float seam_bleed_px;
#endif
	/* clone vars */
	float cloneOffset[2];
	
	float projectMat[4][4];		/* Projection matrix, use for getting screen coords */
	float viewDir[3];			/* View vector, use for do_backfacecull and for ray casting with an ortho viewport  */
	float viewPos[3];			/* View location in object relative 3D space, so can compare to verts  */
	float clipsta, clipend;
	
	/* reproject vars */
	Image *reproject_image;
	ImBuf *reproject_ibuf;


	/* threads */
	int thread_tot;
	int bucketMin[2];
	int bucketMax[2];
	int context_bucket_x, context_bucket_y; /* must lock threads while accessing these */
} ProjPaintState;

typedef union pixelPointer
{
	float *f_pt;			/* float buffer */
	unsigned int *uint_pt; /* 2 ways to access a char buffer */
	unsigned char *ch_pt;
} PixelPointer;

typedef union pixelStore
{
	unsigned char ch[4];
	unsigned int uint;
	float f[4];
} PixelStore;

typedef struct ProjPixel {
	float projCoSS[2]; /* the floating point screen projection of this pixel */
	
	/* Only used when the airbrush is disabled.
	 * Store the max mask value to avoid painting over an area with a lower opacity
	 * with an advantage that we can avoid touching the pixel at all, if the 
	 * new mask value is lower then mask_max */
	unsigned short mask_max;
	
	/* for various reasons we may want to mask out painting onto this pixel */
	unsigned short mask;
	
	short x_px, y_px;
	
	PixelStore origColor;
	PixelStore newColor;
	PixelPointer pixel;
	
	short image_index; /* if anyone wants to paint onto more then 32768 images they can bite me */
	unsigned char bb_cell_index;
} ProjPixel;

typedef struct ProjPixelClone {
	struct ProjPixel __pp;
	PixelStore clonepx;
} ProjPixelClone;

/* Finish projection painting structs */

typedef struct UndoImageTile {
	struct UndoImageTile *next, *prev;

	char idname[MAX_ID_NAME];	/* name instead of pointer*/
	char ibufname[IB_FILENAME_SIZE];

	void *rect;
	int x, y;

	short source, use_float;
	char gen_type;
} UndoImageTile;

static ImagePaintPartialRedraw imapaintpartial = {0, 0, 0, 0, 0};

/* UNDO */

static void undo_copy_tile(UndoImageTile *tile, ImBuf *tmpibuf, ImBuf *ibuf, int restore)
{
	/* copy or swap contents of tile->rect and region in ibuf->rect */
	IMB_rectcpy(tmpibuf, ibuf, 0, 0, tile->x*IMAPAINT_TILE_SIZE,
		tile->y*IMAPAINT_TILE_SIZE, IMAPAINT_TILE_SIZE, IMAPAINT_TILE_SIZE);

	if(ibuf->rect_float) {
		SWAP(void*, tmpibuf->rect_float, tile->rect);
	} else {
		SWAP(void*, tmpibuf->rect, tile->rect);
	}
	
	if(restore)
		IMB_rectcpy(ibuf, tmpibuf, tile->x*IMAPAINT_TILE_SIZE,
			tile->y*IMAPAINT_TILE_SIZE, 0, 0, IMAPAINT_TILE_SIZE, IMAPAINT_TILE_SIZE);
}

static void *image_undo_push_tile(Image *ima, ImBuf *ibuf, ImBuf **tmpibuf, int x_tile, int y_tile)
{
	ListBase *lb= undo_paint_push_get_list(UNDO_PAINT_IMAGE);
	UndoImageTile *tile;
	int allocsize;
	short use_float = ibuf->rect_float ? 1 : 0;

	for(tile=lb->first; tile; tile=tile->next)
		if(tile->x == x_tile && tile->y == y_tile && ima->gen_type == tile->gen_type && ima->source == tile->source)
			if(tile->use_float == use_float)
				if(strcmp(tile->idname, ima->id.name)==0 && strcmp(tile->ibufname, ibuf->name)==0)
					return tile->rect;
	
	if (*tmpibuf==NULL)
		*tmpibuf = IMB_allocImBuf(IMAPAINT_TILE_SIZE, IMAPAINT_TILE_SIZE, 32, IB_rectfloat|IB_rect);
	
	tile= MEM_callocN(sizeof(UndoImageTile), "UndoImageTile");
	BLI_strncpy(tile->idname, ima->id.name, sizeof(tile->idname));
	tile->x= x_tile;
	tile->y= y_tile;

	allocsize= IMAPAINT_TILE_SIZE*IMAPAINT_TILE_SIZE*4;
	allocsize *= (ibuf->rect_float)? sizeof(float): sizeof(char);
	tile->rect= MEM_mapallocN(allocsize, "UndeImageTile.rect");

	BLI_strncpy(tile->ibufname, ibuf->name, sizeof(tile->ibufname));

	tile->gen_type= ima->gen_type;
	tile->source= ima->source;
	tile->use_float= use_float;

	undo_copy_tile(tile, *tmpibuf, ibuf, 0);
	undo_paint_push_count_alloc(UNDO_PAINT_IMAGE, allocsize);

	BLI_addtail(lb, tile);
	
	return tile->rect;
}

static void image_undo_restore(bContext *C, ListBase *lb)
{
	Main *bmain= CTX_data_main(C);
	Image *ima = NULL;
	ImBuf *ibuf, *tmpibuf;
	UndoImageTile *tile;

	tmpibuf= IMB_allocImBuf(IMAPAINT_TILE_SIZE, IMAPAINT_TILE_SIZE, 32,
							IB_rectfloat|IB_rect);
	
	for(tile=lb->first; tile; tile=tile->next) {
		short use_float;

		/* find image based on name, pointer becomes invalid with global undo */
		if(ima && strcmp(tile->idname, ima->id.name)==0) {
			/* ima is valid */
		}
		else {
			ima= BLI_findstring(&bmain->image, tile->idname, offsetof(ID, name));
		}

		ibuf= BKE_image_get_ibuf(ima, NULL);

		if(ima && ibuf && strcmp(tile->ibufname, ibuf->name)!=0) {
			/* current ImBuf filename was changed, probably current frame
			   was changed when paiting on image sequence, rather than storing
			   full image user (which isn't so obvious, btw) try to find ImBuf with
			   matched file name in list of already loaded images */

			ibuf= BLI_findstring(&ima->ibufs, tile->ibufname, offsetof(ImBuf, name));
		}

		if (!ima || !ibuf || !(ibuf->rect || ibuf->rect_float))
			continue;

		if (ima->gen_type != tile->gen_type || ima->source != tile->source)
			continue;

		use_float = ibuf->rect_float ? 1 : 0;

		if (use_float != tile->use_float)
			continue;

		undo_copy_tile(tile, tmpibuf, ibuf, 1);

		GPU_free_image(ima); /* force OpenGL reload */
		if(ibuf->rect_float)
			ibuf->userflags |= IB_RECT_INVALID; /* force recreate of char rect */
		if(ibuf->mipmap[0])
			ibuf->userflags |= IB_MIPMAP_INVALID; /* force mipmap recreatiom */

	}

	IMB_freeImBuf(tmpibuf);
}

static void image_undo_free(ListBase *lb)
{
	UndoImageTile *tile;

	for(tile=lb->first; tile; tile=tile->next)
		MEM_freeN(tile->rect);
}

/* get active image for face depending on old/new shading system */

static Image *imapaint_face_image(const ImagePaintState *s, int face_index)
{
	Image *ima;

	if(scene_use_new_shading_nodes(s->scene)) {
		MFace *mf = s->me->mface+face_index;
		ED_object_get_active_image(s->ob, mf->mat_nr, &ima, NULL, NULL);
	}
	else {
		MTFace *tf = s->me->mtface+face_index;
		ima = tf->tpage;
	}

	return ima;
}

static Image *project_paint_face_image(const ProjPaintState *ps, MTFace *dm_mtface, int face_index)
{
	Image *ima;

	if(ps->do_new_shading_nodes) { /* cached scene_use_new_shading_nodes result */
		MFace *mf = ps->dm_mface+face_index;
		ED_object_get_active_image(ps->ob, mf->mat_nr, &ima, NULL, NULL);
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
	 * projCoSS[0] - ps->screenMin[0]	: zero origin
	 * ... / ps->screen_width				: range from 0.0 to 1.0
	 * ... * ps->buckets_x		: use as a bucket index
	 *
	 * Second multiplication does similar but for vertical offset
	 */
	return	(	(int)(((projCoSS[0] - ps->screenMin[0]) / ps->screen_width)  * ps->buckets_x)) + 
		(	(	(int)(((projCoSS[1] - ps->screenMin[1])  / ps->screen_height) * ps->buckets_y)) * ps->buckets_x);
}

static int project_bucket_offset_safe(const ProjPaintState *ps, const float projCoSS[2])
{
	int bucket_index = project_bucket_offset(ps, projCoSS);
	
	if (bucket_index < 0 || bucket_index >= ps->buckets_x*ps->buckets_y) {	
		return -1;
	}
	else {
		return bucket_index;
	}
}

/* still use 2D X,Y space but this works for verts transformed by a perspective matrix, using their 4th component as a weight */
static void barycentric_weights_v2_persp(float v1[4], float v2[4], float v3[4], float co[2], float w[3])
{
	float wtot_inv, wtot;

	w[0] = area_tri_signed_v2(v2, v3, co) / v1[3];
	w[1] = area_tri_signed_v2(v3, v1, co) / v2[3];
	w[2] = area_tri_signed_v2(v1, v2, co) / v3[3];
	wtot = w[0]+w[1]+w[2];

	if (wtot != 0.0f) {
		wtot_inv = 1.0f/wtot;

		w[0] = w[0]*wtot_inv;
		w[1] = w[1]*wtot_inv;
		w[2] = w[2]*wtot_inv;
	}
	else /* dummy values for zero area face */
		w[0] = w[1] = w[2] = 1.0f/3.0f;
}

static float VecZDepthOrtho(float pt[2], float v1[3], float v2[3], float v3[3], float w[3])
{
	barycentric_weights_v2(v1, v2, v3, pt, w);
	return (v1[2]*w[0]) + (v2[2]*w[1]) + (v3[2]*w[2]);
}

static float VecZDepthPersp(float pt[2], float v1[4], float v2[4], float v3[4], float w[3])
{
	float wtot_inv, wtot;
	float w_tmp[3];

	barycentric_weights_v2_persp(v1, v2, v3, pt, w);
	/* for the depth we need the weights to match what
	 * barycentric_weights_v2 would return, in this case its easiest just to
	 * undo the 4th axis division and make it unit-sum
	 *
	 * don't call barycentric_weights_v2() becaue our callers expect 'w'
	 * to be weighted from the perspective */
	w_tmp[0]= w[0] * v1[3];
	w_tmp[1]= w[1] * v2[3];
	w_tmp[2]= w[2] * v3[3];

	wtot = w_tmp[0]+w_tmp[1]+w_tmp[2];

	if (wtot != 0.0f) {
		wtot_inv = 1.0f/wtot;

		w_tmp[0] = w_tmp[0]*wtot_inv;
		w_tmp[1] = w_tmp[1]*wtot_inv;
		w_tmp[2] = w_tmp[2]*wtot_inv;
	}
	else /* dummy values for zero area face */
		w_tmp[0] = w_tmp[1] = w_tmp[2] = 1.0f/3.0f;
	/* done mimicing barycentric_weights_v2() */

	return (v1[2]*w_tmp[0]) + (v2[2]*w_tmp[1]) + (v3[2]*w_tmp[2]);
}


/* Return the top-most face index that the screen space coord 'pt' touches (or -1) */
static int project_paint_PickFace(const ProjPaintState *ps, float pt[2], float w[3], int *side)
{
	LinkNode *node;
	float w_tmp[3];
	float *v1, *v2, *v3, *v4;
	int bucket_index;
	int face_index;
	int best_side = -1;
	int best_face_index = -1;
	float z_depth_best = FLT_MAX, z_depth;
	MFace *mf;
	
	bucket_index = project_bucket_offset_safe(ps, pt);
	if (bucket_index==-1)
		return -1;
	
	
	
	/* we could return 0 for 1 face buckets, as long as this function assumes
	 * that the point its testing is only every originated from an existing face */
	
	for (node= ps->bucketFaces[bucket_index]; node; node= node->next) {
		face_index = GET_INT_FROM_POINTER(node->link);
		mf= ps->dm_mface + face_index;
		
		v1= ps->screenCoords[mf->v1];
		v2= ps->screenCoords[mf->v2];
		v3= ps->screenCoords[mf->v3];
		
		if (isect_point_tri_v2(pt, v1, v2, v3)) {
			if (ps->is_ortho)	z_depth= VecZDepthOrtho(pt, v1, v2, v3, w_tmp);
			else				z_depth= VecZDepthPersp(pt, v1, v2, v3, w_tmp);
			
			if (z_depth < z_depth_best) {
				best_face_index = face_index;
				best_side = 0;
				z_depth_best = z_depth;
				copy_v3_v3(w, w_tmp);
			}
		}
		else if (mf->v4) {
			v4= ps->screenCoords[mf->v4];
			
			if (isect_point_tri_v2(pt, v1, v3, v4)) {
				if (ps->is_ortho)	z_depth= VecZDepthOrtho(pt, v1, v3, v4, w_tmp);
				else				z_depth= VecZDepthPersp(pt, v1, v3, v4, w_tmp);

				if (z_depth < z_depth_best) {
					best_face_index = face_index;
					best_side= 1;
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
static void uvco_to_wrapped_pxco(float uv[2], int ibuf_x, int ibuf_y, float *x, float *y)
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
static int project_paint_PickColor(const ProjPaintState *ps, float pt[2], float *rgba_fp, unsigned char *rgba, const int interp)
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
	ibuf = ima->ibufs.first; /* we must have got the imbuf before getting here */
	if (!ibuf) return 0;
	
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
				IMAPAINT_FLOAT_RGBA_TO_CHAR(rgba, rgba_tmp_f);
			}
		}
		else {
			if (rgba) {
				bilinear_interpolation_color_wrap(ibuf, rgba, NULL, x, y);
			}
			else {
				unsigned char rgba_tmp[4];
				bilinear_interpolation_color_wrap(ibuf, rgba_tmp, NULL, x, y);
				IMAPAINT_CHAR_RGBA_TO_FLOAT(rgba_fp, rgba_tmp);
			}
		}
	}
	else {
		//xi = (int)((uv[0]*ibuf->x) + 0.5f);
		//yi = (int)((uv[1]*ibuf->y) + 0.5f);
		//if (xi<0 || xi>=ibuf->x  ||  yi<0 || yi>=ibuf->y) return 0;
		
		/* wrap */
		xi = ((int)(uv[0]*ibuf->x)) % ibuf->x;
		if (xi<0) xi += ibuf->x;
		yi = ((int)(uv[1]*ibuf->y)) % ibuf->y;
		if (yi<0) yi += ibuf->y;
		
		
		if (rgba) {
			if (ibuf->rect_float) {
				float *rgba_tmp_fp = ibuf->rect_float + (xi + yi * ibuf->x * 4);
				IMAPAINT_FLOAT_RGBA_TO_CHAR(rgba, rgba_tmp_fp);
			}
			else {
				*((unsigned int *)rgba) = *(unsigned int *)(((char *)ibuf->rect) + ((xi + yi * ibuf->x) * 4));
			}
		}
		
		if (rgba_fp) {
			if (ibuf->rect_float) {
				copy_v4_v4(rgba_fp, ((float *)ibuf->rect_float + ((xi + yi * ibuf->x) * 4)));
			}
			else {
				char *tmp_ch= ((char *)ibuf->rect) + ((xi + yi * ibuf->x) * 4);
				IMAPAINT_CHAR_RGBA_TO_FLOAT(rgba_fp, tmp_ch);
			}
		}
	}
	return 1;
}

/* Check if 'pt' is infront of the 3 verts on the Z axis (used for screenspace occlusuion test)
 * return...
 *  0	: no occlusion
 * -1	: no occlusion but 2D intersection is true (avoid testing the other half of a quad)
 *  1	: occluded
	2	: occluded with w[3] weights set (need to know in some cases) */

static int project_paint_occlude_ptv(float pt[3], float v1[4], float v2[4], float v3[4], float w[3], int is_ortho)
{
	/* if all are behind us, return false */
	if(v1[2] > pt[2] && v2[2] > pt[2] && v3[2] > pt[2])
		return 0;
		
	/* do a 2D point in try intersection */
	if (!isect_point_tri_v2(pt, v1, v2, v3))
		return 0; /* we know there is  */
	

	/* From here on we know there IS an intersection */
	/* if ALL of the verts are infront of us then we know it intersects ? */
	if(v1[2] < pt[2] && v2[2] < pt[2] && v3[2] < pt[2]) {
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


static int project_paint_occlude_ptv_clip(
		const ProjPaintState *ps, const MFace *mf,
		float pt[3], float v1[4], float v2[4], float v3[4],
		const int side )
{
	float w[3], wco[3];
	int ret = project_paint_occlude_ptv(pt, v1, v2, v3, w, ps->is_ortho);

	if (ret <= 0)
		return ret;

	if (ret==1) { /* weights not calculated */
		if (ps->is_ortho)	barycentric_weights_v2(v1, v2, v3, pt, w);
		else				barycentric_weights_v2_persp(v1, v2, v3, pt, w);
	}

	/* Test if we're in the clipped area, */
	if (side)	interp_v3_v3v3v3(wco, ps->dm_mvert[mf->v1].co, ps->dm_mvert[mf->v3].co, ps->dm_mvert[mf->v4].co, w);
	else		interp_v3_v3v3v3(wco, ps->dm_mvert[mf->v1].co, ps->dm_mvert[mf->v2].co, ps->dm_mvert[mf->v3].co, w);
	
	if(!ED_view3d_test_clipping(ps->rv3d, wco, 1)) {
		return 1;
	}
	
	return -1;
}


/* Check if a screenspace location is occluded by any other faces
 * check, pixelScreenCo must be in screenspace, its Z-Depth only needs to be used for comparison
 * and dosn't need to be correct in relation to X and Y coords (this is the case in perspective view) */
static int project_bucket_point_occluded(const ProjPaintState *ps, LinkNode *bucketFace, const int orig_face, float pixelScreenCo[4])
{
	MFace *mf;
	int face_index;
	int isect_ret;
	float w[3]; /* not needed when clipping */
	const short do_clip= ps->rv3d ? ps->rv3d->rflag & RV3D_CLIPPING : 0;
	
	/* we could return 0 for 1 face buckets, as long as this function assumes
	 * that the point its testing is only every originated from an existing face */

	for (; bucketFace; bucketFace = bucketFace->next) {
		face_index = GET_INT_FROM_POINTER(bucketFace->link);

		if (orig_face != face_index) {
			mf = ps->dm_mface + face_index;
			if(do_clip)
				isect_ret = project_paint_occlude_ptv_clip(ps, mf, pixelScreenCo, ps->screenCoords[mf->v1], ps->screenCoords[mf->v2], ps->screenCoords[mf->v3], 0);
			else
				isect_ret = project_paint_occlude_ptv(pixelScreenCo, ps->screenCoords[mf->v1], ps->screenCoords[mf->v2], ps->screenCoords[mf->v3], w, ps->is_ortho);

			/* Note, if isect_ret==-1 then we dont want to test the other side of the quad */
			if (isect_ret==0 && mf->v4) {
				if(do_clip)
					isect_ret = project_paint_occlude_ptv_clip(ps, mf, pixelScreenCo, ps->screenCoords[mf->v1], ps->screenCoords[mf->v3], ps->screenCoords[mf->v4], 1);
				else
					isect_ret = project_paint_occlude_ptv(pixelScreenCo, ps->screenCoords[mf->v1], ps->screenCoords[mf->v3], ps->screenCoords[mf->v4], w, ps->is_ortho);
			}
			if (isect_ret>=1) {
				/* TODO - we may want to cache the first hit,
				 * it is not possible to swap the face order in the list anymore */
				return 1;
			}
		}
	}
	return 0;
}

/* basic line intersection, could move to math_geom.c, 2 points with a horiz line
 * 1 for an intersection, 2 if the first point is aligned, 3 if the second point is aligned */
#define ISECT_TRUE 1
#define ISECT_TRUE_P1 2
#define ISECT_TRUE_P2 3
static int line_isect_y(const float p1[2], const float p2[2], const float y_level, float *x_isect)
{
	float y_diff;
	
	if (y_level==p1[1]) { /* are we touching the first point? - no interpolation needed */
		*x_isect = p1[0];
		return ISECT_TRUE_P1;
	}
	if (y_level==p2[1]) { /* are we touching the second point? - no interpolation needed */
		*x_isect = p2[0];
		return ISECT_TRUE_P2;
	}
	
	y_diff= fabsf(p1[1]-p2[1]); /* yuck, horizontal line, we cant do much here */
	
	if (y_diff < 0.000001f) {
		*x_isect = (p1[0]+p2[0]) * 0.5f;
		return ISECT_TRUE;		
	}
	
	if (p1[1] > y_level && p2[1] < y_level) {
		*x_isect = (p2[0]*(p1[1]-y_level) + p1[0]*(y_level-p2[1])) / y_diff;  /*(p1[1]-p2[1]);*/
		return ISECT_TRUE;
	}
	else if (p1[1] < y_level && p2[1] > y_level) {
		*x_isect = (p2[0]*(y_level-p1[1]) + p1[0]*(p2[1]-y_level)) / y_diff;  /*(p2[1]-p1[1]);*/
		return ISECT_TRUE;
	}
	else {
		return 0;
	}
}

static int line_isect_x(const float p1[2], const float p2[2], const float x_level, float *y_isect)
{
	float x_diff;
	
	if (x_level==p1[0]) { /* are we touching the first point? - no interpolation needed */
		*y_isect = p1[1];
		return ISECT_TRUE_P1;
	}
	if (x_level==p2[0]) { /* are we touching the second point? - no interpolation needed */
		*y_isect = p2[1];
		return ISECT_TRUE_P2;
	}
	
	x_diff= fabsf(p1[0]-p2[0]); /* yuck, horizontal line, we cant do much here */
	
	if (x_diff < 0.000001f) { /* yuck, vertical line, we cant do much here */
		*y_isect = (p1[0]+p2[0]) * 0.5f;
		return ISECT_TRUE;		
	}
	
	if (p1[0] > x_level && p2[0] < x_level) {
		*y_isect = (p2[1]*(p1[0]-x_level) + p1[1]*(x_level-p2[0])) / x_diff; /*(p1[0]-p2[0]);*/
		return ISECT_TRUE;
	}
	else if (p1[0] < x_level && p2[0] > x_level) {
		*y_isect = (p2[1]*(x_level-p1[0]) + p1[1]*(p2[0]-x_level)) / x_diff; /*(p2[0]-p1[0]);*/
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
static int cmp_uv(const float vec2a[2], const float vec2b[2])
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
	
	return ((fabsf(xa-xb) < PROJ_GEOM_TOLERANCE) && (fabsf(ya-yb) < PROJ_GEOM_TOLERANCE)) ? 1:0;
}
#endif

/* set min_px and max_px to the image space bounds of the UV coords 
 * return zero if there is no area in the returned rectangle */
#ifndef PROJ_DEBUG_NOSEAMBLEED
static int pixel_bounds_uv(
		const float uv1[2], const float uv2[2], const float uv3[2], const float uv4[2],
		rcti *bounds_px,
		const int ibuf_x, const int ibuf_y,
		int is_quad
) {
	float min_uv[2], max_uv[2]; /* UV bounds */
	
	INIT_MINMAX2(min_uv, max_uv);
	
	DO_MINMAX2(uv1, min_uv, max_uv);
	DO_MINMAX2(uv2, min_uv, max_uv);
	DO_MINMAX2(uv3, min_uv, max_uv);
	if (is_quad)
		DO_MINMAX2(uv4, min_uv, max_uv);
	
	bounds_px->xmin = (int)(ibuf_x * min_uv[0]);
	bounds_px->ymin = (int)(ibuf_y * min_uv[1]);
	
	bounds_px->xmax = (int)(ibuf_x * max_uv[0]) +1;
	bounds_px->ymax = (int)(ibuf_y * max_uv[1]) +1;
	
	/*printf("%d %d %d %d \n", min_px[0], min_px[1], max_px[0], max_px[1]);*/
	
	/* face uses no UV area when quantized to pixels? */
	return (bounds_px->xmin == bounds_px->xmax || bounds_px->ymin == bounds_px->ymax) ? 0 : 1;
}
#endif

static int pixel_bounds_array(float (* uv)[2], rcti *bounds_px, const int ibuf_x, const int ibuf_y, int tot)
{
	float min_uv[2], max_uv[2]; /* UV bounds */
	
	if (tot==0) {
		return 0;
	}
	
	INIT_MINMAX2(min_uv, max_uv);
	
	while (tot--) {
		DO_MINMAX2((*uv), min_uv, max_uv);
		uv++;
	}
	
	bounds_px->xmin = (int)(ibuf_x * min_uv[0]);
	bounds_px->ymin = (int)(ibuf_y * min_uv[1]);
	
	bounds_px->xmax = (int)(ibuf_x * max_uv[0]) +1;
	bounds_px->ymax = (int)(ibuf_y * max_uv[1]) +1;
	
	/*printf("%d %d %d %d \n", min_px[0], min_px[1], max_px[0], max_px[1]);*/
	
	/* face uses no UV area when quantized to pixels? */
	return (bounds_px->xmin == bounds_px->xmax || bounds_px->ymin == bounds_px->ymax) ? 0 : 1;
}

#ifndef PROJ_DEBUG_NOSEAMBLEED

/* This function returns 1 if this face has a seam along the 2 face-vert indices
 * 'orig_i1_fidx' and 'orig_i2_fidx' */
static int check_seam(const ProjPaintState *ps, const int orig_face, const int orig_i1_fidx, const int orig_i2_fidx, int *other_face, int *orig_fidx)
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
			if		(mf->v1==i1)			i1_fidx = 0;
			else if	(mf->v2==i1)			i1_fidx = 1;
			else if	(mf->v3==i1)			i1_fidx = 2;
			else if	(mf->v4 && mf->v4==i1)	i1_fidx = 3;
			
			if		(mf->v1==i2)			i2_fidx = 0;
			else if	(mf->v2==i2)			i2_fidx = 1;
			else if	(mf->v3==i2)			i2_fidx = 2;
			else if	(mf->v4 && mf->v4==i2)	i2_fidx = 3;
			
			/* Only need to check if 'i2_fidx' is valid because we know i1_fidx is the same vert on both faces */
			if (i2_fidx != -1) {
				Image *tpage = project_paint_face_image(ps, ps->dm_mtface, face_index);
				Image *orig_tpage = project_paint_face_image(ps, ps->dm_mtface, orig_face);

				/* This IS an adjacent face!, now lets check if the UVs are ok */
				tf = ps->dm_mtface + face_index;
				
				/* set up the other face */
				*other_face = face_index;
				*orig_fidx = (i1_fidx < i2_fidx) ? i1_fidx : i2_fidx;
				
				/* first test if they have the same image */
				if (	(orig_tpage == tpage) &&
						cmp_uv(orig_tf->uv[orig_i1_fidx], tf->uv[i1_fidx]) &&
						cmp_uv(orig_tf->uv[orig_i2_fidx], tf->uv[i2_fidx]) )
				{
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
static void uv_image_outset(float (*orig_uv)[2], float (*outset_uv)[2], const float scaler, const int ibuf_x, const int ibuf_y, const int is_quad)
{
	float a1, a2, a3, a4=0.0f;
	float puv[4][2]; /* pixelspace uv's */
	float no1[2], no2[2], no3[2], no4[2]; /* normals */
	float dir1[2], dir2[2], dir3[2], dir4[2];
	float ibuf_inv[2];

	ibuf_inv[0]= 1.0f / (float)ibuf_x;
	ibuf_inv[1]= 1.0f / (float)ibuf_y;

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

	/* TODO - angle_normalized_v2v2(...) * (M_PI/180.0f)
	 * This is incorrect. Its already given radians but without it wont work.
	 * need to look into a fix - campbell */
	if (is_quad) {
		a1 = shell_angle_to_dist(angle_normalized_v2v2(dir4, dir1) * ((float)M_PI/180.0f));
		a2 = shell_angle_to_dist(angle_normalized_v2v2(dir1, dir2) * ((float)M_PI/180.0f));
		a3 = shell_angle_to_dist(angle_normalized_v2v2(dir2, dir3) * ((float)M_PI/180.0f));
		a4 = shell_angle_to_dist(angle_normalized_v2v2(dir3, dir4) * ((float)M_PI/180.0f));
	}
	else {
		a1 = shell_angle_to_dist(angle_normalized_v2v2(dir3, dir1) * ((float)M_PI/180.0f));
		a2 = shell_angle_to_dist(angle_normalized_v2v2(dir1, dir2) * ((float)M_PI/180.0f));
		a3 = shell_angle_to_dist(angle_normalized_v2v2(dir2, dir3) * ((float)M_PI/180.0f));
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
		mul_v2_fl(no1, a1*scaler);
		mul_v2_fl(no2, a2*scaler);
		mul_v2_fl(no3, a3*scaler);
		mul_v2_fl(no4, a4*scaler);
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
		mul_v2_fl(no1, a1*scaler);
		mul_v2_fl(no2, a2*scaler);
		mul_v2_fl(no3, a3*scaler);
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
	
	do {
		if ((ps->faceSeamFlags[face_index] & (1<<fidx1|16<<fidx1)) == 0) {
			if (check_seam(ps, face_index, fidx1, fidx2, &other_face, &other_fidx)) {
				ps->faceSeamFlags[face_index] |= 1<<fidx1;
				if (other_face != -1)
					ps->faceSeamFlags[other_face] |= 1<<other_fidx;
			}
			else {
				ps->faceSeamFlags[face_index] |= 16<<fidx1;
				if (other_face != -1)
					ps->faceSeamFlags[other_face] |= 16<<other_fidx; /* second 4 bits for disabled */
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
		float uv[2],
		float v1co[3], float v2co[3], float v3co[3], /* Screenspace coords */
		float uv1co[2], float uv2co[2], float uv3co[2],
		float pixelScreenCo[4],
		float w[3])
{
	barycentric_weights_v2(uv1co, uv2co, uv3co, uv, w);
	interp_v3_v3v3v3(pixelScreenCo, v1co, v2co, v3co, w);
}

/* same as screen_px_from_ortho except we need to take into account
 * the perspective W coord for each vert */
static void screen_px_from_persp(
		float uv[2],
		float v1co[4], float v2co[4], float v3co[4], /* screenspace coords */
		float uv1co[2], float uv2co[2], float uv3co[2],
		float pixelScreenCo[4],
		float w[3])
{

	float wtot_inv, wtot;
	barycentric_weights_v2(uv1co, uv2co, uv3co, uv, w);
	
	/* re-weight from the 4th coord of each screen vert */
	w[0] *= v1co[3];
	w[1] *= v2co[3];
	w[2] *= v3co[3];
	
	wtot = w[0]+w[1]+w[2];
	
	if (wtot > 0.0f) {
		wtot_inv = 1.0f / wtot;
		w[0] *= wtot_inv;
		w[1] *= wtot_inv;
		w[2] *= wtot_inv;
	}
	else {
		w[0] = w[1] = w[2] = 1.0f/3.0f; /* dummy values for zero area face */
	}
	/* done re-weighting */
	
	interp_v3_v3v3v3(pixelScreenCo, v1co, v2co, v3co, w);
}

static void project_face_pixel(const MTFace *tf_other, ImBuf *ibuf_other, const float w[3], int side, unsigned char rgba_ub[4], float rgba_f[4])
{
	float *uvCo1, *uvCo2, *uvCo3;
	float uv_other[2], x, y;
	
	uvCo1 =  (float *)tf_other->uv[0];
	if (side==1) {
		uvCo2 =  (float *)tf_other->uv[2];
		uvCo3 =  (float *)tf_other->uv[3];
	}
	else {
		uvCo2 =  (float *)tf_other->uv[1];
		uvCo3 =  (float *)tf_other->uv[2];
	}
	
	interp_v2_v2v2v2(uv_other, uvCo1, uvCo2, uvCo3, (float*)w);
	
	/* use */
	uvco_to_wrapped_pxco(uv_other, ibuf_other->x, ibuf_other->y, &x, &y);
	
	
	if (ibuf_other->rect_float) { /* from float to float */
		bilinear_interpolation_color_wrap(ibuf_other, NULL, rgba_f, x, y);
	}
	else { /* from char to float */
		bilinear_interpolation_color_wrap(ibuf_other, rgba_ub, NULL, x, y);
	}
		
}

/* run this outside project_paint_uvpixel_init since pixels with mask 0 dont need init */
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
		
		if (other_tpage && (ibuf_other = BKE_image_get_ibuf(other_tpage, NULL))) {
			/* BKE_image_get_ibuf - TODO - this may be slow */
			unsigned char rgba_ub[4];
			float rgba_f[4];
			
			project_face_pixel(tf_other, ibuf_other, w, side, rgba_ub, rgba_f);
			
			if (ibuf_other->rect_float) { /* from float to float */
				mask = ((rgba_f[0]+rgba_f[1]+rgba_f[2])/3.0f) * rgba_f[3];
			}
			else { /* from char to float */
				mask = ((rgba_ub[0]+rgba_ub[1]+rgba_ub[2])/(256*3.0f)) * (rgba_ub[3]/256.0f);
			}
			
			if (!ps->do_layer_stencil_inv) /* matching the gimps layer mask black/white rules, white==full opacity */
				mask = (1.0f - mask);

			if (mask == 0.0f) {
				return 0.0f;
			}
		}
		else {
			return 0.0f;
		}
	} else {
		mask = 1.0f;
	}
	
	/* calculate mask */
	if (ps->do_mask_normal) {
		MFace *mf = ps->dm_mface + face_index;
		short *no1, *no2, *no3;
		float no[3], angle;
		no1 = ps->dm_mvert[mf->v1].no;
		if (side==1) {
			no2 = ps->dm_mvert[mf->v3].no;
			no3 = ps->dm_mvert[mf->v4].no;
		}
		else {
			no2 = ps->dm_mvert[mf->v2].no;
			no3 = ps->dm_mvert[mf->v3].no;
		}
		
		no[0] = w[0]*no1[0] + w[1]*no2[0] + w[2]*no3[0];
		no[1] = w[0]*no1[1] + w[1]*no2[1] + w[2]*no3[1];
		no[2] = w[0]*no1[2] + w[1]*no2[2] + w[2]*no3[2];
		normalize_v3(no);
		
		/* now we can use the normal as a mask */
		if (ps->is_ortho) {
			angle = angle_normalized_v3v3((float *)ps->viewDir, no);
		}
		else {
			/* Annoying but for the perspective view we need to get the pixels location in 3D space :/ */
			float viewDirPersp[3];
			float *co1, *co2, *co3;
			co1 = ps->dm_mvert[mf->v1].co;
			if (side==1) {
				co2 = ps->dm_mvert[mf->v3].co;
				co3 = ps->dm_mvert[mf->v4].co;
			}
			else {
				co2 = ps->dm_mvert[mf->v2].co;
				co3 = ps->dm_mvert[mf->v3].co;
			}

			/* Get the direction from the viewPoint to the pixel and normalize */
			viewDirPersp[0] = (ps->viewPos[0] - (w[0]*co1[0] + w[1]*co2[0] + w[2]*co3[0]));
			viewDirPersp[1] = (ps->viewPos[1] - (w[0]*co1[1] + w[1]*co2[1] + w[2]*co3[1]));
			viewDirPersp[2] = (ps->viewPos[2] - (w[0]*co1[2] + w[1]*co2[2] + w[2]*co3[2]));
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
	
	// This only works when the opacity dosnt change while painting, stylus pressure messes with this
	// so dont use it.
	// if (ps->is_airbrush==0) mask *= brush_alpha(ps->brush);
	
	return mask;
}

/* run this function when we know a bucket's, face's pixel can be initialized,
 * return the ProjPixel which is added to 'ps->bucketRect[bucket_index]' */
static ProjPixel *project_paint_uvpixel_init(
		const ProjPaintState *ps,
		MemArena *arena,
		const ImBuf *ibuf,
		short x_px, short y_px,
		const float mask,
		const int face_index,
		const int image_index,
		const float pixelScreenCo[4],
		const int side,
		const float w[3])
{
	ProjPixel *projPixel;
	short size;
	
	/* wrap pixel location */
	x_px = x_px % ibuf->x;
	if (x_px<0) x_px += ibuf->x;
	y_px = y_px % ibuf->y;
	if (y_px<0) y_px += ibuf->y;
	
	if (ps->tool==PAINT_TOOL_CLONE) {
		size = sizeof(ProjPixelClone);
	}
	else if (ps->tool==PAINT_TOOL_SMEAR) {
		size = sizeof(ProjPixelClone);
	}
	else {
		size = sizeof(ProjPixel);
	}
	
	projPixel = (ProjPixel *)BLI_memarena_alloc(arena, size);
	//memset(projPixel, 0, size);
	
	if (ibuf->rect_float) {
		projPixel->pixel.f_pt = (float *)ibuf->rect_float + ((x_px + y_px * ibuf->x) * 4);
		projPixel->origColor.f[0] = projPixel->newColor.f[0] = projPixel->pixel.f_pt[0];  
		projPixel->origColor.f[1] = projPixel->newColor.f[1] = projPixel->pixel.f_pt[1];  
		projPixel->origColor.f[2] = projPixel->newColor.f[2] = projPixel->pixel.f_pt[2];  
		projPixel->origColor.f[3] = projPixel->newColor.f[3] = projPixel->pixel.f_pt[3];  
	}
	else {
		projPixel->pixel.ch_pt = ((unsigned char *)ibuf->rect + ((x_px + y_px * ibuf->x) * 4));
		projPixel->origColor.uint = projPixel->newColor.uint = *projPixel->pixel.uint_pt;
	}
	
	/* screenspace unclamped, we could keep its z and w values but dont need them at the moment */
	copy_v2_v2(projPixel->projCoSS, pixelScreenCo);
	
	projPixel->x_px = x_px;
	projPixel->y_px = y_px;
	
	projPixel->mask = (unsigned short)(mask * 65535);
	projPixel->mask_max = 0;
	
	/* which bounding box cell are we in?, needed for undo */
	projPixel->bb_cell_index = ((int)(((float)x_px / (float)ibuf->x) * PROJ_BOUNDBOX_DIV)) +
	                           ((int)(((float)y_px / (float)ibuf->y) * PROJ_BOUNDBOX_DIV)) * PROJ_BOUNDBOX_DIV;
	
	/* done with view3d_project_float inline */
	if (ps->tool==PAINT_TOOL_CLONE) {
		if (ps->dm_mtface_clone) {
			ImBuf *ibuf_other;
			Image *other_tpage = project_paint_face_image(ps, ps->dm_mtface_clone, face_index);
			const MTFace *tf_other = ps->dm_mtface_clone + face_index;
			
			if (other_tpage && (ibuf_other = BKE_image_get_ibuf(other_tpage, NULL))) {
				/* BKE_image_get_ibuf - TODO - this may be slow */
				
				if (ibuf->rect_float) {
					if (ibuf_other->rect_float) { /* from float to float */
						project_face_pixel(tf_other, ibuf_other, w, side, NULL, ((ProjPixelClone *)projPixel)->clonepx.f);
					}
					else { /* from char to float */
						unsigned char rgba_ub[4];
						project_face_pixel(tf_other, ibuf_other, w, side, rgba_ub, NULL);
						IMAPAINT_CHAR_RGBA_TO_FLOAT(((ProjPixelClone *)projPixel)->clonepx.f, rgba_ub);
					}
				}
				else {
					if (ibuf_other->rect_float) { /* float to char */
						float rgba[4];
						project_face_pixel(tf_other, ibuf_other, w, side, NULL, rgba);
						IMAPAINT_FLOAT_RGBA_TO_CHAR(((ProjPixelClone *)projPixel)->clonepx.ch, rgba)
					}
					else { /* char to char */
						project_face_pixel(tf_other, ibuf_other, w, side, ((ProjPixelClone *)projPixel)->clonepx.ch, NULL);
					}
				}
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
	if (ibuf->rect_float)	projPixel->pixel.f_pt[0] = 0;
	else					projPixel->pixel.ch_pt[0] = 0;
#endif
	projPixel->image_index = image_index;
	
	return projPixel;
}

static int line_clip_rect2f(
		rctf *rect,
		const float l1[2], const float l2[2],
		float l1_clip[2], float l2_clip[2])
{
	/* first account for horizontal, then vertical lines */
	/* horiz */
	if (fabsf(l1[1]-l2[1]) < PROJ_GEOM_TOLERANCE) {
		/* is the line out of range on its Y axis? */
		if (l1[1] < rect->ymin || l1[1] > rect->ymax) {
			return 0;
		}
		/* line is out of range on its X axis */
		if ((l1[0] < rect->xmin && l2[0] < rect->xmin) || (l1[0] > rect->xmax && l2[0] > rect->xmax)) {
			return 0;
		}
		
		
		if (fabsf(l1[0]-l2[0]) < PROJ_GEOM_TOLERANCE) { /* this is a single point  (or close to)*/
			if (BLI_in_rctf(rect, l1[0], l1[1])) {
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
	else if (fabsf(l1[0]-l2[0]) < PROJ_GEOM_TOLERANCE) {
		/* is the line out of range on its X axis? */
		if (l1[0] < rect->xmin || l1[0] > rect->xmax) {
			return 0;
		}
		
		/* line is out of range on its Y axis */
		if ((l1[1] < rect->ymin && l2[1] < rect->ymin) || (l1[1] > rect->ymax && l2[1] > rect->ymax)) {
			return 0;
		}
		
		if (fabsf(l1[1]-l2[1]) < PROJ_GEOM_TOLERANCE) { /* this is a single point  (or close to)*/
			if (BLI_in_rctf(rect, l1[0], l1[1])) {
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
		if (BLI_in_rctf(rect, l1[0], l1[1])) {
			copy_v2_v2(l1_clip, l1);
			ok1 = 1;
		}
		
		if (BLI_in_rctf(rect, l2[0], l2[1])) {
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
 * edge of the face but slightly inside it occlusion tests dont return hits on adjacent faces */
#ifndef PROJ_DEBUG_NOSEAMBLEED
static void scale_quad(float insetCos[4][3], float *origCos[4], const float inset)
{
	float cent[3];
	cent[0] = (origCos[0][0] + origCos[1][0] + origCos[2][0] + origCos[3][0]) / 4.0f;
	cent[1] = (origCos[0][1] + origCos[1][1] + origCos[2][1] + origCos[3][1]) / 4.0f;
	cent[2] = (origCos[0][2] + origCos[1][2] + origCos[2][2] + origCos[3][2]) / 4.0f;
	
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
	cent[0] = (origCos[0][0] + origCos[1][0] + origCos[2][0]) / 3.0f;
	cent[1] = (origCos[0][1] + origCos[1][1] + origCos[2][1]) / 3.0f;
	cent[2] = (origCos[0][2] + origCos[1][2] + origCos[2][2]) / 3.0f;
	
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

static float len_squared_v2v2_alt(const float *v1, const float v2_1, const float v2_2)
{
	float x, y;

	x = v1[0]-v2_1;
	y = v1[1]-v2_2;
	return x*x+y*y;
}

/* note, use a squared value so we can use len_squared_v2v2
 * be sure that you have done a bounds check first or this may fail */
/* only give bucket_bounds as an arg because we need it elsewhere */
static int project_bucket_isect_circle(const float cent[2], const float radius_squared, rctf *bucket_bounds)
{
	 
	/* Would normally to a simple intersection test, however we know the bounds of these 2 already intersect 
	 * so we only need to test if the center is inside the vertical or horizontal bounds on either axis,
	 * this is even less work then an intersection test
	 * 
	if (BLI_in_rctf(bucket_bounds, cent[0], cent[1]))
		return 1;
	 */
	
	if ( (bucket_bounds->xmin <= cent[0] && bucket_bounds->xmax >= cent[0]) ||
	     (bucket_bounds->ymin <= cent[1] && bucket_bounds->ymax >= cent[1]) )
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
 * however in some cases, perspective view will mess up with faces that have minimal screenspace area (viewed from the side)
 * 
 * for this reason its not relyable in this case so we'll use the Simple Barycentric' funcs that only account for points inside the triangle.
 * however switching back to this for ortho is always an option */

static void rect_to_uvspace_ortho(
		rctf *bucket_bounds,
		float *v1coSS, float *v2coSS, float *v3coSS,
		float *uv1co, float *uv2co, float *uv3co,
		float bucket_bounds_uv[4][2],
		const int flip)
{
	float uv[2];
	float w[3];
	
	/* get the UV space bounding box */
	uv[0] = bucket_bounds->xmax;
	uv[1] = bucket_bounds->ymin;
	barycentric_weights_v2(v1coSS, v2coSS, v3coSS, uv, w);
	interp_v2_v2v2v2(bucket_bounds_uv[flip?3:0], uv1co, uv2co, uv3co, w);

	//uv[0] = bucket_bounds->xmax; // set above
	uv[1] = bucket_bounds->ymax;
	barycentric_weights_v2(v1coSS, v2coSS, v3coSS, uv, w);
	interp_v2_v2v2v2(bucket_bounds_uv[flip?2:1], uv1co, uv2co, uv3co, w);

	uv[0] = bucket_bounds->xmin;
	//uv[1] = bucket_bounds->ymax; // set above
	barycentric_weights_v2(v1coSS, v2coSS, v3coSS, uv, w);
	interp_v2_v2v2v2(bucket_bounds_uv[flip?1:2], uv1co, uv2co, uv3co, w);

	//uv[0] = bucket_bounds->xmin; // set above
	uv[1] = bucket_bounds->ymin;
	barycentric_weights_v2(v1coSS, v2coSS, v3coSS, uv, w);
	interp_v2_v2v2v2(bucket_bounds_uv[flip?0:3], uv1co, uv2co, uv3co, w);
}

/* same as above but use barycentric_weights_v2_persp */
static void rect_to_uvspace_persp(
		rctf *bucket_bounds,
		float *v1coSS, float *v2coSS, float *v3coSS,
		float *uv1co, float *uv2co, float *uv3co,
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
	interp_v2_v2v2v2(bucket_bounds_uv[flip?3:0], uv1co, uv2co, uv3co, w);

	//uv[0] = bucket_bounds->xmax; // set above
	uv[1] = bucket_bounds->ymax;
	barycentric_weights_v2_persp(v1coSS, v2coSS, v3coSS, uv, w);
	interp_v2_v2v2v2(bucket_bounds_uv[flip?2:1], uv1co, uv2co, uv3co, w);

	uv[0] = bucket_bounds->xmin;
	//uv[1] = bucket_bounds->ymax; // set above
	barycentric_weights_v2_persp(v1coSS, v2coSS, v3coSS, uv, w);
	interp_v2_v2v2v2(bucket_bounds_uv[flip?1:2], uv1co, uv2co, uv3co, w);

	//uv[0] = bucket_bounds->xmin; // set above
	uv[1] = bucket_bounds->ymin;
	barycentric_weights_v2_persp(v1coSS, v2coSS, v3coSS, uv, w);
	interp_v2_v2v2v2(bucket_bounds_uv[flip?0:3], uv1co, uv2co, uv3co, w);
}

/* This works as we need it to but we can save a few steps and not use it */

#if 0
static float angle_2d_clockwise(const float p1[2], const float p2[2], const float p3[2])
{
	float v1[2], v2[2];
	
	v1[0] = p1[0]-p2[0];	v1[1] = p1[1]-p2[1];
	v2[0] = p3[0]-p2[0];	v2[1] = p3[1]-p2[1];
	
	return -atan2(v1[0]*v2[1] - v1[1]*v2[0], v1[0]*v2[0]+v1[1]*v2[1]);
}
#endif

#define ISECT_1 (1)
#define ISECT_2 (1<<1)
#define ISECT_3 (1<<2)
#define ISECT_4 (1<<3)
#define ISECT_ALL3 ((1<<3)-1)
#define ISECT_ALL4 ((1<<4)-1)

/* limit must be a fraction over 1.0f */
static int IsectPT2Df_limit(float pt[2], float v1[2], float v2[2], float v3[2], float limit)
{
	return ((area_tri_v2(pt,v1,v2) + area_tri_v2(pt,v2,v3) + area_tri_v2(pt,v3,v1)) / (area_tri_v2(v1,v2,v3))) < limit;
}

/* Clip the face by a bucket and set the uv-space bucket_bounds_uv
 * so we have the clipped UV's to do pixel intersection tests with 
 * */
static int float_z_sort_flip(const void *p1, const void *p2)
{
	return (((float *)p1)[2] < ((float *)p2)[2] ? 1:-1);
}

static int float_z_sort(const void *p1, const void *p2)
{
	return (((float *)p1)[2] < ((float *)p2)[2] ?-1:1);
}

static void project_bucket_clip_face(
		const int is_ortho,
		rctf *bucket_bounds,
		float *v1coSS, float *v2coSS, float *v3coSS,
		float *uv1co, float *uv2co, float *uv3co,
		float bucket_bounds_uv[8][2],
		int *tot)
{
	int inside_bucket_flag = 0;
	int inside_face_flag = 0;
	const int flip = ((line_point_side_v2(v1coSS, v2coSS, v3coSS) > 0.0f) != (line_point_side_v2(uv1co, uv2co, uv3co) > 0.0f));
	
	float bucket_bounds_ss[4][2];

	/* get the UV space bounding box */
	inside_bucket_flag |= BLI_in_rctf(bucket_bounds, v1coSS[0], v1coSS[1]);
	inside_bucket_flag |= BLI_in_rctf(bucket_bounds, v2coSS[0], v2coSS[1])		<< 1;
	inside_bucket_flag |= BLI_in_rctf(bucket_bounds, v3coSS[0], v3coSS[1])		<< 2;
	
	if (inside_bucket_flag == ISECT_ALL3) {
		/* all screenspace points are inside the bucket bounding box, this means we dont need to clip and can simply return the UVs */
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
	inside_face_flag |= (IsectPT2Df_limit(bucket_bounds_ss[0], v1coSS, v2coSS, v3coSS, 1+PROJ_GEOM_TOLERANCE) ? ISECT_1 : 0);
	
	bucket_bounds_ss[1][0] = bucket_bounds->xmax;
	bucket_bounds_ss[1][1] = bucket_bounds->ymax;
	inside_face_flag |= (IsectPT2Df_limit(bucket_bounds_ss[1], v1coSS, v2coSS, v3coSS, 1+PROJ_GEOM_TOLERANCE) ? ISECT_2 : 0);

	bucket_bounds_ss[2][0] = bucket_bounds->xmin;
	bucket_bounds_ss[2][1] = bucket_bounds->ymax;
	inside_face_flag |= (IsectPT2Df_limit(bucket_bounds_ss[2], v1coSS, v2coSS, v3coSS, 1+PROJ_GEOM_TOLERANCE) ? ISECT_3 : 0);

	bucket_bounds_ss[3][0] = bucket_bounds->xmin;
	bucket_bounds_ss[3][1] = bucket_bounds->ymin;
	inside_face_flag |= (IsectPT2Df_limit(bucket_bounds_ss[3], v1coSS, v2coSS, v3coSS, 1+PROJ_GEOM_TOLERANCE) ? ISECT_4 : 0);
	
	if (inside_face_flag == ISECT_ALL4) {
		/* bucket is totally inside the screenspace face, we can safely use weights */
		
		if (is_ortho)	rect_to_uvspace_ortho(bucket_bounds, v1coSS, v2coSS, v3coSS, uv1co, uv2co, uv3co, bucket_bounds_uv, flip);
		else			rect_to_uvspace_persp(bucket_bounds, v1coSS, v2coSS, v3coSS, uv1co, uv2co, uv3co, bucket_bounds_uv, flip);
		
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
		
		/* calc center*/
		float cent[2] = {0.0f, 0.0f};
		/*float up[2] = {0.0f, 1.0f};*/
		int i;
		short doubles;
		
		(*tot) = 0;
		
		if (inside_face_flag & ISECT_1)	{ copy_v2_v2(isectVCosSS[*tot], bucket_bounds_ss[0]); (*tot)++; }
		if (inside_face_flag & ISECT_2)	{ copy_v2_v2(isectVCosSS[*tot], bucket_bounds_ss[1]); (*tot)++; }
		if (inside_face_flag & ISECT_3)	{ copy_v2_v2(isectVCosSS[*tot], bucket_bounds_ss[2]); (*tot)++; }
		if (inside_face_flag & ISECT_4)	{ copy_v2_v2(isectVCosSS[*tot], bucket_bounds_ss[3]); (*tot)++; }
		
		if (inside_bucket_flag & ISECT_1) {	copy_v2_v2(isectVCosSS[*tot], v1coSS); (*tot)++; }
		if (inside_bucket_flag & ISECT_2) {	copy_v2_v2(isectVCosSS[*tot], v2coSS); (*tot)++; }
		if (inside_bucket_flag & ISECT_3) {	copy_v2_v2(isectVCosSS[*tot], v3coSS); (*tot)++; }
		
		if ((inside_bucket_flag & (ISECT_1|ISECT_2)) != (ISECT_1|ISECT_2)) {
			if (line_clip_rect2f(bucket_bounds, v1coSS, v2coSS, v1_clipSS, v2_clipSS)) {
				if ((inside_bucket_flag & ISECT_1)==0) { copy_v2_v2(isectVCosSS[*tot], v1_clipSS); (*tot)++; }
				if ((inside_bucket_flag & ISECT_2)==0) { copy_v2_v2(isectVCosSS[*tot], v2_clipSS); (*tot)++; }
			}
		}
		
		if ((inside_bucket_flag & (ISECT_2|ISECT_3)) != (ISECT_2|ISECT_3)) {
			if (line_clip_rect2f(bucket_bounds, v2coSS, v3coSS, v1_clipSS, v2_clipSS)) {
				if ((inside_bucket_flag & ISECT_2)==0) { copy_v2_v2(isectVCosSS[*tot], v1_clipSS); (*tot)++; }
				if ((inside_bucket_flag & ISECT_3)==0) { copy_v2_v2(isectVCosSS[*tot], v2_clipSS); (*tot)++; }
			}
		}	
		
		if ((inside_bucket_flag & (ISECT_3|ISECT_1)) != (ISECT_3|ISECT_1)) {
			if (line_clip_rect2f(bucket_bounds, v3coSS, v1coSS, v1_clipSS, v2_clipSS)) {
				if ((inside_bucket_flag & ISECT_3)==0) { copy_v2_v2(isectVCosSS[*tot], v1_clipSS); (*tot)++; }
				if ((inside_bucket_flag & ISECT_1)==0) { copy_v2_v2(isectVCosSS[*tot], v2_clipSS); (*tot)++; }
			}
		}
		
		
		if ((*tot) < 3) { /* no intersections to speak of */
			*tot = 0;
			return;
		}
	
		/* now we have all points we need, collect their angles and sort them clockwise */
		
		for(i=0; i<(*tot); i++) {
			cent[0] += isectVCosSS[i][0];
			cent[1] += isectVCosSS[i][1];
		}
		cent[0] = cent[0] / (float)(*tot);
		cent[1] = cent[1] / (float)(*tot);
		
		
		
		/* Collect angles for every point around the center point */

		
#if 0	/* uses a few more cycles then the above loop */
		for(i=0; i<(*tot); i++) {
			isectVCosSS[i][2] = angle_2d_clockwise(up, cent, isectVCosSS[i]);
		}
#endif

		v1_clipSS[0] = cent[0]; /* Abuse this var for the loop below */
		v1_clipSS[1] = cent[1] + 1.0f;
		
		for(i=0; i<(*tot); i++) {
			v2_clipSS[0] = isectVCosSS[i][0] - cent[0];
			v2_clipSS[1] = isectVCosSS[i][1] - cent[1];
			isectVCosSS[i][2] = atan2f(v1_clipSS[0]*v2_clipSS[1] - v1_clipSS[1]*v2_clipSS[0], v1_clipSS[0]*v2_clipSS[0]+v1_clipSS[1]*v2_clipSS[1]); 
		}
		
		if (flip)	qsort(isectVCosSS, *tot, sizeof(float)*3, float_z_sort_flip);
		else		qsort(isectVCosSS, *tot, sizeof(float)*3, float_z_sort);
		
		/* remove doubles */
		/* first/last check */
		if (fabsf(isectVCosSS[0][0]-isectVCosSS[(*tot)-1][0]) < PROJ_GEOM_TOLERANCE &&  fabsf(isectVCosSS[0][1]-isectVCosSS[(*tot)-1][1]) < PROJ_GEOM_TOLERANCE) {
			(*tot)--;
		}
		
		/* its possible there is only a few left after remove doubles */
		if ((*tot) < 3) {
			// printf("removed too many doubles A\n");
			*tot = 0;
			return;
		}
		
		doubles = TRUE;
		while (doubles==TRUE) {
			doubles = FALSE;
			for(i=1; i<(*tot); i++) {
				if (fabsf(isectVCosSS[i-1][0]-isectVCosSS[i][0]) < PROJ_GEOM_TOLERANCE &&
					fabsf(isectVCosSS[i-1][1]-isectVCosSS[i][1]) < PROJ_GEOM_TOLERANCE)
				{
					int j;
					for(j=i+1; j<(*tot); j++) {
						isectVCosSS[j-1][0] = isectVCosSS[j][0]; 
						isectVCosSS[j-1][1] = isectVCosSS[j][1]; 
					}
					doubles = TRUE; /* keep looking for more doubles */
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
			for(i=0; i<(*tot); i++) {
				barycentric_weights_v2(v1coSS, v2coSS, v3coSS, isectVCosSS[i], w);
				interp_v2_v2v2v2(bucket_bounds_uv[i], uv1co, uv2co, uv3co, w);
			}
		}
		else {
			for(i=0; i<(*tot); i++) {
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
		if (is_ortho)	rect_to_uvspace_ortho(bucket_bounds, v1coSS, v2coSS, v3coSS, uv1co, uv2co, uv3co, test_uv, flip);
		else				rect_to_uvspace_persp(bucket_bounds, v1coSS, v2coSS, v3coSS, uv1co, uv2co, uv3co, test_uv, flip);
		printf("(  [(%f,%f), (%f,%f), (%f,%f), (%f,%f)], ", test_uv[0][0], test_uv[0][1],   test_uv[1][0], test_uv[1][1],    test_uv[2][0], test_uv[2][1],    test_uv[3][0], test_uv[3][1]);
		
		printf("  [(%f,%f), (%f,%f), (%f,%f)], ", uv1co[0], uv1co[1],   uv2co[0], uv2co[1],    uv3co[0], uv3co[1]);
		
		printf("[");
		for (i=0; i < (*tot); i++) {
			printf("(%f, %f),", bucket_bounds_uv[i][0], bucket_bounds_uv[i][1]);
		}
		printf("]),\\\n");
	}
#endif
}

	/*
# This script creates faces in a blender scene from printed data above.

project_ls = [
...(output from above block)...
]
 
from Blender import Scene, Mesh, Window, sys, Mathutils

import bpy

V = Mathutils.Vector

def main():
	sce = bpy.data.scenes.active
	
	for item in project_ls:
		bb = item[0]
		uv = item[1]
		poly = item[2]
		
		me = bpy.data.meshes.new()
		ob = sce.objects.new(me)
		
		me.verts.extend([V(bb[0]).resize3D(), V(bb[1]).resize3D(), V(bb[2]).resize3D(), V(bb[3]).resize3D()])
		me.faces.extend([(0,1,2,3),])
		me.verts.extend([V(uv[0]).resize3D(), V(uv[1]).resize3D(), V(uv[2]).resize3D()])
		me.faces.extend([(4,5,6),])
		
		vs = [V(p).resize3D() for p in poly]
		print len(vs)
		l = len(me.verts)
		me.verts.extend(vs)
		
		i = l
		while i < len(me.verts):
			ii = i+1
			if ii==len(me.verts):
				ii = l
			me.edges.extend([i, ii])
			i+=1

if __name__ == '__main__':
	main()
 */	


#undef ISECT_1
#undef ISECT_2
#undef ISECT_3
#undef ISECT_4
#undef ISECT_ALL3
#undef ISECT_ALL4

	
/* checks if pt is inside a convex 2D polyline, the polyline must be ordered rotating clockwise
 * otherwise it would have to test for mixed (line_point_side_v2 > 0.0f) cases */
static int IsectPoly2Df(const float pt[2], float uv[][2], const int tot)
{
	int i;
	if (line_point_side_v2(uv[tot-1], uv[0], pt) < 0.0f)
		return 0;
	
	for (i=1; i<tot; i++) {
		if (line_point_side_v2(uv[i-1], uv[i], pt) < 0.0f)
			return 0;
		
	}
	
	return 1;
}
static int IsectPoly2Df_twoside(const float pt[2], float uv[][2], const int tot)
{
	int i;
	int side = (line_point_side_v2(uv[tot-1], uv[0], pt) > 0.0f);
	
	for (i=1; i<tot; i++) {
		if ((line_point_side_v2(uv[i-1], uv[i], pt) > 0.0f) != side)
			return 0;
		
	}
	
	return 1;
}

/* One of the most important function for projectiopn painting, since it selects the pixels to be added into each bucket.
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
	int y;/* Image Y-Pixel */
	float mask;
	float uv[2]; /* Image floating point UV - same as x, y but from 0.0-1.0 */
	
	int side;
	float *v1coSS, *v2coSS, *v3coSS; /* vert co screen-space, these will be assigned to mf->v1,2,3 or mf->v1,3,4 */
	
	float *vCo[4]; /* vertex screenspace coords */
	
	float w[3], wco[3];
	
	float *uv1co, *uv2co, *uv3co; /* for convenience only, these will be assigned to tf->uv[0],1,2 or tf->uv[0],2,3 */
	float pixelScreenCo[4];
	
	rcti bounds_px; /* ispace bounds */
	/* vars for getting uvspace bounds */
	
	float tf_uv_pxoffset[4][2]; /* bucket bounds in UV space so we can init pixels only for this face,  */
	float xhalfpx, yhalfpx;
	const float ibuf_xf = (float)ibuf->x, ibuf_yf = (float)ibuf->y;
	
	int has_x_isect = 0, has_isect = 0; /* for early loop exit */
	
	int i1, i2, i3;
	
	float uv_clip[8][2];
	int uv_clip_tot;
	const short is_ortho = ps->is_ortho;
	const short do_backfacecull = ps->do_backfacecull;
	const short do_clip= ps->rv3d ? ps->rv3d->rflag & RV3D_CLIPPING : 0;
	
	vCo[0] = ps->dm_mvert[mf->v1].co;
	vCo[1] = ps->dm_mvert[mf->v2].co;
	vCo[2] = ps->dm_mvert[mf->v3].co;
	
	
	/* Use tf_uv_pxoffset instead of tf->uv so we can offset the UV half a pixel
	 * this is done so we can avoid offseting all the pixels by 0.5 which causes
	 * problems when wrapping negative coords */
	xhalfpx = (0.5f+   (PROJ_GEOM_TOLERANCE/3.0f)   ) / ibuf_xf;
	yhalfpx = (0.5f+   (PROJ_GEOM_TOLERANCE/4.0f)   ) / ibuf_yf;
	
	/* Note about (PROJ_GEOM_TOLERANCE/x) above...
	  Needed to add this offset since UV coords are often quads aligned to pixels.
	  In this case pixels can be exactly between 2 triangles causing nasty
	  artifacts.
	  
	  This workaround can be removed and painting will still work on most cases
	  but since the first thing most people try is painting onto a quad- better make it work.
	 */



	tf_uv_pxoffset[0][0] = tf->uv[0][0] - xhalfpx;
	tf_uv_pxoffset[0][1] = tf->uv[0][1] - yhalfpx;

	tf_uv_pxoffset[1][0] = tf->uv[1][0] - xhalfpx;
	tf_uv_pxoffset[1][1] = tf->uv[1][1] - yhalfpx;
	
	tf_uv_pxoffset[2][0] = tf->uv[2][0] - xhalfpx;
	tf_uv_pxoffset[2][1] = tf->uv[2][1] - yhalfpx;	
	
	if (mf->v4) {
		vCo[3] = ps->dm_mvert[ mf->v4 ].co;
		
		tf_uv_pxoffset[3][0] = tf->uv[3][0] - xhalfpx;
		tf_uv_pxoffset[3][1] = tf->uv[3][1] - yhalfpx;
		side = 1;
	}
	else {
		side = 0;
	}
	
	do {
		if (side==1) {
			i1=0; i2=2; i3=3;
		}
		else {
			i1=0; i2=1; i3=2;
		}
		
		uv1co = tf_uv_pxoffset[i1]; // was tf->uv[i1];
		uv2co = tf_uv_pxoffset[i2]; // was tf->uv[i2];
		uv3co = tf_uv_pxoffset[i3]; // was tf->uv[i3];

		v1coSS = ps->screenCoords[ (*(&mf->v1 + i1)) ];
		v2coSS = ps->screenCoords[ (*(&mf->v1 + i2)) ];
		v3coSS = ps->screenCoords[ (*(&mf->v1 + i3)) ];
		
		/* This funtion gives is a concave polyline in UV space from the clipped quad and tri*/
		project_bucket_clip_face(
				is_ortho, bucket_bounds,
				v1coSS, v2coSS, v3coSS,
				uv1co, uv2co, uv3co,
				uv_clip, &uv_clip_tot
		);

		/* sometimes this happens, better just allow for 8 intersectiosn even though there should be max 6 */
		/*
		if (uv_clip_tot>6) {
			printf("this should never happen! %d\n", uv_clip_tot);
		}*/
		

		if (pixel_bounds_array(uv_clip, &bounds_px, ibuf->x, ibuf->y, uv_clip_tot)) {

			if(clamp_u) {
				CLAMP(bounds_px.xmin, 0, ibuf->x);
				CLAMP(bounds_px.xmax, 0, ibuf->x);
			}

			if(clamp_v) {
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
					if(	(do_backfacecull		&& IsectPoly2Df(uv, uv_clip, uv_clip_tot)) ||
						(do_backfacecull==0		&& IsectPoly2Df_twoside(uv, uv_clip, uv_clip_tot))) {
						
						has_x_isect = has_isect = 1;
						
						if (is_ortho)	screen_px_from_ortho(uv, v1coSS, v2coSS, v3coSS, uv1co, uv2co, uv3co, pixelScreenCo, w);
						else			screen_px_from_persp(uv, v1coSS, v2coSS, v3coSS, uv1co, uv2co, uv3co, pixelScreenCo, w);
						
						/* a pitty we need to get the worldspace pixel location here */
						if(do_clip) {
							interp_v3_v3v3v3(wco, ps->dm_mvert[ (*(&mf->v1 + i1)) ].co, ps->dm_mvert[ (*(&mf->v1 + i2)) ].co, ps->dm_mvert[ (*(&mf->v1 + i3)) ].co, w);
							if(ED_view3d_test_clipping(ps->rv3d, wco, 1)) {
								continue; /* Watch out that no code below this needs to run */
							}
						}
						
						/* Is this UV visible from the view? - raytrace */
						/* project_paint_PickFace is less complex, use for testing */
						//if (project_paint_PickFace(ps, pixelScreenCo, w, &side) == face_index) {
						if (ps->do_occlude==0 || !project_bucket_point_occluded(ps, bucketFaceNodes, face_index, pixelScreenCo)) {
							
							mask = project_paint_uvpixel_mask(ps, face_index, side, w);
							
							if (mask > 0.0f) {
								BLI_linklist_prepend_arena(
									bucketPixelNodes,
									project_paint_uvpixel_init(ps, arena, ibuf, x, y, mask, face_index, image_index, pixelScreenCo, side, w),
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
				
				
#if 0			/* TODO - investigate why this dosnt work sometimes! it should! */
				/* no intersection for this entire row, after some intersection above means we can quit now */
				if (has_x_isect==0 && has_isect) { 
					break;
				}
#endif
			}
		}
	} while(side--);

	
	
#ifndef PROJ_DEBUG_NOSEAMBLEED
	if (ps->seam_bleed_px > 0.0f) {
		int face_seam_flag;
		
		if (ps->thread_tot > 1)
			BLI_lock_thread(LOCK_CUSTOM1); /* Other threads could be modifying these vars */
		
		face_seam_flag = ps->faceSeamFlags[face_index];
		
		/* are any of our edges un-initialized? */
		if ((face_seam_flag & (PROJ_FACE_SEAM1|PROJ_FACE_NOSEAM1))==0 || 
			(face_seam_flag & (PROJ_FACE_SEAM2|PROJ_FACE_NOSEAM2))==0 || 
			(face_seam_flag & (PROJ_FACE_SEAM3|PROJ_FACE_NOSEAM3))==0 || 
			(face_seam_flag & (PROJ_FACE_SEAM4|PROJ_FACE_NOSEAM4))==0
		) {
			project_face_seams_init(ps, face_index, mf->v4);
			face_seam_flag = ps->faceSeamFlags[face_index];
			//printf("seams - %d %d %d %d\n", flag&PROJ_FACE_SEAM1, flag&PROJ_FACE_SEAM2, flag&PROJ_FACE_SEAM3, flag&PROJ_FACE_SEAM4);
		}
		
		if ((face_seam_flag & (PROJ_FACE_SEAM1|PROJ_FACE_SEAM2|PROJ_FACE_SEAM3|PROJ_FACE_SEAM4))==0) {
			
			if (ps->thread_tot > 1)
				BLI_unlock_thread(LOCK_CUSTOM1); /* Other threads could be modifying these vars */
			
		}
		else {
			/* we have a seam - deal with it! */
			
			/* Now create new UV's for the seam face */
			float (*outset_uv)[2] = ps->faceSeamUVs[face_index];
			float insetCos[4][3]; /* inset face coords.  NOTE!!! ScreenSace for ortho, Worldspace in prespective view */

			float fac;
			float *vCoSS[4]; /* vertex screenspace coords */
			
			float bucket_clip_edges[2][2]; /* store the screenspace coords of the face, clipped by the bucket's screen aligned rectangle */
			float edge_verts_inset_clip[2][3];
			int fidx1, fidx2; /* face edge pairs - loop throuh these ((0,1), (1,2), (2,3), (3,0)) or ((0,1), (1,2), (2,0)) for a tri */
			
			float seam_subsection[4][2];
			float fac1, fac2, ftot;
			
			
			if (outset_uv[0][0]==FLT_MAX) /* first time initialize */
				uv_image_outset(tf_uv_pxoffset, outset_uv, ps->seam_bleed_px, ibuf->x, ibuf->y, mf->v4);
			
			/* ps->faceSeamUVs cant be modified when threading, now this is done we can unlock */
			if (ps->thread_tot > 1)
				BLI_unlock_thread(LOCK_CUSTOM1); /* Other threads could be modifying these vars */
			
			vCoSS[0] = ps->screenCoords[mf->v1];
			vCoSS[1] = ps->screenCoords[mf->v2];
			vCoSS[2] = ps->screenCoords[mf->v3];
			if (mf->v4)
				vCoSS[3] = ps->screenCoords[ mf->v4 ];
			
			/* PROJ_FACE_SCALE_SEAM must be slightly less then 1.0f */
			if (is_ortho) {
				if (mf->v4)	scale_quad(insetCos, vCoSS, PROJ_FACE_SCALE_SEAM);
				else		scale_tri(insetCos, vCoSS, PROJ_FACE_SCALE_SEAM);
			}
			else {
				if (mf->v4)	scale_quad(insetCos, vCo, PROJ_FACE_SCALE_SEAM);
				else		scale_tri(insetCos, vCo, PROJ_FACE_SCALE_SEAM);
			}
			
			side = 0; /* for triangles this wont need to change */
			
			for (fidx1 = 0; fidx1 < (mf->v4 ? 4 : 3); fidx1++) {
				if (mf->v4)		fidx2 = (fidx1==3) ? 0 : fidx1+1; /* next fidx in the face (0,1,2,3) -> (1,2,3,0) */
				else			fidx2 = (fidx1==2) ? 0 : fidx1+1; /* next fidx in the face (0,1,2) -> (1,2,0) */
				
				if (	(face_seam_flag & (1<<fidx1)) && /* 1<<fidx1 -> PROJ_FACE_SEAM# */
						line_clip_rect2f(bucket_bounds, vCoSS[fidx1], vCoSS[fidx2], bucket_clip_edges[0], bucket_clip_edges[1])
				) {

					ftot = len_v2v2(vCoSS[fidx1], vCoSS[fidx2]); /* screenspace edge length */
					
					if (ftot > 0.0f) { /* avoid div by zero */
						if (mf->v4) {
							if (fidx1==2 || fidx2==2)	side= 1;
							else						side= 0;
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
						

						if (pixel_bounds_uv(seam_subsection[0], seam_subsection[1], seam_subsection[2], seam_subsection[3], &bounds_px, ibuf->x, ibuf->y, 1)) {
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
										
										/* We need to find the closest point along the face edge,
										 * getting the screen_px_from_*** wont work because our actual location
										 * is not relevent, since we are outside the face, Use VecLerpf to find
										 * our location on the side of the face's UV */
										/*
										if (is_ortho)	screen_px_from_ortho(ps, uv, v1co, v2co, v3co, uv1co, uv2co, uv3co, pixelScreenCo);
										else					screen_px_from_persp(ps, uv, v1co, v2co, v3co, uv1co, uv2co, uv3co, pixelScreenCo);
										*/
										
										/* Since this is a seam we need to work out where on the line this pixel is */
										//fac = line_point_factor_v2(uv, uv_seam_quad[0], uv_seam_quad[1]);
										
										fac = line_point_factor_v2(uv, seam_subsection[0], seam_subsection[1]);
										if (fac < 0.0f)		{ copy_v3_v3(pixelScreenCo, edge_verts_inset_clip[0]); }
										else if (fac > 1.0f)	{ copy_v3_v3(pixelScreenCo, edge_verts_inset_clip[1]); }
										else				{ interp_v3_v3v3(pixelScreenCo, edge_verts_inset_clip[0], edge_verts_inset_clip[1], fac); }
										
										if (!is_ortho) {
											pixelScreenCo[3] = 1.0f;
											mul_m4_v4((float(*)[4])ps->projectMat, pixelScreenCo); /* cast because of const */
											pixelScreenCo[0] = (float)(ps->winx/2.0f)+(ps->winx/2.0f)*pixelScreenCo[0]/pixelScreenCo[3];
											pixelScreenCo[1] = (float)(ps->winy/2.0f)+(ps->winy/2.0f)*pixelScreenCo[1]/pixelScreenCo[3];
											pixelScreenCo[2] = pixelScreenCo[2]/pixelScreenCo[3]; /* Use the depth for bucket point occlusion */
										}
										
										if (ps->do_occlude==0 || !project_bucket_point_occluded(ps, bucketFaceNodes, face_index, pixelScreenCo)) {
											
											/* Only bother calculating the weights if we intersect */
											if (ps->do_mask_normal || ps->dm_mtface_clone) {
#if 1
												/* get the UV on the line since we want to copy the pixels from there for bleeding */
												float uv_close[2];
												float fac= closest_to_line_v2(uv_close, uv, tf_uv_pxoffset[fidx1], tf_uv_pxoffset[fidx2]);
												if		(fac < 0.0f) copy_v2_v2(uv_close, tf_uv_pxoffset[fidx1]);
												else if	(fac > 1.0f) copy_v2_v2(uv_close, tf_uv_pxoffset[fidx2]);

												if (side) {
													barycentric_weights_v2(tf_uv_pxoffset[0], tf_uv_pxoffset[2], tf_uv_pxoffset[3], uv_close, w);
												}
												else {
													barycentric_weights_v2(tf_uv_pxoffset[0], tf_uv_pxoffset[1], tf_uv_pxoffset[2], uv_close, w);
												}
#else											/* this is buggy with quads, dont use for now */

												/* Cheat, we know where we are along the edge so work out the weights from that */
												fac = fac1 + (fac * (fac2-fac1));

												w[0]=w[1]=w[2]= 0.0;
												if (side) {
													w[fidx1?fidx1-1:0] = 1.0f-fac;
													w[fidx2?fidx2-1:0] = fac;
												}
												else {
													w[fidx1] = 1.0f-fac;
													w[fidx2] = fac;
												}
#endif
											}
											
											/* a pitty we need to get the worldspace pixel location here */
											if(do_clip) {
												if (side)	interp_v3_v3v3v3(wco, ps->dm_mvert[mf->v1].co, ps->dm_mvert[mf->v3].co, ps->dm_mvert[mf->v4].co, w);
												else		interp_v3_v3v3v3(wco, ps->dm_mvert[mf->v1].co, ps->dm_mvert[mf->v2].co, ps->dm_mvert[mf->v3].co, w);

												if(ED_view3d_test_clipping(ps->rv3d, wco, 1)) {
													continue; /* Watch out that no code below this needs to run */
												}
											}
											
											mask = project_paint_uvpixel_mask(ps, face_index, side, w);
											
											if (mask > 0.0f) {
												BLI_linklist_prepend_arena(
													bucketPixelNodes,
													project_paint_uvpixel_init(ps, arena, ibuf, x, y, mask, face_index, image_index, pixelScreenCo, side, w),
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
								
#if 0							/* TODO - investigate why this dosnt work sometimes! it should! */
								/* no intersection for this entire row, after some intersection above means we can quit now */
								if (has_x_isect==0 && has_isect) { 
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
	
	/* incase the rect is outside the mesh 2d bounds */
	CLAMP(bucketMin[0], 0, ps->buckets_x);
	CLAMP(bucketMin[1], 0, ps->buckets_y);
	
	CLAMP(bucketMax[0], 0, ps->buckets_x);
	CLAMP(bucketMax[1], 0, ps->buckets_y);
}

/* set bucket_bounds to a screen space-aligned floating point bound-box */
static void project_bucket_bounds(const ProjPaintState *ps, const int bucket_x, const int bucket_y, rctf *bucket_bounds)
{
	bucket_bounds->xmin =	ps->screenMin[0]+((bucket_x)*(ps->screen_width / ps->buckets_x));		/* left */
	bucket_bounds->xmax =	ps->screenMin[0]+((bucket_x+1)*(ps->screen_width / ps->buckets_x));	/* right */
	
	bucket_bounds->ymin =	ps->screenMin[1]+((bucket_y)*(ps->screen_height / ps->buckets_y));		/* bottom */
	bucket_bounds->ymax =	ps->screenMin[1]+((bucket_y+1)*(ps->screen_height  / ps->buckets_y));	/* top */
}

/* Fill this bucket with pixels from the faces that intersect it.
 * 
 * have bucket_bounds as an argument so we don;t need to give bucket_x/y the rect function needs */
static void project_bucket_init(const ProjPaintState *ps, const int thread_index, const int bucket_index, rctf *bucket_bounds)
{
	LinkNode *node;
	int face_index, image_index=0;
	ImBuf *ibuf = NULL;
	Image *tpage_last = NULL, *tpage;
	Image *ima = NULL;

	if (ps->image_tot==1) {
		/* Simple loop, no context switching */
		ibuf = ps->projImages[0].ibuf;
		ima = ps->projImages[0].ima;

		for (node = ps->bucketFaces[bucket_index]; node; node= node->next) { 
			project_paint_face_init(ps, thread_index, bucket_index, GET_INT_FROM_POINTER(node->link), 0, bucket_bounds, ibuf, ima->tpageflag & IMA_CLAMP_U, ima->tpageflag & IMA_CLAMP_V);
		}
	}
	else {
		
		/* More complicated loop, switch between images */
		for (node = ps->bucketFaces[bucket_index]; node; node= node->next) {
			face_index = GET_INT_FROM_POINTER(node->link);
				
			/* Image context switching */
			tpage = project_paint_face_image(ps, ps->dm_mtface, face_index);
			if (tpage_last != tpage) {
				tpage_last = tpage;

				for (image_index=0; image_index < ps->image_tot; image_index++) {
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

static int project_bucket_face_isect(ProjPaintState *ps, int bucket_x, int bucket_y, const MFace *mf)
{
	/* TODO - replace this with a tricker method that uses sideofline for all screenCoords's edges against the closest bucket corner */
	rctf bucket_bounds;
	float p1[2], p2[2], p3[2], p4[2];
	float *v, *v1,*v2,*v3,*v4=NULL;
	int fidx;
	
	project_bucket_bounds(ps, bucket_x, bucket_y, &bucket_bounds);
	
	/* Is one of the faces verts in the bucket bounds? */
	
	fidx = mf->v4 ? 3:2;
	do {
		v = ps->screenCoords[ (*(&mf->v1 + fidx)) ];
		if (BLI_in_rctf(&bucket_bounds, v[0], v[1])) {
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
	p2[0] = bucket_bounds.xmin;	p2[1] = bucket_bounds.ymax;
	p3[0] = bucket_bounds.xmax;	p3[1] = bucket_bounds.ymax;
	p4[0] = bucket_bounds.xmax;	p4[1] = bucket_bounds.ymin;
		
	if (mf->v4) {
		if ( isect_point_quad_v2(p1, v1, v2, v3, v4) ||
		     isect_point_quad_v2(p2, v1, v2, v3, v4) ||
		     isect_point_quad_v2(p3, v1, v2, v3, v4) ||
		     isect_point_quad_v2(p4, v1, v2, v3, v4) ||

			/* we can avoid testing v3,v1 because another intersection MUST exist if this intersects */
			(isect_line_line_v2(p1, p2, v1, v2) || isect_line_line_v2(p1, p2, v2, v3) || isect_line_line_v2(p1, p2, v3, v4)) ||
			(isect_line_line_v2(p2, p3, v1, v2) || isect_line_line_v2(p2, p3, v2, v3) || isect_line_line_v2(p2, p3, v3, v4)) ||
			(isect_line_line_v2(p3, p4, v1, v2) || isect_line_line_v2(p3, p4, v2, v3) || isect_line_line_v2(p3, p4, v3, v4)) ||
			(isect_line_line_v2(p4, p1, v1, v2) || isect_line_line_v2(p4, p1, v2, v3) || isect_line_line_v2(p4, p1, v3, v4))
		) {
			return 1;
		}
	}
	else {
		if ( isect_point_tri_v2(p1, v1, v2, v3) ||
		     isect_point_tri_v2(p2, v1, v2, v3) ||
		     isect_point_tri_v2(p3, v1, v2, v3) ||
		     isect_point_tri_v2(p4, v1, v2, v3) ||
			/* we can avoid testing v3,v1 because another intersection MUST exist if this intersects */
			(isect_line_line_v2(p1, p2, v1, v2) || isect_line_line_v2(p1, p2, v2, v3)) ||
			(isect_line_line_v2(p2, p3, v1, v2) || isect_line_line_v2(p2, p3, v2, v3)) ||
			(isect_line_line_v2(p3, p4, v1, v2) || isect_line_line_v2(p3, p4, v2, v3)) ||
			(isect_line_line_v2(p4, p1, v1, v2) || isect_line_line_v2(p4, p1, v2, v3))
		) {
			return 1;
		}
	}

	return 0;
}

/* Add faces to the bucket but dont initialize its pixels
 * TODO - when painting occluded, sort the faces on their min-Z and only add faces that faces that are not occluded */
static void project_paint_delayed_face_init(ProjPaintState *ps, const MFace *mf, const int face_index)
{
	float min[2], max[2], *vCoSS;
	int bucketMin[2], bucketMax[2]; /* for  ps->bucketRect indexing */
	int fidx, bucket_x, bucket_y;
	int has_x_isect = -1, has_isect = 0; /* for early loop exit */
	MemArena *arena = ps->arena_mt[0]; /* just use the first thread arena since threading has not started yet */
	
	INIT_MINMAX2(min, max);
	
	fidx = mf->v4 ? 3:2;
	do {
		vCoSS = ps->screenCoords[ *(&mf->v1 + fidx) ];
		DO_MINMAX2(vCoSS, min, max);
	} while (fidx--);
	
	project_paint_bucket_bounds(ps, min, max, bucketMin, bucketMax);
	
	for (bucket_y = bucketMin[1]; bucket_y < bucketMax[1]; bucket_y++) {
		has_x_isect = 0;
		for (bucket_x = bucketMin[0]; bucket_x < bucketMax[0]; bucket_x++) {
			if (project_bucket_face_isect(ps, bucket_x, bucket_y, mf)) {
				int bucket_index= bucket_x + (bucket_y * ps->buckets_x);
				BLI_linklist_prepend_arena(
					&ps->bucketFaces[ bucket_index ],
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
		if (has_x_isect==0 && has_isect) { 
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

static int project_paint_view_clip(View3D *v3d, RegionView3D *rv3d, float *clipsta, float *clipend)
{
	int orth= ED_view3d_clip_range_get(v3d, rv3d, clipsta, clipend);

	if (orth) { /* only needed for ortho */
		float fac = 2.0f / ((*clipend) - (*clipsta));
		*clipsta *= fac;
		*clipend *= fac;
	}

	return orth;
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
	MFace *mf;
	MTFace *tf;
	
	int a, i; /* generic looping vars */
	int image_index = -1, face_index;
	MVert *mv;
	
	MemArena *arena; /* at the moment this is just ps->arena_mt[0], but use this to show were not multithreading */

	const int diameter= 2*brush_size(ps->scene, ps->brush);
	
	/* ---- end defines ---- */
	
	if(ps->source==PROJ_SRC_VIEW)
		ED_view3d_local_clipping(ps->rv3d, ps->ob->obmat); /* faster clipping lookups */

	/* paint onto the derived mesh */
	
	/* Workaround for subsurf selection, try the display mesh first */
	if (ps->source==PROJ_SRC_IMAGE_CAM) {
		/* using render mesh, assume only camera was rendered from */
		ps->dm = mesh_create_derived_render(ps->scene, ps->ob, ps->scene->customdata_mask | CD_MASK_MTFACE);
		ps->dm_release= TRUE;
	}
	else if(ps->ob->derivedFinal && CustomData_has_layer( &ps->ob->derivedFinal->faceData, CD_MTFACE)) {
		ps->dm = ps->ob->derivedFinal;
		ps->dm_release= FALSE;
	}
	else {
		ps->dm = mesh_get_derived_final(ps->scene, ps->ob, ps->scene->customdata_mask | CD_MASK_MTFACE);
		ps->dm_release= TRUE;
	}
	
	if ( !CustomData_has_layer( &ps->dm->faceData, CD_MTFACE) ) {
		
		if(ps->dm_release)
			ps->dm->release(ps->dm);
		
		ps->dm = NULL;
		return; 
	}
	
	ps->dm_mvert = ps->dm->getVertArray(ps->dm);
	ps->dm_mface = ps->dm->getTessFaceArray(ps->dm);
	ps->dm_mtface= ps->dm->getTessFaceDataArray(ps->dm, CD_MTFACE);
	
	ps->dm_totvert = ps->dm->getNumVerts(ps->dm);
	ps->dm_totface = ps->dm->getNumTessFaces(ps->dm);
	
	/* use clone mtface? */
	
	
	/* Note, use the original mesh for getting the clone and mask layer index
	 * this avoids re-generating the derived mesh just to get the new index */
	if (ps->do_layer_clone) {
		//int layer_num = CustomData_get_clone_layer(&ps->dm->faceData, CD_MTFACE);
		int layer_num = CustomData_get_clone_layer(&((Mesh *)ps->ob->data)->fdata, CD_MTFACE);
		if (layer_num != -1)
			ps->dm_mtface_clone = CustomData_get_layer_n(&ps->dm->faceData, CD_MTFACE, layer_num);
		
		if (ps->dm_mtface_clone==NULL || ps->dm_mtface_clone==ps->dm_mtface) {
			ps->do_layer_clone = 0;
			ps->dm_mtface_clone= NULL;
			printf("ACK!\n");
		}
	}
	
	if (ps->do_layer_stencil) {
		//int layer_num = CustomData_get_stencil_layer(&ps->dm->faceData, CD_MTFACE);
		int layer_num = CustomData_get_stencil_layer(&((Mesh *)ps->ob->data)->fdata, CD_MTFACE);
		if (layer_num != -1)
			ps->dm_mtface_stencil = CustomData_get_layer_n(&ps->dm->faceData, CD_MTFACE, layer_num);
		
		if (ps->dm_mtface_stencil==NULL || ps->dm_mtface_stencil==ps->dm_mtface) {
			ps->do_layer_stencil = 0;
			ps->dm_mtface_stencil = NULL;
		}
	}
	
	/* when using subsurf or multires, mface arrays are thrown away, we need to keep a copy */
	if(ps->dm->type != DM_TYPE_CDDM) {
		ps->dm_mvert= MEM_dupallocN(ps->dm_mvert);
		ps->dm_mface= MEM_dupallocN(ps->dm_mface);
		/* looks like these are ok for now.*/
		/*
		ps->dm_mtface= MEM_dupallocN(ps->dm_mtface);
		ps->dm_mtface_clone= MEM_dupallocN(ps->dm_mtface_clone);
		ps->dm_mtface_stencil= MEM_dupallocN(ps->dm_mtface_stencil);
		 */
	}
	
	ps->viewDir[0] = 0.0f;
	ps->viewDir[1] = 0.0f;
	ps->viewDir[2] = 1.0f;
	
	{
		float viewmat[4][4];
		float viewinv[4][4];

		invert_m4_m4(ps->ob->imat, ps->ob->obmat);

		if(ps->source==PROJ_SRC_VIEW) {
			/* normal drawing */
			ps->winx= ps->ar->winx;
			ps->winy= ps->ar->winy;

			copy_m4_m4(viewmat, ps->rv3d->viewmat);
			copy_m4_m4(viewinv, ps->rv3d->viewinv);

			ED_view3d_ob_project_mat_get(ps->rv3d, ps->ob, ps->projectMat);

			ps->is_ortho= project_paint_view_clip(ps->v3d, ps->rv3d, &ps->clipsta, &ps->clipend);
		}
		else {
			/* reprojection */
			float winmat[4][4];
			float vmat[4][4];

			ps->winx= ps->reproject_ibuf->x;
			ps->winy= ps->reproject_ibuf->y;

			if (ps->source==PROJ_SRC_IMAGE_VIEW) {
				/* image stores camera data, tricky */
				IDProperty *idgroup= IDP_GetProperties(&ps->reproject_image->id, 0);
				IDProperty *view_data= IDP_GetPropertyFromGroup(idgroup, PROJ_VIEW_DATA_ID);

				float *array= (float *)IDP_Array(view_data);

				/* use image array, written when creating image */
				memcpy(winmat, array, sizeof(winmat)); array += sizeof(winmat)/sizeof(float);
				memcpy(viewmat, array, sizeof(viewmat)); array += sizeof(viewmat)/sizeof(float);
				ps->clipsta= array[0];
				ps->clipend= array[1];
				ps->is_ortho= array[2] ? 1:0;

				invert_m4_m4(viewinv, viewmat);
			}
			else if (ps->source==PROJ_SRC_IMAGE_CAM) {
				Object *cam_ob= ps->scene->camera;
				CameraParams params;

				/* viewmat & viewinv */
				copy_m4_m4(viewinv, cam_ob->obmat);
				normalize_m4(viewinv);
				invert_m4_m4(viewmat, viewinv);

				/* window matrix, clipping and ortho */
				camera_params_init(&params);
				camera_params_from_object(&params, cam_ob);
				camera_params_compute_viewplane(&params, ps->winx, ps->winy, 1.0f, 1.0f);
				camera_params_compute_matrix(&params);

				copy_m4_m4(winmat, params.winmat);
				ps->clipsta= params.clipsta;
				ps->clipend= params.clipend;
				ps->is_ortho= params.is_ortho;
			}

			/* same as view3d_get_object_project_mat */
			mult_m4_m4m4(vmat, viewmat, ps->ob->obmat);
			mult_m4_m4m4(ps->projectMat, winmat, vmat);
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
	projScreenCo= *ps->screenCoords;
	
	if (ps->is_ortho) {
		for(a=0, mv=ps->dm_mvert; a < ps->dm_totvert; a++, mv++, projScreenCo+=4) {
			mul_v3_m4v3(projScreenCo, ps->projectMat, mv->co);
			
			/* screen space, not clamped */
			projScreenCo[0] = (float)(ps->winx/2.0f)+(ps->winx/2.0f)*projScreenCo[0];
			projScreenCo[1] = (float)(ps->winy/2.0f)+(ps->winy/2.0f)*projScreenCo[1];
			DO_MINMAX2(projScreenCo, ps->screenMin, ps->screenMax);
		}
	}
	else {
		for(a=0, mv=ps->dm_mvert; a < ps->dm_totvert; a++, mv++, projScreenCo+=4) {
			copy_v3_v3(projScreenCo, mv->co);
			projScreenCo[3] = 1.0f;

			mul_m4_v4(ps->projectMat, projScreenCo);

			if (projScreenCo[3] > ps->clipsta) {
				/* screen space, not clamped */
				projScreenCo[0] = (float)(ps->winx/2.0f)+(ps->winx/2.0f)*projScreenCo[0]/projScreenCo[3];
				projScreenCo[1] = (float)(ps->winy/2.0f)+(ps->winy/2.0f)*projScreenCo[1]/projScreenCo[3];
				projScreenCo[2] = projScreenCo[2]/projScreenCo[3]; /* Use the depth for bucket point occlusion */
				DO_MINMAX2(projScreenCo, ps->screenMin, ps->screenMax);
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
	
	if(ps->source==PROJ_SRC_VIEW) {
#ifdef PROJ_DEBUG_WINCLIP
		CLAMP(ps->screenMin[0], (float)(-diameter), (float)(ps->winx + diameter));
		CLAMP(ps->screenMax[0], (float)(-diameter), (float)(ps->winx + diameter));

		CLAMP(ps->screenMin[1], (float)(-diameter), (float)(ps->winy + diameter));
		CLAMP(ps->screenMax[1], (float)(-diameter), (float)(ps->winy + diameter));
#endif
	}
	else { /* reprojection, use bounds */
		ps->screenMin[0]= 0;
		ps->screenMax[0]= (float)(ps->winx);

		ps->screenMin[1]= 0;
		ps->screenMax[1]= (float)(ps->winy);
	}

	/* only for convenience */
	ps->screen_width  = ps->screenMax[0] - ps->screenMin[0];
	ps->screen_height = ps->screenMax[1] - ps->screenMin[1];
	
	ps->buckets_x = (int)(ps->screen_width / (((float)diameter) / PROJ_BUCKET_BRUSH_DIV));
	ps->buckets_y = (int)(ps->screen_height / (((float)diameter) / PROJ_BUCKET_BRUSH_DIV));
	
	/* printf("\tscreenspace bucket division x:%d y:%d\n", ps->buckets_x, ps->buckets_y); */
	
	/* really high values could cause problems since it has to allocate a few
	 * (ps->buckets_x*ps->buckets_y) sized arrays  */
	CLAMP(ps->buckets_x, PROJ_BUCKET_RECT_MIN, PROJ_BUCKET_RECT_MAX);
	CLAMP(ps->buckets_y, PROJ_BUCKET_RECT_MIN, PROJ_BUCKET_RECT_MAX);
	
	ps->bucketRect = (LinkNode **)MEM_callocN(sizeof(LinkNode *) * ps->buckets_x * ps->buckets_y, "paint-bucketRect");
	ps->bucketFaces= (LinkNode **)MEM_callocN(sizeof(LinkNode *) * ps->buckets_x * ps->buckets_y, "paint-bucketFaces");
	
	ps->bucketFlags= (unsigned char *)MEM_callocN(sizeof(char) * ps->buckets_x * ps->buckets_y, "paint-bucketFaces");
#ifndef PROJ_DEBUG_NOSEAMBLEED
	if (ps->seam_bleed_px > 0.0f) {
		ps->vertFaces= (LinkNode **)MEM_callocN(sizeof(LinkNode *) * ps->dm_totvert, "paint-vertFaces");
		ps->faceSeamFlags = (char *)MEM_callocN(sizeof(char) * ps->dm_totface, "paint-faceSeamFlags");
		ps->faceSeamUVs= MEM_mallocN(sizeof(float) * ps->dm_totface * 8, "paint-faceSeamUVs");
	}
#endif
	
	/* Thread stuff
	 * 
	 * very small brushes run a lot slower multithreaded since the advantage with
	 * threads is being able to fill in multiple buckets at once.
	 * Only use threads for bigger brushes. */
	
	if (ps->scene->r.mode & R_FIXED_THREADS) {
		ps->thread_tot = ps->scene->r.threads;
	}
	else {
		ps->thread_tot = BLI_system_thread_count();
	}
	for (a=0; a<ps->thread_tot; a++) {
		ps->arena_mt[a] = BLI_memarena_new(1<<16, "project paint arena");
	}
	
	arena = ps->arena_mt[0]; 
	
	if (ps->do_backfacecull && ps->do_mask_normal) {
		float viewDirPersp[3];
		
		ps->vertFlags = MEM_callocN(sizeof(char) * ps->dm_totvert, "paint-vertFlags");
		
		for(a=0, mv=ps->dm_mvert; a < ps->dm_totvert; a++, mv++) {
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
	

	for(face_index = 0, tf = ps->dm_mtface, mf = ps->dm_mface; face_index < ps->dm_totface; mf++, tf++, face_index++) {
		
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
		
		tpage = project_paint_face_image(ps, ps->dm_mtface, face_index);

		if (tpage && ((((Mesh *)ps->ob->data)->editflag & ME_EDIT_PAINT_MASK)==0 || mf->flag & ME_FACE_SEL)) {
			
			float *v1coSS, *v2coSS, *v3coSS, *v4coSS=NULL;
			
			v1coSS = ps->screenCoords[mf->v1]; 
			v2coSS = ps->screenCoords[mf->v2]; 
			v3coSS = ps->screenCoords[mf->v3];
			if (mf->v4) {
				v4coSS = ps->screenCoords[mf->v4]; 
			}
			
			
			if (!ps->is_ortho) {
				if (	v1coSS[0]==FLT_MAX ||
						v2coSS[0]==FLT_MAX ||
						v3coSS[0]==FLT_MAX ||
						(mf->v4 && v4coSS[0]==FLT_MAX)
				) {
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
			) {
				continue;
			}
			
#endif //PROJ_DEBUG_WINCLIP
	
			
			if (ps->do_backfacecull) {
				if (ps->do_mask_normal) {
					/* Since we are interpolating the normals of faces, we want to make 
					 * sure all the verts are pointing away from the view,
					 * not just the face */
					if (	(ps->vertFlags[mf->v1] & PROJ_VERT_CULL) &&
							(ps->vertFlags[mf->v2] & PROJ_VERT_CULL) &&
							(ps->vertFlags[mf->v3] & PROJ_VERT_CULL) &&
							(mf->v4==0 || ps->vertFlags[mf->v4] & PROJ_VERT_CULL)
							
					) {
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
				
				if (image_index==-1 && BKE_image_get_ibuf(tpage, NULL)) { /* MemArena dosnt have an append func */
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
	
	for (node= image_LinkList, i=0; node; node= node->next, i++, projIma++) {
		projIma->ima = node->link;
		projIma->touch = 0;
		projIma->ibuf = BKE_image_get_ibuf(projIma->ima, NULL);
		projIma->partRedrawRect =  BLI_memarena_alloc(arena, sizeof(ImagePaintPartialRedraw) * PROJ_BOUNDBOX_SQUARED);
		memset(projIma->partRedrawRect, 0, sizeof(ImagePaintPartialRedraw) * PROJ_BOUNDBOX_SQUARED);
	}
	
	/* we have built the array, discard the linked list */
	BLI_linklist_free(image_LinkList, NULL);
}

static void project_paint_begin_clone(ProjPaintState *ps, int mouse[2])
{
	/* setup clone offset */
	if (ps->tool == PAINT_TOOL_CLONE) {
		float projCo[4];
		copy_v3_v3(projCo, give_cursor(ps->scene, ps->v3d));
		mul_m4_v3(ps->ob->imat, projCo);
		
		projCo[3] = 1.0f;
		mul_m4_v4(ps->projectMat, projCo);
		ps->cloneOffset[0] = mouse[0] - ((float)(ps->winx/2.0f)+(ps->winx/2.0f)*projCo[0]/projCo[3]);
		ps->cloneOffset[1] = mouse[1] - ((float)(ps->winy/2.0f)+(ps->winy/2.0f)*projCo[1]/projCo[3]);
	}	
}	

static void project_paint_end(ProjPaintState *ps)
{
	int a;
	
	/* build undo data from original pixel colors */
	if(U.uiflag & USER_GLOBALUNDO) {
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
		int last_tile_width=0;
		
		for(a=0, last_projIma=ps->projImages; a < ps->image_tot; a++, last_projIma++) {
			int size = sizeof(void **) * IMAPAINT_TILE_NUMBER(last_projIma->ibuf->x) * IMAPAINT_TILE_NUMBER(last_projIma->ibuf->y);
			last_projIma->undoRect = (void **) BLI_memarena_alloc(arena, size);
			memset(last_projIma->undoRect, 0, size);
			last_projIma->ibuf->userflags |= IB_BITMAPDIRTY;
		}
		
		for (bucket_index = 0; bucket_index < bucket_tot; bucket_index++) {
			/* loop through all pixels */
			for(pixel_node= ps->bucketRect[bucket_index]; pixel_node; pixel_node= pixel_node->next) {
			
				/* ok we have a pixel, was it modified? */
				projPixel = (ProjPixel *)pixel_node->link;
				
				if (last_image_index != projPixel->image_index) {
					/* set the context */
					last_image_index =	projPixel->image_index;
					last_projIma =		ps->projImages + last_image_index;
					last_tile_width =	IMAPAINT_TILE_NUMBER(last_projIma->ibuf->x);
					is_float =			last_projIma->ibuf->rect_float ? 1 : 0;
				}
				
				
				if (	(is_float == 0 && projPixel->origColor.uint != *projPixel->pixel.uint_pt) || 
								
						(is_float == 1 && 
						(	projPixel->origColor.f[0] != projPixel->pixel.f_pt[0] || 
							projPixel->origColor.f[1] != projPixel->pixel.f_pt[1] ||
							projPixel->origColor.f[2] != projPixel->pixel.f_pt[2] ||
							projPixel->origColor.f[3] != projPixel->pixel.f_pt[3] ))
				) {
					
					x_tile =  projPixel->x_px >> IMAPAINT_TILE_BITS;
					y_tile =  projPixel->y_px >> IMAPAINT_TILE_BITS;
					
					x_round = x_tile * IMAPAINT_TILE_SIZE;
					y_round = y_tile * IMAPAINT_TILE_SIZE;
					
					tile_index = x_tile + y_tile * last_tile_width;
					
					if (last_projIma->undoRect[tile_index]==NULL) {
						/* add the undo tile from the modified image, then write the original colors back into it */
						tilerect = last_projIma->undoRect[tile_index] = image_undo_push_tile(last_projIma->ima, last_projIma->ibuf, is_float ? (&tmpibuf_float):(&tmpibuf) , x_tile, y_tile);
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
						((unsigned int *)tilerect)[ (projPixel->x_px - x_round) + (projPixel->y_px - y_round) * IMAPAINT_TILE_SIZE ] = projPixel->origColor.uint;
					}
				}
			}
		}
		
		if (tmpibuf)		IMB_freeImBuf(tmpibuf);
		if (tmpibuf_float)	IMB_freeImBuf(tmpibuf_float);
	}
	/* done calculating undo data */
	
	MEM_freeN(ps->screenCoords);
	MEM_freeN(ps->bucketRect);
	MEM_freeN(ps->bucketFaces);
	MEM_freeN(ps->bucketFlags);
	
#ifndef PROJ_DEBUG_NOSEAMBLEED
	if (ps->seam_bleed_px > 0.0f) {
		MEM_freeN(ps->vertFaces);
		MEM_freeN(ps->faceSeamFlags);
		MEM_freeN(ps->faceSeamUVs);
	}
#endif
	
	if (ps->vertFlags) MEM_freeN(ps->vertFlags);
	
	for (a=0; a<ps->thread_tot; a++) {
		BLI_memarena_free(ps->arena_mt[a]);
	}
	
	/* copy for subsurf/multires, so throw away */
	if(ps->dm->type != DM_TYPE_CDDM) {
		if(ps->dm_mvert) MEM_freeN(ps->dm_mvert);
		if(ps->dm_mface) MEM_freeN(ps->dm_mface);
		/* looks like these dont need copying */
		/*
		if(ps->dm_mtface) MEM_freeN(ps->dm_mtface);
		if(ps->dm_mtface_clone) MEM_freeN(ps->dm_mtface_clone);
		if(ps->dm_mtface_stencil) MEM_freeN(ps->dm_mtface_stencil);
		*/
	}

	if(ps->dm_release)
		ps->dm->release(ps->dm);
}

/* 1= an undo, -1 is a redo. */
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


static int partial_redraw_array_merge(ImagePaintPartialRedraw *pr, ImagePaintPartialRedraw *pr_other, int tot)
{
	int touch= 0;
	while (tot--) {
		pr->x1 = MIN2(pr->x1, pr_other->x1);
		pr->y1 = MIN2(pr->y1, pr_other->y1);
		
		pr->x2 = MAX2(pr->x2, pr_other->x2);
		pr->y2 = MAX2(pr->y2, pr_other->y2);
		
		if (pr->x2 != -1)
			touch = 1;
		
		pr++; pr_other++;
	}
	
	return touch;
}

/* Loop over all images on this mesh and update any we have touched */
static int project_image_refresh_tagged(ProjPaintState *ps)
{
	ImagePaintPartialRedraw *pr;
	ProjPaintImage *projIma;
	int a,i;
	int redraw = 0;
	
	
	for (a=0, projIma=ps->projImages; a < ps->image_tot; a++, projIma++) {
		if (projIma->touch) {
			/* look over each bound cell */
			for (i=0; i<PROJ_BOUNDBOX_SQUARED; i++) {
				pr = &(projIma->partRedrawRect[i]);
				if (pr->x2 != -1) { /* TODO - use 'enabled' ? */
					imapaintpartial = *pr;
					imapaint_image_update(NULL, projIma->ima, projIma->ibuf, 1); /*last 1 is for texpaint*/
					redraw = 1;
				}
			}
			
			projIma->touch = 0; /* clear for reuse */
		}
	}
	
	return redraw;
}

/* run this per painting onto each mouse location */
static int project_bucket_iter_init(ProjPaintState *ps, const float mval_f[2])
{
	if(ps->source==PROJ_SRC_VIEW) {
		float min_brush[2], max_brush[2];
		const float radius = (float)brush_size(ps->scene, ps->brush);

		/* so we dont have a bucket bounds that is way too small to paint into */
		// if (radius < 1.0f) radius = 1.0f; // this doesn't work yet :/

		min_brush[0] = mval_f[0] - radius;
		min_brush[1] = mval_f[1] - radius;

		max_brush[0] = mval_f[0] + radius;
		max_brush[1] = mval_f[1] + radius;

		/* offset to make this a valid bucket index */
		project_paint_bucket_bounds(ps, min_brush, max_brush, ps->bucketMin, ps->bucketMax);

		/* mouse outside the model areas? */
		if (ps->bucketMin[0]==ps->bucketMax[0] || ps->bucketMin[1]==ps->bucketMax[1]) {
			return 0;
		}

		ps->context_bucket_x = ps->bucketMin[0];
		ps->context_bucket_y = ps->bucketMin[1];
	}
	else { /* reproject: PROJ_SRC_* */
		ps->bucketMin[0]= 0;
		ps->bucketMin[1]= 0;

		ps->bucketMax[0]= ps->buckets_x;
		ps->bucketMax[1]= ps->buckets_y;

		ps->context_bucket_x = 0;
		ps->context_bucket_y = 0;
	}
	return 1;
}


static int project_bucket_iter_next(ProjPaintState *ps, int *bucket_index, rctf *bucket_bounds, const float mval[2])
{
	const int diameter= 2*brush_size(ps->scene, ps->brush);

	if (ps->thread_tot > 1)
		BLI_lock_thread(LOCK_CUSTOM1);
	
	//printf("%d %d \n", ps->context_bucket_x, ps->context_bucket_y);
	
	for ( ; ps->context_bucket_y < ps->bucketMax[1]; ps->context_bucket_y++) {
		for ( ; ps->context_bucket_x < ps->bucketMax[0]; ps->context_bucket_x++) {
			
			/* use bucket_bounds for project_bucket_isect_circle and project_bucket_init*/
			project_bucket_bounds(ps, ps->context_bucket_x, ps->context_bucket_y, bucket_bounds);
			
			if (	(ps->source != PROJ_SRC_VIEW) ||
					project_bucket_isect_circle(mval, (float)(diameter*diameter), bucket_bounds)
			) {
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
	ProjPaintImage *projImages;	/* array of partial redraws */
	
	/* thread settings */
	int thread_index;
} ProjectHandle;

static void blend_color_mix(unsigned char *cp, const unsigned char *cp1, const unsigned char *cp2, const int fac)
{
	/* this and other blending modes previously used >>8 instead of /255. both
	   are not equivalent (>>8 is /256), and the former results in rounding
	   errors that can turn colors black fast after repeated blending */
	const int mfac= 255-fac;

	cp[0]= (mfac*cp1[0]+fac*cp2[0])/255;
	cp[1]= (mfac*cp1[1]+fac*cp2[1])/255;
	cp[2]= (mfac*cp1[2]+fac*cp2[2])/255;
	cp[3]= (mfac*cp1[3]+fac*cp2[3])/255;
}

static void blend_color_mix_float(float *cp, const float *cp1, const float *cp2, const float fac)
{
	const float mfac= 1.0f-fac;
	cp[0]= mfac*cp1[0] + fac*cp2[0];
	cp[1]= mfac*cp1[1] + fac*cp2[1];
	cp[2]= mfac*cp1[2] + fac*cp2[2];
	cp[3]= mfac*cp1[3] + fac*cp2[3];
}

static void blend_color_mix_accum(unsigned char *cp, const unsigned char *cp1, const unsigned char *cp2, const int fac)
{
	/* this and other blending modes previously used >>8 instead of /255. both
	   are not equivalent (>>8 is /256), and the former results in rounding
	   errors that can turn colors black fast after repeated blending */
	const int mfac= 255-fac;
	const int alpha= cp1[3] + ((fac * cp2[3]) / 255);

	cp[0]= (mfac*cp1[0]+fac*cp2[0])/255;
	cp[1]= (mfac*cp1[1]+fac*cp2[1])/255;
	cp[2]= (mfac*cp1[2]+fac*cp2[2])/255;
	cp[3]= alpha > 255 ? 255 : alpha;
}

static void do_projectpaint_clone(ProjPaintState *ps, ProjPixel *projPixel, float alpha, float mask)
{
	if (ps->is_airbrush==0 && mask < 1.0f) {
		projPixel->newColor.uint = IMB_blend_color(projPixel->newColor.uint, ((ProjPixelClone*)projPixel)->clonepx.uint, (int)(alpha*255), ps->blend);
		blend_color_mix(projPixel->pixel.ch_pt,  projPixel->origColor.ch, projPixel->newColor.ch, (int)(mask*255));
	}
	else {
		*projPixel->pixel.uint_pt = IMB_blend_color(*projPixel->pixel.uint_pt, ((ProjPixelClone*)projPixel)->clonepx.uint, (int)(alpha*mask*255), ps->blend);
	}
}

static void do_projectpaint_clone_f(ProjPaintState *ps, ProjPixel *projPixel, float alpha, float mask)
{
	if (ps->is_airbrush==0 && mask < 1.0f) {
		IMB_blend_color_float(projPixel->newColor.f, projPixel->newColor.f, ((ProjPixelClone *)projPixel)->clonepx.f, alpha, ps->blend);
		blend_color_mix_float(projPixel->pixel.f_pt,  projPixel->origColor.f, projPixel->newColor.f, mask);
	}
	else {
		IMB_blend_color_float(projPixel->pixel.f_pt, projPixel->pixel.f_pt, ((ProjPixelClone *)projPixel)->clonepx.f, alpha*mask, ps->blend);
	}
}

/* do_projectpaint_smear*
 * 
 * note, mask is used to modify the alpha here, this is not correct since it allows
 * accumulation of color greater then 'projPixel->mask' however in the case of smear its not 
 * really that important to be correct as it is with clone and painting 
 */
static void do_projectpaint_smear(ProjPaintState *ps, ProjPixel *projPixel, float alpha, float mask, MemArena *smearArena, LinkNode **smearPixels, float co[2])
{
	unsigned char rgba_ub[4];
	
	if (project_paint_PickColor(ps, co, NULL, rgba_ub, 1)==0)
		return; 
	/* ((ProjPixelClone *)projPixel)->clonepx.uint = IMB_blend_color(*projPixel->pixel.uint_pt, *((unsigned int *)rgba_ub), (int)(alpha*mask*255), ps->blend); */
	blend_color_mix(((ProjPixelClone *)projPixel)->clonepx.ch, projPixel->pixel.ch_pt, rgba_ub, (int)(alpha*mask*255));
	BLI_linklist_prepend_arena(smearPixels, (void *)projPixel, smearArena);
} 

static void do_projectpaint_smear_f(ProjPaintState *ps, ProjPixel *projPixel, float alpha, float mask, MemArena *smearArena, LinkNode **smearPixels_f, float co[2])
{
	float rgba[4];
	
	if (project_paint_PickColor(ps, co, rgba, NULL, 1)==0)
		return;
	
	/* (ProjPixelClone *)projPixel)->clonepx.uint = IMB_blend_color(*((unsigned int *)rgba_smear), *((unsigned int *)rgba_ub), (int)(alpha*mask*255), ps->blend); */
	blend_color_mix_float(((ProjPixelClone *)projPixel)->clonepx.f, projPixel->pixel.f_pt, rgba, alpha*mask); 
	BLI_linklist_prepend_arena(smearPixels_f, (void *)projPixel, smearArena);
}

static void do_projectpaint_draw(ProjPaintState *ps, ProjPixel *projPixel, float *rgba, float alpha, float mask)
{
	unsigned char rgba_ub[4];
	
	if (ps->is_texbrush) {
		rgba_ub[0] = FTOCHAR(rgba[0] * ps->brush->rgb[0]);
		rgba_ub[1] = FTOCHAR(rgba[1] * ps->brush->rgb[1]);
		rgba_ub[2] = FTOCHAR(rgba[2] * ps->brush->rgb[2]);
		rgba_ub[3] = FTOCHAR(rgba[3]);
	}
	else {
		IMAPAINT_FLOAT_RGB_TO_CHAR(rgba_ub, ps->brush->rgb);
		rgba_ub[3] = 255;
	}
	
	if (ps->is_airbrush==0 && mask < 1.0f) {
		projPixel->newColor.uint = IMB_blend_color(projPixel->newColor.uint, *((unsigned int *)rgba_ub), (int)(alpha*255), ps->blend);
		blend_color_mix(projPixel->pixel.ch_pt,  projPixel->origColor.ch, projPixel->newColor.ch, (int)(mask*255));
	}
	else {
		*projPixel->pixel.uint_pt = IMB_blend_color(*projPixel->pixel.uint_pt, *((unsigned int *)rgba_ub), (int)(alpha*mask*255), ps->blend);
	}
}

static void do_projectpaint_draw_f(ProjPaintState *ps, ProjPixel *projPixel, float *rgba, float alpha, float mask, int use_color_correction)
{
	if (ps->is_texbrush) {
		/* rgba already holds a texture result here from higher level function */
		if(use_color_correction){
			float rgba_br[3];
			srgb_to_linearrgb_v3_v3(rgba_br, ps->brush->rgb);
			mul_v3_v3(rgba, rgba_br);
		}
		else{
			mul_v3_v3(rgba, ps->brush->rgb);
		}
	}
	else {
		if(use_color_correction){
			srgb_to_linearrgb_v3_v3(rgba, ps->brush->rgb);
		}
		else {
			copy_v3_v3(rgba, ps->brush->rgb);
		}
		rgba[3] = 1.0;
	}
	
	if (ps->is_airbrush==0 && mask < 1.0f) {
		IMB_blend_color_float(projPixel->newColor.f, projPixel->newColor.f, rgba, alpha, ps->blend);
		blend_color_mix_float(projPixel->pixel.f_pt,  projPixel->origColor.f, projPixel->newColor.f, mask);
	}
	else {
		IMB_blend_color_float(projPixel->pixel.f_pt, projPixel->pixel.f_pt, rgba, alpha*mask, ps->blend);
	}
}



/* run this for single and multithreaded painting */
static void *do_projectpaint_thread(void *ph_v)
{
	/* First unpack args from the struct */
	ProjPaintState *ps =			((ProjectHandle *)ph_v)->ps;
	ProjPaintImage *projImages =	((ProjectHandle *)ph_v)->projImages;
	const float *lastpos =			((ProjectHandle *)ph_v)->prevmval;
	const float *pos =				((ProjectHandle *)ph_v)->mval;
	const int thread_index =		((ProjectHandle *)ph_v)->thread_index;
	/* Done with args from ProjectHandle */

	LinkNode *node;
	ProjPixel *projPixel;
	
	int last_index = -1;
	ProjPaintImage *last_projIma= NULL;
	ImagePaintPartialRedraw *last_partial_redraw_cell;
	
	float rgba[4], alpha, dist_nosqrt, dist;
	
	float falloff;
	int bucket_index;
	int is_floatbuf = 0;
	int use_color_correction = 0;
	const short tool =  ps->tool;
	rctf bucket_bounds;
	
	/* for smear only */
	float pos_ofs[2] = {0};
	float co[2];
	float mask = 1.0f; /* airbrush wont use mask */
	unsigned short mask_short;
	const float radius= (float)brush_size(ps->scene, ps->brush);
	const float radius_squared= radius*radius; /* avoid a square root with every dist comparison */
	
	short lock_alpha= ELEM(ps->brush->blend, IMB_BLEND_ERASE_ALPHA, IMB_BLEND_ADD_ALPHA) ? 0 : ps->brush->flag & BRUSH_LOCK_ALPHA;
	
	LinkNode *smearPixels = NULL;
	LinkNode *smearPixels_f = NULL;
	MemArena *smearArena = NULL; /* mem arena for this brush projection only */
	
	if (tool==PAINT_TOOL_SMEAR) {
		pos_ofs[0] = pos[0] - lastpos[0];
		pos_ofs[1] = pos[1] - lastpos[1];
		
		smearArena = BLI_memarena_new(1<<16, "paint smear arena");
	}
	
	/* printf("brush bounds %d %d %d %d\n", bucketMin[0], bucketMin[1], bucketMax[0], bucketMax[1]); */
	
	while (project_bucket_iter_next(ps, &bucket_index, &bucket_bounds, pos)) {				
		
		/* Check this bucket and its faces are initialized */
		if (ps->bucketFlags[bucket_index] == PROJ_BUCKET_NULL) {
			/* No pixels initialized */
			project_bucket_init(ps, thread_index, bucket_index, &bucket_bounds);
		}

		if(ps->source != PROJ_SRC_VIEW) {

			/* Re-Projection, simple, no brushes! */
			
			for (node = ps->bucketRect[bucket_index]; node; node = node->next) {
				projPixel = (ProjPixel *)node->link;

				bicubic_interpolation_color(ps->reproject_ibuf, projPixel->newColor.ch, NULL, projPixel->projCoSS[0], projPixel->projCoSS[1]);
				if(projPixel->newColor.ch[3]) {
					mask = ((float)projPixel->mask)/65535.0f;
					blend_color_mix_accum(projPixel->pixel.ch_pt,  projPixel->origColor.ch, projPixel->newColor.ch, (int)(mask*projPixel->newColor.ch[3]));

				}
			}
		}
		else {
			/* Normal brush painting */
			
			for (node = ps->bucketRect[bucket_index]; node; node = node->next) {

				projPixel = (ProjPixel *)node->link;

				dist_nosqrt = len_squared_v2v2(projPixel->projCoSS, pos);

				/*if (dist < radius) {*/ /* correct but uses a sqrtf */
				if (dist_nosqrt <= radius_squared) {
					dist=sqrtf(dist_nosqrt);

					falloff = brush_curve_strength_clamp(ps->brush, dist, radius);

					if (falloff > 0.0f) {
						if (ps->is_texbrush) {
							/* note, for clone and smear, we only use the alpha, could be a special function */
							brush_sample_tex(ps->scene, ps->brush, projPixel->projCoSS, rgba, thread_index);
							alpha = rgba[3];
						} else {
							alpha = 1.0f;
						}
						
						if (ps->is_airbrush) {
							/* for an aurbrush there is no real mask, so just multiply the alpha by it */
							alpha *= falloff * brush_alpha(ps->scene, ps->brush);
							mask = ((float)projPixel->mask)/65535.0f;
						}
						else {
							/* This brush dosnt accumulate so add some curve to the brushes falloff */
							falloff = 1.0f - falloff;
							falloff = 1.0f - (falloff * falloff);
							
							mask_short = (unsigned short)(projPixel->mask * (brush_alpha(ps->scene, ps->brush) * falloff));
							if (mask_short > projPixel->mask_max) {
								mask = ((float)mask_short)/65535.0f;
								projPixel->mask_max = mask_short;
							}
							else {
								/*mask = ((float)projPixel->mask_max)/65535.0f;*/

								/* Go onto the next pixel */
								continue;
							}
						}
						
						if (alpha > 0.0f) {

							if (last_index != projPixel->image_index) {
								last_index = projPixel->image_index;
								last_projIma = projImages + last_index;

								last_projIma->touch = 1;
								is_floatbuf = last_projIma->ibuf->rect_float ? 1 : 0;
								use_color_correction = (last_projIma->ibuf->profile == IB_PROFILE_LINEAR_RGB) ? 1 : 0;
							}

							last_partial_redraw_cell = last_projIma->partRedrawRect + projPixel->bb_cell_index;
							last_partial_redraw_cell->x1 = MIN2(last_partial_redraw_cell->x1, projPixel->x_px);
							last_partial_redraw_cell->y1 = MIN2(last_partial_redraw_cell->y1, projPixel->y_px);

							last_partial_redraw_cell->x2 = MAX2(last_partial_redraw_cell->x2, projPixel->x_px+1);
							last_partial_redraw_cell->y2 = MAX2(last_partial_redraw_cell->y2, projPixel->y_px+1);

							
							switch(tool) {
							case PAINT_TOOL_CLONE:
								if (is_floatbuf) {
									if (((ProjPixelClone *)projPixel)->clonepx.f[3]) {
										do_projectpaint_clone_f(ps, projPixel, alpha, mask); /* rgba isnt used for cloning, only alpha */
									}
								}
								else {
									if (((ProjPixelClone*)projPixel)->clonepx.ch[3]) {
										do_projectpaint_clone(ps, projPixel, alpha, mask); /* rgba isnt used for cloning, only alpha */
									}
								}
								break;
							case PAINT_TOOL_SMEAR:
								sub_v2_v2v2(co, projPixel->projCoSS, pos_ofs);

								if (is_floatbuf)	do_projectpaint_smear_f(ps, projPixel, alpha, mask, smearArena, &smearPixels_f, co);
								else				do_projectpaint_smear(ps, projPixel, alpha, mask, smearArena, &smearPixels, co);
								break;
							default:
								if (is_floatbuf)	do_projectpaint_draw_f(ps, projPixel, rgba, alpha, mask, use_color_correction);
								else				do_projectpaint_draw(ps, projPixel, rgba, alpha, mask);
								break;
							}
						}

						if(lock_alpha) {
							if (is_floatbuf)	projPixel->pixel.f_pt[3]= projPixel->origColor.f[3];
							else				projPixel->pixel.ch_pt[3]= projPixel->origColor.ch[3];
						}

						/* done painting */
					}
				}
			}
		}
	}

	
	if (tool==PAINT_TOOL_SMEAR) {
		
		for (node= smearPixels; node; node= node->next) { /* this wont run for a float image */
			projPixel = node->link;
			*projPixel->pixel.uint_pt = ((ProjPixelClone *)projPixel)->clonepx.uint;
		}
		
		for (node= smearPixels_f; node; node= node->next) {
			projPixel = node->link;
			copy_v4_v4(projPixel->pixel.f_pt, ((ProjPixelClone *)projPixel)->clonepx.f);
		}
		
		BLI_memarena_free(smearArena);
	}
	
	return NULL;
}

static int project_paint_op(void *state, ImBuf *UNUSED(ibufb), float *lastpos, float *pos)
{
	/* First unpack args from the struct */
	ProjPaintState *ps = (ProjPaintState *)state;
	int touch_any = 0;	
	
	ProjectHandle handles[BLENDER_MAX_THREADS];
	ListBase threads;
	int a,i;
	
	if (!project_bucket_iter_init(ps, pos)) {
		return 0;
	}
	
	if (ps->thread_tot > 1)
		BLI_init_threads(&threads, do_projectpaint_thread, ps->thread_tot);
	
	/* get the threads running */
	for(a=0; a < ps->thread_tot; a++) {
		
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
		for (i=0; i< ps->image_tot; i++) {
			handles[a].projImages[i].partRedrawRect = (ImagePaintPartialRedraw *)BLI_memarena_alloc(ps->arena_mt[a], sizeof(ImagePaintPartialRedraw) * PROJ_BOUNDBOX_SQUARED);
			memcpy(handles[a].projImages[i].partRedrawRect, ps->projImages[i].partRedrawRect, sizeof(ImagePaintPartialRedraw) * PROJ_BOUNDBOX_SQUARED);			
		}

		if (ps->thread_tot > 1)
			BLI_insert_thread(&threads, &handles[a]);
	}
	
	if (ps->thread_tot > 1) /* wait for everything to be done */
		BLI_end_threads(&threads);
	else
		do_projectpaint_thread(&handles[0]);
		
	
	/* move threaded bounds back into ps->projectPartialRedraws */
	for(i=0; i < ps->image_tot; i++) {
		int touch = 0;
		for(a=0; a < ps->thread_tot; a++) {
			touch |= partial_redraw_array_merge(ps->projImages[i].partRedrawRect, handles[a].projImages[i].partRedrawRect, PROJ_BOUNDBOX_SQUARED);
		}
		
		if (touch) {
			ps->projImages[i].touch = 1;
			touch_any = 1;
		}
	}
	
	return touch_any;
}


static int project_paint_sub_stroke(ProjPaintState *ps, BrushPainter *painter, const int UNUSED(prevmval_i[2]), const int mval_i[2], double time, float pressure)
{
	
	/* Use mouse coords as floats for projection painting */
	float pos[2];
	
	pos[0] = (float)(mval_i[0]);
	pos[1] = (float)(mval_i[1]);
	
	// we may want to use this later 
	// brush_painter_require_imbuf(painter, ((ibuf->rect_float)? 1: 0), 0, 0);
	
	if (brush_painter_paint(painter, project_paint_op, pos, time, pressure, ps, 0)) {
		return 1;
	}
	else return 0;
}


static int project_paint_stroke(ProjPaintState *ps, BrushPainter *painter, const int prevmval_i[2], const int mval_i[2], double time, float pressure)
{
	int a, redraw;
	
	for (a=0; a < ps->image_tot; a++)
		partial_redraw_array_init(ps->projImages[a].partRedrawRect);
	
	redraw= project_paint_sub_stroke(ps, painter, prevmval_i, mval_i, time, pressure);
	
	if(project_image_refresh_tagged(ps))
		return redraw;
	
	return 0;
}

/* Imagepaint Partial Redraw & Dirty Region */

static void imapaint_clear_partial_redraw(void)
{
	memset(&imapaintpartial, 0, sizeof(imapaintpartial));
}

static void imapaint_dirty_region(Image *ima, ImBuf *ibuf, int x, int y, int w, int h)
{
	ImBuf *tmpibuf = NULL;
	int srcx= 0, srcy= 0, origx;

	IMB_rectclip(ibuf, NULL, &x, &y, &srcx, &srcy, &w, &h);

	if (w == 0 || h == 0)
		return;
	
	if (!imapaintpartial.enabled) {
		imapaintpartial.x1 = x;
		imapaintpartial.y1 = y;
		imapaintpartial.x2 = x+w;
		imapaintpartial.y2 = y+h;
		imapaintpartial.enabled = 1;
	}
	else {
		imapaintpartial.x1 = MIN2(imapaintpartial.x1, x);
		imapaintpartial.y1 = MIN2(imapaintpartial.y1, y);
		imapaintpartial.x2 = MAX2(imapaintpartial.x2, x+w);
		imapaintpartial.y2 = MAX2(imapaintpartial.y2, y+h);
	}

	w = ((x + w - 1) >> IMAPAINT_TILE_BITS);
	h = ((y + h - 1) >> IMAPAINT_TILE_BITS);
	origx = (x >> IMAPAINT_TILE_BITS);
	y = (y >> IMAPAINT_TILE_BITS);
	
	for (; y <= h; y++)
		for (x=origx; x <= w; x++)
			image_undo_push_tile(ima, ibuf, &tmpibuf, x, y);

	ibuf->userflags |= IB_BITMAPDIRTY;
	
	if (tmpibuf)
		IMB_freeImBuf(tmpibuf);
}

static void imapaint_image_update(SpaceImage *sima, Image *image, ImBuf *ibuf, short texpaint)
{
	if(ibuf->rect_float)
		ibuf->userflags |= IB_RECT_INVALID; /* force recreate of char rect */
	
	if(ibuf->mipmap[0])
		ibuf->userflags |= IB_MIPMAP_INVALID;

	/* todo: should set_tpage create ->rect? */
	if(texpaint || (sima && sima->lock)) {
		int w = imapaintpartial.x2 - imapaintpartial.x1;
		int h = imapaintpartial.y2 - imapaintpartial.y1;
		/* Testing with partial update in uv editor too */
		GPU_paint_update_image(image, imapaintpartial.x1, imapaintpartial.y1, w, h, 0);//!texpaint);
	}
}

/* Image Paint Operations */

static void imapaint_ibuf_get_set_rgb(ImBuf *ibuf, int x, int y, short torus, short set, float *rgb)
{
	if (torus) {
		x %= ibuf->x;
		if (x < 0) x += ibuf->x;
		y %= ibuf->y;
		if (y < 0) y += ibuf->y;
	}

	if (ibuf->rect_float) {
		float *rrgbf = ibuf->rect_float + (ibuf->x*y + x)*4;

		if (set) {
			IMAPAINT_FLOAT_RGB_COPY(rrgbf, rgb);
		} else {
			IMAPAINT_FLOAT_RGB_COPY(rgb, rrgbf);
		}
	}
	else {
		char *rrgb = (char*)ibuf->rect + (ibuf->x*y + x)*4;

		if (set) {
			IMAPAINT_FLOAT_RGB_TO_CHAR(rrgb, rgb)
		} else {
			IMAPAINT_CHAR_RGB_TO_FLOAT(rgb, rrgb)
		}
	}
}

static int imapaint_ibuf_add_if(ImBuf *ibuf, unsigned int x, unsigned int y, float *outrgb, short torus)
{
	float inrgb[3];

	// XXX: signed unsigned mismatch
	if ((x >= (unsigned int)(ibuf->x)) || (y >= (unsigned int)(ibuf->y))) {
		if (torus) imapaint_ibuf_get_set_rgb(ibuf, x, y, 1, 0, inrgb);
		else return 0;
	}
	else imapaint_ibuf_get_set_rgb(ibuf, x, y, 0, 0, inrgb);

	outrgb[0] += inrgb[0];
	outrgb[1] += inrgb[1];
	outrgb[2] += inrgb[2];

	return 1;
}

static void imapaint_lift_soften(ImBuf *ibuf, ImBuf *ibufb, int *pos, short torus)
{
	int x, y, count, xi, yi, xo, yo;
	int out_off[2], in_off[2], dim[2];
	float outrgb[3];

	dim[0] = ibufb->x;
	dim[1] = ibufb->y;
	in_off[0] = pos[0];
	in_off[1] = pos[1];
	out_off[0] = out_off[1] = 0;

	if (!torus) {
		IMB_rectclip(ibuf, ibufb, &in_off[0], &in_off[1], &out_off[0],
			&out_off[1], &dim[0], &dim[1]);

		if ((dim[0] == 0) || (dim[1] == 0))
			return;
	}

	for (y=0; y < dim[1]; y++) {
		for (x=0; x < dim[0]; x++) {
			/* get input pixel */
			xi = in_off[0] + x;
			yi = in_off[1] + y;

			count = 1;
			imapaint_ibuf_get_set_rgb(ibuf, xi, yi, torus, 0, outrgb);

			count += imapaint_ibuf_add_if(ibuf, xi-1, yi-1, outrgb, torus);
			count += imapaint_ibuf_add_if(ibuf, xi-1, yi  , outrgb, torus);
			count += imapaint_ibuf_add_if(ibuf, xi-1, yi+1, outrgb, torus);

			count += imapaint_ibuf_add_if(ibuf, xi  , yi-1, outrgb, torus);
			count += imapaint_ibuf_add_if(ibuf, xi  , yi+1, outrgb, torus);

			count += imapaint_ibuf_add_if(ibuf, xi+1, yi-1, outrgb, torus);
			count += imapaint_ibuf_add_if(ibuf, xi+1, yi  , outrgb, torus);
			count += imapaint_ibuf_add_if(ibuf, xi+1, yi+1, outrgb, torus);

			outrgb[0] /= count;
			outrgb[1] /= count;
			outrgb[2] /= count;

			/* write into brush buffer */
			xo = out_off[0] + x;
			yo = out_off[1] + y;
			imapaint_ibuf_get_set_rgb(ibufb, xo, yo, 0, 1, outrgb);
		}
	}
}

static void imapaint_set_region(ImagePaintRegion *region, int destx, int desty, int srcx, int srcy, int width, int height)
{
	region->destx= destx;
	region->desty= desty;
	region->srcx= srcx;
	region->srcy= srcy;
	region->width= width;
	region->height= height;
}

static int imapaint_torus_split_region(ImagePaintRegion region[4], ImBuf *dbuf, ImBuf *sbuf)
{
	int destx= region->destx;
	int desty= region->desty;
	int srcx= region->srcx;
	int srcy= region->srcy;
	int width= region->width;
	int height= region->height;
	int origw, origh, w, h, tot= 0;

	/* convert destination and source coordinates to be within image */
	destx = destx % dbuf->x;
	if (destx < 0) destx += dbuf->x;
	desty = desty % dbuf->y;
	if (desty < 0) desty += dbuf->y;
	srcx = srcx % sbuf->x;
	if (srcx < 0) srcx += sbuf->x;
	srcy = srcy % sbuf->y;
	if (srcy < 0) srcy += sbuf->y;

	/* clip width of blending area to destination imbuf, to avoid writing the
	   same pixel twice */
	origw = w = (width > dbuf->x)? dbuf->x: width;
	origh = h = (height > dbuf->y)? dbuf->y: height;

	/* clip within image */
	IMB_rectclip(dbuf, sbuf, &destx, &desty, &srcx, &srcy, &w, &h);
	imapaint_set_region(&region[tot++], destx, desty, srcx, srcy, w, h);

	/* do 3 other rects if needed */
	if (w < origw)
		imapaint_set_region(&region[tot++], (destx+w)%dbuf->x, desty, (srcx+w)%sbuf->x, srcy, origw-w, h);
	if (h < origh)
		imapaint_set_region(&region[tot++], destx, (desty+h)%dbuf->y, srcx, (srcy+h)%sbuf->y, w, origh-h);
	if ((w < origw) && (h < origh))
		imapaint_set_region(&region[tot++], (destx+w)%dbuf->x, (desty+h)%dbuf->y, (srcx+w)%sbuf->x, (srcy+h)%sbuf->y, origw-w, origh-h);
	
	return tot;
}

static void imapaint_lift_smear(ImBuf *ibuf, ImBuf *ibufb, int *pos)
{
	ImagePaintRegion region[4];
	int a, tot;

	imapaint_set_region(region, 0, 0, pos[0], pos[1], ibufb->x, ibufb->y);
	tot= imapaint_torus_split_region(region, ibufb, ibuf);

	for(a=0; a<tot; a++)
		IMB_rectblend(ibufb, ibuf, region[a].destx, region[a].desty,
			region[a].srcx, region[a].srcy,
			region[a].width, region[a].height, IMB_BLEND_COPY_RGB);
}

static ImBuf *imapaint_lift_clone(ImBuf *ibuf, ImBuf *ibufb, int *pos)
{
	/* note: allocImbuf returns zero'd memory, so regions outside image will
	   have zero alpha, and hence not be blended onto the image */
	int w=ibufb->x, h=ibufb->y, destx=0, desty=0, srcx=pos[0], srcy=pos[1];
	ImBuf *clonebuf= IMB_allocImBuf(w, h, ibufb->planes, ibufb->flags);

	IMB_rectclip(clonebuf, ibuf, &destx, &desty, &srcx, &srcy, &w, &h);
	IMB_rectblend(clonebuf, ibuf, destx, desty, srcx, srcy, w, h,
		IMB_BLEND_COPY_RGB);
	IMB_rectblend(clonebuf, ibufb, destx, desty, destx, desty, w, h,
		IMB_BLEND_COPY_ALPHA);

	return clonebuf;
}

static void imapaint_convert_brushco(ImBuf *ibufb, float *pos, int *ipos)
{
	ipos[0]= (int)floorf((pos[0] - ibufb->x/2) + 1.0f);
	ipos[1]= (int)floorf((pos[1] - ibufb->y/2) + 1.0f);
}

/* dosnt run for projection painting
 * only the old style painting in the 3d view */
static int imapaint_paint_op(void *state, ImBuf *ibufb, float *lastpos, float *pos)
{
	ImagePaintState *s= ((ImagePaintState*)state);
	ImBuf *clonebuf= NULL, *frombuf;
	ImagePaintRegion region[4];
	short torus= s->brush->flag & BRUSH_TORUS;
	short blend= s->blend;
	float *offset= s->brush->clone.offset;
	float liftpos[2];
	int bpos[2], blastpos[2], bliftpos[2];
	int a, tot;

	imapaint_convert_brushco(ibufb, pos, bpos);

	/* lift from canvas */
	if(s->tool == PAINT_TOOL_SOFTEN) {
		imapaint_lift_soften(s->canvas, ibufb, bpos, torus);
	}
	else if(s->tool == PAINT_TOOL_SMEAR) {
		if (lastpos[0]==pos[0] && lastpos[1]==pos[1])
			return 0;

		imapaint_convert_brushco(ibufb, lastpos, blastpos);
		imapaint_lift_smear(s->canvas, ibufb, blastpos);
	}
	else if(s->tool == PAINT_TOOL_CLONE && s->clonecanvas) {
		liftpos[0]= pos[0] - offset[0]*s->canvas->x;
		liftpos[1]= pos[1] - offset[1]*s->canvas->y;

		imapaint_convert_brushco(ibufb, liftpos, bliftpos);
		clonebuf= imapaint_lift_clone(s->clonecanvas, ibufb, bliftpos);
	}

	frombuf= (clonebuf)? clonebuf: ibufb;

	if(torus) {
		imapaint_set_region(region, bpos[0], bpos[1], 0, 0, frombuf->x, frombuf->y);
		tot= imapaint_torus_split_region(region, s->canvas, frombuf);
	}
	else {
		imapaint_set_region(region, bpos[0], bpos[1], 0, 0, frombuf->x, frombuf->y);
		tot= 1;
	}

	/* blend into canvas */
	for(a=0; a<tot; a++) {
		imapaint_dirty_region(s->image, s->canvas,
			region[a].destx, region[a].desty,
			region[a].width, region[a].height);
		
		IMB_rectblend(s->canvas, frombuf,
			region[a].destx, region[a].desty,
			region[a].srcx, region[a].srcy,
			region[a].width, region[a].height, blend);
	}

	if(clonebuf) IMB_freeImBuf(clonebuf);

	return 1;
}

/* 3D TexturePaint */

static int texpaint_break_stroke(float *prevuv, float *fwuv, float *bkuv, float *uv)
{
	float d1[2], d2[2];
	float mismatch = len_v2v2(fwuv, uv);
	float len1 = len_v2v2(prevuv, fwuv);
	float len2 = len_v2v2(bkuv, uv);

	sub_v2_v2v2(d1, fwuv, prevuv);
	sub_v2_v2v2(d2, uv, bkuv);

	return ((dot_v2v2(d1, d2) < 0.0f) || (mismatch > MAX2(len1, len2)*2));
}

/* ImagePaint Common */

static int imapaint_canvas_set(ImagePaintState *s, Image *ima)
{
	ImBuf *ibuf= BKE_image_get_ibuf(ima, s->sima? &s->sima->iuser: NULL);
	
	/* verify that we can paint and set canvas */
	if(ima==NULL) {
		return 0;
	}
	else if(ima->packedfile && ima->rr) {
		s->warnpackedfile = ima->id.name + 2;
		return 0;
	}	
	else if(ibuf && ibuf->channels!=4) {
		s->warnmultifile = ima->id.name + 2;
		return 0;
	}
	else if(!ibuf || !(ibuf->rect || ibuf->rect_float))
		return 0;

	s->image= ima;
	s->canvas= ibuf;

	/* set clone canvas */
	if(s->tool == PAINT_TOOL_CLONE) {
		ima= s->brush->clone.image;
		ibuf= BKE_image_get_ibuf(ima, s->sima? &s->sima->iuser: NULL);
		
		if(!ima || !ibuf || !(ibuf->rect || ibuf->rect_float))
			return 0;

		s->clonecanvas= ibuf;

		/* temporarily add float rect for cloning */
		if(s->canvas->rect_float && !s->clonecanvas->rect_float) {
			int profile = IB_PROFILE_NONE;
			
			/* Don't want to color manage, but don't disturb existing profiles */
			SWAP(int, s->clonecanvas->profile, profile);

			IMB_float_from_rect(s->clonecanvas);
			s->clonefreefloat= 1;
			
			SWAP(int, s->clonecanvas->profile, profile);
		}
		else if(!s->canvas->rect_float && !s->clonecanvas->rect)
			IMB_rect_from_float(s->clonecanvas);
	}

	return 1;
}

static void imapaint_canvas_free(ImagePaintState *s)
{
	if (s->clonefreefloat)
		imb_freerectfloatImBuf(s->clonecanvas);
}

static int imapaint_paint_sub_stroke(ImagePaintState *s, BrushPainter *painter, Image *image, short texpaint, float *uv, double time, int update, float pressure)
{
	ImBuf *ibuf= BKE_image_get_ibuf(image, s->sima? &s->sima->iuser: NULL);
	float pos[2];

	if(!ibuf)
		return 0;

	pos[0] = uv[0]*ibuf->x;
	pos[1] = uv[1]*ibuf->y;

	brush_painter_require_imbuf(painter, ((ibuf->rect_float)? 1: 0), 0, 0);

	if (brush_painter_paint(painter, imapaint_paint_op, pos, time, pressure, s, ibuf->profile == IB_PROFILE_LINEAR_RGB)) {
		if (update)
			imapaint_image_update(s->sima, image, ibuf, texpaint);
		return 1;
	}
	else return 0;
}

static int imapaint_paint_stroke(ViewContext *vc, ImagePaintState *s, BrushPainter *painter, short texpaint, const int prevmval[2], const int mval[2], double time, float pressure)
{
	Image *newimage = NULL;
	float fwuv[2], bkuv[2], newuv[2];
	unsigned int newfaceindex;
	int breakstroke = 0, redraw = 0;

	if (texpaint) {
		/* pick new face and image */
		if (	imapaint_pick_face(vc, s->me, mval, &newfaceindex) &&
				((s->me->editflag & ME_EDIT_PAINT_MASK)==0 || (s->me->mface+newfaceindex)->flag & ME_FACE_SEL)
		) {
			ImBuf *ibuf;
			
			newimage = imapaint_face_image(s, newfaceindex);
			ibuf= BKE_image_get_ibuf(newimage, s->sima? &s->sima->iuser: NULL);

			if(ibuf && ibuf->rect)
				imapaint_pick_uv(s->scene, s->ob, newfaceindex, mval, newuv);
			else {
				newimage = NULL;
				newuv[0] = newuv[1] = 0.0f;
			}
		}
		else
			newuv[0] = newuv[1] = 0.0f;

		/* see if stroke is broken, and if so finish painting in old position */
		if (s->image) {
			imapaint_pick_uv(s->scene, s->ob, s->faceindex, mval, fwuv);
			imapaint_pick_uv(s->scene, s->ob, newfaceindex, prevmval, bkuv);

			if (newimage == s->image)
				breakstroke= texpaint_break_stroke(s->uv, fwuv, bkuv, newuv);
			else
				breakstroke= 1;
		}
		else
			fwuv[0]= fwuv[1]= 0.0f;

		if (breakstroke) {
			imapaint_pick_uv(s->scene, s->ob, s->faceindex, mval, fwuv);
			redraw |= imapaint_paint_sub_stroke(s, painter, s->image, texpaint,
				fwuv, time, 1, pressure);
			imapaint_clear_partial_redraw();
			brush_painter_break_stroke(painter);
		}

		/* set new canvas */
		if (newimage && (newimage != s->image))
			if (!imapaint_canvas_set(s, newimage))
				newimage = NULL;

		/* paint in new image */
		if (newimage) {
			if (breakstroke)
				redraw|= imapaint_paint_sub_stroke(s, painter, newimage,
					texpaint, bkuv, time, 0, pressure);
			redraw|= imapaint_paint_sub_stroke(s, painter, newimage, texpaint,
				newuv, time, 1, pressure);
		}

		/* update state */
		s->image = newimage;
		s->faceindex = newfaceindex;
		s->uv[0] = newuv[0];
		s->uv[1] = newuv[1];
	}
	else {
		UI_view2d_region_to_view(s->v2d, mval[0], mval[1], &newuv[0], &newuv[1]);
		redraw |= imapaint_paint_sub_stroke(s, painter, s->image, texpaint, newuv,
			time, 1, pressure);
	}

	if (redraw)
		imapaint_clear_partial_redraw();

	return redraw;
}

/************************ image paint poll ************************/

static Brush *image_paint_brush(bContext *C)
{
	Scene *scene= CTX_data_scene(C);
	ToolSettings *settings= scene->toolsettings;

	return paint_brush(&settings->imapaint.paint);
}

static Brush *uv_sculpt_brush(bContext *C)
{
	Scene *scene= CTX_data_scene(C);
	ToolSettings *settings= scene->toolsettings;

	if(!settings->uvsculpt)
		return NULL;
	return paint_brush(&settings->uvsculpt->paint);
}

static int image_paint_poll(bContext *C)
{
	Object *obact = CTX_data_active_object(C);

	if(!image_paint_brush(C))
		return 0;

	if((obact && obact->mode & OB_MODE_TEXTURE_PAINT) && CTX_wm_region_view3d(C)) {
		return 1;
	}
	else {
		SpaceImage *sima= CTX_wm_space_image(C);

		if(sima) {
			ARegion *ar= CTX_wm_region(C);

			if((sima->flag & SI_DRAWTOOL) && ar->regiontype==RGN_TYPE_WINDOW)
				return 1;
		}
	}

	return 0;
}

static int uv_sculpt_brush_poll(bContext *C)
{
	BMEditMesh *em;
	int ret;
	Object *obedit = CTX_data_edit_object(C);
	SpaceImage *sima= CTX_wm_space_image(C);
	Scene *scene = CTX_data_scene(C);
	ToolSettings *toolsettings = scene->toolsettings;

	if(!uv_sculpt_brush(C) || !obedit || obedit->type != OB_MESH)
		return 0;

	em = ((Mesh *)obedit->data)->edit_btmesh;
	ret = EDBM_texFaceCheck(em);

	if(ret && sima) {
		ARegion *ar= CTX_wm_region(C);
		if((toolsettings->use_uv_sculpt) && ar->regiontype==RGN_TYPE_WINDOW)
			return 1;
	}

	return 0;
}

static int image_paint_3d_poll(bContext *C)
{
	if(CTX_wm_region_view3d(C))
		return image_paint_poll(C);
	
	return 0;
}

static int image_paint_2d_clone_poll(bContext *C)
{
	Brush *brush= image_paint_brush(C);

	if(!CTX_wm_region_view3d(C) && image_paint_poll(C))
		if(brush && (brush->imagepaint_tool == PAINT_TOOL_CLONE))
			if(brush->clone.image)
				return 1;
	
	return 0;
}

/************************ paint operator ************************/

typedef enum PaintMode {
	PAINT_MODE_2D,
	PAINT_MODE_3D,
	PAINT_MODE_3D_PROJECT
} PaintMode;

typedef struct PaintOperation {
	PaintMode mode;

	BrushPainter *painter;
	ImagePaintState s;
	ProjPaintState ps;

	int first;
	int prevmouse[2];
	float prev_pressure; /* need this since we dont get tablet events for pressure change */
	int orig_brush_size;
	double starttime;

	ViewContext vc;
	wmTimer *timer;

	short restore_projection;
} PaintOperation;

static void paint_redraw(bContext *C, ImagePaintState *s, int final)
{
	if(final) {
		if(s->image)
			GPU_free_image(s->image);

		/* compositor listener deals with updating */
		WM_event_add_notifier(C, NC_IMAGE|NA_EDITED, s->image);
	}
	else {
		if(!s->sima || !s->sima->lock)
			ED_region_tag_redraw(CTX_wm_region(C));
		else
			WM_event_add_notifier(C, NC_IMAGE|NA_EDITED, s->image);
	}
}

/* initialize project paint settings from context */
static void project_state_init(bContext *C, Object *ob, ProjPaintState *ps)
{
	Scene *scene= CTX_data_scene(C);
	ToolSettings *settings= scene->toolsettings;
	Brush *brush= paint_brush(&settings->imapaint.paint);

	/* brush */
	ps->brush = brush;
	ps->tool = brush->imagepaint_tool;
	ps->blend = brush->blend;

	ps->is_airbrush = (brush->flag & BRUSH_AIRBRUSH) ? 1 : 0;
	ps->is_texbrush = (brush->mtex.tex) ? 1 : 0;


	/* these can be NULL */
	ps->v3d= CTX_wm_view3d(C);
	ps->rv3d= CTX_wm_region_view3d(C);
	ps->ar= CTX_wm_region(C);

	ps->scene= scene;
	ps->ob= ob; /* allow override of active object */

	/* setup projection painting data */
	ps->do_backfacecull = (settings->imapaint.flag & IMAGEPAINT_PROJECT_BACKFACE) ? 0 : 1;
	ps->do_occlude = (settings->imapaint.flag & IMAGEPAINT_PROJECT_XRAY) ? 0 : 1;
	ps->do_mask_normal = (settings->imapaint.flag & IMAGEPAINT_PROJECT_FLAT) ? 0 : 1;
	ps->do_new_shading_nodes = scene_use_new_shading_nodes(scene); /* only cache the value */

	if (ps->tool == PAINT_TOOL_CLONE)
		ps->do_layer_clone = (settings->imapaint.flag & IMAGEPAINT_PROJECT_LAYER_CLONE);

	ps->do_layer_stencil = (settings->imapaint.flag & IMAGEPAINT_PROJECT_LAYER_STENCIL) ? 1 : 0;
	ps->do_layer_stencil_inv = (settings->imapaint.flag & IMAGEPAINT_PROJECT_LAYER_STENCIL_INV) ? 1 : 0;


#ifndef PROJ_DEBUG_NOSEAMBLEED
	ps->seam_bleed_px = settings->imapaint.seam_bleed; /* pixel num to bleed */
#endif

	if(ps->do_mask_normal) {
		ps->normal_angle_inner = settings->imapaint.normal_angle;
		ps->normal_angle = (ps->normal_angle_inner + 90.0f) * 0.5f;
	}
	else {
		ps->normal_angle_inner= ps->normal_angle= settings->imapaint.normal_angle;
	}

	ps->normal_angle_inner *=	(float)(M_PI_2 / 90);
	ps->normal_angle *=			(float)(M_PI_2 / 90);
	ps->normal_angle_range = ps->normal_angle - ps->normal_angle_inner;

	if(ps->normal_angle_range <= 0.0f)
		ps->do_mask_normal = 0; /* no need to do blending */
}

static void paint_brush_init_tex(Brush *brush)
{
	/* init mtex nodes */ 
	if(brush) {
		MTex *mtex= &brush->mtex;
		if(mtex->tex && mtex->tex->nodetree)
			ntreeTexBeginExecTree(mtex->tex->nodetree, 1); /* has internal flag to detect it only does it once */
	}
	
}

static int texture_paint_init(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	ToolSettings *settings= scene->toolsettings;
	Brush *brush= paint_brush(&settings->imapaint.paint);
	PaintOperation *pop= MEM_callocN(sizeof(PaintOperation), "PaintOperation"); /* caller frees */

	pop->first= 1;
	op->customdata= pop;
	
	/* XXX: Soften tool does not support projection painting atm, so just disable
	        projection for this brush */
	if(brush->imagepaint_tool == PAINT_TOOL_SOFTEN) {
		settings->imapaint.flag |= IMAGEPAINT_PROJECT_DISABLE;
		pop->restore_projection = 1;
	}

	/* initialize from context */
	if(CTX_wm_region_view3d(C)) {
		pop->mode= PAINT_MODE_3D;

		if(!(settings->imapaint.flag & IMAGEPAINT_PROJECT_DISABLE))
			pop->mode= PAINT_MODE_3D_PROJECT;
		else
			view3d_set_viewcontext(C, &pop->vc);
	}
	else {
		pop->s.sima= CTX_wm_space_image(C);
		pop->s.v2d= &CTX_wm_region(C)->v2d;
	}

	pop->s.scene= scene;
	pop->s.screen= CTX_wm_screen(C);

	pop->s.brush = brush;
	pop->s.tool = brush->imagepaint_tool;
	if(pop->mode == PAINT_MODE_3D && (pop->s.tool == PAINT_TOOL_CLONE))
		pop->s.tool = PAINT_TOOL_DRAW;
	pop->s.blend = brush->blend;
	pop->orig_brush_size= brush_size(scene, brush);

	if(pop->mode != PAINT_MODE_2D) {
		pop->s.ob = OBACT;
		pop->s.me = get_mesh(pop->s.ob);
		if (!pop->s.me) return 0;
	}
	else {
		pop->s.image = pop->s.sima->image;

		if(!imapaint_canvas_set(&pop->s, pop->s.image)) {
			if(pop->s.warnmultifile)
				BKE_report(op->reports, RPT_WARNING, "Image requires 4 color channels to paint");
			if(pop->s.warnpackedfile)
				BKE_report(op->reports, RPT_WARNING, "Packed MultiLayer files cannot be painted");

			return 0;
		}
	}
	
	paint_brush_init_tex(pop->s.brush);
	
	/* note, if we have no UVs on the derived mesh, then we must return here */
	if(pop->mode == PAINT_MODE_3D_PROJECT) {

		/* initialize all data from the context */
		project_state_init(C, OBACT, &pop->ps);
		
		paint_brush_init_tex(pop->ps.brush);

		pop->ps.source= PROJ_SRC_VIEW;

		if (pop->ps.ob==NULL || !(pop->ps.ob->lay & pop->ps.v3d->lay))
			return 0;

		/* Dont allow brush size below 2 */
		if (brush_size(scene, brush) < 2)
			brush_set_size(scene, brush, 2);

		/* allocate and initialize spacial data structures */
		project_paint_begin(&pop->ps);
		
		if(pop->ps.dm==NULL)
			return 0;
	}
	
	settings->imapaint.flag |= IMAGEPAINT_DRAWING;
	undo_paint_push_begin(UNDO_PAINT_IMAGE, op->type->name,
		image_undo_restore, image_undo_free);

	/* create painter */
	pop->painter= brush_painter_new(scene, pop->s.brush);

	return 1;
}

static void paint_apply(bContext *C, wmOperator *op, PointerRNA *itemptr)
{
	PaintOperation *pop= op->customdata;
	float time, mousef[2];
	float pressure;
	int mouse[2], redraw;

	RNA_float_get_array(itemptr, "mouse", mousef);
	mouse[0] = (int)(mousef[0]);
	mouse[1] = (int)(mousef[1]);
	time= RNA_float_get(itemptr, "time");
	pressure= RNA_float_get(itemptr, "pressure");

	if(pop->first)
		project_paint_begin_clone(&pop->ps, mouse);

	if(pop->mode == PAINT_MODE_3D)
		view3d_operator_needs_opengl(C);

	if(pop->mode == PAINT_MODE_3D_PROJECT) {
		redraw= project_paint_stroke(&pop->ps, pop->painter, pop->prevmouse, mouse, time, pressure);
		pop->prevmouse[0]= mouse[0];
		pop->prevmouse[1]= mouse[1];

	}
	else { 
		redraw= imapaint_paint_stroke(&pop->vc, &pop->s, pop->painter, pop->mode == PAINT_MODE_3D, pop->prevmouse, mouse, time, pressure);
		pop->prevmouse[0]= mouse[0];
		pop->prevmouse[1]= mouse[1];
	}

	if(redraw)
		paint_redraw(C, &pop->s, 0);

	pop->first= 0;
}

static void paint_brush_exit_tex(Brush *brush)
{
	if(brush) {
		MTex *mtex= &brush->mtex;
		if(mtex->tex && mtex->tex->nodetree)
			ntreeTexEndExecTree(mtex->tex->nodetree->execdata, 1);
	}	
}

static void paint_exit(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	ToolSettings *settings= scene->toolsettings;
	PaintOperation *pop= op->customdata;

	if(pop->timer)
		WM_event_remove_timer(CTX_wm_manager(C), CTX_wm_window(C), pop->timer);

	if(pop->restore_projection)
		settings->imapaint.flag &= ~IMAGEPAINT_PROJECT_DISABLE;

	paint_brush_exit_tex(pop->s.brush);
	
	settings->imapaint.flag &= ~IMAGEPAINT_DRAWING;
	imapaint_canvas_free(&pop->s);
	brush_painter_free(pop->painter);

	if(pop->mode == PAINT_MODE_3D_PROJECT) {
		brush_set_size(scene, pop->ps.brush, pop->orig_brush_size);
		paint_brush_exit_tex(pop->ps.brush);
		
		project_paint_end(&pop->ps);
	}
	
	paint_redraw(C, &pop->s, 1);
	undo_paint_push_end(UNDO_PAINT_IMAGE);
	
	if(pop->s.warnmultifile)
		BKE_reportf(op->reports, RPT_WARNING, "Image requires 4 color channels to paint: %s", pop->s.warnmultifile);
	if(pop->s.warnpackedfile)
		BKE_reportf(op->reports, RPT_WARNING, "Packed MultiLayer files cannot be painted: %s", pop->s.warnpackedfile);

	MEM_freeN(pop);
}

static int paint_exec(bContext *C, wmOperator *op)
{
	if(!texture_paint_init(C, op)) {
		MEM_freeN(op->customdata);
		return OPERATOR_CANCELLED;
	}

	RNA_BEGIN(op->ptr, itemptr, "stroke") {
		paint_apply(C, op, &itemptr);
	}
	RNA_END;

	paint_exit(C, op);

	return OPERATOR_FINISHED;
}

static void paint_apply_event(bContext *C, wmOperator *op, wmEvent *event)
{
	const Scene *scene = CTX_data_scene(C);
	PaintOperation *pop= op->customdata;
	wmTabletData *wmtab;
	PointerRNA itemptr;
	float pressure, mousef[2];
	double time;
	int tablet;

	time= PIL_check_seconds_timer();

	tablet= 0;
	pop->s.blend= pop->s.brush->blend;

	if(event->custom == EVT_DATA_TABLET) {
		wmtab= event->customdata;

		tablet= (wmtab->Active != EVT_TABLET_NONE);
		pressure= wmtab->Pressure;
		if(wmtab->Active == EVT_TABLET_ERASER)
			pop->s.blend= IMB_BLEND_ERASE_ALPHA;
	}
	else { /* otherwise airbrush becomes 1.0 pressure instantly */
		pressure= pop->prev_pressure ? pop->prev_pressure : 1.0f;
	}

	if(pop->first) {
		pop->prevmouse[0]= event->mval[0];
		pop->prevmouse[1]= event->mval[1];
		pop->starttime= time;

		/* special exception here for too high pressure values on first touch in
		   windows for some tablets, then we just skip first touch ..  */
		if (tablet && (pressure >= 0.99f) && ((pop->s.brush->flag & BRUSH_SPACING_PRESSURE) || brush_use_alpha_pressure(scene, pop->s.brush) || brush_use_size_pressure(scene, pop->s.brush)))
			return;

		/* This can be removed once fixed properly in
		 brush_painter_paint(BrushPainter *painter, BrushFunc func, float *pos, double time, float pressure, void *user) 
		 at zero pressure we should do nothing 1/2^12 is .0002 which is the sensitivity of the most sensitive pen tablet available*/
		if (tablet && (pressure < .0002f) && ((pop->s.brush->flag & BRUSH_SPACING_PRESSURE) || brush_use_alpha_pressure(scene, pop->s.brush) || brush_use_size_pressure(scene, pop->s.brush)))
			return;
	
	}

	/* fill in stroke */
	RNA_collection_add(op->ptr, "stroke", &itemptr);

	mousef[0] = (float)(event->mval[0]);
	mousef[1] = (float)(event->mval[1]);
	RNA_float_set_array(&itemptr, "mouse", mousef);
	RNA_float_set(&itemptr, "time", (float)(time - pop->starttime));
	RNA_float_set(&itemptr, "pressure", pressure);

	/* apply */
	paint_apply(C, op, &itemptr);

	pop->prev_pressure= pressure;
}

static int paint_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	PaintOperation *pop;

	if(!texture_paint_init(C, op)) {
		MEM_freeN(op->customdata);
		return OPERATOR_CANCELLED;
	}
	
	paint_apply_event(C, op, event);

	pop= op->customdata;
	WM_event_add_modal_handler(C, op);

	if(pop->s.brush->flag & BRUSH_AIRBRUSH)
		pop->timer= WM_event_add_timer(CTX_wm_manager(C), CTX_wm_window(C), TIMER, 0.01f);

	return OPERATOR_RUNNING_MODAL;
}

static int paint_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	PaintOperation *pop= op->customdata;

	switch(event->type) {
		case LEFTMOUSE:
		case MIDDLEMOUSE:
		case RIGHTMOUSE: // XXX hardcoded
			paint_exit(C, op);
			return OPERATOR_FINISHED;
		case MOUSEMOVE:
		case INBETWEEN_MOUSEMOVE:
			paint_apply_event(C, op, event);
			break;
		case TIMER:
			if(event->customdata == pop->timer)
				paint_apply_event(C, op, event);
			break;
	}

	return OPERATOR_RUNNING_MODAL;
}

static int paint_cancel(bContext *C, wmOperator *op)
{
	paint_exit(C, op);

	return OPERATOR_CANCELLED;
}

void PAINT_OT_image_paint(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Image Paint";
	ot->idname= "PAINT_OT_image_paint";
	
	/* api callbacks */
	ot->exec= paint_exec;
	ot->invoke= paint_invoke;
	ot->modal= paint_modal;
	ot->cancel= paint_cancel;
	ot->poll= image_paint_poll;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;

	/* properties */
	RNA_def_collection_runtime(ot->srna, "stroke", &RNA_OperatorStrokeElement, "Stroke", "");
}

int get_imapaint_zoom(bContext *C, float *zoomx, float *zoomy)
{
	RegionView3D *rv3d= CTX_wm_region_view3d(C);

	if(!rv3d) {
		SpaceImage *sima= CTX_wm_space_image(C);
		ARegion *ar= CTX_wm_region(C);
		
		ED_space_image_zoom(sima, ar, zoomx, zoomy);

		return 1;
	}

	*zoomx = *zoomy = 1;

	return 0;
}

/************************ cursor drawing *******************************/

static void brush_drawcursor(bContext *C, int x, int y, void *UNUSED(customdata))
{
#define PX_SIZE_FADE_MAX 12.0f
#define PX_SIZE_FADE_MIN 4.0f

	Scene *scene= CTX_data_scene(C);
	//Brush *brush= image_paint_brush(C);
	Paint *paint= paint_get_active(scene);
	Brush *brush= paint_brush(paint);

	if(paint && brush && paint->flags & PAINT_SHOW_BRUSH) {
		ToolSettings *ts;
		float zoomx, zoomy;
		const float size= (float)brush_size(scene, brush);
		const short use_zoom= get_imapaint_zoom(C, &zoomx, &zoomy);
		float pixel_size;
		float alpha= 0.5f;

		ts = scene->toolsettings;

		if(use_zoom && !ts->use_uv_sculpt){
			pixel_size = MAX2(size * zoomx, size * zoomy);
		}
		else {
			pixel_size = size;
		}

		/* fade out the brush (cheap trick to work around brush interfearing with sampling [#])*/
		if(pixel_size < PX_SIZE_FADE_MIN) {
			return;
		}
		else if (pixel_size < PX_SIZE_FADE_MAX) {
			alpha *= (pixel_size - PX_SIZE_FADE_MIN) / (PX_SIZE_FADE_MAX - PX_SIZE_FADE_MIN);
		}

		glPushMatrix();

		glTranslatef((float)x, (float)y, 0.0f);

		/* No need to scale for uv sculpting, on the contrary it might be useful to keep unscaled */
		if(use_zoom && !ts->use_uv_sculpt)
			glScalef(zoomx, zoomy, 1.0f);

		glColor4f(brush->add_col[0], brush->add_col[1], brush->add_col[2], alpha);
		glEnable( GL_LINE_SMOOTH );
		glEnable(GL_BLEND);
		glutil_draw_lined_arc(0, (float)(M_PI*2.0), size, 40);
		glDisable(GL_BLEND);
		glDisable( GL_LINE_SMOOTH );

		glPopMatrix();
	}
#undef PX_SIZE_FADE_MAX
#undef PX_SIZE_FADE_MIN
}

static void toggle_paint_cursor(bContext *C, int enable)
{
	wmWindowManager *wm= CTX_wm_manager(C);
	Scene *scene = CTX_data_scene(C);
	ToolSettings *settings= scene->toolsettings;

	if(settings->imapaint.paintcursor && !enable) {
		WM_paint_cursor_end(wm, settings->imapaint.paintcursor);
		settings->imapaint.paintcursor = NULL;
	}
	else if(enable)
		settings->imapaint.paintcursor= WM_paint_cursor_activate(wm, image_paint_poll, brush_drawcursor, NULL);
}

/* enable the paint cursor if it isn't already.

   purpose is to make sure the paint cursor is shown if paint
   mode is enabled in the image editor. the paint poll will
   ensure that the cursor is hidden when not in paint mode */
void ED_space_image_paint_update(wmWindowManager *wm, ToolSettings *settings)
{
	ImagePaintSettings *imapaint = &settings->imapaint;

	if(!imapaint->paintcursor) {
		imapaint->paintcursor =
			WM_paint_cursor_activate(wm, image_paint_poll,
						 brush_drawcursor, NULL);
	}
}


void ED_space_image_uv_sculpt_update(wmWindowManager *wm, ToolSettings *settings)
{
	if(settings->use_uv_sculpt) {
		if(!settings->uvsculpt) {
			settings->uvsculpt = MEM_callocN(sizeof(*settings->uvsculpt), "UV Smooth paint");
			settings->uv_sculpt_tool = UV_SCULPT_TOOL_GRAB;
			settings->uv_sculpt_settings = UV_SCULPT_LOCK_BORDERS | UV_SCULPT_ALL_ISLANDS;
			settings->uv_relax_method = UV_SCULPT_TOOL_RELAX_LAPLACIAN;
		}

		paint_init(&settings->uvsculpt->paint, PAINT_CURSOR_SCULPT);

		WM_paint_cursor_activate(wm, uv_sculpt_brush_poll,
			brush_drawcursor, NULL);
	}
	else {
		if(settings->uvsculpt)
			settings->uvsculpt->paint.flags &= ~PAINT_SHOW_BRUSH;
	}
}
/************************ grab clone operator ************************/

typedef struct GrabClone {
	float startoffset[2];
	int startx, starty;
} GrabClone;

static void grab_clone_apply(bContext *C, wmOperator *op)
{
	Brush *brush= image_paint_brush(C);
	float delta[2];

	RNA_float_get_array(op->ptr, "delta", delta);
	add_v2_v2(brush->clone.offset, delta);
	ED_region_tag_redraw(CTX_wm_region(C));
}

static int grab_clone_exec(bContext *C, wmOperator *op)
{
	grab_clone_apply(C, op);

	return OPERATOR_FINISHED;
}

static int grab_clone_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Brush *brush= image_paint_brush(C);
	GrabClone *cmv;

	cmv= MEM_callocN(sizeof(GrabClone), "GrabClone");
	copy_v2_v2(cmv->startoffset, brush->clone.offset);
	cmv->startx= event->x;
	cmv->starty= event->y;
	op->customdata= cmv;

	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

static int grab_clone_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	Brush *brush= image_paint_brush(C);
	ARegion *ar= CTX_wm_region(C);
	GrabClone *cmv= op->customdata;
	float startfx, startfy, fx, fy, delta[2];
	int xmin= ar->winrct.xmin, ymin= ar->winrct.ymin;

	switch(event->type) {
		case LEFTMOUSE:
		case MIDDLEMOUSE:
		case RIGHTMOUSE: // XXX hardcoded
			MEM_freeN(op->customdata);
			return OPERATOR_FINISHED;
		case MOUSEMOVE:
			/* mouse moved, so move the clone image */
			UI_view2d_region_to_view(&ar->v2d, cmv->startx - xmin, cmv->starty - ymin, &startfx, &startfy);
			UI_view2d_region_to_view(&ar->v2d, event->x - xmin, event->y - ymin, &fx, &fy);

			delta[0]= fx - startfx;
			delta[1]= fy - startfy;
			RNA_float_set_array(op->ptr, "delta", delta);

			copy_v2_v2(brush->clone.offset, cmv->startoffset);

			grab_clone_apply(C, op);
			break;
	}

	return OPERATOR_RUNNING_MODAL;
}

static int grab_clone_cancel(bContext *UNUSED(C), wmOperator *op)
{
	MEM_freeN(op->customdata);
	return OPERATOR_CANCELLED;
}

void PAINT_OT_grab_clone(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Grab Clone";
	ot->idname= "PAINT_OT_grab_clone";
	
	/* api callbacks */
	ot->exec= grab_clone_exec;
	ot->invoke= grab_clone_invoke;
	ot->modal= grab_clone_modal;
	ot->cancel= grab_clone_cancel;
	ot->poll= image_paint_2d_clone_poll;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;

	/* properties */
	RNA_def_float_vector(ot->srna, "delta", 2, NULL, -FLT_MAX, FLT_MAX, "Delta", "Delta offset of clone image in 0.0..1.0 coordinates", -1.0f, 1.0f);
}

/******************** sample color operator ********************/

static int sample_color_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Brush *brush= image_paint_brush(C);
	ARegion *ar= CTX_wm_region(C);
	int location[2];

	RNA_int_get_array(op->ptr, "location", location);
	paint_sample_color(scene, ar, location[0], location[1]);

	WM_event_add_notifier(C, NC_BRUSH|NA_EDITED, brush);
	
	return OPERATOR_FINISHED;
}

static int sample_color_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	RNA_int_set_array(op->ptr, "location", event->mval);
	sample_color_exec(C, op);

	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

static int sample_color_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	switch(event->type) {
		case LEFTMOUSE:
		case RIGHTMOUSE: // XXX hardcoded
			return OPERATOR_FINISHED;
		case MOUSEMOVE:
			RNA_int_set_array(op->ptr, "location", event->mval);
			sample_color_exec(C, op);
			break;
	}

	return OPERATOR_RUNNING_MODAL;
}

/* same as image_paint_poll but fail when face mask mode is enabled */
static int image_paint_sample_color_poll(bContext *C)
{
	if(image_paint_poll(C)) {
		if(CTX_wm_view3d(C)) {
			Object *obact = CTX_data_active_object(C);
			if (obact && obact->mode & OB_MODE_TEXTURE_PAINT) {
				Mesh *me= get_mesh(obact);
				if(me) {
					return !(me->editflag & ME_EDIT_PAINT_MASK);
				}
			}
		}

		return 1;
	}

	return 0;
}

void PAINT_OT_sample_color(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Sample Color";
	ot->idname= "PAINT_OT_sample_color";
	
	/* api callbacks */
	ot->exec= sample_color_exec;
	ot->invoke= sample_color_invoke;
	ot->modal= sample_color_modal;
	ot->poll= image_paint_sample_color_poll;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_int_vector(ot->srna, "location", 2, NULL, 0, INT_MAX, "Location", "Cursor location in region coordinates", 0, 16384);
}

/******************** set clone cursor operator ********************/

static int set_clone_cursor_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	View3D *v3d= CTX_wm_view3d(C);
	float *cursor= give_cursor(scene, v3d);

	RNA_float_get_array(op->ptr, "location", cursor);
	
	ED_area_tag_redraw(CTX_wm_area(C));
	
	return OPERATOR_FINISHED;
}

static int set_clone_cursor_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Scene *scene= CTX_data_scene(C);
	View3D *v3d= CTX_wm_view3d(C);
	ARegion *ar= CTX_wm_region(C);
	float location[3];

	view3d_operator_needs_opengl(C);

	if(!ED_view3d_autodist(scene, ar, v3d, event->mval, location))
		return OPERATOR_CANCELLED;

	RNA_float_set_array(op->ptr, "location", location);

	return set_clone_cursor_exec(C, op);
}

void PAINT_OT_clone_cursor_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Set Clone Cursor";
	ot->idname= "PAINT_OT_clone_cursor_set";
	
	/* api callbacks */
	ot->exec= set_clone_cursor_exec;
	ot->invoke= set_clone_cursor_invoke;
	ot->poll= image_paint_3d_poll;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_float_vector(ot->srna, "location", 3, NULL, -FLT_MAX, FLT_MAX, "Location", "Cursor location in world space coordinates", -10000.0f, 10000.0f);
}

/******************** texture paint toggle operator ********************/

static int texture_paint_toggle_poll(bContext *C)
{
	if(CTX_data_edit_object(C))
		return 0;
	if(CTX_data_active_object(C)==NULL)
		return 0;

	return 1;
}

static int texture_paint_toggle_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_active_object(C);
	Mesh *me= NULL;
	
	if(ob==NULL)
		return OPERATOR_CANCELLED;
	
	if (object_data_is_libdata(ob)) {
		BKE_report(op->reports, RPT_ERROR, "Can't edit external libdata");
		return OPERATOR_CANCELLED;
	}

	me= get_mesh(ob);

	if(!(ob->mode & OB_MODE_TEXTURE_PAINT) && !me) {
		BKE_report(op->reports, RPT_ERROR, "Can only enter texture paint mode for mesh objects");
		return OPERATOR_CANCELLED;
	}

	if(ob->mode & OB_MODE_TEXTURE_PAINT) {
		ob->mode &= ~OB_MODE_TEXTURE_PAINT;

		if(U.glreslimit != 0)
			GPU_free_images();
		GPU_paint_set_mipmap(1);

		toggle_paint_cursor(C, 0);
	}
	else {
		ob->mode |= OB_MODE_TEXTURE_PAINT;

		if(me->mtface==NULL)
			me->mtface= CustomData_add_layer(&me->fdata, CD_MTFACE, CD_DEFAULT,
							 NULL, me->totface);

		paint_init(&scene->toolsettings->imapaint.paint, PAINT_CURSOR_TEXTURE_PAINT);

		if(U.glreslimit != 0)
			GPU_free_images();
		GPU_paint_set_mipmap(0);

		toggle_paint_cursor(C, 1);
	}

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_SCENE|ND_MODE, scene);

	return OPERATOR_FINISHED;
}

void PAINT_OT_texture_paint_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Texture Paint Toggle";
	ot->idname= "PAINT_OT_texture_paint_toggle";
	
	/* api callbacks */
	ot->exec= texture_paint_toggle_exec;
	ot->poll= texture_paint_toggle_poll;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int texture_paint_poll(bContext *C)
{
	if(texture_paint_toggle_poll(C))
		if(CTX_data_active_object(C)->mode & OB_MODE_TEXTURE_PAINT)
			return 1;
	
	return 0;
}

int image_texture_paint_poll(bContext *C)
{
	return (texture_paint_poll(C) || image_paint_poll(C));
}

int uv_sculpt_poll(bContext *C)
{
	return uv_sculpt_brush_poll(C);
}

int facemask_paint_poll(bContext *C)
{
	return paint_facesel_test(CTX_data_active_object(C));
}

int vert_paint_poll(bContext *C)
{
	return paint_vertsel_test(CTX_data_active_object(C));
}

int mask_paint_poll(bContext *C)
{
	return paint_facesel_test(CTX_data_active_object(C)) || paint_vertsel_test(CTX_data_active_object(C));
}
/* use project paint to re-apply an image */
static int texture_paint_camera_project_exec(bContext *C, wmOperator *op)
{
	Image *image= BLI_findlink(&CTX_data_main(C)->image, RNA_enum_get(op->ptr, "image"));
	Scene *scene= CTX_data_scene(C);
	ProjPaintState ps= {NULL};
	int orig_brush_size;
	IDProperty *idgroup;
	IDProperty *view_data= NULL;

	project_state_init(C, OBACT, &ps);

	if(ps.ob==NULL || ps.ob->type != OB_MESH) {
		BKE_report(op->reports, RPT_ERROR, "No active mesh object");
		return OPERATOR_CANCELLED;
	}

	if(image==NULL) {
		BKE_report(op->reports, RPT_ERROR, "Image could not be found");
		return OPERATOR_CANCELLED;
	}

	ps.reproject_image= image;
	ps.reproject_ibuf= BKE_image_get_ibuf(image, NULL);

	if(ps.reproject_ibuf==NULL || ps.reproject_ibuf->rect==NULL) {
		BKE_report(op->reports, RPT_ERROR, "Image data could not be found");
		return OPERATOR_CANCELLED;
	}

	idgroup= IDP_GetProperties(&image->id, 0);

	if(idgroup) {
		view_data= IDP_GetPropertyTypeFromGroup(idgroup, PROJ_VIEW_DATA_ID, IDP_ARRAY);

		/* type check to make sure its ok */
		if(view_data->len != PROJ_VIEW_DATA_SIZE || view_data->subtype != IDP_FLOAT) {
			BKE_report(op->reports, RPT_ERROR, "Image project data invalid");
			return OPERATOR_CANCELLED;
		}
	}

	if(view_data) {
		/* image has stored view projection info */
		ps.source= PROJ_SRC_IMAGE_VIEW;
	}
	else {
		ps.source= PROJ_SRC_IMAGE_CAM;

		if(scene->camera==NULL) {
			BKE_report(op->reports, RPT_ERROR, "No active camera set");
			return OPERATOR_CANCELLED;
		}
	}

	/* override */
	ps.is_texbrush= 0;
	ps.is_airbrush= 1;
	orig_brush_size= brush_size(scene, ps.brush);
	brush_set_size(scene, ps.brush, 32); /* cover the whole image */

	ps.tool= PAINT_TOOL_DRAW; /* so pixels are initialized with minimal info */

	scene->toolsettings->imapaint.flag |= IMAGEPAINT_DRAWING;

	undo_paint_push_begin(UNDO_PAINT_IMAGE, op->type->name,
		image_undo_restore, image_undo_free);

	/* allocate and initialize spacial data structures */
	project_paint_begin(&ps);

	if(ps.dm==NULL) {
		brush_set_size(scene, ps.brush, orig_brush_size);
		return OPERATOR_CANCELLED;
	}
	else {
		float pos[2]= {0.0, 0.0};
		float lastpos[2]= {0.0, 0.0};
		int a;

		for (a=0; a < ps.image_tot; a++)
			partial_redraw_array_init(ps.projImages[a].partRedrawRect);

		project_paint_op(&ps, NULL, lastpos, pos);

		project_image_refresh_tagged(&ps);

		for (a=0; a < ps.image_tot; a++) {
			GPU_free_image(ps.projImages[a].ima);
			WM_event_add_notifier(C, NC_IMAGE|NA_EDITED, ps.projImages[a].ima);
		}
	}

	project_paint_end(&ps);

	scene->toolsettings->imapaint.flag &= ~IMAGEPAINT_DRAWING;
	brush_set_size(scene, ps.brush, orig_brush_size);

	return OPERATOR_FINISHED;
}

void PAINT_OT_project_image(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name= "Project Image";
	ot->idname= "PAINT_OT_project_image";
	ot->description= "Project an edited render from the active camera back onto the object";

	/* api callbacks */
	ot->invoke= WM_enum_search_invoke;
	ot->exec= texture_paint_camera_project_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	prop= RNA_def_enum(ot->srna, "image", DummyRNA_NULL_items, 0, "Image", "");
	RNA_def_enum_funcs(prop, RNA_image_itemf);
	ot->prop= prop;
}

static int texture_paint_image_from_view_exec(bContext *C, wmOperator *op)
{
	Image *image;
	ImBuf *ibuf;
	char filename[FILE_MAX];

	Scene *scene= CTX_data_scene(C);
	ToolSettings *settings= scene->toolsettings;
	int w= settings->imapaint.screen_grab_size[0];
	int h= settings->imapaint.screen_grab_size[1];
	int maxsize;
	char err_out[256]= "unknown";

	RNA_string_get(op->ptr, "filepath", filename);

	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxsize);

	if(w > maxsize) w= maxsize;
	if(h > maxsize) h= maxsize;

	ibuf= ED_view3d_draw_offscreen_imbuf(CTX_data_scene(C), CTX_wm_view3d(C), CTX_wm_region(C), w, h, IB_rect, err_out);
	if(!ibuf) {
		/* Mostly happens when OpenGL offscreen buffer was failed to create, */
		/* but could be other reasons. Should be handled in the future. nazgul */
		BKE_reportf(op->reports, RPT_ERROR, "Failed to create OpenGL offscreen buffer: %s", err_out);
		return OPERATOR_CANCELLED;
	}

	image= BKE_add_image_imbuf(ibuf);

	if(image) {
		/* now for the trickyness. store the view projection here!
		 * reprojection will reuse this */
		View3D *v3d= CTX_wm_view3d(C);
		RegionView3D *rv3d= CTX_wm_region_view3d(C);

		IDPropertyTemplate val;
		IDProperty *idgroup= IDP_GetProperties(&image->id, 1);
		IDProperty *view_data;
		int orth;
		float *array;

		val.array.len = PROJ_VIEW_DATA_SIZE;
		val.array.type = IDP_FLOAT;
		view_data = IDP_New(IDP_ARRAY, &val, PROJ_VIEW_DATA_ID);

		array= (float *)IDP_Array(view_data);
		memcpy(array, rv3d->winmat, sizeof(rv3d->winmat)); array += sizeof(rv3d->winmat)/sizeof(float);
		memcpy(array, rv3d->viewmat, sizeof(rv3d->viewmat)); array += sizeof(rv3d->viewmat)/sizeof(float);
		orth= project_paint_view_clip(v3d, rv3d, &array[0], &array[1]);
		array[2]= orth ? 1.0f : 0.0f; /* using float for a bool is dodgy but since its an extra member in the array... easier then adding a single bool prop */

		IDP_AddToGroup(idgroup, view_data);

		rename_id(&image->id, "image_view");
	}

	return OPERATOR_FINISHED;
}

void PAINT_OT_image_from_view(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Image from View";
	ot->idname= "PAINT_OT_image_from_view";
	ot->description= "Make an image from the current 3D view for re-projection";

	/* api callbacks */
	ot->exec= texture_paint_image_from_view_exec;
	ot->poll= ED_operator_region_view3d_active;

	/* flags */
	ot->flag= OPTYPE_REGISTER;

	RNA_def_string_file_name(ot->srna, "filepath", "", FILE_MAX, "File Path", "Name of the file");
}
