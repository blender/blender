/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edobj
 */

#include <cctype>
#include <cfloat>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "AS_asset_library.hh"
#include "BLI_path_utils.hh"
#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_rotation.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_armature_types.h"
#include "DNA_asset_types.h"
#include "DNA_collection_types.h"
#include "DNA_curve_types.h"
#include "DNA_lattice_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_object_force_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_anim_visualization.h"
#include "BKE_armature.hh"
#include "BKE_collection.hh"
#include "BKE_constraint.h"
#include "BKE_context.hh"
#include "BKE_curve.hh"
#include "BKE_editlattice.h"
#include "BKE_editmesh.hh"
#include "BKE_effect.h"
#include "BKE_global.hh"
#include "BKE_idprop.hh"
#include "BKE_image.hh"
#include "BKE_lattice.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_material.hh"
#include "BKE_mball.hh"
#include "BKE_mesh.hh"
#include "BKE_modifier.hh"
#include "BKE_node_runtime.hh"
#include "BKE_object.hh"
#include "BKE_paint.hh"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_report.hh"
#include "BKE_scene.hh"
#include "BKE_screen.hh"
#include "BKE_softbody.h"
#include "BKE_workspace.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "ED_anim_api.hh"
#include "ED_armature.hh"
#include "ED_asset.hh"
#include "ED_asset_menu_utils.hh"
#include "ED_curve.hh"
#include "ED_gpencil_legacy.hh"
#include "ED_grease_pencil.hh"
#include "ED_image.hh"
#include "ED_keyframes_keylist.hh"
#include "ED_lattice.hh"
#include "ED_mball.hh"
#include "ED_mesh.hh"
#include "ED_object.hh"
#include "ED_outliner.hh"
#include "ED_screen.hh"
#include "ED_undo.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_types.hh"

#include "UI_interface.hh"
#include "UI_interface_icons.hh"
#include "UI_interface_layout.hh"

#include "CLG_log.h"

/* For menu/popup icons etc. */

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_message.hh"
#include "WM_toolsystem.hh"
#include "WM_types.hh"

#include "MOD_nodes.hh"

#include "object_intern.hh" /* own include */

namespace blender::ed::object {

static CLG_LogRef LOG = {"object.edit"};

/* prototypes */
static ListBase selected_objects_get(bContext *C);

/* -------------------------------------------------------------------- */
/** \name Internal Utilities
 * \{ */

static bool object_mode_set_ok_or_report(ReportList *reports)
{
  /* NOTE(@ideasman42): toggling modes while transforming should not be allowed by the key-map,
   * so users should not be able do this. Python scripts can though,
   * so check here to report an error instead of crashing.
   *
   * This is *not* a comprehensive check, since users might be trying to change modes
   * while in the middle of *any* modal operator (painting or dragging a UI slider... etc).
   *
   * This check could be removed if it causes any problems since the error it prevents
   * is quite obscure. See: #137380. */

  if (G.moving & (G_TRANSFORM_OBJ | G_TRANSFORM_EDIT)) {
    BKE_reportf(reports, RPT_ERROR, "Unable to change object mode while transforming");
    return false;
  }

  return true;
}

Object *context_object(const bContext *C)
{
  return static_cast<Object *>(CTX_data_pointer_get_type(C, "object", &RNA_Object).data);
}

Object *context_active_object(const bContext *C)
{
  Object *ob = nullptr;
  if (C) {
    ob = context_object(C);
    if (!ob) {
      ob = CTX_data_active_object(C);
    }
  }
  return ob;
}

Vector<Object *> objects_in_mode_or_selected(bContext *C,
                                             bool (*filter_fn)(const Object *ob, void *user_data),
                                             void *filter_user_data)
{
  ScrArea *area = CTX_wm_area(C);
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob_active = BKE_view_layer_active_object_get(view_layer);
  ID *id_pin = nullptr;
  const bool use_objects_in_mode = (ob_active != nullptr) &&
                                   (ob_active->mode & (OB_MODE_EDIT | OB_MODE_POSE));
  const eSpace_Type space_type = area ? eSpace_Type(area->spacetype) : SPACE_EMPTY;

  Object *ob = nullptr;
  bool use_ob = true;

  if (space_type == SPACE_PROPERTIES) {
    SpaceProperties *sbuts = static_cast<SpaceProperties *>(area->spacedata.first);
    id_pin = sbuts->pinid;
  }

  if (id_pin && (GS(id_pin->name) == ID_OB)) {
    /* Pinned data takes priority, in this case ignore selection & other objects in the mode. */
    ob = (Object *)id_pin;
  }
  else if ((space_type == SPACE_PROPERTIES) && (use_objects_in_mode == false)) {
    /* When using the space-properties, we don't want to use the entire selection
     * as the current active object may not be selected.
     *
     * This is not the case when we're in a mode that supports multi-mode editing,
     * since the active object and all other objects in the mode will be included
     * irrespective of selection. */
    ob = ob_active;
  }
  else if (ob_active && (ob_active->mode &
                         (OB_MODE_ALL_PAINT | OB_MODE_ALL_SCULPT | OB_MODE_ALL_PAINT_GPENCIL)))
  {
    /* When painting, limit to active. */
    ob = ob_active;
  }
  else {
    /* Otherwise use full selection. */
    use_ob = false;
  }

  if (use_ob) {
    if ((ob != nullptr) && !filter_fn(ob, filter_user_data)) {
      ob = nullptr;
    }
    return ob ? Vector<Object *>({ob}) : Vector<Object *>();
  }
  const View3D *v3d = (space_type == SPACE_VIEW3D) ?
                          static_cast<const View3D *>(area->spacedata.first) :
                          nullptr;

  /* When in a mode that supports multiple active objects, use "objects in mode"
   * instead of the object's selection. */
  if (use_objects_in_mode) {
    ObjectsInModeParams params = {0};
    params.object_mode = ob_active->mode;
    params.no_dup_data = true;
    params.filter_fn = filter_fn;
    params.filter_userdata = filter_user_data;
    return BKE_view_layer_array_from_objects_in_mode_params(scene, view_layer, v3d, &params);
  }

  ObjectsInViewLayerParams params{};
  params.no_dup_data = true;
  params.filter_fn = filter_fn;
  params.filter_userdata = filter_user_data;
  return BKE_view_layer_array_selected_objects_params(view_layer, v3d, &params);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Object Index Lookup/Creation
 * \{ */

int object_in_mode_to_index(const Scene *scene,
                            ViewLayer *view_layer,
                            const eObjectMode mode,
                            const Object *ob)
{
  BLI_assert(ob != nullptr);
  /* NOTE: the `v3d` is always nullptr because the purpose of this function is to return
   * a reusable index, using the `v3d` only increases the chance the index may become
   * invalid-parameters. */
  int index = -1;
  int i = 0;
  FOREACH_BASE_IN_MODE_BEGIN (scene, view_layer, nullptr, -1, mode, base_iter) {
    if (base_iter->object == ob) {
      index = i;
      break;
    }
    i++;
  }
  FOREACH_BASE_IN_MODE_END;
  return index;
}

Object *object_in_mode_from_index(const Scene *scene,
                                  ViewLayer *view_layer,
                                  const eObjectMode mode,
                                  int index)
{
  BLI_assert(index >= 0);
  Object *ob = nullptr;
  int i = 0;
  FOREACH_BASE_IN_MODE_BEGIN (scene, view_layer, nullptr, -1, mode, base_iter) {
    if (index == i) {
      ob = base_iter->object;
      break;
    }
    i++;
  }
  FOREACH_BASE_IN_MODE_END;
  return ob;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Hide Operator
 * \{ */

static bool object_hide_poll(bContext *C)
{
  if (CTX_wm_space_outliner(C) != nullptr) {
    return ED_outliner_collections_editor_poll(C);
  }
  return ED_operator_view3d_active(C);
}

static wmOperatorStatus object_hide_view_clear_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const bool select = RNA_boolean_get(op->ptr, "select");
  bool changed = false;

  BKE_view_layer_synced_ensure(scene, view_layer);
  LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
    if (base->flag & BASE_HIDDEN) {
      base->flag &= ~BASE_HIDDEN;
      changed = true;

      if (select) {
        /* We cannot call `base_select` because
         * base is not selectable while it is hidden. */
        base->flag |= BASE_SELECTED;
        BKE_scene_object_base_flag_sync_from_base(base);
      }
    }
  }

  if (!changed) {
    return OPERATOR_CANCELLED;
  }

  BKE_view_layer_need_resync_tag(view_layer);
  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_VISIBLE, scene);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_hide_view_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Show Hidden Objects";
  ot->description = "Reveal temporarily hidden objects";
  ot->idname = "OBJECT_OT_hide_view_clear";

  /* API callbacks. */
  ot->exec = object_hide_view_clear_exec;
  ot->poll = object_hide_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna, "select", true, "Select", "Select revealed objects");
}

static wmOperatorStatus object_hide_view_set_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const bool unselected = RNA_boolean_get(op->ptr, "unselected");
  bool changed = false;
  const bool confirm = op->flag & OP_IS_INVOKE;
  uint hide_count = 0;

  /* Hide selected or unselected objects. */
  BKE_view_layer_synced_ensure(scene, view_layer);
  LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
    if (!(base->flag & BASE_ENABLED_AND_VISIBLE_IN_DEFAULT_VIEWPORT)) {
      continue;
    }

    if (!unselected) {
      if (base->flag & BASE_SELECTED) {
        base_select(base, BA_DESELECT);
        base->flag |= BASE_HIDDEN;
        hide_count++;
        changed = true;
      }
    }
    else {
      if (!(base->flag & BASE_SELECTED)) {
        base_select(base, BA_DESELECT);
        base->flag |= BASE_HIDDEN;
        hide_count++;
        changed = true;
      }
    }
  }
  if (!changed) {
    return OPERATOR_CANCELLED;
  }

  if (hide_count > 0 && confirm) {
    BKE_reportf(op->reports, RPT_INFO, "%u object(s) hidden", (hide_count));
  }

  BKE_view_layer_need_resync_tag(view_layer);
  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_VISIBLE, scene);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_hide_view_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Hide Objects";
  ot->description = "Temporarily hide objects from the viewport";
  ot->idname = "OBJECT_OT_hide_view_set";

  /* API callbacks. */
  ot->exec = object_hide_view_set_exec;
  ot->poll = object_hide_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  PropertyRNA *prop;
  prop = RNA_def_boolean(
      ot->srna, "unselected", false, "Unselected", "Hide unselected rather than selected objects");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
}

