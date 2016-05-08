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
#include "BLI_math_vector.h"

#include "BLT_translation.h"

#include "DNA_gpencil_types.h"
#include "DNA_userdef_types.h"

#include "BKE_animsys.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"
#include "BKE_library.h"


/* ************************************************** */
/* GENERAL STUFF */

/* --------- Memory Management ------------ */

/* Free strokes belonging to a gp-frame */
bool free_gpencil_strokes(bGPDframe *gpf)
{
	bGPDstroke *gps, *gpsn;
	bool changed = (BLI_listbase_is_empty(&gpf->strokes) == false);

	/* free strokes */
	for (gps = gpf->strokes.first; gps; gps = gpsn) {
		gpsn = gps->next;
		
		/* free stroke memory arrays, then stroke itself */
		if (gps->points) MEM_freeN(gps->points);
		if (gps->triangles) MEM_freeN(gps->triangles);
		BLI_freelinkN(&gpf->strokes, gps);
	}

	return changed;
}

/* Free all of a gp-layer's frames */
void free_gpencil_frames(bGPDlayer *gpl)
{
	bGPDframe *gpf, *gpfn;
	
	/* error checking */
	if (gpl == NULL) return;
	
	/* free frames */
	for (gpf = gpl->frames.first; gpf; gpf = gpfn) {
		gpfn = gpf->next;
		
		/* free strokes and their associated memory */
		free_gpencil_strokes(gpf);
		BLI_freelinkN(&gpl->frames, gpf);
	}
	gpl->actframe = NULL;
}

