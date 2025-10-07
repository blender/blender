/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include <algorithm>

#include "DNA_brush_types.h"

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_rand.h"
#include "BLI_string_utf8.h"
#include "BLI_time.h"

#include "BLT_translation.hh"

#include "RNA_access.hh"

#include "BKE_brush.hh"
#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_mask.h"
#include "BKE_modifier.hh"
#include "BKE_paint.hh"
#include "BKE_screen.hh"

#include "SEQ_transform.hh"

#include "ED_clip.hh"
#include "ED_image.hh"
#include "ED_object.hh"
#include "ED_screen.hh"
#include "ED_space_api.hh"
#include "ED_uvedit.hh"

#include "WM_api.hh"

#include "UI_view2d.hh"

#include "SEQ_sequencer.hh"

#include "transform.hh"
#include "transform_convert.hh"
#include "transform_gizmo.hh"
#include "transform_orientations.hh"
#include "transform_snap.hh"

namespace blender::ed::transform {

/* ************************** GENERICS **************************** */

void resetTransModal(TransInfo *t)
{
  freeTransCustomDataForMode(t);
}

void resetTransRestrictions(TransInfo *t)
{
  t->flag &= ~T_ALL_RESTRICTIONS;
}

static void *t_view_get(TransInfo *t)
{
  if (t->spacetype == SPACE_VIEW3D) {
    View3D *v3d = static_cast<View3D *>(t->area->spacedata.first);
    return (void *)v3d;
  }
  if (t->region) {
    return (void *)&t->region->v2d;
  }
  return nullptr;
}

static int t_around_get(TransInfo *t)
{
  if (t->flag & T_OVERRIDE_CENTER) {
    /* Avoid initialization of individual origins (#V3D_AROUND_LOCAL_ORIGINS). */
    return V3D_AROUND_CENTER_BOUNDS;
  }

  ScrArea *area = t->area;
  switch (t->spacetype) {
    case SPACE_VIEW3D: {
      if (t->mode == TFM_BEND) {
        /* Bend always uses the cursor. */
        return V3D_AROUND_CURSOR;
      }
      return t->settings->transform_pivot_point;
    }
    case SPACE_IMAGE: {
      SpaceImage *sima = static_cast<SpaceImage *>(area->spacedata.first);
      return sima->around;
    }
    case SPACE_GRAPH: {
      SpaceGraph *sipo = static_cast<SpaceGraph *>(area->spacedata.first);
      return sipo->around;
    }
    case SPACE_CLIP: {
      SpaceClip *sclip = static_cast<SpaceClip *>(area->spacedata.first);
      return sclip->around;
    }
    case SPACE_SEQ: {
      if (t->region->regiontype == RGN_TYPE_PREVIEW) {
        return seq::tool_settings_pivot_point_get(t->scene);
      }
      break;
    }
    default:
      break;
  }

  return V3D_AROUND_CENTER_BOUNDS;
}

void initTransInfo(bContext *C, TransInfo *t, wmOperator *op, const wmEvent *event)
{
  Scene *sce = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(sce, view_layer);
  Object *obact = BKE_view_layer_active_object_get(view_layer);
  const eObjectMode object_mode = eObjectMode(obact ? obact->mode : OB_MODE_OBJECT);
  ToolSettings *ts = CTX_data_tool_settings(C);
  ARegion *region = CTX_wm_region(C);
  ScrArea *area = CTX_wm_area(C);
  const bool is_sequencer = CTX_wm_space_seq(C) != nullptr;
  if (!is_sequencer) {
    t->scene = sce;
    t->view_layer = view_layer;
  }
  else {
    t->scene = CTX_data_sequencer_scene(C);
    t->view_layer = t->scene ? BKE_view_layer_default_render(t->scene) : nullptr;
  }

  PropertyRNA *prop;

  t->mbus = CTX_wm_message_bus(C);
  t->depsgraph = CTX_data_depsgraph_pointer(C);

  t->area = area;
  t->region = region;
  t->settings = ts;
  t->reports = op ? op->reports : nullptr;

  t->helpline = HLP_NONE;

  t->flag = eTFlag(0);

  if (obact && !(t->options & (CTX_CURSOR | CTX_TEXTURE_SPACE)) &&
      ELEM(object_mode, OB_MODE_EDIT, OB_MODE_EDIT_GPENCIL_LEGACY))
  {
    t->obedit_type = obact->type;
  }
  else {
    t->obedit_type = -1;
  }

  if (t->options & CTX_CURSOR) {
    /* Cursor should always use the drag start as the combination of click-drag to place & move
     * doesn't work well if the click location isn't used when transforming. */
    t->flag |= T_EVENT_DRAG_START;
  }

  /* Many kinds of transform only use a single handle. */
  if (t->data_container == nullptr) {
    t->data_container = static_cast<TransDataContainer *>(
        MEM_callocN(sizeof(*t->data_container), __func__));
    t->data_container_len = 1;
  }

  t->redraw = TREDRAW_HARD; /* Redraw first time. */

  float2 mval;
  if (event) {
    if (t->flag & T_EVENT_DRAG_START) {
      WM_event_drag_start_mval_fl(event, region, mval);
    }
    else {
      mval = float2(event->mval);
    }
  }
  else {
    mval = float2(0, 0);
  }

  t->mval = mval;

  /* Initialize this mouse variable in advance as it is required by
   * `transform_convert_frame_side_dir_get` which is called before `initMouseInput`. */
  t->mouse.imval = mval;

  t->mode_info = nullptr;

  t->data_len_all = 0;

  zero_v3(t->center_global);

  unit_m3(t->mat);

  /* Default to rotate on the Z axis. */
  t->orient_axis = 2;
  t->orient_axis_ortho = 1;

  /* If there's an event, we're modal. */
  if (event) {
    t->flag |= T_MODAL;
  }

  /* Crease needs edge flag. */
  if (ELEM(t->mode, TFM_EDGE_CREASE, TFM_BWEIGHT)) {
    t->options |= CTX_EDGE_DATA;
  }

  t->remove_on_cancel = false;

  if (op && (prop = RNA_struct_find_property(op->ptr, "remove_on_cancel")) &&
      RNA_property_is_set(op->ptr, prop))
  {
    if (RNA_property_boolean_get(op->ptr, prop)) {
      t->remove_on_cancel = true;
    }
  }

  /* Grease Pencil editing context. */
  if (t->obedit_type == OB_GREASE_PENCIL && object_mode == OB_MODE_EDIT &&
      ((area == nullptr) || (area->spacetype == SPACE_VIEW3D)))
  {
    t->options |= CTX_GPENCIL_STROKES;
  }

  /* Assign the space type, some exceptions for running in different mode. */
  if (area == nullptr) {
    /* Background mode. */
    t->spacetype = SPACE_EMPTY;
  }
  else if (((region == nullptr) || (region->regiondata == nullptr)) &&
           (area->spacetype == SPACE_VIEW3D))
  {
    /* Running the operator through the text editor where e.g. `area.type` was
     * set to 'VIEW_3D' but the viewport was not updated. */
    t->spacetype = SPACE_EMPTY;
  }
  else {
    /* Normal operation. */
    t->spacetype = area->spacetype;
  }

  /* Handle #T_ALT_TRANSFORM initialization, we may use for different operators. */
  if (op) {
    const char *prop_id = nullptr;
    if (t->mode == TFM_SHRINKFATTEN) {
      prop_id = "use_even_offset";
    }

    if (prop_id && (prop = RNA_struct_find_property(op->ptr, prop_id))) {
      SET_FLAG_FROM_TEST(t->flag, RNA_property_boolean_get(op->ptr, prop), T_ALT_TRANSFORM);
    }
  }

  if (t->spacetype == SPACE_VIEW3D) {
    bScreen *animscreen = ED_screen_animation_playing(CTX_wm_manager(C));

    t->animtimer = (animscreen) ? animscreen->animtimer : nullptr;

    if (t->scene->toolsettings->transform_flag & SCE_XFORM_AXIS_ALIGN) {
      t->flag |= T_V3D_ALIGN;
    }

    if ((object_mode & OB_MODE_ALL_PAINT) || (object_mode & OB_MODE_SCULPT_CURVES)) {
      Paint *paint = BKE_paint_get_active_from_context(C);
      Brush *brush = (paint) ? BKE_paint_brush(paint) : nullptr;
      if (brush && (brush->flag & BRUSH_CURVE)) {
        t->options |= CTX_PAINT_CURVE;
      }
    }

    /* Initialize UV transform from. */
    if (op && (prop = RNA_struct_find_property(op->ptr, "correct_uv"))) {
      if (RNA_property_is_set(op->ptr, prop)) {
        if (RNA_property_boolean_get(op->ptr, prop)) {
          t->settings->uvcalc_flag |= UVCALC_TRANSFORM_CORRECT_SLIDE;
        }
        else {
          t->settings->uvcalc_flag &= ~UVCALC_TRANSFORM_CORRECT_SLIDE;
        }
      }
      else {
        RNA_property_boolean_set(
            op->ptr, prop, (t->settings->uvcalc_flag & UVCALC_TRANSFORM_CORRECT_SLIDE) != 0);
      }
    }
  }
  else if (t->spacetype == SPACE_IMAGE) {
    SpaceImage *sima = static_cast<SpaceImage *>(area->spacedata.first);
    BKE_view_layer_synced_ensure(t->scene, t->view_layer);
    if (ED_space_image_show_uvedit(sima, BKE_view_layer_active_object_get(t->view_layer))) {
      /* UV transform. */
    }
    else if (sima->mode == SI_MODE_MASK) {
      t->options |= CTX_MASK;
    }
    else if (sima->mode == SI_MODE_PAINT) {
      Paint *paint = &sce->toolsettings->imapaint.paint;
      Brush *brush = (paint) ? BKE_paint_brush(paint) : nullptr;
      if (brush && (brush->flag & BRUSH_CURVE)) {
        t->options |= CTX_PAINT_CURVE;
      }
    }
    /* Image not in UV edit, nor in mask mode, can happen for some tools. */
  }
  else if (t->spacetype == SPACE_CLIP) {
    SpaceClip *sclip = static_cast<SpaceClip *>(area->spacedata.first);
    if (ED_space_clip_check_show_trackedit(sclip)) {
      t->options |= CTX_MOVIECLIP;
    }
    else if (ED_space_clip_check_show_maskedit(sclip)) {
      t->options |= CTX_MASK;
    }
  }
  else if (t->spacetype == SPACE_SEQ && region->regiontype == RGN_TYPE_PREVIEW) {
    t->options |= CTX_SEQUENCER_IMAGE;

    /* Needed for auto-keying transforms in preview during playback. */
    bScreen *animscreen = ED_screen_animation_playing(CTX_wm_manager(C));
    t->animtimer = (animscreen) ? animscreen->animtimer : nullptr;
  }

  setTransformViewAspect(t, t->aspect);

  if (op && (prop = RNA_struct_find_property(op->ptr, "center_override")) &&
      RNA_property_is_set(op->ptr, prop))
  {
    RNA_property_float_get_array(op->ptr, prop, t->center_global);
    mul_v3_v3(t->center_global, t->aspect);
    t->flag |= T_OVERRIDE_CENTER;
  }

  t->view = t_view_get(t);
  t->around = t_around_get(t);

  /* Exceptional case. */
  if (t->around == V3D_AROUND_LOCAL_ORIGINS) {
    if (ELEM(t->mode, TFM_ROTATION, TFM_RESIZE, TFM_TRACKBALL)) {
      const bool use_island = transdata_check_local_islands(t, t->around);

      if ((t->obedit_type != -1) && !use_island) {
        t->options |= CTX_NO_PET;
      }
    }
  }

  bool t_values_set_is_array = false;

  if (op && (prop = RNA_struct_find_property(op->ptr, "value")) &&
      RNA_property_is_set(op->ptr, prop))
  {
    float values[4] = {0}; /* In case value isn't length 4, avoid uninitialized memory. */
    if (RNA_property_array_check(prop)) {
      RNA_property_float_get_array(op->ptr, prop, values);
      t_values_set_is_array = true;
    }
    else {
      values[0] = RNA_property_float_get(op->ptr, prop);
    }

    if (t->flag & T_MODAL) {
      /* Run before init functions so 'values_modal_offset' can be applied on mouse input. */
      copy_v4_v4(t->values_modal_offset, values);
    }
    else {
      copy_v4_v4(t->values, values);
      t->flag |= T_INPUT_IS_VALUES_FINAL;
    }
  }

  if (op && (prop = RNA_struct_find_property(op->ptr, "constraint_axis"))) {
    bool constraint_axis[3] = {false, false, false};
    if (t_values_set_is_array && t->flag & T_INPUT_IS_VALUES_FINAL) {
      /* For operators whose `t->values` is array (as Move and Scale), set constraint so that the
       * orientation is more intuitive in the Redo Panel. */
      constraint_axis[0] = constraint_axis[1] = constraint_axis[2] = true;
    }
    else if (RNA_property_is_set(op->ptr, prop)) {
      RNA_property_boolean_get_array(op->ptr, prop, constraint_axis);
    }

    if (constraint_axis[0] || constraint_axis[1] || constraint_axis[2]) {
      t->con.mode |= CON_APPLY;

      if (constraint_axis[0]) {
        t->con.mode |= CON_AXIS0;
      }
      if (constraint_axis[1]) {
        t->con.mode |= CON_AXIS1;
      }
      if (constraint_axis[2]) {
        t->con.mode |= CON_AXIS2;
      }
    }
  }

  {
    eTOType orient_types[3];
    eTOType orient_type_apply = O_DEFAULT;
    float custom_matrix[3][3];

    int orient_type_scene = V3D_ORIENT_GLOBAL;
    int orient_type_default = -1;
    int orient_type_set = -1;
    int orient_type_matrix_set = -1;

    if ((t->spacetype == SPACE_VIEW3D) && (t->region->regiontype == RGN_TYPE_WINDOW)) {
      TransformOrientationSlot *orient_slot = &t->scene->orientation_slots[SCE_ORIENT_DEFAULT];
      orient_type_scene = orient_slot->type;
      if (orient_type_scene == V3D_ORIENT_CUSTOM) {
        const int index_custom = orient_slot->index_custom;
        orient_type_scene += index_custom;
      }
    }

    if (op && ((prop = RNA_struct_find_property(op->ptr, "orient_type")) &&
               RNA_property_is_set(op->ptr, prop)))
    {
      orient_type_set = RNA_property_enum_get(op->ptr, prop);
      if (orient_type_set >= V3D_ORIENT_CUSTOM + BIF_countTransformOrientation(C)) {
        orient_type_set = V3D_ORIENT_GLOBAL;
      }
    }

    if (op && (prop = RNA_struct_find_property(op->ptr, "orient_axis"))) {
      t->orient_axis = RNA_property_enum_get(op->ptr, prop);
    }

    if (op && (prop = RNA_struct_find_property(op->ptr, "orient_axis_ortho"))) {
      t->orient_axis_ortho = RNA_property_enum_get(op->ptr, prop);
    }

    /* The properties "orient_matrix" and "orient_matrix_type" are used to store the orientation
     * calculated in the first operator call. This allows for reuse of the orientation during
     * subsequent calls of the same operator. When making adjustments through the Redo panel
     * (#OP_IS_REPEAT), reusing the orientation prevents unpredictable changes that can occur when
     * using #V3D_ORIENT_VIEW. However, when activated by #SCREEN_OT_repeat_last
     * (#OP_IS_REPEAT_LAST), it's best to avoid reusing the orientation to prevent unintended
     * changes. */
    if (op && !(op->flag & OP_IS_REPEAT_LAST) &&
        ((prop = RNA_struct_find_property(op->ptr, "orient_matrix")) &&
         RNA_property_is_set(op->ptr, prop)))
    {
      RNA_property_float_get_array(op->ptr, prop, &custom_matrix[0][0]);

      if ((prop = RNA_struct_find_property(op->ptr, "orient_matrix_type")) &&
          RNA_property_is_set(op->ptr, prop))
      {
        orient_type_matrix_set = RNA_property_enum_get(op->ptr, prop);
      }
      else if (orient_type_set == -1) {
        orient_type_set = V3D_ORIENT_CUSTOM_MATRIX;
      }
    }

    orient_type_default = orient_type_scene;

    if (orient_type_set != -1) {
      if (!(t->con.mode & CON_APPLY)) {
        /* Only overwrite default if not constrained. */
        orient_type_default = orient_type_set;
        t->is_orient_default_overwrite = true;
      }
    }
    else if (orient_type_matrix_set != -1) {
      orient_type_set = orient_type_matrix_set;
      if (!(t->con.mode & CON_APPLY)) {
        /* Only overwrite default if not constrained. */
        orient_type_default = orient_type_set;
        t->is_orient_default_overwrite = true;
      }
    }

    if (orient_type_set == -1) {
      if (orient_type_scene == V3D_ORIENT_GLOBAL) {
        orient_type_set = V3D_ORIENT_LOCAL;
      }
      else {
        orient_type_set = V3D_ORIENT_GLOBAL;
      }

      if (t->con.mode & CON_APPLY) {
        orient_type_apply = O_SCENE;
      }
    }
    else {
      orient_type_apply = O_SET;
    }

    BLI_assert(!ELEM(-1, orient_type_default, orient_type_set));
    if (orient_type_matrix_set == orient_type_set) {
      /* Constraints are forced to use the custom matrix when redoing. */
      orient_type_set = V3D_ORIENT_CUSTOM_MATRIX;
    }

    orient_types[O_DEFAULT] = eTOType(orient_type_default);
    orient_types[O_SCENE] = eTOType(orient_type_scene);
    orient_types[O_SET] = eTOType(orient_type_set);

    for (int i = 0; i < 3; i++) {
      /* For efficiency, avoid calculating the same orientation twice. */
      int j;
      for (j = 0; j < i; j++) {
        if (orient_types[j] == orient_types[i]) {
          memcpy(&t->orient[i], &t->orient[j], sizeof(*t->orient));
          break;
        }
      }
      if (j == i) {
        t->orient[i].type = transform_orientation_matrix_get(
            C, t, orient_types[i], custom_matrix, t->orient[i].matrix);
      }
    }

    t->orient_type_mask = 0;
    for (int i = 0; i < 3; i++) {
      const int type = t->orient[i].type;
      if (type < V3D_ORIENT_CUSTOM_MATRIX) {
        BLI_assert(type < 32);
        t->orient_type_mask |= (1 << type);
      }
    }

    transform_orientations_current_set(t, orient_type_apply);
  }

  if (op && ((prop = RNA_struct_find_property(op->ptr, "release_confirm")) &&
             RNA_property_is_set(op->ptr, prop)))
  {
    if (RNA_property_boolean_get(op->ptr, prop)) {
      t->flag |= T_RELEASE_CONFIRM;
    }
  }
  else {
    /* Release confirms preference should not affect node editor (#69288, #70504). */
    if (ISMOUSE_BUTTON(t->launch_event) &&
        ((U.flag & USER_RELEASECONFIRM) || (t->spacetype == SPACE_NODE)))
    {
      /* Global "release confirm" on mouse bindings. */
      t->flag |= T_RELEASE_CONFIRM;
    }
  }

  if (op &&
      ((prop = RNA_struct_find_property(op->ptr, "mirror")) && RNA_property_is_set(op->ptr, prop)))
  {
    if (!RNA_property_boolean_get(op->ptr, prop)) {
      t->flag |= T_NO_MIRROR;
    }
  }
  else if ((t->spacetype == SPACE_VIEW3D) && (t->obedit_type == OB_MESH)) {
    /* Pass. */
  }
  else {
    /* Avoid mirroring for unsupported contexts. */
    t->flag |= T_NO_MIRROR;
  }

  /* Setting proportional editing flag only if property exist in operator. Otherwise, assume it's
   * not supported. */
  if (op && (prop = RNA_struct_find_property(op->ptr, "use_proportional_edit"))) {
    if (RNA_property_is_set(op->ptr, prop)) {
      if (RNA_property_boolean_get(op->ptr, prop)) {
        t->flag |= T_PROP_EDIT;
        if (RNA_boolean_get(op->ptr, "use_proportional_connected")) {
          t->flag |= T_PROP_CONNECTED;
        }
        if (RNA_boolean_get(op->ptr, "use_proportional_projected")) {
          t->flag |= T_PROP_PROJECTED;
        }
      }
    }
    else {
      /* Use settings from scene only if modal. */
      if (t->flag & T_MODAL) {
        if ((t->options & CTX_NO_PET) == 0) {
          bool use_prop_edit = false;
          if (t->spacetype == SPACE_GRAPH) {
            use_prop_edit = ts->proportional_fcurve;
          }
          else if (t->spacetype == SPACE_ACTION) {
            use_prop_edit = ts->proportional_action;
          }
          else if (t->options & CTX_MASK) {
            use_prop_edit = ts->proportional_mask;
          }
          else if (object_mode == OB_MODE_OBJECT) {
            /* No active object means #TransConvertType_Object [see #convert_type_get()], so use
             * toolsetting for *object*. */
            use_prop_edit = ts->proportional_objects;
          }
          else {
            use_prop_edit = (ts->proportional_edit & PROP_EDIT_USE) != 0;
          }

          if (use_prop_edit) {
            t->flag |= T_PROP_EDIT;
            if (ts->proportional_edit & PROP_EDIT_CONNECTED) {
              t->flag |= T_PROP_CONNECTED;
            }
            if (ts->proportional_edit & PROP_EDIT_PROJECTED) {
              t->flag |= T_PROP_PROJECTED;
            }
          }
        }
      }
    }

    if (op && ((prop = RNA_struct_find_property(op->ptr, "proportional_size")) &&
               RNA_property_is_set(op->ptr, prop)))
    {
      t->prop_size = RNA_property_float_get(op->ptr, prop);
    }
    else {
      t->prop_size = ts->proportional_size;
    }

    /* TRANSFORM_FIX_ME rna restrictions. */
    if (t->prop_size <= 0.00001f) {
      printf("Proportional size (%f) under 0.00001, resetting to 1!\n", t->prop_size);
      t->prop_size = 1.0f;
    }

    if (op && ((prop = RNA_struct_find_property(op->ptr, "proportional_edit_falloff")) &&
               RNA_property_is_set(op->ptr, prop)))
    {
      t->prop_mode = RNA_property_enum_get(op->ptr, prop);
    }
    else {
      t->prop_mode = ts->prop_mode;
    }
  }
  else { /* Add not pet option to context when not available. */
    t->options |= CTX_NO_PET;
  }

  if (op && (prop = RNA_struct_find_property(op->ptr, "use_automerge_and_split")) &&
      RNA_property_is_set(op->ptr, prop))
  {
    if (RNA_property_boolean_get(op->ptr, prop)) {
      t->flag |= T_AUTOMERGE | T_AUTOSPLIT;
    }
  }
  else if (t->obedit_type == OB_MESH) {
    char automerge = t->scene->toolsettings->automerge;
    if (automerge & AUTO_MERGE) {
      t->flag |= T_AUTOMERGE;
      if (automerge & AUTO_MERGE_AND_SPLIT) {
        t->flag |= T_AUTOSPLIT;
      }
    }
  }

  if (op && (prop = RNA_struct_find_property(op->ptr, "use_duplicated_keyframes")) &&
      RNA_property_is_set(op->ptr, prop))
  {
    if (RNA_property_boolean_get(op->ptr, prop)) {
      t->flag |= T_DUPLICATED_KEYFRAMES;
    }
  }

/* Mirror is not supported with proportional editing, turn it off. */
#if 0
  if (t->flag & T_PROP_EDIT) {
    t->flag &= ~T_MIRROR;
  }
#endif

  /* Disable cursor wrap when edge panning is enabled. */
  if (t->options & CTX_VIEW2D_EDGE_PAN) {
    t->flag |= T_NO_CURSOR_WRAP;
  }

  if (op && (t->flag & T_MODAL) &&
      ELEM(t->mode,
           TFM_TRANSLATION,
           TFM_RESIZE,
           TFM_ROTATION,
           TFM_SHRINKFATTEN,
           TFM_EDGE_SLIDE,
           TFM_VERT_SLIDE))
  {
    wmWindowManager *wm = CTX_wm_manager(C);
    wmKeyMap *keymap = WM_keymap_active(wm, op->type->modalkeymap);
    const wmKeyMapItem *kmi_passthrough = nullptr;
    LISTBASE_FOREACH (const wmKeyMapItem *, kmi, &keymap->items) {
      if (kmi->flag & KMI_INACTIVE) {
        continue;
      }

      if (kmi->propvalue == TFM_MODAL_PASSTHROUGH_NAVIGATE) {
        kmi_passthrough = kmi;
        break;
      }
    }
    t->vod = ED_view3d_navigation_init(C, kmi_passthrough);
  }

  if (t->mode == TFM_TRANSLATION) {
    if ((prop = RNA_struct_find_property(op->ptr, "translate_origin")) &&
        RNA_property_is_set(op->ptr, prop))
    {
      if (RNA_property_boolean_get(op->ptr, prop)) {
        t->flag |= T_ORIGIN;
      }
    }
  }

  setTransformViewMatrices(t);
  calculateCenter2D(t);
  calculateCenterLocal(t, t->center_global);
  initNumInput(&t->num);

  transform_gizmo_3d_model_from_constraint_and_mode_init(t);
}

static void freeTransCustomData(TransInfo *t, TransDataContainer *tc, TransCustomData *custom_data)
{
  if (custom_data->free_cb) {
    /* Can take over freeing t->data and data_2d etc... */
    custom_data->free_cb(t, tc, custom_data);
    BLI_assert(custom_data->data == nullptr);
  }
  else if ((custom_data->data != nullptr) && custom_data->use_free) {
    MEM_freeN(custom_data->data);
    custom_data->data = nullptr;
  }
  /* In case modes are switched in the same transform session. */
  custom_data->free_cb = nullptr;
  custom_data->use_free = false;
}

static void freeTransCustomDataContainer(TransInfo *t,
                                         TransDataContainer *tc,
                                         TransCustomDataContainer *tcdc)
{
  TransCustomData *custom_data = &tcdc->first_elem;
  for (int i = 0; i < TRANS_CUSTOM_DATA_ELEM_MAX; i++, custom_data++) {
    freeTransCustomData(t, tc, custom_data);
  }
}

void freeTransCustomDataForMode(TransInfo *t)
{
  freeTransCustomData(t, nullptr, &t->custom.mode);
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    freeTransCustomData(t, tc, &tc->custom.mode);
  }
}

