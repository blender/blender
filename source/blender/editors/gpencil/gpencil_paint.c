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
 * The Original Code is Copyright (C) 2008, Blender Foundation, Joshua Leung
 * This is a new part of Blender
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/gpencil/gpencil_paint.c
 *  \ingroup edgpencil
 */


#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_gpencil.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_report.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_windowmanager_types.h"

#include "UI_view2d.h"

#include "ED_gpencil.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "RNA_access.h"

#include "RNA_define.h"
#include "WM_api.h"
#include "WM_types.h"

#include "gpencil_intern.h"

/* ******************************************* */
/* 'Globals' and Defines */

/* Temporary 'Stroke' Operation data */
typedef struct tGPsdata {
	Scene *scene;       /* current scene from context */
	
	wmWindow *win;		/* window where painting originated */
	ScrArea *sa;		/* area where painting originated */
	ARegion *ar;        /* region where painting originated */
	View2D *v2d;		/* needed for GP_STROKE_2DSPACE */
	rctf *subrect;		/* for using the camera rect within the 3d view */
	rctf subrect_data;
	
	
#if 0 // XXX review this 2d image stuff...
	ImBuf *ibuf;		/* needed for GP_STROKE_2DIMAGE */
	struct IBufViewSettings {
		int offsx, offsy;			/* offsets */
		int sizex, sizey;			/* dimensions to use as scale-factor */
	} im2d_settings;	/* needed for GP_STROKE_2DIMAGE */
#endif
	
	PointerRNA ownerPtr;/* pointer to owner of gp-datablock */
	bGPdata *gpd;		/* gp-datablock layer comes from */
	bGPDlayer *gpl;		/* layer we're working on */
	bGPDframe *gpf;		/* frame we're working on */
	
	short status;		/* current status of painting */
	short paintmode;	/* mode for painting */
	
	int mval[2];		/* current mouse-position */
	int mvalo[2];		/* previous recorded mouse-position */
	
	float pressure;		/* current stylus pressure */
	float opressure;	/* previous stylus pressure */
	
	short radius;		/* radius of influence for eraser */
	short flags;		/* flags that can get set during runtime */

	float imat[4][4];	/* inverted transformation matrix applying when converting coords from screen-space
						 * to region space */

	float custom_color[4]; /* custom color for (?) */
} tGPsdata;

/* values for tGPsdata->status */
enum {
	GP_STATUS_IDLING = 0,	/* stroke isn't in progress yet */
	GP_STATUS_PAINTING,		/* a stroke is in progress */
	GP_STATUS_ERROR,		/* something wasn't correctly set up */
	GP_STATUS_DONE			/* painting done */
};

/* Return flags for adding points to stroke buffer */
enum {
	GP_STROKEADD_INVALID	= -2,		/* error occurred - insufficient info to do so */
	GP_STROKEADD_OVERFLOW	= -1,		/* error occurred - cannot fit any more points */
	GP_STROKEADD_NORMAL,				/* point was successfully added */
	GP_STROKEADD_FULL					/* cannot add any more points to buffer */
};

/* Runtime flags */
enum {
	GP_PAINTFLAG_FIRSTRUN		= (1<<0),	/* operator just started */
	GP_PAINTFLAG_STROKEADDED	= (1<<1)	/* stroke was already added during draw session */
};

/* ------ */

/* maximum sizes of gp-session buffer */
#define GP_STROKE_BUFFER_MAX	5000

/* Macros for accessing sensitivity thresholds... */
	/* minimum number of pixels mouse should move before new point created */
#define MIN_MANHATTEN_PX	(U.gp_manhattendist)
	/* minimum length of new segment before new point can be added */
#define MIN_EUCLIDEAN_PX	(U.gp_euclideandist)

/* ------ */
/* Forward defines for some functions... */

static void gp_session_validatebuffer(tGPsdata *p);

/* ******************************************* */
/* Context Wrangling... */

/* check if context is suitable for drawing */
static int gpencil_draw_poll (bContext *C)
{
	if (ED_operator_regionactive(C)) {
		/* check if current context can support GPencil data */
		if (gpencil_data_get_pointers(C, NULL) != NULL) {
			/* check if Grease Pencil isn't already running */
			if (ED_gpencil_session_active() == 0)
				return 1;
			else
				CTX_wm_operator_poll_msg_set(C, "Grease Pencil operator is already active");
		}
		else {
			CTX_wm_operator_poll_msg_set(C, "Failed to find Grease Pencil data to draw into");
		}
	}
	else {
		CTX_wm_operator_poll_msg_set(C, "Active region not set");
	}
	
	return 0;
}

/* check if projecting strokes into 3d-geometry in the 3D-View */
static int gpencil_project_check (tGPsdata *p)
{
	bGPdata *gpd= p->gpd;
	return ((gpd->sbuffer_sflag & GP_STROKE_3DSPACE) && (p->gpd->flag & (GP_DATA_DEPTH_VIEW | GP_DATA_DEPTH_STROKE)));
}

/* ******************************************* */
/* Calculations/Conversions */

/* Utilities --------------------------------- */

/* get the reference point for stroke-point conversions */
static void gp_get_3d_reference (tGPsdata *p, float vec[3])
{
	View3D *v3d= p->sa->spacedata.first;
	float *fp= give_cursor(p->scene, v3d);
	
	/* the reference point used depends on the owner... */
#if 0 // XXX: disabled for now, since we can't draw relative to the owner yet
	if (p->ownerPtr.type == &RNA_Object) 
	{
		Object *ob= (Object *)p->ownerPtr.data;
		
		/* active Object 
		 * 	- use relative distance of 3D-cursor from object center 
		 */
		sub_v3_v3v3(vec, fp, ob->loc);
	}
	else
#endif	
	{
		/* use 3D-cursor */
		copy_v3_v3(vec, fp);
	}
}

/* Stroke Editing ---------------------------- */

/* check if the current mouse position is suitable for adding a new point */
static short gp_stroke_filtermval (tGPsdata *p, const int mval[2], int pmval[2])
{
	int dx= abs(mval[0] - pmval[0]);
	int dy= abs(mval[1] - pmval[1]);
	
	/* if buffer is empty, just let this go through (i.e. so that dots will work) */
	if (p->gpd->sbuffer_size == 0)
		return 1;
	
	/* check if mouse moved at least certain distance on both axes (best case) 
	 *	- aims to eliminate some jitter-noise from input when trying to draw straight lines freehand
	 */
	else if ((dx > MIN_MANHATTEN_PX) && (dy > MIN_MANHATTEN_PX))
		return 1;
	
	/* check if the distance since the last point is significant enough 
	 *	- prevents points being added too densely
	 *	- distance here doesn't use sqrt to prevent slowness... we should still be safe from overflows though
	 */
	else if ((dx*dx + dy*dy) > MIN_EUCLIDEAN_PX*MIN_EUCLIDEAN_PX)
		return 1;
	
	/* mouse 'didn't move' */
	else
		return 0;
}

/* convert screen-coordinates to buffer-coordinates */
// XXX this method needs a total overhaul!
static void gp_stroke_convertcoords (tGPsdata *p, const int mval[2], float out[3], float *depth)
{
	bGPdata *gpd= p->gpd;
	
	/* in 3d-space - pt->x/y/z are 3 side-by-side floats */
	if (gpd->sbuffer_sflag & GP_STROKE_3DSPACE) {
		if (gpencil_project_check(p) && (ED_view3d_autodist_simple(p->ar, mval, out, 0, depth))) {
			/* projecting onto 3D-Geometry
			 *	- nothing more needs to be done here, since view_autodist_simple() has already done it
			 */
		}
		else {
			int mval_prj[2];
			float rvec[3], dvec[3];
			float mval_f[2];

			/* Current method just converts each point in screen-coordinates to
			 * 3D-coordinates using the 3D-cursor as reference. In general, this
			 * works OK, but it could of course be improved.
			 *
			 * TODO:
			 *	- investigate using nearest point(s) on a previous stroke as
			 *	  reference point instead or as offset, for easier stroke matching
			 */
			
			gp_get_3d_reference(p, rvec);
			
			/* method taken from editview.c - mouse_cursor() */
			project_int_noclip(p->ar, rvec, mval_prj);

			VECSUB2D(mval_f, mval_prj, mval);
			ED_view3d_win_to_delta(p->ar, mval_f, dvec);
			sub_v3_v3v3(out, rvec, dvec);
		}
	}
	
	/* 2d - on 'canvas' (assume that p->v2d is set) */
	else if ((gpd->sbuffer_sflag & GP_STROKE_2DSPACE) && (p->v2d)) {
		UI_view2d_region_to_view(p->v2d, mval[0], mval[1], &out[0], &out[1]);
		mul_v3_m4v3(out, p->imat, out);
	}
	
#if 0
	/* 2d - on image 'canvas' (assume that p->v2d is set) */
	else if (gpd->sbuffer_sflag & GP_STROKE_2DIMAGE) {
		int sizex, sizey, offsx, offsy;
		
		/* get stored settings 
		 *	- assume that these have been set already (there are checks that set sane 'defaults' just in case)
		 */
		sizex= p->im2d_settings.sizex;
		sizey= p->im2d_settings.sizey;
		offsx= p->im2d_settings.offsx;
		offsy= p->im2d_settings.offsy;
		
		/* calculate new points */
		out[0]= (float)(mval[0] - offsx) / (float)sizex;
		out[1]= (float)(mval[1] - offsy) / (float)sizey;
	}
#endif
	
	/* 2d - relative to screen (viewport area) */
	else {
		if (p->subrect == NULL) { /* normal 3D view */
			out[0] = (float)(mval[0]) / (float)(p->ar->winx) * 100;
			out[1] = (float)(mval[1]) / (float)(p->ar->winy) * 100;
		}
		else { /* camera view, use subrect */
			out[0]= ((mval[0] - p->subrect->xmin) / ((p->subrect->xmax - p->subrect->xmin))) * 100;
			out[1]= ((mval[1] - p->subrect->ymin) / ((p->subrect->ymax - p->subrect->ymin))) * 100;
		}
	}
}

