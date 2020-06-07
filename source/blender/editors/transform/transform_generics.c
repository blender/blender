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
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup edtransform
 */

#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_gpencil_types.h"
#include "DNA_mesh_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_rand.h"

#include "PIL_time.h"

#include "BLT_translation.h"

#include "RNA_access.h"

#include "GPU_immediate.h"
#include "GPU_matrix.h"

#include "BKE_context.h"
#include "BKE_layer.h"
#include "BKE_mask.h"
#include "BKE_paint.h"

#include "ED_clip.h"
#include "ED_image.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_uvedit.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_resources.h"
#include "UI_view2d.h"

#include "transform.h"
#include "transform_mode.h"
#include "transform_snap.h"

/* ************************** Functions *************************** */

void getViewVector(const TransInfo *t, const float coord[3], float vec[3])
{
  if (t->persp != RV3D_ORTHO) {
    sub_v3_v3v3(vec, coord, t->viewinv[3]);
  }
  else {
    copy_v3_v3(vec, t->viewinv[2]);
  }
  normalize_v3(vec);
}

/* ************************** GENERICS **************************** */

void drawLine(TransInfo *t, const float center[3], const float dir[3], char axis, short options)
{
  float v1[3], v2[3], v3[3];
  uchar col[3], col2[3];

  if (t->spacetype == SPACE_VIEW3D) {
    View3D *v3d = t->view;

    GPU_matrix_push();

    copy_v3_v3(v3, dir);
    mul_v3_fl(v3, v3d->clip_end);

    sub_v3_v3v3(v2, center, v3);
    add_v3_v3v3(v1, center, v3);

    if (options & DRAWLIGHT) {
      col[0] = col[1] = col[2] = 220;
    }
    else {
      UI_GetThemeColor3ubv(TH_GRID, col);
    }
    UI_make_axis_color(col, col2, axis);

    uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
    immUniformColor3ubv(col2);

    immBegin(GPU_PRIM_LINES, 2);
    immVertex3fv(pos, v1);
    immVertex3fv(pos, v2);
    immEnd();

    immUnbindProgram();

    GPU_matrix_pop();
  }
}

/**
 * Free data before switching to another mode.
 */
void resetTransModal(TransInfo *t)
{
  freeTransCustomDataForMode(t);
}

void resetTransRestrictions(TransInfo *t)
{
  t->flag &= ~T_ALL_RESTRICTIONS;
}

static int initTransInfo_edit_pet_to_flag(const int proportional)
{
  int flag = 0;
  if (proportional & PROP_EDIT_USE) {
    flag |= T_PROP_EDIT;
  }
  if (proportional & PROP_EDIT_CONNECTED) {
    flag |= T_PROP_CONNECTED;
  }
  if (proportional & PROP_EDIT_PROJECTED) {
    flag |= T_PROP_PROJECTED;
  }
  return flag;
}

void initTransDataContainers_FromObjectData(TransInfo *t,
                                            Object *obact,
                                            Object **objects,
                                            uint objects_len)
{
  const eObjectMode object_mode = obact ? obact->mode : OB_MODE_OBJECT;
  const short object_type = obact ? obact->type : -1;

  if ((object_mode & OB_MODE_EDIT) || (t->options & CTX_GPENCIL_STROKES) ||
      ((object_mode & OB_MODE_POSE) && (object_type == OB_ARMATURE))) {
    if (t->data_container) {
      MEM_freeN(t->data_container);
    }

    bool free_objects = false;
    if (objects == NULL) {
      objects = BKE_view_layer_array_from_objects_in_mode(
          t->view_layer,
          (t->spacetype == SPACE_VIEW3D) ? t->view : NULL,
          &objects_len,
          {
              .object_mode = object_mode,
              .no_dup_data = true,
          });
      free_objects = true;
    }

    t->data_container = MEM_callocN(sizeof(*t->data_container) * objects_len, __func__);
    t->data_container_len = objects_len;

    for (int i = 0; i < objects_len; i++) {
      TransDataContainer *tc = &t->data_container[i];
      if (((t->flag & T_NO_MIRROR) == 0) && ((t->options & CTX_NO_MIRROR) == 0) &&
          (objects[i]->type == OB_MESH)) {
        tc->mirror.axis_x = (((Mesh *)objects[i]->data)->editflag & ME_EDIT_MIRROR_X) != 0;
        tc->mirror.axis_y = (((Mesh *)objects[i]->data)->editflag & ME_EDIT_MIRROR_Y) != 0;
        tc->mirror.axis_z = (((Mesh *)objects[i]->data)->editflag & ME_EDIT_MIRROR_Z) != 0;
      }

      if (object_mode & OB_MODE_EDIT) {
        tc->obedit = objects[i];
        /* Check needed for UV's */
        if ((t->flag & T_2D_EDIT) == 0) {
          tc->use_local_mat = true;
        }
      }
      else if (object_mode & OB_MODE_POSE) {
        tc->poseobj = objects[i];
        tc->use_local_mat = true;
      }
      else if (t->options & CTX_GPENCIL_STROKES) {
        tc->use_local_mat = true;
      }

      if (tc->use_local_mat) {
        BLI_assert((t->flag & T_2D_EDIT) == 0);
        copy_m4_m4(tc->mat, objects[i]->obmat);
        copy_m3_m4(tc->mat3, tc->mat);
        /* for non-invertible scale matrices, invert_m4_m4_fallback()
         * can still provide a valid pivot */
        invert_m4_m4_fallback(tc->imat, tc->mat);
        invert_m3_m3(tc->imat3, tc->mat3);
        normalize_m3_m3(tc->mat3_unit, tc->mat3);
      }
      /* Otherwise leave as zero. */
    }

    if (free_objects) {
      MEM_freeN(objects);
    }
  }
}

