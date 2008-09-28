/**
 * $Id: gpencil.c 14881 2008-05-18 10:41:42Z aligorith $
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
 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "BMF_Api.h"

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

#include "BKE_global.h"
#include "BKE_utildefines.h"
#include "BKE_blender.h"
#include "BKE_armature.h"
#include "BKE_curve.h"
#include "BKE_image.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BIF_butspace.h"
#include "BIF_drawseq.h"
#include "BIF_editarmature.h"
#include "BIF_editview.h"
#include "BIF_graphics.h"
#include "BIF_interface.h"
#include "BIF_mywindow.h"
#include "BIF_resources.h"
#include "BIF_space.h"
#include "BIF_screen.h"
#include "BIF_toolbox.h"
#include "BIF_toets.h"

#include "BDR_gpencil.h"
#include "BIF_drawgpencil.h"

#include "BDR_editobject.h"

#include "BSE_drawipo.h"
#include "BSE_headerbuttons.h"
#include "BSE_view.h"

#include "blendef.h"

#include "PIL_time.h"			/* sleep				*/
#include "mydevice.h"

/* ************************************************** */
/* GENERAL STUFF */

/* --------- Memory Management ------------ */

/* Free strokes belonging to a gp-frame */
void free_gpencil_strokes (bGPDframe *gpf)
{
	bGPDstroke *gps, *gpsn;
	
	/* error checking */
	if (gpf == NULL) return;
	
	/* free strokes */
	for (gps= gpf->strokes.first; gps; gps= gpsn) {
		gpsn= gps->next;
		
		/* free stroke memory arrays, then stroke itself */
		MEM_freeN(gps->points);
		BLI_freelinkN(&gpf->strokes, gps);
	}
}

/* Free all of a gp-layer's frames */
void free_gpencil_frames (bGPDlayer *gpl)
{
	bGPDframe *gpf, *gpfn;
	
	/* error checking */
	if (gpl == NULL) return;
	
	/* free frames */
	for (gpf= gpl->frames.first; gpf; gpf= gpfn) {
		gpfn= gpf->next;
		
		/* free strokes and their associated memory */
		free_gpencil_strokes(gpf);
		BLI_freelinkN(&gpl->frames, gpf);
	}
}

/* Free all of the gp-layers for a viewport (list should be &G.vd->gpd or so) */
void free_gpencil_layers (ListBase *list) 
{
	bGPDlayer *gpl, *gpln;
	
	/* error checking */
	if (list == NULL) return;
	
	/* delete layers*/
	for (gpl= list->first; gpl; gpl= gpln) {
		gpln= gpl->next;
		
		/* free layers and their data */
		free_gpencil_frames(gpl);
		BLI_freelinkN(list, gpl);
	}
}

/* Free gp-data and all it's related data */
void free_gpencil_data (bGPdata *gpd)
{
	/* free layers then data itself */
	free_gpencil_layers(&gpd->layers);
	MEM_freeN(gpd);
}

/* -------- Container Creation ---------- */

/* add a new gp-frame to the given layer */
bGPDframe *gpencil_frame_addnew (bGPDlayer *gpl, int cframe)
{
	bGPDframe *gpf, *gf;
	short state=0;
	
	/* error checking */
	if ((gpl == NULL) || (cframe <= 0))
		return NULL;
		
	/* allocate memory for this frame */
	gpf= MEM_callocN(sizeof(bGPDframe), "bGPDframe");
	gpf->framenum= cframe;
	
	/* find appropriate place to add frame */
	if (gpl->frames.first) {
		for (gf= gpl->frames.first; gf; gf= gf->next) {
			/* check if frame matches one that is supposed to be added */
			if (gf->framenum == cframe) {
				state= -1;
				break;
			}
			
			/* if current frame has already exceeded the frame to add, add before */
			if (gf->framenum > cframe) {
				BLI_insertlinkbefore(&gpl->frames, gf, gpf);
				state= 1;
				break;
			}
		}
	}
	
	/* check whether frame was added successfully */
	if (state == -1) {
		MEM_freeN(gpf);
		printf("Error: frame (%d) existed already for this layer \n", cframe);
	}
	else if (state == 0) {
		/* add to end then! */
		BLI_addtail(&gpl->frames, gpf);
	}
	
	/* return frame */
	return gpf;
}

