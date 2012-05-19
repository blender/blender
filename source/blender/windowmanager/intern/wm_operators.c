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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/intern/wm_operators.c
 *  \ingroup wm
 */


#include <float.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stddef.h>
#include <assert.h>

#include "GHOST_C-api.h"

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_mesh_types.h" /* only for USE_BMESH_SAVE_AS_COMPAT */

#include "BLF_translation.h"

#include "PIL_time.h"

#include "BLI_blenlib.h"
#include "BLI_dynstr.h" /*for WM_operator_pystring */
#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_ghash.h"

#include "BLO_readfile.h"

#include "BKE_blender.h"
#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_idprop.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h" /* BKE_ST_MAXNAME */

#include "BKE_idcode.h"

#include "BIF_gl.h"
#include "BIF_glutil.h" /* for paint cursor */
#include "BLF_api.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "ED_screen.h"
#include "ED_util.h"
#include "ED_object.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

#include "wm.h"
#include "wm_draw.h"
#include "wm_event_system.h"
#include "wm_event_types.h"
#include "wm_subwindow.h"
#include "wm_window.h"

static GHash *global_ops_hash = NULL;

/* ************ operator API, exported ********** */


wmOperatorType *WM_operatortype_find(const char *idname, int quiet)
{
	if (idname[0]) {
		wmOperatorType *ot;

		/* needed to support python style names without the _OT_ syntax */
		char idname_bl[OP_MAX_TYPENAME];
		WM_operator_bl_idname(idname_bl, idname);

		ot = BLI_ghash_lookup(global_ops_hash, idname_bl);
		if (ot) {
			return ot;
		}

		if (!quiet) {
			printf("search for unknown operator '%s', '%s'\n", idname_bl, idname);
		}
	}
	else {
		if (!quiet) {
			printf("search for empty operator\n");
		}
	}

	return NULL;
}

/* caller must free */
GHashIterator *WM_operatortype_iter(void)
{
	return BLI_ghashIterator_new(global_ops_hash);
}

/* all ops in 1 list (for time being... needs evaluation later) */
void WM_operatortype_append(void (*opfunc)(wmOperatorType *))
{
	wmOperatorType *ot;
	
	ot = MEM_callocN(sizeof(wmOperatorType), "operatortype");
	ot->srna = RNA_def_struct(&BLENDER_RNA, "", "OperatorProperties");
	/* Set the default i18n context now, so that opfunc can redefine it if needed! */
	RNA_def_struct_translation_context(ot->srna, BLF_I18NCONTEXT_OPERATOR_DEFAULT);
	opfunc(ot);

	if (ot->name == NULL) {
		fprintf(stderr, "ERROR: Operator %s has no name property!\n", ot->idname);
		ot->name = N_("Dummy Name");
	}

	// XXX All ops should have a description but for now allow them not to.
	RNA_def_struct_ui_text(ot->srna, ot->name, ot->description ? ot->description : N_("(undocumented operator)"));
	RNA_def_struct_identifier(ot->srna, ot->idname);

	BLI_ghash_insert(global_ops_hash, (void *)ot->idname, ot);
}

void WM_operatortype_append_ptr(void (*opfunc)(wmOperatorType *, void *), void *userdata)
{
	wmOperatorType *ot;

	ot = MEM_callocN(sizeof(wmOperatorType), "operatortype");
	ot->srna = RNA_def_struct(&BLENDER_RNA, "", "OperatorProperties");
	/* Set the default i18n context now, so that opfunc can redefine it if needed! */
	RNA_def_struct_translation_context(ot->srna, BLF_I18NCONTEXT_OPERATOR_DEFAULT);
	opfunc(ot, userdata);
	RNA_def_struct_ui_text(ot->srna, ot->name, ot->description ? ot->description : N_("(undocumented operator)"));
	RNA_def_struct_identifier(ot->srna, ot->idname);

	BLI_ghash_insert(global_ops_hash, (void *)ot->idname, ot);
}

/* ********************* macro operator ******************** */

typedef struct {
	int retval;
} MacroData;

static void wm_macro_start(wmOperator *op)
{
	if (op->customdata == NULL) {
		op->customdata = MEM_callocN(sizeof(MacroData), "MacroData");
	}
}

static int wm_macro_end(wmOperator *op, int retval)
{
	if (retval & OPERATOR_CANCELLED) {
		MacroData *md = op->customdata;

		if (md->retval & OPERATOR_FINISHED) {
			retval |= OPERATOR_FINISHED;
			retval &= ~OPERATOR_CANCELLED;
		}
	}

	/* if modal is ending, free custom data */
	if (retval & (OPERATOR_FINISHED | OPERATOR_CANCELLED)) {
		if (op->customdata) {
			MEM_freeN(op->customdata);
			op->customdata = NULL;
		}
	}

	return retval;
}

/* macro exec only runs exec calls */
static int wm_macro_exec(bContext *C, wmOperator *op)
{
	wmOperator *opm;
	int retval = OPERATOR_FINISHED;
	
	wm_macro_start(op);

	for (opm = op->macro.first; opm; opm = opm->next) {
		
		if (opm->type->exec) {
			retval = opm->type->exec(C, opm);
			OPERATOR_RETVAL_CHECK(retval);
		
			if (retval & OPERATOR_FINISHED) {
				MacroData *md = op->customdata;
				md->retval = OPERATOR_FINISHED; /* keep in mind that at least one operator finished */
			}
			else {
				break; /* operator didn't finish, end macro */
			}
		}
	}
	
	return wm_macro_end(op, retval);
}

static int wm_macro_invoke_internal(bContext *C, wmOperator *op, wmEvent *event, wmOperator *opm)
{
	int retval = OPERATOR_FINISHED;

	/* start from operator received as argument */
	for (; opm; opm = opm->next) {
		if (opm->type->invoke)
			retval = opm->type->invoke(C, opm, event);
		else if (opm->type->exec)
			retval = opm->type->exec(C, opm);

		OPERATOR_RETVAL_CHECK(retval);

		BLI_movelisttolist(&op->reports->list, &opm->reports->list);
		
		if (retval & OPERATOR_FINISHED) {
			MacroData *md = op->customdata;
			md->retval = OPERATOR_FINISHED; /* keep in mind that at least one operator finished */
		}
		else {
			break; /* operator didn't finish, end macro */
		}
	}

	return wm_macro_end(op, retval);
}

static int wm_macro_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	wm_macro_start(op);
	return wm_macro_invoke_internal(C, op, event, op->macro.first);
}

static int wm_macro_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	wmOperator *opm = op->opm;
	int retval = OPERATOR_FINISHED;
	
	if (opm == NULL)
		printf("%s: macro error, calling NULL modal()\n", __func__);
	else {
		retval = opm->type->modal(C, opm, event);
		OPERATOR_RETVAL_CHECK(retval);

		/* if this one is done but it's not the last operator in the macro */
		if ((retval & OPERATOR_FINISHED) && opm->next) {
			MacroData *md = op->customdata;

			md->retval = OPERATOR_FINISHED; /* keep in mind that at least one operator finished */

			retval = wm_macro_invoke_internal(C, op, event, opm->next);

			/* if new operator is modal and also added its own handler */
			if (retval & OPERATOR_RUNNING_MODAL && op->opm != opm) {
				wmWindow *win = CTX_wm_window(C);
				wmEventHandler *handler = NULL;

				for (handler = win->modalhandlers.first; handler; handler = handler->next) {
					/* first handler in list is the new one */
					if (handler->op == op)
						break;
				}

				if (handler) {
					BLI_remlink(&win->modalhandlers, handler);
					wm_event_free_handler(handler);
				}

				/* if operator is blocking, grab cursor
				 * This may end up grabbing twice, but we don't care.
				 * */
				if (op->opm->type->flag & OPTYPE_BLOCKING) {
					int bounds[4] = {-1, -1, -1, -1};
					int wrap = (U.uiflag & USER_CONTINUOUS_MOUSE) && ((op->opm->flag & OP_GRAB_POINTER) || (op->opm->type->flag & OPTYPE_GRAB_POINTER));

					if (wrap) {
						ARegion *ar = CTX_wm_region(C);
						if (ar) {
							bounds[0] = ar->winrct.xmin;
							bounds[1] = ar->winrct.ymax;
							bounds[2] = ar->winrct.xmax;
							bounds[3] = ar->winrct.ymin;
						}
					}

					WM_cursor_grab(CTX_wm_window(C), wrap, FALSE, bounds);
				}
			}
		}
	}

	return wm_macro_end(op, retval);
}

static int wm_macro_cancel(bContext *C, wmOperator *op)
{
	/* call cancel on the current modal operator, if any */
	if (op->opm && op->opm->type->cancel) {
		op->opm->type->cancel(C, op->opm);
	}

	return wm_macro_end(op, OPERATOR_CANCELLED);
}

/* Names have to be static for now */
wmOperatorType *WM_operatortype_append_macro(const char *idname, const char *name, const char *description, int flag)
{
	wmOperatorType *ot;
	
	if (WM_operatortype_find(idname, TRUE)) {
		printf("%s: macro error: operator %s exists\n", __func__, idname);
		return NULL;
	}
	
	ot = MEM_callocN(sizeof(wmOperatorType), "operatortype");
	ot->srna = RNA_def_struct(&BLENDER_RNA, "", "OperatorProperties");
	
	ot->idname = idname;
	ot->name = name;
	ot->description = description;
	ot->flag = OPTYPE_MACRO | flag;
	
	ot->exec = wm_macro_exec;
	ot->invoke = wm_macro_invoke;
	ot->modal = wm_macro_modal;
	ot->cancel = wm_macro_cancel;
	ot->poll = NULL;

	if (!ot->description) /* XXX All ops should have a description but for now allow them not to. */
		ot->description = N_("(undocumented operator)");
	
	RNA_def_struct_ui_text(ot->srna, ot->name, ot->description);
	RNA_def_struct_identifier(ot->srna, ot->idname);
	RNA_def_struct_translation_context(ot->srna, BLF_I18NCONTEXT_OPERATOR_DEFAULT);

	BLI_ghash_insert(global_ops_hash, (void *)ot->idname, ot);

	return ot;
}

void WM_operatortype_append_macro_ptr(void (*opfunc)(wmOperatorType *, void *), void *userdata)
{
	wmOperatorType *ot;

	ot = MEM_callocN(sizeof(wmOperatorType), "operatortype");
	ot->srna = RNA_def_struct(&BLENDER_RNA, "", "OperatorProperties");

	ot->flag = OPTYPE_MACRO;
	ot->exec = wm_macro_exec;
	ot->invoke = wm_macro_invoke;
	ot->modal = wm_macro_modal;
	ot->cancel = wm_macro_cancel;
	ot->poll = NULL;

	if (!ot->description)
		ot->description = N_("(undocumented operator)");

	/* Set the default i18n context now, so that opfunc can redefine it if needed! */
	RNA_def_struct_translation_context(ot->srna, BLF_I18NCONTEXT_OPERATOR_DEFAULT);
	opfunc(ot, userdata);

	RNA_def_struct_ui_text(ot->srna, ot->name, ot->description);
	RNA_def_struct_identifier(ot->srna, ot->idname);

	BLI_ghash_insert(global_ops_hash, (void *)ot->idname, ot);
}

wmOperatorTypeMacro *WM_operatortype_macro_define(wmOperatorType *ot, const char *idname)
{
	wmOperatorTypeMacro *otmacro = MEM_callocN(sizeof(wmOperatorTypeMacro), "wmOperatorTypeMacro");

	BLI_strncpy(otmacro->idname, idname, OP_MAX_TYPENAME);

	/* do this on first use, since operatordefinitions might have been not done yet */
	WM_operator_properties_alloc(&(otmacro->ptr), &(otmacro->properties), idname);
	WM_operator_properties_sanitize(otmacro->ptr, 1);

	BLI_addtail(&ot->macro, otmacro);

	{
		/* operator should always be found but in the event its not. don't segfault */
		wmOperatorType *otsub = WM_operatortype_find(idname, 0);
		if (otsub) {
			RNA_def_pointer_runtime(ot->srna, otsub->idname, otsub->srna,
			                        otsub->name, otsub->description);
		}
	}

	return otmacro;
}

static void wm_operatortype_free_macro(wmOperatorType *ot)
{
	wmOperatorTypeMacro *otmacro;
	
	for (otmacro = ot->macro.first; otmacro; otmacro = otmacro->next) {
		if (otmacro->ptr) {
			WM_operator_properties_free(otmacro->ptr);
			MEM_freeN(otmacro->ptr);
		}
	}
	BLI_freelistN(&ot->macro);
}


int WM_operatortype_remove(const char *idname)
{
	wmOperatorType *ot = WM_operatortype_find(idname, 0);

	if (ot == NULL)
		return 0;
	
	RNA_struct_free(&BLENDER_RNA, ot->srna);

	if (ot->last_properties) {
		IDP_FreeProperty(ot->last_properties);
		MEM_freeN(ot->last_properties);
	}

	if (ot->macro.first)
		wm_operatortype_free_macro(ot);

	BLI_ghash_remove(global_ops_hash, (void *)ot->idname, NULL, NULL);

	MEM_freeN(ot);
	return 1;
}

/* SOME_OT_op -> some.op */
void WM_operator_py_idname(char *to, const char *from)
{
	char *sep = strstr(from, "_OT_");
	if (sep) {
		int ofs = (sep - from);
		
		/* note, we use ascii tolower instead of system tolower, because the
		 * latter depends on the locale, and can lead to idname mistmatch */
		memcpy(to, from, sizeof(char) * ofs);
		BLI_ascii_strtolower(to, ofs);

		to[ofs] = '.';
		BLI_strncpy(to + (ofs + 1), sep + 4, OP_MAX_TYPENAME);
	}
	else {
		/* should not happen but support just in case */
		BLI_strncpy(to, from, OP_MAX_TYPENAME);
	}
}

/* some.op -> SOME_OT_op */
void WM_operator_bl_idname(char *to, const char *from)
{
	if (from) {
		char *sep = strchr(from, '.');

		if (sep) {
			int ofs = (sep - from);

			memcpy(to, from, sizeof(char) * ofs);
			BLI_ascii_strtoupper(to, ofs);

			BLI_strncpy(to + ofs, "_OT_", OP_MAX_TYPENAME);
			BLI_strncpy(to + (ofs + 4), sep + 1, OP_MAX_TYPENAME);
		}
		else {
			/* should not happen but support just in case */
			BLI_strncpy(to, from, OP_MAX_TYPENAME);
		}
	}
	else
		to[0] = 0;
}

/* print a string representation of the operator, with the args that it runs 
 * so python can run it again,
 *
 * When calling from an existing wmOperator do.
 * WM_operator_pystring(op->type, op->ptr);
 */
char *WM_operator_pystring(bContext *C, wmOperatorType *ot, PointerRNA *opptr, int all_args)
{
	char idname_py[OP_MAX_TYPENAME];

	/* for building the string */
	DynStr *dynstr = BLI_dynstr_new();
	char *cstring;
	char *cstring_args;

	/* only to get the orginal props for comparisons */
	PointerRNA opptr_default;

	if (all_args == 0 || opptr == NULL) {
		WM_operator_properties_create_ptr(&opptr_default, ot);

		if (opptr == NULL)
			opptr = &opptr_default;
	}

	WM_operator_py_idname(idname_py, ot->idname);
	BLI_dynstr_appendf(dynstr, "bpy.ops.%s(", idname_py);

	cstring_args = RNA_pointer_as_string_keywords(C, opptr, &opptr_default, FALSE, all_args);
	BLI_dynstr_append(dynstr, cstring_args);
	MEM_freeN(cstring_args);

	if (all_args == 0 || opptr == &opptr_default)
		WM_operator_properties_free(&opptr_default);

	BLI_dynstr_append(dynstr, ")");

	cstring = BLI_dynstr_get_cstring(dynstr);
	BLI_dynstr_free(dynstr);
	return cstring;
}

void WM_operator_properties_create_ptr(PointerRNA *ptr, wmOperatorType *ot)
{
	RNA_pointer_create(NULL, ot->srna, NULL, ptr);
}

void WM_operator_properties_create(PointerRNA *ptr, const char *opstring)
{
	wmOperatorType *ot = WM_operatortype_find(opstring, 0);

	if (ot)
		WM_operator_properties_create_ptr(ptr, ot);
	else
		RNA_pointer_create(NULL, &RNA_OperatorProperties, NULL, ptr);
}

/* similar to the function above except its uses ID properties
 * used for keymaps and macros */
void WM_operator_properties_alloc(PointerRNA **ptr, IDProperty **properties, const char *opstring)
{
	if (*properties == NULL) {
		IDPropertyTemplate val = {0};
		*properties = IDP_New(IDP_GROUP, &val, "wmOpItemProp");
	}

	if (*ptr == NULL) {
		*ptr = MEM_callocN(sizeof(PointerRNA), "wmOpItemPtr");
		WM_operator_properties_create(*ptr, opstring);
	}

	(*ptr)->data = *properties;

}

void WM_operator_properties_sanitize(PointerRNA *ptr, const short no_context)
{
	RNA_STRUCT_BEGIN (ptr, prop)
	{
		switch (RNA_property_type(prop)) {
			case PROP_ENUM:
				if (no_context)
					RNA_def_property_flag(prop, PROP_ENUM_NO_CONTEXT);
				else
					RNA_def_property_clear_flag(prop, PROP_ENUM_NO_CONTEXT);
				break;
			case PROP_POINTER:
			{
				StructRNA *ptype = RNA_property_pointer_type(ptr, prop);

				/* recurse into operator properties */
				if (RNA_struct_is_a(ptype, &RNA_OperatorProperties)) {
					PointerRNA opptr = RNA_property_pointer_get(ptr, prop);
					WM_operator_properties_sanitize(&opptr, no_context);
				}
				break;
			}
			default:
				break;
		}
	}
	RNA_STRUCT_END;
}

/* remove all props without PROP_SKIP_SAVE */
void WM_operator_properties_reset(wmOperator *op)
{
	if (op->ptr->data) {
		PropertyRNA *iterprop;
		iterprop = RNA_struct_iterator_property(op->type->srna);

		RNA_PROP_BEGIN (op->ptr, itemptr, iterprop)
		{
			PropertyRNA *prop = itemptr.data;

			if ((RNA_property_flag(prop) & PROP_SKIP_SAVE) == 0) {
				const char *identifier = RNA_property_identifier(prop);
				RNA_struct_idprops_unset(op->ptr, identifier);
			}
		}
		RNA_PROP_END;
	}
}

void WM_operator_properties_free(PointerRNA *ptr)
{
	IDProperty *properties = ptr->data;

	if (properties) {
		IDP_FreeProperty(properties);
		MEM_freeN(properties);
		ptr->data = NULL; /* just in case */
	}
}

/* ************ default op callbacks, exported *********** */

