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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/mask/mask_editaction.c
 *  \ingroup edmask
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

#include "DNA_mask_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_fcurve.h"
#include "BKE_mask.h"

#include "ED_anim_api.h"
#include "ED_keyframes_edit.h"
#include "ED_mask.h"  /* own include */
#include "ED_markers.h"

/* ***************************************** */
/* NOTE ABOUT THIS FILE:
 *  This file contains code for editing Mask data in the Action Editor
 *  as a 'keyframes', so that a user can adjust the timing of Mask shapekeys.
 *  Therefore, this file mostly contains functions for selecting Mask frames (shapekeys).
 */
/* ***************************************** */
/* Generics - Loopers */

/* Loops over the mask-frames for a mask-layer, and applies the given callback */
short ED_masklayer_frames_looper(MaskLayer *masklay, Scene *scene, short (*masklay_shape_cb)(MaskLayerShape *, Scene *))
{
	MaskLayerShape *masklay_shape;

	/* error checker */
	if (masklay == NULL)
		return 0;

	/* do loop */
	for (masklay_shape = masklay->splines_shapes.first; masklay_shape; masklay_shape = masklay_shape->next) {
		/* execute callback */
		if (masklay_shape_cb(masklay_shape, scene))
			return 1;
	}

	/* nothing to return */
	return 0;
}

/* ****************************************** */
/* Data Conversion Tools */

/* make a listing all the mask-frames in a layer as cfraelems */
void ED_masklayer_make_cfra_list(MaskLayer *masklay, ListBase *elems, short onlysel)
{
	MaskLayerShape *masklay_shape;
	CfraElem *ce;

	/* error checking */
	if (ELEM(NULL, masklay, elems))
		return;

	/* loop through mask-frames, adding */
	for (masklay_shape = masklay->splines_shapes.first; masklay_shape; masklay_shape = masklay_shape->next) {
		if ((onlysel == 0) || (masklay_shape->flag & MASK_SHAPE_SELECT)) {
			ce = MEM_callocN(sizeof(CfraElem), "CfraElem");

			ce->cfra = (float)masklay_shape->frame;
			ce->sel = (masklay_shape->flag & MASK_SHAPE_SELECT) ? 1 : 0;

			BLI_addtail(elems, ce);
		}
	}
}

/* ***************************************** */
/* Selection Tools */

/* check if one of the frames in this layer is selected */
bool ED_masklayer_frame_select_check(MaskLayer *masklay)
{
	MaskLayerShape *masklay_shape;

	/* error checking */
	if (masklay == NULL)
		return 0;

	/* stop at the first one found */
	for (masklay_shape = masklay->splines_shapes.first; masklay_shape; masklay_shape = masklay_shape->next) {
		if (masklay_shape->flag & MASK_SHAPE_SELECT)
			return 1;
	}

	/* not found */
	return 0;
}

/* helper function - select mask-frame based on SELECT_* mode */
static void masklayshape_select(MaskLayerShape *masklay_shape, short select_mode)
{
	if (masklay_shape == NULL)
		return;

	switch (select_mode) {
		case SELECT_ADD:
			masklay_shape->flag |= MASK_SHAPE_SELECT;
			break;
		case SELECT_SUBTRACT:
			masklay_shape->flag &= ~MASK_SHAPE_SELECT;
			break;
		case SELECT_INVERT:
			masklay_shape->flag ^= MASK_SHAPE_SELECT;
			break;
	}
}

/* set all/none/invert select (like above, but with SELECT_* modes) */
void ED_mask_select_frames(MaskLayer *masklay, short select_mode)
{
	MaskLayerShape *masklay_shape;

	/* error checking */
	if (masklay == NULL)
		return;

	/* handle according to mode */
	for (masklay_shape = masklay->splines_shapes.first; masklay_shape; masklay_shape = masklay_shape->next) {
		masklayshape_select(masklay_shape, select_mode);
	}
}

/* set all/none/invert select */
void ED_masklayer_frame_select_set(MaskLayer *masklay, short mode)
{
	/* error checking */
	if (masklay == NULL)
		return;

	/* now call the standard function */
	ED_mask_select_frames(masklay, mode);
}

/* select the frame in this layer that occurs on this frame (there should only be one at most) */
void ED_mask_select_frame(MaskLayer *masklay, int selx, short select_mode)
{
	MaskLayerShape *masklay_shape;

	if (masklay == NULL)
		return;

	masklay_shape = BKE_mask_layer_shape_find_frame(masklay, selx);

	if (masklay_shape) {
		masklayshape_select(masklay_shape, select_mode);
	}
}

