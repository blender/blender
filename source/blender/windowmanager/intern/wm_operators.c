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
 *
 * Functions for dealing with wmOperator, adding, removing, calling
 * as well as some generic operators and shared operator properties.
 */


#include <float.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stddef.h>
#include <assert.h>
#include <errno.h>

#ifdef WIN32
#  include "GHOST_C-api.h"
#endif

#include "MEM_guardedalloc.h"

#include "CLG_log.h"

#include "DNA_ID.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_workspace_types.h"

#include "BLT_translation.h"

#include "PIL_time.h"

#include "BLI_blenlib.h"
#include "BLI_dial_2d.h"
#include "BLI_dynstr.h" /*for WM_operator_pystring */
#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_ghash.h"

#include "BLO_readfile.h"

#include "BKE_appdir.h"
#include "BKE_blender_version.h"
#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_icons.h"
#include "BKE_idprop.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_library_query.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h" /* BKE_ST_MAXNAME */
#include "BKE_unit.h"

#include "BKE_idcode.h"

#include "BLF_api.h"

#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "ED_numinput.h"
#include "ED_screen.h"
#include "ED_undo.h"
#include "ED_view3d.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

#include "wm.h"
#include "wm_draw.h"
#include "wm_event_system.h"
#include "wm_event_types.h"
#include "wm_files.h"
#include "wm_window.h"

#define UNDOCUMENTED_OPERATOR_TIP N_("(undocumented operator)")

/* ************ operator API, exported ********** */

/* SOME_OT_op -> some.op */
void WM_operator_py_idname(char *to, const char *from)
{
	const char *sep = strstr(from, "_OT_");
	if (sep) {
		int ofs = (sep - from);

		/* note, we use ascii tolower instead of system tolower, because the
		 * latter depends on the locale, and can lead to idname mismatch */
		memcpy(to, from, sizeof(char) * ofs);
		BLI_str_tolower_ascii(to, ofs);

		to[ofs] = '.';
		BLI_strncpy(to + (ofs + 1), sep + 4, OP_MAX_TYPENAME - (ofs + 1));
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
		const char *sep = strchr(from, '.');

		if (sep) {
			int ofs = (sep - from);

			memcpy(to, from, sizeof(char) * ofs);
			BLI_str_toupper_ascii(to, ofs);
			strcpy(to + ofs, "_OT_");
			strcpy(to + (ofs + 4), sep + 1);
		}
		else {
			/* should not happen but support just in case */
			BLI_strncpy(to, from, OP_MAX_TYPENAME);
		}
	}
	else
		to[0] = 0;
}

/**
 * Sanity check to ensure #WM_operator_bl_idname won't fail.
 * \returns true when there are no problems with \a idname, otherwise report an error.
 */
bool WM_operator_py_idname_ok_or_report(ReportList *reports, const char *classname, const char *idname)
{
	const char *ch = idname;
	int dot = 0;
	int i;
	for (i = 0; *ch; i++, ch++) {
		if ((*ch >= 'a' && *ch <= 'z') || (*ch >= '0' && *ch <= '9') || *ch == '_') {
			/* pass */
		}
		else if (*ch == '.') {
			dot++;
		}
		else {
			BKE_reportf(reports, RPT_ERROR,
			            "Registering operator class: '%s', invalid bl_idname '%s', at position %d",
			            classname, idname, i);
			return false;
		}
	}

	if (i > (MAX_NAME - 3)) {
		BKE_reportf(reports, RPT_ERROR, "Registering operator class: '%s', invalid bl_idname '%s', "
		            "is too long, maximum length is %d", classname, idname,
		            MAX_NAME - 3);
		return false;
	}

	if (dot != 1) {
		BKE_reportf(reports, RPT_ERROR,
		            "Registering operator class: '%s', invalid bl_idname '%s', must contain 1 '.' character",
		            classname, idname);
		return false;
	}
	return true;
}

/**
 * Print a string representation of the operator, with the args that it runs so python can run it again.
 *
 * When calling from an existing wmOperator, better to use simple version:
 * `WM_operator_pystring(C, op);`
 *
 * \note Both \a op and \a opptr may be `NULL` (\a op is only used for macro operators).
 */
char *WM_operator_pystring_ex(bContext *C, wmOperator *op, const bool all_args, const bool macro_args,
                              wmOperatorType *ot, PointerRNA *opptr)
{
	char idname_py[OP_MAX_TYPENAME];

	/* for building the string */
	DynStr *dynstr = BLI_dynstr_new();
	char *cstring;
	char *cstring_args;

	/* arbitrary, but can get huge string with stroke painting otherwise */
	int max_prop_length = 10;

	WM_operator_py_idname(idname_py, ot->idname);
	BLI_dynstr_appendf(dynstr, "bpy.ops.%s(", idname_py);

	if (op && op->macro.first) {
		/* Special handling for macros, else we only get default values in this case... */
		wmOperator *opm;
		bool first_op = true;

		opm = macro_args ? op->macro.first : NULL;

		for (; opm; opm = opm->next) {
			PointerRNA *opmptr = opm->ptr;
			PointerRNA opmptr_default;
			if (opmptr == NULL) {
				WM_operator_properties_create_ptr(&opmptr_default, opm->type);
				opmptr = &opmptr_default;
			}

			cstring_args = RNA_pointer_as_string_id(C, opmptr);
			if (first_op) {
				BLI_dynstr_appendf(dynstr, "%s=%s", opm->type->idname, cstring_args);
				first_op = false;
			}
			else {
				BLI_dynstr_appendf(dynstr, ", %s=%s", opm->type->idname, cstring_args);
			}
			MEM_freeN(cstring_args);

			if (opmptr == &opmptr_default) {
				WM_operator_properties_free(&opmptr_default);
			}
		}
	}
	else {
		/* only to get the orginal props for comparisons */
		PointerRNA opptr_default;
		const bool macro_args_test = ot->macro.first ? macro_args : true;

		if (opptr == NULL) {
			WM_operator_properties_create_ptr(&opptr_default, ot);
			opptr = &opptr_default;
		}

		cstring_args = RNA_pointer_as_string_keywords(C, opptr, false, all_args, macro_args_test, max_prop_length);
		BLI_dynstr_append(dynstr, cstring_args);
		MEM_freeN(cstring_args);

		if (opptr == &opptr_default) {
			WM_operator_properties_free(&opptr_default);
		}
	}

	BLI_dynstr_append(dynstr, ")");

	cstring = BLI_dynstr_get_cstring(dynstr);
	BLI_dynstr_free(dynstr);
	return cstring;
}

char *WM_operator_pystring(bContext *C, wmOperator *op,
                           const bool all_args, const bool macro_args)
{
	return WM_operator_pystring_ex(C, op, all_args, macro_args, op->type, op->ptr);
}


/**
 * \return true if the string was shortened
 */
bool WM_operator_pystring_abbreviate(char *str, int str_len_max)
{
	const int str_len = strlen(str);
	const char *parens_start = strchr(str, '(');

	if (parens_start) {
		const int parens_start_pos = parens_start - str;
		const char *parens_end = strrchr(parens_start + 1, ')');

		if (parens_end) {
			const int parens_len = parens_end - parens_start;

			if (parens_len > str_len_max) {
				const char *comma_first = strchr(parens_start, ',');

				/* truncate after the first comma */
				if (comma_first) {
					const char end_str[] = " ... )";
					const int end_str_len = sizeof(end_str) - 1;

					/* leave a place for the first argument*/
					const int new_str_len = (comma_first - parens_start) + 1;

					if (str_len >= new_str_len + parens_start_pos + end_str_len + 1) {
						/* append " ... )" to the string after the comma */
						memcpy(str + new_str_len + parens_start_pos, end_str, end_str_len + 1);

						return true;
					}
				}
			}
		}
	}

	return false;
}

/* return NULL if no match is found */
#if 0
static const char *wm_context_member_from_ptr(bContext *C, const PointerRNA *ptr)
{
	/* loop over all context items and do 2 checks
	 *
	 * - see if the pointer is in the context.
	 * - see if the pointers ID is in the context.
	 */

	/* don't get from the context store since this is normally set only for the UI and not usable elsewhere */
	ListBase lb = CTX_data_dir_get_ex(C, false, true, true);
	LinkData *link;

	const char *member_found = NULL;
	const char *member_id = NULL;

	for (link = lb.first; link; link = link->next) {
		const char *identifier = link->data;
		PointerRNA ctx_item_ptr = {{0}}; // CTX_data_pointer_get(C, identifier); // XXX, this isnt working

		if (ctx_item_ptr.type == NULL) {
			continue;
		}

		if (ptr->id.data == ctx_item_ptr.id.data) {
			if ((ptr->data == ctx_item_ptr.data) &&
			    (ptr->type == ctx_item_ptr.type))
			{
				/* found! */
				member_found = identifier;
				break;
			}
			else if (RNA_struct_is_ID(ctx_item_ptr.type)) {
				/* we found a reference to this ID,
				 * so fallback to it if there is no direct reference */
				member_id = identifier;
			}
		}
	}
	BLI_freelistN(&lb);

	if (member_found) {
		return member_found;
	}
	else if (member_id) {
		return member_id;
	}
	else {
		return NULL;
	}
}

#else

/* use hard coded checks for now */

static const char *wm_context_member_from_ptr(bContext *C, const PointerRNA *ptr)
{
	const char *member_id = NULL;

	if (ptr->id.data) {

#define CTX_TEST_PTR_ID(C, member, idptr) \
		{ \
			const char *ctx_member = member; \
			PointerRNA ctx_item_ptr = CTX_data_pointer_get(C, ctx_member); \
			if (ctx_item_ptr.id.data == idptr) { \
				member_id = ctx_member; \
				break; \
			} \
		} (void)0

#define CTX_TEST_PTR_ID_CAST(C, member, member_full, cast, idptr) \
		{ \
			const char *ctx_member = member; \
			const char *ctx_member_full = member_full; \
			PointerRNA ctx_item_ptr = CTX_data_pointer_get(C, ctx_member); \
			if (ctx_item_ptr.id.data && cast(ctx_item_ptr.id.data) == idptr) { \
				member_id = ctx_member_full; \
				break; \
			} \
		} (void)0

#define CTX_TEST_PTR_DATA_TYPE(C, member, rna_type, rna_ptr, dataptr_cmp) \
		{ \
			const char *ctx_member = member; \
			if (RNA_struct_is_a((ptr)->type, &(rna_type)) && (ptr)->data == (dataptr_cmp)) { \
				member_id = ctx_member; \
				break; \
			} \
		} (void)0

#define CTX_TEST_SPACE_TYPE(space_data_type, member_full, dataptr_cmp) \
		{ \
			const char *ctx_member_full = member_full; \
			if (space_data->spacetype == space_data_type && ptr->data == dataptr_cmp) { \
				member_id = ctx_member_full; \
				break; \
			} \
		} (void)0

		switch (GS(((ID *)ptr->id.data)->name)) {
			case ID_SCE:
			{
				CTX_TEST_PTR_DATA_TYPE(C, "active_gpencil_brush", RNA_Brush, ptr, CTX_data_active_gpencil_brush(C));
				CTX_TEST_PTR_ID(C, "scene", ptr->id.data);
				break;
			}
			case ID_OB:
			{
				CTX_TEST_PTR_ID(C, "object", ptr->id.data);
				break;
			}
			/* from rna_Main_objects_new */
			case OB_DATA_SUPPORT_ID_CASE:
			{
#define ID_CAST_OBDATA(id_pt) (((Object *)(id_pt))->data)
				CTX_TEST_PTR_ID_CAST(C, "object", "object.data", ID_CAST_OBDATA, ptr->id.data);
				break;
#undef ID_CAST_OBDATA
			}
			case ID_MA:
			{
#define ID_CAST_OBMATACT(id_pt) (give_current_material(((Object *)id_pt), ((Object *)id_pt)->actcol))
				CTX_TEST_PTR_ID_CAST(C, "object", "object.active_material", ID_CAST_OBMATACT, ptr->id.data);
				break;
#undef ID_CAST_OBMATACT
			}
			case ID_WO:
			{
#define ID_CAST_SCENEWORLD(id_pt) (((Scene *)(id_pt))->world)
				CTX_TEST_PTR_ID_CAST(C, "scene", "scene.world", ID_CAST_SCENEWORLD, ptr->id.data);
				break;
#undef ID_CAST_SCENEWORLD
			}
			case ID_SCR:
			{
				CTX_TEST_PTR_ID(C, "screen", ptr->id.data);

				SpaceLink *space_data = CTX_wm_space_data(C);

				CTX_TEST_PTR_DATA_TYPE(C, "space_data", RNA_Space, ptr, space_data);
				CTX_TEST_PTR_DATA_TYPE(C, "space_data", RNA_View3DOverlay, ptr, space_data);
				CTX_TEST_PTR_DATA_TYPE(C, "space_data", RNA_View3DShading, ptr, space_data);
				CTX_TEST_PTR_DATA_TYPE(C, "area", RNA_Area, ptr, CTX_wm_area(C));
				CTX_TEST_PTR_DATA_TYPE(C, "region", RNA_Region, ptr, CTX_wm_region(C));

				CTX_TEST_SPACE_TYPE(SPACE_IMAGE, "space_data.uv_editor", space_data);
				CTX_TEST_SPACE_TYPE(SPACE_VIEW3D, "space_data.fx_settings", &(CTX_wm_view3d(C)->fx_settings));
				CTX_TEST_SPACE_TYPE(SPACE_NLA, "space_data.dopesheet", CTX_wm_space_nla(C)->ads);
				CTX_TEST_SPACE_TYPE(SPACE_IPO, "space_data.dopesheet", CTX_wm_space_graph(C)->ads);
				CTX_TEST_SPACE_TYPE(SPACE_ACTION, "space_data.dopesheet", &(CTX_wm_space_action(C)->ads));
				CTX_TEST_SPACE_TYPE(SPACE_FILE, "space_data.params", CTX_wm_space_file(C)->params);
				break;
			}
			default:
				break;
		}
#undef CTX_TEST_PTR_ID
#undef CTX_TEST_PTR_ID_CAST
#undef CTX_TEST_SPACE_TYPE
	}

	return member_id;
}
#endif

static char *wm_prop_pystring_from_context(bContext *C, PointerRNA *ptr, PropertyRNA *prop, int index)
{
	const char *member_id = wm_context_member_from_ptr(C, ptr);
	char *ret = NULL;
	if (member_id != NULL) {
		char *prop_str = RNA_path_struct_property_py(ptr, prop, index);
		if (prop_str) {
			ret = BLI_sprintfN("bpy.context.%s.%s", member_id, prop_str);
			MEM_freeN(prop_str);
		}
	}
	return ret;
}

const char *WM_context_member_from_ptr(bContext *C, const PointerRNA *ptr)
{
	return wm_context_member_from_ptr(C, ptr);
}

char *WM_prop_pystring_assign(bContext *C, PointerRNA *ptr, PropertyRNA *prop, int index)
{
	char *lhs, *rhs, *ret;

	lhs = C ? wm_prop_pystring_from_context(C, ptr, prop, index) : NULL;

	if (lhs == NULL) {
		/* fallback to bpy.data.foo[id] if we dont find in the context */
		lhs = RNA_path_full_property_py(ptr, prop, index);
	}

	if (!lhs) {
		return NULL;
	}

	rhs = RNA_property_as_string(C, ptr, prop, index, INT_MAX);
	if (!rhs) {
		MEM_freeN(lhs);
		return NULL;
	}

	ret = BLI_sprintfN("%s = %s", lhs, rhs);
	MEM_freeN(lhs);
	MEM_freeN(rhs);
	return ret;
}

void WM_operator_properties_create_ptr(PointerRNA *ptr, wmOperatorType *ot)
{
	RNA_pointer_create(NULL, ot->srna, NULL, ptr);
}

