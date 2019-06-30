/*
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
 */

/** \file
 * \ingroup edmesh
 */

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"

#include "BLI_string.h"
#include "BLI_math.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_editmesh.h"
#include "BKE_unit.h"
#include "BKE_layer.h"
#include "BKE_mesh.h"

#include "DNA_mesh_types.h"

#include "RNA_define.h"
#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"

#include "ED_mesh.h"
#include "ED_numinput.h"
#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_transform.h"
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
  BMEditMesh *em;
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
  float mcenter[2];
  void *draw_handle_pixel;
  short gizmo_flag;
  short value_mode; /* Which value does mouse movement and numeric input affect? */
  float segments;   /* Segments as float so smooth mouse pan works in small increments */
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
  BEV_MODAL_VERTEX_ONLY_TOGGLE,
  BEV_MODAL_HARDEN_NORMALS_TOGGLE,
  BEV_MODAL_MARK_SEAM_TOGGLE,
  BEV_MODAL_MARK_SHARP_TOGGLE,
  BEV_MODAL_OUTER_MITER_CHANGE,
  BEV_MODAL_INNER_MITER_CHANGE,
};

static float get_bevel_offset(wmOperator *op)
{
  float val;

  if (RNA_enum_get(op->ptr, "offset_type") == BEVEL_AMT_PERCENT) {
    val = RNA_float_get(op->ptr, "offset_pct");
  }
  else {
    val = RNA_float_get(op->ptr, "offset");
  }
  return val;
}

static void edbm_bevel_update_header(bContext *C, wmOperator *op)
{
  char header[UI_MAX_DRAW_STR];
  char buf[UI_MAX_DRAW_STR];
  char *p = buf;
  int available_len = sizeof(buf);
  Scene *sce = CTX_data_scene(C);
  char offset_str[NUM_STR_REP_LEN];
  const char *mode_str, *omiter_str, *imiter_str;
  PropertyRNA *prop;

#define WM_MODALKEY(_id) \
  WM_modalkeymap_operator_items_to_string_buf( \
      op->type, (_id), true, UI_MAX_SHORTCUT_STR, &available_len, &p)

  if (RNA_enum_get(op->ptr, "offset_type") == BEVEL_AMT_PERCENT) {
    BLI_snprintf(offset_str, NUM_STR_REP_LEN, "%.1f%%", RNA_float_get(op->ptr, "offset_pct"));
  }
  else {
    bUnit_AsString2(offset_str,
                    NUM_STR_REP_LEN,
                    (double)RNA_float_get(op->ptr, "offset"),
                    3,
                    B_UNIT_LENGTH,
                    &sce->unit,
                    true);
  }

  prop = RNA_struct_find_property(op->ptr, "offset_type");
  RNA_property_enum_name_gettexted(
      C, op->ptr, prop, RNA_property_enum_get(op->ptr, prop), &mode_str);
  prop = RNA_struct_find_property(op->ptr, "miter_outer");
  RNA_property_enum_name_gettexted(
      C, op->ptr, prop, RNA_property_enum_get(op->ptr, prop), &omiter_str);
  prop = RNA_struct_find_property(op->ptr, "miter_inner");
  RNA_property_enum_name_gettexted(
      C, op->ptr, prop, RNA_property_enum_get(op->ptr, prop), &imiter_str);

  BLI_snprintf(header,
               sizeof(header),
               TIP_("%s: confirm, "
                    "%s: cancel, "
                    "%s: mode (%s), "
                    "%s: width (%s), "
                    "%s: segments (%d), "
                    "%s: profile (%.3f), "
                    "%s: clamp overlap (%s), "
                    "%s: vertex only (%s), "
                    "%s: outer miter (%s), "
                    "%s: inner miter (%s), "
                    "%s: harden normals (%s), "
                    "%s: mark seam (%s), "
                    "%s: mark sharp (%s)"),
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
               WM_MODALKEY(BEV_MODAL_VERTEX_ONLY_TOGGLE),
               WM_bool_as_string(RNA_boolean_get(op->ptr, "vertex_only")),
               WM_MODALKEY(BEV_MODAL_OUTER_MITER_CHANGE),
               omiter_str,
               WM_MODALKEY(BEV_MODAL_INNER_MITER_CHANGE),
               imiter_str,
               WM_MODALKEY(BEV_MODAL_HARDEN_NORMALS_TOGGLE),
               WM_bool_as_string(RNA_boolean_get(op->ptr, "harden_normals")),
               WM_MODALKEY(BEV_MODAL_MARK_SEAM_TOGGLE),
               WM_bool_as_string(RNA_boolean_get(op->ptr, "mark_seam")),
               WM_MODALKEY(BEV_MODAL_MARK_SHARP_TOGGLE),
               WM_bool_as_string(RNA_boolean_get(op->ptr, "mark_sharp")));

#undef WM_MODALKEY

  ED_workspace_status_text(C, header);
}