void postTrans(bContext *C, TransInfo *t)
{
  if (t->draw_handle_view) {
    ED_region_draw_cb_exit(t->region->runtime->type, t->draw_handle_view);
  }
  if (t->draw_handle_pixel) {
    ED_region_draw_cb_exit(t->region->runtime->type, t->draw_handle_pixel);
  }
  if (t->draw_handle_cursor) {
    WM_paint_cursor_end(static_cast<wmPaintCursor *>(t->draw_handle_cursor));
  }

  if (t->flag & T_MODAL_CURSOR_SET) {
    WM_cursor_modal_restore(CTX_wm_window(C));
  }

  /* Free all custom-data. */
  freeTransCustomDataContainer(t, nullptr, &t->custom);
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    freeTransCustomDataContainer(t, tc, &tc->custom);
  }

  /* #postTrans can be called when nothing is selected, so data is nullptr already. */
  if (t->data_len_all != 0) {
    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      /* Free data malloced per trans-data. */
      if (ELEM(t->obedit_type, OB_CURVES_LEGACY, OB_SURF) || (t->spacetype == SPACE_GRAPH)) {
        TransData *td = tc->data;
        for (int a = 0; a < tc->data_len; a++, td++) {
          if (td->flag & TD_BEZTRIPLE) {
            MEM_freeN(td->hdata);
          }
        }
      }
      MEM_freeN(tc->data);

      MEM_SAFE_FREE(tc->data_mirror);
      MEM_SAFE_FREE(tc->data_ext);
      MEM_SAFE_FREE(tc->data_2d);
      MEM_SAFE_FREE(tc->sorted_index_map);
    }
  }

  MEM_SAFE_FREE(t->data_container);
  t->data_container = nullptr;

  BLI_freelistN(&t->tsnap.points);

  if (t->spacetype == SPACE_IMAGE) {
    if (t->options & (CTX_MASK | CTX_PAINT_CURVE)) {
      /* Pass. */
    }
    else {
      SpaceImage *sima = static_cast<SpaceImage *>(t->area->spacedata.first);
      if (sima->flag & SI_LIVE_UNWRAP) {
        ED_uvedit_live_unwrap_end(t->state == TRANS_CANCEL);
      }
    }
  }

  if (t->mouse.data) {
    MEM_freeN(t->mouse.data);
  }

  if (t->rng != nullptr) {
    BLI_rng_free(t->rng);
  }

  freeSnapping(t);

  if (t->vod) {
    ED_view3d_navigation_free(C, t->vod);
  }
}

