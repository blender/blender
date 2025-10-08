/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include <cstdlib>

#include "DNA_armature_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_shader_fx_types.h"

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BKE_armature.hh"
#include "BKE_collection.hh"
#include "BKE_constraint.h"
#include "BKE_context.hh"
#include "BKE_deform.hh"
#include "BKE_gpencil_legacy.h"
#include "BKE_grease_pencil.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_modifier.hh"
#include "BKE_object.hh"
#include "BKE_particle.h"
#include "BKE_report.hh"
#include "BKE_shader_fx.h"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "ED_armature.hh"
#include "ED_buttons.hh"
#include "ED_object.hh"
#include "ED_outliner.hh"
#include "ED_screen.hh"
#include "ED_select_utils.hh"
#include "ED_sequencer.hh"
#include "ED_text.hh"
#include "ED_undo.hh"

#include "SEQ_select.hh"
#include "SEQ_sequencer.hh"

#include "WM_api.hh"
#include "WM_message.hh"
#include "WM_types.hh"

#include "UI_interface.hh"
#include "UI_view2d.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.hh"

#include "ANIM_armature.hh"
#include "ANIM_bone_collections.hh"

#include "outliner_intern.hh"
#include "tree/tree_element_grease_pencil_node.hh"
#include "tree/tree_element_seq.hh"
#include "tree/tree_iterator.hh"

namespace blender::ed::outliner {

/* -------------------------------------------------------------------- */
/** \name Internal Utilities
 * \{ */

/**
 * \note changes to selection are by convention and not essential.
 *
 * \note Handles its own undo push.
 */
static void do_outliner_item_editmode_toggle(bContext *C, Scene *scene, Base *base)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = base->object;

  bool changed = false;
  if (BKE_object_is_in_editmode(ob)) {
    changed = object::editmode_exit_ex(bmain, scene, ob, object::EM_FREEDATA);
    if (changed) {
      object::base_select(base, object::BA_DESELECT);
      WM_event_add_notifier(C, NC_SCENE | ND_MODE | NS_MODE_OBJECT, nullptr);
    }
  }
  else {
    changed = object::editmode_enter_ex(CTX_data_main(C), scene, ob, object::EM_NO_CONTEXT);
    if (changed) {
      object::base_select(base, object::BA_SELECT);
      WM_event_add_notifier(C, NC_SCENE | ND_MODE, nullptr);
    }
  }

  if (changed) {
    DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
    ED_outliner_select_sync_from_object_tag(C);
    ED_undo_push(C, "Outliner Edit Mode Toggle");
  }
}

/**
 * \note changes to selection are by convention and not essential.
 *
 * \note Handles its own undo push.
 */
static void do_outliner_item_posemode_toggle(bContext *C, Scene *scene, Base *base)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = base->object;

  if (!BKE_id_is_editable(CTX_data_main(C), &ob->id)) {
    BKE_report(CTX_wm_reports(C), RPT_WARNING, "Cannot pose non-editable data");
    return;
  }

  bool changed = false;
  if (ob->mode & OB_MODE_POSE) {
    changed = ED_object_posemode_exit_ex(bmain, ob);
    if (changed) {
      object::base_select(base, object::BA_DESELECT);
      WM_event_add_notifier(C, NC_SCENE | ND_MODE | NS_MODE_OBJECT, nullptr);
    }
  }
  else {
    changed = ED_object_posemode_enter_ex(bmain, ob);
    if (changed) {
      object::base_select(base, object::BA_SELECT);
      WM_event_add_notifier(C, NC_SCENE | ND_MODE | NS_MODE_POSE, nullptr);
    }
  }

  if (changed) {
    DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
    ED_outliner_select_sync_from_object_tag(C);
    ED_undo_push(C, "Outliner Pose Mode Toggle");
  }
}

/**
 * Swap the current active object from the interaction mode with the given base.
 *
 * \note Changes to selection _are_ needed in this case,
 * since entering the object mode uses the selection.
 *
 * If we didn't want to touch selection we could add an option to the operators
 * not to do multi-object editing.
 *
 * \note Handles its own undo push.
 */
static void do_outliner_item_mode_toggle_generic(bContext *C,
                                                 const TreeViewContext &tvc,
                                                 Base *base)
{
  const eObjectMode active_mode = (eObjectMode)tvc.obact->mode;
  ED_undo_group_begin(C);

  if (object::mode_set(C, OB_MODE_OBJECT)) {
    BKE_view_layer_synced_ensure(tvc.scene, tvc.view_layer);
    Base *base_active = BKE_view_layer_base_find(tvc.view_layer, tvc.obact);
    if (base_active != base) {
      BKE_view_layer_base_deselect_all(tvc.scene, tvc.view_layer);
      BKE_view_layer_base_select_and_set_active(tvc.view_layer, base);
      DEG_id_tag_update(&tvc.scene->id, ID_RECALC_SELECT);
      ED_undo_push(C, "Change Active");

      /* Operator call does undo push. */
      object::mode_set(C, active_mode);
      ED_outliner_select_sync_from_object_tag(C);
    }
  }
  ED_undo_group_end(C);
}

