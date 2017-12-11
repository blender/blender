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

/** \file blender/makesrna/intern/rna_wm_manipulator.c
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

/** \name Manipulator API
 * \{ */

static void rna_manipulator_draw_cb(
        const struct bContext *C, struct wmManipulator *mpr)
{
	extern FunctionRNA rna_Manipulator_draw_func;
	wmManipulatorGroup *mgroup = mpr->parent_mgroup;
	PointerRNA mpr_ptr;
	ParameterList list;
	FunctionRNA *func;
	RNA_pointer_create(NULL, mpr->type->ext.srna, mpr, &mpr_ptr);
	/* RNA_struct_find_function(&mpr_ptr, "draw"); */
	func = &rna_Manipulator_draw_func;
	RNA_parameter_list_create(&list, &mpr_ptr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	mgroup->type->ext.call((bContext *)C, &mpr_ptr, func, &list);
	RNA_parameter_list_free(&list);
}

static void rna_manipulator_draw_select_cb(
        const struct bContext *C, struct wmManipulator *mpr, int select_id)
{
	extern FunctionRNA rna_Manipulator_draw_select_func;
	wmManipulatorGroup *mgroup = mpr->parent_mgroup;
	PointerRNA mpr_ptr;
	ParameterList list;
	FunctionRNA *func;
	RNA_pointer_create(NULL, mpr->type->ext.srna, mpr, &mpr_ptr);
	/* RNA_struct_find_function(&mpr_ptr, "draw_select"); */
	func = &rna_Manipulator_draw_select_func;
	RNA_parameter_list_create(&list, &mpr_ptr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	RNA_parameter_set_lookup(&list, "select_id", &select_id);
	mgroup->type->ext.call((bContext *)C, &mpr_ptr, func, &list);
	RNA_parameter_list_free(&list);
}

static int rna_manipulator_test_select_cb(
        struct bContext *C, struct wmManipulator *mpr, const struct wmEvent *event)
{
	extern FunctionRNA rna_Manipulator_test_select_func;
	wmManipulatorGroup *mgroup = mpr->parent_mgroup;
	PointerRNA mpr_ptr;
	ParameterList list;
	FunctionRNA *func;
	RNA_pointer_create(NULL, mpr->type->ext.srna, mpr, &mpr_ptr);
	/* RNA_struct_find_function(&mpr_ptr, "test_select"); */
	func = &rna_Manipulator_test_select_func;
	RNA_parameter_list_create(&list, &mpr_ptr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	RNA_parameter_set_lookup(&list, "event", &event);
	mgroup->type->ext.call((bContext *)C, &mpr_ptr, func, &list);

	void *ret;
	RNA_parameter_get_lookup(&list, "intersect_id", &ret);
	int intersect_id = *(int *)ret;

	RNA_parameter_list_free(&list);
	return intersect_id;
}

static int rna_manipulator_modal_cb(
        struct bContext *C, struct wmManipulator *mpr, const struct wmEvent *event,
        eWM_ManipulatorTweak tweak_flag)
{
	extern FunctionRNA rna_Manipulator_modal_func;
	wmManipulatorGroup *mgroup = mpr->parent_mgroup;
	PointerRNA mpr_ptr;
	ParameterList list;
	FunctionRNA *func;
	const int tweak_flag_int = tweak_flag;
	RNA_pointer_create(NULL, mpr->type->ext.srna, mpr, &mpr_ptr);
	/* RNA_struct_find_function(&mpr_ptr, "modal"); */
	func = &rna_Manipulator_modal_func;
	RNA_parameter_list_create(&list, &mpr_ptr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	RNA_parameter_set_lookup(&list, "event", &event);
	RNA_parameter_set_lookup(&list, "tweak", &tweak_flag_int);
	mgroup->type->ext.call((bContext *)C, &mpr_ptr, func, &list);

	void *ret;
	RNA_parameter_get_lookup(&list, "result", &ret);
	int ret_enum = *(int *)ret;

	RNA_parameter_list_free(&list);
	return ret_enum;
}

static void rna_manipulator_setup_cb(
        struct wmManipulator *mpr)
{
	extern FunctionRNA rna_Manipulator_setup_func;
	wmManipulatorGroup *mgroup = mpr->parent_mgroup;
	PointerRNA mpr_ptr;
	ParameterList list;
	FunctionRNA *func;
	RNA_pointer_create(NULL, mpr->type->ext.srna, mpr, &mpr_ptr);
	/* RNA_struct_find_function(&mpr_ptr, "setup"); */
	func = &rna_Manipulator_setup_func;
	RNA_parameter_list_create(&list, &mpr_ptr, func);
	mgroup->type->ext.call((bContext *)NULL, &mpr_ptr, func, &list);
	RNA_parameter_list_free(&list);
}


static int rna_manipulator_invoke_cb(
        struct bContext *C, struct wmManipulator *mpr, const struct wmEvent *event)
{
	extern FunctionRNA rna_Manipulator_invoke_func;
	wmManipulatorGroup *mgroup = mpr->parent_mgroup;
	PointerRNA mpr_ptr;
	ParameterList list;
	FunctionRNA *func;
	RNA_pointer_create(NULL, mpr->type->ext.srna, mpr, &mpr_ptr);
	/* RNA_struct_find_function(&mpr_ptr, "invoke"); */
	func = &rna_Manipulator_invoke_func;
	RNA_parameter_list_create(&list, &mpr_ptr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	RNA_parameter_set_lookup(&list, "event", &event);
	mgroup->type->ext.call((bContext *)C, &mpr_ptr, func, &list);

	void *ret;
	RNA_parameter_get_lookup(&list, "result", &ret);
	int ret_enum = *(int *)ret;

	RNA_parameter_list_free(&list);
	return ret_enum;
}

static void rna_manipulator_exit_cb(
        struct bContext *C, struct wmManipulator *mpr, bool cancel)
{
	extern FunctionRNA rna_Manipulator_exit_func;
	wmManipulatorGroup *mgroup = mpr->parent_mgroup;
	PointerRNA mpr_ptr;
	ParameterList list;
	FunctionRNA *func;
	RNA_pointer_create(NULL, mpr->type->ext.srna, mpr, &mpr_ptr);
	/* RNA_struct_find_function(&mpr_ptr, "exit"); */
	func = &rna_Manipulator_exit_func;
	RNA_parameter_list_create(&list, &mpr_ptr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	{
		int cancel_i = cancel;
		RNA_parameter_set_lookup(&list, "cancel", &cancel_i);
	}
	mgroup->type->ext.call((bContext *)C, &mpr_ptr, func, &list);
	RNA_parameter_list_free(&list);
}

static void rna_manipulator_select_refresh_cb(
        struct wmManipulator *mpr)
{
	extern FunctionRNA rna_Manipulator_select_refresh_func;
	wmManipulatorGroup *mgroup = mpr->parent_mgroup;
	PointerRNA mpr_ptr;
	ParameterList list;
	FunctionRNA *func;
	RNA_pointer_create(NULL, mpr->type->ext.srna, mpr, &mpr_ptr);
	/* RNA_struct_find_function(&mpr_ptr, "select_refresh"); */
	func = &rna_Manipulator_select_refresh_func;
	RNA_parameter_list_create(&list, &mpr_ptr, func);
	mgroup->type->ext.call((bContext *)NULL, &mpr_ptr, func, &list);
	RNA_parameter_list_free(&list);
}

/* just to work around 'const char *' warning and to ensure this is a python op */
static void rna_Manipulator_bl_idname_set(PointerRNA *ptr, const char *value)
{
	wmManipulator *data = ptr->data;
	char *str = (char *)data->type->idname;
	if (!str[0]) {
		BLI_strncpy(str, value, MAX_NAME);    /* utf8 already ensured */
	}
	else {
		assert(!"setting the bl_idname on a non-builtin operator");
	}
}

static wmManipulator *rna_ManipulatorProperties_find_operator(PointerRNA *ptr)
{
#if 0
	wmWindowManager *wm = ptr->id.data;
#endif

	/* We could try workaruond this lookup, but not trivial. */
	for (bScreen *screen = G.main->screen.first; screen; screen = screen->id.next) {
		IDProperty *properties = ptr->data;
		for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
			for (ARegion *ar = sa->regionbase.first; ar; ar = ar->next) {
				if (ar->manipulator_map) {
					wmManipulatorMap *mmap = ar->manipulator_map;
					for (wmManipulatorGroup *mgroup = WM_manipulatormap_group_list(mmap)->first;
					     mgroup;
					     mgroup = mgroup->next)
					{
						for (wmManipulator *mpr = mgroup->manipulators.first; mpr; mpr = mpr->next) {
							if (mpr->properties == properties) {
								return mpr;
							}
						}
					}
				}
			}
		}
	}
	return NULL;
}

static StructRNA *rna_ManipulatorProperties_refine(PointerRNA *ptr)
{
	wmManipulator *mpr = rna_ManipulatorProperties_find_operator(ptr);

	if (mpr)
		return mpr->type->srna;
	else
		return ptr->type;
}

static IDProperty *rna_ManipulatorProperties_idprops(PointerRNA *ptr, bool create)
{
	if (create && !ptr->data) {
		IDPropertyTemplate val = {0};
		ptr->data = IDP_New(IDP_GROUP, &val, "RNA_ManipulatorProperties group");
	}

	return ptr->data;
}

static PointerRNA rna_Manipulator_properties_get(PointerRNA *ptr)
{
	wmManipulator *mpr = ptr->data;
	return rna_pointer_inherit_refine(ptr, mpr->type->srna, mpr->properties);
}

/* wmManipulator.float */
#define RNA_MANIPULATOR_GENERIC_FLOAT_RW_DEF(func_id, member_id) \
static float rna_Manipulator_##func_id##_get(PointerRNA *ptr) \
{ \
	wmManipulator *mpr = ptr->data; \
	return mpr->member_id; \
} \
static void rna_Manipulator_##func_id##_set(PointerRNA *ptr, float value) \
{ \
	wmManipulator *mpr = ptr->data; \
	mpr->member_id = value; \
}
#define RNA_MANIPULATOR_GENERIC_FLOAT_ARRAY_INDEX_RW_DEF(func_id, member_id, index) \
static float rna_Manipulator_##func_id##_get(PointerRNA *ptr) \
{ \
	wmManipulator *mpr = ptr->data; \
	return mpr->member_id[index]; \
} \
static void rna_Manipulator_##func_id##_set(PointerRNA *ptr, float value) \
{ \
	wmManipulator *mpr = ptr->data; \
	mpr->member_id[index] = value; \
}
/* wmManipulator.float[len] */
#define RNA_MANIPULATOR_GENERIC_FLOAT_ARRAY_RW_DEF(func_id, member_id, len) \
static void rna_Manipulator_##func_id##_get(PointerRNA *ptr, float value[len]) \
{ \
	wmManipulator *mpr = ptr->data; \
	memcpy(value, mpr->member_id, sizeof(float[len])); \
} \
static void rna_Manipulator_##func_id##_set(PointerRNA *ptr, const float value[len]) \
{ \
	wmManipulator *mpr = ptr->data; \
	memcpy(mpr->member_id, value, sizeof(float[len])); \
}

/* wmManipulator.flag */
#define RNA_MANIPULATOR_GENERIC_FLAG_RW_DEF(func_id, member_id, flag_value) \
static int rna_Manipulator_##func_id##_get(PointerRNA *ptr) \
{ \
	wmManipulator *mpr = ptr->data; \
	return (mpr->member_id & flag_value) != 0; \
} \
static void rna_Manipulator_##func_id##_set(PointerRNA *ptr, int value) \
{ \
	wmManipulator *mpr = ptr->data; \
	SET_FLAG_FROM_TEST(mpr->member_id, value, flag_value); \
}

/* wmManipulator.flag (negative) */
#define RNA_MANIPULATOR_GENERIC_FLAG_NEG_RW_DEF(func_id, member_id, flag_value) \
static int rna_Manipulator_##func_id##_get(PointerRNA *ptr) \
{ \
	wmManipulator *mpr = ptr->data; \
	return (mpr->member_id & flag_value) == 0; \
} \
static void rna_Manipulator_##func_id##_set(PointerRNA *ptr, int value) \
{ \
	wmManipulator *mpr = ptr->data; \
	SET_FLAG_FROM_TEST(mpr->member_id, !value, flag_value); \
}

#define RNA_MANIPULATOR_FLAG_RO_DEF(func_id, member_id, flag_value) \
static int rna_Manipulator_##func_id##_get(PointerRNA *ptr) \
{ \
	wmManipulator *mpr = ptr->data; \
	return (mpr->member_id & flag_value) != 0; \
}

RNA_MANIPULATOR_GENERIC_FLOAT_ARRAY_RW_DEF(color, color, 3);
RNA_MANIPULATOR_GENERIC_FLOAT_ARRAY_RW_DEF(color_hi, color_hi, 3);

RNA_MANIPULATOR_GENERIC_FLOAT_ARRAY_INDEX_RW_DEF(alpha, color, 3);
RNA_MANIPULATOR_GENERIC_FLOAT_ARRAY_INDEX_RW_DEF(alpha_hi, color_hi, 3);

RNA_MANIPULATOR_GENERIC_FLOAT_ARRAY_RW_DEF(matrix_space, matrix_space, 16);
RNA_MANIPULATOR_GENERIC_FLOAT_ARRAY_RW_DEF(matrix_basis, matrix_basis, 16);
RNA_MANIPULATOR_GENERIC_FLOAT_ARRAY_RW_DEF(matrix_offset, matrix_offset, 16);

static void rna_Manipulator_matrix_world_get(PointerRNA *ptr, float value[16])
{
	wmManipulator *mpr = ptr->data;
	WM_manipulator_calc_matrix_final(mpr, (float (*)[4])value);
}

RNA_MANIPULATOR_GENERIC_FLOAT_RW_DEF(scale_basis, scale_basis);
RNA_MANIPULATOR_GENERIC_FLOAT_RW_DEF(line_width, line_width);

RNA_MANIPULATOR_GENERIC_FLAG_RW_DEF(flag_use_draw_hover, flag, WM_MANIPULATOR_DRAW_HOVER);
RNA_MANIPULATOR_GENERIC_FLAG_RW_DEF(flag_use_draw_modal, flag, WM_MANIPULATOR_DRAW_MODAL);
RNA_MANIPULATOR_GENERIC_FLAG_RW_DEF(flag_use_draw_value, flag, WM_MANIPULATOR_DRAW_VALUE);
RNA_MANIPULATOR_GENERIC_FLAG_RW_DEF(flag_use_draw_offset_scale, flag, WM_MANIPULATOR_DRAW_OFFSET_SCALE);
RNA_MANIPULATOR_GENERIC_FLAG_NEG_RW_DEF(flag_use_draw_scale, flag, WM_MANIPULATOR_DRAW_OFFSET_SCALE);
RNA_MANIPULATOR_GENERIC_FLAG_RW_DEF(flag_hide, flag, WM_MANIPULATOR_HIDDEN);
RNA_MANIPULATOR_GENERIC_FLAG_RW_DEF(flag_use_grab_cursor, flag, WM_MANIPULATOR_GRAB_CURSOR);

/* wmManipulator.state */
RNA_MANIPULATOR_FLAG_RO_DEF(state_is_highlight, state, WM_MANIPULATOR_STATE_HIGHLIGHT);
RNA_MANIPULATOR_FLAG_RO_DEF(state_is_modal, state, WM_MANIPULATOR_STATE_MODAL);
RNA_MANIPULATOR_FLAG_RO_DEF(state_select, state, WM_MANIPULATOR_STATE_SELECT);

static void rna_Manipulator_state_select_set(struct PointerRNA *ptr, int value)
{
	wmManipulator *mpr = ptr->data;
	wmManipulatorGroup *mgroup = mpr->parent_mgroup;
	WM_manipulator_select_set(mgroup->parent_mmap, mpr, value);
}

static PointerRNA rna_Manipulator_group_get(PointerRNA *ptr)
{
	wmManipulator *mpr = ptr->data;
	return rna_pointer_inherit_refine(ptr, &RNA_ManipulatorGroup, mpr->parent_mgroup);
}

#ifdef WITH_PYTHON

static void rna_Manipulator_unregister(struct Main *bmain, StructRNA *type);
void BPY_RNA_manipulator_wrapper(wmManipulatorType *wgt, void *userdata);

static StructRNA *rna_Manipulator_register(
        Main *bmain, ReportList *reports, void *data, const char *identifier,
        StructValidateFunc validate, StructCallbackFunc call, StructFreeFunc free)
{
	struct {
		char idname[MAX_NAME];
	} temp_buffers;

	wmManipulatorType dummywt = {NULL};
	wmManipulator dummymnp = {NULL};
	PointerRNA mnp_ptr;

	/* Two sets of functions. */
	int have_function[8];

	/* setup dummy manipulator & manipulator type to store static properties in */
	dummymnp.type = &dummywt;
	dummywt.idname = temp_buffers.idname;
	RNA_pointer_create(NULL, &RNA_Manipulator, &dummymnp, &mnp_ptr);

	/* Clear so we can detect if it's left unset. */
	temp_buffers.idname[0] = '\0';

	/* validate the python class */
	if (validate(&mnp_ptr, data, have_function) != 0)
		return NULL;

	if (strlen(identifier) >= sizeof(temp_buffers.idname)) {
		BKE_reportf(reports, RPT_ERROR, "Registering manipulator class: '%s' is too long, maximum length is %d",
		            identifier, (int)sizeof(temp_buffers.idname));
		return NULL;
	}

	/* check if we have registered this manipulator type before, and remove it */
	{
		const wmManipulatorType *wt = WM_manipulatortype_find(dummywt.idname, true);
		if (wt && wt->ext.srna) {
			rna_Manipulator_unregister(bmain, wt->ext.srna);
		}
	}
	if (!RNA_struct_available_or_report(reports, dummywt.idname)) {
		return NULL;
	}

	{   /* allocate the idname */
		/* For multiple strings see ManipulatorGroup. */
		dummywt.idname = BLI_strdup(temp_buffers.idname);
	}

	/* create a new manipulator type */
	dummywt.ext.srna = RNA_def_struct_ptr(&BLENDER_RNA, dummywt.idname, &RNA_Manipulator);
	/* manipulator properties are registered separately */
	RNA_def_struct_flag(dummywt.ext.srna, STRUCT_NO_IDPROPERTIES);
	dummywt.ext.data = data;
	dummywt.ext.call = call;
	dummywt.ext.free = free;

	{
		int i = 0;
		dummywt.draw = (have_function[i++]) ? rna_manipulator_draw_cb : NULL;
		dummywt.draw_select = (have_function[i++]) ? rna_manipulator_draw_select_cb : NULL;
		dummywt.test_select = (have_function[i++]) ? rna_manipulator_test_select_cb : NULL;
		dummywt.modal = (have_function[i++]) ? rna_manipulator_modal_cb : NULL;
//		dummywt.property_update = (have_function[i++]) ? rna_manipulator_property_update : NULL;
//		dummywt.position_get = (have_function[i++]) ? rna_manipulator_position_get : NULL;
		dummywt.setup = (have_function[i++]) ? rna_manipulator_setup_cb : NULL;
		dummywt.invoke = (have_function[i++]) ? rna_manipulator_invoke_cb : NULL;
		dummywt.exit = (have_function[i++]) ? rna_manipulator_exit_cb : NULL;
		dummywt.select_refresh = (have_function[i++]) ? rna_manipulator_select_refresh_cb : NULL;

		BLI_assert(i == ARRAY_SIZE(have_function));
	}

	WM_manipulatortype_append_ptr(BPY_RNA_manipulator_wrapper, (void *)&dummywt);

	/* update while blender is running */
	WM_main_add_notifier(NC_SCREEN | NA_EDITED, NULL);

	return dummywt.ext.srna;
}

static void rna_Manipulator_unregister(struct Main *bmain, StructRNA *type)
{
	wmManipulatorType *wt = RNA_struct_blender_type_get(type);

	if (!wt)
		return;

	RNA_struct_free_extension(type, &wt->ext);
	RNA_struct_free(&BLENDER_RNA, type);

	WM_main_add_notifier(NC_SCREEN | NA_EDITED, NULL);

	WM_manipulatortype_remove_ptr(NULL, bmain, wt);
}

static void **rna_Manipulator_instance(PointerRNA *ptr)
{
	wmManipulator *mpr = ptr->data;
	return &mpr->py_instance;
}

#endif  /* WITH_PYTHON */


static StructRNA *rna_Manipulator_refine(PointerRNA *mnp_ptr)
{
	wmManipulator *mpr = mnp_ptr->data;
	return (mpr->type && mpr->type->ext.srna) ? mpr->type->ext.srna : &RNA_Manipulator;
}

/** \} */

/** \name Manipulator Group API
 * \{ */

static wmManipulator *rna_ManipulatorGroup_manipulator_new(
        wmManipulatorGroup *mgroup, ReportList *reports, const char *idname)
{
	const wmManipulatorType *wt = WM_manipulatortype_find(idname, true);
	if (wt == NULL) {
		BKE_reportf(reports, RPT_ERROR, "ManipulatorType '%s' not known", idname);
		return NULL;
	}
	wmManipulator *mpr = WM_manipulator_new_ptr(wt, mgroup, NULL);
	return mpr;
}

static void rna_ManipulatorGroup_manipulator_remove(
        wmManipulatorGroup *mgroup, bContext *C, wmManipulator *mpr)
{
	WM_manipulator_unlink(&mgroup->manipulators, mgroup->parent_mmap, mpr, C);
}

static void rna_ManipulatorGroup_manipulator_clear(
        wmManipulatorGroup *mgroup, bContext *C)
{
	while (mgroup->manipulators.first) {
		WM_manipulator_unlink(&mgroup->manipulators, mgroup->parent_mmap, mgroup->manipulators.first, C);
	}
}

static void rna_ManipulatorGroup_name_get(PointerRNA *ptr, char *value)
{
	wmManipulatorGroup *mgroup = ptr->data;
	strcpy(value, mgroup->type->name);
}

static int rna_ManipulatorGroup_name_length(PointerRNA *ptr)
{
	wmManipulatorGroup *mgroup = ptr->data;
	return strlen(mgroup->type->name);
}

/* just to work around 'const char *' warning and to ensure this is a python op */
static void rna_ManipulatorGroup_bl_idname_set(PointerRNA *ptr, const char *value)
{
	wmManipulatorGroup *data = ptr->data;
	char *str = (char *)data->type->idname;
	if (!str[0])
		BLI_strncpy(str, value, MAX_NAME);    /* utf8 already ensured */
	else
		assert(!"setting the bl_idname on a non-builtin operator");
}

static void rna_ManipulatorGroup_bl_label_set(PointerRNA *ptr, const char *value)
{
	wmManipulatorGroup *data = ptr->data;
	char *str = (char *)data->type->name;
	if (!str[0])
		BLI_strncpy(str, value, MAX_NAME);    /* utf8 already ensured */
	else
		assert(!"setting the bl_label on a non-builtin operator");
}

static int rna_ManipulatorGroup_has_reports_get(PointerRNA *ptr)
{
	wmManipulatorGroup *mgroup = ptr->data;
	return (mgroup->reports && mgroup->reports->list.first);
}

#ifdef WITH_PYTHON

static bool rna_manipulatorgroup_poll_cb(const bContext *C, wmManipulatorGroupType *wgt)
{

	extern FunctionRNA rna_ManipulatorGroup_poll_func;

	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;
	void *ret;
	int visible;

	RNA_pointer_create(NULL, wgt->ext.srna, NULL, &ptr); /* dummy */
	func = &rna_ManipulatorGroup_poll_func; /* RNA_struct_find_function(&ptr, "poll"); */

	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	wgt->ext.call((bContext *)C, &ptr, func, &list);

	RNA_parameter_get_lookup(&list, "visible", &ret);
	visible = *(int *)ret;

	RNA_parameter_list_free(&list);

	return visible;
}

static void rna_manipulatorgroup_setup_cb(const bContext *C, wmManipulatorGroup *mgroup)
{
	extern FunctionRNA rna_ManipulatorGroup_setup_func;

	PointerRNA mgroup_ptr;
	ParameterList list;
	FunctionRNA *func;

	RNA_pointer_create(NULL, mgroup->type->ext.srna, mgroup, &mgroup_ptr);
	func = &rna_ManipulatorGroup_setup_func; /* RNA_struct_find_function(&wgroupr, "setup"); */

	RNA_parameter_list_create(&list, &mgroup_ptr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	mgroup->type->ext.call((bContext *)C, &mgroup_ptr, func, &list);

	RNA_parameter_list_free(&list);
}

static wmKeyMap *rna_manipulatorgroup_setup_keymap_cb(const wmManipulatorGroupType *wgt, wmKeyConfig *config)
{
	extern FunctionRNA rna_ManipulatorGroup_setup_keymap_func;
	void *ret;

	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;

	RNA_pointer_create(NULL, wgt->ext.srna, NULL, &ptr); /* dummy */
	func = &rna_ManipulatorGroup_setup_keymap_func; /* RNA_struct_find_function(&wgroupr, "setup_keymap"); */

	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "keyconfig", &config);
	wgt->ext.call(NULL, &ptr, func, &list);

	RNA_parameter_get_lookup(&list, "keymap", &ret);
	wmKeyMap *keymap = *(wmKeyMap **)ret;

	RNA_parameter_list_free(&list);

	return keymap;
}

static void rna_manipulatorgroup_refresh_cb(const bContext *C, wmManipulatorGroup *mgroup)
{
	extern FunctionRNA rna_ManipulatorGroup_refresh_func;

	PointerRNA mgroup_ptr;
	ParameterList list;
	FunctionRNA *func;

	RNA_pointer_create(NULL, mgroup->type->ext.srna, mgroup, &mgroup_ptr);
	func = &rna_ManipulatorGroup_refresh_func; /* RNA_struct_find_function(&wgroupr, "refresh"); */

	RNA_parameter_list_create(&list, &mgroup_ptr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	mgroup->type->ext.call((bContext *)C, &mgroup_ptr, func, &list);

	RNA_parameter_list_free(&list);
}

static void rna_manipulatorgroup_draw_prepare_cb(const bContext *C, wmManipulatorGroup *mgroup)
{
	extern FunctionRNA rna_ManipulatorGroup_draw_prepare_func;

	PointerRNA mgroup_ptr;
	ParameterList list;
	FunctionRNA *func;

	RNA_pointer_create(NULL, mgroup->type->ext.srna, mgroup, &mgroup_ptr);
	func = &rna_ManipulatorGroup_draw_prepare_func; /* RNA_struct_find_function(&wgroupr, "draw_prepare"); */

	RNA_parameter_list_create(&list, &mgroup_ptr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	mgroup->type->ext.call((bContext *)C, &mgroup_ptr, func, &list);

	RNA_parameter_list_free(&list);
}

void BPY_RNA_manipulatorgroup_wrapper(wmManipulatorGroupType *wgt, void *userdata);
static void rna_ManipulatorGroup_unregister(struct Main *bmain, StructRNA *type);

static StructRNA *rna_ManipulatorGroup_register(
        Main *bmain, ReportList *reports, void *data, const char *identifier,
        StructValidateFunc validate, StructCallbackFunc call, StructFreeFunc free)
{
	struct {
		char name[MAX_NAME];
		char idname[MAX_NAME];
	} temp_buffers;

	wmManipulatorGroupType dummywgt = {NULL};
	wmManipulatorGroup dummywg = {NULL};
	PointerRNA wgptr;

	/* Two sets of functions. */
	int have_function[5];

	/* setup dummy manipulatorgroup & manipulatorgroup type to store static properties in */
	dummywg.type = &dummywgt;
	dummywgt.name = temp_buffers.name;
	dummywgt.idname = temp_buffers.idname;

	RNA_pointer_create(NULL, &RNA_ManipulatorGroup, &dummywg, &wgptr);

	/* Clear so we can detect if it's left unset. */
	temp_buffers.idname[0] = temp_buffers.name[0] = '\0';

	/* validate the python class */
	if (validate(&wgptr, data, have_function) != 0)
		return NULL;

	if (strlen(identifier) >= sizeof(temp_buffers.idname)) {
		BKE_reportf(reports, RPT_ERROR, "Registering manipulatorgroup class: '%s' is too long, maximum length is %d",
		            identifier, (int)sizeof(temp_buffers.idname));
		return NULL;
	}

	/* check if the area supports widgets */
	const struct wmManipulatorMapType_Params wmap_params = {
		.spaceid = dummywgt.mmap_params.spaceid,
		.regionid = dummywgt.mmap_params.regionid,
	};

	wmManipulatorMapType *mmap_type = WM_manipulatormaptype_ensure(&wmap_params);
	if (mmap_type == NULL) {
		BKE_reportf(reports, RPT_ERROR, "Area type does not support manipulators");
		return NULL;
	}

	/* check if we have registered this manipulatorgroup type before, and remove it */
	{
		wmManipulatorGroupType *wgt = WM_manipulatorgrouptype_find(dummywgt.idname, true);
		if (wgt && wgt->ext.srna) {
			rna_ManipulatorGroup_unregister(bmain, wgt->ext.srna);
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

	/* create a new manipulatorgroup type */
	dummywgt.ext.srna = RNA_def_struct_ptr(&BLENDER_RNA, dummywgt.idname, &RNA_ManipulatorGroup);
	RNA_def_struct_flag(dummywgt.ext.srna, STRUCT_NO_IDPROPERTIES); /* manipulatorgroup properties are registered separately */
	dummywgt.ext.data = data;
	dummywgt.ext.call = call;
	dummywgt.ext.free = free;

	/* We used to register widget group types like this, now we do it similar to
	 * operator types. Thus we should be able to do the same as operator types now. */
	dummywgt.poll = (have_function[0]) ? rna_manipulatorgroup_poll_cb : NULL;
	dummywgt.setup_keymap =     (have_function[1]) ? rna_manipulatorgroup_setup_keymap_cb : NULL;
	dummywgt.setup =            (have_function[2]) ? rna_manipulatorgroup_setup_cb : NULL;
	dummywgt.refresh =          (have_function[3]) ? rna_manipulatorgroup_refresh_cb : NULL;
	dummywgt.draw_prepare =     (have_function[4]) ? rna_manipulatorgroup_draw_prepare_cb : NULL;

	wmManipulatorGroupType *wgt = WM_manipulatorgrouptype_append_ptr(
	        BPY_RNA_manipulatorgroup_wrapper, (void *)&dummywgt);

	if (wgt->flag & WM_MANIPULATORGROUPTYPE_PERSISTENT) {
		WM_manipulator_group_type_add_ptr_ex(wgt, mmap_type);

		/* update while blender is running */
		WM_main_add_notifier(NC_SCREEN | NA_EDITED, NULL);
	}

	return dummywgt.ext.srna;
}

static void rna_ManipulatorGroup_unregister(struct Main *bmain, StructRNA *type)
{
	wmManipulatorGroupType *wgt = RNA_struct_blender_type_get(type);

	if (!wgt)
		return;

	RNA_struct_free_extension(type, &wgt->ext);
	RNA_struct_free(&BLENDER_RNA, type);

	WM_main_add_notifier(NC_SCREEN | NA_EDITED, NULL);

	WM_manipulator_group_type_remove_ptr(bmain, wgt);
}

static void **rna_ManipulatorGroup_instance(PointerRNA *ptr)
{
	wmManipulatorGroup *mgroup = ptr->data;
	return &mgroup->py_instance;
}

#endif  /* WITH_PYTHON */

static StructRNA *rna_ManipulatorGroup_refine(PointerRNA *mgroup_ptr)
{
	wmManipulatorGroup *mgroup = mgroup_ptr->data;
	return (mgroup->type && mgroup->type->ext.srna) ? mgroup->type->ext.srna : &RNA_ManipulatorGroup;
}

static void rna_ManipulatorGroup_manipulators_begin(CollectionPropertyIterator *iter, PointerRNA *mgroup_ptr)
{
	wmManipulatorGroup *mgroup = mgroup_ptr->data;
	rna_iterator_listbase_begin(iter, &mgroup->manipulators, NULL);
}

/** \} */


#else /* RNA_RUNTIME */


/* ManipulatorGroup.manipulators */
static void rna_def_manipulators(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;

	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "Manipulators");
	srna = RNA_def_struct(brna, "Manipulators", NULL);
	RNA_def_struct_sdna(srna, "wmManipulatorGroup");
	RNA_def_struct_ui_text(srna, "Manipulators", "Collection of manipulators");

	func = RNA_def_function(srna, "new", "rna_ManipulatorGroup_manipulator_new");
	RNA_def_function_ui_description(func, "Add manipulator");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	RNA_def_string(func, "type", "Type", 0, "", "Manipulator identifier"); /* optional */
	parm = RNA_def_pointer(func, "manipulator", "Manipulator", "", "New manipulator");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "remove", "rna_ManipulatorGroup_manipulator_remove");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	RNA_def_function_ui_description(func, "Delete manipulator");
	parm = RNA_def_pointer(func, "manipulator", "Manipulator", "", "New manipulator");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
	RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);

	func = RNA_def_function(srna, "clear", "rna_ManipulatorGroup_manipulator_clear");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	RNA_def_function_ui_description(func, "Delete all manipulators");
}


static void rna_def_manipulator(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *prop;

	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "Manipulator");
	srna = RNA_def_struct(brna, "Manipulator", NULL);
	RNA_def_struct_sdna(srna, "wmManipulator");
	RNA_def_struct_ui_text(srna, "Manipulator", "Collection of manipulators");
	RNA_def_struct_refine_func(srna, "rna_Manipulator_refine");

#ifdef WITH_PYTHON
	RNA_def_struct_register_funcs(
	        srna,
	        "rna_Manipulator_register",
	        "rna_Manipulator_unregister",
	        "rna_Manipulator_instance");
#endif
	RNA_def_struct_translation_context(srna, BLT_I18NCONTEXT_OPERATOR_DEFAULT);

	prop = RNA_def_property(srna, "properties", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_struct_type(prop, "ManipulatorProperties");
	RNA_def_property_ui_text(prop, "Properties", "");
	RNA_def_property_pointer_funcs(prop, "rna_Manipulator_properties_get", NULL, NULL, NULL);

	/* -------------------------------------------------------------------- */
	/* Registerable Variables */

	RNA_define_verify_sdna(0); /* not in sdna */

	prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "type->idname");
	RNA_def_property_string_maxlength(prop, MAX_NAME);
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_Manipulator_bl_idname_set");
	/* RNA_def_property_clear_flag(prop, PROP_EDITABLE); */
	RNA_def_property_flag(prop, PROP_REGISTER);

	RNA_define_verify_sdna(1); /* not in sdna */

	/* wmManipulator.draw */
	func = RNA_def_function(srna, "draw", NULL);
	RNA_def_function_ui_description(func, "");
	RNA_def_function_flag(func, FUNC_REGISTER);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

	/* wmManipulator.draw_select */
	func = RNA_def_function(srna, "draw_select", NULL);
	RNA_def_function_ui_description(func, "");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
	parm = RNA_def_int(func, "select_id", 0, 0, INT_MAX, "", "", 0, INT_MAX);

	/* wmManipulator.test_select */
	func = RNA_def_function(srna, "test_select", NULL);
	RNA_def_function_ui_description(func, "");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
	parm = RNA_def_pointer(func, "event", "Event", "", "");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
	parm = RNA_def_int(func, "intersect_id", 0, 0, INT_MAX, "", "", 0, INT_MAX);
	RNA_def_function_return(func, parm);

	/* wmManipulator.handler */
	static EnumPropertyItem tweak_actions[] = {
		{WM_MANIPULATOR_TWEAK_PRECISE, "PRECISE", 0, "Precise", ""},
		{WM_MANIPULATOR_TWEAK_SNAP, "SNAP", 0, "Snap", ""},
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
	/* wmManipulator.property_update */
	/* TODO */

	/* wmManipulator.setup */
	func = RNA_def_function(srna, "setup", NULL);
	RNA_def_function_ui_description(func, "");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);

	/* wmManipulator.invoke */
	func = RNA_def_function(srna, "invoke", NULL);
	RNA_def_function_ui_description(func, "");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
	parm = RNA_def_pointer(func, "event", "Event", "", "");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
	parm = RNA_def_enum_flag(func, "result", rna_enum_operator_return_items, OPERATOR_CANCELLED, "result", "");
	RNA_def_function_return(func, parm);

	/* wmManipulator.exit */
	func = RNA_def_function(srna, "exit", NULL);
	RNA_def_function_ui_description(func, "");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
	parm = RNA_def_boolean(func, "cancel", 0, "Cancel, otherwise confirm", "");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

	/* wmManipulator.cursor_get */
	/* TODO */

	/* wmManipulator.select_refresh */
	func = RNA_def_function(srna, "select_refresh", NULL);
	RNA_def_function_ui_description(func, "");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);


	/* -------------------------------------------------------------------- */
	/* Instance Variables */

	prop = RNA_def_property(srna, "group", PROP_POINTER, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "ManipulatorGroup");
	RNA_def_property_pointer_funcs(prop, "rna_Manipulator_group_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "", "Manipulator group this manipulator is a member of");

	/* Color & Alpha */
	prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_funcs(prop, "rna_Manipulator_color_get", "rna_Manipulator_color_set", NULL);

	prop = RNA_def_property(srna, "alpha", PROP_FLOAT, PROP_NONE);
	RNA_def_property_ui_text(prop, "Alpha", "");
	RNA_def_property_float_funcs(prop, "rna_Manipulator_alpha_get", "rna_Manipulator_alpha_set", NULL);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_update(prop, NC_SCREEN | NA_EDITED, NULL);

	/* Color & Alpha (highlight) */
	prop = RNA_def_property(srna, "color_highlight", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_funcs(prop, "rna_Manipulator_color_hi_get", "rna_Manipulator_color_hi_set", NULL);

	prop = RNA_def_property(srna, "alpha_highlight", PROP_FLOAT, PROP_NONE);
	RNA_def_property_ui_text(prop, "Alpha", "");
	RNA_def_property_float_funcs(prop, "rna_Manipulator_alpha_hi_get", "rna_Manipulator_alpha_hi_set", NULL);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_update(prop, NC_SCREEN | NA_EDITED, NULL);

	prop = RNA_def_property(srna, "matrix_space", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
	RNA_def_property_ui_text(prop, "Space Matrix", "");
	RNA_def_property_float_funcs(prop, "rna_Manipulator_matrix_space_get", "rna_Manipulator_matrix_space_set", NULL);
	RNA_def_property_update(prop, NC_SCREEN | NA_EDITED, NULL);

	prop = RNA_def_property(srna, "matrix_basis", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
	RNA_def_property_ui_text(prop, "Basis Matrix", "");
	RNA_def_property_float_funcs(prop, "rna_Manipulator_matrix_basis_get", "rna_Manipulator_matrix_basis_set", NULL);
	RNA_def_property_update(prop, NC_SCREEN | NA_EDITED, NULL);

	prop = RNA_def_property(srna, "matrix_offset", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
	RNA_def_property_ui_text(prop, "Offset Matrix", "");
	RNA_def_property_float_funcs(prop, "rna_Manipulator_matrix_offset_get", "rna_Manipulator_matrix_offset_set", NULL);
	RNA_def_property_update(prop, NC_SCREEN | NA_EDITED, NULL);

	prop = RNA_def_property(srna, "matrix_world", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
	RNA_def_property_ui_text(prop, "Final World Matrix", "");
	RNA_def_property_float_funcs(prop, "rna_Manipulator_matrix_world_get", NULL, NULL);

	prop = RNA_def_property(srna, "scale_basis", PROP_FLOAT, PROP_NONE);
	RNA_def_property_ui_text(prop, "Scale Basis", "");
	RNA_def_property_float_funcs(prop, "rna_Manipulator_scale_basis_get", "rna_Manipulator_scale_basis_set", NULL);
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_update(prop, NC_SCREEN | NA_EDITED, NULL);

	prop = RNA_def_property(srna, "line_width", PROP_FLOAT, PROP_PIXEL);
	RNA_def_property_ui_text(prop, "Line Width", "");
	RNA_def_property_float_funcs(prop, "rna_Manipulator_line_width_get", "rna_Manipulator_line_width_set", NULL);
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_update(prop, NC_SCREEN | NA_EDITED, NULL);

	/* wmManipulator.flag */
	/* WM_MANIPULATOR_HIDDEN */
	prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(
	        prop, "rna_Manipulator_flag_hide_get", "rna_Manipulator_flag_hide_set");
	RNA_def_property_ui_text(prop, "Hide", "");
	RNA_def_property_update(prop, NC_SCREEN | NA_EDITED, NULL);
	/* WM_MANIPULATOR_GRAB_CURSOR */
	prop = RNA_def_property(srna, "use_grab_cursor", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(
	        prop, "rna_Manipulator_flag_use_grab_cursor_get", "rna_Manipulator_flag_use_grab_cursor_set");
	RNA_def_property_ui_text(prop, "Grab Cursor", "");
	RNA_def_property_update(prop, NC_SCREEN | NA_EDITED, NULL);

	/* WM_MANIPULATOR_DRAW_HOVER */
	prop = RNA_def_property(srna, "use_draw_hover", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(
	        prop, "rna_Manipulator_flag_use_draw_hover_get", "rna_Manipulator_flag_use_draw_hover_set");
	RNA_def_property_ui_text(prop, "Draw Hover", "");
	RNA_def_property_update(prop, NC_SCREEN | NA_EDITED, NULL);
	/* WM_MANIPULATOR_DRAW_MODAL */
	prop = RNA_def_property(srna, "use_draw_modal", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(
	        prop, "rna_Manipulator_flag_use_draw_modal_get", "rna_Manipulator_flag_use_draw_modal_set");
	RNA_def_property_ui_text(prop, "Draw Active", "Draw while dragging");
	RNA_def_property_update(prop, NC_SCREEN | NA_EDITED, NULL);
	/* WM_MANIPULATOR_DRAW_VALUE */
	prop = RNA_def_property(srna, "use_draw_value", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(
	        prop, "rna_Manipulator_flag_use_draw_value_get", "rna_Manipulator_flag_use_draw_value_set");
	RNA_def_property_ui_text(prop, "Draw Value", "Show an indicator for the current value while dragging");
	RNA_def_property_update(prop, NC_SCREEN | NA_EDITED, NULL);
	/* WM_MANIPULATOR_DRAW_OFFSET_SCALE */
	prop = RNA_def_property(srna, "use_draw_offset_scale", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(
	        prop, "rna_Manipulator_flag_use_draw_offset_scale_get", "rna_Manipulator_flag_use_draw_offset_scale_set");
	RNA_def_property_ui_text(prop, "Scale Offset", "Scale the offset matrix (use to apply screen-space offset)");
	RNA_def_property_update(prop, NC_SCREEN | NA_EDITED, NULL);
	/* WM_MANIPULATOR_DRAW_NO_SCALE (negated) */
	prop = RNA_def_property(srna, "use_draw_scale", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(
	        prop, "rna_Manipulator_flag_use_draw_scale_get", "rna_Manipulator_flag_use_draw_scale_set");
	RNA_def_property_ui_text(prop, "Scale", "Use scale when calculating the matrix");
	RNA_def_property_update(prop, NC_SCREEN | NA_EDITED, NULL);

	/* wmManipulator.state (readonly) */
	/* WM_MANIPULATOR_STATE_HIGHLIGHT */
	prop = RNA_def_property(srna, "is_highlight", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_Manipulator_state_is_highlight_get", NULL);
	RNA_def_property_ui_text(prop, "Highlight", "");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	/* WM_MANIPULATOR_STATE_MODAL */
	prop = RNA_def_property(srna, "is_modal", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_Manipulator_state_is_modal_get", NULL);
	RNA_def_property_ui_text(prop, "Highlight", "");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	/* WM_MANIPULATOR_STATE_SELECT */
	/* (note that setting is involved, needs to handle array) */
	prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_Manipulator_state_select_get", "rna_Manipulator_state_select_set");
	RNA_def_property_ui_text(prop, "Select", "");

	RNA_api_manipulator(srna);

	srna = RNA_def_struct(brna, "ManipulatorProperties", NULL);
	RNA_def_struct_ui_text(srna, "Manipulator Properties", "Input properties of an Manipulator");
	RNA_def_struct_refine_func(srna, "rna_ManipulatorProperties_refine");
	RNA_def_struct_idprops_func(srna, "rna_ManipulatorProperties_idprops");
	RNA_def_struct_flag(srna, STRUCT_NO_DATABLOCK_IDPROPERTIES);
}

static void rna_def_manipulatorgroup(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	FunctionRNA *func;
	PropertyRNA *parm;

	srna = RNA_def_struct(brna, "ManipulatorGroup", NULL);
	RNA_def_struct_ui_text(srna, "ManipulatorGroup", "Storage of an operator being executed, or registered after execution");
	RNA_def_struct_sdna(srna, "wmManipulatorGroup");
	RNA_def_struct_refine_func(srna, "rna_ManipulatorGroup_refine");
#ifdef WITH_PYTHON
	RNA_def_struct_register_funcs(
	        srna,
	        "rna_ManipulatorGroup_register",
	        "rna_ManipulatorGroup_unregister",
	        "rna_ManipulatorGroup_instance");
#endif
	RNA_def_struct_translation_context(srna, BLT_I18NCONTEXT_OPERATOR_DEFAULT);

	/* -------------------------------------------------------------------- */
	/* Registration */

	RNA_define_verify_sdna(0); /* not in sdna */

	prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "type->idname");
	RNA_def_property_string_maxlength(prop, MAX_NAME);
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_ManipulatorGroup_bl_idname_set");
	RNA_def_property_flag(prop, PROP_REGISTER);
	RNA_def_struct_name_property(srna, prop);

	prop = RNA_def_property(srna, "bl_label", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "type->name");
	RNA_def_property_string_maxlength(prop, MAX_NAME); /* else it uses the pointer size! */
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_ManipulatorGroup_bl_label_set");
	/* RNA_def_property_clear_flag(prop, PROP_EDITABLE); */
	RNA_def_property_flag(prop, PROP_REGISTER);

	prop = RNA_def_property(srna, "bl_space_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type->mmap_params.spaceid");
	RNA_def_property_enum_items(prop, rna_enum_space_type_items);
	RNA_def_property_flag(prop, PROP_REGISTER);
	RNA_def_property_ui_text(prop, "Space type", "The space where the panel is going to be used in");

	prop = RNA_def_property(srna, "bl_region_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type->mmap_params.regionid");
	RNA_def_property_enum_items(prop, rna_enum_region_type_items);
	RNA_def_property_flag(prop, PROP_REGISTER);
	RNA_def_property_ui_text(prop, "Region Type", "The region where the panel is going to be used in");

	/* bl_options */
	static EnumPropertyItem manipulatorgroup_flag_items[] = {
		{WM_MANIPULATORGROUPTYPE_3D, "3D", 0, "3D",
		 "Use in 3D viewport"},
		{WM_MANIPULATORGROUPTYPE_SCALE, "SCALE", 0, "Scale",
		 "Scale to respect zoom (otherwise zoom independent draw size)"},
		{WM_MANIPULATORGROUPTYPE_DEPTH_3D, "DEPTH_3D", 0, "Depth 3D",
		 "Supports culled depth by other objects in the view"},
		{WM_MANIPULATORGROUPTYPE_SELECT, "SELECT", 0, "Select",
		 "Supports selection"},
		{WM_MANIPULATORGROUPTYPE_PERSISTENT, "PERSISTENT", 0, "Persistent",
		 ""},
		{WM_MANIPULATORGROUPTYPE_DRAW_MODAL_ALL, "SHOW_MODAL_ALL", 0, "Show Modal All",
		 "Show all while interacting"},
		{0, NULL, 0, NULL, NULL}
	};
	prop = RNA_def_property(srna, "bl_options", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type->flag");
	RNA_def_property_enum_items(prop, manipulatorgroup_flag_items);
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL | PROP_ENUM_FLAG);
	RNA_def_property_ui_text(prop, "Options",  "Options for this operator type");

	RNA_define_verify_sdna(1); /* not in sdna */


	/* Functions */

	/* poll */
	func = RNA_def_function(srna, "poll", NULL);
	RNA_def_function_ui_description(func, "Test if the manipulator group can be called or not");
	RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_REGISTER_OPTIONAL);
	RNA_def_function_return(func, RNA_def_boolean(func, "visible", 1, "", ""));
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

	/* setup_keymap */
	func = RNA_def_function(srna, "setup_keymap", NULL);
	RNA_def_function_ui_description(
	        func,
	        "Initialize keymaps for this manipulator group, use fallback keymap when not present");
	RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_REGISTER_OPTIONAL);
	parm = RNA_def_pointer(func, "keyconfig", "KeyConfig", "", "");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
	/* return */
	parm = RNA_def_pointer(func, "keymap", "KeyMap", "", "");
	RNA_def_property_flag(parm, PROP_NEVER_NULL);
	RNA_def_function_return(func, parm);

	/* setup */
	func = RNA_def_function(srna, "setup", NULL);
	RNA_def_function_ui_description(func, "Create manipulators function for the manipulator group");
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
	RNA_def_property_string_funcs(prop, "rna_ManipulatorGroup_name_get", "rna_ManipulatorGroup_name_length", NULL);
	RNA_def_property_ui_text(prop, "Name", "");

	prop = RNA_def_property(srna, "has_reports", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE); /* this is 'virtual' property */
	RNA_def_property_boolean_funcs(prop, "rna_ManipulatorGroup_has_reports_get", NULL);
	RNA_def_property_ui_text(prop, "Has Reports",
	                         "ManipulatorGroup has a set of reports (warnings and errors) from last execution");


	RNA_define_verify_sdna(0); /* not in sdna */

	prop = RNA_def_property(srna, "manipulators", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "manipulators", NULL);
	RNA_def_property_struct_type(prop, "Manipulator");
	RNA_def_property_collection_funcs(
	        prop, "rna_ManipulatorGroup_manipulators_begin", "rna_iterator_listbase_next",
	        "rna_iterator_listbase_end", "rna_iterator_listbase_get",
	        NULL, NULL, NULL, NULL);

	RNA_def_property_ui_text(prop, "Manipulators", "List of manipulators in the Manipulator Map");
	rna_def_manipulator(brna, prop);
	rna_def_manipulators(brna, prop);

	RNA_define_verify_sdna(1); /* not in sdna */

	RNA_api_manipulatorgroup(srna);
}

void RNA_def_wm_manipulator(BlenderRNA *brna)
{
	rna_def_manipulatorgroup(brna);
}

#endif /* RNA_RUNTIME */