/* Free all of the gp-layers for a viewport (list should be &gpd->layers or so) */
void free_gpencil_layers(ListBase *list)
{
	bGPDlayer *gpl, *gpln;
	
	/* error checking */
	if (list == NULL) return;
	
	/* delete layers */
	for (gpl = list->first; gpl; gpl = gpln) {
		gpln = gpl->next;
		
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
	
	/* free animation data */
	if (gpd->adt) {
		BKE_animdata_free(&gpd->id);
		gpd->adt = NULL;
	}
}

/* -------- Container Creation ---------- */

/* add a new gp-frame to the given layer */
bGPDframe *gpencil_frame_addnew(bGPDlayer *gpl, int cframe)
{
	bGPDframe *gpf = NULL, *gf = NULL;
	short state = 0;
	
	/* error checking */
	if (gpl == NULL)
		return NULL;
		
	/* allocate memory for this frame */
	gpf = MEM_callocN(sizeof(bGPDframe), "bGPDframe");
	gpf->framenum = cframe;
	
	/* find appropriate place to add frame */
	if (gpl->frames.first) {
		for (gf = gpl->frames.first; gf; gf = gf->next) {
			/* check if frame matches one that is supposed to be added */
			if (gf->framenum == cframe) {
				state = -1;
				break;
			}
			
			/* if current frame has already exceeded the frame to add, add before */
			if (gf->framenum > cframe) {
				BLI_insertlinkbefore(&gpl->frames, gf, gpf);
				state = 1;
				break;
			}
		}
	}
	
	/* check whether frame was added successfully */
	if (state == -1) {
		printf("Error: Frame (%d) existed already for this layer. Using existing frame\n", cframe);
		
		/* free the newly created one, and use the old one instead */
		MEM_freeN(gpf);
		
		/* return existing frame instead... */
		BLI_assert(gf != NULL);
		gpf = gf;
	}
	else if (state == 0) {
		/* add to end then! */
		BLI_addtail(&gpl->frames, gpf);
	}
	
	/* return frame */
	return gpf;
}

/* add a copy of the active gp-frame to the given layer */
bGPDframe *gpencil_frame_addcopy(bGPDlayer *gpl, int cframe)
{
	bGPDframe *new_frame, *gpf;
	bool found = false;
	
	/* Error checking/handling */
	if (gpl == NULL) {
		/* no layer */
		return NULL;
	}
	else if (gpl->actframe == NULL) {
		/* no active frame, so just create a new one from scratch */
		return gpencil_frame_addnew(gpl, cframe);
	}
	
	/* Create a copy of the frame */
	new_frame = gpencil_frame_duplicate(gpl->actframe);
	
	/* Find frame to insert it before */
	for (gpf = gpl->frames.first; gpf; gpf = gpf->next) {
		if (gpf->framenum > cframe) {
			/* Add it here */
			BLI_insertlinkbefore(&gpl->frames, gpf, new_frame);
			
			found = true;
			break;
		}
		else if (gpf->framenum == cframe) {
			/* This only happens when we're editing with framelock on...
			 * - Delete the new frame and don't do anything else here...
			 */
			free_gpencil_strokes(new_frame);
			MEM_freeN(new_frame);
			new_frame = NULL;
			
			found = true;
			break;
		}
	}
	
	if (found == false) {
		/* Add new frame to the end */
		BLI_addtail(&gpl->frames, new_frame);
	}
	
	/* Ensure that frame is set up correctly, and return it */
	if (new_frame) {
		new_frame->framenum = cframe;
		gpl->actframe = new_frame;
	}
	
	return new_frame;
}

/* add a new gp-layer and make it the active layer */
bGPDlayer *gpencil_layer_addnew(bGPdata *gpd, const char *name, bool setactive)
{
	bGPDlayer *gpl;
	
	/* check that list is ok */
	if (gpd == NULL)
		return NULL;
		
	/* allocate memory for frame and add to end of list */
	gpl = MEM_callocN(sizeof(bGPDlayer), "bGPDlayer");
	
	/* add to datablock */
	BLI_addtail(&gpd->layers, gpl);
	
	/* set basic settings */
	copy_v4_v4(gpl->color, U.gpencil_new_layer_col);
	gpl->thickness = 3;
	
	/* onion-skinning settings */
	if (gpd->flag & GP_DATA_SHOW_ONIONSKINS)
		gpl->flag |= GP_LAYER_ONIONSKIN;
	
	gpl->flag |= (GP_LAYER_GHOST_PREVCOL | GP_LAYER_GHOST_NEXTCOL);
	
	ARRAY_SET_ITEMS(gpl->gcolor_prev, 0.145098f, 0.419608f, 0.137255f); /* green */
	ARRAY_SET_ITEMS(gpl->gcolor_next, 0.125490f, 0.082353f, 0.529412f); /* blue */
	
	/* high quality fill by default */
	gpl->flag |= GP_LAYER_HQ_FILL;
	
	/* default smooth iterations */
	gpl->draw_smoothlvl = 1;
	
	/* auto-name */
	BLI_strncpy(gpl->info, name, sizeof(gpl->info));
	BLI_uniquename(&gpd->layers, gpl, DATA_("GP_Layer"), '.', offsetof(bGPDlayer, info), sizeof(gpl->info));
	
	/* make this one the active one */
	if (setactive)
		gpencil_layer_setactive(gpd, gpl);
	
	/* return layer */
	return gpl;
}

/* add a new gp-datablock */
bGPdata *gpencil_data_addnew(const char name[])
{
	bGPdata *gpd;
	
	/* allocate memory for a new block */
	gpd = BKE_libblock_alloc(G.main, ID_GD, name);
	
	/* initial settings */
	gpd->flag = (GP_DATA_DISPINFO | GP_DATA_EXPAND);
	
	/* for now, stick to view is also enabled by default
	 * since this is more useful...
	 */
	gpd->flag |= GP_DATA_VIEWALIGN;
	
	return gpd;
}

/* -------- Data Duplication ---------- */

/* make a copy of a given gpencil frame */
bGPDframe *gpencil_frame_duplicate(bGPDframe *src)
{
	bGPDstroke *gps, *gpsd;
	bGPDframe *dst;
	
	/* error checking */
	if (src == NULL)
		return NULL;
		
	/* make a copy of the source frame */
	dst = MEM_dupallocN(src);
	dst->prev = dst->next = NULL;
	
	/* copy strokes */
	BLI_listbase_clear(&dst->strokes);
	for (gps = src->strokes.first; gps; gps = gps->next) {
		/* make copy of source stroke, then adjust pointer to points too */
		gpsd = MEM_dupallocN(gps);
		gpsd->points = MEM_dupallocN(gps->points);
		gpsd->triangles = MEM_dupallocN(gps->triangles);
		gpsd->flag |= GP_STROKE_RECALC_CACHES;
		BLI_addtail(&dst->strokes, gpsd);
	}
	
	/* return new frame */
	return dst;
}

/* make a copy of a given gpencil layer */
bGPDlayer *gpencil_layer_duplicate(bGPDlayer *src)
{
	bGPDframe *gpf, *gpfd;
	bGPDlayer *dst;
	
	/* error checking */
	if (src == NULL)
		return NULL;
		
	/* make a copy of source layer */
	dst = MEM_dupallocN(src);
	dst->prev = dst->next = NULL;
	
	/* copy frames */
	BLI_listbase_clear(&dst->frames);
	for (gpf = src->frames.first; gpf; gpf = gpf->next) {
		/* make a copy of source frame */
		gpfd = gpencil_frame_duplicate(gpf);
		BLI_addtail(&dst->frames, gpfd);
		
		/* if source frame was the current layer's 'active' frame, reassign that too */
		if (gpf == dst->actframe)
			dst->actframe = gpfd;
	}
	
	/* return new layer */
	return dst;
}

/* make a copy of a given gpencil datablock */
bGPdata *gpencil_data_duplicate(bGPdata *src, bool internal_copy)
{
	bGPDlayer *gpl, *gpld;
	bGPdata *dst;
	
	/* error checking */
	if (src == NULL)
		return NULL;
	
	/* make a copy of the base-data */
	if (internal_copy) {
		/* make a straight copy for undo buffers used during stroke drawing */
		dst = MEM_dupallocN(src);
	}
	else {
		/* make a copy when others use this */
		dst = BKE_libblock_copy(&src->id);
	}
	
	/* copy layers */
	BLI_listbase_clear(&dst->layers);
	for (gpl = src->layers.first; gpl; gpl = gpl->next) {
		/* make a copy of source layer and its data */
		gpld = gpencil_layer_duplicate(gpl);
		BLI_addtail(&dst->layers, gpld);
	}
	
	/* return new */
	return dst;
}

/* -------- GP-Stroke API --------- */

/* ensure selection status of stroke is in sync with its points */
void gpencil_stroke_sync_selection(bGPDstroke *gps)
{
	bGPDspoint *pt;
	int i;
	
	/* error checking */
	if (gps == NULL)
		return;
	
	/* we'll stop when we find the first selected point,
	 * so initially, we must deselect
	 */
	gps->flag &= ~GP_STROKE_SELECT;
	
	for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
		if (pt->flag & GP_SPOINT_SELECT) {
			gps->flag |= GP_STROKE_SELECT;
			break;
		}
	}
}