void outliner_item_mode_toggle(bContext *C,
                               const TreeViewContext &tvc,
                               TreeElement *te,
                               const bool do_extend)
{
  TreeStoreElem *tselem = TREESTORE(te);

  if ((tselem->type == TSE_SOME_ID) && (te->idcode == ID_OB)) {
    Object *ob = (Object *)tselem->id;
    BKE_view_layer_synced_ensure(tvc.scene, tvc.view_layer);
    Base *base = BKE_view_layer_base_find(tvc.view_layer, ob);

    /* Hidden objects can be removed from the mode. */
    if (!base || (!(base->flag & BASE_ENABLED_AND_MAYBE_VISIBLE_IN_VIEWPORT) &&
                  (ob->mode != tvc.obact->mode)))
    {
      return;
    }

    if (!do_extend) {
      do_outliner_item_mode_toggle_generic(C, tvc, base);
    }
    else if (tvc.ob_edit && OB_TYPE_SUPPORT_EDITMODE(ob->type)) {
      do_outliner_item_editmode_toggle(C, tvc.scene, base);
    }
    else if (tvc.ob_pose && ob->type == OB_ARMATURE) {
      do_outliner_item_posemode_toggle(C, tvc.scene, base);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Outliner Element Selection/Activation on Click Operator
 * \{ */

static void tree_element_viewlayer_activate(bContext *C, TreeElement *te)
{
  /* paranoia check */
  if (te->store_elem->type != TSE_R_LAYER) {
    return;
  }

  ViewLayer *view_layer = static_cast<ViewLayer *>(te->directdata);
  wmWindow *win = CTX_wm_window(C);
  Scene *scene = WM_window_get_active_scene(win);

  if (BLI_findindex(&scene->view_layers, view_layer) != -1) {
    WM_window_set_active_view_layer(win, view_layer);
    WM_event_add_notifier(C, NC_SCREEN | ND_LAYER, nullptr);
  }
}

/**
 * Select object tree
 */
static void do_outliner_object_select_recursive(const Scene *scene,
                                                ViewLayer *view_layer,
                                                Object *ob_parent,
                                                bool select)
{
  BKE_view_layer_synced_ensure(scene, view_layer);
  LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
    Object *ob = base->object;
    if (((base->flag & BASE_ENABLED_AND_MAYBE_VISIBLE_IN_VIEWPORT) != 0) &&
        BKE_object_is_child_recursive(ob_parent, ob))
    {
      object::base_select(base, select ? object::BA_SELECT : object::BA_DESELECT);
    }
  }
}

static void do_outliner_bone_select_recursive(bArmature *arm, Bone *bone_parent, bool select)
{
  LISTBASE_FOREACH (Bone *, bone, &bone_parent->childbase) {
    if (select && blender::animrig::bone_is_selectable(arm, bone)) {
      bone->flag |= BONE_SELECTED;
    }
    else {
      bone->flag &= ~(BONE_TIPSEL | BONE_SELECTED | BONE_ROOTSEL);
    }
    do_outliner_bone_select_recursive(arm, bone, select);
  }
}

static void do_outliner_ebone_select_recursive(bArmature *arm, EditBone *ebone_parent, bool select)
{
  EditBone *ebone;
  for (ebone = ebone_parent->next; ebone; ebone = ebone->next) {
    if (ED_armature_ebone_is_child_recursive(ebone_parent, ebone)) {
      if (select && EBONE_SELECTABLE(arm, ebone)) {
        ebone->flag |= BONE_TIPSEL | BONE_SELECTED | BONE_ROOTSEL;
      }
      else {
        ebone->flag &= ~(BONE_TIPSEL | BONE_SELECTED | BONE_ROOTSEL);
      }
    }
  }
}

static void tree_element_object_activate(bContext *C,
                                         Scene *scene,
                                         ViewLayer *view_layer,
                                         TreeElement *te,
                                         const eOLSetState set,
                                         bool recursive)
{
  TreeStoreElem *tselem = TREESTORE(te);
  TreeStoreElem *parent_tselem = nullptr;
  TreeElement *parent_te = nullptr;
  Scene *sce;
  Base *base;
  Object *ob = nullptr;

  /* if id is not object, we search back */
  if ((tselem->type == TSE_SOME_ID) && (te->idcode == ID_OB)) {
    ob = (Object *)tselem->id;
  }
  else {
    parent_te = outliner_search_back_te(te, ID_OB);
    if (parent_te) {
      parent_tselem = TREESTORE(parent_te);
      ob = (Object *)parent_tselem->id;

      /* Don't return when activating children of the previous active object. */
      BKE_view_layer_synced_ensure(scene, view_layer);
      if (ob == BKE_view_layer_active_object_get(view_layer) && set == OL_SETSEL_NONE) {
        return;
      }
    }
  }
  if (ob == nullptr) {
    return;
  }

  sce = (Scene *)outliner_search_back(te, ID_SCE);
  if (sce && scene != sce) {
    WM_window_set_active_scene(CTX_data_main(C), C, CTX_wm_window(C), sce);
    view_layer = WM_window_get_active_view_layer(CTX_wm_window(C));
    scene = sce;
  }

  /* find associated base in current scene */
  BKE_view_layer_synced_ensure(scene, view_layer);
  base = BKE_view_layer_base_find(view_layer, ob);

  if (scene->toolsettings->object_flag & SCE_OBJECT_MODE_LOCK) {
    if (base != nullptr) {
      Object *obact = BKE_view_layer_active_object_get(view_layer);
      const eObjectMode object_mode = obact ? (eObjectMode)obact->mode : OB_MODE_OBJECT;
      if (base && !BKE_object_is_mode_compat(base->object, object_mode)) {
        if (object_mode == OB_MODE_OBJECT) {
          Main *bmain = CTX_data_main(C);
          Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
          object::mode_generic_exit(bmain, depsgraph, scene, base->object);
        }
        if (!BKE_object_is_mode_compat(base->object, object_mode)) {
          base = nullptr;
        }
      }
    }
  }

  if (base) {
    if (set == OL_SETSEL_EXTEND) {
      /* swap select */
      if (base->flag & BASE_SELECTED) {
        object::base_select(base, object::BA_DESELECT);
        if (parent_tselem) {
          parent_tselem->flag &= ~TSE_SELECTED;
        }
      }
      else {
        object::base_select(base, object::BA_SELECT);
        if (parent_tselem) {
          parent_tselem->flag |= TSE_SELECTED;
        }
      }
    }
    else if (recursive) {
      /* Pass */
    }
    else {
      /* De-select all. */

      /* Only in object mode so we can switch the active object,
       * keeping all objects in the current 'mode' selected, useful for multi-pose/edit mode.
       * This keeps the convention that all objects in the current mode are also selected.
       * see #55246. */
      if ((scene->toolsettings->object_flag & SCE_OBJECT_MODE_LOCK) ?
              (ob->mode == OB_MODE_OBJECT) :
              true)
      {
        BKE_view_layer_base_deselect_all(scene, view_layer);
      }
      object::base_select(base, object::BA_SELECT);
      if (parent_tselem) {
        parent_tselem->flag |= TSE_SELECTED;
      }
    }

    if (recursive) {
      /* Recursive select/deselect for Object hierarchies */
      do_outliner_object_select_recursive(
          scene, view_layer, ob, (base->flag & BASE_SELECTED) != 0);
    }

    if (set != OL_SETSEL_NONE) {
      if (!recursive) {
        object::base_activate_with_mode_exit_if_needed(C, base); /* adds notifier */
      }
      DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
      WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
    }
  }
}

static void tree_element_material_activate(bContext *C,
                                           const Scene *scene,
                                           ViewLayer *view_layer,
                                           TreeElement *te)
{
  /* we search for the object parent */
  Object *ob = (Object *)outliner_search_back(te, ID_OB);
  /* NOTE: `ob->matbits` can be nullptr when a local object points to a library mesh. */
  BKE_view_layer_synced_ensure(scene, view_layer);
  if (ob == nullptr || ob != BKE_view_layer_active_object_get(view_layer) ||
      ob->matbits == nullptr)
  {
    return; /* just paranoia */
  }

  /* In ob mat array? */
  TreeElement *tes = te->parent;
  if (tes->idcode == ID_OB) {
    ob->actcol = te->index + 1;
    ob->matbits[te->index] = 1; /* Make ob material active too. */
  }
  else {
    /* or in obdata material */
    ob->actcol = te->index + 1;
    ob->matbits[te->index] = 0; /* Make obdata material active too. */
  }

  /* Tagging object for update seems a bit stupid here, but looks like we have to do it
   * for render views to update. See #42973.
   * Note that RNA material update does it too, see e.g. rna_MaterialSlot_update(). */
  DEG_id_tag_update((ID *)ob, ID_RECALC_TRANSFORM);
  WM_event_add_notifier(C, NC_MATERIAL | ND_SHADING_LINKS, nullptr);
}

static void tree_element_camera_activate(bContext *C, Scene *scene, TreeElement *te)
{
  Object *ob = (Object *)outliner_search_back(te, ID_OB);

  scene->camera = ob;

  Main *bmain = CTX_data_main(C);
  wmWindowManager *wm = static_cast<wmWindowManager *>(bmain->wm.first);

  WM_windows_scene_data_sync(&wm->windows, scene);
  DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);
  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_SCENE | NA_EDITED, nullptr);
}

static void tree_element_world_activate(bContext *C, Scene *scene, TreeElement *te)
{
  Scene *sce = nullptr;

  TreeElement *tep = te->parent;
  if (tep) {
    TreeStoreElem *tselem = TREESTORE(tep);
    if (tselem->type == TSE_SOME_ID) {
      sce = (Scene *)tselem->id;
    }
  }

  /* make new scene active */
  if (sce && scene != sce) {
    WM_window_set_active_scene(CTX_data_main(C), C, CTX_wm_window(C), sce);
  }
}

static void tree_element_defgroup_activate(bContext *C, TreeElement *te, TreeStoreElem *tselem)
{
  /* id in tselem is object */
  Object *ob = (Object *)tselem->id;
  BLI_assert(te->index + 1 >= 0);
  BKE_object_defgroup_active_index_set(ob, te->index + 1);

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, ob);
}

static void tree_element_gplayer_activate(bContext *C, TreeElement *te, TreeStoreElem *tselem)
{
  bGPdata *gpd = (bGPdata *)tselem->id;
  bGPDlayer *gpl = static_cast<bGPDlayer *>(te->directdata);

  /* We can only have a single "active" layer at a time
   * and there must always be an active layer... */
  if (gpl) {
    BKE_gpencil_layer_active_set(gpd, gpl);
    DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_SELECTED, gpd);
  }
}

static void tree_element_grease_pencil_node_activate(bContext *C,
                                                     TreeElement *te,
                                                     TreeStoreElem *tselem)
{
  GreasePencil &grease_pencil = *(GreasePencil *)tselem->id;
  bke::greasepencil::TreeNode &node = tree_element_cast<TreeElementGreasePencilNode>(te)->node();

  if (node.is_layer()) {
    if (grease_pencil.has_active_group()) {
      WM_msg_publish_rna_prop(CTX_wm_message_bus(C),
                              &grease_pencil.id,
                              &grease_pencil,
                              GreasePencilv3LayerGroup,
                              active);
    }
    WM_msg_publish_rna_prop(
        CTX_wm_message_bus(C), &grease_pencil.id, &grease_pencil, GreasePencilv3Layers, active);
  }
  if (node.is_group()) {
    if (grease_pencil.has_active_layer()) {
      WM_msg_publish_rna_prop(
          CTX_wm_message_bus(C), &grease_pencil.id, &grease_pencil, GreasePencilv3Layers, active);
    }
    WM_msg_publish_rna_prop(CTX_wm_message_bus(C),
                            &grease_pencil.id,
                            &grease_pencil,
                            GreasePencilv3LayerGroup,
                            active);
  }

  grease_pencil.set_active_node(&node);

  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_SELECTED, &grease_pencil);
}

static void tree_element_bonecollection_activate(bContext *C,
                                                 TreeElement *te,
                                                 TreeStoreElem *tselem)
{
  bArmature *arm = reinterpret_cast<bArmature *>(tselem->id);
  BoneCollection *bcoll = reinterpret_cast<BoneCollection *>(te->directdata);
  ANIM_armature_bonecoll_active_set(arm, bcoll);
  WM_event_add_notifier(C, NC_OBJECT | ND_BONE_COLLECTION, arm);
}

static void tree_element_posechannel_activate(bContext *C,
                                              const Scene *scene,
                                              ViewLayer *view_layer,
                                              TreeElement *te,
                                              TreeStoreElem *tselem,
                                              const eOLSetState set,
                                              bool recursive)
{
  Object *ob = (Object *)tselem->id;
  bArmature *arm = static_cast<bArmature *>(ob->data);
  bPoseChannel *pchan = static_cast<bPoseChannel *>(te->directdata);

  if (set != OL_SETSEL_EXTEND) {
    /* Single select forces all other bones to get unselected. */
    const Vector<Object *> objects = BKE_object_pose_array_get_unique(scene, view_layer, nullptr);

    for (Object *ob : objects) {
      Object *ob_iter = BKE_object_pose_armature_get(ob);

      /* Sanity checks. */
      if (ELEM(nullptr, ob_iter, ob_iter->pose, ob_iter->data)) {
        continue;
      }

      LISTBASE_FOREACH (bPoseChannel *, pchannel, &ob_iter->pose->chanbase) {
        pchannel->flag &= ~POSE_SELECTED;
      }

      if (ob != ob_iter) {
        DEG_id_tag_update(static_cast<ID *>(ob_iter->data), ID_RECALC_SELECT);
      }
    }
  }

  if ((set == OL_SETSEL_EXTEND) && (pchan->flag & POSE_SELECTED)) {
    pchan->flag &= ~POSE_SELECTED;
  }
  else {
    if (blender::animrig::bone_is_visible(arm, pchan)) {
      pchan->flag |= POSE_SELECTED;
    }
    arm->act_bone = pchan->bone;
  }

  if (recursive) {
    /* Recursive select/deselect */
    do_outliner_bone_select_recursive(arm, pchan->bone, (pchan->flag & POSE_SELECTED) != 0);
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_BONE_ACTIVE, ob);
  DEG_id_tag_update(&arm->id, ID_RECALC_SELECT);
}

