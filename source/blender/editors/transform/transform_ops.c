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
 * \ingroup edtransform
 */

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_report.h"
#include "BKE_editmesh.h"
#include "BKE_layer.h"
#include "BKE_scene.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_message.h"
#include "WM_types.h"
#include "WM_toolsystem.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "ED_screen.h"
/* for USE_LOOPSLIDE_HACK only */
#include "ED_mesh.h"

#include "transform.h"

typedef struct TransformModeItem {
  const char *idname;
  int mode;
  void (*opfunc)(wmOperatorType *);
} TransformModeItem;

static const float VecOne[3] = {1, 1, 1};

static const char OP_TRANSLATION[] = "TRANSFORM_OT_translate";
static const char OP_ROTATION[] = "TRANSFORM_OT_rotate";
static const char OP_TOSPHERE[] = "TRANSFORM_OT_tosphere";
static const char OP_RESIZE[] = "TRANSFORM_OT_resize";
static const char OP_SKIN_RESIZE[] = "TRANSFORM_OT_skin_resize";
static const char OP_SHEAR[] = "TRANSFORM_OT_shear";
static const char OP_BEND[] = "TRANSFORM_OT_bend";
static const char OP_SHRINK_FATTEN[] = "TRANSFORM_OT_shrink_fatten";
static const char OP_PUSH_PULL[] = "TRANSFORM_OT_push_pull";
static const char OP_TILT[] = "TRANSFORM_OT_tilt";
static const char OP_TRACKBALL[] = "TRANSFORM_OT_trackball";
static const char OP_MIRROR[] = "TRANSFORM_OT_mirror";
static const char OP_EDGE_SLIDE[] = "TRANSFORM_OT_edge_slide";
static const char OP_VERT_SLIDE[] = "TRANSFORM_OT_vert_slide";
static const char OP_EDGE_CREASE[] = "TRANSFORM_OT_edge_crease";
static const char OP_EDGE_BWEIGHT[] = "TRANSFORM_OT_edge_bevelweight";
static const char OP_SEQ_SLIDE[] = "TRANSFORM_OT_seq_slide";
static const char OP_NORMAL_ROTATION[] = "TRANSFORM_OT_rotate_normal";

static void TRANSFORM_OT_translate(struct wmOperatorType *ot);
static void TRANSFORM_OT_rotate(struct wmOperatorType *ot);
static void TRANSFORM_OT_tosphere(struct wmOperatorType *ot);
static void TRANSFORM_OT_resize(struct wmOperatorType *ot);
static void TRANSFORM_OT_skin_resize(struct wmOperatorType *ot);
static void TRANSFORM_OT_shear(struct wmOperatorType *ot);
static void TRANSFORM_OT_bend(struct wmOperatorType *ot);
static void TRANSFORM_OT_shrink_fatten(struct wmOperatorType *ot);
static void TRANSFORM_OT_push_pull(struct wmOperatorType *ot);
static void TRANSFORM_OT_tilt(struct wmOperatorType *ot);
static void TRANSFORM_OT_trackball(struct wmOperatorType *ot);
static void TRANSFORM_OT_mirror(struct wmOperatorType *ot);
static void TRANSFORM_OT_edge_slide(struct wmOperatorType *ot);
static void TRANSFORM_OT_vert_slide(struct wmOperatorType *ot);
static void TRANSFORM_OT_edge_crease(struct wmOperatorType *ot);
static void TRANSFORM_OT_edge_bevelweight(struct wmOperatorType *ot);
static void TRANSFORM_OT_seq_slide(struct wmOperatorType *ot);
static void TRANSFORM_OT_rotate_normal(struct wmOperatorType *ot);

static TransformModeItem transform_modes[] = {
    {OP_TRANSLATION, TFM_TRANSLATION, TRANSFORM_OT_translate},
    {OP_ROTATION, TFM_ROTATION, TRANSFORM_OT_rotate},
    {OP_TOSPHERE, TFM_TOSPHERE, TRANSFORM_OT_tosphere},
    {OP_RESIZE, TFM_RESIZE, TRANSFORM_OT_resize},
    {OP_SKIN_RESIZE, TFM_SKIN_RESIZE, TRANSFORM_OT_skin_resize},
    {OP_SHEAR, TFM_SHEAR, TRANSFORM_OT_shear},
    {OP_BEND, TFM_BEND, TRANSFORM_OT_bend},
    {OP_SHRINK_FATTEN, TFM_SHRINKFATTEN, TRANSFORM_OT_shrink_fatten},
    {OP_PUSH_PULL, TFM_PUSHPULL, TRANSFORM_OT_push_pull},
    {OP_TILT, TFM_TILT, TRANSFORM_OT_tilt},
    {OP_TRACKBALL, TFM_TRACKBALL, TRANSFORM_OT_trackball},
    {OP_MIRROR, TFM_MIRROR, TRANSFORM_OT_mirror},
    {OP_EDGE_SLIDE, TFM_EDGE_SLIDE, TRANSFORM_OT_edge_slide},
    {OP_VERT_SLIDE, TFM_VERT_SLIDE, TRANSFORM_OT_vert_slide},
    {OP_EDGE_CREASE, TFM_CREASE, TRANSFORM_OT_edge_crease},
    {OP_EDGE_BWEIGHT, TFM_BWEIGHT, TRANSFORM_OT_edge_bevelweight},
    {OP_SEQ_SLIDE, TFM_SEQ_SLIDE, TRANSFORM_OT_seq_slide},
    {OP_NORMAL_ROTATION, TFM_NORMAL_ROTATION, TRANSFORM_OT_rotate_normal},
    {NULL, 0},
};

