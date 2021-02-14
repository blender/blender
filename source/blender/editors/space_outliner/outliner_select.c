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
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spoutliner
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "DNA_armature_types.h"
#include "DNA_collection_types.h"
#include "DNA_constraint_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_light_types.h"
#include "DNA_material_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_shader_fx_types.h"
#include "DNA_world_types.h"

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BKE_armature.h"
#include "BKE_collection.h"
#include "BKE_constraint.h"
#include "BKE_context.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_shader_fx.h"
#include "BKE_workspace.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "ED_armature.h"
#include "ED_buttons.h"
#include "ED_gpencil.h"
#include "ED_object.h"
#include "ED_outliner.h"
#include "ED_screen.h"
#include "ED_select_utils.h"
#include "ED_sequencer.h"
#include "ED_undo.h"

#include "SEQ_select.h"
#include "SEQ_sequencer.h"

#include "WM_api.h"
#include "WM_toolsystem.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_view2d.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "outliner_intern.h"

/**
 * \note changes to selection are by convention and not essential.
 *
 * \note Handles own undo push.
 */
static void do_outliner_item_editmode_toggle(bContext *C, Scene *scene, Base *base)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = base->object;

  bool changed = false;
  if (BKE_object_is_in_editmode(ob)) {
    changed = ED_object_editmode_exit_ex(bmain, scene, ob, EM_FREEDATA);
    if (changed) {
      ED_object_base_select(base, BA_DESELECT);
      WM_event_add_notifier(C, NC_SCENE | ND_MODE | NS_MODE_OBJECT, NULL);
    }
  }
  else {
    changed = ED_object_editmode_enter_ex(CTX_data_main(C), scene, ob, EM_NO_CONTEXT);
    if (changed) {
      ED_object_base_select(base, BA_SELECT);
      WM_event_add_notifier(C, NC_SCENE | ND_MODE, NULL);
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
 * \note Handles own undo push.
 */
static void do_outliner_item_posemode_toggle(bContext *C, Scene *scene, Base *base)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = base->object;

  if (ID_IS_LINKED(ob)) {
    BKE_report(CTX_wm_reports(C), RPT_WARNING, "Cannot pose libdata");
    return;
  }

  bool changed = false;
  if (ob->mode & OB_MODE_POSE) {
    changed = ED_object_posemode_exit_ex(bmain, ob);
    if (changed) {
      ED_object_base_select(base, BA_DESELECT);
      WM_event_add_notifier(C, NC_SCENE | ND_MODE | NS_MODE_OBJECT, NULL);
    }
  }
  else {
    changed = ED_object_posemode_enter_ex(bmain, ob);
    if (changed) {
      ED_object_base_select(base, BA_SELECT);
      WM_event_add_notifier(C, NC_SCENE | ND_MODE | NS_MODE_POSE, NULL);
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
 * \note Handles own undo push.
 */
static void do_outliner_item_mode_toggle_generic(bContext *C, TreeViewContext *tvc, Base *base)
{
  const int active_mode = tvc->obact->mode;
  ED_undo_group_begin(C);

  if (ED_object_mode_set(C, OB_MODE_OBJECT)) {
    Base *base_active = BKE_view_layer_base_find(tvc->view_layer, tvc->obact);
    if (base_active != base) {
      BKE_view_layer_base_deselect_all(tvc->view_layer);
      BKE_view_layer_base_select_and_set_active(tvc->view_layer, base);
      DEG_id_tag_update(&tvc->scene->id, ID_RECALC_SELECT);
      ED_undo_push(C, "Change Active");

      /* Operator call does undo push. */
      ED_object_mode_set(C, active_mode);
      ED_outliner_select_sync_from_object_tag(C);
    }
  }
  ED_undo_group_end(C);
}

/* Toggle the item's interaction mode if supported */
void outliner_item_mode_toggle(bContext *C,
                               TreeViewContext *tvc,
                               TreeElement *te,
                               const bool do_extend)
{
  TreeStoreElem *tselem = TREESTORE(te);

  if (tselem->type == 0 && te->idcode == ID_OB) {
    Object *ob = (Object *)tselem->id;
    Base *base = BKE_view_layer_base_find(tvc->view_layer, ob);

    /* Hidden objects can be removed from the mode. */
    if (!base || (!(base->flag & BASE_VISIBLE_DEPSGRAPH) && (ob->mode != tvc->obact->mode))) {
      return;
    }

    if (!do_extend) {
      do_outliner_item_mode_toggle_generic(C, tvc, base);
    }
    else if (tvc->ob_edit && OB_TYPE_SUPPORT_EDITMODE(ob->type)) {
      do_outliner_item_editmode_toggle(C, tvc->scene, base);
    }
    else if (tvc->ob_pose && ob->type == OB_ARMATURE) {
      do_outliner_item_posemode_toggle(C, tvc->scene, base);
    }
  }
}

/* ****************************************************** */
/* Outliner Element Selection/Activation on Click */

static eOLDrawState active_viewlayer(bContext *C,
                                     Scene *UNUSED(scene),
                                     ViewLayer *UNUSED(sl),
                                     TreeElement *te,
                                     const eOLSetState set)
{
  /* paranoia check */
  if (te->idcode != ID_SCE) {
    return OL_DRAWSEL_NONE;
  }

  ViewLayer *view_layer = te->directdata;

  if (set != OL_SETSEL_NONE) {
    wmWindow *win = CTX_wm_window(C);
    Scene *scene = WM_window_get_active_scene(win);

    if (BLI_findindex(&scene->view_layers, view_layer) != -1) {
      WM_window_set_active_view_layer(win, view_layer);
      WM_event_add_notifier(C, NC_SCREEN | ND_LAYER, NULL);
    }
  }
  else {
    return CTX_data_view_layer(C) == view_layer;
  }
  return OL_DRAWSEL_NONE;
}

/**
 * Select object tree
 */
static void do_outliner_object_select_recursive(ViewLayer *view_layer,
                                                Object *ob_parent,
                                                bool select)
{
  Base *base;

  for (base = FIRSTBASE(view_layer); base; base = base->next) {
    Object *ob = base->object;
    if ((((base->flag & BASE_VISIBLE_DEPSGRAPH) != 0) &&
         BKE_object_is_child_recursive(ob_parent, ob))) {
      ED_object_base_select(base, select ? BA_SELECT : BA_DESELECT);
    }
  }
}

static void do_outliner_bone_select_recursive(bArmature *arm, Bone *bone_parent, bool select)
{
  Bone *bone;
  for (bone = bone_parent->childbase.first; bone; bone = bone->next) {
    if (select && PBONE_SELECTABLE(arm, bone)) {
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

static eOLDrawState tree_element_set_active_object(bContext *C,
                                                   Scene *scene,
                                                   ViewLayer *view_layer,
                                                   SpaceOutliner *UNUSED(space_outliner),
                                                   TreeElement *te,
                                                   const eOLSetState set,
                                                   bool recursive)
{
  TreeStoreElem *tselem = TREESTORE(te);
  TreeStoreElem *parent_tselem = NULL;
  TreeElement *parent_te = NULL;
  Scene *sce;
  Base *base;
  Object *ob = NULL;

  /* if id is not object, we search back */
  if (tselem->type == 0 && te->idcode == ID_OB) {
    ob = (Object *)tselem->id;
  }
  else {
    parent_te = outliner_search_back_te(te, ID_OB);
    if (parent_te) {
      parent_tselem = TREESTORE(parent_te);
      ob = (Object *)parent_tselem->id;

      /* Don't return when activating children of the previous active object. */
      if (ob == OBACT(view_layer) && set == OL_SETSEL_NONE) {
        return OL_DRAWSEL_NONE;
      }
    }
  }
  if (ob == NULL) {
    return OL_DRAWSEL_NONE;
  }

  sce = (Scene *)outliner_search_back(te, ID_SCE);
  if (sce && scene != sce) {
    WM_window_set_active_scene(CTX_data_main(C), C, CTX_wm_window(C), sce);
    scene = sce;
  }

  /* find associated base in current scene */
  base = BKE_view_layer_base_find(view_layer, ob);

  if (scene->toolsettings->object_flag & SCE_OBJECT_MODE_LOCK) {
    if (base != NULL) {
      Object *obact = OBACT(view_layer);
      const eObjectMode object_mode = obact ? obact->mode : OB_MODE_OBJECT;
      if (base && !BKE_object_is_mode_compat(base->object, object_mode)) {
        if (object_mode == OB_MODE_OBJECT) {
          struct Main *bmain = CTX_data_main(C);
          Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
          ED_object_mode_generic_exit(bmain, depsgraph, scene, base->object);
        }
        if (!BKE_object_is_mode_compat(base->object, object_mode)) {
          base = NULL;
        }
      }
    }
  }

  if (base) {
    if (set == OL_SETSEL_EXTEND) {
      /* swap select */
      if (base->flag & BASE_SELECTED) {
        ED_object_base_select(base, BA_DESELECT);
        if (parent_tselem) {
          parent_tselem->flag &= ~TSE_SELECTED;
        }
      }
      else {
        ED_object_base_select(base, BA_SELECT);
        if (parent_tselem) {
          parent_tselem->flag |= TSE_SELECTED;
        }
      }
    }
    else {
      /* deleselect all */

      /* Only in object mode so we can switch the active object,
       * keeping all objects in the current 'mode' selected, useful for multi-pose/edit mode.
       * This keeps the convention that all objects in the current mode are also selected.
       * see T55246. */
      if ((scene->toolsettings->object_flag & SCE_OBJECT_MODE_LOCK) ?
              (ob->mode == OB_MODE_OBJECT) :
              true) {
        BKE_view_layer_base_deselect_all(view_layer);
      }
      ED_object_base_select(base, BA_SELECT);
      if (parent_tselem) {
        parent_tselem->flag |= TSE_SELECTED;
      }
    }

    if (recursive) {
      /* Recursive select/deselect for Object hierarchies */
      do_outliner_object_select_recursive(view_layer, ob, (base->flag & BASE_SELECTED) != 0);
    }

    if (set != OL_SETSEL_NONE) {
      ED_object_base_activate_with_mode_exit_if_needed(C, base); /* adds notifier */
      DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
      WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
    }
  }
  return OL_DRAWSEL_NORMAL;
}

static eOLDrawState tree_element_active_material(bContext *C,
                                                 Scene *UNUSED(scene),
                                                 ViewLayer *view_layer,
                                                 TreeElement *te,
                                                 const eOLSetState set)
{
  TreeElement *tes;
  Object *ob;

  /* we search for the object parent */
  ob = (Object *)outliner_search_back(te, ID_OB);
  /* Note : ob->matbits can be NULL when a local object points to a library mesh. */
  if (ob == NULL || ob != OBACT(view_layer) || ob->matbits == NULL) {
    return OL_DRAWSEL_NONE; /* just paranoia */
  }

  /* searching in ob mat array? */
  tes = te->parent;
  if (tes->idcode == ID_OB) {
    if (set != OL_SETSEL_NONE) {
      ob->actcol = te->index + 1;
      ob->matbits[te->index] = 1; /* Make ob material active too. */
    }
    else {
      if (ob->actcol == te->index + 1) {
        if (ob->matbits[te->index]) {
          return OL_DRAWSEL_NORMAL;
        }
      }
    }
  }
  /* or we search for obdata material */
  else {
    if (set != OL_SETSEL_NONE) {
      ob->actcol = te->index + 1;
      ob->matbits[te->index] = 0; /* Make obdata material active too. */
    }
    else {
      if (ob->actcol == te->index + 1) {
        if (ob->matbits[te->index] == 0) {
          return OL_DRAWSEL_NORMAL;
        }
      }
    }
  }
  if (set != OL_SETSEL_NONE) {
    /* Tagging object for update seems a bit stupid here, but looks like we have to do it
     * for render views to update. See T42973.
     * Note that RNA material update does it too, see e.g. rna_MaterialSlot_update(). */
    DEG_id_tag_update((ID *)ob, ID_RECALC_TRANSFORM);
    WM_event_add_notifier(C, NC_MATERIAL | ND_SHADING_LINKS, NULL);
  }
  return OL_DRAWSEL_NONE;
}

static eOLDrawState tree_element_active_camera(bContext *C,
                                               Scene *scene,
                                               ViewLayer *UNUSED(view_layer),
                                               TreeElement *te,
                                               const eOLSetState set)
{
  Object *ob = (Object *)outliner_search_back(te, ID_OB);

  if (set != OL_SETSEL_NONE) {
    scene->camera = ob;

    Main *bmain = CTX_data_main(C);
    wmWindowManager *wm = bmain->wm.first;

    WM_windows_scene_data_sync(&wm->windows, scene);
    DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
    DEG_relations_tag_update(bmain);
    WM_event_add_notifier(C, NC_SCENE | NA_EDITED, NULL);

    return OL_DRAWSEL_NONE;
  }
  return scene->camera == ob;
}

static eOLDrawState tree_element_active_world(bContext *C,
                                              Scene *scene,
                                              ViewLayer *UNUSED(sl),
                                              SpaceOutliner *UNUSED(space_outliner),
                                              TreeElement *te,
                                              const eOLSetState set)
{
  TreeElement *tep;
  TreeStoreElem *tselem = NULL;
  Scene *sce = NULL;

  tep = te->parent;
  if (tep) {
    tselem = TREESTORE(tep);
    if (tselem->type == 0) {
      sce = (Scene *)tselem->id;
    }
  }

  if (set != OL_SETSEL_NONE) {
    /* make new scene active */
    if (sce && scene != sce) {
      WM_window_set_active_scene(CTX_data_main(C), C, CTX_wm_window(C), sce);
    }
  }

  if (tep == NULL || tselem->id == (ID *)scene) {
    if (set == OL_SETSEL_NONE) {
      return OL_DRAWSEL_NORMAL;
    }
  }
  return OL_DRAWSEL_NONE;
}

static eOLDrawState tree_element_active_defgroup(bContext *C,
                                                 ViewLayer *view_layer,
                                                 TreeElement *te,
                                                 TreeStoreElem *tselem,
                                                 const eOLSetState set)
{
  Object *ob;

  /* id in tselem is object */
  ob = (Object *)tselem->id;
  if (set != OL_SETSEL_NONE) {
    BLI_assert(te->index + 1 >= 0);
    ob->actdef = te->index + 1;

    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, ob);
  }
  else {
    if (ob == OBACT(view_layer)) {
      if (ob->actdef == te->index + 1) {
        return OL_DRAWSEL_NORMAL;
      }
    }
  }
  return OL_DRAWSEL_NONE;
}

static eOLDrawState tree_element_active_gplayer(bContext *C,
                                                Scene *UNUSED(scene),
                                                TreeElement *te,
                                                TreeStoreElem *tselem,
                                                const eOLSetState set)
{
  bGPdata *gpd = (bGPdata *)tselem->id;
  bGPDlayer *gpl = te->directdata;

  /* We can only have a single "active" layer at a time
   * and there must always be an active layer...
   */
  if (set != OL_SETSEL_NONE) {
    if (gpl) {
      BKE_gpencil_layer_active_set(gpd, gpl);
      DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);
      WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_SELECTED, gpd);
    }
  }
  else {
    return OL_DRAWSEL_NORMAL;
  }

  return OL_DRAWSEL_NONE;
}

static eOLDrawState tree_element_active_posegroup(bContext *C,
                                                  Scene *UNUSED(scene),
                                                  ViewLayer *view_layer,
                                                  TreeElement *te,
                                                  TreeStoreElem *tselem,
                                                  const eOLSetState set)
{
  Object *ob = (Object *)tselem->id;

  if (set != OL_SETSEL_NONE) {
    if (ob->pose) {
      ob->pose->active_group = te->index + 1;
      WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
    }
  }
  else {
    if (ob == OBACT(view_layer) && ob->pose) {
      if (ob->pose->active_group == te->index + 1) {
        return OL_DRAWSEL_NORMAL;
      }
    }
  }
  return OL_DRAWSEL_NONE;
}

static eOLDrawState tree_element_active_posechannel(bContext *C,
                                                    Scene *UNUSED(scene),
                                                    ViewLayer *view_layer,
                                                    Object *ob_pose,
                                                    TreeElement *te,
                                                    TreeStoreElem *tselem,
                                                    const eOLSetState set,
                                                    bool recursive)
{
  Object *ob = (Object *)tselem->id;
  bArmature *arm = ob->data;
  bPoseChannel *pchan = te->directdata;

  if (set != OL_SETSEL_NONE) {
    if (!(pchan->bone->flag & BONE_HIDDEN_P)) {

      if (set != OL_SETSEL_EXTEND) {
        /* Single select forces all other bones to get unselected. */
        uint objects_len = 0;
        Object **objects = BKE_object_pose_array_get_unique(view_layer, NULL, &objects_len);

        for (uint object_index = 0; object_index < objects_len; object_index++) {
          Object *ob_iter = BKE_object_pose_armature_get(objects[object_index]);

          /* Sanity checks. */
          if (ELEM(NULL, ob_iter, ob_iter->pose, ob_iter->data)) {
            continue;
          }

          bPoseChannel *pchannel;
          for (pchannel = ob_iter->pose->chanbase.first; pchannel; pchannel = pchannel->next) {
            pchannel->bone->flag &= ~(BONE_TIPSEL | BONE_SELECTED | BONE_ROOTSEL);
          }

          if (ob != ob_iter) {
            DEG_id_tag_update(ob_iter->data, ID_RECALC_SELECT);
          }
        }
        MEM_freeN(objects);
      }

      if ((set == OL_SETSEL_EXTEND) && (pchan->bone->flag & BONE_SELECTED)) {
        pchan->bone->flag &= ~BONE_SELECTED;
      }
      else {
        pchan->bone->flag |= BONE_SELECTED;
        arm->act_bone = pchan->bone;
      }

      if (recursive) {
        /* Recursive select/deselect */
        do_outliner_bone_select_recursive(
            arm, pchan->bone, (pchan->bone->flag & BONE_SELECTED) != 0);
      }

      WM_event_add_notifier(C, NC_OBJECT | ND_BONE_ACTIVE, ob);
      DEG_id_tag_update(&arm->id, ID_RECALC_SELECT);
    }
  }
  else {
    if (ob == ob_pose && ob->pose) {
      if (pchan->bone->flag & BONE_SELECTED) {
        return OL_DRAWSEL_NORMAL;
      }
    }
  }
  return OL_DRAWSEL_NONE;
}

static eOLDrawState tree_element_active_bone(bContext *C,
                                             ViewLayer *view_layer,
                                             TreeElement *te,
                                             TreeStoreElem *tselem,
                                             const eOLSetState set,
                                             bool recursive)
{
  bArmature *arm = (bArmature *)tselem->id;
  Bone *bone = te->directdata;

  if (set != OL_SETSEL_NONE) {
    if (!(bone->flag & BONE_HIDDEN_P)) {
      Object *ob = OBACT(view_layer);
      if (ob) {
        if (set != OL_SETSEL_EXTEND) {
          /* single select forces all other bones to get unselected */
          for (Bone *bone_iter = arm->bonebase.first; bone_iter != NULL;
               bone_iter = bone_iter->next) {
            bone_iter->flag &= ~(BONE_TIPSEL | BONE_SELECTED | BONE_ROOTSEL);
            do_outliner_bone_select_recursive(arm, bone_iter, false);
          }
        }
      }

      if (set == OL_SETSEL_EXTEND && (bone->flag & BONE_SELECTED)) {
        bone->flag &= ~BONE_SELECTED;
      }
      else {
        bone->flag |= BONE_SELECTED;
        arm->act_bone = bone;
      }

      if (recursive) {
        /* Recursive select/deselect */
        do_outliner_bone_select_recursive(arm, bone, (bone->flag & BONE_SELECTED) != 0);
      }

      WM_event_add_notifier(C, NC_OBJECT | ND_BONE_ACTIVE, ob);
    }
  }
  else {
    Object *ob = OBACT(view_layer);

    if (ob && ob->data == arm) {
      if (bone->flag & BONE_SELECTED) {
        return OL_DRAWSEL_NORMAL;
      }
    }
  }
  return OL_DRAWSEL_NONE;
}

/** Edit-bones only draw in edit-mode armature. */
static void tree_element_active_ebone__sel(bContext *C, bArmature *arm, EditBone *ebone, short sel)
{
  if (sel) {
    arm->act_edbone = ebone;
  }
  ED_armature_ebone_select_set(ebone, sel);
  WM_event_add_notifier(C, NC_OBJECT | ND_BONE_ACTIVE, CTX_data_edit_object(C));
}
static eOLDrawState tree_element_active_ebone(bContext *C,
                                              ViewLayer *view_layer,
                                              TreeElement *te,
                                              TreeStoreElem *tselem,
                                              const eOLSetState set,
                                              bool recursive)
{
  bArmature *arm = (bArmature *)tselem->id;
  EditBone *ebone = te->directdata;
  eOLDrawState status = OL_DRAWSEL_NONE;

  if (set != OL_SETSEL_NONE) {
    if (set == OL_SETSEL_NORMAL) {
      if (!(ebone->flag & BONE_HIDDEN_A)) {
        uint bases_len = 0;
        Base **bases = BKE_view_layer_array_from_bases_in_edit_mode_unique_data(
            view_layer, NULL, &bases_len);
        ED_armature_edit_deselect_all_multi_ex(bases, bases_len);
        MEM_freeN(bases);

        tree_element_active_ebone__sel(C, arm, ebone, true);
        status = OL_DRAWSEL_NORMAL;
      }
    }
    else if (set == OL_SETSEL_EXTEND) {
      if (!(ebone->flag & BONE_HIDDEN_A)) {
        if (!(ebone->flag & BONE_SELECTED)) {
          tree_element_active_ebone__sel(C, arm, ebone, true);
          status = OL_DRAWSEL_NORMAL;
        }
        else {
          /* entirely selected, so de-select */
          tree_element_active_ebone__sel(C, arm, ebone, false);
          status = OL_DRAWSEL_NONE;
        }
      }
    }

    if (recursive) {
      /* Recursive select/deselect */
      do_outliner_ebone_select_recursive(arm, ebone, (ebone->flag & BONE_SELECTED) != 0);
    }
  }
  else if (ebone->flag & BONE_SELECTED) {
    status = OL_DRAWSEL_NORMAL;
  }

  return status;
}

static eOLDrawState tree_element_active_modifier(bContext *C,
                                                 Scene *UNUSED(scene),
                                                 ViewLayer *UNUSED(sl),
                                                 TreeElement *UNUSED(te),
                                                 TreeStoreElem *tselem,
                                                 const eOLSetState set)
{
  if (set != OL_SETSEL_NONE) {
    Object *ob = (Object *)tselem->id;

    WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);
  }

  return OL_DRAWSEL_NONE;
}

static eOLDrawState tree_element_active_psys(bContext *C,
                                             Scene *UNUSED(scene),
                                             TreeElement *UNUSED(te),
                                             TreeStoreElem *tselem,
                                             const eOLSetState set)
{
  if (set != OL_SETSEL_NONE) {
    Object *ob = (Object *)tselem->id;

    WM_event_add_notifier(C, NC_OBJECT | ND_PARTICLE | NA_EDITED, ob);
  }

  return OL_DRAWSEL_NONE;
}

static int tree_element_active_constraint(bContext *C,
                                          Scene *scene,
                                          ViewLayer *view_layer,
                                          TreeElement *te,
                                          TreeStoreElem *tselem,
                                          const eOLSetState set)
{
  if (set != OL_SETSEL_NONE) {
    Object *ob = (Object *)tselem->id;

    /* Activate the parent bone if this is a bone constraint. */
    te = te->parent;
    while (te) {
      tselem = TREESTORE(te);
      if (tselem->type == TSE_POSE_CHANNEL) {
        tree_element_active_posechannel(C, scene, view_layer, ob, te, tselem, set, false);
        return OL_DRAWSEL_NONE;
      }
      te = te->parent;
    }

    WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT, ob);
  }

  return OL_DRAWSEL_NONE;
}

static eOLDrawState tree_element_active_pose(bContext *UNUSED(C),
                                             Scene *UNUSED(scene),
                                             ViewLayer *view_layer,
                                             TreeElement *UNUSED(te),
                                             TreeStoreElem *tselem,
                                             const eOLSetState set)
{
  Object *ob = (Object *)tselem->id;
  Base *base = BKE_view_layer_base_find(view_layer, ob);

  if (base == NULL) {
    /* Armature not instantiated in current scene (e.g. inside an appended group...). */
    return OL_DRAWSEL_NONE;
  }

  if (set != OL_SETSEL_NONE) {
  }
  else {
    if (ob->mode & OB_MODE_POSE) {
      return OL_DRAWSEL_NORMAL;
    }
  }
  return OL_DRAWSEL_NONE;
}

static eOLDrawState tree_element_active_sequence(bContext *C,
                                                 Scene *scene,
                                                 TreeElement *te,
                                                 TreeStoreElem *UNUSED(tselem),
                                                 const eOLSetState set)
{
  Sequence *seq = (Sequence *)te->directdata;
  Editing *ed = SEQ_editing_get(scene, false);

  if (set != OL_SETSEL_NONE) {
    /* only check on setting */
    if (BLI_findindex(ed->seqbasep, seq) != -1) {
      if (set == OL_SETSEL_EXTEND) {
        SEQ_select_active_set(scene, NULL);
      }
      ED_sequencer_deselect_all(scene);

      if ((set == OL_SETSEL_EXTEND) && seq->flag & SELECT) {
        seq->flag &= ~SELECT;
      }
      else {
        seq->flag |= SELECT;
        SEQ_select_active_set(scene, seq);
      }
    }

    WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER | NA_SELECTED, scene);
  }
  else {
    if (ed->act_seq == seq && seq->flag & SELECT) {
      return OL_DRAWSEL_NORMAL;
    }
  }
  return OL_DRAWSEL_NONE;
}

static eOLDrawState tree_element_active_sequence_dup(Scene *scene,
                                                     TreeElement *te,
                                                     TreeStoreElem *UNUSED(tselem),
                                                     const eOLSetState set)
{
  Sequence *seq, *p;
  Editing *ed = SEQ_editing_get(scene, false);

  seq = (Sequence *)te->directdata;
  if (set == OL_SETSEL_NONE) {
    if (seq->flag & SELECT) {
      return OL_DRAWSEL_NORMAL;
    }
    return OL_DRAWSEL_NONE;
  }

  /* XXX  select_single_seq(seq, 1); */
  p = ed->seqbasep->first;
  while (p) {
    if ((!p->strip) || (!p->strip->stripdata) || (p->strip->stripdata->name[0] == '\0')) {
      p = p->next;
      continue;
    }

    /* XXX: if (STREQ(p->strip->stripdata->name, seq->strip->stripdata->name)) select_single_seq(p,
     * 0); */
    p = p->next;
  }
  return OL_DRAWSEL_NONE;
}

static eOLDrawState tree_element_active_master_collection(bContext *C,
                                                          TreeElement *UNUSED(te),
                                                          const eOLSetState set)
{
  if (set == OL_SETSEL_NONE) {
    ViewLayer *view_layer = CTX_data_view_layer(C);
    LayerCollection *active = CTX_data_layer_collection(C);

    if (active == view_layer->layer_collections.first) {
      return OL_DRAWSEL_NORMAL;
    }
  }
  else {
    ViewLayer *view_layer = CTX_data_view_layer(C);
    LayerCollection *layer_collection = view_layer->layer_collections.first;
    BKE_layer_collection_activate(view_layer, layer_collection);
    /* A very precise notifier - ND_LAYER alone is quite vague, we want to avoid unnecessary work
     * when only the active collection changes. */
    WM_main_add_notifier(NC_SCENE | ND_LAYER | NS_LAYER_COLLECTION | NA_ACTIVATED, NULL);
  }

  return OL_DRAWSEL_NONE;
}

static eOLDrawState tree_element_active_layer_collection(bContext *C,
                                                         TreeElement *te,
                                                         const eOLSetState set)
{
  if (set == OL_SETSEL_NONE) {
    LayerCollection *active = CTX_data_layer_collection(C);

    if (active == te->directdata) {
      return OL_DRAWSEL_NORMAL;
    }
  }
  else {
    Scene *scene = CTX_data_scene(C);
    LayerCollection *layer_collection = te->directdata;
    ViewLayer *view_layer = BKE_view_layer_find_from_collection(scene, layer_collection);
    BKE_layer_collection_activate(view_layer, layer_collection);
    /* A very precise notifier - ND_LAYER alone is quite vague, we want to avoid unnecessary work
     * when only the active collection changes. */
    WM_main_add_notifier(NC_SCENE | ND_LAYER | NS_LAYER_COLLECTION | NA_ACTIVATED, NULL);
  }

  return OL_DRAWSEL_NONE;
}

/* ---------------------------------------------- */

/* generic call for ID data check or make/check active in UI */
eOLDrawState tree_element_active(bContext *C,
                                 const TreeViewContext *tvc,
                                 SpaceOutliner *space_outliner,
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
        return tree_element_set_active_object(
            C, tvc->scene, tvc->view_layer, space_outliner, te, set, false);
      }
      break;
    case ID_MA:
      return tree_element_active_material(C, tvc->scene, tvc->view_layer, te, set);
    case ID_WO:
      return tree_element_active_world(C, tvc->scene, tvc->view_layer, space_outliner, te, set);
    case ID_CA:
      return tree_element_active_camera(C, tvc->scene, tvc->view_layer, te, set);
  }
  return OL_DRAWSEL_NONE;
}

