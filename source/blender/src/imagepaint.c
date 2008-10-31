/**
 * $Id$
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
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

#include <string.h>
#include <stdio.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#ifdef WIN32
#include "BLI_winstuff.h"
#endif
#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_dynstr.h"
#include "BLI_linklist.h"
#include "BLI_memarena.h"
#include "PIL_time.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "DNA_brush_types.h"
#include "DNA_image_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"

#include "BKE_brush.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_node.h"
#include "BKE_utildefines.h"
#include "BKE_DerivedMesh.h"

#include "BIF_interface.h"
#include "BIF_mywindow.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"

#include "BSE_drawipo.h"
#include "BSE_node.h"
#include "BSE_trans_types.h"
#include "BSE_view.h"

#include "BDR_imagepaint.h"
#include "BDR_vpaint.h"

#include "RE_raytrace.h" /* For projection painting occlusion */

#include "GPU_draw.h"

#include "GHOST_Types.h"

#include "blendef.h"
#include "butspace.h"
#include "mydevice.h"

/* Defines and Structs */

#define IMAPAINT_CHAR_TO_FLOAT(c) (c/255.0f)

#define IMAPAINT_FLOAT_RGB_TO_CHAR(c, f) { c[0]=FTOCHAR(f[0]); \
	c[1]=FTOCHAR(f[1]); c[2]=FTOCHAR(f[2]); }
#define IMAPAINT_CHAR_RGB_TO_FLOAT(f, c) { f[0]=IMAPAINT_CHAR_TO_FLOAT(c[0]); \
	f[1]=IMAPAINT_CHAR_TO_FLOAT(c[1]); f[2]=IMAPAINT_CHAR_TO_FLOAT(c[2]); }
#define IMAPAINT_FLOAT_RGB_COPY(a, b) VECCOPY(a, b)

#define IMAPAINT_TILE_BITS			6
#define IMAPAINT_TILE_SIZE			(1 << IMAPAINT_TILE_BITS)
#define IMAPAINT_TILE_NUMBER(size)	(((size)+IMAPAINT_TILE_SIZE-1) >> IMAPAINT_TILE_BITS)

#define MAXUNDONAME	64

typedef struct ImagePaintState {
	Brush *brush;
	short tool, blend;
	Image *image;
	ImBuf *canvas;
	ImBuf *clonecanvas;
	short clonefreefloat;
	short project;			/* is projection texture painting enabled */
	char *warnpackedfile;
	char *warnmultifile;

	/* texture paint only */
	Object *ob;
	Mesh *me;
	int faceindex;
	float uv[2];
} ImagePaintState;


/* testing options */
#define PROJ_BUCKET_DIV 128 /* TODO - test other values, this is a guess, seems ok */

#define PROJ_LAZY_INIT 1
// #define PROJ_PAINT_DEBUG 1

/* projectFaceFlags options */
#define PROJ_FACE_IGNORE	1<<0	/* When the face is hidden, backfacing or occluded */
#define PROJ_FACE_INIT	1<<1	/* When we have initialized the faces data */
#define PROJ_FACE_OWNED	1<<2	/* When the face is in 1 bucket */

#define PROJ_BUCKET_NULL	0
#define PROJ_BUCKET_INIT	1

/* only for readability */
#define PROJ_BUCKET_LEFT		0
#define PROJ_BUCKET_RIGHT	1
#define PROJ_BUCKET_BOTTOM	2
#define PROJ_BUCKET_TOP		3

typedef struct ProjectPaintState {
	
	DerivedMesh    *dm;
	int 			dm_totface;
	int 			dm_totvert;
	
	MVert 		   *dm_mvert;
	MFace 		   *dm_mface;
	MTFace 		   *dm_mtface;
	
	
	/* projection painting only */
	MemArena *projectArena;		/* use for alocating many pixel structs and link-lists */
	LinkNode **projectBuckets;	/* screen sized 2D array, each pixel has a linked list of ImagePaintProjectPixel's */
#ifdef PROJ_LAZY_INIT
	LinkNode **projectFaces;	/* projectBuckets alligned array linkList of faces overlapping each bucket */
	char *projectBucketFlags;	/* store if the bucks have been initialized  */
	char *projectFaceFlags;		/* store info about faces, if they are initialized etc*/
#endif
	int bucketsX;				/* The size of the bucket grid, the grid span's viewMin2D/viewMax2D so you can paint outsize the screen or with 2 brushes at once */
	int bucketsY;
	
	Image **projectImages;		/* array of images we are painting onto while, use so we can tag for updates */
	int projectImageTotal;		/* size of projectImages array */
	int image_index;			/* current image, use for context switching */
	
	float (*projectVertCos2D)[2];	/* verts projected into floating point screen space */
	
	RayTree *projectRayTree;	/* ray tracing acceleration structure */
	Isect isec;					/* re-use ray intersection var */
	
	/* options for projection painting */
	short projectOcclude;		/* Use raytraced occlusion? - ortherwise will paint right through to the back*/
	short projectBackfaceCull;	/* ignore faces with normals pointing away, skips a lot of raycasts if your normals are correctly flipped */
	
	float projectMat[4][4];		/* Projection matrix, use for getting screen coords */
	float viewMat[4][4];
	float viewPoint[3];			/* Use only when in perspective mode with projectOcclude, the point we are viewing from, cast rays to this */
	float viewDir[3];			/* View vector, use for projectBackfaceCull and for ray casting with an ortho viewport  */
	
	float viewMin2D[2];			/* 2D bounds for mesh verts on the screen's plane (screenspace) */
	float viewMax2D[2]; 
	float viewWidth;			/* Calculated from viewMin2D & viewMax2D */
	float viewHeight;
	
	
	
} ProjectPaintState;

typedef struct ImagePaintProjectPixel {
	float projCo2D[2]; /* the floating point screen projection of this pixel */
	char *pixel;
	int image_index;
} ImagePaintProjectPixel;

/* Finish projection painting structs */


typedef struct UndoTile {
	struct UndoTile *next, *prev;
	ID id;
	void *rect;
	int x, y;
} UndoTile;

typedef struct UndoElem {
	struct UndoElem *next, *prev;
	char name[MAXUNDONAME];
	unsigned long undosize;

	ImBuf *ibuf;
	ListBase tiles;
} UndoElem;

typedef struct ImagePaintPartialRedraw {
	int x1, y1, x2, y2;
	int enabled;
} ImagePaintPartialRedraw;

static ListBase undobase = {NULL, NULL};
static UndoElem *curundo = NULL;
static ImagePaintPartialRedraw imapaintpartial = {0, 0, 0, 0, 0};

/* UNDO */

/* internal functions */

static void undo_copy_tile(UndoTile *tile, ImBuf *tmpibuf, ImBuf *ibuf, int restore)
{
	/* copy or swap contents of tile->rect and region in ibuf->rect */
	IMB_rectcpy(tmpibuf, ibuf, 0, 0, tile->x*IMAPAINT_TILE_SIZE,
		tile->y*IMAPAINT_TILE_SIZE, IMAPAINT_TILE_SIZE, IMAPAINT_TILE_SIZE);

	if(ibuf->rect_float) SWAP(void*, tmpibuf->rect_float, tile->rect)
	else SWAP(void*, tmpibuf->rect, tile->rect)
	
	if(restore)
		IMB_rectcpy(ibuf, tmpibuf, tile->x*IMAPAINT_TILE_SIZE,
			tile->y*IMAPAINT_TILE_SIZE, 0, 0, IMAPAINT_TILE_SIZE, IMAPAINT_TILE_SIZE);
}

static void undo_restore(UndoElem *undo)
{
	Image *ima = NULL;
	ImBuf *ibuf, *tmpibuf;
	UndoTile *tile;

	if(!undo)
		return;

	tmpibuf= IMB_allocImBuf(IMAPAINT_TILE_SIZE, IMAPAINT_TILE_SIZE, 32,
	                        IB_rectfloat|IB_rect, 0);
	
	for(tile=undo->tiles.first; tile; tile=tile->next) {
		/* find image based on name, pointer becomes invalid with global undo */
		if(ima && strcmp(tile->id.name, ima->id.name)==0);
		else {
			for(ima=G.main->image.first; ima; ima=ima->id.next)
				if(strcmp(tile->id.name, ima->id.name)==0)
					break;
		}

		ibuf= BKE_image_get_ibuf(ima, NULL);

		if (!ima || !ibuf || !(ibuf->rect || ibuf->rect_float))
			continue;

		undo_copy_tile(tile, tmpibuf, ibuf, 1);

		GPU_free_image(ima); /* force OpenGL reload */
		if(ibuf->rect_float)
			imb_freerectImBuf(ibuf); /* force recreate of char rect */
	}

	IMB_freeImBuf(tmpibuf);
}