const EnumPropertyItem rna_enum_transform_mode_types[] = {
    {TFM_INIT, "INIT", 0, "Init", ""},
    {TFM_DUMMY, "DUMMY", 0, "Dummy", ""},
    {TFM_TRANSLATION, "TRANSLATION", 0, "Translation", ""},
    {TFM_ROTATION, "ROTATION", 0, "Rotation", ""},
    {TFM_RESIZE, "RESIZE", 0, "Resize", ""},
    {TFM_SKIN_RESIZE, "SKIN_RESIZE", 0, "Skin Resize", ""},
    {TFM_TOSPHERE, "TOSPHERE", 0, "Tosphere", ""},
    {TFM_SHEAR, "SHEAR", 0, "Shear", ""},
    {TFM_BEND, "BEND", 0, "Bend", ""},
    {TFM_SHRINKFATTEN, "SHRINKFATTEN", 0, "Shrinkfatten", ""},
    {TFM_TILT, "TILT", 0, "Tilt", ""},
    {TFM_TRACKBALL, "TRACKBALL", 0, "Trackball", ""},
    {TFM_PUSHPULL, "PUSHPULL", 0, "Pushpull", ""},
    {TFM_CREASE, "CREASE", 0, "Crease", ""},
    {TFM_MIRROR, "MIRROR", 0, "Mirror", ""},
    {TFM_BONESIZE, "BONE_SIZE", 0, "Bonesize", ""},
    {TFM_BONE_ENVELOPE, "BONE_ENVELOPE", 0, "Bone_Envelope", ""},
    {TFM_BONE_ENVELOPE_DIST, "BONE_ENVELOPE_DIST", 0, "Bone_Envelope_Distance", ""},
    {TFM_CURVE_SHRINKFATTEN, "CURVE_SHRINKFATTEN", 0, "Curve_Shrinkfatten", ""},
    {TFM_MASK_SHRINKFATTEN, "MASK_SHRINKFATTEN", 0, "Mask_Shrinkfatten", ""},
    {TFM_GPENCIL_SHRINKFATTEN, "GPENCIL_SHRINKFATTEN", 0, "GPencil_Shrinkfatten", ""},
    {TFM_BONE_ROLL, "BONE_ROLL", 0, "Bone_Roll", ""},
    {TFM_TIME_TRANSLATE, "TIME_TRANSLATE", 0, "Time_Translate", ""},
    {TFM_TIME_SLIDE, "TIME_SLIDE", 0, "Time_Slide", ""},
    {TFM_TIME_SCALE, "TIME_SCALE", 0, "Time_Scale", ""},
    {TFM_TIME_EXTEND, "TIME_EXTEND", 0, "Time_Extend", ""},
    {TFM_BAKE_TIME, "BAKE_TIME", 0, "Bake_Time", ""},
    {TFM_BWEIGHT, "BWEIGHT", 0, "Bweight", ""},
    {TFM_ALIGN, "ALIGN", 0, "Align", ""},
    {TFM_EDGE_SLIDE, "EDGESLIDE", 0, "Edge Slide", ""},
    {TFM_SEQ_SLIDE, "SEQSLIDE", 0, "Sequence Slide", ""},
    {TFM_GPENCIL_OPACITY, "GPENCIL_OPACITY", 0, "GPencil_Opacity", ""},
    {0, NULL, 0, NULL, NULL},
};

static int select_orientation_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  View3D *v3d = CTX_wm_view3d(C);

  int orientation = RNA_enum_get(op->ptr, "orientation");

  BKE_scene_orientation_slot_set_index(&scene->orientation_slots[SCE_ORIENT_DEFAULT], orientation);

  WM_event_add_notifier(C, NC_SCENE | ND_TOOLSETTINGS, NULL);
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, v3d);

  struct wmMsgBus *mbus = CTX_wm_message_bus(C);
  WM_msg_publish_rna_prop(mbus, &scene->id, scene, TransformOrientationSlot, type);

  return OPERATOR_FINISHED;
}

static int select_orientation_invoke(bContext *C,
                                     wmOperator *UNUSED(op),
                                     const wmEvent *UNUSED(event))
{
  uiPopupMenu *pup;
  uiLayout *layout;

  pup = UI_popup_menu_begin(C, IFACE_("Orientation"), ICON_NONE);
  layout = UI_popup_menu_layout(pup);
  uiItemsEnumO(layout, "TRANSFORM_OT_select_orientation", "orientation");
  UI_popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

static void TRANSFORM_OT_select_orientation(struct wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Select Orientation";
  ot->description = "Select transformation orientation";
  ot->idname = "TRANSFORM_OT_select_orientation";
  ot->flag = OPTYPE_UNDO;

  /* api callbacks */
  ot->invoke = select_orientation_invoke;
  ot->exec = select_orientation_exec;
  ot->poll = ED_operator_view3d_active;

  prop = RNA_def_property(ot->srna, "orientation", PROP_ENUM, PROP_NONE);
  RNA_def_property_ui_text(prop, "Orientation", "Transformation orientation");
  RNA_def_enum_funcs(prop, rna_TransformOrientation_itemf);
}

static int delete_orientation_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  BIF_removeTransformOrientationIndex(C,
                                      scene->orientation_slots[SCE_ORIENT_DEFAULT].index_custom);

  WM_event_add_notifier(C, NC_SCENE | NA_EDITED, scene);

  return OPERATOR_FINISHED;
}

static int delete_orientation_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  return delete_orientation_exec(C, op);
}

static bool delete_orientation_poll(bContext *C)
{
  Scene *scene = CTX_data_scene(C);

  if (ED_operator_areaactive(C) == 0) {
    return 0;
  }

  return ((scene->orientation_slots[SCE_ORIENT_DEFAULT].type >= V3D_ORIENT_CUSTOM) &&
          (scene->orientation_slots[SCE_ORIENT_DEFAULT].index_custom != -1));
}

static void TRANSFORM_OT_delete_orientation(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete Orientation";
  ot->description = "Delete transformation orientation";
  ot->idname = "TRANSFORM_OT_delete_orientation";
  ot->flag = OPTYPE_UNDO;

  /* api callbacks */
  ot->invoke = delete_orientation_invoke;
  ot->exec = delete_orientation_exec;
  ot->poll = delete_orientation_poll;
}