/* invoke callback, uses enum property named "type" */
int WM_menu_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	PropertyRNA *prop = op->type->prop;
	uiPopupMenu *pup;
	uiLayout *layout;

	if (prop == NULL) {
		printf("%s: %s has no enum property set\n", __func__, op->type->idname);
	}
	else if (RNA_property_type(prop) != PROP_ENUM) {
		printf("%s: %s \"%s\" is not an enum property\n",
		       __func__, op->type->idname, RNA_property_identifier(prop));
	}
	else if (RNA_property_is_set(op->ptr, prop)) {
		const int retval = op->type->exec(C, op);
		OPERATOR_RETVAL_CHECK(retval);
		return retval;
	}
	else {
		pup = uiPupMenuBegin(C, RNA_struct_ui_name(op->type->srna), ICON_NONE);
		layout = uiPupMenuLayout(pup);
		uiItemsFullEnumO(layout, op->type->idname, RNA_property_identifier(prop), op->ptr->data, WM_OP_EXEC_REGION_WIN, 0);
		uiPupMenuEnd(C, pup);
	}

	return OPERATOR_CANCELLED;
}


/* generic enum search invoke popup */
static void operator_enum_search_cb(const struct bContext *C, void *arg_ot, const char *str, uiSearchItems *items)
{
	wmOperatorType *ot = (wmOperatorType *)arg_ot;
	PropertyRNA *prop = ot->prop;

	if (prop == NULL) {
		printf("%s: %s has no enum property set\n",
		       __func__, ot->idname);
	}
	else if (RNA_property_type(prop) != PROP_ENUM) {
		printf("%s: %s \"%s\" is not an enum property\n",
		       __func__, ot->idname, RNA_property_identifier(prop));
	}
	else {
		PointerRNA ptr;

		EnumPropertyItem *item, *item_array;
		int do_free;

		RNA_pointer_create(NULL, ot->srna, NULL, &ptr);
		RNA_property_enum_items((bContext *)C, &ptr, prop, &item_array, NULL, &do_free);

		for (item = item_array; item->identifier; item++) {
			/* note: need to give the index rather than the identifier because the enum can be freed */
			if (BLI_strcasestr(item->name, str))
				if (0 == uiSearchItemAdd(items, item->name, SET_INT_IN_POINTER(item->value), 0))
					break;
		}

		if (do_free)
			MEM_freeN(item_array);
	}
}

static void operator_enum_call_cb(struct bContext *C, void *arg1, void *arg2)
{
	wmOperatorType *ot = arg1;

	if (ot) {
		if (ot->prop) {
			PointerRNA props_ptr;
			WM_operator_properties_create_ptr(&props_ptr, ot);
			RNA_property_enum_set(&props_ptr, ot->prop, GET_INT_FROM_POINTER(arg2));
			WM_operator_name_call(C, ot->idname, WM_OP_EXEC_DEFAULT, &props_ptr);
			WM_operator_properties_free(&props_ptr);
		}
		else {
			printf("%s: op->prop for '%s' is NULL\n", __func__, ot->idname);
		}
	}
}

static uiBlock *wm_enum_search_menu(bContext *C, ARegion *ar, void *arg_op)
{
	static char search[256] = "";
	wmEvent event;
	wmWindow *win = CTX_wm_window(C);
	uiBlock *block;
	uiBut *but;
	wmOperator *op = (wmOperator *)arg_op;

	block = uiBeginBlock(C, ar, "_popup", UI_EMBOSS);
	uiBlockSetFlag(block, UI_BLOCK_LOOP | UI_BLOCK_RET_1 | UI_BLOCK_MOVEMOUSE_QUIT);

#if 0 /* ok, this isn't so easy... */
	uiDefBut(block, LABEL, 0, RNA_struct_ui_name(op->type->srna), 10, 10, 180, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");
#endif
	but = uiDefSearchBut(block, search, 0, ICON_VIEWZOOM, sizeof(search), 10, 10, 9 * UI_UNIT_X, UI_UNIT_Y, 0, 0, "");
	uiButSetSearchFunc(but, operator_enum_search_cb, op->type, operator_enum_call_cb, NULL);

	/* fake button, it holds space for search items */
	uiDefBut(block, LABEL, 0, "", 10, 10 - uiSearchBoxhHeight(), 9 * UI_UNIT_X, uiSearchBoxhHeight(), NULL, 0, 0, 0, 0, NULL);

	uiPopupBoundsBlock(block, 6, 0, -UI_UNIT_Y); /* move it downwards, mouse over button */
	uiEndBlock(C, block);

	event = *(win->eventstate);  /* XXX huh huh? make api call */
	event.type = EVT_BUT_OPEN;
	event.val = KM_PRESS;
	event.customdata = but;
	event.customdatafree = FALSE;
	wm_event_add(win, &event);

	return block;
}


int WM_enum_search_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	uiPupBlock(C, wm_enum_search_menu, op);
	return OPERATOR_CANCELLED;
}

/* Can't be used as an invoke directly, needs message arg (can be NULL) */
int WM_operator_confirm_message(bContext *C, wmOperator *op, const char *message)
{
	uiPopupMenu *pup;
	uiLayout *layout;
	IDProperty *properties = op->ptr->data;

	if (properties && properties->len)
		properties = IDP_CopyProperty(op->ptr->data);
	else
		properties = NULL;

	pup = uiPupMenuBegin(C, IFACE_("OK?"), ICON_QUESTION);
	layout = uiPupMenuLayout(pup);
	uiItemFullO_ptr(layout, op->type, message, ICON_NONE, properties, WM_OP_EXEC_REGION_WIN, 0);
	uiPupMenuEnd(C, pup);
	
	return OPERATOR_CANCELLED;
}


int WM_operator_confirm(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	return WM_operator_confirm_message(C, op, NULL);
}

/* op->invoke, opens fileselect if path property not set, otherwise executes */
int WM_operator_filesel(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	if (RNA_struct_property_is_set(op->ptr, "filepath")) {
		return WM_operator_call_notest(C, op); /* call exec direct */
	} 
	else {
		WM_event_add_fileselect(C, op);
		return OPERATOR_RUNNING_MODAL;
	}
}

int WM_operator_filesel_ensure_ext_imtype(wmOperator *op, const char imtype)
{
	PropertyRNA *prop;
	char filepath[FILE_MAX];
	/* dont NULL check prop, this can only run on ops with a 'filepath' */
	prop = RNA_struct_find_property(op->ptr, "filepath");
	RNA_property_string_get(op->ptr, prop, filepath);
	if (BKE_add_image_extension(filepath, imtype)) {
		RNA_property_string_set(op->ptr, prop, filepath);
		/* note, we could check for and update 'filename' here,
		 * but so far nothing needs this. */
		return TRUE;
	}
	return FALSE;
}

/* default properties for fileselect */
void WM_operator_properties_filesel(wmOperatorType *ot, int filter, short type, short action, short flag, short display)
{
	PropertyRNA *prop;

	static EnumPropertyItem file_display_items[] = {
		{FILE_DEFAULTDISPLAY, "FILE_DEFAULTDISPLAY", 0, "Default", "Automatically determine display type for files"},
		{FILE_SHORTDISPLAY, "FILE_SHORTDISPLAY", ICON_SHORTDISPLAY, "Short List", "Display files as short list"},
		{FILE_LONGDISPLAY, "FILE_LONGDISPLAY", ICON_LONGDISPLAY, "Long List", "Display files as a detailed list"},
		{FILE_IMGDISPLAY, "FILE_IMGDISPLAY", ICON_IMGDISPLAY, "Thumbnails", "Display files as thumbnails"},
		{0, NULL, 0, NULL, NULL}
	};


	if (flag & WM_FILESEL_FILEPATH)
		RNA_def_string_file_path(ot->srna, "filepath", "", FILE_MAX, "File Path", "Path to file");

	if (flag & WM_FILESEL_DIRECTORY)
		RNA_def_string_dir_path(ot->srna, "directory", "", FILE_MAX, "Directory", "Directory of the file");

	if (flag & WM_FILESEL_FILENAME)
		RNA_def_string_file_name(ot->srna, "filename", "", FILE_MAX, "File Name", "Name of the file");

	if (flag & WM_FILESEL_FILES)
		RNA_def_collection_runtime(ot->srna, "files", &RNA_OperatorFileListElement, "Files", "");

	if (action == FILE_SAVE) {
		prop = RNA_def_boolean(ot->srna, "check_existing", 1, "Check Existing", "Check and warn on overwriting existing files");
		RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
	}
	
	prop = RNA_def_boolean(ot->srna, "filter_blender", (filter & BLENDERFILE), "Filter .blend files", "");
	RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
	prop = RNA_def_boolean(ot->srna, "filter_image", (filter & IMAGEFILE), "Filter image files", "");
	RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
	prop = RNA_def_boolean(ot->srna, "filter_movie", (filter & MOVIEFILE), "Filter movie files", "");
	RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
	prop = RNA_def_boolean(ot->srna, "filter_python", (filter & PYSCRIPTFILE), "Filter python files", "");
	RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
	prop = RNA_def_boolean(ot->srna, "filter_font", (filter & FTFONTFILE), "Filter font files", "");
	RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
	prop = RNA_def_boolean(ot->srna, "filter_sound", (filter & SOUNDFILE), "Filter sound files", "");
	RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
	prop = RNA_def_boolean(ot->srna, "filter_text", (filter & TEXTFILE), "Filter text files", "");
	RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
	prop = RNA_def_boolean(ot->srna, "filter_btx", (filter & BTXFILE), "Filter btx files", "");
	RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
	prop = RNA_def_boolean(ot->srna, "filter_collada", (filter & COLLADAFILE), "Filter COLLADA files", "");
	RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
	prop = RNA_def_boolean(ot->srna, "filter_folder", (filter & FOLDERFILE), "Filter folders", "");
	RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

	prop = RNA_def_int(ot->srna, "filemode", type, FILE_LOADLIB, FILE_SPECIAL,
	                   "File Browser Mode", "The setting for the file browser mode to load a .blend file, a library or a special file",
	                   FILE_LOADLIB, FILE_SPECIAL);
	RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

	if (flag & WM_FILESEL_RELPATH)
		RNA_def_boolean(ot->srna, "relative_path", TRUE, "Relative Path", "Select the file relative to the blend file");

	prop = RNA_def_enum(ot->srna, "display_type", file_display_items, display, "Display Type", "");
	RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

void WM_operator_properties_select_all(wmOperatorType *ot)
{
	static EnumPropertyItem select_all_actions[] = {
		{SEL_TOGGLE, "TOGGLE", 0, "Toggle", "Toggle selection for all elements"},
		{SEL_SELECT, "SELECT", 0, "Select", "Select all elements"},
		{SEL_DESELECT, "DESELECT", 0, "Deselect", "Deselect all elements"},
		{SEL_INVERT, "INVERT", 0, "Invert", "Invert selection of all elements"},
		{0, NULL, 0, NULL, NULL}
	};

	RNA_def_enum(ot->srna, "action", select_all_actions, SEL_TOGGLE, "Action", "Selection action to execute");
}

void WM_operator_properties_gesture_border(wmOperatorType *ot, int extend)
{
	RNA_def_int(ot->srna, "gesture_mode", 0, INT_MIN, INT_MAX, "Gesture Mode", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "xmin", 0, INT_MIN, INT_MAX, "X Min", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "xmax", 0, INT_MIN, INT_MAX, "X Max", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "ymin", 0, INT_MIN, INT_MAX, "Y Min", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "ymax", 0, INT_MIN, INT_MAX, "Y Max", "", INT_MIN, INT_MAX);

	if (extend)
		RNA_def_boolean(ot->srna, "extend", 1, "Extend", "Extend selection instead of deselecting everything first");
}

void WM_operator_properties_gesture_straightline(wmOperatorType *ot, int cursor)
{
	RNA_def_int(ot->srna, "xstart", 0, INT_MIN, INT_MAX, "X Start", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "xend", 0, INT_MIN, INT_MAX, "X End", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "ystart", 0, INT_MIN, INT_MAX, "Y Start", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "yend", 0, INT_MIN, INT_MAX, "Y End", "", INT_MIN, INT_MAX);
	
	if (cursor)
		RNA_def_int(ot->srna, "cursor", cursor, 0, INT_MAX, "Cursor", "Mouse cursor style to use during the modal operator", 0, INT_MAX);
}


/* op->poll */
int WM_operator_winactive(bContext *C)
{
	if (CTX_wm_window(C) == NULL) return 0;
	return 1;
}

/* return FALSE, if the UI should be disabled */
int WM_operator_check_ui_enabled(const bContext *C, const char *idname)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	Scene *scene = CTX_data_scene(C);

	return !(ED_undo_valid(C, idname) == 0 || WM_jobs_test(wm, scene));
}

wmOperator *WM_operator_last_redo(const bContext *C)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	wmOperator *op;

	/* only for operators that are registered and did an undo push */
	for (op = wm->operators.last; op; op = op->prev)
		if ((op->type->flag & OPTYPE_REGISTER) && (op->type->flag & OPTYPE_UNDO))
			break;

	return op;
}

static uiBlock *wm_block_create_redo(bContext *C, ARegion *ar, void *arg_op)
{
	wmOperator *op = arg_op;
	uiBlock *block;
	uiLayout *layout;
	uiStyle *style = UI_GetStyle();
	int width = 300;
	

	block = uiBeginBlock(C, ar, __func__, UI_EMBOSS);
	uiBlockClearFlag(block, UI_BLOCK_LOOP);
	uiBlockSetFlag(block, UI_BLOCK_KEEP_OPEN | UI_BLOCK_RET_1 | UI_BLOCK_MOVEMOUSE_QUIT);

	/* if register is not enabled, the operator gets freed on OPERATOR_FINISHED
	 * ui_apply_but_funcs_after calls ED_undo_operator_repeate_cb and crashes */
	assert(op->type->flag & OPTYPE_REGISTER);

	uiBlockSetHandleFunc(block, ED_undo_operator_repeat_cb_evt, arg_op);
	layout = uiBlockLayout(block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL, 0, 0, width, UI_UNIT_Y, style);

	if (!WM_operator_check_ui_enabled(C, op->type->name))
		uiLayoutSetEnabled(layout, 0);

	if (op->type->flag & OPTYPE_MACRO) {
		for (op = op->macro.first; op; op = op->next) {
			uiItemL(layout, RNA_struct_ui_name(op->type->srna), ICON_NONE);
			uiLayoutOperatorButs(C, layout, op, NULL, 'H', UI_LAYOUT_OP_SHOW_TITLE);
		}
	}
	else {
		uiLayoutOperatorButs(C, layout, op, NULL, 'H', UI_LAYOUT_OP_SHOW_TITLE);
	}
	

	uiPopupBoundsBlock(block, 4, 0, 0);
	uiEndBlock(C, block);

	return block;
}

typedef struct wmOpPopUp {
	wmOperator *op;
	int width;
	int height;
	int free_op;
} wmOpPopUp;

/* Only invoked by OK button in popups created with wm_block_dialog_create() */
static void dialog_exec_cb(bContext *C, void *arg1, void *arg2)
{
	wmOpPopUp *data = arg1;
	uiBlock *block = arg2;

	WM_operator_call(C, data->op);

	/* let execute handle freeing it */
	//data->free_op= FALSE;
	//data->op= NULL;

	/* in this case, wm_operator_ui_popup_cancel wont run */
	MEM_freeN(data);

	uiPupBlockClose(C, block);
}

static void dialog_check_cb(bContext *C, void *op_ptr, void *UNUSED(arg))
{
	wmOperator *op = op_ptr;
	if (op->type->check) {
		if (op->type->check(C, op)) {
			/* refresh */
		}
	}
}

/* Dialogs are popups that require user verification (click OK) before exec */
static uiBlock *wm_block_dialog_create(bContext *C, ARegion *ar, void *userData)
{
	wmOpPopUp *data = userData;
	wmOperator *op = data->op;
	uiBlock *block;
	uiLayout *layout;
	uiStyle *style = UI_GetStyle();

	block = uiBeginBlock(C, ar, __func__, UI_EMBOSS);
	uiBlockClearFlag(block, UI_BLOCK_LOOP);
	uiBlockSetFlag(block, UI_BLOCK_KEEP_OPEN | UI_BLOCK_RET_1 | UI_BLOCK_MOVEMOUSE_QUIT);

	layout = uiBlockLayout(block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL, 0, 0, data->width, data->height, style);
	
	uiBlockSetFunc(block, dialog_check_cb, op, NULL);

	uiLayoutOperatorButs(C, layout, op, NULL, 'H', UI_LAYOUT_OP_SHOW_TITLE);
	
	/* clear so the OK button is left alone */
	uiBlockSetFunc(block, NULL, NULL, NULL);

	/* new column so as not to interfear with custom layouts [#26436] */
	{
		uiBlock *col_block;
		uiLayout *col;
		uiBut *btn;

		col = uiLayoutColumn(layout, FALSE);
		col_block = uiLayoutGetBlock(col);
		/* Create OK button, the callback of which will execute op */
		btn = uiDefBut(col_block, BUT, 0, IFACE_("OK"), 0, -30, 0, UI_UNIT_Y, NULL, 0, 0, 0, 0, "");
		uiButSetFunc(btn, dialog_exec_cb, data, col_block);
	}

	/* center around the mouse */
	uiPopupBoundsBlock(block, 4, data->width / -2, data->height / 2);
	uiEndBlock(C, block);

	return block;
}

static uiBlock *wm_operator_ui_create(bContext *C, ARegion *ar, void *userData)
{
	wmOpPopUp *data = userData;
	wmOperator *op = data->op;
	uiBlock *block;
	uiLayout *layout;
	uiStyle *style = UI_GetStyle();

	block = uiBeginBlock(C, ar, __func__, UI_EMBOSS);
	uiBlockClearFlag(block, UI_BLOCK_LOOP);
	uiBlockSetFlag(block, UI_BLOCK_KEEP_OPEN | UI_BLOCK_RET_1 | UI_BLOCK_MOVEMOUSE_QUIT);

	layout = uiBlockLayout(block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL, 0, 0, data->width, data->height, style);

	/* since ui is defined the auto-layout args are not used */
	uiLayoutOperatorButs(C, layout, op, NULL, 'V', 0);

	uiPopupBoundsBlock(block, 4, 0, 0);
	uiEndBlock(C, block);

	return block;
}

static void wm_operator_ui_popup_cancel(void *userData)
{
	wmOpPopUp *data = userData;
	if (data->free_op && data->op) {
		wmOperator *op = data->op;
		WM_operator_free(op);
	}

	MEM_freeN(data);
}

static void wm_operator_ui_popup_ok(struct bContext *C, void *arg, int retval)
{
	wmOpPopUp *data = arg;
	wmOperator *op = data->op;

	if (op && retval > 0)
		WM_operator_call(C, op);
}

int WM_operator_ui_popup(bContext *C, wmOperator *op, int width, int height)
{
	wmOpPopUp *data = MEM_callocN(sizeof(wmOpPopUp), "WM_operator_ui_popup");
	data->op = op;
	data->width = width;
	data->height = height;
	data->free_op = TRUE; /* if this runs and gets registered we may want not to free it */
	uiPupBlockEx(C, wm_operator_ui_create, NULL, wm_operator_ui_popup_cancel, data);
	return OPERATOR_RUNNING_MODAL;
}

