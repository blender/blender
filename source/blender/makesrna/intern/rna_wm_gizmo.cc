/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>

#include "DNA_windowmanager_types.h"

#include "BLT_translation.hh"

#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "rna_internal.hh"

#include "WM_types.hh"

#ifdef RNA_RUNTIME
/* enum definitions */
#endif /* RNA_RUNTIME */

#ifdef RNA_RUNTIME

#  include "BLI_string.h"
#  include "BLI_string_utf8.h"
#  include "BLI_string_utils.hh"

#  include "WM_api.hh"

#  include "ED_screen.hh"

#  include "UI_interface.hh"

#  include "BKE_context.hh"
#  include "BKE_global.hh"
#  include "BKE_main.hh"
#  include "BKE_report.hh"
#  include "BKE_screen.hh"
#  include "BKE_workspace.hh"

#  include "GPU_state.hh"

#  ifdef WITH_PYTHON
#    include "BPY_extern.hh"
#  endif

/* -------------------------------------------------------------------- */
/** \name Gizmo API
 * \{ */

#  ifdef WITH_PYTHON
static void rna_gizmo_draw_cb(const bContext *C, wmGizmo *gz)
{
  extern FunctionRNA rna_Gizmo_draw_func;
  wmGizmoGroup *gzgroup = gz->parent_gzgroup;
  ParameterList list;
  FunctionRNA *func;
  PointerRNA gz_ptr = RNA_pointer_create_discrete(nullptr, gz->type->rna_ext.srna, gz);
  /* Reference `RNA_struct_find_function(&gz_ptr, "draw")` directly. */
  func = &rna_Gizmo_draw_func;
  RNA_parameter_list_create(&list, &gz_ptr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  gzgroup->type->rna_ext.call((bContext *)C, &gz_ptr, func, &list);
  RNA_parameter_list_free(&list);
}

static void rna_gizmo_draw_select_cb(const bContext *C, wmGizmo *gz, int select_id)
{
  extern FunctionRNA rna_Gizmo_draw_select_func;
  wmGizmoGroup *gzgroup = gz->parent_gzgroup;
  ParameterList list;
  FunctionRNA *func;
  PointerRNA gz_ptr = RNA_pointer_create_discrete(nullptr, gz->type->rna_ext.srna, gz);
  /* Reference `RNA_struct_find_function(&gz_ptr, "draw_select")` directly. */
  func = &rna_Gizmo_draw_select_func;
  RNA_parameter_list_create(&list, &gz_ptr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  RNA_parameter_set_lookup(&list, "select_id", &select_id);
  gzgroup->type->rna_ext.call((bContext *)C, &gz_ptr, func, &list);
  RNA_parameter_list_free(&list);
}

static int rna_gizmo_test_select_cb(bContext *C, wmGizmo *gz, const int location[2])
{
  extern FunctionRNA rna_Gizmo_test_select_func;
  wmGizmoGroup *gzgroup = gz->parent_gzgroup;
  ParameterList list;
  FunctionRNA *func;
  PointerRNA gz_ptr = RNA_pointer_create_discrete(nullptr, gz->type->rna_ext.srna, gz);
  /* Reference `RNA_struct_find_function(&gz_ptr, "test_select")` directly. */
  func = &rna_Gizmo_test_select_func;
  RNA_parameter_list_create(&list, &gz_ptr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  RNA_parameter_set_lookup(&list, "location", location);
  gzgroup->type->rna_ext.call(C, &gz_ptr, func, &list);

  void *ret;
  RNA_parameter_get_lookup(&list, "intersect_id", &ret);
  int intersect_id = *(int *)ret;

  RNA_parameter_list_free(&list);
  return intersect_id;
}

static wmOperatorStatus rna_gizmo_modal_cb(bContext *C,
                                           wmGizmo *gz,
                                           const wmEvent *event,
                                           eWM_GizmoFlagTweak tweak_flag)
{
  extern FunctionRNA rna_Gizmo_modal_func;
  wmGizmoGroup *gzgroup = gz->parent_gzgroup;
  ParameterList list;
  FunctionRNA *func;
  const int tweak_flag_int = tweak_flag;
  PointerRNA gz_ptr = RNA_pointer_create_discrete(nullptr, gz->type->rna_ext.srna, gz);
  /* Reference `RNA_struct_find_function(&gz_ptr, "modal")` directly. */
  func = &rna_Gizmo_modal_func;
  RNA_parameter_list_create(&list, &gz_ptr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  RNA_parameter_set_lookup(&list, "event", &event);
  RNA_parameter_set_lookup(&list, "tweak", &tweak_flag_int);
  gzgroup->type->rna_ext.call(C, &gz_ptr, func, &list);

  void *ret;
  RNA_parameter_get_lookup(&list, "result", &ret);
  wmOperatorStatus retval = wmOperatorStatus(*(int *)ret);

  RNA_parameter_list_free(&list);

  OPERATOR_RETVAL_CHECK(retval);
  return retval;
}

static void rna_gizmo_setup_cb(wmGizmo *gz)
{
  extern FunctionRNA rna_Gizmo_setup_func;
  wmGizmoGroup *gzgroup = gz->parent_gzgroup;
  ParameterList list;
  FunctionRNA *func;
  PointerRNA gz_ptr = RNA_pointer_create_discrete(nullptr, gz->type->rna_ext.srna, gz);
  /* Reference `RNA_struct_find_function(&gz_ptr, "setup")` directly. */
  func = &rna_Gizmo_setup_func;
  RNA_parameter_list_create(&list, &gz_ptr, func);
  gzgroup->type->rna_ext.call((bContext *)nullptr, &gz_ptr, func, &list);
  RNA_parameter_list_free(&list);
}

static wmOperatorStatus rna_gizmo_invoke_cb(bContext *C, wmGizmo *gz, const wmEvent *event)
{
  extern FunctionRNA rna_Gizmo_invoke_func;
  wmGizmoGroup *gzgroup = gz->parent_gzgroup;
  ParameterList list;
  FunctionRNA *func;
  PointerRNA gz_ptr = RNA_pointer_create_discrete(nullptr, gz->type->rna_ext.srna, gz);
  /* Reference `RNA_struct_find_function(&gz_ptr, "invoke")` directly. */
  func = &rna_Gizmo_invoke_func;
  RNA_parameter_list_create(&list, &gz_ptr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  RNA_parameter_set_lookup(&list, "event", &event);
  gzgroup->type->rna_ext.call(C, &gz_ptr, func, &list);

  void *ret;
  RNA_parameter_get_lookup(&list, "result", &ret);
  const wmOperatorStatus retval = wmOperatorStatus(*(int *)ret);

  RNA_parameter_list_free(&list);

  OPERATOR_RETVAL_CHECK(retval);
  return retval;
}

static void rna_gizmo_exit_cb(bContext *C, wmGizmo *gz, bool cancel)
{
  extern FunctionRNA rna_Gizmo_exit_func;
  wmGizmoGroup *gzgroup = gz->parent_gzgroup;
  ParameterList list;
  FunctionRNA *func;
  PointerRNA gz_ptr = RNA_pointer_create_discrete(nullptr, gz->type->rna_ext.srna, gz);
  /* Reference `RNA_struct_find_function(&gz_ptr, "exit")` directly. */
  func = &rna_Gizmo_exit_func;
  RNA_parameter_list_create(&list, &gz_ptr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  {
    int cancel_i = cancel;
    RNA_parameter_set_lookup(&list, "cancel", &cancel_i);
  }
  gzgroup->type->rna_ext.call(C, &gz_ptr, func, &list);
  RNA_parameter_list_free(&list);
}

static void rna_gizmo_select_refresh_cb(wmGizmo *gz)
{
  extern FunctionRNA rna_Gizmo_select_refresh_func;
  wmGizmoGroup *gzgroup = gz->parent_gzgroup;
  ParameterList list;
  FunctionRNA *func;
  PointerRNA gz_ptr = RNA_pointer_create_discrete(nullptr, gz->type->rna_ext.srna, gz);
  /* Reference `RNA_struct_find_function(&gz_ptr, "select_refresh")` directly. */
  func = &rna_Gizmo_select_refresh_func;
  RNA_parameter_list_create(&list, &gz_ptr, func);
  gzgroup->type->rna_ext.call((bContext *)nullptr, &gz_ptr, func, &list);
  RNA_parameter_list_free(&list);
}

#  endif /* WITH_PYTHON */

/* just to work around 'const char *' warning and to ensure this is a python op */
static void rna_Gizmo_bl_idname_set(PointerRNA *ptr, const char *value)
{
  wmGizmo *data = static_cast<wmGizmo *>(ptr->data);
  char *str = (char *)data->type->idname;
  if (!str[0]) {
    /* Calling UTF8 copy is disputable since registering ensures the value isn't truncated.
     * Use a UTF8 copy to ensure truncating never causes an incomplete UTF8 sequence,
     * even before registration. */
    BLI_strncpy_utf8(str, value, MAX_NAME);
  }
  else {
    BLI_assert_msg(0, "setting the bl_idname on a non-builtin operator");
  }
}

static void rna_Gizmo_update_redraw(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  wmGizmo *gizmo = static_cast<wmGizmo *>(ptr->data);
  gizmo->do_draw = true;
}

static wmGizmo *rna_GizmoProperties_find_operator(PointerRNA *ptr)
{
#  if 0
  wmWindowManager *wm = (wmWindowManager *)ptr->owner_id;
#  endif

  /* We could try workaround this lookup, but not trivial. */
  for (bScreen *screen = static_cast<bScreen *>(G_MAIN->screens.first); screen;
       screen = static_cast<bScreen *>(screen->id.next))
  {
    IDProperty *properties = static_cast<IDProperty *>(ptr->data);
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
        if (region->runtime->gizmo_map) {
          wmGizmoMap *gzmap = region->runtime->gizmo_map;
          LISTBASE_FOREACH (wmGizmoGroup *, gzgroup, WM_gizmomap_group_list(gzmap)) {
            LISTBASE_FOREACH (wmGizmo *, gz, &gzgroup->gizmos) {
              if (gz->properties == properties) {
                return gz;
              }
            }
          }
        }
      }
    }
  }
  return nullptr;
}

static StructRNA *rna_GizmoProperties_refine(PointerRNA *ptr)
{
  wmGizmo *gz = rna_GizmoProperties_find_operator(ptr);

  if (gz) {
    return gz->type->srna;
  }
  return ptr->type;
}

static IDProperty **rna_GizmoProperties_idprops(PointerRNA *ptr)
{
  return (IDProperty **)&ptr->data;
}

static PointerRNA rna_Gizmo_properties_get(PointerRNA *ptr)
{
  wmGizmo *gz = static_cast<wmGizmo *>(ptr->data);
  return RNA_pointer_create_with_parent(*ptr, gz->type->srna, gz->properties);
}

/* wmGizmo.float */
#  define RNA_GIZMO_GENERIC_FLOAT_RW_DEF(func_id, member_id) \
    static float rna_Gizmo_##func_id##_get(PointerRNA *ptr) \
    { \
      wmGizmo *gz = static_cast<wmGizmo *>(ptr->data); \
      return gz->member_id; \
    } \
    static void rna_Gizmo_##func_id##_set(PointerRNA *ptr, float value) \
    { \
      wmGizmo *gz = static_cast<wmGizmo *>(ptr->data); \
      gz->member_id = value; \
    }
#  define RNA_GIZMO_GENERIC_FLOAT_ARRAY_INDEX_RW_DEF(func_id, member_id, index) \
    static float rna_Gizmo_##func_id##_get(PointerRNA *ptr) \
    { \
      wmGizmo *gz = static_cast<wmGizmo *>(ptr->data); \
      return gz->member_id[index]; \
    } \
    static void rna_Gizmo_##func_id##_set(PointerRNA *ptr, float value) \
    { \
      wmGizmo *gz = static_cast<wmGizmo *>(ptr->data); \
      gz->member_id[index] = value; \
    }
/* wmGizmo.float[len] */
#  define RNA_GIZMO_GENERIC_FLOAT_ARRAY_RW_DEF(func_id, member_id, len) \
    static void rna_Gizmo_##func_id##_get(PointerRNA *ptr, float value[len]) \
    { \
      wmGizmo *gz = static_cast<wmGizmo *>(ptr->data); \
      memcpy(value, gz->member_id, sizeof(float[len])); \
    } \
    static void rna_Gizmo_##func_id##_set(PointerRNA *ptr, const float value[len]) \
    { \
      wmGizmo *gz = static_cast<wmGizmo *>(ptr->data); \
      memcpy(gz->member_id, value, sizeof(float[len])); \
    }

/* wmGizmo.flag */
#  define RNA_GIZMO_GENERIC_FLAG_RW_DEF(func_id, member_id, flag_value) \
    static bool rna_Gizmo_##func_id##_get(PointerRNA *ptr) \
    { \
      wmGizmo *gz = static_cast<wmGizmo *>(ptr->data); \
      return (gz->member_id & flag_value) != 0; \
    } \
    static void rna_Gizmo_##func_id##_set(PointerRNA *ptr, bool value) \
    { \
      wmGizmo *gz = static_cast<wmGizmo *>(ptr->data); \
      SET_FLAG_FROM_TEST(gz->member_id, value, flag_value); \
    }

/* wmGizmo.flag (negative) */
#  define RNA_GIZMO_GENERIC_FLAG_NEG_RW_DEF(func_id, member_id, flag_value) \
    static bool rna_Gizmo_##func_id##_get(PointerRNA *ptr) \
    { \
      wmGizmo *gz = static_cast<wmGizmo *>(ptr->data); \
      return (gz->member_id & flag_value) == 0; \
    } \
    static void rna_Gizmo_##func_id##_set(PointerRNA *ptr, bool value) \
    { \
      wmGizmo *gz = static_cast<wmGizmo *>(ptr->data); \
      SET_FLAG_FROM_TEST(gz->member_id, !value, flag_value); \
    }

#  define RNA_GIZMO_FLAG_RO_DEF(func_id, member_id, flag_value) \
    static bool rna_Gizmo_##func_id##_get(PointerRNA *ptr) \
    { \
      wmGizmo *gz = static_cast<wmGizmo *>(ptr->data); \
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
  wmGizmo *gz = static_cast<wmGizmo *>(ptr->data);
  WM_gizmo_calc_matrix_final(gz, (float (*)[4])value);
}

RNA_GIZMO_GENERIC_FLOAT_RW_DEF(scale_basis, scale_basis);
RNA_GIZMO_GENERIC_FLOAT_RW_DEF(line_width, line_width);
RNA_GIZMO_GENERIC_FLOAT_RW_DEF(select_bias, select_bias);

RNA_GIZMO_GENERIC_FLAG_RW_DEF(flag_use_draw_hover, flag, WM_GIZMO_DRAW_HOVER);
RNA_GIZMO_GENERIC_FLAG_RW_DEF(flag_use_draw_modal, flag, WM_GIZMO_DRAW_MODAL);
RNA_GIZMO_GENERIC_FLAG_RW_DEF(flag_use_draw_value, flag, WM_GIZMO_DRAW_VALUE);
RNA_GIZMO_GENERIC_FLAG_RW_DEF(flag_use_draw_offset_scale, flag, WM_GIZMO_DRAW_OFFSET_SCALE);
RNA_GIZMO_GENERIC_FLAG_NEG_RW_DEF(flag_use_draw_scale, flag, WM_GIZMO_DRAW_NO_SCALE);
RNA_GIZMO_GENERIC_FLAG_RW_DEF(flag_hide, flag, WM_GIZMO_HIDDEN);
RNA_GIZMO_GENERIC_FLAG_RW_DEF(flag_hide_select, flag, WM_GIZMO_HIDDEN_SELECT);
RNA_GIZMO_GENERIC_FLAG_RW_DEF(flag_hide_keymap, flag, WM_GIZMO_HIDDEN_KEYMAP);
RNA_GIZMO_GENERIC_FLAG_RW_DEF(flag_use_grab_cursor, flag, WM_GIZMO_MOVE_CURSOR);
RNA_GIZMO_GENERIC_FLAG_RW_DEF(flag_use_select_background, flag, WM_GIZMO_SELECT_BACKGROUND);
RNA_GIZMO_GENERIC_FLAG_RW_DEF(flag_use_operator_tool_properties,
                              flag,
                              WM_GIZMO_OPERATOR_TOOL_INIT);
RNA_GIZMO_GENERIC_FLAG_RW_DEF(flag_use_event_handle_all, flag, WM_GIZMO_EVENT_HANDLE_ALL);
RNA_GIZMO_GENERIC_FLAG_NEG_RW_DEF(flag_use_tooltip, flag, WM_GIZMO_NO_TOOLTIP);

/* wmGizmo.state */
RNA_GIZMO_FLAG_RO_DEF(state_is_highlight, state, WM_GIZMO_STATE_HIGHLIGHT);
RNA_GIZMO_FLAG_RO_DEF(state_is_modal, state, WM_GIZMO_STATE_MODAL);
RNA_GIZMO_FLAG_RO_DEF(state_select, state, WM_GIZMO_STATE_SELECT);

static void rna_Gizmo_state_select_set(PointerRNA *ptr, bool value)
{
  wmGizmo *gz = static_cast<wmGizmo *>(ptr->data);
  wmGizmoGroup *gzgroup = gz->parent_gzgroup;
  WM_gizmo_select_set(gzgroup->parent_gzmap, gz, value);
}

static PointerRNA rna_Gizmo_group_get(PointerRNA *ptr)
{
  wmGizmo *gz = static_cast<wmGizmo *>(ptr->data);
  return RNA_pointer_create_with_parent(*ptr, &RNA_GizmoGroup, gz->parent_gzgroup);
}

#  ifdef WITH_PYTHON

static bool rna_Gizmo_unregister(Main *bmain, StructRNA *type);
extern void BPY_RNA_gizmo_wrapper(wmGizmoType *gzgt, void *userdata);

static StructRNA *rna_Gizmo_register(Main *bmain,
                                     ReportList *reports,
                                     void *data,
                                     const char *identifier,
                                     StructValidateFunc validate,
                                     StructCallbackFunc call,
                                     StructFreeFunc free)
{
  const char *error_prefix = "Registering gizmo class:";
  struct {
    char idname[MAX_NAME];
  } temp_buffers;

  wmGizmoType dummy_gt = {nullptr};
  wmGizmo dummy_gizmo = {nullptr};

  /* Two sets of functions. */
  bool have_function[8];

  /* setup dummy gizmo & gizmo type to store static properties in */
  dummy_gizmo.type = &dummy_gt;
  dummy_gt.idname = temp_buffers.idname;
  PointerRNA dummy_gizmo_ptr = RNA_pointer_create_discrete(nullptr, &RNA_Gizmo, &dummy_gizmo);

  /* Clear so we can detect if it's left unset. */
  temp_buffers.idname[0] = '\0';

  /* validate the python class */
  if (validate(&dummy_gizmo_ptr, data, have_function) != 0) {
    return nullptr;
  }

  if (strlen(identifier) >= sizeof(temp_buffers.idname)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "%s '%s' is too long, maximum length is %d",
                error_prefix,
                identifier,
                int(sizeof(temp_buffers.idname)));
    return nullptr;
  }

  /* check if we have registered this gizmo type before, and remove it */
  {
    const wmGizmoType *gzt = WM_gizmotype_find(dummy_gt.idname, true);
    if (gzt) {
      BKE_reportf(reports,
                  RPT_INFO,
                  "%s '%s', bl_idname '%s' has been registered before, unregistering previous",
                  error_prefix,
                  identifier,
                  dummy_gt.idname);

      StructRNA *srna = gzt->rna_ext.srna;
      if (!(srna && rna_Gizmo_unregister(bmain, srna))) {
        BKE_reportf(reports,
                    RPT_ERROR,
                    "%s '%s', bl_idname '%s' %s",
                    error_prefix,
                    identifier,
                    dummy_gt.idname,
                    srna ? "is built-in" : "could not be unregistered");
        return nullptr;
      }
    }
  }
  if (!RNA_struct_available_or_report(reports, dummy_gt.idname)) {
    return nullptr;
  }

  { /* allocate the idname */
    /* For multiple strings see GizmoGroup. */
    dummy_gt.idname = BLI_strdup(temp_buffers.idname);
  }

  /* create a new gizmo type */
  dummy_gt.rna_ext.srna = RNA_def_struct_ptr(&BLENDER_RNA, dummy_gt.idname, &RNA_Gizmo);
  /* gizmo properties are registered separately */
  RNA_def_struct_flag(dummy_gt.rna_ext.srna, STRUCT_NO_IDPROPERTIES);
  dummy_gt.rna_ext.data = data;
  dummy_gt.rna_ext.call = call;
  dummy_gt.rna_ext.free = free;

  {
    int i = 0;
    dummy_gt.draw = (have_function[i++]) ? rna_gizmo_draw_cb : nullptr;
    dummy_gt.draw_select = (have_function[i++]) ? rna_gizmo_draw_select_cb : nullptr;
    dummy_gt.test_select = (have_function[i++]) ? rna_gizmo_test_select_cb : nullptr;
    dummy_gt.modal = (have_function[i++]) ? rna_gizmo_modal_cb : nullptr;
    // dummy_gt.property_update = (have_function[i++]) ? rna_gizmo_property_update : nullptr;
    // dummy_gt.position_get = (have_function[i++]) ? rna_gizmo_position_get : nullptr;
    dummy_gt.setup = (have_function[i++]) ? rna_gizmo_setup_cb : nullptr;
    dummy_gt.invoke = (have_function[i++]) ? rna_gizmo_invoke_cb : nullptr;
    dummy_gt.exit = (have_function[i++]) ? rna_gizmo_exit_cb : nullptr;
    dummy_gt.select_refresh = (have_function[i++]) ? rna_gizmo_select_refresh_cb : nullptr;

    BLI_assert(i == ARRAY_SIZE(have_function));
  }

  WM_gizmotype_append_ptr(BPY_RNA_gizmo_wrapper, (void *)&dummy_gt);

  /* update while blender is running */
  WM_main_add_notifier(NC_SCREEN | NA_EDITED, nullptr);

  return dummy_gt.rna_ext.srna;
}

static bool rna_Gizmo_unregister(Main *bmain, StructRNA *type)
{
  wmGizmoType *gzt = static_cast<wmGizmoType *>(RNA_struct_blender_type_get(type));

  if (!gzt) {
    return false;
  }

  WM_gizmotype_remove_ptr(nullptr, bmain, gzt);

  /* Free extension after removing instances so `__del__` doesn't crash, see: #85567. */
  RNA_struct_free_extension(type, &gzt->rna_ext);
  RNA_struct_free(&BLENDER_RNA, type);

  /* Free gizmo group after the extension as it owns the identifier memory. */
  WM_gizmotype_free_ptr(gzt);

  WM_main_add_notifier(NC_SCREEN | NA_EDITED, nullptr);
  return true;
}

static void **rna_Gizmo_instance(PointerRNA *ptr)
{
  wmGizmo *gz = static_cast<wmGizmo *>(ptr->data);
  return &gz->py_instance;
}

#  endif /* WITH_PYTHON */

static StructRNA *rna_Gizmo_refine(PointerRNA *gz_ptr)
{
  wmGizmo *gz = static_cast<wmGizmo *>(gz_ptr->data);
  return (gz->type && gz->type->rna_ext.srna) ? gz->type->rna_ext.srna : &RNA_Gizmo;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Gizmo Group API
 * \{ */

static wmGizmoGroupType *rna_GizmoGroupProperties_find_gizmo_group_type(PointerRNA *ptr)
{
  IDProperty *properties = (IDProperty *)ptr->data;
  wmGizmoGroupType *gzgt = WM_gizmogrouptype_find(properties->name, false);
  return gzgt;
}

static StructRNA *rna_GizmoGroupProperties_refine(PointerRNA *ptr)
{
  wmGizmoGroupType *gzgt = rna_GizmoGroupProperties_find_gizmo_group_type(ptr);

  if (gzgt) {
    return gzgt->srna;
  }
  return ptr->type;
}

static IDProperty **rna_GizmoGroupProperties_idprops(PointerRNA *ptr)
{
  return (IDProperty **)&ptr->data;
}

static wmGizmo *rna_GizmoGroup_gizmo_new(wmGizmoGroup *gzgroup,
                                         ReportList *reports,
                                         const char *idname)
{
  const wmGizmoType *gzt = WM_gizmotype_find(idname, true);
  if (gzt == nullptr) {
    BKE_reportf(reports, RPT_ERROR, "GizmoType '%s' not known", idname);
    return nullptr;
  }
  if ((gzgroup->type->flag & WM_GIZMOGROUPTYPE_3D) == 0) {
    /* Allow for neither callbacks to be set, while this doesn't seem like a valid use case,
     * there may be rare situations where a developer wants a gizmo to be draw-only. */
    if ((gzt->test_select == nullptr) && (gzt->draw_select != nullptr)) {
      BKE_reportf(reports,
                  RPT_ERROR,
                  "GizmoType '%s' is for a 3D gizmo-group. "
                  "The 'draw_select' callback is set where only 'test_select' will be used.",
                  idname);
      return nullptr;
    }
  }
  wmGizmo *gz = WM_gizmo_new_ptr(gzt, gzgroup, nullptr);
  return gz;
}

static void rna_GizmoGroup_gizmo_remove(wmGizmoGroup *gzgroup, bContext *C, wmGizmo *gz)
{
  WM_gizmo_unlink(&gzgroup->gizmos, gzgroup->parent_gzmap, gz, C);
}

static void rna_GizmoGroup_gizmo_clear(wmGizmoGroup *gzgroup, bContext *C)
{
  while (gzgroup->gizmos.first) {
    WM_gizmo_unlink(
        &gzgroup->gizmos, gzgroup->parent_gzmap, static_cast<wmGizmo *>(gzgroup->gizmos.first), C);
  }
}

static void rna_GizmoGroup_name_get(PointerRNA *ptr, char *value)
{
  wmGizmoGroup *gzgroup = static_cast<wmGizmoGroup *>(ptr->data);
  strcpy(value, gzgroup->type->name);
}

static int rna_GizmoGroup_name_length(PointerRNA *ptr)
{
  wmGizmoGroup *gzgroup = static_cast<wmGizmoGroup *>(ptr->data);
  return strlen(gzgroup->type->name);
}

/* just to work around 'const char *' warning and to ensure this is a python op */
static void rna_GizmoGroup_bl_idname_set(PointerRNA *ptr, const char *value)
{
  wmGizmoGroup *data = static_cast<wmGizmoGroup *>(ptr->data);
  char *str = (char *)data->type->idname;
  if (!str[0]) {
    /* Calling UTF8 copy is disputable since registering ensures the value isn't truncated.
     * Use a UTF8 copy to ensure truncating never causes an incomplete UTF8 sequence,
     * even before registration. */
    BLI_strncpy_utf8(str, value, MAX_NAME);
  }
  else {
    BLI_assert_msg(0, "setting the bl_idname on a non-builtin operator");
  }
}

static void rna_GizmoGroup_bl_label_set(PointerRNA *ptr, const char *value)
{
  wmGizmoGroup *data = static_cast<wmGizmoGroup *>(ptr->data);
  char *str = (char *)data->type->name;
  if (!str[0]) {
    BLI_strncpy_utf8(str, value, MAX_NAME);
  }
  else {
    BLI_assert_msg(0, "setting the bl_label on a non-builtin operator");
  }
}

#  ifdef WITH_PYTHON

static bool rna_gizmogroup_poll_cb(const bContext *C, wmGizmoGroupType *gzgt)
{

  extern FunctionRNA rna_GizmoGroup_poll_func;

  ParameterList list;
  FunctionRNA *func;
  void *ret;
  bool visible;

  PointerRNA ptr = RNA_pointer_create_discrete(nullptr, gzgt->rna_ext.srna, nullptr); /* dummy */
  func = &rna_GizmoGroup_poll_func; /* RNA_struct_find_function(&ptr, "poll"); */

  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  gzgt->rna_ext.call((bContext *)C, &ptr, func, &list);

  RNA_parameter_get_lookup(&list, "visible", &ret);
  visible = *(bool *)ret;

  RNA_parameter_list_free(&list);

  return visible;
}

static void rna_gizmogroup_setup_cb(const bContext *C, wmGizmoGroup *gzgroup)
{
  extern FunctionRNA rna_GizmoGroup_setup_func;

  ParameterList list;
  FunctionRNA *func;

  PointerRNA gzgroup_ptr = RNA_pointer_create_discrete(
      nullptr, gzgroup->type->rna_ext.srna, gzgroup);
  func = &rna_GizmoGroup_setup_func; /* RNA_struct_find_function(&wgroupr, "setup"); */

  RNA_parameter_list_create(&list, &gzgroup_ptr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  gzgroup->type->rna_ext.call((bContext *)C, &gzgroup_ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static wmKeyMap *rna_gizmogroup_setup_keymap_cb(const wmGizmoGroupType *gzgt, wmKeyConfig *config)
{
  extern FunctionRNA rna_GizmoGroup_setup_keymap_func;
  void *ret;

  ParameterList list;
  FunctionRNA *func;

  PointerRNA ptr = RNA_pointer_create_discrete(nullptr, gzgt->rna_ext.srna, nullptr); /* dummy */
  func =
      &rna_GizmoGroup_setup_keymap_func; /* RNA_struct_find_function(&wgroupr, "setup_keymap"); */

  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "keyconfig", &config);
  gzgt->rna_ext.call(nullptr, &ptr, func, &list);

  RNA_parameter_get_lookup(&list, "keymap", &ret);
  wmKeyMap *keymap = *(wmKeyMap **)ret;

  RNA_parameter_list_free(&list);

  return keymap;
}

static void rna_gizmogroup_refresh_cb(const bContext *C, wmGizmoGroup *gzgroup)
{
  extern FunctionRNA rna_GizmoGroup_refresh_func;

  ParameterList list;
  FunctionRNA *func;

  PointerRNA gzgroup_ptr = RNA_pointer_create_discrete(
      nullptr, gzgroup->type->rna_ext.srna, gzgroup);
  func = &rna_GizmoGroup_refresh_func; /* RNA_struct_find_function(&wgroupr, "refresh"); */

  RNA_parameter_list_create(&list, &gzgroup_ptr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  gzgroup->type->rna_ext.call((bContext *)C, &gzgroup_ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static void rna_gizmogroup_draw_prepare_cb(const bContext *C, wmGizmoGroup *gzgroup)
{
  extern FunctionRNA rna_GizmoGroup_draw_prepare_func;

  ParameterList list;
  FunctionRNA *func;

  PointerRNA gzgroup_ptr = RNA_pointer_create_discrete(
      nullptr, gzgroup->type->rna_ext.srna, gzgroup);
  func =
      &rna_GizmoGroup_draw_prepare_func; /* RNA_struct_find_function(&wgroupr, "draw_prepare"); */

  RNA_parameter_list_create(&list, &gzgroup_ptr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  gzgroup->type->rna_ext.call((bContext *)C, &gzgroup_ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static void rna_gizmogroup_invoke_prepare_cb(const bContext *C,
                                             wmGizmoGroup *gzgroup,
                                             wmGizmo *gz,
                                             const wmEvent *event)
{
  extern FunctionRNA rna_GizmoGroup_invoke_prepare_func;

  ParameterList list;
  FunctionRNA *func;

  PointerRNA gzgroup_ptr = RNA_pointer_create_discrete(
      nullptr, gzgroup->type->rna_ext.srna, gzgroup);
  /* Reference `RNA_struct_find_function(&wgroupr, "invoke_prepare")` directly. */
  func = &rna_GizmoGroup_invoke_prepare_func;

  RNA_parameter_list_create(&list, &gzgroup_ptr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  RNA_parameter_set_lookup(&list, "gizmo", &gz);
  RNA_parameter_set_lookup(&list, "event", &event);
  gzgroup->type->rna_ext.call((bContext *)C, &gzgroup_ptr, func, &list);

  RNA_parameter_list_free(&list);
}

extern void BPY_RNA_gizmogroup_wrapper(wmGizmoGroupType *gzgt, void *userdata);
static bool rna_GizmoGroup_unregister(Main *bmain, StructRNA *type);

static StructRNA *rna_GizmoGroup_register(Main *bmain,
                                          ReportList *reports,
                                          void *data,
                                          const char *identifier,
                                          StructValidateFunc validate,
                                          StructCallbackFunc call,
                                          StructFreeFunc free)
{
  const char *error_prefix = "Registering gizmogroup class:";
  struct {
    char name[MAX_NAME];
    char idname[MAX_NAME];
  } temp_buffers;

  wmGizmoGroupType dummy_wgt = {nullptr};
  wmGizmoGroup dummy_gizmo_group = {nullptr};

  /* Two sets of functions. */
  bool have_function[6];

  /* setup dummy gizmogroup & gizmogroup type to store static properties in */
  dummy_gizmo_group.type = &dummy_wgt;
  dummy_wgt.name = temp_buffers.name;
  dummy_wgt.idname = temp_buffers.idname;

  PointerRNA wgptr = RNA_pointer_create_discrete(nullptr, &RNA_GizmoGroup, &dummy_gizmo_group);

  /* Clear so we can detect if it's left unset. */
  temp_buffers.idname[0] = temp_buffers.name[0] = '\0';

  /* validate the python class */
  if (validate(&wgptr, data, have_function) != 0) {
    return nullptr;
  }

  if (strlen(identifier) >= sizeof(temp_buffers.idname)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "%s '%s' is too long, maximum length is %d",
                error_prefix,
                identifier,
                int(sizeof(temp_buffers.idname)));
    return nullptr;
  }

  /* check if the area supports widgets */
  wmGizmoMapType_Params wmap_params{};
  wmap_params.spaceid = dummy_wgt.gzmap_params.spaceid;
  wmap_params.regionid = dummy_wgt.gzmap_params.regionid;

  wmGizmoMapType *gzmap_type = WM_gizmomaptype_ensure(&wmap_params);
  if (gzmap_type == nullptr) {
    BKE_reportf(reports, RPT_ERROR, "%s area type does not support gizmos", error_prefix);
    return nullptr;
  }

  /* check if we have registered this gizmogroup type before, and remove it */
  {
    wmGizmoGroupType *gzgt = WM_gizmogrouptype_find(dummy_wgt.idname, true);
    if (gzgt) {
      StructRNA *srna = gzgt->rna_ext.srna;
      if (!(srna && rna_GizmoGroup_unregister(bmain, srna))) {
        BKE_reportf(reports,
                    RPT_ERROR,
                    "%s '%s', bl_idname '%s' %s",
                    error_prefix,
                    identifier,
                    dummy_wgt.idname,
                    srna ? "is built-in" : "could not be unregistered");
        return nullptr;
      }
    }
  }
  if (!RNA_struct_available_or_report(reports, dummy_wgt.idname)) {
    return nullptr;
  }

  { /* allocate the idname */
    const char *strings[] = {
        temp_buffers.idname,
        temp_buffers.name,
    };
    char *strings_table[ARRAY_SIZE(strings)];
    BLI_string_join_array_by_sep_char_with_tableN(
        '\0', strings_table, strings, ARRAY_SIZE(strings));

    dummy_wgt.idname = strings_table[0]; /* allocated string stored here */
    dummy_wgt.name = strings_table[1];
    BLI_STATIC_ASSERT(ARRAY_SIZE(strings) == 2, "Unexpected number of strings")
  }

  /* create a new gizmogroup type */
  dummy_wgt.rna_ext.srna = RNA_def_struct_ptr(&BLENDER_RNA, dummy_wgt.idname, &RNA_GizmoGroup);

  /* Gizmo group properties are registered separately. */
  RNA_def_struct_flag(dummy_wgt.rna_ext.srna, STRUCT_NO_IDPROPERTIES);

  dummy_wgt.rna_ext.data = data;
  dummy_wgt.rna_ext.call = call;
  dummy_wgt.rna_ext.free = free;

  /* We used to register widget group types like this, now we do it similar to
   * operator types. Thus we should be able to do the same as operator types now. */
  dummy_wgt.poll = (have_function[0]) ? rna_gizmogroup_poll_cb : nullptr;
  dummy_wgt.setup_keymap = (have_function[1]) ? rna_gizmogroup_setup_keymap_cb : nullptr;
  dummy_wgt.setup = (have_function[2]) ? rna_gizmogroup_setup_cb : nullptr;
  dummy_wgt.refresh = (have_function[3]) ? rna_gizmogroup_refresh_cb : nullptr;
  dummy_wgt.draw_prepare = (have_function[4]) ? rna_gizmogroup_draw_prepare_cb : nullptr;
  dummy_wgt.invoke_prepare = (have_function[5]) ? rna_gizmogroup_invoke_prepare_cb : nullptr;

  wmGizmoGroupType *gzgt = WM_gizmogrouptype_append_ptr(BPY_RNA_gizmogroup_wrapper,
                                                        (void *)&dummy_wgt);

  {
    const char *owner_id = RNA_struct_state_owner_get();
    if (owner_id) {
      STRNCPY(gzgt->owner_id, owner_id);
    }
  }

  if (gzgt->flag & WM_GIZMOGROUPTYPE_PERSISTENT) {
    WM_gizmo_group_type_add_ptr_ex(gzgt, gzmap_type);

    /* update while blender is running */
    WM_main_add_notifier(NC_SCREEN | NA_EDITED, nullptr);
  }

  return dummy_wgt.rna_ext.srna;
}

static bool rna_GizmoGroup_unregister(Main *bmain, StructRNA *type)
{
  wmGizmoGroupType *gzgt = static_cast<wmGizmoGroupType *>(RNA_struct_blender_type_get(type));

  if (!gzgt) {
    return false;
  }

  WM_gizmo_group_type_remove_ptr(bmain, gzgt);

  /* Free extension after removing instances so `__del__` doesn't crash, see: #85567. */
  RNA_struct_free_extension(type, &gzgt->rna_ext);
  RNA_struct_free(&BLENDER_RNA, type);

  /* Free gizmo group after the extension as it owns the identifier memory. */
  WM_gizmo_group_type_free_ptr(gzgt);

  WM_main_add_notifier(NC_SCREEN | NA_EDITED, nullptr);
  return true;
}

static void **rna_GizmoGroup_instance(PointerRNA *ptr)
{
  wmGizmoGroup *gzgroup = static_cast<wmGizmoGroup *>(ptr->data);
  return &gzgroup->py_instance;
}

#  endif /* WITH_PYTHON */

static StructRNA *rna_GizmoGroup_refine(PointerRNA *gzgroup_ptr)
{
  wmGizmoGroup *gzgroup = static_cast<wmGizmoGroup *>(gzgroup_ptr->data);
  return (gzgroup->type && gzgroup->type->rna_ext.srna) ? gzgroup->type->rna_ext.srna :
                                                          &RNA_GizmoGroup;
}

static void rna_GizmoGroup_gizmos_begin(CollectionPropertyIterator *iter, PointerRNA *gzgroup_ptr)
{
  wmGizmoGroup *gzgroup = static_cast<wmGizmoGroup *>(gzgroup_ptr->data);
  rna_iterator_listbase_begin(iter, gzgroup_ptr, &gzgroup->gizmos, nullptr);
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
  srna = RNA_def_struct(brna, "Gizmos", nullptr);
  RNA_def_struct_sdna(srna, "wmGizmoGroup");
  RNA_def_struct_ui_text(srna, "Gizmos", "Collection of gizmos");

  func = RNA_def_function(srna, "new", "rna_GizmoGroup_gizmo_new");
  RNA_def_function_ui_description(func, "Add gizmo");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_string(func, "type", "Type", 0, "", "Gizmo identifier"); /* optional */
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "gizmo", "Gizmo", "", "New gizmo");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_GizmoGroup_gizmo_remove");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  RNA_def_function_ui_description(func, "Delete gizmo");
  parm = RNA_def_pointer(func, "gizmo", "Gizmo", "", "New gizmo");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));

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
  srna = RNA_def_struct(brna, "Gizmo", nullptr);
  RNA_def_struct_sdna(srna, "wmGizmo");
  RNA_def_struct_ui_text(srna, "Gizmo", "Collection of gizmos");
  RNA_def_struct_refine_func(srna, "rna_Gizmo_refine");

#  ifdef WITH_PYTHON
  RNA_def_struct_register_funcs(
      srna, "rna_Gizmo_register", "rna_Gizmo_unregister", "rna_Gizmo_instance");
#  endif
  RNA_def_struct_translation_context(srna, BLT_I18NCONTEXT_OPERATOR_DEFAULT);

  prop = RNA_def_property(srna, "properties", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "GizmoProperties");
  RNA_def_property_ui_text(prop, "Properties", "");
  RNA_def_property_pointer_funcs(prop, "rna_Gizmo_properties_get", nullptr, nullptr, nullptr);

  /* -------------------------------------------------------------------- */
  /* Registerable Variables */

  RNA_define_verify_sdna(false); /* not in sdna */

  prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "type->idname");
  RNA_def_property_string_maxlength(prop, MAX_NAME);
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_Gizmo_bl_idname_set");
  // RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_flag(prop, PROP_REGISTER);

  RNA_define_verify_sdna(true); /* not in sdna */

  /* wmGizmo.draw */
  func = RNA_def_function(srna, "draw", nullptr);
  RNA_def_function_ui_description(func, "");
  RNA_def_function_flag(func, FUNC_REGISTER);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  /* wmGizmo.draw_select */
  func = RNA_def_function(srna, "draw_select", nullptr);
  RNA_def_function_ui_description(func, "");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_int(func, "select_id", 0, 0, INT_MAX, "", "", 0, INT_MAX);

  /* wmGizmo.test_select */
  func = RNA_def_function(srna, "test_select", nullptr);
  RNA_def_function_ui_description(func, "");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_int_array(func,
                           "location",
                           2,
                           nullptr,
                           INT_MIN,
                           INT_MAX,
                           "Location",
                           "Region coordinates",
                           INT_MIN,
                           INT_MAX);
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_int(
      func, "intersect_id", -1, -1, INT_MAX, "", "Use -1 to skip this gizmo", -1, INT_MAX);
  RNA_def_function_return(func, parm);

  /* wmGizmo.handler */
  static const EnumPropertyItem tweak_actions[] = {
      {WM_GIZMO_TWEAK_PRECISE, "PRECISE", 0, "Precise", ""},
      {WM_GIZMO_TWEAK_SNAP, "SNAP", 0, "Snap", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };
  func = RNA_def_function(srna, "modal", nullptr);
  RNA_def_function_ui_description(func, "");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "event", "Event", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  /* TODO: should be a enum-flag. */
  parm = RNA_def_enum_flag(func, "tweak", tweak_actions, 0, "Tweak", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_enum_flag(
      func, "result", rna_enum_operator_return_items, OPERATOR_FINISHED, "result", "");
  RNA_def_function_return(func, parm);
  /* wmGizmo.property_update */
  /* TODO */

  /* wmGizmo.setup */
  func = RNA_def_function(srna, "setup", nullptr);
  RNA_def_function_ui_description(func, "");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);

  /* wmGizmo.invoke */
  func = RNA_def_function(srna, "invoke", nullptr);
  RNA_def_function_ui_description(func, "");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "event", "Event", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_enum_flag(
      func, "result", rna_enum_operator_return_items, OPERATOR_FINISHED, "result", "");
  RNA_def_function_return(func, parm);

  /* wmGizmo.exit */
  func = RNA_def_function(srna, "exit", nullptr);
  RNA_def_function_ui_description(func, "");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_boolean(func, "cancel", false, "Cancel, otherwise confirm", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  /* wmGizmo.cursor_get */
  /* TODO */

  /* wmGizmo.select_refresh */
  func = RNA_def_function(srna, "select_refresh", nullptr);
  RNA_def_function_ui_description(func, "");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);

  /* -------------------------------------------------------------------- */
  /* Instance Variables */

  prop = RNA_def_property(srna, "group", PROP_POINTER, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "GizmoGroup");
  RNA_def_property_pointer_funcs(prop, "rna_Gizmo_group_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(prop, "", "Gizmo group this gizmo is a member of");

  /* Color & Alpha */
  prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_funcs(prop, "rna_Gizmo_color_get", "rna_Gizmo_color_set", nullptr);

  prop = RNA_def_property(srna, "alpha", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Alpha", "");
  RNA_def_property_float_funcs(prop, "rna_Gizmo_alpha_get", "rna_Gizmo_alpha_set", nullptr);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_update(prop, 0, "rna_Gizmo_update_redraw");

  /* Color & Alpha (highlight) */
  prop = RNA_def_property(srna, "color_highlight", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_funcs(prop, "rna_Gizmo_color_hi_get", "rna_Gizmo_color_hi_set", nullptr);

  prop = RNA_def_property(srna, "alpha_highlight", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Alpha", "");
  RNA_def_property_float_funcs(prop, "rna_Gizmo_alpha_hi_get", "rna_Gizmo_alpha_hi_set", nullptr);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_update(prop, 0, "rna_Gizmo_update_redraw");

  prop = RNA_def_property(srna, "matrix_space", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_ui_text(prop, "Space Matrix", "");
  RNA_def_property_float_funcs(
      prop, "rna_Gizmo_matrix_space_get", "rna_Gizmo_matrix_space_set", nullptr);
  RNA_def_property_update(prop, 0, "rna_Gizmo_update_redraw");

  prop = RNA_def_property(srna, "matrix_basis", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_ui_text(prop, "Basis Matrix", "");
  RNA_def_property_float_funcs(
      prop, "rna_Gizmo_matrix_basis_get", "rna_Gizmo_matrix_basis_set", nullptr);
  RNA_def_property_update(prop, 0, "rna_Gizmo_update_redraw");

  prop = RNA_def_property(srna, "matrix_offset", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_ui_text(prop, "Offset Matrix", "");
  RNA_def_property_float_funcs(
      prop, "rna_Gizmo_matrix_offset_get", "rna_Gizmo_matrix_offset_set", nullptr);
  RNA_def_property_update(prop, 0, "rna_Gizmo_update_redraw");

  prop = RNA_def_property(srna, "matrix_world", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_ui_text(prop, "Final World Matrix", "");
  RNA_def_property_float_funcs(prop, "rna_Gizmo_matrix_world_get", nullptr, nullptr);

  prop = RNA_def_property(srna, "scale_basis", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Scale Basis", "");
  RNA_def_property_float_funcs(
      prop, "rna_Gizmo_scale_basis_get", "rna_Gizmo_scale_basis_set", nullptr);
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_update(prop, 0, "rna_Gizmo_update_redraw");

  prop = RNA_def_property(srna, "line_width", PROP_FLOAT, PROP_PIXEL);
  RNA_def_property_ui_text(prop, "Line Width", "");
  RNA_def_property_float_funcs(
      prop, "rna_Gizmo_line_width_get", "rna_Gizmo_line_width_set", nullptr);
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_update(prop, 0, "rna_Gizmo_update_redraw");

  prop = RNA_def_property(srna, "select_bias", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Select Bias", "Depth bias used for selection");
  RNA_def_property_float_funcs(
      prop, "rna_Gizmo_select_bias_get", "rna_Gizmo_select_bias_set", nullptr);
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);

  /* wmGizmo.flag */
  /* WM_GIZMO_HIDDEN */
  prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_Gizmo_flag_hide_get", "rna_Gizmo_flag_hide_set");
  RNA_def_property_ui_text(prop, "Hide", "");
  RNA_def_property_update(prop, 0, "rna_Gizmo_update_redraw");
  /* WM_GIZMO_HIDDEN_SELECT */
  prop = RNA_def_property(srna, "hide_select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(
      prop, "rna_Gizmo_flag_hide_select_get", "rna_Gizmo_flag_hide_select_set");
  RNA_def_property_ui_text(prop, "Hide Select", "");
  RNA_def_property_update(prop, 0, "rna_Gizmo_update_redraw");
  /* WM_GIZMO_HIDDEN_KEYMAP */
  prop = RNA_def_property(srna, "hide_keymap", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(
      prop, "rna_Gizmo_flag_hide_keymap_get", "rna_Gizmo_flag_hide_keymap_set");
  RNA_def_property_ui_text(prop, "Hide Keymap", "Ignore the key-map for this gizmo");
  RNA_def_property_update(prop, 0, "rna_Gizmo_update_redraw");
  /* WM_GIZMO_MOVE_CURSOR */
  prop = RNA_def_property(srna, "use_grab_cursor", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(
      prop, "rna_Gizmo_flag_use_grab_cursor_get", "rna_Gizmo_flag_use_grab_cursor_set");
  RNA_def_property_ui_text(prop, "Grab Cursor", "");
  RNA_def_property_update(prop, 0, "rna_Gizmo_update_redraw");

  /* WM_GIZMO_DRAW_HOVER */
  prop = RNA_def_property(srna, "use_draw_hover", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(
      prop, "rna_Gizmo_flag_use_draw_hover_get", "rna_Gizmo_flag_use_draw_hover_set");
  RNA_def_property_ui_text(prop, "Show Hover", "");
  RNA_def_property_update(prop, 0, "rna_Gizmo_update_redraw");
  /* WM_GIZMO_DRAW_MODAL */
  prop = RNA_def_property(srna, "use_draw_modal", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(
      prop, "rna_Gizmo_flag_use_draw_modal_get", "rna_Gizmo_flag_use_draw_modal_set");
  RNA_def_property_ui_text(prop, "Show Active", "Show while dragging");
  RNA_def_property_update(prop, 0, "rna_Gizmo_update_redraw");
  /* WM_GIZMO_DRAW_VALUE */
  prop = RNA_def_property(srna, "use_draw_value", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(
      prop, "rna_Gizmo_flag_use_draw_value_get", "rna_Gizmo_flag_use_draw_value_set");
  RNA_def_property_ui_text(
      prop, "Show Value", "Show an indicator for the current value while dragging");
  RNA_def_property_update(prop, 0, "rna_Gizmo_update_redraw");
  /* WM_GIZMO_DRAW_OFFSET_SCALE */
  prop = RNA_def_property(srna, "use_draw_offset_scale", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop,
                                 "rna_Gizmo_flag_use_draw_offset_scale_get",
                                 "rna_Gizmo_flag_use_draw_offset_scale_set");
  RNA_def_property_ui_text(
      prop, "Scale Offset", "Scale the offset matrix (use to apply screen-space offset)");
  RNA_def_property_update(prop, 0, "rna_Gizmo_update_redraw");
  /* WM_GIZMO_DRAW_NO_SCALE (negated) */
  prop = RNA_def_property(srna, "use_draw_scale", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(
      prop, "rna_Gizmo_flag_use_draw_scale_get", "rna_Gizmo_flag_use_draw_scale_set");
  RNA_def_property_boolean_default(prop, true);
  RNA_def_property_ui_text(prop, "Scale", "Use scale when calculating the matrix");
  RNA_def_property_update(prop, 0, "rna_Gizmo_update_redraw");

  /* WM_GIZMO_SELECT_BACKGROUND */
  prop = RNA_def_property(srna, "use_select_background", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop,
                                 "rna_Gizmo_flag_use_select_background_get",
                                 "rna_Gizmo_flag_use_select_background_set");
  RNA_def_property_ui_text(prop, "Select Background", "Don't write into the depth buffer");
  RNA_def_property_update(prop, 0, "rna_Gizmo_update_redraw");

  /* WM_GIZMO_OPERATOR_TOOL_INIT */
  prop = RNA_def_property(srna, "use_operator_tool_properties", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop,
                                 "rna_Gizmo_flag_use_operator_tool_properties_get",
                                 "rna_Gizmo_flag_use_operator_tool_properties_set");
  RNA_def_property_ui_text(
      prop,
      "Tool Property Init",
      "Merge active tool properties on activation (does not overwrite existing)");
  RNA_def_property_update(prop, 0, "rna_Gizmo_update_redraw");

  /* WM_GIZMO_EVENT_HANDLE_ALL */
  prop = RNA_def_property(srna, "use_event_handle_all", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(
      prop, "rna_Gizmo_flag_use_event_handle_all_get", "rna_Gizmo_flag_use_event_handle_all_set");
  RNA_def_property_ui_text(prop,
                           "Handle All Events",
                           "When highlighted, "
                           "do not pass events through to be handled by other keymaps");
  RNA_def_property_update(prop, 0, "rna_Gizmo_update_redraw");

  /* WM_GIZMO_NO_TOOLTIP (negated) */
  prop = RNA_def_property(srna, "use_tooltip", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(
      prop, "rna_Gizmo_flag_use_tooltip_get", "rna_Gizmo_flag_use_tooltip_set");
  RNA_def_property_boolean_default(prop, true);
  RNA_def_property_ui_text(prop, "Use Tooltip", "Use tooltips when hovering over this gizmo");
  /* No update needed. */

  /* wmGizmo.state (readonly) */
  /* WM_GIZMO_STATE_HIGHLIGHT */
  prop = RNA_def_property(srna, "is_highlight", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_Gizmo_state_is_highlight_get", nullptr);
  RNA_def_property_ui_text(prop, "Highlight", "");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  /* WM_GIZMO_STATE_MODAL */
  prop = RNA_def_property(srna, "is_modal", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_Gizmo_state_is_modal_get", nullptr);
  RNA_def_property_ui_text(prop, "Highlight", "");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  /* WM_GIZMO_STATE_SELECT */
  /* (note that setting is involved, needs to handle array) */
  prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_Gizmo_state_select_get", "rna_Gizmo_state_select_set");
  RNA_def_property_ui_text(prop, "Select", "");

  RNA_api_gizmo(srna);

  srna = RNA_def_struct(brna, "GizmoProperties", nullptr);
  RNA_def_struct_ui_text(srna, "Gizmo Properties", "Input properties of a Gizmo");
  RNA_def_struct_refine_func(srna, "rna_GizmoProperties_refine");
  RNA_def_struct_system_idprops_func(srna, "rna_GizmoProperties_idprops");
  RNA_def_struct_flag(srna, STRUCT_NO_DATABLOCK_IDPROPERTIES);
}

static void rna_def_gizmogroup(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  srna = RNA_def_struct(brna, "GizmoGroup", nullptr);
  RNA_def_struct_ui_text(
      srna, "GizmoGroup", "Storage of an operator being executed, or registered after execution");
  RNA_def_struct_sdna(srna, "wmGizmoGroup");
  RNA_def_struct_refine_func(srna, "rna_GizmoGroup_refine");
#  ifdef WITH_PYTHON
  RNA_def_struct_register_funcs(
      srna, "rna_GizmoGroup_register", "rna_GizmoGroup_unregister", "rna_GizmoGroup_instance");
#  endif
  RNA_def_struct_translation_context(srna, BLT_I18NCONTEXT_OPERATOR_DEFAULT);

  /* -------------------------------------------------------------------- */
  /* Registration */

  RNA_define_verify_sdna(false); /* not in sdna */

  prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "type->idname");
  RNA_def_property_string_maxlength(prop, MAX_NAME);
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_GizmoGroup_bl_idname_set");
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_def_struct_name_property(srna, prop);

  prop = RNA_def_property(srna, "bl_label", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "type->name");
  RNA_def_property_string_maxlength(prop, MAX_NAME); /* else it uses the pointer size! */
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_GizmoGroup_bl_label_set");
  // RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_flag(prop, PROP_REGISTER);

  prop = RNA_def_property(srna, "bl_space_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "type->gzmap_params.spaceid");
  RNA_def_property_enum_items(prop, rna_enum_space_type_items);
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_def_property_ui_text(prop, "Space Type", "The space where the panel is going to be used in");

  prop = RNA_def_property(srna, "bl_region_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "type->gzmap_params.regionid");
  RNA_def_property_enum_items(prop, rna_enum_region_type_items);
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_def_property_ui_text(
      prop, "Region Type", "The region where the panel is going to be used in");

  prop = RNA_def_property(srna, "bl_owner_id", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "type->owner_id");
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

  /* bl_options */
  static const EnumPropertyItem gizmogroup_flag_items[] = {
      {WM_GIZMOGROUPTYPE_3D, "3D", 0, "3D", "Use in 3D viewport"},
      {WM_GIZMOGROUPTYPE_SCALE,
       "SCALE",
       0,
       "Scale",
       "Scale to respect zoom (otherwise zoom independent display size)"},
      {WM_GIZMOGROUPTYPE_DEPTH_3D,
       "DEPTH_3D",
       0,
       "Depth 3D",
       "Supports culled depth by other objects in the view"},
      {WM_GIZMOGROUPTYPE_SELECT, "SELECT", 0, "Select", "Supports selection"},
      {WM_GIZMOGROUPTYPE_PERSISTENT, "PERSISTENT", 0, "Persistent", ""},
      {WM_GIZMOGROUPTYPE_DRAW_MODAL_ALL,
       "SHOW_MODAL_ALL",
       0,
       "Show Modal All",
       "Show all while interacting, as well as this group when another is being interacted with"},
      {WM_GIZMOGROUPTYPE_DRAW_MODAL_EXCLUDE,
       "EXCLUDE_MODAL",
       0,
       "Exclude Modal",
       "Show all except this group while interacting"},
      {WM_GIZMOGROUPTYPE_TOOL_INIT,
       "TOOL_INIT",
       0,
       "Tool Init",
       "Postpone running until tool operator run (when used with a tool)"},
      {WM_GIZMOGROUPTYPE_TOOL_FALLBACK_KEYMAP,
       "TOOL_FALLBACK_KEYMAP",
       0,
       "Use fallback tools keymap",
       "Add fallback tools keymap to this gizmo type"},
      {WM_GIZMOGROUPTYPE_VR_REDRAWS,
       "VR_REDRAWS",
       0,
       "VR Redraws",
       "The gizmos are made for use with virtual reality sessions and require special redraw "
       "management"},
      {0, nullptr, 0, nullptr, nullptr},
  };
  prop = RNA_def_property(srna, "bl_options", PROP_ENUM, PROP_NONE);
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL | PROP_ENUM_FLAG);
  RNA_def_property_enum_sdna(prop, nullptr, "type->flag");
  RNA_def_property_enum_items(prop, gizmogroup_flag_items);
  RNA_def_property_ui_text(prop, "Options", "Options for this operator type");

  RNA_define_verify_sdna(true); /* not in sdna */

  /* Functions */

  /* poll */
  func = RNA_def_function(srna, "poll", nullptr);
  RNA_def_function_ui_description(func, "Test if the gizmo group can be called or not");
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_REGISTER_OPTIONAL);
  RNA_def_function_return(func, RNA_def_boolean(func, "visible", false, "", ""));
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  /* setup_keymap */
  func = RNA_def_function(srna, "setup_keymap", nullptr);
  RNA_def_function_ui_description(
      func, "Initialize keymaps for this gizmo group, use fallback keymap when not present");
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_REGISTER_OPTIONAL);
  parm = RNA_def_pointer(func, "keyconfig", "KeyConfig", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  /* return */
  parm = RNA_def_pointer(func, "keymap", "KeyMap", "", "");
  RNA_def_property_flag(parm, PROP_NEVER_NULL);
  RNA_def_function_return(func, parm);

  /* setup */
  func = RNA_def_function(srna, "setup", nullptr);
  RNA_def_function_ui_description(func, "Create gizmos function for the gizmo group");
  RNA_def_function_flag(func, FUNC_REGISTER);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  /* refresh */
  func = RNA_def_function(srna, "refresh", nullptr);
  RNA_def_function_ui_description(
      func, "Refresh data (called on common state changes such as selection)");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  func = RNA_def_function(srna, "draw_prepare", nullptr);
  RNA_def_function_ui_description(func, "Run before each redraw");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  func = RNA_def_function(srna, "invoke_prepare", nullptr);
  RNA_def_function_ui_description(func, "Run before invoke");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "gizmo", "Gizmo", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  /* -------------------------------------------------------------------- */
  /* Instance Variables */

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_string_funcs(
      prop, "rna_GizmoGroup_name_get", "rna_GizmoGroup_name_length", nullptr);
  RNA_def_property_ui_text(prop, "Name", "");

  RNA_define_verify_sdna(false); /* not in sdna */

  prop = RNA_def_property(srna, "gizmos", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "gizmos", nullptr);
  RNA_def_property_struct_type(prop, "Gizmo");
  RNA_def_property_collection_funcs(prop,
                                    "rna_GizmoGroup_gizmos_begin",
                                    "rna_iterator_listbase_next",
                                    "rna_iterator_listbase_end",
                                    "rna_iterator_listbase_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);

  RNA_def_property_ui_text(prop, "Gizmos", "List of gizmos in the Gizmo Map");
  rna_def_gizmo(brna, prop);
  rna_def_gizmos(brna, prop);

  RNA_define_verify_sdna(true); /* not in sdna */

  RNA_api_gizmogroup(srna);

  srna = RNA_def_struct(brna, "GizmoGroupProperties", nullptr);
  RNA_def_struct_ui_text(srna, "Gizmo Group Properties", "Input properties of a Gizmo Group");
  RNA_def_struct_refine_func(srna, "rna_GizmoGroupProperties_refine");
  RNA_def_struct_system_idprops_func(srna, "rna_GizmoGroupProperties_idprops");
  RNA_def_struct_flag(srna, STRUCT_NO_DATABLOCK_IDPROPERTIES);
}

void RNA_def_wm_gizmo(BlenderRNA *brna)
{
  rna_def_gizmogroup(brna);
}

#endif /* RNA_RUNTIME */