static void undo_free(UndoElem *undo)
{
	UndoTile *tile;

	for(tile=undo->tiles.first; tile; tile=tile->next)
		MEM_freeN(tile->rect);
	BLI_freelistN(&undo->tiles);
}

static void undo_imagepaint_push_begin(char *name)
{
	UndoElem *uel;
	int nr;
	
	/* Undo push is split up in begin and end, the reason is that as painting
	 * happens more tiles are added to the list, and at the very end we know
	 * how much memory the undo used to remove old undo elements */

	/* remove all undos after (also when curundo==NULL) */
	while(undobase.last != curundo) {
		uel= undobase.last;
		undo_free(uel);
		BLI_freelinkN(&undobase, uel);
	}
	
	/* make new */
	curundo= uel= MEM_callocN(sizeof(UndoElem), "undo file");
	BLI_addtail(&undobase, uel);

	/* name can be a dynamic string */
	strncpy(uel->name, name, MAXUNDONAME-1);
	
	/* limit amount to the maximum amount*/
	nr= 0;
	uel= undobase.last;
	while(uel) {
		nr++;
		if(nr==U.undosteps) break;
		uel= uel->prev;
	}
	if(uel) {
		while(undobase.first!=uel) {
			UndoElem *first= undobase.first;
			undo_free(first);
			BLI_freelinkN(&undobase, first);
		}
	}
}

static void undo_imagepaint_push_end()
{
	UndoElem *uel;
	unsigned long totmem, maxmem;

	if(U.undomemory != 0) {
		/* limit to maximum memory (afterwards, we can't know in advance) */
		totmem= 0;
		maxmem= ((unsigned long)U.undomemory)*1024*1024;

		uel= undobase.last;
		while(uel) {
			totmem+= uel->undosize;
			if(totmem>maxmem) break;
			uel= uel->prev;
		}

		if(uel) {
			while(undobase.first!=uel) {
				UndoElem *first= undobase.first;
				undo_free(first);
				BLI_freelinkN(&undobase, first);
			}
		}
	}
}


static MVert * mvert_static = NULL;
static void project_paint_begin_coords_func(RayFace *face, float **v1, float **v2, float **v3, float **v4)
{
	MFace *mface= (MFace*)face;

	*v1= mvert_static[mface->v1].co;
	*v2= mvert_static[mface->v2].co;
	*v3= mvert_static[mface->v3].co;
	*v4= (mface->v4)? mvert_static[mface->v4].co: NULL;
}

static int project_paint_begin_check_func(Isect *is, int ob, RayFace *face)
{
	return 1;
}
static int project_paint_BucketOffset(ProjectPaintState *ps, float *projCo2D)
{
	
	/* If we were not dealing with screenspace 2D coords we could simple do...
	 * ps->projectBuckets[x + (y*ps->bucketsY)] */
	
	/* please explain?
	 * projCo2D[0] - ps->viewMin2D[0]	: zero origin
	 * ... / ps->viewWidth				: range from 0.0 to 1.0
	 * ... * ps->bucketsX		: use as a bucket index
	 *
	 * Second multiplication does similar but for vertical offset
	 */
	return	(	(int)(( (projCo2D[0] - ps->viewMin2D[0]) / ps->viewWidth)  * ps->bucketsX)) + 
		(	(	(int)(( (projCo2D[1] - ps->viewMin2D[1])  / ps->viewHeight) * ps->bucketsY)) * ps->bucketsX );
}

static void project_paint_face_init(ProjectPaintState *ps, MFace *mf, MTFace *tf, ImBuf *ibuf)
{
	/* Projection vars, to get the 3D locations into screen space  */
	ImagePaintProjectPixel *projPixel;
	
	float pxWorldCo[3];
	float pxProjCo[4];
	
	/* UV/pixel seeking data */
	int x; /* Image X-Pixel */
	int y;/* Image Y-Pixel */
	float uv[2]; /* Image floating point UV - same as x,y but from 0.0-1.0 */
	int pixel_size = 4;	/* each pixel is 4 x 8-bits packed in unsigned int */
	float xmin, ymin, xmax, ymax; /* UV bounds */
	int xmini, ymini, xmaxi, ymaxi; /* UV Bounds converted to int's for pixel */
	float w1, w2, w3, wtot; /* weights for converting the pixel into 3d worldspace coords */
	float *v1co, *v2co, *v3co, *v4co; /* for convenience only */
	
	int i;
	
	/* clamp to 0-1 for now */
	xmin = ymin = 1.0f;
	xmax = ymax = 0.0f;
	
	i = mf->v4 ? 3:2;
	do {
		xmin = MIN2(xmin, tf->uv[i][0]);
		ymin = MIN2(ymin, tf->uv[i][1]);
		
		xmax = MAX2(xmax, tf->uv[i][0]);
		ymax = MAX2(ymax, tf->uv[i][1]);
	} while (i--);
	
	xmini = (int)(ibuf->x * xmin);
	ymini = (int)(ibuf->y * ymin);
	
	xmaxi = (int)(ibuf->x * xmax) +1;
	ymaxi = (int)(ibuf->y * ymax) +1;
	
	/*printf("%d %d %d %d \n", xmini, ymini, xmaxi, ymaxi);*/
	
	if (xmini < 0) xmini = 0;
	if (ymini < 0) ymini = 0;
	
	if (xmaxi > ibuf->x) xmaxi = ibuf->x;
	if (ymaxi > ibuf->y) ymaxi = ibuf->y;
	
	/* face uses no UV area when quanticed to pixels? */
	if (xmini == xmaxi || ymini == ymaxi)
		return;

	v1co = ps->dm_mvert[mf->v1].co;
	v2co = ps->dm_mvert[mf->v2].co;
	v3co = ps->dm_mvert[mf->v3].co;
	if (mf->v4)
		v4co = ps->dm_mvert[mf->v4].co;
	
	for (y = ymini; y < ymaxi; y++) {
		uv[1] = (((float)y)+0.5) / (float)ibuf->y;
		for (x = xmini; x < xmaxi; x++) {
			uv[0] = (((float)x)+0.5) / (float)ibuf->x;
			
			wtot = -1.0;
			if ( IsectPT2Df( uv, tf->uv[0], tf->uv[1], tf->uv[2] )) {
				
				w1 = AreaF2Dfl(tf->uv[1], tf->uv[2], uv);
				w2 = AreaF2Dfl(tf->uv[2], tf->uv[0], uv);
				w3 = AreaF2Dfl(tf->uv[0], tf->uv[1], uv);
				wtot = w1 + w2 + w3;
				
				w1 /= wtot; w2 /= wtot; w3 /= wtot;
				
				i=2;
				do {
					pxWorldCo[i] = v1co[i]*w1 + v2co[i]*w2 + v3co[i]*w3;
				} while (i--);
					
				
			} else if ( mf->v4 && IsectPT2Df( uv, tf->uv[0], tf->uv[2], tf->uv[3] ) ) {
				
				w1 = AreaF2Dfl(tf->uv[2], tf->uv[3], uv);
				w2 = AreaF2Dfl(tf->uv[3], tf->uv[0], uv);
				w3 = AreaF2Dfl(tf->uv[0], tf->uv[2], uv);
				wtot = w1 + w2 + w3;
				
				w1 /= wtot; w2 /= wtot; w3 /= wtot;
				
				i=2;
				do {
					pxWorldCo[i] = v1co[i]*w1 + v3co[i]*w2 + v4co[i]*w3;
				} while (i--);
			}
			
			/* view3d_project_float(curarea, vec, projCo2D, s->projectMat);
			if (projCo2D[0]==IS_CLIPPED)
				continue;*/
			if (wtot != -1.0) {
				
				/* Inline, a bit faster */
				VECCOPY(pxProjCo, pxWorldCo);
				pxProjCo[3] = 1.0;
				
				Mat4MulVec4fl(ps->projectMat, pxProjCo);
				
				if( pxProjCo[3] > 0.001 ) {
					/* Use viewMin2D to make (0,0) the bottom left of the bounds 
					 * Then this can be used to index the bucket array */
					
					if (ps->projectOcclude) {
						VECCOPY(ps->isec.start, pxWorldCo);
						
						if (G.vd->persp==V3D_ORTHO) {
							VecAddf(ps->isec.end, pxWorldCo, ps->viewDir);
						} else { /* value dosnt change but it is modified by RE_ray_tree_intersect() - keep this line */
							VECCOPY(ps->isec.end, ps->viewPoint);
						}

						ps->isec.faceorig = mf;
					}
					
					/* Is this UV visible from the view? - raytrace */
					if (ps->projectOcclude==0 || !RE_ray_tree_intersect(ps->projectRayTree, &ps->isec)) {
						
						/* done with view3d_project_float inline */
						projPixel = (ImagePaintProjectPixel *)BLI_memarena_alloc( ps->projectArena, sizeof(ImagePaintProjectPixel) );
						
						/* screenspace unclamped */
						projPixel->projCo2D[0] = (float)(curarea->winx/2.0)+(curarea->winx/2.0)*pxProjCo[0]/pxProjCo[3];
						projPixel->projCo2D[1] = (float)(curarea->winy/2.0)+(curarea->winy/2.0)*pxProjCo[1]/pxProjCo[3];
						
						projPixel->pixel = (( char * ) ibuf->rect) + (( x + y * ibuf->x ) * pixel_size);
#ifdef PROJ_PAINT_DEBUG
						projPixel->pixel[1] = 0;
#endif
						projPixel->image_index = ps->image_index;
						
						BLI_linklist_prepend_arena(
							&ps->projectBuckets[ project_paint_BucketOffset(ps, projPixel->projCo2D) ],
							projPixel,
							ps->projectArena
						);
					}
				}
			}
		}
	}
}