static int create_orientation_exec(bContext *C, wmOperator *op)
{
  char name[MAX_NAME];
  const bool use = RNA_boolean_get(op->ptr, "use");
  const bool overwrite = RNA_boolean_get(op->ptr, "overwrite");
  const bool use_view = RNA_boolean_get(op->ptr, "use_view");
  View3D *v3d = CTX_wm_view3d(C);

  RNA_string_get(op->ptr, "name", name);

  if (use && !v3d) {
    BKE_report(op->reports,
               RPT_ERROR,
               "Create Orientation's 'use' parameter only valid in a 3DView context");
    return OPERATOR_CANCELLED;
  }

  BIF_createTransformOrientation(C, op->reports, name, use_view, use, overwrite);

  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, v3d);
  WM_event_add_notifier(C, NC_SCENE | NA_EDITED, CTX_data_scene(C));

  return OPERATOR_FINISHED;
}

static void TRANSFORM_OT_create_orientation(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Create Orientation";
  ot->description = "Create transformation orientation from selection";
  ot->idname = "TRANSFORM_OT_create_orientation";
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* api callbacks */
  ot->exec = create_orientation_exec;
  ot->poll = ED_operator_areaactive;

  RNA_def_string(ot->srna, "name", NULL, MAX_NAME, "Name", "Name of the new custom orientation");
  RNA_def_boolean(
      ot->srna,
      "use_view",
      false,
      "Use View",
      "Use the current view instead of the active object to create the new orientation");

  WM_operatortype_props_advanced_begin(ot);

  RNA_def_boolean(
      ot->srna, "use", false, "Use after creation", "Select orientation after its creation");
  RNA_def_boolean(ot->srna,
                  "overwrite",
                  false,
                  "Overwrite previous",
                  "Overwrite previously created orientation with same name");
}

#ifdef USE_LOOPSLIDE_HACK
/**
 * Special hack for MESH_OT_loopcut_slide so we get back to the selection mode
 */
static void transformops_loopsel_hack(bContext *C, wmOperator *op)
{
  if (op->type->idname == OP_EDGE_SLIDE) {
    if (op->opm && op->opm->opm && op->opm->opm->prev) {
      wmOperator *op_prev = op->opm->opm->prev;
      Scene *scene = CTX_data_scene(C);
      bool mesh_select_mode[3];
      PropertyRNA *prop = RNA_struct_find_property(op_prev->ptr, "mesh_select_mode_init");

      if (prop && RNA_property_is_set(op_prev->ptr, prop)) {
        ToolSettings *ts = scene->toolsettings;
        short selectmode_orig;

        RNA_property_boolean_get_array(op_prev->ptr, prop, mesh_select_mode);
        selectmode_orig = ((mesh_select_mode[0] ? SCE_SELECT_VERTEX : 0) |
                           (mesh_select_mode[1] ? SCE_SELECT_EDGE : 0) |
                           (mesh_select_mode[2] ? SCE_SELECT_FACE : 0));

        /* still switch if we were originally in face select mode */
        if ((ts->selectmode != selectmode_orig) && (selectmode_orig != SCE_SELECT_FACE)) {
          Object *obedit = CTX_data_edit_object(C);
          BMEditMesh *em = BKE_editmesh_from_object(obedit);
          em->selectmode = ts->selectmode = selectmode_orig;
          EDBM_selectmode_set(em);
        }
      }
    }
  }
}
#else
/* prevent removal by cleanup */
#  error "loopslide hack removed!"
#endif /* USE_LOOPSLIDE_HACK */

static void transformops_exit(bContext *C, wmOperator *op)
{
#ifdef USE_LOOPSLIDE_HACK
  transformops_loopsel_hack(C, op);
#endif

  saveTransform(C, op->customdata, op);
  MEM_freeN(op->customdata);
  op->customdata = NULL;
  G.moving = 0;
}

static int transformops_data(bContext *C, wmOperator *op, const wmEvent *event)
{
  int retval = 1;
  if (op->customdata == NULL) {
    TransInfo *t = MEM_callocN(sizeof(TransInfo), "TransInfo data2");
    TransformModeItem *tmode;
    int mode = -1;

    for (tmode = transform_modes; tmode->idname; tmode++) {
      if (op->type->idname == tmode->idname) {
        mode = tmode->mode;
        break;
      }
    }

    if (mode == -1) {
      mode = RNA_enum_get(op->ptr, "mode");
    }

    retval = initTransform(C, t, op, event, mode);

    /* store data */
    if (retval) {
      G.moving = special_transform_moving(t);
      op->customdata = t;
    }
    else {
      MEM_freeN(t);
    }
  }

  return retval; /* return 0 on error */
}

static int transform_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  int exit_code;

  TransInfo *t = op->customdata;
  const enum TfmMode mode_prev = t->mode;

#if defined(WITH_INPUT_NDOF) && 0
  // stable 2D mouse coords map to different 3D coords while the 3D mouse is active
  // in other words, 2D deltas are no longer good enough!
  // disable until individual 'transformers' behave better

  if (event->type == NDOF_MOTION) {
    return OPERATOR_PASS_THROUGH;
  }
#endif

  /* XXX insert keys are called here, and require context */
  t->context = C;
  exit_code = transformEvent(t, event);
  t->context = NULL;

  /* XXX, workaround: active needs to be calculated before transforming,
   * since we're not reading from 'td->center' in this case. see: T40241 */
  if (t->tsnap.target == SCE_SNAP_TARGET_ACTIVE) {
    /* In camera view, tsnap callback is not set
     * (see initSnappingMode() in transfrom_snap.c, and T40348). */
    if (t->tsnap.targetSnap && ((t->tsnap.status & TARGET_INIT) == 0)) {
      t->tsnap.targetSnap(t);
    }
  }

  transformApply(C, t);

  exit_code |= transformEnd(C, t);

  if ((exit_code & OPERATOR_RUNNING_MODAL) == 0) {
    transformops_exit(C, op);
    exit_code &= ~OPERATOR_PASS_THROUGH; /* preventively remove passthrough */
  }
  else {
    if (mode_prev != t->mode) {
      /* WARNING: this is not normal to switch operator types
       * normally it would not be supported but transform happens
       * to share callbacks between different operators. */
      wmOperatorType *ot_new = NULL;
      TransformModeItem *item = transform_modes;
      while (item->idname) {
        if (item->mode == t->mode) {
          ot_new = WM_operatortype_find(item->idname, false);
          break;
        }
        item++;
      }

      BLI_assert(ot_new != NULL);
      if (ot_new) {
        WM_operator_type_set(op, ot_new);
      }
      /* end suspicious code */
    }
  }

  return exit_code;
}

