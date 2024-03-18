/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include <cstdio>

#include "DNA_armature_types.h"
#include "DNA_layer_types.h"
#include "DNA_outliner_types.h"
#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"
#include "DNA_space_types.h"

#include "BLI_compiler_compat.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"

#include "BKE_armature.hh"
#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_main.hh"

#include "DEG_depsgraph.hh"

#include "ED_armature.hh"
#include "ED_object.hh"
#include "ED_outliner.hh"

#include "SEQ_select.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ANIM_bone_collections.hh"

#include "tree/tree_element_seq.hh"

#include "outliner_intern.hh"

void ED_outliner_select_sync_from_object_tag(bContext *C)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wm->outliner_sync_select_dirty |= WM_OUTLINER_SYNC_SELECT_FROM_OBJECT;
}

void ED_outliner_select_sync_from_edit_bone_tag(bContext *C)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wm->outliner_sync_select_dirty |= WM_OUTLINER_SYNC_SELECT_FROM_EDIT_BONE;
}

void ED_outliner_select_sync_from_pose_bone_tag(bContext *C)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wm->outliner_sync_select_dirty |= WM_OUTLINER_SYNC_SELECT_FROM_POSE_BONE;
}

void ED_outliner_select_sync_from_sequence_tag(bContext *C)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wm->outliner_sync_select_dirty |= WM_OUTLINER_SYNC_SELECT_FROM_SEQUENCE;
}

void ED_outliner_select_sync_from_all_tag(bContext *C)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wm->outliner_sync_select_dirty |= WM_OUTLINER_SYNC_SELECT_FROM_ALL;
}

bool ED_outliner_select_sync_is_dirty(const bContext *C)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  return wm->outliner_sync_select_dirty & WM_OUTLINER_SYNC_SELECT_FROM_ALL;
}

void ED_outliner_select_sync_flag_outliners(const bContext *C)
{
  Main *bmain = CTX_data_main(C);
  wmWindowManager *wm = CTX_wm_manager(C);

  for (bScreen *screen = static_cast<bScreen *>(bmain->screens.first); screen;
       screen = static_cast<bScreen *>(screen->id.next))
  {
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
        if (sl->spacetype == SPACE_OUTLINER) {
          SpaceOutliner *space_outliner = (SpaceOutliner *)sl;

          space_outliner->sync_select_dirty |= wm->outliner_sync_select_dirty;
        }
      }
    }
  }

  /* Clear global sync flag */
  wm->outliner_sync_select_dirty = 0;
}

