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

/** \file blender/editors/mask/mask_editaction.c
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

#include "DNA_mask_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_fcurve.h"
#include "BKE_mask.h"

#include "ED_anim_api.h"
#include "ED_keyframes_edit.h"

/* ***************************************** */
/* NOTE ABOUT THIS FILE:
 *  This file contains code for editing Grease Pencil data in the Action Editor
 *  as a 'keyframes', so that a user can adjust the timing of Grease Pencil drawings.
 *  Therefore, this file mostly contains functions for selecting Grease-Pencil frames.
 */
/* ***************************************** */
/* Generics - Loopers */

/* Loops over the gp-frames for a gp-layer, and applies the given callback */
short masklayer_frames_looper(MaskLayer *masklay, Scene *scene, short (*masklay_shape_cb)(MaskLayerShape *, Scene *))
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

/* make a listing all the gp-frames in a layer as cfraelems */
void masklayer_make_cfra_list(MaskLayer *masklay, ListBase *elems, short onlysel)
{
	MaskLayerShape *masklay_shape;
	CfraElem *ce;

	/* error checking */
	if (ELEM(NULL, masklay, elems))
		return;

	/* loop through gp-frames, adding */
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
short is_masklayer_frame_selected(MaskLayer *masklay)
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

/* helper function - select gp-frame based on SELECT_* mode */
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
void select_mask_frames(MaskLayer *masklay, short select_mode)
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
void set_masklayer_frame_selection(MaskLayer *masklay, short mode)
{
	/* error checking */
	if (masklay == NULL)
		return;

	/* now call the standard function */
	select_mask_frames(masklay, mode);
}

/* select the frame in this layer that occurs on this frame (there should only be one at most) */
void select_mask_frame(MaskLayer *masklay, int selx, short select_mode)
{
	MaskLayerShape *masklay_shape;

	if (masklay == NULL)
		return;

	/* search through frames for a match */
	for (masklay_shape = masklay->splines_shapes.first; masklay_shape; masklay_shape = masklay_shape->next) {
		/* there should only be one frame with this frame-number */
		if (masklay_shape->frame == selx) {
			masklayshape_select(masklay_shape, select_mode);
			break;
		}
	}
}

/* select the frames in this layer that occur within the bounds specified */
void borderselect_masklayer_frames(MaskLayer *masklay, float min, float max, short select_mode)
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
void delete_masklayer_frames(MaskLayer *masklay)
{
	MaskLayerShape *masklay_shape, *masklay_shape_next;

	/* error checking */
	if (masklay == NULL)
		return;

	/* check for frames to delete */
	for (masklay_shape = masklay->splines_shapes.first; masklay_shape; masklay_shape = masklay_shape_next) {
		masklay_shape_next = masklay_shape->next;

		if (masklay_shape->flag & MASK_SHAPE_SELECT)
			BKE_mask_layer_shape_unlink(masklay, masklay_shape);
	}
}

/* Duplicate selected frames from given gp-layer */
void duplicate_masklayer_frames(MaskLayer *masklay)
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

			// XXX - how to handle duplicate frames?
			BLI_insertlinkafter(&masklay->splines_shapes, masklay_shape, mask_shape_dupe);
		}
	}
}
