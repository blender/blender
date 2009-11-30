/**
 * $Id$
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2008, Blender Foundation, Joshua Leung
 * This is a new part of Blender
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "BKE_gpencil.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_report.h"
#include "BKE_utildefines.h"

#include "DNA_gpencil_types.h"
#include "DNA_action_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view2d_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"

#include "UI_view2d.h"

#include "ED_armature.h"
#include "ED_gpencil.h"
#include "ED_sequencer.h"
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
	ScrArea *sa;		/* area where painting originated */
	ARegion *ar;        /* region where painting originated */
	View2D *v2d;		/* needed for GP_STROKE_2DSPACE */
	
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
	/* check if current context can support GPencil data */
	return (gpencil_data_get_pointers(C, NULL) != NULL);
}

/* ******************************************* */
/* Calculations/Conversions */

/* Utilities --------------------------------- */

/* get the reference point for stroke-point conversions */
static void gp_get_3d_reference (tGPsdata *p, float *vec)
{
	View3D *v3d= p->sa->spacedata.first;
	float *fp= give_cursor(p->scene, v3d);
	
	/* the reference point used depends on the owner... */
	if (p->ownerPtr.type == &RNA_Object) {
		Object *ob= (Object *)p->ownerPtr.data;
		
		/* active Object 
		 * 	- use relative distance of 3D-cursor from object center 
		 */
		sub_v3_v3v3(vec, fp, ob->loc);
	}
	else {
		/* use 3D-cursor */
		copy_v3_v3(vec, fp);
	}
}

/* Stroke Editing ---------------------------- */

/* check if the current mouse position is suitable for adding a new point */
static short gp_stroke_filtermval (tGPsdata *p, int mval[2], int pmval[2])
{
	int dx= abs(mval[0] - pmval[0]);
	int dy= abs(mval[1] - pmval[1]);
	
	/* if buffer is empty, just let this go through (i.e. so that dots will work) */
	if (p->gpd->sbuffer_size == 0)
		return 1;
	
	/* check if mouse moved at least certain distance on both axes (best case) */
	else if ((dx > MIN_MANHATTEN_PX) && (dy > MIN_MANHATTEN_PX))
		return 1;
	
	/* check if the distance since the last point is significant enough */
	// future optimisation: sqrt here may be too slow?
	else if (sqrt(dx*dx + dy*dy) > MIN_EUCLIDEAN_PX)
		return 1;
	
	/* mouse 'didn't move' */
	else
		return 0;
}

/* convert screen-coordinates to buffer-coordinates */
// XXX this method needs a total overhaul!
static void gp_stroke_convertcoords (tGPsdata *p, short mval[], float out[])
{
	bGPdata *gpd= p->gpd;
	
	/* in 3d-space - pt->x/y/z are 3 side-by-side floats */
	if (gpd->sbuffer_sflag & GP_STROKE_3DSPACE) {
		const short mx=mval[0], my=mval[1];
		float rvec[3], dvec[3];
		
		/* Current method just converts each point in screen-coordinates to 
		 * 3D-coordinates using the 3D-cursor as reference. In general, this 
		 * works OK, but it could of course be improved.
		 *
		 * TODO:
		 *	- investigate using nearest point(s) on a previous stroke as
		 *	  reference point instead or as offset, for easier stroke matching
		 *	- investigate projection onto geometry (ala retopo)
		 */
		gp_get_3d_reference(p, rvec);
		
		/* method taken from editview.c - mouse_cursor() */
		project_short_noclip(p->ar, rvec, mval);
		window_to_3d_delta(p->ar, dvec, mval[0]-mx, mval[1]-my);
		sub_v3_v3v3(out, rvec, dvec);
	}
	
	/* 2d - on 'canvas' (assume that p->v2d is set) */
	else if ((gpd->sbuffer_sflag & GP_STROKE_2DSPACE) && (p->v2d)) {
		float x, y;
		
		UI_view2d_region_to_view(p->v2d, mval[0], mval[1], &x, &y);
		
		out[0]= x;
		out[1]= y;
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
		out[0] = (float)(mval[0]) / (float)(p->ar->winx) * 100;
		out[1] = (float)(mval[1]) / (float)(p->ar->winy) * 100;
	}
}