/* add a new gp-layer and make it the active layer */
bGPDlayer *gpencil_layer_addnew (bGPdata *gpd)
{
	bGPDlayer *gpl;
	
	/* check that list is ok */
	if (gpd == NULL)
		return NULL;
		
	/* allocate memory for frame and add to end of list */
	gpl= MEM_callocN(sizeof(bGPDlayer), "bGPDlayer");
	
	/* add to datablock */
	BLI_addtail(&gpd->layers, gpl);
	
	/* set basic settings */
	gpl->color[3]= 0.9f;
	gpl->thickness = 3;
	
	/* auto-name */
	sprintf(gpl->info, "GP_Layer");
	BLI_uniquename(&gpd->layers, gpl, "GP_Layer", offsetof(bGPDlayer, info[0]), 128);
	
	/* make this one the active one */
	gpencil_layer_setactive(gpd, gpl);
	
	/* return layer */
	return gpl;
}

/* add a new gp-datablock */
bGPdata *gpencil_data_addnew (void)
{
	bGPdata *gpd;
	
	/* allocate memory for a new block */
	gpd= MEM_callocN(sizeof(bGPdata), "GreasePencilData");
	
	/* initial settings */
	gpd->flag = (GP_DATA_DISPINFO|GP_DATA_EXPAND);
	
	return gpd;
}

/* -------- Data Duplication ---------- */

/* make a copy of a given gpencil datablock */
bGPdata *gpencil_data_duplicate (bGPdata *src)
{
	bGPdata *dst;
	bGPDlayer *gpld, *gpls;
	bGPDframe *gpfd, *gpfs;
	bGPDstroke *gps;
	
	/* error checking */
	if (src == NULL)
		return NULL;
	
	/* make a copy of the base-data */
	dst= MEM_dupallocN(src);
	
	/* copy layers */
	duplicatelist(&dst->layers, &src->layers);
	
	for (gpld=dst->layers.first, gpls=src->layers.first; gpld && gpls; 
		 gpld=gpld->next, gpls=gpls->next) 
	{
		/* copy frames */
		duplicatelist(&gpld->frames, &gpls->frames);
		
		for (gpfd=gpld->frames.first, gpfs=gpls->frames.first; gpfd && gpfs;
			 gpfd=gpfd->next, gpfs=gpfs->next) 
		{
			/* copy strokes */
			duplicatelist(&gpfd->strokes, &gpfs->strokes);
			
			for (gps= gpfd->strokes.first; gps; gps= gps->next) 
			{
				gps->points= MEM_dupallocN(gps->points);
			}
		}
	}
	
	/* return new */
	return dst;
}

/* ----------- GP-Datablock API ------------- */