static void tree_element_bone_activate(bContext *C,
                                       const Scene *scene,
                                       ViewLayer *view_layer,
                                       TreeElement *te,
                                       TreeStoreElem *tselem,
                                       const eOLSetState set,
                                       bool recursive)
{
  bArmature *arm = (bArmature *)tselem->id;
  Bone *bone = static_cast<Bone *>(te->directdata);

  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  if (ob) {
    if (set != OL_SETSEL_EXTEND) {
      /* single select forces all other bones to get unselected */
      for (Bone *bone_iter = static_cast<Bone *>(arm->bonebase.first); bone_iter != nullptr;
           bone_iter = bone_iter->next)
      {
        bone_iter->flag &= ~(BONE_TIPSEL | BONE_SELECTED | BONE_ROOTSEL);
        do_outliner_bone_select_recursive(arm, bone_iter, false);
      }
    }
  }

  if (set == OL_SETSEL_EXTEND && (bone->flag & BONE_SELECTED)) {
    bone->flag &= ~BONE_SELECTED;
  }
  else {
    if (blender::animrig::bone_is_visible(arm, bone) && ((bone->flag & BONE_UNSELECTABLE) == 0)) {
      bone->flag |= BONE_SELECTED;
    }
    arm->act_bone = bone;
  }

  if (recursive) {
    /* Recursive select/deselect */
    do_outliner_bone_select_recursive(arm, bone, (bone->flag & BONE_SELECTED) != 0);
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_BONE_ACTIVE, ob);
}

/** Edit-bones only draw in edit-mode armature. */
static void tree_element_active_ebone__sel(bContext *C, bArmature *arm, EditBone *ebone, short sel)
{
  if (sel) {
    arm->act_edbone = ebone;
  }
  if (EBONE_SELECTABLE(arm, ebone)) {
    ED_armature_ebone_select_set(ebone, sel);
  }
  WM_event_add_notifier(C, NC_OBJECT | ND_BONE_ACTIVE, CTX_data_edit_object(C));
}

static void tree_element_ebone_activate(bContext *C,
                                        const Scene *scene,
                                        ViewLayer *view_layer,
                                        TreeElement *te,
                                        TreeStoreElem *tselem,
                                        const eOLSetState set,
                                        bool recursive)
{
  bArmature *arm = (bArmature *)tselem->id;
  EditBone *ebone = static_cast<EditBone *>(te->directdata);

  if (set == OL_SETSEL_NORMAL) {
    ObjectsInModeParams ob_params{};
    ob_params.object_mode = OB_MODE_EDIT;
    ob_params.no_dup_data = true;

    Vector<Base *> bases = BKE_view_layer_array_from_bases_in_mode_params(
        scene, view_layer, nullptr, &ob_params);
    ED_armature_edit_deselect_all_multi_ex(bases);

    tree_element_active_ebone__sel(C, arm, ebone, true);
  }
  else if (set == OL_SETSEL_EXTEND) {
    if (!(ebone->flag & BONE_SELECTED)) {
      tree_element_active_ebone__sel(C, arm, ebone, true);
    }
    else {
      /* entirely selected, so de-select */
      tree_element_active_ebone__sel(C, arm, ebone, false);
    }
  }

  if (recursive) {
    /* Recursive select/deselect */
    do_outliner_ebone_select_recursive(arm, ebone, (ebone->flag & BONE_SELECTED) != 0);
  }
}

static void tree_element_modifier_activate(bContext *C,
                                           TreeElement *te,
                                           TreeStoreElem *tselem,
                                           const eOLSetState set)
{
  Object *ob = (Object *)tselem->id;
  ModifierData *md = (ModifierData *)te->directdata;

  if (set == OL_SETSEL_NORMAL) {
    BKE_object_modifier_set_active(ob, md);
    WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);
  }
}

static void tree_element_psys_activate(bContext *C, TreeStoreElem *tselem)
{
  Object *ob = (Object *)tselem->id;

  WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE | NA_EDITED, ob);
}

static void tree_element_constraint_activate(bContext *C,
                                             const Scene *scene,
                                             ViewLayer *view_layer,
                                             TreeElement *te,
                                             TreeStoreElem *tselem,
                                             const eOLSetState set)
{
  Object *ob = (Object *)tselem->id;

  /* Activate the parent bone if this is a bone constraint. */
  te = te->parent;
  while (te) {
    tselem = TREESTORE(te);
    if (tselem->type == TSE_POSE_CHANNEL) {
      tree_element_posechannel_activate(C, scene, view_layer, te, tselem, set, false);
      return;
    }
    te = te->parent;
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT, ob);
}

static void tree_element_strip_activate(bContext *C,
                                        WorkSpace *workspace,
                                        TreeElement *te,
                                        const eOLSetState set)
{
  Scene *sequencer_scene = workspace->sequencer_scene;
  if (!sequencer_scene) {
    return;
  }
  const TreeElementStrip *te_strip = tree_element_cast<TreeElementStrip>(te);
  Strip *strip = &te_strip->get_strip();
  Editing *ed = seq::editing_get(sequencer_scene);

  if (BLI_findindex(ed->current_strips(), strip) != -1) {
    if (set == OL_SETSEL_EXTEND) {
      seq::select_active_set(sequencer_scene, nullptr);
    }
    vse::deselect_all_strips(sequencer_scene);

    if ((set == OL_SETSEL_EXTEND) && strip->flag & SELECT) {
      strip->flag &= ~SELECT;
    }
    else {
      strip->flag |= SELECT;
      seq::select_active_set(sequencer_scene, strip);
    }
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER | NA_SELECTED, sequencer_scene);
}

static void tree_element_strip_dup_activate(WorkSpace *workspace, TreeElement * /*te*/)
{
  Scene *sequencer_scene = workspace->sequencer_scene;
  if (!sequencer_scene) {
    return;
  }
  Editing *ed = seq::editing_get(sequencer_scene);

#if 0
  select_single_seq(strip, 1);
#endif
  Strip *p = static_cast<Strip *>(ed->current_strips()->first);
  while (p) {
    if ((!p->data) || (!p->data->stripdata) || (p->data->stripdata->filename[0] == '\0')) {
      p = p->next;
      continue;
    }

#if 0
    if (STREQ(p->strip->stripdata->filename, strip->data->stripdata->filename)) {
      select_single_seq(p, 0);
    }
#endif
    p = p->next;
  }
}

static void tree_element_master_collection_activate(const bContext *C)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  LayerCollection *layer_collection = static_cast<LayerCollection *>(
      view_layer->layer_collections.first);
  BKE_layer_collection_activate(view_layer, layer_collection);
  /* A very precise notifier - ND_LAYER alone is quite vague, we want to avoid unnecessary work
   * when only the active collection changes. */
  WM_main_add_notifier(NC_SCENE | ND_LAYER | NS_LAYER_COLLECTION | NA_ACTIVATED, nullptr);
}

static void tree_element_layer_collection_activate(bContext *C, TreeElement *te)
{
  Scene *scene = CTX_data_scene(C);
  LayerCollection *layer_collection = static_cast<LayerCollection *>(te->directdata);
  ViewLayer *view_layer = BKE_view_layer_find_from_collection(scene, layer_collection);
  BKE_layer_collection_activate(view_layer, layer_collection);
  /* A very precise notifier - ND_LAYER alone is quite vague, we want to avoid unnecessary work
   * when only the active collection changes. */
  WM_main_add_notifier(NC_SCENE | ND_LAYER | NS_LAYER_COLLECTION | NA_ACTIVATED, nullptr);
}

static void tree_element_text_activate(bContext *C, TreeElement *te)
{
  Text *text = (Text *)te->store_elem->id;
  ED_text_activate_in_screen(C, text);
}

/* ---------------------------------------------- */

void tree_element_activate(bContext *C,
                           const TreeViewContext &tvc,
                           TreeElement *te,
                           const eOLSetState set,
                           const bool handle_all_types)
{
  switch (te->idcode) {
    /** \note #ID_OB only if handle_all_type is true,
     * else objects are handled specially to allow multiple selection.
     * See #do_outliner_item_activate. */
    case ID_OB:
      if (handle_all_types) {
        tree_element_object_activate(C, tvc.scene, tvc.view_layer, te, set, false);
      }
      break;
    case ID_MA:
      tree_element_material_activate(C, tvc.scene, tvc.view_layer, te);
      break;
    case ID_WO:
      tree_element_world_activate(C, tvc.scene, te);
      break;
    case ID_CA:
      tree_element_camera_activate(C, tvc.scene, te);
      break;
    case ID_TXT:
      tree_element_text_activate(C, te);
      break;
  }
}