/* add current stroke-point to buffer (returns whether point was successfully added) */
static short gp_stroke_addpoint (tGPsdata *p, const int mval[2], float pressure)
{
	bGPdata *gpd= p->gpd;
	tGPspoint *pt;
	
	/* check painting mode */
	if (p->paintmode == GP_PAINTMODE_DRAW_STRAIGHT) {
		/* straight lines only - i.e. only store start and end point in buffer */
		if (gpd->sbuffer_size == 0) {
			/* first point in buffer (start point) */
			pt= (tGPspoint *)(gpd->sbuffer);
			
			/* store settings */
			copy_v2_v2_int(&pt->x, mval);
			pt->pressure= pressure;
			
			/* increment buffer size */
			gpd->sbuffer_size++;
		}
		else {
			/* normally, we just reset the endpoint to the latest value 
			 *	- assume that pointers for this are always valid...
			 */
			pt= ((tGPspoint *)(gpd->sbuffer) + 1);
			
			/* store settings */
			copy_v2_v2_int(&pt->x, mval);
			pt->pressure= pressure;
			
			/* if this is just the second point we've added, increment the buffer size
			 * so that it will be drawn properly...
			 * otherwise, just leave it alone, otherwise we get problems
			 */
			if (gpd->sbuffer_size != 2)
				gpd->sbuffer_size= 2;
		}
		
		/* can keep carrying on this way :) */
		return GP_STROKEADD_NORMAL;
	}
	else if (p->paintmode == GP_PAINTMODE_DRAW) { /* normal drawing */
		/* check if still room in buffer */
		if (gpd->sbuffer_size >= GP_STROKE_BUFFER_MAX)
			return GP_STROKEADD_OVERFLOW;
		
		/* get pointer to destination point */
		pt= ((tGPspoint *)(gpd->sbuffer) + gpd->sbuffer_size);
		
		/* store settings */
		copy_v2_v2_int(&pt->x, mval);
		pt->pressure= pressure;
		
		/* increment counters */
		gpd->sbuffer_size++;
		
		/* check if another operation can still occur */
		if (gpd->sbuffer_size == GP_STROKE_BUFFER_MAX)
			return GP_STROKEADD_FULL;
		else
			return GP_STROKEADD_NORMAL;
	}
	else if (p->paintmode == GP_PAINTMODE_DRAW_POLY) {
		/* get pointer to destination point */
		pt= (tGPspoint *)(gpd->sbuffer);

		/* store settings */
		copy_v2_v2_int(&pt->x, mval);
		pt->pressure= pressure;

		/* if there's stroke for this poly line session add (or replace last) point
		 * to stroke. This allows to draw lines more interactively (see new segment
		 * during mouse slide, i.e.) 
		 */
		if (p->flags & GP_PAINTFLAG_STROKEADDED) {
			bGPDstroke *gps= p->gpf->strokes.last;
			bGPDspoint *pts;

			/* first time point is adding to temporary buffer -- need to allocate new point in stroke */
			if (gpd->sbuffer_size == 0) {
				gps->points = MEM_reallocN(gps->points, sizeof(bGPDspoint)*(gps->totpoints+1));
				gps->totpoints++;
			}

			pts = &gps->points[gps->totpoints-1];

			/* special case for poly lines: normally, depth is needed only when creating new stroke from buffer,
			 * but poly lines are converting to stroke instantly, so initialize depth buffer before converting coordinates 
			 */
			if (gpencil_project_check(p)) {
				View3D *v3d= p->sa->spacedata.first;

				view3d_region_operator_needs_opengl(p->win, p->ar);
				ED_view3d_autodist_init(p->scene, p->ar, v3d, (p->gpd->flag & GP_DATA_DEPTH_STROKE) ? 1:0);
			}

			/* convert screen-coordinates to appropriate coordinates (and store them) */
			gp_stroke_convertcoords(p, &pt->x, &pts->x, NULL);

			/* copy pressure */
			pts->pressure= pt->pressure;
		}

		/* increment counters */
		if (gpd->sbuffer_size == 0)
			gpd->sbuffer_size++;

		return GP_STROKEADD_NORMAL;
	}
	
	/* return invalid state for now... */
	return GP_STROKEADD_INVALID;
}


/* temp struct for gp_stroke_smooth() */
typedef struct tGpSmoothCo {
	int x;
	int y;
} tGpSmoothCo;

/* smooth a stroke (in buffer) before storing it */
static void gp_stroke_smooth (tGPsdata *p)
{
	bGPdata *gpd= p->gpd;
	tGpSmoothCo *smoothArray, *spc;
	int i=0, cmx=gpd->sbuffer_size;
	
	/* only smooth if smoothing is enabled, and we're not doing a straight line */
	if (!(U.gp_settings & GP_PAINT_DOSMOOTH) || ELEM(p->paintmode, GP_PAINTMODE_DRAW_STRAIGHT, GP_PAINTMODE_DRAW_POLY))
		return;
	
	/* don't try if less than 2 points in buffer */
	if ((cmx <= 2) || (gpd->sbuffer == NULL))
		return;
	
	/* create a temporary smoothing coordinates buffer, use to store calculated values to prevent sequential error */
	smoothArray = MEM_callocN(sizeof(tGpSmoothCo)*cmx, "gp_stroke_smooth smoothArray");
	
	/* first pass: calculate smoothing coordinates using weighted-averages */
	for (i=0, spc=smoothArray; i < gpd->sbuffer_size; i++, spc++) {
		const tGPspoint *pc= (((tGPspoint *)gpd->sbuffer) + i);
		const tGPspoint *pb= (i-1 > 0)?(pc-1):(pc);
		const tGPspoint *pa= (i-2 > 0)?(pc-2):(pb);
		const tGPspoint *pd= (i+1 < cmx)?(pc+1):(pc);
		const tGPspoint *pe= (i+2 < cmx)?(pc+2):(pd);
		
		spc->x= (int)(0.1*pa->x + 0.2*pb->x + 0.4*pc->x + 0.2*pd->x + 0.1*pe->x);
		spc->y= (int)(0.1*pa->y + 0.2*pb->y + 0.4*pc->y + 0.2*pd->y + 0.1*pe->y);
	}
	
	/* second pass: apply smoothed coordinates */
	for (i=0, spc=smoothArray; i < gpd->sbuffer_size; i++, spc++) {
		tGPspoint *pc= (((tGPspoint *)gpd->sbuffer) + i);

		copy_v2_v2_int(&pc->x, &spc->x);
	}
	
	/* free temp array */
	MEM_freeN(smoothArray);
}

/* simplify a stroke (in buffer) before storing it 
 *	- applies a reverse Chaikin filter
 *	- code adapted from etch-a-ton branch (editarmature_sketch.c)
 */