void WM_operator_properties_create(PointerRNA *ptr, const char *opstring)
{
	wmOperatorType *ot = WM_operatortype_find(opstring, false);

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

void WM_operator_properties_sanitize(PointerRNA *ptr, const bool no_context)
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


/** set all props to their default,
 * \param do_update Only update un-initialized props.
 *
 * \note, theres nothing specific to operators here.
 * this could be made a general function.
 */
bool WM_operator_properties_default(PointerRNA *ptr, const bool do_update)
{
	bool changed = false;
	RNA_STRUCT_BEGIN (ptr, prop)
	{
		switch (RNA_property_type(prop)) {
			case PROP_POINTER:
			{
				StructRNA *ptype = RNA_property_pointer_type(ptr, prop);
				if (ptype != &RNA_Struct) {
					PointerRNA opptr = RNA_property_pointer_get(ptr, prop);
					changed |= WM_operator_properties_default(&opptr, do_update);
				}
				break;
			}
			default:
				if ((do_update == false) || (RNA_property_is_set(ptr, prop) == false)) {
					if (RNA_property_reset(ptr, prop, -1)) {
						changed = true;
					}
				}
				break;
		}
	}
	RNA_STRUCT_END;

	return changed;
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

void WM_operator_properties_clear(PointerRNA *ptr)
{
	IDProperty *properties = ptr->data;

	if (properties) {
		IDP_ClearProperty(properties);
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

void WM_operator_view3d_unit_defaults(struct bContext *C, struct wmOperator *op)
{
	if (op->flag & OP_IS_INVOKE) {
		Scene *scene = CTX_data_scene(C);
		View3D *v3d = CTX_wm_view3d(C);

		const float dia = v3d ? ED_view3d_grid_scale(scene, v3d, NULL) : ED_scene_grid_scale(scene, NULL);

		/* always run, so the values are initialized,
		 * otherwise we may get differ behavior when (dia != 1.0) */
		RNA_STRUCT_BEGIN (op->ptr, prop)
		{
			if (RNA_property_type(prop) == PROP_FLOAT) {
				PropertySubType pstype = RNA_property_subtype(prop);
				if (pstype == PROP_DISTANCE) {
					/* we don't support arrays yet */
					BLI_assert(RNA_property_array_check(prop) == false);
					/* initialize */
					if (!RNA_property_is_set_ex(op->ptr, prop, false)) {
						const float value = RNA_property_float_get_default(op->ptr, prop) * dia;
						RNA_property_float_set(op->ptr, prop, value);
					}
				}
			}
		}
		RNA_STRUCT_END;
	}
}

int WM_operator_smooth_viewtx_get(const wmOperator *op)
{
	return (op->flag & OP_IS_INVOKE) ? U.smooth_viewtx : 0;
}

/* invoke callback, uses enum property named "type" */
int WM_menu_invoke_ex(bContext *C, wmOperator *op, int opcontext)
{
	PropertyRNA *prop = op->type->prop;
	uiPopupMenu *pup;
	uiLayout *layout;

	if (prop == NULL) {
		CLOG_ERROR(WM_LOG_OPERATORS,
		           "'%s' has no enum property set",
		           op->type->idname);
	}
	else if (RNA_property_type(prop) != PROP_ENUM) {
		CLOG_ERROR(WM_LOG_OPERATORS,
		           "'%s', '%s' is not an enum property",
		           op->type->idname, RNA_property_identifier(prop));
	}
	else if (RNA_property_is_set(op->ptr, prop)) {
		const int retval = op->type->exec(C, op);
		OPERATOR_RETVAL_CHECK(retval);
		return retval;
	}
	else {
		pup = UI_popup_menu_begin(C, RNA_struct_ui_name(op->type->srna), ICON_NONE);
		layout = UI_popup_menu_layout(pup);
		/* set this so the default execution context is the same as submenus */
		uiLayoutSetOperatorContext(layout, opcontext);
		uiItemsFullEnumO(layout, op->type->idname, RNA_property_identifier(prop), op->ptr->data, opcontext, 0);
		UI_popup_menu_end(C, pup);
		return OPERATOR_INTERFACE;
	}

	return OPERATOR_CANCELLED;
}

int WM_menu_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	return WM_menu_invoke_ex(C, op, WM_OP_INVOKE_REGION_WIN);
}

struct EnumSearchMenu {
	wmOperator *op; /* the operator that will be executed when selecting an item */

	bool use_previews;
	short prv_cols, prv_rows;
};

/* generic enum search invoke popup */
static uiBlock *wm_enum_search_menu(bContext *C, ARegion *ar, void *arg)
{
	struct EnumSearchMenu *search_menu = arg;
	wmWindow *win = CTX_wm_window(C);
	wmOperator *op = search_menu->op;
	/* template_ID uses 4 * widget_unit for width, we use a bit more, some items may have a suffix to show */
	const int width = search_menu->use_previews ? 5 * U.widget_unit * search_menu->prv_cols : UI_searchbox_size_x();
	const int height = search_menu->use_previews ? 5 * U.widget_unit * search_menu->prv_rows : UI_searchbox_size_y();
	static char search[256] = "";
	uiBlock *block;
	uiBut *but;

	block = UI_block_begin(C, ar, "_popup", UI_EMBOSS);
	UI_block_flag_enable(block, UI_BLOCK_LOOP | UI_BLOCK_MOVEMOUSE_QUIT | UI_BLOCK_SEARCH_MENU);

	search[0] = '\0';
	BLI_assert(search_menu->use_previews || (search_menu->prv_cols == 0 && search_menu->prv_rows == 0));
#if 0 /* ok, this isn't so easy... */
	uiDefBut(block, UI_BTYPE_LABEL, 0, RNA_struct_ui_name(op->type->srna), 10, 10, UI_searchbox_size_x(), UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");
#endif
	but = uiDefSearchButO_ptr(block, op->type, op->ptr->data, search, 0, ICON_VIEWZOOM, sizeof(search),
	                          10, 10, width, UI_UNIT_Y, search_menu->prv_rows, search_menu->prv_cols, "");

	/* fake button, it holds space for search items */
	uiDefBut(block, UI_BTYPE_LABEL, 0, "", 10, 10 - UI_searchbox_size_y(), width, height, NULL, 0, 0, 0, 0, NULL);

	UI_block_bounds_set_popup(block, 6, 0, -UI_UNIT_Y); /* move it downwards, mouse over button */
	UI_but_focus_on_enter_event(win, but);

	return block;
}

/**
 * Similar to #WM_enum_search_invoke, but draws previews. Also, this can't
 * be used as invoke callback directly since it needs additional info.
 */
int WM_enum_search_invoke_previews(
        bContext *C, wmOperator *op, short prv_cols, short prv_rows)
{
	static struct EnumSearchMenu search_menu;

	search_menu.op = op;
	search_menu.use_previews = true;
	search_menu.prv_cols = prv_cols;
	search_menu.prv_rows = prv_rows;

	UI_popup_block_invoke(C, wm_enum_search_menu, &search_menu);

	return OPERATOR_INTERFACE;
}

int WM_enum_search_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	static struct EnumSearchMenu search_menu;
	search_menu.op = op;
	UI_popup_block_invoke(C, wm_enum_search_menu, &search_menu);
	return OPERATOR_INTERFACE;
}

/* Can't be used as an invoke directly, needs message arg (can be NULL) */
int WM_operator_confirm_message_ex(bContext *C, wmOperator *op,
                                   const char *title, const int icon,
                                   const char *message)
{
	uiPopupMenu *pup;
	uiLayout *layout;
	IDProperty *properties = op->ptr->data;

	if (properties && properties->len)
		properties = IDP_CopyProperty(op->ptr->data);
	else
		properties = NULL;

	pup = UI_popup_menu_begin(C, title, icon);
	layout = UI_popup_menu_layout(pup);
	uiItemFullO_ptr(layout, op->type, message, ICON_NONE, properties, WM_OP_EXEC_REGION_WIN, 0, NULL);
	UI_popup_menu_end(C, pup);

	return OPERATOR_INTERFACE;
}

int WM_operator_confirm_message(bContext *C, wmOperator *op, const char *message)
{
	return WM_operator_confirm_message_ex(C, op, IFACE_("OK?"), ICON_QUESTION, message);
}

int WM_operator_confirm(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	return WM_operator_confirm_message(C, op, NULL);
}

/* op->invoke, opens fileselect if path property not set, otherwise executes */
int WM_operator_filesel(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	if (RNA_struct_property_is_set(op->ptr, "filepath")) {
		return WM_operator_call_notest(C, op); /* call exec direct */
	}
	else {
		WM_event_add_fileselect(C, op);
		return OPERATOR_RUNNING_MODAL;
	}
}

bool WM_operator_filesel_ensure_ext_imtype(wmOperator *op, const struct ImageFormatData *im_format)
{
	PropertyRNA *prop;
	char filepath[FILE_MAX];
	/* dont NULL check prop, this can only run on ops with a 'filepath' */
	prop = RNA_struct_find_property(op->ptr, "filepath");
	RNA_property_string_get(op->ptr, prop, filepath);
	if (BKE_image_path_ensure_ext_from_imformat(filepath, im_format)) {
		RNA_property_string_set(op->ptr, prop, filepath);
		/* note, we could check for and update 'filename' here,
		 * but so far nothing needs this. */
		return true;
	}
	return false;
}

/* op->poll */
bool WM_operator_winactive(bContext *C)
{
	if (CTX_wm_window(C) == NULL) return 0;
	return 1;
}

/* return false, if the UI should be disabled */
bool WM_operator_check_ui_enabled(const bContext *C, const char *idname)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	Scene *scene = CTX_data_scene(C);

	return !((ED_undo_is_valid(C, idname) == false) || WM_jobs_test(wm, scene, WM_JOB_TYPE_ANY));
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

IDProperty *WM_operator_last_properties_ensure_idprops(wmOperatorType *ot)
{
	if (ot->last_properties == NULL) {
		IDPropertyTemplate val = {0};
		ot->last_properties = IDP_New(IDP_GROUP, &val, "wmOperatorProperties");
	}
	return ot->last_properties;
}

void WM_operator_last_properties_ensure(wmOperatorType *ot, PointerRNA *ptr)
{
	IDProperty *props = WM_operator_last_properties_ensure_idprops(ot);
	RNA_pointer_create(NULL, ot->srna, props, ptr);
}

/**
 * Use for drag & drop a path or name with operators invoke() function.
 */
ID *WM_operator_drop_load_path(struct bContext *C, wmOperator *op, const short idcode)
{
	Main *bmain = CTX_data_main(C);
	ID *id = NULL;
	/* check input variables */
	if (RNA_struct_property_is_set(op->ptr, "filepath")) {
		const bool is_relative_path = RNA_boolean_get(op->ptr, "relative_path");
		char path[FILE_MAX];
		bool exists = false;

		RNA_string_get(op->ptr, "filepath", path);

		errno = 0;

		if (idcode == ID_IM) {
			id = (ID *)BKE_image_load_exists_ex(bmain, path, &exists);
		}
		else {
			BLI_assert(0);
		}

		if (!id) {
			BKE_reportf(op->reports, RPT_ERROR, "Cannot read %s '%s': %s",
			            BKE_idcode_to_name(idcode), path,
			            errno ? strerror(errno) : TIP_("unsupported format"));
			return NULL;
		}

		if (is_relative_path ) {
			if (exists == false) {
				if (idcode == ID_IM) {
					BLI_path_rel(((Image *)id)->name, BKE_main_blendfile_path(bmain));
				}
				else {
					BLI_assert(0);
				}
			}
		}
	}
	else if (RNA_struct_property_is_set(op->ptr, "name")) {
		char name[MAX_ID_NAME - 2];
		RNA_string_get(op->ptr, "name", name);
		id = BKE_libblock_find_name(bmain, idcode, name);
		if (!id) {
			BKE_reportf(op->reports, RPT_ERROR, "%s '%s' not found",
			            BKE_idcode_to_name(idcode), name);
			return NULL;
		}
		id_us_plus(id);
	}

	return id;
}

static void wm_block_redo_cb(bContext *C, void *arg_op, int UNUSED(arg_event))
{
	wmOperator *op = arg_op;

	if (op == WM_operator_last_redo(C)) {
		/* operator was already executed once? undo & repeat */
		ED_undo_operator_repeat(C, op);
	}
	else {
		/* operator not executed yet, call it */
		ED_undo_push_op(C, op);
		wm_operator_register(C, op);

		WM_operator_repeat(C, op);
	}
}

static void wm_block_redo_cancel_cb(bContext *C, void *arg_op)
{
	wmOperator *op = arg_op;

	/* if operator never got executed, free it */
	if (op != WM_operator_last_redo(C))
		WM_operator_free(op);
}

static uiBlock *wm_block_create_redo(bContext *C, ARegion *ar, void *arg_op)
{
	wmOperator *op = arg_op;
	uiBlock *block;
	uiLayout *layout;
	uiStyle *style = UI_style_get();
	int width = 15 * UI_UNIT_X;

	block = UI_block_begin(C, ar, __func__, UI_EMBOSS);
	UI_block_flag_disable(block, UI_BLOCK_LOOP);
	/* UI_BLOCK_NUMSELECT for layer buttons */
	UI_block_flag_enable(block, UI_BLOCK_NUMSELECT | UI_BLOCK_KEEP_OPEN | UI_BLOCK_MOVEMOUSE_QUIT);

	/* if register is not enabled, the operator gets freed on OPERATOR_FINISHED
	 * ui_apply_but_funcs_after calls ED_undo_operator_repeate_cb and crashes */
	assert(op->type->flag & OPTYPE_REGISTER);

	UI_block_func_handle_set(block, wm_block_redo_cb, arg_op);
	layout = UI_block_layout(block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL, 0, 0, width, UI_UNIT_Y, 0, style);

	if (op == WM_operator_last_redo(C))
		if (!WM_operator_check_ui_enabled(C, op->type->name))
			uiLayoutSetEnabled(layout, false);

	if (op->type->flag & OPTYPE_MACRO) {
		for (op = op->macro.first; op; op = op->next) {
			uiTemplateOperatorPropertyButs(
			        C, layout, op, UI_BUT_LABEL_ALIGN_SPLIT_COLUMN,
			        UI_TEMPLATE_OP_PROPS_SHOW_TITLE);
			if (op->next)
				uiItemS(layout);
		}
	}
	else {
		uiTemplateOperatorPropertyButs(
		        C, layout, op, UI_BUT_LABEL_ALIGN_SPLIT_COLUMN,
		        UI_TEMPLATE_OP_PROPS_SHOW_TITLE);
	}

	UI_block_bounds_set_popup(block, 4, 0, 0);

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

	/* Explicitly set UI_RETURN_OK flag, otherwise the menu might be canceled
	 * in case WM_operator_call_ex exits/reloads the current file (T49199). */
	UI_popup_menu_retval_set(block, UI_RETURN_OK, true);

	WM_operator_call_ex(C, data->op, true);

	/* let execute handle freeing it */
	//data->free_op = false;
	//data->op = NULL;

	/* in this case, wm_operator_ui_popup_cancel wont run */
	MEM_freeN(data);

	/* get context data *after* WM_operator_call_ex which might have closed the current file and changed context */
	wmWindowManager *wm = CTX_wm_manager(C);
	wmWindow *win = CTX_wm_window(C);

	/* check window before 'block->handle' incase the
	 * popup execution closed the window and freed the block. see T44688.
	 */
	/* Post 2.78 TODO: Check if this fix and others related to T44688 are still
	 * needed or can be improved now that requesting context data has been corrected
	 * (see above). We're close to release so not a good time for experiments.
	 * -- Julian
	 */
	if (BLI_findindex(&wm->windows, win) != -1) {
		UI_popup_block_close(C, win, block);
	}
}

/* Dialogs are popups that require user verification (click OK) before exec */
static uiBlock *wm_block_dialog_create(bContext *C, ARegion *ar, void *userData)
{
	wmOpPopUp *data = userData;
	wmOperator *op = data->op;
	uiBlock *block;
	uiLayout *layout;
	uiStyle *style = UI_style_get();

	block = UI_block_begin(C, ar, __func__, UI_EMBOSS);
	UI_block_flag_disable(block, UI_BLOCK_LOOP);

	/* intentionally don't use 'UI_BLOCK_MOVEMOUSE_QUIT', some dialogues have many items
	 * where quitting by accident is very annoying */
	UI_block_flag_enable(block, UI_BLOCK_KEEP_OPEN | UI_BLOCK_NUMSELECT);

	layout = UI_block_layout(block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL, 0, 0, data->width, data->height, 0, style);

	uiTemplateOperatorPropertyButs(
	        C, layout, op, UI_BUT_LABEL_ALIGN_SPLIT_COLUMN,
	        UI_TEMPLATE_OP_PROPS_SHOW_TITLE);

	/* clear so the OK button is left alone */
	UI_block_func_set(block, NULL, NULL, NULL);

	/* new column so as not to interfere with custom layouts [#26436] */
	{
		uiBlock *col_block;
		uiLayout *col;
		uiBut *btn;

		col = uiLayoutColumn(layout, false);
		col_block = uiLayoutGetBlock(col);
		/* Create OK button, the callback of which will execute op */
		btn = uiDefBut(col_block, UI_BTYPE_BUT, 0, IFACE_("OK"), 0, -30, 0, UI_UNIT_Y, NULL, 0, 0, 0, 0, "");
		UI_but_func_set(btn, dialog_exec_cb, data, col_block);
	}

	/* center around the mouse */
	UI_block_bounds_set_popup(block, 4, data->width / -2, data->height / 2);

	return block;
}

static uiBlock *wm_operator_ui_create(bContext *C, ARegion *ar, void *userData)
{
	wmOpPopUp *data = userData;
	wmOperator *op = data->op;
	uiBlock *block;
	uiLayout *layout;
	uiStyle *style = UI_style_get();

	block = UI_block_begin(C, ar, __func__, UI_EMBOSS);
	UI_block_flag_disable(block, UI_BLOCK_LOOP);
	UI_block_flag_enable(block, UI_BLOCK_KEEP_OPEN | UI_BLOCK_MOVEMOUSE_QUIT);

	layout = UI_block_layout(block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL, 0, 0, data->width, data->height, 0, style);

	/* since ui is defined the auto-layout args are not used */
	uiTemplateOperatorPropertyButs(C, layout, op, UI_BUT_LABEL_ALIGN_COLUMN, 0);

	UI_block_func_set(block, NULL, NULL, NULL);

	UI_block_bounds_set_popup(block, 4, 0, 0);

	return block;
}

static void wm_operator_ui_popup_cancel(struct bContext *C, void *userData)
{
	wmOpPopUp *data = userData;
	wmOperator *op = data->op;

	if (op) {
		if (op->type->cancel) {
			op->type->cancel(C, op);
		}

		if (data->free_op) {
			WM_operator_free(op);
		}
	}

	MEM_freeN(data);
}

static void wm_operator_ui_popup_ok(struct bContext *C, void *arg, int retval)
{
	wmOpPopUp *data = arg;
	wmOperator *op = data->op;

	if (op && retval > 0)
		WM_operator_call_ex(C, op, true);

	MEM_freeN(data);
}

int WM_operator_ui_popup(bContext *C, wmOperator *op, int width, int height)
{
	wmOpPopUp *data = MEM_callocN(sizeof(wmOpPopUp), "WM_operator_ui_popup");
	data->op = op;
	data->width = width;
	data->height = height;
	data->free_op = true; /* if this runs and gets registered we may want not to free it */
	UI_popup_block_ex(C, wm_operator_ui_create, NULL, wm_operator_ui_popup_cancel, data, op);
	return OPERATOR_RUNNING_MODAL;
}

/**
 * For use by #WM_operator_props_popup_call, #WM_operator_props_popup only.
 *
 * \note operator menu needs undo flag enabled, for redo callback */
static int wm_operator_props_popup_ex(bContext *C, wmOperator *op,
                                      const bool do_call, const bool do_redo)
{
	if ((op->type->flag & OPTYPE_REGISTER) == 0) {
		BKE_reportf(op->reports, RPT_ERROR,
		            "Operator '%s' does not have register enabled, incorrect invoke function", op->type->idname);
		return OPERATOR_CANCELLED;
	}

	if (do_redo) {
		if ((op->type->flag & OPTYPE_UNDO) == 0) {
			BKE_reportf(op->reports, RPT_ERROR,
			            "Operator '%s' does not have undo enabled, incorrect invoke function", op->type->idname);
			return OPERATOR_CANCELLED;
		}
	}

	/* if we don't have global undo, we can't do undo push for automatic redo,
	 * so we require manual OK clicking in this popup */
	if (!do_redo || !(U.uiflag & USER_GLOBALUNDO))
		return WM_operator_props_dialog_popup(C, op, 15 * UI_UNIT_X, UI_UNIT_Y);

	UI_popup_block_ex(C, wm_block_create_redo, NULL, wm_block_redo_cancel_cb, op, op);

	if (do_call)
		wm_block_redo_cb(C, op, 0);

	return OPERATOR_RUNNING_MODAL;
}

/**
 * Same as #WM_operator_props_popup but don't use operator redo.
 * just wraps #WM_operator_props_dialog_popup.
 */
int WM_operator_props_popup_confirm(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	return wm_operator_props_popup_ex(C, op, false, false);
}

/**
 * Same as #WM_operator_props_popup but call the operator first,
 * This way - the button values correspond to the result of the operator.
 * Without this, first access to a button will make the result jump, see T32452.
 */
int WM_operator_props_popup_call(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	return wm_operator_props_popup_ex(C, op, true, true);
}

int WM_operator_props_popup(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	return wm_operator_props_popup_ex(C, op, false, true);
}

int WM_operator_props_dialog_popup(bContext *C, wmOperator *op, int width, int height)
{
	wmOpPopUp *data = MEM_callocN(sizeof(wmOpPopUp), "WM_operator_props_dialog_popup");

	data->op = op;
	data->width = width;
	data->height = height;
	data->free_op = true; /* if this runs and gets registered we may want not to free it */

	/* op is not executed until popup OK but is clicked */
	UI_popup_block_ex(C, wm_block_dialog_create, wm_operator_ui_popup_ok, wm_operator_ui_popup_cancel, data, op);

	return OPERATOR_RUNNING_MODAL;
}

int WM_operator_redo_popup(bContext *C, wmOperator *op)
{
	/* CTX_wm_reports(C) because operator is on stack, not active in event system */
	if ((op->type->flag & OPTYPE_REGISTER) == 0) {
		BKE_reportf(CTX_wm_reports(C), RPT_ERROR,
		            "Operator redo '%s' does not have register enabled, incorrect invoke function", op->type->idname);
		return OPERATOR_CANCELLED;
	}
	if (op->type->poll && op->type->poll(C) == 0) {
		BKE_reportf(CTX_wm_reports(C), RPT_ERROR, "Operator redo '%s': wrong context", op->type->idname);
		return OPERATOR_CANCELLED;
	}

	UI_popup_block_invoke(C, wm_block_create_redo, op);

	return OPERATOR_CANCELLED;
}

/* ***************** Debug menu ************************* */

static int wm_debug_menu_exec(bContext *C, wmOperator *op)
{
	G.debug_value = RNA_int_get(op->ptr, "debug_value");
	ED_screen_refresh(CTX_wm_manager(C), CTX_wm_window(C));
	WM_event_add_notifier(C, NC_WINDOW, NULL);

	return OPERATOR_FINISHED;
}

static int wm_debug_menu_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	RNA_int_set(op->ptr, "debug_value", G.debug_value);
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

	RNA_def_int(ot->srna, "debug_value", 0, SHRT_MIN, SHRT_MAX, "Debug Value", "", -10000, 10000);
}

/* ***************** Operator defaults ************************* */
static int wm_operator_defaults_exec(bContext *C, wmOperator *op)
{
	PointerRNA ptr = CTX_data_pointer_get_type(C, "active_operator", &RNA_Operator);

	if (!ptr.data) {
		BKE_report(op->reports, RPT_ERROR, "No operator in context");
		return OPERATOR_CANCELLED;
	}

	WM_operator_properties_reset((wmOperator *)ptr.data);
	return OPERATOR_FINISHED;
}

/* used by operator preset menu. pre-2.65 this was a 'Reset' button */
static void WM_OT_operator_defaults(wmOperatorType *ot)
{
	ot->name = "Restore Defaults";
	ot->idname = "WM_OT_operator_defaults";
	ot->description = "Set the active operator to its default values";

	ot->exec = wm_operator_defaults_exec;

	ot->flag = OPTYPE_INTERNAL;
}

/* ***************** Splash Screen ************************* */

static void wm_block_splash_close(bContext *C, void *arg_block, void *UNUSED(arg))
{
	wmWindow *win = CTX_wm_window(C);
	UI_popup_block_close(C, win, arg_block);
}

static uiBlock *wm_block_create_splash(bContext *C, ARegion *ar, void *arg_unused);

static void wm_block_splash_refreshmenu(bContext *C, void *UNUSED(arg_block), void *UNUSED(arg))
{
	ARegion *ar_menu = CTX_wm_menu(C);
	ED_region_tag_refresh_ui(ar_menu);
}

static int wm_resource_check_prev(void)
{

	const char *res = BKE_appdir_folder_id_version(BLENDER_RESOURCE_PATH_USER, BLENDER_VERSION, true);

	// if (res) printf("USER: %s\n", res);

#if 0 /* ignore the local folder */
	if (res == NULL) {
		/* with a local dir, copying old files isn't useful since local dir get priority for config */
		res = BKE_appdir_folder_id_version(BLENDER_RESOURCE_PATH_LOCAL, BLENDER_VERSION, true);
	}
#endif

	// if (res) printf("LOCAL: %s\n", res);
	if (res) {
		return false;
	}
	else {
		return (BKE_appdir_folder_id_version(BLENDER_RESOURCE_PATH_USER, BLENDER_VERSION - 1, true) != NULL);
	}
}

static uiBlock *wm_block_create_splash(bContext *C, ARegion *ar, void *UNUSED(arg))
{
	uiBlock *block;
	uiBut *but;
	uiLayout *layout, *split, *col;
	uiStyle *style = UI_style_get();
	const struct RecentFile *recent;
	int i;
	MenuType *mt = WM_menutype_find("USERPREF_MT_splash", true);
	char url[96];
	const char *version_suffix = NULL;

#ifndef WITH_HEADLESS
	extern char datatoc_splash_png[];
	extern int datatoc_splash_png_size;

	extern char datatoc_splash_2x_png[];
	extern int datatoc_splash_2x_png_size;
	ImBuf *ibuf;
#else
	ImBuf *ibuf = NULL;
#endif

#ifdef WITH_BUILDINFO
	int label_delta = 0;
	int hash_width, date_width;
	char date_buf[128] = "\0";
	char hash_buf[128] = "\0";
	extern unsigned long build_commit_timestamp;
	extern char build_hash[], build_commit_date[], build_commit_time[], build_branch[];

	/* Builds made from tag only shows tag sha */
	BLI_snprintf(hash_buf, sizeof(hash_buf), "Hash: %s", build_hash);
	BLI_snprintf(date_buf, sizeof(date_buf), "Date: %s %s", build_commit_date, build_commit_time);

	BLF_size(style->widgetlabel.uifont_id, style->widgetlabel.points, U.pixelsize * U.dpi);
	hash_width = (int)BLF_width(style->widgetlabel.uifont_id, hash_buf, sizeof(hash_buf)) + U.widget_unit;
	date_width = (int)BLF_width(style->widgetlabel.uifont_id, date_buf, sizeof(date_buf)) + U.widget_unit;
#endif  /* WITH_BUILDINFO */

#ifndef WITH_HEADLESS
	if (U.pixelsize == 2) {
		ibuf = IMB_ibImageFromMemory((unsigned char *)datatoc_splash_2x_png,
		                             datatoc_splash_2x_png_size, IB_rect, NULL, "<splash screen>");
	}
	else {
		ibuf = IMB_ibImageFromMemory((unsigned char *)datatoc_splash_png,
		                             datatoc_splash_png_size, IB_rect, NULL, "<splash screen>");
	}

	/* overwrite splash with template image */
	if (U.app_template[0] != '\0') {
		ImBuf *ibuf_template = NULL;
		char splash_filepath[FILE_MAX];
		char template_directory[FILE_MAX];

		if (BKE_appdir_app_template_id_search(
		        U.app_template,
		        template_directory, sizeof(template_directory)))
		{
			BLI_join_dirfile(
			        splash_filepath, sizeof(splash_filepath), template_directory,
			        (U.pixelsize == 2) ? "splash_2x.png" : "splash.png");
			ibuf_template = IMB_loadiffname(splash_filepath, IB_rect, NULL);
			if (ibuf_template) {
				const int x_expect = ibuf->x;
				const int y_expect = 282 * (int)U.pixelsize;
				/* don't cover the header text */
				if (ibuf_template->x == x_expect && ibuf_template->y == y_expect) {
					memcpy(ibuf->rect, ibuf_template->rect, ibuf_template->x * ibuf_template->y * sizeof(char[4]));
				}
				else {
					CLOG_ERROR(WM_LOG_OPERATORS,
					           "Splash expected %dx%d found %dx%d, ignoring: %s\n",
					           x_expect, y_expect, ibuf_template->x, ibuf_template->y, splash_filepath);
				}
				IMB_freeImBuf(ibuf_template);
			}
		}
	}
#endif

	block = UI_block_begin(C, ar, "_popup", UI_EMBOSS);

	/* note on UI_BLOCK_NO_WIN_CLIP, the window size is not always synchronized
	 * with the OS when the splash shows, window clipping in this case gives
	 * ugly results and clipping the splash isn't useful anyway, just disable it [#32938] */
	UI_block_flag_enable(block, UI_BLOCK_LOOP | UI_BLOCK_KEEP_OPEN | UI_BLOCK_NO_WIN_CLIP);

	/* XXX splash scales with pixelsize, should become widget-units */
	but = uiDefBut(block, UI_BTYPE_IMAGE, 0, "", 0, 0.5f * U.widget_unit, U.pixelsize * 501, U.pixelsize * 282, ibuf, 0.0, 0.0, 0, 0, ""); /* button owns the imbuf now */
	UI_but_func_set(but, wm_block_splash_close, block, NULL);
	UI_block_func_set(block, wm_block_splash_refreshmenu, block, NULL);

	/* label for 'a' bugfix releases, or 'Release Candidate 1'...
	 *  avoids recreating splash for version updates */
	if (STREQ(STRINGIFY(BLENDER_VERSION_CYCLE), "alpha")) {
		version_suffix = "Alpha 2";
	}
	else if (STREQ(STRINGIFY(BLENDER_VERSION_CYCLE), "beta")) {
		version_suffix = "Beta";
	}
	else if (STREQ(STRINGIFY(BLENDER_VERSION_CYCLE), "rc")) {
		version_suffix = "Release Candidate";
	}
	else if (STREQ(STRINGIFY(BLENDER_VERSION_CYCLE), "release")) {
		version_suffix = STRINGIFY(BLENDER_VERSION_CHAR);
	}
	if (version_suffix != NULL && version_suffix[0]) {
		/* placed after the version number in the image,
		 * placing y is tricky to match baseline */
		int x = 236 * U.pixelsize - (2 * UI_DPI_FAC);
		int y = 231 * U.pixelsize + (4 * UI_DPI_FAC);
		int w = 240 * U.pixelsize;

		/* hack to have text draw 'text_sel' */
		UI_block_emboss_set(block, UI_EMBOSS_NONE);
		but = uiDefBut(block, UI_BTYPE_LABEL, 0, version_suffix, x, y, w, UI_UNIT_Y, NULL, 0, 0, 0, 0, NULL);
		/* XXX, set internal flag - UI_SELECT */
		UI_but_flag_enable(but, 1);
		UI_block_emboss_set(block, UI_EMBOSS);
	}

#ifdef WITH_BUILDINFO
	if (build_commit_timestamp != 0) {
		but = uiDefBut(
		          block, UI_BTYPE_LABEL, 0, date_buf,
		          U.pixelsize * 502 - date_width, U.pixelsize * 267,
		          date_width, UI_UNIT_Y, NULL, 0, 0, 0, 0, NULL);
		/* XXX, set internal flag - UI_SELECT */
		UI_but_flag_enable(but, 1);
		label_delta = 12;
	}
	but = uiDefBut(
	          block, UI_BTYPE_LABEL, 0, hash_buf,
	          U.pixelsize * 502 - hash_width, U.pixelsize * (267 - label_delta),
	          hash_width, UI_UNIT_Y, NULL, 0, 0, 0, 0, NULL);
	/* XXX, set internal flag - UI_SELECT */
	UI_but_flag_enable(but, 1);

	if (!STREQ(build_branch, "master")) {
		char branch_buf[128] = "\0";
		int branch_width;
		BLI_snprintf(branch_buf, sizeof(branch_buf), "Branch: %s", build_branch);
		branch_width = (int)BLF_width(style->widgetlabel.uifont_id, branch_buf, sizeof(branch_buf)) + U.widget_unit;
		but = uiDefBut(
		          block, UI_BTYPE_LABEL, 0, branch_buf,
		          U.pixelsize * 502 - branch_width, U.pixelsize * (255 - label_delta),
		          branch_width, UI_UNIT_Y, NULL, 0, 0, 0, 0, NULL);
		/* XXX, set internal flag - UI_SELECT */
		UI_but_flag_enable(but, 1);
	}
#endif  /* WITH_BUILDINFO */

	layout = UI_block_layout(block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL, 10, 2, U.pixelsize * 480, U.pixelsize * 110, 0, style);

	UI_block_emboss_set(block, UI_EMBOSS);
	/* show the splash menu (containing interaction presets), using python */
	if (mt) {
		UI_menutype_draw(C, mt, layout);

//		uiItemM(layout, "USERPREF_MT_keyconfigs", U.keyconfigstr, ICON_NONE);
	}

	UI_block_emboss_set(block, UI_EMBOSS_PULLDOWN);
	uiLayoutSetOperatorContext(layout, WM_OP_EXEC_REGION_WIN);

	split = uiLayoutSplit(layout, 0.0f, false);
	col = uiLayoutColumn(split, false);
	uiItemL(col, IFACE_("Links"), ICON_NONE);
	uiItemStringO(col, IFACE_("Join the Development Fund"), ICON_URL, "WM_OT_url_open", "url",
	              "https://www.blender.org/foundation/development-fund/");
	uiItemStringO(col, IFACE_("Donations"), ICON_URL, "WM_OT_url_open", "url",
	              "http://www.blender.org/foundation/donation-payment/");
	uiItemStringO(col, IFACE_("Credits"), ICON_URL, "WM_OT_url_open", "url",
	              "http://www.blender.org/about/credits/");
	BLI_snprintf(url, sizeof(url), "https://wiki.blender.org/wiki/Reference/Release_Notes/%d.%d",
	             BLENDER_VERSION / 100, BLENDER_VERSION % 100);
	uiItemStringO(col, IFACE_("Release Log"), ICON_URL, "WM_OT_url_open", "url", url);
	uiItemStringO(col, IFACE_("Manual"), ICON_URL, "WM_OT_url_open", "url",
	              "https://docs.blender.org/manual/en/dev/");
	uiItemStringO(col, IFACE_("Blender Website"), ICON_URL, "WM_OT_url_open", "url", "http://www.blender.org");
	if (STREQ(STRINGIFY(BLENDER_VERSION_CYCLE), "release")) {
		BLI_snprintf(url, sizeof(url), "https://docs.blender.org/api/%d.%d"STRINGIFY(BLENDER_VERSION_CHAR),
		             BLENDER_VERSION / 100, BLENDER_VERSION % 100);
	}
	else {
		BLI_snprintf(url, sizeof(url), "https://docs.blender.org/api/master");
	}
	uiItemStringO(col, IFACE_("Python API Reference"), ICON_URL, "WM_OT_url_open", "url", url);
	uiItemL(col, "", ICON_NONE);

	col = uiLayoutColumn(split, false);

	if (wm_resource_check_prev()) {
		uiItemO(col, NULL, ICON_NEW, "WM_OT_copy_prev_settings");
		uiItemS(col);
	}

	uiItemL(col, IFACE_("Recent"), ICON_NONE);
	for (recent = G.recent_files.first, i = 0; (i < 5) && (recent); recent = recent->next, i++) {
		const char *filename = BLI_path_basename(recent->filepath);
		uiItemStringO(col, filename,
		              BLO_has_bfile_extension(filename) ? ICON_FILE_BLEND : ICON_FILE_BACKUP,
		              "WM_OT_open_mainfile", "filepath", recent->filepath);
	}

	uiItemS(col);
	uiItemO(col, NULL, ICON_RECOVER_LAST, "WM_OT_recover_last_session");
	uiItemL(col, "", ICON_NONE);

	mt = WM_menutype_find("USERPREF_MT_splash_footer", false);
	if (mt) {
		UI_menutype_draw(C, mt, uiLayoutColumn(layout, false));
	}

	UI_block_bounds_set_centered(block, 0);

	return block;
}

static int wm_splash_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *UNUSED(event))
{
	UI_popup_block_invoke(C, wm_block_create_splash, NULL);

	return OPERATOR_FINISHED;
}

static void WM_OT_splash(wmOperatorType *ot)
{
	ot->name = "Splash Screen";
	ot->idname = "WM_OT_splash";
	ot->description = "Open the splash screen with release info";

	ot->invoke = wm_splash_invoke;
	ot->poll = WM_operator_winactive;
}


/* ***************** Search menu ************************* */

struct SearchPopupInit_Data {
	int size[2];
};

static uiBlock *wm_block_search_menu(bContext *C, ARegion *ar, void *userdata)
{
	const struct SearchPopupInit_Data *init_data = userdata;
	static char search[256] = "";
	wmEvent event;
	wmWindow *win = CTX_wm_window(C);
	uiBlock *block;
	uiBut *but;

	block = UI_block_begin(C, ar, "_popup", UI_EMBOSS);
	UI_block_flag_enable(block, UI_BLOCK_LOOP | UI_BLOCK_MOVEMOUSE_QUIT | UI_BLOCK_SEARCH_MENU);

	but = uiDefSearchBut(block, search, 0, ICON_VIEWZOOM, sizeof(search), 10, 10, init_data->size[0], UI_UNIT_Y, 0, 0, "");
	UI_but_func_operator_search(but);

	/* fake button, it holds space for search items */
	uiDefBut(block, UI_BTYPE_LABEL, 0, "", 10, 10 - init_data->size[1],
	         init_data->size[0], init_data->size[1], NULL, 0, 0, 0, 0, NULL);

	UI_block_bounds_set_popup(block, 6, 0, -UI_UNIT_Y); /* move it downwards, mouse over button */

	wm_event_init_from_window(win, &event);
	event.type = EVT_BUT_OPEN;
	event.val = KM_PRESS;
	event.customdata = but;
	event.customdatafree = false;
	wm_event_add(win, &event);

	return block;
}

static int wm_search_menu_exec(bContext *UNUSED(C), wmOperator *UNUSED(op))
{
	return OPERATOR_FINISHED;
}

static int wm_search_menu_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *UNUSED(event))
{
	struct SearchPopupInit_Data data = {
		.size = {
		    UI_searchbox_size_x() * 2,
		    UI_searchbox_size_y(),
		},
	};

	UI_popup_block_invoke(C, wm_block_search_menu, &data);

	return OPERATOR_INTERFACE;
}

