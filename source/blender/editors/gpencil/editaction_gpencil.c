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

/** \file blender/editors/gpencil/editaction_gpencil.c
 *  \ingroup edgpencil
 */

 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_gpencil_types.h"
#include "DNA_scene_types.h"

#include "BKE_fcurve.h"
#include "BKE_gpencil.h"

#include "ED_anim_api.h"
#include "ED_gpencil.h"
#include "ED_keyframes_edit.h"

#include "gpencil_intern.h"

/* ***************************************** */
/* NOTE ABOUT THIS FILE:
 * 	This file contains code for editing Grease Pencil data in the Action Editor
 *	as a 'keyframes', so that a user can adjust the timing of Grease Pencil drawings.
 * 	Therefore, this file mostly contains functions for selecting Grease-Pencil frames.
 */
/* ***************************************** */
/* Generics - Loopers */

/* Loops over the gp-frames for a gp-layer, and applies the given callback */
short gplayer_frames_looper (bGPDlayer *gpl, Scene *scene, short (*gpf_cb)(bGPDframe *, Scene *))
{
	bGPDframe *gpf;
	
	/* error checker */
	if (gpl == NULL)
		return 0;
	
	/* do loop */
	for (gpf= gpl->frames.first; gpf; gpf= gpf->next) {
		/* execute callback */
		if (gpf_cb(gpf, scene))
			return 1;
	}
		
	/* nothing to return */
	return 0;
}

/* ****************************************** */
/* Data Conversion Tools */

/* make a listing all the gp-frames in a layer as cfraelems */
void gplayer_make_cfra_list (bGPDlayer *gpl, ListBase *elems, short onlysel)
{
	bGPDframe *gpf;
	CfraElem *ce;
	
	/* error checking */
	if (ELEM(NULL, gpl, elems))
		return;
	
	/* loop through gp-frames, adding */
	for (gpf= gpl->frames.first; gpf; gpf= gpf->next) {
		if ((onlysel == 0) || (gpf->flag & GP_FRAME_SELECT)) {
			ce= MEM_callocN(sizeof(CfraElem), "CfraElem");
			
			ce->cfra= (float)gpf->framenum;
			ce->sel= (gpf->flag & GP_FRAME_SELECT) ? 1 : 0;
			
			BLI_addtail(elems, ce);
		}
	}
}

/* ***************************************** */
/* Selection Tools */

/* check if one of the frames in this layer is selected */
short is_gplayer_frame_selected (bGPDlayer *gpl)
{
	bGPDframe *gpf;
	
	/* error checking */
	if (gpl == NULL) 
		return 0;
	
	/* stop at the first one found */
	for (gpf= gpl->frames.first; gpf; gpf= gpf->next) {
		if (gpf->flag & GP_FRAME_SELECT)
			return 1;
	}
	
	/* not found */
	return 0;
}

/* helper function - select gp-frame based on SELECT_* mode */
static void gpframe_select (bGPDframe *gpf, short select_mode)
{
	if (gpf == NULL)
		return;
	
	switch (select_mode) {
		case SELECT_ADD:
			gpf->flag |= GP_FRAME_SELECT;
			break;
		case SELECT_SUBTRACT:
			gpf->flag &= ~GP_FRAME_SELECT;
			break;
		case SELECT_INVERT:
			gpf->flag ^= GP_FRAME_SELECT;
			break;
	}
}

/* set all/none/invert select (like above, but with SELECT_* modes) */
void select_gpencil_frames (bGPDlayer *gpl, short select_mode)
{
	bGPDframe *gpf;
	
	/* error checking */
	if (gpl == NULL) 
		return;
		
	/* handle according to mode */
	for (gpf= gpl->frames.first; gpf; gpf= gpf->next) {
		gpframe_select(gpf, select_mode);
	}
}

/* set all/none/invert select */
void set_gplayer_frame_selection (bGPDlayer *gpl, short mode)
{
	/* error checking */
	if (gpl == NULL) 
		return;
	
	/* now call the standard function */
	select_gpencil_frames(gpl, mode);
}

/* select the frame in this layer that occurs on this frame (there should only be one at most) */
void select_gpencil_frame (bGPDlayer *gpl, int selx, short select_mode)
{
	bGPDframe *gpf;
	
	if (gpl == NULL) 
		return;
	
	/* search through frames for a match */
	for (gpf= gpl->frames.first; gpf; gpf= gpf->next) {
		/* there should only be one frame with this frame-number */
		if (gpf->framenum == selx) {
			gpframe_select(gpf, select_mode);
			break;
		}
	}
}

