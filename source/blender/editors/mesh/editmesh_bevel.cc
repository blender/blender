/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 */

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"

#include "BLI_math.h"
#include "BLI_string.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_global.h"
#include "BKE_layer.h"
#include "BKE_unit.h"

#include "DNA_curveprofile_types.h"
#include "DNA_mesh_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_prototypes.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "ED_mesh.h"
#include "ED_numinput.h"
#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_transform.h"
#include "ED_util.h"
#include "ED_view3d.h"

#include "mesh_intern.h" /* own include */

#define MVAL_PIXEL_MARGIN 5.0f

#define PROFILE_HARD_MIN 0.0f

#define SEGMENTS_HARD_MAX 1000

/* which value is mouse movement and numeric input controlling? */
#define OFFSET_VALUE 0
#define OFFSET_VALUE_PERCENT 1
#define PROFILE_VALUE 2
#define SEGMENTS_VALUE 3
#define NUM_VALUE_KINDS 4

static const char *value_rna_name[NUM_VALUE_KINDS] = {
    "offset", "offset_pct", "profile", "segments"};
static const float value_clamp_min[NUM_VALUE_KINDS] = {0.0f, 0.0f, PROFILE_HARD_MIN, 1.0f};
static const float value_clamp_max[NUM_VALUE_KINDS] = {1e6, 100.0f, 1.0f, SEGMENTS_HARD_MAX};
static const float value_start[NUM_VALUE_KINDS] = {0.0f, 0.0f, 0.5f, 1.0f};
static const float value_scale_per_inch[NUM_VALUE_KINDS] = {0.0f, 100.0f, 1.0f, 4.0f};

typedef struct {
  /** Every object must have a valid #BMEditMesh. */
  Object *ob;
  BMBackup mesh_backup;
} BevelObjectStore;

typedef struct {
  float initial_length[NUM_VALUE_KINDS];
  float scale[NUM_VALUE_KINDS];
  NumInput num_input[NUM_VALUE_KINDS];
  /** The current value when shift is pressed. Negative when shift not active. */
  float shift_value[NUM_VALUE_KINDS];
  float max_obj_scale;
  bool is_modal;

  BevelObjectStore *ob_store;
  uint ob_store_len;

  /* modal only */
  int launch_event;
  float mcenter[2];
  void *draw_handle_pixel;
  short value_mode; /* Which value does mouse movement and numeric input affect? */
  float segments;   /* Segments as float so smooth mouse pan works in small increments */

  CurveProfile *custom_profile;
} BevelData;

enum {
  BEV_MODAL_CANCEL = 1,
  BEV_MODAL_CONFIRM,
  BEV_MODAL_VALUE_OFFSET,
  BEV_MODAL_VALUE_PROFILE,
  BEV_MODAL_VALUE_SEGMENTS,
  BEV_MODAL_SEGMENTS_UP,
  BEV_MODAL_SEGMENTS_DOWN,
  BEV_MODAL_OFFSET_MODE_CHANGE,
  BEV_MODAL_CLAMP_OVERLAP_TOGGLE,
  BEV_MODAL_AFFECT_CHANGE,
  BEV_MODAL_HARDEN_NORMALS_TOGGLE,
  BEV_MODAL_MARK_SEAM_TOGGLE,
  BEV_MODAL_MARK_SHARP_TOGGLE,
  BEV_MODAL_OUTER_MITER_CHANGE,
  BEV_MODAL_INNER_MITER_CHANGE,
  BEV_MODAL_PROFILE_TYPE_CHANGE,
  BEV_MODAL_VERTEX_MESH_CHANGE,
};

static float get_bevel_offset(wmOperator *op)
{
  if (RNA_enum_get(op->ptr, "offset_type") == BEVEL_AMT_PERCENT) {
    return RNA_float_get(op->ptr, "offset_pct");
  }
  return RNA_float_get(op->ptr, "offset");
}