static void gp_stroke_simplify (tGPsdata *p)
{
	bGPdata *gpd= p->gpd;
	tGPspoint *old_points= (tGPspoint *)gpd->sbuffer;
	short num_points= gpd->sbuffer_size;
	short flag= gpd->sbuffer_sflag;
	short i, j;
	
	/* only simplify if simplification is enabled, and we're not doing a straight line */
	if (!(U.gp_settings & GP_PAINT_DOSIMPLIFY) || (p->paintmode == GP_PAINTMODE_DRAW_STRAIGHT))
		return;
	
	/* don't simplify if less than 4 points in buffer */
	if ((num_points <= 4) || (old_points == NULL))
		return;
		
	/* clear buffer (but don't free mem yet) so that we can write to it 
	 *	- firstly set sbuffer to NULL, so a new one is allocated
	 *	- secondly, reset flag after, as it gets cleared auto
	 */
	gpd->sbuffer= NULL;
	gp_session_validatebuffer(p);
	gpd->sbuffer_sflag = flag;
	
/* macro used in loop to get position of new point
 *	- used due to the mixture of datatypes in use here
 */
#define GP_SIMPLIFY_AVPOINT(offs, sfac) \
	{ \
		co[0] += (float)(old_points[offs].x * sfac); \
		co[1] += (float)(old_points[offs].y * sfac); \
		pressure += old_points[offs].pressure * sfac; \
	}
	
	for (i = 0, j = 0; i < num_points; i++)
	{
		if (i - j == 3)
		{
			float co[2], pressure;
			int mco[2];
			
			/* initialize values */
			co[0]= 0;
			co[1]= 0;
			pressure = 0;
			
			/* using macro, calculate new point */
			GP_SIMPLIFY_AVPOINT(j, -0.25f);
			GP_SIMPLIFY_AVPOINT(j+1, 0.75f);
			GP_SIMPLIFY_AVPOINT(j+2, 0.75f);
			GP_SIMPLIFY_AVPOINT(j+3, -0.25f);
			
			/* set values for adding */
			mco[0]= (int)co[0];
			mco[1]= (int)co[1];
			
			/* ignore return values on this... assume to be ok for now */
			gp_stroke_addpoint(p, mco, pressure);
			
			j += 2;
		}
	} 
	
	/* free old buffer */
	MEM_freeN(old_points);
}


/* make a new stroke from the buffer data */
static void gp_stroke_newfrombuffer (tGPsdata *p)
{
	bGPdata *gpd= p->gpd;
	bGPDstroke *gps;
	bGPDspoint *pt;
	tGPspoint *ptc;
	int i, totelem;
	/* since strokes are so fine, when using their depth we need a margin otherwise they might get missed */
	int depth_margin = (p->gpd->flag & GP_DATA_DEPTH_STROKE) ? 4 : 0;
	
	/* get total number of points to allocate space for 
	 *	- drawing straight-lines only requires the endpoints
	 */
	if (p->paintmode == GP_PAINTMODE_DRAW_STRAIGHT)
		totelem = (gpd->sbuffer_size >= 2) ? 2: gpd->sbuffer_size;
	else
		totelem = gpd->sbuffer_size;
	
	/* exit with error if no valid points from this stroke */
	if (totelem == 0) {
		if (G.f & G_DEBUG) 
			printf("Error: No valid points in stroke buffer to convert (tot=%d) \n", gpd->sbuffer_size);
		return;
	}
	
	/* special case for poly line -- for already added stroke during session
	   coordinates are getting added to stroke immediatelly to allow more
	   interactive behavior */
	if (p->paintmode == GP_PAINTMODE_DRAW_POLY) {
		if (p->flags & GP_PAINTFLAG_STROKEADDED)
			return;
	}

	/* allocate memory for a new stroke */
	gps= MEM_callocN(sizeof(bGPDstroke), "gp_stroke");
	
	/* copy appropriate settings for stroke */
	gps->totpoints= totelem;
	gps->thickness= p->gpl->thickness;
	gps->flag= gpd->sbuffer_sflag;
	
	/* allocate enough memory for a continuous array for storage points */
	gps->points= MEM_callocN(sizeof(bGPDspoint)*gps->totpoints, "gp_stroke_points");

	/* set pointer to first non-initialized point */
	pt= gps->points + (gps->totpoints - totelem);

	/* copy points from the buffer to the stroke */
	if (p->paintmode == GP_PAINTMODE_DRAW_STRAIGHT) {
		/* straight lines only -> only endpoints */
		{
			/* first point */
			ptc= gpd->sbuffer;
			
			/* convert screen-coordinates to appropriate coordinates (and store them) */
			gp_stroke_convertcoords(p, &ptc->x, &pt->x, NULL);
			
			/* copy pressure */
			pt->pressure= ptc->pressure;
			
			pt++;
		}
			
		if (totelem == 2) {
			/* last point if applicable */
			ptc= ((tGPspoint *)gpd->sbuffer) + (gpd->sbuffer_size - 1);
			
			/* convert screen-coordinates to appropriate coordinates (and store them) */
			gp_stroke_convertcoords(p, &ptc->x, &pt->x, NULL);
			
			/* copy pressure */
			pt->pressure= ptc->pressure;
		}
	}
	else if (p->paintmode == GP_PAINTMODE_DRAW_POLY) {
		/* first point */
		ptc= gpd->sbuffer;

		/* convert screen-coordinates to appropriate coordinates (and store them) */
		gp_stroke_convertcoords(p, &ptc->x, &pt->x, NULL);

		/* copy pressure */
		pt->pressure= ptc->pressure;
	}
	else {
		float *depth_arr= NULL;
		
		/* get an array of depths, far depths are blended */
		if (gpencil_project_check(p)) {
			int mval[2], mval_prev[2]= {0};
			int interp_depth = 0;
			int found_depth = 0;
			
			depth_arr= MEM_mallocN(sizeof(float) * gpd->sbuffer_size, "depth_points");

			for (i=0, ptc=gpd->sbuffer; i < gpd->sbuffer_size; i++, ptc++, pt++) {
				copy_v2_v2_int(mval, &ptc->x);

				if ((ED_view3d_autodist_depth(p->ar, mval, depth_margin, depth_arr+i) == 0) &&
					(i && (ED_view3d_autodist_depth_seg(p->ar, mval, mval_prev, depth_margin + 1, depth_arr+i) == 0))
				) {
					interp_depth= TRUE;
				}
				else {
					found_depth= TRUE;
				}

				copy_v2_v2_int(mval_prev, mval);
			}
			
			if (found_depth == FALSE) {
				/* eeh... not much we can do.. :/, ignore depth in this case, use the 3D cursor */
				for (i=gpd->sbuffer_size-1; i >= 0; i--)
					depth_arr[i] = 0.9999f;
			}
			else {
				if (p->gpd->flag & GP_DATA_DEPTH_STROKE_ENDPOINTS) {
					/* remove all info between the valid endpoints */
					int first_valid = 0;
					int last_valid = 0;
					
					for (i=0; i < gpd->sbuffer_size; i++) {
						if (depth_arr[i] != FLT_MAX)
							break;
					}
					first_valid= i;
					
					for (i=gpd->sbuffer_size-1; i >= 0; i--) {
						if (depth_arr[i] != FLT_MAX)
							break;
					}
					last_valid= i;
					
					/* invalidate non-endpoints, so only blend between first and last */
					for (i=first_valid+1; i < last_valid; i++)
						depth_arr[i]= FLT_MAX;
					
					interp_depth= TRUE;
				}
				
				if (interp_depth) {
					interp_sparse_array(depth_arr, gpd->sbuffer_size, FLT_MAX);
				}
			}
		}
		
		
		pt= gps->points;
		
		/* convert all points (normal behavior) */
		for (i=0, ptc=gpd->sbuffer; i < gpd->sbuffer_size && ptc; i++, ptc++, pt++) {
			/* convert screen-coordinates to appropriate coordinates (and store them) */
			gp_stroke_convertcoords(p, &ptc->x, &pt->x, depth_arr ? depth_arr+i:NULL);
			
			/* copy pressure */
			pt->pressure= ptc->pressure;
		}
		
		if (depth_arr)
			MEM_freeN(depth_arr);
	}
	
	p->flags |= GP_PAINTFLAG_STROKEADDED;

	/* add stroke to frame */
	BLI_addtail(&p->gpf->strokes, gps);
}

/* --- 'Eraser' for 'Paint' Tool ------ */