/* operator menu needs undo, for redo callback */
int WM_operator_props_popup(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	
	if ((op->type->flag & OPTYPE_REGISTER) == 0) {
		BKE_reportf(op->reports, RPT_ERROR,
		            "Operator '%s' does not have register enabled, incorrect invoke function.", op->type->idname);
		return OPERATOR_CANCELLED;
	}
	
	ED_undo_push_op(C, op);
	wm_operator_register(C, op);

	uiPupBlock(C, wm_block_create_redo, op);

	return OPERATOR_RUNNING_MODAL;
}

int WM_operator_props_dialog_popup(bContext *C, wmOperator *op, int width, int height)
{
	wmOpPopUp *data = MEM_callocN(sizeof(wmOpPopUp), "WM_operator_props_dialog_popup");
	
	data->op = op;
	data->width = width;
	data->height = height;
	data->free_op = TRUE; /* if this runs and gets registered we may want not to free it */

	/* op is not executed until popup OK but is clicked */
	uiPupBlockEx(C, wm_block_dialog_create, wm_operator_ui_popup_ok, wm_operator_ui_popup_cancel, data);

	return OPERATOR_RUNNING_MODAL;
}

int WM_operator_redo_popup(bContext *C, wmOperator *op)
{
	/* CTX_wm_reports(C) because operator is on stack, not active in event system */
	if ((op->type->flag & OPTYPE_REGISTER) == 0) {
		BKE_reportf(CTX_wm_reports(C), RPT_ERROR, "Operator redo '%s' does not have register enabled, incorrect invoke function.", op->type->idname);
		return OPERATOR_CANCELLED;
	}
	if (op->type->poll && op->type->poll(C) == 0) {
		BKE_reportf(CTX_wm_reports(C), RPT_ERROR, "Operator redo '%s': wrong context.", op->type->idname);
		return OPERATOR_CANCELLED;
	}
	
	uiPupBlock(C, wm_block_create_redo, op);

	return OPERATOR_CANCELLED;
}

/* ***************** Debug menu ************************* */

static int wm_debug_menu_exec(bContext *C, wmOperator *op)
{
	G.rt = RNA_int_get(op->ptr, "debug_value");
	ED_screen_refresh(CTX_wm_manager(C), CTX_wm_window(C));
	WM_event_add_notifier(C, NC_WINDOW, NULL);

	return OPERATOR_FINISHED;	
}

static int wm_debug_menu_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	RNA_int_set(op->ptr, "debug_value", G.rt);
	return WM_operator_props_dialog_popup(C, op, 9 * UI_UNIT_X, UI_UNIT_Y);
}

static void WM_OT_debug_menu(wmOperatorType *ot)
{
	ot->name = "Debug Menu";
	ot->idname = "WM_OT_debug_menu";
	ot->description = "Open a popup to set the debug level";
	
	ot->invoke = wm_debug_menu_invoke;
	ot->exec = wm_debug_menu_exec;
	ot->poll = WM_operator_winactive;
	
	RNA_def_int(ot->srna, "debug_value", 0, -10000, 10000, "Debug Value", "", INT_MIN, INT_MAX);
}


/* ***************** Splash Screen ************************* */

static void wm_block_splash_close(bContext *C, void *arg_block, void *UNUSED(arg))
{
	uiPupBlockClose(C, arg_block);
}

static uiBlock *wm_block_create_splash(bContext *C, ARegion *ar, void *arg_unused);

/* XXX: hack to refresh splash screen with updated prest menu name,
 * since popup blocks don't get regenerated like panels do */
static void wm_block_splash_refreshmenu(bContext *UNUSED(C), void *UNUSED(arg_block), void *UNUSED(arg))
{
	/* ugh, causes crashes in other buttons, disabling for now until 
	 * a better fix */
#if 0
	uiPupBlockClose(C, arg_block);
	uiPupBlock(C, wm_block_create_splash, NULL);
#endif
}

static int wm_resource_check_prev(void)
{

	char *res = BLI_get_folder_version(BLENDER_RESOURCE_PATH_USER, BLENDER_VERSION, TRUE);

	// if (res) printf("USER: %s\n", res);

#if 0 /* ignore the local folder */
	if (res == NULL) {
		/* with a local dir, copying old files isn't useful since local dir get priority for config */
		res = BLI_get_folder_version(BLENDER_RESOURCE_PATH_LOCAL, BLENDER_VERSION, TRUE);
	}
#endif

	// if (res) printf("LOCAL: %s\n", res);
	if (res) {
		return FALSE;
	}
	else {
		return (BLI_get_folder_version(BLENDER_RESOURCE_PATH_USER, BLENDER_VERSION - 1, TRUE) != NULL);
	}
}

static uiBlock *wm_block_create_splash(bContext *C, ARegion *ar, void *UNUSED(arg))
{
	uiBlock *block;
	uiBut *but;
	uiLayout *layout, *split, *col;
	uiStyle *style = UI_GetStyle();
	struct RecentFile *recent;
	int i;
	MenuType *mt = WM_menutype_find("USERPREF_MT_splash", TRUE);
	char url[96];

#ifndef WITH_HEADLESS
	extern char datatoc_splash_png[];
	extern int datatoc_splash_png_size;

	ImBuf *ibuf = IMB_ibImageFromMemory((unsigned char *)datatoc_splash_png,
	                                    datatoc_splash_png_size, IB_rect, "<splash screen>");
#else
	ImBuf *ibuf = NULL;
#endif


#ifdef WITH_BUILDINFO
	int ver_width, rev_width;
	char version_buf[128];
	char revision_buf[128];
	extern char build_rev[];
	
	BLI_snprintf(version_buf, sizeof(version_buf),
	             "%d.%02d.%d", BLENDER_VERSION / 100, BLENDER_VERSION % 100, BLENDER_SUBVERSION);
	BLI_snprintf(revision_buf, sizeof(revision_buf), "r%s", build_rev);
	
	BLF_size(style->widgetlabel.uifont_id, style->widgetlabel.points, U.dpi);
	ver_width = (int)BLF_width(style->widgetlabel.uifont_id, version_buf) + 5;
	rev_width = (int)BLF_width(style->widgetlabel.uifont_id, revision_buf) + 5;
#endif //WITH_BUILDINFO

	block = uiBeginBlock(C, ar, "_popup", UI_EMBOSS);
	uiBlockSetFlag(block, UI_BLOCK_KEEP_OPEN);
	
	but = uiDefBut(block, BUT_IMAGE, 0, "", 0, 10, 501, 282, ibuf, 0.0, 0.0, 0, 0, ""); /* button owns the imbuf now */
	uiButSetFunc(but, wm_block_splash_close, block, NULL);
	uiBlockSetFunc(block, wm_block_splash_refreshmenu, block, NULL);
	
#ifdef WITH_BUILDINFO	
	uiDefBut(block, LABEL, 0, version_buf, 494 - ver_width, 282 - 24, ver_width, UI_UNIT_Y, NULL, 0, 0, 0, 0, NULL);
	uiDefBut(block, LABEL, 0, revision_buf, 494 - rev_width, 282 - 36, rev_width, UI_UNIT_Y, NULL, 0, 0, 0, 0, NULL);
#endif //WITH_BUILDINFO
	
	layout = uiBlockLayout(block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL, 10, 2, 480, 110, style);
	
	uiBlockSetEmboss(block, UI_EMBOSS);
	/* show the splash menu (containing interaction presets), using python */
	if (mt) {
		Menu menu = {NULL};
		menu.layout = layout;
		menu.type = mt;
		mt->draw(C, &menu);

//		wmWindowManager *wm= CTX_wm_manager(C);
//		uiItemM(layout, C, "USERPREF_MT_keyconfigs", U.keyconfigstr, ICON_NONE);
	}
	
	uiBlockSetEmboss(block, UI_EMBOSSP);
	uiLayoutSetOperatorContext(layout, WM_OP_EXEC_REGION_WIN);
	
	split = uiLayoutSplit(layout, 0, 0);
	col = uiLayoutColumn(split, 0);
	uiItemL(col, "Links", ICON_NONE);
	uiItemStringO(col, IFACE_("Donations"), ICON_URL, "WM_OT_url_open", "url", "http://www.blender.org/blenderorg/blender-foundation/donation-payment");
	uiItemStringO(col, IFACE_("Credits"), ICON_URL, "WM_OT_url_open", "url", "http://www.blender.org/development/credits");
	uiItemStringO(col, IFACE_("Release Log"), ICON_URL, "WM_OT_url_open", "url", "http://www.blender.org/development/release-logs/blender-263");
	uiItemStringO(col, IFACE_("Manual"), ICON_URL, "WM_OT_url_open", "url", "http://wiki.blender.org/index.php/Doc:2.6/Manual");
	uiItemStringO(col, IFACE_("Blender Website"), ICON_URL, "WM_OT_url_open", "url", "http://www.blender.org");
	uiItemStringO(col, IFACE_("User Community"), ICON_URL, "WM_OT_url_open", "url", "http://www.blender.org/community/user-community");
	if (strcmp(STRINGIFY(BLENDER_VERSION_CYCLE), "release") == 0) {
		BLI_snprintf(url, sizeof(url), "http://www.blender.org/documentation/blender_python_api_%d_%d" STRINGIFY(BLENDER_VERSION_CHAR) "_release", BLENDER_VERSION / 100, BLENDER_VERSION % 100);
	}
	else {
		BLI_snprintf(url, sizeof(url), "http://www.blender.org/documentation/blender_python_api_%d_%d_%d", BLENDER_VERSION / 100, BLENDER_VERSION % 100, BLENDER_SUBVERSION);
	}
	uiItemStringO(col, IFACE_("Python API Reference"), ICON_URL, "WM_OT_url_open", "url", url);
	uiItemL(col, "", ICON_NONE);

	col = uiLayoutColumn(split, 0);

	if (wm_resource_check_prev()) {
		uiItemO(col, NULL, ICON_NEW, "WM_OT_copy_prev_settings");
		uiItemS(col);
	}

	uiItemL(col, IFACE_("Recent"), ICON_NONE);
	for (recent = G.recent_files.first, i = 0; (i < 5) && (recent); recent = recent->next, i++) {
		uiItemStringO(col, BLI_path_basename(recent->filepath), ICON_FILE_BLEND, "WM_OT_open_mainfile", "filepath", recent->filepath);
	}

	uiItemS(col);
	uiItemO(col, NULL, ICON_RECOVER_LAST, "WM_OT_recover_last_session");
	uiItemL(col, "", ICON_NONE);
	
	uiCenteredBoundsBlock(block, 0);
	uiEndBlock(C, block);
	
	return block;
}

static int wm_splash_invoke(bContext *C, wmOperator *UNUSED(op), wmEvent *UNUSED(event))
{
	uiPupBlock(C, wm_block_create_splash, NULL);
	
	return OPERATOR_FINISHED;
}

static void WM_OT_splash(wmOperatorType *ot)
{
	ot->name = "Splash Screen";
	ot->idname = "WM_OT_splash";
	ot->description = "Opens a blocking popup region with release info";
	
	ot->invoke = wm_splash_invoke;
	ot->poll = WM_operator_winactive;
}


/* ***************** Search menu ************************* */
static void operator_call_cb(struct bContext *C, void *UNUSED(arg1), void *arg2)
{
	wmOperatorType *ot = arg2;
	
	if (ot)
		WM_operator_name_call(C, ot->idname, WM_OP_INVOKE_DEFAULT, NULL);
}

static void operator_search_cb(const struct bContext *C, void *UNUSED(arg), const char *str, uiSearchItems *items)
{
	GHashIterator *iter = WM_operatortype_iter();

	for (; !BLI_ghashIterator_isDone(iter); BLI_ghashIterator_step(iter)) {
		wmOperatorType *ot = BLI_ghashIterator_getValue(iter);

		if ((ot->flag & OPTYPE_INTERNAL) && (G.debug & G_DEBUG_WM) == 0)
			continue;

		if (BLI_strcasestr(ot->name, str)) {
			if (WM_operator_poll((bContext *)C, ot)) {
				char name[256];
				int len = strlen(ot->name);
				
				/* display name for menu, can hold hotkey */
				BLI_strncpy(name, ot->name, sizeof(name));
				
				/* check for hotkey */
				if (len < sizeof(name) - 6) {
					if (WM_key_event_operator_string(C, ot->idname, WM_OP_EXEC_DEFAULT, NULL, TRUE,
					                                 &name[len + 1], sizeof(name) - len - 1))
					{
						name[len] = '|';
					}
				}
				
				if (0 == uiSearchItemAdd(items, name, ot, 0))
					break;
			}
		}
	}
	BLI_ghashIterator_free(iter);
}

static uiBlock *wm_block_search_menu(bContext *C, ARegion *ar, void *UNUSED(arg_op))
{
	static char search[256] = "";
	wmEvent event;
	wmWindow *win = CTX_wm_window(C);
	uiBlock *block;
	uiBut *but;
	
	block = uiBeginBlock(C, ar, "_popup", UI_EMBOSS);
	uiBlockSetFlag(block, UI_BLOCK_LOOP | UI_BLOCK_RET_1 | UI_BLOCK_MOVEMOUSE_QUIT);
	
	but = uiDefSearchBut(block, search, 0, ICON_VIEWZOOM, sizeof(search), 10, 10, 9 * UI_UNIT_X, UI_UNIT_Y, 0, 0, "");
	uiButSetSearchFunc(but, operator_search_cb, NULL, operator_call_cb, NULL);
	
	/* fake button, it holds space for search items */
	uiDefBut(block, LABEL, 0, "", 10, 10 - uiSearchBoxhHeight(), 9 * UI_UNIT_X, uiSearchBoxhHeight(), NULL, 0, 0, 0, 0, NULL);
	
	uiPopupBoundsBlock(block, 6, 0, -UI_UNIT_Y); /* move it downwards, mouse over button */
	uiEndBlock(C, block);
	
	event = *(win->eventstate);  /* XXX huh huh? make api call */
	event.type = EVT_BUT_OPEN;
	event.val = KM_PRESS;
	event.customdata = but;
	event.customdatafree = FALSE;
	wm_event_add(win, &event);
	
	return block;
}

static int wm_search_menu_exec(bContext *UNUSED(C), wmOperator *UNUSED(op))
{
	return OPERATOR_FINISHED;
}

static int wm_search_menu_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	uiPupBlock(C, wm_block_search_menu, op);
	
	return OPERATOR_CANCELLED;
}

/* op->poll */
static int wm_search_menu_poll(bContext *C)
{
	if (CTX_wm_window(C) == NULL) {
		return 0;
	}
	else {
		ScrArea *sa = CTX_wm_area(C);
		if (sa) {
			if (sa->spacetype == SPACE_CONSOLE) return 0;  // XXX - so we can use the shortcut in the console
			if (sa->spacetype == SPACE_TEXT) return 0;  // XXX - so we can use the spacebar in the text editor
		}
		else {
			Object *editob = CTX_data_edit_object(C);
			if (editob && editob->type == OB_FONT) return 0;  // XXX - so we can use the spacebar for entering text
		}
	}
	return 1;
}

static void WM_OT_search_menu(wmOperatorType *ot)
{
	ot->name = "Search Menu";
	ot->idname = "WM_OT_search_menu";
	ot->description = "Pop-up a search menu over all available operators in current context";
	
	ot->invoke = wm_search_menu_invoke;
	ot->exec = wm_search_menu_exec;
	ot->poll = wm_search_menu_poll;
}

static int wm_call_menu_exec(bContext *C, wmOperator *op)
{
	char idname[BKE_ST_MAXNAME];
	RNA_string_get(op->ptr, "name", idname);

	uiPupMenuInvoke(C, idname);

	return OPERATOR_CANCELLED;
}

static void WM_OT_call_menu(wmOperatorType *ot)
{
	ot->name = "Call Menu";
	ot->idname = "WM_OT_call_menu";

	ot->exec = wm_call_menu_exec;
	ot->poll = WM_operator_winactive;

	ot->flag = OPTYPE_INTERNAL;

	RNA_def_string(ot->srna, "name", "", BKE_ST_MAXNAME, "Name", "Name of the menu");
}

/* ************ window / screen operator definitions ************** */

/* this poll functions is needed in place of WM_operator_winactive
 * while it crashes on full screen */
static int wm_operator_winactive_normal(bContext *C)
{
	wmWindow *win = CTX_wm_window(C);

	if (win == NULL || win->screen == NULL || win->screen->full != SCREENNORMAL)
		return 0;

	return 1;
}

static void WM_OT_window_duplicate(wmOperatorType *ot)
{
	ot->name = "Duplicate Window";
	ot->idname = "WM_OT_window_duplicate";
	ot->description = "Duplicate the current Blender window";
		
	ot->exec = wm_window_duplicate_exec;
	ot->poll = wm_operator_winactive_normal;
}

static void WM_OT_save_homefile(wmOperatorType *ot)
{
	ot->name = "Save User Settings";
	ot->idname = "WM_OT_save_homefile";
	ot->description = "Make the current file the default .blend file";
		
	ot->invoke = WM_operator_confirm;
	ot->exec = WM_write_homefile;
	ot->poll = WM_operator_winactive;
}

static void WM_OT_read_homefile(wmOperatorType *ot)
{
	ot->name = "Reload Start-Up File";
	ot->idname = "WM_OT_read_homefile";
	ot->description = "Open the default file (doesn't save the current file)";
	
	ot->invoke = WM_operator_confirm;
	ot->exec = WM_read_homefile_exec;
	/* ommit poll to run in background mode */
}

static void WM_OT_read_factory_settings(wmOperatorType *ot)
{
	ot->name = "Load Factory Settings";
	ot->idname = "WM_OT_read_factory_settings";
	ot->description = "Load default file and user preferences";
	
	ot->invoke = WM_operator_confirm;
	ot->exec = WM_read_homefile_exec;
	/* ommit poll to run in background mode */
}

/* *************** open file **************** */

static void open_set_load_ui(wmOperator *op)
{
	if (!RNA_struct_property_is_set(op->ptr, "load_ui"))
		RNA_boolean_set(op->ptr, "load_ui", !(U.flag & USER_FILENOUI));
}

static void open_set_use_scripts(wmOperator *op)
{
	if (!RNA_struct_property_is_set(op->ptr, "use_scripts")) {
		/* use G_SCRIPT_AUTOEXEC rather than the userpref because this means if
		 * the flag has been disabled from the command line, then opening
		 * from the menu wont enable this setting. */
		RNA_boolean_set(op->ptr, "use_scripts", (G.f & G_SCRIPT_AUTOEXEC));
	}
}

static int wm_open_mainfile_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	const char *openname = G.main->name;

	if (CTX_wm_window(C) == NULL) {
		/* in rare cases this could happen, when trying to invoke in background
		 * mode on load for example. Don't use poll for this because exec()
		 * can still run without a window */
		BKE_report(op->reports, RPT_ERROR, "Context window not set");
		return OPERATOR_CANCELLED;
	}

	/* if possible, get the name of the most recently used .blend file */
	if (G.recent_files.first) {
		struct RecentFile *recent = G.recent_files.first;
		openname = recent->filepath;
	}

	RNA_string_set(op->ptr, "filepath", openname);
	open_set_load_ui(op);
	open_set_use_scripts(op);

	WM_event_add_fileselect(C, op);

	return OPERATOR_RUNNING_MODAL;
}