static void transform_cancel(bContext *C, wmOperator *op)
{
  TransInfo *t = op->customdata;

  t->state = TRANS_CANCEL;
  transformEnd(C, t);
  transformops_exit(C, op);
}

static int transform_exec(bContext *C, wmOperator *op)
{
  TransInfo *t;

  if (!transformops_data(C, op, NULL)) {
    G.moving = 0;
    return OPERATOR_CANCELLED;
  }

  t = op->customdata;

  t->options |= CTX_AUTOCONFIRM;

  transformApply(C, t);

  transformEnd(C, t);

  transformops_exit(C, op);

  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);

  return OPERATOR_FINISHED;
}

static int transform_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (!transformops_data(C, op, event)) {
    G.moving = 0;
    return OPERATOR_CANCELLED;
  }

  /* When modal, allow 'value' to set initial offset. */
  if ((event == NULL) && RNA_struct_property_is_set(op->ptr, "value")) {
    return transform_exec(C, op);
  }
  else {
    /* add temp handler */
    WM_event_add_modal_handler(C, op);

    op->flag |= OP_IS_MODAL_GRAB_CURSOR;  // XXX maybe we want this with the gizmo only?

    /* Use when modal input has some transformation to begin with. */
    {
      TransInfo *t = op->customdata;
      if (UNLIKELY(!is_zero_v4(t->values_modal_offset))) {
        transformApply(C, t);
      }
    }

    return OPERATOR_RUNNING_MODAL;
  }
}

static bool transform_poll_property(const bContext *UNUSED(C),
                                    wmOperator *op,
                                    const PropertyRNA *prop)
{
  const char *prop_id = RNA_property_identifier(prop);

  /* Orientation/Constraints. */
  {
    /* Hide orientation axis if no constraints are set, since it wont be used. */
    PropertyRNA *prop_con = RNA_struct_find_property(op->ptr, "orient_type");
    if (prop_con != NULL && (prop_con != prop)) {
      if (STRPREFIX(prop_id, "constraint")) {

        /* Special case: show constraint axis if we don't have values,
         * needed for mirror operator. */
        if (STREQ(prop_id, "constraint_axis") &&
            (RNA_struct_find_property(op->ptr, "value") == NULL)) {
          return true;
        }

        return false;
      }
    }
  }

  /* Proportional Editing. */
  {
    PropertyRNA *prop_pet = RNA_struct_find_property(op->ptr, "use_proportional_edit");
    if (prop_pet && (prop_pet != prop) && (RNA_property_boolean_get(op->ptr, prop_pet) == false)) {
      if (STRPREFIX(prop_id, "proportional") || STRPREFIX(prop_id, "use_proportional")) {
        return false;
      }
    }
  }

  return true;
}