/* eraser tool - remove segment from stroke/split stroke (after lasso inside) */
static short gp_stroke_eraser_splitdel (bGPDframe *gpf, bGPDstroke *gps, int i)
{
	bGPDspoint *pt_tmp= gps->points;
	bGPDstroke *gsn = NULL;

	/* if stroke only had two points, get rid of stroke */
	if (gps->totpoints == 2) {
		/* free stroke points, then stroke */
		MEM_freeN(pt_tmp);
		BLI_freelinkN(&gpf->strokes, gps);
		
		/* nothing left in stroke, so stop */
		return 1;
	}

	/* if last segment, just remove segment from the stroke */
	else if (i == gps->totpoints - 2) {
		/* allocate new points array, and assign most of the old stroke there */
		gps->totpoints--;
		gps->points= MEM_callocN(sizeof(bGPDspoint)*gps->totpoints, "gp_stroke_points");
		memcpy(gps->points, pt_tmp, sizeof(bGPDspoint)*gps->totpoints);
		
		/* free temp buffer */
		MEM_freeN(pt_tmp);
		
		/* nothing left in stroke, so stop */
		return 1;
	}

	/* if first segment, just remove segment from the stroke */
	else if (i == 0) {
		/* allocate new points array, and assign most of the old stroke there */
		gps->totpoints--;
		gps->points= MEM_callocN(sizeof(bGPDspoint)*gps->totpoints, "gp_stroke_points");
		memcpy(gps->points, pt_tmp + 1, sizeof(bGPDspoint)*gps->totpoints);
		
		/* free temp buffer */
		MEM_freeN(pt_tmp);
		
		/* no break here, as there might still be stuff to remove in this stroke */
		return 0;
	}

	/* segment occurs in 'middle' of stroke, so split */
	else {
		/* duplicate stroke, and assign 'later' data to that stroke */
		gsn= MEM_dupallocN(gps);
		gsn->prev= gsn->next= NULL;
		BLI_insertlinkafter(&gpf->strokes, gps, gsn);
		
		gsn->totpoints= gps->totpoints - i;
		gsn->points= MEM_callocN(sizeof(bGPDspoint)*gsn->totpoints, "gp_stroke_points");
		memcpy(gsn->points, pt_tmp + i, sizeof(bGPDspoint)*gsn->totpoints);
		
		/* adjust existing stroke  */
		gps->totpoints= i;
		gps->points= MEM_callocN(sizeof(bGPDspoint)*gps->totpoints, "gp_stroke_points");
		memcpy(gps->points, pt_tmp, sizeof(bGPDspoint)*i);
		
		/* free temp buffer */
		MEM_freeN(pt_tmp);
		
		/* nothing left in stroke, so stop */
		return 1;
	}
}

/* eraser tool - check if part of stroke occurs within last segment drawn by eraser */
static short gp_stroke_eraser_strokeinside (int mval[], int UNUSED(mvalo[]), short rad, short x0, short y0, short x1, short y1)
{
	/* simple within-radius check for now */
	if (edge_inside_circle(mval[0], mval[1], rad, x0, y0, x1, y1))
		return 1;
	
	/* not inside */
	return 0;
} 

/* eraser tool - evaluation per stroke */
// TODO: this could really do with some optimization (KD-Tree/BVH?)
static void gp_stroke_eraser_dostroke (tGPsdata *p, int mval[], int mvalo[], short rad, rcti *rect, bGPDframe *gpf, bGPDstroke *gps)
{
	bGPDspoint *pt1, *pt2;
	int x0=0, y0=0, x1=0, y1=0;
	int xyval[2];
	int i;
	
	if (gps->totpoints == 0) {
		/* just free stroke */
		if (gps->points) 
			MEM_freeN(gps->points);
		BLI_freelinkN(&gpf->strokes, gps);
	}
	else if (gps->totpoints == 1) {
		/* get coordinates */
		if (gps->flag & GP_STROKE_3DSPACE) {
			project_int(p->ar, &gps->points->x, xyval);
			x0= xyval[0];
			y0= xyval[1];
		}
		else if (gps->flag & GP_STROKE_2DSPACE) {			
			UI_view2d_view_to_region(p->v2d, gps->points->x, gps->points->y, &x0, &y0);
		}
#if 0
		else if (gps->flag & GP_STROKE_2DIMAGE) {			
			int offsx, offsy, sizex, sizey;
			
			/* get stored settings */
			sizex= p->im2d_settings.sizex;
			sizey= p->im2d_settings.sizey;
			offsx= p->im2d_settings.offsx;
			offsy= p->im2d_settings.offsy;
			
			/* calculate new points */
			x0= (int)((gps->points->x * sizex) + offsx);
			y0= (int)((gps->points->y * sizey) + offsy);
		}
#endif
		else {
			if (p->subrect == NULL) { /* normal 3D view */
				x0= (int)(gps->points->x / 100 * p->ar->winx);
				y0= (int)(gps->points->y / 100 * p->ar->winy);
			}
			else { /* camera view, use subrect */
				x0= (int)((gps->points->x / 100) * (p->subrect->xmax - p->subrect->xmin)) + p->subrect->xmin;
				y0= (int)((gps->points->y / 100) * (p->subrect->ymax - p->subrect->ymin)) + p->subrect->ymin;
			}
		}
		
		/* do boundbox check first */
		if (BLI_in_rcti(rect, x0, y0)) {
			/* only check if point is inside */
			if ( ((x0-mval[0])*(x0-mval[0]) + (y0-mval[1])*(y0-mval[1])) <= rad*rad ) {
				/* free stroke */
				MEM_freeN(gps->points);
				BLI_freelinkN(&gpf->strokes, gps);
			}
		}
	}
	else {	
		/* loop over the points in the stroke, checking for intersections 
		 * 	- an intersection will require the stroke to be split
		 */
		for (i=0; (i+1) < gps->totpoints; i++) {
			/* get points to work with */
			pt1= gps->points + i;
			pt2= gps->points + i + 1;
			
			/* get coordinates */
			if (gps->flag & GP_STROKE_3DSPACE) {
				project_int(p->ar, &pt1->x, xyval);
				x0= xyval[0];
				y0= xyval[1];
				
				project_int(p->ar, &pt2->x, xyval);
				x1= xyval[0];
				y1= xyval[1];
			}
			else if (gps->flag & GP_STROKE_2DSPACE) {
				UI_view2d_view_to_region(p->v2d, pt1->x, pt1->y, &x0, &y0);
				
				UI_view2d_view_to_region(p->v2d, pt2->x, pt2->y, &x1, &y1);
			}
#if 0
			else if (gps->flag & GP_STROKE_2DIMAGE) {
				int offsx, offsy, sizex, sizey;
				
				/* get stored settings */
				sizex= p->im2d_settings.sizex;
				sizey= p->im2d_settings.sizey;
				offsx= p->im2d_settings.offsx;
				offsy= p->im2d_settings.offsy;
				
				/* calculate new points */
				x0= (int)((pt1->x * sizex) + offsx);
				y0= (int)((pt1->y * sizey) + offsy);
				
				x1= (int)((pt2->x * sizex) + offsx);
				y1= (int)((pt2->y * sizey) + offsy);
			}
#endif
			else {
				if(p->subrect == NULL) { /* normal 3D view */
					x0= (int)(pt1->x / 100 * p->ar->winx);
					y0= (int)(pt1->y / 100 * p->ar->winy);
					x1= (int)(pt2->x / 100 * p->ar->winx);
					y1= (int)(pt2->y / 100 * p->ar->winy);
				}
				else { /* camera view, use subrect */ 
					x0= (int)((pt1->x / 100) * (p->subrect->xmax - p->subrect->xmin)) + p->subrect->xmin;
					y0= (int)((pt1->y / 100) * (p->subrect->ymax - p->subrect->ymin)) + p->subrect->ymin;
					x1= (int)((pt2->x / 100) * (p->subrect->xmax - p->subrect->xmin)) + p->subrect->xmin;
					y1= (int)((pt2->y / 100) * (p->subrect->ymax - p->subrect->ymin)) + p->subrect->ymin;
				}
			}
			
			/* check that point segment of the boundbox of the eraser stroke */
			if (BLI_in_rcti(rect, x0, y0) || BLI_in_rcti(rect, x1, y1)) {
				/* check if point segment of stroke had anything to do with
				 * eraser region  (either within stroke painted, or on its lines)
				 * 	- this assumes that linewidth is irrelevant
				 */
				if (gp_stroke_eraser_strokeinside(mval, mvalo, rad, x0, y0, x1, y1)) {
					/* if function returns true, break this loop (as no more point to check) */
					if (gp_stroke_eraser_splitdel(gpf, gps, i))
						break;
				}
			}
		}
	}
}

/* erase strokes which fall under the eraser strokes */
static void gp_stroke_doeraser (tGPsdata *p)
{
	bGPDframe *gpf= p->gpf;
	bGPDstroke *gps, *gpn;
	rcti rect;
	
	/* rect is rectangle of eraser */
	rect.xmin= p->mval[0] - p->radius;
	rect.ymin= p->mval[1] - p->radius;
	rect.xmax= p->mval[0] + p->radius;
	rect.ymax= p->mval[1] + p->radius;
	
	/* loop over strokes, checking segments for intersections */
	for (gps= gpf->strokes.first; gps; gps= gpn) {
		gpn= gps->next;
		gp_stroke_eraser_dostroke(p, p->mval, p->mvalo, p->radius, &rect, gpf, gps);
	}
}