static int wm_open_mainfile_exec(bContext *C, wmOperator *op)
{
	char path[FILE_MAX];

	RNA_string_get(op->ptr, "filepath", path);
	open_set_load_ui(op);
	open_set_use_scripts(op);

	if (RNA_boolean_get(op->ptr, "load_ui"))
		G.fileflags &= ~G_FILE_NO_UI;
	else
		G.fileflags |= G_FILE_NO_UI;
		
	if (RNA_boolean_get(op->ptr, "use_scripts"))
		G.f |= G_SCRIPT_AUTOEXEC;
	else
		G.f &= ~G_SCRIPT_AUTOEXEC;
	
	// XXX wm in context is not set correctly after WM_read_file -> crash
	// do it before for now, but is this correct with multiple windows?
	WM_event_add_notifier(C, NC_WINDOW, NULL);

	WM_read_file(C, path, op->reports);
	
	return OPERATOR_FINISHED;
}

static void WM_OT_open_mainfile(wmOperatorType *ot)
{
	ot->name = "Open Blender File";
	ot->idname = "WM_OT_open_mainfile";
	ot->description = "Open a Blender file";

	ot->invoke = wm_open_mainfile_invoke;
	ot->exec = wm_open_mainfile_exec;
	/* ommit window poll so this can work in background mode */

	WM_operator_properties_filesel(ot, FOLDERFILE | BLENDERFILE, FILE_BLENDER, FILE_OPENFILE,
	                               WM_FILESEL_FILEPATH, FILE_DEFAULTDISPLAY);

	RNA_def_boolean(ot->srna, "load_ui", 1, "Load UI", "Load user interface setup in the .blend file");
	RNA_def_boolean(ot->srna, "use_scripts", 1, "Trusted Source",
	                "Allow .blend file to execute scripts automatically, default available from system preferences");
}

/* **************** link/append *************** */

int wm_link_append_poll(bContext *C)
{
	if (WM_operator_winactive(C)) {
		/* linking changes active object which is pretty useful in general,
		 * but which totally confuses edit mode (i.e. it becoming not so obvious
		 * to leave from edit mode and inwalid tools in toolbar might be displayed)
		 * so disable link/append when in edit mode (sergey) */
		if (CTX_data_edit_object(C))
			return 0;

		return 1;
	}

	return 0;
}

static int wm_link_append_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	if (RNA_struct_property_is_set(op->ptr, "filepath")) {
		return WM_operator_call_notest(C, op);
	} 
	else {
		/* XXX TODO solve where to get last linked library from */
		if (G.lib[0] != '\0') {
			RNA_string_set(op->ptr, "filepath", G.lib);
		}
		else if (G.relbase_valid) {
			char path[FILE_MAX];
			BLI_strncpy(path, G.main->name, sizeof(G.main->name));
			BLI_parent_dir(path);
			RNA_string_set(op->ptr, "filepath", path);
		}
		WM_event_add_fileselect(C, op);
		return OPERATOR_RUNNING_MODAL;
	}
}

static short wm_link_append_flag(wmOperator *op)
{
	short flag = 0;

	if (RNA_boolean_get(op->ptr, "autoselect")) flag |= FILE_AUTOSELECT;
	if (RNA_boolean_get(op->ptr, "active_layer")) flag |= FILE_ACTIVELAY;
	if (RNA_boolean_get(op->ptr, "relative_path")) flag |= FILE_RELPATH;
	if (RNA_boolean_get(op->ptr, "link")) flag |= FILE_LINK;
	if (RNA_boolean_get(op->ptr, "instance_groups")) flag |= FILE_GROUP_INSTANCE;

	return flag;
}

static int wm_link_append_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	Main *mainl = NULL;
	BlendHandle *bh;
	PropertyRNA *prop;
	char name[FILE_MAX], dir[FILE_MAX], libname[FILE_MAX], group[GROUP_MAX];
	int idcode, totfiles = 0;
	short flag;

	RNA_string_get(op->ptr, "filename", name);
	RNA_string_get(op->ptr, "directory", dir);

	/* test if we have a valid data */
	if (BLO_is_a_library(dir, libname, group) == 0) {
		BKE_report(op->reports, RPT_ERROR, "Not a library");
		return OPERATOR_CANCELLED;
	}
	else if (group[0] == 0) {
		BKE_report(op->reports, RPT_ERROR, "Nothing indicated");
		return OPERATOR_CANCELLED;
	}
	else if (BLI_path_cmp(bmain->name, libname) == 0) {
		BKE_report(op->reports, RPT_ERROR, "Cannot use current file as library");
		return OPERATOR_CANCELLED;
	}

	/* check if something is indicated for append/link */
	prop = RNA_struct_find_property(op->ptr, "files");
	if (prop) {
		totfiles = RNA_property_collection_length(op->ptr, prop);
		if (totfiles == 0) {
			if (name[0] == '\0') {
				BKE_report(op->reports, RPT_ERROR, "Nothing indicated");
				return OPERATOR_CANCELLED;
			}
		}
	}
	else if (name[0] == '\0') {
		BKE_report(op->reports, RPT_ERROR, "Nothing indicated");
		return OPERATOR_CANCELLED;
	}

	bh = BLO_blendhandle_from_file(libname, op->reports);

	if (bh == NULL) {
		/* unlikely since we just browsed it, but possible
		 * error reports will have been made by BLO_blendhandle_from_file() */
		return OPERATOR_CANCELLED;
	}


	/* from here down, no error returns */

	idcode = BKE_idcode_from_name(group);

	/* now we have or selected, or an indicated file */
	if (RNA_boolean_get(op->ptr, "autoselect"))
		BKE_scene_base_deselect_all(scene);

	
	flag = wm_link_append_flag(op);

	/* sanity checks for flag */
	if (scene->id.lib && (flag & FILE_GROUP_INSTANCE)) {
		/* TODO, user never gets this message */
		BKE_reportf(op->reports, RPT_WARNING, "Scene '%s' is linked, group instance disabled", scene->id.name + 2);
		flag &= ~FILE_GROUP_INSTANCE;
	}


	/* tag everything, all untagged data can be made local
	 * its also generally useful to know what is new
	 *
	 * take extra care flag_all_listbases_ids(LIB_LINK_TAG, 0) is called after! */
	flag_all_listbases_ids(LIB_PRE_EXISTING, 1);

	/* here appending/linking starts */
	mainl = BLO_library_append_begin(bmain, &bh, libname);
	if (totfiles == 0) {
		BLO_library_append_named_part_ex(C, mainl, &bh, name, idcode, flag);
	}
	else {
		RNA_BEGIN (op->ptr, itemptr, "files")
		{
			RNA_string_get(&itemptr, "name", name);
			BLO_library_append_named_part_ex(C, mainl, &bh, name, idcode, flag);
		}
		RNA_END;
	}
	BLO_library_append_end(C, mainl, &bh, idcode, flag);
	
	/* mark all library linked objects to be updated */
	recalc_all_library_objects(bmain);

	/* append, rather than linking */
	if ((flag & FILE_LINK) == 0) {
		Library *lib = BLI_findstring(&bmain->library, libname, offsetof(Library, filepath));
		if (lib) BKE_library_make_local(bmain, lib, 1);
		else BLI_assert(!"cant find name of just added library!");
	}

	/* important we unset, otherwise these object wont
	 * link into other scenes from this blend file */
	flag_all_listbases_ids(LIB_PRE_EXISTING, 0);

	/* recreate dependency graph to include new objects */
	DAG_scene_sort(bmain, scene);
	DAG_ids_flush_update(bmain, 0);

	BLO_blendhandle_close(bh);

	/* XXX TODO: align G.lib with other directory storage (like last opened image etc...) */
	BLI_strncpy(G.lib, dir, FILE_MAX);

	WM_event_add_notifier(C, NC_WINDOW, NULL);

	return OPERATOR_FINISHED;
}

static void WM_OT_link_append(wmOperatorType *ot)
{
	ot->name = "Link/Append from Library";
	ot->idname = "WM_OT_link_append";
	ot->description = "Link or Append from a Library .blend file";
	
	ot->invoke = wm_link_append_invoke;
	ot->exec = wm_link_append_exec;
	ot->poll = wm_link_append_poll;
	
	ot->flag |= OPTYPE_UNDO;

	WM_operator_properties_filesel(
	    ot, FOLDERFILE | BLENDERFILE, FILE_LOADLIB, FILE_OPENFILE,
	    WM_FILESEL_FILEPATH | WM_FILESEL_DIRECTORY | WM_FILESEL_FILENAME | WM_FILESEL_RELPATH | WM_FILESEL_FILES,
	    FILE_DEFAULTDISPLAY);
	
	RNA_def_boolean(ot->srna, "link", 1, "Link", "Link the objects or datablocks rather than appending");
	RNA_def_boolean(ot->srna, "autoselect", 1, "Select", "Select the linked objects");
	RNA_def_boolean(ot->srna, "active_layer", 1, "Active Layer", "Put the linked objects on the active layer");
	RNA_def_boolean(ot->srna, "instance_groups", 1, "Instance Groups", "Create instances for each group as a DupliGroup");
}	

/* *************** recover last session **************** */

static int wm_recover_last_session_exec(bContext *C, wmOperator *op)
{
	char filename[FILE_MAX];

	G.fileflags |= G_FILE_RECOVER;

	// XXX wm in context is not set correctly after WM_read_file -> crash
	// do it before for now, but is this correct with multiple windows?
	WM_event_add_notifier(C, NC_WINDOW, NULL);

	/* load file */
	BLI_make_file_string("/", filename, BLI_temporary_dir(), "quit.blend");
	WM_read_file(C, filename, op->reports);

	G.fileflags &= ~G_FILE_RECOVER;
	return OPERATOR_FINISHED;
}

static void WM_OT_recover_last_session(wmOperatorType *ot)
{
	ot->name = "Recover Last Session";
	ot->idname = "WM_OT_recover_last_session";
	ot->description = "Open the last closed file (\"quit.blend\")";
	
	ot->exec = wm_recover_last_session_exec;
	ot->poll = WM_operator_winactive;
}

/* *************** recover auto save **************** */

static int wm_recover_auto_save_exec(bContext *C, wmOperator *op)
{
	char path[FILE_MAX];

	RNA_string_get(op->ptr, "filepath", path);

	G.fileflags |= G_FILE_RECOVER;

	// XXX wm in context is not set correctly after WM_read_file -> crash
	// do it before for now, but is this correct with multiple windows?
	WM_event_add_notifier(C, NC_WINDOW, NULL);

	/* load file */
	WM_read_file(C, path, op->reports);

	G.fileflags &= ~G_FILE_RECOVER;

	return OPERATOR_FINISHED;
}

static int wm_recover_auto_save_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	char filename[FILE_MAX];

	wm_autosave_location(filename);
	RNA_string_set(op->ptr, "filepath", filename);
	WM_event_add_fileselect(C, op);

	return OPERATOR_RUNNING_MODAL;
}

static void WM_OT_recover_auto_save(wmOperatorType *ot)
{
	ot->name = "Recover Auto Save";
	ot->idname = "WM_OT_recover_auto_save";
	ot->description = "Open an automatically saved file to recover it";
	
	ot->exec = wm_recover_auto_save_exec;
	ot->invoke = wm_recover_auto_save_invoke;
	ot->poll = WM_operator_winactive;

	WM_operator_properties_filesel(ot, BLENDERFILE, FILE_BLENDER, FILE_OPENFILE, WM_FILESEL_FILEPATH, FILE_LONGDISPLAY);
}

/* *************** save file as **************** */

static void untitled(char *name)
{
	if (G.save_over == 0 && strlen(name) < FILE_MAX - 16) {
		char *c = BLI_last_slash(name);
		
		if (c)
			strcpy(&c[1], "untitled.blend");
		else
			strcpy(name, "untitled.blend");
	}
}

static void save_set_compress(wmOperator *op)
{
	if (!RNA_struct_property_is_set(op->ptr, "compress")) {
		if (G.save_over) /* keep flag for existing file */
			RNA_boolean_set(op->ptr, "compress", G.fileflags & G_FILE_COMPRESS);
		else /* use userdef for new file */
			RNA_boolean_set(op->ptr, "compress", U.flag & USER_FILECOMPRESS);
	}
}

static int wm_save_as_mainfile_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	char name[FILE_MAX];

	save_set_compress(op);
	
	/* if not saved before, get the name of the most recently used .blend file */
	if (G.main->name[0] == 0 && G.recent_files.first) {
		struct RecentFile *recent = G.recent_files.first;
		BLI_strncpy(name, recent->filepath, FILE_MAX);
	}
	else
		BLI_strncpy(name, G.main->name, FILE_MAX);
	
	untitled(name);
	RNA_string_set(op->ptr, "filepath", name);
	
	WM_event_add_fileselect(C, op);

	return OPERATOR_RUNNING_MODAL;
}

/* function used for WM_OT_save_mainfile too */
static int wm_save_as_mainfile_exec(bContext *C, wmOperator *op)
{
	char path[FILE_MAX];
	int fileflags;
	int copy = 0;

	save_set_compress(op);
	
	if (RNA_struct_property_is_set(op->ptr, "filepath"))
		RNA_string_get(op->ptr, "filepath", path);
	else {
		BLI_strncpy(path, G.main->name, FILE_MAX);
		untitled(path);
	}

	if (RNA_struct_property_is_set(op->ptr, "copy"))
		copy = RNA_boolean_get(op->ptr, "copy");
	
	fileflags = G.fileflags;

	/* set compression flag */
	if (RNA_boolean_get(op->ptr, "compress")) fileflags |=  G_FILE_COMPRESS;
	else fileflags &= ~G_FILE_COMPRESS;
	if (RNA_boolean_get(op->ptr, "relative_remap")) fileflags |=  G_FILE_RELATIVE_REMAP;
	else fileflags &= ~G_FILE_RELATIVE_REMAP;
#ifdef USE_BMESH_SAVE_AS_COMPAT
	/* property only exists for 'Save As' */
	if (RNA_struct_find_property(op->ptr, "use_mesh_compat")) {
		if (RNA_boolean_get(op->ptr, "use_mesh_compat")) fileflags |=  G_FILE_MESH_COMPAT;
		else fileflags &= ~G_FILE_MESH_COMPAT;
	}
	else {
		fileflags &= ~G_FILE_MESH_COMPAT;
	}
#endif

	if (WM_write_file(C, path, fileflags, op->reports, copy) != 0)
		return OPERATOR_CANCELLED;

	WM_event_add_notifier(C, NC_WM | ND_FILESAVE, NULL);

	return OPERATOR_FINISHED;
}

/* function used for WM_OT_save_mainfile too */
static int blend_save_check(bContext *UNUSED(C), wmOperator *op)
{
	char filepath[FILE_MAX];
	RNA_string_get(op->ptr, "filepath", filepath);
	if (!BLO_has_bfile_extension(filepath)) {
		/* some users would prefer BLI_replace_extension(),
		 * we keep getting knit-picking bug reports about this - campbell */
		BLI_ensure_extension(filepath, FILE_MAX, ".blend");
		RNA_string_set(op->ptr, "filepath", filepath);
		return TRUE;
	}
	return FALSE;
}

static void WM_OT_save_as_mainfile(wmOperatorType *ot)
{
	ot->name = "Save As Blender File";
	ot->idname = "WM_OT_save_as_mainfile";
	ot->description = "Save the current file in the desired location";
	
	ot->invoke = wm_save_as_mainfile_invoke;
	ot->exec = wm_save_as_mainfile_exec;
	ot->check = blend_save_check;
	/* ommit window poll so this can work in background mode */

	WM_operator_properties_filesel(ot, FOLDERFILE | BLENDERFILE, FILE_BLENDER, FILE_SAVE, WM_FILESEL_FILEPATH, FILE_DEFAULTDISPLAY);
	RNA_def_boolean(ot->srna, "compress", 0, "Compress", "Write compressed .blend file");
	RNA_def_boolean(ot->srna, "relative_remap", 1, "Remap Relative", "Remap relative paths when saving in a different directory");
	RNA_def_boolean(ot->srna, "copy", 0, "Save Copy", "Save a copy of the actual working state but does not make saved file active");
#ifdef USE_BMESH_SAVE_AS_COMPAT
	RNA_def_boolean(ot->srna, "use_mesh_compat", 0, "Legacy Mesh Format", "Save using legacy mesh format (no ngons)");
#endif
}

/* *************** save file directly ******** */

static int wm_save_mainfile_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	char name[FILE_MAX];
	int check_existing = 1;
	int ret;
	
	/* cancel if no active window */
	if (CTX_wm_window(C) == NULL)
		return OPERATOR_CANCELLED;

	save_set_compress(op);

	/* if not saved before, get the name of the most recently used .blend file */
	if (G.main->name[0] == 0 && G.recent_files.first) {
		struct RecentFile *recent = G.recent_files.first;
		BLI_strncpy(name, recent->filepath, FILE_MAX);
	}
	else
		BLI_strncpy(name, G.main->name, FILE_MAX);

	untitled(name);
	
	RNA_string_set(op->ptr, "filepath", name);
	
	if (RNA_struct_find_property(op->ptr, "check_existing"))
		if (RNA_boolean_get(op->ptr, "check_existing") == 0)
			check_existing = 0;
	
	if (G.save_over) {
		if (check_existing && BLI_exists(name)) {
			uiPupMenuSaveOver(C, op, name);
			ret = OPERATOR_RUNNING_MODAL;
		}
		else {
			ret = wm_save_as_mainfile_exec(C, op);
		}
	}
	else {
		WM_event_add_fileselect(C, op);
		ret = OPERATOR_RUNNING_MODAL;
	}
	
	return ret;
}

static void WM_OT_save_mainfile(wmOperatorType *ot)
{
	ot->name = "Save Blender File";
	ot->idname = "WM_OT_save_mainfile";
	ot->description = "Save the current Blender file";
	
	ot->invoke = wm_save_mainfile_invoke;
	ot->exec = wm_save_as_mainfile_exec;
	ot->check = blend_save_check;
	/* ommit window poll so this can work in background mode */
	
	WM_operator_properties_filesel(ot, FOLDERFILE | BLENDERFILE, FILE_BLENDER, FILE_SAVE, WM_FILESEL_FILEPATH, FILE_DEFAULTDISPLAY);
	RNA_def_boolean(ot->srna, "compress", 0, "Compress", "Write compressed .blend file");
	RNA_def_boolean(ot->srna, "relative_remap", 0, "Remap Relative", "Remap relative paths when saving in a different directory");
}

/* XXX: move these collada operators to a more appropriate place */
#ifdef WITH_COLLADA

#include "../../collada/collada.h"

static int wm_collada_export_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{	
	if (!RNA_struct_property_is_set(op->ptr, "filepath")) {
		char filepath[FILE_MAX];

		if (G.main->name[0] == 0)
			BLI_strncpy(filepath, "untitled", sizeof(filepath));
		else
			BLI_strncpy(filepath, G.main->name, sizeof(filepath));

		BLI_replace_extension(filepath, sizeof(filepath), ".dae");
		RNA_string_set(op->ptr, "filepath", filepath);
	}

	WM_event_add_fileselect(C, op);

	return OPERATOR_RUNNING_MODAL;
}