/* op->poll */
static bool wm_search_menu_poll(bContext *C)
{
	if (CTX_wm_window(C) == NULL) {
		return 0;
	}
	else {
		ScrArea *sa = CTX_wm_area(C);
		if (sa) {
			if (sa->spacetype == SPACE_CONSOLE) return 0;  /* XXX - so we can use the shortcut in the console */
			if (sa->spacetype == SPACE_TEXT) return 0;     /* XXX - so we can use the spacebar in the text editor */
		}
		else {
			Object *editob = CTX_data_edit_object(C);
			if (editob && editob->type == OB_FONT) return 0;  /* XXX - so we can use the spacebar for entering text */
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

	return UI_popup_menu_invoke(C, idname, op->reports);
}

static void WM_OT_call_menu(wmOperatorType *ot)
{
	ot->name = "Call Menu";
	ot->idname = "WM_OT_call_menu";
	ot->description = "Call (draw) a pre-defined menu";

	ot->exec = wm_call_menu_exec;
	ot->poll = WM_operator_winactive;

	ot->flag = OPTYPE_INTERNAL;

	RNA_def_string(ot->srna, "name", NULL, BKE_ST_MAXNAME, "Name", "Name of the menu");
}

static int wm_call_pie_menu_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	char idname[BKE_ST_MAXNAME];
	RNA_string_get(op->ptr, "name", idname);

	return UI_pie_menu_invoke(C, idname, event);
}

static int wm_call_pie_menu_exec(bContext *C, wmOperator *op)
{
	char idname[BKE_ST_MAXNAME];
	RNA_string_get(op->ptr, "name", idname);

	return UI_pie_menu_invoke(C, idname, CTX_wm_window(C)->eventstate);
}

static void WM_OT_call_menu_pie(wmOperatorType *ot)
{
	ot->name = "Call Pie Menu";
	ot->idname = "WM_OT_call_menu_pie";
	ot->description = "Call (draw) a pre-defined pie menu";

	ot->invoke = wm_call_pie_menu_invoke;
	ot->exec = wm_call_pie_menu_exec;
	ot->poll = WM_operator_winactive;

	ot->flag = OPTYPE_INTERNAL;

	RNA_def_string(ot->srna, "name", NULL, BKE_ST_MAXNAME, "Name", "Name of the pie menu");
}

static int wm_call_panel_exec(bContext *C, wmOperator *op)
{
	char idname[BKE_ST_MAXNAME];
	RNA_string_get(op->ptr, "name", idname);
	const bool keep_open = RNA_boolean_get(op->ptr, "keep_open");

	return UI_popover_panel_invoke(C, idname, keep_open, op->reports);
}

static void WM_OT_call_panel(wmOperatorType *ot)
{
	ot->name = "Call Panel";
	ot->idname = "WM_OT_call_panel";
	ot->description = "Call (draw) a pre-defined panel";

	ot->exec = wm_call_panel_exec;
	ot->poll = WM_operator_winactive;

	ot->flag = OPTYPE_INTERNAL;

	PropertyRNA *prop;

	prop = RNA_def_string(ot->srna, "name", NULL, BKE_ST_MAXNAME, "Name", "Name of the menu");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	prop = RNA_def_boolean(ot->srna, "keep_open", true, "Keep Open", "");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/* ************ window / screen operator definitions ************** */

/* this poll functions is needed in place of WM_operator_winactive
 * while it crashes on full screen */
static bool wm_operator_winactive_normal(bContext *C)
{
	wmWindow *win = CTX_wm_window(C);
	bScreen *screen;

	if (win == NULL)
		return 0;
	if (!((screen = WM_window_get_active_screen(win)) && (screen->state == SCREENNORMAL)))
		return 0;

	return 1;
}

/* included for script-access */
static void WM_OT_window_close(wmOperatorType *ot)
{
	ot->name = "Close Window";
	ot->idname = "WM_OT_window_close";
	ot->description = "Close the current window";

	ot->exec = wm_window_close_exec;
	ot->poll = WM_operator_winactive;
}

static void WM_OT_window_new(wmOperatorType *ot)
{
	ot->name = "New Window";
	ot->idname = "WM_OT_window_new";
	ot->description = "Create a new window";

	ot->exec = wm_window_new_exec;
	ot->poll = wm_operator_winactive_normal;
}

static void WM_OT_window_new_main(wmOperatorType *ot)
{
	ot->name = "New Main Window";
	ot->idname = "WM_OT_window_new_main";
	ot->description = "Create a new main window with its own workspace and scene selection";

	ot->exec = wm_window_new_main_exec;
	ot->poll = wm_operator_winactive_normal;
}

static void WM_OT_window_fullscreen_toggle(wmOperatorType *ot)
{
	ot->name = "Toggle Window Fullscreen";
	ot->idname = "WM_OT_window_fullscreen_toggle";
	ot->description = "Toggle the current window fullscreen";

	ot->exec = wm_window_fullscreen_toggle_exec;
	ot->poll = WM_operator_winactive;
}

static int wm_exit_blender_exec(bContext *C, wmOperator *UNUSED(op))
{
	wm_quit_with_optional_confirmation_prompt(C, CTX_wm_window(C));
	return OPERATOR_FINISHED;
}

static int wm_exit_blender_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	if (U.uiflag & USER_QUIT_PROMPT) {
		return wm_exit_blender_exec(C, op);
	}
	else {
		return WM_operator_confirm(C, op, event);
	}
}

static void WM_OT_quit_blender(wmOperatorType *ot)
{
	ot->name = "Quit Blender";
	ot->idname = "WM_OT_quit_blender";
	ot->description = "Quit Blender";

	ot->invoke = wm_exit_blender_invoke;
	ot->exec = wm_exit_blender_exec;
}

/* *********************** */

#if defined(WIN32)

static int wm_console_toggle_exec(bContext *UNUSED(C), wmOperator *UNUSED(op))
{
	GHOST_toggleConsole(2);
	return OPERATOR_FINISHED;
}

static void WM_OT_console_toggle(wmOperatorType *ot)
{
	/* XXX Have to mark these for xgettext, as under linux they do not exists... */
	ot->name = CTX_N_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Toggle System Console");
	ot->idname = "WM_OT_console_toggle";
	ot->description = N_("Toggle System Console");

	ot->exec = wm_console_toggle_exec;
	ot->poll = WM_operator_winactive;
}

#endif

/* ************ default paint cursors, draw always around cursor *********** */
/*
 * - returns handler to free
 * - poll(bContext): returns 1 if draw should happen
 * - draw(bContext): drawing callback for paint cursor
 */

void *WM_paint_cursor_activate(
        wmWindowManager *wm, bool (*poll)(bContext *C),
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

/* *********************** radial control ****************** */

#define WM_RADIAL_CONTROL_DISPLAY_SIZE (200 * UI_DPI_FAC)
#define WM_RADIAL_CONTROL_DISPLAY_MIN_SIZE (35 * UI_DPI_FAC)
#define WM_RADIAL_CONTROL_DISPLAY_WIDTH (WM_RADIAL_CONTROL_DISPLAY_SIZE - WM_RADIAL_CONTROL_DISPLAY_MIN_SIZE)
#define WM_RADIAL_MAX_STR 10

typedef struct {
	PropertyType type;
	PropertySubType subtype;
	PointerRNA ptr, col_ptr, fill_col_ptr, rot_ptr, zoom_ptr, image_id_ptr;
	PointerRNA fill_col_override_ptr, fill_col_override_test_ptr;
	PropertyRNA *prop, *col_prop, *fill_col_prop, *rot_prop, *zoom_prop;
	PropertyRNA *fill_col_override_prop, *fill_col_override_test_prop;
	StructRNA *image_id_srna;
	float initial_value, current_value, min_value, max_value;
	int initial_mouse[2];
	int slow_mouse[2];
	bool slow_mode;
	Dial *dial;
	unsigned int gltex;
	ListBase orig_paintcursors;
	bool use_secondary_tex;
	void *cursor;
	NumInput num_input;
} RadialControl;

static void radial_control_update_header(wmOperator *op, bContext *C)
{
	RadialControl *rc = op->customdata;
	char msg[UI_MAX_DRAW_STR];
	ScrArea *sa = CTX_wm_area(C);
	Scene *scene = CTX_data_scene(C);

	if (hasNumInput(&rc->num_input)) {
		char num_str[NUM_STR_REP_LEN];
		outputNumInput(&rc->num_input, num_str, &scene->unit);
		BLI_snprintf(msg, sizeof(msg), "%s: %s", RNA_property_ui_name(rc->prop), num_str);
	}
	else {
		const char *ui_name = RNA_property_ui_name(rc->prop);
		switch (rc->subtype) {
			case PROP_NONE:
			case PROP_DISTANCE:
				BLI_snprintf(msg, sizeof(msg), "%s: %0.4f", ui_name, rc->current_value);
				break;
			case PROP_PIXEL:
				BLI_snprintf(msg, sizeof(msg), "%s: %d", ui_name, (int)rc->current_value); /* XXX: round to nearest? */
				break;
			case PROP_PERCENTAGE:
				BLI_snprintf(msg, sizeof(msg), "%s: %3.1f%%", ui_name, rc->current_value);
				break;
			case PROP_FACTOR:
				BLI_snprintf(msg, sizeof(msg), "%s: %1.3f", ui_name, rc->current_value);
				break;
			case PROP_ANGLE:
				BLI_snprintf(msg, sizeof(msg), "%s: %3.2f", ui_name, RAD2DEGF(rc->current_value));
				break;
			default:
				BLI_snprintf(msg, sizeof(msg), "%s", ui_name); /* XXX: No value? */
				break;
		}
	}

	ED_area_status_text(sa, msg);
}

static void radial_control_set_initial_mouse(RadialControl *rc, const wmEvent *event)
{
	float d[2] = {0, 0};
	float zoom[2] = {1, 1};

	rc->initial_mouse[0] = event->x;
	rc->initial_mouse[1] = event->y;

	switch (rc->subtype) {
		case PROP_NONE:
		case PROP_DISTANCE:
		case PROP_PIXEL:
			d[0] = rc->initial_value;
			break;
		case PROP_PERCENTAGE:
			d[0] = (rc->initial_value) / 100.0f * WM_RADIAL_CONTROL_DISPLAY_WIDTH + WM_RADIAL_CONTROL_DISPLAY_MIN_SIZE;
			break;
		case PROP_FACTOR:
			d[0] = (1 - rc->initial_value) * WM_RADIAL_CONTROL_DISPLAY_WIDTH + WM_RADIAL_CONTROL_DISPLAY_MIN_SIZE;
			break;
		case PROP_ANGLE:
			d[0] = WM_RADIAL_CONTROL_DISPLAY_SIZE * cosf(rc->initial_value);
			d[1] = WM_RADIAL_CONTROL_DISPLAY_SIZE * sinf(rc->initial_value);
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
			if ((ibuf = BKE_brush_gen_radial_control_imbuf(rc->image_id_ptr.data, rc->use_secondary_tex))) {
				glGenTextures(1, &rc->gltex);
				glBindTexture(GL_TEXTURE_2D, rc->gltex);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, ibuf->x, ibuf->y, 0,
				             GL_RED, GL_FLOAT, ibuf->rect_float);
				glBindTexture(GL_TEXTURE_2D, 0);
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
	if (rc->fill_col_prop) {
		PointerRNA *fill_ptr;
		PropertyRNA *fill_prop;

		if (rc->fill_col_override_prop &&
		    RNA_property_boolean_get(&rc->fill_col_override_test_ptr, rc->fill_col_override_test_prop))
		{
			fill_ptr = &rc->fill_col_override_ptr;
			fill_prop = rc->fill_col_override_prop;
		}
		else {
			fill_ptr = &rc->fill_col_ptr;
			fill_prop = rc->fill_col_prop;
		}

		RNA_property_float_get_array(fill_ptr, fill_prop, col);
	}

	GPUVertFormat *format = immVertexFormat();
	uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

	if (rc->gltex) {

		uint texCoord = GPU_vertformat_attr_add(format, "texCoord", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, rc->gltex);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		GLint swizzleMask[] = {GL_ZERO, GL_ZERO, GL_ZERO, GL_RED};
		glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);

		immBindBuiltinProgram(GPU_SHADER_2D_IMAGE_MASK_UNIFORM_COLOR);

		immUniformColor3fvAlpha(col, alpha);
		immUniform1i("image", 0);

		/* set up rotation if available */
		if (rc->rot_prop) {
			rot = RNA_property_float_get(&rc->rot_ptr, rc->rot_prop);
			GPU_matrix_push();
			GPU_matrix_rotate_2d(RAD2DEGF(rot));
		}

		/* draw textured quad */
		immBegin(GPU_PRIM_TRI_FAN, 4);

		immAttrib2f(texCoord, 0, 0);
		immVertex2f(pos, -radius, -radius);

		immAttrib2f(texCoord, 1, 0);
		immVertex2f(pos, radius, -radius);

		immAttrib2f(texCoord, 1, 1);
		immVertex2f(pos, radius, radius);

		immAttrib2f(texCoord, 0, 1);
		immVertex2f(pos, -radius, radius);

		immEnd();

		/* undo rotation */
		if (rc->rot_prop)
			GPU_matrix_pop();
	}
	else {
		/* flat color if no texture available */
		immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
		immUniformColor3fvAlpha(col, alpha);
		imm_draw_circle_fill_2d(pos, 0.0f, 0.0f, radius, 40);
	}

	immUnbindProgram();
}

static void radial_control_paint_cursor(bContext *UNUSED(C), int x, int y, void *customdata)
{
	RadialControl *rc = customdata;
	uiStyle *style = UI_style_get();
	const uiFontStyle *fstyle = &style->widget;
	const int fontid = fstyle->uifont_id;
	short fstyle_points = fstyle->points;
	char str[WM_RADIAL_MAX_STR];
	short strdrawlen = 0;
	float strwidth, strheight;
	float r1 = 0.0f, r2 = 0.0f, rmin = 0.0, tex_radius, alpha;
	float zoom[2], col[3] = {1, 1, 1};

	switch (rc->subtype) {
		case PROP_NONE:
		case PROP_DISTANCE:
		case PROP_PIXEL:
			r1 = rc->current_value;
			r2 = rc->initial_value;
			tex_radius = r1;
			alpha = 0.75;
			break;
		case PROP_PERCENTAGE:
			r1 = rc->current_value / 100.0f * WM_RADIAL_CONTROL_DISPLAY_WIDTH + WM_RADIAL_CONTROL_DISPLAY_MIN_SIZE;
			r2 = tex_radius = WM_RADIAL_CONTROL_DISPLAY_SIZE;
			rmin = WM_RADIAL_CONTROL_DISPLAY_MIN_SIZE;
			BLI_snprintf(str, WM_RADIAL_MAX_STR, "%3.1f%%", rc->current_value);
			strdrawlen = BLI_strlen_utf8(str);
			tex_radius = r1;
			alpha = 0.75;
			break;
		case PROP_FACTOR:
			r1 = (1 - rc->current_value) * WM_RADIAL_CONTROL_DISPLAY_WIDTH + WM_RADIAL_CONTROL_DISPLAY_MIN_SIZE;
			r2 = tex_radius = WM_RADIAL_CONTROL_DISPLAY_SIZE;
			rmin = WM_RADIAL_CONTROL_DISPLAY_MIN_SIZE;
			alpha = rc->current_value / 2.0f + 0.5f;
			BLI_snprintf(str, WM_RADIAL_MAX_STR, "%1.3f", rc->current_value);
			strdrawlen = BLI_strlen_utf8(str);
			break;
		case PROP_ANGLE:
			r1 = r2 = tex_radius = WM_RADIAL_CONTROL_DISPLAY_SIZE;
			alpha = 0.75;
			rmin = WM_RADIAL_CONTROL_DISPLAY_MIN_SIZE;
			BLI_snprintf(str, WM_RADIAL_MAX_STR, "%3.2f", RAD2DEGF(rc->current_value));
			strdrawlen = BLI_strlen_utf8(str);
			break;
		default:
			tex_radius = WM_RADIAL_CONTROL_DISPLAY_SIZE; /* note, this is a dummy value */
			alpha = 0.75;
			break;
	}

	/* Keep cursor in the original place */
	x = rc->initial_mouse[0];
	y = rc->initial_mouse[1];
	GPU_matrix_translate_2f((float)x, (float)y);

	glEnable(GL_BLEND);
	glEnable(GL_LINE_SMOOTH);

	/* apply zoom if available */
	if (rc->zoom_prop) {
		RNA_property_float_get_array(&rc->zoom_ptr, rc->zoom_prop, zoom);
		GPU_matrix_scale_2fv(zoom);
	}

	/* draw rotated texture */
	radial_control_paint_tex(rc, tex_radius, alpha);

	/* set line color */
	if (rc->col_prop)
		RNA_property_float_get_array(&rc->col_ptr, rc->col_prop, col);

	GPUVertFormat *format = immVertexFormat();
	uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
	immUniformColor3fvAlpha(col, 0.5f);

	if (rc->subtype == PROP_ANGLE) {
		GPU_matrix_push();

		/* draw original angle line */
		GPU_matrix_rotate_2d(RAD2DEGF(rc->initial_value));
		immBegin(GPU_PRIM_LINES, 2);
		immVertex2f(pos, (float)WM_RADIAL_CONTROL_DISPLAY_MIN_SIZE, 0.0f);
		immVertex2f(pos, (float)WM_RADIAL_CONTROL_DISPLAY_SIZE, 0.0f);
		immEnd();

		/* draw new angle line */
		GPU_matrix_rotate_2d(RAD2DEGF(rc->current_value - rc->initial_value));
		immBegin(GPU_PRIM_LINES, 2);
		immVertex2f(pos, (float)WM_RADIAL_CONTROL_DISPLAY_MIN_SIZE, 0.0f);
		immVertex2f(pos, (float)WM_RADIAL_CONTROL_DISPLAY_SIZE, 0.0f);
		immEnd();

		GPU_matrix_pop();
	}

	/* draw circles on top */
	imm_draw_circle_wire_2d(pos, 0.0f, 0.0f, r1, 40);
	imm_draw_circle_wire_2d(pos, 0.0f, 0.0f, r2, 40);
	if (rmin > 0.0f)
		imm_draw_circle_wire_2d(pos, 0.0, 0.0f, rmin, 40);
	immUnbindProgram();

	BLF_size(fontid, 1.5 * fstyle_points * U.pixelsize, U.dpi);
	BLF_enable(fontid, BLF_SHADOW);
	BLF_shadow(fontid, 3, (const float[4]){0.0f, 0.0f, 0.0f, 0.5f});
	BLF_shadow_offset(fontid, 1, -1);

	/* draw value */
	BLF_width_and_height(fontid, str, strdrawlen, &strwidth, &strheight);
	BLF_position(fontid, -0.5f * strwidth, -0.5f * strheight, 0.0f);
	BLF_draw(fontid, str, strdrawlen);

	BLF_disable(fontid, BLF_SHADOW);

	glDisable(GL_BLEND);
	glDisable(GL_LINE_SMOOTH);

}

typedef enum {
	RC_PROP_ALLOW_MISSING = 1,
	RC_PROP_REQUIRE_FLOAT = 2,
	RC_PROP_REQUIRE_BOOL = 4,
} RCPropFlags;

/**
 * Attempt to retrieve the rna pointer/property from an rna path.
 *
 * \return 0 for failure, 1 for success, and also 1 if property is not set.
 */
static int radial_control_get_path(
        PointerRNA *ctx_ptr, wmOperator *op,
        const char *name, PointerRNA *r_ptr,
        PropertyRNA **r_prop, int req_length, RCPropFlags flags)
{
	PropertyRNA *unused_prop;
	int len;
	char *str;

	/* check flags */
	if ((flags & RC_PROP_REQUIRE_BOOL) && (flags & RC_PROP_REQUIRE_FLOAT)) {
		BKE_report(op->reports, RPT_ERROR, "Property cannot be both boolean and float");
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
			BKE_reportf(op->reports, RPT_ERROR, "Could not resolve path '%s'", name);
			return 0;
		}
	}

	/* check property type */
	if (flags & (RC_PROP_REQUIRE_BOOL | RC_PROP_REQUIRE_FLOAT)) {
		PropertyType prop_type = RNA_property_type(*r_prop);

		if (((flags & RC_PROP_REQUIRE_BOOL) && (prop_type != PROP_BOOLEAN)) ||
		    ((flags & RC_PROP_REQUIRE_FLOAT) && (prop_type != PROP_FLOAT)))
		{
			MEM_freeN(str);
			BKE_reportf(op->reports, RPT_ERROR, "Property from path '%s' is not a float", name);
			return 0;
		}
	}

	/* check property's array length */
	if (*r_prop && (len = RNA_property_array_length(r_ptr, *r_prop)) != req_length) {
		MEM_freeN(str);
		BKE_reportf(op->reports, RPT_ERROR, "Property from path '%s' has length %d instead of %d",
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
	PropertyRNA *use_secondary_prop = NULL;
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


	if (!radial_control_get_path(
	        &ctx_ptr, op, "fill_color_path", &rc->fill_col_ptr, &rc->fill_col_prop, 3, RC_PROP_REQUIRE_FLOAT))
	{
		return 0;
	}

	if (!radial_control_get_path(
	        &ctx_ptr, op, "fill_color_override_path",
	        &rc->fill_col_override_ptr, &rc->fill_col_override_prop, 3, RC_PROP_REQUIRE_FLOAT))
	{
		return 0;
	}
	if (!radial_control_get_path(
	        &ctx_ptr, op, "fill_color_override_test_path",
	        &rc->fill_col_override_test_ptr, &rc->fill_col_override_test_prop, 0, RC_PROP_REQUIRE_BOOL))
	{
		return 0;
	}

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
			BKE_report(op->reports, RPT_ERROR, "Pointer from path image_id is not an ID");
			return 0;
		}
	}

	rc->use_secondary_tex = RNA_boolean_get(op->ptr, "secondary_tex");

	return 1;
}

static int radial_control_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	wmWindowManager *wm;
	RadialControl *rc;


	if (!(op->customdata = rc = MEM_callocN(sizeof(RadialControl), "RadialControl")))
		return OPERATOR_CANCELLED;

	if (!radial_control_get_properties(C, op)) {
		MEM_freeN(rc);
		return OPERATOR_CANCELLED;
	}

	/* get type, initial, min, and max values of the property */
	switch ((rc->type = RNA_property_type(rc->prop))) {
		case PROP_INT:
		{
			int value, min, max, step;

			value = RNA_property_int_get(&rc->ptr, rc->prop);
			RNA_property_int_ui_range(&rc->ptr, rc->prop, &min, &max, &step);

			rc->initial_value = value;
			rc->min_value = min_ii(value, min);
			rc->max_value = max_ii(value, max);
			break;
		}
		case PROP_FLOAT:
		{
			float value, min, max, step, precision;

			value = RNA_property_float_get(&rc->ptr, rc->prop);
			RNA_property_float_ui_range(&rc->ptr, rc->prop, &min, &max, &step, &precision);

			rc->initial_value = value;
			rc->min_value = min_ff(value, min);
			rc->max_value = max_ff(value, max);
			break;
		}
		default:
			BKE_report(op->reports, RPT_ERROR, "Property must be an integer or a float");
			MEM_freeN(rc);
			return OPERATOR_CANCELLED;
	}

	/* initialize numerical input */
	initNumInput(&rc->num_input);
	rc->num_input.idx_max = 0;
	rc->num_input.val_flag[0] |= NUM_NO_NEGATIVE;
	rc->num_input.unit_sys = USER_UNIT_NONE;
	rc->num_input.unit_type[0] = B_UNIT_LENGTH;

	/* get subtype of property */
	rc->subtype = RNA_property_subtype(rc->prop);
	if (!ELEM(rc->subtype, PROP_NONE, PROP_DISTANCE, PROP_FACTOR, PROP_PERCENTAGE, PROP_ANGLE, PROP_PIXEL)) {
		BKE_report(op->reports, RPT_ERROR, "Property must be a none, distance, factor, percentage, angle, or pixel");
		MEM_freeN(rc);
		return OPERATOR_CANCELLED;
	}

	rc->current_value = rc->initial_value;
	radial_control_set_initial_mouse(rc, event);
	radial_control_set_tex(rc);

	/* temporarily disable other paint cursors */
	wm = CTX_wm_manager(C);
	rc->orig_paintcursors = wm->paintcursors;
	BLI_listbase_clear(&wm->paintcursors);

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

static void radial_control_cancel(bContext *C, wmOperator *op)
{
	RadialControl *rc = op->customdata;
	wmWindowManager *wm = CTX_wm_manager(C);
	ScrArea *sa = CTX_wm_area(C);

	if (rc->dial) {
		MEM_freeN(rc->dial);
		rc->dial = NULL;
	}

	ED_area_status_text(sa, NULL);

	WM_paint_cursor_end(wm, rc->cursor);

	/* restore original paint cursors */
	wm->paintcursors = rc->orig_paintcursors;

	/* not sure if this is a good notifier to use;
	 * intended purpose is to update the UI so that the
	 * new value is displayed in sliders/numfields */
	WM_event_add_notifier(C, NC_WINDOW, NULL);

	glDeleteTextures(1, &rc->gltex);

	MEM_freeN(rc);
}

static int radial_control_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	RadialControl *rc = op->customdata;
	float new_value, dist = 0.0f, zoom[2];
	float delta[2], ret = OPERATOR_RUNNING_MODAL;
	bool snap;
	float angle_precision = 0.0f;
	const bool has_numInput = hasNumInput(&rc->num_input);
	bool handled = false;
	float numValue;
	/* TODO: fix hardcoded events */

	snap = event->ctrl != 0;

	/* Modal numinput active, try to handle numeric inputs first... */
	if (event->val == KM_PRESS && has_numInput && handleNumInput(C, &rc->num_input, event)) {
		handled = true;
		applyNumInput(&rc->num_input, &numValue);

		if (rc->subtype == PROP_ANGLE) {
			numValue = DEG2RADF(numValue);
			numValue = fmod(numValue, 2.0f * (float)M_PI);
			if (numValue < 0.0f)
				numValue += 2.0f * (float)M_PI;
		}

		CLAMP(numValue, rc->min_value, rc->max_value);
		new_value = numValue;

		radial_control_set_value(rc, new_value);
		rc->current_value = new_value;
		radial_control_update_header(op, C);
		return OPERATOR_RUNNING_MODAL;
	}
	else {
		handled = false;
		switch (event->type) {
			case ESCKEY:
			case RIGHTMOUSE:
				/* canceled; restore original value */
				radial_control_set_value(rc, rc->initial_value);
				ret = OPERATOR_CANCELLED;
				break;

			case LEFTMOUSE:
			case PADENTER:
			case RETKEY:
				/* done; value already set */
				RNA_property_update(C, &rc->ptr, rc->prop);
				ret = OPERATOR_FINISHED;
				break;

			case MOUSEMOVE:
				if (!has_numInput) {
					if (rc->slow_mode) {
						if (rc->subtype == PROP_ANGLE) {
							float position[2] = {event->x, event->y};

							/* calculate the initial angle here first */
							delta[0] = rc->initial_mouse[0] - rc->slow_mouse[0];
							delta[1] = rc->initial_mouse[1] - rc->slow_mouse[1];

							/* precision angle gets calculated from dial and gets added later */
							angle_precision = -0.1f * BLI_dial_angle(rc->dial, position);
						}
						else {
							delta[0] = rc->initial_mouse[0] - rc->slow_mouse[0];
							delta[1] = rc->initial_mouse[1] - rc->slow_mouse[1];

							if (rc->zoom_prop) {
								RNA_property_float_get_array(&rc->zoom_ptr, rc->zoom_prop, zoom);
								delta[0] /= zoom[0];
								delta[1] /= zoom[1];
							}

							dist = len_v2(delta);

							delta[0] = event->x - rc->slow_mouse[0];
							delta[1] = event->y - rc->slow_mouse[1];

							if (rc->zoom_prop) {
								delta[0] /= zoom[0];
								delta[1] /= zoom[1];
							}

							dist = dist + 0.1f * (delta[0] + delta[1]);
						}
					}
					else {
						delta[0] = rc->initial_mouse[0] - event->x;
						delta[1] = rc->initial_mouse[1] - event->y;

						if (rc->zoom_prop) {
							RNA_property_float_get_array(&rc->zoom_ptr, rc->zoom_prop, zoom);
							delta[0] /= zoom[0];
							delta[1] /= zoom[1];
						}

						dist = len_v2(delta);
					}

					/* calculate new value and apply snapping  */
					switch (rc->subtype) {
						case PROP_NONE:
						case PROP_DISTANCE:
						case PROP_PIXEL:
							new_value = dist;
							if (snap) new_value = ((int)new_value + 5) / 10 * 10;
							break;
						case PROP_PERCENTAGE:
							new_value = ((dist - WM_RADIAL_CONTROL_DISPLAY_MIN_SIZE) / WM_RADIAL_CONTROL_DISPLAY_WIDTH) * 100.0f;
							if (snap) new_value = ((int)(new_value + 2.5f)) / 5 * 5;
							break;
						case PROP_FACTOR:
							new_value = (WM_RADIAL_CONTROL_DISPLAY_SIZE - dist) / WM_RADIAL_CONTROL_DISPLAY_WIDTH;
							if (snap) new_value = ((int)ceil(new_value * 10.f) * 10.0f) / 100.f;
							break;
						case PROP_ANGLE:
							new_value = atan2f(delta[1], delta[0]) + (float)M_PI + angle_precision;
							new_value = fmod(new_value, 2.0f * (float)M_PI);
							if (new_value < 0.0f)
								new_value += 2.0f * (float)M_PI;
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
					handled = true;
					break;
				}
				break;

			case LEFTSHIFTKEY:
			case RIGHTSHIFTKEY:
			{
				if (event->val == KM_PRESS) {
					rc->slow_mouse[0] = event->x;
					rc->slow_mouse[1] = event->y;
					rc->slow_mode = true;
					if (rc->subtype == PROP_ANGLE) {
						float initial_position[2] = {UNPACK2(rc->initial_mouse)};
						float current_position[2] = {UNPACK2(rc->slow_mouse)};
						rc->dial = BLI_dial_initialize(initial_position, 0.0f);
						/* immediately set the position to get a an initial direction */
						BLI_dial_angle(rc->dial, current_position);
					}
					handled = true;
				}
				if (event->val == KM_RELEASE) {
					rc->slow_mode = false;
					handled = true;
					if (rc->dial) {
						MEM_freeN(rc->dial);
						rc->dial = NULL;
					}
				}
				break;
			}
		}

		/* Modal numinput inactive, try to handle numeric inputs last... */
		if (!handled && event->val == KM_PRESS && handleNumInput(C, &rc->num_input, event)) {
			applyNumInput(&rc->num_input, &numValue);

			if (rc->subtype == PROP_ANGLE) {
				numValue = DEG2RADF(numValue);
				numValue = fmod(numValue, 2.0f * (float)M_PI);
				if (numValue < 0.0f)
					numValue += 2.0f * (float)M_PI;
			}

			CLAMP(numValue, rc->min_value, rc->max_value);
			new_value = numValue;

			radial_control_set_value(rc, new_value);

			rc->current_value = new_value;
			radial_control_update_header(op, C);
			return OPERATOR_RUNNING_MODAL;
		}
	}

	ED_region_tag_redraw(CTX_wm_region(C));
	radial_control_update_header(op, C);

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
	PropertyRNA *prop;
	prop = RNA_def_string(ot->srna, "data_path_primary", NULL, 0, "Primary Data Path", "Primary path of property to be set by the radial control");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);

	prop = RNA_def_string(ot->srna, "data_path_secondary", NULL, 0, "Secondary Data Path", "Secondary path of property to be set by the radial control");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);

	prop = RNA_def_string(ot->srna, "use_secondary", NULL, 0, "Use Secondary", "Path of property to select between the primary and secondary data paths");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);

	prop = RNA_def_string(ot->srna, "rotation_path", NULL, 0, "Rotation Path", "Path of property used to rotate the texture display");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);

	prop = RNA_def_string(ot->srna, "color_path", NULL, 0, "Color Path", "Path of property used to set the color of the control");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);

	prop = RNA_def_string(ot->srna, "fill_color_path", NULL, 0, "Fill Color Path", "Path of property used to set the fill color of the control");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);

	prop = RNA_def_string(ot->srna, "fill_color_override_path", NULL, 0, "Fill Color Override Path", "");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	prop = RNA_def_string(ot->srna, "fill_color_override_test_path", NULL, 0, "Fill Color Override Test", "");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);

	prop = RNA_def_string(ot->srna, "zoom_path", NULL, 0, "Zoom Path", "Path of property used to set the zoom level for the control");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);

	prop = RNA_def_string(ot->srna, "image_id", NULL, 0, "Image ID", "Path of ID that is used to generate an image for the control");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);

	prop = RNA_def_boolean(ot->srna, "secondary_tex", false, "Secondary Texture", "Tweak brush secondary/mask texture");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/* ************************** timer for testing ***************** */