/* add current stroke-point to buffer (returns whether point was successfully added) */
static short gp_stroke_addpoint (tGPsdata *p, int mval[2], float pressure)
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
			pt->x= mval[0];
			pt->y= mval[1];
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
			pt->x= mval[0];
			pt->y= mval[1];
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
		pt->x= mval[0];
		pt->y= mval[1];
		pt->pressure= pressure;
		
		/* increment counters */
		gpd->sbuffer_size++;
		
		/* check if another operation can still occur */
		if (gpd->sbuffer_size == GP_STROKE_BUFFER_MAX)
			return GP_STROKEADD_FULL;
		else
			return GP_STROKEADD_NORMAL;
	}
	
	/* return invalid state for now... */
	return GP_STROKEADD_INVALID;
}

/* smooth a stroke (in buffer) before storing it */
static void gp_stroke_smooth (tGPsdata *p)
{
	bGPdata *gpd= p->gpd;
	int i=0, cmx=gpd->sbuffer_size;
	
	/* only smooth if smoothing is enabled, and we're not doing a straight line */
	if (!(U.gp_settings & GP_PAINT_DOSMOOTH) || (p->paintmode == GP_PAINTMODE_DRAW_STRAIGHT))
		return;
	
	/* don't try if less than 2 points in buffer */
	if ((cmx <= 2) || (gpd->sbuffer == NULL))
		return;
	
	/* apply weighting-average (note doing this along path sequentially does introduce slight error) */
	for (i=0; i < gpd->sbuffer_size; i++) {
		tGPspoint *pc= (((tGPspoint *)gpd->sbuffer) + i);
		tGPspoint *pb= (i-1 > 0)?(pc-1):(pc);
		tGPspoint *pa= (i-2 > 0)?(pc-2):(pb);
		tGPspoint *pd= (i+1 < cmx)?(pc+1):(pc);
		tGPspoint *pe= (i+2 < cmx)?(pc+2):(pd);
		
		pc->x= (short)(0.1*pa->x + 0.2*pb->x + 0.4*pc->x + 0.2*pd->x + 0.1*pe->x);
		pc->y= (short)(0.1*pa->y + 0.2*pb->y + 0.4*pc->y + 0.2*pd->y + 0.1*pe->y);
	}
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
	
	/* only simplify if simlification is enabled, and we're not doing a straight line */
	if (!(U.gp_settings & GP_PAINT_DOSIMPLIFY) || (p->paintmode == GP_PAINTMODE_DRAW_STRAIGHT))
		return;
	
	/* don't simplify if less than 4 points in buffer */
	if ((num_points <= 2) || (old_points == NULL))
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
			
			/* initialise values */
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
	
	/* allocate memory for a new stroke */
	gps= MEM_callocN(sizeof(bGPDstroke), "gp_stroke");
	
	/* allocate enough memory for a continuous array for storage points */
	pt= gps->points= MEM_callocN(sizeof(bGPDspoint)*totelem, "gp_stroke_points");
	
	/* copy appropriate settings for stroke */
	gps->totpoints= totelem;
	gps->thickness= p->gpl->thickness;
	gps->flag= gpd->sbuffer_sflag;
	
	/* copy points from the buffer to the stroke */
	if (p->paintmode == GP_PAINTMODE_DRAW_STRAIGHT) {
		/* straight lines only -> only endpoints */
		{
			/* first point */
			ptc= gpd->sbuffer;
			
			/* convert screen-coordinates to appropriate coordinates (and store them) */
			gp_stroke_convertcoords(p, &ptc->x, &pt->x);
			
			/* copy pressure */
			pt->pressure= ptc->pressure;
			
			pt++;
		}
			
		if (totelem == 2) {
			/* last point if applicable */
			ptc= ((tGPspoint *)gpd->sbuffer) + (gpd->sbuffer_size - 1);
			
			/* convert screen-coordinates to appropriate coordinates (and store them) */
			gp_stroke_convertcoords(p, &ptc->x, &pt->x);
			
			/* copy pressure */
			pt->pressure= ptc->pressure;
		}
	}
	else {
		/* convert all points (normal behaviour) */
		for (i=0, ptc=gpd->sbuffer; i < gpd->sbuffer_size && ptc; i++, ptc++) {
			/* convert screen-coordinates to appropriate coordinates (and store them) */
			gp_stroke_convertcoords(p, &ptc->x, &pt->x);
			
			/* copy pressure */
			pt->pressure= ptc->pressure;
			
			pt++;
		}
	}
	
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
static short gp_stroke_eraser_strokeinside (int mval[], int mvalo[], short rad, short x0, short y0, short x1, short y1)
{
	/* simple within-radius check for now */
	if (edge_inside_circle(mval[0], mval[1], rad, x0, y0, x1, y1))
		return 1;
	
	/* not inside */
	return 0;
} 

/* eraser tool - evaluation per stroke */
// TODO: this could really do with some optimisation (KD-Tree/BVH?)
static void gp_stroke_eraser_dostroke (tGPsdata *p, int mval[], int mvalo[], short rad, rcti *rect, bGPDframe *gpf, bGPDstroke *gps)
{
	bGPDspoint *pt1, *pt2;
	int x0=0, y0=0, x1=0, y1=0;
	short xyval[2];
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
			project_short(p->ar, &gps->points->x, xyval);
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
			x0= (int)(gps->points->x / 100 * p->ar->winx);
			y0= (int)(gps->points->y / 100 * p->ar->winy);
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
				project_short(p->ar, &pt1->x, xyval);
				x0= xyval[0];
				y0= xyval[1];
				
				project_short(p->ar, &pt2->x, xyval);
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
				x0= (int)(pt1->x / 100 * p->ar->winx);
				y0= (int)(pt1->y / 100 * p->ar->winy);
				x1= (int)(pt2->x / 100 * p->ar->winx);
				y1= (int)(pt2->y / 100 * p->ar->winy);
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
	if (gpd->sbuffer)
		memset(gpd->sbuffer, 0, sizeof(tGPspoint)*GP_STROKE_BUFFER_MAX);
	else
		gpd->sbuffer= MEM_callocN(sizeof(tGPspoint)*GP_STROKE_BUFFER_MAX, "gp_session_strokebuffer");
	
	/* reset indices */
	gpd->sbuffer_size = 0;
	
	/* reset flags */
	gpd->sbuffer_sflag= 0;
}

/* init new painting session */
static tGPsdata *gp_session_initpaint (bContext *C)
{
	tGPsdata *p = NULL;
	bGPdata **gpd_ptr = NULL;
	ScrArea *curarea= CTX_wm_area(C);
	ARegion *ar= CTX_wm_region(C);
	
	/* make sure the active view (at the starting time) is a 3d-view */
	if (curarea == NULL) {
		if (G.f & G_DEBUG) 
			printf("Error: No active view for painting \n");
		return NULL;
	}
	
	/* create new context data */
	p= MEM_callocN(sizeof(tGPsdata), "GPencil Drawing Data");
	
	/* pass on current scene */
	p->scene= CTX_data_scene(C);
	
	switch (curarea->spacetype) {
		/* supported views first */
		case SPACE_VIEW3D:
		{
			//View3D *v3d= curarea->spacedata.first;
			
			/* set current area 
			 *	- must verify that region data is 3D-view (and not something else)
			 */
			p->sa= curarea;
			p->ar= ar;
			
			if (ar->regiondata == NULL) {
				p->status= GP_STATUS_ERROR;
				if (G.f & G_DEBUG)
					printf("Error: 3D-View active region doesn't have any region data, so cannot be drawable \n");
				return p;
			}
			
#if 0 // XXX will this sort of antiquated stuff be restored?
			/* check that gpencil data is allowed to be drawn */
			if ((v3d->flag2 & V3D_DISPGP)==0) {
				p->status= GP_STATUS_ERROR;
				if (G.f & G_DEBUG) 
					printf("Error: In active view, Grease Pencil not shown \n");
				return p;
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
				return;
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
				return;
			}
			if ((sseq->flag & SEQ_DRAW_GPENCIL)==0) {
				p->status= GP_STATUS_ERROR;
				if (G.f & G_DEBUG) 
					printf("Error: In active view, Grease Pencil not shown \n");
				return;
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
				return p;
			}
#endif
		}
			break;

		/* unsupported views */
		default:
		{
			p->status= GP_STATUS_ERROR;
			if (G.f & G_DEBUG) 
				printf("Error: Active view not appropriate for Grease Pencil drawing \n");
			return p;
		}
			break;
	}
	
	/* get gp-data */
	gpd_ptr= gpencil_data_get_pointers(C, &p->ownerPtr);
	if (gpd_ptr == NULL) {
		p->status= GP_STATUS_ERROR;
		if (G.f & G_DEBUG)
			printf("Error: Current context doesn't allow for any Grease Pencil data \n");
		return p;
	}
	else {
		/* if no existing GPencil block exists, add one */
		if (*gpd_ptr == NULL)
			*gpd_ptr= gpencil_data_addnew("GPencil");
		p->gpd= *gpd_ptr;
	}
	
	/* set edit flags - so that buffer will get drawn */
	G.f |= G_GREASEPENCIL;
	
	/* set initial run flag */
	p->flags |= GP_PAINTFLAG_FIRSTRUN;
	
	/* clear out buffer (stored in gp-data), in case something contaminated it */
	gp_session_validatebuffer(p);
	
#if 0
	/* set 'default' im2d_settings just in case something that uses this doesn't set it */
	p->im2d_settings.sizex= 1;
	p->im2d_settings.sizey= 1;
#endif
	
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
	if (p->gpl == NULL)
		p->gpl= gpencil_layer_addnew(p->gpd);
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
	
	/* check if points will need to be made in view-aligned space */
	if (p->gpd->flag & GP_DATA_VIEWALIGN) {
		switch (p->sa->spacetype) {
			case SPACE_VIEW3D:
			{
				RegionView3D *rv3d= p->ar->regiondata;
				float rvec[3];
				
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
					zoomx = zoom * ((float)p->scene->r.xasp / (float)p->scene->r.yasp);
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
				p->gpd->sbuffer_sflag |= GP_STROKE_2DSPACE;
			}
				break;
		}
	}
}

/* finish off a stroke (clears buffer, but doesn't finish the paint operation) */
static void gp_paint_strokeend (tGPsdata *p)
{
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
	/* finish off a stroke */
	gp_paint_strokeend(p);
	
	/* "unlock" frame */
	p->gpf->flag &= ~GP_FRAME_PAINT;
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
		gp_session_cleanup(p);
		return 0;
	}
	
	/* init painting data */
	gp_paint_initstroke(p, paintmode);
	if (p->status == GP_STATUS_ERROR) {
		gp_session_cleanup(p);
		return 0;
	}
	
	/* radius for eraser circle is defined in userprefs now */
	p->radius= U.gp_eraser;
	
	/* everything is now setup ok */
	return 1;
}

