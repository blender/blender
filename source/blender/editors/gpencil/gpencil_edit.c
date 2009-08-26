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
 * The Original Code is Copyright (C) 2008, Blender Foundation
 * This is a new part of Blender
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
#if 0 // XXX COMPILE GUARDS FOR OLD CODE
 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"

#include "DNA_listBase.h"
#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_vec_types.h"
#include "DNA_view3d_types.h"

#include "BKE_armature.h"
#include "BKE_blender.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_utildefines.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_view2d.h"

#include "ED_armature.h"
#include "ED_gpencil.h"
#include "ED_sequencer.h"
#include "ED_view3d.h"

#include "PIL_time.h"			/* sleep				*/

#include "gpencil_intern.h"

/* XXX */
static void BIF_undo_push() {}
static void error() {}
static int pupmenu() {return 0;}
static void add_object_draw() {}
static int get_activedevice() {return 0;}
#define L_MOUSE 0
#define R_MOUSE 0

/* ************************************************** */
/* XXX - OLD DEPRECEATED CODE... */

/* ----------- GP-Datablock API ------------- */

/* get the appropriate bGPdata from the active/given context */
// XXX region or region data?
bGPdata *gpencil_data_getactive (ScrArea *sa)
{
	ScrArea *curarea= NULL; // XXX
	
	/* error checking */
	if ((sa == NULL) && (curarea == NULL))
		return NULL;
	if (sa == NULL)
		sa= curarea;
		
	/* handle depending on spacetype */
	switch (sa->spacetype) {
		case SPACE_VIEW3D:
		{
			View3D *v3d= sa->spacedata.first;
			return v3d->gpd;
		}
			break;
		case SPACE_NODE:
		{
			SpaceNode *snode= sa->spacedata.first;
			return snode->gpd;
		}
			break;
		case SPACE_SEQ:
		{
			SpaceSeq *sseq= sa->spacedata.first;
			
			/* only applicable for image modes */
			if (sseq->mainb != SEQ_DRAW_SEQUENCE)
				return sseq->gpd;
		}
			break;
		case SPACE_IMAGE:
		{
			SpaceImage *sima= sa->spacedata.first;
			return sima->gpd;
		}
			break;
	}
	
	/* nothing found */
	return NULL;
}

/* set bGPdata for the active/given context, and return success/fail */
short gpencil_data_setactive (ScrArea *sa, bGPdata *gpd)
{
	ScrArea *curarea= NULL; // XXX
	
	/* error checking */
	if ((sa == NULL) && (curarea == NULL))
		return 0;
	if (gpd == NULL)
		return 0;
	if (sa == NULL)
		sa= curarea;
	
	/* handle depending on spacetype */
	// TODO: someday we should have multi-user data, so no need to loose old data
	switch (sa->spacetype) {
		case SPACE_VIEW3D:
		{
			View3D *v3d= sa->spacedata.first;
			
			/* free the existing block */
			if (v3d->gpd)
				free_gpencil_data(v3d->gpd);
			v3d->gpd= gpd;
			
			return 1;
		}
			break;
		case SPACE_NODE:
		{
			SpaceNode *snode= sa->spacedata.first;
			
			/* free the existing block */
			if (snode->gpd)
				free_gpencil_data(snode->gpd);
			snode->gpd= gpd;
			
			/* set special settings */
			gpd->flag |= GP_DATA_VIEWALIGN;
			
			return 1;
		}
			break;
		case SPACE_SEQ:
		{
			SpaceSeq *sseq= sa->spacedata.first;
			
			/* only applicable if right mode */
			if (sseq->mainb != SEQ_DRAW_SEQUENCE) {
				/* free the existing block */
				if (sseq->gpd)
					free_gpencil_data(sseq->gpd);
				sseq->gpd= gpd;
				
				return 1;
			}
		}
			break;
		case SPACE_IMAGE:
		{
			SpaceImage *sima= sa->spacedata.first;
			
			if (sima->gpd)
				free_gpencil_data(sima->gpd);
			sima->gpd= gpd;
			
			return 1;
		}
			break;
	}
	
	/* failed to add */
	return 0;
}

/* return the ScrArea that has the given GP-datablock
 *	- assumes that only searching in current screen
 *	- is based on GP-datablocks only being able to 
 * 	  exist for one area at a time (i.e. not multiuser)
 */
ScrArea *gpencil_data_findowner (bGPdata *gpd)
{
	bScreen *curscreen= NULL; // XXX
	ScrArea *sa;
	
	/* error checking */
	if (gpd == NULL)
		return NULL;
		
	/* loop over all scrareas for current screen, and check if that area has this gpd */
	for (sa= curscreen->areabase.first; sa; sa= sa->next) {
		/* use get-active func to see if match */
		if (gpencil_data_getactive(sa) == gpd)
			return sa;
	}
	
	/* not found */
	return NULL;
}