/* uses no type defines, fully local testing function anyway... ;) */

static void redraw_timer_window_swap(bContext *C)
{
	wmWindow *win = CTX_wm_window(C);
	ScrArea *sa;
	CTX_wm_menu_set(C, NULL);

	for (sa = CTX_wm_screen(C)->areabase.first; sa; sa = sa->next)
		ED_area_tag_redraw(sa);
	wm_draw_update(C);

	CTX_wm_window_set(C, win);  /* XXX context manipulation warning! */
}

enum {
	eRTDrawRegion = 0,
	eRTDrawRegionSwap = 1,
	eRTDrawWindow = 2,
	eRTDrawWindowSwap = 3,
	eRTAnimationStep = 4,
	eRTAnimationPlay = 5,
	eRTUndo = 6,
};

static const EnumPropertyItem redraw_timer_type_items[] = {
	{eRTDrawRegion, "DRAW", 0, "Draw Region", "Draw Region"},
	{eRTDrawRegionSwap, "DRAW_SWAP", 0, "Draw Region + Swap", "Draw Region and Swap"},
	{eRTDrawWindow, "DRAW_WIN", 0, "Draw Window", "Draw Window"},
	{eRTDrawWindowSwap, "DRAW_WIN_SWAP", 0, "Draw Window + Swap", "Draw Window and Swap"},
	{eRTAnimationStep, "ANIM_STEP", 0, "Anim Step", "Animation Steps"},
	{eRTAnimationPlay, "ANIM_PLAY", 0, "Anim Play", "Animation Playback"},
	{eRTUndo, "UNDO", 0, "Undo/Redo", "Undo/Redo"},
	{0, NULL, 0, NULL, NULL}
};