/* ------------------------------- */

static void gpencil_draw_exit (bContext *C, wmOperator *op)
{
	tGPsdata *p= op->customdata;
	
	/* clear edit flags */
	G.f &= ~G_GREASEPENCIL;
	
	/* restore cursor to indicate end of drawing */
	WM_cursor_restore(CTX_wm_window(C));
	
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
	op->customdata= NULL;
}

static int gpencil_draw_cancel (bContext *C, wmOperator *op)
{
	/* this is just a wrapper around exit() */
	gpencil_draw_exit(C, op);
	return OPERATOR_CANCELLED;
}

/* ------------------------------- */

/* create a new stroke point at the point indicated by the painting context */
static void gpencil_draw_apply (bContext *C, wmOperator *op, tGPsdata *p)
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
			ok= gp_stroke_addpoint(p, p->mval, p->pressure);
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
static void gpencil_draw_apply_event (bContext *C, wmOperator *op, wmEvent *event)
{
	tGPsdata *p= op->customdata;
	ARegion *ar= p->ar;
	//PointerRNA itemptr;
	//float mousef[2];
	int tablet=0;

	/* convert from window-space to area-space mouse coordintes */
	// NOTE: float to ints conversions, +1 factor is probably used to ensure a bit more accurate rounding...
	p->mval[0]= event->x - ar->winrct.xmin + 1;
	p->mval[1]= event->y - ar->winrct.ymin + 1;
	
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
	
#if 0 // NOTE: disabled for now, since creating this data is currently useless anyways (and slows things down)
	/* fill in stroke data (not actually used directly by gpencil_draw_apply) */
	RNA_collection_add(op->ptr, "stroke", &itemptr);

	mousef[0]= p->mval[0];
	mousef[1]= p->mval[1];
	RNA_float_set_array(&itemptr, "mouse", mousef);
	RNA_float_set(&itemptr, "pressure", p->pressure);
#endif 
	
	/* apply the current latest drawing point */
	gpencil_draw_apply(C, op, p);
	
	/* force refresh */
	ED_region_tag_redraw(p->ar); /* just active area for now, since doing whole screen is too slow */
}