static void transdata_restore_basic(TransDataBasic *td_basic)
{
  if (td_basic->loc) {
    copy_v3_v3(td_basic->loc, td_basic->iloc);
  }

  /* TODO(mano-wii): Only use 3D or larger vectors in `td->loc`.
   * If `loc` and `val` point to the same address, it may indicate that `loc` is not 3D which is
   * not safe for `copy_v3_v3`. */
  if (td_basic->val && td_basic->val != td_basic->loc) {
    *td_basic->val = td_basic->ival;
  }
}

static void restoreElement(TransData *td)
{
  transdata_restore_basic((TransDataBasic *)td);

  if (td->flag & TD_BEZTRIPLE) {
    *(td->hdata->h1) = td->hdata->ih1;
    *(td->hdata->h2) = td->hdata->ih2;
  }
}

void restoreTransObjects(TransInfo *t)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {

    TransData *td;
    TransData2D *td2d;
    TransDataMirror *tdm;

    for (td = tc->data; td < tc->data + tc->data_len; td++) {
      restoreElement(td);
    }

    for (tdm = tc->data_mirror; tdm < tc->data_mirror + tc->data_mirror_len; tdm++) {
      transdata_restore_basic((TransDataBasic *)tdm);
    }

    if (tc->data_ext) {
      for (int i = 0; i < tc->data_len; i++) {
        if (tc->data[i].flag & TD_NO_EXT) {
          continue;
        }

        TransDataExtension *td_ext = &tc->data_ext[i];
        if (td_ext->rot) {
          copy_v3_v3(td_ext->rot, td_ext->irot);
        }
        if (td_ext->rotAngle) {
          *td_ext->rotAngle = td_ext->irotAngle;
        }
        if (td_ext->rotAxis) {
          copy_v3_v3(td_ext->rotAxis, td_ext->irotAxis);
        }
        /* XXX, `drotAngle` & `drotAxis` not used yet. */
        if (td_ext->scale) {
          copy_v3_v3(td_ext->scale, td_ext->iscale);
        }
        if (td_ext->quat) {
          copy_qt_qt(td_ext->quat, td_ext->iquat);
        }
      }
    }

    for (td2d = tc->data_2d; tc->data_2d && td2d < tc->data_2d + tc->data_len; td2d++) {
      if (td2d->h1) {
        td2d->h1[0] = td2d->ih1[0];
        td2d->h1[1] = td2d->ih1[1];
      }
      if (td2d->h2) {
        td2d->h2[0] = td2d->ih2[0];
        td2d->h2[1] = td2d->ih2[1];
      }
    }

    unit_m3(t->mat);
  }

  recalc_data(t);
}