static void redraw_timer_step(
        bContext *C, Main *bmain, Scene *scene,
        struct Depsgraph *depsgraph,
        wmWindow *win, ScrArea *sa, ARegion *ar,
        const int type, const int cfra)
{
	if (type == eRTDrawRegion) {
		if (ar) {
			ED_region_do_draw(C, ar);
			ar->do_draw = false;
		}
	}
	else if (type == eRTDrawRegionSwap) {
		CTX_wm_menu_set(C, NULL);

		ED_region_tag_redraw(ar);
		wm_draw_update(C);

		CTX_wm_window_set(C, win);  /* XXX context manipulation warning! */
	}
	else if (type == eRTDrawWindow) {
		bScreen *screen = WM_window_get_active_screen(win);
		ScrArea *sa_iter;

		CTX_wm_menu_set(C, NULL);

		for (sa_iter = screen->areabase.first; sa_iter; sa_iter = sa_iter->next) {
			ARegion *ar_iter;
			CTX_wm_area_set(C, sa_iter);

			for (ar_iter = sa_iter->regionbase.first; ar_iter; ar_iter = ar_iter->next) {
				if (ar_iter->visible) {
					CTX_wm_region_set(C, ar_iter);
					ED_region_do_draw(C, ar_iter);
					ar_iter->do_draw = false;
				}
			}
		}

		CTX_wm_window_set(C, win);  /* XXX context manipulation warning! */

		CTX_wm_area_set(C, sa);
		CTX_wm_region_set(C, ar);
	}
	else if (type == eRTDrawWindowSwap) {
		redraw_timer_window_swap(C);
	}
	else if (type == eRTAnimationStep) {
		scene->r.cfra += (cfra == scene->r.cfra) ? 1 : -1;
		BKE_scene_graph_update_for_newframe(depsgraph, bmain);
	}
	else if (type == eRTAnimationPlay) {
		/* play anim, return on same frame as started with */
		int tot = (scene->r.efra - scene->r.sfra) + 1;

		while (tot--) {
			/* todo, ability to escape! */
			scene->r.cfra++;
			if (scene->r.cfra > scene->r.efra)
				scene->r.cfra = scene->r.sfra;

			BKE_scene_graph_update_for_newframe(depsgraph, bmain);
			redraw_timer_window_swap(C);
		}
	}
	else { /* eRTUndo */
		ED_undo_pop(C);
		ED_undo_redo(C);
	}
}

