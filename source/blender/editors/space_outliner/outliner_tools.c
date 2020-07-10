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

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_collection_types.h"
#include "DNA_constraint_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_hair_types.h"
#include "DNA_light_types.h"
#include "DNA_linestyle_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_simulation_types.h"
#include "DNA_volume_types.h"
#include "DNA_world_types.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_utildefines.h"

#include "BKE_anim_data.h"
#include "BKE_animsys.h"
#include "BKE_collection.h"
#include "BKE_constraint.h"
#include "BKE_context.h"
#include "BKE_fcurve.h"
#include "BKE_global.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_lib_override.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_sequencer.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "ED_armature.h"
#include "ED_object.h"
#include "ED_outliner.h"
#include "ED_scene.h"
#include "ED_screen.h"
#include "ED_sequencer.h"
#include "ED_undo.h"

#include "WM_api.h"
#include "WM_message.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "outliner_intern.h"

/* -------------------------------------------------------------------- */
/** \name ID/Library/Data Set/Un-link Utilities
 * \{ */

static void set_operation_types(SpaceOutliner *soops,
                                ListBase *lb,
                                int *scenelevel,
                                int *objectlevel,
                                int *idlevel,
                                int *datalevel)
{
  TreeElement *te;
  TreeStoreElem *tselem;

  for (te = lb->first; te; te = te->next) {
    tselem = TREESTORE(te);
    if (tselem->flag & TSE_SELECTED) {
      /* Layer collection points to collection ID. */
      if (!ELEM(tselem->type, 0, TSE_LAYER_COLLECTION)) {
        if (*datalevel == 0) {
          *datalevel = tselem->type;
        }
        else if (*datalevel != tselem->type) {
          *datalevel = -1;
        }
      }
      else {
        const int idcode = (int)GS(tselem->id->name);
        bool is_standard_id = false;
        switch ((ID_Type)idcode) {
          case ID_SCE:
            *scenelevel = 1;
            break;
          case ID_OB:
            *objectlevel = 1;
            break;

          case ID_ME:
          case ID_CU:
          case ID_MB:
          case ID_LT:
          case ID_LA:
          case ID_AR:
          case ID_CA:
          case ID_SPK:
          case ID_MA:
          case ID_TE:
          case ID_IP:
          case ID_IM:
          case ID_SO:
          case ID_KE:
          case ID_WO:
          case ID_AC:
          case ID_TXT:
          case ID_GR:
          case ID_LS:
          case ID_LI:
          case ID_VF:
          case ID_NT:
          case ID_BR:
          case ID_PA:
          case ID_GD:
          case ID_MC:
          case ID_MSK:
          case ID_PAL:
          case ID_PC:
          case ID_CF:
          case ID_WS:
          case ID_LP:
          case ID_HA:
          case ID_PT:
          case ID_VO:
          case ID_SIM:
            is_standard_id = true;
            break;
          case ID_WM:
          case ID_SCR:
            /* Those are ignored here. */
            /* Note: while Screens should be manageable here, deleting a screen used by a workspace
             * will cause crashes when trying to use that workspace, so for now let's play minimal,
             * safe change. */
            break;
        }
        if (idcode == ID_NLA) {
          /* Fake one, not an actual ID type... */
          is_standard_id = true;
        }

        if (is_standard_id) {
          if (*idlevel == 0) {
            *idlevel = idcode;
          }
          else if (*idlevel != idcode) {
            *idlevel = -1;
          }
          if (ELEM(*datalevel, TSE_VIEW_COLLECTION_BASE, TSE_SCENE_COLLECTION_BASE)) {
            *datalevel = 0;
          }
        }
      }
    }
    if (TSELEM_OPEN(tselem, soops)) {
      set_operation_types(soops, &te->subtree, scenelevel, objectlevel, idlevel, datalevel);
    }
  }
}

static void unlink_action_cb(bContext *C,
                             ReportList *UNUSED(reports),
                             Scene *UNUSED(scene),
                             TreeElement *UNUSED(te),
                             TreeStoreElem *tsep,
                             TreeStoreElem *UNUSED(tselem),
                             void *UNUSED(user_data))
{
  /* just set action to NULL */
  BKE_animdata_set_action(CTX_wm_reports(C), tsep->id, NULL);
}

static void unlink_material_cb(bContext *UNUSED(C),
                               ReportList *UNUSED(reports),
                               Scene *UNUSED(scene),
                               TreeElement *te,
                               TreeStoreElem *tsep,
                               TreeStoreElem *UNUSED(tselem),
                               void *UNUSED(user_data))
{
  Material **matar = NULL;
  int a, totcol = 0;

  if (GS(tsep->id->name) == ID_OB) {
    Object *ob = (Object *)tsep->id;
    totcol = ob->totcol;
    matar = ob->mat;
  }
  else if (GS(tsep->id->name) == ID_ME) {
    Mesh *me = (Mesh *)tsep->id;
    totcol = me->totcol;
    matar = me->mat;
  }
  else if (GS(tsep->id->name) == ID_CU) {
    Curve *cu = (Curve *)tsep->id;
    totcol = cu->totcol;
    matar = cu->mat;
  }
  else if (GS(tsep->id->name) == ID_MB) {
    MetaBall *mb = (MetaBall *)tsep->id;
    totcol = mb->totcol;
    matar = mb->mat;
  }
  else if (GS(tsep->id->name) == ID_HA) {
    Hair *hair = (Hair *)tsep->id;
    totcol = hair->totcol;
    matar = hair->mat;
  }
  else if (GS(tsep->id->name) == ID_PT) {
    PointCloud *pointcloud = (PointCloud *)tsep->id;
    totcol = pointcloud->totcol;
    matar = pointcloud->mat;
  }
  else if (GS(tsep->id->name) == ID_VO) {
    Volume *volume = (Volume *)tsep->id;
    totcol = volume->totcol;
    matar = volume->mat;
  }
  else {
    BLI_assert(0);
  }

  if (LIKELY(matar != NULL)) {
    for (a = 0; a < totcol; a++) {
      if (a == te->index && matar[a]) {
        id_us_min(&matar[a]->id);
        matar[a] = NULL;
      }
    }
  }
}

static void unlink_texture_cb(bContext *UNUSED(C),
                              ReportList *UNUSED(reports),
                              Scene *UNUSED(scene),
                              TreeElement *te,
                              TreeStoreElem *tsep,
                              TreeStoreElem *UNUSED(tselem),
                              void *UNUSED(user_data))
{
  MTex **mtex = NULL;
  int a;

  if (GS(tsep->id->name) == ID_LS) {
    FreestyleLineStyle *ls = (FreestyleLineStyle *)tsep->id;
    mtex = ls->mtex;
  }
  else {
    return;
  }

  for (a = 0; a < MAX_MTEX; a++) {
    if (a == te->index && mtex[a]) {
      if (mtex[a]->tex) {
        id_us_min(&mtex[a]->tex->id);
        mtex[a]->tex = NULL;
      }
    }
  }
}

static void unlink_collection_cb(bContext *C,
                                 ReportList *UNUSED(reports),
                                 Scene *UNUSED(scene),
                                 TreeElement *UNUSED(te),
                                 TreeStoreElem *tsep,
                                 TreeStoreElem *tselem,
                                 void *UNUSED(user_data))
{
  Main *bmain = CTX_data_main(C);
  Collection *collection = (Collection *)tselem->id;

  if (tsep) {
    if (GS(tsep->id->name) == ID_OB) {
      Object *ob = (Object *)tsep->id;
      ob->instance_collection = NULL;
      DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);
      DEG_relations_tag_update(bmain);
    }
    else if (GS(tsep->id->name) == ID_GR) {
      Collection *parent = (Collection *)tsep->id;
      id_fake_user_set(&collection->id);
      BKE_collection_child_remove(bmain, parent, collection);
      DEG_id_tag_update(&parent->id, ID_RECALC_COPY_ON_WRITE);
      DEG_relations_tag_update(bmain);
    }
    else if (GS(tsep->id->name) == ID_SCE) {
      Scene *scene = (Scene *)tsep->id;
      Collection *parent = scene->master_collection;
      id_fake_user_set(&collection->id);
      BKE_collection_child_remove(bmain, parent, collection);
      DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
      DEG_relations_tag_update(bmain);
    }
  }
}

static void unlink_object_cb(bContext *C,
                             ReportList *UNUSED(reports),
                             Scene *UNUSED(scene),
                             TreeElement *te,
                             TreeStoreElem *tsep,
                             TreeStoreElem *tselem,
                             void *UNUSED(user_data))
{
  if (tsep && tsep->id) {
    Main *bmain = CTX_data_main(C);
    Object *ob = (Object *)tselem->id;

    if (GS(tsep->id->name) == ID_OB) {
      /* Parented objects need to find which collection to unlink from. */
      TreeElement *te_parent = te->parent;
      while (tsep && GS(tsep->id->name) == ID_OB) {
        te_parent = te_parent->parent;
        tsep = te_parent ? TREESTORE(te_parent) : NULL;
      }
    }

    if (tsep && tsep->id) {
      if (GS(tsep->id->name) == ID_GR) {
        Collection *parent = (Collection *)tsep->id;
        BKE_collection_object_remove(bmain, parent, ob, true);
        DEG_id_tag_update(&parent->id, ID_RECALC_COPY_ON_WRITE);
        DEG_relations_tag_update(bmain);
      }
      else if (GS(tsep->id->name) == ID_SCE) {
        Scene *scene = (Scene *)tsep->id;
        Collection *parent = scene->master_collection;
        BKE_collection_object_remove(bmain, parent, ob, true);
        DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
        DEG_relations_tag_update(bmain);
      }
    }
  }
}

static void unlink_world_cb(bContext *UNUSED(C),
                            ReportList *UNUSED(reports),
                            Scene *UNUSED(scene),
                            TreeElement *UNUSED(te),
                            TreeStoreElem *tsep,
                            TreeStoreElem *tselem,
                            void *UNUSED(user_data))
{
  Scene *parscene = (Scene *)tsep->id;
  World *wo = (World *)tselem->id;

  /* need to use parent scene not just scene, otherwise may end up getting wrong one */
  id_us_min(&wo->id);
  parscene->world = NULL;
}

