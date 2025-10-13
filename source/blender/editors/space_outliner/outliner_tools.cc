/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "MEM_guardedalloc.h"

#include "CLG_log.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_collection_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curves_types.h"
#include "DNA_gpencil_legacy_types.h"
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
#include "DNA_volume_types.h"
#include "DNA_world_types.h"

#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_set.hh"
#include "BLI_string.h"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "BLT_translation.hh"

#include "BKE_anim_data.hh"
#include "BKE_animsys.h"
#include "BKE_armature.hh"
#include "BKE_collection.hh"
#include "BKE_constraint.h"
#include "BKE_context.hh"
#include "BKE_fcurve.hh"
#include "BKE_global.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_idtype.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_override.hh"
#include "BKE_lib_query.hh"
#include "BKE_lib_remap.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_main_invariants.hh"
#include "BKE_object.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"
#include "BKE_screen.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "ED_node.hh"
#include "ED_object.hh"
#include "ED_outliner.hh"
#include "ED_scene.hh"
#include "ED_screen.hh"
#include "ED_sequencer.hh"
#include "ED_undo.hh"

#include "WM_api.hh"
#include "WM_message.hh"
#include "WM_types.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "../../blender/blenloader/BLO_readfile.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "ANIM_action_legacy.hh"

#include "SEQ_relations.hh"
#include "SEQ_sequencer.hh"

#include "outliner_intern.hh"
#include "tree/tree_element_grease_pencil_node.hh"
#include "tree/tree_element_rna.hh"
#include "tree/tree_element_seq.hh"
#include "tree/tree_iterator.hh"

namespace blender::ed::outliner {

static CLG_LogRef LOG = {"outliner.tools"};

/* -------------------------------------------------------------------- */
/** \name ID/Library/Data Set/Un-link Utilities
 * \{ */

static void get_element_operation_type(
    const TreeElement *te, int *scenelevel, int *objectlevel, int *idlevel, int *datalevel)
{
  *scenelevel = *objectlevel = *idlevel = *datalevel = 0;

  const TreeStoreElem *tselem = TREESTORE(te);
  if ((tselem->flag & TSE_SELECTED) == 0) {
    return;
  }

  /* Layer collection points to collection ID. */
  if (!ELEM(tselem->type, TSE_SOME_ID, TSE_LAYER_COLLECTION)) {
    *datalevel = tselem->type;
  }
  else {
    const int idcode = int(GS(tselem->id->name));
    bool is_standard_id = false;
    switch ((ID_Type)idcode) {
      case ID_SCE:
        *scenelevel = 1;
        break;
      case ID_OB:
        *objectlevel = 1;
        break;

      case ID_ME:
      case ID_CU_LEGACY:
      case ID_MB:
      case ID_LT:
      case ID_LA:
      case ID_AR:
      case ID_CA:
      case ID_SPK:
      case ID_MA:
      case ID_TE:
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
      case ID_GD_LEGACY:
      case ID_MC:
      case ID_MSK:
      case ID_PAL:
      case ID_PC:
      case ID_CF:
      case ID_WS:
      case ID_LP:
      case ID_CV:
      case ID_PT:
      case ID_VO:
      case ID_GP:
        is_standard_id = true;
        break;
      case ID_WM:
      case ID_SCR:
        /* Those are ignored here. */
        /* NOTE: while Screens should be manageable here, deleting a screen used by a workspace
         * will cause crashes when trying to use that workspace, so for now let's play minimal,
         * safe change. */
        break;
    }
    if (idcode == ID_NLA) {
      /* Fake one, not an actual ID type... */
      is_standard_id = true;
    }

    if (is_standard_id) {
      *idlevel = idcode;
    }
  }

  /* Return values are exclusive, only one may be non-null. */
  BLI_assert(((*scenelevel != 0) && (*objectlevel == 0) && (*idlevel == 0) && (*datalevel == 0)) ||
             ((*scenelevel == 0) && (*objectlevel != 0) && (*idlevel == 0) && (*datalevel == 0)) ||
             ((*scenelevel == 0) && (*objectlevel == 0) && (*idlevel != 0) && (*datalevel == 0)) ||
             ((*scenelevel == 0) && (*objectlevel == 0) && (*idlevel == 0) && (*datalevel != 0)) ||
             /* All null. */
             ((*scenelevel == 0) && (*objectlevel == 0) && (*idlevel == 0) && (*datalevel == 0)));
}

static TreeElement *get_target_element(const SpaceOutliner *space_outliner)
{
  TreeElement *te = outliner_find_element_with_flag(&space_outliner->tree, TSE_ACTIVE);

  return te;
}

static bool outliner_operation_tree_element_poll(bContext *C)
{
  if (!ED_operator_outliner_active(C)) {
    return false;
  }
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  TreeElement *te = get_target_element(space_outliner);
  if (te == nullptr) {
    return false;
  }

  return true;
}

static void unlink_action_fn(bContext *C,
                             ReportList *reports,
                             Scene * /*scene*/,
                             TreeElement * /*te*/,
                             TreeStoreElem *tsep,
                             TreeStoreElem *tselem)
{
  if (!tsep || !TSE_IS_REAL_ID(tsep)) {
    /* Valid case, no parent element of the action or it is not an ID (could be a #TSE_ID_BASE
     * for example) so there's no data to unlink from. */
    BKE_reportf(reports,
                RPT_WARNING,
                "Cannot unlink action '%s'. It's not clear which object or object-data it "
                "should be unlinked from, there's no object or object-data as parent in the "
                "Outliner tree",
                tselem->id->name + 2);
    return;
  }

  /* just set action to nullptr */
  BKE_animdata_set_action(CTX_wm_reports(C), tsep->id, nullptr);
  DEG_id_tag_update(tsep->id, ID_RECALC_ANIMATION);
}

static void unlink_material_fn(bContext * /*C*/,
                               ReportList *reports,
                               Scene * /*scene*/,
                               TreeElement *te,
                               TreeStoreElem *tsep,
                               TreeStoreElem *tselem)
{
  const bool te_is_material = TSE_IS_REAL_ID(tselem) && (GS(tselem->id->name) == ID_MA);

  if (!te_is_material) {
    /* Just fail silently. Another element may be selected that is a material, we don't want to
     * confuse users with an error in that case. */
    return;
  }

  if (!tsep || !TSE_IS_REAL_ID(tsep)) {
    /* Valid case, no parent element of the material or it is not an ID (could be a #TSE_ID_BASE
     * for example) so there's no data to unlink from. */
    BKE_reportf(reports,
                RPT_WARNING,
                "Cannot unlink material '%s'. It's not clear which object or object-data it "
                "should be unlinked from, there's no object or object-data as parent in the "
                "Outliner tree",
                tselem->id->name + 2);
    return;
  }

  if (!ID_IS_EDITABLE(tsep->id) || ID_IS_OVERRIDE_LIBRARY(tsep->id)) {
    BKE_reportf(reports,
                RPT_WARNING,
                "Cannot unlink the material '%s' from linked object data",
                tselem->id->name + 2);
    return;
  }

  Material **matar = nullptr;
  int a, totcol = 0;

  switch (GS(tsep->id->name)) {
    case ID_OB: {
      Object *ob = (Object *)tsep->id;
      totcol = ob->totcol;
      matar = ob->mat;
      break;
    }
    case ID_ME: {
      Mesh *mesh = (Mesh *)tsep->id;
      totcol = mesh->totcol;
      matar = mesh->mat;
      break;
    }
    case ID_CU_LEGACY: {
      Curve *cu = (Curve *)tsep->id;
      totcol = cu->totcol;
      matar = cu->mat;
      break;
    }
    case ID_MB: {
      MetaBall *mb = (MetaBall *)tsep->id;
      totcol = mb->totcol;
      matar = mb->mat;
      break;
    }
    case ID_CV: {
      Curves *curves = (Curves *)tsep->id;
      totcol = curves->totcol;
      matar = curves->mat;
      break;
    }
    case ID_PT: {
      PointCloud *pointcloud = (PointCloud *)tsep->id;
      totcol = pointcloud->totcol;
      matar = pointcloud->mat;
      break;
    }
    case ID_VO: {
      Volume *volume = (Volume *)tsep->id;
      totcol = volume->totcol;
      matar = volume->mat;
      break;
    }
    default:
      BLI_assert_unreachable();
  }

  if (LIKELY(matar != nullptr)) {
    for (a = 0; a < totcol; a++) {
      if (a == te->index && matar[a]) {
        id_us_min(&matar[a]->id);
        matar[a] = nullptr;
      }
    }
  }
}

static void unlink_texture_fn(bContext * /*C*/,
                              ReportList *reports,
                              Scene * /*scene*/,
                              TreeElement *te,
                              TreeStoreElem *tsep,
                              TreeStoreElem *tselem)
{
  if (!tsep || !TSE_IS_REAL_ID(tsep)) {
    /* Valid case, no parent element of the texture or it is not an ID (could be a #TSE_ID_BASE
     * for example) so there's no data to unlink from. */
    BKE_reportf(reports,
                RPT_WARNING,
                "Cannot unlink texture '%s'. It's not clear which Freestyle line style it should "
                "be unlinked from, there's no Freestyle line style as parent in the Outliner tree",
                tselem->id->name + 2);
    return;
  }

  MTex **mtex = nullptr;
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
        mtex[a]->tex = nullptr;
      }
    }
  }
}

static void unlink_collection_fn(bContext *C,
                                 ReportList *reports,
                                 Scene * /*scene*/,
                                 TreeElement * /*te*/,
                                 TreeStoreElem *tsep,
                                 TreeStoreElem *tselem)
{
  Main *bmain = CTX_data_main(C);
  Collection *collection = (Collection *)tselem->id;

  if (!tsep || !TSE_IS_REAL_ID(tsep)) {
    /* Valid case, no parent element of the collection or it is not an ID (could be a #TSE_ID_BASE
     * for example) so there's no data to unlink from. */
    BKE_reportf(reports,
                RPT_WARNING,
                "Cannot unlink collection '%s'. It's not clear which scene, collection or "
                "instance empties it should be unlinked from, there's no scene, collection or "
                "instance empties as parent in the Outliner tree",
                tselem->id->name + 2);
    return;
  }

  if (tsep && (!ID_IS_EDITABLE(tsep->id) || ID_IS_OVERRIDE_LIBRARY(tsep->id))) {
    BKE_reportf(reports,
                RPT_WARNING,
                "Cannot unlink collection '%s' parented to another linked collection '%s'",
                collection->id.name + 2,
                tsep->id->name + 2);
    return;
  }

  if (tsep) {
    if (GS(tsep->id->name) == ID_OB) {
      Object *ob = (Object *)tsep->id;
      ob->instance_collection = nullptr;
      DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM | ID_RECALC_HIERARCHY);
      DEG_relations_tag_update(bmain);
    }
    else if (GS(tsep->id->name) == ID_GR) {
      Collection *parent = (Collection *)tsep->id;
      id_fake_user_set(&collection->id);
      BKE_collection_child_remove(bmain, parent, collection);
      DEG_id_tag_update(&parent->id, ID_RECALC_SYNC_TO_EVAL | ID_RECALC_HIERARCHY);
      DEG_relations_tag_update(bmain);
    }
    else if (GS(tsep->id->name) == ID_SCE) {
      Scene *scene = (Scene *)tsep->id;
      Collection *parent = scene->master_collection;
      id_fake_user_set(&collection->id);
      BKE_collection_child_remove(bmain, parent, collection);
      DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL | ID_RECALC_HIERARCHY);
      DEG_relations_tag_update(bmain);
    }
  }
}

static void unlink_object_fn(bContext *C,
                             ReportList *reports,
                             Scene * /*scene*/,
                             TreeElement *te,
                             TreeStoreElem *tsep,
                             TreeStoreElem *tselem)
{
  if (tsep && tsep->id) {

    if (!TSE_IS_REAL_ID(tsep)) {
      return;
    }
    Main *bmain = CTX_data_main(C);
    Object *ob = (Object *)tselem->id;
    const eSpaceOutliner_Mode outliner_mode = eSpaceOutliner_Mode(
        CTX_wm_space_outliner(C)->outlinevis);

    if (GS(tsep->id->name) == ID_OB) {
      /* Parented objects need to find which collection to unlink from. */
      TreeElement *te_parent = te->parent;
      while (tsep && GS(tsep->id->name) == ID_OB) {
        if (!ID_IS_EDITABLE(tsep->id)) {
          BKE_reportf(reports,
                      RPT_WARNING,
                      "Cannot unlink object '%s' parented to another linked object '%s'",
                      ob->id.name + 2,
                      tsep->id->name + 2);
          return;
        }
        te_parent = te_parent->parent;
        tsep = te_parent ? TREESTORE(te_parent) : nullptr;
      }
    }

    if (tsep && tsep->id) {
      if (!ID_IS_EDITABLE(tsep->id) || ID_IS_OVERRIDE_LIBRARY(tsep->id)) {
        BKE_reportf(reports,
                    RPT_WARNING,
                    "Cannot unlink object '%s' from linked collection or scene '%s'",
                    ob->id.name + 2,
                    tsep->id->name + 2);
        return;
      }
      switch (GS(tsep->id->name)) {
        case ID_GR: {
          Collection *parent = (Collection *)tsep->id;
          BKE_collection_object_remove(bmain, parent, ob, true);
          DEG_id_tag_update(&parent->id, ID_RECALC_SYNC_TO_EVAL);
          break;
        }
        case ID_SCE: {
          Scene *scene = reinterpret_cast<Scene *>(tsep->id);
          /* In Scene view, remove the object from all collections in the scene. */
          if (outliner_mode == SO_SCENES) {
            FOREACH_SCENE_COLLECTION_BEGIN (scene, collection) {
              if (BKE_collection_has_object(collection, ob)) {
                BKE_collection_object_remove(bmain, collection, ob, true);
                DEG_id_tag_update(&collection->id, ID_RECALC_HIERARCHY);
                DEG_id_tag_update(&collection->id, ID_RECALC_SYNC_TO_EVAL);
              }
            }
            FOREACH_SCENE_COLLECTION_END;
          }
          /* Otherwise, remove the object from the scene's main collection. */
          else {
            Collection *parent = scene->master_collection;
            BKE_collection_object_remove(bmain, parent, ob, true);
            DEG_id_tag_update(&parent->id, ID_RECALC_SYNC_TO_EVAL);
          }
          break;
        }
        default: {
          /* Un-handled case, should never be reached. */
          BLI_assert_unreachable();
          return;
        }
      }
      /* NOTE: Cannot risk tagging the object here, as it may have been deleted if its last usage
       * was removed by above code. */
      DEG_id_tag_update(tsep->id, ID_RECALC_HIERARCHY);
      DEG_relations_tag_update(bmain);
    }
  }
}