static bool edbm_bevel_init(bContext *C, wmOperator *op, const bool is_modal)
{
  Scene *scene = CTX_data_scene(C);
  BevelData *opdata;
  ViewLayer *view_layer = CTX_data_view_layer(C);
  float pixels_per_inch;
  int i, otype;

  if (is_modal) {
    RNA_float_set(op->ptr, "offset", 0.0f);
    RNA_float_set(op->ptr, "offset_pct", 0.0f);
  }

  op->customdata = opdata = MEM_mallocN(sizeof(BevelData), "beveldata_mesh_operator");
  uint objects_used_len = 0;
  opdata->max_obj_scale = FLT_MIN;

  {
    uint ob_store_len = 0;
    Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
        view_layer, CTX_wm_view3d(C), &ob_store_len);
    opdata->ob_store = MEM_malloc_arrayN(ob_store_len, sizeof(*opdata->ob_store), __func__);
    for (uint ob_index = 0; ob_index < ob_store_len; ob_index++) {
      Object *obedit = objects[ob_index];
      float scale = mat4_to_scale(obedit->obmat);
      opdata->max_obj_scale = max_ff(opdata->max_obj_scale, scale);
      BMEditMesh *em = BKE_editmesh_from_object(obedit);
      if (em->bm->totvertsel > 0) {
        opdata->ob_store[objects_used_len].em = em;
        objects_used_len++;
      }
    }
    MEM_freeN(objects);
    opdata->ob_store_len = objects_used_len;
  }

  opdata->is_modal = is_modal;
  otype = RNA_enum_get(op->ptr, "offset_type");
  opdata->value_mode = (otype == BEVEL_AMT_PERCENT) ? OFFSET_VALUE_PERCENT : OFFSET_VALUE;
  opdata->segments = (float)RNA_int_get(op->ptr, "segments");
  pixels_per_inch = U.dpi * U.pixelsize;

  for (i = 0; i < NUM_VALUE_KINDS; i++) {
    opdata->shift_value[i] = -1.0f;
    opdata->initial_length[i] = -1.0f;
    /* note: scale for OFFSET_VALUE will get overwritten in edbm_bevel_invoke */
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
    View3D *v3d = CTX_wm_view3d(C);
    ARegion *ar = CTX_wm_region(C);

    for (uint ob_index = 0; ob_index < opdata->ob_store_len; ob_index++) {
      opdata->ob_store[ob_index].mesh_backup = EDBM_redo_state_store(
          opdata->ob_store[ob_index].em);
    }
    opdata->draw_handle_pixel = ED_region_draw_cb_activate(
        ar->type, ED_region_draw_mouse_line_cb, opdata->mcenter, REGION_DRAW_POST_PIXEL);
    G.moving = G_TRANSFORM_EDIT;

    if (v3d) {
      opdata->gizmo_flag = v3d->gizmo_flag;
      v3d->gizmo_flag = V3D_GIZMO_HIDE;
    }
  }

  return true;
}