void tree_element_type_active_set(bContext *C,
                                  const TreeViewContext &tvc,
                                  TreeElement *te,
                                  TreeStoreElem *tselem,
                                  const eOLSetState set,
                                  bool recursive)
{
  BLI_assert(set != OL_SETSEL_NONE);
  switch (tselem->type) {
    case TSE_DEFGROUP:
      tree_element_defgroup_activate(C, te, tselem);
      break;
    case TSE_BONE:
      tree_element_bone_activate(C, tvc.scene, tvc.view_layer, te, tselem, set, recursive);
      break;
    case TSE_EBONE:
      tree_element_ebone_activate(C, tvc.scene, tvc.view_layer, te, tselem, set, recursive);
      break;
    case TSE_MODIFIER:
      tree_element_modifier_activate(C, te, tselem, set);
      break;
    case TSE_LINKED_OB:
      tree_element_object_activate(C, tvc.scene, tvc.view_layer, te, set, false);
      break;
    case TSE_LINKED_PSYS:
      tree_element_psys_activate(C, tselem);
      break;
    case TSE_POSE_BASE:
      return;
    case TSE_POSE_CHANNEL:
      tree_element_posechannel_activate(C, tvc.scene, tvc.view_layer, te, tselem, set, recursive);
      break;
    case TSE_CONSTRAINT_BASE:
    case TSE_CONSTRAINT:
      tree_element_constraint_activate(C, tvc.scene, tvc.view_layer, te, tselem, set);
      break;
    case TSE_R_LAYER:
      tree_element_viewlayer_activate(C, te);
      break;
    case TSE_BONE_COLLECTION:
      tree_element_bonecollection_activate(C, te, tselem);
      break;
    case TSE_STRIP:
      tree_element_strip_activate(C, tvc.workspace, te, set);
      break;
    case TSE_STRIP_DUP:
      tree_element_strip_dup_activate(tvc.workspace, te);
      break;
    case TSE_GP_LAYER:
      tree_element_gplayer_activate(C, te, tselem);
      break;
    case TSE_GREASE_PENCIL_NODE:
      tree_element_grease_pencil_node_activate(C, te, tselem);
      break;
    case TSE_VIEW_COLLECTION_BASE:
      tree_element_master_collection_activate(C);
      break;
    case TSE_LAYER_COLLECTION:
      tree_element_layer_collection_activate(C, te);
      break;
  }
}

static eOLDrawState tree_element_defgroup_state_get(const Scene *scene,
                                                    ViewLayer *view_layer,
                                                    const TreeElement *te,
                                                    const TreeStoreElem *tselem)
{
  const Object *ob = (const Object *)tselem->id;
  BKE_view_layer_synced_ensure(scene, view_layer);
  if (ob == BKE_view_layer_active_object_get(view_layer)) {
    if (BKE_object_defgroup_active_index_get(ob) == te->index + 1) {
      return OL_DRAWSEL_NORMAL;
    }
  }
  return OL_DRAWSEL_NONE;
}

static eOLDrawState tree_element_bone_state_get(const Scene *scene,
                                                ViewLayer *view_layer,
                                                const TreeElement *te,
                                                const TreeStoreElem *tselem)
{
  const bArmature *arm = (const bArmature *)tselem->id;
  const Bone *bone = static_cast<Bone *>(te->directdata);
  BKE_view_layer_synced_ensure(scene, view_layer);
  const Object *ob = BKE_view_layer_active_object_get(view_layer);
  if (ob && ob->data == arm) {
    if (bone->flag & BONE_SELECTED) {
      return OL_DRAWSEL_NORMAL;
    }
  }
  return OL_DRAWSEL_NONE;
}

static eOLDrawState tree_element_ebone_state_get(const TreeElement *te)
{
  const EditBone *ebone = static_cast<EditBone *>(te->directdata);
  if (ebone->flag & BONE_SELECTED) {
    return OL_DRAWSEL_NORMAL;
  }
  return OL_DRAWSEL_NONE;
}

static eOLDrawState tree_element_modifier_state_get(const TreeElement *te,
                                                    const TreeStoreElem *tselem)
{
  const Object *ob = (const Object *)tselem->id;
  const ModifierData *md = (const ModifierData *)te->directdata;

  return (BKE_object_active_modifier(ob) == md) ? OL_DRAWSEL_NORMAL : OL_DRAWSEL_NONE;
}

static eOLDrawState tree_element_object_state_get(const TreeViewContext &tvc,
                                                  const TreeStoreElem *tselem)
{
  return (tselem->id == (const ID *)tvc.obact) ? OL_DRAWSEL_NORMAL : OL_DRAWSEL_NONE;
}

static eOLDrawState tree_element_pose_state_get(const Scene *scene,
                                                const ViewLayer *view_layer,
                                                const TreeStoreElem *tselem)
{
  const Object *ob = (const Object *)tselem->id;
  /* This will just lookup in a cache, it will not change the arguments. */
  BKE_view_layer_synced_ensure(scene, (ViewLayer *)view_layer);
  const Base *base = BKE_view_layer_base_find((ViewLayer *)view_layer, (Object *)ob);
  if (base == nullptr) {
    /* Armature not instantiated in current scene (e.g. inside an appended group). */
    return OL_DRAWSEL_NONE;
  }

  if (ob->mode & OB_MODE_POSE) {
    return OL_DRAWSEL_NORMAL;
  }
  return OL_DRAWSEL_NONE;
}

static eOLDrawState tree_element_posechannel_state_get(const Object *ob_pose,
                                                       const TreeElement *te,
                                                       const TreeStoreElem *tselem)
{
  const Object *ob = (const Object *)tselem->id;
  const bPoseChannel *pchan = static_cast<bPoseChannel *>(te->directdata);
  if (ob == ob_pose && ob->pose) {
    if (pchan->flag & POSE_SELECTED) {
      return OL_DRAWSEL_NORMAL;
    }
  }
  return OL_DRAWSEL_NONE;
}

static eOLDrawState tree_element_viewlayer_state_get(const ViewLayer *view_layer,
                                                     const TreeElement *te)
{
  const ViewLayer *te_view_layer = static_cast<ViewLayer *>(te->directdata);

  if (view_layer == te_view_layer) {
    return OL_DRAWSEL_NORMAL;
  }
  return OL_DRAWSEL_NONE;
}

static eOLDrawState tree_element_bone_collection_state_get(const TreeElement *te,
                                                           const TreeStoreElem *tselem)
{
  const bArmature *arm = reinterpret_cast<const bArmature *>(tselem->id);
  const BoneCollection *bcoll = reinterpret_cast<const BoneCollection *>(te->directdata);

  if (arm->runtime.active_collection == bcoll) {
    return OL_DRAWSEL_ACTIVE;
  }
  return OL_DRAWSEL_NONE;
}

static eOLDrawState tree_element_strip_state_get(const WorkSpace *workspace, const TreeElement *te)
{
  const Scene *sequencer_scene = workspace->sequencer_scene;
  if (!sequencer_scene) {
    return OL_DRAWSEL_NONE;
  }
  const TreeElementStrip *te_strip = tree_element_cast<TreeElementStrip>(te);
  const Strip *strip = &te_strip->get_strip();
  const Editing *ed = seq::editing_get(sequencer_scene);

  if (ed && ed->act_strip == strip && strip->flag & SELECT) {
    return OL_DRAWSEL_NORMAL;
  }
  return OL_DRAWSEL_NONE;
}

static eOLDrawState tree_element_strip_dup_state_get(const TreeElement *te)
{
  const TreeElementStripDuplicate *te_dup = tree_element_cast<TreeElementStripDuplicate>(te);
  const Strip *strip = &te_dup->get_strip();
  if (strip->flag & SELECT) {
    return OL_DRAWSEL_NORMAL;
  }
  return OL_DRAWSEL_NONE;
}

static eOLDrawState tree_element_gplayer_state_get(const TreeElement *te)
{
  if (((const bGPDlayer *)te->directdata)->flag & GP_LAYER_ACTIVE) {
    return OL_DRAWSEL_NORMAL;
  }
  return OL_DRAWSEL_NONE;
}

static eOLDrawState tree_element_grease_pencil_node_state_get(const TreeElement *te)
{
  GreasePencil &grease_pencil = *(GreasePencil *)te->store_elem->id;
  bke::greasepencil::TreeNode &node = tree_element_cast<TreeElementGreasePencilNode>(te)->node();
  if (node.is_layer() && grease_pencil.is_layer_active(&node.as_layer())) {
    return OL_DRAWSEL_NORMAL;
  }
  return OL_DRAWSEL_NONE;
}

static eOLDrawState tree_element_master_collection_state_get(
    const ViewLayer *view_layer, const LayerCollection *layer_collection)
{
  if (layer_collection == view_layer->layer_collections.first) {
    return OL_DRAWSEL_NORMAL;
  }
  return OL_DRAWSEL_NONE;
}

static eOLDrawState tree_element_layer_collection_state_get(
    const LayerCollection *layer_collection, const TreeElement *te)
{
  if (layer_collection == te->directdata) {
    return OL_DRAWSEL_NORMAL;
  }
  return OL_DRAWSEL_NONE;
}

static eOLDrawState tree_element_active_material_get(const Scene *scene,
                                                     ViewLayer *view_layer,
                                                     const TreeElement *te)
{
  /* we search for the object parent */
  const Object *ob = (const Object *)outliner_search_back((TreeElement *)te, ID_OB);
  /* NOTE: `ob->matbits` can be nullptr when a local object points to a library mesh. */
  BKE_view_layer_synced_ensure(scene, view_layer);
  if (ob == nullptr || ob != BKE_view_layer_active_object_get(view_layer) ||
      ob->matbits == nullptr)
  {
    return OL_DRAWSEL_NONE; /* just paranoia */
  }

  /* searching in ob mat array? */
  const TreeElement *tes = te->parent;
  if (tes->idcode == ID_OB) {
    if (ob->actcol == te->index + 1) {
      if (ob->matbits[te->index]) {
        return OL_DRAWSEL_NORMAL;
      }
    }
  }
  /* or we search for obdata material */
  else {
    if (ob->actcol == te->index + 1) {
      if (ob->matbits[te->index] == 0) {
        return OL_DRAWSEL_NORMAL;
      }
    }
  }
  return OL_DRAWSEL_NONE;
}

static eOLDrawState tree_element_active_scene_get(const TreeViewContext &tvc,
                                                  const TreeElement *te,
                                                  const TreeStoreElem *tselem)
{
  if (te->idcode == ID_SCE) {
    if (tselem->id == (ID *)tvc.scene) {
      return OL_DRAWSEL_NORMAL;
    }
  }
  return OL_DRAWSEL_NONE;
}