static void edbm_bevel_update_status_text(bContext *C, wmOperator *op)
{
  char status_text[UI_MAX_DRAW_STR];
  char buf[UI_MAX_DRAW_STR];
  char *p = buf;
  int available_len = sizeof(buf);
  Scene *sce = CTX_data_scene(C);

#define WM_MODALKEY(_id) \
  WM_modalkeymap_operator_items_to_string_buf( \
      op->type, (_id), true, UI_MAX_SHORTCUT_STR, &available_len, &p)

  char offset_str[NUM_STR_REP_LEN];
  if (RNA_enum_get(op->ptr, "offset_type") == BEVEL_AMT_PERCENT) {
    SNPRINTF(offset_str, "%.1f%%", RNA_float_get(op->ptr, "offset_pct"));
  }
  else {
    double offset_val = double(RNA_float_get(op->ptr, "offset"));
    BKE_unit_value_as_string(offset_str,
                             NUM_STR_REP_LEN,
                             offset_val * sce->unit.scale_length,
                             3,
                             B_UNIT_LENGTH,
                             &sce->unit,
                             true);
  }

  PropertyRNA *prop;
  const char *mode_str, *omiter_str, *imiter_str, *vmesh_str, *profile_type_str, *affect_str;
  prop = RNA_struct_find_property(op->ptr, "offset_type");
  RNA_property_enum_name_gettexted(
      C, op->ptr, prop, RNA_property_enum_get(op->ptr, prop), &mode_str);
  prop = RNA_struct_find_property(op->ptr, "profile_type");
  RNA_property_enum_name_gettexted(
      C, op->ptr, prop, RNA_property_enum_get(op->ptr, prop), &profile_type_str);
  prop = RNA_struct_find_property(op->ptr, "miter_outer");
  RNA_property_enum_name_gettexted(
      C, op->ptr, prop, RNA_property_enum_get(op->ptr, prop), &omiter_str);
  prop = RNA_struct_find_property(op->ptr, "miter_inner");
  RNA_property_enum_name_gettexted(
      C, op->ptr, prop, RNA_property_enum_get(op->ptr, prop), &imiter_str);
  prop = RNA_struct_find_property(op->ptr, "vmesh_method");
  RNA_property_enum_name_gettexted(
      C, op->ptr, prop, RNA_property_enum_get(op->ptr, prop), &vmesh_str);
  prop = RNA_struct_find_property(op->ptr, "affect");
  RNA_property_enum_name_gettexted(
      C, op->ptr, prop, RNA_property_enum_get(op->ptr, prop), &affect_str);

  SNPRINTF(status_text,
           TIP_("%s: Confirm, "
                "%s: Cancel, "
                "%s: Width Type (%s), "
                "%s: Width (%s), "
                "%s: Segments (%d), "
                "%s: Profile (%.3f), "
                "%s: Clamp Overlap (%s), "
                "%s: Affect (%s), "
                "%s: Outer Miter (%s), "
                "%s: Inner Miter (%s), "
                "%s: Harden Normals (%s), "
                "%s: Mark Seam (%s), "
                "%s: Mark Sharp (%s), "
                "%s: Profile Type (%s), "
                "%s: Intersection (%s)"),
           WM_MODALKEY(BEV_MODAL_CONFIRM),
           WM_MODALKEY(BEV_MODAL_CANCEL),
           WM_MODALKEY(BEV_MODAL_OFFSET_MODE_CHANGE),
           mode_str,
           WM_MODALKEY(BEV_MODAL_VALUE_OFFSET),
           offset_str,
           WM_MODALKEY(BEV_MODAL_VALUE_SEGMENTS),
           RNA_int_get(op->ptr, "segments"),
           WM_MODALKEY(BEV_MODAL_VALUE_PROFILE),
           RNA_float_get(op->ptr, "profile"),
           WM_MODALKEY(BEV_MODAL_CLAMP_OVERLAP_TOGGLE),
           WM_bool_as_string(RNA_boolean_get(op->ptr, "clamp_overlap")),
           WM_MODALKEY(BEV_MODAL_AFFECT_CHANGE),
           affect_str,
           WM_MODALKEY(BEV_MODAL_OUTER_MITER_CHANGE),
           omiter_str,
           WM_MODALKEY(BEV_MODAL_INNER_MITER_CHANGE),
           imiter_str,
           WM_MODALKEY(BEV_MODAL_HARDEN_NORMALS_TOGGLE),
           WM_bool_as_string(RNA_boolean_get(op->ptr, "harden_normals")),
           WM_MODALKEY(BEV_MODAL_MARK_SEAM_TOGGLE),
           WM_bool_as_string(RNA_boolean_get(op->ptr, "mark_seam")),
           WM_MODALKEY(BEV_MODAL_MARK_SHARP_TOGGLE),
           WM_bool_as_string(RNA_boolean_get(op->ptr, "mark_sharp")),
           WM_MODALKEY(BEV_MODAL_PROFILE_TYPE_CHANGE),
           profile_type_str,
           WM_MODALKEY(BEV_MODAL_VERTEX_MESH_CHANGE),
           vmesh_str);

#undef WM_MODALKEY

  ED_workspace_status_text(C, status_text);
}

static bool edbm_bevel_init(bContext *C, wmOperator *op, const bool is_modal)
{
  Scene *scene = CTX_data_scene(C);
  View3D *v3d = CTX_wm_view3d(C);
  ToolSettings *ts = CTX_data_tool_settings(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  if (is_modal) {
    RNA_float_set(op->ptr, "offset", 0.0f);
    RNA_float_set(op->ptr, "offset_pct", 0.0f);
  }

  op->customdata = MEM_mallocN(sizeof(BevelData), "beveldata_mesh_operator");
  BevelData *opdata = static_cast<BevelData *>(op->customdata);
  uint objects_used_len = 0;
  opdata->max_obj_scale = FLT_MIN;

  /* Put the Curve Profile from the toolsettings into the opdata struct */
  opdata->custom_profile = ts->custom_bevel_profile_preset;

  {
    uint ob_store_len = 0;
    Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
        scene, view_layer, v3d, &ob_store_len);
    opdata->ob_store = static_cast<BevelObjectStore *>(
        MEM_malloc_arrayN(ob_store_len, sizeof(*opdata->ob_store), __func__));
    for (uint ob_index = 0; ob_index < ob_store_len; ob_index++) {
      Object *obedit = objects[ob_index];
      float scale = mat4_to_scale(obedit->object_to_world);
      opdata->max_obj_scale = max_ff(opdata->max_obj_scale, scale);
      BMEditMesh *em = BKE_editmesh_from_object(obedit);
      if (em->bm->totvertsel > 0) {
        opdata->ob_store[objects_used_len].ob = obedit;
        objects_used_len++;
      }
    }
    MEM_freeN(objects);
    opdata->ob_store_len = objects_used_len;
  }

  opdata->is_modal = is_modal;
  int otype = RNA_enum_get(op->ptr, "offset_type");
  opdata->value_mode = (otype == BEVEL_AMT_PERCENT) ? OFFSET_VALUE_PERCENT : OFFSET_VALUE;
  opdata->segments = float(RNA_int_get(op->ptr, "segments"));
  float pixels_per_inch = U.dpi;

  for (int i = 0; i < NUM_VALUE_KINDS; i++) {
    opdata->shift_value[i] = -1.0f;
    opdata->initial_length[i] = -1.0f;
    /* NOTE: scale for #OFFSET_VALUE will get overwritten in #edbm_bevel_invoke. */
    opdata->scale[i] = value_scale_per_inch[i] / pixels_per_inch;

    initNumInput(&opdata->num_input[i]);
    opdata->num_input[i].idx_max = 0;
    opdata->num_input[i].val_flag[0] |= NUM_NO_NEGATIVE;
    opdata->num_input[i].unit_type[0] = B_UNIT_NONE;
    if (i == SEGMENTS_VALUE) {
      opdata->num_input[i].val_flag[0] |= NUM_NO_FRACTION | NUM_NO_ZERO;
    }
    if (i == OFFSET_VALUE) {
      opdata->num_input[i].unit_sys = scene->unit.system;
      opdata->num_input[i].unit_type[0] = B_UNIT_LENGTH;
    }
  }

  /* avoid the cost of allocating a bm copy */
  if (is_modal) {
    ARegion *region = CTX_wm_region(C);

    for (uint ob_index = 0; ob_index < opdata->ob_store_len; ob_index++) {
      Object *obedit = opdata->ob_store[ob_index].ob;
      BMEditMesh *em = BKE_editmesh_from_object(obedit);
      opdata->ob_store[ob_index].mesh_backup = EDBM_redo_state_store(em);
    }
    opdata->draw_handle_pixel = ED_region_draw_cb_activate(
        region->type, ED_region_draw_mouse_line_cb, opdata->mcenter, REGION_DRAW_POST_PIXEL);
    G.moving = G_TRANSFORM_EDIT;
  }

  return true;
}