/* select the frames in this layer that occur within the bounds specified */
void borderselect_gplayer_frames (bGPDlayer *gpl, float min, float max, short select_mode)
{
	bGPDframe *gpf;
	
	if (gpl == NULL)
		return;
	
	/* only select those frames which are in bounds */
	for (gpf= gpl->frames.first; gpf; gpf= gpf->next) {
		if (IN_RANGE(gpf->framenum, min, max))
			gpframe_select(gpf, select_mode);
	}
}

/* ***************************************** */
/* Frame Editing Tools */

/* Delete selected frames */
void delete_gplayer_frames (bGPDlayer *gpl)
{
	bGPDframe *gpf, *gpfn;
	
	/* error checking */
	if (gpl == NULL)
		return;
		
	/* check for frames to delete */
	for (gpf= gpl->frames.first; gpf; gpf= gpfn) {
		gpfn= gpf->next;
		
		if (gpf->flag & GP_FRAME_SELECT)
			gpencil_layer_delframe(gpl, gpf);
	}
}

/* Duplicate selected frames from given gp-layer */
void duplicate_gplayer_frames (bGPDlayer *gpl)
{
	bGPDframe *gpf, *gpfn;
	
	/* error checking */
	if (gpl == NULL)
		return;
	
	/* duplicate selected frames  */
	for (gpf= gpl->frames.first; gpf; gpf= gpfn) {
		gpfn= gpf->next;
		
		/* duplicate this frame */
		if (gpf->flag & GP_FRAME_SELECT) {
			bGPDframe *gpfd; 
			
			/* duplicate frame, and deselect self */
			gpfd= gpencil_frame_duplicate(gpf);
			gpf->flag &= ~GP_FRAME_SELECT;
			
			BLI_insertlinkafter(&gpl->frames, gpf, gpfd);
		}
	}
}

#if 0 // XXX disabled until grease pencil code stabilises again
/* -------------------------------------- */
/* Copy and Paste Tools */
/* - The copy/paste buffer currently stores a set of GP_Layers, with temporary
 *	GP_Frames with the necessary strokes
 * - Unless there is only one element in the buffer, names are also tested to check for compatibility.
 * - All pasted frames are offset by the same amount. This is calculated as the difference in the times of
 *	the current frame and the 'first keyframe' (i.e. the earliest one in all channels).
 * - The earliest frame is calculated per copy operation.
 */
 
/* globals for copy/paste data (like for other copy/paste buffers) */
ListBase gpcopybuf = {NULL, NULL};
static int gpcopy_firstframe= 999999999;

/* This function frees any MEM_calloc'ed copy/paste buffer data */
void free_gpcopybuf ()
{
	free_gpencil_layers(&gpcopybuf); 
	
	gpcopybuf.first= gpcopybuf.last= NULL;
	gpcopy_firstframe= 999999999;
}

/* This function adds data to the copy/paste buffer, freeing existing data first
 * Only the selected GP-layers get their selected keyframes copied.
 */
void copy_gpdata ()
{
	ListBase act_data = {NULL, NULL};
	bActListElem *ale;
	int filter;
	void *data;
	short datatype;
	
	/* clear buffer first */
	free_gpcopybuf();
	
	/* get data */
	data= get_action_context(&datatype);
	if (data == NULL) return;
	if (datatype != ACTCONT_GPENCIL) return;
	
	/* filter data */
	filter= (ACTFILTER_VISIBLE | ACTFILTER_SEL);
	actdata_filter(&act_data, filter, data, datatype);
	
	/* assume that each of these is an ipo-block */
	for (ale= act_data.first; ale; ale= ale->next) {
		bGPDlayer *gpls, *gpln;
		bGPDframe *gpf, *gpfn;
		
		/* get new layer to put into buffer */
		gpls= (bGPDlayer *)ale->data;
		gpln= MEM_callocN(sizeof(bGPDlayer), "GPCopyPasteLayer");
		
		gpln->frames.first= gpln->frames.last= NULL;
		BLI_strncpy(gpln->info, gpls->info, sizeof(gpln->info));
		
		BLI_addtail(&gpcopybuf, gpln);
		
		/* loop over frames, and copy only selected frames */
		for (gpf= gpls->frames.first; gpf; gpf= gpf->next) {
			/* if frame is selected, make duplicate it and its strokes */
			if (gpf->flag & GP_FRAME_SELECT) {
				/* add frame to buffer */
				gpfn= gpencil_frame_duplicate(gpf);
				BLI_addtail(&gpln->frames, gpfn);
				
				/* check if this is the earliest frame encountered so far */
				if (gpf->framenum < gpcopy_firstframe)
					gpcopy_firstframe= gpf->framenum;
			}
		}
	}
	
	/* check if anything ended up in the buffer */
	if (ELEM(NULL, gpcopybuf.first, gpcopybuf.last))
		error("Nothing copied to buffer");
	
	/* free temp memory */
	BLI_freelistN(&act_data);
}

