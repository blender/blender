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
#include "DNA_light_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_world_types.h"
#include "DNA_gpencil_types.h"

#include "BLI_utildefines.h"
#include "BLI_listbase.h"

#include "BKE_armature.h"
#include "BKE_collection.h"
#include "BKE_context.h"
#include "BKE_gpencil.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_scene.h"
#include "BKE_sequencer.h"
#include "BKE_workspace.h"

#include "DEG_depsgraph.h"

#include "ED_armature.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_select_utils.h"
#include "ED_sequencer.h"
#include "ED_undo.h"
#include "ED_gpencil.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_view2d.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "outliner_intern.h"

static bool do_outliner_activate_common(bContext *C,
                                        Main *bmain,
                                        Depsgraph *depsgraph,
                                        Scene *scene,
                                        ViewLayer *view_layer,
                                        Base *base,
                                        const bool extend,
                                        const bool do_exit)
{
  bool use_all = false;

  if (do_exit) {
    FOREACH_OBJECT_BEGIN (view_layer, ob_iter) {
      ED_object_mode_generic_exit(bmain, depsgraph, scene, ob_iter);
    }
    FOREACH_OBJECT_END;
  }

  /* Just like clicking in the object changes the active object,
   * clicking on the object data should change it as well. */
  ED_object_base_activate(C, base);

  if (extend) {
    use_all = true;
  }
  else {
    ED_object_base_deselect_all(view_layer, NULL, SEL_DESELECT);
  }

  return use_all;
}

/**
 * Bring the newly selected object into edit mode.
 *
 * If extend is used, we try to have the other compatible selected objects in the new mode as well.
 * Otherwise only the new object will be active, selected and in the edit mode.
 */
static void do_outliner_activate_obdata(
    bContext *C, Scene *scene, ViewLayer *view_layer, Base *base, const bool extend)
{
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_depsgraph(C);
  Object *obact = OBACT(view_layer);
  Object *ob = base->object;
  bool use_all = false;

  if (obact == NULL) {
    ED_object_base_activate(C, base);
    DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
    obact = ob;
    use_all = true;
  }
  else if (obact->data == ob->data) {
    use_all = true;
  }
  else if (obact->mode == OB_MODE_OBJECT) {
    use_all = do_outliner_activate_common(
        C, bmain, depsgraph, scene, view_layer, base, extend, false);
  }
  else if ((ob->type != obact->type) || ((obact->mode & OB_MODE_EDIT) == 0) ||
           ((obact->mode & OB_MODE_POSE) && ELEM(OB_ARMATURE, ob->type, obact->type)) || !extend) {
    use_all = do_outliner_activate_common(
        C, bmain, depsgraph, scene, view_layer, base, extend, true);
  }

  if (use_all) {
    WM_operator_name_call(C, "OBJECT_OT_editmode_toggle", WM_OP_INVOKE_REGION_WIN, NULL);
  }
  else {
    bool ok;
    if (BKE_object_is_in_editmode(ob)) {
      ok = ED_object_editmode_exit_ex(bmain, scene, ob, EM_FREEDATA);
    }
    else {
      ok = ED_object_editmode_enter_ex(CTX_data_main(C), scene, ob, EM_NO_CONTEXT);
    }
    if (ok) {
      ED_object_base_select(base, (ob->mode & OB_MODE_EDIT) ? BA_SELECT : BA_DESELECT);
      DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
      WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
    }
  }
}