/* takes floating point screenspace min/max and returns int min/max to be used as indicies for ps->projectBuckets, ps->projectBucketFlags */
static void project_paint_rect(ProjectPaintState *ps, float min[2], float max[2], int bucket_min[2], int bucket_max[2])
{
	/* divide by bucketWidth & bucketHeight so the bounds are offset in bucket grid units */
	bucket_min[0] = (int)(((float)(min[0] - ps->viewMin2D[0]) / ps->viewWidth) * ps->bucketsX) + 0.5;
	bucket_min[1] = (int)(((float)(min[1] - ps->viewMin2D[1]) / ps->viewHeight) * ps->bucketsY) + 0.5;
	
	bucket_max[0] = (int)(((float)(max[0] - ps->viewMin2D[0]) / ps->viewWidth) * ps->bucketsX) + 1.5;
	bucket_max[1] = (int)(((float)(max[1] - ps->viewMin2D[1]) / ps->viewHeight) * ps->bucketsY) + 1.5;	
	
	/* incase the rect is outside the mesh 2d bounds */
	CLAMP(bucket_min[0], 0, ps->bucketsX);
	CLAMP(bucket_min[1], 0, ps->bucketsY);
	
	CLAMP(bucket_max[0], 0, ps->bucketsX);
	CLAMP(bucket_max[1], 0, ps->bucketsY);
}

static void project_bucket_bounds(ProjectPaintState *ps, int bucket_x, int bucket_y, float bucket_bounds[4])
{
	bucket_bounds[ PROJ_BUCKET_LEFT ] =		ps->viewMin2D[0]+((bucket_x)*(ps->viewWidth / ps->bucketsX));		/* left */
	bucket_bounds[ PROJ_BUCKET_RIGHT ] =		ps->viewMin2D[0]+((bucket_x+1)*(ps->viewWidth / ps->bucketsX));	/* right */
	
	bucket_bounds[ PROJ_BUCKET_BOTTOM ] =	ps->viewMin2D[1]+((bucket_y)*(ps->viewHeight / ps->bucketsY));		/* bottom */
	bucket_bounds[ PROJ_BUCKET_TOP ] =		ps->viewMin2D[1]+((bucket_y+1)*(ps->viewHeight  / ps->bucketsY));	/* top */
}

#ifdef PROJ_LAZY_INIT
static void project_paint_bucket_init(ProjectPaintState *ps, int bucket_index)
{
	
	LinkNode *node;
	int face_index;
	ImBuf *ibuf;
	MTFace *tf;
	
	/*printf("\tinit bucket %d\n", bucket_index);*/
	
	ps->projectBucketFlags[bucket_index] = PROJ_BUCKET_INIT; 
	
	if ((node = ps->projectFaces[bucket_index])) {
		do {
			face_index = (int)node->link;
			/* Have we initialized this face in another bucket? */
			if ((ps->projectFaceFlags[face_index] & PROJ_FACE_INIT)==0) {
				
				ps->projectFaceFlags[face_index] |= PROJ_FACE_INIT;
				
				tf = ps->dm_mtface+face_index;
				ibuf = BKE_image_get_ibuf((Image *)tf->tpage, NULL); /* TODO - this may be slow */
				
				project_paint_face_init(ps, ps->dm_mface + face_index, tf, ibuf);
			}
			node = node->next;
		} while (node);
	}
}

/* We want to know if a bucket and a face overlap in screenspace
 * 
 * Note, if this ever returns false positives its not that bad, since a face in the bounding area will have its pixels
 * calculated when it might not be needed later, (at the moment at least)
 * obviously it shouldnt have bugs though */

static int project_bucket_face_isect(ProjectPaintState *ps, float min[2], float max[2], int bucket_x, int bucket_y, int bucket_index, MFace *mf)
{
	/* TODO - replace this with a tricker method that uses sideofline for all projectVertCos2D's edges against the closest bucket corner */
	float bucket_bounds[4];
	float p1[2], p2[2], p3[2], p4[2];
	float *v, *v1,*v2,*v3,*v4;
	int i;
	
	project_bucket_bounds(ps, bucket_x, bucket_y, bucket_bounds);
	
	/* Is one of the faces verts in the bucket bounds? */
	
	i = mf->v4 ? 3:2;
	do {
		v = ps->projectVertCos2D[ (*(&mf->v1 + i)) ];
		
		if ((v[0] > bucket_bounds[PROJ_BUCKET_LEFT]) &&
			(v[0] < bucket_bounds[PROJ_BUCKET_RIGHT]) &&
			(v[1] > bucket_bounds[PROJ_BUCKET_BOTTOM]) &&
			(v[1] < bucket_bounds[PROJ_BUCKET_TOP]) )
		{
			return 1;
		}
	} while (i--);
		
	v1 = ps->projectVertCos2D[mf->v1];
	v2 = ps->projectVertCos2D[mf->v2];
	v3 = ps->projectVertCos2D[mf->v3];
	if (mf->v4) {
		v4 = ps->projectVertCos2D[mf->v4];
	}
	
	p1[0] = bucket_bounds[PROJ_BUCKET_LEFT];	p1[1] = bucket_bounds[PROJ_BUCKET_BOTTOM];
	p2[0] = bucket_bounds[PROJ_BUCKET_LEFT];	p2[1] = bucket_bounds[PROJ_BUCKET_TOP];
	p3[0] = bucket_bounds[PROJ_BUCKET_RIGHT];	p3[1] = bucket_bounds[PROJ_BUCKET_TOP];
	p4[0] = bucket_bounds[PROJ_BUCKET_RIGHT];	p4[1] = bucket_bounds[PROJ_BUCKET_BOTTOM];
		
	if (mf->v4) {
		if(	IsectPQ2Df(p1, v1, v2, v3, v4) || IsectPQ2Df(p2, v1, v2, v3, v4) || IsectPQ2Df(p3, v1, v2, v3, v4) || IsectPQ2Df(p4, v1, v2, v3, v4) ||
			/* we can avoid testing v3,v1 because another intersection MUST exist if this intersects */
			(IsectLL2Df(p1,p2, v1, v2) || IsectLL2Df(p1,p2, v2, v3) || IsectLL2Df(p1,p2, v3, v4)) ||
			(IsectLL2Df(p2,p3, v1, v2) || IsectLL2Df(p2,p3, v2, v3) || IsectLL2Df(p2,p3, v3, v4)) ||
			(IsectLL2Df(p3,p4, v1, v2) || IsectLL2Df(p3,p4, v2, v3) || IsectLL2Df(p3,p4, v3, v4)) ||
			(IsectLL2Df(p4,p1, v1, v2) || IsectLL2Df(p4,p1, v2, v3) || IsectLL2Df(p4,p1, v3, v4))
		) {
			return 1;
		}
	} else {
		if(	IsectPT2Df(p1, v1, v2, v3) || IsectPT2Df(p2, v1, v2, v3) || IsectPT2Df(p3, v1, v2, v3) || IsectPT2Df(p4, v1, v2, v3) ||
			/* we can avoid testing v3,v1 because another intersection MUST exist if this intersects */
			(IsectLL2Df(p1,p2, v1, v2) || IsectLL2Df(p1,p2, v2, v3)) ||
			(IsectLL2Df(p2,p3, v1, v2) || IsectLL2Df(p2,p3, v2, v3)) ||
			(IsectLL2Df(p3,p4, v1, v2) || IsectLL2Df(p3,p4, v2, v3)) ||
			(IsectLL2Df(p4,p1, v1, v2) || IsectLL2Df(p4,p1, v2, v3))
		) {
			return 1;
		}
	}

	return 0;
}