void Transform_Properties(struct wmOperatorType *ot, int flags)
{
  PropertyRNA *prop;

  if (flags & P_ORIENT_AXIS) {
    prop = RNA_def_property(ot->srna, "orient_axis", PROP_ENUM, PROP_NONE);
    RNA_def_property_ui_text(prop, "Axis", "");
    RNA_def_property_enum_default(prop, 2);
    RNA_def_property_enum_items(prop, rna_enum_axis_xyz_items);
    RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  }
  if (flags & P_ORIENT_AXIS_ORTHO) {
    prop = RNA_def_property(ot->srna, "orient_axis_ortho", PROP_ENUM, PROP_NONE);
    RNA_def_property_ui_text(prop, "Axis Ortho", "");
    RNA_def_property_enum_default(prop, 1);
    RNA_def_property_enum_items(prop, rna_enum_axis_xyz_items);
    RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  }

  if (flags & P_ORIENT_MATRIX) {
    prop = RNA_def_property(ot->srna, "orient_type", PROP_ENUM, PROP_NONE);
    RNA_def_property_ui_text(prop, "Orientation", "Transformation orientation");
    RNA_def_enum_funcs(prop, rna_TransformOrientation_itemf);

    /* Set by 'orient_type' or gizmo which acts on non-standard orientation. */
    prop = RNA_def_float_matrix(
        ot->srna, "orient_matrix", 3, 3, NULL, 0.0f, 0.0f, "Matrix", "", 0.0f, 0.0f);
    RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

    /* Only use 'orient_matrix' when 'orient_matrix_type == orient_type',
     * this allows us to reuse the orientation set by a gizmo for eg, without disabling the ability
     * to switch over to other orientations. */
    prop = RNA_def_property(ot->srna, "orient_matrix_type", PROP_ENUM, PROP_NONE);
    RNA_def_property_ui_text(prop, "Matrix Orientation", "");
    RNA_def_enum_funcs(prop, rna_TransformOrientation_itemf);
    RNA_def_property_flag(prop, PROP_HIDDEN);
  }

  if (flags & P_CONSTRAINT) {
    RNA_def_boolean_vector(ot->srna, "constraint_axis", 3, NULL, "Constraint Axis", "");
  }

  if (flags & P_MIRROR) {
    prop = RNA_def_boolean(ot->srna, "mirror", 0, "Mirror Editing", "");
    if (flags & P_MIRROR_DUMMY) {
      /* only used so macros can disable this option */
      RNA_def_property_flag(prop, PROP_HIDDEN);
    }
  }

  if (flags & P_PROPORTIONAL) {
    RNA_def_boolean(ot->srna, "use_proportional_edit", 0, "Proportional Editing", "");
    prop = RNA_def_enum(ot->srna,
                        "proportional_edit_falloff",
                        rna_enum_proportional_falloff_items,
                        0,
                        "Proportional Falloff",
                        "Falloff type for proportional editing mode");
    /* Abusing id_curve :/ */
    RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_CURVE);
    RNA_def_float(ot->srna,
                  "proportional_size",
                  1,
                  T_PROP_SIZE_MIN,
                  T_PROP_SIZE_MAX,
                  "Proportional Size",
                  "",
                  0.001f,
                  100.0f);

    RNA_def_boolean(ot->srna, "use_proportional_connected", 0, "Connected", "");
    RNA_def_boolean(ot->srna, "use_proportional_projected", 0, "Projected (2D)", "");
  }

  if (flags & P_SNAP) {
    prop = RNA_def_boolean(ot->srna, "snap", 0, "Use Snapping Options", "");
    RNA_def_property_flag(prop, PROP_HIDDEN);

    if (flags & P_GEO_SNAP) {
      prop = RNA_def_enum(ot->srna, "snap_target", rna_enum_snap_target_items, 0, "Target", "");
      RNA_def_property_flag(prop, PROP_HIDDEN);
      prop = RNA_def_float_vector(
          ot->srna, "snap_point", 3, NULL, -FLT_MAX, FLT_MAX, "Point", "", -FLT_MAX, FLT_MAX);
      RNA_def_property_flag(prop, PROP_HIDDEN);

      if (flags & P_ALIGN_SNAP) {
        prop = RNA_def_boolean(ot->srna, "snap_align", 0, "Align with Point Normal", "");
        RNA_def_property_flag(prop, PROP_HIDDEN);
        prop = RNA_def_float_vector(
            ot->srna, "snap_normal", 3, NULL, -FLT_MAX, FLT_MAX, "Normal", "", -FLT_MAX, FLT_MAX);
        RNA_def_property_flag(prop, PROP_HIDDEN);
      }
    }
  }

  if (flags & P_GPENCIL_EDIT) {
    prop = RNA_def_boolean(ot->srna,
                           "gpencil_strokes",
                           0,
                           "Edit Grease Pencil",
                           "Edit selected Grease Pencil strokes");
    RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  }

  if (flags & P_CURSOR_EDIT) {
    prop = RNA_def_boolean(ot->srna, "cursor_transform", 0, "Transform Cursor", "");
    RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  }

  if ((flags & P_OPTIONS) && !(flags & P_NO_TEXSPACE)) {
    prop = RNA_def_boolean(
        ot->srna, "texture_space", 0, "Edit Texture Space", "Edit Object data texture space");
    RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
    prop = RNA_def_boolean(
        ot->srna, "remove_on_cancel", 0, "Remove on Cancel", "Remove elements on cancel");
    RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  }

  if (flags & P_CORRECT_UV) {
    RNA_def_boolean(
        ot->srna, "correct_uv", true, "Correct UVs", "Correct UV coordinates when transforming");
  }

  if (flags & P_CENTER) {
    /* For gizmos that define their own center. */
    prop = RNA_def_property(ot->srna, "center_override", PROP_FLOAT, PROP_XYZ);
    RNA_def_property_array(prop, 3);
    RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
    RNA_def_property_ui_text(prop, "Center Override", "Force using this center value (when set)");
  }

  if ((flags & P_NO_DEFAULTS) == 0) {
    prop = RNA_def_boolean(ot->srna,
                           "release_confirm",
                           0,
                           "Confirm on Release",
                           "Always confirm operation when releasing button");
    RNA_def_property_flag(prop, PROP_HIDDEN);

    prop = RNA_def_boolean(ot->srna, "use_accurate", 0, "Accurate", "Use accurate transformation");
    RNA_def_property_flag(prop, PROP_HIDDEN);
  }
}

static void TRANSFORM_OT_translate(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Move";
  ot->description = "Move selected items";
  ot->idname = OP_TRANSLATION;
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* api callbacks */
  ot->invoke = transform_invoke;
  ot->exec = transform_exec;
  ot->modal = transform_modal;
  ot->cancel = transform_cancel;
  ot->poll = ED_operator_screenactive;
  ot->poll_property = transform_poll_property;

  RNA_def_float_vector_xyz(
      ot->srna, "value", 3, NULL, -FLT_MAX, FLT_MAX, "Move", "", -FLT_MAX, FLT_MAX);

  WM_operatortype_props_advanced_begin(ot);

  Transform_Properties(ot,
                       P_ORIENT_MATRIX | P_CONSTRAINT | P_PROPORTIONAL | P_MIRROR | P_ALIGN_SNAP |
                           P_OPTIONS | P_GPENCIL_EDIT | P_CURSOR_EDIT);
}

static void TRANSFORM_OT_resize(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Resize";
  ot->description = "Scale (resize) selected items";
  ot->idname = OP_RESIZE;
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* api callbacks */
  ot->invoke = transform_invoke;
  ot->exec = transform_exec;
  ot->modal = transform_modal;
  ot->cancel = transform_cancel;
  ot->poll = ED_operator_screenactive;
  ot->poll_property = transform_poll_property;

  RNA_def_float_vector(
      ot->srna, "value", 3, VecOne, -FLT_MAX, FLT_MAX, "Scale", "", -FLT_MAX, FLT_MAX);

  WM_operatortype_props_advanced_begin(ot);

  Transform_Properties(ot,
                       P_ORIENT_MATRIX | P_CONSTRAINT | P_PROPORTIONAL | P_MIRROR | P_GEO_SNAP |
                           P_OPTIONS | P_GPENCIL_EDIT | P_CENTER);
}

static bool skin_resize_poll(bContext *C)
{
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      CTX_data_view_layer(C), CTX_wm_view3d(C), &objects_len);

  bool ok = false;

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    if (obedit->type == OB_MESH) {
      BMEditMesh *em = BKE_editmesh_from_object(obedit);
      if (em && CustomData_has_layer(&em->bm->vdata, CD_MVERT_SKIN)) {
        ok = true;
      }
    }
  }
  MEM_freeN(objects);

  return ok;
}