/* get the appropriate bGPdata from the active/given context */
bGPdata *gpencil_data_getactive (ScrArea *sa)
{
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
			if (sseq->mainb)
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
			if (sseq->mainb) {
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

/* -------- GP-Frame API ---------- */

/* delete the last stroke of the given frame */
void gpencil_frame_delete_laststroke (bGPDframe *gpf)
{
	bGPDstroke *gps= (gpf) ? gpf->strokes.last : NULL;
	
	/* error checking */
	if (ELEM(NULL, gpf, gps))
		return;
	
	/* free the stroke and its data */
	MEM_freeN(gps->points);
	BLI_freelinkN(&gpf->strokes, gps);
}

/* -------- GP-Layer API ---------- */

/* get the appropriate gp-frame from a given layer
 *	- this sets the layer's actframe var (if allowed to)
 *	- extension beyond range (if first gp-frame is after all frame in interest and cannot add)
 */
bGPDframe *gpencil_layer_getframe (bGPDlayer *gpl, int cframe, short addnew)
{
	bGPDframe *gpf = NULL;
	short found = 0;
	
	/* error checking */
	if (gpl == NULL) return NULL;
	if (cframe <= 0) cframe = 1;
	
	/* check if there is already an active frame */
	if (gpl->actframe) {
		gpf= gpl->actframe;
		
		/* do not allow any changes to layer's active frame if layer is locked */
		if (gpl->flag & GP_LAYER_LOCKED)
			return gpf;
		/* do not allow any changes to actframe if frame has painting tag attached to it */
		if (gpf->flag & GP_FRAME_PAINT) 
			return gpf;
		
		/* try to find matching frame */
		if (gpf->framenum < cframe) {
			for (; gpf; gpf= gpf->next) {
				if (gpf->framenum == cframe) {
					found= 1;
					break;
				}
				else if ((gpf->next) && (gpf->next->framenum > cframe)) {
					found= 1;
					break;
				}
			}
			
			/* set the appropriate frame */
			if (addnew) {
				if ((found) && (gpf->framenum == cframe))
					gpl->actframe= gpf;
				else
					gpl->actframe= gpencil_frame_addnew(gpl, cframe);
			}
			else if (found)
				gpl->actframe= gpf;
			else
				gpl->actframe= gpl->frames.last;
		}
		else {
			for (; gpf; gpf= gpf->prev) {
				if (gpf->framenum <= cframe) {
					found= 1;
					break;
				}
			}
			
			/* set the appropriate frame */
			if (addnew) {
				if ((found) && (gpf->framenum == cframe))
					gpl->actframe= gpf;
				else
					gpl->actframe= gpencil_frame_addnew(gpl, cframe);
			}
			else if (found)
				gpl->actframe= gpf;
			else
				gpl->actframe= gpl->frames.first;
		}
	}
	else if (gpl->frames.first) {
		/* check which of the ends to start checking from */
		const int first= ((bGPDframe *)(gpl->frames.first))->framenum;
		const int last= ((bGPDframe *)(gpl->frames.last))->framenum;
		
		if (abs(cframe-first) > abs(cframe-last)) {
			/* find gp-frame which is less than or equal to cframe */
			for (gpf= gpl->frames.last; gpf; gpf= gpf->prev) {
				if (gpf->framenum <= cframe) {
					found= 1;
					break;
				}
			}
		}
		else {
			/* find gp-frame which is less than or equal to cframe */
			for (gpf= gpl->frames.first; gpf; gpf= gpf->next) {
				if (gpf->framenum <= cframe) {
					found= 1;
					break;
				}
			}
		}
		
		/* set the appropriate frame */
		if (addnew) {
			if ((found) && (gpf->framenum == cframe))
				gpl->actframe= gpf;
			else
				gpl->actframe= gpencil_frame_addnew(gpl, cframe);
		}
		else if (found)
			gpl->actframe= gpf;
		else {
			/* unresolved errogenous situation! */
			printf("Error: cannot find appropriate gp-frame \n");
		}
	}
	else {
		/* currently no frames (add if allowed to) */
		if (addnew)
			gpl->actframe= gpencil_frame_addnew(gpl, cframe);
		else {
			/* don't do anything... this may be when no frames yet! */
		}
	}
	
	/* return */
	return gpl->actframe;
}

/* delete the given frame from a layer */
void gpencil_layer_delframe (bGPDlayer *gpl, bGPDframe *gpf)
{
	/* error checking */
	if (ELEM(NULL, gpl, gpf))
		return;
		
	/* free the frame and its data */
	free_gpencil_strokes(gpf);
	BLI_freelinkN(&gpl->frames, gpf);
	gpl->actframe = NULL;
}

/* get the active gp-layer for editing */
bGPDlayer *gpencil_layer_getactive (bGPdata *gpd)
{
	bGPDlayer *gpl;
	
	/* error checking */
	if (ELEM(NULL, gpd, gpd->layers.first))
		return NULL;
		
	/* loop over layers until found (assume only one active) */
	for (gpl=gpd->layers.first; gpl; gpl=gpl->next) {
		if (gpl->flag & GP_LAYER_ACTIVE)
			return gpl;
	}
	
	/* no active layer found */
	return NULL;
}

/* set the active gp-layer */
void gpencil_layer_setactive (bGPdata *gpd, bGPDlayer *active)
{
	bGPDlayer *gpl;
	
	/* error checking */
	if (ELEM3(NULL, gpd, gpd->layers.first, active))
		return;
		
	/* loop over layers deactivating all */
	for (gpl=gpd->layers.first; gpl; gpl=gpl->next)
		gpl->flag &= ~GP_LAYER_ACTIVE;
	
	/* set as active one */
	active->flag |= GP_LAYER_ACTIVE;
}

/* delete the active gp-layer */
void gpencil_layer_delactive (bGPdata *gpd)
{
	bGPDlayer *gpl= gpencil_layer_getactive(gpd);
	
	/* error checking */
	if (ELEM(NULL, gpd, gpl)) 
		return;
	
	/* free layer */	
	free_gpencil_frames(gpl);
	BLI_freelinkN(&gpd->layers, gpl);

}

/* ************************************************** */
/* GREASE-PENCIL EDITING - Tools */

/* --------- Data Deletion ---------- */

/* delete the last stroke on the active layer */
void gpencil_delete_laststroke (bGPdata *gpd)
{
	bGPDlayer *gpl= gpencil_layer_getactive(gpd);
	bGPDframe *gpf= gpencil_layer_getframe(gpl, CFRA, 0);
	
	gpencil_frame_delete_laststroke(gpf);
}

/* delete the active frame */
void gpencil_delete_actframe (bGPdata *gpd)
{
	bGPDlayer *gpl= gpencil_layer_getactive(gpd);
	bGPDframe *gpf= gpencil_layer_getframe(gpl, CFRA, 0);
	
	gpencil_layer_delframe(gpl, gpf);
}



/* delete various grase-pencil elements 
 *	mode: 	1 - last stroke
 *		 	2 - active frame
 *			3 - active layer
 */
void gpencil_delete_operation (short mode)
{
	bGPdata *gpd;
	
	/* get datablock to work on */
	gpd= gpencil_data_getactive(NULL);
	if (gpd == NULL) return;
	
	switch (mode) {
		case 1: /* last stroke */
			gpencil_delete_laststroke(gpd);
			break;
		case 2: /* active frame */
			gpencil_delete_actframe(gpd);
			break;
		case 3: /* active layer */
			gpencil_layer_delactive(gpd);
			break;
	}
	
	/* redraw and undo-push */
	BIF_undo_push("GPencil Delete");
	allqueue(REDRAWVIEW3D, 0);
}

/* display a menu for deleting different grease-pencil elements */
void gpencil_delete_menu (void)
{
	bGPdata *gpd= gpencil_data_getactive(NULL);
	short mode;
	
	/* only show menu if it will be relevant */
	if (gpd == NULL) return;
	
	mode= pupmenu("Grease Pencil Erase...%t|Last Stroke%x1|Active Frame%x2|Active Layer%x3");
	if (mode <= 0) return;
	
	gpencil_delete_operation(mode);
}

/* --------- Data Conversion ---------- */

/* convert the coordinates from the given stroke point into 3d-coordinates */
static void gp_strokepoint_convertcoords (bGPDstroke *gps, bGPDspoint *pt, float p3d[3])
{
	if (gps->flag & GP_STROKE_3DSPACE) {
		/* directly use 3d-coordinates */
		VecCopyf(p3d, &pt->x);
	}
	else {
		short mval[2], mx, my;
		float *fp= give_cursor();
		float dvec[3];
		
		/* get screen coordinate */
		if (gps->flag & GP_STROKE_2DSPACE) {
			View2D *v2d= spacelink_get_view2d(curarea->spacedata.first);
			ipoco_to_areaco_noclip(v2d, &pt->x, mval);
		}
		else {
			mval[0]= (pt->x / 1000 * curarea->winx);
			mval[1]= (pt->y / 1000 * curarea->winy);
		}
		mx= mval[0]; 
		my= mval[1];
		
		/* convert screen coordinate to 3d coordinates 
		 *	- method taken from editview.c - mouse_cursor() 
		 */
		project_short_noclip(fp, mval);
		window_to_3d(dvec, mval[0]-mx, mval[1]-my);
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
		bezt->radius = bezt->weight = pt->pressure * gpl->thickness;
	}
	
	/* must calculate handles or else we crash */
	calchandlesNurb(nu);
	
	/* add nurb to curve */
	BLI_addtail(&cu->nurb, nu);
}

/* convert a given grease-pencil layer to a 3d-curve representation (using current view if appropriate) */
static void gp_layer_to_curve (bGPdata *gpd, bGPDlayer *gpl, short mode)
{
	bGPDframe *gpf= gpencil_layer_getframe(gpl, CFRA, 0);
	bGPDstroke *gps;
	Object *ob;
	Curve *cu;
	
	/* error checking */
	if (ELEM3(NULL, gpd, gpl, gpf))
		return;
		
	/* only convert if there are any strokes on this layer's frame to convert */
	if (gpf->strokes.first == NULL)
		return;
		
	/* initialise the curve */	
	cu= add_curve(gpl->info, 1);
	cu->flag |= CU_3D;
	
	/* init the curve object (remove rotation and assign curve data to it) */
	add_object_draw(OB_CURVE);
	ob= OBACT;
	ob->loc[0]= ob->loc[1]= ob->loc[2]= 0;
	ob->rot[0]= ob->rot[1]= ob->rot[2]= 0;
	ob->data= cu;
	
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
		BLI_strncpy(ebo->name, "Stroke", 32);
		unique_editbone_name(bones, ebo->name);
		
		BLI_addtail(bones, ebo);
		
		ebo->flag |= BONE_CONNECTED;
		ebo->weight= 1.0F;
		ebo->dist= 0.25F;
		ebo->xwidth= 0.1;
		ebo->zwidth= 0.1;
		ebo->ease1= 1.0;
		ebo->ease2= 1.0;
		ebo->rad_head= pt->pressure * gpl->thickness * 0.1;
		ebo->rad_tail= ptn->pressure * gpl->thickness * 0.1;
		ebo->segments= 1;
		ebo->layer= arm->layer;
		
		/* set parenting */
		// TODO: also adjust roll....
		ebo->parent= prev;
	}
}

/* convert a given grease-pencil layer to a 3d-curve representation (using current view if appropriate) */
static void gp_layer_to_armature (bGPdata *gpd, bGPDlayer *gpl, short mode)
{
	bGPDframe *gpf= gpencil_layer_getframe(gpl, CFRA, 0);
	bGPDstroke *gps;
	Object *ob;
	bArmature *arm;
	ListBase bones = {0,0};
	
	/* error checking */
	if (ELEM3(NULL, gpd, gpl, gpf))
		return;
		
	/* only convert if there are any strokes on this layer's frame to convert */
	if (gpf->strokes.first == NULL)
		return;
		
	/* initialise the armature */	
	arm= add_armature(gpl->info);
	
	/* init the armature object (remove rotation and assign armature data to it) */
	add_object_draw(OB_ARMATURE);
	ob= OBACT;
	ob->loc[0]= ob->loc[1]= ob->loc[2]= 0;
	ob->rot[0]= ob->rot[1]= ob->rot[2]= 0;
	ob->data= arm;
	
	/* convert segments to bones, strokes to bone chains */
	for (gps= gpf->strokes.first; gps; gps= gps->next) {
		gp_stroke_to_bonechain(gpl, gps, arm, &bones);
	}
	
	/* flush editbones to armature */
	editbones_to_armature(&bones, ob);
	if (bones.first) BLI_freelistN(&bones);
}

/* --- */

/* convert grease-pencil strokes to another representation 
 *	mode: 	1 - Active layer to path
 *			2 - Active layer to bezier
 *			3 - Active layer to armature
 */
void gpencil_convert_operation (short mode)
{
	bGPdata *gpd;	
	float *fp= give_cursor();
	
	/* get datablock to work on */
	gpd= gpencil_data_getactive(NULL);
	if (gpd == NULL) return;
	
	/* initialise 3d-cursor correction globals */
	initgrabz(fp[0], fp[1], fp[2]);
	
	/* handle selection modes */
	switch (mode) {
		case 1: /* active layer only (to path) */
		case 2: /* active layer only (to bezier) */
		{
			bGPDlayer *gpl= gpencil_layer_getactive(gpd);
			gp_layer_to_curve(gpd, gpl, mode);
		}
			break;
		case 3: /* active layer only (to armature) */
		{
			bGPDlayer *gpl= gpencil_layer_getactive(gpd);
			gp_layer_to_armature(gpd, gpl, mode);
		}
			break;
	}
	
	/* redraw and undo-push */
	BIF_undo_push("GPencil Convert");
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWOOPS, 0);
}