/* ******************************************* */
/* Sketching Operator */

/* clear the session buffers (call this before AND after a paint operation) */
static void gp_session_validatebuffer (tGPsdata *p)
{
	bGPdata *gpd= p->gpd;
	
	/* clear memory of buffer (or allocate it if starting a new session) */
	if (gpd->sbuffer) {
		//printf("\t\tGP - reset sbuffer\n");
		memset(gpd->sbuffer, 0, sizeof(tGPspoint)*GP_STROKE_BUFFER_MAX);
	}
	else {
		//printf("\t\tGP - allocate sbuffer\n");
		gpd->sbuffer= MEM_callocN(sizeof(tGPspoint)*GP_STROKE_BUFFER_MAX, "gp_session_strokebuffer");
	}
	
	/* reset indices */
	gpd->sbuffer_size = 0;
	
	/* reset flags */
	gpd->sbuffer_sflag= 0;
}

/* (re)init new painting data */
static int gp_session_initdata (bContext *C, tGPsdata *p)
{
	bGPdata **gpd_ptr = NULL;
	ScrArea *curarea= CTX_wm_area(C);
	ARegion *ar= CTX_wm_region(C);
	
	/* make sure the active view (at the starting time) is a 3d-view */
	if (curarea == NULL) {
		p->status= GP_STATUS_ERROR;
		if (G.f & G_DEBUG) 
			printf("Error: No active view for painting \n");
		return 0;
	}
	
	/* pass on current scene and window */
	p->scene= CTX_data_scene(C);
	p->win= CTX_wm_window(C);

	unit_m4(p->imat);
	
	switch (curarea->spacetype) {
		/* supported views first */
		case SPACE_VIEW3D:
		{
			// View3D *v3d= curarea->spacedata.first;
			// RegionView3D *rv3d= ar->regiondata;
			
			/* set current area 
			 *	- must verify that region data is 3D-view (and not something else)
			 */
			p->sa= curarea;
			p->ar= ar;
			
			if (ar->regiondata == NULL) {
				p->status= GP_STATUS_ERROR;
				if (G.f & G_DEBUG)
					printf("Error: 3D-View active region doesn't have any region data, so cannot be drawable \n");
				return 0;
			}

#if 0 // XXX will this sort of antiquated stuff be restored?
			/* check that gpencil data is allowed to be drawn */
			if ((v3d->flag2 & V3D_DISPGP)==0) {
				p->status= GP_STATUS_ERROR;
				if (G.f & G_DEBUG) 
					printf("Error: In active view, Grease Pencil not shown \n");
				return 0;
			}
#endif
		}
			break;

		case SPACE_NODE:
		{
			//SpaceNode *snode= curarea->spacedata.first;
			
			/* set current area */
			p->sa= curarea;
			p->ar= ar;
			p->v2d= &ar->v2d;
			
#if 0 // XXX will this sort of antiquated stuff be restored?
			/* check that gpencil data is allowed to be drawn */
			if ((snode->flag & SNODE_DISPGP)==0) {
				p->status= GP_STATUS_ERROR;
				if (G.f & G_DEBUG) 
					printf("Error: In active view, Grease Pencil not shown \n");
				return 0;
			}
#endif
		}
			break;
#if 0 // XXX these other spaces will come over time...
		case SPACE_SEQ:
		{
			SpaceSeq *sseq= curarea->spacedata.first;
			
			/* set current area */
			p->sa= curarea;
			p->ar= ar;
			p->v2d= &ar->v2d;
			
			/* check that gpencil data is allowed to be drawn */
			if (sseq->mainb == SEQ_DRAW_SEQUENCE) {
				p->status= GP_STATUS_ERROR;
				if (G.f & G_DEBUG) 
					printf("Error: In active view (sequencer), active mode doesn't support Grease Pencil \n");
				return 0;
			}
			if ((sseq->flag & SEQ_DRAW_GPENCIL)==0) {
				p->status= GP_STATUS_ERROR;
				if (G.f & G_DEBUG) 
					printf("Error: In active view, Grease Pencil not shown \n");
				return 0;
			}
		}
			break;	
#endif
		case SPACE_IMAGE:
		{
			//SpaceImage *sima= curarea->spacedata.first;
			
			/* set the current area */
			p->sa= curarea;
			p->ar= ar;
			p->v2d= &ar->v2d;
			//p->ibuf= BKE_image_get_ibuf(sima->image, &sima->iuser);
			
#if 0 // XXX disabled for now
			/* check that gpencil data is allowed to be drawn */
			if ((sima->flag & SI_DISPGP)==0) {
				p->status= GP_STATUS_ERROR;
				if (G.f & G_DEBUG)
					printf("Error: In active view, Grease Pencil not shown \n");
				return 0;
			}
#endif
		}
			break;
		case SPACE_CLIP:
		{
			SpaceClip *sc= curarea->spacedata.first;

			/* set the current area */
			p->sa= curarea;
			p->ar= ar;
			p->v2d= &ar->v2d;
			//p->ibuf= BKE_image_get_ibuf(sima->image, &sima->iuser);

			invert_m4_m4(p->imat, sc->unistabmat);

			/* custom color for new layer */
			p->custom_color[0]= 1.0f;
			p->custom_color[1]= 0.0f;
			p->custom_color[2]= 0.5f;
			p->custom_color[3]= 0.9f;
		}
			break;

		/* unsupported views */
		default:
		{
			p->status= GP_STATUS_ERROR;
			if (G.f & G_DEBUG) 
				printf("Error: Active view not appropriate for Grease Pencil drawing \n");
			return 0;
		}
			break;
	}
	
	/* get gp-data */
	gpd_ptr= gpencil_data_get_pointers(C, &p->ownerPtr);
	if (gpd_ptr == NULL) {
		p->status= GP_STATUS_ERROR;
		if (G.f & G_DEBUG)
			printf("Error: Current context doesn't allow for any Grease Pencil data \n");
		return 0;
	}
	else {
		/* if no existing GPencil block exists, add one */
		if (*gpd_ptr == NULL)
			*gpd_ptr= gpencil_data_addnew("GPencil");
		p->gpd= *gpd_ptr;
	}
	
	if (ED_gpencil_session_active()==0) {
		/* initialize undo stack,
		   also, existing undo stack would make buffer drawn */
		gpencil_undo_init(p->gpd);
	}
	
	/* clear out buffer (stored in gp-data), in case something contaminated it */
	gp_session_validatebuffer(p);
	
#if 0
	/* set 'default' im2d_settings just in case something that uses this doesn't set it */
	p->im2d_settings.sizex= 1;
	p->im2d_settings.sizey= 1;
#endif

	return 1;
}

/* init new painting session */
static tGPsdata *gp_session_initpaint (bContext *C)
{
	tGPsdata *p = NULL;

	/* create new context data */
	p= MEM_callocN(sizeof(tGPsdata), "GPencil Drawing Data");

	gp_session_initdata(C, p);
	
	/* return context data for running paint operator */
	return p;
}

/* cleanup after a painting session */
static void gp_session_cleanup (tGPsdata *p)
{
	bGPdata *gpd= (p) ? p->gpd : NULL;
	
	/* error checking */
	if (gpd == NULL)
		return;
	
	/* free stroke buffer */
	if (gpd->sbuffer) {
		//printf("\t\tGP - free sbuffer\n");
		MEM_freeN(gpd->sbuffer);
		gpd->sbuffer= NULL;
	}
	
	/* clear flags */
	gpd->sbuffer_size= 0;
	gpd->sbuffer_sflag= 0;
}