static eOLDrawState tree_element_active_world_get(const Scene *scene, const TreeElement *te)
{
  const TreeElement *tep = te->parent;
  if (tep == nullptr) {
    return OL_DRAWSEL_NORMAL;
  }

  const TreeStoreElem *tselem = TREESTORE(tep);
  if (tselem->id == (const ID *)scene) {
    return OL_DRAWSEL_NORMAL;
  }
  return OL_DRAWSEL_NONE;
}

static eOLDrawState tree_element_active_camera_get(const Scene *scene, const TreeElement *te)
{
  const Object *ob = (const Object *)outliner_search_back((TreeElement *)te, ID_OB);

  return (scene->camera == ob) ? OL_DRAWSEL_NORMAL : OL_DRAWSEL_NONE;
}

eOLDrawState tree_element_active_state_get(const TreeViewContext &tvc,
                                           const TreeElement *te,
                                           const TreeStoreElem *tselem)
{
  switch (te->idcode) {
    case ID_SCE:
      return tree_element_active_scene_get(tvc, te, tselem);
    case ID_OB:
      /* Objects are currently handled by the caller in order to also change text color. */
      return OL_DRAWSEL_NONE;
      break;
    case ID_MA:
      return tree_element_active_material_get(tvc.scene, tvc.view_layer, te);
    case ID_WO:
      return tree_element_active_world_get(tvc.scene, te);
    case ID_CA:
      return tree_element_active_camera_get(tvc.scene, te);
  }
  return OL_DRAWSEL_NONE;
}

eOLDrawState tree_element_type_active_state_get(const TreeViewContext &tvc,
                                                const TreeElement *te,
                                                const TreeStoreElem *tselem)
{
  switch (tselem->type) {
    case TSE_DEFGROUP:
      return tree_element_defgroup_state_get(tvc.scene, tvc.view_layer, te, tselem);
    case TSE_BONE:
      return tree_element_bone_state_get(tvc.scene, tvc.view_layer, te, tselem);
    case TSE_EBONE:
      return tree_element_ebone_state_get(te);
    case TSE_MODIFIER:
      return tree_element_modifier_state_get(te, tselem);
    case TSE_LINKED_NODE_TREE:
      return OL_DRAWSEL_NONE;
    case TSE_LINKED_OB:
      return tree_element_object_state_get(tvc, tselem);
    case TSE_LINKED_PSYS:
      return OL_DRAWSEL_NONE;
    case TSE_POSE_BASE:
      return tree_element_pose_state_get(tvc.scene, tvc.view_layer, tselem);
    case TSE_POSE_CHANNEL:
      return tree_element_posechannel_state_get(tvc.ob_pose, te, tselem);
    case TSE_CONSTRAINT_BASE:
    case TSE_CONSTRAINT:
      return OL_DRAWSEL_NONE;
    case TSE_R_LAYER:
      return tree_element_viewlayer_state_get(tvc.view_layer, te);
    case TSE_STRIP:
      return tree_element_strip_state_get(tvc.workspace, te);
    case TSE_STRIP_DUP:
      return tree_element_strip_dup_state_get(te);
    case TSE_GP_LAYER:
      return tree_element_gplayer_state_get(te);
    case TSE_GREASE_PENCIL_NODE:
      return tree_element_grease_pencil_node_state_get(te);
    case TSE_VIEW_COLLECTION_BASE:
      return tree_element_master_collection_state_get(tvc.view_layer, tvc.layer_collection);
    case TSE_LAYER_COLLECTION:
      return tree_element_layer_collection_state_get(tvc.layer_collection, te);
    case TSE_BONE_COLLECTION:
      return tree_element_bone_collection_state_get(te, tselem);
  }
  return OL_DRAWSEL_NONE;
}

bPoseChannel *outliner_find_parent_bone(TreeElement *te, TreeElement **r_bone_te)
{
  TreeStoreElem *tselem;

  te = te->parent;
  while (te) {
    tselem = TREESTORE(te);
    if (tselem->type == TSE_POSE_CHANNEL) {
      *r_bone_te = te;
      return (bPoseChannel *)te->directdata;
    }
    te = te->parent;
  }

  return nullptr;
}

static void outliner_sync_to_properties_editors(const bContext *C,
                                                PointerRNA *ptr,
                                                const int context)
{
  bScreen *screen = CTX_wm_screen(C);

  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    if (area->spacetype != SPACE_PROPERTIES) {
      continue;
    }

    SpaceProperties *sbuts = (SpaceProperties *)area->spacedata.first;
    if (ED_buttons_should_sync_with_outliner(C, sbuts, area)) {
      ED_buttons_set_context(C, sbuts, ptr, context);
    }
  }
}

static void outliner_set_properties_tab(bContext *C, TreeElement *te, TreeStoreElem *tselem)
{
  PointerRNA ptr = {};
  int context = 0;

  /* ID Types */
  if (tselem->type == TSE_SOME_ID) {
    ptr = RNA_id_pointer_create(tselem->id);

    switch (te->idcode) {
      case ID_SCE:
        context = BCONTEXT_SCENE;
        break;
      case ID_OB:
        context = BCONTEXT_OBJECT;
        break;
      case ID_ME:
      case ID_CU_LEGACY:
      case ID_MB:
      case ID_IM:
      case ID_LT:
      case ID_LA:
      case ID_CA:
      case ID_KE:
      case ID_SPK:
      case ID_AR:
      case ID_GD_LEGACY:
      case ID_GP:
      case ID_LP:
      case ID_CV:
      case ID_PT:
      case ID_VO:
        context = BCONTEXT_DATA;
        break;
      case ID_MA:
        context = BCONTEXT_MATERIAL;
        break;
      case ID_WO:
        context = BCONTEXT_WORLD;
        break;
    }
  }
  else {
    switch (tselem->type) {
      case TSE_DEFGROUP_BASE:
      case TSE_DEFGROUP:
        ptr = RNA_id_pointer_create(tselem->id);
        context = BCONTEXT_DATA;
        break;
      case TSE_CONSTRAINT_BASE:
      case TSE_CONSTRAINT: {
        TreeElement *bone_te = nullptr;
        bPoseChannel *pchan = outliner_find_parent_bone(te, &bone_te);

        if (pchan) {
          ptr = RNA_pointer_create_discrete(TREESTORE(bone_te)->id, &RNA_PoseBone, pchan);
          context = BCONTEXT_BONE_CONSTRAINT;
        }
        else {
          ptr = RNA_id_pointer_create(tselem->id);
          context = BCONTEXT_CONSTRAINT;
        }

        /* Expand the selected constraint in the properties editor. */
        if (tselem->type != TSE_CONSTRAINT_BASE) {
          BKE_constraint_panel_expand(static_cast<bConstraint *>(te->directdata));
        }
        break;
      }
      case TSE_MODIFIER_BASE:
      case TSE_MODIFIER:
        ptr = RNA_id_pointer_create(tselem->id);
        context = BCONTEXT_MODIFIER;

        if (tselem->type != TSE_MODIFIER_BASE) {
          ModifierData *md = (ModifierData *)te->directdata;

          switch ((ModifierType)md->type) {
            case eModifierType_ParticleSystem:
              context = BCONTEXT_PARTICLE;
              break;
            case eModifierType_Cloth:
            case eModifierType_Softbody:
            case eModifierType_Collision:
            case eModifierType_Fluidsim:
            case eModifierType_DynamicPaint:
            case eModifierType_Fluid:
              context = BCONTEXT_PHYSICS;
              break;
            default:
              break;
          }

          if (context == BCONTEXT_MODIFIER) {
            BKE_modifier_panel_expand(md);
          }
        }
        break;
      case TSE_LINKED_NODE_TREE:
        break;
      case TSE_GPENCIL_EFFECT_BASE:
      case TSE_GPENCIL_EFFECT:
        ptr = RNA_id_pointer_create(tselem->id);
        context = BCONTEXT_SHADERFX;

        if (tselem->type != TSE_GPENCIL_EFFECT_BASE) {
          BKE_shaderfx_panel_expand(static_cast<ShaderFxData *>(te->directdata));
        }
        break;
      case TSE_BONE: {
        bArmature *arm = (bArmature *)tselem->id;
        Bone *bone = static_cast<Bone *>(te->directdata);

        ptr = RNA_pointer_create_discrete(&arm->id, &RNA_Bone, bone);
        context = BCONTEXT_BONE;
        break;
      }
      case TSE_EBONE: {
        bArmature *arm = (bArmature *)tselem->id;
        EditBone *ebone = static_cast<EditBone *>(te->directdata);

        ptr = RNA_pointer_create_discrete(&arm->id, &RNA_EditBone, ebone);
        context = BCONTEXT_BONE;
        break;
      }
      case TSE_POSE_CHANNEL: {
        Object *ob = (Object *)tselem->id;
        bArmature *arm = static_cast<bArmature *>(ob->data);
        bPoseChannel *pchan = static_cast<bPoseChannel *>(te->directdata);

        ptr = RNA_pointer_create_discrete(&arm->id, &RNA_PoseBone, pchan);
        context = BCONTEXT_BONE;
        break;
      }
      case TSE_POSE_BASE: {
        Object *ob = (Object *)tselem->id;
        bArmature *arm = static_cast<bArmature *>(ob->data);

        ptr = RNA_pointer_create_discrete(&arm->id, &RNA_Armature, arm);
        context = BCONTEXT_DATA;
        break;
      }
      case TSE_R_LAYER: {
        ViewLayer *view_layer = static_cast<ViewLayer *>(te->directdata);

        ptr = RNA_pointer_create_discrete(tselem->id, &RNA_ViewLayer, view_layer);
        context = BCONTEXT_VIEW_LAYER;
        break;
      }
      case TSE_LINKED_PSYS: {
        Object *ob = (Object *)tselem->id;
        ParticleSystem *psys = psys_get_current(ob);

        ptr = RNA_pointer_create_discrete(&ob->id, &RNA_ParticleSystem, psys);
        context = BCONTEXT_PARTICLE;
        break;
      }
      case TSE_GP_LAYER:
      case TSE_GREASE_PENCIL_NODE:
        ptr = RNA_id_pointer_create(tselem->id);
        context = BCONTEXT_DATA;
        break;
      case TSE_BONE_COLLECTION_BASE:
        ptr = RNA_pointer_create_discrete(tselem->id, &RNA_Armature, tselem->id);
        context = BCONTEXT_DATA;
        break;
      case TSE_BONE_COLLECTION:
        ptr = RNA_pointer_create_discrete(tselem->id, &RNA_BoneCollection, te->directdata);
        context = BCONTEXT_DATA;
        break;
      case TSE_LAYER_COLLECTION:
        ptr = RNA_pointer_create_discrete(tselem->id, &RNA_Collection, te->directdata);
        context = BCONTEXT_COLLECTION;
        break;
    }
  }

  if (ptr.data) {
    outliner_sync_to_properties_editors(C, &ptr, context);
  }
}