/* ************************************************** */
/* GREASE-PENCIL EDITING - Tools */

/* --------- Data Deletion ---------- */

/* delete the last stroke on the active layer */
void gpencil_delete_laststroke (bGPdata *gpd, int cfra)
{
	bGPDlayer *gpl= gpencil_layer_getactive(gpd);
	bGPDframe *gpf= gpencil_layer_getframe(gpl, cfra, 0);
	
	gpencil_frame_delete_laststroke(gpl, gpf);
}

/* delete the active frame */
void gpencil_delete_actframe (bGPdata *gpd, int cfra)
{
	bGPDlayer *gpl= gpencil_layer_getactive(gpd);
	bGPDframe *gpf= gpencil_layer_getframe(gpl, cfra, 0);
	
	gpencil_layer_delframe(gpl, gpf);
}



/* delete various grase-pencil elements 
 *	mode: 	1 - last stroke
 *		 	2 - active frame
 *			3 - active layer
 */
void gpencil_delete_operation (int cfra, short mode)
{
	bGPdata *gpd;
	
	/* get datablock to work on */
	gpd= gpencil_data_getactive(NULL);
	if (gpd == NULL) return;
	
	switch (mode) {
		case 1: /* last stroke */
			gpencil_delete_laststroke(gpd, cfra);
			break;
		case 2: /* active frame */
			gpencil_delete_actframe(gpd, cfra);
			break;
		case 3: /* active layer */
			gpencil_layer_delactive(gpd);
			break;
	}
	
	/* redraw and undo-push */
	BIF_undo_push("GPencil Delete");
}

/* display a menu for deleting different grease-pencil elements */
void gpencil_delete_menu (void)
{
	bGPdata *gpd= gpencil_data_getactive(NULL);
	int cfra= 0; // XXX
	short mode;
	
	/* only show menu if it will be relevant */
	if (gpd == NULL) return;
	
	mode= pupmenu("Grease Pencil Erase...%t|Last Stroke%x1|Active Frame%x2|Active Layer%x3");
	if (mode <= 0) return;
	
	gpencil_delete_operation(cfra, mode);
}

/* --------- Data Conversion ---------- */

/* convert the coordinates from the given stroke point into 3d-coordinates */
static void gp_strokepoint_convertcoords (bGPDstroke *gps, bGPDspoint *pt, float p3d[3])
{
	ARegion *ar= NULL; // XXX
	
	if (gps->flag & GP_STROKE_3DSPACE) {
		/* directly use 3d-coordinates */
		VecCopyf(p3d, &pt->x);
	}
	else {
		short mval[2];
		int mx=0, my=0;
		float *fp= give_cursor(NULL, NULL); // XXX should be scene, v3d
		float dvec[3];
		
		/* get screen coordinate */
		if (gps->flag & GP_STROKE_2DSPACE) {
			// XXX
			// View2D *v2d= spacelink_get_view2d(curarea->spacedata.first);
			// UI_view2d_view_to_region(v2d, pt->x, pt->y, &mx, &my);
		}
		else {
			// XXX
			// mx= (short)(pt->x / 1000 * curarea->winx);
			// my= (short)(pt->y / 1000 * curarea->winy);
		}
		
		/* convert screen coordinate to 3d coordinates 
		 *	- method taken from editview.c - mouse_cursor() 
		 */
		project_short_noclip(ar, fp, mval);
		window_to_3d_delta(ar, dvec, mval[0]-mx, mval[1]-my);
		VecSubf(p3d, fp, dvec);
	}
}

/* --- */

/* convert stroke to 3d path */
static void gp_stroke_to_path (bGPDlayer *gpl, bGPDstroke *gps, Curve *cu)
{
	bGPDspoint *pt;
	Nurb *nu;
	BPoint *bp;
	int i;
	
	/* create new 'nurb' within the curve */
	nu = (Nurb *)MEM_callocN(sizeof(Nurb), "gpstroke_to_path(nurb)");
	
	nu->pntsu= gps->totpoints;
	nu->pntsv= 1;
	nu->orderu= gps->totpoints;
	nu->flagu= 2;	/* endpoint */
	nu->resolu= 32;
	
	nu->bp= (BPoint *)MEM_callocN(sizeof(BPoint)*gps->totpoints, "bpoints");
	
	/* add points */
	for (i=0, pt=gps->points, bp=nu->bp; i < gps->totpoints; i++, pt++, bp++) {
		float p3d[3];
		
		/* get coordinates to add at */
		gp_strokepoint_convertcoords(gps, pt, p3d);
		VecCopyf(bp->vec, p3d);
		
		/* set settings */
		bp->f1= SELECT;
		bp->radius = bp->weight = pt->pressure * gpl->thickness;
	}
	
	/* add nurb to curve */
	BLI_addtail(&cu->nurb, nu);
}