/* init new stroke */
static void gp_paint_initstroke (tGPsdata *p, short paintmode)
{	
	/* get active layer (or add a new one if non-existent) */
	p->gpl= gpencil_layer_getactive(p->gpd);
	if (p->gpl == NULL) {
		p->gpl= gpencil_layer_addnew(p->gpd);

		if(p->custom_color[3])
			copy_v3_v3(p->gpl->color, p->custom_color);
	}
	if (p->gpl->flag & GP_LAYER_LOCKED) {
		p->status= GP_STATUS_ERROR;
		if (G.f & G_DEBUG)
			printf("Error: Cannot paint on locked layer \n");
		return;
	}
		
	/* get active frame (add a new one if not matching frame) */
	p->gpf= gpencil_layer_getframe(p->gpl, p->scene->r.cfra, 1);
	if (p->gpf == NULL) {
		p->status= GP_STATUS_ERROR;
		if (G.f & G_DEBUG) 
			printf("Error: No frame created (gpencil_paint_init) \n");
		return;
	}
	else
		p->gpf->flag |= GP_FRAME_PAINT;
	
	/* set 'eraser' for this stroke if using eraser */
	p->paintmode= paintmode;
	if (p->paintmode == GP_PAINTMODE_ERASER)
		p->gpd->sbuffer_sflag |= GP_STROKE_ERASER;
		
	/* set 'initial run' flag, which is only used to denote when a new stroke is starting */
	p->flags |= GP_PAINTFLAG_FIRSTRUN;
	

	/* when drawing in the camera view, in 2D space, set the subrect */
	if (!(p->gpd->flag & GP_DATA_VIEWALIGN)) {
		if (p->sa->spacetype == SPACE_VIEW3D) {
			View3D *v3d= p->sa->spacedata.first;
			RegionView3D *rv3d= p->ar->regiondata;

			/* for camera view set the subrect */
			if (rv3d->persp == RV3D_CAMOB) {
				ED_view3d_calc_camera_border(p->scene, p->ar, v3d, rv3d, &p->subrect_data, TRUE); /* no shift */
				p->subrect= &p->subrect_data;
			}
		}
	}

	/* check if points will need to be made in view-aligned space */
	if (p->gpd->flag & GP_DATA_VIEWALIGN) {
		switch (p->sa->spacetype) {
			case SPACE_VIEW3D:
			{
				RegionView3D *rv3d= p->ar->regiondata;
				float rvec[3];
				
				/* get reference point for 3d space placement */
				gp_get_3d_reference(p, rvec);
				initgrabz(rv3d, rvec[0], rvec[1], rvec[2]);
				
				p->gpd->sbuffer_sflag |= GP_STROKE_3DSPACE;
			}
				break;
			
			case SPACE_NODE:
			{
				p->gpd->sbuffer_sflag |= GP_STROKE_2DSPACE;
			}
				break;
#if 0 // XXX other spacetypes to be restored in due course
			case SPACE_SEQ:
			{
				SpaceSeq *sseq= (SpaceSeq *)p->sa->spacedata.first;
				int rectx, recty;
				float zoom, zoomx, zoomy;
				
				/* set draw 2d-stroke flag */
				p->gpd->sbuffer_sflag |= GP_STROKE_2DIMAGE;
				
				/* calculate zoom factor */
				zoom= (float)(SEQ_ZOOM_FAC(sseq->zoom));
				if (sseq->mainb == SEQ_DRAW_IMG_IMBUF) {
					zoomx = zoom * (p->scene->r.xasp / p->scene->r.yasp);
					zoomy = zoom;
				} 
				else
					zoomx = zoomy = zoom;
				
				/* calculate rect size to use to calculate the size of the drawing area
				 *	- We use the size of the output image not the size of the ibuf being shown
				 *	  as it is too messy getting the ibuf (and could be too slow). This should be
				 *	  a reasonable for most cases anyway.
				 */
				rectx= (p->scene->r.size * p->scene->r.xsch) / 100;
				recty= (p->scene->r.size * p->scene->r.ysch) / 100; 
				
				/* set offset and scale values for opertations to use */
				p->im2d_settings.sizex= (int)(zoomx * rectx);
				p->im2d_settings.sizey= (int)(zoomy * recty);
				p->im2d_settings.offsx= (int)((p->sa->winx-p->im2d_settings.sizex)/2 + sseq->xof);
				p->im2d_settings.offsy= (int)((p->sa->winy-p->im2d_settings.sizey)/2 + sseq->yof);
			}
				break;
#endif
			case SPACE_IMAGE:
			{
				SpaceImage *sima= (SpaceImage *)p->sa->spacedata.first;
				
				/* only set these flags if the image editor doesn't have an image active,
				 * otherwise user will be confused by strokes not appearing after they're drawn
				 *
				 * Admittedly, this is a bit hacky, but it works much nicer from an ergonomic standpoint!
				 */
				if ELEM(NULL, sima, sima->image) {
					/* make strokes be drawn in screen space */
					p->gpd->sbuffer_sflag &= ~GP_STROKE_2DSPACE;
					p->gpd->flag &= ~GP_DATA_VIEWALIGN;
				}	
				else
					p->gpd->sbuffer_sflag |= GP_STROKE_2DSPACE;
			}
				break;
				
			case SPACE_CLIP:
			{
				p->gpd->sbuffer_sflag |= GP_STROKE_2DSPACE;
			}
				break;
		}
	}
}

/* finish off a stroke (clears buffer, but doesn't finish the paint operation) */
static void gp_paint_strokeend (tGPsdata *p)
{
	/* for surface sketching, need to set the right OpenGL context stuff so that 
	 * the conversions will project the values correctly...
	 */
	if (gpencil_project_check(p)) {
		View3D *v3d= p->sa->spacedata.first;
		
		/* need to restore the original projection settings before packing up */
		view3d_region_operator_needs_opengl(p->win, p->ar);
		ED_view3d_autodist_init(p->scene, p->ar, v3d, (p->gpd->flag & GP_DATA_DEPTH_STROKE) ? 1:0);
	}
	
	/* check if doing eraser or not */
	if ((p->gpd->sbuffer_sflag & GP_STROKE_ERASER) == 0) {
		/* smooth stroke before transferring? */
		gp_stroke_smooth(p);
		
		/* simplify stroke before transferring? */
		gp_stroke_simplify(p);
		
		/* transfer stroke to frame */
		gp_stroke_newfrombuffer(p);
	}
	
	/* clean up buffer now */
	gp_session_validatebuffer(p);
}

/* finish off stroke painting operation */
static void gp_paint_cleanup (tGPsdata *p)
{
	/* p->gpd==NULL happens when stroke failed to initialize,
	      for example. when GP is hidden in current space (sergey) */
	if (p->gpd) {
		/* finish off a stroke */
		gp_paint_strokeend(p);
	}
	
	/* "unlock" frame */
	if (p->gpf)
		p->gpf->flag &= ~GP_FRAME_PAINT;
}

/* ------------------------------- */

static void gpencil_draw_exit (bContext *C, wmOperator *op)
{
	tGPsdata *p= op->customdata;
	
	/* clear undo stack */
	gpencil_undo_finish();
	
	/* restore cursor to indicate end of drawing */
	WM_cursor_restore(CTX_wm_window(C));
	
	/* don't assume that operator data exists at all */
	if (p) {
		/* check size of buffer before cleanup, to determine if anything happened here */
		if (p->paintmode == GP_PAINTMODE_ERASER) {
			// TODO clear radial cursor thing
			// XXX draw_sel_circle(NULL, p.mvalo, 0, p.radius, 0);
		}
		
		/* cleanup */
		gp_paint_cleanup(p);
		gp_session_cleanup(p);
		
		/* finally, free the temp data */
		MEM_freeN(p);	
	}
	
	op->customdata= NULL;
}

static int gpencil_draw_cancel (bContext *C, wmOperator *op)
{
	/* this is just a wrapper around exit() */
	gpencil_draw_exit(C, op);
	return OPERATOR_CANCELLED;
}

/* ------------------------------- */


static int gpencil_draw_init (bContext *C, wmOperator *op)
{
	tGPsdata *p;
	int paintmode= RNA_enum_get(op->ptr, "mode");
	
	/* check context */
	p= op->customdata= gp_session_initpaint(C);
	if ((p == NULL) || (p->status == GP_STATUS_ERROR)) {
		/* something wasn't set correctly in context */
		gpencil_draw_exit(C, op);
		return 0;
	}
	
	/* init painting data */
	gp_paint_initstroke(p, paintmode);
	if (p->status == GP_STATUS_ERROR) {
		gpencil_draw_exit(C, op);
		return 0;
	}
	
	/* radius for eraser circle is defined in userprefs now */
	p->radius= U.gp_eraser;
	
	/* everything is now setup ok */
	return 1;
}

/* ------------------------------- */

/* update UI indicators of status, including cursor and header prints */
static void gpencil_draw_status_indicators (tGPsdata *p)
{
	/* header prints */
	switch (p->status) {
		case GP_STATUS_PAINTING:
			/* only print this for paint-sessions, otherwise it gets annoying */
			if (GPENCIL_SKETCH_SESSIONS_ON(p->scene))
				ED_area_headerprint(p->sa, "Grease Pencil: Drawing/erasing stroke... Release to end stroke");
			break;
		
		case GP_STATUS_IDLING:
			/* print status info */
			switch (p->paintmode) {
				case GP_PAINTMODE_ERASER:
					ED_area_headerprint(p->sa, "Grease Pencil Erase Session: Hold and drag LMB or RMB to erase | ESC/Enter to end");
					break;
				case GP_PAINTMODE_DRAW_STRAIGHT:
					ED_area_headerprint(p->sa, "Grease Pencil Line Session: Hold and drag LMB to draw | ESC/Enter to end");
					break;
				case GP_PAINTMODE_DRAW:
					ED_area_headerprint(p->sa, "Grease Pencil Freehand Session: Hold and drag LMB to draw | ESC/Enter to end");
					break;
					
				default: /* unhandled future cases */
					ED_area_headerprint(p->sa, "Grease Pencil Session: ESC/Enter to end");
					break;
			}
			break;
			
		case GP_STATUS_ERROR:
		case GP_STATUS_DONE:
			/* clear status string */
			ED_area_headerprint(p->sa, NULL);
			break;
	}
}