static void project_paint_begin_face_delayed_init(ProjectPaintState *ps, MFace *mf, MTFace *tf, int face_index)
{
	float min[2], max[2];
	int bucket_min[2], bucket_max[2]; /* for  ps->projectBuckets indexing */
	int i, bucket_x, bucket_y, bucket_index;

	INIT_MINMAX2(min,max);
	
	i = mf->v4 ? 3:2;
	do {
		DO_MINMAX2(ps->projectVertCos2D[ (*(&mf->v1 + i)) ], min, max);
	} while (i--);
	
	project_paint_rect(ps, min, max, bucket_min, bucket_max);
	
	for (bucket_y = bucket_min[1]; bucket_y < bucket_max[1]; bucket_y++) {
		for (bucket_x = bucket_min[0]; bucket_x < bucket_max[0]; bucket_x++) {
			
			bucket_index = bucket_x + (bucket_y * ps->bucketsX);
			
			if (project_bucket_face_isect(ps, min, max, bucket_x, bucket_y, bucket_index, mf)) {
				BLI_linklist_prepend_arena(
					&ps->projectFaces[ bucket_index ],
					(void *)face_index, /* cast to a pointer to shut up the compiler */
					ps->projectArena
				);
			}
		}
	}
}
#endif

static void project_paint_begin( ImagePaintState *s, ProjectPaintState *ps )
{	
	/* Viewport vars */
	float mat[3][3];
	float f_no[3];
	
	float projCo[4];
	float (*projCo2D)[2];
	
	/* Image Vars - keep track of images we have used */
	LinkNode *image_LinkList = NULL;
	LinkNode *node;
	
	Image *tpage_last = NULL;
	ImBuf *ibuf = NULL;
	
	/* Face vars */
	MFace *mf;
	MTFace *tf;
	
	int a, i; /* generic looping vars */
	
	/* raytrace for occlusion */
	float min[3], max[3];
	
	/* memory sized to add to arena size */
	int tot_bucketMem=0;
	int tot_faceFlagMem=0;
	int tot_faceListMem=0;
	int tot_bucketFlagMem=0;

	/* ---- end defines ---- */
	
	/* paint onto the derived mesh
	 * note get_viewedit_datamask checks for paint mode and will always give UVs */
	ps->dm = mesh_get_derived_final(s->ob, get_viewedit_datamask());
	
	ps->dm_mvert = ps->dm->getVertArray( ps->dm );
	ps->dm_mface = ps->dm->getFaceArray( ps->dm );
	ps->dm_mtface= ps->dm->getFaceDataArray( ps->dm, CD_MTFACE );
	
	ps->dm_totvert = ps->dm->getNumVerts( ps->dm );
	ps->dm_totface = ps->dm->getNumFaces( ps->dm );
	
	printf("\n\nstarting\n");
	
	ps->bucketsX = PROJ_BUCKET_DIV;
	ps->bucketsY = PROJ_BUCKET_DIV;
	
	ps->image_index = -1;
	
	ps->viewPoint[0] = ps->viewPoint[1] = ps->viewPoint[2] = 0.0;
	
	ps->viewDir[0] = 0.0;
	ps->viewDir[1] = 0.0;
	ps->viewDir[2] = 1.0;
	
	view3d_get_object_project_mat(curarea, s->ob, ps->projectMat, ps->viewMat);
	
	printmatrix4( "ps->projectMat",  ps->projectMat);

	
	
	tot_bucketMem =		sizeof(LinkNode *) * ps->bucketsX * ps->bucketsY;
#ifdef PROJ_LAZY_INIT
	tot_faceListMem =	sizeof(LinkNode *) * ps->bucketsX * ps->bucketsY;
	tot_faceFlagMem =	sizeof(char) * ps->dm_totface;
	tot_bucketFlagMem =	sizeof(char) * ps->bucketsX * ps->bucketsY;
	
#endif
	
	ps->projectArena = BLI_memarena_new(tot_bucketMem + tot_faceListMem + tot_faceFlagMem + (1<<16) );
	
	ps->projectBuckets = (LinkNode **)BLI_memarena_alloc( ps->projectArena, tot_bucketMem);
#ifdef PROJ_LAZY_INIT
	ps->projectFaces= (LinkNode **)BLI_memarena_alloc( ps->projectArena, tot_faceListMem);
	ps->projectFaceFlags = (char *)BLI_memarena_alloc( ps->projectArena, tot_faceFlagMem);
	ps->projectBucketFlags= (char *)BLI_memarena_alloc( ps->projectArena, tot_bucketFlagMem);
#endif

	memset(ps->projectBuckets,		0, tot_bucketMem);
#ifdef PROJ_LAZY_INIT
	memset(ps->projectFaces,		0, tot_faceListMem);
	memset(ps->projectFaceFlags,	0, tot_faceFlagMem);
	memset(ps->projectBucketFlags,	0, tot_bucketFlagMem);
#endif
	
	/* view raytrace stuff */
	mvert_static = ps->dm_mvert;
	
	memset(&ps->isec, 0, sizeof(ps->isec)); /* Initialize ray intersection */
	ps->isec.mode= RE_RAY_SHADOW;
	ps->isec.lay= -1;
	
	Mat4MulVecfl(G.vd->viewinv, ps->viewPoint);
	
	
	
	VECCOPY(G.scene->cursor, ps->viewPoint);
	
	if (G.vd->persp==V3D_ORTHO) { // Use the cem location in this case TODO
		ps->viewDir[2] = 10000.0; /* ortho view needs a far off point */
	/* Even Though this dosnt change, we cant set it here because blenders internal raytest changes the value each time */
	//} else { /* Watch it, same endpoint for all perspective ray casts  */
	//	VECCOPY(isec.end, viewPoint);
	}

	printmatrix4( "G.vd->viewinv",  G.vd->viewinv);
	
	Mat4Invert(s->ob->imat, s->ob->obmat);
	
	Mat3CpyMat4(mat, G.vd->viewinv);
	Mat3MulVecfl(mat, ps->viewDir);
	Mat3CpyMat4(mat, s->ob->imat);
	Mat3MulVecfl(mat, ps->viewDir);
	
	printmatrix3( "mat",  mat);
	
	/* move the viewport center into object space */
	Mat4MulVec4fl(s->ob->imat, ps->viewPoint);
	
	printvecf("ps->viewDir", ps->viewDir);
	printvecf("ps->viewPoint", ps->viewPoint);
	
	printmatrix4( "s->ob->imat",  s->ob->imat);
	
	/* calculate vert screen coords */
	ps->projectVertCos2D = BLI_memarena_alloc( ps->projectArena, sizeof(float) * ps->dm_totvert * 2);
	projCo2D = ps->projectVertCos2D;
	
	INIT_MINMAX(min, max);
	INIT_MINMAX2(ps->viewMin2D, ps->viewMax2D);
	
	for(a=0; a < ps->dm_totvert; a++, projCo2D++) {
		VECCOPY(projCo, ps->dm_mvert[a].co);		
		
		/* ray-tree needs worldspace min/max, do here and save another loop */
		if (ps->projectOcclude) {
			DO_MINMAX(projCo, min, max);
		}
		
		projCo[3] = 1.0;
		Mat4MulVec4fl(ps->projectMat, projCo);
		
		if( projCo[3] > 0.001 ) {
			/* screen space, not clamped */
			(*projCo2D)[0] = (float)(curarea->winx/2.0)+(curarea->winx/2.0)*projCo[0]/projCo[3];	
			(*projCo2D)[1] = (float)(curarea->winy/2.0)+(curarea->winy/2.0)*projCo[1]/projCo[3]; /* Maybe the Z value is useful too.. but not yet */
			DO_MINMAX2((*projCo2D), ps->viewMin2D, ps->viewMax2D);
		} else {
			(*projCo2D)[0] = (*projCo2D)[1] = MAXFLOAT;
		}
	}
	
	/* only for convenience */
	ps->viewWidth  = ps->viewMax2D[0] - ps->viewMin2D[0];
	ps->viewHeight = ps->viewMax2D[1] - ps->viewMin2D[1];
	
	/* Build a ray tree so we can invalidate UV pixels that have a face infront of them */
	if (ps->projectOcclude) {
		
		if (G.f & G_FACESELECT) {
			mf = ps->dm_mface;
			a = ps->dm_totface - 1;
			do {
				if (!(mf->flag & ME_HIDE)) {
					i++;
				}
				mf++;
			} while (a--);
			
			ps->projectRayTree =	RE_ray_tree_create(
					64, i, min, max,
					project_paint_begin_coords_func,
					project_paint_begin_check_func,
					NULL, NULL);
			
			mf = ps->dm_mface;
			a = ps->dm_totface - 1;
			
			do {
				if (!(mf->flag & ME_HIDE)) {
					RE_ray_tree_add_face(ps->projectRayTree, 0, mf);
				}
			} while (a--);
			
		} else { 
			ps->projectRayTree =	RE_ray_tree_create(
					64, ps->dm_totface, min, max,
					project_paint_begin_coords_func,
					project_paint_begin_check_func,
					NULL, NULL);
			
			mf = ps->dm_mface;
			a = ps->dm_totface - 1;
			do {
				RE_ray_tree_add_face(ps->projectRayTree, 0, mf);
				mf++;
			} while (a--);
		}
		
		RE_ray_tree_done(ps->projectRayTree);
	}
	
	
	for( a = 0, tf = ps->dm_mtface, mf = ps->dm_mface; a < ps->dm_totface; mf++, tf++, a++ ) {
		if (tf->tpage && ((G.f & G_FACESELECT)==0 || mf->flag & ME_FACE_SEL)) {
			
			if (ps->projectBackfaceCull) {
				/* TODO - we dont really need the normal, just the direction, save a sqrt? */
				if (mf->v4)	CalcNormFloat4(ps->dm_mvert[mf->v1].co, ps->dm_mvert[mf->v2].co, ps->dm_mvert[mf->v3].co, ps->dm_mvert[mf->v4].co, f_no);
				else		CalcNormFloat(ps->dm_mvert[mf->v1].co, ps->dm_mvert[mf->v2].co, ps->dm_mvert[mf->v3].co, f_no);
				
				if (Inpf(f_no, ps->viewDir) < 0) {
					continue;
				}
			}
			
			if (tpage_last != tf->tpage) {
				ibuf= BKE_image_get_ibuf((Image *)tf->tpage, NULL);
				
				if (ibuf) {
					/* TODO - replace this with not yet existant BLI_linklist_index function */
					for	(
						node=image_LinkList, ps->image_index=0;
						node && node->link != tf->tpage ;
						node = node->next, ps->image_index++
					) {}
					
					if (node==NULL) { /* MemArena dosnt have an append func */
						BLI_linklist_append(&image_LinkList, tf->tpage);
						ps->projectImageTotal = ps->image_index+1;
					}
				}
				
				tpage_last = tf->tpage;
			}
			
			if (ibuf) {
				/* Initialize the faces screen pixels */
#ifdef PROJ_LAZY_INIT
				/* Add this to a list to initialize later */
				project_paint_begin_face_delayed_init(ps, mf, tf, a);
#else
				project_paint_face_init(ps, mf, tf, ibuf);
#endif
			}
		}
	}
	
	/* build an array of images we use*/
	ps->projectImages = BLI_memarena_alloc( ps->projectArena, sizeof(Image *) * ps->projectImageTotal);
	
	for (node= image_LinkList, i=0; node; node= node->next, i++) {
		
		ps->projectImages[i] = node->link;
		ps->projectImages[i]->id.flag &= ~LIB_DOIT;
		// printf("'%s' %d\n", ps->projectImages[i]->id.name+2, i);
	}
	/* we have built the array, discard the linked list */
	BLI_linklist_free(image_LinkList, NULL);
}

