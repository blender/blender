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
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation,
 *                 Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/mask/mask_shapekey.c
 *  \ingroup edmask
 */

#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_mask.h"

#include "DNA_mask_types.h"
#include "DNA_scene_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_mask.h"  /* own include */

#include "mask_intern.h"  /* own include */

static int mask_shape_key_insert_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);
	const int frame = CFRA;
	Mask *mask = CTX_data_edit_mask(C);
	MaskLayer *masklay;
	int change = FALSE;

	for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
		MaskLayerShape *masklay_shape;

		if (!ED_mask_layer_select_check(masklay)) {
			continue;
		}

		masklay_shape = BKE_mask_layer_shape_varify_frame(masklay, frame);
		BKE_mask_layer_shape_from_mask(masklay, masklay_shape);
		change = TRUE;
	}

	if (change) {
		WM_event_add_notifier(C, NC_MASK | ND_DATA, mask);
		DAG_id_tag_update(&mask->id, 0);

		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

void MASK_OT_shape_key_insert(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Insert Shape Key";
	ot->description = "";
	ot->idname = "MASK_OT_shape_key_insert";

	/* api callbacks */
	ot->exec = mask_shape_key_insert_exec;
	ot->poll = ED_maskedit_mask_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int mask_shape_key_clear_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);
	const int frame = CFRA;
	Mask *mask = CTX_data_edit_mask(C);
	MaskLayer *masklay;
	int change = FALSE;

	for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
		MaskLayerShape *masklay_shape;

		if (!ED_mask_layer_select_check(masklay)) {
			continue;
		}

		masklay_shape = BKE_mask_layer_shape_find_frame(masklay, frame);

		if (masklay_shape) {
			BKE_mask_layer_shape_unlink(masklay, masklay_shape);
			change = TRUE;
		}
	}

	if (change) {
		WM_event_add_notifier(C, NC_MASK | ND_DATA, mask);
		DAG_id_tag_update(&mask->id, 0);

		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

void MASK_OT_shape_key_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear Shape Key";
	ot->description = "";
	ot->idname = "MASK_OT_shape_key_clear";

	/* api callbacks */
	ot->exec = mask_shape_key_clear_exec;
	ot->poll = ED_maskedit_mask_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

int ED_mask_layer_shape_auto_key_all(Mask *mask, const int frame)
{
	MaskLayer *masklay;
	int change = FALSE;

	for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
		MaskLayerShape *masklay_shape;

		masklay_shape = BKE_mask_layer_shape_varify_frame(masklay, frame);
		BKE_mask_layer_shape_from_mask(masklay, masklay_shape);
		change = TRUE;
	}

	return change;
}