/* select the frames in this layer that occur within the bounds specified */
void ED_masklayer_frames_select_border(MaskLayer *masklay, float min, float max, short select_mode)
{
	MaskLayerShape *masklay_shape;

	if (masklay == NULL)
		return;

	/* only select those frames which are in bounds */
	for (masklay_shape = masklay->splines_shapes.first; masklay_shape; masklay_shape = masklay_shape->next) {
		if (IN_RANGE(masklay_shape->frame, min, max))
			masklayshape_select(masklay_shape, select_mode);
	}
}

/* ***************************************** */
/* Frame Editing Tools */

/* Delete selected frames */
bool ED_masklayer_frames_delete(MaskLayer *masklay)
{
	MaskLayerShape *masklay_shape, *masklay_shape_next;
	bool changed = false;

	/* error checking */
	if (masklay == NULL)
		return false;

	/* check for frames to delete */
	for (masklay_shape = masklay->splines_shapes.first; masklay_shape; masklay_shape = masklay_shape_next) {
		masklay_shape_next = masklay_shape->next;

		if (masklay_shape->flag & MASK_SHAPE_SELECT) {
			BKE_mask_layer_shape_unlink(masklay, masklay_shape);
			changed = true;
		}
	}

	return changed;
}

/* Duplicate selected frames from given mask-layer */
void ED_masklayer_frames_duplicate(MaskLayer *masklay)
{
	MaskLayerShape *masklay_shape, *gpfn;

	/* error checking */
	if (masklay == NULL)
		return;

	/* duplicate selected frames  */
	for (masklay_shape = masklay->splines_shapes.first; masklay_shape; masklay_shape = gpfn) {
		gpfn = masklay_shape->next;

		/* duplicate this frame */
		if (masklay_shape->flag & MASK_SHAPE_SELECT) {
			MaskLayerShape *mask_shape_dupe;

			/* duplicate frame, and deselect self */
			mask_shape_dupe = BKE_mask_layer_shape_duplicate(masklay_shape);
			masklay_shape->flag &= ~MASK_SHAPE_SELECT;

			/* XXX - how to handle duplicate frames? */
			BLI_insertlinkafter(&masklay->splines_shapes, masklay_shape, mask_shape_dupe);
		}
	}
}

/* -------------------------------------- */
/* Snap Tools */

static short snap_masklayer_nearest(MaskLayerShape *masklay_shape, Scene *UNUSED(scene))
{
	if (masklay_shape->flag & MASK_SHAPE_SELECT)
		masklay_shape->frame = (int)(floor(masklay_shape->frame + 0.5));
	return 0;
}

static short snap_masklayer_nearestsec(MaskLayerShape *masklay_shape, Scene *scene)
{
	float secf = (float)FPS;
	if (masklay_shape->flag & MASK_SHAPE_SELECT)
		masklay_shape->frame = (int)(floorf(masklay_shape->frame / secf + 0.5f) * secf);
	return 0;
}

static short snap_masklayer_cframe(MaskLayerShape *masklay_shape, Scene *scene)
{
	if (masklay_shape->flag & MASK_SHAPE_SELECT)
		masklay_shape->frame = (int)CFRA;
	return 0;
}

static short snap_masklayer_nearmarker(MaskLayerShape *masklay_shape, Scene *scene)
{
	if (masklay_shape->flag & MASK_SHAPE_SELECT)
		masklay_shape->frame = (int)ED_markers_find_nearest_marker_time(&scene->markers, (float)masklay_shape->frame);
	return 0;
}

/* snap selected frames to ... */
void ED_masklayer_snap_frames(MaskLayer *masklay, Scene *scene, short mode)
{
	switch (mode) {
		case SNAP_KEYS_NEARFRAME: /* snap to nearest frame */
			ED_masklayer_frames_looper(masklay, scene, snap_masklayer_nearest);
			break;
		case SNAP_KEYS_CURFRAME: /* snap to current frame */
			ED_masklayer_frames_looper(masklay, scene, snap_masklayer_cframe);
			break;
		case SNAP_KEYS_NEARMARKER: /* snap to nearest marker */
			ED_masklayer_frames_looper(masklay, scene, snap_masklayer_nearmarker);
			break;
		case SNAP_KEYS_NEARSEC: /* snap to nearest second */
			ED_masklayer_frames_looper(masklay, scene, snap_masklayer_nearestsec);
			break;
		default: /* just in case */
			break;
	}
}