static bool edbm_bevel_calc(wmOperator *op)
{
  BevelData *opdata = op->customdata;
  BMEditMesh *em;
  BMOperator bmop;
  bool changed = false;

  const float offset = get_bevel_offset(op);
  const int offset_type = RNA_enum_get(op->ptr, "offset_type");
  const int segments = RNA_int_get(op->ptr, "segments");
  const float profile = RNA_float_get(op->ptr, "profile");
  const bool vertex_only = RNA_boolean_get(op->ptr, "vertex_only");
  const bool clamp_overlap = RNA_boolean_get(op->ptr, "clamp_overlap");
  int material = RNA_int_get(op->ptr, "material");
  const bool loop_slide = RNA_boolean_get(op->ptr, "loop_slide");
  const bool mark_seam = RNA_boolean_get(op->ptr, "mark_seam");
  const bool mark_sharp = RNA_boolean_get(op->ptr, "mark_sharp");
  const bool harden_normals = RNA_boolean_get(op->ptr, "harden_normals");
  const int face_strength_mode = RNA_enum_get(op->ptr, "face_strength_mode");
  const int miter_outer = RNA_enum_get(op->ptr, "miter_outer");
  const int miter_inner = RNA_enum_get(op->ptr, "miter_inner");
  const float spread = RNA_float_get(op->ptr, "spread");

  for (uint ob_index = 0; ob_index < opdata->ob_store_len; ob_index++) {
    em = opdata->ob_store[ob_index].em;

    /* revert to original mesh */
    if (opdata->is_modal) {
      EDBM_redo_state_restore(opdata->ob_store[ob_index].mesh_backup, em, false);
    }

    if (em->ob) {
      material = CLAMPIS(material, -1, em->ob->totcol - 1);
    }

    Mesh *me = em->ob->data;

    if (harden_normals && !(me->flag & ME_AUTOSMOOTH)) {
      /* harden_normals only has a visible effect if autosmooth is on, so turn it on */
      me->flag |= ME_AUTOSMOOTH;
    }

    EDBM_op_init(em,
                 &bmop,
                 op,
                 "bevel geom=%hev offset=%f segments=%i vertex_only=%b offset_type=%i profile=%f "
                 "clamp_overlap=%b material=%i loop_slide=%b mark_seam=%b mark_sharp=%b "
                 "harden_normals=%b face_strength_mode=%i "
                 "miter_outer=%i miter_inner=%i spread=%f smoothresh=%f",
                 BM_ELEM_SELECT,
                 offset,
                 segments,
                 vertex_only,
                 offset_type,
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
                 me->smoothresh);

    BMO_op_exec(em->bm, &bmop);

    if (offset != 0.0f) {
      /* not essential, but we may have some loose geometry that
       * won't get bevel'd and better not leave it selected */
      EDBM_flag_disable_all(em, BM_ELEM_SELECT);
      BMO_slot_buffer_hflag_enable(
          em->bm, bmop.slots_out, "faces.out", BM_FACE, BM_ELEM_SELECT, true);
    }

    /* no need to de-select existing geometry */
    if (!EDBM_op_finish(em, &bmop, op, true)) {
      continue;
    }

    EDBM_mesh_normals_update(em);

    EDBM_update_generic(em, true, true);
    changed = true;
  }
  return changed;
}

static void edbm_bevel_exit(bContext *C, wmOperator *op)
{
  BevelData *opdata = op->customdata;

  ScrArea *sa = CTX_wm_area(C);

  if (sa) {
    ED_area_status_text(sa, NULL);
  }

  if (opdata->is_modal) {
    View3D *v3d = CTX_wm_view3d(C);
    ARegion *ar = CTX_wm_region(C);
    for (uint ob_index = 0; ob_index < opdata->ob_store_len; ob_index++) {
      EDBM_redo_state_free(&opdata->ob_store[ob_index].mesh_backup, NULL, false);
    }
    ED_region_draw_cb_exit(ar->type, opdata->draw_handle_pixel);
    if (v3d) {
      v3d->gizmo_flag = opdata->gizmo_flag;
    }
    G.moving = 0;
  }
  MEM_SAFE_FREE(opdata->ob_store);
  MEM_SAFE_FREE(op->customdata);
  op->customdata = NULL;
}