/* ------------------------------- */

/* create a new stroke point at the point indicated by the painting context */
static void gpencil_draw_apply (wmOperator *op, tGPsdata *p)
{
	/* handle drawing/erasing -> test for erasing first */
	if (p->paintmode == GP_PAINTMODE_ERASER) {
		/* do 'live' erasing now */
		gp_stroke_doeraser(p);
		
		/* store used values */
		p->mvalo[0]= p->mval[0];
		p->mvalo[1]= p->mval[1];
		p->opressure= p->pressure;
	}
	/* only add current point to buffer if mouse moved (even though we got an event, it might be just noise) */
	else if (gp_stroke_filtermval(p, p->mval, p->mvalo)) {
		/* try to add point */
		short ok= gp_stroke_addpoint(p, p->mval, p->pressure);
		
		/* handle errors while adding point */
		if ((ok == GP_STROKEADD_FULL) || (ok == GP_STROKEADD_OVERFLOW)) {
			/* finish off old stroke */
			gp_paint_strokeend(p);
			
			/* start a new stroke, starting from previous point */
			gp_stroke_addpoint(p, p->mvalo, p->opressure);
			gp_stroke_addpoint(p, p->mval, p->pressure);
		}
		else if (ok == GP_STROKEADD_INVALID) {
			/* the painting operation cannot continue... */
			BKE_report(op->reports, RPT_ERROR, "Cannot paint stroke");
			p->status = GP_STATUS_ERROR;
			
			if (G.f & G_DEBUG) 
				printf("Error: Grease-Pencil Paint - Add Point Invalid \n");
			return;
		}
		
		/* store used values */
		p->mvalo[0]= p->mval[0];
		p->mvalo[1]= p->mval[1];
		p->opressure= p->pressure;
	}
}

/* handle draw event */
static void gpencil_draw_apply_event (wmOperator *op, wmEvent *event)
{
	tGPsdata *p= op->customdata;
	PointerRNA itemptr;
	float mousef[2];
	int tablet=0;

	/* convert from window-space to area-space mouse coordintes */
	// NOTE: float to ints conversions, +1 factor is probably used to ensure a bit more accurate rounding...
	p->mval[0]= event->mval[0] + 1;
	p->mval[1]= event->mval[1] + 1;

	/* handle pressure sensitivity (which is supplied by tablets) */
	if (event->custom == EVT_DATA_TABLET) {
		wmTabletData *wmtab= event->customdata;
		
		tablet= (wmtab->Active != EVT_TABLET_NONE);
		p->pressure= wmtab->Pressure;
		
		//if (wmtab->Active == EVT_TABLET_ERASER)
			// TODO... this should get caught by the keymaps which call drawing in the first place
	}
	else
		p->pressure= 1.0f;
	
	/* fill in stroke data (not actually used directly by gpencil_draw_apply) */
	RNA_collection_add(op->ptr, "stroke", &itemptr);
	
	mousef[0]= p->mval[0];
	mousef[1]= p->mval[1];
	RNA_float_set_array(&itemptr, "mouse", mousef);
	RNA_float_set(&itemptr, "pressure", p->pressure);
	RNA_boolean_set(&itemptr, "is_start", (p->flags & GP_PAINTFLAG_FIRSTRUN));
	
	/* special exception for start of strokes (i.e. maybe for just a dot) */
	if (p->flags & GP_PAINTFLAG_FIRSTRUN) {
		p->flags &= ~GP_PAINTFLAG_FIRSTRUN;
		
		p->mvalo[0]= p->mval[0];
		p->mvalo[1]= p->mval[1];
		p->opressure= p->pressure;
		
		/* special exception here for too high pressure values on first touch in
		 *  windows for some tablets, then we just skip first touch ..  
		 */
		if (tablet && (p->pressure >= 0.99f))
			return;
	}
	
	/* apply the current latest drawing point */
	gpencil_draw_apply(op, p);
	
	/* force refresh */
	ED_region_tag_redraw(p->ar); /* just active area for now, since doing whole screen is too slow */
}

/* ------------------------------- */

/* operator 'redo' (i.e. after changing some properties, but also for repeat last) */
static int gpencil_draw_exec (bContext *C, wmOperator *op)
{
	tGPsdata *p = NULL;
	
	//printf("GPencil - Starting Re-Drawing \n");
	
	/* try to initialize context data needed while drawing */
	if (!gpencil_draw_init(C, op)) {
		if (op->customdata) MEM_freeN(op->customdata);
		//printf("\tGP - no valid data \n");
		return OPERATOR_CANCELLED;
	}
	else
		p= op->customdata;
	
	//printf("\tGP - Start redrawing stroke \n");
	
	/* loop over the stroke RNA elements recorded (i.e. progress of mouse movement),
	 * setting the relevant values in context at each step, then applying
	 */
	RNA_BEGIN(op->ptr, itemptr, "stroke") 
	{
		float mousef[2];
		
		//printf("\t\tGP - stroke elem \n");
		
		/* get relevant data for this point from stroke */
		RNA_float_get_array(&itemptr, "mouse", mousef);
		p->mval[0] = (int)mousef[0];
		p->mval[1] = (int)mousef[1];
		p->pressure= RNA_float_get(&itemptr, "pressure");
		
		if (RNA_boolean_get(&itemptr, "is_start")) {
			/* if first-run flag isn't set already (i.e. not true first stroke),
			 * then we must terminate the previous one first before continuing
			 */
			if ((p->flags & GP_PAINTFLAG_FIRSTRUN) == 0) {
				// TODO: both of these ops can set error-status, but we probably don't need to worry
				gp_paint_strokeend(p);
				gp_paint_initstroke(p, p->paintmode);
			}
		}
		
		/* if first run, set previous data too */
		if (p->flags & GP_PAINTFLAG_FIRSTRUN) {
			p->flags &= ~GP_PAINTFLAG_FIRSTRUN;
			
			p->mvalo[0]= p->mval[0];
			p->mvalo[1]= p->mval[1];
			p->opressure= p->pressure;
		}
		
		/* apply this data as necessary now (as per usual) */
		gpencil_draw_apply(op, p);
	}
	RNA_END;
	
	//printf("\tGP - done \n");
	
	/* cleanup */
	gpencil_draw_exit(C, op);
	
	/* refreshes */
	WM_event_add_notifier(C, NC_SCREEN|ND_GPENCIL|NA_EDITED, NULL); // XXX need a nicer one that will work	
	
	/* done */
	return OPERATOR_FINISHED;
}

/* ------------------------------- */

/* start of interactive drawing part of operator */
static int gpencil_draw_invoke (bContext *C, wmOperator *op, wmEvent *event)
{
	tGPsdata *p = NULL;
	wmWindow *win= CTX_wm_window(C);
	
	if (G.f & G_DEBUG)
		printf("GPencil - Starting Drawing \n");
	
	/* try to initialize context data needed while drawing */
	if (!gpencil_draw_init(C, op)) {
		if (op->customdata) 
			MEM_freeN(op->customdata);
		if (G.f & G_DEBUG)
			printf("\tGP - no valid data \n");
		return OPERATOR_CANCELLED;
	}
	else
		p= op->customdata;
	
	// TODO: set any additional settings that we can take from the events?
	// TODO? if tablet is erasing, force eraser to be on?
	
	// TODO: move cursor setting stuff to stroke-start so that paintmode can be changed midway...
	
	/* if eraser is on, draw radial aid */
	if (p->paintmode == GP_PAINTMODE_ERASER) {
		// TODO: this involves mucking around with radial control, so we leave this for now..
	}
	
	/* set cursor */
	if (p->paintmode == GP_PAINTMODE_ERASER)
		WM_cursor_modal(win, BC_CROSSCURSOR); // XXX need a better cursor
	else
		WM_cursor_modal(win, BC_PAINTBRUSHCURSOR);
	
	/* special hack: if there was an initial event, then we were invoked via a hotkey, and 
	 * painting should start immediately. Otherwise, this was called from a toolbar, in which
	 * case we should wait for the mouse to be clicked.
	 */
	if (event->type) {
		/* hotkey invoked - start drawing */
		//printf("\tGP - set first spot\n");
		p->status= GP_STATUS_PAINTING;
		
		/* handle the initial drawing - i.e. for just doing a simple dot */
		gpencil_draw_apply_event(op, event);
	}
	else {
		/* toolbar invoked - don't start drawing yet... */
		//printf("\tGP - hotkey invoked... waiting for click-drag\n");
	}
	
	WM_event_add_notifier(C, NC_SCREEN|ND_GPENCIL, NULL);
	/* add a modal handler for this operator, so that we can then draw continuous strokes */
	WM_event_add_modal_handler(C, op);
	return OPERATOR_RUNNING_MODAL;
}