/* ================================================ */

/**
 * Action when clicking to activate an item (typically under the mouse cursor),
 * but don't do any cursor intersection checks.
 *
 * Needed to run from operators accessed from a menu.
 */
static void do_outliner_item_activate_tree_element(bContext *C,
                                                   const TreeViewContext &tvc,
                                                   SpaceOutliner *space_outliner,
                                                   TreeElement *te,
                                                   TreeStoreElem *tselem,
                                                   const bool extend,
                                                   const bool recursive,
                                                   const bool do_activate_data)
{
  /* Always makes active object, except for some specific types. */
  if (ELEM(tselem->type,
           TSE_STRIP,
           TSE_STRIP_DATA,
           TSE_STRIP_DUP,
           TSE_EBONE,
           TSE_LINKED_NODE_TREE,
           TSE_LAYER_COLLECTION))
  {
    /* Note about TSE_EBONE: In case of a same ID_AR datablock shared among several
     * objects, we do not want to switch out of edit mode (see #48328 for details). */
  }
  else if (do_activate_data) {
    tree_element_object_activate(C,
                                 tvc.scene,
                                 tvc.view_layer,
                                 te,
                                 (extend && tselem->type == TSE_SOME_ID) ? OL_SETSEL_EXTEND :
                                                                           OL_SETSEL_NORMAL,
                                 recursive && tselem->type == TSE_SOME_ID);
  }
  else if (recursive && !(space_outliner->flag & SO_SYNC_SELECT)) {
    /* Selection of child objects in hierarchy when sync-selection is OFF. */
    tree_iterator::all(te->subtree, [&](TreeElement *te) {
      TreeStoreElem *tselem = TREESTORE(te);
      if ((tselem->type == TSE_SOME_ID) && (te->idcode == ID_OB)) {
        tselem->flag |= TSE_SELECTED;
      }
    });
  }

  if (tselem->type == TSE_SOME_ID) { /* The lib blocks. */
    if (do_activate_data == false) {
      /* Only select in outliner. */
    }
    else if (te->idcode == ID_SCE) {
      if (tvc.scene != (Scene *)tselem->id) {
        WM_window_set_active_scene(CTX_data_main(C), C, CTX_wm_window(C), (Scene *)tselem->id);
      }
    }
    else if ((te->idcode == ID_GR) && (space_outliner->outlinevis != SO_VIEW_LAYER)) {
      Collection *gr = (Collection *)tselem->id;
      BKE_view_layer_synced_ensure(tvc.scene, tvc.view_layer);

      if (extend) {
        object::eObjectSelect_Mode sel = object::BA_SELECT;
        FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (gr, object) {
          Base *base = BKE_view_layer_base_find(tvc.view_layer, object);
          if (base && (base->flag & BASE_SELECTED)) {
            sel = object::BA_DESELECT;
            break;
          }
        }
        FOREACH_COLLECTION_OBJECT_RECURSIVE_END;

        FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (gr, object) {
          Base *base = BKE_view_layer_base_find(tvc.view_layer, object);
          if (base) {
            object::base_select(base, sel);
          }
        }
        FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
      }
      else {
        BKE_view_layer_base_deselect_all(tvc.scene, tvc.view_layer);

        FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (gr, object) {
          Base *base = BKE_view_layer_base_find(tvc.view_layer, object);
          /* Object may not be in this scene */
          if (base != nullptr) {
            if ((base->flag & BASE_SELECTED) == 0) {
              object::base_select(base, object::BA_SELECT);
            }
          }
        }
        FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
      }

      DEG_id_tag_update(&tvc.scene->id, ID_RECALC_SELECT);
      WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, tvc.scene);
    }
    else { /* Rest of types. */
      tree_element_activate(C, tvc, te, OL_SETSEL_NORMAL, false);
    }
  }
  else if (do_activate_data) {
    tree_element_type_active_set(
        C, tvc, te, tselem, extend ? OL_SETSEL_EXTEND : OL_SETSEL_NORMAL, recursive);
  }
}

void outliner_item_select(bContext *C,
                          SpaceOutliner *space_outliner,
                          TreeElement *te,
                          const short select_flag)
{
  TreeStoreElem *tselem = TREESTORE(te);
  const bool activate = select_flag & OL_ITEM_ACTIVATE;
  const bool extend = select_flag & OL_ITEM_EXTEND;
  const bool activate_data = select_flag & OL_ITEM_SELECT_DATA;
  const bool recursive = select_flag & OL_ITEM_RECURSIVE;

  /* Clear previous active when activating and clear selection when not extending selection */
  const short clear_flag = (activate ? TSE_ACTIVE : 0) | (extend ? 0 : TSE_SELECTED);

  /* Do not clear the active and select flag when selecting hierarchies. */
  if (clear_flag && !recursive) {
    outliner_flag_set(*space_outliner, clear_flag, false);
  }

  if (select_flag & OL_ITEM_SELECT) {
    tselem->flag |= TSE_SELECTED;
  }
  else {
    tselem->flag &= ~TSE_SELECTED;
  }

  if (activate) {
    TreeViewContext tvc;
    outliner_viewcontext_init(C, &tvc);

    if (!recursive) {
      tselem->flag |= TSE_ACTIVE;
    }

    do_outliner_item_activate_tree_element(C,
                                           tvc,
                                           space_outliner,
                                           te,
                                           tselem,
                                           extend,
                                           select_flag & OL_ITEM_RECURSIVE,
                                           activate_data || space_outliner->flag & SO_SYNC_SELECT);
  }
}

static Collection *outliner_collection_get_for_recursive(bContext *C, TreeElement *te)
{
  /* If we're recursing, we need to know the collection of the selected item in order
   * to prevent selecting across collection boundaries. (Object hierarchies might cross
   * collection boundaries, i.e., children may be in different collections from their
   * parents.) */
  Collection *parent_collection = nullptr;
  if (te->store_elem->type == TSE_LAYER_COLLECTION) {
    parent_collection = static_cast<LayerCollection *>(te->directdata)->collection;
  }
  else if (te->store_elem->type == TSE_SOME_ID && te->idcode == ID_OB) {
    parent_collection = BKE_collection_object_find(CTX_data_main(C),
                                                   CTX_data_scene(C),
                                                   nullptr,
                                                   reinterpret_cast<Object *>(te->store_elem->id));
  }
  return parent_collection;
}

static bool can_select_recursive(TreeElement *te, Collection *in_collection)
{
  if (te->store_elem->type == TSE_LAYER_COLLECTION) {
    return true;
  }

  if (te->store_elem->type == TSE_SOME_ID && te->idcode == ID_OB) {
    /* Only actually select the object if
     * 1. We are not restricted to any collection, or
     * 2. The object is in fact in the given collection. */
    if (!in_collection || BKE_collection_has_object_recursive(
                              in_collection, reinterpret_cast<Object *>(te->store_elem->id)))
    {
      return true;
    }
  }

  return false;
}

static void do_outliner_select_recursive(ListBase *lb, bool selecting, Collection *in_collection)
{
  LISTBASE_FOREACH (TreeElement *, te, lb) {
    TreeStoreElem *tselem = TREESTORE(te);
    /* Recursive selection only on collections or objects. */
    if (can_select_recursive(te, in_collection)) {
      tselem->flag = selecting ? (tselem->flag | TSE_SELECTED) : (tselem->flag & ~TSE_SELECTED);
      if (tselem->type == TSE_LAYER_COLLECTION) {
        /* Restrict sub-tree selections to this collection. This prevents undesirable behavior in
         * the edge-case where there is an object which is part of this collection, but which has
         * children that are part of another collection. */
        do_outliner_select_recursive(
            &te->subtree, selecting, static_cast<LayerCollection *>(te->directdata)->collection);
      }
      else {
        do_outliner_select_recursive(&te->subtree, selecting, in_collection);
      }
    }
    else {
      tselem->flag &= ~TSE_SELECTED;
    }
  }
}