static wmOperatorStatus object_hide_collection_exec(bContext *C, wmOperator *op)
{
  View3D *v3d = CTX_wm_view3d(C);

  int index = RNA_int_get(op->ptr, "collection_index");
  const bool extend = RNA_boolean_get(op->ptr, "extend");
  const bool toggle = RNA_boolean_get(op->ptr, "toggle");

  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  LayerCollection *lc = BKE_layer_collection_from_index(view_layer, index);

  if (!lc) {
    return OPERATOR_CANCELLED;
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);

  if (v3d && v3d->flag & V3D_LOCAL_COLLECTIONS) {
    if (lc->runtime_flag & LAYER_COLLECTION_HIDE_VIEWPORT) {
      return OPERATOR_CANCELLED;
    }
    if (toggle) {
      lc->local_collections_bits ^= v3d->local_collections_uid;
      BKE_layer_collection_local_sync(scene, view_layer, v3d);
    }
    else {
      BKE_layer_collection_isolate_local(scene, view_layer, v3d, lc, extend);
    }
  }
  else {
    BKE_layer_collection_isolate_global(scene, view_layer, lc, extend);
  }

  WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);

  return OPERATOR_FINISHED;
}

#define COLLECTION_INVALID_INDEX -1

void collection_hide_menu_draw(const bContext *C, uiLayout *layout)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  LayerCollection *lc_scene = static_cast<LayerCollection *>(view_layer->layer_collections.first);

  /* Use the "invoke" operator context so the "Shift" modifier is used to extend. */
  layout->operator_context_set(wm::OpCallContext::InvokeRegionWin);

  LISTBASE_FOREACH (LayerCollection *, lc, &lc_scene->layer_collections) {
    int index = BKE_layer_collection_findindex(view_layer, lc);
    uiLayout *row = &layout->row(false);

    if (lc->flag & LAYER_COLLECTION_EXCLUDE) {
      continue;
    }

    if (lc->collection->flag & COLLECTION_HIDE_VIEWPORT) {
      continue;
    }

    int icon = ICON_NONE;
    if (BKE_layer_collection_has_selected_objects(scene, view_layer, lc)) {
      icon = ICON_LAYER_ACTIVE;
    }
    else if (lc->runtime_flag & LAYER_COLLECTION_HAS_OBJECTS) {
      icon = ICON_LAYER_USED;
    }
    PointerRNA op_ptr = row->op("OBJECT_OT_hide_collection", lc->collection->id.name + 2, icon);
    RNA_int_set(&op_ptr, "collection_index", index);
  }
}

static wmOperatorStatus object_hide_collection_invoke(bContext *C,
                                                      wmOperator *op,
                                                      const wmEvent *event)
{
  /* Immediately execute if collection index was specified. */
  int index = RNA_int_get(op->ptr, "collection_index");
  if (index != COLLECTION_INVALID_INDEX) {
    /* Only initialize extend from the shift key if the property isn't set
     * (typically initialized from the key-map). */
    PropertyRNA *prop = RNA_struct_find_property(op->ptr, "extend");
    if (!RNA_property_is_set(op->ptr, prop)) {
      RNA_property_boolean_set(op->ptr, prop, (event->modifier & KM_SHIFT) != 0);
    }
    return object_hide_collection_exec(C, op);
  }

  /* Open popup menu. */
  const char *title = CTX_IFACE_(op->type->translation_context, op->type->name);
  uiPopupMenu *pup = UI_popup_menu_begin(C, title, ICON_OUTLINER_COLLECTION);
  uiLayout *layout = UI_popup_menu_layout(pup);

  collection_hide_menu_draw(C, layout);

  UI_popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

void OBJECT_OT_hide_collection(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Hide Other Collections";
  ot->description = "Show only objects in collection (Shift to extend)";
  ot->idname = "OBJECT_OT_hide_collection";

  /* API callbacks. */
  ot->exec = object_hide_collection_exec;
  ot->invoke = object_hide_collection_invoke;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Properties. */
  PropertyRNA *prop;
  prop = RNA_def_int(ot->srna,
                     "collection_index",
                     COLLECTION_INVALID_INDEX,
                     COLLECTION_INVALID_INDEX,
                     INT_MAX,
                     "Collection Index",
                     "Index of the collection to change visibility",
                     0,
                     INT_MAX);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
  prop = RNA_def_boolean(ot->srna, "toggle", false, "Toggle", "Toggle visibility");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
  prop = RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend visibility");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Toggle Edit-Mode Operator
 * \{ */

/* When switching mode, certain data needs to be copied from the `bPoseChannel` to the `Bone`. This
 * is not done in `BKE_pose_rebuild` because that is called in other cases other than mode
 * switching. */
static void flush_bone_selection_to_pose(Object &ob)
{
  BLI_assert(ob.pose);
  LISTBASE_FOREACH (bPoseChannel *, pose_bone, &ob.pose->chanbase) {
    if (pose_bone->bone->flag & BONE_SELECTED) {
      pose_bone->flag |= POSE_SELECTED;
    }
    else {
      pose_bone->flag &= ~POSE_SELECTED;
    }
  }
}
static void flush_pose_selection_to_bone(Object &ob)
{
  BLI_assert(ob.pose);
  constexpr int selection_flags = (BONE_SELECTED | BONE_ROOTSEL | BONE_TIPSEL);
  LISTBASE_FOREACH (bPoseChannel *, pose_bone, &ob.pose->chanbase) {
    if (pose_bone->flag & POSE_SELECTED) {
      pose_bone->bone->flag |= selection_flags;
    }
    else {
      pose_bone->bone->flag &= ~selection_flags;
    }
  }
}

static bool mesh_needs_keyindex(Main *bmain, const Mesh *mesh)
{
  if (mesh->key) {
    return false; /* will be added */
  }

  LISTBASE_FOREACH (const Object *, ob, &bmain->objects) {
    if ((ob->parent) && (ob->parent->data == mesh) && ELEM(ob->partype, PARVERT1, PARVERT3)) {
      return true;
    }
    if (ob->data == mesh) {
      LISTBASE_FOREACH (const ModifierData *, md, &ob->modifiers) {
        if (md->type == eModifierType_Hook) {
          return true;
        }
      }
    }
  }
  return false;
}

/**
 * Load edit-mode data back into the object.
 *
 * \param load_data: Flush the edit-mode data back to the object.
 * \param free_data: Free the edit-mode data.
 */
static bool editmode_load_free_ex(Main *bmain,
                                  Object *obedit,
                                  const bool load_data,
                                  const bool free_data)
{
  BLI_assert(load_data || free_data);

  if (obedit == nullptr) {
    return false;
  }

  if (obedit->type == OB_MESH) {
    Mesh *mesh = static_cast<Mesh *>(obedit->data);
    if (mesh->runtime->edit_mesh == nullptr) {
      return false;
    }

    if (mesh->runtime->edit_mesh->bm->totvert > MESH_MAX_VERTS) {
      /* This used to be warned int the UI, we could warn again although it's quite rare. */
      CLOG_WARN(&LOG,
                "Too many vertices for mesh '%s' (%d)",
                mesh->id.name + 2,
                mesh->runtime->edit_mesh->bm->totvert);
      return false;
    }

    if (load_data) {
      EDBM_mesh_load_ex(bmain, obedit, free_data);
    }

    if (free_data) {
      EDBM_mesh_free_data(mesh->runtime->edit_mesh.get());
      mesh->runtime->edit_mesh.reset();
    }
    /* will be recalculated as needed. */
    {
      ED_mesh_mirror_spatial_table_end(obedit);
      ED_mesh_mirror_topo_table_end(obedit);
    }
  }
  else if (obedit->type == OB_ARMATURE) {
    const bArmature *arm = static_cast<const bArmature *>(obedit->data);
    if (arm->edbo == nullptr) {
      return false;
    }

    if (load_data) {
      ED_armature_from_edit(bmain, static_cast<bArmature *>(obedit->data));
    }

    if (free_data) {
      ED_armature_edit_free(static_cast<bArmature *>(obedit->data));

      if (load_data == false) {
        /* Don't keep unused pose channels created by duplicating bones
         * which may have been deleted/undone, see: #87631. */
        if (obedit->pose != nullptr) {
          BKE_pose_channels_clear_with_null_bone(obedit->pose, true);
        }
      }
    }

    /* After regenerating the bones, sync the selection onto the pose bones. */
    flush_bone_selection_to_pose(*obedit);
    /* TODO(sergey): Pose channels might have been changed, so need
     * to inform dependency graph about this. But is it really the
     * best place to do this?
     */
    DEG_relations_tag_update(bmain);
  }
  else if (ELEM(obedit->type, OB_CURVES_LEGACY, OB_SURF)) {
    const Curve *cu = static_cast<const Curve *>(obedit->data);
    if (cu->editnurb == nullptr) {
      return false;
    }

    if (load_data) {
      ED_curve_editnurb_load(bmain, obedit);
    }

    if (free_data) {
      ED_curve_editnurb_free(obedit);
    }
  }
  else if (obedit->type == OB_FONT) {
    const Curve *cu = static_cast<const Curve *>(obedit->data);
    if (cu->editfont == nullptr) {
      return false;
    }

    if (load_data) {
      ED_curve_editfont_load(obedit);
    }

    if (free_data) {
      ED_curve_editfont_free(obedit);
    }
  }
  else if (obedit->type == OB_LATTICE) {
    const Lattice *lt = static_cast<const Lattice *>(obedit->data);
    if (lt->editlatt == nullptr) {
      return false;
    }

    if (load_data) {
      BKE_editlattice_load(obedit);
    }

    if (free_data) {
      BKE_editlattice_free(obedit);
    }
  }
  else if (obedit->type == OB_MBALL) {
    const MetaBall *mb = static_cast<const MetaBall *>(obedit->data);
    if (mb->editelems == nullptr) {
      return false;
    }

    if (load_data) {
      ED_mball_editmball_load(obedit);
    }

    if (free_data) {
      ED_mball_editmball_free(obedit);
    }
  }
  else if (ELEM(obedit->type, OB_CURVES, OB_GREASE_PENCIL, OB_POINTCLOUD)) {
    /* Object doesn't have specific edit mode data, so pass. */
  }
  else {
    return false;
  }

  if (load_data) {
    char *needs_flush_ptr = BKE_object_data_editmode_flush_ptr_get(
        static_cast<ID *>(obedit->data));
    if (needs_flush_ptr) {
      *needs_flush_ptr = false;
    }
  }

  return true;
}

bool editmode_load(Main *bmain, Object *obedit)
{
  return editmode_load_free_ex(bmain, obedit, true, false);
}

bool editmode_exit_ex(Main *bmain, Scene *scene, Object *obedit, int flag)
{
  const bool free_data = (flag & EM_FREEDATA) != 0;

  if (editmode_load_free_ex(bmain, obedit, true, free_data) == false) {
    /* in rare cases (background mode) its possible active object
     * is flagged for editmode, without 'obedit' being set #35489. */
    if (UNLIKELY(obedit && obedit->mode & OB_MODE_EDIT)) {
      obedit->mode &= ~OB_MODE_EDIT;
      /* Also happens when mesh is shared across multiple objects. #69834. */
      DEG_id_tag_update(&obedit->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
      /* Leaving edit mode may modify the original object data; tag that as well. */
      DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_GEOMETRY);
    }
    return true;
  }

  /* `free_data` only false now on file saves and render. */
  if (free_data) {
    /* flag object caches as outdated */
    ListBase pidlist;
    BKE_ptcache_ids_from_object(&pidlist, obedit, scene, 0);
    LISTBASE_FOREACH (PTCacheID *, pid, &pidlist) {
      /* particles don't need reset on geometry change */
      if (pid->type != PTCACHE_TYPE_PARTICLES) {
        pid->cache->flag |= PTCACHE_OUTDATED;
      }
    }
    BLI_freelistN(&pidlist);

    BKE_particlesystem_reset_all(obedit);
    BKE_ptcache_object_reset(scene, obedit, PTCACHE_RESET_OUTDATED);

    /* also flush ob recalc, doesn't take much overhead, but used for particles */
    DEG_id_tag_update(&obedit->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    /* Leaving edit mode may modify the original object data; tag that as well. */
    DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_GEOMETRY);

    WM_main_add_notifier(NC_SCENE | ND_MODE | NS_MODE_OBJECT, scene);

    obedit->mode &= ~OB_MODE_EDIT;
  }

  return (obedit->mode & OB_MODE_EDIT) == 0;
}

bool editmode_exit(bContext *C, int flag)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Object *obedit = CTX_data_edit_object(C);
  return editmode_exit_ex(bmain, scene, obedit, flag);
}

bool editmode_free_ex(Main *bmain, Object *obedit)
{
  return editmode_load_free_ex(bmain, obedit, false, true);
}

bool editmode_exit_multi_ex(Main *bmain, Scene *scene, ViewLayer *view_layer, int flag)
{
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *obedit = BKE_view_layer_edit_object_get(view_layer);
  if (obedit == nullptr) {
    return false;
  }
  bool changed = false;
  const short obedit_type = obedit->type;

  BKE_view_layer_synced_ensure(scene, view_layer);
  LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
    Object *ob = base->object;
    if ((ob->type == obedit_type) && (ob->mode & OB_MODE_EDIT)) {
      changed |= editmode_exit_ex(bmain, scene, base->object, flag);
    }
  }
  return changed;
}

bool editmode_exit_multi(bContext *C, int flag)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  return editmode_exit_multi_ex(bmain, scene, view_layer, flag);
}