static void do_outliner_activate_pose(
    bContext *C, Scene *scene, ViewLayer *view_layer, Base *base, const bool extend)
{
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_depsgraph(C);
  Object *obact = OBACT(view_layer);
  Object *ob = base->object;
  bool use_all = false;

  if (obact == NULL) {
    ED_object_base_activate(C, base);
    DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
    obact = ob;
    use_all = true;
  }
  else if (obact->data == ob->data) {
    use_all = true;
  }
  else if (obact->mode == OB_MODE_OBJECT) {
    use_all = do_outliner_activate_common(
        C, bmain, depsgraph, scene, view_layer, base, extend, false);
  }
  else if ((!ELEM(ob->type, obact->type)) ||
           ((obact->mode & OB_MODE_EDIT) && ELEM(OB_ARMATURE, ob->type, obact->type))) {
    use_all = do_outliner_activate_common(
        C, bmain, depsgraph, scene, view_layer, base, extend, true);
  }

  if (use_all) {
    WM_operator_name_call(C, "OBJECT_OT_posemode_toggle", WM_OP_INVOKE_REGION_WIN, NULL);
  }
  else {
    bool ok = false;
    if (ob->mode & OB_MODE_POSE) {
      ok = ED_object_posemode_exit_ex(bmain, ob);
    }
    else {
      ok = ED_object_posemode_enter_ex(bmain, ob);
    }
    if (ok) {
      ED_object_base_select(base, (ob->mode & OB_MODE_POSE) ? BA_SELECT : BA_DESELECT);

      DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
      WM_event_add_notifier(C, NC_SCENE | ND_MODE | NS_MODE_OBJECT, NULL);
      WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
    }
  }
}