/* ------------------------------- */

/* operator 'redo' (i.e. after changing some properties) */
static int gpencil_draw_exec (bContext *C, wmOperator *op)
{
	tGPsdata *p = NULL;
	
	//printf("GPencil - Starting Re-Drawing \n");
	
	/* try to initialise context data needed while drawing */
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
		p->mval[0] = (short)mousef[0];
		p->mval[1] = (short)mousef[1];
		p->pressure= RNA_float_get(&itemptr, "pressure");
		
		/* if first run, set previous data too */
		if (p->flags & GP_PAINTFLAG_FIRSTRUN) {
			p->flags &= ~GP_PAINTFLAG_FIRSTRUN;
			
			p->mvalo[0]= p->mval[0];
			p->mvalo[1]= p->mval[1];
			p->opressure= p->pressure;
		}
		
		/* apply this data as necessary now (as per usual) */
		gpencil_draw_apply(C, op, p);
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
	
	/* try to initialise context data needed while drawing */
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
		gpencil_draw_apply_event(C, op, event);
	}
	else {
		/* toolbar invoked - don't start drawing yet... */
		//printf("\tGP - hotkey invoked... waiting for click-drag\n");
	}
	
	/* add a modal handler for this operator, so that we can then draw continuous strokes */
	WM_event_add_modal_handler(C, op);
	return OPERATOR_RUNNING_MODAL;
}

