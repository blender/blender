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
#include "BLI_utildefines.h"

#include "DNA_gpencil_types.h"
#include "DNA_scene_types.h"

#include "BKE_fcurve.h"
#include "BKE_gpencil.h"
#include "BKE_report.h"

#include "ED_anim_api.h"
#include "ED_gpencil.h"
#include "ED_keyframes_edit.h"
#include "ED_markers.h"

#include "WM_api.h"

/* ***************************************** */
/* NOTE ABOUT THIS FILE:
 *  This file contains code for editing Grease Pencil data in the Action Editor
 *  as a 'keyframes', so that a user can adjust the timing of Grease Pencil drawings.
 *  Therefore, this file mostly contains functions for selecting Grease-Pencil frames.
 */
/* ***************************************** */
/* Generics - Loopers */

/* Loops over the gp-frames for a gp-layer, and applies the given callback */
bool ED_gplayer_frames_looper(bGPDlayer *gpl, Scene *scene, short (*gpf_cb)(bGPDframe *, Scene *))
{
	bGPDframe *gpf;

	/* error checker */
	if (gpl == NULL)
		return false;

	/* do loop */
	for (gpf = gpl->frames.first; gpf; gpf = gpf->next) {
		/* execute callback */
		if (gpf_cb(gpf, scene))
			return true;
	}

	/* nothing to return */
	return false;
}

/* ****************************************** */
/* Data Conversion Tools */

/* make a listing all the gp-frames in a layer as cfraelems */
void ED_gplayer_make_cfra_list(bGPDlayer *gpl, ListBase *elems, bool onlysel)
{
	bGPDframe *gpf;
	CfraElem *ce;

	/* error checking */
	if (ELEM(NULL, gpl, elems))
		return;

	/* loop through gp-frames, adding */
	for (gpf = gpl->frames.first; gpf; gpf = gpf->next) {
		if ((onlysel == 0) || (gpf->flag & GP_FRAME_SELECT)) {
			ce = MEM_callocN(sizeof(CfraElem), "CfraElem");

			ce->cfra = (float)gpf->framenum;
			ce->sel = (gpf->flag & GP_FRAME_SELECT) ? 1 : 0;

			BLI_addtail(elems, ce);
		}
	}
}

/* ***************************************** */
/* Selection Tools */

/* check if one of the frames in this layer is selected */
bool ED_gplayer_frame_select_check(bGPDlayer *gpl)
{
	bGPDframe *gpf;

	/* error checking */
	if (gpl == NULL)
		return false;

	/* stop at the first one found */
	for (gpf = gpl->frames.first; gpf; gpf = gpf->next) {
		if (gpf->flag & GP_FRAME_SELECT)
			return true;
	}

	/* not found */
	return false;
}

/* helper function - select gp-frame based on SELECT_* mode */
static void gpframe_select(bGPDframe *gpf, short select_mode)
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
void ED_gpencil_select_frames(bGPDlayer *gpl, short select_mode)
{
	bGPDframe *gpf;

	/* error checking */
	if (gpl == NULL)
		return;

	/* handle according to mode */
	for (gpf = gpl->frames.first; gpf; gpf = gpf->next) {
		gpframe_select(gpf, select_mode);
	}
}

/* set all/none/invert select */
void ED_gplayer_frame_select_set(bGPDlayer *gpl, short mode)
{
	/* error checking */
	if (gpl == NULL)
		return;

	/* now call the standard function */
	ED_gpencil_select_frames(gpl, mode);
}

/* select the frame in this layer that occurs on this frame (there should only be one at most) */
void ED_gpencil_select_frame(bGPDlayer *gpl, int selx, short select_mode)
{
	bGPDframe *gpf;

	if (gpl == NULL)
		return;

	gpf = BKE_gpencil_layer_find_frame(gpl, selx);

	if (gpf) {
		gpframe_select(gpf, select_mode);
	}
}

/* select the frames in this layer that occur within the bounds specified */
void ED_gplayer_frames_select_border(bGPDlayer *gpl, float min, float max, short select_mode)
{
	bGPDframe *gpf;

	if (gpl == NULL)
		return;

	/* only select those frames which are in bounds */
	for (gpf = gpl->frames.first; gpf; gpf = gpf->next) {
		if (IN_RANGE(gpf->framenum, min, max))
			gpframe_select(gpf, select_mode);
	}
}