namespace blender::ed::outliner {

/**
 * Outliner sync select dirty flags are not enough to determine which types to sync,
 * outliner display mode also needs to be considered. This stores the types of data
 * to sync to increase code clarity.
 */
struct SyncSelectTypes {
  bool object;
  bool edit_bone;
  bool pose_bone;
  bool sequence;
};

/**
 * Set which types of data to sync when syncing selection from the outliner based on object
 * interaction mode and outliner display mode
 */
static void outliner_sync_select_from_outliner_set_types(bContext *C,
                                                         SpaceOutliner *space_outliner,
                                                         SyncSelectTypes *sync_types)
{
  TreeViewContext tvc;
  outliner_viewcontext_init(C, &tvc);

  const bool sequence_view = space_outliner->outlinevis == SO_SEQUENCE;

  sync_types->object = !sequence_view;
  sync_types->edit_bone = !sequence_view && (tvc.ob_edit && tvc.ob_edit->type == OB_ARMATURE);
  sync_types->pose_bone = !sequence_view && (tvc.ob_pose && tvc.ob_pose->mode == OB_MODE_POSE);
  sync_types->sequence = sequence_view;
}

/**
 * Current dirty flags and outliner display mode determine which type of syncing should occur.
 * This is to ensure sync flag data is not lost on sync in the wrong display mode.
 * Returns true if a sync is needed.
 */
static bool outliner_sync_select_to_outliner_set_types(const bContext *C,
                                                       SpaceOutliner *space_outliner,
                                                       SyncSelectTypes *sync_types)
{
  TreeViewContext tvc;
  outliner_viewcontext_init(C, &tvc);

  const bool sequence_view = space_outliner->outlinevis == SO_SEQUENCE;

  sync_types->object = !sequence_view &&
                       (space_outliner->sync_select_dirty & WM_OUTLINER_SYNC_SELECT_FROM_OBJECT);
  sync_types->edit_bone = !sequence_view && (tvc.ob_edit && tvc.ob_edit->type == OB_ARMATURE) &&
                          (space_outliner->sync_select_dirty &
                           WM_OUTLINER_SYNC_SELECT_FROM_EDIT_BONE);
  sync_types->pose_bone = !sequence_view && (tvc.ob_pose && tvc.ob_pose->mode == OB_MODE_POSE) &&
                          (space_outliner->sync_select_dirty &
                           WM_OUTLINER_SYNC_SELECT_FROM_POSE_BONE);
  sync_types->sequence = sequence_view && (space_outliner->sync_select_dirty &
                                           WM_OUTLINER_SYNC_SELECT_FROM_SEQUENCE);

  return sync_types->object || sync_types->edit_bone || sync_types->pose_bone ||
         sync_types->sequence;
}

/**
 * Stores items selected from a sync from the outliner. Prevents syncing the selection
 * state of the last instance of an object linked in multiple collections.
 */
struct SelectedItems {
  GSet *objects;
  GSet *edit_bones;
  GSet *pose_bones;
};

static void selected_items_init(SelectedItems *selected_items)
{
  selected_items->objects = BLI_gset_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, __func__);
  selected_items->edit_bones = BLI_gset_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, __func__);
  selected_items->pose_bones = BLI_gset_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, __func__);
}

static void selected_items_free(SelectedItems *selected_items)
{
  BLI_gset_free(selected_items->objects, nullptr);
  BLI_gset_free(selected_items->edit_bones, nullptr);
  BLI_gset_free(selected_items->pose_bones, nullptr);
}

/* Check if an instance of this object been selected by the sync */
static bool is_object_selected(GSet *selected_objects, Base *base)
{
  return BLI_gset_haskey(selected_objects, base);
}

/* Check if an instance of this edit bone been selected by the sync */
static bool is_edit_bone_selected(GSet *selected_ebones, EditBone *ebone)
{
  return BLI_gset_haskey(selected_ebones, ebone);
}

/* Check if an instance of this pose bone been selected by the sync */
static bool is_pose_bone_selected(GSet *selected_pbones, bPoseChannel *pchan)
{
  return BLI_gset_haskey(selected_pbones, pchan);
}

/* Add element's data to selected item set */
static void add_selected_item(GSet *selected, void *data)
{
  BLI_gset_add(selected, data);
}

static void outliner_select_sync_to_object(ViewLayer *view_layer,
                                           TreeElement *te,
                                           TreeStoreElem *tselem,
                                           GSet *selected_objects)
{
  Object *ob = (Object *)tselem->id;
  Base *base = (te->directdata) ? (Base *)te->directdata :
                                  BKE_view_layer_base_find(view_layer, ob);

  if (base && (base->flag & BASE_SELECTABLE)) {
    if (tselem->flag & TSE_SELECTED) {
      ED_object_base_select(base, BA_SELECT);

      add_selected_item(selected_objects, base);
    }
    else if (!is_object_selected(selected_objects, base)) {
      ED_object_base_select(base, BA_DESELECT);
    }
  }
}