static void edbm_bevel_cancel(bContext *C, wmOperator *op)
{
  BevelData *opdata = op->customdata;
  if (opdata->is_modal) {
    for (uint ob_index = 0; ob_index < opdata->ob_store_len; ob_index++) {
      EDBM_redo_state_free(
          &opdata->ob_store[ob_index].mesh_backup, opdata->ob_store[ob_index].em, true);
      EDBM_update_generic(opdata->ob_store[ob_index].em, false, true);
    }
  }

  edbm_bevel_exit(C, op);

  /* need to force redisplay or we may still view the modified result */
  ED_region_tag_redraw(CTX_wm_region(C));
}

/* bevel! yay!!*/
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
  BevelData *opdata;
  float mlen[2], len, value, sc, st;
  int vmode;

  opdata = op->customdata;
  mlen[0] = opdata->mcenter[0] - event->mval[0];
  mlen[1] = opdata->mcenter[1] - event->mval[1];
  len = len_v2(mlen);
  vmode = opdata->value_mode;
  if (mode_changed || opdata->initial_length[vmode] == -1.0f) {
    /* If current value is not default start value, adjust len so that
     * the scaling and offset in edbm_bevel_mouse_set_value will
     * start at current value */
    value = (vmode == SEGMENTS_VALUE) ? opdata->segments :
                                        RNA_float_get(op->ptr, value_rna_name[vmode]);
    sc = opdata->scale[vmode];
    st = value_start[vmode];
    if (value != value_start[vmode]) {
      len = (st + sc * (len - MVAL_PIXEL_MARGIN) - value) / sc;
    }
  }
  opdata->initial_length[opdata->value_mode] = len;
}