/* -------- GP-Frame API ---------- */

/* delete the last stroke of the given frame */
void gpencil_frame_delete_laststroke(bGPDlayer *gpl, bGPDframe *gpf)
{
	bGPDstroke *gps = (gpf) ? gpf->strokes.last : NULL;
	int cfra = (gpf) ? gpf->framenum : 0; /* assume that the current frame was not locked */
	
	/* error checking */
	if (ELEM(NULL, gpf, gps))
		return;
	
	/* free the stroke and its data */
	MEM_freeN(gps->points);
	MEM_freeN(gps->triangles);
	BLI_freelinkN(&gpf->strokes, gps);
	
	/* if frame has no strokes after this, delete it */
	if (BLI_listbase_is_empty(&gpf->strokes)) {
		gpencil_layer_delframe(gpl, gpf);
		gpencil_layer_getframe(gpl, cfra, 0);
	}
}

/* -------- GP-Layer API ---------- */

/* Check if the given layer is able to be edited or not */
bool gpencil_layer_is_editable(const bGPDlayer *gpl)
{
	/* Sanity check */
	if (gpl == NULL)
		return false;
	
	/* Layer must be: Visible + Editable */
	if ((gpl->flag & (GP_LAYER_HIDE | GP_LAYER_LOCKED)) == 0) {
		/* Opacity must be sufficiently high that it is still "visible"
		 * Otherwise, it's not really "visible" to the user, so no point editing...
		 */
		if ((gpl->color[3] > GPENCIL_ALPHA_OPACITY_THRESH) || (gpl->fill[3] > GPENCIL_ALPHA_OPACITY_THRESH)) {
			return true;
		}
	}
	
	/* Something failed */
	return false;
}

/* Look up the gp-frame on the requested frame number, but don't add a new one */
bGPDframe *BKE_gpencil_layer_find_frame(bGPDlayer *gpl, int cframe)
{
	bGPDframe *gpf;
	
	/* Search in reverse order, since this is often used for playback/adding,
	 * where it's less likely that we're interested in the earlier frames
	 */
	for (gpf = gpl->frames.last; gpf; gpf = gpf->prev) {
		if (gpf->framenum == cframe) {
			return gpf;
		}
	}
	
	return NULL;
}

/* get the appropriate gp-frame from a given layer
 *	- this sets the layer's actframe var (if allowed to)
 *	- extension beyond range (if first gp-frame is after all frame in interest and cannot add)
 */