static void outliner_select_sync_to_edit_bone(const Scene *scene,
                                              ViewLayer *view_layer,
                                              TreeElement *te,
                                              TreeStoreElem *tselem,
                                              GSet *selected_ebones)
{
  bArmature *arm = (bArmature *)tselem->id;
  EditBone *ebone = (EditBone *)te->directdata;

  short bone_flag = ebone->flag;

  if (EBONE_SELECTABLE(arm, ebone)) {
    if (tselem->flag & TSE_SELECTED) {
      ED_armature_ebone_select_set(ebone, true);
      add_selected_item(selected_ebones, ebone);
    }
    else if (!is_edit_bone_selected(selected_ebones, ebone)) {
      /* Don't flush to parent bone tip, synced selection is iterating the whole tree so
       * deselecting potential children with `ED_armature_ebone_select_set(ebone, false)`
       * would leave own tip deselected. */
      ebone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
    }
  }

  /* Tag if selection changed */
  if (bone_flag != ebone->flag) {
    BKE_view_layer_synced_ensure(scene, view_layer);
    Object *obedit = BKE_view_layer_edit_object_get(view_layer);
    DEG_id_tag_update(&arm->id, ID_RECALC_SELECT);
    WM_main_add_notifier(NC_OBJECT | ND_BONE_SELECT, obedit);
  }
}

static void outliner_select_sync_to_pose_bone(TreeElement *te,
                                              TreeStoreElem *tselem,
                                              GSet *selected_pbones)
{
  Object *ob = (Object *)tselem->id;
  bArmature *arm = static_cast<bArmature *>(ob->data);
  bPoseChannel *pchan = (bPoseChannel *)te->directdata;

  short bone_flag = pchan->bone->flag;

  if (PBONE_SELECTABLE(arm, pchan->bone)) {
    if (tselem->flag & TSE_SELECTED) {
      pchan->bone->flag |= BONE_SELECTED;

      add_selected_item(selected_pbones, pchan);
    }
    else if (!is_pose_bone_selected(selected_pbones, pchan)) {
      pchan->bone->flag &= ~BONE_SELECTED;
    }
  }

  /* Tag if selection changed */
  if (bone_flag != pchan->bone->flag) {
    DEG_id_tag_update(&arm->id, ID_RECALC_SELECT);
    WM_main_add_notifier(NC_OBJECT | ND_BONE_SELECT, ob);
  }
}

static void outliner_select_sync_to_sequence(Scene *scene, const TreeElement *te)
{
  const TreeStoreElem *tselem = TREESTORE(te);

  const TreeElementSequence *te_sequence = tree_element_cast<TreeElementSequence>(te);
  Sequence *seq = &te_sequence->get_sequence();

  if (tselem->flag & TSE_ACTIVE) {
    SEQ_select_active_set(scene, seq);
  }

  if (tselem->flag & TSE_SELECTED) {
    seq->flag |= SELECT;
  }
  else {
    seq->flag &= ~SELECT;
  }
}

/** Sync select and active flags from outliner to active view layer, bones, and sequencer. */
static void outliner_sync_selection_from_outliner(Scene *scene,
                                                  ViewLayer *view_layer,
                                                  ListBase *tree,
                                                  const SyncSelectTypes *sync_types,
                                                  SelectedItems *selected_items)
{

  LISTBASE_FOREACH (TreeElement *, te, tree) {
    TreeStoreElem *tselem = TREESTORE(te);

    if ((tselem->type == TSE_SOME_ID) && (te->idcode == ID_OB)) {
      if (sync_types->object) {
        outliner_select_sync_to_object(view_layer, te, tselem, selected_items->objects);
      }
    }
    else if (tselem->type == TSE_EBONE) {
      if (sync_types->edit_bone) {
        outliner_select_sync_to_edit_bone(
            scene, view_layer, te, tselem, selected_items->edit_bones);
      }
    }
    else if (tselem->type == TSE_POSE_CHANNEL) {
      if (sync_types->pose_bone) {
        outliner_select_sync_to_pose_bone(te, tselem, selected_items->pose_bones);
      }
    }
    else if (tselem->type == TSE_SEQUENCE) {
      if (sync_types->sequence) {
        outliner_select_sync_to_sequence(scene, te);
      }
    }

    outliner_sync_selection_from_outliner(
        scene, view_layer, &te->subtree, sync_types, selected_items);
  }
}

}  // namespace blender::ed::outliner