/**
 * Generic call for non-id data to make/check active in UI
 */
eOLDrawState tree_element_type_active(bContext *C,
                                      const TreeViewContext *tvc,
                                      SpaceOutliner *space_outliner,
                                      TreeElement *te,
                                      TreeStoreElem *tselem,
                                      const eOLSetState set,
                                      bool recursive)
{
  switch (tselem->type) {
    case TSE_DEFGROUP:
      return tree_element_active_defgroup(C, tvc->view_layer, te, tselem, set);
    case TSE_BONE:
      return tree_element_active_bone(C, tvc->view_layer, te, tselem, set, recursive);
    case TSE_EBONE:
      return tree_element_active_ebone(C, tvc->view_layer, te, tselem, set, recursive);
    case TSE_MODIFIER:
      return tree_element_active_modifier(C, tvc->scene, tvc->view_layer, te, tselem, set);
    case TSE_LINKED_OB:
      if (set != OL_SETSEL_NONE) {
        tree_element_set_active_object(
            C, tvc->scene, tvc->view_layer, space_outliner, te, set, false);
      }
      else if (tselem->id == (ID *)tvc->obact) {
        return OL_DRAWSEL_NORMAL;
      }
      break;
    case TSE_LINKED_PSYS:
      return tree_element_active_psys(C, tvc->scene, te, tselem, set);
    case TSE_POSE_BASE:
      return tree_element_active_pose(C, tvc->scene, tvc->view_layer, te, tselem, set);
    case TSE_POSE_CHANNEL:
      return tree_element_active_posechannel(
          C, tvc->scene, tvc->view_layer, tvc->ob_pose, te, tselem, set, recursive);
    case TSE_CONSTRAINT_BASE:
    case TSE_CONSTRAINT:
      return tree_element_active_constraint(C, tvc->scene, tvc->view_layer, te, tselem, set);
    case TSE_R_LAYER:
      return active_viewlayer(C, tvc->scene, tvc->view_layer, te, set);
    case TSE_POSEGRP:
      return tree_element_active_posegroup(C, tvc->scene, tvc->view_layer, te, tselem, set);
    case TSE_SEQUENCE:
      return tree_element_active_sequence(C, tvc->scene, te, tselem, set);
    case TSE_SEQUENCE_DUP:
      return tree_element_active_sequence_dup(tvc->scene, te, tselem, set);
    case TSE_GP_LAYER:
      return tree_element_active_gplayer(C, tvc->scene, te, tselem, set);
      break;
    case TSE_VIEW_COLLECTION_BASE:
      return tree_element_active_master_collection(C, te, set);
    case TSE_LAYER_COLLECTION:
      return tree_element_active_layer_collection(C, te, set);
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

  return NULL;
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
  PointerRNA ptr = {0};
  int context = 0;

  /* ID Types */
  if (tselem->type == 0) {
    RNA_id_pointer_create(tselem->id, &ptr);

    switch (te->idcode) {
      case ID_SCE:
        context = BCONTEXT_SCENE;
        break;
      case ID_OB:
        context = BCONTEXT_OBJECT;
        break;
      case ID_ME:
      case ID_CU:
      case ID_MB:
      case ID_IM:
      case ID_LT:
      case ID_LA:
      case ID_CA:
      case ID_KE:
      case ID_SPK:
      case ID_AR:
      case ID_GD:
      case ID_LP:
      case ID_HA:
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
        RNA_id_pointer_create(tselem->id, &ptr);
        context = BCONTEXT_DATA;
        break;
      case TSE_CONSTRAINT_BASE:
      case TSE_CONSTRAINT: {
        TreeElement *bone_te = NULL;
        bPoseChannel *pchan = outliner_find_parent_bone(te, &bone_te);

        if (pchan) {
          RNA_pointer_create(TREESTORE(bone_te)->id, &RNA_PoseBone, pchan, &ptr);
          context = BCONTEXT_BONE_CONSTRAINT;
        }
        else {
          RNA_id_pointer_create(tselem->id, &ptr);
          context = BCONTEXT_CONSTRAINT;
        }

        /* Expand the selected constraint in the properties editor. */
        if (tselem->type != TSE_CONSTRAINT_BASE) {
          BKE_constraint_panel_expand(te->directdata);
        }
        break;
      }
      case TSE_MODIFIER_BASE:
      case TSE_MODIFIER:
        RNA_id_pointer_create(tselem->id, &ptr);
        context = BCONTEXT_MODIFIER;

        if (tselem->type != TSE_MODIFIER_BASE) {
          Object *ob = (Object *)tselem->id;

          if (ob->type == OB_GPENCIL) {
            BKE_gpencil_modifier_panel_expand(te->directdata);
          }
          else {
            ModifierData *md = (ModifierData *)te->directdata;
            BKE_object_modifier_set_active(ob, md);

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
        }
        break;
      case TSE_GPENCIL_EFFECT_BASE:
      case TSE_GPENCIL_EFFECT:
        RNA_id_pointer_create(tselem->id, &ptr);
        context = BCONTEXT_SHADERFX;

        if (tselem->type != TSE_GPENCIL_EFFECT_BASE) {
          BKE_shaderfx_panel_expand(te->directdata);
        }
        break;
      case TSE_BONE: {
        bArmature *arm = (bArmature *)tselem->id;
        Bone *bone = te->directdata;

        RNA_pointer_create(&arm->id, &RNA_Bone, bone, &ptr);
        context = BCONTEXT_BONE;
        break;
      }
      case TSE_EBONE: {
        bArmature *arm = (bArmature *)tselem->id;
        EditBone *ebone = te->directdata;

        RNA_pointer_create(&arm->id, &RNA_EditBone, ebone, &ptr);
        context = BCONTEXT_BONE;
        break;
      }
      case TSE_POSE_CHANNEL: {
        Object *ob = (Object *)tselem->id;
        bArmature *arm = ob->data;
        bPoseChannel *pchan = te->directdata;

        RNA_pointer_create(&arm->id, &RNA_PoseBone, pchan, &ptr);
        context = BCONTEXT_BONE;
        break;
      }
      case TSE_POSE_BASE: {
        Object *ob = (Object *)tselem->id;
        bArmature *arm = ob->data;

        RNA_pointer_create(&arm->id, &RNA_Armature, arm, &ptr);
        context = BCONTEXT_DATA;
        break;
      }
      case TSE_R_LAYER_BASE:
      case TSE_R_LAYER: {
        ViewLayer *view_layer = te->directdata;

        RNA_pointer_create(tselem->id, &RNA_ViewLayer, view_layer, &ptr);
        context = BCONTEXT_VIEW_LAYER;
        break;
      }
      case TSE_POSEGRP_BASE:
      case TSE_POSEGRP: {
        Object *ob = (Object *)tselem->id;
        bArmature *arm = ob->data;

        RNA_pointer_create(&arm->id, &RNA_Armature, arm, &ptr);
        context = BCONTEXT_DATA;
        break;
      }
      case TSE_LINKED_PSYS: {
        Object *ob = (Object *)tselem->id;
        ParticleSystem *psys = psys_get_current(ob);

        RNA_pointer_create(&ob->id, &RNA_ParticleSystem, psys, &ptr);
        context = BCONTEXT_PARTICLE;
        break;
      }
      case TSE_GP_LAYER:
        RNA_id_pointer_create(tselem->id, &ptr);
        context = BCONTEXT_DATA;
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
                                                   const TreeViewContext *tvc,
                                                   SpaceOutliner *space_outliner,
                                                   TreeElement *te,
                                                   TreeStoreElem *tselem,
                                                   const bool extend,
                                                   const bool recursive,
                                                   const bool do_activate_data)
{
  /* Always makes active object, except for some specific types. */
  if (ELEM(tselem->type,
           TSE_SEQUENCE,
           TSE_SEQ_STRIP,
           TSE_SEQUENCE_DUP,
           TSE_EBONE,
           TSE_LAYER_COLLECTION)) {
    /* Note about TSE_EBONE: In case of a same ID_AR datablock shared among several
     * objects, we do not want to switch out of edit mode (see T48328 for details). */
  }
  else if (do_activate_data) {
    tree_element_set_active_object(C,
                                   tvc->scene,
                                   tvc->view_layer,
                                   space_outliner,
                                   te,
                                   (extend && tselem->type == 0) ? OL_SETSEL_EXTEND :
                                                                   OL_SETSEL_NORMAL,
                                   recursive && tselem->type == 0);
  }

  if (tselem->type == 0) { /* The lib blocks. */
    if (do_activate_data == false) {
      /* Only select in outliner. */
    }
    else if (te->idcode == ID_SCE) {
      if (tvc->scene != (Scene *)tselem->id) {
        WM_window_set_active_scene(CTX_data_main(C), C, CTX_wm_window(C), (Scene *)tselem->id);
      }
    }
    else if ((te->idcode == ID_GR) && (space_outliner->outlinevis != SO_VIEW_LAYER)) {
      Collection *gr = (Collection *)tselem->id;

      if (extend) {
        int sel = BA_SELECT;
        FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (gr, object) {
          Base *base = BKE_view_layer_base_find(tvc->view_layer, object);
          if (base && (base->flag & BASE_SELECTED)) {
            sel = BA_DESELECT;
            break;
          }
        }
        FOREACH_COLLECTION_OBJECT_RECURSIVE_END;

        FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (gr, object) {
          Base *base = BKE_view_layer_base_find(tvc->view_layer, object);
          if (base) {
            ED_object_base_select(base, sel);
          }
        }
        FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
      }
      else {
        BKE_view_layer_base_deselect_all(tvc->view_layer);

        FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (gr, object) {
          Base *base = BKE_view_layer_base_find(tvc->view_layer, object);
          /* Object may not be in this scene */
          if (base != NULL) {
            if ((base->flag & BASE_SELECTED) == 0) {
              ED_object_base_select(base, BA_SELECT);
            }
          }
        }
        FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
      }

      DEG_id_tag_update(&tvc->scene->id, ID_RECALC_SELECT);
      WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, tvc->scene);
    }
    else { /* Rest of types. */
      tree_element_active(C, tvc, space_outliner, te, OL_SETSEL_NORMAL, false);
    }
  }
  else if (do_activate_data) {
    tree_element_type_active(C,
                             tvc,
                             space_outliner,
                             te,
                             tselem,
                             extend ? OL_SETSEL_EXTEND : OL_SETSEL_NORMAL,
                             recursive);
  }
}

/* Select the item using the set flags */
void outliner_item_select(bContext *C,
                          SpaceOutliner *space_outliner,
                          TreeElement *te,
                          const short select_flag)
{
  TreeStoreElem *tselem = TREESTORE(te);
  const bool activate = select_flag & OL_ITEM_ACTIVATE;
  const bool extend = select_flag & OL_ITEM_EXTEND;
  const bool activate_data = select_flag & OL_ITEM_SELECT_DATA;

  /* Clear previous active when activating and clear selection when not extending selection */
  const short clear_flag = (activate ? TSE_ACTIVE : 0) | (extend ? 0 : TSE_SELECTED);
  if (clear_flag) {
    outliner_flag_set(&space_outliner->tree, clear_flag, false);
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

    tselem->flag |= TSE_ACTIVE;
    do_outliner_item_activate_tree_element(C,
                                           &tvc,
                                           space_outliner,
                                           te,
                                           tselem,
                                           extend,
                                           select_flag & OL_ITEM_RECURSIVE,
                                           activate_data || space_outliner->flag & SO_SYNC_SELECT);
  }
}

static bool do_outliner_range_select_recursive(ListBase *lb,
                                               TreeElement *active,
                                               TreeElement *cursor,
                                               bool selecting)
{
  LISTBASE_FOREACH (TreeElement *, te, lb) {
    TreeStoreElem *tselem = TREESTORE(te);

    if (selecting) {
      tselem->flag |= TSE_SELECTED;
    }

    /* Set state for selection */
    if (ELEM(te, active, cursor)) {
      selecting = !selecting;
    }

    if (selecting) {
      tselem->flag |= TSE_SELECTED;
    }

    /* Don't look inside closed elements */
    if (!(tselem->flag & TSE_CLOSED)) {
      selecting = do_outliner_range_select_recursive(&te->subtree, active, cursor, selecting);
    }
  }

  return selecting;
}

/* Select a range of items between cursor and active element */
static void do_outliner_range_select(bContext *C,
                                     SpaceOutliner *space_outliner,
                                     TreeElement *cursor,
                                     const bool extend)
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
    outliner_flag_set(&space_outliner->tree, TSE_SELECTED, false);
  }

  /* Select active if under cursor */
  if (active == cursor) {
    outliner_item_select(C, space_outliner, cursor, OL_ITEM_SELECT);
    return;
  }

  /* If active is not selected or visible, select and activate the element under the cursor */
  if (!active_selected || !outliner_is_element_visible(active)) {
    outliner_item_select(C, space_outliner, cursor, OL_ITEM_SELECT | OL_ITEM_ACTIVATE);
    return;
  }

  do_outliner_range_select_recursive(&space_outliner->tree, active, cursor, false);
}

static bool outliner_is_co_within_restrict_columns(const SpaceOutliner *space_outliner,
                                                   const ARegion *region,
                                                   float view_co_x)
{
  return (view_co_x > region->v2d.cur.xmax - outliner_restrict_columns_width(space_outliner));
}

bool outliner_is_co_within_mode_column(SpaceOutliner *space_outliner, const float view_mval[2])
{
  /* Mode toggles only show in View Layer and Scenes modes. */
  if (!ELEM(space_outliner->outlinevis, SO_VIEW_LAYER, SO_SCENES)) {
    return false;
  }

  return space_outliner->flag & SO_MODE_COLUMN && view_mval[0] < UI_UNIT_X;
}

static bool outliner_is_co_within_active_mode_column(bContext *C,
                                                     SpaceOutliner *space_outliner,
                                                     const float view_mval[2])
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *obact = OBACT(view_layer);

  return outliner_is_co_within_mode_column(space_outliner, view_mval) && obact &&
         obact->mode != OB_MODE_OBJECT;
}