void calculateCenter2D(TransInfo *t)
{
  BLI_assert(!is_zero_v3(t->aspect));
  projectFloatView(t, t->center_global, t->center2d);
}

void calculateCenterLocal(TransInfo *t, const float center_global[3])
{
  /* Setting constraint center. */
  /* NOTE: init functions may over-ride `t->center`. */
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    if (tc->use_local_mat) {
      mul_v3_m4v3(tc->center_local, tc->imat, center_global);
    }
    else {
      copy_v3_v3(tc->center_local, center_global);
    }
  }
}

void calculateCenterCursor(TransInfo *t, float r_center[3])
{
  const float *cursor = t->scene->cursor.location;
  copy_v3_v3(r_center, cursor);

  /* If edit or pose mode, move cursor in local space. */
  if (t->options & CTX_PAINT_CURVE) {
    if (ED_view3d_project_float_global(t->region, cursor, r_center, V3D_PROJ_TEST_NOP) !=
        V3D_PROJ_RET_OK)
    {
      projectFloatViewCenterFallback(t, r_center);
    }
    r_center[2] = 0.0f;
  }
}

void calculateCenterCursor2D(TransInfo *t, float r_center[2])
{
  float cursor_local_buf[2];
  const float *cursor = nullptr;

  if (t->spacetype == SPACE_IMAGE) {
    SpaceImage *sima = (SpaceImage *)t->area->spacedata.first;
    cursor = sima->cursor;
  }
  if (t->spacetype == SPACE_SEQ) {
    SpaceSeq *sseq = (SpaceSeq *)t->area->spacedata.first;
    const float2 cursor_pixel = seq::image_preview_unit_to_px(t->scene, sseq->cursor);
    copy_v2_v2(cursor_local_buf, cursor_pixel);
    cursor = cursor_local_buf;
  }
  else if (t->spacetype == SPACE_CLIP) {
    SpaceClip *space_clip = (SpaceClip *)t->area->spacedata.first;
    cursor = space_clip->cursor;
  }

  if (cursor) {
    if (t->options & CTX_MASK) {
      float co[2];

      if (t->spacetype == SPACE_IMAGE) {
        SpaceImage *sima = (SpaceImage *)t->area->spacedata.first;
        BKE_mask_coord_from_image(sima->image, &sima->iuser, co, cursor);
      }
      else if (t->spacetype == SPACE_CLIP) {
        SpaceClip *space_clip = (SpaceClip *)t->area->spacedata.first;
        BKE_mask_coord_from_movieclip(space_clip->clip, &space_clip->user, co, cursor);
      }
      else {
        BLI_assert_msg(0, "Shall not happen");
      }

      r_center[0] = co[0] * t->aspect[0];
      r_center[1] = co[1] * t->aspect[1];
    }
    else if (t->options & CTX_PAINT_CURVE) {
      if (t->spacetype == SPACE_IMAGE) {
        r_center[0] = UI_view2d_view_to_region_x(&t->region->v2d, cursor[0]);
        r_center[1] = UI_view2d_view_to_region_y(&t->region->v2d, cursor[1]);
      }
    }
    else {
      r_center[0] = cursor[0] * t->aspect[0];
      r_center[1] = cursor[1] * t->aspect[1];
    }
  }
}