/* convert stroke to 3d bezier */
static void gp_stroke_to_bezier (bGPDlayer *gpl, bGPDstroke *gps, Curve *cu)
{
	bGPDspoint *pt;
	Nurb *nu;
	BezTriple *bezt;
	int i;
	
	/* create new 'nurb' within the curve */
	nu = (Nurb *)MEM_callocN(sizeof(Nurb), "gpstroke_to_bezier(nurb)");
	
	nu->pntsu= gps->totpoints;
	nu->resolu= 12;
	nu->resolv= 12;
	nu->type= CU_BEZIER;
	nu->bezt = (BezTriple *)MEM_callocN(gps->totpoints*sizeof(BezTriple), "bezts");
	
	/* add points */
	for (i=0, pt=gps->points, bezt=nu->bezt; i < gps->totpoints; i++, pt++, bezt++) {
		float p3d[3];
		
		/* get coordinates to add at */
		gp_strokepoint_convertcoords(gps, pt, p3d);
		
		/* TODO: maybe in future the handles shouldn't be in same place */
		VecCopyf(bezt->vec[0], p3d);
		VecCopyf(bezt->vec[1], p3d);
		VecCopyf(bezt->vec[2], p3d);
		
		/* set settings */
		bezt->h1= bezt->h2= HD_FREE;
		bezt->f1= bezt->f2= bezt->f3= SELECT;
		bezt->radius = bezt->weight = pt->pressure * gpl->thickness * 0.1f;
	}
	
	/* must calculate handles or else we crash */
	calchandlesNurb(nu);
	
	/* add nurb to curve */
	BLI_addtail(&cu->nurb, nu);
}

/* convert a given grease-pencil layer to a 3d-curve representation (using current view if appropriate) */
static void gp_layer_to_curve (bGPdata *gpd, bGPDlayer *gpl, Scene *scene, short mode)
{
	bGPDframe *gpf= gpencil_layer_getframe(gpl, scene->r.cfra, 0);
	bGPDstroke *gps;
	Object *ob;
	Curve *cu;
	
	/* error checking */
	if (ELEM3(NULL, gpd, gpl, gpf))
		return;
		
	/* only convert if there are any strokes on this layer's frame to convert */
	if (gpf->strokes.first == NULL)
		return;
	
	/* init the curve object (remove rotation and get curve data from it)
	 *	- must clear transforms set on object, as those skew our results
	 */
	add_object_draw(OB_CURVE);
	ob= OBACT;
	ob->loc[0]= ob->loc[1]= ob->loc[2]= 0;
	ob->rot[0]= ob->rot[1]= ob->rot[2]= 0;
	cu= ob->data;
	cu->flag |= CU_3D;
	
	/* rename object and curve to layer name */
	rename_id((ID *)ob, gpl->info);
	rename_id((ID *)cu, gpl->info);
	
	/* add points to curve */
	for (gps= gpf->strokes.first; gps; gps= gps->next) {
		switch (mode) {
			case 1: 
				gp_stroke_to_path(gpl, gps, cu);
				break;
			case 2:
				gp_stroke_to_bezier(gpl, gps, cu);
				break;
		}
	}
}

/* --- */

/* convert a stroke to a bone chain */
static void gp_stroke_to_bonechain (bGPDlayer *gpl, bGPDstroke *gps, bArmature *arm, ListBase *bones)
{
	EditBone *ebo, *prev=NULL;
	bGPDspoint *pt, *ptn;
	int i;
	
	/* add each segment separately */
	for (i=0, pt=gps->points, ptn=gps->points+1; i < (gps->totpoints-1); prev=ebo, i++, pt++, ptn++) {
		float p3da[3], p3db[3];
		
		/* get coordinates to add at */
		gp_strokepoint_convertcoords(gps, pt, p3da);
		gp_strokepoint_convertcoords(gps, ptn, p3db);
		
		/* allocate new bone */
		ebo= MEM_callocN(sizeof(EditBone), "eBone");
		
		VecCopyf(ebo->head, p3da);
		VecCopyf(ebo->tail, p3db);
		
		/* add new bone - note: sync with editarmature.c::add_editbone() */
		{
			BLI_strncpy(ebo->name, "Stroke", 32);
			unique_editbone_name(bones, ebo->name, NULL);
			
			BLI_addtail(bones, ebo);
			
			if (i > 0)
			{
				ebo->flag |= BONE_CONNECTED;
			}
			ebo->weight= 1.0f;
			ebo->dist= 0.25f;
			ebo->xwidth= 0.1f;
			ebo->zwidth= 0.1f;
			ebo->ease1= 1.0f;
			ebo->ease2= 1.0f;
			ebo->rad_head= pt->pressure * gpl->thickness * 0.1f;
			ebo->rad_tail= ptn->pressure * gpl->thickness * 0.1f;
			ebo->segments= 1;
			ebo->layer= arm->layer;
		}
		
		/* set parenting */
		ebo->parent= prev;
	}
}