/**
 * Action to run when clicking in the outliner,
 *
 * May expend/collapse branches or activate items.
 */
static int outliner_item_do_activate_from_cursor(bContext *C,
                                                 const int mval[2],
                                                 const bool extend,
                                                 const bool use_range,
                                                 const bool deselect_all)
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
      outliner_flag_set(&space_outliner->tree, TSE_SELECTED, false);
      changed = true;
    }
  }
  /* Don't allow toggle on scene collection */
  else if ((TREESTORE(te)->type != TSE_VIEW_COLLECTION_BASE) &&
           outliner_item_is_co_within_close_toggle(te, view_mval[0])) {
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

    if (use_range) {
      do_outliner_range_select(C, space_outliner, activate_te, extend);
    }
    else {
      const bool is_over_name_icons = outliner_item_is_co_over_name_icons(activate_te,
                                                                          view_mval[0]);
      /* Always select unless already active and selected */
      const bool select = !extend || !(activate_tselem->flag & TSE_ACTIVE &&
                                       activate_tselem->flag & TSE_SELECTED);

      const short select_flag = OL_ITEM_ACTIVATE | (select ? OL_ITEM_SELECT : OL_ITEM_DESELECT) |
                                (is_over_name_icons ? OL_ITEM_SELECT_DATA : 0) |
                                (extend ? OL_ITEM_EXTEND : 0);

      outliner_item_select(C, space_outliner, activate_te, select_flag);

      /* Only switch properties editor tabs when icons are selected. */
      if (is_over_icon) {
        outliner_set_properties_tab(C, activate_te, activate_tselem);
      }
    }

    changed = true;
  }

  if (changed) {
    if (rebuild_tree) {
      ED_region_tag_redraw(region);
    }
    else {
      ED_region_tag_redraw_no_rebuild(region);
    }

    ED_outliner_select_sync_from_outliner(C, space_outliner);
  }

  return OPERATOR_FINISHED;
}