void calculateCenterCursorGraph2D(TransInfo *t, float r_center[2])
{
  SpaceGraph *sipo = (SpaceGraph *)t->area->spacedata.first;
  Scene *scene = t->scene;

  /* Cursor is combination of current frame, and graph-editor cursor value. */
  if (sipo->mode == SIPO_MODE_DRIVERS) {
    r_center[0] = sipo->cursorTime;
    r_center[1] = sipo->cursorVal;
  }
  else {
    r_center[0] = float(scene->r.cfra);
    r_center[1] = sipo->cursorVal;
  }
}

static bool transdata_center_global_get(const TransDataContainer *tc,
                                        const TransDataBasic *td_basic,
                                        float r_vec[3])
{
  if (td_basic->flag & TD_SELECTED) {
    if (!(td_basic->flag & TD_NOCENTER)) {
      if (tc->use_local_mat) {
        mul_v3_m4v3(r_vec, tc->mat, td_basic->center);
      }
      else {
        copy_v3_v3(r_vec, td_basic->center);
      }
      return true;
    }
  }
  return false;
}

void calculateCenterMedian(TransInfo *t, float r_center[3])
{
  float partial[3] = {0.0f, 0.0f, 0.0f};
  int total = 0;

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    float center[3];
    for (int i = 0; i < tc->data_len; i++) {
      if (transdata_center_global_get(tc, (TransDataBasic *)&tc->data[i], center)) {
        add_v3_v3(partial, center);
        total++;
      }
    }
    for (int i = 0; i < tc->data_mirror_len; i++) {
      if (transdata_center_global_get(tc, (TransDataBasic *)&tc->data_mirror[i], center)) {
        add_v3_v3(partial, center);
        total++;
      }
    }
  }
  if (total) {
    mul_v3_fl(partial, 1.0f / float(total));
  }
  copy_v3_v3(r_center, partial);
}