static int redraw_timer_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	wmWindow *win = CTX_wm_window(C);
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);
	double time_start, time_delta;
	const int type = RNA_enum_get(op->ptr, "type");
	const int iter = RNA_int_get(op->ptr, "iterations");
	const double time_limit = (double)RNA_float_get(op->ptr, "time_limit");
	const int cfra = scene->r.cfra;
	int a, iter_steps = 0;
	const char *infostr = "";
	struct Depsgraph *depsgraph = CTX_data_depsgraph(C);

	WM_cursor_wait(1);

	time_start = PIL_check_seconds_timer();

	for (a = 0; a < iter; a++) {
		redraw_timer_step(C, bmain, scene, depsgraph, win, sa, ar, type, cfra);
		iter_steps += 1;

		if (time_limit != 0.0) {
			if ((PIL_check_seconds_timer() - time_start) > time_limit) {
				break;
			}
			a = 0;
		}
	}

	time_delta = (PIL_check_seconds_timer() - time_start) * 1000;

	RNA_enum_description(redraw_timer_type_items, type, &infostr);

	WM_cursor_wait(0);

	BKE_reportf(op->reports, RPT_WARNING,
	            "%d x %s: %.4f ms, average: %.8f ms",
	            iter_steps, infostr, time_delta, time_delta / iter_steps);

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

	ot->prop = RNA_def_enum(ot->srna, "type", redraw_timer_type_items, eRTDrawRegion, "Type", "");
	RNA_def_int(ot->srna, "iterations", 10, 1, INT_MAX, "Iterations", "Number of times to redraw", 1, 1000);
	RNA_def_float(ot->srna, "time_limit", 0.0, 0.0, FLT_MAX,
	              "Time Limit", "Seconds to run the test for (override iterations)", 0.0, 60.0);

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

/* *************************** Mat/tex/etc. previews generation ************* */

typedef struct PreviewsIDEnsureData {
	bContext *C;
	Scene *scene;
} PreviewsIDEnsureData;

static void previews_id_ensure(bContext *C, Scene *scene, ID *id)
{
	BLI_assert(ELEM(GS(id->name), ID_MA, ID_TE, ID_IM, ID_WO, ID_LA));

	/* Only preview non-library datablocks, lib ones do not pertain to this .blend file!
	 * Same goes for ID with no user. */
	if (!ID_IS_LINKED(id) && (id->us != 0)) {
		UI_id_icon_render(C, scene, id, false, false);
		UI_id_icon_render(C, scene, id, true, false);
	}
}