static void TRANSFORM_OT_skin_resize(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Skin Resize";
  ot->description = "Scale selected vertices' skin radii";
  ot->idname = OP_SKIN_RESIZE;
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* api callbacks */
  ot->invoke = transform_invoke;
  ot->exec = transform_exec;
  ot->modal = transform_modal;
  ot->cancel = transform_cancel;
  ot->poll = skin_resize_poll;
  ot->poll_property = transform_poll_property;

  RNA_def_float_vector(
      ot->srna, "value", 3, VecOne, -FLT_MAX, FLT_MAX, "Scale", "", -FLT_MAX, FLT_MAX);

  WM_operatortype_props_advanced_begin(ot);

  Transform_Properties(ot,
                       P_ORIENT_MATRIX | P_CONSTRAINT | P_PROPORTIONAL | P_MIRROR | P_GEO_SNAP |
                           P_OPTIONS | P_NO_TEXSPACE);
}

static void TRANSFORM_OT_trackball(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Trackball";
  ot->description = "Trackball style rotation of selected items";
  ot->idname = OP_TRACKBALL;
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* api callbacks */
  ot->invoke = transform_invoke;
  ot->exec = transform_exec;
  ot->modal = transform_modal;
  ot->cancel = transform_cancel;
  ot->poll = ED_operator_screenactive;
  ot->poll_property = transform_poll_property;

  /* Maybe we could use float_vector_xyz here too? */
  RNA_def_float_rotation(
      ot->srna, "value", 2, NULL, -FLT_MAX, FLT_MAX, "Angle", "", -FLT_MAX, FLT_MAX);

  WM_operatortype_props_advanced_begin(ot);

  Transform_Properties(ot, P_PROPORTIONAL | P_MIRROR | P_SNAP | P_GPENCIL_EDIT | P_CENTER);
}

static void TRANSFORM_OT_rotate(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Rotate";
  ot->description = "Rotate selected items";
  ot->idname = OP_ROTATION;
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* api callbacks */
  ot->invoke = transform_invoke;
  ot->exec = transform_exec;
  ot->modal = transform_modal;
  ot->cancel = transform_cancel;
  ot->poll = ED_operator_screenactive;
  ot->poll_property = transform_poll_property;

  RNA_def_float_rotation(
      ot->srna, "value", 0, NULL, -FLT_MAX, FLT_MAX, "Angle", "", -M_PI * 2, M_PI * 2);

  WM_operatortype_props_advanced_begin(ot);

  Transform_Properties(ot,
                       P_ORIENT_AXIS | P_ORIENT_MATRIX | P_CONSTRAINT | P_PROPORTIONAL | P_MIRROR |
                           P_GEO_SNAP | P_GPENCIL_EDIT | P_CENTER);
}

static void TRANSFORM_OT_tilt(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Tilt";
  /* optional -
   * "Tilt selected vertices"
   * "Specify an extra axis rotation for selected vertices of 3D curve" */
  ot->description = "Tilt selected control vertices of 3D curve";
  ot->idname = OP_TILT;
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* api callbacks */
  ot->invoke = transform_invoke;
  ot->exec = transform_exec;
  ot->modal = transform_modal;
  ot->cancel = transform_cancel;
  ot->poll = ED_operator_editcurve_3d;
  ot->poll_property = transform_poll_property;

  RNA_def_float_rotation(
      ot->srna, "value", 0, NULL, -FLT_MAX, FLT_MAX, "Angle", "", -M_PI * 2, M_PI * 2);

  WM_operatortype_props_advanced_begin(ot);

  Transform_Properties(ot, P_PROPORTIONAL | P_MIRROR | P_SNAP);
}

static void TRANSFORM_OT_bend(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Bend";
  ot->description = "Bend selected items between the 3D cursor and the mouse";
  ot->idname = OP_BEND;
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* api callbacks */
  ot->invoke = transform_invoke;
  // ot->exec   = transform_exec;  // unsupported
  ot->modal = transform_modal;
  ot->cancel = transform_cancel;
  ot->poll = ED_operator_region_view3d_active;
  ot->poll_property = transform_poll_property;

  RNA_def_float_rotation(
      ot->srna, "value", 1, NULL, -FLT_MAX, FLT_MAX, "Angle", "", -M_PI * 2, M_PI * 2);

  WM_operatortype_props_advanced_begin(ot);

  Transform_Properties(ot, P_PROPORTIONAL | P_MIRROR | P_SNAP | P_GPENCIL_EDIT | P_CENTER);
}

static void TRANSFORM_OT_shear(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Shear";
  ot->description = "Shear selected items along the horizontal screen axis";
  ot->idname = OP_SHEAR;
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* api callbacks */
  ot->invoke = transform_invoke;
  ot->exec = transform_exec;
  ot->modal = transform_modal;
  ot->cancel = transform_cancel;
  ot->poll = ED_operator_screenactive;
  ot->poll_property = transform_poll_property;

  RNA_def_float(ot->srna, "value", 0, -FLT_MAX, FLT_MAX, "Offset", "", -FLT_MAX, FLT_MAX);
  RNA_def_enum(ot->srna, "shear_axis", rna_enum_axis_xy_items, 0, "Shear Axis", "");

  WM_operatortype_props_advanced_begin(ot);

  Transform_Properties(ot,
                       P_ORIENT_AXIS | P_ORIENT_AXIS_ORTHO | P_ORIENT_MATRIX | P_PROPORTIONAL |
                           P_MIRROR | P_SNAP | P_GPENCIL_EDIT);
}

static void TRANSFORM_OT_push_pull(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Push/Pull";
  ot->description = "Push/Pull selected items";
  ot->idname = OP_PUSH_PULL;
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* api callbacks */
  ot->invoke = transform_invoke;
  ot->exec = transform_exec;
  ot->modal = transform_modal;
  ot->cancel = transform_cancel;
  ot->poll = ED_operator_screenactive;
  ot->poll_property = transform_poll_property;

  RNA_def_float(ot->srna, "value", 0, -FLT_MAX, FLT_MAX, "Distance", "", -FLT_MAX, FLT_MAX);

  WM_operatortype_props_advanced_begin(ot);

  Transform_Properties(ot, P_PROPORTIONAL | P_MIRROR | P_SNAP | P_CENTER);
}