/**
 * Setup internal data, mouse, vectors
 *
 * \note \a op and \a event can be NULL
 *
 * \see #saveTransform does the reverse.
 */
void initTransInfo(bContext *C, TransInfo *t, wmOperator *op, const wmEvent *event)
{
  Scene *sce = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *obact = OBACT(view_layer);
  const eObjectMode object_mode = obact ? obact->mode : OB_MODE_OBJECT;
  ToolSettings *ts = CTX_data_tool_settings(C);
  ARegion *region = CTX_wm_region(C);
  ScrArea *area = CTX_wm_area(C);

  bGPdata *gpd = CTX_data_gpencil_data(C);
  PropertyRNA *prop;

  t->mbus = CTX_wm_message_bus(C);
  t->depsgraph = CTX_data_depsgraph_pointer(C);
  t->scene = sce;
  t->view_layer = view_layer;
  t->area = area;
  t->region = region;
  t->settings = ts;
  t->reports = op ? op->reports : NULL;

  t->helpline = HLP_NONE;

  t->flag = 0;

  if (obact && !(t->options & (CTX_CURSOR | CTX_TEXTURE)) &&
      ELEM(object_mode, OB_MODE_EDIT, OB_MODE_EDIT_GPENCIL)) {
    t->obedit_type = obact->type;
  }
  else {
    t->obedit_type = -1;
  }

  /* Many kinds of transform only use a single handle. */
  if (t->data_container == NULL) {
    t->data_container = MEM_callocN(sizeof(*t->data_container), __func__);
    t->data_container_len = 1;
  }

  t->redraw = TREDRAW_HARD; /* redraw first time */

  int mval[2];
  if (event) {
    copy_v2_v2_int(mval, event->mval);
  }
  else {
    zero_v2_int(mval);
  }
  copy_v2_v2_int(t->mval, mval);
  copy_v2_v2_int(t->mouse.imval, mval);
  copy_v2_v2_int(t->con.imval, mval);

  t->transform = NULL;
  t->handleEvent = NULL;

  t->data_len_all = 0;

  t->val = 0.0f;

  zero_v3(t->vec);
  zero_v3(t->center_global);

  unit_m3(t->mat);

  /* Default to rotate on the Z axis. */
  t->orient_axis = 2;
  t->orient_axis_ortho = 1;

  /* if there's an event, we're modal */
  if (event) {
    t->flag |= T_MODAL;
  }

  /* Crease needs edge flag */
  if (ELEM(t->mode, TFM_CREASE, TFM_BWEIGHT)) {
    t->options |= CTX_EDGE;
  }

  t->remove_on_cancel = false;

  if (op && (prop = RNA_struct_find_property(op->ptr, "remove_on_cancel")) &&
      RNA_property_is_set(op->ptr, prop)) {
    if (RNA_property_boolean_get(op->ptr, prop)) {
      t->remove_on_cancel = true;
    }
  }

  /* GPencil editing context */
  if (GPENCIL_EDIT_MODE(gpd)) {
    t->options |= CTX_GPENCIL_STROKES;
  }

  /* Assign the space type, some exceptions for running in different mode */
  if (area == NULL) {
    /* background mode */
    t->spacetype = SPACE_EMPTY;
  }
  else if ((region == NULL) && (area->spacetype == SPACE_VIEW3D)) {
    /* running in the text editor */
    t->spacetype = SPACE_EMPTY;
  }
  else {
    /* normal operation */
    t->spacetype = area->spacetype;
  }

  /* handle T_ALT_TRANSFORM initialization, we may use for different operators */
  if (op) {
    const char *prop_id = NULL;
    if (t->mode == TFM_SHRINKFATTEN) {
      prop_id = "use_even_offset";
    }

    if (prop_id && (prop = RNA_struct_find_property(op->ptr, prop_id))) {
      SET_FLAG_FROM_TEST(t->flag, RNA_property_boolean_get(op->ptr, prop), T_ALT_TRANSFORM);
    }
  }

  if (t->spacetype == SPACE_VIEW3D) {
    View3D *v3d = area->spacedata.first;
    bScreen *animscreen = ED_screen_animation_playing(CTX_wm_manager(C));

    t->view = v3d;
    t->animtimer = (animscreen) ? animscreen->animtimer : NULL;

    /* turn gizmo off during transform */
    if (t->flag & T_MODAL) {
      t->gizmo_flag = v3d->gizmo_flag;
      v3d->gizmo_flag = V3D_GIZMO_HIDE;
    }

    if (t->scene->toolsettings->transform_flag & SCE_XFORM_AXIS_ALIGN) {
      t->flag |= T_V3D_ALIGN;
    }
    t->around = t->scene->toolsettings->transform_pivot_point;

    /* bend always uses the cursor */
    if (t->mode == TFM_BEND) {
      t->around = V3D_AROUND_CURSOR;
    }

    /* exceptional case */
    if (t->around == V3D_AROUND_LOCAL_ORIGINS) {
      if (ELEM(t->mode, TFM_ROTATION, TFM_RESIZE, TFM_TRACKBALL)) {
        const bool use_island = transdata_check_local_islands(t, t->around);

        if ((t->obedit_type != -1) && !use_island) {
          t->options |= CTX_NO_PET;
        }
      }
    }

    if (object_mode & OB_MODE_ALL_PAINT) {
      Paint *p = BKE_paint_get_active_from_context(C);
      if (p && p->brush && (p->brush->flag & BRUSH_CURVE)) {
        t->options |= CTX_PAINT_CURVE;
      }
    }

    /* initialize UV transform from */
    if (op && ((prop = RNA_struct_find_property(op->ptr, "correct_uv")))) {
      if (RNA_property_is_set(op->ptr, prop)) {
        if (RNA_property_boolean_get(op->ptr, prop)) {
          t->settings->uvcalc_flag |= UVCALC_TRANSFORM_CORRECT;
        }
        else {
          t->settings->uvcalc_flag &= ~UVCALC_TRANSFORM_CORRECT;
        }
      }
      else {
        RNA_property_boolean_set(
            op->ptr, prop, (t->settings->uvcalc_flag & UVCALC_TRANSFORM_CORRECT) != 0);
      }
    }
  }
  else if (t->spacetype == SPACE_IMAGE) {
    SpaceImage *sima = area->spacedata.first;
    // XXX for now, get View2D from the active region
    t->view = &region->v2d;
    t->around = sima->around;

    if (ED_space_image_show_uvedit(sima, OBACT(t->view_layer))) {
      /* UV transform */
    }
    else if (sima->mode == SI_MODE_MASK) {
      t->options |= CTX_MASK;
    }
    else if (sima->mode == SI_MODE_PAINT) {
      Paint *p = &sce->toolsettings->imapaint.paint;
      if (p->brush && (p->brush->flag & BRUSH_CURVE)) {
        t->options |= CTX_PAINT_CURVE;
      }
    }
    /* image not in uv edit, nor in mask mode, can happen for some tools */
  }
  else if (t->spacetype == SPACE_NODE) {
    // XXX for now, get View2D from the active region
    t->view = &region->v2d;
    t->around = V3D_AROUND_CENTER_BOUNDS;
  }
  else if (t->spacetype == SPACE_GRAPH) {
    SpaceGraph *sipo = area->spacedata.first;
    t->view = &region->v2d;
    t->around = sipo->around;
  }
  else if (t->spacetype == SPACE_CLIP) {
    SpaceClip *sclip = area->spacedata.first;
    t->view = &region->v2d;
    t->around = sclip->around;

    if (ED_space_clip_check_show_trackedit(sclip)) {
      t->options |= CTX_MOVIECLIP;
    }
    else if (ED_space_clip_check_show_maskedit(sclip)) {
      t->options |= CTX_MASK;
    }
  }
  else {
    if (region) {
      // XXX for now, get View2D  from the active region
      t->view = &region->v2d;
      // XXX for now, the center point is the midpoint of the data
    }
    else {
      t->view = NULL;
    }
    t->around = V3D_AROUND_CENTER_BOUNDS;
  }

  BLI_assert(is_zero_v4(t->values_modal_offset));
  bool t_values_set_is_array = false;
  if (op && (prop = RNA_struct_find_property(op->ptr, "value")) &&
      RNA_property_is_set(op->ptr, prop)) {
    float values[4] = {0}; /* in case value isn't length 4, avoid uninitialized memory  */
    if (RNA_property_array_check(prop)) {
      RNA_float_get_array(op->ptr, "value", values);
      t_values_set_is_array = true;
    }
    else {
      values[0] = RNA_float_get(op->ptr, "value");
    }

    copy_v4_v4(t->values, values);
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
    if (RNA_property_is_set(op->ptr, prop)) {
      RNA_property_boolean_get_array(op->ptr, prop, constraint_axis);
    }

    if (t_values_set_is_array && t->flag & T_INPUT_IS_VALUES_FINAL) {
      /* For operators whose `t->values` is array, set constraint so that the
       * orientation is more intuitive in the Redo Panel. */
      for (int i = 3; i--;) {
        constraint_axis[i] |= t->values[i] != 0.0f;
      }
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
    short orient_type_set = -1;
    short orient_type_matrix_set = -1;
    short orient_type_scene = V3D_ORIENT_GLOBAL;

    if ((t->spacetype == SPACE_VIEW3D) && (t->region->regiontype == RGN_TYPE_WINDOW)) {
      TransformOrientationSlot *orient_slot = &t->scene->orientation_slots[SCE_ORIENT_DEFAULT];
      orient_type_scene = orient_slot->type;
      if (orient_type_scene == V3D_ORIENT_CUSTOM) {
        const int index_custom = orient_slot->index_custom;
        orient_type_scene += index_custom;
      }
    }

    short orient_types[3];
    float custom_matrix[3][3];
    bool use_orient_axis = false;

    if (op && (prop = RNA_struct_find_property(op->ptr, "orient_axis"))) {
      t->orient_axis = RNA_property_enum_get(op->ptr, prop);
      use_orient_axis = true;
    }
    if (op && (prop = RNA_struct_find_property(op->ptr, "orient_axis_ortho"))) {
      t->orient_axis_ortho = RNA_property_enum_get(op->ptr, prop);
    }

    if (op && ((prop = RNA_struct_find_property(op->ptr, "orient_type")) &&
               RNA_property_is_set(op->ptr, prop))) {
      orient_type_set = RNA_property_enum_get(op->ptr, prop);
      if (orient_type_set >= V3D_ORIENT_CUSTOM) {
        if (orient_type_set >= V3D_ORIENT_CUSTOM + BIF_countTransformOrientation(C)) {
          orient_type_set = V3D_ORIENT_GLOBAL;
        }
      }

      /* Change the default orientation to be used when redoing. */
      orient_types[0] = orient_type_set;
      orient_types[1] = orient_type_set;
      orient_types[2] = orient_type_scene;
    }
    else {
      if ((t->flag & T_MODAL) && (use_orient_axis || transform_mode_is_changeable(t->mode))) {
        orient_types[0] = V3D_ORIENT_VIEW;
      }
      else {
        orient_types[0] = orient_type_scene;
      }
      orient_types[1] = orient_type_scene;
      orient_types[2] = orient_type_scene != V3D_ORIENT_GLOBAL ? V3D_ORIENT_GLOBAL :
                                                                 V3D_ORIENT_LOCAL;
    }

    if (op && ((prop = RNA_struct_find_property(op->ptr, "orient_matrix")) &&
               RNA_property_is_set(op->ptr, prop))) {
      RNA_property_float_get_array(op->ptr, prop, &custom_matrix[0][0]);

      if ((prop = RNA_struct_find_property(op->ptr, "orient_matrix_type")) &&
          RNA_property_is_set(op->ptr, prop)) {
        orient_type_matrix_set = RNA_property_enum_get(op->ptr, prop);
      }
      else if (orient_type_set != -1) {
        orient_type_matrix_set = orient_type_set;
      }
      else {
        orient_type_matrix_set = orient_type_set = V3D_ORIENT_GLOBAL;
      }

      if (orient_type_matrix_set == orient_type_set) {
        /* Constraints are forced to use the custom matrix when redoing. */
        orient_types[0] = V3D_ORIENT_CUSTOM_MATRIX;
      }
    }

    if (t->con.mode & CON_APPLY) {
      t->orient_curr = 1;
    }

    /* For efficiency, avoid calculating the same orientation twice. */
    for (int i = 1; i < 3; i++) {
      t->orient[i].type = transform_orientation_matrix_get(
          C, t, orient_types[i], custom_matrix, t->orient[i].matrix);
    }

    if (orient_types[0] != orient_types[1]) {
      t->orient[0].type = transform_orientation_matrix_get(
          C, t, orient_types[0], custom_matrix, t->orient[0].matrix);
    }
    else {
      memcpy(&t->orient[0], &t->orient[1], sizeof(t->orient[0]));
    }

    const char *spacename = transform_orientations_spacename_get(t, orient_types[0]);
    BLI_strncpy(t->spacename, spacename, sizeof(t->spacename));
  }

  if (op && ((prop = RNA_struct_find_property(op->ptr, "release_confirm")) &&
             RNA_property_is_set(op->ptr, prop))) {
    if (RNA_property_boolean_get(op->ptr, prop)) {
      t->flag |= T_RELEASE_CONFIRM;
    }
  }
  else {
    /* Release confirms preference should not affect node editor (T69288, T70504). */
    if (ISMOUSE(t->launch_event) &&
        ((U.flag & USER_RELEASECONFIRM) || (t->spacetype == SPACE_NODE))) {
      /* Global "release confirm" on mouse bindings */
      t->flag |= T_RELEASE_CONFIRM;
    }
  }

  if (op && ((prop = RNA_struct_find_property(op->ptr, "mirror")) &&
             RNA_property_is_set(op->ptr, prop))) {
    if (!RNA_property_boolean_get(op->ptr, prop)) {
      t->flag |= T_NO_MIRROR;
    }
  }
  else if ((t->spacetype == SPACE_VIEW3D) && (t->obedit_type == OB_MESH)) {
    /* pass */
  }
  else {
    /* Avoid mirroring for unsupported contexts. */
    t->options |= CTX_NO_MIRROR;
  }

  /* setting PET flag only if property exist in operator. Otherwise, assume it's not supported */
  if (op && (prop = RNA_struct_find_property(op->ptr, "use_proportional_edit"))) {
    if (RNA_property_is_set(op->ptr, prop)) {
      int proportional = 0;
      if (RNA_property_boolean_get(op->ptr, prop)) {
        proportional |= PROP_EDIT_USE;
        if (RNA_boolean_get(op->ptr, "use_proportional_connected")) {
          proportional |= PROP_EDIT_CONNECTED;
        }
        if (RNA_boolean_get(op->ptr, "use_proportional_projected")) {
          proportional |= PROP_EDIT_PROJECTED;
        }
      }
      t->flag |= initTransInfo_edit_pet_to_flag(proportional);
    }
    else {
      /* use settings from scene only if modal */
      if (t->flag & T_MODAL) {
        if ((t->options & CTX_NO_PET) == 0) {
          if (t->spacetype == SPACE_GRAPH) {
            t->flag |= initTransInfo_edit_pet_to_flag(ts->proportional_fcurve);
          }
          else if (t->spacetype == SPACE_ACTION) {
            t->flag |= initTransInfo_edit_pet_to_flag(ts->proportional_action);
          }
          else if (t->obedit_type != -1) {
            t->flag |= initTransInfo_edit_pet_to_flag(ts->proportional_edit);
          }
          else if (t->options & CTX_GPENCIL_STROKES) {
            t->flag |= initTransInfo_edit_pet_to_flag(ts->proportional_edit);
          }
          else if (t->options & CTX_MASK) {
            if (ts->proportional_mask) {
              t->flag |= T_PROP_EDIT;

              if (ts->proportional_edit & PROP_EDIT_CONNECTED) {
                t->flag |= T_PROP_CONNECTED;
              }
            }
          }
          else if (!(t->options & CTX_CURSOR) && ts->proportional_objects) {
            t->flag |= T_PROP_EDIT;
          }
        }
      }
    }

    if (op && ((prop = RNA_struct_find_property(op->ptr, "proportional_size")) &&
               RNA_property_is_set(op->ptr, prop))) {
      t->prop_size = RNA_property_float_get(op->ptr, prop);
    }
    else {
      t->prop_size = ts->proportional_size;
    }

    /* TRANSFORM_FIX_ME rna restrictions */
    if (t->prop_size <= 0.00001f) {
      printf("Proportional size (%f) under 0.00001, resetting to 1!\n", t->prop_size);
      t->prop_size = 1.0f;
    }

    if (op && ((prop = RNA_struct_find_property(op->ptr, "proportional_edit_falloff")) &&
               RNA_property_is_set(op->ptr, prop))) {
      t->prop_mode = RNA_property_enum_get(op->ptr, prop);
    }
    else {
      t->prop_mode = ts->prop_mode;
    }
  }
  else { /* add not pet option to context when not available */
    t->options |= CTX_NO_PET;
  }

  if (t->obedit_type == OB_MESH) {
    if (op && (prop = RNA_struct_find_property(op->ptr, "use_automerge_and_split")) &&
        RNA_property_is_set(op->ptr, prop)) {
      if (RNA_property_boolean_get(op->ptr, prop)) {
        t->flag |= T_AUTOMERGE | T_AUTOSPLIT;
      }
    }
    else {
      char automerge = t->scene->toolsettings->automerge;
      if (automerge & AUTO_MERGE) {
        t->flag |= T_AUTOMERGE;
        if (automerge & AUTO_MERGE_AND_SPLIT) {
          t->flag |= T_AUTOSPLIT;
        }
      }
    }
  }

  // Mirror is not supported with PET, turn it off.
#if 0
  if (t->flag & T_PROP_EDIT) {
    t->flag &= ~T_MIRROR;
  }
#endif

  setTransformViewAspect(t, t->aspect);

  if (op && (prop = RNA_struct_find_property(op->ptr, "center_override")) &&
      RNA_property_is_set(op->ptr, prop)) {
    RNA_property_float_get_array(op->ptr, prop, t->center_global);
    mul_v3_v3(t->center_global, t->aspect);
    t->flag |= T_OVERRIDE_CENTER;
  }

  setTransformViewMatrices(t);
  initNumInput(&t->num);
}

static void freeTransCustomData(TransInfo *t, TransDataContainer *tc, TransCustomData *custom_data)
{
  if (custom_data->free_cb) {
    /* Can take over freeing t->data and data_2d etc... */
    custom_data->free_cb(t, tc, custom_data);
    BLI_assert(custom_data->data == NULL);
  }
  else if ((custom_data->data != NULL) && custom_data->use_free) {
    MEM_freeN(custom_data->data);
    custom_data->data = NULL;
  }
  /* In case modes are switched in the same transform session. */
  custom_data->free_cb = NULL;
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

/**
 * Needed for mode switching.
 */
void freeTransCustomDataForMode(TransInfo *t)
{
  freeTransCustomData(t, NULL, &t->custom.mode);
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    freeTransCustomData(t, tc, &tc->custom.mode);
  }
}

/* Here I would suggest only TransInfo related issues, like free data & reset vars. Not redraws */
void postTrans(bContext *C, TransInfo *t)
{
  if (t->draw_handle_view) {
    ED_region_draw_cb_exit(t->region->type, t->draw_handle_view);
  }
  if (t->draw_handle_apply) {
    ED_region_draw_cb_exit(t->region->type, t->draw_handle_apply);
  }
  if (t->draw_handle_pixel) {
    ED_region_draw_cb_exit(t->region->type, t->draw_handle_pixel);
  }
  if (t->draw_handle_cursor) {
    WM_paint_cursor_end(t->draw_handle_cursor);
  }

  if (t->flag & T_MODAL_CURSOR_SET) {
    WM_cursor_modal_restore(CTX_wm_window(C));
  }

  /* Free all custom-data */
  freeTransCustomDataContainer(t, NULL, &t->custom);
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    freeTransCustomDataContainer(t, tc, &tc->custom);
  }

  /* postTrans can be called when nothing is selected, so data is NULL already */
  if (t->data_len_all != 0) {
    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      /* free data malloced per trans-data */
      if (ELEM(t->obedit_type, OB_CURVE, OB_SURF) || (t->spacetype == SPACE_GRAPH)) {
        TransData *td = tc->data;
        for (int a = 0; a < tc->data_len; a++, td++) {
          if (td->flag & TD_BEZTRIPLE) {
            MEM_freeN(td->hdata);
          }
        }
      }
      MEM_freeN(tc->data);

      MEM_SAFE_FREE(tc->data_ext);
      MEM_SAFE_FREE(tc->data_2d);
      MEM_SAFE_FREE(tc->mirror.data);
    }
  }

  MEM_SAFE_FREE(t->data_container);
  t->data_container = NULL;

  BLI_freelistN(&t->tsnap.points);

  if (t->spacetype == SPACE_IMAGE) {
    if (t->options & (CTX_MASK | CTX_PAINT_CURVE)) {
      /* pass */
    }
    else {
      SpaceImage *sima = t->area->spacedata.first;
      if (sima->flag & SI_LIVE_UNWRAP) {
        ED_uvedit_live_unwrap_end(t->state == TRANS_CANCEL);
      }
    }
  }
  else if (t->spacetype == SPACE_VIEW3D) {
    View3D *v3d = t->area->spacedata.first;
    /* restore gizmo */
    if (t->flag & T_MODAL) {
      v3d->gizmo_flag = t->gizmo_flag;
    }
  }

  if (t->mouse.data) {
    MEM_freeN(t->mouse.data);
  }

  if (t->rng != NULL) {
    BLI_rng_free(t->rng);
  }

  freeSnapping(t);
}