static void unlink_world_fn(bContext * /*C*/,
                            ReportList *reports,
                            Scene * /*scene*/,
                            TreeElement * /*te*/,
                            TreeStoreElem *tsep,
                            TreeStoreElem *tselem)
{
  if (!tsep || !TSE_IS_REAL_ID(tsep)) {
    /* Valid case, no parent element of the world or it is not an ID (could be a #TSE_ID_BASE
     * for example) so there's no data to unlink from. */
    BKE_reportf(reports,
                RPT_WARNING,
                "Cannot unlink world '%s'. It's not clear which scene it should be unlinked from, "
                "there's no scene as parent in the Outliner tree",
                tselem->id->name + 2);
    return;
  }

  Scene *parscene = (Scene *)tsep->id;
  World *wo = (World *)tselem->id;

  /* need to use parent scene not just scene, otherwise may end up getting wrong one */
  id_us_min(&wo->id);
  parscene->world = nullptr;
}

static void outliner_do_libdata_operation(bContext *C,
                                          ReportList *reports,
                                          Scene *scene,
                                          SpaceOutliner *space_outliner,
                                          outliner_operation_fn operation_fn)
{
  tree_iterator::all_open(*space_outliner, [&](TreeElement *te) {
    TreeStoreElem *tselem = TREESTORE(te);
    if (tselem->flag & TSE_SELECTED) {
      if (((tselem->type == TSE_SOME_ID) && (te->idcode != 0)) ||
          tselem->type == TSE_LAYER_COLLECTION)
      {
        TreeStoreElem *tsep = te->parent ? TREESTORE(te->parent) : nullptr;
        operation_fn(C, reports, scene, te, tsep, tselem);
      }
    }
  });
}

enum eOutlinerLibOpSelectionSet {
  /* Only selected items. */
  OUTLINER_LIB_SELECTIONSET_SELECTED,
  /* Only content 'inside' selected items (their sub-tree). */
  OUTLINER_LIB_LIB_SELECTIONSET_CONTENT,
  /* Combining both options above. */
  OUTLINER_LIB_LIB_SELECTIONSET_SELECTED_AND_CONTENT,
};

static const EnumPropertyItem prop_lib_op_selection_set[] = {
    {OUTLINER_LIB_SELECTIONSET_SELECTED,
     "SELECTED",
     0,
     "Selected",
     "Apply the operation over selected data-blocks only"},
    {OUTLINER_LIB_LIB_SELECTIONSET_CONTENT,
     "CONTENT",
     0,
     "Content",
     "Apply the operation over content of the selected items only (the data-blocks in their "
     "sub-tree)"},
    {OUTLINER_LIB_LIB_SELECTIONSET_SELECTED_AND_CONTENT,
     "SELECTED_AND_CONTENT",
     0,
     "Selected & Content",
     "Apply the operation over selected data-blocks and all their dependencies"},
    {0, nullptr, 0, nullptr, nullptr},
};

static bool outliner_do_libdata_operation_selection_set_element(
    bContext *C,
    ReportList *reports,
    Scene *scene,
    TreeElement *element,
    TreeStoreElem *tselem,
    const bool has_parent_selected,
    outliner_operation_fn operation_fn,
    eOutlinerLibOpSelectionSet selection_set)
{
  const bool do_selected = ELEM(selection_set,
                                OUTLINER_LIB_SELECTIONSET_SELECTED,
                                OUTLINER_LIB_LIB_SELECTIONSET_SELECTED_AND_CONTENT);
  const bool do_content = ELEM(selection_set,
                               OUTLINER_LIB_LIB_SELECTIONSET_CONTENT,
                               OUTLINER_LIB_LIB_SELECTIONSET_SELECTED_AND_CONTENT);

  const bool is_selected = tselem->flag & TSE_SELECTED;
  if ((is_selected && do_selected) || (has_parent_selected && do_content)) {
    if (((tselem->type == TSE_SOME_ID) && (element->idcode != 0)) ||
        tselem->type == TSE_LAYER_COLLECTION)
    {
      TreeStoreElem *tsep = element->parent ? TREESTORE(element->parent) : nullptr;
      operation_fn(C, reports, scene, element, tsep, tselem);
    }
  }
  return is_selected;
}

static void outliner_do_libdata_operation_selection_set(bContext *C,
                                                        ReportList *reports,
                                                        Scene *scene,
                                                        SpaceOutliner *space_outliner,
                                                        const ListBase &subtree,
                                                        const bool has_parent_selected,
                                                        outliner_operation_fn operation_fn,
                                                        eOutlinerLibOpSelectionSet selection_set)
{
  LISTBASE_FOREACH_MUTABLE (TreeElement *, element, &subtree) {
    /* Get needed data out in case element gets freed. */
    TreeStoreElem *tselem = TREESTORE(element);
    const ListBase subtree = element->subtree;

    const bool is_selected = outliner_do_libdata_operation_selection_set_element(
        C, reports, scene, element, tselem, has_parent_selected, operation_fn, selection_set);

    /* Don't access element from now on, it may be freed. Note that the open/collapsed state may
     * also have been changed in the visitor callback. */
    outliner_do_libdata_operation_selection_set(C,
                                                reports,
                                                scene,
                                                space_outliner,
                                                subtree,
                                                is_selected || has_parent_selected,
                                                operation_fn,
                                                selection_set);
  }
}