static bool edbm_bevel_calc(wmOperator *op)
{
  BevelData *opdata = static_cast<BevelData *>(op->customdata);
  BMOperator bmop;
  bool changed = false;

  const float offset = get_bevel_offset(op);
  const int offset_type = RNA_enum_get(op->ptr, "offset_type");
  const int profile_type = RNA_enum_get(op->ptr, "profile_type");
  const int segments = RNA_int_get(op->ptr, "segments");
  const float profile = RNA_float_get(op->ptr, "profile");
  const bool affect = RNA_enum_get(op->ptr, "affect");
  const bool clamp_overlap = RNA_boolean_get(op->ptr, "clamp_overlap");
  const int material_init = RNA_int_get(op->ptr, "material");
  const bool loop_slide = RNA_boolean_get(op->ptr, "loop_slide");
  const bool mark_seam = RNA_boolean_get(op->ptr, "mark_seam");
  const bool mark_sharp = RNA_boolean_get(op->ptr, "mark_sharp");
  const bool harden_normals = RNA_boolean_get(op->ptr, "harden_normals");
  const int face_strength_mode = RNA_enum_get(op->ptr, "face_strength_mode");
  const int miter_outer = RNA_enum_get(op->ptr, "miter_outer");
  const int miter_inner = RNA_enum_get(op->ptr, "miter_inner");
  const float spread = RNA_float_get(op->ptr, "spread");
  const int vmesh_method = RNA_enum_get(op->ptr, "vmesh_method");

  for (uint ob_index = 0; ob_index < opdata->ob_store_len; ob_index++) {
    Object *obedit = opdata->ob_store[ob_index].ob;
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    /* revert to original mesh */
    if (opdata->is_modal) {
      EDBM_redo_state_restore(&opdata->ob_store[ob_index].mesh_backup, em, false);
    }

    const int material = CLAMPIS(material_init, -1, obedit->totcol - 1);

    Mesh *me = static_cast<Mesh *>(obedit->data);

    if (harden_normals && !(me->flag & ME_AUTOSMOOTH)) {
      /* `harden_normals` only has a visible effect if auto-smooth is on, so turn it on. */
      me->flag |= ME_AUTOSMOOTH;
    }

    EDBM_op_init(em,
                 &bmop,
                 op,
                 "bevel geom=%hev offset=%f segments=%i affect=%i offset_type=%i "
                 "profile_type=%i profile=%f clamp_overlap=%b material=%i loop_slide=%b "
                 "mark_seam=%b mark_sharp=%b harden_normals=%b face_strength_mode=%i "
                 "miter_outer=%i miter_inner=%i spread=%f smoothresh=%f custom_profile=%p "
                 "vmesh_method=%i",
                 BM_ELEM_SELECT,
                 offset,
                 segments,
                 affect,
                 offset_type,
                 profile_type,
                 profile,
                 clamp_overlap,
                 material,
                 loop_slide,
                 mark_seam,
                 mark_sharp,
                 harden_normals,
                 face_strength_mode,
                 miter_outer,
                 miter_inner,
                 spread,
                 me->smoothresh,
                 opdata->custom_profile,
                 vmesh_method);

    BMO_op_exec(em->bm, &bmop);

    if (offset != 0.0f) {
      /* Not essential, but we may have some loose geometry that
       * won't get beveled and better not leave it selected. */
      EDBM_flag_disable_all(em, BM_ELEM_SELECT);
      BMO_slot_buffer_hflag_enable(
          em->bm, bmop.slots_out, "faces.out", BM_FACE, BM_ELEM_SELECT, true);
    }

    /* no need to de-select existing geometry */
    if (!EDBM_op_finish(em, &bmop, op, true)) {
      continue;
    }

    EDBMUpdate_Params params{};
    params.calc_looptri = true;
    params.calc_normals = true;
    params.is_destructive = true;
    EDBM_update(static_cast<Mesh *>(obedit->data), &params);
    changed = true;
  }
  return changed;
}

static void edbm_bevel_exit(bContext *C, wmOperator *op)
{
  BevelData *opdata = static_cast<BevelData *>(op->customdata);
  ScrArea *area = CTX_wm_area(C);

  if (area) {
    ED_area_status_text(area, nullptr);
  }

  for (uint ob_index = 0; ob_index < opdata->ob_store_len; ob_index++) {
    Object *obedit = opdata->ob_store[ob_index].ob;
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    /* Without this, faces surrounded by selected edges/verts will be unselected. */
    if ((em->selectmode & SCE_SELECT_FACE) == 0) {
      EDBM_selectmode_flush(em);
    }
  }

  if (opdata->is_modal) {
    ARegion *region = CTX_wm_region(C);
    for (uint ob_index = 0; ob_index < opdata->ob_store_len; ob_index++) {
      EDBM_redo_state_free(&opdata->ob_store[ob_index].mesh_backup);
    }
    ED_region_draw_cb_exit(region->type, opdata->draw_handle_pixel);
    G.moving = 0;
  }
  MEM_SAFE_FREE(opdata->ob_store);
  MEM_SAFE_FREE(op->customdata);
  op->customdata = nullptr;
}