/* convert a given grease-pencil layer to a 3d-curve representation (using current view if appropriate) */
// XXX depreceated... we now have etch-a-ton for this...
static void gp_layer_to_armature (bGPdata *gpd, bGPDlayer *gpl, Scene *scene, View3D *v3d, short mode)
{
	bGPDframe *gpf= gpencil_layer_getframe(gpl, scene->r.cfra, 0);
	bGPDstroke *gps;
	Object *ob;
	bArmature *arm;
	
	/* error checking */
	if (ELEM3(NULL, gpd, gpl, gpf))
		return;
		
	/* only convert if there are any strokes on this layer's frame to convert */
	if (gpf->strokes.first == NULL)
		return;
	
	/* init the armature object (remove rotation and assign armature data to it) 
	 *	- must clear transforms set on object, as those skew our results
	 */
	add_object_draw(OB_ARMATURE);
	ob= OBACT;
	ob->loc[0]= ob->loc[1]= ob->loc[2]= 0;
	ob->rot[0]= ob->rot[1]= ob->rot[2]= 0;
	arm= ob->data;
	
	/* rename object and armature to layer name */
	rename_id((ID *)ob, gpl->info);
	rename_id((ID *)arm, gpl->info);
	
	/* this is editmode armature */
	arm->edbo= MEM_callocN(sizeof(ListBase), "arm edbo");
	
	/* convert segments to bones, strokes to bone chains */
	for (gps= gpf->strokes.first; gps; gps= gps->next) {
		gp_stroke_to_bonechain(gpl, gps, arm, arm->edbo);
	}
	
	/* adjust roll of bones
	 * 	- set object as EditMode object, but need to clear afterwards!
	 *	- use 'align to world z-up' option
	 */
	{
		/* set our data as if we're in editmode to fool auto_align_armature() */
		scene->obedit= ob;
		
		/* WARNING: need to make sure this magic number doesn't change */
		auto_align_armature(scene, v3d, 2);	
		
		scene->obedit= NULL;
	}
	
	/* flush editbones to armature */
	ED_armature_from_edit(scene, ob);
	ED_armature_edit_free(ob);
}

/* --- */

/* convert grease-pencil strokes to another representation 
 *	mode: 	1 - Active layer to path
 *			2 - Active layer to bezier
 *			3 - Active layer to armature
 */
void gpencil_convert_operation (short mode)
{
	Scene *scene= NULL; // XXX
	View3D *v3d= NULL; // XXX
	RegionView3D *rv3d= NULL; // XXX
	bGPdata *gpd;	
	float *fp= give_cursor(scene, v3d);
	
	/* get datablock to work on */
	gpd= gpencil_data_getactive(NULL);
	if (gpd == NULL) return;
	
	/* initialise 3d-cursor correction globals */
	initgrabz(rv3d, fp[0], fp[1], fp[2]);
	
	/* handle selection modes */
	switch (mode) {
		case 1: /* active layer only (to path) */
		case 2: /* active layer only (to bezier) */
		{
			bGPDlayer *gpl= gpencil_layer_getactive(gpd);
			gp_layer_to_curve(gpd, gpl, scene, mode);
		}
			break;
		case 3: /* active layer only (to armature) */
		{
			bGPDlayer *gpl= gpencil_layer_getactive(gpd);
			gp_layer_to_armature(gpd, gpl, scene, v3d, mode);
		}
			break;
	}
	
	/* redraw and undo-push */
	BIF_undo_push("GPencil Convert");
}

/* ************************************************** */
/* GREASE-PENCIL EDITING MODE - Painting */

/* ---------- 'Globals' and Defines ----------------- */

/* maximum sizes of gp-session buffer */
#define GP_STROKE_BUFFER_MAX	5000

/* Macros for accessing sensitivity thresholds... */
	/* minimum number of pixels mouse should move before new point created */
#define MIN_MANHATTEN_PX	(U.gp_manhattendist)
	/* minimum length of new segment before new point can be added */
#define MIN_EUCLIDEAN_PX	(U.gp_euclideandist)

/* macro to test if only converting endpoints - only for use when converting!  */	
#define GP_BUFFER2STROKE_ENDPOINTS ((gpd->flag & GP_DATA_EDITPAINT) && (ctrl))
	
/* ------ */