static void outliner_do_libdata_operation(bContext *C,
                                          ReportList *reports,
                                          Scene *scene,
                                          SpaceOutliner *soops,
                                          ListBase *lb,
                                          outliner_operation_cb operation_cb,
                                          void *user_data)
{
  TreeElement *te;
  TreeStoreElem *tselem;

  for (te = lb->first; te; te = te->next) {
    tselem = TREESTORE(te);
    if (tselem->flag & TSE_SELECTED) {
      if (ELEM(tselem->type, 0, TSE_LAYER_COLLECTION)) {
        TreeStoreElem *tsep = te->parent ? TREESTORE(te->parent) : NULL;
        operation_cb(C, reports, scene, te, tsep, tselem, user_data);
      }
    }
    if (TSELEM_OPEN(tselem, soops)) {
      outliner_do_libdata_operation(
          C, reports, scene, soops, &te->subtree, operation_cb, user_data);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Scene Menu Operator
 * \{ */

typedef enum eOutliner_PropSceneOps {
  OL_SCENE_OP_DELETE = 1,
} eOutliner_PropSceneOps;

static const EnumPropertyItem prop_scene_op_types[] = {
    {OL_SCENE_OP_DELETE, "DELETE", ICON_X, "Delete", ""},
    {0, NULL, 0, NULL, NULL},
};

static bool outliner_do_scene_operation(
    bContext *C,
    eOutliner_PropSceneOps event,
    ListBase *lb,
    bool (*operation_cb)(bContext *, eOutliner_PropSceneOps, TreeElement *, TreeStoreElem *))
{
  TreeElement *te;
  TreeStoreElem *tselem;
  bool success = false;

  for (te = lb->first; te; te = te->next) {
    tselem = TREESTORE(te);
    if (tselem->flag & TSE_SELECTED) {
      if (operation_cb(C, event, te, tselem)) {
        success = true;
      }
    }
  }

  return success;
}

static bool scene_cb(bContext *C,
                     eOutliner_PropSceneOps event,
                     TreeElement *UNUSED(te),
                     TreeStoreElem *tselem)
{
  Scene *scene = (Scene *)tselem->id;

  if (event == OL_SCENE_OP_DELETE) {
    if (ED_scene_delete(C, CTX_data_main(C), scene)) {
      WM_event_add_notifier(C, NC_SCENE | NA_REMOVED, scene);
    }
    else {
      return false;
    }
  }

  return true;
}

static int outliner_scene_operation_exec(bContext *C, wmOperator *op)
{
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  const eOutliner_PropSceneOps event = RNA_enum_get(op->ptr, "type");

  if (outliner_do_scene_operation(C, event, &soops->tree, scene_cb) == false) {
    return OPERATOR_CANCELLED;
  }

  if (event == OL_SCENE_OP_DELETE) {
    outliner_cleanup_tree(soops);
    ED_undo_push(C, "Delete Scene(s)");
  }
  else {
    BLI_assert(0);
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_scene_operation(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Outliner Scene Operation";
  ot->idname = "OUTLINER_OT_scene_operation";
  ot->description = "Context menu for scene operations";

  /* callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = outliner_scene_operation_exec;
  ot->poll = ED_operator_outliner_active;

  ot->flag = 0;

  ot->prop = RNA_def_enum(ot->srna, "type", prop_scene_op_types, 0, "Scene Operation", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Search Utilities
 * \{ */

/**
 * Stores the parent and a child element of a merged icon-row icon for
 * the merged select popup menu. The sub-tree of the parent is searched and
 * the child is needed to only show elements of the same type in the popup.
 */
typedef struct MergedSearchData {
  TreeElement *parent_element;
  TreeElement *select_element;
} MergedSearchData;

static void merged_element_search_cb_recursive(
    const ListBase *tree, short tselem_type, short type, const char *str, uiSearchItems *items)
{
  char name[64];
  int iconid;

  LISTBASE_FOREACH (TreeElement *, te, tree) {
    TreeStoreElem *tselem = TREESTORE(te);

    if (tree_element_id_type_to_index(te) == type && tselem_type == tselem->type) {
      if (BLI_strcasestr(te->name, str)) {
        BLI_strncpy(name, te->name, 64);

        iconid = tree_element_get_icon(tselem, te).icon;

        /* Don't allow duplicate named items */
        if (UI_search_items_find_index(items, name) == -1) {
          if (!UI_search_item_add(items, name, te, iconid, 0)) {
            break;
          }
        }
      }
    }

    merged_element_search_cb_recursive(&te->subtree, tselem_type, type, str, items);
  }
}

/* Get a list of elements that match the search string */
static void merged_element_search_update_fn(const bContext *UNUSED(C),
                                            void *data,
                                            const char *str,
                                            uiSearchItems *items)
{
  MergedSearchData *search_data = (MergedSearchData *)data;
  TreeElement *parent = search_data->parent_element;
  TreeElement *te = search_data->select_element;

  int type = tree_element_id_type_to_index(te);

  merged_element_search_cb_recursive(&parent->subtree, TREESTORE(te)->type, type, str, items);
}

/* Activate an element from the merged element search menu */
static void merged_element_search_exec_fn(struct bContext *C, void *UNUSED(arg1), void *element)
{
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  TreeElement *te = (TreeElement *)element;

  outliner_item_select(C, soops, te, OL_ITEM_SELECT | OL_ITEM_ACTIVATE);

  ED_outliner_select_sync_from_outliner(C, soops);
}

/**
 * Merged element search menu
 * Created on activation of a merged or aggregated icon-row icon.
 */
static uiBlock *merged_element_search_menu(bContext *C, ARegion *region, void *data)
{
  static char search[64] = "";
  uiBlock *block;
  uiBut *but;

  /* Clear search on each menu creation */
  *search = '\0';

  block = UI_block_begin(C, region, __func__, UI_EMBOSS);
  UI_block_flag_enable(block, UI_BLOCK_LOOP | UI_BLOCK_MOVEMOUSE_QUIT | UI_BLOCK_SEARCH_MENU);
  UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);

  short menu_width = 10 * UI_UNIT_X;
  but = uiDefSearchBut(
      block, search, 0, ICON_VIEWZOOM, sizeof(search), 10, 10, menu_width, UI_UNIT_Y, 0, 0, "");
  UI_but_func_search_set(
      but, NULL, merged_element_search_update_fn, data, NULL, merged_element_search_exec_fn, NULL);
  UI_but_flag_enable(but, UI_BUT_ACTIVATE_ON_INIT);

  /* Fake button to hold space for search items */
  uiDefBut(block,
           UI_BTYPE_LABEL,
           0,
           "",
           10,
           10 - UI_searchbox_size_y(),
           menu_width,
           UI_searchbox_size_y(),
           NULL,
           0,
           0,
           0,
           0,
           NULL);

  /* Center the menu on the cursor */
  UI_block_bounds_set_popup(block, 6, (const int[2]){-(menu_width / 2), 0});

  return block;
}

void merged_element_search_menu_invoke(bContext *C,
                                       TreeElement *parent_te,
                                       TreeElement *activate_te)
{
  MergedSearchData *select_data = MEM_callocN(sizeof(MergedSearchData), "merge_search_data");
  select_data->parent_element = parent_te;
  select_data->select_element = activate_te;

  UI_popup_block_invoke(C, merged_element_search_menu, select_data, MEM_freeN);
}

static void object_select_cb(bContext *C,
                             ReportList *UNUSED(reports),
                             Scene *UNUSED(scene),
                             TreeElement *UNUSED(te),
                             TreeStoreElem *UNUSED(tsep),
                             TreeStoreElem *tselem,
                             void *UNUSED(user_data))
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = (Object *)tselem->id;
  Base *base = BKE_view_layer_base_find(view_layer, ob);

  if (base) {
    ED_object_base_select(base, BA_SELECT);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Callbacks (Selection, Users & Library) Utilities
 * \{ */

static void object_select_hierarchy_cb(bContext *C,
                                       ReportList *UNUSED(reports),
                                       Scene *UNUSED(scene),
                                       TreeElement *te,
                                       TreeStoreElem *UNUSED(tsep),
                                       TreeStoreElem *UNUSED(tselem),
                                       void *UNUSED(user_data))
{
  /* Don't extend because this toggles, which is nice for Ctrl-Click but not for a menu item.
   * it's especially confusing when multiple items are selected since some toggle on/off. */
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  outliner_item_select(C, soops, te, OL_ITEM_SELECT | OL_ITEM_ACTIVATE | OL_ITEM_RECURSIVE);
}

static void object_deselect_cb(bContext *C,
                               ReportList *UNUSED(reports),
                               Scene *UNUSED(scene),
                               TreeElement *UNUSED(te),
                               TreeStoreElem *UNUSED(tsep),
                               TreeStoreElem *tselem,
                               void *UNUSED(user_data))
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = (Object *)tselem->id;
  Base *base = BKE_view_layer_base_find(view_layer, ob);

  if (base) {
    base->flag &= ~BASE_SELECTED;
  }
}

static void outliner_object_delete_fn(bContext *C, ReportList *reports, Scene *scene, Object *ob)
{
  if (ob) {
    Main *bmain = CTX_data_main(C);
    if (ob->id.tag & LIB_TAG_INDIRECT) {
      BKE_reportf(
          reports, RPT_WARNING, "Cannot delete indirectly linked object '%s'", ob->id.name + 2);
      return;
    }
    if (BKE_library_ID_is_indirectly_used(bmain, ob) && ID_REAL_USERS(ob) <= 1 &&
        ID_EXTRA_USERS(ob) == 0) {
      BKE_reportf(reports,
                  RPT_WARNING,
                  "Cannot delete object '%s' from scene '%s', indirectly used objects need at "
                  "least one user",
                  ob->id.name + 2,
                  scene->id.name + 2);
      return;
    }

    /* Check also library later. */
    if ((ob->mode & OB_MODE_EDIT) && BKE_object_is_in_editmode(ob)) {
      ED_object_editmode_exit_ex(bmain, scene, ob, EM_FREEDATA);
    }
    BKE_id_delete(bmain, ob);
  }
}

static void id_local_cb(bContext *C,
                        ReportList *UNUSED(reports),
                        Scene *UNUSED(scene),
                        TreeElement *UNUSED(te),
                        TreeStoreElem *UNUSED(tsep),
                        TreeStoreElem *tselem,
                        void *UNUSED(user_data))
{
  if (ID_IS_LINKED(tselem->id) && (tselem->id->tag & LIB_TAG_EXTERN)) {
    Main *bmain = CTX_data_main(C);
    /* if the ID type has no special local function,
     * just clear the lib */
    if (BKE_lib_id_make_local(bmain, tselem->id, false, 0) == false) {
      BKE_lib_id_clear_library_data(bmain, tselem->id);
    }
    else {
      BKE_main_id_clear_newpoins(bmain);
    }
  }
}

static void id_override_library_cb(bContext *C,
                                   ReportList *UNUSED(reports),
                                   Scene *UNUSED(scene),
                                   TreeElement *UNUSED(te),
                                   TreeStoreElem *UNUSED(tsep),
                                   TreeStoreElem *tselem,
                                   void *UNUSED(user_data))
{
  if (ID_IS_OVERRIDABLE_LIBRARY(tselem->id)) {
    Main *bmain = CTX_data_main(C);
    /* For now, remapp all local usages of linked ID to local override one here. */
    BKE_main_id_tag_all(bmain, LIB_TAG_DOIT, true);
    ID *override_id = BKE_lib_override_library_create_from_id(bmain, tselem->id, true);
    if (override_id != NULL) {
      BKE_main_id_clear_newpoins(bmain);
    }
    BKE_main_id_tag_all(bmain, LIB_TAG_DOIT, false);
  }
}

static void id_fake_user_set_cb(bContext *UNUSED(C),
                                ReportList *UNUSED(reports),
                                Scene *UNUSED(scene),
                                TreeElement *UNUSED(te),
                                TreeStoreElem *UNUSED(tsep),
                                TreeStoreElem *tselem,
                                void *UNUSED(user_data))
{
  ID *id = tselem->id;

  id_fake_user_set(id);
}

static void id_fake_user_clear_cb(bContext *UNUSED(C),
                                  ReportList *UNUSED(reports),
                                  Scene *UNUSED(scene),
                                  TreeElement *UNUSED(te),
                                  TreeStoreElem *UNUSED(tsep),
                                  TreeStoreElem *tselem,
                                  void *UNUSED(user_data))
{
  ID *id = tselem->id;

  id_fake_user_clear(id);
}

static void id_select_linked_cb(bContext *C,
                                ReportList *UNUSED(reports),
                                Scene *UNUSED(scene),
                                TreeElement *UNUSED(te),
                                TreeStoreElem *UNUSED(tsep),
                                TreeStoreElem *tselem,
                                void *UNUSED(user_data))
{
  ID *id = tselem->id;

  ED_object_select_linked_by_id(C, id);
}

static void singleuser_action_cb(bContext *C,
                                 ReportList *UNUSED(reports),
                                 Scene *UNUSED(scene),
                                 TreeElement *UNUSED(te),
                                 TreeStoreElem *tsep,
                                 TreeStoreElem *tselem,
                                 void *UNUSED(user_data))
{
  ID *id = tselem->id;

  if (id) {
    IdAdtTemplate *iat = (IdAdtTemplate *)tsep->id;
    PointerRNA ptr = {NULL};
    PropertyRNA *prop;

    RNA_pointer_create(&iat->id, &RNA_AnimData, iat->adt, &ptr);
    prop = RNA_struct_find_property(&ptr, "action");

    id_single_user(C, id, &ptr, prop);
  }
}

static void singleuser_world_cb(bContext *C,
                                ReportList *UNUSED(reports),
                                Scene *UNUSED(scene),
                                TreeElement *UNUSED(te),
                                TreeStoreElem *tsep,
                                TreeStoreElem *tselem,
                                void *UNUSED(user_data))
{
  ID *id = tselem->id;

  /* need to use parent scene not just scene, otherwise may end up getting wrong one */
  if (id) {
    Scene *parscene = (Scene *)tsep->id;
    PointerRNA ptr = {NULL};
    PropertyRNA *prop;

    RNA_id_pointer_create(&parscene->id, &ptr);
    prop = RNA_struct_find_property(&ptr, "world");

    id_single_user(C, id, &ptr, prop);
  }
}

/**
 * \param select_recurse: Set to false for operations which are already
 * recursively operating on their children.
 */
void outliner_do_object_operation_ex(bContext *C,
                                     ReportList *reports,
                                     Scene *scene_act,
                                     SpaceOutliner *soops,
                                     ListBase *lb,
                                     outliner_operation_cb operation_cb,
                                     void *user_data,
                                     bool select_recurse)
{
  TreeElement *te;
  for (te = lb->first; te; te = te->next) {
    TreeStoreElem *tselem = TREESTORE(te);
    bool select_handled = false;
    if (tselem->flag & TSE_SELECTED) {
      if (tselem->type == 0 && te->idcode == ID_OB) {
        /* When objects selected in other scenes... dunno if that should be allowed. */
        Scene *scene_owner = (Scene *)outliner_search_back(te, ID_SCE);
        if (scene_owner && scene_act != scene_owner) {
          WM_window_set_active_scene(CTX_data_main(C), C, CTX_wm_window(C), scene_owner);
        }
        /* Important to use 'scene_owner' not scene_act else deleting objects can crash.
         * only use 'scene_act' when 'scene_owner' is NULL, which can happen when the
         * outliner isn't showing scenes: Visible Layer draw mode for eg. */
        operation_cb(
            C, reports, scene_owner ? scene_owner : scene_act, te, NULL, tselem, user_data);
        select_handled = true;
      }
    }
    if (TSELEM_OPEN(tselem, soops)) {
      if ((select_handled == false) || select_recurse) {
        outliner_do_object_operation_ex(
            C, reports, scene_act, soops, &te->subtree, operation_cb, NULL, select_recurse);
      }
    }
  }
}

void outliner_do_object_operation(bContext *C,
                                  ReportList *reports,
                                  Scene *scene_act,
                                  SpaceOutliner *soops,
                                  ListBase *lb,
                                  outliner_operation_cb operation_cb)
{
  outliner_do_object_operation_ex(C, reports, scene_act, soops, lb, operation_cb, NULL, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal Tagging Utilities
 * \{ */

static void clear_animdata_cb(int UNUSED(event),
                              TreeElement *UNUSED(te),
                              TreeStoreElem *tselem,
                              void *UNUSED(arg))
{
  BKE_animdata_free(tselem->id, true);
  DEG_id_tag_update(tselem->id, ID_RECALC_ANIMATION);
}

static void unlinkact_animdata_cb(int UNUSED(event),
                                  TreeElement *UNUSED(te),
                                  TreeStoreElem *tselem,
                                  void *UNUSED(arg))
{
  /* just set action to NULL */
  BKE_animdata_set_action(NULL, tselem->id, NULL);
  DEG_id_tag_update(tselem->id, ID_RECALC_ANIMATION);
}

static void cleardrivers_animdata_cb(int UNUSED(event),
                                     TreeElement *UNUSED(te),
                                     TreeStoreElem *tselem,
                                     void *UNUSED(arg))
{
  IdAdtTemplate *iat = (IdAdtTemplate *)tselem->id;

  /* just free drivers - stored as a list of F-Curves */
  BKE_fcurves_free(&iat->adt->drivers);
  DEG_id_tag_update(tselem->id, ID_RECALC_ANIMATION);
}

static void refreshdrivers_animdata_cb(int UNUSED(event),
                                       TreeElement *UNUSED(te),
                                       TreeStoreElem *tselem,
                                       void *UNUSED(arg))
{
  IdAdtTemplate *iat = (IdAdtTemplate *)tselem->id;
  FCurve *fcu;

  /* Loop over drivers, performing refresh
   * (i.e. check graph_buttons.c and rna_fcurve.c for details). */
  for (fcu = iat->adt->drivers.first; fcu; fcu = fcu->next) {
    fcu->flag &= ~FCURVE_DISABLED;

    if (fcu->driver) {
      fcu->driver->flag &= ~DRIVER_FLAG_INVALID;
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Object Operation Utilities
 * \{ */

typedef enum eOutliner_PropDataOps {
  OL_DOP_SELECT = 1,
  OL_DOP_DESELECT,
  OL_DOP_HIDE,
  OL_DOP_UNHIDE,
  OL_DOP_SELECT_LINKED,
} eOutliner_PropDataOps;

typedef enum eOutliner_PropConstraintOps {
  OL_CONSTRAINTOP_ENABLE = 1,
  OL_CONSTRAINTOP_DISABLE,
  OL_CONSTRAINTOP_DELETE,
} eOutliner_PropConstraintOps;

typedef enum eOutliner_PropModifierOps {
  OL_MODIFIER_OP_TOGVIS = 1,
  OL_MODIFIER_OP_TOGREN,
  OL_MODIFIER_OP_DELETE,
} eOutliner_PropModifierOps;

static void pchan_cb(int event, TreeElement *te, TreeStoreElem *UNUSED(tselem), void *UNUSED(arg))
{
  bPoseChannel *pchan = (bPoseChannel *)te->directdata;

  if (event == OL_DOP_SELECT) {
    pchan->bone->flag |= BONE_SELECTED;
  }
  else if (event == OL_DOP_DESELECT) {
    pchan->bone->flag &= ~BONE_SELECTED;
  }
  else if (event == OL_DOP_HIDE) {
    pchan->bone->flag |= BONE_HIDDEN_P;
    pchan->bone->flag &= ~BONE_SELECTED;
  }
  else if (event == OL_DOP_UNHIDE) {
    pchan->bone->flag &= ~BONE_HIDDEN_P;
  }
}

static void bone_cb(int event, TreeElement *te, TreeStoreElem *UNUSED(tselem), void *UNUSED(arg))
{
  Bone *bone = (Bone *)te->directdata;

  if (event == OL_DOP_SELECT) {
    bone->flag |= BONE_SELECTED;
  }
  else if (event == OL_DOP_DESELECT) {
    bone->flag &= ~BONE_SELECTED;
  }
  else if (event == OL_DOP_HIDE) {
    bone->flag |= BONE_HIDDEN_P;
    bone->flag &= ~BONE_SELECTED;
  }
  else if (event == OL_DOP_UNHIDE) {
    bone->flag &= ~BONE_HIDDEN_P;
  }
}

static void ebone_cb(int event, TreeElement *te, TreeStoreElem *UNUSED(tselem), void *UNUSED(arg))
{
  EditBone *ebone = (EditBone *)te->directdata;

  if (event == OL_DOP_SELECT) {
    ebone->flag |= BONE_SELECTED;
  }
  else if (event == OL_DOP_DESELECT) {
    ebone->flag &= ~BONE_SELECTED;
  }
  else if (event == OL_DOP_HIDE) {
    ebone->flag |= BONE_HIDDEN_A;
    ebone->flag &= ~BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL;
  }
  else if (event == OL_DOP_UNHIDE) {
    ebone->flag &= ~BONE_HIDDEN_A;
  }
}

static void sequence_cb(int event, TreeElement *te, TreeStoreElem *tselem, void *scene_ptr)
{
  Sequence *seq = (Sequence *)te->directdata;
  if (event == OL_DOP_SELECT) {
    Scene *scene = (Scene *)scene_ptr;
    Editing *ed = BKE_sequencer_editing_get(scene, false);
    if (BLI_findindex(ed->seqbasep, seq) != -1) {
      ED_sequencer_select_sequence_single(scene, seq, true);
    }
  }

  (void)tselem;
}

static void gpencil_layer_cb(int event,
                             TreeElement *te,
                             TreeStoreElem *UNUSED(tselem),
                             void *UNUSED(arg))
{
  bGPDlayer *gpl = (bGPDlayer *)te->directdata;

  if (event == OL_DOP_SELECT) {
    gpl->flag |= GP_LAYER_SELECT;
  }
  else if (event == OL_DOP_DESELECT) {
    gpl->flag &= ~GP_LAYER_SELECT;
  }
  else if (event == OL_DOP_HIDE) {
    gpl->flag |= GP_LAYER_HIDE;
  }
  else if (event == OL_DOP_UNHIDE) {
    gpl->flag &= ~GP_LAYER_HIDE;
  }
}

static void data_select_linked_cb(int event,
                                  TreeElement *te,
                                  TreeStoreElem *UNUSED(tselem),
                                  void *C_v)
{
  if (event == OL_DOP_SELECT_LINKED) {
    if (RNA_struct_is_ID(te->rnaptr.type)) {
      bContext *C = (bContext *)C_v;
      ID *id = te->rnaptr.data;

      ED_object_select_linked_by_id(C, id);
    }
  }
}

static void constraint_cb(int event, TreeElement *te, TreeStoreElem *UNUSED(tselem), void *C_v)
{
  bContext *C = C_v;
  Main *bmain = CTX_data_main(C);
  bConstraint *constraint = (bConstraint *)te->directdata;
  Object *ob = (Object *)outliner_search_back(te, ID_OB);

  if (event == OL_CONSTRAINTOP_ENABLE) {
    constraint->flag &= ~CONSTRAINT_OFF;
    ED_object_constraint_update(bmain, ob);
    WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT, ob);
  }
  else if (event == OL_CONSTRAINTOP_DISABLE) {
    constraint->flag = CONSTRAINT_OFF;
    ED_object_constraint_update(bmain, ob);
    WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT, ob);
  }
  else if (event == OL_CONSTRAINTOP_DELETE) {
    ListBase *lb = NULL;

    if (TREESTORE(te->parent->parent)->type == TSE_POSE_CHANNEL) {
      lb = &((bPoseChannel *)te->parent->parent->directdata)->constraints;
    }
    else {
      lb = &ob->constraints;
    }

    if (BKE_constraint_remove_ex(lb, ob, constraint, true)) {
      /* there's no active constraint now, so make sure this is the case */
      BKE_constraints_active_set(&ob->constraints, NULL);

      /* needed to set the flags on posebones correctly */
      ED_object_constraint_update(bmain, ob);

      WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT | NA_REMOVED, ob);
      te->store_elem->flag &= ~TSE_SELECTED;
    }
  }
}

static void modifier_cb(int event, TreeElement *te, TreeStoreElem *UNUSED(tselem), void *Carg)
{
  bContext *C = (bContext *)Carg;
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ModifierData *md = (ModifierData *)te->directdata;
  Object *ob = (Object *)outliner_search_back(te, ID_OB);

  if (event == OL_MODIFIER_OP_TOGVIS) {
    md->mode ^= eModifierMode_Realtime;
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);
  }
  else if (event == OL_MODIFIER_OP_TOGREN) {
    md->mode ^= eModifierMode_Render;
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);
  }
  else if (event == OL_MODIFIER_OP_DELETE) {
    ED_object_modifier_remove(NULL, bmain, scene, ob, md);
    WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER | NA_REMOVED, ob);
    te->store_elem->flag &= ~TSE_SELECTED;
  }
}

static void outliner_do_data_operation(
    SpaceOutliner *soops,
    int type,
    int event,
    ListBase *lb,
    void (*operation_cb)(int, TreeElement *, TreeStoreElem *, void *),
    void *arg)
{
  TreeElement *te;
  TreeStoreElem *tselem;

  for (te = lb->first; te; te = te->next) {
    tselem = TREESTORE(te);
    if (tselem->flag & TSE_SELECTED) {
      if (tselem->type == type) {
        operation_cb(event, te, tselem, arg);
      }
    }
    if (TSELEM_OPEN(tselem, soops)) {
      outliner_do_data_operation(soops, type, event, &te->subtree, operation_cb, arg);
    }
  }
}

static Base *outline_batch_delete_hierarchy(
    ReportList *reports, Main *bmain, ViewLayer *view_layer, Scene *scene, Base *base)
{
  Base *child_base, *base_next;
  Object *object, *parent;

  if (!base) {
    return NULL;
  }

  object = base->object;
  for (child_base = view_layer->object_bases.first; child_base; child_base = base_next) {
    base_next = child_base->next;
    for (parent = child_base->object->parent; parent && (parent != object);
         parent = parent->parent) {
      /* pass */
    }
    if (parent) {
      base_next = outline_batch_delete_hierarchy(reports, bmain, view_layer, scene, child_base);
    }
  }

  base_next = base->next;

  if (object->id.tag & LIB_TAG_INDIRECT) {
    BKE_reportf(reports,
                RPT_WARNING,
                "Cannot delete indirectly linked object '%s'",
                base->object->id.name + 2);
    return base_next;
  }
  if (BKE_library_ID_is_indirectly_used(bmain, object) && ID_REAL_USERS(object) <= 1 &&
      ID_EXTRA_USERS(object) == 0) {
    BKE_reportf(reports,
                RPT_WARNING,
                "Cannot delete object '%s' from scene '%s', indirectly used objects need at least "
                "one user",
                object->id.name + 2,
                scene->id.name + 2);
    return base_next;
  }

  DEG_id_tag_update_ex(bmain, &object->id, ID_RECALC_BASE_FLAGS);
  BKE_scene_collections_object_remove(bmain, scene, object, false);

  if (object->id.us == 0) {
    object->id.tag |= LIB_TAG_DOIT;
  }

  return base_next;
}

static void object_batch_delete_hierarchy_fn(bContext *C,
                                             ReportList *reports,
                                             Scene *scene,
                                             Object *ob)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *obedit = CTX_data_edit_object(C);

  Base *base = BKE_view_layer_base_find(view_layer, ob);

  if (base) {
    /* Check also library later. */
    for (; obedit && (obedit != base->object); obedit = obedit->parent) {
      /* pass */
    }
    if (obedit == base->object) {
      ED_object_editmode_exit(C, EM_FREEDATA);
    }

    outline_batch_delete_hierarchy(reports, CTX_data_main(C), view_layer, scene, base);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Object Menu Operator
 * \{ */

enum {
  OL_OP_SELECT = 1,
  OL_OP_DESELECT,
  OL_OP_SELECT_HIERARCHY,
  OL_OP_REMAP,
  OL_OP_LOCALIZED, /* disabled, see below */
  OL_OP_TOGVIS,
  OL_OP_TOGSEL,
  OL_OP_TOGREN,
  OL_OP_RENAME,
  OL_OP_OBJECT_MODE_ENTER,
  OL_OP_OBJECT_MODE_EXIT,
};

static const EnumPropertyItem prop_object_op_types[] = {
    {OL_OP_SELECT, "SELECT", ICON_RESTRICT_SELECT_OFF, "Select", ""},
    {OL_OP_DESELECT, "DESELECT", 0, "Deselect", ""},
    {OL_OP_SELECT_HIERARCHY, "SELECT_HIERARCHY", 0, "Select Hierarchy", ""},
    {OL_OP_REMAP,
     "REMAP",
     0,
     "Remap Users",
     "Make all users of selected data-blocks to use instead a new chosen one"},
    {OL_OP_RENAME, "RENAME", 0, "Rename", ""},
    {OL_OP_OBJECT_MODE_ENTER, "OBJECT_MODE_ENTER", 0, "Enter Mode", ""},
    {OL_OP_OBJECT_MODE_EXIT, "OBJECT_MODE_EXIT", 0, "Exit Mode", ""},
    {0, NULL, 0, NULL, NULL},
};

static int outliner_object_operation_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  wmWindow *win = CTX_wm_window(C);
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  int event;
  const char *str = NULL;
  bool selection_changed = false;

  /* check for invalid states */
  if (soops == NULL) {
    return OPERATOR_CANCELLED;
  }

  event = RNA_enum_get(op->ptr, "type");

  if (event == OL_OP_SELECT) {
    Scene *sce = scene; /* To be able to delete, scenes are set... */
    outliner_do_object_operation(C, op->reports, scene, soops, &soops->tree, object_select_cb);
    if (scene != sce) {
      WM_window_set_active_scene(bmain, C, win, sce);
    }

    str = "Select Objects";
    selection_changed = true;
  }
  else if (event == OL_OP_SELECT_HIERARCHY) {
    Scene *sce = scene; /* To be able to delete, scenes are set... */
    outliner_do_object_operation_ex(
        C, op->reports, scene, soops, &soops->tree, object_select_hierarchy_cb, NULL, false);
    if (scene != sce) {
      WM_window_set_active_scene(bmain, C, win, sce);
    }
    str = "Select Object Hierarchy";
    selection_changed = true;
  }
  else if (event == OL_OP_DESELECT) {
    outliner_do_object_operation(C, op->reports, scene, soops, &soops->tree, object_deselect_cb);
    str = "Deselect Objects";
    selection_changed = true;
  }
  else if (event == OL_OP_REMAP) {
    outliner_do_libdata_operation(C, op->reports, scene, soops, &soops->tree, id_remap_cb, NULL);
    /* No undo push here, operator does it itself (since it's a modal one, the op_undo_depth trick
     * does not work here). */
  }
  else if (event == OL_OP_LOCALIZED) { /* disabled, see above enum (ton) */
    outliner_do_object_operation(C, op->reports, scene, soops, &soops->tree, id_local_cb);
    str = "Localized Objects";
  }
  else if (event == OL_OP_RENAME) {
    outliner_do_object_operation(C, op->reports, scene, soops, &soops->tree, item_rename_cb);
    str = "Rename Object";
  }
  else if (event == OL_OP_OBJECT_MODE_ENTER) {
    outliner_do_object_operation(
        C, op->reports, scene, soops, &soops->tree, item_object_mode_enter_cb);
    str = "Enter Current Mode";
  }
  else if (event == OL_OP_OBJECT_MODE_EXIT) {
    outliner_do_object_operation(
        C, op->reports, scene, soops, &soops->tree, item_object_mode_exit_cb);
    str = "Exit Current Mode";
  }
  else {
    BLI_assert(0);
    return OPERATOR_CANCELLED;
  }

  if (selection_changed) {
    DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
    ED_outliner_select_sync_from_object_tag(C);
  }

  if (str != NULL) {
    ED_undo_push(C, str);
  }

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_object_operation(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Outliner Object Operation";
  ot->idname = "OUTLINER_OT_object_operation";

  /* callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = outliner_object_operation_exec;
  ot->poll = ED_operator_outliner_active;

  ot->flag = 0;

  ot->prop = RNA_def_enum(ot->srna, "type", prop_object_op_types, 0, "Object Operation", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Delete Object/Collection Operator
 * \{ */

typedef void (*OutlinerDeleteFunc)(bContext *C, ReportList *reports, Scene *scene, Object *ob);

static void outliner_do_object_delete(bContext *C,
                                      ReportList *reports,
                                      Scene *scene,
                                      GSet *objects_to_delete,
                                      OutlinerDeleteFunc delete_fn)
{
  GSetIterator objects_to_delete_iter;
  GSET_ITER (objects_to_delete_iter, objects_to_delete) {
    Object *ob = (Object *)BLI_gsetIterator_getKey(&objects_to_delete_iter);

    delete_fn(C, reports, scene, ob);
  }
}

static TreeTraversalAction outliner_find_objects_to_delete(TreeElement *te, void *customdata)
{
  GSet *objects_to_delete = (GSet *)customdata;
  TreeStoreElem *tselem = TREESTORE(te);

  if (outliner_is_collection_tree_element(te)) {
    return TRAVERSE_CONTINUE;
  }

  if (tselem->type || (tselem->id == NULL) || (GS(tselem->id->name) != ID_OB)) {
    return TRAVERSE_SKIP_CHILDS;
  }

  BLI_gset_add(objects_to_delete, tselem->id);

  return TRAVERSE_CONTINUE;
}

static int outliner_delete_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  struct wmMsgBus *mbus = CTX_wm_message_bus(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const Base *basact_prev = BASACT(view_layer);

  const bool delete_hierarchy = RNA_boolean_get(op->ptr, "hierarchy");

  /* Get selected objects skipping duplicates to prevent deleting objects linked to multiple
   * collections twice */
  GSet *objects_to_delete = BLI_gset_ptr_new(__func__);
  outliner_tree_traverse(
      soops, &soops->tree, 0, TSE_SELECTED, outliner_find_objects_to_delete, objects_to_delete);

  if (delete_hierarchy) {
    BKE_main_id_tag_all(bmain, LIB_TAG_DOIT, false);

    outliner_do_object_delete(
        C, op->reports, scene, objects_to_delete, object_batch_delete_hierarchy_fn);

    BKE_id_multi_tagged_delete(bmain);
  }
  else {
    outliner_do_object_delete(C, op->reports, scene, objects_to_delete, outliner_object_delete_fn);
  }

  BLI_gset_free(objects_to_delete, NULL);

  outliner_collection_delete(C, bmain, scene, op->reports, delete_hierarchy);

  /* Tree management normally happens from draw_outliner(), but when
   * you're clicking too fast on Delete object from context menu in
   * outliner several mouse events can be handled in one cycle without
   * handling notifiers/redraw which leads to deleting the same object twice.
   * cleanup tree here to prevent such cases. */
  outliner_cleanup_tree(soops);

  DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
  DEG_relations_tag_update(bmain);

  if (basact_prev != BASACT(view_layer)) {
    WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);
    WM_msg_publish_rna_prop(mbus, &scene->id, view_layer, LayerObjects, active);
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
  ED_outliner_select_sync_from_object_tag(C);

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_delete(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete";
  ot->idname = "OUTLINER_OT_delete";
  ot->description = "Delete selected objects and collections";

  /* callbacks */
  ot->exec = outliner_delete_exec;
  ot->poll = ED_operator_outliner_active;

  /* flags */
  ot->flag |= OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  PropertyRNA *prop = RNA_def_boolean(
      ot->srna, "hierarchy", false, "Hierarchy", "Delete child objects and collections");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name ID-Data Menu Operator
 * \{ */

typedef enum eOutlinerIdOpTypes {
  OUTLINER_IDOP_INVALID = 0,

  OUTLINER_IDOP_UNLINK,
  OUTLINER_IDOP_LOCAL,
  OUTLINER_IDOP_OVERRIDE_LIBRARY,
  OUTLINER_IDOP_SINGLE,
  OUTLINER_IDOP_DELETE,
  OUTLINER_IDOP_REMAP,

  OUTLINER_IDOP_COPY,
  OUTLINER_IDOP_PASTE,

  OUTLINER_IDOP_FAKE_ADD,
  OUTLINER_IDOP_FAKE_CLEAR,
  OUTLINER_IDOP_RENAME,

  OUTLINER_IDOP_SELECT_LINKED,
} eOutlinerIdOpTypes;

/* TODO: implement support for changing the ID-block used. */
static const EnumPropertyItem prop_id_op_types[] = {
    {OUTLINER_IDOP_UNLINK, "UNLINK", 0, "Unlink", ""},
    {OUTLINER_IDOP_LOCAL, "LOCAL", 0, "Make Local", ""},
    {OUTLINER_IDOP_OVERRIDE_LIBRARY,
     "OVERRIDE_LIBRARY",
     0,
     "Add Library Override",
     "Add a local override of this linked data-block"},
    {OUTLINER_IDOP_SINGLE, "SINGLE", 0, "Make Single User", ""},
    {OUTLINER_IDOP_DELETE, "DELETE", ICON_X, "Delete", ""},
    {OUTLINER_IDOP_REMAP,
     "REMAP",
     0,
     "Remap Users",
     "Make all users of selected data-blocks to use instead current (clicked) one"},
    {0, "", 0, NULL, NULL},
    {OUTLINER_IDOP_COPY, "COPY", ICON_COPYDOWN, "Copy", ""},
    {OUTLINER_IDOP_PASTE, "PASTE", ICON_PASTEDOWN, "Paste", ""},
    {0, "", 0, NULL, NULL},
    {OUTLINER_IDOP_FAKE_ADD,
     "ADD_FAKE",
     0,
     "Add Fake User",
     "Ensure data-block gets saved even if it isn't in use (e.g. for motion and material "
     "libraries)"},
    {OUTLINER_IDOP_FAKE_CLEAR, "CLEAR_FAKE", 0, "Clear Fake User", ""},
    {OUTLINER_IDOP_RENAME, "RENAME", 0, "Rename", ""},
    {OUTLINER_IDOP_SELECT_LINKED, "SELECT_LINKED", 0, "Select Linked", ""},
    {0, NULL, 0, NULL, NULL},
};

static bool outliner_id_operation_item_poll(bContext *C,
                                            PointerRNA *UNUSED(ptr),
                                            PropertyRNA *UNUSED(prop),
                                            const int enum_value)
{
  SpaceOutliner *soops = CTX_wm_space_outliner(C);

  switch (enum_value) {
    case OUTLINER_IDOP_OVERRIDE_LIBRARY:
      return BKE_lib_override_library_is_enabled();
    case OUTLINER_IDOP_SINGLE:
      if (!soops || ELEM(soops->outlinevis, SO_SCENES, SO_VIEW_LAYER)) {
        return true;
      }
      /* TODO (dalai): enable in the few cases where this can be supported
      (i.e., when we have a valid parent for the tselem). */
      return false;
  }

  return true;
}

static const EnumPropertyItem *outliner_id_operation_itemf(bContext *C,
                                                           PointerRNA *ptr,
                                                           PropertyRNA *prop,
                                                           bool *r_free)
{
  EnumPropertyItem *items = NULL;
  int totitem = 0;

  if (C == NULL) {
    return prop_id_op_types;
  }
  for (const EnumPropertyItem *it = prop_id_op_types; it->identifier != NULL; it++) {
    if (!outliner_id_operation_item_poll(C, ptr, prop, it->value)) {
      continue;
    }
    RNA_enum_item_add(&items, &totitem, it);
  }
  RNA_enum_item_end(&items, &totitem);
  *r_free = true;

  return items;
}

static int outliner_id_operation_exec(bContext *C, wmOperator *op)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  Scene *scene = CTX_data_scene(C);
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  int scenelevel = 0, objectlevel = 0, idlevel = 0, datalevel = 0;
  eOutlinerIdOpTypes event;

  /* check for invalid states */
  if (soops == NULL) {
    return OPERATOR_CANCELLED;
  }

  set_operation_types(soops, &soops->tree, &scenelevel, &objectlevel, &idlevel, &datalevel);

  event = RNA_enum_get(op->ptr, "type");

  switch (event) {
    case OUTLINER_IDOP_UNLINK: {
      /* unlink datablock from its parent */
      if (objectlevel) {
        outliner_do_libdata_operation(
            C, op->reports, scene, soops, &soops->tree, unlink_object_cb, NULL);

        WM_event_add_notifier(C, NC_SCENE | ND_LAYER, NULL);
        ED_undo_push(C, "Unlink Object");
        break;
      }

      switch (idlevel) {
        case ID_AC:
          outliner_do_libdata_operation(
              C, op->reports, scene, soops, &soops->tree, unlink_action_cb, NULL);

          WM_event_add_notifier(C, NC_ANIMATION | ND_NLA_ACTCHANGE, NULL);
          ED_undo_push(C, "Unlink action");
          break;
        case ID_MA:
          outliner_do_libdata_operation(
              C, op->reports, scene, soops, &soops->tree, unlink_material_cb, NULL);

          WM_event_add_notifier(C, NC_OBJECT | ND_OB_SHADING, NULL);
          ED_undo_push(C, "Unlink material");
          break;
        case ID_TE:
          outliner_do_libdata_operation(
              C, op->reports, scene, soops, &soops->tree, unlink_texture_cb, NULL);

          WM_event_add_notifier(C, NC_OBJECT | ND_OB_SHADING, NULL);
          ED_undo_push(C, "Unlink texture");
          break;
        case ID_WO:
          outliner_do_libdata_operation(
              C, op->reports, scene, soops, &soops->tree, unlink_world_cb, NULL);

          WM_event_add_notifier(C, NC_SCENE | ND_WORLD, NULL);
          ED_undo_push(C, "Unlink world");
          break;
        case ID_GR:
          outliner_do_libdata_operation(
              C, op->reports, scene, soops, &soops->tree, unlink_collection_cb, NULL);

          WM_event_add_notifier(C, NC_SCENE | ND_LAYER, NULL);
          ED_undo_push(C, "Unlink Collection");
          break;
        default:
          BKE_report(op->reports, RPT_WARNING, "Not yet implemented");
          break;
      }
      break;
    }
    case OUTLINER_IDOP_LOCAL: {
      /* make local */
      outliner_do_libdata_operation(C, op->reports, scene, soops, &soops->tree, id_local_cb, NULL);
      ED_undo_push(C, "Localized Data");
      break;
    }
    case OUTLINER_IDOP_OVERRIDE_LIBRARY: {
      if (BKE_lib_override_library_is_enabled()) {
        /* make local */
        outliner_do_libdata_operation(
            C, op->reports, scene, soops, &soops->tree, id_override_library_cb, NULL);
        ED_undo_push(C, "Overridden Data");
      }
      break;
    }
    case OUTLINER_IDOP_SINGLE: {
      /* make single user */
      switch (idlevel) {
        case ID_AC:
          outliner_do_libdata_operation(
              C, op->reports, scene, soops, &soops->tree, singleuser_action_cb, NULL);

          WM_event_add_notifier(C, NC_ANIMATION | ND_NLA_ACTCHANGE, NULL);
          ED_undo_push(C, "Single-User Action");
          break;

        case ID_WO:
          outliner_do_libdata_operation(
              C, op->reports, scene, soops, &soops->tree, singleuser_world_cb, NULL);

          WM_event_add_notifier(C, NC_SCENE | ND_WORLD, NULL);
          ED_undo_push(C, "Single-User World");
          break;

        default:
          BKE_report(op->reports, RPT_WARNING, "Not yet implemented");
          break;
      }
      break;
    }
    case OUTLINER_IDOP_DELETE: {
      if (idlevel > 0) {
        outliner_do_libdata_operation(
            C, op->reports, scene, soops, &soops->tree, id_delete_cb, NULL);
        ED_undo_push(C, "Delete");
      }
      break;
    }
    case OUTLINER_IDOP_REMAP: {
      if (idlevel > 0) {
        outliner_do_libdata_operation(
            C, op->reports, scene, soops, &soops->tree, id_remap_cb, NULL);
        /* No undo push here, operator does it itself (since it's a modal one, the op_undo_depth
         * trick does not work here). */
      }
      break;
    }
    case OUTLINER_IDOP_COPY: {
      wm->op_undo_depth++;
      WM_operator_name_call(C, "OUTLINER_OT_id_copy", WM_OP_INVOKE_DEFAULT, NULL);
      wm->op_undo_depth--;
      /* No need for undo, this operation does not change anything... */
      break;
    }
    case OUTLINER_IDOP_PASTE: {
      wm->op_undo_depth++;
      WM_operator_name_call(C, "OUTLINER_OT_id_paste", WM_OP_INVOKE_DEFAULT, NULL);
      wm->op_undo_depth--;
      ED_outliner_select_sync_from_all_tag(C);
      ED_undo_push(C, "Paste");
      break;
    }
    case OUTLINER_IDOP_FAKE_ADD: {
      /* set fake user */
      outliner_do_libdata_operation(
          C, op->reports, scene, soops, &soops->tree, id_fake_user_set_cb, NULL);

      WM_event_add_notifier(C, NC_ID | NA_EDITED, NULL);
      ED_undo_push(C, "Add Fake User");
      break;
    }
    case OUTLINER_IDOP_FAKE_CLEAR: {
      /* clear fake user */
      outliner_do_libdata_operation(
          C, op->reports, scene, soops, &soops->tree, id_fake_user_clear_cb, NULL);

      WM_event_add_notifier(C, NC_ID | NA_EDITED, NULL);
      ED_undo_push(C, "Clear Fake User");
      break;
    }
    case OUTLINER_IDOP_RENAME: {
      /* rename */
      outliner_do_libdata_operation(
          C, op->reports, scene, soops, &soops->tree, item_rename_cb, NULL);

      WM_event_add_notifier(C, NC_ID | NA_EDITED, NULL);
      ED_undo_push(C, "Rename");
      break;
    }
    case OUTLINER_IDOP_SELECT_LINKED:
      outliner_do_libdata_operation(
          C, op->reports, scene, soops, &soops->tree, id_select_linked_cb, NULL);
      ED_outliner_select_sync_from_all_tag(C);
      ED_undo_push(C, "Select");
      break;

    default:
      /* Invalid - unhandled. */
      break;
  }

  /* wrong notifier still... */
  WM_event_add_notifier(C, NC_ID | NA_EDITED, NULL);

  /* XXX: this is just so that outliner is always up to date. */
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_OUTLINER, NULL);

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_id_operation(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Outliner ID data Operation";
  ot->idname = "OUTLINER_OT_id_operation";

  /* callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = outliner_id_operation_exec;
  ot->poll = ED_operator_outliner_active;

  ot->flag = 0;

  ot->prop = RNA_def_enum(ot->srna, "type", prop_id_op_types, 0, "ID data Operation", "");
  RNA_def_enum_funcs(ot->prop, outliner_id_operation_itemf);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Library Menu Operator
 * \{ */

typedef enum eOutlinerLibOpTypes {
  OL_LIB_INVALID = 0,

  OL_LIB_RENAME,
  OL_LIB_DELETE,
  OL_LIB_RELOCATE,
  OL_LIB_RELOAD,
} eOutlinerLibOpTypes;

static const EnumPropertyItem outliner_lib_op_type_items[] = {
    {OL_LIB_RENAME, "RENAME", 0, "Rename", ""},
    {OL_LIB_DELETE,
     "DELETE",
     ICON_X,
     "Delete",
     "Delete this library and all its item from Blender - WARNING: no undo"},
    {OL_LIB_RELOCATE,
     "RELOCATE",
     0,
     "Relocate",
     "Select a new path for this library, and reload all its data"},
    {OL_LIB_RELOAD, "RELOAD", ICON_FILE_REFRESH, "Reload", "Reload all data from this library"},
    {0, NULL, 0, NULL, NULL},
};

static int outliner_lib_operation_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  int scenelevel = 0, objectlevel = 0, idlevel = 0, datalevel = 0;
  eOutlinerLibOpTypes event;

  /* check for invalid states */
  if (soops == NULL) {
    return OPERATOR_CANCELLED;
  }

  set_operation_types(soops, &soops->tree, &scenelevel, &objectlevel, &idlevel, &datalevel);

  event = RNA_enum_get(op->ptr, "type");

  switch (event) {
    case OL_LIB_RENAME: {
      outliner_do_libdata_operation(
          C, op->reports, scene, soops, &soops->tree, item_rename_cb, NULL);

      WM_event_add_notifier(C, NC_ID | NA_EDITED, NULL);
      ED_undo_push(C, "Rename Library");
      break;
    }
    case OL_LIB_DELETE: {
      outliner_do_libdata_operation(
          C, op->reports, scene, soops, &soops->tree, id_delete_cb, NULL);
      ED_undo_push(C, "Delete Library");
      break;
    }
    case OL_LIB_RELOCATE: {
      outliner_do_libdata_operation(
          C, op->reports, scene, soops, &soops->tree, lib_relocate_cb, NULL);
      /* No undo push here, operator does it itself (since it's a modal one, the op_undo_depth
       * trick does not work here). */
      break;
    }
    case OL_LIB_RELOAD: {
      outliner_do_libdata_operation(
          C, op->reports, scene, soops, &soops->tree, lib_reload_cb, NULL);
      /* No undo push here, operator does it itself (since it's a modal one, the op_undo_depth
       * trick does not work here). */
      break;
    }
    default:
      /* invalid - unhandled */
      break;
  }

  /* wrong notifier still... */
  WM_event_add_notifier(C, NC_ID | NA_EDITED, NULL);

  /* XXX: this is just so that outliner is always up to date */
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_OUTLINER, NULL);

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_lib_operation(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Outliner Library Operation";
  ot->idname = "OUTLINER_OT_lib_operation";

  /* callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = outliner_lib_operation_exec;
  ot->poll = ED_operator_outliner_active;

  ot->prop = RNA_def_enum(
      ot->srna, "type", outliner_lib_op_type_items, 0, "Library Operation", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Outliner Set Active Action Operator
 * \{ */

static void outliner_do_id_set_operation(
    SpaceOutliner *soops,
    int type,
    ListBase *lb,
    ID *newid,
    void (*operation_cb)(TreeElement *, TreeStoreElem *, TreeStoreElem *, ID *))
{
  TreeElement *te;
  TreeStoreElem *tselem;

  for (te = lb->first; te; te = te->next) {
    tselem = TREESTORE(te);
    if (tselem->flag & TSE_SELECTED) {
      if (tselem->type == type) {
        TreeStoreElem *tsep = te->parent ? TREESTORE(te->parent) : NULL;
        operation_cb(te, tselem, tsep, newid);
      }
    }
    if (TSELEM_OPEN(tselem, soops)) {
      outliner_do_id_set_operation(soops, type, &te->subtree, newid, operation_cb);
    }
  }
}

static void actionset_id_cb(TreeElement *UNUSED(te),
                            TreeStoreElem *tselem,
                            TreeStoreElem *tsep,
                            ID *actId)
{
  bAction *act = (bAction *)actId;

  if (tselem->type == TSE_ANIM_DATA) {
    /* "animation" entries - action is child of this */
    BKE_animdata_set_action(NULL, tselem->id, act);
  }
  /* TODO: if any other "expander" channels which own actions need to support this menu,
   * add: tselem->type = ...
   */
  else if (tsep && (tsep->type == TSE_ANIM_DATA)) {
    /* "animation" entries case again */
    BKE_animdata_set_action(NULL, tsep->id, act);
  }
  /* TODO: other cases not supported yet. */
}

static int outliner_action_set_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  int scenelevel = 0, objectlevel = 0, idlevel = 0, datalevel = 0;

  bAction *act;

  /* check for invalid states */
  if (soops == NULL) {
    return OPERATOR_CANCELLED;
  }
  set_operation_types(soops, &soops->tree, &scenelevel, &objectlevel, &idlevel, &datalevel);

  /* get action to use */
  act = BLI_findlink(&bmain->actions, RNA_enum_get(op->ptr, "action"));

  if (act == NULL) {
    BKE_report(op->reports, RPT_ERROR, "No valid action to add");
    return OPERATOR_CANCELLED;
  }
  if (act->idroot == 0) {
    /* Hopefully in this case (i.e. library of userless actions),
     * the user knows what they're doing. */
    BKE_reportf(op->reports,
                RPT_WARNING,
                "Action '%s' does not specify what data-blocks it can be used on "
                "(try setting the 'ID Root Type' setting from the data-blocks editor "
                "for this action to avoid future problems)",
                act->id.name + 2);
  }

  /* perform action if valid channel */
  if (datalevel == TSE_ANIM_DATA) {
    outliner_do_id_set_operation(soops, datalevel, &soops->tree, (ID *)act, actionset_id_cb);
  }
  else if (idlevel == ID_AC) {
    outliner_do_id_set_operation(soops, idlevel, &soops->tree, (ID *)act, actionset_id_cb);
  }
  else {
    return OPERATOR_CANCELLED;
  }

  /* set notifier that things have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_NLA_ACTCHANGE, NULL);
  ED_undo_push(C, "Set action");

  /* done */
  return OPERATOR_FINISHED;
}

void OUTLINER_OT_action_set(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Outliner Set Action";
  ot->idname = "OUTLINER_OT_action_set";
  ot->description = "Change the active action used";

  /* api callbacks */
  ot->invoke = WM_enum_search_invoke;
  ot->exec = outliner_action_set_exec;
  ot->poll = ED_operator_outliner_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  /* TODO: this would be nicer as an ID-pointer... */
  prop = RNA_def_enum(ot->srna, "action", DummyRNA_NULL_items, 0, "Action", "");
  RNA_def_enum_funcs(prop, RNA_action_itemf);
  RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
  ot->prop = prop;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Animation Menu Operator
 * \{ */

typedef enum eOutliner_AnimDataOps {
  OUTLINER_ANIMOP_INVALID = 0,

  OUTLINER_ANIMOP_CLEAR_ADT,

  OUTLINER_ANIMOP_SET_ACT,
  OUTLINER_ANIMOP_CLEAR_ACT,

  OUTLINER_ANIMOP_REFRESH_DRV,
  OUTLINER_ANIMOP_CLEAR_DRV

  /* OUTLINER_ANIMOP_COPY_DRIVERS, */
  /* OUTLINER_ANIMOP_PASTE_DRIVERS */
} eOutliner_AnimDataOps;

static const EnumPropertyItem prop_animdata_op_types[] = {
    {OUTLINER_ANIMOP_CLEAR_ADT,
     "CLEAR_ANIMDATA",
     0,
     "Clear Animation Data",
     "Remove this animation data container"},
    {OUTLINER_ANIMOP_SET_ACT, "SET_ACT", 0, "Set Action", ""},
    {OUTLINER_ANIMOP_CLEAR_ACT, "CLEAR_ACT", 0, "Unlink Action", ""},
    {OUTLINER_ANIMOP_REFRESH_DRV, "REFRESH_DRIVERS", 0, "Refresh Drivers", ""},
    /* {OUTLINER_ANIMOP_COPY_DRIVERS, "COPY_DRIVERS", 0, "Copy Drivers", ""}, */
    /* {OUTLINER_ANIMOP_PASTE_DRIVERS, "PASTE_DRIVERS", 0, "Paste Drivers", ""}, */
    {OUTLINER_ANIMOP_CLEAR_DRV, "CLEAR_DRIVERS", 0, "Clear Drivers", ""},
    {0, NULL, 0, NULL, NULL},
};

static int outliner_animdata_operation_exec(bContext *C, wmOperator *op)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  int scenelevel = 0, objectlevel = 0, idlevel = 0, datalevel = 0;
  eOutliner_AnimDataOps event;

  /* check for invalid states */
  if (soops == NULL) {
    return OPERATOR_CANCELLED;
  }

  event = RNA_enum_get(op->ptr, "type");
  set_operation_types(soops, &soops->tree, &scenelevel, &objectlevel, &idlevel, &datalevel);

  if (datalevel != TSE_ANIM_DATA) {
    return OPERATOR_CANCELLED;
  }

  /* perform the core operation */
  switch (event) {
    case OUTLINER_ANIMOP_CLEAR_ADT:
      /* Remove Animation Data - this may remove the active action, in some cases... */
      outliner_do_data_operation(soops, datalevel, event, &soops->tree, clear_animdata_cb, NULL);

      WM_event_add_notifier(C, NC_ANIMATION | ND_NLA_ACTCHANGE, NULL);
      ED_undo_push(C, "Clear Animation Data");
      break;

    case OUTLINER_ANIMOP_SET_ACT:
      /* delegate once again... */
      wm->op_undo_depth++;
      WM_operator_name_call(C, "OUTLINER_OT_action_set", WM_OP_INVOKE_REGION_WIN, NULL);
      wm->op_undo_depth--;
      ED_undo_push(C, "Set active action");
      break;

    case OUTLINER_ANIMOP_CLEAR_ACT:
      /* clear active action - using standard rules */
      outliner_do_data_operation(
          soops, datalevel, event, &soops->tree, unlinkact_animdata_cb, NULL);

      WM_event_add_notifier(C, NC_ANIMATION | ND_NLA_ACTCHANGE, NULL);
      ED_undo_push(C, "Unlink action");
      break;

    case OUTLINER_ANIMOP_REFRESH_DRV:
      outliner_do_data_operation(
          soops, datalevel, event, &soops->tree, refreshdrivers_animdata_cb, NULL);

      WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN, NULL);
      /* ED_undo_push(C, "Refresh Drivers"); No undo needed - shouldn't have any impact? */
      break;

    case OUTLINER_ANIMOP_CLEAR_DRV:
      outliner_do_data_operation(
          soops, datalevel, event, &soops->tree, cleardrivers_animdata_cb, NULL);

      WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN, NULL);
      ED_undo_push(C, "Clear Drivers");
      break;

    default: /* Invalid. */
      break;
  }

  /* update dependencies */
  DEG_relations_tag_update(CTX_data_main(C));

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_animdata_operation(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Outliner Animation Data Operation";
  ot->idname = "OUTLINER_OT_animdata_operation";

  /* callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = outliner_animdata_operation_exec;
  ot->poll = ED_operator_outliner_active;

  ot->flag = 0;

  ot->prop = RNA_def_enum(ot->srna, "type", prop_animdata_op_types, 0, "Animation Operation", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Constraint Menu Operator
 * \{ */

static const EnumPropertyItem prop_constraint_op_types[] = {
    {OL_CONSTRAINTOP_ENABLE, "ENABLE", ICON_HIDE_OFF, "Enable", ""},
    {OL_CONSTRAINTOP_DISABLE, "DISABLE", ICON_HIDE_ON, "Disable", ""},
    {OL_CONSTRAINTOP_DELETE, "DELETE", ICON_X, "Delete", ""},
    {0, NULL, 0, NULL, NULL},
};

static int outliner_constraint_operation_exec(bContext *C, wmOperator *op)
{
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  int scenelevel = 0, objectlevel = 0, idlevel = 0, datalevel = 0;
  eOutliner_PropConstraintOps event;

  event = RNA_enum_get(op->ptr, "type");
  set_operation_types(soops, &soops->tree, &scenelevel, &objectlevel, &idlevel, &datalevel);

  outliner_do_data_operation(soops, datalevel, event, &soops->tree, constraint_cb, C);

  if (event == OL_CONSTRAINTOP_DELETE) {
    outliner_cleanup_tree(soops);
  }

  ED_undo_push(C, "Constraint operation");

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_constraint_operation(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Outliner Constraint Operation";
  ot->idname = "OUTLINER_OT_constraint_operation";

  /* callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = outliner_constraint_operation_exec;
  ot->poll = ED_operator_outliner_active;

  ot->flag = 0;

  ot->prop = RNA_def_enum(
      ot->srna, "type", prop_constraint_op_types, 0, "Constraint Operation", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Modifier Menu Operator
 * \{ */

static const EnumPropertyItem prop_modifier_op_types[] = {
    {OL_MODIFIER_OP_TOGVIS, "TOGVIS", ICON_RESTRICT_VIEW_OFF, "Toggle viewport use", ""},
    {OL_MODIFIER_OP_TOGREN, "TOGREN", ICON_RESTRICT_RENDER_OFF, "Toggle render use", ""},
    {OL_MODIFIER_OP_DELETE, "DELETE", ICON_X, "Delete", ""},
    {0, NULL, 0, NULL, NULL},
};

static int outliner_modifier_operation_exec(bContext *C, wmOperator *op)
{
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  int scenelevel = 0, objectlevel = 0, idlevel = 0, datalevel = 0;
  eOutliner_PropModifierOps event;

  event = RNA_enum_get(op->ptr, "type");
  set_operation_types(soops, &soops->tree, &scenelevel, &objectlevel, &idlevel, &datalevel);

  outliner_do_data_operation(soops, datalevel, event, &soops->tree, modifier_cb, C);

  if (event == OL_MODIFIER_OP_DELETE) {
    outliner_cleanup_tree(soops);
  }

  ED_undo_push(C, "Modifier operation");

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_modifier_operation(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Outliner Modifier Operation";
  ot->idname = "OUTLINER_OT_modifier_operation";

  /* callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = outliner_modifier_operation_exec;
  ot->poll = ED_operator_outliner_active;

  ot->flag = 0;

  ot->prop = RNA_def_enum(ot->srna, "type", prop_modifier_op_types, 0, "Modifier Operation", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Data Menu Operator
 * \{ */

/* XXX: select linked is for RNA structs only. */
static const EnumPropertyItem prop_data_op_types[] = {
    {OL_DOP_SELECT, "SELECT", 0, "Select", ""},
    {OL_DOP_DESELECT, "DESELECT", 0, "Deselect", ""},
    {OL_DOP_HIDE, "HIDE", 0, "Hide", ""},
    {OL_DOP_UNHIDE, "UNHIDE", 0, "Unhide", ""},
    {OL_DOP_SELECT_LINKED, "SELECT_LINKED", 0, "Select Linked", ""},
    {0, NULL, 0, NULL, NULL},
};

static int outliner_data_operation_exec(bContext *C, wmOperator *op)
{
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  int scenelevel = 0, objectlevel = 0, idlevel = 0, datalevel = 0;
  eOutliner_PropDataOps event;

  /* check for invalid states */
  if (soops == NULL) {
    return OPERATOR_CANCELLED;
  }

  event = RNA_enum_get(op->ptr, "type");
  set_operation_types(soops, &soops->tree, &scenelevel, &objectlevel, &idlevel, &datalevel);

  switch (datalevel) {
    case TSE_POSE_CHANNEL: {
      outliner_do_data_operation(soops, datalevel, event, &soops->tree, pchan_cb, NULL);
      WM_event_add_notifier(C, NC_OBJECT | ND_POSE, NULL);
      ED_undo_push(C, "PoseChannel operation");

      break;
    }
    case TSE_BONE: {
      outliner_do_data_operation(soops, datalevel, event, &soops->tree, bone_cb, NULL);
      WM_event_add_notifier(C, NC_OBJECT | ND_POSE, NULL);
      ED_undo_push(C, "Bone operation");

      break;
    }
    case TSE_EBONE: {
      outliner_do_data_operation(soops, datalevel, event, &soops->tree, ebone_cb, NULL);
      WM_event_add_notifier(C, NC_OBJECT | ND_POSE, NULL);
      ED_undo_push(C, "EditBone operation");

      break;
    }
    case TSE_SEQUENCE: {
      Scene *scene = CTX_data_scene(C);
      outliner_do_data_operation(soops, datalevel, event, &soops->tree, sequence_cb, scene);

      break;
    }
    case TSE_GP_LAYER: {
      outliner_do_data_operation(soops, datalevel, event, &soops->tree, gpencil_layer_cb, NULL);
      WM_event_add_notifier(C, NC_GPENCIL | ND_DATA, NULL);
      ED_undo_push(C, "Grease Pencil Layer operation");

      break;
    }
    case TSE_RNA_STRUCT:
      if (event == OL_DOP_SELECT_LINKED) {
        outliner_do_data_operation(
            soops, datalevel, event, &soops->tree, data_select_linked_cb, C);
      }

      break;

    default:
      BKE_report(op->reports, RPT_WARNING, "Not yet implemented");
      break;
  }

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_data_operation(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Outliner Data Operation";
  ot->idname = "OUTLINER_OT_data_operation";

  /* callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = outliner_data_operation_exec;
  ot->poll = ED_operator_outliner_active;

  ot->flag = 0;

  ot->prop = RNA_def_enum(ot->srna, "type", prop_data_op_types, 0, "Data Operation", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Context Menu Operator
 * \{ */

static int outliner_operator_menu(bContext *C, const char *opname)
{
  wmOperatorType *ot = WM_operatortype_find(opname, false);
  uiPopupMenu *pup = UI_popup_menu_begin(C, WM_operatortype_name(ot, NULL), ICON_NONE);
  uiLayout *layout = UI_popup_menu_layout(pup);

  /* set this so the default execution context is the same as submenus */
  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);
  uiItemsEnumO(layout, ot->idname, RNA_property_identifier(ot->prop));

  uiItemS(layout);

  uiItemMContents(layout, "OUTLINER_MT_context_menu");

  UI_popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

static int do_outliner_operation_event(
    bContext *C, ARegion *region, SpaceOutliner *soops, TreeElement *te, const float mval[2])
{
  ReportList *reports = CTX_wm_reports(C); /* XXX... */

  if (mval[1] > te->ys && mval[1] < te->ys + UI_UNIT_Y) {
    int scenelevel = 0, objectlevel = 0, idlevel = 0, datalevel = 0;
    TreeStoreElem *tselem = TREESTORE(te);

    /* select object that's clicked on and popup context menu */
    if (!(tselem->flag & TSE_SELECTED)) {

      if (outliner_flag_is_any_test(&soops->tree, TSE_SELECTED, 1)) {
        outliner_flag_set(&soops->tree, TSE_SELECTED, 0);
      }

      tselem->flag |= TSE_SELECTED;

      /* Only redraw, don't rebuild here because TreeElement pointers will
       * become invalid and operations will crash. */
      ED_region_tag_redraw_no_rebuild(region);
      ED_outliner_select_sync_from_outliner(C, soops);
    }

    set_operation_types(soops, &soops->tree, &scenelevel, &objectlevel, &idlevel, &datalevel);

    if (scenelevel) {
      if (objectlevel || datalevel || idlevel) {
        BKE_report(reports, RPT_WARNING, "Mixed selection");
        return OPERATOR_CANCELLED;
      }
      return outliner_operator_menu(C, "OUTLINER_OT_scene_operation");
    }
    if (objectlevel) {
      WM_menu_name_call(C, "OUTLINER_MT_object", WM_OP_INVOKE_REGION_WIN);
      return OPERATOR_FINISHED;
    }
    if (idlevel) {
      if (idlevel == -1 || datalevel) {
        BKE_report(reports, RPT_WARNING, "Mixed selection");
        return OPERATOR_CANCELLED;
      }

      switch (idlevel) {
        case ID_GR:
          WM_menu_name_call(C, "OUTLINER_MT_collection", WM_OP_INVOKE_REGION_WIN);
          return OPERATOR_FINISHED;
          break;
        case ID_LI:
          return outliner_operator_menu(C, "OUTLINER_OT_lib_operation");
          break;
        default:
          return outliner_operator_menu(C, "OUTLINER_OT_id_operation");
          break;
      }
    }
    else if (datalevel) {
      if (datalevel == -1) {
        BKE_report(reports, RPT_WARNING, "Mixed selection");
        return OPERATOR_CANCELLED;
      }
      if (datalevel == TSE_ANIM_DATA) {
        return outliner_operator_menu(C, "OUTLINER_OT_animdata_operation");
      }
      if (datalevel == TSE_DRIVER_BASE) {
        /* do nothing... no special ops needed yet */
        return OPERATOR_CANCELLED;
      }
      if (datalevel == TSE_LAYER_COLLECTION) {
        WM_menu_name_call(C, "OUTLINER_MT_collection", WM_OP_INVOKE_REGION_WIN);
        return OPERATOR_FINISHED;
      }
      if (ELEM(datalevel, TSE_SCENE_COLLECTION_BASE, TSE_VIEW_COLLECTION_BASE)) {
        WM_menu_name_call(C, "OUTLINER_MT_collection_new", WM_OP_INVOKE_REGION_WIN);
        return OPERATOR_FINISHED;
      }
      if (datalevel == TSE_ID_BASE) {
        /* do nothing... there are no ops needed here yet */
        return 0;
      }
      if (datalevel == TSE_CONSTRAINT) {
        return outliner_operator_menu(C, "OUTLINER_OT_constraint_operation");
      }
      if (datalevel == TSE_MODIFIER) {
        return outliner_operator_menu(C, "OUTLINER_OT_modifier_operation");
      }
      return outliner_operator_menu(C, "OUTLINER_OT_data_operation");
    }

    return 0;
  }

  for (te = te->subtree.first; te; te = te->next) {
    int retval = do_outliner_operation_event(C, region, soops, te, mval);
    if (retval) {
      return retval;
    }
  }

  return 0;
}

static int outliner_operation(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  uiBut *but = UI_context_active_but_get(C);
  TreeElement *te;
  float fmval[2];

  if (but) {
    UI_but_tooltip_timer_remove(C, but);
  }

  UI_view2d_region_to_view(&region->v2d, event->mval[0], event->mval[1], &fmval[0], &fmval[1]);

  for (te = soops->tree.first; te; te = te->next) {
    int retval = do_outliner_operation_event(C, region, soops, te, fmval);
    if (retval) {
      return retval;
    }
  }

  /* Let this fall through to 'OUTLINER_MT_context_menu'. */
  return OPERATOR_PASS_THROUGH;
}

/* Menu only! Calls other operators */
void OUTLINER_OT_operation(wmOperatorType *ot)
{
  ot->name = "Context Menu";
  ot->idname = "OUTLINER_OT_operation";
  ot->description = "Context menu for item operations";

  ot->invoke = outliner_operation;

  ot->poll = ED_operator_outliner_active;
}

/** \} */