/* events handling during interactive drawing part of operator */
static int gpencil_draw_modal (bContext *C, wmOperator *op, wmEvent *event)
{
	tGPsdata *p= op->customdata;
	
	//printf("\tGP - handle modal event...\n");
	
	switch (event->type) {
		/* end of stroke -> ONLY when a mouse-button release occurs 
		 * otherwise, carry on to mouse-move...
		 */
		case LEFTMOUSE:
		case RIGHTMOUSE: 
			/* if painting, end stroke */
			if (p->status == GP_STATUS_PAINTING) {
				/* basically, this should be mouse-button up */
				//printf("\t\tGP - end of stroke \n");
				gpencil_draw_exit(C, op);
				
				/* one last flush before we're done */
				WM_event_add_notifier(C, NC_SCREEN|ND_GPENCIL|NA_EDITED, NULL); // XXX need a nicer one that will work	
				
				return OPERATOR_FINISHED;
			}
			else {
				/* not painting, so start stroke (this should be mouse-button down) */
				//printf("\t\tGP - start stroke \n");
				p->status= GP_STATUS_PAINTING;
				/* no break now, since we should immediately start painting */
			}
		
		/* moving mouse - assumed that mouse button is down if in painting status */
		case MOUSEMOVE:
			/* check if we're currently painting */
			if (p->status == GP_STATUS_PAINTING) {
				/* handle drawing event */
				//printf("\t\tGP - add point\n");
				gpencil_draw_apply_event(C, op, event);
				
				/* finish painting operation if anything went wrong just now */
				if (p->status == GP_STATUS_ERROR) {
					//printf("\t\t\tGP - error done! \n");
					gpencil_draw_exit(C, op);
					return OPERATOR_CANCELLED;
				}
			}
			break;
		
		default:
			//printf("\t\tGP unknown event - %d \n", event->type);
			break;
	}
	
	return OPERATOR_RUNNING_MODAL;
}

/* ------------------------------- */

static EnumPropertyItem prop_gpencil_drawmodes[] = {
	{GP_PAINTMODE_DRAW, "DRAW", 0, "Draw Freehand", ""},
	{GP_PAINTMODE_DRAW_STRAIGHT, "DRAW_STRAIGHT", 0, "Draw Straight Lines", ""},
	{GP_PAINTMODE_ERASER, "ERASER", 0, "Eraser", ""},
	{0, NULL, 0, NULL, NULL}
};

void GPENCIL_OT_draw (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Grease Pencil Draw";
	ot->idname= "GPENCIL_OT_draw";
	ot->description= "Make annotations on the active data.";
	
	/* api callbacks */
	ot->exec= gpencil_draw_exec;
	ot->invoke= gpencil_draw_invoke;
	ot->modal= gpencil_draw_modal;
	ot->cancel= gpencil_draw_cancel;
	ot->poll= gpencil_draw_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;
	
	/* settings for drawing */
	RNA_def_enum(ot->srna, "mode", prop_gpencil_drawmodes, 0, "Mode", "Way to intepret mouse movements.");
		// xxx the stuff below is used only for redo operator, but is not really working
	RNA_def_collection_runtime(ot->srna, "stroke", &RNA_OperatorStrokeElement, "Stroke", "");
}
