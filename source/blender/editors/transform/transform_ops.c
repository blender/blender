/**
 * $Id: transform_ops.c 17542 2008-11-23 15:27:53Z theeth $
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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "MEM_guardedalloc.h"

#include "DNA_space_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "BLI_arithb.h"

#include "BKE_utildefines.h"
#include "BKE_context.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"

#include "transform.h"

static void transformops_exit(bContext *C, wmOperator *op)
{
	MEM_freeN(op->customdata);
	op->customdata = NULL;
}

static void transformops_data(bContext *C, wmOperator *op, wmEvent *event)
{
	int mode    = RNA_int_get(op->ptr, "mode");
	int options = RNA_int_get(op->ptr, "options");
	TransInfo *t = MEM_callocN(sizeof(TransInfo), "TransInfo data");
	
	initTransform(C, t, mode, options, event);

	/* store data */
	op->customdata = t;
}

static int transform_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	TransInfo *t = op->customdata;
	
	transformEvent(t, event);
	
	transformApply(t);
	
	if (transformEnd(t))
	{
		transformops_exit(C, op);
		return OPERATOR_FINISHED;
	}
	else
	{
		return OPERATOR_RUNNING_MODAL;
	}
}

static int transform_exec(bContext *C, wmOperator *op)
{
	TransInfo *t = op->customdata;
	
	transformApply(t);
	
	transformEnd(t);

	ED_region_tag_redraw(CTX_wm_region(C));

	transformops_exit(C, op);
	
	return OPERATOR_FINISHED;
}

static int transform_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	float value[4];
	
	RNA_float_get_array(op->ptr, "value", value);

	/* makes op->customdata */
	transformops_data(C, op, event);

	if(!QuatIsNul(value)) {
		TransInfo *t = op->customdata;
		
		VECCOPY(t->values, value); /* SHOULD BE VEC-4 */
		
		return transform_exec(C, op);
	}
	else {
		/* add temp handler */
		WM_event_add_modal_handler(C, &CTX_wm_window(C)->handlers, op);

		return OPERATOR_RUNNING_MODAL;
	}
}

void TFM_OT_transform(struct wmOperatorType *ot)
{
	PropertyRNA *prop;
	static float value[4] = {0, 0, 0};
	
	/* identifiers */
	ot->name   = "Transform";
	ot->idname = "TFM_OT_transform";

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->poll   = ED_operator_areaactive;

	RNA_def_property(ot->srna, "mode", PROP_INT, PROP_NONE);
	RNA_def_property(ot->srna, "options", PROP_INT, PROP_NONE);
	
	prop = RNA_def_property(ot->srna, "value", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_array(prop, 4);
	RNA_def_property_float_array_default(prop, value);
}

void transform_operatortypes(void)
{
	WM_operatortype_append(TFM_OT_transform);
}
 
void transform_keymap_for_space(struct wmWindowManager *wm, struct ListBase *keymap, int spaceid)
{
	wmKeymapItem *km;
	switch(spaceid)
	{
		case SPACE_VIEW3D:
			km = WM_keymap_add_item(keymap, "TFM_OT_transform", GKEY, KM_PRESS, 0, 0);
			RNA_int_set(km->ptr, "mode", TFM_TRANSLATION);

			km = WM_keymap_add_item(keymap, "TFM_OT_transform", RKEY, KM_PRESS, 0, 0);
			RNA_int_set(km->ptr, "mode", TFM_ROTATION);

			km = WM_keymap_add_item(keymap, "TFM_OT_transform", SKEY, KM_PRESS, 0, 0);
			RNA_int_set(km->ptr, "mode", TFM_RESIZE);

			km = WM_keymap_add_item(keymap, "TFM_OT_transform", WKEY, KM_PRESS, KM_SHIFT, 0);
			RNA_int_set(km->ptr, "mode", TFM_WARP);

			km = WM_keymap_add_item(keymap, "TFM_OT_transform", SKEY, KM_PRESS, KM_CTRL|KM_SHIFT, 0);
			RNA_int_set(km->ptr, "mode", TFM_TOSPHERE);
			
			km = WM_keymap_add_item(keymap, "TFM_OT_transform", SKEY, KM_PRESS, KM_ALT|KM_CTRL|KM_SHIFT, 0);
			RNA_int_set(km->ptr, "mode", TFM_SHEAR);
			
			break;
		case SPACE_ACTION:
			km= WM_keymap_add_item(keymap, "TFM_OT_transform", GKEY, KM_PRESS, 0, 0);
			RNA_int_set(km->ptr, "mode", TFM_TIME_TRANSLATE);
			
			km= WM_keymap_add_item(keymap, "TFM_OT_transform", EKEY, KM_PRESS, 0, 0);
			RNA_int_set(km->ptr, "mode", TFM_TIME_EXTEND);
			
			km= WM_keymap_add_item(keymap, "TFM_OT_transform", SKEY, KM_PRESS, 0, 0);
			RNA_int_set(km->ptr, "mode", TFM_TIME_SCALE);
			
			km= WM_keymap_add_item(keymap, "TFM_OT_transform", TKEY, KM_PRESS, 0, 0);
			RNA_int_set(km->ptr, "mode", TFM_TIME_SLIDE);
		default:
			break;
	}
}