bool editmode_enter_ex(Main *bmain, Scene *scene, Object *ob, int flag)
{
  bool ok = false;

  if (ELEM(nullptr, ob, ob->data) || !ID_IS_EDITABLE(ob) || ID_IS_OVERRIDE_LIBRARY(ob) ||
      ID_IS_OVERRIDE_LIBRARY(ob->data))
  {
    return false;
  }

  /* This checks actual `ob->data`, for cases when other scenes have it in edit-mode context.
   * Currently multiple objects sharing a mesh being in edit-mode at once isn't supported,
   * see: #86767. */
  if (BKE_object_is_in_editmode(ob)) {
    return true;
  }

  if (BKE_object_obdata_is_libdata(ob)) {
    /* Ideally the caller should check this. */
    CLOG_WARN(&LOG, "Unable to enter edit-mode on library data for object '%s'", ob->id.name + 2);
    return false;
  }

  ob->restore_mode = ob->mode;

  ob->mode = OB_MODE_EDIT;

  if (ob->type == OB_MESH) {
    ok = true;

    const bool use_key_index = mesh_needs_keyindex(bmain, static_cast<const Mesh *>(ob->data));

    EDBM_mesh_make(ob, scene->toolsettings->selectmode, use_key_index);

    BMEditMesh *em = BKE_editmesh_from_object(ob);
    if (LIKELY(em)) {
      BKE_editmesh_looptris_and_normals_calc(em);
    }

    WM_main_add_notifier(NC_SCENE | ND_MODE | NS_EDITMODE_MESH, nullptr);
  }
  else if (ob->type == OB_ARMATURE) {
    /* Syncing the selection to the `Bone` before converting to edit bones. This is not possible if
     * the Armature was just created, because then there is no pose data yet. Which is fine, the
     * just-created edit bones already have the expected selection state. */
    if (ob->pose) {
      flush_pose_selection_to_bone(*ob);
    }

    bArmature *arm = static_cast<bArmature *>(ob->data);
    ok = true;
    ED_armature_to_edit(arm);
    /* To ensure all goes in rest-position and without striding. */

    arm->needs_flush_to_id = 0;

    /* WORKAROUND / FIXME: this is a temporary workaround to ensure that
     * full bone collection data gets restored when exiting edit mode
     * via an undo step. The correct fix is to have a full edit-mode
     * copy of bone collections so that edit-mode changes don't modify
     * object-mode armature data until exiting edit mode. But that
     * change is a bit of a project, and will be done later. This line
     * should be removed when that is done. */
    bmain->is_memfile_undo_written = false;

    /* XXX: should this be ID_RECALC_GEOMETRY? */
    DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);

    WM_main_add_notifier(NC_SCENE | ND_MODE | NS_EDITMODE_ARMATURE, scene);
  }
  else if (ob->type == OB_FONT) {
    ok = true;
    ED_curve_editfont_make(ob);

    WM_main_add_notifier(NC_SCENE | ND_MODE | NS_EDITMODE_TEXT, scene);
  }
  else if (ob->type == OB_MBALL) {
    MetaBall *mb = static_cast<MetaBall *>(ob->data);

    ok = true;
    ED_mball_editmball_make(ob);

    mb->needs_flush_to_id = 0;

    WM_main_add_notifier(NC_SCENE | ND_MODE | NS_EDITMODE_MBALL, scene);
  }
  else if (ob->type == OB_LATTICE) {
    ok = true;
    BKE_editlattice_make(ob);

    WM_main_add_notifier(NC_SCENE | ND_MODE | NS_EDITMODE_LATTICE, scene);
  }
  else if (ELEM(ob->type, OB_SURF, OB_CURVES_LEGACY)) {
    ok = true;
    ED_curve_editnurb_make(ob);

    WM_main_add_notifier(NC_SCENE | ND_MODE | NS_EDITMODE_CURVE, scene);
  }
  else if (ob->type == OB_CURVES) {
    ok = true;
    WM_main_add_notifier(NC_SCENE | ND_MODE | NS_EDITMODE_CURVES, scene);
  }
  else if (ob->type == OB_GREASE_PENCIL) {
    ok = true;
    blender::ed::greasepencil::ensure_selection_domain(scene->toolsettings, ob);
    WM_main_add_notifier(NC_SCENE | ND_MODE | NS_EDITMODE_GREASE_PENCIL, scene);
  }
  else if (ob->type == OB_POINTCLOUD) {
    ok = true;
    WM_main_add_notifier(NC_SCENE | ND_MODE | NS_EDITMODE_POINTCLOUD, scene);
  }

  if (ok) {
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  }
  else {
    if ((flag & EM_NO_CONTEXT) == 0) {
      ob->mode &= ~OB_MODE_EDIT;
    }
    WM_main_add_notifier(NC_SCENE | ND_MODE | NS_MODE_OBJECT, scene);
  }

  return (ob->mode & OB_MODE_EDIT) != 0;
}

bool editmode_enter(bContext *C, int flag)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);

  /* Active layer checked here for view3d,
   * callers that don't want view context can call the extended version. */
  Object *ob = CTX_data_active_object(C);
  return editmode_enter_ex(bmain, scene, ob, flag);
}

static wmOperatorStatus editmode_toggle_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  View3D *v3d = CTX_wm_view3d(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *obact = BKE_view_layer_active_object_get(view_layer);
  const int mode_flag = OB_MODE_EDIT;
  const bool is_mode_set = (obact->mode & mode_flag) != 0;
  wmMsgBus *mbus = CTX_wm_message_bus(C);

  if (!object_mode_set_ok_or_report(op->reports)) {
    return OPERATOR_CANCELLED;
  }
  if (!is_mode_set) {
    if (!mode_compat_set(C, obact, eObjectMode(mode_flag), op->reports)) {
      return OPERATOR_CANCELLED;
    }
  }

  if (!is_mode_set) {
    editmode_enter_ex(bmain, scene, obact, 0);
    /* Grease Pencil does not support multi-object editing. */
    if ((obact->type != OB_GREASE_PENCIL) && ((obact->mode & mode_flag) != 0)) {
      FOREACH_SELECTED_OBJECT_BEGIN (view_layer, v3d, ob) {
        if ((ob != obact) && (ob->type == obact->type)) {
          editmode_enter_ex(bmain, scene, ob, EM_NO_CONTEXT);
        }
      }
      FOREACH_SELECTED_OBJECT_END;
    }
  }
  else {
    editmode_exit_ex(bmain, scene, obact, EM_FREEDATA);

    if ((obact->mode & mode_flag) == 0) {
      FOREACH_OBJECT_BEGIN (scene, view_layer, ob) {
        if ((ob != obact) && (ob->type == obact->type)) {
          editmode_exit_ex(bmain, scene, ob, EM_FREEDATA);
        }
      }
      FOREACH_OBJECT_END;
    }
  }

  WM_msg_publish_rna_prop(mbus, &obact->id, obact, Object, mode);

  if (G.background == false) {
    WM_toolsystem_update_from_context_view3d(C);
  }

  return OPERATOR_FINISHED;
}