void paste_gpdata (Scene *scene)
{
	ListBase act_data = {NULL, NULL};
	bActListElem *ale;
	int filter;
	void *data;
	short datatype;
	
	const int offset = (CFRA - gpcopy_firstframe);
	short no_name= 0;
	
	/* check if buffer is empty */
	if (ELEM(NULL, gpcopybuf.first, gpcopybuf.last)) {
		error("No data in buffer to paste");
		return;
	}
	/* check if single channel in buffer (disregard names if so)  */
	if (gpcopybuf.first == gpcopybuf.last)
		no_name= 1;
	
	/* get data */
	data= get_action_context(&datatype);
	if (data == NULL) return;
	if (datatype != ACTCONT_GPENCIL) return;
	
	/* filter data */
	filter= (ACTFILTER_VISIBLE | ACTFILTER_SEL | ACTFILTER_FOREDIT);
	actdata_filter(&act_data, filter, data, datatype);
	
	/* from selected channels */
	for (ale= act_data.first; ale; ale= ale->next) {
		bGPDlayer *gpld= (bGPDlayer *)ale->data;
		bGPDlayer *gpls= NULL;
		bGPDframe *gpfs, *gpf;
		
		/* find suitable layer from buffer to use to paste from */
		for (gpls= gpcopybuf.first; gpls; gpls= gpls->next) {
			/* check if layer name matches */
			if ((no_name) || (strcmp(gpls->info, gpld->info)==0))
				break;
		}
		
		/* this situation might occur! */
		if (gpls == NULL)
			continue;
		
		/* add frames from buffer */
		for (gpfs= gpls->frames.first; gpfs; gpfs= gpfs->next) {
			/* temporarily apply offset to buffer-frame while copying */
			gpfs->framenum += offset;
			
			/* get frame to copy data into (if no frame returned, then just ignore) */
			gpf= gpencil_layer_getframe(gpld, gpfs->framenum, 1);
			if (gpf) {
				bGPDstroke *gps, *gpsn;
				ScrArea *sa;
				
				/* get area that gp-data comes from */
				//sa= gpencil_data_findowner((bGPdata *)ale->owner);	
				sa = NULL;
				
				/* this should be the right frame... as it may be a pre-existing frame, 
				 * must make sure that only compatible stroke types get copied over 
				 *	- we cannot just add a duplicate frame, as that would cause errors
				 *	- need to check for compatible types to minimise memory usage (copying 'junk' over)
				 */
				for (gps= gpfs->strokes.first; gps; gps= gps->next) {
					short stroke_ok;
					
					/* if there's an area, check that it supports this type of stroke */
					if (sa) {
						stroke_ok= 0;
						
						/* check if spacetype supports this type of stroke
						 *	- NOTE: must sync this with gp_paint_initstroke() in gpencil.c
						 */
						switch (sa->spacetype) {
							case SPACE_VIEW3D: /* 3D-View: either screen-aligned or 3d-space */
								if ((gps->flag == 0) || (gps->flag & GP_STROKE_3DSPACE))
									stroke_ok= 1;
								break;
								
							case SPACE_NODE: /* Nodes Editor: either screen-aligned or view-aligned */
							case SPACE_IMAGE: /* Image Editor: either screen-aligned or view\image-aligned */
							case SPACE_CLIP: /* Image Editor: either screen-aligned or view\image-aligned */
								if ((gps->flag == 0) || (gps->flag & GP_STROKE_2DSPACE))
									stroke_ok= 1;
								break;
								
							case SPACE_SEQ: /* Sequence Editor: either screen-aligned or view-aligned */
								if ((gps->flag == 0) || (gps->flag & GP_STROKE_2DIMAGE))
									stroke_ok= 1;
								break;
						}
					}
					else
						stroke_ok= 1;
					
					/* if stroke is ok, we make a copy of this stroke and add to frame */
					if (stroke_ok) {
						/* make a copy of stroke, then of its points array */
						gpsn= MEM_dupallocN(gps);
						gpsn->points= MEM_dupallocN(gps->points);
						
						/* append stroke to frame */
						BLI_addtail(&gpf->strokes, gpsn);
					}
				}
				
				/* if no strokes (i.e. new frame) added, free gpf */
				if (gpf->strokes.first == NULL)
					gpencil_layer_delframe(gpld, gpf);
			}
			
			/* unapply offset from buffer-frame */
			gpfs->framenum -= offset;
		}
	}
	
	/* free temp memory */
	BLI_freelistN(&act_data);
	
	/* undo and redraw stuff */
	BIF_undo_push("Paste Grease Pencil Frames");
}

/* -------------------------------------- */
/* Snap Tools */