/* select the frames in this layer that occur within the lasso/circle region specified */
void ED_gplayer_frames_select_region(KeyframeEditData *ked, bGPDlayer *gpl, short tool, short select_mode)
{
	bGPDframe *gpf;

	if (gpl == NULL)
		return;

	/* only select frames which are within the region */
	for (gpf = gpl->frames.first; gpf; gpf = gpf->next) {
		/* construct a dummy point coordinate to do this testing with */
		float pt[2] = {0};

		pt[0] = gpf->framenum;
		pt[1] = ked->channel_y;

		/* check the necessary regions */
		if (tool == BEZT_OK_CHANNEL_LASSO) {
			/* Lasso */
			if (keyframe_region_lasso_test(ked->data, pt))
				gpframe_select(gpf, select_mode);
		}
		else if (tool == BEZT_OK_CHANNEL_CIRCLE) {
			/* Circle */
			if (keyframe_region_circle_test(ked->data, pt))
				gpframe_select(gpf, select_mode);
		}
	}
}

/* ***************************************** */
/* Frame Editing Tools */

/* Delete selected frames */
bool ED_gplayer_frames_delete(bGPDlayer *gpl)
{
	bGPDframe *gpf, *gpfn;
	bool changed = false;

	/* error checking */
	if (gpl == NULL)
		return false;

	/* check for frames to delete */
	for (gpf = gpl->frames.first; gpf; gpf = gpfn) {
		gpfn = gpf->next;

		if (gpf->flag & GP_FRAME_SELECT) {
			BKE_gpencil_layer_delframe(gpl, gpf);
			changed = true;
		}
	}

	return changed;
}

/* Duplicate selected frames from given gp-layer */
void ED_gplayer_frames_duplicate(bGPDlayer *gpl)
{
	bGPDframe *gpf, *gpfn;

	/* error checking */
	if (gpl == NULL)
		return;

	/* duplicate selected frames  */
	for (gpf = gpl->frames.first; gpf; gpf = gpfn) {
		gpfn = gpf->next;

		/* duplicate this frame */
		if (gpf->flag & GP_FRAME_SELECT) {
			bGPDframe *gpfd;

			/* duplicate frame, and deselect self */
			gpfd = BKE_gpencil_frame_duplicate(gpf);
			gpf->flag &= ~GP_FRAME_SELECT;

			BLI_insertlinkafter(&gpl->frames, gpf, gpfd);
		}
	}
}

/* Set keyframe type for selected frames from given gp-layer
 * \param type The type of keyframe (eBezTriple_KeyframeType) to set selected frames to
 */
void ED_gplayer_frames_keytype_set(bGPDlayer *gpl, short type)
{
	bGPDframe *gpf;

	if (gpl == NULL)
		return;

	for (gpf = gpl->frames.first; gpf; gpf = gpf->next) {
		if (gpf->flag & GP_FRAME_SELECT) {
			gpf->key_type = type;
		}
	}
}


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
static ListBase gp_anim_copybuf = {NULL, NULL};
static int gp_anim_copy_firstframe =  999999999;
static int gp_anim_copy_lastframe  = -999999999;
static int gp_anim_copy_cfra       =  0;


/* This function frees any MEM_calloc'ed copy/paste buffer data */
void ED_gpencil_anim_copybuf_free(void)
{
	BKE_gpencil_free_layers(&gp_anim_copybuf);
	BLI_listbase_clear(&gp_anim_copybuf);

	gp_anim_copy_firstframe =  999999999;
	gp_anim_copy_lastframe  = -999999999;
	gp_anim_copy_cfra       =  0;
}


/* This function adds data to the copy/paste buffer, freeing existing data first
 * Only the selected GP-layers get their selected keyframes copied.
 *
 * Returns whether the copy operation was successful or not
 */
bool ED_gpencil_anim_copybuf_copy(bAnimContext *ac)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;

	Scene *scene = ac->scene;


	/* clear buffer first */
	ED_gpencil_anim_copybuf_free();

	/* filter data */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);

	/* assume that each of these is a GP layer */
	for (ale = anim_data.first; ale; ale = ale->next) {
		ListBase copied_frames = {NULL, NULL};
		bGPDlayer *gpl = (bGPDlayer *)ale->data;
		bGPDframe *gpf;

		/* loop over frames, and copy only selected frames */
		for (gpf = gpl->frames.first; gpf; gpf = gpf->next) {
			/* if frame is selected, make duplicate it and its strokes */
			if (gpf->flag & GP_FRAME_SELECT) {
				/* make a copy of this frame */
				bGPDframe *new_frame = BKE_gpencil_frame_duplicate(gpf);
				BLI_addtail(&copied_frames, new_frame);

				/* extend extents for keyframes encountered */
				if (gpf->framenum  < gp_anim_copy_firstframe)
					gp_anim_copy_firstframe = gpf->framenum;
				if (gpf->framenum > gp_anim_copy_lastframe)
					gp_anim_copy_lastframe = gpf->framenum;
			}
		}

		/* create a new layer in buffer if there were keyframes here */
		if (BLI_listbase_is_empty(&copied_frames) == false) {
			bGPDlayer *new_layer = MEM_callocN(sizeof(bGPDlayer), "GPCopyPasteLayer");
			BLI_addtail(&gp_anim_copybuf, new_layer);

			/* move over copied frames */
			BLI_movelisttolist(&new_layer->frames, &copied_frames);
			BLI_assert(copied_frames.first == NULL);

			/* make a copy of the layer's name - for name-based matching later... */
			BLI_strncpy(new_layer->info, gpl->info, sizeof(new_layer->info));
		}
	}

	/* in case 'relative' paste method is used */
	gp_anim_copy_cfra = CFRA;

	/* clean up */
	ANIM_animdata_freelist(&anim_data);

	/* check if anything ended up in the buffer */
	if (ELEM(NULL, gp_anim_copybuf.first, gp_anim_copybuf.last)) {
		BKE_report(ac->reports, RPT_ERROR, "No keyframes copied to keyframes copy/paste buffer");
		return false;
	}

	/* report success */
	return true;
}