static void project_paint_end( ProjectPaintState *ps )
{
	BLI_memarena_free(ps->projectArena);
	if (ps->projectOcclude)
		RE_ray_tree_free(ps->projectRayTree);
	
	ps->dm->release(ps->dm);
}


/* external functions */

/* 1= an undo, -1 is a redo. */
void undo_imagepaint_step(int step)
{
	UndoElem *undo;

	if(step==1) {
		if(curundo==NULL) error("No more steps to undo");
		else {
			if(G.f & G_DEBUG) printf("undo %s\n", curundo->name);
			undo_restore(curundo);
			curundo= curundo->prev;
		}
	}
	else if(step==-1) {
		if((curundo!=NULL && curundo->next==NULL) || undobase.first==NULL) error("No more steps to redo");
		else {
			undo= (curundo && curundo->next)? curundo->next: undobase.first;
			undo_restore(undo);
			curundo= undo;
			if(G.f & G_DEBUG) printf("redo %s\n", undo->name);
		}
	}

	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWIMAGE, 0);
}

void undo_imagepaint_clear(void)
{
	UndoElem *uel;
	
	uel= undobase.first;
	while(uel) {
		undo_free(uel);
		uel= uel->next;
	}

	BLI_freelistN(&undobase);
	curundo= NULL;
}

/* Imagepaint Partial Redraw & Dirty Region */

static void imapaint_clear_partial_redraw()
{
	memset(&imapaintpartial, 0, sizeof(imapaintpartial));
}

static void imapaint_dirty_region(Image *ima, ImBuf *ibuf, int x, int y, int w, int h)
{
	ImBuf *tmpibuf;
	UndoTile *tile;
	int srcx= 0, srcy= 0, origx, allocsize;

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

	tmpibuf= IMB_allocImBuf(IMAPAINT_TILE_SIZE, IMAPAINT_TILE_SIZE, 32,
	                        IB_rectfloat|IB_rect, 0);
	
	for (; y <= h; y++) {
		for (x=origx; x <= w; x++) {
			for(tile=curundo->tiles.first; tile; tile=tile->next)
				if(tile->x == x && tile->y == y && strcmp(tile->id.name, ima->id.name)==0)
					break;

			if(!tile) {
				tile= MEM_callocN(sizeof(UndoTile), "ImaUndoTile");
				tile->id= ima->id;
				tile->x= x;
				tile->y= y;

				allocsize= IMAPAINT_TILE_SIZE*IMAPAINT_TILE_SIZE*4;
				allocsize *= (ibuf->rect_float)? sizeof(float): sizeof(char);
				tile->rect= MEM_mapallocN(allocsize, "ImaUndoRect");

				undo_copy_tile(tile, tmpibuf, ibuf, 0);
				curundo->undosize += allocsize;

				BLI_addtail(&curundo->tiles, tile);
			}
		}
	}

	ibuf->userflags |= IB_BITMAPDIRTY;

	IMB_freeImBuf(tmpibuf);
}

static void imapaint_image_update(Image *image, ImBuf *ibuf, short texpaint)
{
	if(ibuf->rect_float)
		imb_freerectImBuf(ibuf); /* force recreate of char rect */
	if(ibuf->mipmap[0])
		imb_freemipmapImBuf(ibuf);

	/* todo: should set_tpage create ->rect? */
	if(texpaint || G.sima->lock) {
		int w = imapaintpartial.x2 - imapaintpartial.x1;
		int h = imapaintpartial.y2 - imapaintpartial.y1;
		GPU_paint_update_image(image, imapaintpartial.x1, imapaintpartial.y1, w, h);
	}
}

