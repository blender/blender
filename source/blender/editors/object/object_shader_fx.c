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
 * The Original Code is Copyright (C) 2018 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, 2018
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/object/object_shader_fx.c
 *  \ingroup edobj
 */


#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "DNA_gpencil_types.h"
#include "DNA_shader_fx_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_shader_fx.h"
#include "BKE_report.h"
#include "BKE_object.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "ED_object.h"
#include "ED_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "object_intern.h"

/******************************** API ****************************/

ShaderFxData *ED_object_shaderfx_add(ReportList *reports, Main *bmain, Scene *UNUSED(scene), Object *ob, const char *name, int type)
{
	ShaderFxData *new_fx = NULL;
	const ShaderFxTypeInfo *fxi = BKE_shaderfxType_getInfo(type);

	if (ob->type != OB_GPENCIL) {
		BKE_reportf(reports, RPT_WARNING, "Effect cannot be added to object '%s'", ob->id.name + 2);
		return NULL;
	}

	if (fxi->flags & eShaderFxTypeFlag_Single) {
		if (BKE_shaderfx_findByType(ob, type)) {
			BKE_report(reports, RPT_WARNING, "Only one Effect of this type is allowed");
			return NULL;
		}
	}

	/* get new effect data to add */
	new_fx = BKE_shaderfx_new(type);

	BLI_addtail(&ob->shader_fx, new_fx);

	if (name) {
		BLI_strncpy_utf8(new_fx->name, name, sizeof(new_fx->name));
	}

	/* make sure effect data has unique name */
	BKE_shaderfx_unique_name(&ob->shader_fx, new_fx);

	bGPdata *gpd = ob->data;
	DEG_id_tag_update(&gpd->id, OB_RECALC_OB | OB_RECALC_DATA);

	DEG_id_tag_update(&ob->id, OB_RECALC_DATA);
	DEG_relations_tag_update(bmain);

	return new_fx;
}

/* Return true if the object has a effect of type 'type' other than
 * the shaderfx pointed to be 'exclude', otherwise returns false. */
static bool UNUSED_FUNCTION(object_has_shaderfx)(
        const Object *ob, const ShaderFxData *exclude,
        ShaderFxType type)
{
	ShaderFxData *fx;

	for (fx = ob->shader_fx.first; fx; fx = fx->next) {
		if ((fx != exclude) && (fx->type == type))
			return true;
	}

	return false;
}

static bool object_shaderfx_remove(
        Main *bmain, Object *ob, ShaderFxData *fx,
        bool *UNUSED(r_sort_depsgraph))
{
	/* It seems on rapid delete it is possible to
	 * get called twice on same effect, so make
	 * sure it is in list. */
	if (BLI_findindex(&ob->shader_fx, fx) == -1) {
		return 0;
	}

	DEG_relations_tag_update(bmain);

	BLI_remlink(&ob->shader_fx, fx);
	BKE_shaderfx_free(fx);
	BKE_object_free_derived_caches(ob);

	return 1;
}

bool ED_object_shaderfx_remove(ReportList *reports, Main *bmain, Object *ob, ShaderFxData *fx)
{
	bool sort_depsgraph = false;
	bool ok;

	ok = object_shaderfx_remove(bmain, ob, fx, &sort_depsgraph);

	if (!ok) {
		BKE_reportf(reports, RPT_ERROR, "Effect '%s' not in object '%s'", fx->name, ob->id.name);
		return 0;
	}

	DEG_id_tag_update(&ob->id, OB_RECALC_DATA);
	DEG_relations_tag_update(bmain);

	return 1;
}

void ED_object_shaderfx_clear(Main *bmain, Object *ob)
{
	ShaderFxData *fx = ob->shader_fx.first;
	bool sort_depsgraph = false;

	if (!fx)
		return;

	while (fx) {
		ShaderFxData *next_fx;

		next_fx = fx->next;

		object_shaderfx_remove(bmain, ob, fx, &sort_depsgraph);

		fx = next_fx;
	}

	DEG_id_tag_update(&ob->id, OB_RECALC_DATA);
	DEG_relations_tag_update(bmain);
}