static int edbm_bevel_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  BevelData *opdata;
  float center_3d[3];

  if (!edbm_bevel_init(C, op, true)) {
    return OPERATOR_CANCELLED;
  }

  opdata = op->customdata;

  /* initialize mouse values */
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

  edbm_bevel_update_header(C, op);

  if (!edbm_bevel_calc(op)) {
    edbm_bevel_cancel(C, op);
    ED_workspace_status_text(C, NULL);
    return OPERATOR_CANCELLED;
  }

  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static void edbm_bevel_mouse_set_value(wmOperator *op, const wmEvent *event)
{
  BevelData *opdata = op->customdata;
  int vmode = opdata->value_mode;
  float mdiff[2];
  float value;

  mdiff[0] = opdata->mcenter[0] - event->mval[0];
  mdiff[1] = opdata->mcenter[1] - event->mval[1];

  value = ((len_v2(mdiff) - MVAL_PIXEL_MARGIN) - opdata->initial_length[vmode]);

  /* Scale according to value mode */
  value = value_start[vmode] + value * opdata->scale[vmode];

  /* Fake shift-transform... */
  if (event->shift) {
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

  /* clamp accordingto value mode, and store value back */
  CLAMP(value, value_clamp_min[vmode], value_clamp_max[vmode]);
  if (vmode == SEGMENTS_VALUE) {
    opdata->segments = value;
    RNA_int_set(op->ptr, "segments", (int)(value + 0.5f));
  }
  else {
    RNA_float_set(op->ptr, value_rna_name[vmode], value);
  }
}

static void edbm_bevel_numinput_set_value(wmOperator *op)
{
  BevelData *opdata = op->customdata;
  float value;
  int vmode;

  vmode = opdata->value_mode;
  value = (vmode == SEGMENTS_VALUE) ? opdata->segments :
                                      RNA_float_get(op->ptr, value_rna_name[vmode]);
  applyNumInput(&opdata->num_input[vmode], &value);
  CLAMP(value, value_clamp_min[vmode], value_clamp_max[vmode]);
  if (vmode == SEGMENTS_VALUE) {
    opdata->segments = value;
    RNA_int_set(op->ptr, "segments", (int)value);
  }
  else {
    RNA_float_set(op->ptr, value_rna_name[vmode], value);
  }
}

/* Hide one of offset or offset_pct, depending on offset_type */
static bool edbm_bevel_poll_property(const bContext *UNUSED(C),
                                     wmOperator *op,
                                     const PropertyRNA *prop)
{
  const char *prop_id = RNA_property_identifier(prop);

  if (STRPREFIX(prop_id, "offset")) {
    int offset_type = RNA_enum_get(op->ptr, "offset_type");

    if (STREQ(prop_id, "offset") && offset_type == BEVEL_AMT_PERCENT) {
      return false;
    }
    else if (STREQ(prop_id, "offset_pct") && offset_type != BEVEL_AMT_PERCENT) {
      return false;
    }
  }

  return true;
}

wmKeyMap *bevel_modal_keymap(wmKeyConfig *keyconf)
{
  static const EnumPropertyItem modal_items[] = {
      {BEV_MODAL_CANCEL, "CANCEL", 0, "Cancel", "Cancel bevel"},
      {BEV_MODAL_CONFIRM, "CONFIRM", 0, "Confirm", "Confirm bevel"},
      {BEV_MODAL_VALUE_OFFSET, "VALUE_OFFSET", 0, "Value is offset", "Value changes offset"},
      {BEV_MODAL_VALUE_PROFILE, "VALUE_PROFILE", 0, "Value is profile", "Value changes profile"},
      {BEV_MODAL_VALUE_SEGMENTS,
       "VALUE_SEGMENTS",
       0,
       "Value is segments",
       "Value changes segments"},
      {BEV_MODAL_SEGMENTS_UP, "SEGMENTS_UP", 0, "Increase segments", "Increase segments"},
      {BEV_MODAL_SEGMENTS_DOWN, "SEGMENTS_DOWN", 0, "Decrease segments", "Decrease segments"},
      {BEV_MODAL_OFFSET_MODE_CHANGE,
       "OFFSET_MODE_CHANGE",
       0,
       "Change offset mode",
       "Cycle through offset modes"},
      {BEV_MODAL_CLAMP_OVERLAP_TOGGLE,
       "CLAMP_OVERLAP_TOGGLE",
       0,
       "Toggle clamp overlap",
       "Toggle clamp overlap flag"},
      {BEV_MODAL_VERTEX_ONLY_TOGGLE,
       "VERTEX_ONLY_TOGGLE",
       0,
       "Toggle vertex only",
       "Toggle vertex only flag"},
      {BEV_MODAL_HARDEN_NORMALS_TOGGLE,
       "HARDEN_NORMALS_TOGGLE",
       0,
       "Toggle harden normals",
       "Toggle harden normals flag"},
      {BEV_MODAL_MARK_SEAM_TOGGLE,
       "MARK_SEAM_TOGGLE",
       0,
       "Toggle mark seam",
       "Toggle mark seam flag"},
      {BEV_MODAL_MARK_SHARP_TOGGLE,
       "MARK_SHARP_TOGGLE",
       0,
       "Toggle mark sharp",
       "Toggle mark sharp flag"},
      {BEV_MODAL_OUTER_MITER_CHANGE,
       "OUTER_MITER_CHANGE",
       0,
       "Change outer miter",
       "Cycle through outer miter kinds"},
      {BEV_MODAL_INNER_MITER_CHANGE,
       "INNER_MITER_CHANGE",
       0,
       "Change inner miter",
       "Cycle through inner miter kinds"},
      {0, NULL, 0, NULL, NULL},
  };

  wmKeyMap *keymap = WM_modalkeymap_get(keyconf, "Bevel Modal Map");

  /* this function is called for each spacetype, only needs to add map once */
  if (keymap && keymap->modal_items) {
    return NULL;
  }

  keymap = WM_modalkeymap_add(keyconf, "Bevel Modal Map", modal_items);

  WM_modalkeymap_assign(keymap, "MESH_OT_bevel");

  return keymap;
}

static int edbm_bevel_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  BevelData *opdata = op->customdata;
  const bool has_numinput = hasNumInput(&opdata->num_input[opdata->value_mode]);
  bool handled = false;
  short etype = event->type;
  short eval = event->val;

  /* When activated from toolbar, need to convert leftmouse release to confirm */
  if (etype == LEFTMOUSE && eval == KM_RELEASE && RNA_boolean_get(op->ptr, "release_confirm")) {
    etype = EVT_MODAL_MAP;
    eval = BEV_MODAL_CONFIRM;
  }
  /* Modal numinput active, try to handle numeric inputs first... */
  if (etype != EVT_MODAL_MAP && eval == KM_PRESS && has_numinput &&
      handleNumInput(C, &opdata->num_input[opdata->value_mode], event)) {
    edbm_bevel_numinput_set_value(op);
    edbm_bevel_calc(op);
    edbm_bevel_update_header(C, op);
    return OPERATOR_RUNNING_MODAL;
  }
  else if (etype == MOUSEMOVE) {
    if (!has_numinput) {
      edbm_bevel_mouse_set_value(op, event);
      edbm_bevel_calc(op);
      edbm_bevel_update_header(C, op);
      handled = true;
    }
  }
  else if (etype == MOUSEPAN) {
    float delta = 0.02f * (event->y - event->prevy);
    if (opdata->segments >= 1 && opdata->segments + delta < 1) {
      opdata->segments = 1;
    }
    else {
      opdata->segments += delta;
    }
    RNA_int_set(op->ptr, "segments", (int)opdata->segments);
    edbm_bevel_calc(op);
    edbm_bevel_update_header(C, op);
    handled = true;
  }
  else if (etype == EVT_MODAL_MAP) {
    switch (eval) {
      case BEV_MODAL_CANCEL:
        edbm_bevel_cancel(C, op);
        ED_workspace_status_text(C, NULL);
        return OPERATOR_CANCELLED;

      case BEV_MODAL_CONFIRM:
        edbm_bevel_calc(op);
        edbm_bevel_exit(C, op);
        ED_workspace_status_text(C, NULL);
        return OPERATOR_FINISHED;

      case BEV_MODAL_SEGMENTS_UP:
        opdata->segments = opdata->segments + 1;
        RNA_int_set(op->ptr, "segments", (int)opdata->segments);
        edbm_bevel_calc(op);
        edbm_bevel_update_header(C, op);
        handled = true;
        break;

      case BEV_MODAL_SEGMENTS_DOWN:
        opdata->segments = max_ff(opdata->segments - 1, 1);
        RNA_int_set(op->ptr, "segments", (int)opdata->segments);
        edbm_bevel_calc(op);
        edbm_bevel_update_header(C, op);
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
        if (!has_numinput &&
            (opdata->value_mode == OFFSET_VALUE || opdata->value_mode == OFFSET_VALUE_PERCENT)) {
          edbm_bevel_mouse_set_value(op, event);
        }
        edbm_bevel_calc(op);
        edbm_bevel_update_header(C, op);
        handled = true;
        break;

      case BEV_MODAL_CLAMP_OVERLAP_TOGGLE: {
        bool clamp_overlap = RNA_boolean_get(op->ptr, "clamp_overlap");
        RNA_boolean_set(op->ptr, "clamp_overlap", !clamp_overlap);
        edbm_bevel_calc(op);
        edbm_bevel_update_header(C, op);
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

      case BEV_MODAL_VERTEX_ONLY_TOGGLE: {
        bool vertex_only = RNA_boolean_get(op->ptr, "vertex_only");
        RNA_boolean_set(op->ptr, "vertex_only", !vertex_only);
        edbm_bevel_calc(op);
        edbm_bevel_update_header(C, op);
        handled = true;
        break;
      }

      case BEV_MODAL_MARK_SEAM_TOGGLE: {
        bool mark_seam = RNA_boolean_get(op->ptr, "mark_seam");
        RNA_boolean_set(op->ptr, "mark_seam", !mark_seam);
        edbm_bevel_calc(op);
        edbm_bevel_update_header(C, op);
        handled = true;
        break;
      }

      case BEV_MODAL_MARK_SHARP_TOGGLE: {
        bool mark_sharp = RNA_boolean_get(op->ptr, "mark_sharp");
        RNA_boolean_set(op->ptr, "mark_sharp", !mark_sharp);
        edbm_bevel_calc(op);
        edbm_bevel_update_header(C, op);
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
        edbm_bevel_update_header(C, op);
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
        edbm_bevel_update_header(C, op);
        handled = true;
        break;
      }

      case BEV_MODAL_HARDEN_NORMALS_TOGGLE: {
        bool harden_normals = RNA_boolean_get(op->ptr, "harden_normals");
        RNA_boolean_set(op->ptr, "harden_normals", !harden_normals);
        edbm_bevel_calc(op);
        edbm_bevel_update_header(C, op);
        handled = true;
        break;
      }
    }
  }

  /* Modal numinput inactive, try to handle numeric inputs last... */
  if (!handled && eval == KM_PRESS &&
      handleNumInput(C, &opdata->num_input[opdata->value_mode], event)) {
    edbm_bevel_numinput_set_value(op);
    edbm_bevel_calc(op);
    edbm_bevel_update_header(C, op);
    return OPERATOR_RUNNING_MODAL;
  }

  return OPERATOR_RUNNING_MODAL;
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
      {0, NULL, 0, NULL, NULL},
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
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem miter_outer_items[] = {
      {BEVEL_MITER_SHARP, "SHARP", 0, "Sharp", "Outside of miter is sharp"},
      {BEVEL_MITER_PATCH, "PATCH", 0, "Patch", "Outside of miter is squared-off patch"},
      {BEVEL_MITER_ARC, "ARC", 0, "Arc", "Outside of miter is arc"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem miter_inner_items[] = {
      {BEVEL_MITER_SHARP, "SHARP", 0, "Sharp", "Inside of miter is sharp"},
      {BEVEL_MITER_ARC, "ARC", 0, "Arc", "Inside of miter is arc"},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Bevel";
  ot->description =
      "Cut into selected items at an angle to create flat or rounded bevel or chamfer";
  ot->idname = "MESH_OT_bevel";

  /* api callbacks */
  ot->exec = edbm_bevel_exec;
  ot->invoke = edbm_bevel_invoke;
  ot->modal = edbm_bevel_modal;
  ot->cancel = edbm_bevel_cancel;
  ot->poll = ED_operator_editmesh;
  ot->poll_property = edbm_bevel_poll_property;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_GRAB_CURSOR_XY | OPTYPE_BLOCKING;

  RNA_def_enum(
      ot->srna, "offset_type", offset_type_items, 0, "Width Type", "What distance Width measures");
  prop = RNA_def_property(ot->srna, "offset", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_range(prop, 0.0, 1e6);
  RNA_def_property_ui_range(prop, 0.0f, 100.0, 1, 3);
  RNA_def_property_ui_text(prop, "Width", "Bevel amount");
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
  RNA_def_boolean(ot->srna, "vertex_only", false, "Vertex Only", "Bevel only vertices");
  RNA_def_boolean(ot->srna,
                  "clamp_overlap",
                  false,
                  "Clamp Overlap",
                  "Do not allow beveled edges/vertices to overlap each other");
  RNA_def_boolean(
      ot->srna, "loop_slide", true, "Loop Slide", "Prefer slide along edge to even widths");
  RNA_def_boolean(ot->srna, "mark_seam", false, "Mark Seams", "Mark Seams along beveled edges");
  RNA_def_boolean(ot->srna, "mark_sharp", false, "Mark Sharp", "Mark beveled edges as sharp");
  RNA_def_int(ot->srna,
              "material",
              -1,
              -1,
              INT_MAX,
              "Material",
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
  prop = RNA_def_boolean(ot->srna, "release_confirm", 0, "Confirm on Release", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}