void calculateCenterBound(TransInfo *t, float r_center[3])
{
  float max[3], min[3];
  bool changed = false;
  INIT_MINMAX(min, max);
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    float center[3];
    for (int i = 0; i < tc->data_len; i++) {
      if (transdata_center_global_get(tc, (TransDataBasic *)&tc->data[i], center)) {
        minmax_v3v3_v3(min, max, center);
        changed = true;
      }
    }
    for (int i = 0; i < tc->data_mirror_len; i++) {
      if (transdata_center_global_get(tc, (TransDataBasic *)&tc->data_mirror[i], center)) {
        minmax_v3v3_v3(min, max, center);
        changed = true;
      }
    }
  }
  if (changed) {
    mid_v3_v3v3(r_center, min, max);
  }
}

bool calculateCenterActive(TransInfo *t, bool select_only, float r_center[3])
{
  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_OK(t);

  if (t->spacetype != SPACE_VIEW3D) {
    return false;
  }
  if (tc->obedit) {
    if (object::calc_active_center_for_editmode(tc->obedit, select_only, r_center)) {
      mul_m4_v3(tc->obedit->object_to_world().ptr(), r_center);
      return true;
    }
  }
  else if (t->options & CTX_POSE_BONE) {
    BKE_view_layer_synced_ensure(t->scene, t->view_layer);
    Object *ob = BKE_view_layer_active_object_get(t->view_layer);
    if (object::calc_active_center_for_posemode(ob, select_only, r_center)) {
      mul_m4_v3(ob->object_to_world().ptr(), r_center);
      return true;
    }
  }
  else if (t->options & CTX_PAINT_CURVE) {
    Paint *paint = BKE_paint_get_active(t->scene, t->view_layer);
    Brush *br = BKE_paint_brush(paint);
    PaintCurve *pc = br->paint_curve;
    copy_v3_v3(r_center, pc->points[pc->add_index - 1].bez.vec[1]);
    BKE_brush_tag_unsaved_changes(br);
    r_center[2] = 0.0f;
    return true;
  }
  else {
    /* Object mode. */
    BKE_view_layer_synced_ensure(t->scene, t->view_layer);
    Base *base = BKE_view_layer_active_base_get(t->view_layer);
    if (base && ((!select_only) || ((base->flag & BASE_SELECTED) != 0))) {
      copy_v3_v3(r_center, base->object->object_to_world().location());
      return true;
    }
  }

  return false;
}