/* note; gets called for both 2d image paint and 3d texture paint. in the
   latter case image may be NULL and G.sima may not exist */
static void imapaint_redraw(int final, int texpaint, Image *image)
{
	if(final) {
		if(texpaint)
			allqueue(REDRAWIMAGE, 0);
		else if(!G.sima->lock) {
			if(image)
				GPU_free_image(image); /* force OpenGL reload */
			allqueue(REDRAWVIEW3D, 0);
		}
		allqueue(REDRAWHEADERS, 0);
		
		if(!texpaint && image) {
			/* after paint, tag Image or RenderResult nodes changed */
			if(G.scene->nodetree) {
				imagepaint_composite_tags(G.scene->nodetree, image, &G.sima->iuser);
			}
			/* signal composite (hurmf, need an allqueue?) */
			if(G.sima->lock) {
				ScrArea *sa;
				for(sa=G.curscreen->areabase.first; sa; sa= sa->next) {
					if(sa->spacetype==SPACE_NODE) {
						if(((SpaceNode *)sa->spacedata.first)->treetype==NTREE_COMPOSIT) {
							addqueue(sa->win, UI_BUT_EVENT, B_NODE_TREE_EXEC);
							break;
						}
					}
				}
			}
		}		
	}
	else if(!texpaint && G.sima->lock)
		force_draw_plus(SPACE_VIEW3D, 0);
	else
		force_draw(0);
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

		if (set) IMAPAINT_FLOAT_RGB_COPY(rrgbf, rgb)
		else IMAPAINT_FLOAT_RGB_COPY(rgb, rrgbf)
	}
	else {
		char *rrgb = (char*)ibuf->rect + (ibuf->x*y + x)*4;

		if (set) IMAPAINT_FLOAT_RGB_TO_CHAR(rrgb, rgb)
		else IMAPAINT_CHAR_RGB_TO_FLOAT(rgb, rrgb)
	}
}