static bool do_outliner_range_select_recursive(ListBase *lb,
                                               TreeElement *active,
                                               TreeElement *cursor,
                                               bool selecting,
                                               const bool recurse,
                                               Collection *in_collection)
{
  LISTBASE_FOREACH (TreeElement *, te, lb) {
    TreeStoreElem *tselem = TREESTORE(te);

    bool can_select = !recurse || can_select_recursive(te, in_collection);

    /* Remember if we are selecting before we potentially change the selecting state. */
    bool selecting_before = selecting;

    /* Set state for selection */
    if (ELEM(te, active, cursor)) {
      selecting = !selecting;
    }

    if (can_select && (selecting_before || selecting)) {
      tselem->flag |= TSE_SELECTED;
    }

    /* Don't look inside closed elements, unless we're forcing the recursion all the way down. */
    if (!(tselem->flag & TSE_CLOSED) || recurse) {
      /* If this tree element is a collection, then it sets
       * the precedent for inclusion of its sub-objects. */
      Collection *child_collection = in_collection;
      if (tselem->type == TSE_LAYER_COLLECTION) {
        child_collection = static_cast<LayerCollection *>(te->directdata)->collection;
      }
      selecting = do_outliner_range_select_recursive(
          &te->subtree, active, cursor, selecting, recurse, child_collection);
    }
  }

  return selecting;
}

/* Select a range of items between cursor and active element */
static void do_outliner_range_select(bContext *C,
                                     SpaceOutliner *space_outliner,
                                     TreeElement *cursor,
                                     const bool extend,
                                     const bool recurse,
                                     Collection *in_collection)
{
  TreeElement *active = outliner_find_element_with_flag(&space_outliner->tree, TSE_ACTIVE);

  /* If no active element exists, activate the element under the cursor */
  if (!active) {
    outliner_item_select(C, space_outliner, cursor, OL_ITEM_SELECT | OL_ITEM_ACTIVATE);
    return;
  }

  TreeStoreElem *tselem = TREESTORE(active);
  const bool active_selected = (tselem->flag & TSE_SELECTED);

  if (!extend) {
    outliner_flag_set(*space_outliner, TSE_SELECTED, false);
  }

  /* Select active if under cursor */
  if (active == cursor) {
    outliner_item_select(C, space_outliner, cursor, OL_ITEM_SELECT);
    if (recurse) {
      do_outliner_select_recursive(&cursor->subtree, true, in_collection);
    }
    return;
  }

  /* If active is not selected or visible, select and activate the element under the cursor */
  if (!active_selected || !outliner_is_element_visible(active)) {
    outliner_item_select(C, space_outliner, cursor, OL_ITEM_SELECT | OL_ITEM_ACTIVATE);
    return;
  }

  do_outliner_range_select_recursive(
      &space_outliner->tree, active, cursor, false, recurse, in_collection);

  if (recurse) {
    do_outliner_select_recursive(&cursor->subtree, true, in_collection);
    /* Select children of active tree element. This is required when
     * range selecting from bottom to top, see #117224. */
    in_collection = outliner_collection_get_for_recursive(C, active);
    do_outliner_select_recursive(&active->subtree, true, in_collection);
  }
}

static bool outliner_is_co_within_restrict_columns(const SpaceOutliner *space_outliner,
                                                   const ARegion *region,
                                                   float view_co_x)
{
  return (view_co_x > region->v2d.cur.xmax - outliner_right_columns_width(space_outliner));
}

bool outliner_is_co_within_mode_column(SpaceOutliner *space_outliner, const float view_mval[2])
{
  if (!outliner_shows_mode_column(*space_outliner)) {
    return false;
  }

  return view_mval[0] < UI_UNIT_X;
}

static bool outliner_is_co_within_active_mode_column(bContext *C,
                                                     SpaceOutliner *space_outliner,
                                                     const float view_mval[2])
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *obact = BKE_view_layer_active_object_get(view_layer);

  return outliner_is_co_within_mode_column(space_outliner, view_mval) && obact &&
         obact->mode != OB_MODE_OBJECT;
}

/**
 * Action to run when clicking in the outliner,
 *
 * May expend/collapse branches or activate items.
 */
static wmOperatorStatus outliner_item_do_activate_from_cursor(bContext *C,
                                                              const int mval[2],
                                                              const bool extend,
                                                              const bool use_range,
                                                              const bool deselect_all,
                                                              const bool recurse)
{
  ARegion *region = CTX_wm_region(C);
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  TreeElement *te;
  float view_mval[2];
  bool changed = false, rebuild_tree = false;

  UI_view2d_region_to_view(&region->v2d, mval[0], mval[1], &view_mval[0], &view_mval[1]);

  if (outliner_is_co_within_restrict_columns(space_outliner, region, view_mval[0])) {
    return OPERATOR_CANCELLED;
  }
  if (outliner_is_co_within_active_mode_column(C, space_outliner, view_mval)) {
    return OPERATOR_CANCELLED;
  }

  if (!(te = outliner_find_item_at_y(space_outliner, &space_outliner->tree, view_mval[1]))) {
    if (deselect_all) {
      changed |= outliner_flag_set(*space_outliner, TSE_SELECTED, false);
    }
  }
  /* Don't allow toggle on scene collection */
  else if ((TREESTORE(te)->type != TSE_VIEW_COLLECTION_BASE) &&
           outliner_item_is_co_within_close_toggle(te, view_mval[0]))
  {
    return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
  }
  else {
    /* The row may also contain children, if one is hovered we want this instead of current te. */
    bool merged_elements = false;
    bool is_over_icon = false;
    TreeElement *activate_te = outliner_find_item_at_x_in_row(
        space_outliner, te, view_mval[0], &merged_elements, &is_over_icon);

    /* If the selected icon was an aggregate of multiple elements, run the search popup */
    if (merged_elements) {
      merged_element_search_menu_invoke(C, te, activate_te);
      return OPERATOR_CANCELLED;
    }

    TreeStoreElem *activate_tselem = TREESTORE(activate_te);

    Collection *parent_collection = nullptr;
    if (recurse) {
      parent_collection = outliner_collection_get_for_recursive(C, activate_te);
    }

    /* If we're not recursing (not double clicking), and we are extending or range selecting by
     * holding CTRL or SHIFT, ignore events when the cursor is over the icon. This disambiguates
     * the case where we are recursing *and* holding CTRL or SHIFT in order to extend or range
     * select recursively. */
    if (!recurse && (extend || use_range) && is_over_icon) {
      return OPERATOR_CANCELLED;
    }

    if (use_range) {
      do_outliner_range_select(
          C, space_outliner, activate_te, extend, (recurse && is_over_icon), parent_collection);
    }
    else {
      const bool is_over_name_icons = outliner_item_is_co_over_name_icons(activate_te,
                                                                          view_mval[0]);
      /* Always select unless already active and selected. */
      bool select = !extend || !(activate_tselem->flag & TSE_ACTIVE) ||
                    !(activate_tselem->flag & TSE_SELECTED);

      /* If we're CTRL+double-clicking and the element is already
       * selected, skip the activation and go straight to deselection. */
      if (extend && recurse && activate_tselem->flag & TSE_SELECTED) {
        select = false;
      }

      const short select_flag = OL_ITEM_ACTIVATE | (select ? OL_ITEM_SELECT : OL_ITEM_DESELECT) |
                                (is_over_name_icons ? OL_ITEM_SELECT_DATA : 0) |
                                (extend ? OL_ITEM_EXTEND : 0);

      /* The recurse flag is set when the user double-clicks
       * to select everything in a collection or hierarchy. */
      if (recurse) {
        if (is_over_icon) {
          /* Select or deselect object hierarchy recursively. */
          outliner_item_select(C, space_outliner, activate_te, select_flag);
          do_outliner_select_recursive(&activate_te->subtree, select, parent_collection);
        }
        else {
          /* Double-clicked, but it wasn't on the icon. */
          return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
        }
      }
      else {
        outliner_item_select(C, space_outliner, activate_te, select_flag);
      }

      /* Only switch properties editor tabs when icons are selected. */
      if (is_over_icon) {
        outliner_set_properties_tab(C, activate_te, activate_tselem);
      }
    }

    changed = true;
  }

  if (!changed) {
    return OPERATOR_CANCELLED;
  }

  if (rebuild_tree) {
    ED_region_tag_redraw(region);
  }
  else {
    ED_region_tag_redraw_no_rebuild(region);
  }

  ED_outliner_select_sync_from_outliner(C, space_outliner);

  return OPERATOR_FINISHED;
}

/* Event can enter-key, then it opens/closes. */
static wmOperatorStatus outliner_item_activate_invoke(bContext *C,
                                                      wmOperator *op,
                                                      const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);

  const bool extend = RNA_boolean_get(op->ptr, "extend");
  const bool use_range = RNA_boolean_get(op->ptr, "extend_range");
  const bool deselect_all = RNA_boolean_get(op->ptr, "deselect_all");
  const bool recurse = RNA_boolean_get(op->ptr, "recurse");

  int mval[2];
  WM_event_drag_start_mval(event, region, mval);
  return outliner_item_do_activate_from_cursor(C, mval, extend, use_range, deselect_all, recurse);
}