/* event can enterkey, then it opens/closes */
static int outliner_item_activate_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const bool extend = RNA_boolean_get(op->ptr, "extend");
  const bool use_range = RNA_boolean_get(op->ptr, "extend_range");
  const bool deselect_all = RNA_boolean_get(op->ptr, "deselect_all");
  return outliner_item_do_activate_from_cursor(C, event->mval, extend, use_range, deselect_all);
}

void OUTLINER_OT_item_activate(wmOperatorType *ot)
{
  ot->name = "Select";
  ot->idname = "OUTLINER_OT_item_activate";
  ot->description = "Handle mouse clicks to select and activate items";

  ot->invoke = outliner_item_activate_invoke;

  ot->poll = ED_operator_outliner_active;

  ot->flag |= OPTYPE_REGISTER | OPTYPE_UNDO;

  PropertyRNA *prop;
  RNA_def_boolean(ot->srna, "extend", true, "Extend", "Extend selection for activation");
  prop = RNA_def_boolean(
      ot->srna, "extend_range", false, "Extend Range", "Select a range from active element");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna,
                         "deselect_all",
                         false,
                         "Deselect On Nothing",
                         "Deselect all when nothing under the cursor");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/* ****************************************************** */

/* **************** Box Select Tool ****************** */
static void outliner_item_box_select(bContext *C,
                                     SpaceOutliner *space_outliner,
                                     Scene *scene,
                                     rctf *rectf,
                                     TreeElement *te,
                                     bool select)
{
  TreeStoreElem *tselem = TREESTORE(te);

  if (te->ys <= rectf->ymax && te->ys + UI_UNIT_Y >= rectf->ymin) {
    outliner_item_select(
        C, space_outliner, te, (select ? OL_ITEM_SELECT : OL_ITEM_DESELECT) | OL_ITEM_EXTEND);
  }

  /* Look at its children. */
  if (TSELEM_OPEN(tselem, space_outliner)) {
    LISTBASE_FOREACH (TreeElement *, te_sub, &te->subtree) {
      outliner_item_box_select(C, space_outliner, scene, rectf, te_sub, select);
    }
  }
}