void ED_outliner_select_sync_from_outliner(bContext *C, SpaceOutliner *space_outliner)
{
  using namespace blender::ed::outliner;

  /* Don't sync if not checked or in certain outliner display modes */
  if (!(space_outliner->flag & SO_SYNC_SELECT) || ELEM(space_outliner->outlinevis,
                                                       SO_LIBRARIES,
                                                       SO_OVERRIDES_LIBRARY,
                                                       SO_DATA_API,
                                                       SO_ID_ORPHANS))
  {
    return;
  }

  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  SyncSelectTypes sync_types;
  outliner_sync_select_from_outliner_set_types(C, space_outliner, &sync_types);

  /* To store elements that have been selected to prevent linked object sync errors */
  SelectedItems selected_items;

  selected_items_init(&selected_items);

  outliner_sync_selection_from_outliner(
      scene, view_layer, &space_outliner->tree, &sync_types, &selected_items);

  selected_items_free(&selected_items);

  /* Tag for updates and clear dirty flag to prevent a sync to the outliner on draw. */
  if (sync_types.object) {
    space_outliner->sync_select_dirty &= ~WM_OUTLINER_SYNC_SELECT_FROM_OBJECT;
    DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
  }
  else if (sync_types.edit_bone) {
    space_outliner->sync_select_dirty &= ~WM_OUTLINER_SYNC_SELECT_FROM_EDIT_BONE;
  }
  else if (sync_types.pose_bone) {
    space_outliner->sync_select_dirty &= ~WM_OUTLINER_SYNC_SELECT_FROM_POSE_BONE;
  }
  if (sync_types.sequence) {
    space_outliner->sync_select_dirty &= ~WM_OUTLINER_SYNC_SELECT_FROM_SEQUENCE;
    WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER | NA_SELECTED, scene);
  }
}

namespace blender::ed::outliner {

static void outliner_select_sync_from_object(const Scene *scene,
                                             ViewLayer *view_layer,
                                             Object *obact,
                                             TreeElement *te,
                                             TreeStoreElem *tselem)
{
  Object *ob = (Object *)tselem->id;
  BKE_view_layer_synced_ensure(scene, view_layer);
  Base *base = (te->directdata) ? (Base *)te->directdata :
                                  BKE_view_layer_base_find(view_layer, ob);
  const bool is_selected = (base != nullptr) && ((base->flag & BASE_SELECTED) != 0);

  if (base && (ob == obact)) {
    tselem->flag |= TSE_ACTIVE;
  }
  else {
    tselem->flag &= ~TSE_ACTIVE;
  }

  if (is_selected) {
    tselem->flag |= TSE_SELECTED;
  }
  else {
    tselem->flag &= ~TSE_SELECTED;
  }
}

static void outliner_select_sync_from_edit_bone(EditBone *ebone_active,
                                                TreeElement *te,
                                                TreeStoreElem *tselem)
{
  EditBone *ebone = (EditBone *)te->directdata;

  if (ebone == ebone_active) {
    tselem->flag |= TSE_ACTIVE;
  }
  else {
    tselem->flag &= ~TSE_ACTIVE;
  }

  if (ebone->flag & BONE_SELECTED) {
    tselem->flag |= TSE_SELECTED;
  }
  else {
    tselem->flag &= ~TSE_SELECTED;
  }
}

static void outliner_select_sync_from_pose_bone(bPoseChannel *pchan_active,
                                                TreeElement *te,
                                                TreeStoreElem *tselem)
{
  bPoseChannel *pchan = (bPoseChannel *)te->directdata;
  Bone *bone = pchan->bone;

  if (pchan == pchan_active) {
    tselem->flag |= TSE_ACTIVE;
  }
  else {
    tselem->flag &= ~TSE_ACTIVE;
  }

  if (bone->flag & BONE_SELECTED) {
    tselem->flag |= TSE_SELECTED;
  }
  else {
    tselem->flag &= ~TSE_SELECTED;
  }
}

static void outliner_select_sync_from_sequence(Sequence *sequence_active, const TreeElement *te)
{
  TreeStoreElem *tselem = TREESTORE(te);

  const TreeElementSequence *te_sequence = tree_element_cast<TreeElementSequence>(te);
  const Sequence *seq = &te_sequence->get_sequence();

  if (seq == sequence_active) {
    tselem->flag |= TSE_ACTIVE;
  }
  else {
    tselem->flag &= ~TSE_ACTIVE;
  }

  if (seq->flag & SELECT) {
    tselem->flag |= TSE_SELECTED;
  }
  else {
    tselem->flag &= ~TSE_SELECTED;
  }
}

/**
 * Contains active object, bones, and sequence for syncing to prevent getting active data
 * repeatedly throughout syncing to the outliner.
 */
struct SyncSelectActiveData {
  Object *object;
  EditBone *edit_bone;
  bPoseChannel *pose_channel;
  Sequence *sequence;
};

/** Sync select and active flags from active view layer, bones, and sequences to the outliner. */
static void outliner_sync_selection_to_outliner(const Scene *scene,
                                                ViewLayer *view_layer,
                                                SpaceOutliner *space_outliner,
                                                ListBase *tree,
                                                SyncSelectActiveData *active_data,
                                                const SyncSelectTypes *sync_types)
{
  LISTBASE_FOREACH (TreeElement *, te, tree) {
    TreeStoreElem *tselem = TREESTORE(te);

    if ((tselem->type == TSE_SOME_ID) && te->idcode == ID_OB) {
      if (sync_types->object) {
        outliner_select_sync_from_object(scene, view_layer, active_data->object, te, tselem);
      }
    }
    else if (tselem->type == TSE_EBONE) {
      if (sync_types->edit_bone) {
        outliner_select_sync_from_edit_bone(active_data->edit_bone, te, tselem);
      }
    }
    else if (tselem->type == TSE_POSE_CHANNEL) {
      if (sync_types->pose_bone) {
        outliner_select_sync_from_pose_bone(active_data->pose_channel, te, tselem);
      }
    }
    else if (tselem->type == TSE_SEQUENCE) {
      if (sync_types->sequence) {
        outliner_select_sync_from_sequence(active_data->sequence, te);
      }
    }
    else {
      tselem->flag &= ~(TSE_SELECTED | TSE_ACTIVE);
    }

    /* Sync subtree elements */
    outliner_sync_selection_to_outliner(
        scene, view_layer, space_outliner, &te->subtree, active_data, sync_types);
  }
}

/* Get active data from context */
static void get_sync_select_active_data(const bContext *C, SyncSelectActiveData *active_data)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  active_data->object = BKE_view_layer_active_object_get(view_layer);
  active_data->edit_bone = CTX_data_active_bone(C);
  active_data->pose_channel = CTX_data_active_pose_bone(C);
  active_data->sequence = SEQ_select_active_get(scene);
}