static bool editmode_toggle_poll(bContext *C)
{
  /* Get object the same way as in editmode_toggle_exec(). Otherwise overriding context can crash,
   * see #137998. */
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(CTX_data_scene(C), view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);

  /* Covers liboverrides too. */
  if (ELEM(nullptr, ob, ob->data) || !ID_IS_EDITABLE(ob->data) || ID_IS_OVERRIDE_LIBRARY(ob) ||
      ID_IS_OVERRIDE_LIBRARY(ob->data))
  {
    return false;
  }

  /* If hidden but in edit mode, we still display. */
  if ((ob->visibility_flag & OB_HIDE_VIEWPORT) && !(ob->mode & OB_MODE_EDIT)) {
    return false;
  }

  return OB_TYPE_SUPPORT_EDITMODE(ob->type);
}

void OBJECT_OT_editmode_toggle(wmOperatorType *ot)
{

  /* identifiers */
  ot->name = "Edit Mode";
  ot->description = "Toggle object's edit mode";
  ot->idname = "OBJECT_OT_editmode_toggle";

  /* API callbacks. */
  ot->exec = editmode_toggle_exec;
  ot->poll = editmode_toggle_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Toggle Pose-Mode Operator
 * \{ */

static wmOperatorStatus posemode_exec(bContext *C, wmOperator *op)
{
  wmMsgBus *mbus = CTX_wm_message_bus(C);
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Base *base = CTX_data_active_base(C);

  if (!object_mode_set_ok_or_report(op->reports)) {
    return OPERATOR_CANCELLED;
  }

  /* If the base is nullptr it means we have an active object, but the object itself is hidden. */
  if (base == nullptr) {
    return OPERATOR_CANCELLED;
  }

  Object *obact = base->object;
  const int mode_flag = OB_MODE_POSE;
  bool is_mode_set = (obact->mode & mode_flag) != 0;

  if (!is_mode_set) {
    if (!mode_compat_set(C, obact, eObjectMode(mode_flag), op->reports)) {
      return OPERATOR_CANCELLED;
    }
  }

  if (obact->type != OB_ARMATURE) {
    return OPERATOR_PASS_THROUGH;
  }

  {
    BKE_view_layer_synced_ensure(scene, view_layer);
    Object *obedit = BKE_view_layer_edit_object_get(view_layer);
    if (obact == obedit) {
      editmode_exit_ex(bmain, scene, obedit, EM_FREEDATA);
      is_mode_set = false;
    }
  }

  if (is_mode_set) {
    bool ok = ED_object_posemode_exit(C, obact);
    if (ok) {
      FOREACH_OBJECT_BEGIN (scene, view_layer, ob) {
        if ((ob != obact) && (ob->type == OB_ARMATURE) && (ob->mode & mode_flag)) {
          ED_object_posemode_exit_ex(bmain, ob);
        }
      }
      FOREACH_OBJECT_END;
    }
  }
  else {
    bool ok = ED_object_posemode_enter(C, obact);
    if (ok) {
      const View3D *v3d = CTX_wm_view3d(C);
      FOREACH_SELECTED_OBJECT_BEGIN (view_layer, v3d, ob) {
        if ((ob != obact) && (ob->type == OB_ARMATURE) && (ob->mode == OB_MODE_OBJECT) &&
            BKE_id_is_editable(bmain, &ob->id))
        {
          ED_object_posemode_enter_ex(bmain, ob);
        }
      }
      FOREACH_SELECTED_OBJECT_END;
    }
  }

  WM_msg_publish_rna_prop(mbus, &obact->id, obact, Object, mode);

  if (G.background == false) {
    WM_toolsystem_update_from_context_view3d(C);
  }

  return OPERATOR_FINISHED;
}

void OBJECT_OT_posemode_toggle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Toggle Pose Mode";
  ot->idname = "OBJECT_OT_posemode_toggle";
  ot->description = "Enable or disable posing/selecting bones";

  /* API callbacks. */
  ot->exec = posemode_exec;
  ot->poll = ED_operator_object_active_editable;

  /* flag */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Force Field Toggle Operator
 * \{ */

void check_force_modifiers(Main *bmain, Scene *scene, Object *object)
{
  PartDeflect *pd = object->pd;
  ModifierData *md = BKE_modifiers_findby_type(object, eModifierType_Surface);

  /* add/remove modifier as needed */
  if (!md) {
    if (pd && (pd->shape == PFIELD_SHAPE_SURFACE) &&
        !ELEM(pd->forcefield, 0, PFIELD_GUIDE, PFIELD_TEXTURE))
    {
      if (ELEM(object->type, OB_MESH, OB_SURF, OB_FONT, OB_CURVES_LEGACY)) {
        modifier_add(nullptr, bmain, scene, object, nullptr, eModifierType_Surface);
      }
    }
  }
  else {
    if (!pd || (pd->shape != PFIELD_SHAPE_SURFACE) ||
        ELEM(pd->forcefield, 0, PFIELD_GUIDE, PFIELD_TEXTURE))
    {
      modifier_remove(nullptr, bmain, scene, object, md);
    }
  }
}

static wmOperatorStatus forcefield_toggle_exec(bContext *C, wmOperator * /*op*/)
{
  Object *ob = CTX_data_active_object(C);

  if (ob->pd == nullptr) {
    ob->pd = BKE_partdeflect_new(PFIELD_FORCE);
    ob->empty_drawtype = OB_PLAINAXES;
  }
  else if (ob->pd->forcefield == 0) {
    ob->pd->forcefield = PFIELD_FORCE;
    ob->empty_drawtype = OB_PLAINAXES;
  }
  else {
    ob->pd->forcefield = 0;
  }

  check_force_modifiers(CTX_data_main(C), CTX_data_scene(C), ob);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_forcefield_toggle(wmOperatorType *ot)
{

  /* identifiers */
  ot->name = "Toggle Force Field";
  ot->description = "Toggle object's force field";
  ot->idname = "OBJECT_OT_forcefield_toggle";

  /* API callbacks. */
  ot->exec = forcefield_toggle_exec;
  ot->poll = ED_operator_object_active_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Calculate Motion Paths Operator
 * \{ */

static eAnimvizCalcRange object_path_convert_range(eObjectPathCalcRange range)
{
  switch (range) {
    case OBJECT_PATH_CALC_RANGE_CURRENT_FRAME:
      return ANIMVIZ_CALC_RANGE_CURRENT_FRAME;
    case OBJECT_PATH_CALC_RANGE_CHANGED:
      return ANIMVIZ_CALC_RANGE_CHANGED;
    case OBJECT_PATH_CALC_RANGE_FULL:
      return ANIMVIZ_CALC_RANGE_FULL;
  }
  return ANIMVIZ_CALC_RANGE_FULL;
}

void motion_paths_recalc_selected(bContext *C, Scene *scene, eObjectPathCalcRange range)
{
  ListBase selected_objects = {nullptr, nullptr};
  CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects) {
    BLI_addtail(&selected_objects, BLI_genericNodeN(ob));
  }
  CTX_DATA_END;

  motion_paths_recalc(C, scene, range, &selected_objects);

  BLI_freelistN(&selected_objects);
}

void motion_paths_recalc_visible(bContext *C, Scene *scene, eObjectPathCalcRange range)
{
  ListBase visible_objects = {nullptr, nullptr};
  CTX_DATA_BEGIN (C, Object *, ob, visible_objects) {
    BLI_addtail(&visible_objects, BLI_genericNodeN(ob));
  }
  CTX_DATA_END;

  motion_paths_recalc(C, scene, range, &visible_objects);

  BLI_freelistN(&visible_objects);
}

static bool has_object_motion_paths(Object *ob)
{
  return (ob->avs.path_bakeflag & MOTIONPATH_BAKE_HAS_PATHS) != 0;
}

static bool has_pose_motion_paths(Object *ob)
{
  return ob->pose && (ob->pose->avs.path_bakeflag & MOTIONPATH_BAKE_HAS_PATHS) != 0;
}

void motion_paths_recalc(bContext *C,
                         Scene *scene,
                         eObjectPathCalcRange range,
                         ListBase *ld_objects)
{
  /* Transform doesn't always have context available to do update. */
  if (C == nullptr) {
    return;
  }

  Main *bmain = CTX_data_main(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  blender::Vector<MPathTarget *> targets;
  LISTBASE_FOREACH (LinkData *, link, ld_objects) {
    Object *ob = static_cast<Object *>(link->data);

    /* set flag to force recalc, then grab path(s) from object */
    if (has_object_motion_paths(ob)) {
      ob->avs.recalc |= ANIMVIZ_RECALC_PATHS;
    }

    if (has_pose_motion_paths(ob)) {
      ob->pose->avs.recalc |= ANIMVIZ_RECALC_PATHS;
    }

    animviz_build_motionpath_targets(ob, targets);
  }

  Depsgraph *depsgraph;
  bool free_depsgraph = false;
  /* For a single frame update it's faster to re-use existing dependency graph and avoid overhead
   * of building all the relations and so on for a temporary one. */
  if (range == OBJECT_PATH_CALC_RANGE_CURRENT_FRAME) {
    /* NOTE: Dependency graph will be evaluated at all the frames, but we first need to access some
     * nested pointers, like animation data. */
    depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
    free_depsgraph = false;
  }
  else {
    depsgraph = animviz_depsgraph_build(bmain, scene, view_layer, targets);
    free_depsgraph = true;
  }

  animviz_calc_motionpaths(
      depsgraph, bmain, scene, targets, object_path_convert_range(range), true);
  animviz_free_motionpath_targets(targets);

  if (range != OBJECT_PATH_CALC_RANGE_CURRENT_FRAME) {
    /* Tag objects for copy-on-eval - so paths will draw/redraw
     * For currently frame only we update evaluated object directly. */
    LISTBASE_FOREACH (LinkData *, link, ld_objects) {
      Object *ob = static_cast<Object *>(link->data);

      if (has_object_motion_paths(ob) || has_pose_motion_paths(ob)) {
        DEG_id_tag_update(&ob->id, ID_RECALC_SYNC_TO_EVAL);
      }
    }
  }

  /* Free temporary depsgraph. */
  if (free_depsgraph) {
    DEG_graph_free(depsgraph);
  }
}

/* show popup to determine settings */
static wmOperatorStatus object_calculate_paths_invoke(bContext *C,
                                                      wmOperator *op,
                                                      const wmEvent * /*event*/)
{
  Object *ob = CTX_data_active_object(C);

  if (ob == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* set default settings from existing/stored settings */
  {
    bAnimVizSettings *avs = &ob->avs;
    RNA_enum_set(op->ptr, "display_type", avs->path_type);
    RNA_enum_set(op->ptr, "range", avs->path_range);
  }

  /* show popup dialog to allow editing of range... */
  /* FIXME: hard-coded dimensions here are just arbitrary. */
  return WM_operator_props_dialog_popup(
      C, op, 270, IFACE_("Calculate Object Motion Paths"), IFACE_("Calculate"));
}

/* Calculate/recalculate whole paths (avs.path_sf to avs.path_ef) */
static wmOperatorStatus object_calculate_paths_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  short path_type = RNA_enum_get(op->ptr, "display_type");
  short path_range = RNA_enum_get(op->ptr, "range");

  /* set up path data for objects being calculated */
  CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects) {
    bAnimVizSettings *avs = &ob->avs;
    /* grab baking settings from operator settings */
    avs->path_type = path_type;
    avs->path_range = path_range;
    animviz_motionpath_compute_range(ob, scene);

    /* verify that the selected object has the appropriate settings */
    animviz_verify_motionpaths(op->reports, scene, ob, nullptr);
  }
  CTX_DATA_END;

  /* calculate the paths for objects that have them (and are tagged to get refreshed) */
  motion_paths_recalc_selected(C, scene, OBJECT_PATH_CALC_RANGE_FULL);

  /* notifiers for updates */
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW_ANIMVIZ, nullptr);
  /* NOTE: the notifier below isn't actually correct, but kept around just to be on the safe side.
   * If further testing shows it's not necessary (for both bones and objects) removal is fine. */
  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM | ND_POSE, nullptr);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_paths_calculate(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Calculate Object Motion Paths";
  ot->idname = "OBJECT_OT_paths_calculate";
  ot->description = "Generate motion paths for the selected objects";

  /* API callbacks. */
  ot->invoke = object_calculate_paths_invoke;
  ot->exec = object_calculate_paths_exec;
  ot->poll = ED_operator_object_active_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_enum(ot->srna,
               "display_type",
               rna_enum_motionpath_display_type_items,
               MOTIONPATH_TYPE_RANGE,
               "Display Type",
               "");
  RNA_def_enum(ot->srna,
               "range",
               rna_enum_motionpath_range_items,
               MOTIONPATH_RANGE_SCENE,
               "Computation Range",
               "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Update Motion Paths Operator
 * \{ */

static bool object_update_paths_poll(bContext *C)
{
  if (ED_operator_object_active_editable(C)) {
    Object *ob = context_active_object(C);
    return (ob->avs.path_bakeflag & MOTIONPATH_BAKE_HAS_PATHS) != 0;
  }

  return false;
}

static wmOperatorStatus object_update_paths_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);

  if (scene == nullptr) {
    return OPERATOR_CANCELLED;
  }
  CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects) {
    animviz_motionpath_compute_range(ob, scene);
    /* verify that the selected object has the appropriate settings */
    animviz_verify_motionpaths(op->reports, scene, ob, nullptr);
  }
  CTX_DATA_END;

  /* calculate the paths for objects that have them (and are tagged to get refreshed) */
  motion_paths_recalc_selected(C, scene, OBJECT_PATH_CALC_RANGE_FULL);

  /* notifiers for updates */
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW_ANIMVIZ, nullptr);
  /* NOTE: the notifier below isn't actually correct, but kept around just to be on the safe side.
   * If further testing shows it's not necessary (for both bones and objects) removal is fine. */
  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM | ND_POSE, nullptr);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_paths_update(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Update Object Paths";
  ot->idname = "OBJECT_OT_paths_update";
  ot->description = "Recalculate motion paths for selected objects";

  /* API callbacks. */
  ot->exec = object_update_paths_exec;
  ot->poll = object_update_paths_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Update All Motion Paths Operator
 * \{ */

static bool object_update_all_paths_poll(bContext * /*C*/)
{
  return true;
}

static wmOperatorStatus object_update_all_paths_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);

  if (scene == nullptr) {
    return OPERATOR_CANCELLED;
  }

  motion_paths_recalc_visible(C, scene, OBJECT_PATH_CALC_RANGE_FULL);

  WM_event_add_notifier(C, NC_OBJECT | ND_POSE | ND_TRANSFORM, nullptr);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_paths_update_visible(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Update All Object Paths";
  ot->idname = "OBJECT_OT_paths_update_visible";
  ot->description = "Recalculate all visible motion paths for objects and poses";

  /* API callbacks. */
  ot->exec = object_update_all_paths_exec;
  ot->poll = object_update_all_paths_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clear Motion Paths Operator
 * \{ */

/* Helper for motion_paths_clear() */
static void object_clear_mpath(Object *ob)
{
  if (ob->mpath) {
    animviz_free_motionpath(ob->mpath);
    ob->mpath = nullptr;
    ob->avs.path_bakeflag &= ~MOTIONPATH_BAKE_HAS_PATHS;

    /* tag object for copy-on-eval - so removed paths don't still show */
    DEG_id_tag_update(&ob->id, ID_RECALC_SYNC_TO_EVAL);
  }
}

void motion_paths_clear(bContext *C, bool only_selected)
{
  if (only_selected) {
    /* Loop over all selected + editable objects in scene. */
    CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects) {
      object_clear_mpath(ob);
    }
    CTX_DATA_END;
  }
  else {
    /* Loop over all editable objects in scene. */
    CTX_DATA_BEGIN (C, Object *, ob, editable_objects) {
      object_clear_mpath(ob);
    }
    CTX_DATA_END;
  }
}

/* operator callback for this */
static wmOperatorStatus object_clear_paths_exec(bContext *C, wmOperator *op)
{
  bool only_selected = RNA_boolean_get(op->ptr, "only_selected");

  /* use the backend function for this */
  motion_paths_clear(C, only_selected);

  /* notifiers for updates */
  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, nullptr);

  return OPERATOR_FINISHED;
}

static std::string object_clear_paths_get_description(bContext * /*C*/,
                                                      wmOperatorType * /*ot*/,
                                                      PointerRNA *ptr)
{
  const bool only_selected = RNA_boolean_get(ptr, "only_selected");
  if (only_selected) {
    return TIP_("Clear motion paths of selected objects");
  }
  return TIP_("Clear motion paths of all objects");
}

void OBJECT_OT_paths_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Object Paths";
  ot->idname = "OBJECT_OT_paths_clear";

  /* API callbacks. */
  ot->exec = object_clear_paths_exec;
  ot->poll = ED_operator_object_active_editable;
  ot->get_description = object_clear_paths_get_description;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_boolean(ot->srna,
                             "only_selected",
                             false,
                             "Only Selected",
                             "Only clear motion paths of selected objects");
  RNA_def_property_flag(ot->prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Object Shade Smooth/Flat Operator
 * \{ */

static bool is_smooth_by_angle_modifier(const ModifierData &md)
{
  if (md.type != eModifierType_Nodes) {
    return false;
  }
  const NodesModifierData &nmd = reinterpret_cast<const NodesModifierData &>(md);
  if (!nmd.node_group) {
    return false;
  }
  if (const LibraryWeakReference *library_ref = nmd.node_group->id.library_weak_reference) {
    /* Support appended assets added before asset packing in Blender 5.0. */
    if (!library_ref) {
      return false;
    }
    if (!STREQ(library_ref->library_id_name + 2, "Smooth by Angle")) {
      return false;
    }
    return true;
  }
  const Library *library = nmd.node_group->id.lib;
  if (!library) {
    return false;
  }
  char auto_smooth_asset_path[FILE_MAX] = "datafiles/assets/nodes/geometry_nodes_essentials.blend";
  BLI_path_slash_native(auto_smooth_asset_path);
  if (!StringRef(library->filepath).endswith(auto_smooth_asset_path)) {
    return false;
  }
  if (!STREQ(BKE_id_name(nmd.node_group->id), "Smooth by Angle")) {
    return false;
  }
  return true;
}

static wmOperatorStatus shade_smooth_exec(bContext *C, wmOperator *op)
{
  const bool use_flat = STREQ(op->idname, "OBJECT_OT_shade_flat");
  const bool use_smooth = STREQ(op->idname, "OBJECT_OT_shade_smooth");
  const bool use_smooth_by_angle = STREQ(op->idname, "OBJECT_OT_shade_smooth_by_angle");
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);

  Vector<PointerRNA> ctx_objects;

  /* For modes that only use an active object, don't handle the whole selection. */
  {
    Scene *scene = CTX_data_scene(C);
    ViewLayer *view_layer = CTX_data_view_layer(C);
    BKE_view_layer_synced_ensure(scene, view_layer);
    Object *obact = BKE_view_layer_active_object_get(view_layer);
    if (obact && (obact->mode & OB_MODE_ALL_PAINT)) {
      ctx_objects.append(RNA_id_pointer_create(&obact->id));
    }
  }

  if (ctx_objects.is_empty()) {
    CTX_data_selected_editable_objects(C, &ctx_objects);
  }

  Set<ID *> object_data;
  for (const PointerRNA &ptr : ctx_objects) {
    Object *ob = static_cast<Object *>(ptr.data);
    if (ID *data = static_cast<ID *>(ob->data)) {
      object_data.add(data);

      if (ob->type == OB_MESH) {
        if (use_flat || use_smooth) {
          LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
            if (is_smooth_by_angle_modifier(*md)) {
              modifier_remove(op->reports, bmain, scene, ob, md);
              DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
              WM_main_add_notifier(NC_OBJECT | ND_MODIFIER | NA_REMOVED, ob);
              break;
            }
          }
        }
      }
    }
  }

  bool changed_multi = false;
  bool has_linked_data = false;
  for (ID *data : object_data) {
    if (!BKE_id_is_editable(bmain, data)) {
      has_linked_data = true;
      continue;
    }

    bool changed = false;
    if (GS(data->name) == ID_ME) {
      Mesh &mesh = *reinterpret_cast<Mesh *>(data);
      const bool keep_sharp_edges = RNA_boolean_get(op->ptr, "keep_sharp_edges");
      bke::mesh_smooth_set(mesh, use_smooth || use_smooth_by_angle, keep_sharp_edges);
      if (use_smooth_by_angle) {
        const float angle = RNA_float_get(op->ptr, "angle");
        bke::mesh_sharp_edges_set_from_angle(mesh, angle, keep_sharp_edges);
      }
      BKE_mesh_batch_cache_dirty_tag(reinterpret_cast<Mesh *>(data), BKE_MESH_BATCH_DIRTY_ALL);
      changed = true;
    }
    else if (GS(data->name) == ID_CU_LEGACY) {
      BKE_curve_smooth_flag_set(reinterpret_cast<Curve *>(data), use_smooth);
      changed = true;
    }

    if (changed) {
      changed_multi = true;
      DEG_id_tag_update(data, ID_RECALC_GEOMETRY);
      WM_event_add_notifier(C, NC_GEOM | ND_DATA, data);
    }
  }

  if (has_linked_data) {
    BKE_report(op->reports, RPT_WARNING, "Cannot edit linked mesh or curve data");
  }

  return (changed_multi) ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static bool shade_poll(bContext *C)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *obact = BKE_view_layer_active_object_get(view_layer);
  if (obact != nullptr) {
    /* Doesn't handle edit-data, sculpt dynamic-topology, or their undo systems. */
    if (obact->mode & (OB_MODE_EDIT | OB_MODE_SCULPT) || obact->data == nullptr ||
        !ID_IS_EDITABLE(obact) || !ID_IS_EDITABLE(obact->data) || ID_IS_OVERRIDE_LIBRARY(obact) ||
        ID_IS_OVERRIDE_LIBRARY(obact->data))
    {
      return false;
    }
  }
  return true;
}

void OBJECT_OT_shade_flat(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Shade Flat";
  ot->description = "Render and display faces uniform, using face normals";
  ot->idname = "OBJECT_OT_shade_flat";

  /* API callbacks. */
  ot->poll = shade_poll;
  ot->exec = shade_smooth_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna,
                  "keep_sharp_edges",
                  true,
                  "Keep Sharp Edges",
                  "Don't remove sharp edges, which are redundant with faces shaded smooth");
}

void OBJECT_OT_shade_smooth(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Shade Smooth";
  ot->description = "Render and display faces smooth, using interpolated vertex normals";
  ot->idname = "OBJECT_OT_shade_smooth";

  /* API callbacks. */
  ot->poll = shade_poll;
  ot->exec = shade_smooth_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna,
                  "keep_sharp_edges",
                  true,
                  "Keep Sharp Edges",
                  "Don't remove sharp edges. Tagged edges will remain sharp");
}

void OBJECT_OT_shade_smooth_by_angle(wmOperatorType *ot)
{
  ot->name = "Shade Smooth by Angle";
  ot->description =
      "Set the sharpness of mesh edges based on the angle between the neighboring faces";
  ot->idname = "OBJECT_OT_shade_smooth_by_angle";

  ot->poll = shade_poll;
  ot->exec = shade_smooth_exec;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  PropertyRNA *prop = RNA_def_property(ot->srna, "angle", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_range(prop, 0.0f, DEG2RADF(180.0f));
  RNA_def_property_float_default(prop, DEG2RADF(30.0f));
  RNA_def_property_ui_text(
      prop, "Angle", "Maximum angle between face normals that will be considered as smooth");

  RNA_def_boolean(ot->srna,
                  "keep_sharp_edges",
                  true,
                  "Keep Sharp Edges",
                  "Only add sharp edges instead of clearing existing tags first");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Object Shade Auto Smooth Operator
 * \{ */

/**
 * Does a shallow check for whether the node group could be the Smooth by Angle node group.
 * This should become unnecessary once we asset embedding (#132167).
 */
static bool is_valid_smooth_by_angle_group(const bNodeTree &ntree)
{
  if (ntree.type != NTREE_GEOMETRY) {
    return false;
  }
  if (ntree.interface_inputs().size() != 3) {
    return false;
  }
  if (ntree.interface_outputs().size() != 1) {
    return false;
  }
  return true;
}

static wmOperatorStatus shade_auto_smooth_exec(bContext *C, wmOperator *op)
{
  Main &bmain = *CTX_data_main(C);
  Scene &scene = *CTX_data_scene(C);

  const bool use_auto_smooth = RNA_boolean_get(op->ptr, "use_auto_smooth");
  const float angle = RNA_float_get(op->ptr, "angle");

  Vector<PointerRNA> ctx_objects;
  CTX_data_selected_editable_objects(C, &ctx_objects);

  if (use_auto_smooth) {
    AssetWeakReference asset_weak_ref{};
    asset_weak_ref.asset_library_type = ASSET_LIBRARY_ESSENTIALS;
    asset_weak_ref.relative_asset_identifier = BLI_strdup(
        "nodes/geometry_nodes_essentials.blend/NodeTree/Smooth by Angle");

    if (G.background) {
      /* For testing purposes, make sure assets are loaded (this make take too long to do
       * automatically during user interaction). */
      asset::list::storage_fetch_blocking(asset_system::all_library_reference(), *C);
    }

    const asset_system::AssetRepresentation *asset_representation =
        asset::find_asset_from_weak_ref(*C, asset_weak_ref, op->reports);
    if (!asset_representation) {
      return OPERATOR_CANCELLED;
    }

    bNodeTree *node_group = nullptr;
    while (!node_group) {
      ID *node_group_id = asset::asset_local_id_ensure_imported(bmain, *asset_representation);
      if (!node_group_id) {
        return OPERATOR_CANCELLED;
      }
      if (GS(node_group_id->name) != ID_NT) {
        return OPERATOR_CANCELLED;
      }
      node_group = reinterpret_cast<bNodeTree *>(node_group_id);
      node_group->ensure_topology_cache();
      node_group->ensure_interface_cache();
      if (is_valid_smooth_by_angle_group(*node_group)) {
        break;
      }
      /* Remove the weak library reference, since the already loaded group is not valid anymore. */
      MEM_SAFE_FREE(node_group_id->library_weak_reference);
      /* Stay in the loop and load the asset again. */
      node_group = nullptr;
    }

    const StringRefNull angle_identifier = node_group->interface_inputs()[1]->identifier;

    for (const PointerRNA &ob_ptr : ctx_objects) {
      Object *object = static_cast<Object *>(ob_ptr.data);
      if (object->type == OB_MESH) {
        Mesh *mesh = static_cast<Mesh *>(object->data);
        bke::mesh_smooth_set(*mesh, true, true);
        DEG_id_tag_update(&mesh->id, ID_RECALC_GEOMETRY);
      }
      NodesModifierData *smooth_by_angle_nmd = nullptr;
      LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
        if (is_smooth_by_angle_modifier(*md)) {
          smooth_by_angle_nmd = reinterpret_cast<NodesModifierData *>(md);
          break;
        }
      }
      if (!smooth_by_angle_nmd) {
        smooth_by_angle_nmd = reinterpret_cast<NodesModifierData *>(
            modifier_add(op->reports, &bmain, &scene, object, nullptr, eModifierType_Nodes));
        if (!smooth_by_angle_nmd) {
          continue;
        }
        smooth_by_angle_nmd->modifier.flag |= eModifierFlag_PinLast;
        smooth_by_angle_nmd->node_group = node_group;
        id_us_plus(&node_group->id);
        MOD_nodes_update_interface(object, smooth_by_angle_nmd);
        smooth_by_angle_nmd->flag |= NODES_MODIFIER_HIDE_DATABLOCK_SELECTOR |
                                     NODES_MODIFIER_HIDE_MANAGE_PANEL;
        STRNCPY_UTF8(smooth_by_angle_nmd->modifier.name, DATA_(node_group->id.name + 2));
        BKE_modifier_unique_name(&object->modifiers, &smooth_by_angle_nmd->modifier);
      }

      IDProperty *angle_prop = IDP_GetPropertyFromGroup(smooth_by_angle_nmd->settings.properties,
                                                        angle_identifier.c_str());
      if (angle_prop->type == IDP_FLOAT) {
        IDP_float_set(angle_prop, angle);
      }
      else if (angle_prop->type == IDP_DOUBLE) {
        IDP_double_set(angle_prop, angle);
      }

      DEG_id_tag_update(&object->id, ID_RECALC_GEOMETRY);
      WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, object);
    }
  }
  else {
    for (const PointerRNA &ob_ptr : ctx_objects) {
      Object *object = static_cast<Object *>(ob_ptr.data);
      LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
        if (is_smooth_by_angle_modifier(*md)) {
          modifier_remove(op->reports, &bmain, &scene, object, md);
          WM_main_add_notifier(NC_OBJECT | ND_MODIFIER | NA_REMOVED, object);
          break;
        }
      }
    }
  }

  return OPERATOR_FINISHED;
}

static void shade_auto_smooth_ui(bContext * /*C*/, wmOperator *op)
{
  uiLayout *layout = op->layout;

  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);

  layout->prop(op->ptr, "use_auto_smooth", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  uiLayout *col = &layout->column(false);
  col->active_set(RNA_boolean_get(op->ptr, "use_auto_smooth"));
  layout->prop(op->ptr, "angle", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

void OBJECT_OT_shade_auto_smooth(wmOperatorType *ot)
{
  ot->name = "Shade Auto Smooth";
  ot->description =
      "Add modifier to automatically set the sharpness of mesh edges based on the angle between "
      "the neighboring faces";
  ot->idname = "OBJECT_OT_shade_auto_smooth";

  ot->poll = shade_poll;
  ot->exec = shade_auto_smooth_exec;
  ot->ui = shade_auto_smooth_ui;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  PropertyRNA *prop;

  prop = RNA_def_boolean(ot->srna,
                         "use_auto_smooth",
                         true,
                         "Auto Smooth",
                         "Add modifier to set edge sharpness automatically");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_property(ot->srna, "angle", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_range(prop, 0.0f, DEG2RADF(180.0f));
  RNA_def_property_float_default(prop, DEG2RADF(30.0f));
  RNA_def_property_ui_text(
      prop, "Angle", "Maximum angle between face normals that will be considered as smooth");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Object Mode Set Operator
 * \{ */

static const EnumPropertyItem *object_mode_set_itemf(bContext *C,
                                                     PointerRNA * /*ptr*/,
                                                     PropertyRNA * /*prop*/,
                                                     bool *r_free)
{
  const EnumPropertyItem *input = rna_enum_object_mode_items;
  EnumPropertyItem *item = nullptr;
  int totitem = 0;

  if (!C) { /* needed for docs */
    return rna_enum_object_mode_items;
  }

  const Object *ob = CTX_data_active_object(C);
  if (ob) {
    while (input->identifier) {
      if (mode_compat_test(ob, eObjectMode(input->value))) {
        RNA_enum_item_add(&item, &totitem, input);
      }
      input++;
    }
  }
  else {
    /* We need at least this one! */
    RNA_enum_items_add_value(&item, &totitem, input, OB_MODE_OBJECT);
  }

  RNA_enum_item_end(&item, &totitem);

  *r_free = true;

  return item;
}

static bool object_mode_set_poll(bContext *C)
{
  /* Needed as #ED_operator_object_active_editable doesn't call use 'active_object'. */
  Object *ob = CTX_data_active_object(C);
  return ED_operator_object_active_editable_ex(C, ob);
}

static wmOperatorStatus object_mode_set_exec(bContext *C, wmOperator *op)
{
  const bool use_submode = STREQ(op->idname, "OBJECT_OT_mode_set_with_submode");
  Object *ob = CTX_data_active_object(C);
  eObjectMode mode = eObjectMode(RNA_enum_get(op->ptr, "mode"));
  const bool toggle = RNA_boolean_get(op->ptr, "toggle");

  if (!mode_compat_test(ob, mode)) {
    return OPERATOR_PASS_THROUGH;
  }
  if (!object_mode_set_ok_or_report(op->reports)) {
    return OPERATOR_CANCELLED;
  }

  /**
   * Mode Switching Logic (internal details).
   *
   * Notes:
   * - Code below avoids calling mode switching functions more than once,
   *   as this causes unnecessary calculations and undo steps to be added.
   * - The previous mode (#Object.restore_mode) is object mode by default.
   *
   * Supported Cases:
   * - Setting the mode (when the 'toggle' setting is off).
   * - Toggle the mode:
   *   - Toggle between object mode and non-object mode property.
   *   - Toggle between the previous mode (#Object.restore_mode) and the mode property.
   *   - Toggle object mode.
   *     While this is similar to regular toggle,
   *     this operator depends on there being a previous mode set
   *     (this isn't bound to a key with the default key-map).
   */
  if (toggle == false) {
    if (ob->mode != mode) {
      mode_set_ex(C, mode, true, op->reports);
    }
  }
  else {
    const eObjectMode mode_prev = eObjectMode(ob->mode);
    /* When toggling object mode, we always use the restore mode,
     * otherwise there is nothing to do. */
    if (mode == OB_MODE_OBJECT) {
      if (ob->mode != OB_MODE_OBJECT) {
        if (mode_set_ex(C, OB_MODE_OBJECT, true, op->reports)) {
          /* Store old mode so we know what to go back to. */
          ob->restore_mode = mode_prev;
        }
      }
      else {
        if (ob->restore_mode != OB_MODE_OBJECT) {
          mode_set_ex(C, eObjectMode(ob->restore_mode), true, op->reports);
        }
      }
    }
    else {
      /* Non-object modes, enter the 'mode' unless it's already set,
       * in that case use restore mode. */
      if (ob->mode != mode) {
        if (mode_set_ex(C, mode, true, op->reports)) {
          /* Store old mode so we know what to go back to. */
          ob->restore_mode = mode_prev;
        }
      }
      else {
        if (ob->restore_mode != OB_MODE_OBJECT) {
          mode_set_ex(C, eObjectMode(ob->restore_mode), true, op->reports);
        }
        else {
          mode_set_ex(C, OB_MODE_OBJECT, true, op->reports);
        }
      }
    }
  }

  if (use_submode) {
    if (ob->type == OB_MESH) {
      if (ob->mode & OB_MODE_EDIT) {
        PropertyRNA *prop = RNA_struct_find_property(op->ptr, "mesh_select_mode");
        if (RNA_property_is_set(op->ptr, prop)) {
          int mesh_select_mode = RNA_property_enum_get(op->ptr, prop);
          if (mesh_select_mode != 0) {
            EDBM_selectmode_set_multi(C, mesh_select_mode);
          }
        }
      }
    }
  }

  wmWindowManager *wm = CTX_wm_manager(C);
  if (wm) {
    if (WM_autosave_is_scheduled(wm)) {
      WM_autosave_write(wm, CTX_data_main(C));
    }
  }

  return OPERATOR_FINISHED;
}

void OBJECT_OT_mode_set(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Set Object Mode";
  ot->description = "Sets the object interaction mode";
  ot->idname = "OBJECT_OT_mode_set";

  /* API callbacks. */
  ot->exec = object_mode_set_exec;
  ot->poll = object_mode_set_poll;

  /* flags */
  ot->flag = 0; /* no register/undo here, leave it to operators being called */

  ot->prop = RNA_def_enum(
      ot->srna, "mode", rna_enum_object_mode_items, OB_MODE_OBJECT, "Mode", "");
  RNA_def_enum_funcs(ot->prop, object_mode_set_itemf);
  RNA_def_property_flag(ot->prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna, "toggle", false, "Toggle", "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

void OBJECT_OT_mode_set_with_submode(wmOperatorType *ot)
{
  OBJECT_OT_mode_set(ot);

  /* identifiers */
  ot->name = "Set Object Mode with Sub-mode";
  ot->idname = "OBJECT_OT_mode_set_with_submode";

  /* properties */
  /* we could add other types - particle for eg. */
  PropertyRNA *prop;
  prop = RNA_def_enum_flag(
      ot->srna, "mesh_select_mode", rna_enum_mesh_select_mode_items, 0, "Mesh Mode", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Object Link/Move to Collection Operator
 * \{ */

static ListBase selected_objects_get(bContext *C)
{
  ListBase objects = {nullptr};

  if (CTX_wm_space_outliner(C) != nullptr) {
    ED_outliner_selected_objects_get(C, &objects);
  }
  else {
    CTX_DATA_BEGIN (C, Object *, ob, selected_objects) {
      BLI_addtail(&objects, BLI_genericNodeN(ob));
    }
    CTX_DATA_END;
  }

  return objects;
}

static bool move_to_collection_poll(bContext *C)
{
  if (CTX_wm_space_outliner(C) != nullptr) {
    return ED_outliner_collections_editor_poll(C);
  }
  return ED_operator_objectmode(C);
}

static wmOperatorStatus move_to_collection_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "collection_uid");
  const bool is_link = STREQ(op->idname, "OBJECT_OT_link_to_collection");
  const bool is_new = RNA_boolean_get(op->ptr, "is_new");

  if (!RNA_property_is_set(op->ptr, prop)) {
    BKE_report(op->reports, RPT_ERROR, "No collection selected");
    return OPERATOR_CANCELLED;
  }
  Scene *src_scene = CTX_data_scene(C);

  int collection_uid = RNA_property_int_get(op->ptr, prop);
  Scene *dest_scene = nullptr;
  Collection *collection = BKE_collection_from_session_uid(bmain, collection_uid, &dest_scene);
  if (collection == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "Unexpected error, collection not found");
    return OPERATOR_CANCELLED;
  }

  if (!ID_IS_EDITABLE(collection) || ID_IS_OVERRIDE_LIBRARY(collection)) {
    BKE_report(
        op->reports, RPT_ERROR, "Cannot add objects to a library override or linked collection");
    return OPERATOR_CANCELLED;
  }

  ListBase objects = selected_objects_get(C);

  if (is_new) {
    char new_collection_name[MAX_ID_NAME - 2];
    RNA_string_get(op->ptr, "new_collection_name", new_collection_name);
    collection = BKE_collection_add(bmain, collection, new_collection_name);
  }

  Object *single_object = BLI_listbase_is_single(&objects) ?
                              static_cast<Object *>(((LinkData *)objects.first)->data) :
                              nullptr;

  if ((single_object != nullptr) && is_link &&
      BKE_collection_has_object(collection, single_object))
  {
    BKE_reportf(op->reports,
                RPT_ERROR,
                "%s already in %s",
                single_object->id.name + 2,
                BKE_collection_ui_name_get(collection));
    BLI_freelistN(&objects);
    return OPERATOR_CANCELLED;
  }

  LISTBASE_FOREACH (LinkData *, link, &objects) {
    Object *ob = static_cast<Object *>(link->data);

    if (!is_link) {
      BKE_collection_object_move(bmain, src_scene, collection, nullptr, ob);
    }
    else {
      BKE_collection_object_add(bmain, collection, ob);
    }
  }
  BLI_freelistN(&objects);

  if (is_link) {
    if (single_object != nullptr) {
      BKE_reportf(op->reports,
                  RPT_INFO,
                  "%s linked to %s",
                  single_object->id.name + 2,
                  BKE_collection_ui_name_get(collection));
    }
    else {
      BKE_reportf(
          op->reports, RPT_INFO, "Objects linked to %s", BKE_collection_ui_name_get(collection));
    }
  }
  else {
    if (single_object != nullptr) {
      BKE_reportf(op->reports,
                  RPT_INFO,
                  "%s moved to %s",
                  single_object->id.name + 2,
                  BKE_collection_ui_name_get(collection));
    }
    else {
      BKE_reportf(
          op->reports, RPT_INFO, "Objects moved to %s", BKE_collection_ui_name_get(collection));
    }
  }

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&src_scene->id, ID_RECALC_SYNC_TO_EVAL | ID_RECALC_SELECT);

  WM_event_add_notifier(C, NC_SCENE | ND_LAYER, src_scene);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, src_scene);
  WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, src_scene);

  if (src_scene != dest_scene) {
    DEG_id_tag_update(&dest_scene->id, ID_RECALC_SYNC_TO_EVAL | ID_RECALC_SELECT);

    WM_event_add_notifier(C, NC_SCENE | ND_LAYER, dest_scene);
    WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, dest_scene);
    WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, dest_scene);
  }

  return OPERATOR_FINISHED;
}

static wmOperatorStatus move_to_collection_invoke(bContext *C,
                                                  wmOperator *op,
                                                  const wmEvent * /*event*/)
{
  ListBase objects = selected_objects_get(C);
  if (BLI_listbase_is_empty(&objects)) {
    BKE_report(op->reports, RPT_ERROR, "No objects selected");
    return OPERATOR_CANCELLED;
  }
  BLI_freelistN(&objects);
  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "collection_uid");
  bool is_move = STREQ(op->type->idname, "OBJECT_OT_move_to_collection");
  if (!RNA_property_is_set(op->ptr, prop)) {
    WM_menu_name_call(C,
                      is_move ? "OBJECT_MT_move_to_collection" : "OBJECT_MT_link_to_collection",
                      wm::OpCallContext::InvokeDefault);
    return OPERATOR_FINISHED;
  }

  if (!RNA_boolean_get(op->ptr, "is_new")) {
    return move_to_collection_exec(C, op);
  }

  int collection_uid = RNA_property_int_get(op->ptr, prop);
  Collection *collection = BKE_collection_from_session_uid(CTX_data_main(C), collection_uid);

  if (!collection) {
    BKE_report(op->reports, RPT_ERROR, "Unexpected error, collection not found");
    return OPERATOR_CANCELLED;
  }

  prop = RNA_struct_find_property(op->ptr, "new_collection_name");
  if (!RNA_property_is_set(op->ptr, prop)) {
    char name[MAX_ID_NAME - 2];

    BKE_collection_new_name_get(collection, name);

    RNA_property_string_set(op->ptr, prop, name);
    return WM_operator_props_dialog_popup(C,
                                          op,
                                          200,
                                          is_move ? IFACE_("Move to New Collection") :
                                                    IFACE_("Link to New Collection"),
                                          IFACE_("Create"));
  }
  return move_to_collection_exec(C, op);
}

static void move_to_collection_menu_draw(Menu *menu, Collection *collection, int icon)
{
  uiLayout &layout = *menu->layout;
  bool is_move = ELEM(StringRefNull(menu->type->idname),
                      "OBJECT_MT_move_to_collection",
                      "OBJECT_MT_move_to_collection_recursive");
  wmOperatorType *ot = WM_operatortype_find(
      is_move ? "OBJECT_OT_move_to_collection" : "OBJECT_OT_link_to_collection", false);

  layout.operator_context_set(wm::OpCallContext::InvokeDefault);

  PointerRNA op_ptr = layout.op(
      ot, CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "New Collection"), ICON_ADD);
  RNA_int_set(&op_ptr, "collection_uid", collection->id.session_uid);
  RNA_boolean_set(&op_ptr, "is_new", true);
  layout.separator();

  op_ptr = layout.op(ot, BKE_collection_ui_name_get(collection), icon);
  RNA_int_set(&op_ptr, "collection_uid", collection->id.session_uid);

  LISTBASE_FOREACH (CollectionChild *, child, &collection->children) {
    collection = child->collection;
    if (BLI_listbase_is_empty(&collection->children)) {
      op_ptr = layout.op(
          ot, BKE_collection_ui_name_get(collection), UI_icon_color_from_collection(collection));
      RNA_int_set(&op_ptr, "collection_uid", collection->id.session_uid);
      continue;
    }
    const PointerRNA ptr = RNA_id_pointer_create(&collection->id);
    layout.context_ptr_set("collection", &ptr);
    layout.menu(is_move ? "OBJECT_MT_move_to_collection_recursive" :
                          "OBJECT_MT_link_to_collection_recursive",
                BKE_collection_ui_name_get(collection),
                UI_icon_color_from_collection(collection));
  }
}

static void move_to_collection_recursive_menu_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout &layout = *menu->layout;
  const PointerRNA *ptr = layout.context_ptr_get("collection", &RNA_Collection);
  Collection *collection = ptr ? ptr->data_as<Collection>() : nullptr;
  if (!collection) {
    return;
  }
  move_to_collection_menu_draw(menu, collection, UI_icon_color_from_collection(collection));
}

static void move_to_collection_menu_draw(const bContext *C, Menu *menu)
{
  uiLayout &layout = *menu->layout;
  Scene *scene = CTX_data_scene(C);
  if (layout.operator_context() == wm::OpCallContext::ExecRegionWin) {
    layout.operator_context_set(wm::OpCallContext::InvokeRegionWin);
    PointerRNA op_ptr = layout.op("WM_OT_search_single_menu",
                                  CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Search..."),
                                  ICON_VIEWZOOM);
    RNA_string_set(&op_ptr, "menu_idname", menu->type->idname);
    layout.separator();
  }
  move_to_collection_menu_draw(menu, scene->master_collection, ICON_SCENE_DATA);
}

void move_to_collection_menu_register()
{
  /* Add recursive sub-menu type, to avoid each sub-menu from showing the main menu shortcut. */
  MenuType *mt = MEM_callocN<MenuType>("OBJECT_MT_move_to_collection_recursive");
  STRNCPY_UTF8(mt->idname, "OBJECT_MT_move_to_collection_recursive");
  STRNCPY_UTF8(mt->label, N_("Move to Collection Recursive"));
  STRNCPY_UTF8(mt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  mt->draw = move_to_collection_recursive_menu_draw;
  mt->flag = MenuTypeFlag::ContextDependent;
  WM_menutype_add(mt);

  mt = MEM_callocN<MenuType>("OBJECT_MT_move_to_collection");
  STRNCPY_UTF8(mt->idname, "OBJECT_MT_move_to_collection");
  STRNCPY_UTF8(mt->label, N_("Move to Collection"));
  STRNCPY_UTF8(mt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  mt->draw = move_to_collection_menu_draw;
  mt->flag = MenuTypeFlag::SearchOnKeyPress;
  WM_menutype_add(mt);
}

void link_to_collection_menu_register()
{
  /* Add recursive sub-menu type, to avoid each sub-menu from showing the main menu shortcut. */
  MenuType *mt = MEM_callocN<MenuType>("OBJECT_MT_link_to_collection_recursive");
  STRNCPY_UTF8(mt->idname, "OBJECT_MT_link_to_collection_recursive");
  STRNCPY_UTF8(mt->label, N_("Link to Collection Recursive"));
  STRNCPY_UTF8(mt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  mt->draw = move_to_collection_recursive_menu_draw;
  mt->flag = MenuTypeFlag::ContextDependent;
  WM_menutype_add(mt);

  mt = MEM_callocN<MenuType>("OBJECT_MT_link_to_collection");
  STRNCPY_UTF8(mt->idname, "OBJECT_MT_link_to_collection");
  STRNCPY_UTF8(mt->label, N_("Link to Collection"));
  STRNCPY_UTF8(mt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  mt->draw = move_to_collection_menu_draw;
  mt->flag = MenuTypeFlag::SearchOnKeyPress;
  WM_menutype_add(mt);
}

void OBJECT_OT_move_to_collection(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Move to Collection";
  ot->description = "Move objects to a collection";
  ot->idname = "OBJECT_OT_move_to_collection";

  /* API callbacks. */
  ot->exec = move_to_collection_exec;
  ot->invoke = move_to_collection_invoke;
  ot->poll = move_to_collection_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  prop = RNA_def_int(ot->srna,
                     "collection_uid",
                     COLLECTION_INVALID_INDEX,
                     COLLECTION_INVALID_INDEX,
                     INT_MAX,
                     "Collection UID",
                     "Session UID of the collection to move to",
                     0,
                     INT_MAX);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
  prop = RNA_def_boolean(ot->srna, "is_new", false, "New", "Move objects to a new collection");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
  prop = RNA_def_string(ot->srna,
                        "new_collection_name",
                        nullptr,
                        MAX_ID_NAME - 2,
                        "Name",
                        "Name of the newly added collection");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  ot->prop = prop;
}

void OBJECT_OT_link_to_collection(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Link to Collection";
  ot->description = "Link objects to a collection";
  ot->idname = "OBJECT_OT_link_to_collection";

  /* API callbacks. */
  ot->exec = move_to_collection_exec;
  ot->invoke = move_to_collection_invoke;
  ot->poll = move_to_collection_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  prop = RNA_def_int(ot->srna,
                     "collection_uid",
                     COLLECTION_INVALID_INDEX,
                     COLLECTION_INVALID_INDEX,
                     INT_MAX,
                     "Collection UID",
                     "Session UID of the collection to link to",
                     0,
                     INT_MAX);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
  prop = RNA_def_boolean(ot->srna, "is_new", false, "New", "Link objects to a new collection");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
  prop = RNA_def_string(ot->srna,
                        "new_collection_name",
                        nullptr,
                        MAX_ID_NAME - 2,
                        "Name",
                        "Name of the newly added collection");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  ot->prop = prop;
}

/** \} */

}  // namespace blender::ed::object