static int imapaint_ibuf_add_if(ImBuf *ibuf, unsigned int x, unsigned int y, float *outrgb, short torus)
{
	float inrgb[3];

	if ((x >= ibuf->x) || (y >= ibuf->y)) {
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

static void imapaint_lift_smear(ImBuf *ibuf, ImBuf *ibufb, int *pos)
{
	IMB_rectblend_torus(ibufb, ibuf, 0, 0, pos[0], pos[1],
		ibufb->x, ibufb->y, IMB_BLEND_COPY_RGB);
}

static ImBuf *imapaint_lift_clone(ImBuf *ibuf, ImBuf *ibufb, int *pos)
{
	/* note: allocImbuf returns zero'd memory, so regions outside image will
	   have zero alpha, and hence not be blended onto the image */
	int w=ibufb->x, h=ibufb->y, destx=0, desty=0, srcx=pos[0], srcy=pos[1];
	ImBuf *clonebuf= IMB_allocImBuf(w, h, ibufb->depth, ibufb->flags, 0);

	IMB_rectclip(clonebuf, ibuf, &destx, &desty, &srcx, &srcy, &w, &h);
	IMB_rectblend(clonebuf, ibuf, destx, desty, srcx, srcy, w, h,
		IMB_BLEND_COPY_RGB);
	IMB_rectblend(clonebuf, ibufb, destx, desty, destx, desty, w, h,
		IMB_BLEND_COPY_ALPHA);

	return clonebuf;
}

static void imapaint_convert_brushco(ImBuf *ibufb, float *pos, int *ipos)
{
	ipos[0]= (int)(pos[0] - ibufb->x/2);
	ipos[1]= (int)(pos[1] - ibufb->y/2);
}

/* dosnt run for projection painting
 * only the old style painting in the 3d view */
static int imapaint_paint_op(void *state, ImBuf *ibufb, float *lastpos, float *pos)
{
	ImagePaintState *s= ((ImagePaintState*)state);
	ImBuf *clonebuf= NULL;
	short torus= s->brush->flag & BRUSH_TORUS;
	short blend= s->blend;
	float *offset= s->brush->clone.offset;
	float liftpos[2];
	int bpos[2], blastpos[2], bliftpos[2];

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

	imapaint_dirty_region(s->image, s->canvas, bpos[0], bpos[1], ibufb->x, ibufb->y);

	/* blend into canvas */
	if(torus)
		IMB_rectblend_torus(s->canvas, (clonebuf)? clonebuf: ibufb,
			bpos[0], bpos[1], 0, 0, ibufb->x, ibufb->y, blend);
	else
		IMB_rectblend(s->canvas, (clonebuf)? clonebuf: ibufb,
			bpos[0], bpos[1], 0, 0, ibufb->x, ibufb->y, blend);
			
	if(clonebuf) IMB_freeImBuf(clonebuf);

	return 1;
}

/* 2D ImagePaint */

static void imapaint_compute_uvco(short *mval, float *uv)
{
	areamouseco_to_ipoco(G.v2d, mval, &uv[0], &uv[1]);
}

/* 3D TexturePaint */

int facesel_face_pick(Mesh *me, short *mval, unsigned int *index, short rect);
void texpaint_pick_uv(Object *ob, Mesh *mesh, unsigned int faceindex, short *xy, float *mousepos);

static int texpaint_break_stroke(float *prevuv, float *fwuv, float *bkuv, float *uv)
{
	float d1[2], d2[2];
	float mismatch = Vec2Lenf(fwuv, uv);
	float len1 = Vec2Lenf(prevuv, fwuv);
	float len2 = Vec2Lenf(bkuv, uv);

	Vec2Subf(d1, fwuv, prevuv);
	Vec2Subf(d2, uv, bkuv);

	return ((Inp2f(d1, d2) < 0.0f) || (mismatch > MAX2(len1, len2)*2));
}

/* ImagePaint Common */

static int imapaint_canvas_set(ImagePaintState *s, Image *ima)
{
	ImBuf *ibuf= BKE_image_get_ibuf(ima, G.sima?&G.sima->iuser:NULL);
	
	/* verify that we can paint and set canvas */
	if(ima->packedfile && ima->rr) {
		s->warnpackedfile = ima->id.name + 2;
		return 0;
	}	
	else if(ibuf && ibuf->channels!=4) {
		s->warnmultifile = ima->id.name + 2;
		return 0;
	}
	else if(!ima || !ibuf || !(ibuf->rect || ibuf->rect_float))
		return 0;

	s->image= ima;
	s->canvas= ibuf;

	/* set clone canvas */
	if(s->tool == PAINT_TOOL_CLONE) {
		ima= s->brush->clone.image;
		ibuf= BKE_image_get_ibuf(ima, G.sima?&G.sima->iuser:NULL);
		
		if(!ima || !ibuf || !(ibuf->rect || ibuf->rect_float))
			return 0;

		s->clonecanvas= ibuf;

		if(s->canvas->rect_float && !s->clonecanvas->rect_float) {
			/* temporarily add float rect for cloning */
			IMB_float_from_rect(s->clonecanvas);
			s->clonefreefloat= 1;
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
	ImBuf *ibuf= BKE_image_get_ibuf(image, G.sima?&G.sima->iuser:NULL);
	float pos[2];

	if(!ibuf)
		return 0;

	pos[0] = uv[0]*ibuf->x;
	pos[1] = uv[1]*ibuf->y;

	brush_painter_require_imbuf(painter, ((ibuf->rect_float)? 1: 0), 0, 0);

	if (brush_painter_paint(painter, imapaint_paint_op, pos, time, pressure, s)) {
		if (update)
			imapaint_image_update(image, ibuf, texpaint);
		return 1;
	}
	else return 0;
}

static float Vec2Lenf_nosqrt(float *v1, float *v2)
{
	float x, y;

	x = v1[0]-v2[0];
	y = v1[1]-v2[1];
	return x*x+y*y;
}

static float Vec2Lenf_nosqrt_other(float *v1, float v2_1, float v2_2)
{
	float x, y;

	x = v1[0]-v2_1;
	y = v1[1]-v2_2;
	return x*x+y*y;
}

/* note, use a squared value so we can use Vec2Lenf_nosqrt
 * be sure that you have done a bounds check first or this may fail */
static int project_bucket_circle_isect(ProjectPaintState *ps, int bucket_x, int bucket_y, float cent[2], float radius_squared)
{
	float bucket_bounds[4];
	//return 1;
	
	project_bucket_bounds(ps, bucket_x, bucket_y, bucket_bounds);
	
	// printf("%d %d - %f %f %f %f - %f %f \n", bucket_x, bucket_y, bucket_bounds[0], bucket_bounds[1], bucket_bounds[2], bucket_bounds[3], cent[0], cent[1]);

	/* first check if we are INSIDE the bucket */
	/* if (	bucket_bounds[PROJ_BUCKET_LEFT] <=	cent[0] &&
			bucket_bounds[PROJ_BUCKET_RIGHT] >=	cent[0] &&
			bucket_bounds[PROJ_BUCKET_BOTTOM] <=	cent[1] &&
			bucket_bounds[PROJ_BUCKET_TOP] >=	cent[1]	)
	{
		return 1;
	}*/
	
	/* Would normally to a simple intersection test, however we know the bounds of these 2 alredy intersect 
	 * so we only need to test if the center is inside the vertical or horizontal bounds on either axis,
	 * this is even less work then an intersection test */
	if (  (	bucket_bounds[PROJ_BUCKET_LEFT] <=		cent[0] &&
			bucket_bounds[PROJ_BUCKET_RIGHT] >=		cent[0]) ||
		  (	bucket_bounds[PROJ_BUCKET_BOTTOM] <=	cent[1] &&
			bucket_bounds[PROJ_BUCKET_TOP] >=		cent[1]) )
	{
		return 1;
	}
	
	/* out of bounds left */
	if (cent[0] < bucket_bounds[PROJ_BUCKET_LEFT]) {
		/* lower left out of radius test */
		if (cent[1] < bucket_bounds[PROJ_BUCKET_BOTTOM]) {
			return (Vec2Lenf_nosqrt_other(cent, bucket_bounds[PROJ_BUCKET_LEFT], bucket_bounds[PROJ_BUCKET_BOTTOM]) < radius_squared) ? 1 : 0;
		} 
		/* top left test */
		else if (cent[1] > bucket_bounds[PROJ_BUCKET_TOP]) {
			return (Vec2Lenf_nosqrt_other(cent, bucket_bounds[PROJ_BUCKET_LEFT], bucket_bounds[PROJ_BUCKET_TOP]) < radius_squared) ? 1 : 0;
		}
	}
	else if (cent[0] > bucket_bounds[PROJ_BUCKET_RIGHT]) {
		/* lower right out of radius test */
		if (cent[1] < bucket_bounds[PROJ_BUCKET_BOTTOM]) {
			return (Vec2Lenf_nosqrt_other(cent, bucket_bounds[PROJ_BUCKET_RIGHT], bucket_bounds[PROJ_BUCKET_BOTTOM]) < radius_squared) ? 1 : 0;
		} 
		/* top right test */
		else if (cent[1] > bucket_bounds[PROJ_BUCKET_TOP]) {
			return (Vec2Lenf_nosqrt_other(cent, bucket_bounds[PROJ_BUCKET_RIGHT], bucket_bounds[PROJ_BUCKET_TOP]) < radius_squared) ? 1 : 0;
		}
	}
	
	return 0;
}

static int imapaint_paint_sub_stroke_project(ImagePaintState *s, ProjectPaintState *ps, BrushPainter *painter, short texpaint, short mval[2], double time, int update, float pressure)
{
	/* TODO - texpaint option : is there any use in projection painting from the image window??? - could be interesting */
	/* TODO - floating point images */
	//bucketWidth = ps->viewWidth/ps->bucketsX;
	//bucketHeight = ps->viewHeight/ps->bucketsY;

	LinkNode *node;
	float mval_f[2];
	ImagePaintProjectPixel *projPixel;
	int redraw = 0;
	int last_index = -1;
	float rgba[4], alpha, dist, dist_nosqrt;
	float brush_size_sqared;
	float min[2], max[2]; /* brush bounds in screenspace */
	int bucket_min[2], bucket_max[2]; /* brush bounds in bucket grid space */
	int bucket_index;
	int a;
	
	int bucket_x, bucket_y;
	
	mval_f[0] = mval[0]; mval_f[1] = mval[1]; 
	
	min[0] = mval_f[0] - (s->brush->size/2);
	min[1] = mval_f[1] - (s->brush->size/2);
	
	max[0] = mval_f[0] + (s->brush->size/2);
	max[1] = mval_f[1] + (s->brush->size/2);
	
	/* offset to make this a valid bucket index */
	project_paint_rect(ps, min, max, bucket_min, bucket_max);
	
	/* mouse outside the model areas? */
	if (bucket_min[0]==bucket_max[0] || bucket_min[1]==bucket_max[1]) {
		return 0;
	}
	
	/* avoid a square root with every dist comparison */
	brush_size_sqared = s->brush->size * s->brush->size; 
	
	/* printf("brush bounds %d %d %d %d\n", bucket_min[0], bucket_min[1], bucket_max[0], bucket_max[1]); */
	
	/* If there is ever problems with getting the bounds for the brush, set the bounds to include all */
	/*bucket_min[0] = 0; bucket_min[1] = 0; bucket_max[0] = ps->bucketsX; bucket_max[1] = ps->bucketsY;*/
	
	/* no clamping needed, dont use screen bounds, use vert bounds  */
	
	for (bucket_y = bucket_min[1]; bucket_y < bucket_max[1]; bucket_y++) {
		for (bucket_x = bucket_min[0]; bucket_x < bucket_max[0]; bucket_x++) {
			
			if (project_bucket_circle_isect(ps, bucket_x, bucket_y, mval_f, brush_size_sqared)) {
				
				bucket_index = bucket_x + (bucket_y * ps->bucketsX);

	#ifdef PROJ_LAZY_INIT
				if (ps->projectBucketFlags[bucket_index] == PROJ_BUCKET_NULL) {
					/* This bucket may hold some uninitialized faces, initialize it */
					project_paint_bucket_init(ps, bucket_index);
				}
	#endif

				if ((node = ps->projectBuckets[bucket_index])) {
				
					do {
						projPixel = (ImagePaintProjectPixel *)node->link;
#ifdef PROJ_PAINT_DEBUG
						projPixel->pixel[0] = 0; // use for checking if the starting radius is too big
#endif
						
						/*dist = Vec2Lenf(projPixel->projCo2D, mval_f);*/ /* correct but uses a sqrt */
						dist_nosqrt = Vec2Lenf_nosqrt(projPixel->projCo2D, mval_f);
						
						/*if (dist < s->brush->size) {*/ /* correct but uses a sqrt */
						if (dist_nosqrt < brush_size_sqared) {
						
							brush_sample_tex(s->brush, projPixel->projCo2D, rgba);
							
							dist = (float)sqrt(dist_nosqrt);
							
							alpha = rgba[3]*brush_sample_falloff(s->brush, dist);
							
							if (alpha <= 0.0) {
								/* do nothing */
							} else {
								if (alpha >= 1.0) {
									projPixel->pixel[0] = FTOCHAR(rgba[0]*s->brush->rgb[0]);
									projPixel->pixel[1] = FTOCHAR(rgba[1]*s->brush->rgb[1]);
									projPixel->pixel[2] = FTOCHAR(rgba[2]*s->brush->rgb[2]);
								} else {
									projPixel->pixel[0] = FTOCHAR(((rgba[0]*s->brush->rgb[0])*alpha) + (((projPixel->pixel[0])/255.0)*(1.0-alpha)));
									projPixel->pixel[1] = FTOCHAR(((rgba[1]*s->brush->rgb[1])*alpha) + (((projPixel->pixel[1])/255.0)*(1.0-alpha)));
									projPixel->pixel[2] = FTOCHAR(((rgba[2]*s->brush->rgb[2])*alpha) + (((projPixel->pixel[2])/255.0)*(1.0-alpha)));
								}
							} 
							
							if (last_index != projPixel->image_index) {
								last_index = projPixel->image_index;
								ps->projectImages[last_index]->id.flag |= LIB_DOIT;
							}
							
						}
						
						node = node->next;
					} while (node);
				}
			}
		}
	}

	/* Loop over all images on this mesh and update any we have touched */
	for (a=0; a < ps->projectImageTotal; a++) {
		Image *ima = ps->projectImages[a];
		if (ima->id.flag & LIB_DOIT) {
			imapaint_image_update(ima, BKE_image_get_ibuf(ima, NULL), texpaint);
			redraw = 1;
			
			ima->id.flag &= ~LIB_DOIT; /* clear for reuse */
		}
	}
	
	return redraw;
}


static void imapaint_paint_stroke(ImagePaintState *s, BrushPainter *painter, short texpaint, short *prevmval, short *mval, double time, float pressure)
{
	Image *newimage = NULL;
	float fwuv[2], bkuv[2], newuv[2];
	unsigned int newfaceindex;
	int breakstroke = 0, redraw = 0;

	if (texpaint) {
		/* pick new face and image */
		if (	facesel_face_pick(s->me, mval, &newfaceindex, 0) &&
				((G.f & G_FACESELECT)==0 || (s->me->mface+newfaceindex)->flag & ME_FACE_SEL)
		) {
			ImBuf *ibuf;
			
			newimage = (Image*)((s->me->mtface+newfaceindex)->tpage);
			ibuf= BKE_image_get_ibuf(newimage, G.sima?&G.sima->iuser:NULL);

			if(ibuf && ibuf->rect)
				texpaint_pick_uv(s->ob, s->me, newfaceindex, mval, newuv);
			else {
				newimage = NULL;
				newuv[0] = newuv[1] = 0.0f;
			}
		}
		else
			newuv[0] = newuv[1] = 0.0f;

		/* see if stroke is broken, and if so finish painting in old position */
		if (s->image) {
			texpaint_pick_uv(s->ob, s->me, s->faceindex, mval, fwuv);
			texpaint_pick_uv(s->ob, s->me, newfaceindex, prevmval, bkuv);

			if (newimage == s->image)
				breakstroke= texpaint_break_stroke(s->uv, fwuv, bkuv, newuv);
			else
				breakstroke= 1;
		}
		else
			fwuv[0]= fwuv[1]= 0.0f;

		if (breakstroke) {
			texpaint_pick_uv(s->ob, s->me, s->faceindex, mval, fwuv);
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
		imapaint_compute_uvco(mval, newuv);
		redraw |= imapaint_paint_sub_stroke(s, painter, s->image, texpaint, newuv,
			time, 1, pressure);
	}

	if (redraw) {
		imapaint_redraw(0, texpaint, NULL);
		imapaint_clear_partial_redraw();
	}
}

static void imapaint_paint_stroke_project(ImagePaintState *s, ProjectPaintState *ps, BrushPainter *painter, short texpaint, short *prevmval, short *mval, double time, float pressure)
{
	int redraw = 0;
	
	/* TODO - support more brush operations, airbrush etc */
	{
		redraw |= imapaint_paint_sub_stroke_project(s, ps, painter, texpaint, mval, time, 1, pressure);
	}
	
	if (redraw) {
		imapaint_redraw(0, texpaint, NULL);
		imapaint_clear_partial_redraw();
	}
}

void imagepaint_paint(short mousebutton, short texpaint)
{
	ImagePaintState s;
	ProjectPaintState ps;
	BrushPainter *painter;
	ToolSettings *settings= G.scene->toolsettings;
	short prevmval[2], mval[2];
	double time;
	float pressure;

	if(!settings->imapaint.brush)
		return;

	/* initialize state */
	memset(&s, 0, sizeof(s));
	s.brush = settings->imapaint.brush;
	s.tool = settings->imapaint.tool;
	if(texpaint && (s.tool == PAINT_TOOL_CLONE))
		s.tool = PAINT_TOOL_DRAW;
	s.blend = s.brush->blend;

	if (texpaint) /* TODO - make an option */
		s.project = 1;
	
	if(texpaint) {
		s.ob = OBACT;
		if (!s.ob || !(s.ob->lay & G.vd->lay)) return;
		s.me = get_mesh(s.ob);
		if (!s.me) return;

		persp(PERSP_VIEW);
	}
	else {
		s.image = G.sima->image;

		if(!imapaint_canvas_set(&s, G.sima->image)) {
			if(s.warnmultifile)
				error("Image requires 4 color channels to paint");
			if(s.warnpackedfile)
				error("Packed MultiLayer files cannot be painted");
			return;
		}
	}

	settings->imapaint.flag |= IMAGEPAINT_DRAWING;
	undo_imagepaint_push_begin("Image Paint");

	/* create painter and paint once */
	painter= brush_painter_new(s.brush);

	getmouseco_areawin(mval);

	pressure = get_pressure();
	s.blend = (get_activedevice() == 2)? BRUSH_BLEND_ERASE_ALPHA: s.brush->blend;
	
	time= PIL_check_seconds_timer();
	prevmval[0]= mval[0];
	prevmval[1]= mval[1];
	
	if (s.project) {
		/* setup projection painting data */
		memset(&ps, 0, sizeof(ps));
		
		ps.projectBackfaceCull = 1;
		ps.projectOcclude = 1;
		
		project_paint_begin(&s, &ps);
		
	} else {
		if (!((s.brush->flag & (BRUSH_ALPHA_PRESSURE|BRUSH_SIZE_PRESSURE|
			BRUSH_SPACING_PRESSURE|BRUSH_RAD_PRESSURE)) && (get_activedevice() != 0) && (pressure >= 0.99f)))
			imapaint_paint_stroke(&s, painter, texpaint, prevmval, mval, time, pressure);
	}
	/* paint loop */
	do {
		getmouseco_areawin(mval);

		pressure = get_pressure();
		s.blend = (get_activedevice() == 2)? BRUSH_BLEND_ERASE_ALPHA: s.brush->blend;
			
		time= PIL_check_seconds_timer();

		if (s.project) { /* Projection Painting */
			if((mval[0] != prevmval[0]) || (mval[1] != prevmval[1])) {
				imapaint_paint_stroke_project(&s, &ps, painter, 1, prevmval, mval, time, pressure);
				prevmval[0]= mval[0];
				prevmval[1]= mval[1];
			} else
				BIF_wait_for_statechange();
		} else {
			if((mval[0] != prevmval[0]) || (mval[1] != prevmval[1])) {
				imapaint_paint_stroke(&s, painter, texpaint, prevmval, mval, time, pressure);
				prevmval[0]= mval[0];
				prevmval[1]= mval[1];
			}
			else if (s.brush->flag & BRUSH_AIRBRUSH)
				imapaint_paint_stroke(&s, painter, texpaint, prevmval, mval, time, pressure);
			else
				BIF_wait_for_statechange();
		}
		/* do mouse checking at the end, so don't check twice, and potentially
		   miss a short tap */
	} while(get_mbut() & mousebutton);

	/* clean up */
	settings->imapaint.flag &= ~IMAGEPAINT_DRAWING;
	imapaint_canvas_free(&s);
	brush_painter_free(painter);

	if (s.project) {
		project_paint_end(&ps);
	}
	
	imapaint_redraw(1, texpaint, s.image);
	undo_imagepaint_push_end();
	
	if (texpaint) {
		if (s.warnmultifile)
			error("Image requires 4 color channels to paint: %s", s.warnmultifile);
		if(s.warnpackedfile)
			error("Packed MultiLayer files cannot be painted %s", s.warnpackedfile);

		persp(PERSP_WIN);
	}
}

void imagepaint_pick(short mousebutton)
{
	ToolSettings *settings= G.scene->toolsettings;
	Brush *brush= settings->imapaint.brush;

	if(brush && (settings->imapaint.tool == PAINT_TOOL_CLONE)) {
		if(brush->clone.image) {
			short prevmval[2], mval[2];
			float lastmousepos[2], mousepos[2];
		
			getmouseco_areawin(prevmval);

			while(get_mbut() & mousebutton) {
				getmouseco_areawin(mval);

				if((prevmval[0] != mval[0]) || (prevmval[1] != mval[1]) ) {
					/* mouse moved, so move the clone image */
					imapaint_compute_uvco(prevmval, lastmousepos);
					imapaint_compute_uvco(mval, mousepos);

					brush->clone.offset[0] += mousepos[0] - lastmousepos[0];
					brush->clone.offset[1] += mousepos[1] - lastmousepos[1];

					force_draw(0);

					prevmval[0]= mval[0];
					prevmval[1]= mval[1];
				}
			}
		}
	}
	else if(brush)
		sample_vpaint();
}