/* Temporary 'Stroke' Operation data */
typedef struct tGPsdata {
	Scene *scene;       /* current scene from context */
	ScrArea *sa;		/* area where painting originated */
	ARegion *ar;        /* region where painting originated */
	View2D *v2d;		/* needed for GP_STROKE_2DSPACE */
	
	ImBuf *ibuf;		/* needed for GP_STROKE_2DIMAGE */
	struct IBufViewSettings {
		int offsx, offsy;			/* offsets */
		int sizex, sizey;			/* dimensions to use as scale-factor */
	} im2d_settings;	/* needed for GP_STROKE_2DIMAGE */
	
	bGPdata *gpd;		/* gp-datablock layer comes from */
	bGPDlayer *gpl;		/* layer we're working on */
	bGPDframe *gpf;		/* frame we're working on */
	
	short status;		/* current status of painting */
	short paintmode;	/* mode for painting */
	
	short mval[2];		/* current mouse-position */
	short mvalo[2];		/* previous recorded mouse-position */
	
	float pressure;		/* current stylus pressure */
	float opressure;	/* previous stylus pressure */
	
	short radius;		/* radius of influence for eraser */
} tGPsdata;

/* values for tGPsdata->status */
enum {
	GP_STATUS_NORMAL = 0,	/* running normally */
	GP_STATUS_ERROR,		/* something wasn't correctly set up */
	GP_STATUS_DONE			/* painting done */
};

/* values for tGPsdata->paintmode */
enum {
	GP_PAINTMODE_DRAW = 0,
	GP_PAINTMODE_ERASER
};

/* Return flags for adding points to stroke buffer */
enum {
	GP_STROKEADD_INVALID	= -2,		/* error occurred - insufficient info to do so */
	GP_STROKEADD_OVERFLOW	= -1,		/* error occurred - cannot fit any more points */
	GP_STROKEADD_NORMAL,				/* point was successfully added */
	GP_STROKEADD_FULL					/* cannot add any more points to buffer */
};

/* ---------- Stroke Editing ------------ */

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