/* gpencil modal operator stores area, which can be removed while using it (like fullscreen) */
static int gpencil_area_exists(bContext *C, ScrArea *satest)
{
	bScreen *sc= CTX_wm_screen(C);
	ScrArea *sa;
	
	for (sa= sc->areabase.first; sa; sa= sa->next) {
		if (sa==satest)
			return 1;
	}
	
	return 0;
}

static tGPsdata *gpencil_stroke_begin(bContext *C, wmOperator *op)
{
	tGPsdata *p= op->customdata;

	/* we must check that we're still within the area that we're set up to work from
	 * otherwise we could crash (see bug #20586)
	 */
	if (CTX_wm_area(C) != p->sa) {
		printf("\t\t\tGP - wrong area execution abort! \n");
		p->status= GP_STATUS_ERROR;
	}

	//printf("\t\tGP - start stroke \n");

	/* we may need to set up paint env again if we're resuming */
	// XXX: watch it with the paintmode! in future, it'd be nice to allow changing paint-mode when in sketching-sessions
	// XXX: with tablet events, we may event want to check for eraser here, for nicer tablet support

	if (gp_session_initdata(C, p))
		gp_paint_initstroke(p, p->paintmode);

	if(p->status != GP_STATUS_ERROR)
		p->status= GP_STATUS_PAINTING;

	return op->customdata;
}

static void gpencil_stroke_end(wmOperator *op)
{
	tGPsdata *p= op->customdata;

	gp_paint_cleanup(p);

	gpencil_undo_push(p->gpd);

	gp_session_cleanup(p);

	p->status= GP_STATUS_IDLING;

	p->gpd= NULL;
	p->gpl= NULL;
	p->gpf= NULL;
}

/* events handling during interactive drawing part of operator */
static int gpencil_draw_modal (bContext *C, wmOperator *op, wmEvent *event)
{
	tGPsdata *p= op->customdata;
	int estate = OPERATOR_PASS_THROUGH; /* default exit state - not handled, so let others have a share of the pie */
	
	// if (event->type == NDOF_MOTION)
	//	return OPERATOR_PASS_THROUGH;
	// -------------------------------
	// [mce] Not quite what I was looking
	// for, but a good start! GP continues to
	// draw on the screen while the 3D mouse
	// moves the viewpoint. Problem is that
	// the stroke is converted to 3D only after
	// it is finished. This approach should work
	// better in tools that immediately apply
	// in 3D space.

	//printf("\tGP - handle modal event...\n");
	
	/* exit painting mode (and/or end current stroke) */
	if (ELEM4(event->type, RETKEY, PADENTER, ESCKEY, SPACEKEY)) {
		/* exit() ends the current stroke before cleaning up */
		//printf("\t\tGP - end of paint op + end of stroke\n");
		p->status= GP_STATUS_DONE;
		estate = OPERATOR_FINISHED;
	}
	
	/* toggle painting mode upon mouse-button movement */
	if (ELEM(event->type, LEFTMOUSE, RIGHTMOUSE)) {
		/* if painting, end stroke */
		if (p->status == GP_STATUS_PAINTING) {
			int sketch= 0;
			/* basically, this should be mouse-button up = end stroke 
			 * BUT what happens next depends on whether we 'painting sessions' is enabled
			 */
			sketch |= GPENCIL_SKETCH_SESSIONS_ON(p->scene);
			/* polyline drawing is also 'sketching' -- all knots should be added during one session */
			sketch |= p->paintmode == GP_PAINTMODE_DRAW_POLY;

			if (sketch) {
				/* end stroke only, and then wait to resume painting soon */
				//printf("\t\tGP - end stroke only\n");
				gpencil_stroke_end(op);
				
				/* we've just entered idling state, so this event was processed (but no others yet) */
				estate = OPERATOR_RUNNING_MODAL;

				/* stroke could be smoothed, send notifier to refresh screen */
				WM_event_add_notifier(C, NC_SCREEN|ND_GPENCIL|NA_EDITED, NULL);
			}
			else {
				//printf("\t\tGP - end of stroke + op\n");
				p->status= GP_STATUS_DONE;
				estate = OPERATOR_FINISHED;
			}
		}
		else if (event->val == KM_PRESS) {
			/* not painting, so start stroke (this should be mouse-button down) */
			p= gpencil_stroke_begin(C, op);

			if (p->status == GP_STATUS_ERROR) {
				estate = OPERATOR_CANCELLED;
			}
		} 
		else {
			p->status = GP_STATUS_IDLING;
		}
	}
	
	/* handle mode-specific events */
	if (p->status == GP_STATUS_PAINTING) {
		/* handle painting mouse-movements? */
		if (ELEM(event->type, MOUSEMOVE, INBETWEEN_MOUSEMOVE) || (p->flags & GP_PAINTFLAG_FIRSTRUN)) 
		{
			/* handle drawing event */
			//printf("\t\tGP - add point\n");
			gpencil_draw_apply_event(op, event);
			
			/* finish painting operation if anything went wrong just now */
			if (p->status == GP_STATUS_ERROR) {
				printf("\t\t\t\tGP - add error done! \n");
				estate = OPERATOR_CANCELLED;
			}
			else {
				/* event handled, so just tag as running modal */
				//printf("\t\t\t\tGP - add point handled!\n");
				estate = OPERATOR_RUNNING_MODAL;
			}
		}
		/* there shouldn't be any other events, but just in case there are, let's swallow them 
		 * (i.e. to prevent problems with with undo)
		 */
		else {
			/* swallow event to save ourselves trouble */
			estate = OPERATOR_RUNNING_MODAL;
		}
	}
	
	/* gpencil modal operator stores area, which can be removed while using it (like fullscreen) */
	if (0==gpencil_area_exists(C, p->sa))
		estate= OPERATOR_CANCELLED;
	else
		/* update status indicators - cursor, header, etc. */
		gpencil_draw_status_indicators(p);
	
	/* process last operations before exiting */
	switch (estate) {
		case OPERATOR_FINISHED:
			/* one last flush before we're done */
			gpencil_draw_exit(C, op);
			WM_event_add_notifier(C, NC_SCREEN|ND_GPENCIL|NA_EDITED, NULL); // XXX need a nicer one that will work
			break;
			
		case OPERATOR_CANCELLED:
			gpencil_draw_exit(C, op);
			break;

		case OPERATOR_RUNNING_MODAL|OPERATOR_PASS_THROUGH:
			/* event doesn't need to be handled */
			//printf("unhandled event -> %d (mmb? = %d | mmv? = %d)\n", event->type, event->type == MIDDLEMOUSE, event->type==MOUSEMOVE);
			break;
	}
	
	/* return status code */
	return estate;
}

/* ------------------------------- */

static EnumPropertyItem prop_gpencil_drawmodes[] = {
	{GP_PAINTMODE_DRAW, "DRAW", 0, "Draw Freehand", ""},
	{GP_PAINTMODE_DRAW_STRAIGHT, "DRAW_STRAIGHT", 0, "Draw Straight Lines", ""},
	{GP_PAINTMODE_DRAW_POLY, "DRAW_POLY", 0, "Draw Poly Line", ""},
	{GP_PAINTMODE_ERASER, "ERASER", 0, "Eraser", ""},
	{0, NULL, 0, NULL, NULL}
};

void GPENCIL_OT_draw (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Grease Pencil Draw";
	ot->idname= "GPENCIL_OT_draw";
	ot->description= "Make annotations on the active data";
	
	/* api callbacks */
	ot->exec= gpencil_draw_exec;
	ot->invoke= gpencil_draw_invoke;
	ot->modal= gpencil_draw_modal;
	ot->cancel= gpencil_draw_cancel;
	ot->poll= gpencil_draw_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;
	
	/* settings for drawing */
	RNA_def_enum(ot->srna, "mode", prop_gpencil_drawmodes, 0, "Mode", "Way to interpret mouse movements");
	
	RNA_def_collection_runtime(ot->srna, "stroke", &RNA_OperatorStrokeElement, "Stroke", "");
}