static short snap_gpf_nearest (bGPDframe *gpf, Scene *scene)
{
	if (gpf->flag & GP_FRAME_SELECT)
		gpf->framenum= (int)(floor(gpf->framenum+0.5));
	return 0;
}

static short snap_gpf_nearestsec (bGPDframe *gpf, Scene *scene)
{
	float secf = (float)FPS;
	if (gpf->flag & GP_FRAME_SELECT)
		gpf->framenum= (int)(floor(gpf->framenum/secf + 0.5f) * secf);
	return 0;
}

static short snap_gpf_cframe (bGPDframe *gpf, Scene *scene)
{
	if (gpf->flag & GP_FRAME_SELECT)
		gpf->framenum= (int)CFRA;
	return 0;
}

static short snap_gpf_nearmarker (bGPDframe *gpf, Scene *scene)
{
	if (gpf->flag & GP_FRAME_SELECT)
		gpf->framenum= (int)find_nearest_marker_time(&scene->markers, (float)gpf->framenum);
	return 0;
}


/* snap selected frames to ... */
void snap_gplayer_frames (bGPDlayer *gpl, Scene *scene, short mode)
{
	switch (mode) {
		case 1: /* snap to nearest frame */
			gplayer_frames_looper(gpl, scene, snap_gpf_nearest);
			break;
		case 2: /* snap to current frame */
			gplayer_frames_looper(gpl, scene, snap_gpf_cframe);
			break;
		case 3: /* snap to nearest marker */
			gplayer_frames_looper(gpl, scene, snap_gpf_nearmarker);
			break;
		case 4: /* snap to nearest second */
			gplayer_frames_looper(gpl, scene, snap_gpf_nearestsec);
			break;
		default: /* just in case */
			gplayer_frames_looper(gpl, scene, snap_gpf_nearest);
			break;
	}
}

/* -------------------------------------- */
/* Mirror Tools */

static short mirror_gpf_cframe (bGPDframe *gpf, Scene *scene)
{
	int diff;
	
	if (gpf->flag & GP_FRAME_SELECT) {
		diff= CFRA - gpf->framenum;
		gpf->framenum= CFRA;
	}
	
	return 0;
}

static short mirror_gpf_yaxis (bGPDframe *gpf, Scene *scene)
{
	int diff;
	
	if (gpf->flag & GP_FRAME_SELECT) {
		diff= -gpf->framenum;
		gpf->framenum= diff;
	}
	
	return 0;
}

static short mirror_gpf_xaxis (bGPDframe *gpf, Scene *scene)
{
	int diff;
	
	if (gpf->flag & GP_FRAME_SELECT) {
		diff= -gpf->framenum;
		gpf->framenum= diff;
	}
	
	return 0;
}

static short mirror_gpf_marker (bGPDframe *gpf, Scene *scene)
{
	static TimeMarker *marker;
	static short initialized = 0;
	int diff;
	
	/* In order for this mirror function to work without
	 * any extra arguments being added, we use the case
	 * of bezt==NULL to denote that we should find the 
	 * marker to mirror over. The static pointer is safe
	 * to use this way, as it will be set to null after 
	 * each cycle in which this is called.
	 */
	
	if (gpf) {
		/* mirroring time */
		if ((gpf->flag & GP_FRAME_SELECT) && (marker)) {
			diff= (marker->frame - gpf->framenum);
			gpf->framenum= (marker->frame + diff);
		}
	}
	else {
		/* initialization time */
		if (initialized) {
			/* reset everything for safety */
			marker = NULL;
			initialized = 0;
		}
		else {
			/* try to find a marker */
			marker= ED_markers_get_first_selected(&scene->markers);
			if(marker) {
				initialized= 1;
			}
		}
	}
	
	return 0;
}


/* mirror selected gp-frames on... */
void mirror_gplayer_frames (bGPDlayer *gpl, Scene *scene, short mode)
{
	switch (mode) {
		case 1: /* mirror over current frame */
			gplayer_frames_looper(gpl, scene, mirror_gpf_cframe);
			break;
		case 2: /* mirror over frame 0 */
			gplayer_frames_looper(gpl, scene, mirror_gpf_yaxis);
			break;
		case 3: /* mirror over value 0 */
			gplayer_frames_looper(gpl, scene, mirror_gpf_xaxis);
			break;
		case 4: /* mirror over marker */
			mirror_gpf_marker(NULL, NULL);
			gplayer_frames_looper(gpl, scene, mirror_gpf_marker);
			mirror_gpf_marker(NULL, NULL);
			break;
		default: /* just in case */
			gplayer_frames_looper(gpl, scene, mirror_gpf_yaxis);
			break;
	}
}

/* ***************************************** */
#endif // XXX disabled until Grease Pencil code stabilises again...