/* function used for WM_OT_save_mainfile too */
static int wm_collada_export_exec(bContext *C, wmOperator *op)
{
	char filename[FILE_MAX];
	int selected, second_life, apply_modifiers;
	
	if (!RNA_struct_property_is_set(op->ptr, "filepath")) {
		BKE_report(op->reports, RPT_ERROR, "No filename given");
		return OPERATOR_CANCELLED;
	}

	RNA_string_get(op->ptr, "filepath", filename);

	/* Options panel */
	selected        = RNA_boolean_get(op->ptr, "selected");
	second_life     = RNA_boolean_get(op->ptr, "second_life");
	apply_modifiers = RNA_boolean_get(op->ptr, "apply_modifiers");

	/* get editmode results */
	ED_object_exit_editmode(C, 0);  /* 0 = does not exit editmode */

	if (collada_export(CTX_data_scene(C), filename, selected, apply_modifiers, second_life)) {
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

static void WM_OT_collada_export(wmOperatorType *ot)
{
	ot->name = "Export COLLADA";
	ot->description = "Save a Collada file";
	ot->idname = "WM_OT_collada_export";
	
	ot->invoke = wm_collada_export_invoke;
	ot->exec = wm_collada_export_exec;
	ot->poll = WM_operator_winactive;
	
	WM_operator_properties_filesel(ot, FOLDERFILE | COLLADAFILE, FILE_BLENDER, FILE_SAVE, WM_FILESEL_FILEPATH, FILE_DEFAULTDISPLAY);
	RNA_def_boolean(ot->srna, "selected", 0, "Selection Only",
	                "Export only selected elements");
	RNA_def_boolean(ot->srna, "apply_modifiers", 0, "Apply Modifiers",
	                "Apply modifiers (Preview Resolution)");
	RNA_def_boolean(ot->srna, "second_life", 0, "Export for Second Life",
	                "Compatibility mode for Second Life");
}

/* function used for WM_OT_save_mainfile too */
static int wm_collada_import_exec(bContext *C, wmOperator *op)
{
	char filename[FILE_MAX];
	
	if (!RNA_struct_property_is_set(op->ptr, "filepath")) {
		BKE_report(op->reports, RPT_ERROR, "No filename given");
		return OPERATOR_CANCELLED;
	}

	RNA_string_get(op->ptr, "filepath", filename);
	if (collada_import(C, filename)) return OPERATOR_FINISHED;
	
	BKE_report(op->reports, RPT_ERROR, "Errors found during parsing COLLADA document. Please see console for error log.");
	
	return OPERATOR_FINISHED;
}

static void WM_OT_collada_import(wmOperatorType *ot)
{
	ot->name = "Import COLLADA";
	ot->description = "Load a Collada file";
	ot->idname = "WM_OT_collada_import";
	
	ot->invoke = WM_operator_filesel;
	ot->exec = wm_collada_import_exec;
	ot->poll = WM_operator_winactive;
	
	WM_operator_properties_filesel(ot, FOLDERFILE | COLLADAFILE, FILE_BLENDER, FILE_OPENFILE, WM_FILESEL_FILEPATH, FILE_DEFAULTDISPLAY);
}

#endif


/* *********************** */

static void WM_OT_window_fullscreen_toggle(wmOperatorType *ot)
{
	ot->name = "Toggle Fullscreen";
	ot->idname = "WM_OT_window_fullscreen_toggle";
	ot->description = "Toggle the current window fullscreen";

	ot->exec = wm_window_fullscreen_toggle_exec;
	ot->poll = WM_operator_winactive;
}

static int wm_exit_blender_op(bContext *C, wmOperator *op)
{
	WM_operator_free(op);
	
	WM_exit(C);	
	
	return OPERATOR_FINISHED;
}

static void WM_OT_quit_blender(wmOperatorType *ot)
{
	ot->name = "Quit Blender";
	ot->idname = "WM_OT_quit_blender";
	ot->description = "Quit Blender";

	ot->invoke = WM_operator_confirm;
	ot->exec = wm_exit_blender_op;
	ot->poll = WM_operator_winactive;
}

/* *********************** */

#if defined(WIN32)

static int wm_console_toggle_op(bContext *UNUSED(C), wmOperator *UNUSED(op))
{
	GHOST_toggleConsole(2);
	return OPERATOR_FINISHED;
}

static void WM_OT_console_toggle(wmOperatorType *ot)
{
	/* XXX Have to mark these for xgettext, as under linux they do not exists...
	 *     And even worth, have to give the context as text, as xgettext doesn't expand macros. :( */
	ot->name = CTX_N_("Operator"/* BLF_I18NCONTEXT_OPERATOR_DEFAULT */, "Toggle System Console");
	ot->idname = "WM_OT_console_toggle";
	ot->description = N_("Toggle System Console");
	
	ot->exec = wm_console_toggle_op;
	ot->poll = WM_operator_winactive;
}

#endif

/* ************ default paint cursors, draw always around cursor *********** */
/*
 * - returns handler to free
 * - poll(bContext): returns 1 if draw should happen
 * - draw(bContext): drawing callback for paint cursor
 */

void *WM_paint_cursor_activate(wmWindowManager *wm, int (*poll)(bContext *C),
                               wmPaintCursorDraw draw, void *customdata)
{
	wmPaintCursor *pc = MEM_callocN(sizeof(wmPaintCursor), "paint cursor");
	
	BLI_addtail(&wm->paintcursors, pc);
	
	pc->customdata = customdata;
	pc->poll = poll;
	pc->draw = draw;
	
	return pc;
}

void WM_paint_cursor_end(wmWindowManager *wm, void *handle)
{
	wmPaintCursor *pc;
	
	for (pc = wm->paintcursors.first; pc; pc = pc->next) {
		if (pc == (wmPaintCursor *)handle) {
			BLI_remlink(&wm->paintcursors, pc);
			MEM_freeN(pc);
			return;
		}
	}
}

/* ************ window gesture operator-callback definitions ************** */
/*
 * These are default callbacks for use in operators requiring gesture input
 */

/* **************** Border gesture *************** */

/* Border gesture has two types:
 * 1) WM_GESTURE_CROSS_RECT: starts a cross, on mouse click it changes to border
 * 2) WM_GESTURE_RECT: starts immediate as a border, on mouse click or release it ends
 *
 * It stores 4 values (xmin, xmax, ymin, ymax) and event it ended with (event_type)
 */

static int border_apply_rect(wmOperator *op)
{
	wmGesture *gesture = op->customdata;
	rcti *rect = gesture->customdata;
	
	if (rect->xmin == rect->xmax || rect->ymin == rect->ymax)
		return 0;

	
	/* operator arguments and storage. */
	RNA_int_set(op->ptr, "xmin", MIN2(rect->xmin, rect->xmax));
	RNA_int_set(op->ptr, "ymin", MIN2(rect->ymin, rect->ymax));
	RNA_int_set(op->ptr, "xmax", MAX2(rect->xmin, rect->xmax));
	RNA_int_set(op->ptr, "ymax", MAX2(rect->ymin, rect->ymax));

	return 1;
}

static int border_apply(bContext *C, wmOperator *op, int gesture_mode)
{
	if (!border_apply_rect(op))
		return 0;
	
	/* XXX weak; border should be configured for this without reading event types */
	if (RNA_struct_find_property(op->ptr, "gesture_mode") )
		RNA_int_set(op->ptr, "gesture_mode", gesture_mode);

	op->type->exec(C, op);
	return 1;
}

static void wm_gesture_end(bContext *C, wmOperator *op)
{
	wmGesture *gesture = op->customdata;
	
	WM_gesture_end(C, gesture); /* frees gesture itself, and unregisters from window */
	op->customdata = NULL;

	ED_area_tag_redraw(CTX_wm_area(C));
	
	if (RNA_struct_find_property(op->ptr, "cursor") )
		WM_cursor_restore(CTX_wm_window(C));
}

int WM_border_select_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	if (ISTWEAK(event->type))
		op->customdata = WM_gesture_new(C, event, WM_GESTURE_RECT);
	else
		op->customdata = WM_gesture_new(C, event, WM_GESTURE_CROSS_RECT);

	/* add modal handler */
	WM_event_add_modal_handler(C, op);
	
	wm_gesture_tag_redraw(C);

	return OPERATOR_RUNNING_MODAL;
}

int WM_border_select_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	wmGesture *gesture = op->customdata;
	rcti *rect = gesture->customdata;
	int sx, sy;
	
	if (event->type == MOUSEMOVE) {
		wm_subwindow_getorigin(CTX_wm_window(C), gesture->swinid, &sx, &sy);

		if (gesture->type == WM_GESTURE_CROSS_RECT && gesture->mode == 0) {
			rect->xmin = rect->xmax = event->x - sx;
			rect->ymin = rect->ymax = event->y - sy;
		}
		else {
			rect->xmax = event->x - sx;
			rect->ymax = event->y - sy;
		}
		border_apply_rect(op);

		wm_gesture_tag_redraw(C);
	}
	else if (event->type == EVT_MODAL_MAP) {
		switch (event->val) {
			case GESTURE_MODAL_BEGIN:
				if (gesture->type == WM_GESTURE_CROSS_RECT && gesture->mode == 0) {
					gesture->mode = 1;
					wm_gesture_tag_redraw(C);
				}
				break;
			case GESTURE_MODAL_SELECT:
			case GESTURE_MODAL_DESELECT:
			case GESTURE_MODAL_IN:
			case GESTURE_MODAL_OUT:
				if (border_apply(C, op, event->val)) {
					wm_gesture_end(C, op);
					return OPERATOR_FINISHED;
				}
				wm_gesture_end(C, op);
				return OPERATOR_CANCELLED;
				break;

			case GESTURE_MODAL_CANCEL:
				wm_gesture_end(C, op);
				return OPERATOR_CANCELLED;
		}

	}
//	// Allow view navigation???
//	else {
//		return OPERATOR_PASS_THROUGH;
//	}

	return OPERATOR_RUNNING_MODAL;
}

int WM_border_select_cancel(bContext *C, wmOperator *op)
{
	wm_gesture_end(C, op);

	return OPERATOR_CANCELLED;
}

/* **************** circle gesture *************** */
/* works now only for selection or modal paint stuff, calls exec while hold mouse, exit on release */

#ifdef GESTURE_MEMORY
int circle_select_size = 25; // XXX - need some operator memory thing\!
#endif

int WM_gesture_circle_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	op->customdata = WM_gesture_new(C, event, WM_GESTURE_CIRCLE);
	
	/* add modal handler */
	WM_event_add_modal_handler(C, op);
	
	wm_gesture_tag_redraw(C);
	
	return OPERATOR_RUNNING_MODAL;
}

static void gesture_circle_apply(bContext *C, wmOperator *op)
{
	wmGesture *gesture = op->customdata;
	rcti *rect = gesture->customdata;
	
	if (RNA_int_get(op->ptr, "gesture_mode") == GESTURE_MODAL_NOP)
		return;

	/* operator arguments and storage. */
	RNA_int_set(op->ptr, "x", rect->xmin);
	RNA_int_set(op->ptr, "y", rect->ymin);
	RNA_int_set(op->ptr, "radius", rect->xmax);
	
	if (op->type->exec)
		op->type->exec(C, op);
#ifdef GESTURE_MEMORY
	circle_select_size = rect->xmax;
#endif
}

int WM_gesture_circle_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	wmGesture *gesture = op->customdata;
	rcti *rect = gesture->customdata;
	int sx, sy;

	if (event->type == MOUSEMOVE) {
		wm_subwindow_getorigin(CTX_wm_window(C), gesture->swinid, &sx, &sy);

		rect->xmin = event->x - sx;
		rect->ymin = event->y - sy;

		wm_gesture_tag_redraw(C);

		if (gesture->mode)
			gesture_circle_apply(C, op);
	}
	else if (event->type == EVT_MODAL_MAP) {
		switch (event->val) {
			case GESTURE_MODAL_CIRCLE_ADD:
				rect->xmax += 2 + rect->xmax / 10;
				wm_gesture_tag_redraw(C);
				break;
			case GESTURE_MODAL_CIRCLE_SUB:
				rect->xmax -= 2 + rect->xmax / 10;
				if (rect->xmax < 1) rect->xmax = 1;
				wm_gesture_tag_redraw(C);
				break;
			case GESTURE_MODAL_SELECT:
			case GESTURE_MODAL_DESELECT:
			case GESTURE_MODAL_NOP:
				if (RNA_struct_find_property(op->ptr, "gesture_mode"))
					RNA_int_set(op->ptr, "gesture_mode", event->val);

				if (event->val != GESTURE_MODAL_NOP) {
					/* apply first click */
					gesture_circle_apply(C, op);
					gesture->mode = 1;
					wm_gesture_tag_redraw(C);
				}
				break;

			case GESTURE_MODAL_CANCEL:
			case GESTURE_MODAL_CONFIRM:
				wm_gesture_end(C, op);
				return OPERATOR_FINISHED; /* use finish or we don't get an undo */
		}
	}
//	// Allow view navigation???
//	else {
//		return OPERATOR_PASS_THROUGH;
//	}

	return OPERATOR_RUNNING_MODAL;
}

int WM_gesture_circle_cancel(bContext *C, wmOperator *op)
{
	wm_gesture_end(C, op);

	return OPERATOR_CANCELLED;
}

#if 0
/* template to copy from */
void WM_OT_circle_gesture(wmOperatorType *ot)
{
	ot->name = "Circle Gesture";
	ot->idname = "WM_OT_circle_gesture";
	ot->description = "Enter rotate mode with a circular gesture";
	
	ot->invoke = WM_gesture_circle_invoke;
	ot->modal = WM_gesture_circle_modal;
	
	ot->poll = WM_operator_winactive;
	
	RNA_def_property(ot->srna, "x", PROP_INT, PROP_NONE);
	RNA_def_property(ot->srna, "y", PROP_INT, PROP_NONE);
	RNA_def_property(ot->srna, "radius", PROP_INT, PROP_NONE);

}
#endif

/* **************** Tweak gesture *************** */

static void tweak_gesture_modal(bContext *C, wmEvent *event)
{
	wmWindow *window = CTX_wm_window(C);
	wmGesture *gesture = window->tweak;
	rcti *rect = gesture->customdata;
	int sx, sy, val;
	
	switch (event->type) {
		case MOUSEMOVE:
		case INBETWEEN_MOUSEMOVE:
			
			wm_subwindow_getorigin(window, gesture->swinid, &sx, &sy);
			
			rect->xmax = event->x - sx;
			rect->ymax = event->y - sy;
			
			if ((val = wm_gesture_evaluate(gesture))) {
				wmEvent tevent;

				tevent = *(window->eventstate);
				if (gesture->event_type == LEFTMOUSE)
					tevent.type = EVT_TWEAK_L;
				else if (gesture->event_type == RIGHTMOUSE)
					tevent.type = EVT_TWEAK_R;
				else
					tevent.type = EVT_TWEAK_M;
				tevent.val = val;
				/* mouse coords! */
				wm_event_add(window, &tevent);
				
				WM_gesture_end(C, gesture); /* frees gesture itself, and unregisters from window */
			}
			
			break;
			
		case LEFTMOUSE:
		case RIGHTMOUSE:
		case MIDDLEMOUSE:
			if (gesture->event_type == event->type) {
				WM_gesture_end(C, gesture);

				/* when tweak fails we should give the other keymap entries a chance */
				event->val = KM_RELEASE;
			}
			break;
		default:
			if (!ISTIMER(event->type)) {
				WM_gesture_end(C, gesture);
			}
			break;
	}
}

/* standard tweak, called after window handlers passed on event */
void wm_tweakevent_test(bContext *C, wmEvent *event, int action)
{
	wmWindow *win = CTX_wm_window(C);
	
	if (win->tweak == NULL) {
		if (CTX_wm_region(C)) {
			if (event->val == KM_PRESS) {
				if (ELEM3(event->type, LEFTMOUSE, MIDDLEMOUSE, RIGHTMOUSE) )
					win->tweak = WM_gesture_new(C, event, WM_GESTURE_TWEAK);
			}
		}
	}
	else {
		/* no tweaks if event was handled */
		if ((action & WM_HANDLER_BREAK)) {
			WM_gesture_end(C, win->tweak);
		}
		else
			tweak_gesture_modal(C, event);
	}
}

/* *********************** lasso gesture ****************** */

int WM_gesture_lasso_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	op->customdata = WM_gesture_new(C, event, WM_GESTURE_LASSO);
	
	/* add modal handler */
	WM_event_add_modal_handler(C, op);
	
	wm_gesture_tag_redraw(C);
	
	if (RNA_struct_find_property(op->ptr, "cursor") )
		WM_cursor_modal(CTX_wm_window(C), RNA_int_get(op->ptr, "cursor"));
	
	return OPERATOR_RUNNING_MODAL;
}

int WM_gesture_lines_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	op->customdata = WM_gesture_new(C, event, WM_GESTURE_LINES);
	
	/* add modal handler */
	WM_event_add_modal_handler(C, op);
	
	wm_gesture_tag_redraw(C);
	
	if (RNA_struct_find_property(op->ptr, "cursor") )
		WM_cursor_modal(CTX_wm_window(C), RNA_int_get(op->ptr, "cursor"));
	
	return OPERATOR_RUNNING_MODAL;
}


static void gesture_lasso_apply(bContext *C, wmOperator *op)
{
	wmGesture *gesture = op->customdata;
	PointerRNA itemptr;
	float loc[2];
	int i;
	short *lasso = gesture->customdata;
	
	/* operator storage as path. */

	RNA_collection_clear(op->ptr, "path");
	for (i = 0; i < gesture->points; i++, lasso += 2) {
		loc[0] = lasso[0];
		loc[1] = lasso[1];
		RNA_collection_add(op->ptr, "path", &itemptr);
		RNA_float_set_array(&itemptr, "loc", loc);
	}
	
	wm_gesture_end(C, op);
		
	if (op->type->exec)
		op->type->exec(C, op);
}

int WM_gesture_lasso_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	wmGesture *gesture = op->customdata;
	int sx, sy;
	
	switch (event->type) {
		case MOUSEMOVE:
		case INBETWEEN_MOUSEMOVE:
			
			wm_gesture_tag_redraw(C);
			
			wm_subwindow_getorigin(CTX_wm_window(C), gesture->swinid, &sx, &sy);

			if (gesture->points == gesture->size) {
				short *old_lasso = gesture->customdata;
				gesture->customdata = MEM_callocN(2 * sizeof(short) * (gesture->size + WM_LASSO_MIN_POINTS), "lasso points");
				memcpy(gesture->customdata, old_lasso, 2 * sizeof(short) * gesture->size);
				gesture->size = gesture->size + WM_LASSO_MIN_POINTS;
				MEM_freeN(old_lasso);
				// printf("realloc\n");
			}

			{
				int x, y;
				short *lasso = gesture->customdata;
				
				lasso += (2 * gesture->points - 2);
				x = (event->x - sx - lasso[0]);
				y = (event->y - sy - lasso[1]);
				
				/* make a simple distance check to get a smoother lasso
				 * add only when at least 2 pixels between this and previous location */
				if ((x * x + y * y) > 4) {
					lasso += 2;
					lasso[0] = event->x - sx;
					lasso[1] = event->y - sy;
					gesture->points++;
				}
			}
			break;
			
		case LEFTMOUSE:
		case MIDDLEMOUSE:
		case RIGHTMOUSE:
			if (event->val == KM_RELEASE) {   /* key release */
				gesture_lasso_apply(C, op);
				return OPERATOR_FINISHED;
			}
			break;
		case ESCKEY:
			wm_gesture_end(C, op);
			return OPERATOR_CANCELLED;
	}
	return OPERATOR_RUNNING_MODAL;
}