static void TRANSFORM_OT_shrink_fatten(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Shrink/Fatten";
  ot->description = "Shrink/fatten selected vertices along normals";
  ot->idname = OP_SHRINK_FATTEN;
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* api callbacks */
  ot->invoke = transform_invoke;
  ot->exec = transform_exec;
  ot->modal = transform_modal;
  ot->cancel = transform_cancel;
  ot->poll = ED_operator_editmesh;
  ot->poll_property = transform_poll_property;

  RNA_def_float_distance(ot->srna, "value", 0, -FLT_MAX, FLT_MAX, "Offset", "", -FLT_MAX, FLT_MAX);

  RNA_def_boolean(ot->srna,
                  "use_even_offset",
                  false,
                  "Offset Even",
                  "Scale the offset to give more even thickness");

  WM_operatortype_props_advanced_begin(ot);

  Transform_Properties(ot, P_PROPORTIONAL | P_MIRROR | P_SNAP);
}

static void TRANSFORM_OT_tosphere(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "To Sphere";
  // added "around mesh center" to differentiate between "MESH_OT_vertices_to_sphere()"
  ot->description = "Move selected vertices outward in a spherical shape around mesh center";
  ot->idname = OP_TOSPHERE;
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* api callbacks */
  ot->invoke = transform_invoke;
  ot->exec = transform_exec;
  ot->modal = transform_modal;
  ot->cancel = transform_cancel;
  ot->poll = ED_operator_screenactive;
  ot->poll_property = transform_poll_property;

  RNA_def_float_factor(ot->srna, "value", 0, 0, 1, "Factor", "", 0, 1);

  WM_operatortype_props_advanced_begin(ot);

  Transform_Properties(ot, P_PROPORTIONAL | P_MIRROR | P_SNAP | P_GPENCIL_EDIT | P_CENTER);
}

static void TRANSFORM_OT_mirror(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Mirror";
  ot->description = "Mirror selected items around one or more axes";
  ot->idname = OP_MIRROR;
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* api callbacks */
  ot->invoke = transform_invoke;
  ot->exec = transform_exec;
  ot->modal = transform_modal;
  ot->cancel = transform_cancel;
  ot->poll = ED_operator_screenactive;
  ot->poll_property = transform_poll_property;

  Transform_Properties(
      ot, P_ORIENT_MATRIX | P_CONSTRAINT | P_PROPORTIONAL | P_GPENCIL_EDIT | P_CENTER);
}

