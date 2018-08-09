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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_wm_gizmo.c
 *  \ingroup RNA
 */

#include <stdlib.h>

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_utildefines.h"
#include "BLI_string_utils.h"

#include "BLT_translation.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "WM_types.h"

#ifdef RNA_RUNTIME
/* enum definitions */
#endif /* RNA_RUNTIME */

#ifdef RNA_RUNTIME

#include <assert.h>

#include "WM_api.h"

#include "DNA_workspace_types.h"

#include "ED_screen.h"

#include "UI_interface.h"

#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_workspace.h"

#include "MEM_guardedalloc.h"

#ifdef WITH_PYTHON
#  include "BPY_extern.h"
#endif

/* -------------------------------------------------------------------- */

/** \name Gizmo API
 * \{ */

#ifdef WITH_PYTHON
static void rna_gizmo_draw_cb(
        const struct bContext *C, struct wmGizmo *gz)
{
	extern FunctionRNA rna_Gizmo_draw_func;
	wmGizmoGroup *gzgroup = gz->parent_gzgroup;
	PointerRNA gz_ptr;
	ParameterList list;
	FunctionRNA *func;
	RNA_pointer_create(NULL, gz->type->ext.srna, gz, &gz_ptr);
	/* RNA_struct_find_function(&gz_ptr, "draw"); */
	func = &rna_Gizmo_draw_func;
	RNA_parameter_list_create(&list, &gz_ptr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	gzgroup->type->ext.call((bContext *)C, &gz_ptr, func, &list);
	RNA_parameter_list_free(&list);
}

static void rna_gizmo_draw_select_cb(
        const struct bContext *C, struct wmGizmo *gz, int select_id)
{
	extern FunctionRNA rna_Gizmo_draw_select_func;
	wmGizmoGroup *gzgroup = gz->parent_gzgroup;
	PointerRNA gz_ptr;
	ParameterList list;
	FunctionRNA *func;
	RNA_pointer_create(NULL, gz->type->ext.srna, gz, &gz_ptr);
	/* RNA_struct_find_function(&gz_ptr, "draw_select"); */
	func = &rna_Gizmo_draw_select_func;
	RNA_parameter_list_create(&list, &gz_ptr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	RNA_parameter_set_lookup(&list, "select_id", &select_id);
	gzgroup->type->ext.call((bContext *)C, &gz_ptr, func, &list);
	RNA_parameter_list_free(&list);
}

static int rna_gizmo_test_select_cb(
        struct bContext *C, struct wmGizmo *gz, const int location[2])
{
	extern FunctionRNA rna_Gizmo_test_select_func;
	wmGizmoGroup *gzgroup = gz->parent_gzgroup;
	PointerRNA gz_ptr;
	ParameterList list;
	FunctionRNA *func;
	RNA_pointer_create(NULL, gz->type->ext.srna, gz, &gz_ptr);
	/* RNA_struct_find_function(&gz_ptr, "test_select"); */
	func = &rna_Gizmo_test_select_func;
	RNA_parameter_list_create(&list, &gz_ptr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	RNA_parameter_set_lookup(&list, "location", location);
	gzgroup->type->ext.call((bContext *)C, &gz_ptr, func, &list);

	void *ret;
	RNA_parameter_get_lookup(&list, "intersect_id", &ret);
	int intersect_id = *(int *)ret;

	RNA_parameter_list_free(&list);
	return intersect_id;
}

static int rna_gizmo_modal_cb(
        struct bContext *C, struct wmGizmo *gz, const struct wmEvent *event,
        eWM_GizmoFlagTweak tweak_flag)
{
	extern FunctionRNA rna_Gizmo_modal_func;
	wmGizmoGroup *gzgroup = gz->parent_gzgroup;
	PointerRNA gz_ptr;
	ParameterList list;
	FunctionRNA *func;
	const int tweak_flag_int = tweak_flag;
	RNA_pointer_create(NULL, gz->type->ext.srna, gz, &gz_ptr);
	/* RNA_struct_find_function(&gz_ptr, "modal"); */
	func = &rna_Gizmo_modal_func;
	RNA_parameter_list_create(&list, &gz_ptr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	RNA_parameter_set_lookup(&list, "event", &event);
	RNA_parameter_set_lookup(&list, "tweak", &tweak_flag_int);
	gzgroup->type->ext.call((bContext *)C, &gz_ptr, func, &list);

	void *ret;
	RNA_parameter_get_lookup(&list, "result", &ret);
	int ret_enum = *(int *)ret;

	RNA_parameter_list_free(&list);
	return ret_enum;
}

static void rna_gizmo_setup_cb(
        struct wmGizmo *gz)
{
	extern FunctionRNA rna_Gizmo_setup_func;
	wmGizmoGroup *gzgroup = gz->parent_gzgroup;
	PointerRNA gz_ptr;
	ParameterList list;
	FunctionRNA *func;
	RNA_pointer_create(NULL, gz->type->ext.srna, gz, &gz_ptr);
	/* RNA_struct_find_function(&gz_ptr, "setup"); */
	func = &rna_Gizmo_setup_func;
	RNA_parameter_list_create(&list, &gz_ptr, func);
	gzgroup->type->ext.call((bContext *)NULL, &gz_ptr, func, &list);
	RNA_parameter_list_free(&list);
}


static int rna_gizmo_invoke_cb(
        struct bContext *C, struct wmGizmo *gz, const struct wmEvent *event)
{
	extern FunctionRNA rna_Gizmo_invoke_func;
	wmGizmoGroup *gzgroup = gz->parent_gzgroup;
	PointerRNA gz_ptr;
	ParameterList list;
	FunctionRNA *func;
	RNA_pointer_create(NULL, gz->type->ext.srna, gz, &gz_ptr);
	/* RNA_struct_find_function(&gz_ptr, "invoke"); */
	func = &rna_Gizmo_invoke_func;
	RNA_parameter_list_create(&list, &gz_ptr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	RNA_parameter_set_lookup(&list, "event", &event);
	gzgroup->type->ext.call((bContext *)C, &gz_ptr, func, &list);

	void *ret;
	RNA_parameter_get_lookup(&list, "result", &ret);
	int ret_enum = *(int *)ret;

	RNA_parameter_list_free(&list);
	return ret_enum;
}

static void rna_gizmo_exit_cb(
        struct bContext *C, struct wmGizmo *gz, bool cancel)
{
	extern FunctionRNA rna_Gizmo_exit_func;
	wmGizmoGroup *gzgroup = gz->parent_gzgroup;
	PointerRNA gz_ptr;
	ParameterList list;
	FunctionRNA *func;
	RNA_pointer_create(NULL, gz->type->ext.srna, gz, &gz_ptr);
	/* RNA_struct_find_function(&gz_ptr, "exit"); */
	func = &rna_Gizmo_exit_func;
	RNA_parameter_list_create(&list, &gz_ptr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	{
		int cancel_i = cancel;
		RNA_parameter_set_lookup(&list, "cancel", &cancel_i);
	}
	gzgroup->type->ext.call((bContext *)C, &gz_ptr, func, &list);
	RNA_parameter_list_free(&list);
}

static void rna_gizmo_select_refresh_cb(
        struct wmGizmo *gz)
{
	extern FunctionRNA rna_Gizmo_select_refresh_func;
	wmGizmoGroup *gzgroup = gz->parent_gzgroup;
	PointerRNA gz_ptr;
	ParameterList list;
	FunctionRNA *func;
	RNA_pointer_create(NULL, gz->type->ext.srna, gz, &gz_ptr);
	/* RNA_struct_find_function(&gz_ptr, "select_refresh"); */
	func = &rna_Gizmo_select_refresh_func;
	RNA_parameter_list_create(&list, &gz_ptr, func);
	gzgroup->type->ext.call((bContext *)NULL, &gz_ptr, func, &list);
	RNA_parameter_list_free(&list);
}

#endif  /* WITH_PYTHON */

/* just to work around 'const char *' warning and to ensure this is a python op */
static void rna_Gizmo_bl_idname_set(PointerRNA *ptr, const char *value)
{
	wmGizmo *data = ptr->data;
	char *str = (char *)data->type->idname;
	if (!str[0]) {
		BLI_strncpy(str, value, MAX_NAME);    /* utf8 already ensured */
	}
	else {
		assert(!"setting the bl_idname on a non-builtin operator");
	}
}

static wmGizmo *rna_GizmoProperties_find_operator(PointerRNA *ptr)
{
#if 0
	wmWindowManager *wm = ptr->id.data;
#endif

	/* We could try workaruond this lookup, but not trivial. */
	for (bScreen *screen = G_MAIN->screen.first; screen; screen = screen->id.next) {
		IDProperty *properties = ptr->data;
		for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
			for (ARegion *ar = sa->regionbase.first; ar; ar = ar->next) {
				if (ar->gizmo_map) {
					wmGizmoMap *gzmap = ar->gizmo_map;
					for (wmGizmoGroup *gzgroup = WM_gizmomap_group_list(gzmap)->first;
					     gzgroup;
					     gzgroup = gzgroup->next)
					{
						for (wmGizmo *gz = gzgroup->gizmos.first; gz; gz = gz->next) {
							if (gz->properties == properties) {
								return gz;
							}
						}
					}
				}
			}
		}
	}
	return NULL;
}

static StructRNA *rna_GizmoProperties_refine(PointerRNA *ptr)
{
	wmGizmo *gz = rna_GizmoProperties_find_operator(ptr);

	if (gz)
		return gz->type->srna;
	else
		return ptr->type;
}

static IDProperty *rna_GizmoProperties_idprops(PointerRNA *ptr, bool create)
{
	if (create && !ptr->data) {
		IDPropertyTemplate val = {0};
		ptr->data = IDP_New(IDP_GROUP, &val, "RNA_GizmoProperties group");
	}

	return ptr->data;
}

static PointerRNA rna_Gizmo_properties_get(PointerRNA *ptr)
{
	wmGizmo *gz = ptr->data;
	return rna_pointer_inherit_refine(ptr, gz->type->srna, gz->properties);
}

/* wmGizmo.float */
#define RNA_GIZMO_GENERIC_FLOAT_RW_DEF(func_id, member_id) \
static float rna_Gizmo_##func_id##_get(PointerRNA *ptr) \
{ \
	wmGizmo *gz = ptr->data; \
	return gz->member_id; \
} \
static void rna_Gizmo_##func_id##_set(PointerRNA *ptr, float value) \
{ \
	wmGizmo *gz = ptr->data; \
	gz->member_id = value; \
}
#define RNA_GIZMO_GENERIC_FLOAT_ARRAY_INDEX_RW_DEF(func_id, member_id, index) \
static float rna_Gizmo_##func_id##_get(PointerRNA *ptr) \
{ \
	wmGizmo *gz = ptr->data; \
	return gz->member_id[index]; \
} \
static void rna_Gizmo_##func_id##_set(PointerRNA *ptr, float value) \
{ \
	wmGizmo *gz = ptr->data; \
	gz->member_id[index] = value; \
}
/* wmGizmo.float[len] */
#define RNA_GIZMO_GENERIC_FLOAT_ARRAY_RW_DEF(func_id, member_id, len) \
static void rna_Gizmo_##func_id##_get(PointerRNA *ptr, float value[len]) \
{ \
	wmGizmo *gz = ptr->data; \
	memcpy(value, gz->member_id, sizeof(float[len])); \
} \
static void rna_Gizmo_##func_id##_set(PointerRNA *ptr, const float value[len]) \
{ \
	wmGizmo *gz = ptr->data; \
	memcpy(gz->member_id, value, sizeof(float[len])); \
}

/* wmGizmo.flag */
#define RNA_GIZMO_GENERIC_FLAG_RW_DEF(func_id, member_id, flag_value) \
static bool rna_Gizmo_##func_id##_get(PointerRNA *ptr) \
{ \
	wmGizmo *gz = ptr->data; \
	return (gz->member_id & flag_value) != 0; \
} \
static void rna_Gizmo_##func_id##_set(PointerRNA *ptr, bool value) \
{ \
	wmGizmo *gz = ptr->data; \
	SET_FLAG_FROM_TEST(gz->member_id, value, flag_value); \
}

/* wmGizmo.flag (negative) */
#define RNA_GIZMO_GENERIC_FLAG_NEG_RW_DEF(func_id, member_id, flag_value) \
static bool rna_Gizmo_##func_id##_get(PointerRNA *ptr) \
{ \
	wmGizmo *gz = ptr->data; \
	return (gz->member_id & flag_value) == 0; \
} \
static void rna_Gizmo_##func_id##_set(PointerRNA *ptr, bool value) \
{ \
	wmGizmo *gz = ptr->data; \
	SET_FLAG_FROM_TEST(gz->member_id, !value, flag_value); \
}

#define RNA_GIZMO_FLAG_RO_DEF(func_id, member_id, flag_value) \
static int rna_Gizmo_##func_id##_get(PointerRNA *ptr) \
{ \
	wmGizmo *gz = ptr->data; \
	return (gz->member_id & flag_value) != 0; \
}

RNA_GIZMO_GENERIC_FLOAT_ARRAY_RW_DEF(color, color, 3);
RNA_GIZMO_GENERIC_FLOAT_ARRAY_RW_DEF(color_hi, color_hi, 3);

RNA_GIZMO_GENERIC_FLOAT_ARRAY_INDEX_RW_DEF(alpha, color, 3);
RNA_GIZMO_GENERIC_FLOAT_ARRAY_INDEX_RW_DEF(alpha_hi, color_hi, 3);

RNA_GIZMO_GENERIC_FLOAT_ARRAY_RW_DEF(matrix_space, matrix_space, 16);
RNA_GIZMO_GENERIC_FLOAT_ARRAY_RW_DEF(matrix_basis, matrix_basis, 16);
RNA_GIZMO_GENERIC_FLOAT_ARRAY_RW_DEF(matrix_offset, matrix_offset, 16);

static void rna_Gizmo_matrix_world_get(PointerRNA *ptr, float value[16])
{
	wmGizmo *gz = ptr->data;
	WM_gizmo_calc_matrix_final(gz, (float (*)[4])value);
}

RNA_GIZMO_GENERIC_FLOAT_RW_DEF(scale_basis, scale_basis);
RNA_GIZMO_GENERIC_FLOAT_RW_DEF(line_width, line_width);

RNA_GIZMO_GENERIC_FLAG_RW_DEF(flag_use_draw_hover, flag, WM_GIZMO_DRAW_HOVER);
RNA_GIZMO_GENERIC_FLAG_RW_DEF(flag_use_draw_modal, flag, WM_GIZMO_DRAW_MODAL);
RNA_GIZMO_GENERIC_FLAG_RW_DEF(flag_use_draw_value, flag, WM_GIZMO_DRAW_VALUE);
RNA_GIZMO_GENERIC_FLAG_RW_DEF(flag_use_draw_offset_scale, flag, WM_GIZMO_DRAW_OFFSET_SCALE);
RNA_GIZMO_GENERIC_FLAG_NEG_RW_DEF(flag_use_draw_scale, flag, WM_GIZMO_DRAW_OFFSET_SCALE);
RNA_GIZMO_GENERIC_FLAG_RW_DEF(flag_hide, flag, WM_GIZMO_HIDDEN);
RNA_GIZMO_GENERIC_FLAG_RW_DEF(flag_use_grab_cursor, flag, WM_GIZMO_GRAB_CURSOR);
RNA_GIZMO_GENERIC_FLAG_RW_DEF(flag_use_select_background, flag, WM_GIZMO_SELECT_BACKGROUND);

/* wmGizmo.state */
RNA_GIZMO_FLAG_RO_DEF(state_is_highlight, state, WM_GIZMO_STATE_HIGHLIGHT);
RNA_GIZMO_FLAG_RO_DEF(state_is_modal, state, WM_GIZMO_STATE_MODAL);
RNA_GIZMO_FLAG_RO_DEF(state_select, state, WM_GIZMO_STATE_SELECT);

static void rna_Gizmo_state_select_set(struct PointerRNA *ptr, bool value)
{
	wmGizmo *gz = ptr->data;
	wmGizmoGroup *gzgroup = gz->parent_gzgroup;
	WM_gizmo_select_set(gzgroup->parent_gzmap, gz, value);
}

static PointerRNA rna_Gizmo_group_get(PointerRNA *ptr)
{
	wmGizmo *gz = ptr->data;
	return rna_pointer_inherit_refine(ptr, &RNA_GizmoGroup, gz->parent_gzgroup);
}

#ifdef WITH_PYTHON

static void rna_Gizmo_unregister(struct Main *bmain, StructRNA *type);
void BPY_RNA_gizmo_wrapper(wmGizmoType *gzgt, void *userdata);

static StructRNA *rna_Gizmo_register(
        Main *bmain, ReportList *reports, void *data, const char *identifier,
        StructValidateFunc validate, StructCallbackFunc call, StructFreeFunc free)
{
	struct {
		char idname[MAX_NAME];
	} temp_buffers;

	wmGizmoType dummygt = {NULL};
	wmGizmo dummymnp = {NULL};
	PointerRNA mnp_ptr;

	/* Two sets of functions. */
	int have_function[8];

	/* setup dummy gizmo & gizmo type to store static properties in */
	dummymnp.type = &dummygt;
	dummygt.idname = temp_buffers.idname;
	RNA_pointer_create(NULL, &RNA_Gizmo, &dummymnp, &mnp_ptr);

	/* Clear so we can detect if it's left unset. */
	temp_buffers.idname[0] = '\0';

	/* validate the python class */
	if (validate(&mnp_ptr, data, have_function) != 0)
		return NULL;

	if (strlen(identifier) >= sizeof(temp_buffers.idname)) {
		BKE_reportf(reports, RPT_ERROR, "Registering gizmo class: '%s' is too long, maximum length is %d",
		            identifier, (int)sizeof(temp_buffers.idname));
		return NULL;
	}

	/* check if we have registered this gizmo type before, and remove it */
	{
		const wmGizmoType *gzt = WM_gizmotype_find(dummygt.idname, true);
		if (gzt && gzt->ext.srna) {
			rna_Gizmo_unregister(bmain, gzt->ext.srna);
		}
	}
	if (!RNA_struct_available_or_report(reports, dummygt.idname)) {
		return NULL;
	}

	{   /* allocate the idname */
		/* For multiple strings see GizmoGroup. */
		dummygt.idname = BLI_strdup(temp_buffers.idname);
	}

	/* create a new gizmo type */
	dummygt.ext.srna = RNA_def_struct_ptr(&BLENDER_RNA, dummygt.idname, &RNA_Gizmo);
	/* gizmo properties are registered separately */
	RNA_def_struct_flag(dummygt.ext.srna, STRUCT_NO_IDPROPERTIES);
	dummygt.ext.data = data;
	dummygt.ext.call = call;
	dummygt.ext.free = free;

	{
		int i = 0;
		dummygt.draw = (have_function[i++]) ? rna_gizmo_draw_cb : NULL;
		dummygt.draw_select = (have_function[i++]) ? rna_gizmo_draw_select_cb : NULL;
		dummygt.test_select = (have_function[i++]) ? rna_gizmo_test_select_cb : NULL;
		dummygt.modal = (have_function[i++]) ? rna_gizmo_modal_cb : NULL;
//		dummygt.property_update = (have_function[i++]) ? rna_gizmo_property_update : NULL;
//		dummygt.position_get = (have_function[i++]) ? rna_gizmo_position_get : NULL;
		dummygt.setup = (have_function[i++]) ? rna_gizmo_setup_cb : NULL;
		dummygt.invoke = (have_function[i++]) ? rna_gizmo_invoke_cb : NULL;
		dummygt.exit = (have_function[i++]) ? rna_gizmo_exit_cb : NULL;
		dummygt.select_refresh = (have_function[i++]) ? rna_gizmo_select_refresh_cb : NULL;

		BLI_assert(i == ARRAY_SIZE(have_function));
	}

	WM_gizmotype_append_ptr(BPY_RNA_gizmo_wrapper, (void *)&dummygt);

	/* update while blender is running */
	WM_main_add_notifier(NC_SCREEN | NA_EDITED, NULL);

	return dummygt.ext.srna;
}

static void rna_Gizmo_unregister(struct Main *bmain, StructRNA *type)
{
	wmGizmoType *gzt = RNA_struct_blender_type_get(type);

	if (!gzt) {
		return;
	}

	RNA_struct_free_extension(type, &gzt->ext);
	RNA_struct_free(&BLENDER_RNA, type);

	WM_main_add_notifier(NC_SCREEN | NA_EDITED, NULL);

	WM_gizmotype_remove_ptr(NULL, bmain, gzt);
}

static void **rna_Gizmo_instance(PointerRNA *ptr)
{
	wmGizmo *gz = ptr->data;
	return &gz->py_instance;
}

#endif  /* WITH_PYTHON */


static StructRNA *rna_Gizmo_refine(PointerRNA *mnp_ptr)
{
	wmGizmo *gz = mnp_ptr->data;
	return (gz->type && gz->type->ext.srna) ? gz->type->ext.srna : &RNA_Gizmo;
}

/** \} */

/** \name Gizmo Group API
 * \{ */

static wmGizmo *rna_GizmoGroup_gizmo_new(
        wmGizmoGroup *gzgroup, ReportList *reports, const char *idname)
{
	const wmGizmoType *gzt = WM_gizmotype_find(idname, true);
	if (gzt == NULL) {
		BKE_reportf(reports, RPT_ERROR, "GizmoType '%s' not known", idname);
		return NULL;
	}
	wmGizmo *gz = WM_gizmo_new_ptr(gzt, gzgroup, NULL);
	return gz;
}

static void rna_GizmoGroup_gizmo_remove(
        wmGizmoGroup *gzgroup, bContext *C, wmGizmo *gz)
{
	WM_gizmo_unlink(&gzgroup->gizmos, gzgroup->parent_gzmap, gz, C);
}

static void rna_GizmoGroup_gizmo_clear(
        wmGizmoGroup *gzgroup, bContext *C)
{
	while (gzgroup->gizmos.first) {
		WM_gizmo_unlink(&gzgroup->gizmos, gzgroup->parent_gzmap, gzgroup->gizmos.first, C);
	}
}

static void rna_GizmoGroup_name_get(PointerRNA *ptr, char *value)
{
	wmGizmoGroup *gzgroup = ptr->data;
	strcpy(value, gzgroup->type->name);
}

static int rna_GizmoGroup_name_length(PointerRNA *ptr)
{
	wmGizmoGroup *gzgroup = ptr->data;
	return strlen(gzgroup->type->name);
}

/* just to work around 'const char *' warning and to ensure this is a python op */
static void rna_GizmoGroup_bl_idname_set(PointerRNA *ptr, const char *value)
{
	wmGizmoGroup *data = ptr->data;
	char *str = (char *)data->type->idname;
	if (!str[0])
		BLI_strncpy(str, value, MAX_NAME);    /* utf8 already ensured */
	else
		assert(!"setting the bl_idname on a non-builtin operator");
}

static void rna_GizmoGroup_bl_label_set(PointerRNA *ptr, const char *value)
{
	wmGizmoGroup *data = ptr->data;
	char *str = (char *)data->type->name;
	if (!str[0])
		BLI_strncpy(str, value, MAX_NAME);    /* utf8 already ensured */
	else
		assert(!"setting the bl_label on a non-builtin operator");
}

static bool rna_GizmoGroup_has_reports_get(PointerRNA *ptr)
{
	wmGizmoGroup *gzgroup = ptr->data;
	return (gzgroup->reports && gzgroup->reports->list.first);
}

#ifdef WITH_PYTHON

static bool rna_gizmogroup_poll_cb(const bContext *C, wmGizmoGroupType *gzgt)
{

	extern FunctionRNA rna_GizmoGroup_poll_func;

	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;
	void *ret;
	int visible;

	RNA_pointer_create(NULL, gzgt->ext.srna, NULL, &ptr); /* dummy */
	func = &rna_GizmoGroup_poll_func; /* RNA_struct_find_function(&ptr, "poll"); */

	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	gzgt->ext.call((bContext *)C, &ptr, func, &list);

	RNA_parameter_get_lookup(&list, "visible", &ret);
	visible = *(int *)ret;

	RNA_parameter_list_free(&list);

	return visible;
}

static void rna_gizmogroup_setup_cb(const bContext *C, wmGizmoGroup *gzgroup)
{
	extern FunctionRNA rna_GizmoGroup_setup_func;

	PointerRNA gzgroup_ptr;
	ParameterList list;
	FunctionRNA *func;

	RNA_pointer_create(NULL, gzgroup->type->ext.srna, gzgroup, &gzgroup_ptr);
	func = &rna_GizmoGroup_setup_func; /* RNA_struct_find_function(&wgroupr, "setup"); */

	RNA_parameter_list_create(&list, &gzgroup_ptr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	gzgroup->type->ext.call((bContext *)C, &gzgroup_ptr, func, &list);

	RNA_parameter_list_free(&list);
}

static wmKeyMap *rna_gizmogroup_setup_keymap_cb(const wmGizmoGroupType *gzgt, wmKeyConfig *config)
{
	extern FunctionRNA rna_GizmoGroup_setup_keymap_func;
	void *ret;

	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;

	RNA_pointer_create(NULL, gzgt->ext.srna, NULL, &ptr); /* dummy */
	func = &rna_GizmoGroup_setup_keymap_func; /* RNA_struct_find_function(&wgroupr, "setup_keymap"); */

	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "keyconfig", &config);
	gzgt->ext.call(NULL, &ptr, func, &list);

	RNA_parameter_get_lookup(&list, "keymap", &ret);
	wmKeyMap *keymap = *(wmKeyMap **)ret;

	RNA_parameter_list_free(&list);

	return keymap;
}

static void rna_gizmogroup_refresh_cb(const bContext *C, wmGizmoGroup *gzgroup)
{
	extern FunctionRNA rna_GizmoGroup_refresh_func;

	PointerRNA gzgroup_ptr;
	ParameterList list;
	FunctionRNA *func;

	RNA_pointer_create(NULL, gzgroup->type->ext.srna, gzgroup, &gzgroup_ptr);
	func = &rna_GizmoGroup_refresh_func; /* RNA_struct_find_function(&wgroupr, "refresh"); */

	RNA_parameter_list_create(&list, &gzgroup_ptr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	gzgroup->type->ext.call((bContext *)C, &gzgroup_ptr, func, &list);

	RNA_parameter_list_free(&list);
}

static void rna_gizmogroup_draw_prepare_cb(const bContext *C, wmGizmoGroup *gzgroup)
{
	extern FunctionRNA rna_GizmoGroup_draw_prepare_func;

	PointerRNA gzgroup_ptr;
	ParameterList list;
	FunctionRNA *func;

	RNA_pointer_create(NULL, gzgroup->type->ext.srna, gzgroup, &gzgroup_ptr);
	func = &rna_GizmoGroup_draw_prepare_func; /* RNA_struct_find_function(&wgroupr, "draw_prepare"); */

	RNA_parameter_list_create(&list, &gzgroup_ptr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	gzgroup->type->ext.call((bContext *)C, &gzgroup_ptr, func, &list);

	RNA_parameter_list_free(&list);
}

void BPY_RNA_gizmogroup_wrapper(wmGizmoGroupType *gzgt, void *userdata);
static void rna_GizmoGroup_unregister(struct Main *bmain, StructRNA *type);

static StructRNA *rna_GizmoGroup_register(
        Main *bmain, ReportList *reports, void *data, const char *identifier,
        StructValidateFunc validate, StructCallbackFunc call, StructFreeFunc free)
{
	struct {
		char name[MAX_NAME];
		char idname[MAX_NAME];
	} temp_buffers;

	wmGizmoGroupType dummywgt = {NULL};
	wmGizmoGroup dummywg = {NULL};
	PointerRNA wgptr;

	/* Two sets of functions. */
	int have_function[5];

	/* setup dummy gizmogroup & gizmogroup type to store static properties in */
	dummywg.type = &dummywgt;
	dummywgt.name = temp_buffers.name;
	dummywgt.idname = temp_buffers.idname;

	RNA_pointer_create(NULL, &RNA_GizmoGroup, &dummywg, &wgptr);

	/* Clear so we can detect if it's left unset. */
	temp_buffers.idname[0] = temp_buffers.name[0] = '\0';

	/* validate the python class */
	if (validate(&wgptr, data, have_function) != 0)
		return NULL;

	if (strlen(identifier) >= sizeof(temp_buffers.idname)) {
		BKE_reportf(reports, RPT_ERROR, "Registering gizmogroup class: '%s' is too long, maximum length is %d",
		            identifier, (int)sizeof(temp_buffers.idname));
		return NULL;
	}

	/* check if the area supports widgets */
	const struct wmGizmoMapType_Params wmap_params = {
		.spaceid = dummywgt.gzmap_params.spaceid,
		.regionid = dummywgt.gzmap_params.regionid,
	};

	wmGizmoMapType *gzmap_type = WM_gizmomaptype_ensure(&wmap_params);
	if (gzmap_type == NULL) {
		BKE_reportf(reports, RPT_ERROR, "Area type does not support gizmos");
		return NULL;
	}

	/* check if we have registered this gizmogroup type before, and remove it */
	{
		wmGizmoGroupType *gzgt = WM_gizmogrouptype_find(dummywgt.idname, true);
		if (gzgt && gzgt->ext.srna) {
			rna_GizmoGroup_unregister(bmain, gzgt->ext.srna);
		}
	}
	if (!RNA_struct_available_or_report(reports, dummywgt.idname)) {
		return NULL;
	}

	{   /* allocate the idname */
		const char *strings[] = {
			temp_buffers.idname,
			temp_buffers.name,
		};
		char *strings_table[ARRAY_SIZE(strings)];
		BLI_string_join_array_by_sep_char_with_tableN('\0', strings_table, strings, ARRAY_SIZE(strings));

		dummywgt.idname = strings_table[0];  /* allocated string stored here */
		dummywgt.name = strings_table[1];
		BLI_assert(ARRAY_SIZE(strings) == 2);
	}

	/* create a new gizmogroup type */
	dummywgt.ext.srna = RNA_def_struct_ptr(&BLENDER_RNA, dummywgt.idname, &RNA_GizmoGroup);
	RNA_def_struct_flag(dummywgt.ext.srna, STRUCT_NO_IDPROPERTIES); /* gizmogroup properties are registered separately */
	dummywgt.ext.data = data;
	dummywgt.ext.call = call;
	dummywgt.ext.free = free;

	/* We used to register widget group types like this, now we do it similar to
	 * operator types. Thus we should be able to do the same as operator types now. */
	dummywgt.poll = (have_function[0]) ? rna_gizmogroup_poll_cb : NULL;
	dummywgt.setup_keymap =     (have_function[1]) ? rna_gizmogroup_setup_keymap_cb : NULL;
	dummywgt.setup =            (have_function[2]) ? rna_gizmogroup_setup_cb : NULL;
	dummywgt.refresh =          (have_function[3]) ? rna_gizmogroup_refresh_cb : NULL;
	dummywgt.draw_prepare =     (have_function[4]) ? rna_gizmogroup_draw_prepare_cb : NULL;

	wmGizmoGroupType *gzgt = WM_gizmogrouptype_append_ptr(
	        BPY_RNA_gizmogroup_wrapper, (void *)&dummywgt);

	{
		const char *owner_id = RNA_struct_state_owner_get();
		if (owner_id) {
			BLI_strncpy(gzgt->owner_id, owner_id, sizeof(gzgt->owner_id));
		}
	}

	if (gzgt->flag & WM_GIZMOGROUPTYPE_PERSISTENT) {
		WM_gizmo_group_type_add_ptr_ex(gzgt, gzmap_type);

		/* update while blender is running */
		WM_main_add_notifier(NC_SCREEN | NA_EDITED, NULL);
	}

	return dummywgt.ext.srna;
}

static void rna_GizmoGroup_unregister(struct Main *bmain, StructRNA *type)
{
	wmGizmoGroupType *gzgt = RNA_struct_blender_type_get(type);

	if (!gzgt)
		return;

	RNA_struct_free_extension(type, &gzgt->ext);
	RNA_struct_free(&BLENDER_RNA, type);

	WM_main_add_notifier(NC_SCREEN | NA_EDITED, NULL);

	WM_gizmo_group_type_remove_ptr(bmain, gzgt);
}

static void **rna_GizmoGroup_instance(PointerRNA *ptr)
{
	wmGizmoGroup *gzgroup = ptr->data;
	return &gzgroup->py_instance;
}

#endif  /* WITH_PYTHON */

static StructRNA *rna_GizmoGroup_refine(PointerRNA *gzgroup_ptr)
{
	wmGizmoGroup *gzgroup = gzgroup_ptr->data;
	return (gzgroup->type && gzgroup->type->ext.srna) ? gzgroup->type->ext.srna : &RNA_GizmoGroup;
}

static void rna_GizmoGroup_gizmos_begin(CollectionPropertyIterator *iter, PointerRNA *gzgroup_ptr)
{
	wmGizmoGroup *gzgroup = gzgroup_ptr->data;
	rna_iterator_listbase_begin(iter, &gzgroup->gizmos, NULL);
}

/** \} */


#else /* RNA_RUNTIME */


/* GizmoGroup.gizmos */
static void rna_def_gizmos(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;

	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "Gizmos");
	srna = RNA_def_struct(brna, "Gizmos", NULL);
	RNA_def_struct_sdna(srna, "wmGizmoGroup");
	RNA_def_struct_ui_text(srna, "Gizmos", "Collection of gizmos");

	func = RNA_def_function(srna, "new", "rna_GizmoGroup_gizmo_new");
	RNA_def_function_ui_description(func, "Add gizmo");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	RNA_def_string(func, "type", "Type", 0, "", "Gizmo identifier"); /* optional */
	parm = RNA_def_pointer(func, "gizmo", "Gizmo", "", "New gizmo");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "remove", "rna_GizmoGroup_gizmo_remove");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	RNA_def_function_ui_description(func, "Delete gizmo");
	parm = RNA_def_pointer(func, "gizmo", "Gizmo", "", "New gizmo");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
	RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);

	func = RNA_def_function(srna, "clear", "rna_GizmoGroup_gizmo_clear");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	RNA_def_function_ui_description(func, "Delete all gizmos");
}


static void rna_def_gizmo(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *prop;

	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "Gizmo");
	srna = RNA_def_struct(brna, "Gizmo", NULL);
	RNA_def_struct_sdna(srna, "wmGizmo");
	RNA_def_struct_ui_text(srna, "Gizmo", "Collection of gizmos");
	RNA_def_struct_refine_func(srna, "rna_Gizmo_refine");

#ifdef WITH_PYTHON
	RNA_def_struct_register_funcs(
	        srna,
	        "rna_Gizmo_register",
	        "rna_Gizmo_unregister",
	        "rna_Gizmo_instance");
#endif
	RNA_def_struct_translation_context(srna, BLT_I18NCONTEXT_OPERATOR_DEFAULT);

	prop = RNA_def_property(srna, "properties", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_struct_type(prop, "GizmoProperties");
	RNA_def_property_ui_text(prop, "Properties", "");
	RNA_def_property_pointer_funcs(prop, "rna_Gizmo_properties_get", NULL, NULL, NULL);

	/* -------------------------------------------------------------------- */
	/* Registerable Variables */

	RNA_define_verify_sdna(0); /* not in sdna */

	prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "type->idname");
	RNA_def_property_string_maxlength(prop, MAX_NAME);
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_Gizmo_bl_idname_set");
	/* RNA_def_property_clear_flag(prop, PROP_EDITABLE); */
	RNA_def_property_flag(prop, PROP_REGISTER);

	RNA_define_verify_sdna(1); /* not in sdna */

	/* wmGizmo.draw */
	func = RNA_def_function(srna, "draw", NULL);
	RNA_def_function_ui_description(func, "");
	RNA_def_function_flag(func, FUNC_REGISTER);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

	/* wmGizmo.draw_select */
	func = RNA_def_function(srna, "draw_select", NULL);
	RNA_def_function_ui_description(func, "");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
	parm = RNA_def_int(func, "select_id", 0, 0, INT_MAX, "", "", 0, INT_MAX);

	/* wmGizmo.test_select */
	func = RNA_def_function(srna, "test_select", NULL);
	RNA_def_function_ui_description(func, "");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
	parm = RNA_def_int_array(func, "location", 2, NULL, INT_MIN, INT_MAX, "Location", "Region coordinates", INT_MIN, INT_MAX);
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
	parm = RNA_def_int(func, "intersect_id", 0, 0, INT_MAX, "", "", 0, INT_MAX);
	RNA_def_function_return(func, parm);

	/* wmGizmo.handler */
	static EnumPropertyItem tweak_actions[] = {
		{WM_GIZMO_TWEAK_PRECISE, "PRECISE", 0, "Precise", ""},
		{WM_GIZMO_TWEAK_SNAP, "SNAP", 0, "Snap", ""},
		{0, NULL, 0, NULL, NULL}
	};
	func = RNA_def_function(srna, "modal", NULL);
	RNA_def_function_ui_description(func, "");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
	parm = RNA_def_pointer(func, "event", "Event", "", "");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
	/* TODO, shuold be a enum-flag */
	parm = RNA_def_enum_flag(func, "tweak", tweak_actions, 0, "Tweak", "");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	parm = RNA_def_enum_flag(func, "result", rna_enum_operator_return_items, OPERATOR_CANCELLED, "result", "");
	RNA_def_function_return(func, parm);
	/* wmGizmo.property_update */
	/* TODO */

	/* wmGizmo.setup */
	func = RNA_def_function(srna, "setup", NULL);
	RNA_def_function_ui_description(func, "");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);

	/* wmGizmo.invoke */
	func = RNA_def_function(srna, "invoke", NULL);
	RNA_def_function_ui_description(func, "");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
	parm = RNA_def_pointer(func, "event", "Event", "", "");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
	parm = RNA_def_enum_flag(func, "result", rna_enum_operator_return_items, OPERATOR_CANCELLED, "result", "");
	RNA_def_function_return(func, parm);

	/* wmGizmo.exit */
	func = RNA_def_function(srna, "exit", NULL);
	RNA_def_function_ui_description(func, "");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
	parm = RNA_def_boolean(func, "cancel", 0, "Cancel, otherwise confirm", "");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

	/* wmGizmo.cursor_get */
	/* TODO */

	/* wmGizmo.select_refresh */
	func = RNA_def_function(srna, "select_refresh", NULL);
	RNA_def_function_ui_description(func, "");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);


	/* -------------------------------------------------------------------- */
	/* Instance Variables */

	prop = RNA_def_property(srna, "group", PROP_POINTER, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "GizmoGroup");
	RNA_def_property_pointer_funcs(prop, "rna_Gizmo_group_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "", "Gizmo group this gizmo is a member of");

	/* Color & Alpha */
	prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_funcs(prop, "rna_Gizmo_color_get", "rna_Gizmo_color_set", NULL);

	prop = RNA_def_property(srna, "alpha", PROP_FLOAT, PROP_NONE);
	RNA_def_property_ui_text(prop, "Alpha", "");
	RNA_def_property_float_funcs(prop, "rna_Gizmo_alpha_get", "rna_Gizmo_alpha_set", NULL);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_update(prop, NC_SCREEN | NA_EDITED, NULL);

	/* Color & Alpha (highlight) */
	prop = RNA_def_property(srna, "color_highlight", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_funcs(prop, "rna_Gizmo_color_hi_get", "rna_Gizmo_color_hi_set", NULL);

	prop = RNA_def_property(srna, "alpha_highlight", PROP_FLOAT, PROP_NONE);
	RNA_def_property_ui_text(prop, "Alpha", "");
	RNA_def_property_float_funcs(prop, "rna_Gizmo_alpha_hi_get", "rna_Gizmo_alpha_hi_set", NULL);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_update(prop, NC_SCREEN | NA_EDITED, NULL);

	prop = RNA_def_property(srna, "matrix_space", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
	RNA_def_property_ui_text(prop, "Space Matrix", "");
	RNA_def_property_float_funcs(prop, "rna_Gizmo_matrix_space_get", "rna_Gizmo_matrix_space_set", NULL);
	RNA_def_property_update(prop, NC_SCREEN | NA_EDITED, NULL);

	prop = RNA_def_property(srna, "matrix_basis", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
	RNA_def_property_ui_text(prop, "Basis Matrix", "");
	RNA_def_property_float_funcs(prop, "rna_Gizmo_matrix_basis_get", "rna_Gizmo_matrix_basis_set", NULL);
	RNA_def_property_update(prop, NC_SCREEN | NA_EDITED, NULL);

	prop = RNA_def_property(srna, "matrix_offset", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
	RNA_def_property_ui_text(prop, "Offset Matrix", "");
	RNA_def_property_float_funcs(prop, "rna_Gizmo_matrix_offset_get", "rna_Gizmo_matrix_offset_set", NULL);
	RNA_def_property_update(prop, NC_SCREEN | NA_EDITED, NULL);

	prop = RNA_def_property(srna, "matrix_world", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
	RNA_def_property_ui_text(prop, "Final World Matrix", "");
	RNA_def_property_float_funcs(prop, "rna_Gizmo_matrix_world_get", NULL, NULL);

	prop = RNA_def_property(srna, "scale_basis", PROP_FLOAT, PROP_NONE);
	RNA_def_property_ui_text(prop, "Scale Basis", "");
	RNA_def_property_float_funcs(prop, "rna_Gizmo_scale_basis_get", "rna_Gizmo_scale_basis_set", NULL);
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_update(prop, NC_SCREEN | NA_EDITED, NULL);

	prop = RNA_def_property(srna, "line_width", PROP_FLOAT, PROP_PIXEL);
	RNA_def_property_ui_text(prop, "Line Width", "");
	RNA_def_property_float_funcs(prop, "rna_Gizmo_line_width_get", "rna_Gizmo_line_width_set", NULL);
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_update(prop, NC_SCREEN | NA_EDITED, NULL);

	/* wmGizmo.flag */
	/* WM_GIZMO_HIDDEN */
	prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(
	        prop, "rna_Gizmo_flag_hide_get", "rna_Gizmo_flag_hide_set");
	RNA_def_property_ui_text(prop, "Hide", "");
	RNA_def_property_update(prop, NC_SCREEN | NA_EDITED, NULL);
	/* WM_GIZMO_GRAB_CURSOR */
	prop = RNA_def_property(srna, "use_grab_cursor", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(
	        prop, "rna_Gizmo_flag_use_grab_cursor_get", "rna_Gizmo_flag_use_grab_cursor_set");
	RNA_def_property_ui_text(prop, "Grab Cursor", "");
	RNA_def_property_update(prop, NC_SCREEN | NA_EDITED, NULL);

	/* WM_GIZMO_DRAW_HOVER */
	prop = RNA_def_property(srna, "use_draw_hover", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(
	        prop, "rna_Gizmo_flag_use_draw_hover_get", "rna_Gizmo_flag_use_draw_hover_set");
	RNA_def_property_ui_text(prop, "Draw Hover", "");
	RNA_def_property_update(prop, NC_SCREEN | NA_EDITED, NULL);
	/* WM_GIZMO_DRAW_MODAL */
	prop = RNA_def_property(srna, "use_draw_modal", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(
	        prop, "rna_Gizmo_flag_use_draw_modal_get", "rna_Gizmo_flag_use_draw_modal_set");
	RNA_def_property_ui_text(prop, "Draw Active", "Draw while dragging");
	RNA_def_property_update(prop, NC_SCREEN | NA_EDITED, NULL);
	/* WM_GIZMO_DRAW_VALUE */
	prop = RNA_def_property(srna, "use_draw_value", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(
	        prop, "rna_Gizmo_flag_use_draw_value_get", "rna_Gizmo_flag_use_draw_value_set");
	RNA_def_property_ui_text(prop, "Draw Value", "Show an indicator for the current value while dragging");
	RNA_def_property_update(prop, NC_SCREEN | NA_EDITED, NULL);
	/* WM_GIZMO_DRAW_OFFSET_SCALE */
	prop = RNA_def_property(srna, "use_draw_offset_scale", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(
	        prop, "rna_Gizmo_flag_use_draw_offset_scale_get", "rna_Gizmo_flag_use_draw_offset_scale_set");
	RNA_def_property_ui_text(prop, "Scale Offset", "Scale the offset matrix (use to apply screen-space offset)");
	RNA_def_property_update(prop, NC_SCREEN | NA_EDITED, NULL);
	/* WM_GIZMO_DRAW_NO_SCALE (negated) */
	prop = RNA_def_property(srna, "use_draw_scale", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(
	        prop, "rna_Gizmo_flag_use_draw_scale_get", "rna_Gizmo_flag_use_draw_scale_set");
	RNA_def_property_ui_text(prop, "Scale", "Use scale when calculating the matrix");
	RNA_def_property_update(prop, NC_SCREEN | NA_EDITED, NULL);
	/* WM_GIZMO_SELECT_BACKGROUND */
	prop = RNA_def_property(srna, "use_select_background", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(
	        prop, "rna_Gizmo_flag_use_select_background_get", "rna_Gizmo_flag_use_select_background_set");
	RNA_def_property_ui_text(prop, "Select Background", "Don't write into the depth buffer");
	RNA_def_property_update(prop, NC_SCREEN | NA_EDITED, NULL);

	/* wmGizmo.state (readonly) */
	/* WM_GIZMO_STATE_HIGHLIGHT */
	prop = RNA_def_property(srna, "is_highlight", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_Gizmo_state_is_highlight_get", NULL);
	RNA_def_property_ui_text(prop, "Highlight", "");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	/* WM_GIZMO_STATE_MODAL */
	prop = RNA_def_property(srna, "is_modal", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_Gizmo_state_is_modal_get", NULL);
	RNA_def_property_ui_text(prop, "Highlight", "");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	/* WM_GIZMO_STATE_SELECT */
	/* (note that setting is involved, needs to handle array) */
	prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_Gizmo_state_select_get", "rna_Gizmo_state_select_set");
	RNA_def_property_ui_text(prop, "Select", "");

	RNA_api_gizmo(srna);

	srna = RNA_def_struct(brna, "GizmoProperties", NULL);
	RNA_def_struct_ui_text(srna, "Gizmo Properties", "Input properties of an Gizmo");
	RNA_def_struct_refine_func(srna, "rna_GizmoProperties_refine");
	RNA_def_struct_idprops_func(srna, "rna_GizmoProperties_idprops");
	RNA_def_struct_flag(srna, STRUCT_NO_DATABLOCK_IDPROPERTIES);
}

static void rna_def_gizmogroup(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	FunctionRNA *func;
	PropertyRNA *parm;

	srna = RNA_def_struct(brna, "GizmoGroup", NULL);
	RNA_def_struct_ui_text(srna, "GizmoGroup", "Storage of an operator being executed, or registered after execution");
	RNA_def_struct_sdna(srna, "wmGizmoGroup");
	RNA_def_struct_refine_func(srna, "rna_GizmoGroup_refine");
#ifdef WITH_PYTHON
	RNA_def_struct_register_funcs(
	        srna,
	        "rna_GizmoGroup_register",
	        "rna_GizmoGroup_unregister",
	        "rna_GizmoGroup_instance");
#endif
	RNA_def_struct_translation_context(srna, BLT_I18NCONTEXT_OPERATOR_DEFAULT);

	/* -------------------------------------------------------------------- */
	/* Registration */

	RNA_define_verify_sdna(0); /* not in sdna */

	prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "type->idname");
	RNA_def_property_string_maxlength(prop, MAX_NAME);
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_GizmoGroup_bl_idname_set");
	RNA_def_property_flag(prop, PROP_REGISTER);
	RNA_def_struct_name_property(srna, prop);

	prop = RNA_def_property(srna, "bl_label", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "type->name");
	RNA_def_property_string_maxlength(prop, MAX_NAME); /* else it uses the pointer size! */
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_GizmoGroup_bl_label_set");
	/* RNA_def_property_clear_flag(prop, PROP_EDITABLE); */
	RNA_def_property_flag(prop, PROP_REGISTER);

	prop = RNA_def_property(srna, "bl_space_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type->gzmap_params.spaceid");
	RNA_def_property_enum_items(prop, rna_enum_space_type_items);
	RNA_def_property_flag(prop, PROP_REGISTER);
	RNA_def_property_ui_text(prop, "Space type", "The space where the panel is going to be used in");

	prop = RNA_def_property(srna, "bl_region_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type->gzmap_params.regionid");
	RNA_def_property_enum_items(prop, rna_enum_region_type_items);
	RNA_def_property_flag(prop, PROP_REGISTER);
	RNA_def_property_ui_text(prop, "Region Type", "The region where the panel is going to be used in");

	prop = RNA_def_property(srna, "bl_owner_id", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "type->owner_id");
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

	/* bl_options */
	static EnumPropertyItem gizmogroup_flag_items[] = {
		{WM_GIZMOGROUPTYPE_3D, "3D", 0, "3D",
		 "Use in 3D viewport"},
		{WM_GIZMOGROUPTYPE_SCALE, "SCALE", 0, "Scale",
		 "Scale to respect zoom (otherwise zoom independent draw size)"},
		{WM_GIZMOGROUPTYPE_DEPTH_3D, "DEPTH_3D", 0, "Depth 3D",
		 "Supports culled depth by other objects in the view"},
		{WM_GIZMOGROUPTYPE_SELECT, "SELECT", 0, "Select",
		 "Supports selection"},
		{WM_GIZMOGROUPTYPE_PERSISTENT, "PERSISTENT", 0, "Persistent",
		 ""},
		{WM_GIZMOGROUPTYPE_DRAW_MODAL_ALL, "SHOW_MODAL_ALL", 0, "Show Modal All",
		 "Show all while interacting"},
		{0, NULL, 0, NULL, NULL}
	};
	prop = RNA_def_property(srna, "bl_options", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type->flag");
	RNA_def_property_enum_items(prop, gizmogroup_flag_items);
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL | PROP_ENUM_FLAG);
	RNA_def_property_ui_text(prop, "Options",  "Options for this operator type");

	RNA_define_verify_sdna(1); /* not in sdna */


	/* Functions */

	/* poll */
	func = RNA_def_function(srna, "poll", NULL);
	RNA_def_function_ui_description(func, "Test if the gizmo group can be called or not");
	RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_REGISTER_OPTIONAL);
	RNA_def_function_return(func, RNA_def_boolean(func, "visible", 1, "", ""));
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

	/* setup_keymap */
	func = RNA_def_function(srna, "setup_keymap", NULL);
	RNA_def_function_ui_description(
	        func,
	        "Initialize keymaps for this gizmo group, use fallback keymap when not present");
	RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_REGISTER_OPTIONAL);
	parm = RNA_def_pointer(func, "keyconfig", "KeyConfig", "", "");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
	/* return */
	parm = RNA_def_pointer(func, "keymap", "KeyMap", "", "");
	RNA_def_property_flag(parm, PROP_NEVER_NULL);
	RNA_def_function_return(func, parm);

	/* setup */
	func = RNA_def_function(srna, "setup", NULL);
	RNA_def_function_ui_description(func, "Create gizmos function for the gizmo group");
	RNA_def_function_flag(func, FUNC_REGISTER);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

	/* refresh */
	func = RNA_def_function(srna, "refresh", NULL);
	RNA_def_function_ui_description(func, "Refresh data (called on common state changes such as selection)");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

	func = RNA_def_function(srna, "draw_prepare", NULL);
	RNA_def_function_ui_description(func, "Run before each redraw");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

	/* -------------------------------------------------------------------- */
	/* Instance Variables */

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_string_funcs(prop, "rna_GizmoGroup_name_get", "rna_GizmoGroup_name_length", NULL);
	RNA_def_property_ui_text(prop, "Name", "");

	prop = RNA_def_property(srna, "has_reports", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE); /* this is 'virtual' property */
	RNA_def_property_boolean_funcs(prop, "rna_GizmoGroup_has_reports_get", NULL);
	RNA_def_property_ui_text(prop, "Has Reports",
	                         "GizmoGroup has a set of reports (warnings and errors) from last execution");


	RNA_define_verify_sdna(0); /* not in sdna */

	prop = RNA_def_property(srna, "gizmos", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "gizmos", NULL);
	RNA_def_property_struct_type(prop, "Gizmo");
	RNA_def_property_collection_funcs(
	        prop, "rna_GizmoGroup_gizmos_begin", "rna_iterator_listbase_next",
	        "rna_iterator_listbase_end", "rna_iterator_listbase_get",
	        NULL, NULL, NULL, NULL);

	RNA_def_property_ui_text(prop, "Gizmos", "List of gizmos in the Gizmo Map");
	rna_def_gizmo(brna, prop);
	rna_def_gizmos(brna, prop);

	RNA_define_verify_sdna(1); /* not in sdna */

	RNA_api_gizmogroup(srna);
}

void RNA_def_wm_gizmo(BlenderRNA *brna)
{
	rna_def_gizmogroup(brna);
}

#endif /* RNA_RUNTIME */