/* For draw callback to run mode switching */
void outliner_object_mode_toggle(bContext *C, Scene *scene, ViewLayer *view_layer, Base *base)
{
  Object *obact = OBACT(view_layer);
  if (obact->mode & OB_MODE_EDIT) {
    do_outliner_activate_obdata(C, scene, view_layer, base, true);
  }
  else if (obact->mode & OB_MODE_POSE) {
    do_outliner_activate_pose(C, scene, view_layer, base, true);
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
 * Select object tree:
 * CTRL+LMB: Select/Deselect object and all children.
 * CTRL+SHIFT+LMB: Add/Remove object and all children.
 */
static void do_outliner_object_select_recursive(ViewLayer *view_layer,
                                                Object *ob_parent,
                                                bool select)
{
  Base *base;

  for (base = FIRSTBASE(view_layer); base; base = base->next) {
    Object *ob = base->object;
    if ((((base->flag & BASE_VISIBLE) != 0) && BKE_object_is_child_recursive(ob_parent, ob))) {
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
                                                   SpaceOutliner *soops,
                                                   TreeElement *te,
                                                   const eOLSetState set,
                                                   bool recursive)
{
  TreeStoreElem *tselem = TREESTORE(te);
  Scene *sce;
  Base *base;
  Object *ob = NULL;

  /* if id is not object, we search back */
  if (te->idcode == ID_OB) {
    ob = (Object *)tselem->id;
  }
  else {
    ob = (Object *)outliner_search_back(soops, te, ID_OB);
    if (ob == OBACT(view_layer)) {
      return OL_DRAWSEL_NONE;
    }
  }
  if (ob == NULL) {
    return OL_DRAWSEL_NONE;
  }

  sce = (Scene *)outliner_search_back(soops, te, ID_SCE);
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
          Depsgraph *depsgraph = CTX_data_depsgraph(C);
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
      }
      else {
        ED_object_base_select(base, BA_SELECT);
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
    }

    if (recursive) {
      /* Recursive select/deselect for Object hierarchies */
      do_outliner_object_select_recursive(view_layer, ob, (base->flag & BASE_SELECTED) != 0);
    }

    if (set != OL_SETSEL_NONE) {
      ED_object_base_activate(C, base); /* adds notifier */
      DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
      WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
    }

    if (ob != OBEDIT_FROM_VIEW_LAYER(view_layer)) {
      ED_object_editmode_exit(C, EM_FREEDATA);
    }
  }
  return OL_DRAWSEL_NORMAL;
}

static eOLDrawState tree_element_active_material(bContext *C,
                                                 Scene *UNUSED(scene),
                                                 ViewLayer *view_layer,
                                                 SpaceOutliner *soops,
                                                 TreeElement *te,
                                                 const eOLSetState set)
{
  TreeElement *tes;
  Object *ob;

  /* we search for the object parent */
  ob = (Object *)outliner_search_back(soops, te, ID_OB);
  // note: ob->matbits can be NULL when a local object points to a library mesh.
  if (ob == NULL || ob != OBACT(view_layer) || ob->matbits == NULL) {
    return OL_DRAWSEL_NONE; /* just paranoia */
  }

  /* searching in ob mat array? */
  tes = te->parent;
  if (tes->idcode == ID_OB) {
    if (set != OL_SETSEL_NONE) {
      ob->actcol = te->index + 1;
      ob->matbits[te->index] = 1;  // make ob material active too
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
      ob->matbits[te->index] = 0;  // make obdata material active too
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

static eOLDrawState tree_element_active_light(bContext *UNUSED(C),
                                              Scene *UNUSED(scene),
                                              ViewLayer *view_layer,
                                              SpaceOutliner *soops,
                                              TreeElement *te,
                                              const eOLSetState set)
{
  Object *ob;

  /* we search for the object parent */
  ob = (Object *)outliner_search_back(soops, te, ID_OB);
  if (ob == NULL || ob != OBACT(view_layer)) {
    /* just paranoia */
    return OL_DRAWSEL_NONE;
  }

  if (set != OL_SETSEL_NONE) {
    // XXX      extern_set_butspace(F5KEY, 0);
  }
  else {
    return OL_DRAWSEL_NORMAL;
  }

  return OL_DRAWSEL_NONE;
}

static eOLDrawState tree_element_active_camera(bContext *UNUSED(C),
                                               Scene *scene,
                                               ViewLayer *UNUSED(sl),
                                               SpaceOutliner *soops,
                                               TreeElement *te,
                                               const eOLSetState set)
{
  Object *ob = (Object *)outliner_search_back(soops, te, ID_OB);

  if (set != OL_SETSEL_NONE) {
    return OL_DRAWSEL_NONE;
  }

  return scene->camera == ob;
}

static eOLDrawState tree_element_active_world(bContext *C,
                                              Scene *scene,
                                              ViewLayer *UNUSED(sl),
                                              SpaceOutliner *UNUSED(soops),
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
    if (set != OL_SETSEL_NONE) {
      // XXX          extern_set_butspace(F8KEY, 0);
    }
    else {
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
      BKE_gpencil_layer_setactive(gpd, gpl);
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
        Object **objects = BKE_view_layer_array_from_objects_in_mode_unique_data(
            view_layer, NULL, &objects_len, OB_MODE_POSE);
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
    if (ob == OBACT(view_layer) && ob->pose) {
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

/* ebones only draw in editmode armature */
static void tree_element_active_ebone__sel(bContext *C, bArmature *arm, EditBone *ebone, short sel)
{
  if (sel) {
    ebone->flag |= BONE_SELECTED | BONE_ROOTSEL | BONE_TIPSEL;
    arm->act_edbone = ebone;
    // flush to parent?
    if (ebone->parent && (ebone->flag & BONE_CONNECTED)) {
      ebone->parent->flag |= BONE_TIPSEL;
    }
  }
  else {
    ebone->flag &= ~(BONE_SELECTED | BONE_ROOTSEL | BONE_TIPSEL);
    // flush to parent?
    if (ebone->parent && (ebone->flag & BONE_CONNECTED)) {
      ebone->parent->flag &= ~BONE_TIPSEL;
    }
  }
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

    // XXX      extern_set_butspace(F9KEY, 0);
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

    // XXX      extern_set_butspace(F7KEY, 0);
  }

  return OL_DRAWSEL_NONE;
}

static int tree_element_active_constraint(bContext *C,
                                          Scene *UNUSED(scene),
                                          ViewLayer *UNUSED(sl),
                                          TreeElement *UNUSED(te),
                                          TreeStoreElem *tselem,
                                          const eOLSetState set)
{
  if (set != OL_SETSEL_NONE) {
    Object *ob = (Object *)tselem->id;

    WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT, ob);
    // XXX      extern_set_butspace(F7KEY, 0);
  }

  return OL_DRAWSEL_NONE;
}

static eOLDrawState tree_element_active_text(bContext *UNUSED(C),
                                             Scene *UNUSED(scene),
                                             ViewLayer *UNUSED(sl),
                                             SpaceOutliner *UNUSED(soops),
                                             TreeElement *UNUSED(te),
                                             int UNUSED(set))
{
  // XXX removed
  return OL_DRAWSEL_NONE;
}

static eOLDrawState tree_element_active_pose(bContext *C,
                                             Scene *scene,
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
    do_outliner_activate_pose(C, scene, view_layer, base, (set == OL_SETSEL_EXTEND));
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
  Editing *ed = BKE_sequencer_editing_get(scene, false);

  if (set != OL_SETSEL_NONE) {
    /* only check on setting */
    if (BLI_findindex(ed->seqbasep, seq) != -1) {
      if (set == OL_SETSEL_EXTEND) {
        BKE_sequencer_active_set(scene, NULL);
      }
      ED_sequencer_deselect_all(scene);

      if ((set == OL_SETSEL_EXTEND) && seq->flag & SELECT) {
        seq->flag &= ~SELECT;
      }
      else {
        seq->flag |= SELECT;
        BKE_sequencer_active_set(scene, seq);
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
  Editing *ed = BKE_sequencer_editing_get(scene, false);

  seq = (Sequence *)te->directdata;
  if (set == OL_SETSEL_NONE) {
    if (seq->flag & SELECT) {
      return OL_DRAWSEL_NORMAL;
    }
    return OL_DRAWSEL_NONE;
  }

  // XXX  select_single_seq(seq, 1);
  p = ed->seqbasep->first;
  while (p) {
    if ((!p->strip) || (!p->strip->stripdata) || (p->strip->stripdata->name[0] == '\0')) {
      p = p->next;
      continue;
    }

    //      if (STREQ(p->strip->stripdata->name, seq->strip->stripdata->name))
    // XXX          select_single_seq(p, 0);
    p = p->next;
  }
  return OL_DRAWSEL_NONE;
}

static eOLDrawState tree_element_active_keymap_item(bContext *UNUSED(C),
                                                    Scene *UNUSED(scene),
                                                    ViewLayer *UNUSED(sl),
                                                    TreeElement *te,
                                                    TreeStoreElem *UNUSED(tselem),
                                                    const eOLSetState set)
{
  wmKeyMapItem *kmi = te->directdata;

  if (set == OL_SETSEL_NONE) {
    if (kmi->flag & KMI_INACTIVE) {
      return OL_DRAWSEL_NONE;
    }
    return OL_DRAWSEL_NORMAL;
  }
  else {
    kmi->flag ^= KMI_INACTIVE;
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
    WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);
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
    WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);
  }

  return OL_DRAWSEL_NONE;
}

/* ---------------------------------------------- */

/* generic call for ID data check or make/check active in UI */
eOLDrawState tree_element_active(bContext *C,
                                 Scene *scene,
                                 ViewLayer *view_layer,
                                 SpaceOutliner *soops,
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
        return tree_element_set_active_object(C, scene, view_layer, soops, te, set, false);
      }
      break;
    case ID_MA:
      return tree_element_active_material(C, scene, view_layer, soops, te, set);
    case ID_WO:
      return tree_element_active_world(C, scene, view_layer, soops, te, set);
    case ID_LA:
      return tree_element_active_light(C, scene, view_layer, soops, te, set);
    case ID_TXT:
      return tree_element_active_text(C, scene, view_layer, soops, te, set);
    case ID_CA:
      return tree_element_active_camera(C, scene, view_layer, soops, te, set);
  }
  return OL_DRAWSEL_NONE;
}

/**
 * Generic call for non-id data to make/check active in UI
 */
eOLDrawState tree_element_type_active(bContext *C,
                                      Scene *scene,
                                      ViewLayer *view_layer,
                                      SpaceOutliner *soops,
                                      TreeElement *te,
                                      TreeStoreElem *tselem,
                                      const eOLSetState set,
                                      bool recursive)
{
  switch (tselem->type) {
    case TSE_DEFGROUP:
      return tree_element_active_defgroup(C, view_layer, te, tselem, set);
    case TSE_BONE:
      return tree_element_active_bone(C, view_layer, te, tselem, set, recursive);
    case TSE_EBONE:
      return tree_element_active_ebone(C, view_layer, te, tselem, set, recursive);
    case TSE_MODIFIER:
      return tree_element_active_modifier(C, scene, view_layer, te, tselem, set);
    case TSE_LINKED_OB:
      if (set != OL_SETSEL_NONE) {
        tree_element_set_active_object(C, scene, view_layer, soops, te, set, false);
      }
      else if (tselem->id == (ID *)OBACT(view_layer)) {
        return OL_DRAWSEL_NORMAL;
      }
      break;
    case TSE_LINKED_PSYS:
      return tree_element_active_psys(C, scene, te, tselem, set);
    case TSE_POSE_BASE:
      return tree_element_active_pose(C, scene, view_layer, te, tselem, set);
    case TSE_POSE_CHANNEL:
      return tree_element_active_posechannel(C, scene, view_layer, te, tselem, set, recursive);
    case TSE_CONSTRAINT:
      return tree_element_active_constraint(C, scene, view_layer, te, tselem, set);
    case TSE_R_LAYER:
      return active_viewlayer(C, scene, view_layer, te, set);
    case TSE_POSEGRP:
      return tree_element_active_posegroup(C, scene, view_layer, te, tselem, set);
    case TSE_SEQUENCE:
      return tree_element_active_sequence(C, scene, te, tselem, set);
    case TSE_SEQUENCE_DUP:
      return tree_element_active_sequence_dup(scene, te, tselem, set);
    case TSE_KEYMAP_ITEM:
      return tree_element_active_keymap_item(C, scene, view_layer, te, tselem, set);
    case TSE_GP_LAYER:
      return tree_element_active_gplayer(C, scene, te, tselem, set);
      break;
    case TSE_VIEW_COLLECTION_BASE:
      return tree_element_active_master_collection(C, te, set);
    case TSE_LAYER_COLLECTION:
      return tree_element_active_layer_collection(C, te, set);
  }
  return OL_DRAWSEL_NONE;
}

/* ================================================ */

/**
 * Action when clicking to activate an item (typically under the mouse cursor),
 * but don't do any cursor intersection checks.
 *
 * Needed to run from operators accessed from a menu.
 */
static void do_outliner_item_activate_tree_element(bContext *C,
                                                   Scene *scene,
                                                   ViewLayer *view_layer,
                                                   SpaceOutliner *soops,
                                                   TreeElement *te,
                                                   TreeStoreElem *tselem,
                                                   const bool extend,
                                                   const bool recursive)
{
  /* Always makes active object, except for some specific types. */
  if (ELEM(tselem->type,
           TSE_SEQUENCE,
           TSE_SEQ_STRIP,
           TSE_SEQUENCE_DUP,
           TSE_EBONE,
           TSE_LAYER_COLLECTION)) {
    /* Note about TSE_EBONE: In case of a same ID_AR datablock shared among several objects,
     * we do not want to switch out of edit mode (see T48328 for details). */
  }
  else if (tselem->id && OB_DATA_SUPPORT_EDITMODE(te->idcode)) {
    /* Support edit-mode toggle, keeping the active object as is. */
  }
  else if (tselem->type == TSE_POSE_BASE) {
    /* Support pose mode toggle, keeping the active object as is. */
  }
  else {
    tree_element_set_active_object(C,
                                   scene,
                                   view_layer,
                                   soops,
                                   te,
                                   (extend && tselem->type == 0) ? OL_SETSEL_EXTEND :
                                                                   OL_SETSEL_NORMAL,
                                   recursive && tselem->type == 0);
  }

  if (tselem->type == 0) {  // the lib blocks
    /* editmode? */
    if (te->idcode == ID_SCE) {
      if (scene != (Scene *)tselem->id) {
        WM_window_set_active_scene(CTX_data_main(C), C, CTX_wm_window(C), (Scene *)tselem->id);
      }
    }
    else if (te->idcode == ID_GR) {
      Collection *gr = (Collection *)tselem->id;

      if (extend) {
        int sel = BA_SELECT;
        FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (gr, object) {
          Base *base = BKE_view_layer_base_find(view_layer, object);
          if (base && (base->flag & BASE_SELECTED)) {
            sel = BA_DESELECT;
            break;
          }
        }
        FOREACH_COLLECTION_OBJECT_RECURSIVE_END;

        FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (gr, object) {
          Base *base = BKE_view_layer_base_find(view_layer, object);
          if (base) {
            ED_object_base_select(base, sel);
          }
        }
        FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
      }
      else {
        BKE_view_layer_base_deselect_all(view_layer);

        FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (gr, object) {
          Base *base = BKE_view_layer_base_find(view_layer, object);
          /* Object may not be in this scene */
          if (base != NULL) {
            if ((base->flag & BASE_SELECTED) == 0) {
              ED_object_base_select(base, BA_SELECT);
            }
          }
        }
        FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
      }

      DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
      WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
    }
    else if (OB_DATA_SUPPORT_EDITMODE(te->idcode)) {
      Object *ob = (Object *)outliner_search_back(soops, te, ID_OB);
      if ((ob != NULL) && (ob->data == tselem->id)) {
        Base *base = BKE_view_layer_base_find(view_layer, ob);
        if ((base != NULL) && (base->flag & BASE_VISIBLE)) {
          do_outliner_activate_obdata(C, scene, view_layer, base, extend);
        }
      }
    }
    else if (ELEM(te->idcode, ID_GD)) {
      /* set grease pencil to object mode */
      WM_operator_name_call(C, "GPENCIL_OT_editmode_toggle", WM_OP_INVOKE_REGION_WIN, NULL);
    }
    else {  // rest of types
      tree_element_active(C, scene, view_layer, soops, te, OL_SETSEL_NORMAL, false);
    }
  }
  else {
    tree_element_type_active(C,
                             scene,
                             view_layer,
                             soops,
                             te,
                             tselem,
                             extend ? OL_SETSEL_EXTEND : OL_SETSEL_NORMAL,
                             recursive);
  }
}

/**
 * \param extend: Don't deselect other items, only modify \a te.
 * \param toggle: Select \a te when not selected, deselect when selected.
 */
void outliner_item_select(SpaceOutliner *soops,
                          const TreeElement *te,
                          const bool extend,
                          const bool toggle)
{
  TreeStoreElem *tselem = TREESTORE(te);
  const short new_flag = toggle ? (tselem->flag ^ TSE_SELECTED) : (tselem->flag | TSE_SELECTED);

  if (extend == false) {
    outliner_flag_set(&soops->tree, TSE_SELECTED, false);
  }
  tselem->flag = new_flag;
}

static void outliner_item_toggle_closed(TreeElement *te, const bool toggle_children)
{
  TreeStoreElem *tselem = TREESTORE(te);
  if (toggle_children) {
    tselem->flag &= ~TSE_CLOSED;

    const bool all_opened = !outliner_flag_is_any_test(&te->subtree, TSE_CLOSED, 1);
    outliner_flag_set(&te->subtree, TSE_CLOSED, all_opened);
  }
  else {
    tselem->flag ^= TSE_CLOSED;
  }
}

static bool outliner_item_is_co_within_close_toggle(TreeElement *te, float view_co_x)
{
  return ((te->flag & TE_ICONROW) == 0) && (view_co_x > te->xs) &&
         (view_co_x < te->xs + UI_UNIT_X);
}

static bool outliner_is_co_within_restrict_columns(const SpaceOutliner *soops,
                                                   const ARegion *ar,
                                                   float view_co_x)
{
  return (view_co_x > ar->v2d.cur.xmax - outliner_restrict_columns_width(soops));
}

/**
 * A version of #outliner_item_do_acticate_from_cursor that takes the tree element directly.
 * and doesn't depend on the pointer position.
 *
 * This allows us to simulate clicking on an item without dealing with the mouse cursor.
 */
void outliner_item_do_activate_from_tree_element(
    bContext *C, TreeElement *te, TreeStoreElem *tselem, bool extend, bool recursive)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  SpaceOutliner *soops = CTX_wm_space_outliner(C);

  do_outliner_item_activate_tree_element(
      C, scene, view_layer, soops, te, tselem, extend, recursive);
}

/**
 * Action to run when clicking in the outliner,
 *
 * May expend/collapse branches or activate items.
 * */
static int outliner_item_do_activate_from_cursor(bContext *C,
                                                 const int mval[2],
                                                 const bool extend,
                                                 const bool recursive,
                                                 const bool deselect_all)
{
  ARegion *ar = CTX_wm_region(C);
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  TreeElement *te;
  float view_mval[2];
  bool changed = false, rebuild_tree = false;

  UI_view2d_region_to_view(&ar->v2d, mval[0], mval[1], &view_mval[0], &view_mval[1]);

  if (outliner_is_co_within_restrict_columns(soops, ar, view_mval[0])) {
    return OPERATOR_CANCELLED;
  }

  if (!(te = outliner_find_item_at_y(soops, &soops->tree, view_mval[1]))) {
    if (deselect_all) {
      outliner_flag_set(&soops->tree, TSE_SELECTED, false);
      changed = true;
    }
  }
  else if (outliner_item_is_co_within_close_toggle(te, view_mval[0])) {
    outliner_item_toggle_closed(te, extend);
    changed = true;
    rebuild_tree = true;
  }
  else {
    Scene *scene = CTX_data_scene(C);
    ViewLayer *view_layer = CTX_data_view_layer(C);
    /* the row may also contain children, if one is hovered we want this instead of current te */
    TreeElement *activate_te = outliner_find_item_at_x_in_row(soops, te, view_mval[0]);
    TreeStoreElem *activate_tselem = TREESTORE(activate_te);

    outliner_item_select(soops, activate_te, extend, extend);
    do_outliner_item_activate_tree_element(
        C, scene, view_layer, soops, activate_te, activate_tselem, extend, recursive);
    changed = true;
  }

  if (changed) {
    if (rebuild_tree) {
      ED_region_tag_redraw(ar);
    }
    else {
      ED_region_tag_redraw_no_rebuild(ar);
    }
    ED_undo_push(C, "Outliner selection change");
  }

  return OPERATOR_FINISHED;
}

/* event can enterkey, then it opens/closes */
static int outliner_item_activate_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const bool extend = RNA_boolean_get(op->ptr, "extend");
  const bool recursive = RNA_boolean_get(op->ptr, "recursive");
  const bool deselect_all = RNA_boolean_get(op->ptr, "deselect_all");
  return outliner_item_do_activate_from_cursor(C, event->mval, extend, recursive, deselect_all);
}

void OUTLINER_OT_item_activate(wmOperatorType *ot)
{
  ot->name = "Select";
  ot->idname = "OUTLINER_OT_item_activate";
  ot->description = "Handle mouse clicks to select and activate items";

  ot->invoke = outliner_item_activate_invoke;

  ot->poll = ED_operator_outliner_active;

  PropertyRNA *prop;
  RNA_def_boolean(ot->srna, "extend", true, "Extend", "Extend selection for activation");
  RNA_def_boolean(ot->srna, "recursive", false, "Recursive", "Select Objects and their children");
  prop = RNA_def_boolean(ot->srna,
                         "deselect_all",
                         false,
                         "Deselect On Nothing",
                         "Deselect all when nothing under the cursor");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/* ****************************************************** */

/* **************** Box Select Tool ****************** */
static void outliner_item_box_select(
    SpaceOutliner *soops, Scene *scene, rctf *rectf, TreeElement *te, bool select)
{
  TreeStoreElem *tselem = TREESTORE(te);

  if (te->ys <= rectf->ymax && te->ys + UI_UNIT_Y >= rectf->ymin) {
    if (select) {
      tselem->flag |= TSE_SELECTED;
    }
    else {
      tselem->flag &= ~TSE_SELECTED;
    }
  }

  /* Look at its children. */
  if (TSELEM_OPEN(tselem, soops)) {
    for (te = te->subtree.first; te; te = te->next) {
      outliner_item_box_select(soops, scene, rectf, te, select);
    }
  }
}

static int outliner_box_select_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  ARegion *ar = CTX_wm_region(C);
  rctf rectf;

  const eSelectOp sel_op = RNA_enum_get(op->ptr, "mode");
  const bool select = (sel_op != SEL_OP_SUB);
  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    outliner_flag_set(&soops->tree, TSE_SELECTED, 0);
  }

  WM_operator_properties_border_to_rctf(op, &rectf);
  UI_view2d_region_to_view_rctf(&ar->v2d, &rectf, &rectf);

  for (TreeElement *te = soops->tree.first; te; te = te->next) {
    outliner_item_box_select(soops, scene, &rectf, te, select);
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
  ED_region_tag_redraw(ar);

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_select_box(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Box Select";
  ot->idname = "OUTLINER_OT_select_box";
  ot->description = "Use box selection to select tree elements";

  /* api callbacks */
  ot->invoke = WM_gesture_box_invoke;
  ot->exec = outliner_box_select_exec;
  ot->modal = WM_gesture_box_modal;
  ot->cancel = WM_gesture_box_cancel;

  ot->poll = ED_operator_outliner_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_gesture_box(ot);
  WM_operator_properties_select_operation_simple(ot);
}

/* ****************************************************** */