void applyTransObjects(TransInfo *t)
{
  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  TransData *td;

  for (td = tc->data; td < tc->data + tc->data_len; td++) {
    copy_v3_v3(td->iloc, td->loc);
    if (td->ext->rot) {
      copy_v3_v3(td->ext->irot, td->ext->rot);
    }
    if (td->ext->size) {
      copy_v3_v3(td->ext->isize, td->ext->size);
    }
  }
  recalcData(t);
}

static void restoreElement(TransData *td)
{
  /* TransData for crease has no loc */
  if (td->loc) {
    copy_v3_v3(td->loc, td->iloc);
  }
  if (td->val) {
    *td->val = td->ival;
  }

  if (td->ext && (td->flag & TD_NO_EXT) == 0) {
    if (td->ext->rot) {
      copy_v3_v3(td->ext->rot, td->ext->irot);
    }
    if (td->ext->rotAngle) {
      *td->ext->rotAngle = td->ext->irotAngle;
    }
    if (td->ext->rotAxis) {
      copy_v3_v3(td->ext->rotAxis, td->ext->irotAxis);
    }
    /* XXX, drotAngle & drotAxis not used yet */
    if (td->ext->size) {
      copy_v3_v3(td->ext->size, td->ext->isize);
    }
    if (td->ext->quat) {
      copy_qt_qt(td->ext->quat, td->ext->iquat);
    }
  }

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

    for (td = tc->data; td < tc->data + tc->data_len; td++) {
      restoreElement(td);
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

  recalcData(t);
}

void calculateCenter2D(TransInfo *t)
{
  BLI_assert(!is_zero_v3(t->aspect));
  projectFloatView(t, t->center_global, t->center2d);
}

void calculateCenterLocal(TransInfo *t, const float center_global[3])
{
  /* setting constraint center */
  /* note, init functions may over-ride t->center */
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

  /* If edit or pose mode, move cursor in local space */
  if (t->options & CTX_PAINT_CURVE) {
    if (ED_view3d_project_float_global(t->region, cursor, r_center, V3D_PROJ_TEST_NOP) !=
        V3D_PROJ_RET_OK) {
      r_center[0] = t->region->winx / 2.0f;
      r_center[1] = t->region->winy / 2.0f;
    }
    r_center[2] = 0.0f;
  }
}

void calculateCenterCursor2D(TransInfo *t, float r_center[2])
{
  const float *cursor = NULL;

  if (t->spacetype == SPACE_IMAGE) {
    SpaceImage *sima = (SpaceImage *)t->area->spacedata.first;
    cursor = sima->cursor;
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
        BLI_assert(!"Shall not happen");
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

  /* cursor is combination of current frame, and graph-editor cursor value */
  if (sipo->mode == SIPO_MODE_DRIVERS) {
    r_center[0] = sipo->cursorTime;
    r_center[1] = sipo->cursorVal;
  }
  else {
    r_center[0] = (float)(scene->r.cfra);
    r_center[1] = sipo->cursorVal;
  }
}

void calculateCenterMedian(TransInfo *t, float r_center[3])
{
  float partial[3] = {0.0f, 0.0f, 0.0f};
  int total = 0;

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    for (int i = 0; i < tc->data_len; i++) {
      if (tc->data[i].flag & TD_SELECTED) {
        if (!(tc->data[i].flag & TD_NOCENTER)) {
          if (tc->use_local_mat) {
            float v[3];
            mul_v3_m4v3(v, tc->mat, tc->data[i].center);
            add_v3_v3(partial, v);
          }
          else {
            add_v3_v3(partial, tc->data[i].center);
          }
          total++;
        }
      }
    }
  }
  if (total) {
    mul_v3_fl(partial, 1.0f / (float)total);
  }
  copy_v3_v3(r_center, partial);
}

void calculateCenterBound(TransInfo *t, float r_center[3])
{
  float max[3], min[3];
  bool changed = false;
  INIT_MINMAX(min, max);
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    for (int i = 0; i < tc->data_len; i++) {
      if (tc->data[i].flag & TD_SELECTED) {
        if (!(tc->data[i].flag & TD_NOCENTER)) {
          if (tc->use_local_mat) {
            float v[3];
            mul_v3_m4v3(v, tc->mat, tc->data[i].center);
            minmax_v3v3_v3(min, max, v);
          }
          else {
            minmax_v3v3_v3(min, max, tc->data[i].center);
          }
          changed = true;
        }
      }
    }
  }
  if (changed) {
    mid_v3_v3v3(r_center, min, max);
  }
}