static int outliner_box_select_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  ARegion *region = CTX_wm_region(C);
  rctf rectf;

  const eSelectOp sel_op = RNA_enum_get(op->ptr, "mode");
  const bool select = (sel_op != SEL_OP_SUB);
  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    outliner_flag_set(&space_outliner->tree, TSE_SELECTED, 0);
  }

  WM_operator_properties_border_to_rctf(op, &rectf);
  UI_view2d_region_to_view_rctf(&region->v2d, &rectf, &rectf);

  LISTBASE_FOREACH (TreeElement *, te, &space_outliner->tree) {
    outliner_item_box_select(C, space_outliner, scene, &rectf, te, select);
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
  ED_region_tag_redraw_no_rebuild(region);

  ED_outliner_select_sync_from_outliner(C, space_outliner);

  return OPERATOR_FINISHED;
}

static int outliner_box_select_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  ARegion *region = CTX_wm_region(C);
  float view_mval[2];
  const bool tweak = RNA_boolean_get(op->ptr, "tweak");

  UI_view2d_region_to_view(
      &region->v2d, event->mval[0], event->mval[1], &view_mval[0], &view_mval[1]);

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

  /* api callbacks */
  ot->invoke = outliner_box_select_invoke;
  ot->exec = outliner_box_select_exec;
  ot->modal = WM_gesture_box_modal;
  ot->cancel = WM_gesture_box_cancel;

  ot->poll = ED_operator_outliner_active;

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