int WM_gesture_lines_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	return WM_gesture_lasso_modal(C, op, event);
}

int WM_gesture_lasso_cancel(bContext *C, wmOperator *op)
{
	wm_gesture_end(C, op);

	return OPERATOR_CANCELLED;
}

int WM_gesture_lines_cancel(bContext *C, wmOperator *op)
{
	wm_gesture_end(C, op);

	return OPERATOR_CANCELLED;
}

/**
 * helper function, we may want to add options for conversion to view space
 *
 * caller must free.
 */
int (*WM_gesture_lasso_path_to_array(bContext *UNUSED(C), wmOperator *op, int *mcords_tot))[2]
{
	PropertyRNA *prop = RNA_struct_find_property(op->ptr, "path");
	int (*mcords)[2] = NULL;
	BLI_assert(prop != NULL);

	if (prop) {
		const int len = RNA_property_collection_length(op->ptr, prop);

		if (len) {
			int i = 0;
			mcords = MEM_mallocN(sizeof(int) * 2 * len, __func__);

			RNA_PROP_BEGIN(op->ptr, itemptr, prop)
			{
				float loc[2];

				RNA_float_get_array(&itemptr, "loc", loc);
				mcords[i][0] = (int)loc[0];
				mcords[i][1] = (int)loc[1];
				i++;
			}
			RNA_PROP_END;
		}
		*mcords_tot = len;
	}
	else {
		*mcords_tot = 0;
	}

	return mcords;
}

#if 0
/* template to copy from */

static int gesture_lasso_exec(bContext *C, wmOperator *op)
{
	RNA_BEGIN (op->ptr, itemptr, "path")
	{
		float loc[2];
		
		RNA_float_get_array(&itemptr, "loc", loc);
		printf("Location: %f %f\n", loc[0], loc[1]);
	}
	RNA_END;
	
	return OPERATOR_FINISHED;
}

void WM_OT_lasso_gesture(wmOperatorType *ot)
{
	PropertyRNA *prop;
	
	ot->name = "Lasso Gesture";
	ot->idname = "WM_OT_lasso_gesture";
	ot->description = "Select objects within the lasso as you move the pointer";
	
	ot->invoke = WM_gesture_lasso_invoke;
	ot->modal = WM_gesture_lasso_modal;
	ot->exec = gesture_lasso_exec;
	
	ot->poll = WM_operator_winactive;
	
	prop = RNA_def_property(ot->srna, "path", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_runtime(prop, &RNA_OperatorMousePath);
}
#endif

/* *********************** straight line gesture ****************** */

static int straightline_apply(bContext *C, wmOperator *op)
{
	wmGesture *gesture = op->customdata;
	rcti *rect = gesture->customdata;
	
	if (rect->xmin == rect->xmax && rect->ymin == rect->ymax)
		return 0;
	
	/* operator arguments and storage. */
	RNA_int_set(op->ptr, "xstart", rect->xmin);
	RNA_int_set(op->ptr, "ystart", rect->ymin);
	RNA_int_set(op->ptr, "xend", rect->xmax);
	RNA_int_set(op->ptr, "yend", rect->ymax);

	if (op->type->exec)
		op->type->exec(C, op);
	
	return 1;
}


int WM_gesture_straightline_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	op->customdata = WM_gesture_new(C, event, WM_GESTURE_STRAIGHTLINE);
	
	/* add modal handler */
	WM_event_add_modal_handler(C, op);
	
	wm_gesture_tag_redraw(C);
	
	if (RNA_struct_find_property(op->ptr, "cursor") )
		WM_cursor_modal(CTX_wm_window(C), RNA_int_get(op->ptr, "cursor"));
		
	return OPERATOR_RUNNING_MODAL;
}

int WM_gesture_straightline_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	wmGesture *gesture = op->customdata;
	rcti *rect = gesture->customdata;
	int sx, sy;
	
	if (event->type == MOUSEMOVE) {
		wm_subwindow_getorigin(CTX_wm_window(C), gesture->swinid, &sx, &sy);
		
		if (gesture->mode == 0) {
			rect->xmin = rect->xmax = event->x - sx;
			rect->ymin = rect->ymax = event->y - sy;
		}
		else {
			rect->xmax = event->x - sx;
			rect->ymax = event->y - sy;
			straightline_apply(C, op);
		}
		
		wm_gesture_tag_redraw(C);
	}
	else if (event->type == EVT_MODAL_MAP) {
		switch (event->val) {
			case GESTURE_MODAL_BEGIN:
				if (gesture->mode == 0) {
					gesture->mode = 1;
					wm_gesture_tag_redraw(C);
				}
				break;
			case GESTURE_MODAL_SELECT:
				if (straightline_apply(C, op)) {
					wm_gesture_end(C, op);
					return OPERATOR_FINISHED;
				}
				wm_gesture_end(C, op);
				return OPERATOR_CANCELLED;
				break;
				
			case GESTURE_MODAL_CANCEL:
				wm_gesture_end(C, op);
				return OPERATOR_CANCELLED;
		}
		
	}

	return OPERATOR_RUNNING_MODAL;
}

int WM_gesture_straightline_cancel(bContext *C, wmOperator *op)
{
	wm_gesture_end(C, op);

	return OPERATOR_CANCELLED;
}

#if 0
/* template to copy from */
void WM_OT_straightline_gesture(wmOperatorType *ot)
{
	PropertyRNA *prop;
	
	ot->name = "Straight Line Gesture";
	ot->idname = "WM_OT_straightline_gesture";
	ot->description = "Draw a straight line as you move the pointer";
	
	ot->invoke = WM_gesture_straightline_invoke;
	ot->modal = WM_gesture_straightline_modal;
	ot->exec = gesture_straightline_exec;
	
	ot->poll = WM_operator_winactive;
	
	WM_operator_properties_gesture_straightline(ot, 0);
}
#endif

/* *********************** radial control ****************** */

static const int WM_RADIAL_CONTROL_DISPLAY_SIZE = 200;

typedef struct {
	PropertyType type;
	PropertySubType subtype;
	PointerRNA ptr, col_ptr, fill_col_ptr, rot_ptr, zoom_ptr, image_id_ptr;
	PropertyRNA *prop, *col_prop, *fill_col_prop, *rot_prop, *zoom_prop;
	StructRNA *image_id_srna;
	float initial_value, current_value, min_value, max_value;
	int initial_mouse[2];
	unsigned int gltex;
	ListBase orig_paintcursors;
	void *cursor;
} RadialControl;

static void radial_control_set_initial_mouse(RadialControl *rc, wmEvent *event)
{
	float d[2] = {0, 0};
	float zoom[2] = {1, 1};

	rc->initial_mouse[0] = event->x;
	rc->initial_mouse[1] = event->y;

	switch (rc->subtype) {
		case PROP_DISTANCE:
			d[0] = rc->initial_value;
			break;
		case PROP_FACTOR:
			d[0] = WM_RADIAL_CONTROL_DISPLAY_SIZE * (1 - rc->initial_value);
			break;
		case PROP_ANGLE:
			d[0] = WM_RADIAL_CONTROL_DISPLAY_SIZE * cos(rc->initial_value);
			d[1] = WM_RADIAL_CONTROL_DISPLAY_SIZE * sin(rc->initial_value);
			break;
		default:
			return;
	}

	if (rc->zoom_prop) {
		RNA_property_float_get_array(&rc->zoom_ptr, rc->zoom_prop, zoom);
		d[0] *= zoom[0];
		d[1] *= zoom[1];
	}

	rc->initial_mouse[0] -= d[0];
	rc->initial_mouse[1] -= d[1];
}

static void radial_control_set_tex(RadialControl *rc)
{
	ImBuf *ibuf;

	switch (RNA_type_to_ID_code(rc->image_id_ptr.type)) {
		case ID_BR:
			if ((ibuf = BKE_brush_gen_radial_control_imbuf(rc->image_id_ptr.data))) {
				glGenTextures(1, &rc->gltex);
				glBindTexture(GL_TEXTURE_2D, rc->gltex);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, ibuf->x, ibuf->y, 0,
				             GL_ALPHA, GL_FLOAT, ibuf->rect_float);
				MEM_freeN(ibuf->rect_float);
				MEM_freeN(ibuf);
			}
			break;
		default:
			break;
	}
}

static void radial_control_paint_tex(RadialControl *rc, float radius, float alpha)
{
	float col[3] = {0, 0, 0};
	float rot;

	/* set fill color */
	if (rc->fill_col_prop)
		RNA_property_float_get_array(&rc->fill_col_ptr, rc->fill_col_prop, col);
	glColor4f(col[0], col[1], col[2], alpha);

	if (rc->gltex) {
		glBindTexture(GL_TEXTURE_2D, rc->gltex);

		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		/* set up rotation if available */
		if (rc->rot_prop) {
			rot = RNA_property_float_get(&rc->rot_ptr, rc->rot_prop);
			glPushMatrix();
			glRotatef(RAD2DEGF(rot), 0, 0, 1);
		}

		/* draw textured quad */
		glEnable(GL_TEXTURE_2D);
		glBegin(GL_QUADS);
		glTexCoord2f(0, 0);
		glVertex2f(-radius, -radius);
		glTexCoord2f(1, 0);
		glVertex2f(radius, -radius);
		glTexCoord2f(1, 1);
		glVertex2f(radius, radius);
		glTexCoord2f(0, 1);
		glVertex2f(-radius, radius);
		glEnd();
		glDisable(GL_TEXTURE_2D);

		/* undo rotation */
		if (rc->rot_prop)
			glPopMatrix();
	}
	else {
		/* flat color if no texture available */
		glutil_draw_filled_arc(0, M_PI * 2, radius, 40);
	}
}

static void radial_control_paint_cursor(bContext *C, int x, int y, void *customdata)
{
	RadialControl *rc = customdata;
	ARegion *ar = CTX_wm_region(C);
	float r1 = 0.0f, r2 = 0.0f, tex_radius, alpha;
	float zoom[2], col[3] = {1, 1, 1};

	switch (rc->subtype) {
		case PROP_DISTANCE:
			r1 = rc->current_value;
			r2 = rc->initial_value;
			tex_radius = r1;
			alpha = 0.75;
			break;
		case PROP_FACTOR:
			r1 = (1 - rc->current_value) * WM_RADIAL_CONTROL_DISPLAY_SIZE;
			r2 = tex_radius = WM_RADIAL_CONTROL_DISPLAY_SIZE;
			alpha = rc->current_value / 2.0f + 0.5f;
			break;
		case PROP_ANGLE:
			r1 = r2 = tex_radius = WM_RADIAL_CONTROL_DISPLAY_SIZE;
			alpha = 0.75;
			break;
		default:
			tex_radius = WM_RADIAL_CONTROL_DISPLAY_SIZE; /* note, this is a dummy value */
			alpha = 0.75;
			break;
	}

	/* Keep cursor in the original place */
	x = rc->initial_mouse[0] - ar->winrct.xmin;
	y = rc->initial_mouse[1] - ar->winrct.ymin;
	glTranslatef((float)x, (float)y, 0.0f);

	glEnable(GL_BLEND);
	glEnable(GL_LINE_SMOOTH);

	/* apply zoom if available */
	if (rc->zoom_prop) {
		RNA_property_float_get_array(&rc->zoom_ptr, rc->zoom_prop, zoom);
		glScalef(zoom[0], zoom[1], 1);
	}

	/* draw rotated texture */
	radial_control_paint_tex(rc, tex_radius, alpha);

	/* set line color */
	if (rc->col_prop)
		RNA_property_float_get_array(&rc->col_ptr, rc->col_prop, col);
	glColor4f(col[0], col[1], col[2], 0.5);

	if (rc->subtype == PROP_ANGLE) {
		glPushMatrix();
		/* draw original angle line */
		glRotatef(RAD2DEGF(rc->initial_value), 0, 0, 1);
		fdrawline(0.0f, 0.0f, (float)WM_RADIAL_CONTROL_DISPLAY_SIZE, 0.0f);
		/* draw new angle line */
		glRotatef(RAD2DEGF(rc->current_value - rc->initial_value), 0, 0, 1);
		fdrawline(0.0f, 0.0f, (float)WM_RADIAL_CONTROL_DISPLAY_SIZE, 0.0f);
		glPopMatrix();
	}

	/* draw circles on top */
	glutil_draw_lined_arc(0.0, (float)(M_PI * 2.0), r1, 40);
	glutil_draw_lined_arc(0.0, (float)(M_PI * 2.0), r2, 40);

	glDisable(GL_BLEND);
	glDisable(GL_LINE_SMOOTH);
}

typedef enum {
	RC_PROP_ALLOW_MISSING = 1,
	RC_PROP_REQUIRE_FLOAT = 2,
	RC_PROP_REQUIRE_BOOL = 4,
} RCPropFlags;

/* attempt to retrieve the rna pointer/property from an rna path;
 * returns 0 for failure, 1 for success, and also 1 if property is not
 * set */
static int radial_control_get_path(PointerRNA *ctx_ptr, wmOperator *op,
                                   const char *name, PointerRNA *r_ptr,
                                   PropertyRNA **r_prop, int req_length, RCPropFlags flags)
{
	PropertyRNA *unused_prop;
	int len;
	char *str;

	/* check flags */
	if ((flags & RC_PROP_REQUIRE_BOOL) && (flags & RC_PROP_REQUIRE_FLOAT)) {
		BKE_reportf(op->reports, RPT_ERROR, "Property can't be both boolean and float");
		return 0;
	}

	/* get an rna string path from the operator's properties */
	if (!(str = RNA_string_get_alloc(op->ptr, name, NULL, 0)))
		return 1;

	if (str[0] == '\0') {
		if (r_prop) *r_prop = NULL;
		MEM_freeN(str);
		return 1;
	}

	if (!r_prop)
		r_prop = &unused_prop;

	/* get rna from path */
	if (!RNA_path_resolve(ctx_ptr, str, r_ptr, r_prop)) {
		MEM_freeN(str);
		if (flags & RC_PROP_ALLOW_MISSING)
			return 1;
		else {
			BKE_reportf(op->reports, RPT_ERROR, "Couldn't resolve path %s", name);
			return 0;
		}
	}

	/* check property type */
	if (flags & (RC_PROP_REQUIRE_BOOL | RC_PROP_REQUIRE_FLOAT)) {
		PropertyType prop_type = RNA_property_type(*r_prop);

		if (((flags & RC_PROP_REQUIRE_BOOL) && (prop_type != PROP_BOOLEAN)) ||
		    ((flags & RC_PROP_REQUIRE_FLOAT) && prop_type != PROP_FLOAT)) {
			MEM_freeN(str);
			BKE_reportf(op->reports, RPT_ERROR,
			            "Property from path %s is not a float", name);
			return 0;
		}
	}
	
	/* check property's array length */
	if (*r_prop && (len = RNA_property_array_length(r_ptr, *r_prop)) != req_length) {
		MEM_freeN(str);
		BKE_reportf(op->reports, RPT_ERROR,
		            "Property from path %s has length %d instead of %d",
		            name, len, req_length);
		return 0;
	}

	/* success */
	MEM_freeN(str);
	return 1;
}

/* initialize the rna pointers and properties using rna paths */
static int radial_control_get_properties(bContext *C, wmOperator *op)
{
	RadialControl *rc = op->customdata;
	PointerRNA ctx_ptr, use_secondary_ptr;
	PropertyRNA *use_secondary_prop;
	const char *data_path;

	RNA_pointer_create(NULL, &RNA_Context, C, &ctx_ptr);

	/* check if we use primary or secondary path */
	if (!radial_control_get_path(&ctx_ptr, op, "use_secondary",
	                             &use_secondary_ptr, &use_secondary_prop,
	                             0, (RC_PROP_ALLOW_MISSING |
	                                 RC_PROP_REQUIRE_BOOL)))
	{
		return 0;
	}
	else {
		if (use_secondary_prop &&
		    RNA_property_boolean_get(&use_secondary_ptr, use_secondary_prop))
		{
			data_path = "data_path_secondary";
		}
		else {
			data_path = "data_path_primary";
		}
	}

	if (!radial_control_get_path(&ctx_ptr, op, data_path, &rc->ptr, &rc->prop, 0, 0))
		return 0;

	/* data path is required */
	if (!rc->prop)
		return 0;
	
	if (!radial_control_get_path(&ctx_ptr, op, "rotation_path", &rc->rot_ptr, &rc->rot_prop, 0, RC_PROP_REQUIRE_FLOAT))
		return 0;
	if (!radial_control_get_path(&ctx_ptr, op, "color_path", &rc->col_ptr, &rc->col_prop, 3, RC_PROP_REQUIRE_FLOAT))
		return 0;
	if (!radial_control_get_path(&ctx_ptr, op, "fill_color_path", &rc->fill_col_ptr, &rc->fill_col_prop, 3, RC_PROP_REQUIRE_FLOAT))
		return 0;
	
	/* slightly ugly; allow this property to not resolve
	 * correctly. needed because 3d texture paint shares the same
	 * keymap as 2d image paint */
	if (!radial_control_get_path(&ctx_ptr, op, "zoom_path",
	                             &rc->zoom_ptr, &rc->zoom_prop, 2,
	                             RC_PROP_REQUIRE_FLOAT | RC_PROP_ALLOW_MISSING))
	{
		return 0;
	}
	
	if (!radial_control_get_path(&ctx_ptr, op, "image_id", &rc->image_id_ptr, NULL, 0, 0))
		return 0;
	else if (rc->image_id_ptr.data) {
		/* extra check, pointer must be to an ID */
		if (!RNA_struct_is_ID(rc->image_id_ptr.type)) {
			BKE_report(op->reports, RPT_ERROR,
			           "Pointer from path image_id is not an ID");
			return 0;
		}
	}

	return 1;
}