static void calculateCenter_FromAround(TransInfo *t, int around, float r_center[3])
{
  switch (around) {
    case V3D_AROUND_CENTER_BOUNDS:
      calculateCenterBound(t, r_center);
      break;
    case V3D_AROUND_CENTER_MEDIAN:
      calculateCenterMedian(t, r_center);
      break;
    case V3D_AROUND_CURSOR:
      if (ELEM(t->spacetype, SPACE_IMAGE, SPACE_SEQ, SPACE_CLIP)) {
        calculateCenterCursor2D(t, r_center);
      }
      else if (t->spacetype == SPACE_GRAPH) {
        calculateCenterCursorGraph2D(t, r_center);
      }
      else {
        calculateCenterCursor(t, r_center);
      }
      break;
    case V3D_AROUND_LOCAL_ORIGINS:
      /* Individual element center uses median center for helpline and such. */
      calculateCenterMedian(t, r_center);
      break;
    case V3D_AROUND_ACTIVE: {
      if (calculateCenterActive(t, false, r_center)) {
        /* Pass. */
      }
      else {
        /* Fallback. */
        calculateCenterMedian(t, r_center);
      }
      break;
    }
  }
}

static void calculateZfac(TransInfo *t)
{
  /* #ED_view3d_calc_zfac() defines a factor for perspective depth correction,
   * used in #ED_view3d_win_to_delta(). */

  /* `zfac` is only used #convertViewVec only in cases operator was invoked in #RGN_TYPE_WINDOW
   * and never used in other cases.
   *
   * We need special case here as well, since #ED_view3d_calc_zfac will crash when called
   * for a region different from #RGN_TYPE_WINDOW.
   */
  if ((t->spacetype == SPACE_VIEW3D) && (t->region->regiontype == RGN_TYPE_WINDOW)) {
    t->zfac = ED_view3d_calc_zfac(static_cast<const RegionView3D *>(t->region->regiondata),
                                  t->center_global);
  }
  else if (t->spacetype == SPACE_IMAGE) {
    SpaceImage *sima = static_cast<SpaceImage *>(t->area->spacedata.first);
    t->zfac = 1.0f / sima->zoom;
  }
  else if (t->region) {
    View2D *v2d = &t->region->v2d;
    /* Get zoom factor the same way as in
     * #ui_view2d_curRect_validate_resize - better keep in sync! */
    const float zoomx = float(BLI_rcti_size_x(&v2d->mask) + 1) / BLI_rctf_size_x(&v2d->cur);
    t->zfac = 1.0f / zoomx;
  }
}

void calculateCenter(TransInfo *t)
{
  if ((t->flag & T_OVERRIDE_CENTER) == 0) {
    calculateCenter_FromAround(t, t->around, t->center_global);
  }
  calculateCenterLocal(t, t->center_global);

  calculateCenter2D(t);

  /* For panning from the camera-view. */
  if ((t->options & CTX_OBJECT) && (t->flag & T_OVERRIDE_CENTER) == 0) {
    if (t->spacetype == SPACE_VIEW3D && t->region && t->region->regiontype == RGN_TYPE_WINDOW) {

      if (t->options & CTX_CAMERA) {
        float axis[3];
        /* `persinv` is nasty, use `viewinv` instead, always right. */
        copy_v3_v3(axis, t->viewinv[2]);
        normalize_v3(axis);

        /* 6.0 = 6 grid units. */
        axis[0] = t->center_global[0] - 6.0f * axis[0];
        axis[1] = t->center_global[1] - 6.0f * axis[1];
        axis[2] = t->center_global[2] - 6.0f * axis[2];

        projectFloatView(t, axis, t->center2d);

        /* Rotate only needs correct 2d center, grab needs #ED_view3d_calc_zfac() value. */
        if (t->mode == TFM_TRANSLATION) {
          copy_v3_v3(t->center_global, axis);
        }
      }
    }
  }

  calculateZfac(t);
}

void transformViewUpdate(TransInfo *t)
{
  float zoom_prev = t->zfac;
  float zoom_new;
  if ((t->spacetype == SPACE_VIEW3D) && (t->region->regiontype == RGN_TYPE_WINDOW)) {
    if (!t->persp) {
      zoom_prev *= len_v3(t->persinv[0]);
    }

    setTransformViewMatrices(t);
    calculateZfac(t);

    zoom_new = t->zfac;
    if (!t->persp) {
      zoom_new *= len_v3(t->persinv[0]);
    }

    for (int i = 0; i < ARRAY_SIZE(t->orient); i++) {
      if (t->orient[i].type == V3D_ORIENT_VIEW) {
        copy_m3_m4(t->orient[i].matrix, t->viewinv);
        normalize_m3(t->orient[i].matrix);
        if (t->orient_curr == i) {
          copy_m3_m3(t->spacemtx, t->orient[i].matrix);
          invert_m3_m3_safe_ortho(t->spacemtx_inv, t->spacemtx);
        }
      }
    }
  }
  else {
    calculateZfac(t);
    zoom_new = t->zfac;
  }

  calculateCenter2D(t);
  transform_snap_grid_init(t, t->snap_spatial, &t->snap_spatial_precision);
  transform_input_update(t, zoom_prev / zoom_new);
}