/* display a menu for converting grease-pencil strokes */
void gpencil_convert_menu (void)
{
	bGPdata *gpd= gpencil_data_getactive(NULL);
	short mode;
	
	/* only show menu if it will be relevant */
	if (gpd == NULL) return;
	
	mode= pupmenu("Grease Pencil Convert %t|Active Layer To Path%x1|Active Layer to Bezier%x2|Active Layer to Armature%x3");
	if (mode <= 0) return;
	
	gpencil_convert_operation(mode);
}

/* ************************************************** */
/* GREASE-PENCIL EDITING MODE - Painting */

/* ---------- 'Globals' and Defines ----------------- */

/* maximum sizes of gp-session buffer */
#define GP_STROKE_BUFFER_MAX	5000

/* Hardcoded sensitivity thresholds... */
	/* minimum number of pixels mouse should move before new point created */
#define MIN_MANHATTEN_PX	U.gp_manhattendist
	/* minimum length of new segment before new point can be added */
#define MIN_EUCLIDEAN_PX	U.gp_euclideandist

/* ------ */

/* Temporary 'Stroke' Operation data */
typedef struct tGPsdata {
	ScrArea *sa;		/* area where painting originated */
	View2D *v2d;		/* needed for GP_STROKE_2DSPACE */
	ImBuf *ibuf;		/* needed for GP_STROKE_2DIMAGE */
	
	bGPdata *gpd;		/* gp-datablock layer comes from */
	bGPDlayer *gpl;		/* layer we're working on */
	bGPDframe *gpf;		/* frame we're working on */
	
	short status;		/* current status of painting */
	short paintmode;	/* mode for painting */
	
	short mval[2];		/* current mouse-position */
	short mvalo[2];		/* previous recorded mouse-position */
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

/* init new painting session */
static void gp_session_initpaint (tGPsdata *p)
{
	/* clear previous data (note: is on stack) */
	memset(p, 0, sizeof(tGPsdata));
	
	/* make sure the active view (at the starting time) is a 3d-view */
	if (curarea == NULL) {
		p->status= GP_STATUS_ERROR;
		if (G.f & G_DEBUG) 
			printf("Error: No active view for painting \n");
		return;
	}
	switch (curarea->spacetype) {
		/* supported views first */
		case SPACE_VIEW3D:
		{
			View3D *v3d= curarea->spacedata.first;
			
			/* set current area */
			p->sa= curarea;
			
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
			p->v2d= &snode->v2d;
			
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
			p->v2d= &sseq->v2d;
			
			/* check that gpencil data is allowed to be drawn */
			if (sseq->mainb == 0) {
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
			p->v2d= &sima->v2d;
			p->ibuf= BKE_image_get_ibuf(sima->image, &sima->iuser);
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
		
		p->gpd= gpencil_data_addnew();
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
		float *fp= give_cursor();
		float dvec[3];
		
		/* method taken from editview.c - mouse_cursor() */
		project_short_noclip(fp, mval);
		window_to_3d(dvec, mval[0]-mx, mval[1]-my);
		VecSubf(out, fp, dvec);
	}
	
	/* 2d - on 'canvas' (assume that p->v2d is set) */
	else if ((gpd->sbuffer_sflag & GP_STROKE_2DSPACE) && (p->v2d)) {
		float x, y;
		
		areamouseco_to_ipoco(p->v2d, mval, &x, &y);
		
		out[0]= x;
		out[1]= y;
	}
	
	/* 2d - on image 'canvas' (assume that p->v2d is set) */
	else if ( (gpd->sbuffer_sflag & GP_STROKE_2DIMAGE) && (p->v2d) ) 
	{
		/* for now - space specific */
		switch (p->sa->spacetype) {
			case SPACE_SEQ: /* sequencer */
			{
				SpaceSeq *sseq= (SpaceSeq *)p->sa->spacedata.first;
				int sizex, sizey, offsx, offsy, rectx, recty;
				float zoom, zoomx, zoomy;
				
				/* calculate zoom factor */
				zoom= SEQ_ZOOM_FAC(sseq->zoom);
				if (sseq->mainb == SEQ_DRAW_IMG_IMBUF) {
					zoomx = zoom * ((float)G.scene->r.xasp / (float)G.scene->r.yasp);
					zoomy = zoom;
				} 
				else
					zoomx = zoomy = zoom;
				
				/* calculate rect size */
				rectx= (G.scene->r.size*G.scene->r.xsch)/100;
				recty= (G.scene->r.size*G.scene->r.ysch)/100; 
				sizex= zoomx * rectx;
				sizey= zoomy * recty;
				offsx= (p->sa->winx-sizex)/2 + sseq->xof;
				offsy= (p->sa->winy-sizey)/2 + sseq->yof;
				
				/* calculate new points */
				out[0]= (float)(mval[0] - offsx) / (float)sizex;
				out[1]= (float)(mval[1] - offsy) / (float)sizey;
			}
				break;
				
			default: /* just use raw mouse coordinates - BAD! */
				out[0]= mval[0];
				out[1]= mval[1];
				break;
		}		
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

/* make a new stroke from the buffer data */
static void gp_stroke_newfrombuffer (tGPsdata *p)
{
	bGPdata *gpd= p->gpd;
	bGPDstroke *gps;
	bGPDspoint *pt;
	tGPspoint *ptc;
	int i, totelem;

	/* macro to test if only converting endpoints  */	
	#define GP_BUFFER2STROKE_ENDPOINTS ((gpd->flag & GP_DATA_EDITPAINT) && (G.qual & LR_CTRLKEY))
	
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
	
	/* undefine macro to test if only converting endpoints  */	
	#undef GP_BUFFER2STROKE_ENDPOINTS
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
	short x0=0, y0=0, x1=0, y1=0;
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
			project_short(&gps->points->x, xyval);
			x0= xyval[0];
			y0= xyval[1];
		}
		else if (gps->flag & GP_STROKE_2DSPACE) {			
			ipoco_to_areaco_noclip(p->v2d, &gps->points->x, xyval);
			x0= xyval[0];
			y0= xyval[1];
		}
		else if (gps->flag & GP_STROKE_2DIMAGE) {			
			ipoco_to_areaco_noclip(p->v2d, &gps->points->x, xyval);
			x0= xyval[0];
			y0= xyval[1];
		}
		else {
			x0= (gps->points->x / 1000 * p->sa->winx);
			y0= (gps->points->y / 1000 * p->sa->winy);
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
				project_short(&pt1->x, xyval);
				x0= xyval[0];
				y0= xyval[1];
				
				project_short(&pt2->x, xyval);
				x1= xyval[0];
				y1= xyval[1];
			}
			else if (gps->flag & GP_STROKE_2DSPACE) {
				ipoco_to_areaco_noclip(p->v2d, &pt1->x, xyval);
				x0= xyval[0];
				y0= xyval[1];
				
				ipoco_to_areaco_noclip(p->v2d, &pt2->x, xyval);
				x1= xyval[0];
				y1= xyval[1];
			}
			else if (gps->flag & GP_STROKE_2DIMAGE) {
				ipoco_to_areaco_noclip(p->v2d, &pt1->x, xyval);
				x0= xyval[0];
				y0= xyval[1];
				
				ipoco_to_areaco_noclip(p->v2d, &pt2->x, xyval);
				x1= xyval[0];
				y1= xyval[1];
			}
			else {
				x0= (pt1->x / 1000 * p->sa->winx);
				y0= (pt1->y / 1000 * p->sa->winy);
				x1= (pt2->x / 1000 * p->sa->winx);
				y1= (pt2->y / 1000 * p->sa->winy);
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
	p->gpf= gpencil_layer_getframe(p->gpl, CFRA, 1);
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
				float *fp= give_cursor();
				initgrabz(fp[0], fp[1], fp[2]);
				
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
				/* for now, this is not applicable here... */
				p->gpd->sbuffer_sflag |= GP_STROKE_2DIMAGE;
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
	force_draw_plus(SPACE_ACTION, 0);
}

/* -------- */

/* main call to paint a new stroke */
short gpencil_paint (short mousebutton, short paintmode)
{
	tGPsdata p;
	float opressure, pressure;
	short ok = GP_STROKEADD_NORMAL;
	
	/* init paint-data */
	gp_session_initpaint(&p);
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
	setcursor_space(p.sa->spacetype, CURSOR_VPAINT);
	
	/* init drawing-device settings */
	getmouseco_areawin(p.mval);
	pressure = get_pressure();
	
	p.mvalo[0]= p.mval[0];
	p.mvalo[1]= p.mval[1];
	opressure= pressure;
	
	/* radius for eraser circle is thickness^2 */
	p.radius= p.gpl->thickness * p.gpl->thickness;
	
	/* start drawing eraser-circle (if applicable) */
	if (paintmode == GP_PAINTMODE_ERASER)
		draw_sel_circle(p.mval, NULL, p.radius, p.radius, 0); // draws frontbuffer, but sets backbuf again
	
	/* only allow painting of single 'dots' if: 
	 *	- pressure is not excessive (as it can be on some windows tablets)
	 *	- draw-mode for active datablock is turned on
	 * 	- not erasing
	 */
	if (paintmode != GP_PAINTMODE_ERASER) {
		if (!(pressure >= 0.99f) || (p.gpd->flag & GP_DATA_EDITPAINT)) { 
			gp_stroke_addpoint(&p, p.mval, pressure);
		}
	}
	
	/* paint loop */
	do {
		/* get current user input */
		getmouseco_areawin(p.mval);
		pressure = get_pressure();
		
		/* only add current point to buffer if mouse moved (otherwise wait until it does) */
		if (paintmode == GP_PAINTMODE_ERASER) {
			/* do 'live' erasing now */
			gp_stroke_doeraser(&p);
			
			draw_sel_circle(p.mval, p.mvalo, p.radius, p.radius, 0);
			force_draw(0);
			
			p.mvalo[0]= p.mval[0];
			p.mvalo[1]= p.mval[1];
		}
		else if (gp_stroke_filtermval(&p, p.mval, p.mvalo)) {
			/* try to add point */
			ok= gp_stroke_addpoint(&p, p.mval, pressure);
			
			/* handle errors while adding point */
			if ((ok == GP_STROKEADD_FULL) || (ok == GP_STROKEADD_OVERFLOW)) {
				/* finish off old stroke */
				gp_paint_strokeend(&p);
				
				/* start a new stroke, starting from previous point */
				gp_stroke_addpoint(&p, p.mvalo, opressure);
				ok= gp_stroke_addpoint(&p, p.mval, pressure);
			}
			else if (ok == GP_STROKEADD_INVALID) {
				/* the painting operation cannot continue... */
				error("Cannot paint stroke");
				p.status = GP_STATUS_ERROR;
				
				if (G.f & G_DEBUG) 
					printf("Error: Grease-Pencil Paint - Add Point Invalid \n");
				break;
			}
			force_draw(0);
			
			p.mvalo[0]= p.mval[0];
			p.mvalo[1]= p.mval[1];
			opressure= pressure;
		}
		else
			BIF_wait_for_statechange();
		
		/* do mouse checking at the end, so don't check twice, and potentially
		 * miss a short tap 
		 */
	} while (get_mbut() & mousebutton);
	
	/* clear edit flags */
	G.f &= ~G_GREASEPENCIL;
	
	/* restore cursor to indicate end of drawing */
	setcursor_space(p.sa->spacetype, CURSOR_STD);
	
	/* check size of buffer before cleanup, to determine if anything happened here */
	if (paintmode == GP_PAINTMODE_ERASER) {
		ok= 1; // fixme
		draw_sel_circle(NULL, p.mvalo, 0, p.radius, 0);
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
short gpencil_do_paint (ScrArea *sa, short mbut)
{
	bGPdata *gpd = gpencil_data_getactive(sa);
	short retval= 0;
	
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
		retval = gpencil_paint(mbut, GP_PAINTMODE_ERASER);
	}
	else if (gpd->flag & GP_DATA_EDITPAINT) {
		/* try to paint/erase */
		if (mbut == L_MOUSE)
			retval = gpencil_paint(mbut, GP_PAINTMODE_DRAW);
		else if (mbut == R_MOUSE)
			retval = gpencil_paint(mbut, GP_PAINTMODE_ERASER);
	}
	else if (!(gpd->flag & GP_DATA_LMBPLOCK)) {
		/* try to paint/erase as not locked */
		if ((G.qual == LR_SHIFTKEY) && (mbut == L_MOUSE)) {
			retval = gpencil_paint(mbut, GP_PAINTMODE_DRAW);
		}
		else if (G.qual == LR_ALTKEY) {
			if ((U.flag & USER_LMOUSESELECT) && (mbut == L_MOUSE))
				retval = gpencil_paint(mbut, GP_PAINTMODE_ERASER);
			else if (!(U.flag & USER_LMOUSESELECT) && (mbut == R_MOUSE))
				retval = gpencil_paint(mbut, GP_PAINTMODE_ERASER);
		}
	}
	
	/* return result of trying to paint */
	return retval;
}

/* ************************************************** */