int ED_object_shaderfx_move_up(ReportList *UNUSED(reports), Object *ob, ShaderFxData *fx)
{
	if (fx->prev) {
		BLI_remlink(&ob->shader_fx, fx);
		BLI_insertlinkbefore(&ob->shader_fx, fx->prev, fx);
	}

	return 1;
}

int ED_object_shaderfx_move_down(ReportList *UNUSED(reports), Object *ob, ShaderFxData *fx)
{
	if (fx->next) {
		BLI_remlink(&ob->shader_fx, fx);
		BLI_insertlinkafter(&ob->shader_fx, fx->next, fx);
	}

	return 1;
}

/************************ add effect operator *********************/

static int shaderfx_add_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	Object *ob = ED_object_active_context(C);
	int type = RNA_enum_get(op->ptr, "type");

	if (!ED_object_shaderfx_add(op->reports, bmain, scene, ob, NULL, type))
		return OPERATOR_CANCELLED;

	WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

	return OPERATOR_FINISHED;
}

static const EnumPropertyItem *shaderfx_add_itemf(
        bContext *C, PointerRNA *UNUSED(ptr), PropertyRNA *UNUSED(prop), bool *r_free)
{
	Object *ob = ED_object_active_context(C);
	EnumPropertyItem *item = NULL;
	const EnumPropertyItem *fx_item, *group_item = NULL;
	const ShaderFxTypeInfo *mti;
	int totitem = 0, a;

	if (!ob)
		return rna_enum_object_shaderfx_type_items;

	for (a = 0; rna_enum_object_shaderfx_type_items[a].identifier; a++) {
		fx_item = &rna_enum_object_shaderfx_type_items[a];
		if (fx_item->identifier[0]) {
			mti = BKE_shaderfxType_getInfo(fx_item->value);

			if (mti->flags & eShaderFxTypeFlag_NoUserAdd)
				continue;
		}
		else {
			group_item = fx_item;
			fx_item = NULL;

			continue;
		}

		if (group_item) {
			RNA_enum_item_add(&item, &totitem, group_item);
			group_item = NULL;
		}

		RNA_enum_item_add(&item, &totitem, fx_item);
	}

	RNA_enum_item_end(&item, &totitem);
	*r_free = true;

	return item;
}

void OBJECT_OT_shaderfx_add(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Add Effect";
	ot->description = "Add a visual effect to the active object";
	ot->idname = "OBJECT_OT_shaderfx_add";

	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = shaderfx_add_exec;
	ot->poll = ED_operator_object_active_editable;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	prop = RNA_def_enum(ot->srna, "type", rna_enum_object_shaderfx_type_items, eShaderFxType_Blur, "Type", "");
	RNA_def_enum_funcs(prop, shaderfx_add_itemf);
	ot->prop = prop;
}

/************************ generic functions for operators using names and data context *********************/

static bool edit_shaderfx_poll_generic(bContext *C, StructRNA *rna_type, int obtype_flag)
{
	PointerRNA ptr = CTX_data_pointer_get_type(C, "shaderfx", rna_type);
	Object *ob = (ptr.id.data) ? ptr.id.data : ED_object_active_context(C);

	if (!ptr.data) {
		CTX_wm_operator_poll_msg_set(C, "Context missing 'shaderfx'");
		return 0;
	}

	if (!ob || ID_IS_LINKED(ob)) return 0;
	if (obtype_flag && ((1 << ob->type) & obtype_flag) == 0) return 0;
	if (ptr.id.data && ID_IS_LINKED(ptr.id.data)) return 0;

	if (ID_IS_STATIC_OVERRIDE(ob)) {
		CTX_wm_operator_poll_msg_set(C, "Cannot edit shaderfxs comming from static override");
		return (((ShaderFxData *)ptr.data)->flag & eShaderFxFlag_StaticOverride_Local) != 0;
	}

	return 1;
}

static bool edit_shaderfx_poll(bContext *C)
{
	return edit_shaderfx_poll_generic(C, &RNA_ShaderFx, 0);
}

static void edit_shaderfx_properties(wmOperatorType *ot)
{
	RNA_def_string(ot->srna, "shaderfx", NULL, MAX_NAME, "Shader", "Name of the shaderfx to edit");
}