static int previews_id_ensure_callback(void *userdata, ID *UNUSED(self_id), ID **idptr, int cb_flag)
{
	if (cb_flag & IDWALK_CB_PRIVATE) {
		return IDWALK_RET_NOP;
	}

	PreviewsIDEnsureData *data = userdata;
	ID *id = *idptr;

	if (id && (id->tag & LIB_TAG_DOIT)) {
		BLI_assert(ELEM(GS(id->name), ID_MA, ID_TE, ID_IM, ID_WO, ID_LA));
		previews_id_ensure(data->C, data->scene, id);
		id->tag &= ~LIB_TAG_DOIT;
	}

	return IDWALK_RET_NOP;
}

static int previews_ensure_exec(bContext *C, wmOperator *UNUSED(op))
{
	Main *bmain = CTX_data_main(C);
	ListBase *lb[] = {&bmain->mat, &bmain->tex, &bmain->image, &bmain->world, &bmain->lamp, NULL};
	PreviewsIDEnsureData preview_id_data;
	Scene *scene;
	ID *id;
	int i;

	/* We use LIB_TAG_DOIT to check whether we have already handled a given ID or not. */
	BKE_main_id_tag_all(bmain, LIB_TAG_DOIT, false);
	for (i = 0; lb[i]; i++) {
		BKE_main_id_tag_listbase(lb[i], LIB_TAG_DOIT, true);
	}

	preview_id_data.C = C;
	for (scene = bmain->scene.first; scene; scene = scene->id.next) {
		preview_id_data.scene = scene;
		id = (ID *)scene;

		BKE_library_foreach_ID_link(NULL, id, previews_id_ensure_callback, &preview_id_data, IDWALK_RECURSE);
	}

	/* Check a last time for ID not used (fake users only, in theory), and
	 * do our best for those, using current scene... */
	for (i = 0; lb[i]; i++) {
		for (id = lb[i]->first; id; id = id->next) {
			if (id->tag & LIB_TAG_DOIT) {
				previews_id_ensure(C, NULL, id);
				id->tag &= ~LIB_TAG_DOIT;
			}
		}
	}

	return OPERATOR_FINISHED;
}

static void WM_OT_previews_ensure(wmOperatorType *ot)
{
	ot->name = "Refresh Data-Block Previews";
	ot->idname = "WM_OT_previews_ensure";
	ot->description = "Ensure data-block previews are available and up-to-date "
	                  "(to be saved in .blend file, only for some types like materials, textures, etc.)";

	ot->exec = previews_ensure_exec;
}

/* *************************** Datablocks previews clear ************* */

/* Only types supporting previews currently. */
static const EnumPropertyItem preview_id_type_items[] = {
    {FILTER_ID_SCE, "SCENE", 0, "Scenes", ""},
    {FILTER_ID_GR, "GROUP", 0, "Groups", ""},
    {FILTER_ID_OB, "OBJECT", 0, "Objects", ""},
    {FILTER_ID_MA, "MATERIAL", 0, "Materials", ""},
    {FILTER_ID_LA, "LIGHT", 0, "Lights", ""},
    {FILTER_ID_WO, "WORLD", 0, "Worlds", ""},
    {FILTER_ID_TE, "TEXTURE", 0, "Textures", ""},
    {FILTER_ID_IM, "IMAGE", 0, "Images", ""},
#if 0  /* XXX TODO */
    {FILTER_ID_BR, "BRUSH", 0, "Brushes", ""},
#endif
    {0, NULL, 0, NULL, NULL}
};

static int previews_clear_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	ListBase *lb[] = {&bmain->object, &bmain->collection,
	                  &bmain->mat, &bmain->world, &bmain->lamp, &bmain->tex, &bmain->image, NULL};
	int i;

	const int id_filters = RNA_enum_get(op->ptr, "id_type");

	for (i = 0; lb[i]; i++) {
		ID *id = lb[i]->first;

		if (!id) continue;

//		printf("%s: %d, %d, %d -> %d\n", id->name, GS(id->name), BKE_idcode_to_idfilter(GS(id->name)),
//		                                 id_filters, BKE_idcode_to_idfilter(GS(id->name)) & id_filters);

		if (!id || !(BKE_idcode_to_idfilter(GS(id->name)) & id_filters)) {
			continue;
		}

		for (; id; id = id->next) {
			PreviewImage *prv_img = BKE_previewimg_id_ensure(id);

			BKE_previewimg_clear(prv_img);
		}
	}

	return OPERATOR_FINISHED;
}

static void WM_OT_previews_clear(wmOperatorType *ot)
{
	ot->name = "Clear Data-Block Previews";
	ot->idname = "WM_OT_previews_clear";
	ot->description = "Clear data-block previews (only for some types like objects, materials, textures, etc.)";

	ot->exec = previews_clear_exec;
	ot->invoke = WM_menu_invoke;

	ot->prop = RNA_def_enum_flag(ot->srna, "id_type", preview_id_type_items,
	                             FILTER_ID_SCE | FILTER_ID_OB | FILTER_ID_GR |
	                             FILTER_ID_MA | FILTER_ID_LA | FILTER_ID_WO | FILTER_ID_TE | FILTER_ID_IM,
	                             "Data-Block Type", "Which data-block previews to clear");
}

/* *************************** Doc from UI ************* */

static int doc_view_manual_ui_context_exec(bContext *C, wmOperator *UNUSED(op))
{
	PointerRNA ptr_props;
	char buf[512];
	short retval = OPERATOR_CANCELLED;

	if (UI_but_online_manual_id_from_active(C, buf, sizeof(buf))) {
		WM_operator_properties_create(&ptr_props, "WM_OT_doc_view_manual");
		RNA_string_set(&ptr_props, "doc_id", buf);

		retval = WM_operator_name_call_ptr(
		        C, WM_operatortype_find("WM_OT_doc_view_manual", false),
		        WM_OP_EXEC_DEFAULT, &ptr_props);

		WM_operator_properties_free(&ptr_props);
	}

	return retval;
}

static void WM_OT_doc_view_manual_ui_context(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "View Online Manual";
	ot->idname = "WM_OT_doc_view_manual_ui_context";
	ot->description = "View a context based online manual in a web browser";

	/* callbacks */
	ot->poll = ED_operator_regionactive;
	ot->exec = doc_view_manual_ui_context_exec;
}

/* ******************************************************* */
/* toggle 3D for current window, turning it fullscreen if needed */
static void WM_OT_stereo3d_set(wmOperatorType *ot)
{
	PropertyRNA *prop;

	ot->name = "Set Stereo 3D";
	ot->idname = "WM_OT_set_stereo_3d";
	ot->description = "Toggle 3D stereo support for current window (or change the display mode)";

	ot->exec = wm_stereo3d_set_exec;
	ot->invoke = wm_stereo3d_set_invoke;
	ot->poll = WM_operator_winactive;
	ot->ui = wm_stereo3d_set_draw;
	ot->check = wm_stereo3d_set_check;
	ot->cancel = wm_stereo3d_set_cancel;

	prop = RNA_def_enum(ot->srna, "display_mode", rna_enum_stereo3d_display_items, S3D_DISPLAY_ANAGLYPH, "Display Mode", "");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	prop = RNA_def_enum(ot->srna, "anaglyph_type", rna_enum_stereo3d_anaglyph_type_items, S3D_ANAGLYPH_REDCYAN, "Anaglyph Type", "");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	prop = RNA_def_enum(ot->srna, "interlace_type", rna_enum_stereo3d_interlace_type_items, S3D_INTERLACE_ROW, "Interlace Type", "");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	prop = RNA_def_boolean(ot->srna, "use_interlace_swap", false, "Swap Left/Right",
	                       "Swap left and right stereo channels");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	prop = RNA_def_boolean(ot->srna, "use_sidebyside_crosseyed", false, "Cross-Eyed",
	                       "Right eye should see left image and vice-versa");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

void wm_operatortypes_register(void)
{
	WM_operatortype_append(WM_OT_window_close);
	WM_operatortype_append(WM_OT_window_new);
	WM_operatortype_append(WM_OT_window_new_main);
	WM_operatortype_append(WM_OT_read_history);
	WM_operatortype_append(WM_OT_read_homefile);
	WM_operatortype_append(WM_OT_read_factory_settings);
	WM_operatortype_append(WM_OT_save_homefile);
	WM_operatortype_append(WM_OT_save_userpref);
	WM_operatortype_append(WM_OT_save_workspace_file);
	WM_operatortype_append(WM_OT_userpref_autoexec_path_add);
	WM_operatortype_append(WM_OT_userpref_autoexec_path_remove);
	WM_operatortype_append(WM_OT_window_fullscreen_toggle);
	WM_operatortype_append(WM_OT_quit_blender);
	WM_operatortype_append(WM_OT_open_mainfile);
	WM_operatortype_append(WM_OT_revert_mainfile);
	WM_operatortype_append(WM_OT_link);
	WM_operatortype_append(WM_OT_append);
	WM_operatortype_append(WM_OT_lib_relocate);
	WM_operatortype_append(WM_OT_lib_reload);
	WM_operatortype_append(WM_OT_recover_last_session);
	WM_operatortype_append(WM_OT_recover_auto_save);
	WM_operatortype_append(WM_OT_save_as_mainfile);
	WM_operatortype_append(WM_OT_save_mainfile);
	WM_operatortype_append(WM_OT_redraw_timer);
	WM_operatortype_append(WM_OT_memory_statistics);
	WM_operatortype_append(WM_OT_debug_menu);
	WM_operatortype_append(WM_OT_operator_defaults);
	WM_operatortype_append(WM_OT_splash);
	WM_operatortype_append(WM_OT_search_menu);
	WM_operatortype_append(WM_OT_call_menu);
	WM_operatortype_append(WM_OT_call_menu_pie);
	WM_operatortype_append(WM_OT_call_panel);
	WM_operatortype_append(WM_OT_radial_control);
	WM_operatortype_append(WM_OT_stereo3d_set);
#if defined(WIN32)
	WM_operatortype_append(WM_OT_console_toggle);
#endif
	WM_operatortype_append(WM_OT_previews_ensure);
	WM_operatortype_append(WM_OT_previews_clear);
	WM_operatortype_append(WM_OT_doc_view_manual_ui_context);

	/* gizmos */
	WM_operatortype_append(GIZMOGROUP_OT_gizmo_select);
	WM_operatortype_append(GIZMOGROUP_OT_gizmo_tweak);
}

/* circleselect-like modal operators */
static void gesture_circle_modal_keymap(wmKeyConfig *keyconf)
{
	static const EnumPropertyItem modal_items[] = {
		{GESTURE_MODAL_CANCEL,  "CANCEL", 0, "Cancel", ""},
		{GESTURE_MODAL_CONFIRM, "CONFIRM", 0, "Confirm", ""},
		{GESTURE_MODAL_CIRCLE_ADD, "ADD", 0, "Add", ""},
		{GESTURE_MODAL_CIRCLE_SUB, "SUBTRACT", 0, "Subtract", ""},
		{GESTURE_MODAL_CIRCLE_SIZE, "SIZE", 0, "Size", ""},

		{GESTURE_MODAL_SELECT,  "SELECT", 0, "Select", ""},
		{GESTURE_MODAL_DESELECT, "DESELECT", 0, "DeSelect", ""},
		{GESTURE_MODAL_NOP, "NOP", 0, "No Operation", ""},

		{0, NULL, 0, NULL, NULL}
	};

	/* WARNING - name is incorrect, use for non-3d views */
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

	/* Note: use 'KM_ANY' for release, so the circle exits on any mouse release,
	 * this is needed when circle select is activated as a tool. */

	/* left mouse shift for deselect too */
	WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_PRESS, KM_SHIFT, 0, GESTURE_MODAL_DESELECT);
	WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_RELEASE, KM_ANY, 0, GESTURE_MODAL_NOP);

	WM_modalkeymap_add_item(keymap, MIDDLEMOUSE, KM_PRESS, 0, 0, GESTURE_MODAL_DESELECT); //  default 2.4x
	WM_modalkeymap_add_item(keymap, MIDDLEMOUSE, KM_RELEASE, KM_ANY, 0, GESTURE_MODAL_NOP); //  default 2.4x

	WM_modalkeymap_add_item(keymap, WHEELUPMOUSE, KM_PRESS, 0, 0, GESTURE_MODAL_CIRCLE_SUB);
	WM_modalkeymap_add_item(keymap, PADMINUS, KM_PRESS, 0, 0, GESTURE_MODAL_CIRCLE_SUB);
	WM_modalkeymap_add_item(keymap, WHEELDOWNMOUSE, KM_PRESS, 0, 0, GESTURE_MODAL_CIRCLE_ADD);
	WM_modalkeymap_add_item(keymap, PADPLUSKEY, KM_PRESS, 0, 0, GESTURE_MODAL_CIRCLE_ADD);
	WM_modalkeymap_add_item(keymap, MOUSEPAN, 0, 0, 0, GESTURE_MODAL_CIRCLE_SIZE);

	/* assign map to operators */
	WM_modalkeymap_assign(keymap, "VIEW3D_OT_select_circle");
	WM_modalkeymap_assign(keymap, "UV_OT_circle_select");
	WM_modalkeymap_assign(keymap, "CLIP_OT_select_circle");
	WM_modalkeymap_assign(keymap, "MASK_OT_select_circle");
	WM_modalkeymap_assign(keymap, "NODE_OT_select_circle");
	WM_modalkeymap_assign(keymap, "GPENCIL_OT_select_circle");
	WM_modalkeymap_assign(keymap, "GRAPH_OT_select_circle");
	WM_modalkeymap_assign(keymap, "ACTION_OT_select_circle");

}

/* straight line modal operators */
static void gesture_straightline_modal_keymap(wmKeyConfig *keyconf)
{
	static const EnumPropertyItem modal_items[] = {
		{GESTURE_MODAL_CANCEL,  "CANCEL", 0, "Cancel", ""},
		{GESTURE_MODAL_SELECT,  "SELECT", 0, "Select", ""},
		{GESTURE_MODAL_BEGIN,   "BEGIN", 0, "Begin", ""},
		{0, NULL, 0, NULL, NULL}
	};

	wmKeyMap *keymap = WM_modalkeymap_get(keyconf, "Gesture Straight Line");

	/* this function is called for each spacetype, only needs to add map once */
	if (keymap && keymap->modal_items) return;

	keymap = WM_modalkeymap_add(keyconf, "Gesture Straight Line", modal_items);

	/* items for modal map */
	WM_modalkeymap_add_item(keymap, ESCKEY,    KM_PRESS, KM_ANY, 0, GESTURE_MODAL_CANCEL);
	WM_modalkeymap_add_item(keymap, RIGHTMOUSE, KM_ANY, KM_ANY, 0, GESTURE_MODAL_CANCEL);

	WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_PRESS, 0, 0, GESTURE_MODAL_BEGIN);
	WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_RELEASE, KM_ANY, 0, GESTURE_MODAL_SELECT);

	/* assign map to operators */
	WM_modalkeymap_assign(keymap, "IMAGE_OT_sample_line");
	WM_modalkeymap_assign(keymap, "PAINT_OT_weight_gradient");
	WM_modalkeymap_assign(keymap, "MESH_OT_bisect");
}