static void edbm_bevel_cancel(bContext *C, wmOperator *op)
{
  BevelData *opdata = static_cast<BevelData *>(op->customdata);
  if (opdata->is_modal) {
    for (uint ob_index = 0; ob_index < opdata->ob_store_len; ob_index++) {
      Object *obedit = opdata->ob_store[ob_index].ob;
      BMEditMesh *em = BKE_editmesh_from_object(obedit);
      EDBM_redo_state_restore_and_free(&opdata->ob_store[ob_index].mesh_backup, em, true);

      EDBMUpdate_Params params{};
      params.calc_looptri = false;
      params.calc_normals = true;
      params.is_destructive = true;
      EDBM_update(static_cast<Mesh *>(obedit->data), &params);
    }
  }

  edbm_bevel_exit(C, op);

  /* Need to force re-display or we may still view the modified result. */
  ED_region_tag_redraw(CTX_wm_region(C));
}

/* bevel! yay!! */
static int edbm_bevel_exec(bContext *C, wmOperator *op)
{
  if (!edbm_bevel_init(C, op, false)) {
    return OPERATOR_CANCELLED;
  }

  if (!edbm_bevel_calc(op)) {
    edbm_bevel_cancel(C, op);
    return OPERATOR_CANCELLED;
  }

  edbm_bevel_exit(C, op);

  return OPERATOR_FINISHED;
}

static void edbm_bevel_calc_initial_length(wmOperator *op, const wmEvent *event, bool mode_changed)
{
  BevelData *opdata = static_cast<BevelData *>(op->customdata);
  const float mlen[2] = {
      opdata->mcenter[0] - event->mval[0],
      opdata->mcenter[1] - event->mval[1],
  };
  float len = len_v2(mlen);
  int vmode = opdata->value_mode;
  if (mode_changed || opdata->initial_length[vmode] == -1.0f) {
    /* If current value is not default start value, adjust len so that
     * the scaling and offset in edbm_bevel_mouse_set_value will
     * start at current value */
    float value = (vmode == SEGMENTS_VALUE) ? opdata->segments :
                                              RNA_float_get(op->ptr, value_rna_name[vmode]);
    float sc = opdata->scale[vmode];
    float st = value_start[vmode];
    if (value != value_start[vmode]) {
      len = (st + sc * (len - MVAL_PIXEL_MARGIN) - value) / sc;
    }
  }
  opdata->initial_length[opdata->value_mode] = len;
}

static int edbm_bevel_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  RegionView3D *rv3d = CTX_wm_region_view3d(C);

  if (!edbm_bevel_init(C, op, true)) {
    return OPERATOR_CANCELLED;
  }

  BevelData *opdata = static_cast<BevelData *>(op->customdata);

  opdata->launch_event = WM_userdef_event_type_from_keymap_type(event->type);

  /* initialize mouse values */
  float center_3d[3];
  if (!calculateTransformCenter(C, V3D_AROUND_CENTER_MEDIAN, center_3d, opdata->mcenter)) {
    /* in this case the tool will likely do nothing,
     * ideally this will never happen and should be checked for above */
    opdata->mcenter[0] = opdata->mcenter[1] = 0;
  }

  /* for OFFSET_VALUE only, the scale is the size of a pixel under the mouse in 3d space */
  opdata->scale[OFFSET_VALUE] = rv3d ? ED_view3d_pixel_size(rv3d, center_3d) : 1.0f;
  /* since we are affecting untransformed object but seeing in transformed space,
   * compensate for that */
  opdata->scale[OFFSET_VALUE] /= opdata->max_obj_scale;

  edbm_bevel_calc_initial_length(op, event, false);

  edbm_bevel_update_status_text(C, op);

  if (!edbm_bevel_calc(op)) {
    edbm_bevel_cancel(C, op);
    ED_workspace_status_text(C, nullptr);
    return OPERATOR_CANCELLED;
  }

  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static void edbm_bevel_mouse_set_value(wmOperator *op, const wmEvent *event)
{
  BevelData *opdata = static_cast<BevelData *>(op->customdata);
  int vmode = opdata->value_mode;

  const float mdiff[2] = {
      opdata->mcenter[0] - event->mval[0],
      opdata->mcenter[1] - event->mval[1],
  };

  float value = ((len_v2(mdiff) - MVAL_PIXEL_MARGIN) - opdata->initial_length[vmode]);

  /* Scale according to value mode */
  value = value_start[vmode] + value * opdata->scale[vmode];

  /* Fake shift-transform... */
  if (event->modifier & KM_SHIFT) {
    if (opdata->shift_value[vmode] < 0.0f) {
      opdata->shift_value[vmode] = (vmode == SEGMENTS_VALUE) ?
                                       opdata->segments :
                                       RNA_float_get(op->ptr, value_rna_name[vmode]);
    }
    value = (value - opdata->shift_value[vmode]) * 0.1f + opdata->shift_value[vmode];
  }
  else if (opdata->shift_value[vmode] >= 0.0f) {
    opdata->shift_value[vmode] = -1.0f;
  }

  /* Clamp according to value mode, and store value back. */
  CLAMP(value, value_clamp_min[vmode], value_clamp_max[vmode]);
  if (vmode == SEGMENTS_VALUE) {
    opdata->segments = value;
    RNA_int_set(op->ptr, "segments", int(value + 0.5f));
  }
  else {
    RNA_float_set(op->ptr, value_rna_name[vmode], value);
  }
}

static void edbm_bevel_numinput_set_value(wmOperator *op)
{
  BevelData *opdata = static_cast<BevelData *>(op->customdata);

  int vmode = opdata->value_mode;
  float value = (vmode == SEGMENTS_VALUE) ? opdata->segments :
                                            RNA_float_get(op->ptr, value_rna_name[vmode]);
  applyNumInput(&opdata->num_input[vmode], &value);
  CLAMP(value, value_clamp_min[vmode], value_clamp_max[vmode]);
  if (vmode == SEGMENTS_VALUE) {
    opdata->segments = value;
    RNA_int_set(op->ptr, "segments", int(value));
  }
  else {
    RNA_float_set(op->ptr, value_rna_name[vmode], value);
  }
}