void OUTLINER_OT_item_activate(wmOperatorType *ot)
{
  ot->name = "Select";
  ot->idname = "OUTLINER_OT_item_activate";
  ot->description = "Handle mouse clicks to select and activate items";

  ot->invoke = outliner_item_activate_invoke;

  ot->poll = ED_operator_region_outliner_active;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  PropertyRNA *prop;
  prop = RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend selection for activation");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(
      ot->srna, "extend_range", false, "Extend Range", "Select a range from active element");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna,
                         "deselect_all",
                         false,
                         "Deselect On Nothing",
                         "Deselect all when nothing under the cursor");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(
      ot->srna, "recurse", false, "Recurse", "Select objects recursively from active element");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Box Select Operator
 * \{ */

static void outliner_box_select(bContext *C,
                                SpaceOutliner *space_outliner,
                                const rctf *rectf,
                                const bool select)
{
  tree_iterator::all_open(*space_outliner, [&](TreeElement *te) {
    if (te->ys <= rectf->ymax && te->ys + UI_UNIT_Y >= rectf->ymin) {
      outliner_item_select(
          C, space_outliner, te, (select ? OL_ITEM_SELECT : OL_ITEM_DESELECT) | OL_ITEM_EXTEND);
    }
  });
}

static wmOperatorStatus outliner_box_select_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  ARegion *region = CTX_wm_region(C);
  rctf rectf;

  const eSelectOp sel_op = (eSelectOp)RNA_enum_get(op->ptr, "mode");
  const bool select = (sel_op != SEL_OP_SUB);
  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    outliner_flag_set(*space_outliner, TSE_SELECTED, 0);
  }

  WM_operator_properties_border_to_rctf(op, &rectf);
  UI_view2d_region_to_view_rctf(&region->v2d, &rectf, &rectf);

  outliner_box_select(C, space_outliner, &rectf, select);

  DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
  ED_region_tag_redraw_no_rebuild(region);

  ED_outliner_select_sync_from_outliner(C, space_outliner);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus outliner_box_select_invoke(bContext *C,
                                                   wmOperator *op,
                                                   const wmEvent *event)
{
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  ARegion *region = CTX_wm_region(C);
  float view_mval[2];
  const bool tweak = RNA_boolean_get(op->ptr, "tweak");

  int mval[2];
  WM_event_drag_start_mval(event, region, mval);
  UI_view2d_region_to_view(&region->v2d, mval[0], mval[1], &view_mval[0], &view_mval[1]);

  /* Find element clicked on */
  TreeElement *te = outliner_find_item_at_y(space_outliner, &space_outliner->tree, view_mval[1]);

  /* Pass through if click is over name or icons, or not tweak event */
  if (te && tweak && outliner_item_is_co_over_name_icons(te, view_mval[0])) {
    return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
  }

  if (outliner_is_co_within_active_mode_column(C, space_outliner, view_mval)) {
    return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
  }

  return WM_gesture_box_invoke(C, op, event);
}

void OUTLINER_OT_select_box(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Box Select";
  ot->idname = "OUTLINER_OT_select_box";
  ot->description = "Use box selection to select tree elements";

  /* API callbacks. */
  ot->invoke = outliner_box_select_invoke;
  ot->exec = outliner_box_select_exec;
  ot->modal = WM_gesture_box_modal;
  ot->cancel = WM_gesture_box_cancel;

  ot->poll = ED_operator_region_outliner_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  PropertyRNA *prop;

  prop = RNA_def_boolean(
      ot->srna, "tweak", false, "Tweak", "Tweak gesture from empty space for box selection");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  WM_operator_properties_gesture_box(ot);
  WM_operator_properties_select_operation_simple(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Walk Select Operator
 * \{ */

/* Given a tree element return the rightmost child that is visible in the outliner */
static TreeElement *outliner_find_rightmost_visible_child(SpaceOutliner *space_outliner,
                                                          TreeElement *te)
{
  while (te->subtree.last) {
    if (TSELEM_OPEN(TREESTORE(te), space_outliner)) {
      te = static_cast<TreeElement *>(te->subtree.last);
    }
    else {
      break;
    }
  }
  return te;
}

/* Find previous visible element in the tree. */
static TreeElement *outliner_find_previous_element(SpaceOutliner *space_outliner, TreeElement *te)
{
  if (te->prev) {
    te = outliner_find_rightmost_visible_child(space_outliner, te->prev);
  }
  else if (te->parent) {
    /* Use parent if at beginning of list */
    te = te->parent;
  }

  return te;
}

/* Recursively search up the tree until a successor to a given element is found */
static TreeElement *outliner_element_find_successor_in_parents(TreeElement *te)
{
  TreeElement *successor = te;
  while (successor->parent) {
    if (successor->parent->next) {
      te = successor->parent->next;
      break;
    }
    successor = successor->parent;
  }

  return te;
}

/* Find next visible element in the tree */
static TreeElement *outliner_find_next_element(SpaceOutliner *space_outliner, TreeElement *te)
{
  TreeStoreElem *tselem = TREESTORE(te);

  if (TSELEM_OPEN(tselem, space_outliner) && te->subtree.first) {
    te = static_cast<TreeElement *>(te->subtree.first);
  }
  else if (te->next) {
    te = te->next;
  }
  else {
    te = outliner_element_find_successor_in_parents(te);
  }

  return te;
}

static TreeElement *outliner_walk_left(SpaceOutliner *space_outliner,
                                       TreeElement *te,
                                       bool toggle_all)
{
  TreeStoreElem *tselem = TREESTORE(te);

  if (TSELEM_OPEN(tselem, space_outliner)) {
    outliner_item_openclose(te, false, toggle_all);
  }
  /* Only walk up a level if the element is closed and not toggling expand */
  else if (!toggle_all && te->parent) {
    te = te->parent;
  }

  return te;
}

static TreeElement *outliner_walk_right(SpaceOutliner *space_outliner,
                                        TreeElement *te,
                                        bool toggle_all)
{
  TreeStoreElem *tselem = TREESTORE(te);

  /* Only walk down a level if the element is open and not toggling expand */
  if (!toggle_all && TSELEM_OPEN(tselem, space_outliner) && !BLI_listbase_is_empty(&te->subtree)) {
    te = static_cast<TreeElement *>(te->subtree.first);
  }
  else {
    outliner_item_openclose(te, true, toggle_all);
  }

  return te;
}

static TreeElement *do_outliner_select_walk(SpaceOutliner *space_outliner,
                                            TreeElement *te,
                                            const int direction,
                                            const bool extend,
                                            const bool toggle_all)
{
  TreeStoreElem *tselem = TREESTORE(te);

  switch (direction) {
    case UI_SELECT_WALK_UP:
      te = outliner_find_previous_element(space_outliner, te);
      break;
    case UI_SELECT_WALK_DOWN:
      te = outliner_find_next_element(space_outliner, te);
      break;
    case UI_SELECT_WALK_LEFT:
      te = outliner_walk_left(space_outliner, te, toggle_all);
      break;
    case UI_SELECT_WALK_RIGHT:
      te = outliner_walk_right(space_outliner, te, toggle_all);
      break;
  }

  /* If new element is already selected, deselect the previous element */
  TreeStoreElem *tselem_new = TREESTORE(te);
  if (extend) {
    tselem->flag = (tselem_new->flag & TSE_SELECTED) ? (tselem->flag & ~TSE_SELECTED) :
                                                       (tselem->flag | TSE_SELECTED);
  }

  return te;
}

/* Find the active element to walk from, or set one if none exists.
 * Changed is set to true if the active element is found, or false if it was set */
static TreeElement *find_walk_select_start_element(SpaceOutliner *space_outliner, bool *r_changed)
{
  TreeElement *active_te = outliner_find_element_with_flag(&space_outliner->tree, TSE_ACTIVE);
  *r_changed = false;

  /* If no active element exists, use the first element in the tree */
  if (!active_te) {
    active_te = static_cast<TreeElement *>(space_outliner->tree.first);
    *r_changed = true;
  }

  /* If the active element is not visible, activate the first visible parent element */
  if (!outliner_is_element_visible(active_te)) {
    while (!outliner_is_element_visible(active_te)) {
      active_te = active_te->parent;
    }
    *r_changed = true;
  }

  return active_te;
}

/* Scroll the outliner when the walk element reaches the top or bottom boundary */
static void outliner_walk_scroll(SpaceOutliner *space_outliner, ARegion *region, TreeElement *te)
{
  /* Account for the header height */
  int y_max = region->v2d.cur.ymax - UI_UNIT_Y;
  int y_min = region->v2d.cur.ymin;

  /* Scroll if walked position is beyond the border */
  if (te->ys > y_max) {
    outliner_scroll_view(space_outliner, region, te->ys - y_max);
  }
  else if (te->ys < y_min) {
    outliner_scroll_view(space_outliner, region, -(y_min - te->ys));
  }
}

static wmOperatorStatus outliner_walk_select_invoke(bContext *C,
                                                    wmOperator *op,
                                                    const wmEvent * /*event*/)
{
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  ARegion *region = CTX_wm_region(C);

  const short direction = RNA_enum_get(op->ptr, "direction");
  const bool extend = RNA_boolean_get(op->ptr, "extend");
  const bool toggle_all = RNA_boolean_get(op->ptr, "toggle_all");

  bool changed;
  TreeElement *active_te = find_walk_select_start_element(space_outliner, &changed);

  /* If finding the active element did not modify the selection, proceed to walk */
  if (!changed) {
    active_te = do_outliner_select_walk(space_outliner, active_te, direction, extend, toggle_all);
  }

  outliner_item_select(C,
                       space_outliner,
                       active_te,
                       OL_ITEM_SELECT | OL_ITEM_ACTIVATE | (extend ? OL_ITEM_EXTEND : 0));

  /* Scroll outliner to focus on walk element */
  outliner_walk_scroll(space_outliner, region, active_te);

  ED_outliner_select_sync_from_outliner(C, space_outliner);
  outliner_tag_redraw_avoid_rebuild_on_open_change(space_outliner, region);

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_select_walk(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Walk Select";
  ot->idname = "OUTLINER_OT_select_walk";
  ot->description = "Use walk navigation to select tree elements";

  /* API callbacks. */
  ot->invoke = outliner_walk_select_invoke;
  ot->poll = ED_operator_outliner_active;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  PropertyRNA *prop;
  WM_operator_properties_select_walk_direction(ot);
  prop = RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend selection on walk");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(
      ot->srna, "toggle_all", false, "Toggle All", "Toggle open/close hierarchy");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

}  // namespace blender::ed::outliner