/* check if the current mouse position is suitable for adding a new point */
static short gp_stroke_filtermval (tGPsdata *p, short mval[2], short pmval[2])
{
	short dx= abs(mval[0] - pmval[0]);
	short dy= abs(mval[1] - pmval[1]);
	
	/* check if mouse moved at least certain distance on both axes (best case) */
	if ((dx > MIN_MANHATTEN_PX) && (dy > MIN_MANHATTEN_PX))
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
static void gp_stroke_convertcoords (tGPsdata *p, short mval[], float out[])
{
	bGPdata *gpd= p->gpd;
	
	/* in 3d-space - pt->x/y/z are 3 side-by-side floats */
	if (gpd->sbuffer_sflag & GP_STROKE_3DSPACE) {
		const short mx=mval[0], my=mval[1];
		float *fp= give_cursor(p->scene, NULL); // XXX NULL could be v3d
		float dvec[3];
		
		/* Current method just converts each point in screen-coordinates to 
		 * 3D-coordinates using the 3D-cursor as reference. In general, this 
		 * works OK, but it could of course be improved.
		 *
		 * TODO:
		 *	- investigate using nearest point(s) on a previous stroke as
		 *	  reference point instead or as offset, for easier stroke matching
		 *	- investigate projection onto geometry (ala retopo)
		 */
		
		/* method taken from editview.c - mouse_cursor() */
		project_short_noclip(p->ar, fp, mval);
		window_to_3d_delta(p->ar, dvec, mval[0]-mx, mval[1]-my);
		VecSubf(out, fp, dvec);
	}
	
	/* 2d - on 'canvas' (assume that p->v2d is set) */
	else if ((gpd->sbuffer_sflag & GP_STROKE_2DSPACE) && (p->v2d)) {
		float x, y;
		
		UI_view2d_region_to_view(p->v2d, mval[0], mval[1], &x, &y);
		
		out[0]= x;
		out[1]= y;
	}
	
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
	
	/* 2d - relative to screen (viewport area) */
	else {
		out[0] = (float)(mval[0]) / (float)(p->sa->winx) * 1000;
		out[1] = (float)(mval[1]) / (float)(p->sa->winy) * 1000;
	}
}

/* add current stroke-point to buffer (returns whether point was successfully added) */
static short gp_stroke_addpoint (tGPsdata *p, short mval[2], float pressure)
{
	bGPdata *gpd= p->gpd;
	tGPspoint *pt;
	
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

/* smooth a stroke (in buffer) before storing it */
static void gp_stroke_smooth (tGPsdata *p)
{
	bGPdata *gpd= p->gpd;
	int i=0, cmx=gpd->sbuffer_size;
	int ctrl= 0; // XXX
	
	/* only smooth if smoothing is enabled, and we're not doing a straight line */
	if (!(U.gp_settings & GP_PAINT_DOSMOOTH) || GP_BUFFER2STROKE_ENDPOINTS)
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
	int ctrl= 0; // XXX
	
	/* only simplify if simlification is enabled, and we're not doing a straight line */
	if (!(U.gp_settings & GP_PAINT_DOSIMPLIFY) || GP_BUFFER2STROKE_ENDPOINTS)
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
			short mco[2];
			
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
			mco[0]= (short)co[0];
			mco[1]= (short)co[1];
			
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
	int ctrl= 0; // XXX
	
	/* get total number of points to allocate space for:
	 *	- in 'Draw Mode', holding the Ctrl-Modifier will only take endpoints
	 *	- otherwise, do whole stroke
	 */
	if (GP_BUFFER2STROKE_ENDPOINTS)
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
	if (GP_BUFFER2STROKE_ENDPOINTS) {
		/* 'Draw Mode' + Ctrl-Modifier - only endpoints */
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
static short gp_stroke_eraser_strokeinside (short mval[], short mvalo[], short rad, short x0, short y0, short x1, short y1)
{
	/* simple within-radius check for now */
	if (edge_inside_circle(mval[0], mval[1], rad, x0, y0, x1, y1))
		return 1;
	
	/* not inside */
	return 0;
} 

/* eraser tool - evaluation per stroke */
static void gp_stroke_eraser_dostroke (tGPsdata *p, short mval[], short mvalo[], short rad, rcti *rect, bGPDframe *gpf, bGPDstroke *gps)
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
		else if (gps->flag & GP_STROKE_2DIMAGE) {			
			int offsx, offsy, sizex, sizey;
			
			/* get stored settings */
			sizex= p->im2d_settings.sizex;
			sizey= p->im2d_settings.sizey;
			offsx= p->im2d_settings.offsx;
			offsy= p->im2d_settings.offsy;
			
			/* calculate new points */
			x0= (short)((gps->points->x * sizex) + offsx);
			y0= (short)((gps->points->y * sizey) + offsy);
		}
		else {
			x0= (short)(gps->points->x / 1000 * p->sa->winx);
			y0= (short)(gps->points->y / 1000 * p->sa->winy);
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
			else if (gps->flag & GP_STROKE_2DIMAGE) {
				int offsx, offsy, sizex, sizey;
				
				/* get stored settings */
				sizex= p->im2d_settings.sizex;
				sizey= p->im2d_settings.sizey;
				offsx= p->im2d_settings.offsx;
				offsy= p->im2d_settings.offsy;
				
				/* calculate new points */
				x0= (short)((pt1->x * sizex) + offsx);
				y0= (short)((pt1->y * sizey) + offsy);
				
				x1= (short)((pt2->x * sizex) + offsx);
				y1= (short)((pt2->y * sizey) + offsy);
			}
			else {
				x0= (short)(pt1->x / 1000 * p->sa->winx);
				y0= (short)(pt1->y / 1000 * p->sa->winy);
				x1= (short)(pt2->x / 1000 * p->sa->winx);
				y1= (short)(pt2->y / 1000 * p->sa->winy);
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

/* ---------- 'Paint' Tool ------------ */

/* init new painting session */
static void gp_session_initpaint (bContext *C, tGPsdata *p)
{
	ScrArea *curarea= CTX_wm_area(C);
	ARegion *ar= CTX_wm_region(C);
	
	/* clear previous data (note: is on stack) */
	memset(p, 0, sizeof(tGPsdata));
	
	/* make sure the active view (at the starting time) is a 3d-view */
	if (curarea == NULL) {
		p->status= GP_STATUS_ERROR;
		if (G.f & G_DEBUG) 
			printf("Error: No active view for painting \n");
		return;
	}
	
	/* pass on current scene */
	p->scene= CTX_data_scene(C);
	
	switch (curarea->spacetype) {
		/* supported views first */
		case SPACE_VIEW3D:
		{
			View3D *v3d= curarea->spacedata.first;
			
			/* set current area */
			p->sa= curarea;
			p->ar= ar;
			
			/* check that gpencil data is allowed to be drawn */
			if ((v3d->flag2 & V3D_DISPGP)==0) {
				p->status= GP_STATUS_ERROR;
				if (G.f & G_DEBUG) 
					printf("Error: In active view, Grease Pencil not shown \n");
				return;
			}
		}
			break;
		case SPACE_NODE:
		{
			SpaceNode *snode= curarea->spacedata.first;
			
			/* set current area */
			p->sa= curarea;
			p->ar= ar;
			p->v2d= &ar->v2d;
			
			/* check that gpencil data is allowed to be drawn */
			if ((snode->flag & SNODE_DISPGP)==0) {
				p->status= GP_STATUS_ERROR;
				if (G.f & G_DEBUG) 
					printf("Error: In active view, Grease Pencil not shown \n");
				return;
			}
		}
			break;
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
		case SPACE_IMAGE:
		{
			SpaceImage *sima= curarea->spacedata.first;
			
			/* set the current area */
			p->sa= curarea;
			p->ar= ar;
			p->v2d= &ar->v2d;
			p->ibuf= BKE_image_get_ibuf(sima->image, &sima->iuser);
			
			/* check that gpencil data is allowed to be drawn */
			if ((sima->flag & SI_DISPGP)==0) {
				p->status= GP_STATUS_ERROR;
				if (G.f & G_DEBUG)
					printf("Error: In active view, Grease Pencil not shown \n");
				return;
			}
		}
			break;
		/* unsupported views */
		default:
		{
			p->status= GP_STATUS_ERROR;
			if (G.f & G_DEBUG) 
				printf("Error: Active view not appropriate for Grease Pencil drawing \n");
			return;
		}
			break;
	}
	
	/* get gp-data */
	p->gpd= gpencil_data_getactive(p->sa);
	if (p->gpd == NULL) {
		short ok;
		
		p->gpd= gpencil_data_addnew("GPencil");
		ok= gpencil_data_setactive(p->sa, p->gpd);
		
		/* most of the time, the following check isn't needed */
		if (ok == 0) {
			/* free gpencil data as it can't be used */
			free_gpencil_data(p->gpd);
			p->gpd= NULL;
			p->status= GP_STATUS_ERROR;
			if (G.f & G_DEBUG) 
				printf("Error: Could not assign newly created Grease Pencil data to active area \n");
			return;
		}
	}
	
	/* set edit flags */
	G.f |= G_GREASEPENCIL;
	
	/* clear out buffer (stored in gp-data) in case something contaminated it */
	gp_session_validatebuffer(p);
	
	/* set 'default' im2d_settings just in case something that uses this doesn't set it */
	p->im2d_settings.sizex= 1;
	p->im2d_settings.sizey= 1;
}

/* cleanup after a painting session */
static void gp_session_cleanup (tGPsdata *p)
{
	bGPdata *gpd= p->gpd;
	
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
				View3D *v3d= (View3D *)p->sa->spacedata.first;
				RegionView3D *rv3d= NULL; // XXX
				float *fp= give_cursor(p->scene, v3d);
				
				initgrabz(rv3d, fp[0], fp[1], fp[2]);
				
				p->gpd->sbuffer_sflag |= GP_STROKE_3DSPACE;
			}
				break;
			case SPACE_NODE:
			{
				p->gpd->sbuffer_sflag |= GP_STROKE_2DSPACE;
			}
				break;
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
			case SPACE_IMAGE:
			{
				/* check if any ibuf available */
				if (p->ibuf)
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
	
	/* add undo-push so stroke can be undone */
	/* FIXME: currently disabled, as it's impossible to get this working nice
	 * as gpenci data is on currently screen-level (which isn't saved to undo files)
	 */
	//BIF_undo_push("GPencil Stroke");
	
	/* force redraw after drawing action */
	// XXX force_draw_plus(SPACE_ACTION, 0);
}

/* -------- */

/* main call to paint a new stroke */
// XXX will become modal(), gets event, includes all info!
short gpencil_paint (bContext *C, short paintmode)
{
	tGPsdata p;
	short ok = GP_STROKEADD_NORMAL;
	
	/* init paint-data */
	gp_session_initpaint(C, &p);
	if (p.status == GP_STATUS_ERROR) {
		gp_session_cleanup(&p);
		return 0;
	}
	gp_paint_initstroke(&p, paintmode);
	if (p.status == GP_STATUS_ERROR) {
		gp_session_cleanup(&p);
		return 0;
	}
	
	/* set cursor to indicate drawing */
	// XXX (cursor callbacks in regiontype) setcursor_space(p.sa->spacetype, CURSOR_VPAINT);
	
	/* init drawing-device settings */
	// XXX getmouseco_areawin(p.mval);
	// XXX p.pressure = get_pressure();
	
	p.mvalo[0]= p.mval[0];
	p.mvalo[1]= p.mval[1];
	p.opressure= p.pressure;
	
	/* radius for eraser circle is defined in userprefs now */
	// TODO: make this more easily tweaked... 
	p.radius= U.gp_eraser;
	
	/* start drawing eraser-circle (if applicable) */
	//if (paintmode == GP_PAINTMODE_ERASER)
	// XXX	draw_sel_circle(p.mval, NULL, p.radius, p.radius, 0); // draws frontbuffer, but sets backbuf again
	
	/* only allow painting of single 'dots' if: 
	 *	- pressure is not excessive (as it can be on some windows tablets)
	 *	- draw-mode for active datablock is turned on
	 * 	- not erasing
	 */
	if (paintmode != GP_PAINTMODE_ERASER) {
		if (!(p.pressure >= 0.99f) || (p.gpd->flag & GP_DATA_EDITPAINT)) { 
			gp_stroke_addpoint(&p, p.mval, p.pressure);
		}
	}
	
	/* XXX paint loop */
	if(0) {
		/* get current user input */
		// XXX getmouseco_areawin(p.mval);
		// XXX p.pressure = get_pressure();
		
		/* only add current point to buffer if mouse moved (otherwise wait until it does) */
		if (paintmode == GP_PAINTMODE_ERASER) {
			/* do 'live' erasing now */
			gp_stroke_doeraser(&p);
			
			// XXX draw_sel_circle(p.mval, p.mvalo, p.radius, p.radius, 0);
			// XXX force_draw(0);
			
			p.mvalo[0]= p.mval[0];
			p.mvalo[1]= p.mval[1];
			p.opressure= p.pressure;
		}
		else if (gp_stroke_filtermval(&p, p.mval, p.mvalo)) {
			/* try to add point */
			ok= gp_stroke_addpoint(&p, p.mval, p.pressure);
			
			/* handle errors while adding point */
			if ((ok == GP_STROKEADD_FULL) || (ok == GP_STROKEADD_OVERFLOW)) {
				/* finish off old stroke */
				gp_paint_strokeend(&p);
				
				/* start a new stroke, starting from previous point */
				gp_stroke_addpoint(&p, p.mvalo, p.opressure);
				ok= gp_stroke_addpoint(&p, p.mval, p.pressure);
			}
			else if (ok == GP_STROKEADD_INVALID) {
				/* the painting operation cannot continue... */
				error("Cannot paint stroke");
				p.status = GP_STATUS_ERROR;
				
				if (G.f & G_DEBUG) 
					printf("Error: Grease-Pencil Paint - Add Point Invalid \n");
				// XXX break;
			}
			// XXX force_draw(0);
			
			p.mvalo[0]= p.mval[0];
			p.mvalo[1]= p.mval[1];
			p.opressure= p.pressure;
		}
		
		/* do mouse checking at the end, so don't check twice, and potentially
		 * miss a short tap 
		 */
	}
	
	/* clear edit flags */
	G.f &= ~G_GREASEPENCIL;
	
	/* restore cursor to indicate end of drawing */
	// XXX  (cursor callbacks in regiontype) setcursor_space(p.sa->spacetype, CURSOR_STD);
	
	/* check size of buffer before cleanup, to determine if anything happened here */
	if (paintmode == GP_PAINTMODE_ERASER) {
		ok= 1; /* assume that we did something... */
		// XXX draw_sel_circle(NULL, p.mvalo, 0, p.radius, 0);
	}
	else
		ok= p.gpd->sbuffer_size;
	
	/* cleanup */
	gp_paint_cleanup(&p);
	gp_session_cleanup(&p);
	
	/* done! return if a stroke was successfully added */
	return ok;
}


/* All event (loops) handling checking if stroke drawing should be initiated
 * should call this function.
 */
short gpencil_do_paint (bContext *C)
{
	ScrArea *sa= CTX_wm_area(C);
	bGPdata *gpd = gpencil_data_getactive(sa);
	short retval= 0;
	int alt= 0, shift= 0, mbut= 0; // XXX
	
	/* check if possible to do painting */
	if (gpd == NULL) 
		return 0;
	
	/* currently, we will only 'paint' if:
	 * 	1. draw-mode on gpd is set (for accessibility reasons)
	 *		a) single dots are only available by this method if a single click is made
	 *		b) a straight line is drawn if ctrl-modifier is held (check is done when stroke is converted!)
	 *	2. if shift-modifier is held + lmb -> 'quick paint'
	 *
	 *	OR
	 * 
	 * draw eraser stroke if:
	 *	1. using the eraser on a tablet
	 *	2. draw-mode on gpd is set (for accessiblity reasons)
	 *		(eraser is mapped to right-mouse)
	 *	3. Alt + 'select' mouse-button
	 *		i.e.  if LMB = select: Alt-LMB
	 *			  if RMB = select: Alt-RMB
	 */
	if (get_activedevice() == 2) {
		/* eraser on a tablet - always try to erase strokes */
		retval = gpencil_paint(C, GP_PAINTMODE_ERASER);
	}
	else if (gpd->flag & GP_DATA_EDITPAINT) {
		/* try to paint/erase */
		if (mbut == L_MOUSE)
			retval = gpencil_paint(C, GP_PAINTMODE_DRAW);
		else if (mbut == R_MOUSE)
			retval = gpencil_paint(C, GP_PAINTMODE_ERASER);
	}
	else if (!(gpd->flag & GP_DATA_LMBPLOCK)) {
		/* try to paint/erase as not locked */
		if (shift && (mbut == L_MOUSE)) {
			retval = gpencil_paint(C, GP_PAINTMODE_DRAW);
		}
		else if (alt) {
			if ((U.flag & USER_LMOUSESELECT) && (mbut == L_MOUSE))
				retval = gpencil_paint(C, GP_PAINTMODE_ERASER);
			else if (!(U.flag & USER_LMOUSESELECT) && (mbut == R_MOUSE))
				retval = gpencil_paint(C, GP_PAINTMODE_ERASER);
		}
	}
	
	/* return result of trying to paint */
	return retval;
}

/* ************************************************** */
#endif // XXX COMPILE GUARDS FOR OLD CODE