/* Pastes keyframes from buffer, and reports success */
bool ED_gpencil_anim_copybuf_paste(bAnimContext *ac, const short offset_mode)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;

	Scene *scene = ac->scene;
	bool no_name = false;
	int offset = 0;

	/* check if buffer is empty */
	if (BLI_listbase_is_empty(&gp_anim_copybuf)) {
		BKE_report(ac->reports, RPT_ERROR, "No data in buffer to paste");
		return false;
	}

	/* check if single channel in buffer (disregard names if so)  */
	if (gp_anim_copybuf.first == gp_anim_copybuf.last) {
		no_name = true;
	}

	/* methods of offset (eKeyPasteOffset) */
	switch (offset_mode) {
		case KEYFRAME_PASTE_OFFSET_CFRA_START:
			offset = (CFRA - gp_anim_copy_firstframe);
			break;
		case KEYFRAME_PASTE_OFFSET_CFRA_END:
			offset = (CFRA - gp_anim_copy_lastframe);
			break;
		case KEYFRAME_PASTE_OFFSET_CFRA_RELATIVE:
			offset = (CFRA - gp_anim_copy_cfra);
			break;
		case KEYFRAME_PASTE_OFFSET_NONE:
			offset = 0;
			break;
	}


	/* filter data */
	// TODO: try doing it with selection, then without selection imits
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_SEL | ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);

	/* from selected channels */
	for (ale = anim_data.first; ale; ale = ale->next) {
		bGPDlayer *gpld = (bGPDlayer *)ale->data;
		bGPDlayer *gpls = NULL;
		bGPDframe *gpfs, *gpf;


		/* find suitable layer from buffer to use to paste from */
		for (gpls = gp_anim_copybuf.first; gpls; gpls = gpls->next) {
			/* check if layer name matches */
			if ((no_name) || STREQ(gpls->info, gpld->info)) {
				break;
			}
		}

		/* this situation might occur! */
		if (gpls == NULL)
			continue;

		/* add frames from buffer */
		for (gpfs = gpls->frames.first; gpfs; gpfs = gpfs->next) {
			/* temporarily apply offset to buffer-frame while copying */
			gpfs->framenum += offset;

			/* get frame to copy data into (if no frame returned, then just ignore) */
			gpf = BKE_gpencil_layer_getframe(gpld, gpfs->framenum, 1);
			if (gpf) {
				bGPDstroke *gps, *gpsn;

				/* This should be the right frame... as it may be a pre-existing frame,
				 * must make sure that only compatible stroke types get copied over
				 *	- We cannot just add a duplicate frame, as that would cause errors
				 *  - For now, we don't check if the types will be compatible since we
				 *    don't have enough info to do so. Instead, we simply just paste,
				 *    af it works, it will show up.
				 */
				for (gps = gpfs->strokes.first; gps; gps = gps->next) {
					/* make a copy of stroke, then of its points array */
					gpsn = MEM_dupallocN(gps);
					gpsn->points = MEM_dupallocN(gps->points);
					gpsn->dvert = MEM_dupallocN(gps->dvert);
					BKE_gpencil_stroke_weights_duplicate(gps, gpsn);
					/* duplicate triangle information */
					gpsn->triangles = MEM_dupallocN(gps->triangles);
					/* append stroke to frame */
					BLI_addtail(&gpf->strokes, gpsn);
				}

				/* if no strokes (i.e. new frame) added, free gpf */
				if (BLI_listbase_is_empty(&gpf->strokes))
					BKE_gpencil_layer_delframe(gpld, gpf);
			}

			/* unapply offset from buffer-frame */
			gpfs->framenum -= offset;
		}
	}

	/* clean up */
	ANIM_animdata_freelist(&anim_data);
	return true;
}