static int edit_shaderfx_invoke_properties(bContext *C, wmOperator *op)
{
	ShaderFxData *fx;

	if (RNA_struct_property_is_set(op->ptr, "shaderfx")) {
		return true;
	}
	else {
		PointerRNA ptr = CTX_data_pointer_get_type(C, "shaderfx", &RNA_ShaderFx);
		if (ptr.data) {
			fx = ptr.data;
			RNA_string_set(op->ptr, "shaderfx", fx->name);
			return true;
		}
	}

	return false;
}

static ShaderFxData *edit_shaderfx_property_get(wmOperator *op, Object *ob, int type)
{
	char shaderfx_name[MAX_NAME];
	ShaderFxData *fx;
	RNA_string_get(op->ptr, "shaderfx", shaderfx_name);

	fx = BKE_shaderfx_findByName(ob, shaderfx_name);

	if (fx && type != 0 && fx->type != type)
		fx = NULL;

	return fx;
}

/************************ remove shaderfx operator *********************/

static int shaderfx_remove_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Object *ob = ED_object_active_context(C);
	ShaderFxData *fx = edit_shaderfx_property_get(op, ob, 0);

	if (!fx || !ED_object_shaderfx_remove(op->reports, bmain, ob, fx))
		return OPERATOR_CANCELLED;

	WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

	return OPERATOR_FINISHED;
}

static int shaderfx_remove_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	if (edit_shaderfx_invoke_properties(C, op))
		return shaderfx_remove_exec(C, op);
	else
		return OPERATOR_CANCELLED;
}

void OBJECT_OT_shaderfx_remove(wmOperatorType *ot)
{
	ot->name = "Remove Grease Pencil Modifier";
	ot->description = "Remove a shaderfx from the active grease pencil object";
	ot->idname = "OBJECT_OT_shaderfx_remove";

	ot->invoke = shaderfx_remove_invoke;
	ot->exec = shaderfx_remove_exec;
	ot->poll = edit_shaderfx_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
	edit_shaderfx_properties(ot);
}

/************************ move up shaderfx operator *********************/

static int shaderfx_move_up_exec(bContext *C, wmOperator *op)
{
	Object *ob = ED_object_active_context(C);
	ShaderFxData *fx = edit_shaderfx_property_get(op, ob, 0);

	if (!fx || !ED_object_shaderfx_move_up(op->reports, ob, fx))
		return OPERATOR_CANCELLED;

	DEG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

	return OPERATOR_FINISHED;
}

static int shaderfx_move_up_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	if (edit_shaderfx_invoke_properties(C, op))
		return shaderfx_move_up_exec(C, op);
	else
		return OPERATOR_CANCELLED;
}

void OBJECT_OT_shaderfx_move_up(wmOperatorType *ot)
{
	ot->name = "Move Up Modifier";
	ot->description = "Move shaderfx up in the stack";
	ot->idname = "OBJECT_OT_shaderfx_move_up";

	ot->invoke = shaderfx_move_up_invoke;
	ot->exec = shaderfx_move_up_exec;
	ot->poll = edit_shaderfx_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
	edit_shaderfx_properties(ot);
}

/************************ move down shaderfx operator *********************/

static int shaderfx_move_down_exec(bContext *C, wmOperator *op)
{
	Object *ob = ED_object_active_context(C);
	ShaderFxData *fx = edit_shaderfx_property_get(op, ob, 0);

	if (!fx || !ED_object_shaderfx_move_down(op->reports, ob, fx))
		return OPERATOR_CANCELLED;

	DEG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

	return OPERATOR_FINISHED;
}

static int shaderfx_move_down_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	if (edit_shaderfx_invoke_properties(C, op))
		return shaderfx_move_down_exec(C, op);
	else
		return OPERATOR_CANCELLED;
}

void OBJECT_OT_shaderfx_move_down(wmOperatorType *ot)
{
	ot->name = "Move Down Modifier";
	ot->description = "Move shaderfx down in the stack";
	ot->idname = "OBJECT_OT_shaderfx_move_down";

	ot->invoke = shaderfx_move_down_invoke;
	ot->exec = shaderfx_move_down_exec;
	ot->poll = edit_shaderfx_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
	edit_shaderfx_properties(ot);
}