void outliner_sync_selection(const bContext *C, SpaceOutliner *space_outliner)
{
  /* Set which types of data to sync from sync dirty flag and outliner display mode */
  SyncSelectTypes sync_types;
  const bool sync_required = outliner_sync_select_to_outliner_set_types(
      C, space_outliner, &sync_types);

  if (sync_required) {
    const Scene *scene = CTX_data_scene(C);
    ViewLayer *view_layer = CTX_data_view_layer(C);

    /* Store active object, bones, and sequence */
    SyncSelectActiveData active_data;
    get_sync_select_active_data(C, &active_data);

    outliner_sync_selection_to_outliner(
        scene, view_layer, space_outliner, &space_outliner->tree, &active_data, &sync_types);

    /* Keep any un-synced data in the dirty flag. */
    if (sync_types.object) {
      space_outliner->sync_select_dirty &= ~WM_OUTLINER_SYNC_SELECT_FROM_OBJECT;
    }
    if (sync_types.edit_bone) {
      space_outliner->sync_select_dirty &= ~WM_OUTLINER_SYNC_SELECT_FROM_EDIT_BONE;
    }
    if (sync_types.pose_bone) {
      space_outliner->sync_select_dirty &= ~WM_OUTLINER_SYNC_SELECT_FROM_POSE_BONE;
    }
    if (sync_types.sequence) {
      space_outliner->sync_select_dirty &= ~WM_OUTLINER_SYNC_SELECT_FROM_SEQUENCE;
    }
  }
}

}  // namespace blender::ed::outliner