static int radial_control_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	wmWindowManager *wm;
	RadialControl *rc;
	int min_value_int, max_value_int, step_int;
	float step_float, precision;

	if (!(op->customdata = rc = MEM_callocN(sizeof(RadialControl), "RadialControl")))
		return OPERATOR_CANCELLED;

	if (!radial_control_get_properties(C, op)) {
		MEM_freeN(rc);
		return OPERATOR_CANCELLED;
	}

	/* get type, initial, min, and max values of the property */
	switch ((rc->type = RNA_property_type(rc->prop))) {
		case PROP_INT:
			rc->initial_value = RNA_property_int_get(&rc->ptr, rc->prop);
			RNA_property_int_ui_range(&rc->ptr, rc->prop, &min_value_int,
			                          &max_value_int, &step_int);
			rc->min_value = min_value_int;
			rc->max_value = max_value_int;
			break;
		case PROP_FLOAT:
			rc->initial_value = RNA_property_float_get(&rc->ptr, rc->prop);
			RNA_property_float_ui_range(&rc->ptr, rc->prop, &rc->min_value,
			                            &rc->max_value, &step_float, &precision);
			break;
		default:
			BKE_report(op->reports, RPT_ERROR, "Property must be an integer or a float");
			MEM_freeN(rc);
			return OPERATOR_CANCELLED;
	}

	/* get subtype of property */
	rc->subtype = RNA_property_subtype(rc->prop);
	if (!ELEM3(rc->subtype, PROP_DISTANCE, PROP_FACTOR, PROP_ANGLE)) {
		BKE_report(op->reports, RPT_ERROR, "Property must be a distance, a factor, or an angle");
		MEM_freeN(rc);
		return OPERATOR_CANCELLED;
	}
		
	rc->current_value = rc->initial_value;
	radial_control_set_initial_mouse(rc, event);
	radial_control_set_tex(rc);

	/* temporarily disable other paint cursors */
	wm = CTX_wm_manager(C);
	rc->orig_paintcursors = wm->paintcursors;
	wm->paintcursors.first = wm->paintcursors.last = NULL;

	/* add radial control paint cursor */
	rc->cursor = WM_paint_cursor_activate(wm, op->type->poll,
	                                      radial_control_paint_cursor, rc);

	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

static void radial_control_set_value(RadialControl *rc, float val)
{
	switch (rc->type) {
		case PROP_INT:
			RNA_property_int_set(&rc->ptr, rc->prop, val);
			break;
		case PROP_FLOAT:
			RNA_property_float_set(&rc->ptr, rc->prop, val);
			break;
		default:
			break;
	}
}

static int radial_control_cancel(bContext *C, wmOperator *op)
{
	RadialControl *rc = op->customdata;
	wmWindowManager *wm = CTX_wm_manager(C);

	WM_paint_cursor_end(wm, rc->cursor);

	/* restore original paint cursors */
	wm->paintcursors = rc->orig_paintcursors;

	/* not sure if this is a good notifier to use;
	 * intended purpose is to update the UI so that the
	 * new value is displayed in sliders/numfields */
	WM_event_add_notifier(C, NC_WINDOW, NULL);

	glDeleteTextures(1, &rc->gltex);

	MEM_freeN(rc);

	return OPERATOR_CANCELLED;
}

static int radial_control_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	RadialControl *rc = op->customdata;
	float new_value, dist, zoom[2];
	float delta[2], snap, ret = OPERATOR_RUNNING_MODAL;

	/* TODO: fix hardcoded events */

	snap = event->ctrl;

	switch (event->type) {
		case MOUSEMOVE:
			delta[0] = rc->initial_mouse[0] - event->x;
			delta[1] = rc->initial_mouse[1] - event->y;

			if (rc->zoom_prop) {
				RNA_property_float_get_array(&rc->zoom_ptr, rc->zoom_prop, zoom);
				delta[0] /= zoom[0];
				delta[1] /= zoom[1];
			}

			dist = sqrt(delta[0] * delta[0] + delta[1] * delta[1]);

			/* calculate new value and apply snapping  */
			switch (rc->subtype) {
				case PROP_DISTANCE:
					new_value = dist;
					if (snap) new_value = ((int)new_value + 5) / 10 * 10;
					break;
				case PROP_FACTOR:
					new_value = 1 - dist / WM_RADIAL_CONTROL_DISPLAY_SIZE;
					if (snap) new_value = ((int)ceil(new_value * 10.f) * 10.0f) / 100.f;
					break;
				case PROP_ANGLE:
					new_value = atan2(delta[1], delta[0]) + M_PI;
					if (snap) new_value = DEG2RADF(((int)RAD2DEGF(new_value) + 5) / 10 * 10);
					break;
				default:
					new_value = dist; /* dummy value, should this ever happen? - campbell */
					break;
			}

			/* clamp and update */
			CLAMP(new_value, rc->min_value, rc->max_value);
			radial_control_set_value(rc, new_value);
			rc->current_value = new_value;
			break;

		case ESCKEY:
		case RIGHTMOUSE:
			/* canceled; restore original value */
			radial_control_set_value(rc, rc->initial_value);
			ret = OPERATOR_CANCELLED;
			break;

		case LEFTMOUSE:
		case PADENTER:
			/* done; value already set */
			ret = OPERATOR_FINISHED;
			break;
	}

	ED_region_tag_redraw(CTX_wm_region(C));

	if (ret != OPERATOR_RUNNING_MODAL)
		radial_control_cancel(C, op);

	return ret;
}

static void WM_OT_radial_control(wmOperatorType *ot)
{
	ot->name = "Radial Control";
	ot->idname = "WM_OT_radial_control";
	ot->description = "Set some size property (like e.g. brush size) with mouse wheel";

	ot->invoke = radial_control_invoke;
	ot->modal = radial_control_modal;
	ot->cancel = radial_control_cancel;

	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

	/* all paths relative to the context */
	RNA_def_string(ot->srna, "data_path_primary", "", 0, "Primary Data Path", "Primary path of property to be set by the radial control");
	RNA_def_string(ot->srna, "data_path_secondary", "", 0, "Secondary Data Path", "Secondary path of property to be set by the radial control");
	RNA_def_string(ot->srna, "use_secondary", "", 0, "Use Secondary", "Path of property to select between the primary and secondary data paths");
	RNA_def_string(ot->srna, "rotation_path", "", 0, "Rotation Path", "Path of property used to rotate the texture display");
	RNA_def_string(ot->srna, "color_path", "", 0, "Color Path", "Path of property used to set the color of the control");
	RNA_def_string(ot->srna, "fill_color_path", "", 0, "Fill Color Path", "Path of property used to set the fill color of the control");
	RNA_def_string(ot->srna, "zoom_path", "", 0, "Zoom Path", "Path of property used to set the zoom level for the control");
	RNA_def_string(ot->srna, "image_id", "", 0, "Image ID", "Path of ID that is used to generate an image for the control");
}

/* ************************** timer for testing ***************** */

/* uses no type defines, fully local testing function anyway... ;) */

static void redraw_timer_window_swap(bContext *C)
{
	wmWindow *win = CTX_wm_window(C);
	ScrArea *sa;

	for (sa = CTX_wm_screen(C)->areabase.first; sa; sa = sa->next)
		ED_area_tag_redraw(sa);
	wm_draw_update(C);

	CTX_wm_window_set(C, win);  /* XXX context manipulation warning! */
}

static EnumPropertyItem redraw_timer_type_items[] = {
	{0, "DRAW", 0, "Draw Region", "Draw Region"},
	{1, "DRAW_SWAP", 0, "Draw Region + Swap", "Draw Region and Swap"},
	{2, "DRAW_WIN", 0, "Draw Window", "Draw Window"},
	{3, "DRAW_WIN_SWAP", 0, "Draw Window + Swap", "Draw Window and Swap"},
	{4, "ANIM_STEP", 0, "Anim Step", "Animation Steps"},
	{5, "ANIM_PLAY", 0, "Anim Play", "Animation Playback"},
	{6, "UNDO", 0, "Undo/Redo", "Undo/Redo"},
	{0, NULL, 0, NULL, NULL}
};

static int redraw_timer_exec(bContext *C, wmOperator *op)
{
	ARegion *ar = CTX_wm_region(C);
	double stime = PIL_check_seconds_timer();
	int type = RNA_enum_get(op->ptr, "type");
	int iter = RNA_int_get(op->ptr, "iterations");
	int a;
	float time;
	const char *infostr = "";
	
	WM_cursor_wait(1);

	for (a = 0; a < iter; a++) {
		if (type == 0) {
			if (ar)
				ED_region_do_draw(C, ar);
		} 
		else if (type == 1) {
			wmWindow *win = CTX_wm_window(C);
			
			ED_region_tag_redraw(ar);
			wm_draw_update(C);
			
			CTX_wm_window_set(C, win);  /* XXX context manipulation warning! */
		}
		else if (type == 2) {
			wmWindow *win = CTX_wm_window(C);
			ScrArea *sa;
			
			ScrArea *sa_back = CTX_wm_area(C);
			ARegion *ar_back = CTX_wm_region(C);

			for (sa = CTX_wm_screen(C)->areabase.first; sa; sa = sa->next) {
				ARegion *ar_iter;
				CTX_wm_area_set(C, sa);

				for (ar_iter = sa->regionbase.first; ar_iter; ar_iter = ar_iter->next) {
					if (ar_iter->swinid) {
						CTX_wm_region_set(C, ar_iter);
						ED_region_do_draw(C, ar_iter);
					}
				}
			}

			CTX_wm_window_set(C, win);  /* XXX context manipulation warning! */

			CTX_wm_area_set(C, sa_back);
			CTX_wm_region_set(C, ar_back);
		}
		else if (type == 3) {
			redraw_timer_window_swap(C);
		}
		else if (type == 4) {
			Main *bmain = CTX_data_main(C);
			Scene *scene = CTX_data_scene(C);
			
			if (a & 1) scene->r.cfra--;
			else scene->r.cfra++;
			BKE_scene_update_for_newframe(bmain, scene, scene->lay);
		}
		else if (type == 5) {

			/* play anim, return on same frame as started with */
			Main *bmain = CTX_data_main(C);
			Scene *scene = CTX_data_scene(C);
			int tot = (scene->r.efra - scene->r.sfra) + 1;

			while (tot--) {
				/* todo, ability to escape! */
				scene->r.cfra++;
				if (scene->r.cfra > scene->r.efra)
					scene->r.cfra = scene->r.sfra;

				BKE_scene_update_for_newframe(bmain, scene, scene->lay);
				redraw_timer_window_swap(C);
			}
		}
		else { /* 6 */
			ED_undo_pop(C);
			ED_undo_redo(C);
		}
	}
	
	time = (float)((PIL_check_seconds_timer() - stime) * 1000);

	RNA_enum_description(redraw_timer_type_items, type, &infostr);

	WM_cursor_wait(0);

	BKE_reportf(op->reports, RPT_WARNING, "%d x %s: %.2f ms,  average: %.4f", iter, infostr, time, time / iter);
	
	return OPERATOR_FINISHED;
}

static void WM_OT_redraw_timer(wmOperatorType *ot)
{
	ot->name = "Redraw Timer";
	ot->idname = "WM_OT_redraw_timer";
	ot->description = "Simple redraw timer to test the speed of updating the interface";

	ot->invoke = WM_menu_invoke;
	ot->exec = redraw_timer_exec;
	ot->poll = WM_operator_winactive;

	ot->prop = RNA_def_enum(ot->srna, "type", redraw_timer_type_items, 0, "Type", "");
	RNA_def_int(ot->srna, "iterations", 10, 1, INT_MAX, "Iterations", "Number of times to redraw", 1, 1000);

}

/* ************************** memory statistics for testing ***************** */

static int memory_statistics_exec(bContext *UNUSED(C), wmOperator *UNUSED(op))
{
	MEM_printmemlist_stats();
	return OPERATOR_FINISHED;
}

static void WM_OT_memory_statistics(wmOperatorType *ot)
{
	ot->name = "Memory Statistics";
	ot->idname = "WM_OT_memory_statistics";
	ot->description = "Print memory statistics to the console";
	
	ot->exec = memory_statistics_exec;
}

/* ************************** memory statistics for testing ***************** */

static int dependency_relations_exec(bContext *C, wmOperator *UNUSED(op))
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);

	DAG_print_dependencies(bmain, scene, ob);

	return OPERATOR_FINISHED;
}

static void WM_OT_dependency_relations(wmOperatorType *ot)
{
	ot->name = "Dependency Relations";
	ot->idname = "WM_OT_dependency_relations";
	ot->description = "Print dependency graph relations to the console";
	
	ot->exec = dependency_relations_exec;
}

/* ******************************************************* */

static int wm_ndof_sensitivity_exec(bContext *UNUSED(C), wmOperator *op)
{
	const float min = 0.25f, max = 4.f; // TODO: get these from RNA property
	float change;
	float sensitivity = U.ndof_sensitivity;

	if (RNA_boolean_get(op->ptr, "fast"))
		change = 0.5f;  // 50% change
	else
		change = 0.1f;  // 10%

	if (RNA_boolean_get(op->ptr, "decrease")) {
		sensitivity -= sensitivity * change; 
		if (sensitivity < min)
			sensitivity = min;
	}
	else {
		sensitivity += sensitivity * change; 
		if (sensitivity > max)
			sensitivity = max;
	}

	if (sensitivity != U.ndof_sensitivity) {
		U.ndof_sensitivity = sensitivity;
	}

	return OPERATOR_FINISHED;
}

static void WM_OT_ndof_sensitivity_change(wmOperatorType *ot)
{
	ot->name = "Change NDOF sensitivity";
	ot->idname = "WM_OT_ndof_sensitivity_change";
	ot->description = "Change NDOF sensitivity";
	
	ot->exec = wm_ndof_sensitivity_exec;

	RNA_def_boolean(ot->srna, "decrease", 1, "Decrease NDOF sensitivity", "If true then action decreases NDOF sensitivity instead of increasing");
	RNA_def_boolean(ot->srna, "fast", 0, "Fast NDOF sensitivity change", "If true then sensitivity changes 50%, otherwise 10%");
} 


static void operatortype_ghash_free_cb(wmOperatorType *ot)
{
	if (ot->last_properties) {
		IDP_FreeProperty(ot->last_properties);
		MEM_freeN(ot->last_properties);
	}

	if (ot->macro.first)
		wm_operatortype_free_macro(ot);

	if (ot->ext.srna) /* python operator, allocs own string */
		MEM_freeN((void *)ot->idname);

	MEM_freeN(ot);
}

/* ******************************************************* */
/* called on initialize WM_exit() */
void wm_operatortype_free(void)
{
	BLI_ghash_free(global_ops_hash, NULL, (GHashValFreeFP)operatortype_ghash_free_cb);
	global_ops_hash = NULL;
}

/* called on initialize WM_init() */
void wm_operatortype_init(void)
{
	global_ops_hash = BLI_ghash_str_new("wm_operatortype_init gh");

	WM_operatortype_append(WM_OT_window_duplicate);
	WM_operatortype_append(WM_OT_read_homefile);
	WM_operatortype_append(WM_OT_read_factory_settings);
	WM_operatortype_append(WM_OT_save_homefile);
	WM_operatortype_append(WM_OT_window_fullscreen_toggle);
	WM_operatortype_append(WM_OT_quit_blender);
	WM_operatortype_append(WM_OT_open_mainfile);
	WM_operatortype_append(WM_OT_link_append);
	WM_operatortype_append(WM_OT_recover_last_session);
	WM_operatortype_append(WM_OT_recover_auto_save);
	WM_operatortype_append(WM_OT_save_as_mainfile);
	WM_operatortype_append(WM_OT_save_mainfile);
	WM_operatortype_append(WM_OT_redraw_timer);
	WM_operatortype_append(WM_OT_memory_statistics);
	WM_operatortype_append(WM_OT_dependency_relations);
	WM_operatortype_append(WM_OT_debug_menu);
	WM_operatortype_append(WM_OT_splash);
	WM_operatortype_append(WM_OT_search_menu);
	WM_operatortype_append(WM_OT_call_menu);
	WM_operatortype_append(WM_OT_radial_control);
	WM_operatortype_append(WM_OT_ndof_sensitivity_change);
#if defined(WIN32)
	WM_operatortype_append(WM_OT_console_toggle);
#endif

#ifdef WITH_COLLADA
	/* XXX: move these */
	WM_operatortype_append(WM_OT_collada_export);
	WM_operatortype_append(WM_OT_collada_import);
#endif
}

/* circleselect-like modal operators */
static void gesture_circle_modal_keymap(wmKeyConfig *keyconf)
{
	static EnumPropertyItem modal_items[] = {
		{GESTURE_MODAL_CANCEL,  "CANCEL", 0, "Cancel", ""},
		{GESTURE_MODAL_CONFIRM, "CONFIRM", 0, "Confirm", ""},
		{GESTURE_MODAL_CIRCLE_ADD, "ADD", 0, "Add", ""},
		{GESTURE_MODAL_CIRCLE_SUB, "SUBTRACT", 0, "Subtract", ""},

		{GESTURE_MODAL_SELECT,  "SELECT", 0, "Select", ""},
		{GESTURE_MODAL_DESELECT, "DESELECT", 0, "DeSelect", ""},
		{GESTURE_MODAL_NOP, "NOP", 0, "No Operation", ""},

		{0, NULL, 0, NULL, NULL}};

	wmKeyMap *keymap = WM_modalkeymap_get(keyconf, "View3D Gesture Circle");

	/* this function is called for each spacetype, only needs to add map once */
	if (keymap && keymap->modal_items) return;

	keymap = WM_modalkeymap_add(keyconf, "View3D Gesture Circle", modal_items);

	/* items for modal map */
	WM_modalkeymap_add_item(keymap, ESCKEY,    KM_PRESS, KM_ANY, 0, GESTURE_MODAL_CANCEL);
	WM_modalkeymap_add_item(keymap, RIGHTMOUSE, KM_ANY, KM_ANY, 0, GESTURE_MODAL_CANCEL);

	WM_modalkeymap_add_item(keymap, RETKEY, KM_PRESS, KM_ANY, 0, GESTURE_MODAL_CONFIRM);
	WM_modalkeymap_add_item(keymap, PADENTER, KM_PRESS, 0, 0, GESTURE_MODAL_CONFIRM);

	WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_PRESS, 0, 0, GESTURE_MODAL_SELECT);

#if 0 // Durien guys like this :S
	WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_PRESS, KM_SHIFT, 0, GESTURE_MODAL_DESELECT);
	WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_RELEASE, KM_SHIFT, 0, GESTURE_MODAL_NOP);
#else
	WM_modalkeymap_add_item(keymap, MIDDLEMOUSE, KM_PRESS, 0, 0, GESTURE_MODAL_DESELECT); //  default 2.4x
	WM_modalkeymap_add_item(keymap, MIDDLEMOUSE, KM_RELEASE, 0, 0, GESTURE_MODAL_NOP); //  default 2.4x
#endif

	WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_RELEASE, 0, 0, GESTURE_MODAL_NOP);

	WM_modalkeymap_add_item(keymap, WHEELUPMOUSE, KM_PRESS, 0, 0, GESTURE_MODAL_CIRCLE_SUB);
	WM_modalkeymap_add_item(keymap, PADMINUS, KM_PRESS, 0, 0, GESTURE_MODAL_CIRCLE_SUB);
	WM_modalkeymap_add_item(keymap, WHEELDOWNMOUSE, KM_PRESS, 0, 0, GESTURE_MODAL_CIRCLE_ADD);
	WM_modalkeymap_add_item(keymap, PADPLUSKEY, KM_PRESS, 0, 0, GESTURE_MODAL_CIRCLE_ADD);

	/* assign map to operators */
	WM_modalkeymap_assign(keymap, "VIEW3D_OT_select_circle");
	WM_modalkeymap_assign(keymap, "UV_OT_circle_select");
	WM_modalkeymap_assign(keymap, "CLIP_OT_select_circle");

}