bGPDframe *gpencil_layer_getframe(bGPDlayer *gpl, int cframe, eGP_GetFrame_Mode addnew)
{
	bGPDframe *gpf = NULL;
	short found = 0;
	
	/* error checking */
	if (gpl == NULL) return NULL;
	
	/* check if there is already an active frame */
	if (gpl->actframe) {
		gpf = gpl->actframe;
		
		/* do not allow any changes to layer's active frame if layer is locked from changes
		 * or if the layer has been set to stay on the current frame
		 */
		if (gpl->flag & GP_LAYER_FRAMELOCK)
			return gpf;
		/* do not allow any changes to actframe if frame has painting tag attached to it */
		if (gpf->flag & GP_FRAME_PAINT) 
			return gpf;
		
		/* try to find matching frame */
		if (gpf->framenum < cframe) {
			for (; gpf; gpf = gpf->next) {
				if (gpf->framenum == cframe) {
					found = 1;
					break;
				}
				else if ((gpf->next) && (gpf->next->framenum > cframe)) {
					found = 1;
					break;
				}
			}
			
			/* set the appropriate frame */
			if (addnew) {
				if ((found) && (gpf->framenum == cframe))
					gpl->actframe = gpf;
				else if (addnew == GP_GETFRAME_ADD_COPY)
					gpl->actframe = gpencil_frame_addcopy(gpl, cframe);
				else
					gpl->actframe = gpencil_frame_addnew(gpl, cframe);
			}
			else if (found)
				gpl->actframe = gpf;
			else
				gpl->actframe = gpl->frames.last;
		}
		else {
			for (; gpf; gpf = gpf->prev) {
				if (gpf->framenum <= cframe) {
					found = 1;
					break;
				}
			}
			
			/* set the appropriate frame */
			if (addnew) {
				if ((found) && (gpf->framenum == cframe))
					gpl->actframe = gpf;
				else if (addnew == GP_GETFRAME_ADD_COPY)
					gpl->actframe = gpencil_frame_addcopy(gpl, cframe);
				else
					gpl->actframe = gpencil_frame_addnew(gpl, cframe);
			}
			else if (found)
				gpl->actframe = gpf;
			else
				gpl->actframe = gpl->frames.first;
		}
	}
	else if (gpl->frames.first) {
		/* check which of the ends to start checking from */
		const int first = ((bGPDframe *)(gpl->frames.first))->framenum;
		const int last = ((bGPDframe *)(gpl->frames.last))->framenum;
		
		if (abs(cframe - first) > abs(cframe - last)) {
			/* find gp-frame which is less than or equal to cframe */
			for (gpf = gpl->frames.last; gpf; gpf = gpf->prev) {
				if (gpf->framenum <= cframe) {
					found = 1;
					break;
				}
			}
		}
		else {
			/* find gp-frame which is less than or equal to cframe */
			for (gpf = gpl->frames.first; gpf; gpf = gpf->next) {
				if (gpf->framenum <= cframe) {
					found = 1;
					break;
				}
			}
		}
		
		/* set the appropriate frame */
		if (addnew) {
			if ((found) && (gpf->framenum == cframe))
				gpl->actframe = gpf;
			else
				gpl->actframe = gpencil_frame_addnew(gpl, cframe);
		}
		else if (found)
			gpl->actframe = gpf;
		else {
			/* unresolved errogenous situation! */
			printf("Error: cannot find appropriate gp-frame\n");
			/* gpl->actframe should still be NULL */
		}
	}
	else {
		/* currently no frames (add if allowed to) */
		if (addnew)
			gpl->actframe = gpencil_frame_addnew(gpl, cframe);
		else {
			/* don't do anything... this may be when no frames yet! */
			/* gpl->actframe should still be NULL */
		}
	}
	
	/* return */
	return gpl->actframe;
}

/* delete the given frame from a layer */
bool gpencil_layer_delframe(bGPDlayer *gpl, bGPDframe *gpf)
{
	bool changed = false;
	
	/* error checking */
	if (ELEM(NULL, gpl, gpf))
		return false;
	
	/* if this frame was active, make the previous frame active instead 
	 * since it's tricky to set active frame otherwise
	 */
	if (gpl->actframe == gpf)
		gpl->actframe = gpf->prev;
	else
		gpl->actframe = NULL;
	
	/* free the frame and its data */
	changed = free_gpencil_strokes(gpf);
	BLI_freelinkN(&gpl->frames, gpf);
	
	return changed;
}

/* get the active gp-layer for editing */
bGPDlayer *gpencil_layer_getactive(bGPdata *gpd)
{
	bGPDlayer *gpl;
	
	/* error checking */
	if (ELEM(NULL, gpd, gpd->layers.first))
		return NULL;
		
	/* loop over layers until found (assume only one active) */
	for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
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
	if (ELEM(NULL, gpd, gpd->layers.first, active))
		return;
		
	/* loop over layers deactivating all */
	for (gpl = gpd->layers.first; gpl; gpl = gpl->next)
		gpl->flag &= ~GP_LAYER_ACTIVE;
	
	/* set as active one */
	active->flag |= GP_LAYER_ACTIVE;
}

/* delete the active gp-layer */
void gpencil_layer_delete(bGPdata *gpd, bGPDlayer *gpl)
{
	/* error checking */
	if (ELEM(NULL, gpd, gpl)) 
		return;
	
	/* free layer */
	free_gpencil_frames(gpl);
	BLI_freelinkN(&gpd->layers, gpl);
}

/* ************************************************** */