/* borderselect-like modal operators */
static void gesture_border_modal_keymap(wmKeyConfig *keyconf)
{
	static const EnumPropertyItem modal_items[] = {
		{GESTURE_MODAL_CANCEL,  "CANCEL", 0, "Cancel", ""},
		{GESTURE_MODAL_SELECT,  "SELECT", 0, "Select", ""},
		{GESTURE_MODAL_DESELECT, "DESELECT", 0, "DeSelect", ""},
		{GESTURE_MODAL_BEGIN,   "BEGIN", 0, "Begin", ""},
		{0, NULL, 0, NULL, NULL}
	};

	wmKeyMap *keymap = WM_modalkeymap_get(keyconf, "Gesture Border");

	/* this function is called for each spacetype, only needs to add map once */
	if (keymap && keymap->modal_items) return;

	keymap = WM_modalkeymap_add(keyconf, "Gesture Border", modal_items);

	/* items for modal map */
	WM_modalkeymap_add_item(keymap, ESCKEY,    KM_PRESS, KM_ANY, 0, GESTURE_MODAL_CANCEL);

	/* Note: cancel only on press otherwise you cannot map this to RMB-gesture */
	WM_modalkeymap_add_item(keymap, RIGHTMOUSE, KM_PRESS, KM_ANY, 0, GESTURE_MODAL_CANCEL);
	WM_modalkeymap_add_item(keymap, RIGHTMOUSE, KM_RELEASE, KM_ANY, 0, GESTURE_MODAL_SELECT);

	/* allow shift leftclick for deselect too */
	WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_PRESS, KM_SHIFT, 0, GESTURE_MODAL_BEGIN);
	WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_RELEASE, KM_SHIFT, 0, GESTURE_MODAL_DESELECT);

	/* any unhandled leftclick release handles select */
	WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_PRESS, 0, 0, GESTURE_MODAL_BEGIN);
	WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_RELEASE, KM_ANY, 0, GESTURE_MODAL_SELECT);

	WM_modalkeymap_add_item(keymap, MIDDLEMOUSE, KM_PRESS, 0, 0, GESTURE_MODAL_BEGIN);
	WM_modalkeymap_add_item(keymap, MIDDLEMOUSE, KM_RELEASE, 0, 0, GESTURE_MODAL_DESELECT);

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
	WM_modalkeymap_assign(keymap, "NODE_OT_viewer_border");
	WM_modalkeymap_assign(keymap, "PAINT_OT_hide_show");
	WM_modalkeymap_assign(keymap, "OUTLINER_OT_select_border");
//	WM_modalkeymap_assign(keymap, "SCREEN_OT_border_select"); // template
	WM_modalkeymap_assign(keymap, "SEQUENCER_OT_select_border");
	WM_modalkeymap_assign(keymap, "SEQUENCER_OT_view_ghost_border");
	WM_modalkeymap_assign(keymap, "UV_OT_select_border");
	WM_modalkeymap_assign(keymap, "CLIP_OT_select_border");
	WM_modalkeymap_assign(keymap, "CLIP_OT_graph_select_border");
	WM_modalkeymap_assign(keymap, "MASK_OT_select_border");
	WM_modalkeymap_assign(keymap, "VIEW2D_OT_zoom_border");
	WM_modalkeymap_assign(keymap, "VIEW3D_OT_clip_border");
	WM_modalkeymap_assign(keymap, "VIEW3D_OT_render_border");
	WM_modalkeymap_assign(keymap, "VIEW3D_OT_select_border");
	WM_modalkeymap_assign(keymap, "VIEW3D_OT_zoom_border"); /* XXX TODO: zoom border should perhaps map rightmouse to zoom out instead of in+cancel */
	WM_modalkeymap_assign(keymap, "IMAGE_OT_render_border");
	WM_modalkeymap_assign(keymap, "IMAGE_OT_view_zoom_border");
	WM_modalkeymap_assign(keymap, "GPENCIL_OT_select_border");
}

/* zoom to border modal operators */
static void gesture_zoom_border_modal_keymap(wmKeyConfig *keyconf)
{
	static const EnumPropertyItem modal_items[] = {
		{GESTURE_MODAL_CANCEL, "CANCEL", 0, "Cancel", ""},
		{GESTURE_MODAL_IN,  "IN", 0, "In", ""},
		{GESTURE_MODAL_OUT, "OUT", 0, "Out", ""},
		{GESTURE_MODAL_BEGIN, "BEGIN", 0, "Begin", ""},
		{0, NULL, 0, NULL, NULL}
	};

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
	WM_modalkeymap_assign(keymap, "IMAGE_OT_view_zoom_border");
}

/* default keymap for windows and screens, only call once per WM */
void wm_window_keymap(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap = WM_keymap_find(keyconf, "Window", 0, 0);
	wmKeyMapItem *kmi;

	/* note, this doesn't replace existing keymap items */
#ifdef USE_WM_KEYMAP_27X
	WM_keymap_verify_item(keymap, "WM_OT_window_new", WKEY, KM_PRESS, KM_CTRL | KM_ALT, 0);
#endif

#ifdef __APPLE__
	WM_keymap_add_item(keymap, "WM_OT_read_homefile", NKEY, KM_PRESS, KM_OSKEY, 0);
	WM_keymap_add_menu(keymap, "INFO_MT_file_open_recent", OKEY, KM_PRESS, KM_SHIFT | KM_OSKEY, 0);
	WM_keymap_add_item(keymap, "WM_OT_open_mainfile", OKEY, KM_PRESS, KM_OSKEY, 0);
	WM_keymap_add_item(keymap, "WM_OT_save_mainfile", SKEY, KM_PRESS, KM_OSKEY, 0);
	WM_keymap_add_item(keymap, "WM_OT_save_as_mainfile", SKEY, KM_PRESS, KM_SHIFT | KM_OSKEY, 0);
	WM_keymap_add_item(keymap, "WM_OT_quit_blender", QKEY, KM_PRESS, KM_OSKEY, 0);
#endif
	WM_keymap_add_item(keymap, "WM_OT_read_homefile", NKEY, KM_PRESS, KM_CTRL, 0);
#ifdef USE_WM_KEYMAP_27X
	WM_keymap_add_item(keymap, "WM_OT_save_homefile", UKEY, KM_PRESS, KM_CTRL, 0);
#endif
	WM_keymap_add_menu(keymap, "INFO_MT_file_open_recent", OKEY, KM_PRESS, KM_SHIFT | KM_CTRL, 0);
	WM_keymap_add_item(keymap, "WM_OT_open_mainfile", OKEY, KM_PRESS, KM_CTRL, 0);
#ifdef USE_WM_KEYMAP_27X
	WM_keymap_add_item(keymap, "WM_OT_open_mainfile", F1KEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "WM_OT_link", OKEY, KM_PRESS, KM_CTRL | KM_ALT, 0);
	WM_keymap_add_item(keymap, "WM_OT_append", F1KEY, KM_PRESS, KM_SHIFT, 0);
#endif

	WM_keymap_add_item(keymap, "WM_OT_save_mainfile", SKEY, KM_PRESS, KM_CTRL, 0);
#ifdef USE_WM_KEYMAP_27X
	WM_keymap_add_item(keymap, "WM_OT_save_mainfile", WKEY, KM_PRESS, KM_CTRL, 0);
#endif
	WM_keymap_add_item(keymap, "WM_OT_save_as_mainfile", SKEY, KM_PRESS, KM_SHIFT | KM_CTRL, 0);
#ifdef USE_WM_KEYMAP_27X
	WM_keymap_add_item(keymap, "WM_OT_save_as_mainfile", F2KEY, KM_PRESS, 0, 0);
	kmi = WM_keymap_add_item(keymap, "WM_OT_save_as_mainfile", SKEY, KM_PRESS, KM_ALT | KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "copy", true);

	WM_keymap_verify_item(keymap, "WM_OT_window_fullscreen_toggle", F11KEY, KM_PRESS, KM_ALT, 0);
#endif

	WM_keymap_add_item(keymap, "WM_OT_quit_blender", QKEY, KM_PRESS, KM_CTRL, 0);

	/* F-Keys are a hassle on some macos systems. */
#ifdef __APPLE__
	WM_keymap_add_item(keymap, "WM_OT_search_menu", FKEY, KM_PRESS, KM_OSKEY, 0);
#endif

#ifdef USE_WM_KEYMAP_27X
	WM_keymap_add_item(keymap, "WM_OT_doc_view_manual_ui_context", F1KEY, KM_PRESS, KM_ALT, 0);

	/* debug/testing */
	WM_keymap_verify_item(keymap, "WM_OT_redraw_timer", TKEY, KM_PRESS, KM_ALT | KM_CTRL, 0);
	WM_keymap_verify_item(keymap, "WM_OT_debug_menu", DKEY, KM_PRESS, KM_ALT | KM_CTRL, 0);
#else
	WM_keymap_add_item(keymap, "WM_OT_doc_view_manual_ui_context", F1KEY, KM_PRESS, 0, 0);
	WM_keymap_add_menu(keymap, "TOPBAR_MT_file_specials", F2KEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "WM_OT_search_menu", F3KEY, KM_PRESS, 0, 0);
	WM_keymap_add_menu(keymap, "TOPBAR_MT_window_specials", F4KEY, KM_PRESS, 0, 0);
#endif

	/* menus that can be accessed anywhere in blender */
	WM_keymap_add_menu(keymap, "SCREEN_MT_user_menu", QKEY, KM_PRESS, 0, 0);

#ifdef WITH_INPUT_NDOF
	WM_keymap_add_menu(keymap, "USERPREF_MT_ndof_settings", NDOF_BUTTON_MENU, KM_PRESS, 0, 0);
#endif

	WM_keymap_add_item(keymap, "WM_OT_toolbar", SPACEKEY, KM_PRESS, 0, 0);

	/* Space switching */
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

#ifdef WITH_INPUT_NDOF
	/* ndof speed */
	const char *data_path = "user_preferences.inputs.ndof_sensitivity";
	kmi = WM_keymap_add_item(keymap, "WM_OT_context_scale_float", NDOF_BUTTON_PLUS, KM_PRESS, 0, 0);
	RNA_string_set(kmi->ptr, "data_path", data_path);
	RNA_float_set(kmi->ptr, "value", 1.1f);

	kmi = WM_keymap_add_item(keymap, "WM_OT_context_scale_float", NDOF_BUTTON_MINUS, KM_PRESS, 0, 0);
	RNA_string_set(kmi->ptr, "data_path", data_path);
	RNA_float_set(kmi->ptr, "value", 1.0f / 1.1f);

	kmi = WM_keymap_add_item(keymap, "WM_OT_context_scale_float", NDOF_BUTTON_PLUS, KM_PRESS, KM_SHIFT, 0);
	RNA_string_set(kmi->ptr, "data_path", data_path);
	RNA_float_set(kmi->ptr, "value", 1.5f);

	kmi = WM_keymap_add_item(keymap, "WM_OT_context_scale_float", NDOF_BUTTON_MINUS, KM_PRESS, KM_SHIFT, 0);
	RNA_string_set(kmi->ptr, "data_path", data_path);
	RNA_float_set(kmi->ptr, "value", 1.0f / 1.5f);
#endif /* WITH_INPUT_NDOF */

	wm_gizmos_keymap(keyconf);
	gesture_circle_modal_keymap(keyconf);
	gesture_border_modal_keymap(keyconf);
	gesture_zoom_border_modal_keymap(keyconf);
	gesture_straightline_modal_keymap(keyconf);
}

/**
 * Filter functions that can be used with rna_id_itemf() below.
 * Should return false if 'id' should be excluded.
 */
static bool rna_id_enum_filter_single(ID *id, void *user_data)
{
	return (id != user_data);
}

/* Generic itemf's for operators that take library args */
static const EnumPropertyItem *rna_id_itemf(
        bContext *UNUSED(C), PointerRNA *UNUSED(ptr),
        bool *r_free, ID *id, bool local,
        bool (*filter_ids)(ID *id, void *user_data), void *user_data)
{
	EnumPropertyItem item_tmp = {0}, *item = NULL;
	int totitem = 0;
	int i = 0;

	for (; id; id = id->next) {
		if ((filter_ids != NULL) && filter_ids(user_data, id) == false) {
			i++;
			continue;
		}
		if (local == false || !ID_IS_LINKED(id)) {
			item_tmp.identifier = item_tmp.name = id->name + 2;
			item_tmp.value = i++;
			RNA_enum_item_add(&item, &totitem, &item_tmp);
		}
	}

	RNA_enum_item_end(&item, &totitem);
	*r_free = true;

	return item;
}

/* can add more as needed */
const EnumPropertyItem *RNA_action_itemf(bContext *C, PointerRNA *ptr, PropertyRNA *UNUSED(prop), bool *r_free)
{
	return rna_id_itemf(C, ptr, r_free, C ? (ID *)CTX_data_main(C)->action.first : NULL, false, NULL, NULL);
}
#if 0 /* UNUSED */
const EnumPropertyItem *RNA_action_local_itemf(bContext *C, PointerRNA *ptr, PropertyRNA *UNUSED(prop), bool *r_free)
{
	return rna_id_itemf(C, ptr, r_free, C ? (ID *)CTX_data_main(C)->action.first : NULL, true);
}
#endif

const EnumPropertyItem *RNA_collection_itemf(bContext *C, PointerRNA *ptr, PropertyRNA *UNUSED(prop), bool *r_free)
{
	return rna_id_itemf(C, ptr, r_free, C ? (ID *)CTX_data_main(C)->collection.first : NULL, false, NULL, NULL);
}
const EnumPropertyItem *RNA_collection_local_itemf(bContext *C, PointerRNA *ptr, PropertyRNA *UNUSED(prop), bool *r_free)
{
	return rna_id_itemf(C, ptr, r_free, C ? (ID *)CTX_data_main(C)->collection.first : NULL, true, NULL, NULL);
}

const EnumPropertyItem *RNA_image_itemf(bContext *C, PointerRNA *ptr, PropertyRNA *UNUSED(prop), bool *r_free)
{
	return rna_id_itemf(C, ptr, r_free, C ? (ID *)CTX_data_main(C)->image.first : NULL, false, NULL, NULL);
}
const EnumPropertyItem *RNA_image_local_itemf(bContext *C, PointerRNA *ptr, PropertyRNA *UNUSED(prop), bool *r_free)
{
	return rna_id_itemf(C, ptr, r_free, C ? (ID *)CTX_data_main(C)->image.first : NULL, true, NULL, NULL);
}

const EnumPropertyItem *RNA_scene_itemf(bContext *C, PointerRNA *ptr, PropertyRNA *UNUSED(prop), bool *r_free)
{
	return rna_id_itemf(C, ptr, r_free, C ? (ID *)CTX_data_main(C)->scene.first : NULL, false, NULL, NULL);
}
const EnumPropertyItem *RNA_scene_local_itemf(bContext *C, PointerRNA *ptr, PropertyRNA *UNUSED(prop), bool *r_free)
{
	return rna_id_itemf(C, ptr, r_free, C ? (ID *)CTX_data_main(C)->scene.first : NULL, true, NULL, NULL);
}
const EnumPropertyItem *RNA_scene_without_active_itemf(bContext *C, PointerRNA *ptr, PropertyRNA *UNUSED(prop), bool *r_free)
{
	Scene *scene_active = C ? CTX_data_scene(C) : NULL;
	return rna_id_itemf(
	        C, ptr, r_free, C ? (ID *)CTX_data_main(C)->scene.first : NULL, true,
	        rna_id_enum_filter_single, scene_active);
}
const EnumPropertyItem *RNA_movieclip_itemf(bContext *C, PointerRNA *ptr, PropertyRNA *UNUSED(prop), bool *r_free)
{
	return rna_id_itemf(C, ptr, r_free, C ? (ID *)CTX_data_main(C)->movieclip.first : NULL, false, NULL, NULL);
}
const EnumPropertyItem *RNA_movieclip_local_itemf(bContext *C, PointerRNA *ptr, PropertyRNA *UNUSED(prop), bool *r_free)
{
	return rna_id_itemf(C, ptr, r_free, C ? (ID *)CTX_data_main(C)->movieclip.first : NULL, true, NULL, NULL);
}

const EnumPropertyItem *RNA_mask_itemf(bContext *C, PointerRNA *ptr, PropertyRNA *UNUSED(prop), bool *r_free)
{
	return rna_id_itemf(C, ptr, r_free, C ? (ID *)CTX_data_main(C)->mask.first : NULL, false, NULL, NULL);
}
const EnumPropertyItem *RNA_mask_local_itemf(bContext *C, PointerRNA *ptr, PropertyRNA *UNUSED(prop), bool *r_free)
{
	return rna_id_itemf(C, ptr, r_free, C ? (ID *)CTX_data_main(C)->mask.first : NULL, true, NULL, NULL);
}
