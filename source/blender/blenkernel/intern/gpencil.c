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
 * The Original Code is Copyright (C) 2008, Blender Foundation
 * This is a new part of Blender
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/gpencil.c
 *  \ingroup bke
 */

 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "DNA_gpencil_types.h"

#include "BKE_global.h"
#include "BKE_gpencil.h"
#include "BKE_library.h"
#include "BKE_main.h"



/* ************************************************** */
/* GENERAL STUFF */

/* --------- Memory Management ------------ */

/* Free strokes belonging to a gp-frame */
void free_gpencil_strokes(bGPDframe *gpf)
{
	bGPDstroke *gps, *gpsn;
	
	/* error checking */
	if (gpf == NULL) return;
	
	/* free strokes */
	for (gps= gpf->strokes.first; gps; gps= gpsn) {
		gpsn= gps->next;
		
		/* free stroke memory arrays, then stroke itself */
		if (gps->points) MEM_freeN(gps->points);
		BLI_freelinkN(&gpf->strokes, gps);
	}
}

/* Free all of a gp-layer's frames */
void free_gpencil_frames(bGPDlayer *gpl)
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

/* Free all of the gp-layers for a viewport (list should be &gpd->layers or so) */
void free_gpencil_layers(ListBase *list)
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

/* Free all of GPencil datablock's related data, but not the block itself */
void BKE_gpencil_free(bGPdata *gpd)
{
	/* free layers */
	free_gpencil_layers(&gpd->layers);
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
		printf("Error: frame (%d) existed already for this layer\n", cframe);
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
	strcpy(gpl->info, "GP_Layer");
	BLI_uniquename(&gpd->layers, gpl, "GP_Layer", '.', offsetof(bGPDlayer, info), sizeof(gpl->info));
	
	/* make this one the active one */
	gpencil_layer_setactive(gpd, gpl);
	
	/* return layer */
	return gpl;
}

/* add a new gp-datablock */
bGPdata *gpencil_data_addnew (const char name[])
{
	bGPdata *gpd;
	
	/* allocate memory for a new block */
	gpd= BKE_libblock_alloc(&G.main->gpencil, ID_GD, name);
	
	/* initial settings */
	gpd->flag = (GP_DATA_DISPINFO|GP_DATA_EXPAND);
	
	/* for now, stick to view is also enabled by default
	 * since this is more useful...
	 */
	gpd->flag |= GP_DATA_VIEWALIGN;
	
	return gpd;
}

/* -------- Data Duplication ---------- */

/* make a copy of a given gpencil frame */
bGPDframe *gpencil_frame_duplicate (bGPDframe *src)
{
	bGPDstroke *gps, *gpsd;
	bGPDframe *dst;
	
	/* error checking */
	if (src == NULL)
		return NULL;
		
	/* make a copy of the source frame */
	dst= MEM_dupallocN(src);
	dst->prev= dst->next= NULL;
	
	/* copy strokes */
	dst->strokes.first = dst->strokes.last= NULL;
	for (gps= src->strokes.first; gps; gps= gps->next) {
		/* make copy of source stroke, then adjust pointer to points too */
		gpsd= MEM_dupallocN(gps);
		gpsd->points= MEM_dupallocN(gps->points);
		
		BLI_addtail(&dst->strokes, gpsd);
	}
	
	/* return new frame */
	return dst;
}

/* make a copy of a given gpencil layer */
bGPDlayer *gpencil_layer_duplicate (bGPDlayer *src)
{
	bGPDframe *gpf, *gpfd;
	bGPDlayer *dst;
	
	/* error checking */
	if (src == NULL)
		return NULL;
		
	/* make a copy of source layer */
	dst= MEM_dupallocN(src);
	dst->prev= dst->next= NULL;
	
	/* copy frames */
	dst->frames.first= dst->frames.last= NULL;
	for (gpf= src->frames.first; gpf; gpf= gpf->next) {
		/* make a copy of source frame */
		gpfd= gpencil_frame_duplicate(gpf);
		BLI_addtail(&dst->frames, gpfd);
		
		/* if source frame was the current layer's 'active' frame, reassign that too */
		if (gpf == dst->actframe)
			dst->actframe= gpfd;
	}
	
	/* return new layer */
	return dst;
}

/* make a copy of a given gpencil datablock */
bGPdata *gpencil_data_duplicate (bGPdata *src)
{
	bGPDlayer *gpl, *gpld;
	bGPdata *dst;
	
	/* error checking */
	if (src == NULL)
		return NULL;
	
	/* make a copy of the base-data */
	dst= MEM_dupallocN(src);
	
	/* copy layers */
	dst->layers.first= dst->layers.last= NULL;
	for (gpl= src->layers.first; gpl; gpl= gpl->next) {
		/* make a copy of source layer and its data */
		gpld= gpencil_layer_duplicate(gpl);
		BLI_addtail(&dst->layers, gpld);
	}
	
	/* return new */
	return dst;
}

/* -------- GP-Frame API ---------- */

/* delete the last stroke of the given frame */
void gpencil_frame_delete_laststroke(bGPDlayer *gpl, bGPDframe *gpf)
{
	bGPDstroke *gps= (gpf) ? gpf->strokes.last : NULL;
	int cfra = (gpf) ? gpf->framenum : 0; /* assume that the current frame was not locked */
	
	/* error checking */
	if (ELEM(NULL, gpf, gps))
		return;
	
	/* free the stroke and its data */
	MEM_freeN(gps->points);
	BLI_freelinkN(&gpf->strokes, gps);
	
	/* if frame has no strokes after this, delete it */
	if (gpf->strokes.first == NULL) {
		gpencil_layer_delframe(gpl, gpf);
		gpencil_layer_getframe(gpl, cfra, 0);
	}
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
		
		/* do not allow any changes to layer's active frame if layer is locked from changes
		 * or if the layer has been set to stay on the current frame
		 */
		if (gpl->flag & (GP_LAYER_LOCKED|GP_LAYER_FRAMELOCK))
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
			printf("Error: cannot find appropriate gp-frame\n");
			/* gpl->actframe should still be NULL */
		}
	}
	else {
		/* currently no frames (add if allowed to) */
		if (addnew)
			gpl->actframe= gpencil_frame_addnew(gpl, cframe);
		else {
			/* don't do anything... this may be when no frames yet! */
			/* gpl->actframe should still be NULL */
		}
	}
	
	/* return */
	return gpl->actframe;
}

/* delete the given frame from a layer */
void gpencil_layer_delframe(bGPDlayer *gpl, bGPDframe *gpf)
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
void gpencil_layer_setactive(bGPdata *gpd, bGPDlayer *active)
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
void gpencil_layer_delactive(bGPdata *gpd)
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