static void outliner_do_libdata_operation_selection_set(bContext *C,
                                                        ReportList *reports,
                                                        Scene *scene,
                                                        SpaceOutliner *space_outliner,
                                                        outliner_operation_fn operation_fn,
                                                        eOutlinerLibOpSelectionSet selection_set,
                                                        const bool do_active_element_first)
{
  if (do_active_element_first) {
    TreeElement *active_element = outliner_find_element_with_flag(&space_outliner->tree,
                                                                  TSE_ACTIVE);
    if (active_element != nullptr) {
      TreeStoreElem *tselem = TREESTORE(active_element);
      const ListBase subtree = active_element->subtree;

      const bool is_selected = outliner_do_libdata_operation_selection_set_element(
          C, reports, scene, active_element, tselem, false, operation_fn, selection_set);

      /* Don't access element from now on, it may be freed. Note that the open/collapsed state may
       * also have been changed in the visitor callback. */
      outliner_do_libdata_operation_selection_set(
          C, reports, scene, space_outliner, subtree, is_selected, operation_fn, selection_set);
    }
  }

  outliner_do_libdata_operation_selection_set(
      C, reports, scene, space_outliner, space_outliner->tree, false, operation_fn, selection_set);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Scene Menu Operator
 * \{ */

enum eOutliner_PropSceneOps {
  OL_SCENE_OP_DELETE = 1,
};

static const EnumPropertyItem prop_scene_op_types[] = {
    {OL_SCENE_OP_DELETE, "DELETE", ICON_X, "Delete", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static bool outliner_do_scene_operation(
    bContext *C,
    SpaceOutliner *space_outliner,
    eOutliner_PropSceneOps event,
    bool (*operation_fn)(bContext *, eOutliner_PropSceneOps, TreeElement *, TreeStoreElem *))
{
  bool success = false;

  tree_iterator::all_open(*space_outliner, [&](TreeElement *te) {
    TreeStoreElem *tselem = TREESTORE(te);
    if (tselem->flag & TSE_SELECTED) {
      if ((tselem->type == TSE_SOME_ID) && (te->idcode == ID_SCE)) {
        if (operation_fn(C, event, te, tselem)) {
          success = true;
        }
      }
    }
  });

  return success;
}

static bool scene_fn(bContext *C,
                     eOutliner_PropSceneOps event,
                     TreeElement * /*te*/,
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

static wmOperatorStatus outliner_scene_operation_exec(bContext *C, wmOperator *op)
{
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  const eOutliner_PropSceneOps event = (eOutliner_PropSceneOps)RNA_enum_get(op->ptr, "type");

  if (outliner_do_scene_operation(C, space_outliner, event, scene_fn) == false) {
    return OPERATOR_CANCELLED;
  }

  if (event == OL_SCENE_OP_DELETE) {
    outliner_cleanup_tree(space_outliner);
    ED_undo_push(C, "Delete Scene(s)");
  }
  else {
    BLI_assert_unreachable();
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
struct MergedSearchData {
  TreeElement *parent_element;
  TreeElement *select_element;
};

static void merged_element_search_fn_recursive(
    const ListBase *tree, short tselem_type, short type, const char *str, uiSearchItems *items)
{
  char name[64];
  int iconid;

  LISTBASE_FOREACH (TreeElement *, te, tree) {
    TreeStoreElem *tselem = TREESTORE(te);

    if (tree_element_id_type_to_index(te) == type && tselem_type == tselem->type) {
      if (BLI_strcasestr(te->name, str)) {
        STRNCPY(name, te->name);

        iconid = tree_element_get_icon(tselem, te).icon;

        /* Don't allow duplicate named items */
        if (UI_search_items_find_index(items, name) == -1) {
          if (!UI_search_item_add(items, name, te, iconid, 0, 0)) {
            break;
          }
        }
      }
    }

    merged_element_search_fn_recursive(&te->subtree, tselem_type, type, str, items);
  }
}

/* Get a list of elements that match the search string */
static void merged_element_search_update_fn(const bContext * /*C*/,
                                            void *data,
                                            const char *str,
                                            uiSearchItems *items,
                                            const bool /*is_first*/)
{
  MergedSearchData *search_data = (MergedSearchData *)data;
  TreeElement *parent = search_data->parent_element;
  TreeElement *te = search_data->select_element;

  int type = tree_element_id_type_to_index(te);

  merged_element_search_fn_recursive(&parent->subtree, TREESTORE(te)->type, type, str, items);
}

/* Activate an element from the merged element search menu */
static void merged_element_search_exec_fn(bContext *C, void * /*arg1*/, void *element)
{
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  TreeElement *te = (TreeElement *)element;

  outliner_item_select(C, space_outliner, te, OL_ITEM_SELECT | OL_ITEM_ACTIVATE);

  ED_outliner_select_sync_from_outliner(C, space_outliner);
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

  block = UI_block_begin(C, region, __func__, ui::EmbossType::Emboss);
  UI_block_flag_enable(block, UI_BLOCK_LOOP | UI_BLOCK_MOVEMOUSE_QUIT | UI_BLOCK_SEARCH_MENU);
  UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);

  short menu_width = 10 * UI_UNIT_X;
  but = uiDefSearchBut(
      block, search, 0, ICON_VIEWZOOM, sizeof(search), 0, 0, menu_width, UI_UNIT_Y, "");
  UI_but_func_search_set(but,
                         nullptr,
                         merged_element_search_update_fn,
                         data,
                         false,
                         nullptr,
                         merged_element_search_exec_fn,
                         nullptr);
  UI_but_flag_enable(but, UI_BUT_ACTIVATE_ON_INIT);

  /* Fake button to hold space for search items */
  const int height = UI_searchbox_size_y() - UI_SEARCHBOX_BOUNDS;
  uiDefBut(
      block, ButType::Label, 0, "", 0, -height, menu_width, height, nullptr, 0, 0, std::nullopt);

  /* Center the menu on the cursor */
  const int offset[2] = {-(menu_width / 2), 0};
  UI_block_bounds_set_popup(block, UI_SEARCHBOX_BOUNDS, offset);

  return block;
}

void merged_element_search_menu_invoke(bContext *C,
                                       TreeElement *parent_te,
                                       TreeElement *activate_te)
{
  MergedSearchData *select_data = MEM_callocN<MergedSearchData>("merge_search_data");
  select_data->parent_element = parent_te;
  select_data->select_element = activate_te;

  UI_popup_block_invoke(C, merged_element_search_menu, select_data, MEM_freeN);
}

static void object_select_fn(bContext *C,
                             ReportList * /*reports*/,
                             Scene * /*scene*/,
                             TreeElement * /*te*/,
                             TreeStoreElem * /*tsep*/,
                             TreeStoreElem *tselem)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = (Object *)tselem->id;
  BKE_view_layer_synced_ensure(scene, view_layer);
  Base *base = BKE_view_layer_base_find(view_layer, ob);

  if (base) {
    object::base_select(base, object::BA_SELECT);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Callbacks (Selection, Users & Library) Utilities
 * \{ */

static void object_select_hierarchy_fn(bContext *C,
                                       ReportList * /*reports*/,
                                       Scene * /*scene*/,
                                       TreeElement *te,
                                       TreeStoreElem * /*tsep*/,
                                       TreeStoreElem * /*tselem*/)
{
  /* Don't extend because this toggles, which is nice for Ctrl-Click but not for a menu item.
   * it's especially confusing when multiple items are selected since some toggle on/off. */
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  outliner_item_select(
      C, space_outliner, te, OL_ITEM_SELECT | OL_ITEM_ACTIVATE | OL_ITEM_RECURSIVE);
}

static void object_deselect_fn(bContext *C,
                               ReportList * /*reports*/,
                               Scene * /*scene*/,
                               TreeElement * /*te*/,
                               TreeStoreElem * /*tsep*/,
                               TreeStoreElem *tselem)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = (Object *)tselem->id;
  BKE_view_layer_synced_ensure(scene, view_layer);
  Base *base = BKE_view_layer_base_find(view_layer, ob);

  if (base) {
    base->flag &= ~BASE_SELECTED;
  }
}

static void outliner_object_delete_fn(bContext *C, ReportList *reports, Scene *scene, Object *ob)
{
  if (ob) {
    Main *bmain = CTX_data_main(C);
    if (ob->id.tag & ID_TAG_INDIRECT) {
      BKE_reportf(
          reports, RPT_WARNING, "Cannot delete indirectly linked object '%s'", ob->id.name + 2);
      return;
    }
    if (ID_REAL_USERS(ob) <= 1 && ID_EXTRA_USERS(ob) == 0 &&
        BKE_library_ID_is_indirectly_used(bmain, ob))
    {
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
      object::editmode_exit_ex(bmain, scene, ob, object::EM_FREEDATA);
    }
    BKE_id_delete(bmain, ob);
  }
}

static void id_local_fn(bContext *C,
                        ReportList * /*reports*/,
                        Scene * /*scene*/,
                        TreeElement * /*te*/,
                        TreeStoreElem * /*tsep*/,
                        TreeStoreElem *tselem)
{
  if (ID_IS_LINKED(tselem->id) && (tselem->id->tag & ID_TAG_EXTERN)) {
    Main *bmain = CTX_data_main(C);
    if (BKE_lib_id_make_local(bmain, tselem->id, LIB_ID_MAKELOCAL_ASSET_DATA_CLEAR)) {
      BKE_id_newptr_and_tag_clear(tselem->id);
    }
  }
  else if (ID_IS_OVERRIDE_LIBRARY_REAL(tselem->id)) {
    BKE_lib_override_library_make_local(CTX_data_main(C), tselem->id);
  }
}

struct OutlinerLiboverrideDataIDRoot {
  /** The linked ID that was selected for override. */
  ID *id_root_reference;

  /** The root of the override hierarchy to which the override of `id_root` belongs, once
   * known/created. */
  ID *id_hierarchy_root_override;

  /** The ID that was detected as being a good candidate as instantiation hint for newly overridden
   * objects, may be null.
   *
   * \note Typically currently only used when the root ID to override is a collection instanced by
   * an empty object. */
  ID *id_instance_hint;

  /** If this override comes from an instancing object (which would be `id_instance_hint` then). */
  bool is_override_instancing_object;
};

struct OutlinerLibOverrideData {
  bool do_hierarchy;

  /** When creating new overrides, make them all user-editable. */
  bool do_fully_editable;

  /**
   * For resync operation, force keeping newly created override IDs (or original linked IDs)
   * instead of re-applying relevant existing ID pointer property override operations. Helps
   * solving broken overrides while not losing *all* of your overrides. */
  bool do_resync_hierarchy_enforce;

  /** A set of the selected tree elements' ID 'uid'. Used to clear 'system override' flags on
   * their newly-created liboverrides in post-process step of override hierarchy creation. */
  Set<uint> selected_id_uid;

  /** A mapping from the found hierarchy roots to a linked list of IDs to override for each of
   * these roots.
   *
   * \note the key may be either linked (in which case it will be replaced by the newly created
   * override), or an actual already existing override. */
  Map<ID *, Vector<OutlinerLiboverrideDataIDRoot>> id_hierarchy_roots;

  /** All 'session_uid' of all hierarchy root IDs used or created by the operation. */
  Set<uint> id_hierarchy_roots_uid;

  void id_root_add(ID *id_hierarchy_root_reference,
                   ID *id_root_reference,
                   ID *id_instance_hint,
                   const bool is_override_instancing_object)
  {
    OutlinerLiboverrideDataIDRoot id_root_data;
    id_root_data.id_root_reference = id_root_reference;
    id_root_data.id_hierarchy_root_override = nullptr;
    id_root_data.id_instance_hint = id_instance_hint;
    id_root_data.is_override_instancing_object = is_override_instancing_object;

    Vector<OutlinerLiboverrideDataIDRoot> &value = id_hierarchy_roots.lookup_or_add_default(
        id_hierarchy_root_reference);
    value.append(id_root_data);
  }
  void id_root_set(ID *id_hierarchy_root_reference)
  {
    OutlinerLiboverrideDataIDRoot id_root_data;
    id_root_data.id_root_reference = nullptr;
    id_root_data.id_hierarchy_root_override = nullptr;
    id_root_data.id_instance_hint = nullptr;
    id_root_data.is_override_instancing_object = false;

    Vector<OutlinerLiboverrideDataIDRoot> &value = id_hierarchy_roots.lookup_or_add_default(
        id_hierarchy_root_reference);
    if (value.is_empty()) {
      value.append(id_root_data);
    }
  }
};

/* Store 'UUID' of IDs of selected elements in the Outliner tree, before generating the override
 * hierarchy. */
static void id_override_library_create_hierarchy_pre_process(bContext *C,
                                                             OutlinerLibOverrideData *data,
                                                             ReportList *reports,
                                                             TreeElement *te,
                                                             TreeStoreElem *tsep,
                                                             TreeStoreElem *tselem)
{
  BLI_assert(TSE_IS_REAL_ID(tselem));

  const bool do_hierarchy = data->do_hierarchy;
  ID *id_root_reference = tselem->id;

  if (!BKE_idtype_idcode_is_linkable(GS(id_root_reference->name)) ||
      (id_root_reference->flag & (ID_FLAG_EMBEDDED_DATA | ID_FLAG_EMBEDDED_DATA_LIB_OVERRIDE)) !=
          0)
  {
    return;
  }

  /* Only process a given ID once. Otherwise, all kind of weird things can happen if e.g. a
   * selected sub-collection is part of more than one override hierarchies. */
  if (data->selected_id_uid.contains(id_root_reference->session_uid)) {
    return;
  }
  data->selected_id_uid.add(id_root_reference->session_uid);

  if (ID_IS_OVERRIDE_LIBRARY_REAL(id_root_reference) && !ID_IS_LINKED(id_root_reference)) {
    id_root_reference->override_library->flag &= ~LIBOVERRIDE_FLAG_SYSTEM_DEFINED;
    return;
  }

  if (!ID_IS_OVERRIDABLE_LIBRARY_HIERARCHY(id_root_reference)) {
    if (ID_IS_LINKED(id_root_reference)) {
      BKE_reportf(
          reports,
          RPT_WARNING,
          "Could not create library override from data-block '%s', as it is not overridable",
          id_root_reference->name);
    }
    /* Else it's a local ID, do not bother reporting this, as it gets annoyingly noisy then when
     * operated e.g. on a hierarchy of liboverrides. */
    return;
  }

  BLI_assert(do_hierarchy);
  UNUSED_VARS_NDEBUG(do_hierarchy);

  if (GS(id_root_reference->name) == ID_GR && (tselem->flag & TSE_CLOSED) != 0) {
    /* If selected element is a (closed) collection, check all of its objects recursively, and also
     * consider the armature ones as 'selected' (i.e. to not become system overrides). */
    Collection *root_collection = reinterpret_cast<Collection *>(id_root_reference);
    FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (root_collection, object_iter) {
      if (id_root_reference->lib == object_iter->id.lib && object_iter->type == OB_ARMATURE) {
        data->selected_id_uid.add(object_iter->id.session_uid);
      }
    }
    FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
  }

  ID *id_instance_hint = nullptr;
  bool is_override_instancing_object = false;
  if (tsep != nullptr && tsep->type == TSE_SOME_ID && tsep->id != nullptr &&
      GS(tsep->id->name) == ID_OB && !ID_IS_OVERRIDE_LIBRARY(tsep->id))
  {
    Object *ob = reinterpret_cast<Object *>(tsep->id);
    if (ob->type == OB_EMPTY && &ob->instance_collection->id == id_root_reference) {
      BLI_assert(GS(id_root_reference->name) == ID_GR);
      /* Empty instantiating the collection we override, we need to pass it to BKE overriding code
       * for proper handling. */
      id_instance_hint = tsep->id;
      is_override_instancing_object = true;
    }
  }

  if (!ID_IS_OVERRIDABLE_LIBRARY(id_root_reference) &&
      !(ID_IS_LINKED(id_root_reference) && do_hierarchy))
  {
    return;
  }

  Main *bmain = CTX_data_main(C);

  if (do_hierarchy) {
    /* Tag all linked parents in tree hierarchy to be also overridden. */
    ID *id_hierarchy_root_reference = id_root_reference;
    while ((te = te->parent) != nullptr) {
      if (!TSE_IS_REAL_ID(te->store_elem)) {
        continue;
      }

      /* Tentative hierarchy root. */
      ID *id_current_hierarchy_root = te->store_elem->id;

      /* If the parent ID is from a different library than the reference root one, we are done
       * with upwards tree processing in any case. */
      if (id_current_hierarchy_root->lib != id_root_reference->lib) {
        if (ID_IS_OVERRIDE_LIBRARY_VIRTUAL(id_current_hierarchy_root)) {
          /* Virtual overrides (i.e. embedded IDs), we can simply keep processing their parent to
           * get an actual real override. */
          continue;
        }

        /* If the parent ID is already an override, and is valid (i.e. local override), we can
         * access its hierarchy root directly. */
        if (!ID_IS_LINKED(id_current_hierarchy_root) &&
            ID_IS_OVERRIDE_LIBRARY_REAL(id_current_hierarchy_root) &&
            id_current_hierarchy_root->override_library->reference->lib == id_root_reference->lib)
        {
          id_hierarchy_root_reference =
              id_current_hierarchy_root->override_library->hierarchy_root;
          BLI_assert(ID_IS_OVERRIDE_LIBRARY_REAL(id_hierarchy_root_reference));
          break;
        }

        if (ID_IS_LINKED(id_current_hierarchy_root)) {
          /* No local 'anchor' was found for the hierarchy to override, do not proceed, as this
           * would most likely generate invisible/confusing/hard to use and manage overrides. */
          BKE_main_id_tag_all(bmain, ID_TAG_DOIT, false);
          BKE_reportf(reports,
                      RPT_WARNING,
                      "Invalid anchor ('%s') found, needed to create library override from "
                      "data-block '%s'",
                      id_current_hierarchy_root->name,
                      id_root_reference->name);
          return;
        }

        /* In all other cases, `id_current_hierarchy_root` cannot be a valid hierarchy root, so
         * current `id_hierarchy_root_reference` is our best candidate. */

        break;
      }

      /* If some element in the tree needs to be overridden, but its ID is not overridable,
       * abort. */
      if (!ID_IS_OVERRIDABLE_LIBRARY_HIERARCHY(id_current_hierarchy_root)) {
        BKE_main_id_tag_all(bmain, ID_TAG_DOIT, false);
        BKE_reportf(reports,
                    RPT_WARNING,
                    "Could not create library override from data-block '%s', one of its parents "
                    "is not overridable ('%s')",
                    id_root_reference->name,
                    id_current_hierarchy_root->name);
        return;
      }
      id_current_hierarchy_root->tag |= ID_TAG_DOIT;
      id_hierarchy_root_reference = id_current_hierarchy_root;
    }

    /* That case can happen when linked data is a complex mix involving several libraries and/or
     * linked overrides. E.g. a mix of overrides from one library, and indirectly linked data
     * from another library. Do not try to support such cases for now. */
    if (!((id_hierarchy_root_reference->lib == id_root_reference->lib) ||
          (!ID_IS_LINKED(id_hierarchy_root_reference) &&
           ID_IS_OVERRIDE_LIBRARY_REAL(id_hierarchy_root_reference) &&
           id_hierarchy_root_reference->override_library->reference->lib ==
               id_root_reference->lib)))
    {
      BKE_main_id_tag_all(bmain, ID_TAG_DOIT, false);
      BKE_reportf(reports,
                  RPT_WARNING,
                  "Invalid hierarchy root ('%s') found, needed to create library override from "
                  "data-block '%s'",
                  id_hierarchy_root_reference->name,
                  id_root_reference->name);
      return;
    }

    /* While ideally this should not be needed, in practice user almost _never_ wants to actually
     * create liboverrides for all data under a selected hierarchy node, and this has currently a
     * dreadful consequences over performances (since it would call
     * #BKE_lib_override_library_create over _all_ items in the hierarchy). So only the clearing of
     * the system override flag is supported for non-selected items for now.
     */
    const bool is_selected = tselem->flag & TSE_SELECTED;
    if (!is_selected && data->id_hierarchy_roots.contains(id_hierarchy_root_reference)) {
      return;
    }

    data->id_root_add(id_hierarchy_root_reference,
                      id_root_reference,
                      id_instance_hint,
                      is_override_instancing_object);
  }
  else if (ID_IS_OVERRIDABLE_LIBRARY(id_root_reference)) {
    data->id_root_add(
        id_root_reference, id_root_reference, id_instance_hint, is_override_instancing_object);
  }
}

static void id_override_library_create_hierarchy(
    Main &bmain,
    Scene *scene,
    ViewLayer *view_layer,
    OutlinerLibOverrideData &data,
    ID *id_hierarchy_root_reference,
    Vector<OutlinerLiboverrideDataIDRoot> &data_idroots,
    bool &r_aggregated_success)
{
  BLI_assert(ID_IS_LINKED(id_hierarchy_root_reference) ||
             ID_IS_OVERRIDE_LIBRARY_REAL(id_hierarchy_root_reference));

  const bool do_hierarchy = data.do_hierarchy;

  /* NOTE: This process is not the most efficient, but allows to re-use existing code.
   * If this becomes a bottle-neck at some point, we need to implement a new
   * `BKE_lib_override_library_hierarchy_create()` function able to process several roots inside of
   * a same hierarchy in a single call. */
  for (OutlinerLiboverrideDataIDRoot &data_idroot : data_idroots) {
    /* For now, remap all local usages of linked ID to local override one here. */
    ID *id_iter;
    FOREACH_MAIN_ID_BEGIN (&bmain, id_iter) {
      if (ID_IS_LINKED(id_iter) || ID_IS_OVERRIDE_LIBRARY(id_iter)) {
        id_iter->tag &= ~ID_TAG_DOIT;
      }
      else {
        id_iter->tag |= ID_TAG_DOIT;
      }
    }
    FOREACH_MAIN_ID_END;

    bool success = false;
    if (do_hierarchy) {
      ID *id_root_override = nullptr;
      success = BKE_lib_override_library_create(&bmain,
                                                scene,
                                                view_layer,
                                                nullptr,
                                                data_idroot.id_root_reference,
                                                id_hierarchy_root_reference,
                                                data_idroot.id_instance_hint,
                                                &id_root_override,
                                                data.do_fully_editable);

      if (success) {
        BLI_assert(id_root_override != nullptr);
        BLI_assert(!ID_IS_LINKED(id_root_override));
        BLI_assert(ID_IS_OVERRIDE_LIBRARY_REAL(id_root_override));

        ID *id_hierarchy_root_override = id_root_override->override_library->hierarchy_root;
        BLI_assert(ID_IS_OVERRIDE_LIBRARY_REAL(id_hierarchy_root_override));
        if (ID_IS_LINKED(id_hierarchy_root_reference)) {
          BLI_assert(id_hierarchy_root_override->override_library->reference ==
                     id_hierarchy_root_reference);
          /* If the hierarchy root reference was a linked data, after the first iteration there is
           * now a matching override, which shall be used for all further partial overrides with
           * this same hierarchy. */
          id_hierarchy_root_reference = id_hierarchy_root_override;
        }
        else {
          BLI_assert(id_hierarchy_root_override == id_hierarchy_root_reference);
        }
        data_idroot.id_hierarchy_root_override = id_hierarchy_root_override;
        data.id_hierarchy_roots_uid.add(id_hierarchy_root_override->session_uid);
      }
    }
    else if (ID_IS_OVERRIDABLE_LIBRARY(data_idroot.id_root_reference)) {
      ID *id_root_override = BKE_lib_override_library_create_from_id(
          &bmain, data_idroot.id_root_reference, true);

      success = id_root_override != nullptr;
      if (success) {
        BLI_assert(ID_IS_OVERRIDE_LIBRARY_REAL(id_root_override));
        id_root_override->override_library->flag &= ~LIBOVERRIDE_FLAG_SYSTEM_DEFINED;
      }
      /* Cleanup. */
      BKE_main_id_newptr_and_tag_clear(&bmain);
      BKE_main_id_tag_all(&bmain, ID_TAG_DOIT, false);
    }
    else {
      BLI_assert_unreachable();
    }

    /* Remove the instance empty from this scene, the items now have an overridden collection
     * instead. */
    if (success && data_idroot.is_override_instancing_object) {
      BLI_assert(GS(data_idroot.id_instance_hint->name) == ID_OB);
      object::base_free_and_unlink(
          &bmain, scene, reinterpret_cast<Object *>(data_idroot.id_instance_hint));
    }

    r_aggregated_success = r_aggregated_success && success;
  }
}

/* Clear system override flag from newly created overrides which linked reference were previously
 * selected in the Outliner tree. */
static void id_override_library_create_hierarchy_process(bContext *C,
                                                         ReportList *reports,
                                                         OutlinerLibOverrideData &data)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const bool do_hierarchy = data.do_hierarchy;

  bool success = true;
  for (auto &&[id_hierarchy_root_reference, data_idroots] : data.id_hierarchy_roots.items()) {
    id_override_library_create_hierarchy(
        *bmain, scene, view_layer, data, id_hierarchy_root_reference, data_idroots, success);
  }

  if (!success) {
    BKE_reportf(reports,
                RPT_WARNING,
                "Could not create library override from one or more of the selected data-blocks");
  }

  if (!do_hierarchy) {
    return;
  }

  ID *id_iter;
  FOREACH_MAIN_ID_BEGIN (bmain, id_iter) {
    if (ID_IS_LINKED(id_iter) || !ID_IS_OVERRIDE_LIBRARY_REAL(id_iter)) {
      continue;
    }
    if (id_iter->override_library->hierarchy_root != nullptr &&
        !data.id_hierarchy_roots_uid.contains(
            id_iter->override_library->hierarchy_root->session_uid))
    {
      continue;
    }
    if (data.selected_id_uid.contains(id_iter->override_library->reference->session_uid) ||
        data.selected_id_uid.contains(id_iter->session_uid))
    {
      id_iter->override_library->flag &= ~LIBOVERRIDE_FLAG_SYSTEM_DEFINED;
    }
  }
  FOREACH_MAIN_ID_END;
}

static void id_override_library_reset(bContext *C,
                                      OutlinerLibOverrideData *data,
                                      TreeStoreElem *tselem)
{
  BLI_assert(TSE_IS_REAL_ID(tselem));
  ID *id_root = tselem->id;
  const bool do_hierarchy = data->do_hierarchy;

  if (!ID_IS_OVERRIDE_LIBRARY_REAL(id_root) || ID_IS_LINKED(id_root)) {
    CLOG_WARN(&LOG, "Could not reset library override of data block '%s'", id_root->name);
    return;
  }

  Main *bmain = CTX_data_main(C);

  if (do_hierarchy) {
    BKE_lib_override_library_id_hierarchy_reset(bmain, id_root, false);
  }
  else {
    BKE_lib_override_library_id_reset(bmain, id_root, false);
  }
}

static void id_override_library_clear_single_process(bContext *C,
                                                     ReportList * /*reports*/,
                                                     OutlinerLibOverrideData &data)
{
  Main *bmain = CTX_data_main(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Scene *scene = CTX_data_scene(C);

  /* TODO: At some point this likely needs to be re-written as a BKE function instead, with better
   * handling of hierarchies among other things. */

  /* Try to process all potential leaves first (deleting some liboverride leaves will turn other
   * liboverrides into leaves as well). */
  bool do_process_leaves = true;
  while (!data.id_hierarchy_roots.is_empty()) {
    bool has_found_leaves = false;

    for (auto &&id : data.id_hierarchy_roots.keys()) {
      if (do_process_leaves) {
        if (BKE_lib_override_library_is_hierarchy_leaf(bmain, id)) {
          /* If given ID is not using any other override (it's a 'leaf' in the override hierarchy),
           * delete it and remap its usages to its linked reference. Otherwise, keep it as a reset
           * system override. */
          bool do_remap_active = false;
          BKE_view_layer_synced_ensure(scene, view_layer);
          if (BKE_view_layer_active_object_get(view_layer) == reinterpret_cast<Object *>(id)) {
            BLI_assert(GS(id->name) == ID_OB);
            do_remap_active = true;
          }
          BKE_libblock_remap(
              bmain, id, id->override_library->reference, ID_REMAP_SKIP_INDIRECT_USAGE);
          if (do_remap_active) {
            BKE_view_layer_synced_ensure(scene, view_layer);
            Object *ref_object = reinterpret_cast<Object *>(id->override_library->reference);
            Base *basact = BKE_view_layer_base_find(view_layer, ref_object);
            if (basact != nullptr) {
              view_layer->basact = basact;
            }
            DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
          }

          BKE_id_delete(bmain, id);
          data.id_hierarchy_roots.remove(id);
          has_found_leaves = true;
        }
      }
      else {
        BLI_assert(!BKE_lib_override_library_is_hierarchy_leaf(bmain, id));
        BKE_lib_override_library_id_reset(bmain, id, true);
        data.id_hierarchy_roots.remove(id);
      }
    }

    do_process_leaves = has_found_leaves;
  }
  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS | ID_RECALC_SYNC_TO_EVAL);
}

static void id_override_library_clear_single(OutlinerLibOverrideData *data,
                                             ReportList *reports,
                                             TreeStoreElem *tselem)
{
  BLI_assert(TSE_IS_REAL_ID(tselem));
  ID *id = tselem->id;

  if (!ID_IS_OVERRIDE_LIBRARY(id)) {
    return;
  }
  if (!ID_IS_OVERRIDE_LIBRARY_REAL(id)) {
    BKE_reportf(reports,
                RPT_WARNING,
                "Cannot clear embedded library override '%s', only overrides of real data-blocks "
                "can be directly cleared",
                id->name);
    return;
  }
  if (ID_IS_LINKED(id)) {
    BKE_reportf(
        reports,
        RPT_WARNING,
        "Cannot clear linked library override '%s', only local overrides can be directly cleared",
        id->name);
    return;
  }

  data->id_root_set(id);
}

static void id_override_library_resync(OutlinerLibOverrideData *data, TreeStoreElem *tselem)
{
  BLI_assert(TSE_IS_REAL_ID(tselem));
  ID *id_root = tselem->id;

  if (!ID_IS_OVERRIDE_LIBRARY_REAL(id_root) || ID_IS_LINKED(id_root)) {
    CLOG_WARN(&LOG, "Could not resync library override of data block '%s'", id_root->name);
    return;
  }

  if (id_root->override_library->hierarchy_root != nullptr) {
    id_root = id_root->override_library->hierarchy_root;
  }

  data->id_root_set(id_root);
}

/* Resync a hierarchy of library overrides. */
static void id_override_library_resync_hierarchy_process(bContext *C,
                                                         ReportList *reports,
                                                         OutlinerLibOverrideData &data)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  const bool do_hierarchy_enforce = data.do_resync_hierarchy_enforce;

  BlendFileReadReport report{};
  report.reports = reports;

  for (auto &&id_hierarchy_root : data.id_hierarchy_roots.keys()) {
    BKE_lib_override_library_resync(bmain,
                                    scene,
                                    CTX_data_view_layer(C),
                                    id_hierarchy_root,
                                    nullptr,
                                    do_hierarchy_enforce,
                                    &report);
  }

  WM_event_add_notifier(C, NC_WINDOW, nullptr);
}

static void id_override_library_delete_hierarchy(OutlinerLibOverrideData *data,
                                                 TreeStoreElem *tselem)
{
  BLI_assert(TSE_IS_REAL_ID(tselem));
  ID *id_root = tselem->id;

  if (!ID_IS_OVERRIDE_LIBRARY_REAL(id_root) || ID_IS_LINKED(id_root)) {
    CLOG_WARN(&LOG, "Could not delete library override of data block '%s'", id_root->name);
    return;
  }

  if (id_root->override_library->hierarchy_root != nullptr) {
    id_root = id_root->override_library->hierarchy_root;
  }

  data->id_root_set(id_root);
}

/* Clear (delete) a hierarchy of library overrides. */
static void id_override_library_delete_hierarchy_process(bContext *C,
                                                         ReportList * /*reports*/,
                                                         OutlinerLibOverrideData &data)
{
  Main *bmain = CTX_data_main(C);

  for (auto &&id_hierarchy_root : data.id_hierarchy_roots.keys()) {
    BKE_lib_override_library_delete(bmain, id_hierarchy_root);
  }
}

static void id_fake_user_set_fn(bContext * /*C*/,
                                ReportList * /*reports*/,
                                Scene * /*scene*/,
                                TreeElement * /*te*/,
                                TreeStoreElem * /*tsep*/,
                                TreeStoreElem *tselem)
{
  ID *id = tselem->id;

  id_fake_user_set(id);
}

static void id_fake_user_clear_fn(bContext * /*C*/,
                                  ReportList * /*reports*/,
                                  Scene * /*scene*/,
                                  TreeElement * /*te*/,
                                  TreeStoreElem * /*tsep*/,
                                  TreeStoreElem *tselem)
{
  ID *id = tselem->id;

  id_fake_user_clear(id);
}

static void id_select_linked_fn(bContext *C,
                                ReportList * /*reports*/,
                                Scene * /*scene*/,
                                TreeElement * /*te*/,
                                TreeStoreElem * /*tsep*/,
                                TreeStoreElem *tselem)
{
  ID *id = tselem->id;

  object::select_linked_by_id(C, id);
}

static void singleuser_action_fn(bContext *C,
                                 ReportList * /*reports*/,
                                 Scene * /*scene*/,
                                 TreeElement *te,
                                 TreeStoreElem *tsep,
                                 TreeStoreElem *tselem)
{
  /* This callback runs for all selected elements, some of which may not be actions which results
   * in a crash. */
  if (te->idcode != ID_AC) {
    return;
  }

  ID *id = tselem->id;

  if (id) {
    IdAdtTemplate *iat = (IdAdtTemplate *)tsep->id;
    PropertyRNA *prop;

    PointerRNA ptr = RNA_pointer_create_discrete(&iat->id, &RNA_AnimData, iat->adt);
    prop = RNA_struct_find_property(&ptr, "action");

    id_single_user(C, id, &ptr, prop);
  }
}

static void singleuser_world_fn(bContext *C,
                                ReportList * /*reports*/,
                                Scene * /*scene*/,
                                TreeElement * /*te*/,
                                TreeStoreElem *tsep,
                                TreeStoreElem *tselem)
{
  ID *id = tselem->id;

  /* need to use parent scene not just scene, otherwise may end up getting wrong one */
  if (id) {
    Scene *parscene = (Scene *)tsep->id;
    PropertyRNA *prop;

    PointerRNA ptr = RNA_id_pointer_create(&parscene->id);
    prop = RNA_struct_find_property(&ptr, "world");

    id_single_user(C, id, &ptr, prop);
  }
}

void outliner_do_object_operation_ex(bContext *C,
                                     ReportList *reports,
                                     Scene *scene_act,
                                     SpaceOutliner *space_outliner,
                                     ListBase *lb,
                                     outliner_operation_fn operation_fn,
                                     bool recurse_selected)
{
  LISTBASE_FOREACH (TreeElement *, te, lb) {
    TreeStoreElem *tselem = TREESTORE(te);
    bool select_handled = false;
    if (tselem->flag & TSE_SELECTED) {
      if ((tselem->type == TSE_SOME_ID) && (te->idcode == ID_OB)) {
        /* When objects selected in other scenes, don't know if that should be allowed. */
        Scene *scene_owner = (Scene *)outliner_search_back(te, ID_SCE);
        if (scene_owner && scene_act != scene_owner) {
          WM_window_set_active_scene(CTX_data_main(C), C, CTX_wm_window(C), scene_owner);
        }
        /* Important to use 'scene_owner' not scene_act else deleting objects can crash.
         * only use 'scene_act' when 'scene_owner' is nullptr, which can happen when the
         * outliner isn't showing scenes: Visible Layer draw mode for eg. */
        operation_fn(C, reports, scene_owner ? scene_owner : scene_act, te, nullptr, tselem);
        select_handled = true;
      }
    }
    if (TSELEM_OPEN(tselem, space_outliner)) {
      if ((select_handled == false) || recurse_selected) {
        outliner_do_object_operation_ex(
            C, reports, scene_act, space_outliner, &te->subtree, operation_fn, recurse_selected);
      }
    }
  }
}

void outliner_do_object_operation(bContext *C,
                                  ReportList *reports,
                                  Scene *scene_act,
                                  SpaceOutliner *space_outliner,
                                  ListBase *lb,
                                  outliner_operation_fn operation_fn)
{
  outliner_do_object_operation_ex(C, reports, scene_act, space_outliner, lb, operation_fn, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal Tagging Utilities
 * \{ */

static void clear_animdata_fn(int /*event*/,
                              TreeElement * /*te*/,
                              TreeStoreElem *tselem,
                              void * /*arg*/)
{
  BKE_animdata_free(tselem->id, true);
  DEG_id_tag_update(tselem->id, ID_RECALC_ANIMATION);
}

static void unlinkact_animdata_fn(int /*event*/,
                                  TreeElement * /*te*/,
                                  TreeStoreElem *tselem,
                                  void * /*arg*/)
{
  /* just set action to nullptr */
  BKE_animdata_set_action(nullptr, tselem->id, nullptr);
  DEG_id_tag_update(tselem->id, ID_RECALC_ANIMATION);
}

static void cleardrivers_animdata_fn(int /*event*/,
                                     TreeElement * /*te*/,
                                     TreeStoreElem *tselem,
                                     void * /*arg*/)
{
  IdAdtTemplate *iat = (IdAdtTemplate *)tselem->id;

  /* just free drivers - stored as a list of F-Curves */
  BKE_fcurves_free(&iat->adt->drivers);
  DEG_id_tag_update(tselem->id, ID_RECALC_ANIMATION);
}

static void refreshdrivers_animdata_fn(int /*event*/,
                                       TreeElement * /*te*/,
                                       TreeStoreElem *tselem,
                                       void * /*arg*/)
{
  IdAdtTemplate *iat = (IdAdtTemplate *)tselem->id;

  /* Loop over drivers, performing refresh
   * (i.e. check `graph_buttons.cc` and `rna_fcurve.cc` for details). */
  LISTBASE_FOREACH (FCurve *, fcu, &iat->adt->drivers) {
    fcu->flag &= ~FCURVE_DISABLED;

    if (fcu->driver) {
      fcu->driver->flag &= ~DRIVER_FLAG_INVALID;
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Library Overrides Operation Menu.
 * \{ */

enum eOutlinerLibOverrideOpTypes {
  OUTLINER_LIBOVERRIDE_OP_INVALID = 0,

  OUTLINER_LIBOVERRIDE_OP_CREATE_HIERARCHY,
  OUTLINER_LIBOVERRIDE_OP_RESET,
  OUTLINER_LIBOVERRIDE_OP_CLEAR_SINGLE,

  OUTLINER_LIBOVERRIDE_OP_RESYNC_HIERARCHY,
  OUTLINER_LIBOVERRIDE_OP_RESYNC_HIERARCHY_ENFORCE,
  OUTLINER_LIBOVERRIDE_OP_DELETE_HIERARCHY,
};

static const EnumPropertyItem prop_liboverride_op_types[] = {
    {OUTLINER_LIBOVERRIDE_OP_CREATE_HIERARCHY,
     "OVERRIDE_LIBRARY_CREATE_HIERARCHY",
     0,
     "Make",
     "Create a local override of the selected linked data-blocks, and their hierarchy of "
     "dependencies"},
    {OUTLINER_LIBOVERRIDE_OP_RESET,
     "OVERRIDE_LIBRARY_RESET",
     0,
     "Reset",
     "Reset the selected local overrides to their linked references values"},
    {OUTLINER_LIBOVERRIDE_OP_CLEAR_SINGLE,
     "OVERRIDE_LIBRARY_CLEAR_SINGLE",
     0,
     "Clear",
     "Delete the selected local overrides and relink their usages to the linked data-blocks if "
     "possible, else reset them and mark them as non editable"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem prop_liboverride_troubleshoot_op_types[] = {
    {OUTLINER_LIBOVERRIDE_OP_RESYNC_HIERARCHY,
     "OVERRIDE_LIBRARY_RESYNC_HIERARCHY",
     0,
     "Resync",
     "Rebuild the selected local overrides from their linked references, as well as their "
     "hierarchies of dependencies"},
    {OUTLINER_LIBOVERRIDE_OP_RESYNC_HIERARCHY_ENFORCE,
     "OVERRIDE_LIBRARY_RESYNC_HIERARCHY_ENFORCE",
     0,
     "Resync Enforce",
     "Rebuild the selected local overrides from their linked references, as well as their "
     "hierarchies of dependencies, enforcing these hierarchies to match the linked data (i.e. "
     "ignoring existing overrides on data-blocks pointer properties)"},
    RNA_ENUM_ITEM_SEPR,
    {OUTLINER_LIBOVERRIDE_OP_DELETE_HIERARCHY,
     "OVERRIDE_LIBRARY_DELETE_HIERARCHY",
     0,
     "Delete",
     "Delete the selected local overrides (including their hierarchies of override dependencies) "
     "and relink their usages to the linked data-blocks"},
    {0, nullptr, 0, nullptr, nullptr},
};

static bool outliner_liboverride_operation_poll(bContext *C)
{
  if (!outliner_operation_tree_element_poll(C)) {
    return false;
  }
  return true;
}

static wmOperatorStatus outliner_liboverride_operation_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);

  /* check for invalid states */
  if (space_outliner == nullptr) {
    return OPERATOR_CANCELLED;
  }

  const eOutlinerLibOpSelectionSet selection_set = static_cast<eOutlinerLibOpSelectionSet>(
      RNA_enum_get(op->ptr, "selection_set"));
  const eOutlinerLibOverrideOpTypes event = static_cast<eOutlinerLibOverrideOpTypes>(
      RNA_enum_get(op->ptr, "type"));
  switch (event) {
    case OUTLINER_LIBOVERRIDE_OP_CREATE_HIERARCHY: {
      OutlinerLibOverrideData override_data{};
      override_data.do_hierarchy = true;
      override_data.do_fully_editable = false;

      outliner_do_libdata_operation_selection_set(
          C,
          op->reports,
          scene,
          space_outliner,
          [&](bContext *C,
              ReportList *reports,
              Scene * /*scene*/,
              TreeElement *te,
              TreeStoreElem *tsep,
              TreeStoreElem *tselem) {
            id_override_library_create_hierarchy_pre_process(
                C, &override_data, reports, te, tsep, tselem);
          },
          selection_set,
          true);

      id_override_library_create_hierarchy_process(C, op->reports, override_data);

      ED_undo_push(C, "Overridden Data Hierarchy");
      break;
    }
    case OUTLINER_LIBOVERRIDE_OP_RESET: {
      OutlinerLibOverrideData override_data{};
      outliner_do_libdata_operation_selection_set(
          C,
          op->reports,
          scene,
          space_outliner,
          [&](bContext *C,
              ReportList * /*reports*/,
              Scene * /*scene*/,
              TreeElement * /*te*/,
              TreeStoreElem * /*tsep*/,
              TreeStoreElem *tselem) { id_override_library_reset(C, &override_data, tselem); },
          selection_set,
          false);
      ED_undo_push(C, "Reset Overridden Data");
      break;
    }
    case OUTLINER_LIBOVERRIDE_OP_CLEAR_SINGLE: {
      OutlinerLibOverrideData override_data{};
      override_data.do_hierarchy = false;
      override_data.do_fully_editable = false;

      outliner_do_libdata_operation_selection_set(
          C,
          op->reports,
          scene,
          space_outliner,
          [&](bContext * /*C*/,
              ReportList *reports,
              Scene * /*scene*/,
              TreeElement * /*te*/,
              TreeStoreElem * /*tsep*/,
              TreeStoreElem *tselem) {
            id_override_library_clear_single(&override_data, reports, tselem);
          },
          selection_set,
          false);

      id_override_library_clear_single_process(C, op->reports, override_data);

      ED_undo_push(C, "Clear Overridden Data");
      break;
    }

    case OUTLINER_LIBOVERRIDE_OP_RESYNC_HIERARCHY: {
      OutlinerLibOverrideData override_data{};
      override_data.do_hierarchy = true;
      outliner_do_libdata_operation_selection_set(
          C,
          op->reports,
          scene,
          space_outliner,
          [&](bContext * /*C*/,
              ReportList * /*reports*/,
              Scene * /*scene*/,
              TreeElement * /*te*/,
              TreeStoreElem * /*tsep*/,
              TreeStoreElem *tselem) { id_override_library_resync(&override_data, tselem); },
          OUTLINER_LIB_SELECTIONSET_SELECTED,
          false);

      id_override_library_resync_hierarchy_process(C, op->reports, override_data);

      ED_undo_push(C, "Resync Overridden Data Hierarchy");
      break;
    }
    case OUTLINER_LIBOVERRIDE_OP_RESYNC_HIERARCHY_ENFORCE: {
      OutlinerLibOverrideData override_data{};
      override_data.do_hierarchy = true;
      override_data.do_resync_hierarchy_enforce = true;
      outliner_do_libdata_operation_selection_set(
          C,
          op->reports,
          scene,
          space_outliner,
          [&](bContext * /*C*/,
              ReportList * /*reports*/,
              Scene * /*scene*/,
              TreeElement * /*te*/,
              TreeStoreElem * /*tsep*/,
              TreeStoreElem *tselem) { id_override_library_resync(&override_data, tselem); },
          OUTLINER_LIB_SELECTIONSET_SELECTED,
          false);

      id_override_library_resync_hierarchy_process(C, op->reports, override_data);

      ED_undo_push(C, "Resync Overridden Data Hierarchy Enforce");
      break;
    }
    case OUTLINER_LIBOVERRIDE_OP_DELETE_HIERARCHY: {
      OutlinerLibOverrideData override_data{};
      override_data.do_hierarchy = true;
      outliner_do_libdata_operation_selection_set(
          C,
          op->reports,
          scene,
          space_outliner,
          [&](bContext * /*C*/,
              ReportList * /*reports*/,
              Scene * /*scene*/,
              TreeElement * /*te*/,
              TreeStoreElem * /*tsep*/,
              TreeStoreElem *tselem) {
            id_override_library_delete_hierarchy(&override_data, tselem);
          },
          OUTLINER_LIB_SELECTIONSET_SELECTED,
          false);

      id_override_library_delete_hierarchy_process(C, op->reports, override_data);

      ED_undo_push(C, "Delete Overridden Data Hierarchy");
      break;
    }
    default:
      /* Invalid - unhandled. */
      break;
  }

  WM_event_add_notifier(C, NC_WINDOW, nullptr);
  WM_event_add_notifier(C, NC_WM | ND_LIB_OVERRIDE_CHANGED, nullptr);
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_liboverride_operation(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Outliner Library Override Operation";
  ot->idname = "OUTLINER_OT_liboverride_operation";
  ot->description = "Create, reset or clear library override hierarchies";

  /* callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = outliner_liboverride_operation_exec;
  ot->poll = outliner_liboverride_operation_poll;

  ot->flag = 0;

  RNA_def_enum(ot->srna, "type", prop_liboverride_op_types, 0, "Library Override Operation", "");
  ot->prop = RNA_def_enum(ot->srna,
                          "selection_set",
                          prop_lib_op_selection_set,
                          0,
                          "Selection Set",
                          "Over which part of the tree items to apply the operation");
}

void OUTLINER_OT_liboverride_troubleshoot_operation(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Outliner Library Override Troubleshoot Operation";
  ot->idname = "OUTLINER_OT_liboverride_troubleshoot_operation";
  ot->description = "Advanced operations over library override to help fix broken hierarchies";

  /* callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = outliner_liboverride_operation_exec;
  ot->poll = outliner_liboverride_operation_poll;

  ot->flag = 0;

  ot->prop = RNA_def_enum(ot->srna,
                          "type",
                          prop_liboverride_troubleshoot_op_types,
                          0,
                          "Library Override Troubleshoot Operation",
                          "");
  RNA_def_enum(ot->srna,
               "selection_set",
               prop_lib_op_selection_set,
               0,
               "Selection Set",
               "Over which part of the tree items to apply the operation");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Object Operation Utilities
 * \{ */

enum eOutliner_PropDataOps {
  OL_DOP_SELECT = 1,
  OL_DOP_DESELECT,
  OL_DOP_HIDE,
  OL_DOP_UNHIDE,
  OL_DOP_SELECT_LINKED,
};

enum eOutliner_PropConstraintOps {
  OL_CONSTRAINTOP_ENABLE = 1,
  OL_CONSTRAINTOP_DISABLE,
  OL_CONSTRAINTOP_DELETE,
};

enum eOutliner_PropModifierOps {
  OL_MODIFIER_OP_TOGVIS = 1,
  OL_MODIFIER_OP_TOGREN,
  OL_MODIFIER_OP_DELETE,
  OL_MODIFIER_OP_APPLY,
};

static void pchan_fn(int event, TreeElement *te, TreeStoreElem * /*tselem*/, void * /*arg*/)
{
  bPoseChannel *pchan = (bPoseChannel *)te->directdata;

  if (event == OL_DOP_SELECT) {
    pchan->flag |= POSE_SELECTED;
  }
  else if (event == OL_DOP_DESELECT) {
    pchan->flag &= ~POSE_SELECTED;
  }
  else if (event == OL_DOP_HIDE) {
    pchan->drawflag |= PCHAN_DRAW_HIDDEN;
    pchan->flag &= ~POSE_SELECTED;
  }
  else if (event == OL_DOP_UNHIDE) {
    pchan->drawflag &= ~PCHAN_DRAW_HIDDEN;
  }
}

static void bone_fn(int event, TreeElement *te, TreeStoreElem * /*tselem*/, void * /*arg*/)
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

static void ebone_fn(int event, TreeElement *te, TreeStoreElem * /*tselem*/, void * /*arg*/)
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

static void sequence_fn(int event, TreeElement *te, TreeStoreElem * /*tselem*/, void *scene_ptr)
{
  TreeElementStrip *te_strip = tree_element_cast<TreeElementStrip>(te);
  Strip *strip = &te_strip->get_strip();
  Scene *scene = (Scene *)scene_ptr;
  Editing *ed = seq::editing_get(scene);
  if (BLI_findindex(ed->current_strips(), strip) != -1) {
    if (event == OL_DOP_SELECT) {
      vse::select_strip_single(scene, strip, true);
    }
    else if (event == OL_DOP_DESELECT) {
      strip->flag &= ~SELECT;
    }
    else if (event == OL_DOP_HIDE) {
      if (!(strip->flag & SEQ_MUTE)) {
        strip->flag |= SEQ_MUTE;
        seq::relations_invalidate_cache(scene, strip);
      }
    }
    else if (event == OL_DOP_UNHIDE) {
      if (strip->flag & SEQ_MUTE) {
        strip->flag &= ~SEQ_MUTE;
        seq::relations_invalidate_cache(scene, strip);
      }
    }
  }
}

static void gpencil_layer_fn(int event,
                             TreeElement *te,
                             TreeStoreElem * /*tselem*/,
                             void * /*arg*/)
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

static void grease_pencil_node_fn(int event,
                                  TreeElement *te,
                                  TreeStoreElem * /*tselem*/,
                                  void * /*arg*/)
{
  bke::greasepencil::TreeNode &node = tree_element_cast<TreeElementGreasePencilNode>(te)->node();

  if (event == OL_DOP_SELECT) {
    node.set_selected(true);
  }
  else if (event == OL_DOP_DESELECT) {
    node.set_selected(false);
  }
  else if (event == OL_DOP_HIDE) {
    node.set_visible(false);
  }
  else if (event == OL_DOP_UNHIDE) {
    node.set_visible(true);
  }
}

static void data_select_linked_fn(int event,
                                  TreeElement *te,
                                  TreeStoreElem * /*tselem*/,
                                  void *C_v)
{
  const TreeElementRNAStruct *te_rna_struct = tree_element_cast<TreeElementRNAStruct>(te);
  if (!te_rna_struct) {
    return;
  }

  if (event == OL_DOP_SELECT_LINKED) {
    const PointerRNA &ptr = te_rna_struct->get_pointer_rna();
    if (RNA_struct_is_ID(ptr.type)) {
      bContext *C = (bContext *)C_v;
      ID *id = static_cast<ID *>(ptr.data);

      object::select_linked_by_id(C, id);
    }
  }
}

static void constraint_fn(int event, TreeElement *te, TreeStoreElem * /*tselem*/, void *C_v)
{
  bContext *C = static_cast<bContext *>(C_v);
  Main *bmain = CTX_data_main(C);
  bConstraint *constraint = (bConstraint *)te->directdata;
  Object *ob = (Object *)outliner_search_back(te, ID_OB);

  if (event == OL_CONSTRAINTOP_ENABLE) {
    constraint->flag &= ~CONSTRAINT_OFF;
    object::constraint_update(bmain, ob);
    WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT, ob);
  }
  else if (event == OL_CONSTRAINTOP_DISABLE) {
    constraint->flag |= CONSTRAINT_OFF;
    object::constraint_update(bmain, ob);
    WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT, ob);
  }
  else if (event == OL_CONSTRAINTOP_DELETE) {
    ListBase *lb = nullptr;

    if (TREESTORE(te->parent->parent)->type == TSE_POSE_CHANNEL) {
      lb = &((bPoseChannel *)te->parent->parent->directdata)->constraints;
    }
    else {
      lb = &ob->constraints;
    }

    if (BKE_constraint_remove_ex(lb, ob, constraint)) {
      /* there's no active constraint now, so make sure this is the case */
      BKE_constraints_active_set(&ob->constraints, nullptr);

      /* Needed to set the flags on pose-bones correctly. */
      object::constraint_update(bmain, ob);

      WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT | NA_REMOVED, ob);
      te->store_elem->flag &= ~TSE_SELECTED;
    }
  }
}

struct ModifierFnArgs {
  bContext *C;
  ReportList *reports;
};

static void modifier_fn(int event, TreeElement *te, TreeStoreElem * /*tselem*/, void *arg)
{
  ModifierFnArgs *data = static_cast<ModifierFnArgs *>(arg);
  bContext *C = data->C;
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
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
    object::modifier_remove(data->reports, bmain, scene, ob, md);
    WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER | NA_REMOVED, ob);
    te->store_elem->flag &= ~TSE_SELECTED;
  }
  else if (event == OL_MODIFIER_OP_APPLY) {
    object::modifier_apply(
        bmain, data->reports, depsgraph, scene, ob, md, object::MODIFIER_APPLY_DATA, false, false);
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    DEG_relations_tag_update(bmain);
    WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);
    te->store_elem->flag &= ~TSE_SELECTED;
  }
}

static void outliner_do_data_operation(
    SpaceOutliner *space_outliner,
    int type,
    int event,
    void (*operation_fn)(int, TreeElement *, TreeStoreElem *, void *),
    void *arg)
{
  tree_iterator::all_open(*space_outliner, [&](TreeElement *te) {
    TreeStoreElem *tselem = TREESTORE(te);
    if (tselem->flag & TSE_SELECTED) {
      if (tselem->type == type) {
        operation_fn(event, te, tselem, arg);
      }
    }
  });
}

static void outliner_batch_delete_object_tag(ReportList *reports,
                                             Main *bmain,
                                             Scene *scene,
                                             Object *object)
{
  if (object->id.tag & ID_TAG_INDIRECT) {
    BKE_reportf(
        reports, RPT_WARNING, "Cannot delete indirectly linked object '%s'", object->id.name + 2);
    BLI_assert((object->id.tag & ID_TAG_DOIT) == 0);
  }
  /* FIXME: This code checking object user-count won't work as expected if a same object belongs to
   * more than one collection in the scene. */
  if (ID_REAL_USERS(object) <= 1 && ID_EXTRA_USERS(object) == 0 &&
      BKE_library_ID_is_indirectly_used(bmain, object))
  {
    BKE_reportf(reports,
                RPT_WARNING,
                "Cannot delete object '%s' from scene '%s', indirectly used objects need at least "
                "one user",
                object->id.name + 2,
                scene->id.name + 2);
    BLI_assert((object->id.tag & ID_TAG_DOIT) == 0);
  }

  object->id.tag |= ID_TAG_DOIT;
}

static void outliner_batch_delete_object_hierarchy_tag(
    ReportList *reports, Main *bmain, ViewLayer *view_layer, Scene *scene, Base *base)
{
  Object *object = base->object;
  BLI_assert(object != nullptr);

  outliner_batch_delete_object_tag(reports, bmain, scene, object);

  /* Even though the object itself may not be deletable, some of its children may still be
   * deletable. */
  for (Base *base_iter = static_cast<Base *>(BKE_view_layer_object_bases_get(view_layer)->first);
       base_iter != nullptr;
       base_iter = base_iter->next)
  {
    Object *parent_ob_iter;
    for (parent_ob_iter = base_iter->object->parent;
         (parent_ob_iter != nullptr && parent_ob_iter != object &&
          (parent_ob_iter->id.tag & ID_TAG_DOIT) == 0);
         parent_ob_iter = parent_ob_iter->parent)
    {
      /* pass */
    }
    if (parent_ob_iter != nullptr) {
      /* There is one or more parents to current iterated object that also need to be deleted,
       * process the parenting chain again to tag them as such.
       *
       * NOTE: Since objects that cannot be deleted are not tagged, the relevant 'parenting'
       * branches may be looped over more than once. Would not expect this to be a real issue in
       * practice though. */
      for (parent_ob_iter = base_iter->object;
           (parent_ob_iter != nullptr && parent_ob_iter != object &&
            (parent_ob_iter->id.tag & ID_TAG_DOIT) == 0);
           parent_ob_iter = parent_ob_iter->parent)
      {
        outliner_batch_delete_object_tag(reports, bmain, scene, parent_ob_iter);
      }
    }
  }
}

static void object_batch_delete_hierarchy_tag_fn(bContext *C,
                                                 ReportList *reports,
                                                 Scene *scene,
                                                 Object *ob)
{
  if (ob->id.tag & ID_TAG_DOIT) {
    /* Object has already been processed and tagged for removal as part of another parenting
     * hierarchy. */
#ifndef NDEBUG
    ViewLayer *view_layer = CTX_data_view_layer(C);
    BKE_view_layer_synced_ensure(scene, view_layer);
    BLI_assert(BKE_view_layer_base_find(view_layer, ob) == nullptr);
#endif
    return;
  }

  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *obedit = CTX_data_edit_object(C);

  Base *base = BKE_view_layer_base_find(view_layer, ob);

  if (base == nullptr) {
    return;
  }

  /* Exit Edit mode if the active object or one of its children are being edited. */
  for (; obedit && (obedit != base->object); obedit = obedit->parent) {
    /* pass */
  }
  if (obedit == base->object) {
    object::editmode_exit(C, object::EM_FREEDATA);
  }

  Main *bmain = CTX_data_main(C);
  outliner_batch_delete_object_hierarchy_tag(reports, bmain, view_layer, scene, base);
}

static void outliner_batch_delete_object_hierarchy(Main *bmain, Scene *scene)
{
  LISTBASE_FOREACH (Object *, ob_iter, &bmain->objects) {
    if ((ob_iter->id.tag & ID_TAG_DOIT) == 0) {
      continue;
    }

    BKE_scene_collections_object_remove(bmain, scene, ob_iter, false);

    /* Check on all objects tagged for deletion, these that are still in use (e.g. in collections
     * from another scene) should not be deleted. They also need to be tagged for depsgraph update.
     */
    if (ob_iter->id.us != 0) {
      ob_iter->id.tag &= ~ID_TAG_DOIT;
      DEG_id_tag_update_ex(bmain, &ob_iter->id, ID_RECALC_BASE_FLAGS);
    }
  }

  BKE_id_multi_tagged_delete(bmain);
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
  OL_OP_RENAME,
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
    {0, nullptr, 0, nullptr, nullptr},
};

static wmOperatorStatus outliner_object_operation_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  wmWindow *win = CTX_wm_window(C);
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  int event;
  const char *str = nullptr;
  bool selection_changed = false;

  /* check for invalid states */
  if (space_outliner == nullptr) {
    return OPERATOR_CANCELLED;
  }

  event = RNA_enum_get(op->ptr, "type");

  switch (event) {
    case OL_OP_SELECT: {
      Scene *sce = scene; /* To be able to delete, scenes are set... */
      outliner_do_object_operation(
          C, op->reports, scene, space_outliner, &space_outliner->tree, object_select_fn);
      /* FIXME: This is most certainly broken, maybe check should rather be
       * `if (CTX_data_scene(C) != scene)` ? */
      if (scene != sce) {
        WM_window_set_active_scene(bmain, C, win, sce);
      }

      str = CTX_N_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Select Objects");
      selection_changed = true;
      break;
    }
    case OL_OP_SELECT_HIERARCHY: {
      Scene *sce = scene; /* To be able to delete, scenes are set... */
      outliner_do_object_operation_ex(C,
                                      op->reports,
                                      scene,
                                      space_outliner,
                                      &space_outliner->tree,
                                      object_select_hierarchy_fn,
                                      false);
      /* FIXME: This is most certainly broken, maybe check should rather be
       * `if (CTX_data_scene(C) != scene)` ? */
      if (scene != sce) {
        WM_window_set_active_scene(bmain, C, win, sce);
      }
      str = CTX_N_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Select Object Hierarchy");
      selection_changed = true;
      break;
    }
    case OL_OP_DESELECT:
      outliner_do_object_operation(
          C, op->reports, scene, space_outliner, &space_outliner->tree, object_deselect_fn);
      str = CTX_N_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Deselect Objects");
      selection_changed = true;
      break;
    case OL_OP_REMAP:
      outliner_do_libdata_operation(C, op->reports, scene, space_outliner, id_remap_fn);
      /* No undo push here, operator does it itself (since it's a modal one, the op_undo_depth
       * trick does not work here). */
      break;
    case OL_OP_RENAME:
      outliner_do_object_operation(
          C, op->reports, scene, space_outliner, &space_outliner->tree, item_rename_fn);
      str = CTX_N_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Rename Object");
      break;
    default:
      BLI_assert_unreachable();
      return OPERATOR_CANCELLED;
  }

  if (selection_changed) {
    DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
    ED_outliner_select_sync_from_object_tag(C);
  }

  if (str != nullptr) {
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

using OutlinerDeleteFn = void (*)(bContext *C, ReportList *reports, Scene *scene, Object *ob);

struct ObjectEditData {
  Set<Object *> objects_set;
  bool is_liboverride_allowed;
  bool is_liboverride_hierarchy_root_allowed;
};

static void outliner_do_object_delete(bContext *C,
                                      ReportList *reports,
                                      Scene *scene,
                                      const Set<Object *> &objects_to_delete,
                                      OutlinerDeleteFn delete_fn)
{
  for (Object *ob : objects_to_delete) {
    delete_fn(C, reports, scene, ob);
  }
}

static TreeTraversalAction outliner_collect_objects_to_delete(TreeElement *te, void *customdata)
{
  ObjectEditData *data = static_cast<ObjectEditData *>(customdata);
  TreeStoreElem *tselem = TREESTORE(te);

  if (outliner_is_collection_tree_element(te)) {
    return TRAVERSE_CONTINUE;
  }

  if ((tselem->type != TSE_SOME_ID) || (tselem->id == nullptr) || (GS(tselem->id->name) != ID_OB))
  {
    return TRAVERSE_SKIP_CHILDS;
  }

  /* Do not allow to delete children objects of an override collection. */
  TreeElement *te_parent = te->parent;
  if (te_parent != nullptr && outliner_is_collection_tree_element(te_parent)) {
    TreeStoreElem *tselem_parent = TREESTORE(te_parent);
    ID *id_parent = tselem_parent->id;
    /* It's not possible to remove an object from an overridden collection (and potentially scene,
     * through the master collection). */
    if (ELEM(GS(id_parent->name), ID_GR, ID_SCE)) {
      if (ID_IS_OVERRIDE_LIBRARY_REAL(id_parent)) {
        return TRAVERSE_SKIP_CHILDS;
      }
    }
  }

  ID *id = tselem->id;

  if (ID_IS_OVERRIDE_LIBRARY_REAL(id)) {
    if (ID_IS_OVERRIDE_LIBRARY_HIERARCHY_ROOT(id)) {
      if (!(data->is_liboverride_hierarchy_root_allowed || data->is_liboverride_allowed)) {
        return TRAVERSE_SKIP_CHILDS;
      }
    }
    else {
      if (!data->is_liboverride_allowed) {
        return TRAVERSE_SKIP_CHILDS;
      }
    }
  }

  data->objects_set.add(reinterpret_cast<Object *>(id));

  return TRAVERSE_CONTINUE;
}

static wmOperatorStatus outliner_delete_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  wmMsgBus *mbus = CTX_wm_message_bus(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  const Base *basact_prev = BKE_view_layer_active_base_get(view_layer);

  const bool delete_hierarchy = RNA_boolean_get(op->ptr, "hierarchy");

  /* Get selected objects skipping duplicates to prevent deleting objects linked to multiple
   * collections twice */
  ObjectEditData object_delete_data = {};
  object_delete_data.is_liboverride_allowed = false;
  object_delete_data.is_liboverride_hierarchy_root_allowed = delete_hierarchy;
  outliner_tree_traverse(space_outliner,
                         &space_outliner->tree,
                         0,
                         TSE_SELECTED,
                         outliner_collect_objects_to_delete,
                         &object_delete_data);

  if (delete_hierarchy) {
    BKE_main_id_tag_all(bmain, ID_TAG_DOIT, false);

    BKE_view_layer_synced_ensure(scene, view_layer);

    /* #object_batch_delete_hierarchy_fn callback will only remove objects from collections and tag
     * them for deletion. */
    outliner_do_object_delete(C,
                              op->reports,
                              scene,
                              object_delete_data.objects_set,
                              object_batch_delete_hierarchy_tag_fn);

    outliner_batch_delete_object_hierarchy(bmain, scene);
  }
  else {
    outliner_do_object_delete(
        C, op->reports, scene, object_delete_data.objects_set, outliner_object_delete_fn);
  }

  outliner_collection_delete(C, bmain, scene, op->reports, delete_hierarchy);

  /* Tree management normally happens from draw_outliner(), but when
   * you're clicking too fast on Delete object from context menu in
   * outliner several mouse events can be handled in one cycle without
   * handling notifiers/redraw which leads to deleting the same object twice.
   * cleanup tree here to prevent such cases. */
  outliner_cleanup_tree(space_outliner);

  DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL | ID_RECALC_HIERARCHY);
  DEG_relations_tag_update(bmain);

  BKE_view_layer_synced_ensure(scene, view_layer);
  if (basact_prev != BKE_view_layer_active_base_get(view_layer)) {
    WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);
    WM_msg_publish_rna_prop(mbus, &scene->id, view_layer, LayerObjects, active);
  }

  BKE_main_ensure_invariants(*bmain);

  DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
  WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, scene);
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
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  PropertyRNA *prop = RNA_def_boolean(
      ot->srna, "hierarchy", false, "Hierarchy", "Delete child objects and collections");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name ID-Data Menu Operator
 * \{ */

enum eOutlinerIdOpTypes {
  OUTLINER_IDOP_INVALID = 0,

  OUTLINER_IDOP_UNLINK,
  OUTLINER_IDOP_LOCAL,
  OUTLINER_IDOP_SINGLE,
  OUTLINER_IDOP_DELETE,
  OUTLINER_IDOP_REMAP,

  OUTLINER_IDOP_COPY,
  OUTLINER_IDOP_PASTE,

  OUTLINER_IDOP_FAKE_ADD,
  OUTLINER_IDOP_FAKE_CLEAR,
  OUTLINER_IDOP_RENAME,

  OUTLINER_IDOP_SELECT_LINKED,
};

/* TODO: implement support for changing the ID-block used. */
static const EnumPropertyItem prop_id_op_types[] = {
    {OUTLINER_IDOP_UNLINK, "UNLINK", 0, "Unlink", ""},
    {OUTLINER_IDOP_LOCAL, "LOCAL", 0, "Make Local", ""},
    {OUTLINER_IDOP_SINGLE, "SINGLE", 0, "Make Single User", ""},
    {OUTLINER_IDOP_DELETE, "DELETE", ICON_X, "Delete", ""},
    {OUTLINER_IDOP_REMAP,
     "REMAP",
     0,
     "Remap Users",
     "Make all users of selected data-blocks to use instead current (clicked) one"},
    RNA_ENUM_ITEM_SEPR,
    {OUTLINER_IDOP_COPY, "COPY", ICON_COPYDOWN, "Copy", ""},
    {OUTLINER_IDOP_PASTE, "PASTE", ICON_PASTEDOWN, "Paste", ""},
    RNA_ENUM_ITEM_SEPR,
    {OUTLINER_IDOP_FAKE_ADD,
     "ADD_FAKE",
     0,
     "Add Fake User",
     "Ensure data-block gets saved even if it isn't in use (e.g. for motion and material "
     "libraries)"},
    {OUTLINER_IDOP_FAKE_CLEAR, "CLEAR_FAKE", 0, "Clear Fake User", ""},
    {OUTLINER_IDOP_RENAME, "RENAME", 0, "Rename", ""},
    {OUTLINER_IDOP_SELECT_LINKED, "SELECT_LINKED", 0, "Select Linked", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static bool outliner_id_operation_item_poll(bContext *C,
                                            PointerRNA * /*ptr*/,
                                            PropertyRNA * /*prop*/,
                                            const int enum_value)
{
  if (!outliner_operation_tree_element_poll(C)) {
    return false;
  }

  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  TreeElement *te = get_target_element(space_outliner);
  TreeStoreElem *tselem = TREESTORE(te);
  if (!TSE_IS_REAL_ID(tselem)) {
    return false;
  }

  switch (enum_value) {
    case OUTLINER_IDOP_SINGLE:
      if (ELEM(space_outliner->outlinevis, SO_SCENES, SO_VIEW_LAYER)) {
        return true;
      }
      /* TODO(dalai): enable in the few cases where this can be supported
       * (i.e., when we have a valid parent for the tselem). */
      return false;
  }

  return true;
}

static const EnumPropertyItem *outliner_id_operation_itemf(bContext *C,
                                                           PointerRNA *ptr,
                                                           PropertyRNA *prop,
                                                           bool *r_free)
{
  EnumPropertyItem *items = nullptr;
  int totitem = 0;

  if ((C == nullptr) || (ED_operator_outliner_active(C) == false)) {
    return prop_id_op_types;
  }
  for (const EnumPropertyItem *it = prop_id_op_types; it->identifier != nullptr; it++) {
    if (!outliner_id_operation_item_poll(C, ptr, prop, it->value)) {
      continue;
    }
    RNA_enum_item_add(&items, &totitem, it);
  }
  RNA_enum_item_end(&items, &totitem);
  *r_free = true;

  return items;
}

static wmOperatorStatus outliner_id_operation_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  wmWindowManager *wm = CTX_wm_manager(C);
  Scene *scene = CTX_data_scene(C);
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  int scenelevel = 0, objectlevel = 0, idlevel = 0, datalevel = 0;

  /* check for invalid states */
  if (space_outliner == nullptr) {
    return OPERATOR_CANCELLED;
  }

  TreeElement *te = get_target_element(space_outliner);
  get_element_operation_type(te, &scenelevel, &objectlevel, &idlevel, &datalevel);

  eOutlinerIdOpTypes event = (eOutlinerIdOpTypes)RNA_enum_get(op->ptr, "type");
  switch (event) {
    case OUTLINER_IDOP_UNLINK: {
      /* unlink datablock from its parent */
      if (objectlevel) {
        outliner_do_libdata_operation(C, op->reports, scene, space_outliner, unlink_object_fn);

        WM_event_add_notifier(C, NC_SCENE | ND_LAYER, nullptr);
        ED_undo_push(C, "Unlink Object");
        break;
      }

      switch (idlevel) {
        case ID_AC:
          outliner_do_libdata_operation(C, op->reports, scene, space_outliner, unlink_action_fn);

          WM_event_add_notifier(C, NC_ANIMATION | ND_NLA_ACTCHANGE, nullptr);
          ED_undo_push(C, "Unlink action");
          break;
        case ID_MA:
          outliner_do_libdata_operation(C, op->reports, scene, space_outliner, unlink_material_fn);

          WM_event_add_notifier(C, NC_OBJECT | ND_OB_SHADING, nullptr);
          ED_undo_push(C, "Unlink material");
          break;
        case ID_TE:
          outliner_do_libdata_operation(C, op->reports, scene, space_outliner, unlink_texture_fn);

          WM_event_add_notifier(C, NC_OBJECT | ND_OB_SHADING, nullptr);
          ED_undo_push(C, "Unlink texture");
          break;
        case ID_WO:
          outliner_do_libdata_operation(C, op->reports, scene, space_outliner, unlink_world_fn);

          WM_event_add_notifier(C, NC_SCENE | ND_WORLD, nullptr);
          ED_undo_push(C, "Unlink world");
          break;
        case ID_GR:
          outliner_do_libdata_operation(
              C, op->reports, scene, space_outliner, unlink_collection_fn);

          WM_event_add_notifier(C, NC_SCENE | ND_LAYER, nullptr);
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
      outliner_do_libdata_operation(C, op->reports, scene, space_outliner, id_local_fn);
      ED_undo_push(C, "Localized Data");
      break;
    }
    case OUTLINER_IDOP_SINGLE: {
      /* make single user */
      switch (idlevel) {
        case ID_AC:
          outliner_do_libdata_operation(
              C, op->reports, scene, space_outliner, singleuser_action_fn);

          WM_event_add_notifier(C, NC_ANIMATION | ND_NLA_ACTCHANGE, nullptr);
          ED_undo_push(C, "Single-User Action");
          break;

        case ID_WO:
          outliner_do_libdata_operation(
              C, op->reports, scene, space_outliner, singleuser_world_fn);

          WM_event_add_notifier(C, NC_SCENE | ND_WORLD, nullptr);
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
        BKE_main_id_tag_all(bmain, ID_TAG_DOIT, false);
        outliner_do_libdata_operation(C, op->reports, scene, space_outliner, id_delete_tag_fn);
        BKE_id_multi_tagged_delete(bmain);
        WM_event_add_notifier(C, NC_OBJECT, nullptr);
        ED_undo_push(C, "Delete");
      }
      break;
    }
    case OUTLINER_IDOP_REMAP: {
      if (idlevel > 0 || objectlevel) {
        outliner_do_libdata_operation(C, op->reports, scene, space_outliner, id_remap_fn);
        /* No undo push here, operator does it itself (since it's a modal one, the op_undo_depth
         * trick does not work here). */
      }
      break;
    }
    case OUTLINER_IDOP_COPY: {
      wm->op_undo_depth++;
      WM_operator_name_call(
          C, "OUTLINER_OT_id_copy", wm::OpCallContext::InvokeDefault, nullptr, nullptr);
      wm->op_undo_depth--;
      /* No need for undo, this operation does not change anything... */
      break;
    }
    case OUTLINER_IDOP_PASTE: {
      wm->op_undo_depth++;
      WM_operator_name_call(
          C, "OUTLINER_OT_id_paste", wm::OpCallContext::InvokeDefault, nullptr, nullptr);
      wm->op_undo_depth--;
      ED_outliner_select_sync_from_all_tag(C);
      ED_undo_push(C, "Paste");
      break;
    }
    case OUTLINER_IDOP_FAKE_ADD: {
      /* set fake user */
      outliner_do_libdata_operation(C, op->reports, scene, space_outliner, id_fake_user_set_fn);

      WM_event_add_notifier(C, NC_ID | NA_EDITED, nullptr);
      ED_undo_push(C, "Add Fake User");
      break;
    }
    case OUTLINER_IDOP_FAKE_CLEAR: {
      /* clear fake user */
      outliner_do_libdata_operation(C, op->reports, scene, space_outliner, id_fake_user_clear_fn);

      WM_event_add_notifier(C, NC_ID | NA_EDITED, nullptr);
      ED_undo_push(C, "Clear Fake User");
      break;
    }
    case OUTLINER_IDOP_RENAME: {
      /* rename */
      outliner_do_libdata_operation(C, op->reports, scene, space_outliner, item_rename_fn);

      WM_event_add_notifier(C, NC_ID | NA_EDITED, nullptr);
      ED_undo_push(C, "Rename");
      break;
    }
    case OUTLINER_IDOP_SELECT_LINKED:
      outliner_do_libdata_operation(C, op->reports, scene, space_outliner, id_select_linked_fn);
      ED_outliner_select_sync_from_all_tag(C);
      ED_undo_push(C, "Select");
      break;

    default:
      /* Invalid - unhandled. */
      break;
  }

  BKE_main_ensure_invariants(*bmain);

  /* wrong notifier still... */
  WM_event_add_notifier(C, NC_ID | NA_EDITED, nullptr);

  /* XXX: this is just so that outliner is always up to date. */
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_OUTLINER, nullptr);

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_id_operation(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Outliner ID Data Operation";
  ot->idname = "OUTLINER_OT_id_operation";
  ot->description = "General data-block management operations";

  /* callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = outliner_id_operation_exec;
  ot->poll = outliner_operation_tree_element_poll;

  ot->flag = 0;

  ot->prop = RNA_def_enum(ot->srna, "type", prop_id_op_types, 0, "ID Data Operation", "");
  RNA_def_enum_funcs(ot->prop, outliner_id_operation_itemf);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Library Menu Operator
 * \{ */

enum eOutlinerLibOpTypes {
  OL_LIB_INVALID = 0,

  OL_LIB_DELETE,
  OL_LIB_RELOCATE,
  OL_LIB_RELOAD,
};

static const EnumPropertyItem outliner_lib_op_type_items[] = {
    {OL_LIB_DELETE, "DELETE", ICON_X, "Delete", "Delete this library and all its items"},
    {OL_LIB_RELOCATE,
     "RELOCATE",
     0,
     "Relocate",
     "Select a new path for this library, and reload all its data"},
    {OL_LIB_RELOAD, "RELOAD", ICON_FILE_REFRESH, "Reload", "Reload all data from this library"},
    {0, nullptr, 0, nullptr, nullptr},
};

static wmOperatorStatus outliner_lib_operation_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);

  /* check for invalid states */
  if (space_outliner == nullptr) {
    return OPERATOR_CANCELLED;
  }

  eOutlinerLibOpTypes event = (eOutlinerLibOpTypes)RNA_enum_get(op->ptr, "type");
  switch (event) {
    case OL_LIB_DELETE: {
      BKE_main_id_tag_all(bmain, ID_TAG_DOIT, false);
      outliner_do_libdata_operation(C, op->reports, scene, space_outliner, id_delete_tag_fn);
      BKE_id_multi_tagged_delete(bmain);
      ED_undo_push(C, "Delete Library");
      break;
    }
    case OL_LIB_RELOCATE: {
      outliner_do_libdata_operation(C, op->reports, scene, space_outliner, lib_relocate_fn);
      /* No undo push here, operator does it itself (since it's a modal one, the op_undo_depth
       * trick does not work here). */
      break;
    }
    case OL_LIB_RELOAD: {
      outliner_do_libdata_operation(C, op->reports, scene, space_outliner, lib_reload_fn);
      /* No undo push here, operator does it itself (since it's a modal one, the op_undo_depth
       * trick does not work here). */
      break;
    }
    default:
      /* invalid - unhandled */
      break;
  }

  BKE_main_ensure_invariants(*bmain);

  /* wrong notifier still... */
  WM_event_add_notifier(C, NC_ID | NA_EDITED, nullptr);

  /* XXX: this is just so that outliner is always up to date */
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_OUTLINER, nullptr);

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
  ot->poll = outliner_operation_tree_element_poll;

  ot->prop = RNA_def_enum(
      ot->srna, "type", outliner_lib_op_type_items, 0, "Library Operation", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Outliner Set Active Action Operator
 * \{ */

static void outliner_do_id_set_operation(
    SpaceOutliner *space_outliner,
    int type,
    ID *newid,
    void (*operation_fn)(TreeElement *, TreeStoreElem *, TreeStoreElem *, ID *))
{
  tree_iterator::all_open(*space_outliner, [&](TreeElement *te) {
    TreeStoreElem *tselem = TREESTORE(te);
    if (tselem->flag & TSE_SELECTED) {
      if (tselem->type == type) {
        TreeStoreElem *tsep = te->parent ? TREESTORE(te->parent) : nullptr;
        operation_fn(te, tselem, tsep, newid);
      }
    }
  });
}

static void actionset_id_fn(TreeElement * /*te*/,
                            TreeStoreElem *tselem,
                            TreeStoreElem *tsep,
                            ID *actId)
{
  bAction *act = (bAction *)actId;

  if (tselem->type == TSE_ANIM_DATA) {
    /* "animation" entries - action is child of this */
    BKE_animdata_set_action(nullptr, tselem->id, act);
  }
  /* TODO: if any other "expander" channels which own actions need to support this menu,
   * add: tselem->type = ...
   */
  else if (tsep && (tsep->type == TSE_ANIM_DATA)) {
    /* "animation" entries case again */
    BKE_animdata_set_action(nullptr, tsep->id, act);
  }
  /* TODO: other cases not supported yet. */
}

static wmOperatorStatus outliner_action_set_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  int scenelevel = 0, objectlevel = 0, idlevel = 0, datalevel = 0;
  bAction *act;

  TreeElement *te = get_target_element(space_outliner);
  get_element_operation_type(te, &scenelevel, &objectlevel, &idlevel, &datalevel);

  /* get action to use */
  act = static_cast<bAction *>(BLI_findlink(&bmain->actions, RNA_enum_get(op->ptr, "action")));

  if (act == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "No valid action to add");
    return OPERATOR_CANCELLED;
  }
  if (act->idroot == 0 && blender::animrig::legacy::action_treat_as_legacy(*act)) {
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
    outliner_do_id_set_operation(space_outliner, datalevel, (ID *)act, actionset_id_fn);
  }
  else if (idlevel == ID_AC) {
    outliner_do_id_set_operation(space_outliner, idlevel, (ID *)act, actionset_id_fn);
  }
  else {
    return OPERATOR_CANCELLED;
  }

  /* set notifier that things have changed */
  DEG_id_tag_update(te->store_elem->id, ID_RECALC_ANIMATION);
  WM_event_add_notifier(C, NC_ANIMATION | ND_NLA_ACTCHANGE, nullptr);
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

  /* API callbacks. */
  ot->invoke = WM_enum_search_invoke;
  ot->exec = outliner_action_set_exec;
  ot->poll = outliner_operation_tree_element_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  /* TODO: this would be nicer as an ID-pointer... */
  prop = RNA_def_enum(ot->srna, "action", rna_enum_dummy_NULL_items, 0, "Action", "");
  RNA_def_enum_funcs(prop, RNA_action_itemf);
  RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
  ot->prop = prop;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Animation Menu Operator
 * \{ */

enum eOutliner_AnimDataOps {
  OUTLINER_ANIMOP_INVALID = 0,

  OUTLINER_ANIMOP_CLEAR_ADT,

  OUTLINER_ANIMOP_SET_ACT,
  OUTLINER_ANIMOP_CLEAR_ACT,

  OUTLINER_ANIMOP_REFRESH_DRV,
  OUTLINER_ANIMOP_CLEAR_DRV
};

static const EnumPropertyItem prop_animdata_op_types[] = {
    {OUTLINER_ANIMOP_CLEAR_ADT,
     "CLEAR_ANIMDATA",
     0,
     "Clear Animation Data",
     "Remove this animation data container"},
    {OUTLINER_ANIMOP_SET_ACT, "SET_ACT", 0, "Set Action", ""},
    {OUTLINER_ANIMOP_CLEAR_ACT, "CLEAR_ACT", 0, "Unlink Action", ""},
    {OUTLINER_ANIMOP_REFRESH_DRV, "REFRESH_DRIVERS", 0, "Refresh Drivers", ""},
    {OUTLINER_ANIMOP_CLEAR_DRV, "CLEAR_DRIVERS", 0, "Clear Drivers", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static wmOperatorStatus outliner_animdata_operation_exec(bContext *C, wmOperator *op)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  int scenelevel = 0, objectlevel = 0, idlevel = 0, datalevel = 0;
  TreeElement *te = get_target_element(space_outliner);
  get_element_operation_type(te, &scenelevel, &objectlevel, &idlevel, &datalevel);

  if (datalevel != TSE_ANIM_DATA) {
    return OPERATOR_CANCELLED;
  }

  /* perform the core operation */
  eOutliner_AnimDataOps event = (eOutliner_AnimDataOps)RNA_enum_get(op->ptr, "type");
  switch (event) {
    case OUTLINER_ANIMOP_CLEAR_ADT:
      /* Remove Animation Data - this may remove the active action, in some cases... */
      outliner_do_data_operation(space_outliner, datalevel, event, clear_animdata_fn, nullptr);

      WM_event_add_notifier(C, NC_ANIMATION | ND_NLA_ACTCHANGE, nullptr);
      ED_undo_push(C, "Clear Animation Data");
      break;

    case OUTLINER_ANIMOP_SET_ACT:
      /* delegate once again... */
      wm->op_undo_depth++;
      WM_operator_name_call(
          C, "OUTLINER_OT_action_set", wm::OpCallContext::InvokeRegionWin, nullptr, nullptr);
      wm->op_undo_depth--;
      ED_undo_push(C, "Set active action");
      break;

    case OUTLINER_ANIMOP_CLEAR_ACT:
      /* clear active action - using standard rules */
      outliner_do_data_operation(space_outliner, datalevel, event, unlinkact_animdata_fn, nullptr);

      WM_event_add_notifier(C, NC_ANIMATION | ND_NLA_ACTCHANGE, nullptr);
      ED_undo_push(C, "Unlink action");
      break;

    case OUTLINER_ANIMOP_REFRESH_DRV:
      outliner_do_data_operation(
          space_outliner, datalevel, event, refreshdrivers_animdata_fn, nullptr);

      WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN, nullptr);
      // ED_undo_push(C, "Refresh Drivers"); /* No undo needed - shouldn't have any impact? */
      break;

    case OUTLINER_ANIMOP_CLEAR_DRV:
      outliner_do_data_operation(
          space_outliner, datalevel, event, cleardrivers_animdata_fn, nullptr);

      WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN, nullptr);
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
    {0, nullptr, 0, nullptr, nullptr},
};

static wmOperatorStatus outliner_constraint_operation_exec(bContext *C, wmOperator *op)
{
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  eOutliner_PropConstraintOps event = (eOutliner_PropConstraintOps)RNA_enum_get(op->ptr, "type");

  outliner_do_data_operation(space_outliner, TSE_CONSTRAINT, event, constraint_fn, C);

  if (event == OL_CONSTRAINTOP_DELETE) {
    outliner_cleanup_tree(space_outliner);
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
    {OL_MODIFIER_OP_APPLY, "APPLY", ICON_CHECKMARK, "Apply", ""},
    {OL_MODIFIER_OP_DELETE, "DELETE", ICON_X, "Delete", ""},
    RNA_ENUM_ITEM_SEPR,
    {OL_MODIFIER_OP_TOGVIS, "TOGVIS", ICON_RESTRICT_VIEW_OFF, "Toggle Viewport Use", ""},
    {OL_MODIFIER_OP_TOGREN, "TOGREN", ICON_RESTRICT_RENDER_OFF, "Toggle Render Use", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static wmOperatorStatus outliner_modifier_operation_exec(bContext *C, wmOperator *op)
{
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  eOutliner_PropModifierOps event = (eOutliner_PropModifierOps)RNA_enum_get(op->ptr, "type");

  ModifierFnArgs args{};
  args.C = C;
  args.reports = op->reports;

  outliner_do_data_operation(space_outliner, TSE_MODIFIER, event, modifier_fn, &args);

  if (ELEM(event, OL_MODIFIER_OP_DELETE, OL_MODIFIER_OP_APPLY)) {
    outliner_cleanup_tree(space_outliner);
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

static bool outliner_data_operation_poll(bContext *C)
{
  if (!ED_operator_outliner_active(C)) {
    return false;
  }
  const SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  const TreeElement *te = get_target_element(space_outliner);

  if (te == nullptr) {
    return false;
  }

  int scenelevel = 0, objectlevel = 0, idlevel = 0, datalevel = 0;
  get_element_operation_type(te, &scenelevel, &objectlevel, &idlevel, &datalevel);
  return ELEM(
      datalevel, TSE_POSE_CHANNEL, TSE_BONE, TSE_EBONE, TSE_STRIP, TSE_GP_LAYER, TSE_RNA_STRUCT);
}

static wmOperatorStatus outliner_data_operation_exec(bContext *C, wmOperator *op)
{
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  int scenelevel = 0, objectlevel = 0, idlevel = 0, datalevel = 0;
  TreeElement *te = get_target_element(space_outliner);
  get_element_operation_type(te, &scenelevel, &objectlevel, &idlevel, &datalevel);

  eOutliner_PropDataOps event = (eOutliner_PropDataOps)RNA_enum_get(op->ptr, "type");
  switch (datalevel) {
    case TSE_POSE_CHANNEL: {
      outliner_do_data_operation(space_outliner, datalevel, event, pchan_fn, nullptr);
      WM_event_add_notifier(C, NC_OBJECT | ND_POSE, nullptr);
      ED_undo_push(C, "PoseChannel operation");

      break;
    }
    case TSE_BONE: {
      outliner_do_data_operation(space_outliner, datalevel, event, bone_fn, nullptr);
      WM_event_add_notifier(C, NC_OBJECT | ND_POSE, nullptr);
      ED_undo_push(C, "Bone operation");

      break;
    }
    case TSE_EBONE: {
      outliner_do_data_operation(space_outliner, datalevel, event, ebone_fn, nullptr);
      WM_event_add_notifier(C, NC_OBJECT | ND_POSE, nullptr);
      ED_undo_push(C, "EditBone operation");

      break;
    }
    case TSE_STRIP: {
      Scene *sequencer_scene = CTX_data_sequencer_scene(C);
      outliner_do_data_operation(space_outliner, datalevel, event, sequence_fn, sequencer_scene);
      WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER | NA_SELECTED, sequencer_scene);
      ED_undo_push(C, "Sequencer operation");

      break;
    }
    case TSE_GP_LAYER: {
      outliner_do_data_operation(space_outliner, datalevel, event, gpencil_layer_fn, nullptr);
      WM_event_add_notifier(C, NC_GPENCIL | ND_DATA, nullptr);
      ED_undo_push(C, "Grease Pencil Layer operation");

      break;
    }
    case TSE_GREASE_PENCIL_NODE: {
      outliner_do_data_operation(space_outliner, datalevel, event, grease_pencil_node_fn, nullptr);
      WM_event_add_notifier(C, NC_GPENCIL | ND_DATA, nullptr);
      ED_undo_push(C, "Grease Pencil Node operation");
      break;
    }
    case TSE_RNA_STRUCT:
      if (event == OL_DOP_SELECT_LINKED) {
        outliner_do_data_operation(space_outliner, datalevel, event, data_select_linked_fn, C);
      }

      break;

    default:
      BKE_report(op->reports, RPT_WARNING, "Not yet implemented");
      break;
  }

  return OPERATOR_FINISHED;
}

/* Dynamically populate an enum of Keying Sets */
static const EnumPropertyItem *outliner_data_op_sets_enum_item_fn(bContext *C,
                                                                  PointerRNA * /*ptr*/,
                                                                  PropertyRNA * /*prop*/,
                                                                  bool * /*r_free*/)
{
  /* Check for invalid states. */
  if (C == nullptr) {
    return rna_enum_dummy_DEFAULT_items;
  }

  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  if (space_outliner == nullptr) {
    return rna_enum_dummy_DEFAULT_items;
  }

  TreeElement *te = get_target_element(space_outliner);
  if (te == nullptr) {
    return rna_enum_dummy_NULL_items;
  }

  TreeStoreElem *tselem = TREESTORE(te);

  static const EnumPropertyItem optype_sel_and_hide[] = {
      {OL_DOP_SELECT, "SELECT", 0, "Select", ""},
      {OL_DOP_DESELECT, "DESELECT", 0, "Deselect", ""},
      {OL_DOP_HIDE, "HIDE", 0, "Hide", ""},
      {OL_DOP_UNHIDE, "UNHIDE", 0, "Unhide", ""},
      {0, nullptr, 0, nullptr, nullptr}};

  static const EnumPropertyItem optype_sel_linked[] = {
      {OL_DOP_SELECT_LINKED, "SELECT_LINKED", 0, "Select Linked", ""},
      {0, nullptr, 0, nullptr, nullptr}};

  if (tselem->type == TSE_RNA_STRUCT) {
    return optype_sel_linked;
  }

  return optype_sel_and_hide;
}

void OUTLINER_OT_data_operation(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Outliner Data Operation";
  ot->idname = "OUTLINER_OT_data_operation";

  /* callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = outliner_data_operation_exec;
  ot->poll = outliner_data_operation_poll;

  ot->flag = 0;

  ot->prop = RNA_def_enum(ot->srna, "type", rna_enum_dummy_DEFAULT_items, 0, "Data Operation", "");
  RNA_def_enum_funcs(ot->prop, outliner_data_op_sets_enum_item_fn);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Context Menu Operator
 * \{ */

static wmOperatorStatus outliner_operator_menu(bContext *C, const char *opname)
{
  wmOperatorType *ot = WM_operatortype_find(opname, false);
  uiPopupMenu *pup = UI_popup_menu_begin(C, WM_operatortype_name(ot, nullptr).c_str(), ICON_NONE);
  uiLayout *layout = UI_popup_menu_layout(pup);

  /* Set this so the default execution context is the same as sub-menus. */
  layout->operator_context_set(wm::OpCallContext::InvokeRegionWin);

  if (WM_operator_poll(C, ot)) {
    layout->op_enum(ot->idname, RNA_property_identifier(ot->prop));

    layout->separator();
  }

  layout->menu_contents("OUTLINER_MT_context_menu");

  UI_popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

static wmOperatorStatus do_outliner_operation_event(bContext *C,
                                                    ARegion *region,
                                                    SpaceOutliner *space_outliner,
                                                    TreeElement *te)
{
  int scenelevel = 0, objectlevel = 0, idlevel = 0, datalevel = 0;
  TreeStoreElem *tselem = TREESTORE(te);

  int select_flag = OL_ITEM_ACTIVATE | OL_ITEM_SELECT;
  if (tselem->flag & TSE_SELECTED) {
    select_flag |= OL_ITEM_EXTEND;
  }

  outliner_item_select(C, space_outliner, te, select_flag);

  /* Only redraw, don't rebuild here because TreeElement pointers will
   * become invalid and operations will crash. */
  ED_region_tag_redraw_no_rebuild(region);
  ED_outliner_select_sync_from_outliner(C, space_outliner);

  get_element_operation_type(te, &scenelevel, &objectlevel, &idlevel, &datalevel);

  if (scenelevel) {
    return outliner_operator_menu(C, "OUTLINER_OT_scene_operation");
  }
  if (objectlevel) {
    WM_menu_name_call(C, "OUTLINER_MT_object", wm::OpCallContext::InvokeRegionWin);
    return OPERATOR_FINISHED;
  }
  if (idlevel) {
    switch (idlevel) {
      case ID_GR:
        WM_menu_name_call(C, "OUTLINER_MT_collection", wm::OpCallContext::InvokeRegionWin);
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
    if (datalevel == TSE_ANIM_DATA) {
      return outliner_operator_menu(C, "OUTLINER_OT_animdata_operation");
    }
    if (datalevel == TSE_DRIVER_BASE) {
      /* do nothing... no special ops needed yet */
      return OPERATOR_CANCELLED;
    }
    if (datalevel == TSE_LAYER_COLLECTION) {
      WM_menu_name_call(C, "OUTLINER_MT_collection", wm::OpCallContext::InvokeRegionWin);
      return OPERATOR_FINISHED;
    }
    if (ELEM(datalevel, TSE_SCENE_COLLECTION_BASE, TSE_VIEW_COLLECTION_BASE)) {
      WM_menu_name_call(C, "OUTLINER_MT_collection_new", wm::OpCallContext::InvokeRegionWin);
      return OPERATOR_FINISHED;
    }
    if (datalevel == TSE_ID_BASE) {
      /* do nothing... there are no ops needed here yet */
      return OPERATOR_CANCELLED;
    }
    if (datalevel == TSE_CONSTRAINT) {
      return outliner_operator_menu(C, "OUTLINER_OT_constraint_operation");
    }
    if (datalevel == TSE_MODIFIER) {
      return outliner_operator_menu(C, "OUTLINER_OT_modifier_operation");
    }
    return outliner_operator_menu(C, "OUTLINER_OT_data_operation");
  }

  return OPERATOR_CANCELLED;
}

static wmOperatorStatus outliner_operation_invoke(bContext *C,
                                                  wmOperator * /*op*/,
                                                  const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  uiBut *but = UI_context_active_but_get(C);
  float view_mval[2];

  if (but) {
    UI_but_tooltip_timer_remove(C, but);
  }

  UI_view2d_region_to_view(
      &region->v2d, event->mval[0], event->mval[1], &view_mval[0], &view_mval[1]);

  TreeElement *hovered_te = outliner_find_item_at_y(
      space_outliner, &space_outliner->tree, view_mval[1]);
  if (!hovered_te) {
    /* Let this fall through to 'OUTLINER_MT_context_menu'. */
    return OPERATOR_PASS_THROUGH;
  }

  return do_outliner_operation_event(C, region, space_outliner, hovered_te);
}

void OUTLINER_OT_operation(wmOperatorType *ot)
{
  ot->name = "Context Menu";
  ot->idname = "OUTLINER_OT_operation";
  ot->description = "Context menu for item operations";

  ot->invoke = outliner_operation_invoke;

  ot->poll = ED_operator_region_outliner_active;
}

/** \} */

}  // namespace blender::ed::outliner