/* straight line modal operators */
static void gesture_straightline_modal_keymap(wmKeyConfig *keyconf)
{
	static EnumPropertyItem modal_items[] = {
		{GESTURE_MODAL_CANCEL,  "CANCEL", 0, "Cancel", ""},
		{GESTURE_MODAL_SELECT,  "SELECT", 0, "Select", ""},
		{GESTURE_MODAL_BEGIN,   "BEGIN", 0, "Begin", ""},
		{0, NULL, 0, NULL, NULL}};
	
	wmKeyMap *keymap = WM_modalkeymap_get(keyconf, "Gesture Straight Line");
	
	/* this function is called for each spacetype, only needs to add map once */
	if (keymap && keymap->modal_items) return;
	
	keymap = WM_modalkeymap_add(keyconf, "Gesture Straight Line", modal_items);
	
	/* items for modal map */
	WM_modalkeymap_add_item(keymap, ESCKEY,    KM_PRESS, KM_ANY, 0, GESTURE_MODAL_CANCEL);
	WM_modalkeymap_add_item(keymap, RIGHTMOUSE, KM_ANY, KM_ANY, 0, GESTURE_MODAL_CANCEL);
	
	WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_PRESS, 0, 0, GESTURE_MODAL_BEGIN);
	WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_RELEASE, 0, 0, GESTURE_MODAL_SELECT);
	
	/* assign map to operators */
	WM_modalkeymap_assign(keymap, "IMAGE_OT_sample_line");
}


/* borderselect-like modal operators */
static void gesture_border_modal_keymap(wmKeyConfig *keyconf)
{
	static EnumPropertyItem modal_items[] = {
		{GESTURE_MODAL_CANCEL,  "CANCEL", 0, "Cancel", ""},
		{GESTURE_MODAL_SELECT,  "SELECT", 0, "Select", ""},
		{GESTURE_MODAL_DESELECT, "DESELECT", 0, "DeSelect", ""},
		{GESTURE_MODAL_BEGIN,   "BEGIN", 0, "Begin", ""},
		{0, NULL, 0, NULL, NULL}};

	wmKeyMap *keymap = WM_modalkeymap_get(keyconf, "Gesture Border");

	/* this function is called for each spacetype, only needs to add map once */
	if (keymap && keymap->modal_items) return;

	keymap = WM_modalkeymap_add(keyconf, "Gesture Border", modal_items);

	/* items for modal map */
	WM_modalkeymap_add_item(keymap, ESCKEY,    KM_PRESS, KM_ANY, 0, GESTURE_MODAL_CANCEL);
	/* Note: cancel only on press otherwise you cannot map this to RMB-gesture */
	WM_modalkeymap_add_item(keymap, RIGHTMOUSE, KM_PRESS, KM_ANY, 0, GESTURE_MODAL_CANCEL);

	WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_PRESS, 0, 0, GESTURE_MODAL_BEGIN);
	WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_RELEASE, KM_ANY, 0, GESTURE_MODAL_SELECT);
	WM_modalkeymap_add_item(keymap, RIGHTMOUSE, KM_RELEASE, KM_ANY, 0, GESTURE_MODAL_SELECT);

#if 0 // Durian guys like this
	WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_PRESS, KM_SHIFT, 0, GESTURE_MODAL_BEGIN);
	WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_RELEASE, KM_SHIFT, 0, GESTURE_MODAL_DESELECT);
#else
	WM_modalkeymap_add_item(keymap, MIDDLEMOUSE, KM_PRESS, 0, 0, GESTURE_MODAL_BEGIN);
	WM_modalkeymap_add_item(keymap, MIDDLEMOUSE, KM_RELEASE, 0, 0, GESTURE_MODAL_DESELECT);
#endif

	/* assign map to operators */
	WM_modalkeymap_assign(keymap, "ACTION_OT_select_border");
	WM_modalkeymap_assign(keymap, "ANIM_OT_channels_select_border");
	WM_modalkeymap_assign(keymap, "ANIM_OT_previewrange_set");
	WM_modalkeymap_assign(keymap, "INFO_OT_select_border");
	WM_modalkeymap_assign(keymap, "FILE_OT_select_border");
	WM_modalkeymap_assign(keymap, "GRAPH_OT_select_border");
	WM_modalkeymap_assign(keymap, "MARKER_OT_select_border");
	WM_modalkeymap_assign(keymap, "NLA_OT_select_border");
	WM_modalkeymap_assign(keymap, "NODE_OT_select_border");
	WM_modalkeymap_assign(keymap, "PAINT_OT_hide_show");
	WM_modalkeymap_assign(keymap, "OUTLINER_OT_select_border");
//	WM_modalkeymap_assign(keymap, "SCREEN_OT_border_select"); // template
	WM_modalkeymap_assign(keymap, "SEQUENCER_OT_select_border");
	WM_modalkeymap_assign(keymap, "SEQUENCER_OT_view_ghost_border");
	WM_modalkeymap_assign(keymap, "UV_OT_select_border");
	WM_modalkeymap_assign(keymap, "CLIP_OT_select_border");
	WM_modalkeymap_assign(keymap, "CLIP_OT_graph_select_border");
	WM_modalkeymap_assign(keymap, "VIEW2D_OT_zoom_border");
	WM_modalkeymap_assign(keymap, "VIEW3D_OT_clip_border");
	WM_modalkeymap_assign(keymap, "VIEW3D_OT_render_border");
	WM_modalkeymap_assign(keymap, "VIEW3D_OT_select_border");
	WM_modalkeymap_assign(keymap, "VIEW3D_OT_zoom_border"); // XXX TODO: zoom border should perhaps map rightmouse to zoom out instead of in+cancel
}

/* zoom to border modal operators */
static void gesture_zoom_border_modal_keymap(wmKeyConfig *keyconf)
{
	static EnumPropertyItem modal_items[] = {
		{GESTURE_MODAL_CANCEL, "CANCEL", 0, "Cancel", ""},
		{GESTURE_MODAL_IN,  "IN", 0, "In", ""},
		{GESTURE_MODAL_OUT, "OUT", 0, "Out", ""},
		{GESTURE_MODAL_BEGIN, "BEGIN", 0, "Begin", ""},
		{0, NULL, 0, NULL, NULL}};

	wmKeyMap *keymap = WM_modalkeymap_get(keyconf, "Gesture Zoom Border");

	/* this function is called for each spacetype, only needs to add map once */
	if (keymap && keymap->modal_items) return;

	keymap = WM_modalkeymap_add(keyconf, "Gesture Zoom Border", modal_items);

	/* items for modal map */
	WM_modalkeymap_add_item(keymap, ESCKEY,    KM_PRESS, KM_ANY, 0, GESTURE_MODAL_CANCEL);
	WM_modalkeymap_add_item(keymap, RIGHTMOUSE, KM_ANY, KM_ANY, 0, GESTURE_MODAL_CANCEL);

	WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_PRESS, 0, 0, GESTURE_MODAL_BEGIN);
	WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_RELEASE, 0, 0, GESTURE_MODAL_IN); 

	WM_modalkeymap_add_item(keymap, MIDDLEMOUSE, KM_PRESS, 0, 0, GESTURE_MODAL_BEGIN);
	WM_modalkeymap_add_item(keymap, MIDDLEMOUSE, KM_RELEASE, 0, 0, GESTURE_MODAL_OUT);

	/* assign map to operators */
	WM_modalkeymap_assign(keymap, "VIEW2D_OT_zoom_border");
	WM_modalkeymap_assign(keymap, "VIEW3D_OT_zoom_border");
}

/* default keymap for windows and screens, only call once per WM */
void wm_window_keymap(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap = WM_keymap_find(keyconf, "Window", 0, 0);
	wmKeyMapItem *kmi;
	
	/* note, this doesn't replace existing keymap items */
	WM_keymap_verify_item(keymap, "WM_OT_window_duplicate", WKEY, KM_PRESS, KM_CTRL | KM_ALT, 0);
	#ifdef __APPLE__
	WM_keymap_add_item(keymap, "WM_OT_read_homefile", NKEY, KM_PRESS, KM_OSKEY, 0);
	WM_keymap_add_menu(keymap, "INFO_MT_file_open_recent", OKEY, KM_PRESS, KM_SHIFT | KM_OSKEY, 0);
	WM_keymap_add_item(keymap, "WM_OT_open_mainfile", OKEY, KM_PRESS, KM_OSKEY, 0);
	WM_keymap_add_item(keymap, "WM_OT_save_mainfile", SKEY, KM_PRESS, KM_OSKEY, 0);
	WM_keymap_add_item(keymap, "WM_OT_save_as_mainfile", SKEY, KM_PRESS, KM_SHIFT | KM_OSKEY, 0);
	WM_keymap_add_item(keymap, "WM_OT_quit_blender", QKEY, KM_PRESS, KM_OSKEY, 0);
	#endif
	WM_keymap_add_item(keymap, "WM_OT_read_homefile", NKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "WM_OT_save_homefile", UKEY, KM_PRESS, KM_CTRL, 0); 
	WM_keymap_add_menu(keymap, "INFO_MT_file_open_recent", OKEY, KM_PRESS, KM_SHIFT | KM_CTRL, 0);
	WM_keymap_add_item(keymap, "WM_OT_open_mainfile", OKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "WM_OT_open_mainfile", F1KEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "WM_OT_link_append", OKEY, KM_PRESS, KM_CTRL | KM_ALT, 0);
	kmi = WM_keymap_add_item(keymap, "WM_OT_link_append", F1KEY, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "link", FALSE);
	RNA_boolean_set(kmi->ptr, "instance_groups", FALSE);

	WM_keymap_add_item(keymap, "WM_OT_save_mainfile", SKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "WM_OT_save_mainfile", WKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "WM_OT_save_as_mainfile", SKEY, KM_PRESS, KM_SHIFT | KM_CTRL, 0);
	WM_keymap_add_item(keymap, "WM_OT_save_as_mainfile", F2KEY, KM_PRESS, 0, 0);
	kmi = WM_keymap_add_item(keymap, "WM_OT_save_as_mainfile", SKEY, KM_PRESS, KM_ALT | KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "copy", TRUE);

	WM_keymap_verify_item(keymap, "WM_OT_window_fullscreen_toggle", F11KEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "WM_OT_quit_blender", QKEY, KM_PRESS, KM_CTRL, 0);

	/* debug/testing */
	WM_keymap_verify_item(keymap, "WM_OT_redraw_timer", TKEY, KM_PRESS, KM_ALT | KM_CTRL, 0);
	WM_keymap_verify_item(keymap, "WM_OT_debug_menu", DKEY, KM_PRESS, KM_ALT | KM_CTRL, 0);

	/* menus that can be accessed anywhere in blender */
	WM_keymap_verify_item(keymap, "WM_OT_search_menu", SPACEKEY, KM_PRESS, 0, 0);
	WM_keymap_add_menu(keymap, "USERPREF_MT_ndof_settings", NDOF_BUTTON_MENU, KM_PRESS, 0, 0);

	/* Space switching */
	kmi = WM_keymap_add_item(keymap, "WM_OT_context_set_enum", F2KEY, KM_PRESS, KM_SHIFT, 0); /* new in 2.5x, was DXF export */
	RNA_string_set(kmi->ptr, "data_path", "area.type");
	RNA_string_set(kmi->ptr, "value", "LOGIC_EDITOR");

	kmi = WM_keymap_add_item(keymap, "WM_OT_context_set_enum", F3KEY, KM_PRESS, KM_SHIFT, 0);
	RNA_string_set(kmi->ptr, "data_path", "area.type");
	RNA_string_set(kmi->ptr, "value", "NODE_EDITOR");

	kmi = WM_keymap_add_item(keymap, "WM_OT_context_set_enum", F4KEY, KM_PRESS, KM_SHIFT, 0); /* new in 2.5x, was data browser */
	RNA_string_set(kmi->ptr, "data_path", "area.type");
	RNA_string_set(kmi->ptr, "value", "CONSOLE");

	kmi = WM_keymap_add_item(keymap, "WM_OT_context_set_enum", F5KEY, KM_PRESS, KM_SHIFT, 0);
	RNA_string_set(kmi->ptr, "data_path", "area.type");
	RNA_string_set(kmi->ptr, "value", "VIEW_3D");

	kmi = WM_keymap_add_item(keymap, "WM_OT_context_set_enum", F6KEY, KM_PRESS, KM_SHIFT, 0);
	RNA_string_set(kmi->ptr, "data_path", "area.type");
	RNA_string_set(kmi->ptr, "value", "GRAPH_EDITOR");

	kmi = WM_keymap_add_item(keymap, "WM_OT_context_set_enum", F7KEY, KM_PRESS, KM_SHIFT, 0);
	RNA_string_set(kmi->ptr, "data_path", "area.type");
	RNA_string_set(kmi->ptr, "value", "PROPERTIES");

	kmi = WM_keymap_add_item(keymap, "WM_OT_context_set_enum", F8KEY, KM_PRESS, KM_SHIFT, 0);
	RNA_string_set(kmi->ptr, "data_path", "area.type");
	RNA_string_set(kmi->ptr, "value", "SEQUENCE_EDITOR");

	kmi = WM_keymap_add_item(keymap, "WM_OT_context_set_enum", F9KEY, KM_PRESS, KM_SHIFT, 0);
	RNA_string_set(kmi->ptr, "data_path", "area.type");
	RNA_string_set(kmi->ptr, "value", "OUTLINER");

	kmi = WM_keymap_add_item(keymap, "WM_OT_context_set_enum", F10KEY, KM_PRESS, KM_SHIFT, 0);
	RNA_string_set(kmi->ptr, "data_path", "area.type");
	RNA_string_set(kmi->ptr, "value", "IMAGE_EDITOR");

	kmi = WM_keymap_add_item(keymap, "WM_OT_context_set_enum", F11KEY, KM_PRESS, KM_SHIFT, 0);
	RNA_string_set(kmi->ptr, "data_path", "area.type");
	RNA_string_set(kmi->ptr, "value", "TEXT_EDITOR");

	kmi = WM_keymap_add_item(keymap, "WM_OT_context_set_enum", F12KEY, KM_PRESS, KM_SHIFT, 0);
	RNA_string_set(kmi->ptr, "data_path", "area.type");
	RNA_string_set(kmi->ptr, "value", "DOPESHEET_EDITOR");
	
	/* ndof speed */
	kmi = WM_keymap_add_item(keymap, "WM_OT_ndof_sensitivity_change", NDOF_BUTTON_PLUS, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "decrease", FALSE);
	RNA_boolean_set(kmi->ptr, "fast", FALSE);

	kmi = WM_keymap_add_item(keymap, "WM_OT_ndof_sensitivity_change", NDOF_BUTTON_MINUS, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "decrease", TRUE);
	RNA_boolean_set(kmi->ptr, "fast", FALSE);

	kmi = WM_keymap_add_item(keymap, "WM_OT_ndof_sensitivity_change", NDOF_BUTTON_PLUS, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "decrease", FALSE);
	RNA_boolean_set(kmi->ptr, "fast", TRUE);

	kmi = WM_keymap_add_item(keymap, "WM_OT_ndof_sensitivity_change", NDOF_BUTTON_MINUS, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "decrease", TRUE);
	RNA_boolean_set(kmi->ptr, "fast", TRUE);

	gesture_circle_modal_keymap(keyconf);
	gesture_border_modal_keymap(keyconf);
	gesture_zoom_border_modal_keymap(keyconf);
	gesture_straightline_modal_keymap(keyconf);
}

/* Generic itemf's for operators that take library args */
static EnumPropertyItem *rna_id_itemf(bContext *UNUSED(C), PointerRNA *UNUSED(ptr), int *do_free, ID *id, int local)
{
	EnumPropertyItem item_tmp = {0}, *item = NULL;
	int totitem = 0;
	int i = 0;

	for (; id; id = id->next) {
		if (local == FALSE || id->lib == NULL) {
			item_tmp.identifier = item_tmp.name = id->name + 2;
			item_tmp.value = i++;
			RNA_enum_item_add(&item, &totitem, &item_tmp);
		}
	}

	RNA_enum_item_end(&item, &totitem);
	*do_free = TRUE;

	return item;
}

/* can add more as needed */
EnumPropertyItem *RNA_action_itemf(bContext *C, PointerRNA *ptr, PropertyRNA *UNUSED(prop), int *do_free)
{
	return rna_id_itemf(C, ptr, do_free, C ? (ID *)CTX_data_main(C)->action.first : NULL, FALSE);
}
#if 0 /* UNUSED */
EnumPropertyItem *RNA_action_local_itemf(bContext *C, PointerRNA *ptr, PropertyRNA *UNUSED(prop), int *do_free)
{
	return rna_id_itemf(C, ptr, do_free, C ? (ID *)CTX_data_main(C)->action.first : NULL, TRUE);
}
#endif

EnumPropertyItem *RNA_group_itemf(bContext *C, PointerRNA *ptr, PropertyRNA *UNUSED(prop), int *do_free)
{
	return rna_id_itemf(C, ptr, do_free, C ? (ID *)CTX_data_main(C)->group.first : NULL, FALSE);
}
EnumPropertyItem *RNA_group_local_itemf(bContext *C, PointerRNA *ptr, PropertyRNA *UNUSED(prop), int *do_free)
{
	return rna_id_itemf(C, ptr, do_free, C ? (ID *)CTX_data_main(C)->group.first : NULL, TRUE);
}

EnumPropertyItem *RNA_image_itemf(bContext *C, PointerRNA *ptr, PropertyRNA *UNUSED(prop), int *do_free)
{
	return rna_id_itemf(C, ptr, do_free, C ? (ID *)CTX_data_main(C)->image.first : NULL, FALSE);
}
EnumPropertyItem *RNA_image_local_itemf(bContext *C, PointerRNA *ptr, PropertyRNA *UNUSED(prop), int *do_free)
{
	return rna_id_itemf(C, ptr, do_free, C ? (ID *)CTX_data_main(C)->image.first : NULL, TRUE);
}

EnumPropertyItem *RNA_scene_itemf(bContext *C, PointerRNA *ptr, PropertyRNA *UNUSED(prop), int *do_free)
{
	return rna_id_itemf(C, ptr, do_free, C ? (ID *)CTX_data_main(C)->scene.first : NULL, FALSE);
}
EnumPropertyItem *RNA_scene_local_itemf(bContext *C, PointerRNA *ptr, PropertyRNA *UNUSED(prop), int *do_free)
{
	return rna_id_itemf(C, ptr, do_free, C ? (ID *)CTX_data_main(C)->scene.first : NULL, TRUE);
}

EnumPropertyItem *RNA_movieclip_itemf(bContext *C, PointerRNA *ptr, PropertyRNA *UNUSED(prop), int *do_free)
{
	return rna_id_itemf(C, ptr, do_free, C ? (ID *)CTX_data_main(C)->movieclip.first : NULL, FALSE);
}
EnumPropertyItem *RNA_movieclip_local_itemf(bContext *C, PointerRNA *ptr, PropertyRNA *UNUSED(prop), int *do_free)
{
	return rna_id_itemf(C, ptr, do_free, C ? (ID *)CTX_data_main(C)->movieclip.first : NULL, TRUE);
}