/* -------------------------------------- */
/* Snap Tools */

static short snap_gpf_nearest(bGPDframe *UNUSED(gpf), Scene *UNUSED(scene))
{
#if 0 /* note: gpf->framenum is already an int! */
	if (gpf->flag & GP_FRAME_SELECT)
		gpf->framenum = (int)(floor(gpf->framenum + 0.5));
#endif
	return 0;
}

static short snap_gpf_nearestsec(bGPDframe *gpf, Scene *scene)
{
	float secf = (float)FPS;
	if (gpf->flag & GP_FRAME_SELECT)
		gpf->framenum = (int)(floorf(gpf->framenum / secf + 0.5f) * secf);
	return 0;
}

static short snap_gpf_cframe(bGPDframe *gpf, Scene *scene)
{
	if (gpf->flag & GP_FRAME_SELECT)
		gpf->framenum = (int)CFRA;
	return 0;
}

static short snap_gpf_nearmarker(bGPDframe *gpf, Scene *scene)
{
	if (gpf->flag & GP_FRAME_SELECT)
		gpf->framenum = (int)ED_markers_find_nearest_marker_time(&scene->markers, (float)gpf->framenum);
	return 0;
}

/* snap selected frames to ... */
void ED_gplayer_snap_frames(bGPDlayer *gpl, Scene *scene, short mode)
{
	switch (mode) {
		case SNAP_KEYS_NEARFRAME: /* snap to nearest frame */
			ED_gplayer_frames_looper(gpl, scene, snap_gpf_nearest);
			break;
		case SNAP_KEYS_CURFRAME: /* snap to current frame */
			ED_gplayer_frames_looper(gpl, scene, snap_gpf_cframe);
			break;
		case SNAP_KEYS_NEARMARKER: /* snap to nearest marker */
			ED_gplayer_frames_looper(gpl, scene, snap_gpf_nearmarker);
			break;
		case SNAP_KEYS_NEARSEC: /* snap to nearest second */
			ED_gplayer_frames_looper(gpl, scene, snap_gpf_nearestsec);
			break;
		default: /* just in case */
			break;
	}
}

/* -------------------------------------- */
/* Mirror Tools */

static short mirror_gpf_cframe(bGPDframe *gpf, Scene *scene)
{
	int diff;

	if (gpf->flag & GP_FRAME_SELECT) {
		diff = CFRA - gpf->framenum;
		gpf->framenum = CFRA + diff;
	}

	return 0;
}

static short mirror_gpf_yaxis(bGPDframe *gpf, Scene *UNUSED(scene))
{
	int diff;

	if (gpf->flag & GP_FRAME_SELECT) {
		diff = -gpf->framenum;
		gpf->framenum = diff;
	}

	return 0;
}

static short mirror_gpf_xaxis(bGPDframe *gpf, Scene *UNUSED(scene))
{
	int diff;

	/* NOTE: since we can't really do this, we just do the same as for yaxis... */
	if (gpf->flag & GP_FRAME_SELECT) {
		diff = -gpf->framenum;
		gpf->framenum = diff;
	}

	return 0;
}

static short mirror_gpf_marker(bGPDframe *gpf, Scene *scene)
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
			diff = (marker->frame - gpf->framenum);
			gpf->framenum = (marker->frame + diff);
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
			marker = ED_markers_get_first_selected(&scene->markers);
			if (marker) {
				initialized = 1;
			}
		}
	}

	return 0;
}


/* mirror selected gp-frames on... */
// TODO: mirror over a specific time
void ED_gplayer_mirror_frames(bGPDlayer *gpl, Scene *scene, short mode)
{
	switch (mode) {
		case MIRROR_KEYS_CURFRAME: /* mirror over current frame */
			ED_gplayer_frames_looper(gpl, scene, mirror_gpf_cframe);
			break;
		case MIRROR_KEYS_YAXIS: /* mirror over frame 0 */
			ED_gplayer_frames_looper(gpl, scene, mirror_gpf_yaxis);
			break;
		case MIRROR_KEYS_XAXIS: /* mirror over value 0 */
			ED_gplayer_frames_looper(gpl, scene, mirror_gpf_xaxis);
			break;
		case MIRROR_KEYS_MARKER: /* mirror over marker */
			mirror_gpf_marker(NULL, NULL);
			ED_gplayer_frames_looper(gpl, scene, mirror_gpf_marker);
			mirror_gpf_marker(NULL, NULL);
			break;
		default: /* just in case */
			ED_gplayer_frames_looper(gpl, scene, mirror_gpf_yaxis);
			break;
	}
}

/* ***************************************** */