wmKeyMap *bevel_modal_keymap(wmKeyConfig *keyconf)
{
  static const EnumPropertyItem modal_items[] = {
      {BEV_MODAL_CANCEL, "CANCEL", 0, "Cancel", "Cancel bevel"},
      {BEV_MODAL_CONFIRM, "CONFIRM", 0, "Confirm", "Confirm bevel"},
      {BEV_MODAL_VALUE_OFFSET, "VALUE_OFFSET", 0, "Change Offset", "Value changes offset"},
      {BEV_MODAL_VALUE_PROFILE, "VALUE_PROFILE", 0, "Change Profile", "Value changes profile"},
      {BEV_MODAL_VALUE_SEGMENTS, "VALUE_SEGMENTS", 0, "Change Segments", "Value changes segments"},
      {BEV_MODAL_SEGMENTS_UP, "SEGMENTS_UP", 0, "Increase Segments", "Increase segments"},
      {BEV_MODAL_SEGMENTS_DOWN, "SEGMENTS_DOWN", 0, "Decrease Segments", "Decrease segments"},
      {BEV_MODAL_OFFSET_MODE_CHANGE,
       "OFFSET_MODE_CHANGE",
       0,
       "Change Offset Mode",
       "Cycle through offset modes"},
      {BEV_MODAL_CLAMP_OVERLAP_TOGGLE,
       "CLAMP_OVERLAP_TOGGLE",
       0,
       "Toggle Clamp Overlap",
       "Toggle clamp overlap flag"},
      {BEV_MODAL_AFFECT_CHANGE,
       "AFFECT_CHANGE",
       0,
       "Change Affect Type",
       "Change which geometry type the operation affects, edges or vertices"},
      {BEV_MODAL_HARDEN_NORMALS_TOGGLE,
       "HARDEN_NORMALS_TOGGLE",
       0,
       "Toggle Harden Normals",
       "Toggle harden normals flag"},
      {BEV_MODAL_MARK_SEAM_TOGGLE,
       "MARK_SEAM_TOGGLE",
       0,
       "Toggle Mark Seam",
       "Toggle mark seam flag"},
      {BEV_MODAL_MARK_SHARP_TOGGLE,
       "MARK_SHARP_TOGGLE",
       0,
       "Toggle Mark Sharp",
       "Toggle mark sharp flag"},
      {BEV_MODAL_OUTER_MITER_CHANGE,
       "OUTER_MITER_CHANGE",
       0,
       "Change Outer Miter",
       "Cycle through outer miter kinds"},
      {BEV_MODAL_INNER_MITER_CHANGE,
       "INNER_MITER_CHANGE",
       0,
       "Change Inner Miter",
       "Cycle through inner miter kinds"},
      {BEV_MODAL_PROFILE_TYPE_CHANGE, "PROFILE_TYPE_CHANGE", 0, "Cycle through profile types", ""},
      {BEV_MODAL_VERTEX_MESH_CHANGE,
       "VERTEX_MESH_CHANGE",
       0,
       "Change Intersection Method",
       "Cycle through intersection methods"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  wmKeyMap *keymap = WM_modalkeymap_find(keyconf, "Bevel Modal Map");

  /* This function is called for each space-type, only needs to add map once. */
  if (keymap && keymap->modal_items) {
    return nullptr;
  }

  keymap = WM_modalkeymap_ensure(keyconf, "Bevel Modal Map", modal_items);

  WM_modalkeymap_assign(keymap, "MESH_OT_bevel");

  return keymap;
}

static int edbm_bevel_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  BevelData *opdata = static_cast<BevelData *>(op->customdata);
  const bool has_numinput = hasNumInput(&opdata->num_input[opdata->value_mode]);
  bool handled = false;
  short etype = event->type;
  short eval = event->val;

  /* When activated from toolbar, need to convert left-mouse release to confirm. */
  if (ELEM(etype, LEFTMOUSE, opdata->launch_event) && (eval == KM_RELEASE) &&
      RNA_boolean_get(op->ptr, "release_confirm"))
  {
    etype = EVT_MODAL_MAP;
    eval = BEV_MODAL_CONFIRM;
  }
  /* Modal numinput active, try to handle numeric inputs first... */
  if (etype != EVT_MODAL_MAP && eval == KM_PRESS && has_numinput &&
      handleNumInput(C, &opdata->num_input[opdata->value_mode], event))
  {
    edbm_bevel_numinput_set_value(op);
    edbm_bevel_calc(op);
    edbm_bevel_update_status_text(C, op);
    return OPERATOR_RUNNING_MODAL;
  }
  if (etype == MOUSEMOVE) {
    if (!has_numinput) {
      edbm_bevel_mouse_set_value(op, event);
      edbm_bevel_calc(op);
      edbm_bevel_update_status_text(C, op);
      handled = true;
    }
  }
  else if (etype == MOUSEPAN) {
    float delta = 0.02f * (event->xy[1] - event->prev_xy[1]);
    if (opdata->segments >= 1 && opdata->segments + delta < 1) {
      opdata->segments = 1;
    }
    else {
      opdata->segments += delta;
    }
    RNA_int_set(op->ptr, "segments", int(opdata->segments));
    edbm_bevel_calc(op);
    edbm_bevel_update_status_text(C, op);
    handled = true;
  }
  else if (etype == EVT_MODAL_MAP) {
    switch (eval) {
      case BEV_MODAL_CANCEL:
        edbm_bevel_cancel(C, op);
        ED_workspace_status_text(C, nullptr);
        return OPERATOR_CANCELLED;

      case BEV_MODAL_CONFIRM:
        edbm_bevel_calc(op);
        edbm_bevel_exit(C, op);
        ED_workspace_status_text(C, nullptr);
        return OPERATOR_FINISHED;

      case BEV_MODAL_SEGMENTS_UP:
        opdata->segments = opdata->segments + 1;
        RNA_int_set(op->ptr, "segments", int(opdata->segments));
        edbm_bevel_calc(op);
        edbm_bevel_update_status_text(C, op);
        handled = true;
        break;

      case BEV_MODAL_SEGMENTS_DOWN:
        opdata->segments = max_ff(opdata->segments - 1, 1);
        RNA_int_set(op->ptr, "segments", int(opdata->segments));
        edbm_bevel_calc(op);
        edbm_bevel_update_status_text(C, op);
        handled = true;
        break;

      case BEV_MODAL_OFFSET_MODE_CHANGE: {
        int type = RNA_enum_get(op->ptr, "offset_type");
        type++;
        if (type > BEVEL_AMT_PERCENT) {
          type = BEVEL_AMT_OFFSET;
        }
        if (opdata->value_mode == OFFSET_VALUE && type == BEVEL_AMT_PERCENT) {
          opdata->value_mode = OFFSET_VALUE_PERCENT;
        }
        else if (opdata->value_mode == OFFSET_VALUE_PERCENT && type != BEVEL_AMT_PERCENT) {
          opdata->value_mode = OFFSET_VALUE;
        }
        RNA_enum_set(op->ptr, "offset_type", type);
        if (opdata->initial_length[opdata->value_mode] == -1.0f) {
          edbm_bevel_calc_initial_length(op, event, true);
        }
      }
        /* Update offset accordingly to new offset_type. */
        if (!has_numinput && ELEM(opdata->value_mode, OFFSET_VALUE, OFFSET_VALUE_PERCENT)) {
          edbm_bevel_mouse_set_value(op, event);
        }
        edbm_bevel_calc(op);
        edbm_bevel_update_status_text(C, op);
        handled = true;
        break;

      case BEV_MODAL_CLAMP_OVERLAP_TOGGLE: {
        bool clamp_overlap = RNA_boolean_get(op->ptr, "clamp_overlap");
        RNA_boolean_set(op->ptr, "clamp_overlap", !clamp_overlap);
        edbm_bevel_calc(op);
        edbm_bevel_update_status_text(C, op);
        handled = true;
        break;
      }

      case BEV_MODAL_VALUE_OFFSET:
        opdata->value_mode = OFFSET_VALUE;
        edbm_bevel_calc_initial_length(op, event, true);
        break;

      case BEV_MODAL_VALUE_PROFILE:
        opdata->value_mode = PROFILE_VALUE;
        edbm_bevel_calc_initial_length(op, event, true);
        break;

      case BEV_MODAL_VALUE_SEGMENTS:
        opdata->value_mode = SEGMENTS_VALUE;
        edbm_bevel_calc_initial_length(op, event, true);
        break;

      case BEV_MODAL_AFFECT_CHANGE: {
        int affect_type = RNA_enum_get(op->ptr, "affect");
        affect_type++;
        if (affect_type > BEVEL_AFFECT_EDGES) {
          affect_type = BEVEL_AFFECT_VERTICES;
        }
        RNA_enum_set(op->ptr, "affect", affect_type);
        edbm_bevel_calc(op);
        edbm_bevel_update_status_text(C, op);
        handled = true;
        break;
      }

      case BEV_MODAL_MARK_SEAM_TOGGLE: {
        bool mark_seam = RNA_boolean_get(op->ptr, "mark_seam");
        RNA_boolean_set(op->ptr, "mark_seam", !mark_seam);
        edbm_bevel_calc(op);
        edbm_bevel_update_status_text(C, op);
        handled = true;
        break;
      }

      case BEV_MODAL_MARK_SHARP_TOGGLE: {
        bool mark_sharp = RNA_boolean_get(op->ptr, "mark_sharp");
        RNA_boolean_set(op->ptr, "mark_sharp", !mark_sharp);
        edbm_bevel_calc(op);
        edbm_bevel_update_status_text(C, op);
        handled = true;
        break;
      }

      case BEV_MODAL_INNER_MITER_CHANGE: {
        int miter_inner = RNA_enum_get(op->ptr, "miter_inner");
        miter_inner++;
        if (miter_inner == BEVEL_MITER_PATCH) {
          miter_inner++; /* no patch option for inner miter */
        }
        if (miter_inner > BEVEL_MITER_ARC) {
          miter_inner = BEVEL_MITER_SHARP;
        }
        RNA_enum_set(op->ptr, "miter_inner", miter_inner);
        edbm_bevel_calc(op);
        edbm_bevel_update_status_text(C, op);
        handled = true;
        break;
      }

      case BEV_MODAL_OUTER_MITER_CHANGE: {
        int miter_outer = RNA_enum_get(op->ptr, "miter_outer");
        miter_outer++;
        if (miter_outer > BEVEL_MITER_ARC) {
          miter_outer = BEVEL_MITER_SHARP;
        }
        RNA_enum_set(op->ptr, "miter_outer", miter_outer);
        edbm_bevel_calc(op);
        edbm_bevel_update_status_text(C, op);
        handled = true;
        break;
      }

      case BEV_MODAL_HARDEN_NORMALS_TOGGLE: {
        bool harden_normals = RNA_boolean_get(op->ptr, "harden_normals");
        RNA_boolean_set(op->ptr, "harden_normals", !harden_normals);
        edbm_bevel_calc(op);
        edbm_bevel_update_status_text(C, op);
        handled = true;
        break;
      }

      case BEV_MODAL_PROFILE_TYPE_CHANGE: {
        int profile_type = RNA_enum_get(op->ptr, "profile_type");
        profile_type++;
        if (profile_type > BEVEL_PROFILE_CUSTOM) {
          profile_type = BEVEL_PROFILE_SUPERELLIPSE;
        }
        RNA_enum_set(op->ptr, "profile_type", profile_type);
        edbm_bevel_calc(op);
        edbm_bevel_update_status_text(C, op);
        handled = true;
        break;
      }

      case BEV_MODAL_VERTEX_MESH_CHANGE: {
        int vmesh_method = RNA_enum_get(op->ptr, "vmesh_method");
        vmesh_method++;
        if (vmesh_method > BEVEL_VMESH_CUTOFF) {
          vmesh_method = BEVEL_VMESH_ADJ;
        }
        RNA_enum_set(op->ptr, "vmesh_method", vmesh_method);
        edbm_bevel_calc(op);
        edbm_bevel_update_status_text(C, op);
        handled = true;
        break;
      }
    }
  }

  /* Modal numinput inactive, try to handle numeric inputs last... */
  if (!handled && eval == KM_PRESS &&
      handleNumInput(C, &opdata->num_input[opdata->value_mode], event))
  {
    edbm_bevel_numinput_set_value(op);
    edbm_bevel_calc(op);
    edbm_bevel_update_status_text(C, op);
    return OPERATOR_RUNNING_MODAL;
  }

  return OPERATOR_RUNNING_MODAL;
}

static void edbm_bevel_ui(bContext *C, wmOperator *op)
{
  uiLayout *layout = op->layout;
  uiLayout *col, *row;
  PointerRNA toolsettings_ptr;

  int profile_type = RNA_enum_get(op->ptr, "profile_type");
  int offset_type = RNA_enum_get(op->ptr, "offset_type");
  bool affect_type = RNA_enum_get(op->ptr, "affect");

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  row = uiLayoutRow(layout, false);
  uiItemR(row, op->ptr, "affect", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);

  uiItemS(layout);

  uiItemR(layout, op->ptr, "offset_type", 0, nullptr, ICON_NONE);

  if (offset_type == BEVEL_AMT_PERCENT) {
    uiItemR(layout, op->ptr, "offset_pct", 0, nullptr, ICON_NONE);
  }
  else {
    uiItemR(layout, op->ptr, "offset", 0, nullptr, ICON_NONE);
  }

  uiItemR(layout, op->ptr, "segments", 0, nullptr, ICON_NONE);
  if (ELEM(profile_type, BEVEL_PROFILE_SUPERELLIPSE, BEVEL_PROFILE_CUSTOM)) {
    uiItemR(layout,
            op->ptr,
            "profile",
            UI_ITEM_R_SLIDER,
            (profile_type == BEVEL_PROFILE_SUPERELLIPSE) ? IFACE_("Shape") : IFACE_("Miter Shape"),
            ICON_NONE);
  }
  uiItemR(layout, op->ptr, "material", 0, nullptr, ICON_NONE);

  col = uiLayoutColumn(layout, true);
  uiItemR(col, op->ptr, "harden_normals", 0, nullptr, ICON_NONE);
  uiItemR(col, op->ptr, "clamp_overlap", 0, nullptr, ICON_NONE);
  uiItemR(col, op->ptr, "loop_slide", 0, nullptr, ICON_NONE);

  col = uiLayoutColumnWithHeading(layout, true, IFACE_("Mark"));
  uiLayoutSetActive(col, affect_type == BEVEL_AFFECT_EDGES);
  uiItemR(col, op->ptr, "mark_seam", 0, IFACE_("Seams"), ICON_NONE);
  uiItemR(col, op->ptr, "mark_sharp", 0, IFACE_("Sharp"), ICON_NONE);

  uiItemS(layout);

  col = uiLayoutColumn(layout, false);
  uiLayoutSetActive(col, affect_type == BEVEL_AFFECT_EDGES);
  uiItemR(col, op->ptr, "miter_outer", 0, IFACE_("Miter Outer"), ICON_NONE);
  uiItemR(col, op->ptr, "miter_inner", 0, IFACE_("Inner"), ICON_NONE);
  if (RNA_enum_get(op->ptr, "miter_inner") == BEVEL_MITER_ARC) {
    uiItemR(col, op->ptr, "spread", 0, nullptr, ICON_NONE);
  }

  uiItemS(layout);

  col = uiLayoutColumn(layout, false);
  uiLayoutSetActive(col, affect_type == BEVEL_AFFECT_EDGES);
  uiItemR(col, op->ptr, "vmesh_method", 0, IFACE_("Intersection Type"), ICON_NONE);

  uiItemR(layout, op->ptr, "face_strength_mode", 0, IFACE_("Face Strength"), ICON_NONE);

  uiItemS(layout);

  row = uiLayoutRow(layout, false);
  uiItemR(row, op->ptr, "profile_type", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
  if (profile_type == BEVEL_PROFILE_CUSTOM) {
    /* Get an RNA pointer to ToolSettings to give to the curve profile template code. */
    Scene *scene = CTX_data_scene(C);
    RNA_pointer_create(&scene->id, &RNA_ToolSettings, scene->toolsettings, &toolsettings_ptr);
    uiTemplateCurveProfile(layout, &toolsettings_ptr, "custom_bevel_profile_preset");
  }
}

void MESH_OT_bevel(wmOperatorType *ot)
{
  PropertyRNA *prop;

  static const EnumPropertyItem offset_type_items[] = {
      {BEVEL_AMT_OFFSET, "OFFSET", 0, "Offset", "Amount is offset of new edges from original"},
      {BEVEL_AMT_WIDTH, "WIDTH", 0, "Width", "Amount is width of new face"},
      {BEVEL_AMT_DEPTH,
       "DEPTH",
       0,
       "Depth",
       "Amount is perpendicular distance from original edge to bevel face"},
      {BEVEL_AMT_PERCENT, "PERCENT", 0, "Percent", "Amount is percent of adjacent edge length"},
      {BEVEL_AMT_ABSOLUTE,
       "ABSOLUTE",
       0,
       "Absolute",
       "Amount is absolute distance along adjacent edge"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem prop_profile_type_items[] = {
      {BEVEL_PROFILE_SUPERELLIPSE,
       "SUPERELLIPSE",
       0,
       "Superellipse",
       "The profile can be a concave or convex curve"},
      {BEVEL_PROFILE_CUSTOM,
       "CUSTOM",
       0,
       "Custom",
       "The profile can be any arbitrary path between its endpoints"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem face_strength_mode_items[] = {
      {BEVEL_FACE_STRENGTH_NONE, "NONE", 0, "None", "Do not set face strength"},
      {BEVEL_FACE_STRENGTH_NEW, "NEW", 0, "New", "Set face strength on new faces only"},
      {BEVEL_FACE_STRENGTH_AFFECTED,
       "AFFECTED",
       0,
       "Affected",
       "Set face strength on new and modified faces only"},
      {BEVEL_FACE_STRENGTH_ALL, "ALL", 0, "All", "Set face strength on all faces"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem miter_outer_items[] = {
      {BEVEL_MITER_SHARP, "SHARP", 0, "Sharp", "Outside of miter is sharp"},
      {BEVEL_MITER_PATCH, "PATCH", 0, "Patch", "Outside of miter is squared-off patch"},
      {BEVEL_MITER_ARC, "ARC", 0, "Arc", "Outside of miter is arc"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem miter_inner_items[] = {
      {BEVEL_MITER_SHARP, "SHARP", 0, "Sharp", "Inside of miter is sharp"},
      {BEVEL_MITER_ARC, "ARC", 0, "Arc", "Inside of miter is arc"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static EnumPropertyItem vmesh_method_items[] = {
      {BEVEL_VMESH_ADJ, "ADJ", 0, "Grid Fill", "Default patterned fill"},
      {BEVEL_VMESH_CUTOFF,
       "CUTOFF",
       0,
       "Cutoff",
       "A cutoff at each profile's end before the intersection"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem prop_affect_items[] = {
      {BEVEL_AFFECT_VERTICES, "VERTICES", 0, "Vertices", "Affect only vertices"},
      {BEVEL_AFFECT_EDGES, "EDGES", 0, "Edges", "Affect only edges"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Bevel";
  ot->description = "Cut into selected items at an angle to create bevel or chamfer";
  ot->idname = "MESH_OT_bevel";

  /* api callbacks */
  ot->exec = edbm_bevel_exec;
  ot->invoke = edbm_bevel_invoke;
  ot->modal = edbm_bevel_modal;
  ot->cancel = edbm_bevel_cancel;
  ot->poll = ED_operator_editmesh;
  ot->ui = edbm_bevel_ui;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_GRAB_CURSOR_XY | OPTYPE_BLOCKING;

  /* properties */
  RNA_def_enum(ot->srna,
               "offset_type",
               offset_type_items,
               0,
               "Width Type",
               "The method for determining the size of the bevel");
  prop = RNA_def_property(ot->srna, "offset", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_range(prop, 0.0, 1e6);
  RNA_def_property_ui_range(prop, 0.0, 100.0, 1, 3);
  RNA_def_property_ui_text(prop, "Width", "Bevel amount");

  RNA_def_enum(ot->srna,
               "profile_type",
               prop_profile_type_items,
               0,
               "Profile Type",
               "The type of shape used to rebuild a beveled section");

  prop = RNA_def_property(ot->srna, "offset_pct", PROP_FLOAT, PROP_PERCENTAGE);
  RNA_def_property_range(prop, 0.0, 100);
  RNA_def_property_ui_text(prop, "Width Percent", "Bevel amount for percentage method");

  RNA_def_int(ot->srna,
              "segments",
              1,
              1,
              SEGMENTS_HARD_MAX,
              "Segments",
              "Segments for curved edge",
              1,
              100);

  RNA_def_float(ot->srna,
                "profile",
                0.5f,
                PROFILE_HARD_MIN,
                1.0f,
                "Profile",
                "Controls profile shape (0.5 = round)",
                PROFILE_HARD_MIN,
                1.0f);

  RNA_def_enum(ot->srna,
               "affect",
               prop_affect_items,
               BEVEL_AFFECT_EDGES,
               "Affect",
               "Affect edges or vertices");

  RNA_def_boolean(ot->srna,
                  "clamp_overlap",
                  false,
                  "Clamp Overlap",
                  "Do not allow beveled edges/vertices to overlap each other");

  RNA_def_boolean(
      ot->srna, "loop_slide", true, "Loop Slide", "Prefer sliding along edges to even widths");

  RNA_def_boolean(ot->srna, "mark_seam", false, "Mark Seams", "Mark Seams along beveled edges");

  RNA_def_boolean(ot->srna, "mark_sharp", false, "Mark Sharp", "Mark beveled edges as sharp");

  RNA_def_int(ot->srna,
              "material",
              -1,
              -1,
              INT_MAX,
              "Material Index",
              "Material for bevel faces (-1 means use adjacent faces)",
              -1,
              100);

  RNA_def_boolean(ot->srna,
                  "harden_normals",
                  false,
                  "Harden Normals",
                  "Match normals of new faces to adjacent faces");

  RNA_def_enum(ot->srna,
               "face_strength_mode",
               face_strength_mode_items,
               BEVEL_FACE_STRENGTH_NONE,
               "Face Strength Mode",
               "Whether to set face strength, and which faces to set face strength on");

  RNA_def_enum(ot->srna,
               "miter_outer",
               miter_outer_items,
               BEVEL_MITER_SHARP,
               "Outer Miter",
               "Pattern to use for outside of miters");

  RNA_def_enum(ot->srna,
               "miter_inner",
               miter_inner_items,
               BEVEL_MITER_SHARP,
               "Inner Miter",
               "Pattern to use for inside of miters");

  RNA_def_float(ot->srna,
                "spread",
                0.1f,
                0.0f,
                1e6f,
                "Spread",
                "Amount to spread arcs for arc inner miters",
                0.0f,
                100.0f);

  RNA_def_enum(ot->srna,
               "vmesh_method",
               vmesh_method_items,
               BEVEL_VMESH_ADJ,
               "Vertex Mesh Method",
               "The method to use to create meshes at intersections");

  prop = RNA_def_boolean(ot->srna, "release_confirm", false, "Confirm on Release", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}