/**
 * \param select_only: only get active center from data being transformed.
 */
bool calculateCenterActive(TransInfo *t, bool select_only, float r_center[3])
{
  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_OK(t);

  if (t->spacetype != SPACE_VIEW3D) {
    return false;
  }
  else if (tc->obedit) {
    if (ED_object_calc_active_center_for_editmode(tc->obedit, select_only, r_center)) {
      mul_m4_v3(tc->obedit->obmat, r_center);
      return true;
    }
  }
  else if (t->flag & T_POSE) {
    ViewLayer *view_layer = t->view_layer;
    Object *ob = OBACT(view_layer);
    if (ED_object_calc_active_center_for_posemode(ob, select_only, r_center)) {
      mul_m4_v3(ob->obmat, r_center);
      return true;
    }
  }
  else if (t->options & CTX_PAINT_CURVE) {
    Paint *p = BKE_paint_get_active(t->scene, t->view_layer);
    Brush *br = p->brush;
    PaintCurve *pc = br->paint_curve;
    copy_v3_v3(r_center, pc->points[pc->add_index - 1].bez.vec[1]);
    r_center[2] = 0.0f;
    return true;
  }
  else {
    /* object mode */
    ViewLayer *view_layer = t->view_layer;
    Object *ob = OBACT(view_layer);
    Base *base = BASACT(view_layer);
    if (ob && ((!select_only) || ((base->flag & BASE_SELECTED) != 0))) {
      copy_v3_v3(r_center, ob->obmat[3]);
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
      if (ELEM(t->spacetype, SPACE_IMAGE, SPACE_CLIP)) {
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
      /* Individual element center uses median center for helpline and such */
      calculateCenterMedian(t, r_center);
      break;
    case V3D_AROUND_ACTIVE: {
      if (calculateCenterActive(t, false, r_center)) {
        /* pass */
      }
      else {
        /* fallback */
        calculateCenterMedian(t, r_center);
      }
      break;
    }
  }
}

void calculateCenter(TransInfo *t)
{
  if ((t->flag & T_OVERRIDE_CENTER) == 0) {
    calculateCenter_FromAround(t, t->around, t->center_global);
  }
  calculateCenterLocal(t, t->center_global);

  /* avoid calculating again */
  {
    TransCenterData *cd = &t->center_cache[t->around];
    copy_v3_v3(cd->global, t->center_global);
    cd->is_set = true;
  }

  calculateCenter2D(t);

  /* for panning from cameraview */
  if ((t->flag & T_OBJECT) && (t->flag & T_OVERRIDE_CENTER) == 0) {
    if (t->spacetype == SPACE_VIEW3D && t->region && t->region->regiontype == RGN_TYPE_WINDOW) {

      if (t->flag & T_CAMERA) {
        float axis[3];
        /* persinv is nasty, use viewinv instead, always right */
        copy_v3_v3(axis, t->viewinv[2]);
        normalize_v3(axis);

        /* 6.0 = 6 grid units */
        axis[0] = t->center_global[0] - 6.0f * axis[0];
        axis[1] = t->center_global[1] - 6.0f * axis[1];
        axis[2] = t->center_global[2] - 6.0f * axis[2];

        projectFloatView(t, axis, t->center2d);

        /* rotate only needs correct 2d center, grab needs ED_view3d_calc_zfac() value */
        if (t->mode == TFM_TRANSLATION) {
          copy_v3_v3(t->center_global, axis);
        }
      }
    }
  }

  if (t->spacetype == SPACE_VIEW3D) {
    /* ED_view3d_calc_zfac() defines a factor for perspective depth correction,
     * used in ED_view3d_win_to_delta() */

    /* zfac is only used convertViewVec only in cases operator was invoked in RGN_TYPE_WINDOW
     * and never used in other cases.
     *
     * We need special case here as well, since ED_view3d_calc_zfac will crash when called
     * for a region different from RGN_TYPE_WINDOW.
     */
    if (t->region->regiontype == RGN_TYPE_WINDOW) {
      t->zfac = ED_view3d_calc_zfac(t->region->regiondata, t->center_global, NULL);
    }
    else {
      t->zfac = 0.0f;
    }
  }
}

BLI_STATIC_ASSERT(ARRAY_SIZE(((TransInfo *)NULL)->center_cache) == (V3D_AROUND_ACTIVE + 1),
                  "test size");

/**
 * Lazy initialize transform center data, when we need to access center values from other types.
 */
const TransCenterData *transformCenter_from_type(TransInfo *t, int around)
{
  BLI_assert(around <= V3D_AROUND_ACTIVE);
  TransCenterData *cd = &t->center_cache[around];
  if (cd->is_set == false) {
    calculateCenter_FromAround(t, around, cd->global);
    cd->is_set = true;
  }
  return cd;
}

void calculatePropRatio(TransInfo *t)
{
  int i;
  float dist;
  const bool connected = (t->flag & T_PROP_CONNECTED) != 0;

  t->proptext[0] = '\0';

  if (t->flag & T_PROP_EDIT) {
    const char *pet_id = NULL;
    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      TransData *td = tc->data;
      for (i = 0; i < tc->data_len; i++, td++) {
        if (td->flag & TD_SELECTED) {
          td->factor = 1.0f;
        }
        else if ((connected && (td->flag & TD_NOTCONNECTED || td->dist > t->prop_size)) ||
                 (connected == 0 && td->rdist > t->prop_size)) {
          td->factor = 0.0f;
          restoreElement(td);
        }
        else {
          /* Use rdist for falloff calculations, it is the real distance */
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
          if (dist < 0.0f) {
            dist = 0.0f;
          }

          switch (t->prop_mode) {
            case PROP_SHARP:
              td->factor = dist * dist;
              break;
            case PROP_SMOOTH:
              td->factor = 3.0f * dist * dist - 2.0f * dist * dist * dist;
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
              if (t->rng == NULL) {
                /* Lazy initialization. */
                uint rng_seed = (uint)(PIL_check_seconds_timer_i() & UINT_MAX);
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
      BLI_strncpy(t->proptext, IFACE_(pet_id), sizeof(t->proptext));
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

/**
 * Rotate an element, low level code, ignore protected channels.
 * (use for objects or pose-bones)
 * Similar to #ElementRotation.
 */
void transform_data_ext_rotate(TransData *td, float mat[3][3], bool use_drot)
{
  float totmat[3][3];
  float smat[3][3];
  float fmat[3][3];
  float obmat[3][3];

  float dmat[3][3]; /* delta rotation */
  float dmat_inv[3][3];

  mul_m3_m3m3(totmat, mat, td->mtx);
  mul_m3_m3m3(smat, td->smtx, mat);

  /* logic from BKE_object_rot_to_mat3 */
  if (use_drot) {
    if (td->ext->rotOrder > 0) {
      eulO_to_mat3(dmat, td->ext->drot, td->ext->rotOrder);
    }
    else if (td->ext->rotOrder == ROT_MODE_AXISANGLE) {
#if 0
      axis_angle_to_mat3(dmat, td->ext->drotAxis, td->ext->drotAngle);
#else
      unit_m3(dmat);
#endif
    }
    else {
      float tquat[4];
      normalize_qt_qt(tquat, td->ext->dquat);
      quat_to_mat3(dmat, tquat);
    }

    invert_m3_m3(dmat_inv, dmat);
  }

  if (td->ext->rotOrder == ROT_MODE_QUAT) {
    float quat[4];

    /* calculate the total rotatation */
    quat_to_mat3(obmat, td->ext->iquat);
    if (use_drot) {
      mul_m3_m3m3(obmat, dmat, obmat);
    }

    /* mat = transform, obmat = object rotation */
    mul_m3_m3m3(fmat, smat, obmat);

    if (use_drot) {
      mul_m3_m3m3(fmat, dmat_inv, fmat);
    }

    mat3_to_quat(quat, fmat);

    /* apply */
    copy_qt_qt(td->ext->quat, quat);
  }
  else if (td->ext->rotOrder == ROT_MODE_AXISANGLE) {
    float axis[3], angle;

    /* calculate the total rotatation */
    axis_angle_to_mat3(obmat, td->ext->irotAxis, td->ext->irotAngle);
    if (use_drot) {
      mul_m3_m3m3(obmat, dmat, obmat);
    }

    /* mat = transform, obmat = object rotation */
    mul_m3_m3m3(fmat, smat, obmat);

    if (use_drot) {
      mul_m3_m3m3(fmat, dmat_inv, fmat);
    }

    mat3_to_axis_angle(axis, &angle, fmat);

    /* apply */
    copy_v3_v3(td->ext->rotAxis, axis);
    *td->ext->rotAngle = angle;
  }
  else {
    float eul[3];

    /* calculate the total rotatation */
    eulO_to_mat3(obmat, td->ext->irot, td->ext->rotOrder);
    if (use_drot) {
      mul_m3_m3m3(obmat, dmat, obmat);
    }

    /* mat = transform, obmat = object rotation */
    mul_m3_m3m3(fmat, smat, obmat);

    if (use_drot) {
      mul_m3_m3m3(fmat, dmat_inv, fmat);
    }

    mat3_to_compatible_eulO(eul, td->ext->rot, td->ext->rotOrder, fmat);

    /* apply */
    copy_v3_v3(td->ext->rot, eul);
  }
}