void calculatePropRatio(TransInfo *t)
{
  int i;
  float dist;
  const bool connected = (t->flag & T_PROP_CONNECTED) != 0;

  t->proptext[0] = '\0';

  if (t->flag & T_PROP_EDIT) {
    const char *pet_id = nullptr;
    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      TransData *td = tc->data;
      for (i = 0; i < tc->data_len; i++, td++) {
        if (td->flag & TD_SELECTED) {
          td->factor = 1.0f;
        }
        else if ((connected ? td->dist : td->rdist) > t->prop_size) {
          td->factor = 0.0f;
          restoreElement(td);
        }
        else {
          /* Use `rdist` for falloff calculations, it is the real distance. */
          if (connected) {
            dist = (t->prop_size - td->dist) / t->prop_size;
          }
          else {
            dist = (t->prop_size - td->rdist) / t->prop_size;
          }

          /*
           * Clamp to positive numbers.
           * Certain corner cases with connectivity and individual centers
           * can give values of rdist larger than propsize.
           */
          dist = std::max(dist, 0.0f);

          switch (t->prop_mode) {
            case PROP_SHARP:
              td->factor = dist * dist;
              break;
            case PROP_SMOOTH:
              /* Float imprecision can cause a `dist` approaching 1.0
               * to assign `td->factor` exceeding 1.0. See #147530. */
              td->factor = std::min(1.0f, 3.0f * dist * dist - 2.0f * dist * dist * dist);
              break;
            case PROP_ROOT:
              td->factor = sqrtf(dist);
              break;
            case PROP_LIN:
              td->factor = dist;
              break;
            case PROP_CONST:
              td->factor = 1.0f;
              break;
            case PROP_SPHERE:
              td->factor = sqrtf(2 * dist - dist * dist);
              break;
            case PROP_RANDOM:
              if (t->rng == nullptr) {
                /* Lazy initialization. */
                uint rng_seed = uint(BLI_time_now_seconds_i() & UINT_MAX);
                t->rng = BLI_rng_new(rng_seed);
              }
              td->factor = BLI_rng_get_float(t->rng) * dist;
              break;
            case PROP_INVSQUARE:
              td->factor = dist * (2.0f - dist);
              break;
            default:
              td->factor = 1;
              break;
          }
          /* An assert here likely means clamping is needed. */
          BLI_assert(td->factor <= 1.0f);
        }
      }
    }

    switch (t->prop_mode) {
      case PROP_SHARP:
        pet_id = N_("(Sharp)");
        break;
      case PROP_SMOOTH:
        pet_id = N_("(Smooth)");
        break;
      case PROP_ROOT:
        pet_id = N_("(Root)");
        break;
      case PROP_LIN:
        pet_id = N_("(Linear)");
        break;
      case PROP_CONST:
        pet_id = N_("(Constant)");
        break;
      case PROP_SPHERE:
        pet_id = N_("(Sphere)");
        break;
      case PROP_RANDOM:
        pet_id = N_("(Random)");
        break;
      case PROP_INVSQUARE:
        pet_id = N_("(InvSquare)");
        break;
      default:
        break;
    }

    if (pet_id) {
      STRNCPY_UTF8(t->proptext, IFACE_(pet_id));
    }
  }
  else {
    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      TransData *td = tc->data;
      for (i = 0; i < tc->data_len; i++, td++) {
        td->factor = 1.0;
      }
    }
  }
}

void transform_data_ext_rotate(TransData *td,
                               TransDataExtension *td_ext,
                               float mat[3][3],
                               bool use_drot)
{
  float totmat[3][3];
  float smat[3][3];
  float fmat[3][3];
  float obmat[3][3];

  float dmat[3][3]; /* Delta rotation. */
  float dmat_inv[3][3];

  mul_m3_m3m3(totmat, mat, td->mtx);
  mul_m3_m3m3(smat, td->smtx, mat);

  /* Logic from #BKE_object_rot_to_mat3. */
  if (use_drot) {
    if (td_ext->rotOrder > 0) {
      eulO_to_mat3(dmat, td_ext->drot, td_ext->rotOrder);
    }
    else if (td_ext->rotOrder == ROT_MODE_AXISANGLE) {
#if 0
      axis_angle_to_mat3(dmat, td_ext->drotAxis, td_ext->drotAngle);
#else
      unit_m3(dmat);
#endif
    }
    else {
      float tquat[4];
      normalize_qt_qt(tquat, td_ext->dquat);
      quat_to_mat3(dmat, tquat);
    }

    invert_m3_m3(dmat_inv, dmat);
  }

  if (td_ext->rotOrder == ROT_MODE_QUAT) {
    float quat[4];

    /* Calculate the total rotation. */
    quat_to_mat3(obmat, td_ext->iquat);
    if (use_drot) {
      mul_m3_m3m3(obmat, dmat, obmat);
    }

    /* `mat = transform`, `obmat = object rotation`. */
    mul_m3_m3m3(fmat, smat, obmat);

    if (use_drot) {
      mul_m3_m3m3(fmat, dmat_inv, fmat);
    }

    mat3_to_quat(quat, fmat);

    /* Apply. */
    copy_qt_qt(td_ext->quat, quat);
  }
  else if (td_ext->rotOrder == ROT_MODE_AXISANGLE) {
    float axis[3], angle;

    /* Calculate the total rotation. */
    axis_angle_to_mat3(obmat, td_ext->irotAxis, td_ext->irotAngle);
    if (use_drot) {
      mul_m3_m3m3(obmat, dmat, obmat);
    }

    /* `mat = transform`, `obmat = object rotation`. */
    mul_m3_m3m3(fmat, smat, obmat);

    if (use_drot) {
      mul_m3_m3m3(fmat, dmat_inv, fmat);
    }

    mat3_to_axis_angle(axis, &angle, fmat);

    /* Apply. */
    copy_v3_v3(td_ext->rotAxis, axis);
    *td_ext->rotAngle = angle;
  }
  else {
    float eul[3];

    /* Calculate the total rotation. */
    eulO_to_mat3(obmat, td_ext->irot, td_ext->rotOrder);
    if (use_drot) {
      mul_m3_m3m3(obmat, dmat, obmat);
    }

    /* `mat = transform`, `obmat = object rotation`. */
    mul_m3_m3m3(fmat, smat, obmat);

    if (use_drot) {
      mul_m3_m3m3(fmat, dmat_inv, fmat);
    }

    mat3_to_compatible_eulO(eul, td_ext->rot, td_ext->rotOrder, fmat);

    /* Apply. */
    copy_v3_v3(td_ext->rot, eul);
  }
}

Object *transform_object_deform_pose_armature_get(const TransInfo *t, Object *ob)
{
  if (!(ob->mode & OB_MODE_ALL_WEIGHT_PAINT)) {
    return nullptr;
  }
  /* Important that ob_armature can be set even when its not selected #23412.
   * Lines below just check is also visible. */
  Object *ob_armature = BKE_modifiers_is_deformed_by_armature(ob);
  if (ob_armature && ob_armature->mode & OB_MODE_POSE) {
    BKE_view_layer_synced_ensure(t->scene, t->view_layer);
    Base *base_arm = BKE_view_layer_base_find(t->view_layer, ob_armature);
    if (base_arm) {
      View3D *v3d = static_cast<View3D *>(t->view);
      if (BASE_VISIBLE(v3d, base_arm)) {
        return ob_armature;
      }
    }
  }
  return nullptr;
}

}  // namespace blender::ed::transform