/* ****************************************************** */

/* **************** Walk Select Tool ****************** */

/* Given a tree element return the rightmost child that is visible in the outliner */
static TreeElement *outliner_find_rightmost_visible_child(SpaceOutliner *space_outliner,
                                                          TreeElement *te)
{
  while (te->subtree.last) {
    if (TSELEM_OPEN(TREESTORE(te), space_outliner)) {
      te = te->subtree.last;
    }
    else {
      break;
    }
  }
  return te;
}

/* Find previous visible element in the tree  */
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
    te = te->subtree.first;
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
    outliner_item_openclose(space_outliner, te, false, toggle_all);
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
    te = te->subtree.first;
  }
  else {
    outliner_item_openclose(space_outliner, te, true, toggle_all);
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
static TreeElement *find_walk_select_start_element(SpaceOutliner *space_outliner, bool *changed)
{
  TreeElement *active_te = outliner_find_element_with_flag(&space_outliner->tree, TSE_ACTIVE);
  *changed = false;

  /* If no active element exists, use the first element in the tree */
  if (!active_te) {
    active_te = space_outliner->tree.first;
    *changed = true;
  }

  /* If the active element is not visible, activate the first visible parent element */
  if (!outliner_is_element_visible(active_te)) {
    while (!outliner_is_element_visible(active_te)) {
      active_te = active_te->parent;
    }
    *changed = true;
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

static int outliner_walk_select_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
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

  /* api callbacks */
  ot->invoke = outliner_walk_select_invoke;
  ot->poll = ED_operator_outliner_active;

  ot->flag |= OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  PropertyRNA *prop;
  WM_operator_properties_select_walk_direction(ot);
  prop = RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend selection on walk");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(
      ot->srna, "toggle_all", false, "Toggle All", "Toggle open/close hierarchy");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/* ****************************************************** */