static void TRANSFORM_OT_edge_slide(struct wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Edge Slide";
  ot->description = "Slide an edge loop along a mesh";
  ot->idname = OP_EDGE_SLIDE;
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* api callbacks */
  ot->invoke = transform_invoke;
  ot->exec = transform_exec;
  ot->modal = transform_modal;
  ot->cancel = transform_cancel;
  ot->poll = ED_operator_editmesh_region_view3d;
  ot->poll_property = transform_poll_property;

  RNA_def_float_factor(ot->srna, "value", 0, -10.0f, 10.0f, "Factor", "", -1.0f, 1.0f);

  prop = RNA_def_boolean(ot->srna, "single_side", false, "Single Side", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  RNA_def_boolean(ot->srna,
                  "use_even",
                  false,
                  "Even",
                  "Make the edge loop match the shape of the adjacent edge loop");

  WM_operatortype_props_advanced_begin(ot);

  RNA_def_boolean(ot->srna,
                  "flipped",
                  false,
                  "Flipped",
                  "When Even mode is active, flips between the two adjacent edge loops");
  RNA_def_boolean(ot->srna, "use_clamp", true, "Clamp", "Clamp within the edge extents");

  Transform_Properties(ot, P_MIRROR | P_SNAP | P_CORRECT_UV);
}

static void TRANSFORM_OT_vert_slide(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Vertex Slide";
  ot->description = "Slide a vertex along a mesh";
  ot->idname = OP_VERT_SLIDE;
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* api callbacks */
  ot->invoke = transform_invoke;
  ot->exec = transform_exec;
  ot->modal = transform_modal;
  ot->cancel = transform_cancel;
  ot->poll = ED_operator_editmesh_region_view3d;
  ot->poll_property = transform_poll_property;

  RNA_def_float_factor(ot->srna, "value", 0, -10.0f, 10.0f, "Factor", "", -1.0f, 1.0f);
  RNA_def_boolean(ot->srna,
                  "use_even",
                  false,
                  "Even",
                  "Make the edge loop match the shape of the adjacent edge loop");

  WM_operatortype_props_advanced_begin(ot);

  RNA_def_boolean(ot->srna,
                  "flipped",
                  false,
                  "Flipped",
                  "When Even mode is active, flips between the two adjacent edge loops");
  RNA_def_boolean(ot->srna, "use_clamp", true, "Clamp", "Clamp within the edge extents");

  Transform_Properties(ot, P_MIRROR | P_SNAP | P_CORRECT_UV);
}

static void TRANSFORM_OT_edge_crease(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Edge Crease";
  ot->description = "Change the crease of edges";
  ot->idname = OP_EDGE_CREASE;
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* api callbacks */
  ot->invoke = transform_invoke;
  ot->exec = transform_exec;
  ot->modal = transform_modal;
  ot->cancel = transform_cancel;
  ot->poll = ED_operator_editmesh;
  ot->poll_property = transform_poll_property;

  RNA_def_float_factor(ot->srna, "value", 0, -1.0f, 1.0f, "Factor", "", -1.0f, 1.0f);

  WM_operatortype_props_advanced_begin(ot);

  Transform_Properties(ot, P_SNAP);
}

static void TRANSFORM_OT_edge_bevelweight(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Edge Bevel Weight";
  ot->description = "Change the bevel weight of edges";
  ot->idname = OP_EDGE_BWEIGHT;
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* api callbacks */
  ot->invoke = transform_invoke;
  ot->exec = transform_exec;
  ot->modal = transform_modal;
  ot->cancel = transform_cancel;
  ot->poll = ED_operator_editmesh;

  RNA_def_float_factor(ot->srna, "value", 0, -1.0f, 1.0f, "Factor", "", -1.0f, 1.0f);

  WM_operatortype_props_advanced_begin(ot);

  Transform_Properties(ot, P_SNAP);
}

static void TRANSFORM_OT_seq_slide(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Sequence Slide";
  ot->description = "Slide a sequence strip in time";
  ot->idname = OP_SEQ_SLIDE;
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* api callbacks */
  ot->invoke = transform_invoke;
  ot->exec = transform_exec;
  ot->modal = transform_modal;
  ot->cancel = transform_cancel;
  ot->poll = ED_operator_sequencer_active;

  RNA_def_float_vector_xyz(
      ot->srna, "value", 2, NULL, -FLT_MAX, FLT_MAX, "Offset", "", -FLT_MAX, FLT_MAX);

  WM_operatortype_props_advanced_begin(ot);

  Transform_Properties(ot, P_SNAP);
}

static void TRANSFORM_OT_rotate_normal(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Rotate Normals";
  ot->description = "Rotate split normal of selected items";
  ot->idname = OP_NORMAL_ROTATION;
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* api callbacks */
  ot->invoke = transform_invoke;
  ot->exec = transform_exec;
  ot->modal = transform_modal;
  ot->cancel = transform_cancel;
  ot->poll = ED_operator_editmesh;

  RNA_def_float_rotation(
      ot->srna, "value", 0, NULL, -FLT_MAX, FLT_MAX, "Angle", "", -M_PI * 2, M_PI * 2);

  Transform_Properties(ot, P_ORIENT_AXIS | P_ORIENT_MATRIX | P_CONSTRAINT | P_MIRROR);
}

static void TRANSFORM_OT_transform(struct wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Transform";
  ot->description = "Transform selected items by mode type";
  ot->idname = "TRANSFORM_OT_transform";
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* api callbacks */
  ot->invoke = transform_invoke;
  ot->exec = transform_exec;
  ot->modal = transform_modal;
  ot->cancel = transform_cancel;
  ot->poll = ED_operator_screenactive;
  ot->poll_property = transform_poll_property;

  prop = RNA_def_enum(
      ot->srna, "mode", rna_enum_transform_mode_types, TFM_TRANSLATION, "Mode", "");
  RNA_def_property_flag(prop, PROP_HIDDEN);

  RNA_def_float_vector(
      ot->srna, "value", 4, NULL, -FLT_MAX, FLT_MAX, "Values", "", -FLT_MAX, FLT_MAX);

  WM_operatortype_props_advanced_begin(ot);

  Transform_Properties(ot,
                       P_ORIENT_AXIS | P_ORIENT_MATRIX | P_CONSTRAINT | P_PROPORTIONAL | P_MIRROR |
                           P_ALIGN_SNAP | P_GPENCIL_EDIT | P_CENTER);
}

static int transform_from_gizmo_invoke(bContext *C,
                                       wmOperator *UNUSED(op),
                                       const wmEvent *UNUSED(event))
{
  bToolRef *tref = WM_toolsystem_ref_from_context(C);
  if (tref) {
    ARegion *ar = CTX_wm_region(C);
    wmGizmoMap *gzmap = ar->gizmo_map;
    wmGizmoGroup *gzgroup = gzmap ? WM_gizmomap_group_find(gzmap, "VIEW3D_GGT_xform_gizmo") : NULL;
    if (gzgroup != NULL) {
      PointerRNA gzg_ptr;
      WM_toolsystem_ref_properties_ensure_from_gizmo_group(tref, gzgroup->type, &gzg_ptr);
      const int drag_action = RNA_enum_get(&gzg_ptr, "drag_action");
      const char *op_id = NULL;
      switch (drag_action) {
        case V3D_GIZMO_SHOW_OBJECT_TRANSLATE:
          op_id = "TRANSFORM_OT_translate";
          break;
        case V3D_GIZMO_SHOW_OBJECT_ROTATE:
          op_id = "TRANSFORM_OT_rotate";
          break;
        case V3D_GIZMO_SHOW_OBJECT_SCALE:
          op_id = "TRANSFORM_OT_resize";
          break;
        default:
          break;
      }
      if (op_id) {
        wmOperatorType *ot = WM_operatortype_find(op_id, true);
        PointerRNA op_ptr;
        WM_operator_properties_create_ptr(&op_ptr, ot);
        RNA_boolean_set(&op_ptr, "release_confirm", true);
        WM_operator_name_call_ptr(C, ot, WM_OP_INVOKE_DEFAULT, &op_ptr);
        WM_operator_properties_free(&op_ptr);
        return OPERATOR_FINISHED;
      }
    }
  }
  return OPERATOR_PASS_THROUGH;
}

/* Use with 'TRANSFORM_GGT_gizmo'. */
static void TRANSFORM_OT_from_gizmo(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Transform From Gizmo";
  ot->description = "Transform selected items by mode type";
  ot->idname = "TRANSFORM_OT_from_gizmo";
  ot->flag = 0;

  /* api callbacks */
  ot->invoke = transform_from_gizmo_invoke;
}

void transform_operatortypes(void)
{
  TransformModeItem *tmode;

  for (tmode = transform_modes; tmode->idname; tmode++) {
    WM_operatortype_append(tmode->opfunc);
  }

  WM_operatortype_append(TRANSFORM_OT_transform);

  WM_operatortype_append(TRANSFORM_OT_select_orientation);
  WM_operatortype_append(TRANSFORM_OT_create_orientation);
  WM_operatortype_append(TRANSFORM_OT_delete_orientation);

  WM_operatortype_append(TRANSFORM_OT_from_gizmo);
}

void ED_keymap_transform(wmKeyConfig *keyconf)
{
  wmKeyMap *modalmap = transform_modal_keymap(keyconf);

  TransformModeItem *tmode;

  for (tmode = transform_modes; tmode->idname; tmode++) {
    WM_modalkeymap_assign(modalmap, tmode->idname);
  }
  WM_modalkeymap_assign(modalmap, "TRANSFORM_OT_transform");
}
