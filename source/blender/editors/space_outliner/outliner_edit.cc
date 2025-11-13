/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include <algorithm>
#include <cstring>

#include <fmt/format.h>

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"
#include "DNA_anim_types.h"
#include "DNA_collection_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_listbase.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BLF_api.hh"

#include "BKE_action.hh"
#include "BKE_animsys.h"
#include "BKE_appdir.hh"
#include "BKE_armature.hh"
#include "BKE_blender_copybuffer.hh"
#include "BKE_blendfile.hh"
#include "BKE_context.hh"
#include "BKE_idtype.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_override.hh"
#include "BKE_lib_query.hh"
#include "BKE_lib_remap.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_object.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"
#include "BKE_screen.hh"
#include "BKE_workspace.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "ED_keyframing.hh"
#include "ED_outliner.hh"
#include "ED_scene.hh"
#include "ED_screen.hh"
#include "ED_select_utils.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_view2d.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_path.hh"
#include "RNA_prototypes.hh"

#include "GPU_material.hh"

#include "outliner_intern.hh"
#include "tree/tree_element_rna.hh"
#include "tree/tree_iterator.hh"

#include "wm_window.hh"

using namespace blender::ed::outliner;

namespace blender::ed::outliner {

static void outliner_show_active(SpaceOutliner *space_outliner,
                                 ARegion *region,
                                 TreeElement *te,
                                 ID *id);

/* -------------------------------------------------------------------- */
/** \name Local Utilities
 * \{ */

static void outliner_copybuffer_filepath_get(char filepath[FILE_MAX], size_t filepath_maxncpy)
{
  /* NOTE: this uses the same path as the 3D viewport. */
  BLI_path_join(filepath, filepath_maxncpy, BKE_tempdir_base(), "copybuffer.blend");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Highlight on Cursor Motion Operator
 * \{ */

static wmOperatorStatus outliner_highlight_update_invoke(bContext *C,
                                                         wmOperator * /*op*/,
                                                         const wmEvent *event)
{
  /* stop highlighting if out of area */
  if (!ED_screen_area_active(C)) {
    return OPERATOR_PASS_THROUGH;
  }

  /* Drag and drop does its own highlighting. */
  wmWindowManager *wm = CTX_wm_manager(C);
  if (wm->runtime->drags.first) {
    return OPERATOR_PASS_THROUGH;
  }

  ARegion *region = CTX_wm_region(C);
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);

  float view_mval[2];
  UI_view2d_region_to_view(
      &region->v2d, event->mval[0], event->mval[1], &view_mval[0], &view_mval[1]);

  TreeElement *hovered_te = outliner_find_item_at_y(
      space_outliner, &space_outliner->tree, view_mval[1]);

  TreeElement *icon_te = nullptr;
  bool is_over_icon = false;
  if (hovered_te) {
    icon_te = outliner_find_item_at_x_in_row(
        space_outliner, hovered_te, view_mval[0], nullptr, &is_over_icon);
  }

  bool changed = false;

  if (!hovered_te || !is_over_icon || !(hovered_te->store_elem->flag & TSE_HIGHLIGHTED) ||
      !(icon_te->store_elem->flag & TSE_HIGHLIGHTED_ICON))
  {
    /* Clear highlights when nothing is hovered or when a new item is hovered. */
    changed = outliner_flag_set(*space_outliner, TSE_HIGHLIGHTED_ANY | TSE_DRAG_ANY, false);
    if (hovered_te) {
      hovered_te->store_elem->flag |= TSE_HIGHLIGHTED;
      changed = true;
    }
    if (is_over_icon) {
      icon_te->store_elem->flag |= TSE_HIGHLIGHTED_ICON;
      changed = true;
    }
  }

  if (changed) {
    ED_region_tag_redraw_no_rebuild(region);
  }

  return OPERATOR_PASS_THROUGH;
}

void OUTLINER_OT_highlight_update(wmOperatorType *ot)
{
  ot->name = "Update Highlight";
  ot->idname = "OUTLINER_OT_highlight_update";
  ot->description = "Update the item highlight based on the current mouse position";

  ot->invoke = outliner_highlight_update_invoke;

  ot->poll = ED_operator_outliner_active;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Toggle Open/Closed Operator
 * \{ */

void outliner_item_openclose(TreeElement *te, bool open, bool toggle_all)
{
  /* Only allow opening elements with children. */
  if (!(te->flag & TE_PRETEND_HAS_CHILDREN) && BLI_listbase_is_empty(&te->subtree)) {
    return;
  }

  /* Don't allow collapsing the scene collection. */
  TreeStoreElem *tselem = TREESTORE(te);
  if (tselem->type == TSE_VIEW_COLLECTION_BASE) {
    return;
  }

  if (open) {
    tselem->flag &= ~TSE_CLOSED;
  }
  else {
    tselem->flag |= TSE_CLOSED;
  }

  if (toggle_all) {
    outliner_flag_set(te->subtree, TSE_CLOSED, !open);
  }
}

struct OpenCloseData {
  TreeStoreElem *prev_tselem;
  bool open;
  int x_location;
};

static wmOperatorStatus outliner_item_openclose_modal(bContext *C,
                                                      wmOperator *op,
                                                      const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  OpenCloseData *data = (OpenCloseData *)op->customdata;

  float view_mval[2];
  UI_view2d_region_to_view(
      &region->v2d, event->mval[0], event->mval[1], &view_mval[0], &view_mval[1]);

  if (event->type == MOUSEMOVE) {
    TreeElement *te = outliner_find_item_at_y(space_outliner, &space_outliner->tree, view_mval[1]);

    /* Only openclose if mouse is not over the previously toggled element */
    if (te && TREESTORE(te) != data->prev_tselem) {

      /* Only toggle openclose on the same level as the first clicked element */
      if (te->xs == data->x_location) {
        outliner_item_openclose(te, data->open, false);

        outliner_tag_redraw_avoid_rebuild_on_open_change(space_outliner, region);
      }
    }

    if (te) {
      data->prev_tselem = TREESTORE(te);
    }
    else {
      data->prev_tselem = nullptr;
    }
  }
  else if (event->val == KM_RELEASE) {
    MEM_freeN(data);

    return OPERATOR_FINISHED;
  }

  return OPERATOR_RUNNING_MODAL;
}

static wmOperatorStatus outliner_item_openclose_invoke(bContext *C,
                                                       wmOperator *op,
                                                       const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);

  const bool toggle_all = RNA_boolean_get(op->ptr, "all");

  float view_mval[2];

  int mval[2];
  WM_event_drag_start_mval(event, region, mval);

  UI_view2d_region_to_view(&region->v2d, mval[0], mval[1], &view_mval[0], &view_mval[1]);

  TreeElement *te = outliner_find_item_at_y(space_outliner, &space_outliner->tree, view_mval[1]);

  if (te && outliner_item_is_co_within_close_toggle(te, view_mval[0])) {
    TreeStoreElem *tselem = TREESTORE(te);

    const bool open = (tselem->flag & TSE_CLOSED) ||
                      (toggle_all && outliner_flag_is_any_test(&te->subtree, TSE_CLOSED, 1));

    outliner_item_openclose(te, open, toggle_all);
    outliner_tag_redraw_avoid_rebuild_on_open_change(space_outliner, region);

    /* Only toggle once for single click toggling */
    if ((event->type == LEFTMOUSE) && (event->val != KM_PRESS_DRAG)) {
      return OPERATOR_FINISHED;
    }

    /* Store last expanded tselem and x coordinate of disclosure triangle */
    OpenCloseData *toggle_data = MEM_callocN<OpenCloseData>("open_close_data");
    toggle_data->prev_tselem = tselem;
    toggle_data->open = open;
    toggle_data->x_location = te->xs;

    /* Store the first clicked on element */
    op->customdata = toggle_data;

    WM_event_add_modal_handler(C, op);
    return OPERATOR_RUNNING_MODAL;
  }

  return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
}

void OUTLINER_OT_item_openclose(wmOperatorType *ot)
{
  ot->name = "Open/Close";
  ot->idname = "OUTLINER_OT_item_openclose";
  ot->description = "Toggle whether item under cursor is enabled or closed";

  ot->invoke = outliner_item_openclose_invoke;
  ot->modal = outliner_item_openclose_modal;

  ot->poll = ED_operator_region_outliner_active;

  RNA_def_boolean(ot->srna, "all", false, "All", "Close or open all items");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Rename Operator
 * \{ */

static void do_item_rename(ARegion *region,
                           TreeElement *te,
                           TreeStoreElem *tselem,
                           ReportList *reports)
{
  bool add_textbut = false;

  /* FIXME: These info messages are often useless, they should be either reworded to be more
   * informative for the user, or purely removed? */

  /* Can't rename rna datablocks entries or listbases. */
  if (ELEM(tselem->type,
           TSE_ANIM_DATA,
           TSE_NLA,
           TSE_DEFGROUP_BASE,
           TSE_CONSTRAINT_BASE,
           TSE_MODIFIER_BASE,
           TSE_DRIVER_BASE,
           TSE_POSE_BASE,
           TSE_R_LAYER_BASE,
           TSE_SCENE_COLLECTION_BASE,
           TSE_VIEW_COLLECTION_BASE,
           TSE_LIBRARY_OVERRIDE_BASE,
           TSE_BONE_COLLECTION_BASE,
           TSE_RNA_STRUCT,
           TSE_RNA_PROPERTY,
           TSE_RNA_ARRAY_ELEM,
           TSE_ID_BASE) ||
      ELEM(tselem->type, TSE_SCENE_OBJECTS_BASE, TSE_GENERIC_LABEL, TSE_GPENCIL_EFFECT_BASE))
  {
    BKE_report(reports, RPT_INFO, "Not an editable name");
  }
  else if (ELEM(tselem->type, TSE_STRIP, TSE_STRIP_DATA, TSE_STRIP_DUP)) {
    BKE_report(reports, RPT_INFO, "Strip names are not editable from the Outliner");
  }
  else if (TSE_IS_REAL_ID(tselem) && !ID_IS_EDITABLE(tselem->id)) {
    BKE_report(reports, RPT_INFO, "External library data is not editable");
  }
  else if (TSE_IS_REAL_ID(tselem) && ID_IS_OVERRIDE_LIBRARY(tselem->id)) {
    BKE_report(reports, RPT_INFO, "Overridden data-blocks names are not editable");
  }
  else if (outliner_is_collection_tree_element(te)) {
    Collection *collection = outliner_collection_from_tree_element(te);

    if (collection->flag & COLLECTION_IS_MASTER) {
      BKE_report(reports, RPT_INFO, "Not an editable name");
    }
    else {
      add_textbut = true;
    }
  }
  else if (te->idcode == ID_LI) {
    BKE_report(reports, RPT_INFO, "Library path is not editable, use the Relocate operation");
  }
  else {
    add_textbut = true;
  }

  if (add_textbut) {
    tselem->flag |= TSE_TEXTBUT;
    ED_region_tag_redraw(region);
  }
}

void item_rename_fn(bContext *C,
                    ReportList *reports,
                    Scene * /*scene*/,
                    TreeElement *te,
                    TreeStoreElem * /*tsep*/,
                    TreeStoreElem *tselem)
{
  ARegion *region = CTX_wm_region(C);
  do_item_rename(region, te, tselem, reports);
}

static TreeElement *outliner_item_rename_find_active(const SpaceOutliner *space_outliner,
                                                     ReportList *reports)
{
  TreeElement *active_element = outliner_find_element_with_flag(&space_outliner->tree, TSE_ACTIVE);

  if (!active_element) {
    BKE_report(reports, RPT_WARNING, "No active item to rename");
    return nullptr;
  }

  return active_element;
}

static TreeElement *outliner_item_rename_find_hovered(const SpaceOutliner *space_outliner,
                                                      ARegion *region,
                                                      const wmEvent *event)
{
  float fmval[2];
  UI_view2d_region_to_view(&region->v2d, event->mval[0], event->mval[1], &fmval[0], &fmval[1]);

  TreeElement *hovered = outliner_find_item_at_y(space_outliner, &space_outliner->tree, fmval[1]);
  if (hovered && outliner_item_is_co_over_name(hovered, fmval[0])) {
    return hovered;
  }

  return nullptr;
}

static wmOperatorStatus outliner_item_rename_invoke(bContext *C,
                                                    wmOperator *op,
                                                    const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  View2D *v2d = &region->v2d;
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  const bool use_active = RNA_boolean_get(op->ptr, "use_active");

  TreeElement *te = use_active ? outliner_item_rename_find_active(space_outliner, op->reports) :
                                 outliner_item_rename_find_hovered(space_outliner, region, event);
  if (!te) {
    return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
  }

  /* Force element into view. */
  outliner_show_active(space_outliner, region, te, TREESTORE(te)->id);

  if (te->ys < int(v2d->cur.ymin + UI_UNIT_Y)) {
    /* Try to show one full row below. */
    const int delta_y = te->ys - int(v2d->cur.ymin + UI_UNIT_Y);
    outliner_scroll_view(space_outliner, region, delta_y);
  }
  else if (te->ys > int(v2d->cur.ymax - (UI_UNIT_Y * 2.0f))) {
    /* Try to show one full row above. */
    const int delta_y = te->ys - int(v2d->cur.ymax - (UI_UNIT_Y * 2.0f));
    outliner_scroll_view(space_outliner, region, delta_y);
  }

  do_item_rename(region, te, TREESTORE(te), op->reports);

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_item_rename(wmOperatorType *ot)
{
  PropertyRNA *prop;

  ot->name = "Rename";
  ot->idname = "OUTLINER_OT_item_rename";
  ot->description = "Rename the active element";

  ot->invoke = outliner_item_rename_invoke;

  ot->poll = ED_operator_region_outliner_active;

  /* Flags. No undo, since this operator only activate the name editing text field in the Outliner,
   * but does not actually change anything. */
  ot->flag = OPTYPE_REGISTER;

  prop = RNA_def_boolean(ot->srna,
                         "use_active",
                         false,
                         "Use Active",
                         "Rename the active item, rather than the one the mouse is over");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name ID Delete Operator
 * \{ */

/**
 * Helper struct to handle Scene deletion.
 *
 * In case the scene to be deleted is active in the context/WM/etc., some valid replacement scene
 * must be found, and some additional extra processing must be done before the actual deletion of
 * the scene.
 */
struct SceneReplaceData {
  Scene *scene_to_delete = nullptr;
  Scene *scene_to_activate = nullptr;

  /** Return the scene currently expected to become the active scene. */
  Scene *active_scene_get(bContext *C)
  {
    return scene_to_activate ? scene_to_activate : CTX_data_scene(C);
  }

  /** Return `true` if the current data allows the active scene replacement. */
  bool can_replace() const
  {
    return (scene_to_delete && scene_to_activate);
  }

  /** Check if the current scene replacement data is fully valid */
  bool is_valid() const
  {
    /* Both pointers should be either null, or non-null (in which case they should also not be the
     * same, and the replacement scene should not be tagged for deletion).
     *
     * Otherwise, the scene to be deleted does not have a valid replacement, and cannot be deleted.
     */
    return ((!scene_to_delete && !scene_to_activate) ||
            (can_replace() && scene_to_delete != scene_to_activate &&
             (scene_to_activate->id.tag & ID_TAG_DOIT) == 0));
  }
};

static bool id_delete_tag(bContext *C,
                          ReportList *reports,
                          TreeElement *te,
                          TreeStoreElem *tselem,
                          SceneReplaceData &scene_replace_data)
{
  Main *bmain = CTX_data_main(C);
  ID *id = tselem->id;

  BLI_assert(id != nullptr);
  BLI_assert(((tselem->type == TSE_SOME_ID) && (te->idcode != 0)) ||
             (tselem->type == TSE_LAYER_COLLECTION));
  UNUSED_VARS_NDEBUG(te);

  if (ID_IS_OVERRIDE_LIBRARY(id)) {
    if (!ID_IS_OVERRIDE_LIBRARY_REAL(id) ||
        (id->override_library->flag & LIBOVERRIDE_FLAG_NO_HIERARCHY) == 0)
    {
      BKE_reportf(reports,
                  RPT_WARNING,
                  "Cannot delete library override id '%s', it is part of an override hierarchy",
                  id->name);
      return false;
    }
  }

  /* Current active scene, and the one to use to replace it, in case the former is to be deleted.
   */
  Scene *scene_curr = nullptr;
  Scene *scene_new = nullptr;

  if (te->idcode == ID_LI) {
    /* Get the scene currently expected to become the active scene. */
    scene_curr = scene_replace_data.active_scene_get(C);
    Library *lib = blender::id_cast<Library *>(id);
    if (lib->runtime->parent != nullptr) {
      BKE_reportf(reports, RPT_WARNING, "Cannot delete indirectly linked library '%s'", id->name);
      return false;
    }
    blender::Set<Library *> libraries{lib};
    libraries.add_multiple_new(lib->runtime->archived_libraries.as_span());
    BLI_assert(!libraries.contains(nullptr));
    if (libraries.contains(scene_curr->id.lib)) {
      scene_new = BKE_scene_find_replacement(
          *bmain, *scene_curr, [&libraries](const Scene &scene) -> bool {
            return (
                /* The candidate scene must belong to a different set of libraries (owner library
                 * and all of its archive ones). */
                !libraries.contains(scene.id.lib) &&
                /* The candidate scene must not be tagged for deletion. */
                (scene.id.tag & ID_TAG_DOIT) == 0 &&
                /* The candidate scene must be locale, or its library must not be tagged for
                 * deletion. */
                (!scene.id.lib || (scene.id.lib->id.tag & ID_TAG_DOIT) == 0));
          });
      if (!scene_new) {
        BKE_reportf(reports,
                    RPT_WARNING,
                    "Cannot find a scene to replace the active one, which belongs to the to be "
                    "deleted library '%s'",
                    id->name);
        return false;
      }
    }
  }
  if (id->tag & ID_TAG_INDIRECT) {
    BKE_reportf(reports, RPT_WARNING, "Cannot delete indirectly linked id '%s'", id->name);
    return false;
  }
  if (ID_REAL_USERS(id) <= 1 && BKE_library_ID_is_indirectly_used(bmain, id)) {
    BKE_reportf(reports,
                RPT_WARNING,
                "Cannot delete id '%s', indirectly used data-blocks need at least one user",
                id->name);
    return false;
  }
  if (te->idcode == ID_WS) {
    BKE_workspace_id_tag_all_visible(bmain, ID_TAG_PRE_EXISTING);
    if (id->tag & ID_TAG_PRE_EXISTING) {
      BKE_reportf(
          reports, RPT_WARNING, "Cannot delete currently visible workspace id '%s'", id->name);
      BKE_main_id_tag_idcode(bmain, ID_WS, ID_TAG_PRE_EXISTING, false);
      return false;
    }
    BKE_main_id_tag_idcode(bmain, ID_WS, ID_TAG_PRE_EXISTING, false);
  }
  else if (te->idcode == ID_SCE) {
    /* Get the scene currently expected to become the active scene. */
    scene_curr = scene_replace_data.active_scene_get(C);
    if (&scene_curr->id == id) {
      scene_new = BKE_scene_find_replacement(*bmain, *scene_curr, [](const Scene &scene) -> bool {
        return (
            /* The candidate scene must not be tagged for deletion. */
            (scene.id.tag & ID_TAG_DOIT) == 0 &&
            /* The candidate scene must be locale, or its library must not be tagged for
             * deletion. */
            (!scene.id.lib || (scene.id.lib->id.tag & ID_TAG_DOIT) == 0));
      });
      if (!scene_new) {
        BKE_reportf(reports,
                    RPT_WARNING,
                    "Cannot find a scene to replace the active deleted one '%s'",
                    id->name);
        return false;
      }
    }
  }

  id->tag |= ID_TAG_DOIT;
  if (scene_curr && scene_new) {
    BLI_assert(scene_curr != scene_new);
    BLI_assert((scene_new->id.tag & ID_TAG_DOIT) == 0);
    if (!scene_replace_data.scene_to_delete) {
      scene_replace_data.scene_to_delete = scene_curr;
    }
    else {
      BLI_assert(scene_replace_data.scene_to_delete == CTX_data_scene(C));
    }
    scene_replace_data.scene_to_activate = scene_new;
  }

  WM_event_add_notifier(C, NC_WINDOW, nullptr);

  return true;
}

void id_delete_tag_fn(bContext *C,
                      ReportList *reports,
                      Scene * /*scene*/,
                      TreeElement *te,
                      TreeStoreElem * /*tsep*/,
                      TreeStoreElem *tselem)
{
  SceneReplaceData scene_replace_data;
  id_delete_tag(C, reports, te, tselem, scene_replace_data);

  BLI_assert(scene_replace_data.is_valid());
  if (scene_replace_data.can_replace()) {
    ED_scene_replace_active_for_deletion(*C,
                                         *CTX_data_main(C),
                                         *scene_replace_data.scene_to_delete,
                                         scene_replace_data.scene_to_activate);
  }
}

static int outliner_id_delete_tag(bContext *C,
                                  ReportList *reports,
                                  TreeElement *te,
                                  const float mval[2],
                                  SceneReplaceData &scene_replace_data)
{
  int id_tagged_num = 0;

  if (mval[1] > te->ys && mval[1] < te->ys + UI_UNIT_Y) {
    TreeStoreElem *tselem = TREESTORE(te);

    if (te->idcode != 0 && tselem->id) {
      if (id_delete_tag(C, reports, te, tselem, scene_replace_data)) {
        id_tagged_num++;
      }
    }
  }
  else {
    LISTBASE_FOREACH (TreeElement *, te_sub, &te->subtree) {
      if ((id_tagged_num += outliner_id_delete_tag(
               C, reports, te_sub, mval, scene_replace_data)) != 0)
      {
        break;
      }
    }
  }

  return id_tagged_num;
}

static wmOperatorStatus outliner_id_delete_invoke(bContext *C,
                                                  wmOperator *op,
                                                  const wmEvent *event)
{
  Main *bmain = CTX_data_main(C);
  ARegion *region = CTX_wm_region(C);
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  float fmval[2];

  BLI_assert(region && space_outliner);

  UI_view2d_region_to_view(&region->v2d, event->mval[0], event->mval[1], &fmval[0], &fmval[1]);

  SceneReplaceData scene_replace_data;

  int id_tagged_num = 0;
  BKE_main_id_tag_all(bmain, ID_TAG_DOIT, false);
  LISTBASE_FOREACH (TreeElement *, te, &space_outliner->tree) {
    if ((id_tagged_num += outliner_id_delete_tag(C, op->reports, te, fmval, scene_replace_data)) !=
        0)
    {
      break;
    }
  }
  if (id_tagged_num == 0) {
    BKE_main_id_tag_all(bmain, ID_TAG_DOIT, false);
    return OPERATOR_CANCELLED;
  }

  BLI_assert(scene_replace_data.is_valid());
  if (scene_replace_data.can_replace()) {
    ED_scene_replace_active_for_deletion(
        *C, *bmain, *scene_replace_data.scene_to_delete, scene_replace_data.scene_to_activate);
  }

  BKE_id_multi_tagged_delete(bmain);
  BKE_main_id_tag_all(bmain, ID_TAG_DOIT, false);
  return OPERATOR_FINISHED;
}

void OUTLINER_OT_id_delete(wmOperatorType *ot)
{
  ot->name = "Delete Data-Block";
  ot->idname = "OUTLINER_OT_id_delete";
  ot->description = "Delete the ID under cursor";

  ot->invoke = outliner_id_delete_invoke;
  ot->poll = ED_operator_region_outliner_active;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name ID Remap Operator
 * \{ */

static wmOperatorStatus outliner_id_remap_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);

  const short id_type = short(RNA_enum_get(op->ptr, "id_type"));
  ID *old_id = static_cast<ID *>(
      BLI_findlink(which_libbase(CTX_data_main(C), id_type), RNA_enum_get(op->ptr, "old_id")));
  ID *new_id = static_cast<ID *>(
      BLI_findlink(which_libbase(CTX_data_main(C), id_type), RNA_enum_get(op->ptr, "new_id")));

  /* check for invalid states */
  if (space_outliner == nullptr) {
    return OPERATOR_CANCELLED;
  }

  if (!(old_id && new_id && (old_id != new_id) && (GS(old_id->name) == GS(new_id->name)))) {
    BKE_reportf(op->reports,
                RPT_ERROR_INVALID_INPUT,
                "Invalid old/new ID pair ('%s' / '%s')",
                old_id ? old_id->name : "Invalid ID",
                new_id ? new_id->name : "Invalid ID");
    return OPERATOR_CANCELLED;
  }

  if (!ID_IS_EDITABLE(old_id)) {
    BKE_reportf(op->reports,
                RPT_WARNING,
                "Old ID '%s' is linked from a library, indirect usages of this data-block will "
                "not be remapped",
                old_id->name);
  }

  BKE_libblock_remap(
      bmain, old_id, new_id, ID_REMAP_SKIP_INDIRECT_USAGE | ID_REMAP_SKIP_NEVER_NULL_USAGE);

  BKE_main_lib_objects_recalc_all(bmain);

  /* recreate dependency graph to include new objects */
  DEG_relations_tag_update(bmain);

  /* Free gpu materials, some materials depend on existing objects,
   * such as lights so freeing correctly refreshes. */
  GPU_materials_free(bmain);

  WM_event_add_notifier(C, NC_WINDOW, nullptr);

  return OPERATOR_FINISHED;
}

static bool outliner_id_remap_find_tree_element(bContext *C,
                                                wmOperator *op,
                                                ListBase *tree,
                                                const float y)
{
  LISTBASE_FOREACH (TreeElement *, te, tree) {
    if (y > te->ys && y < te->ys + UI_UNIT_Y) {
      TreeStoreElem *tselem = TREESTORE(te);

      if ((tselem->type == TSE_SOME_ID) && tselem->id) {
        RNA_enum_set(op->ptr, "id_type", GS(tselem->id->name));
        RNA_enum_set_identifier(C, op->ptr, "new_id", tselem->id->name + 2);
        RNA_enum_set_identifier(C, op->ptr, "old_id", tselem->id->name + 2);
        return true;
      }
    }
    if (outliner_id_remap_find_tree_element(C, op, &te->subtree, y)) {
      return true;
    }
  }
  return false;
}

static wmOperatorStatus outliner_id_remap_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  ARegion *region = CTX_wm_region(C);
  float fmval[2];

  if (!RNA_property_is_set(op->ptr, RNA_struct_find_property(op->ptr, "id_type"))) {
    UI_view2d_region_to_view(&region->v2d, event->mval[0], event->mval[1], &fmval[0], &fmval[1]);

    outliner_id_remap_find_tree_element(C, op, &space_outliner->tree, fmval[1]);
  }

  return WM_operator_props_dialog_popup(C, op, 400, IFACE_("Remap Data ID"), IFACE_("Remap"));
}

static const EnumPropertyItem *outliner_id_itemf(bContext *C,
                                                 PointerRNA *ptr,
                                                 PropertyRNA * /*prop*/,
                                                 bool *r_free)
{
  if (C == nullptr) {
    return rna_enum_dummy_NULL_items;
  }

  EnumPropertyItem item_tmp = {0}, *item = nullptr;
  int totitem = 0;
  int i = 0;

  short id_type = short(RNA_enum_get(ptr, "id_type"));
  ID *id = static_cast<ID *>(which_libbase(CTX_data_main(C), id_type)->first);

  for (; id; id = static_cast<ID *>(id->next)) {
    item_tmp.identifier = item_tmp.name = id->name + 2;
    item_tmp.value = i++;
    RNA_enum_item_add(&item, &totitem, &item_tmp);
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

void OUTLINER_OT_id_remap(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Outliner ID Data Remap";
  ot->idname = "OUTLINER_OT_id_remap";

  /* callbacks */
  ot->invoke = outliner_id_remap_invoke;
  ot->exec = outliner_id_remap_exec;
  ot->poll = ED_operator_region_outliner_active;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  prop = RNA_def_enum(ot->srna, "id_type", rna_enum_id_type_items, ID_OB, "ID Type", "");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_ID);
  /* Changing ID type wont make sense, would return early with "Invalid old/new ID pair" anyways.
   */
  RNA_def_property_flag(prop, PROP_HIDDEN);

  prop = RNA_def_enum(
      ot->srna, "old_id", rna_enum_dummy_NULL_items, 0, "Old ID", "Old ID to replace");
  RNA_def_property_enum_funcs_runtime(prop, nullptr, nullptr, outliner_id_itemf, nullptr, nullptr);
  RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE | PROP_HIDDEN);

  ot->prop = RNA_def_enum(ot->srna,
                          "new_id",
                          rna_enum_dummy_NULL_items,
                          0,
                          "New ID",
                          "New ID to remap all selected IDs' users to");
  RNA_def_property_enum_funcs_runtime(
      ot->prop, nullptr, nullptr, outliner_id_itemf, nullptr, nullptr);
  RNA_def_property_flag(ot->prop, PROP_ENUM_NO_TRANSLATE);
}

void id_remap_fn(bContext *C,
                 ReportList * /*reports*/,
                 Scene * /*scene*/,
                 TreeElement * /*te*/,
                 TreeStoreElem * /*tsep*/,
                 TreeStoreElem *tselem)
{
  wmOperatorType *ot = WM_operatortype_find("OUTLINER_OT_id_remap", false);
  PointerRNA op_props;

  BLI_assert(tselem->id != nullptr);

  WM_operator_properties_create_ptr(&op_props, ot);

  RNA_enum_set(&op_props, "id_type", GS(tselem->id->name));
  RNA_enum_set_identifier(C, &op_props, "old_id", tselem->id->name + 2);

  WM_operator_name_call_ptr(C, ot, wm::OpCallContext::InvokeDefault, &op_props, nullptr);

  WM_operator_properties_free(&op_props);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name ID Copy Operator
 * \{ */

static int outliner_id_copy_tag(SpaceOutliner *space_outliner,
                                ListBase *tree,
                                blender::bke::blendfile::PartialWriteContext &copybuffer,
                                ReportList *reports)
{
  using namespace blender::bke::blendfile;

  int num_ids = 0;

  LISTBASE_FOREACH (TreeElement *, te, tree) {
    TreeStoreElem *tselem = TREESTORE(te);

    /* Add selected item and all of its dependencies to the copy buffer. */
    if (tselem->flag & TSE_SELECTED && ELEM(tselem->type, TSE_SOME_ID, TSE_LAYER_COLLECTION)) {
      if (ID_IS_PACKED(tselem->id)) {
        /* Direct link/append of packed IDs is not supported currently, so neither is their
         * copy/pasting. */
        continue;
      }
      const IDTypeInfo *id_type = BKE_idtype_get_info_from_id(tselem->id);
      if (id_type->flags & (IDTYPE_FLAGS_NO_COPY | IDTYPE_FLAGS_NO_LIBLINKING)) {
        BKE_reportf(reports,
                    RPT_INFO,
                    "Copying ID '%s' is not possible, '%s' type of data-blocks is not supported",
                    tselem->id->name,
                    id_type->name);
      }
      if (copybuffer.id_add(tselem->id,
                            PartialWriteContext::IDAddOptions{
                                (PartialWriteContext::IDAddOperations::SET_FAKE_USER |
                                 PartialWriteContext::IDAddOperations::SET_CLIPBOARD_MARK |
                                 PartialWriteContext::IDAddOperations::ADD_DEPENDENCIES)},
                            nullptr))
      {
        num_ids++;
      }
    }

    /* go over sub-tree */
    num_ids += outliner_id_copy_tag(space_outliner, &te->subtree, copybuffer, reports);
  }

  return num_ids;
}

static wmOperatorStatus outliner_id_copy_exec(bContext *C, wmOperator *op)
{
  using namespace blender::bke::blendfile;

  Main *bmain = CTX_data_main(C);
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  PartialWriteContext copybuffer{*bmain};

  const int num_ids = outliner_id_copy_tag(
      space_outliner, &space_outliner->tree, copybuffer, op->reports);
  if (num_ids == 0) {
    BKE_report(op->reports, RPT_INFO, "No selected data-blocks to copy");
    return OPERATOR_CANCELLED;
  }

  char filepath[FILE_MAX];
  outliner_copybuffer_filepath_get(filepath, sizeof(filepath));
  copybuffer.write(filepath, *op->reports);

  BKE_reportf(op->reports, RPT_INFO, "Copied %d selected data-block(s)", num_ids);

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_id_copy(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Outliner ID Data Copy";
  ot->idname = "OUTLINER_OT_id_copy";
  ot->description = "Copy the selected data-blocks to the internal clipboard";

  /* callbacks */
  ot->exec = outliner_id_copy_exec;
  ot->poll = ED_operator_outliner_active;

  /* Flags, don't need any undo here (this operator does not change anything in Blender data). */
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name ID Paste Operator
 * \{ */

static wmOperatorStatus outliner_id_paste_exec(bContext *C, wmOperator *op)
{
  char filepath[FILE_MAX];
  const short flag = FILE_AUTOSELECT | FILE_ACTIVE_COLLECTION;

  outliner_copybuffer_filepath_get(filepath, sizeof(filepath));

  const int num_pasted = BKE_copybuffer_paste(C, filepath, flag, op->reports, 0);
  if (num_pasted == 0) {
    BKE_report(op->reports, RPT_INFO, "No data to paste");
    return OPERATOR_CANCELLED;
  }

  WM_event_add_notifier(C, NC_WINDOW, nullptr);

  BKE_reportf(op->reports, RPT_INFO, "%d data-block(s) pasted", num_pasted);

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_id_paste(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Outliner ID Data Paste";
  ot->idname = "OUTLINER_OT_id_paste";
  ot->description = "Paste data-blocks from the internal clipboard";

  /* callbacks */
  ot->exec = outliner_id_paste_exec;
  ot->poll = ED_operator_outliner_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Linked ID Relocate Operator
 * \{ */

static wmOperatorStatus outliner_id_relocate_invoke(bContext *C,
                                                    wmOperator *op,
                                                    const wmEvent * /*event*/)
{
  PointerRNA id_linked_ptr = CTX_data_pointer_get_type(C, "id", &RNA_ID);
  ID *id_linked = static_cast<ID *>(id_linked_ptr.data);

  if (!id_linked) {
    BKE_report(op->reports, RPT_ERROR_INVALID_INPUT, "There is no active data-block");
    return OPERATOR_CANCELLED;
  }
  if (!ID_IS_LINKED(id_linked) || !BKE_idtype_idcode_is_linkable(GS(id_linked->name))) {
    BKE_reportf(op->reports,
                RPT_ERROR_INVALID_INPUT,
                "The active data-block '%s' is not a valid linked one",
                BKE_id_name(*id_linked));
    return OPERATOR_CANCELLED;
  }
  if (BKE_library_ID_is_indirectly_used(CTX_data_main(C), id_linked)) {
    BKE_reportf(op->reports,
                RPT_ERROR_INVALID_INPUT,
                "The active data-block '%s' is used by other linked data",
                BKE_id_name(*id_linked));
    return OPERATOR_CANCELLED;
  }

  wmOperatorType *ot = WM_operatortype_find("WM_OT_id_linked_relocate", false);
  PointerRNA op_props;

  WM_operator_properties_create_ptr(&op_props, ot);
  RNA_int_set(&op_props, "id_session_uid", *reinterpret_cast<int *>(&id_linked->session_uid));

  const wmOperatorStatus ret = WM_operator_name_call_ptr(
      C, ot, wm::OpCallContext::InvokeDefault, &op_props, nullptr);

  WM_operator_properties_free(&op_props);

  /* If the matching WM operator invoke was successful, it was added to modal handlers. This
   * operator however is _not_ modal, and will leak memory if it returns this status. */
  return (ret == OPERATOR_RUNNING_MODAL) ? OPERATOR_FINISHED : ret;
}

void OUTLINER_OT_id_linked_relocate(wmOperatorType *ot)
{
  ot->name = "Relocate Linked ID";
  ot->idname = "OUTLINER_OT_id_linked_relocate";
  ot->description =
      "Replace the active linked ID (and its dependencies if any) by another one, from the same "
      "or a different library";

  ot->invoke = outliner_id_relocate_invoke;
  ot->poll = ED_operator_region_outliner_active;

  /* Flags. No undo, no registering, all the actual work/changes is done by the matching WM
   * operator. */
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Library Relocate Operator
 * \{ */

static wmOperatorStatus lib_relocate(
    bContext *C, TreeElement *te, TreeStoreElem *tselem, wmOperatorType *ot, const bool reload)
{
  PointerRNA op_props;
  wmOperatorStatus ret = wmOperatorStatus(0);

  BLI_assert(te->idcode == ID_LI && tselem->id != nullptr);
  UNUSED_VARS_NDEBUG(te);

  WM_operator_properties_create_ptr(&op_props, ot);

  RNA_string_set(&op_props, "library", tselem->id->name + 2);

  if (reload) {
    Library *lib = (Library *)tselem->id;
    char dir[FILE_MAXDIR], filename[FILE_MAX];

    BLI_path_split_dir_file(
        lib->runtime->filepath_abs, dir, sizeof(dir), filename, sizeof(filename));

    printf("%s, %s\n", tselem->id->name, lib->runtime->filepath_abs);

    /* We assume if both paths in lib are not the same then `lib->filepath` was relative. */
    RNA_boolean_set(
        &op_props, "relative_path", BLI_path_cmp(lib->runtime->filepath_abs, lib->filepath) != 0);

    RNA_string_set(&op_props, "directory", dir);
    RNA_string_set(&op_props, "filename", filename);

    ret = WM_operator_name_call_ptr(C, ot, wm::OpCallContext::ExecDefault, &op_props, nullptr);
  }
  else {
    ret = WM_operator_name_call_ptr(C, ot, wm::OpCallContext::InvokeDefault, &op_props, nullptr);
  }

  WM_operator_properties_free(&op_props);

  return ret;
}

static wmOperatorStatus outliner_lib_relocate_invoke_do(
    bContext *C, ReportList *reports, TreeElement *te, const float mval[2], const bool reload)
{
  if (mval[1] > te->ys && mval[1] < te->ys + UI_UNIT_Y) {
    TreeStoreElem *tselem = TREESTORE(te);

    if (te->idcode == ID_LI && tselem->id) {
      if (((Library *)tselem->id)->runtime->parent && !reload) {
        BKE_reportf(reports,
                    RPT_ERROR_INVALID_INPUT,
                    "Cannot relocate indirectly linked library '%s'",
                    ((Library *)tselem->id)->runtime->filepath_abs);
        return OPERATOR_CANCELLED;
      }

      wmOperatorType *ot = WM_operatortype_find(reload ? "WM_OT_lib_reload" : "WM_OT_lib_relocate",
                                                false);
      return lib_relocate(C, te, tselem, ot, reload);
    }
  }
  else {
    LISTBASE_FOREACH (TreeElement *, te_sub, &te->subtree) {
      wmOperatorStatus ret;
      if ((ret = outliner_lib_relocate_invoke_do(C, reports, te_sub, mval, reload))) {
        return ret;
      }
    }
  }

  return wmOperatorStatus(0);
}

static wmOperatorStatus outliner_lib_relocate_invoke(bContext *C,
                                                     wmOperator *op,
                                                     const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  float fmval[2];

  BLI_assert(region && space_outliner);

  UI_view2d_region_to_view(&region->v2d, event->mval[0], event->mval[1], &fmval[0], &fmval[1]);

  LISTBASE_FOREACH (TreeElement *, te, &space_outliner->tree) {
    wmOperatorStatus ret;

    if ((ret = outliner_lib_relocate_invoke_do(C, op->reports, te, fmval, false))) {
      return ret;
    }
  }

  return OPERATOR_CANCELLED;
}

void OUTLINER_OT_lib_relocate(wmOperatorType *ot)
{
  ot->name = "Relocate Library";
  ot->idname = "OUTLINER_OT_lib_relocate";
  ot->description = "Relocate the library under cursor";

  ot->invoke = outliner_lib_relocate_invoke;
  ot->poll = ED_operator_region_outliner_active;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void lib_relocate_fn(bContext *C,
                     ReportList * /*reports*/,
                     Scene * /*scene*/,
                     TreeElement *te,
                     TreeStoreElem * /*tsep*/,
                     TreeStoreElem *tselem)
{
  /* XXX: This does not work with several items
   * (it is only called once in the end, due to the 'deferred'
   * file-browser invocation through event system...). */

  wmOperatorType *ot = WM_operatortype_find("WM_OT_lib_relocate", false);

  lib_relocate(C, te, tselem, ot, false);
}

static wmOperatorStatus outliner_lib_reload_invoke(bContext *C,
                                                   wmOperator *op,
                                                   const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  float fmval[2];

  BLI_assert(region && space_outliner);

  UI_view2d_region_to_view(&region->v2d, event->mval[0], event->mval[1], &fmval[0], &fmval[1]);

  LISTBASE_FOREACH (TreeElement *, te, &space_outliner->tree) {
    wmOperatorStatus ret;

    if ((ret = outliner_lib_relocate_invoke_do(C, op->reports, te, fmval, true))) {
      return ret;
    }
  }

  return OPERATOR_CANCELLED;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Library Reload Operator
 * \{ */

void OUTLINER_OT_lib_reload(wmOperatorType *ot)
{
  ot->name = "Reload Library";
  ot->idname = "OUTLINER_OT_lib_reload";
  ot->description = "Reload the library under cursor";

  ot->invoke = outliner_lib_reload_invoke;
  ot->poll = ED_operator_region_outliner_active;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void lib_reload_fn(bContext *C,
                   ReportList * /*reports*/,
                   Scene * /*scene*/,
                   TreeElement *te,
                   TreeStoreElem * /*tsep*/,
                   TreeStoreElem *tselem)
{
  wmOperatorType *ot = WM_operatortype_find("WM_OT_lib_reload", false);

  lib_relocate(C, te, tselem, ot, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Apply Settings Utilities
 * \{ */

static int outliner_count_levels(ListBase *lb, const int curlevel)
{
  int level = curlevel;

  LISTBASE_FOREACH (TreeElement *, te, lb) {
    int lev = outliner_count_levels(&te->subtree, curlevel + 1);
    level = std::max(lev, level);
  }
  return level;
}

int outliner_flag_is_any_test(ListBase *lb, short flag, const int curlevel)
{
  LISTBASE_FOREACH (TreeElement *, te, lb) {
    TreeStoreElem *tselem = TREESTORE(te);
    if (tselem->flag & flag) {
      return curlevel;
    }

    int level = outliner_flag_is_any_test(&te->subtree, flag, curlevel + 1);
    if (level) {
      return level;
    }
  }
  return 0;
}

bool outliner_flag_set(const SpaceOutliner &space_outliner, const short flag, const short set)
{
  return outliner_flag_set(space_outliner.tree, flag, set);
}

bool outliner_flag_set(const ListBase &lb, const short flag, const short set)
{
  bool changed = false;

  tree_iterator::all(lb, [&](TreeElement *te) {
    TreeStoreElem *tselem = TREESTORE(te);
    bool has_flag = (tselem->flag & flag);
    if (set == 0) {
      if (has_flag) {
        tselem->flag &= ~flag;
        changed = true;
      }
    }
    else if (!has_flag) {
      tselem->flag |= flag;
      changed = true;
    }
  });

  return changed;
}

bool outliner_flag_flip(const SpaceOutliner &space_outliner, const short flag)
{
  return outliner_flag_flip(space_outliner.tree, flag);
}

bool outliner_flag_flip(const ListBase &lb, const short flag)
{
  bool changed = false;

  tree_iterator::all(lb, [&](TreeElement *te) {
    TreeStoreElem *tselem = TREESTORE(te);
    tselem->flag ^= flag;
  });

  return changed;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Toggle Expanded (Outliner) Operator
 * \{ */

static wmOperatorStatus outliner_toggle_expanded_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  ARegion *region = CTX_wm_region(C);

  if (outliner_flag_is_any_test(&space_outliner->tree, TSE_CLOSED, 1)) {
    outliner_flag_set(*space_outliner, TSE_CLOSED, 0);
  }
  else {
    outliner_flag_set(*space_outliner, TSE_CLOSED, 1);
  }

  ED_region_tag_redraw(region);

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_expanded_toggle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Expand/Collapse All";
  ot->idname = "OUTLINER_OT_expanded_toggle";
  ot->description = "Expand/Collapse all items";

  /* callbacks */
  ot->exec = outliner_toggle_expanded_exec;
  ot->poll = ED_operator_region_outliner_active;

  /* no undo or registry, UI option */
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Toggle Selected (Outliner) Operator
 * \{ */

static wmOperatorStatus outliner_select_all_exec(bContext *C, wmOperator *op)
{
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  ARegion *region = CTX_wm_region(C);
  Scene *scene = CTX_data_scene(C);
  int action = RNA_enum_get(op->ptr, "action");
  if (action == SEL_TOGGLE) {
    action = outliner_flag_is_any_test(&space_outliner->tree, TSE_SELECTED, 1) ? SEL_DESELECT :
                                                                                 SEL_SELECT;
  }

  switch (action) {
    case SEL_SELECT:
      outliner_flag_set(*space_outliner, TSE_SELECTED, 1);
      break;
    case SEL_DESELECT:
      outliner_flag_set(*space_outliner, TSE_SELECTED, 0);
      break;
    case SEL_INVERT:
      outliner_flag_flip(*space_outliner, TSE_SELECTED);
      break;
  }

  ED_outliner_select_sync_from_outliner(C, space_outliner);

  DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
  ED_region_tag_redraw_no_rebuild(region);

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_select_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Toggle Selected";
  ot->idname = "OUTLINER_OT_select_all";
  ot->description = "Toggle the Outliner selection of items";

  /* callbacks */
  ot->exec = outliner_select_all_exec;
  ot->poll = ED_operator_outliner_active;

  /* no undo or registry */

  /* rna */
  WM_operator_properties_select_all(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Start / Clear Search Filter Operators
 * \{ */

static wmOperatorStatus outliner_start_filter_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = BKE_area_find_region_type(area, RGN_TYPE_HEADER);
  UI_textbutton_activate_rna(C, region, space_outliner, "filter_text");

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_start_filter(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Filter";
  ot->description = "Start entering filter text";
  ot->idname = "OUTLINER_OT_start_filter";

  /* Callbacks. */
  ot->exec = outliner_start_filter_exec;
  ot->poll = ED_operator_outliner_active;
}

static wmOperatorStatus outliner_clear_filter_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  space_outliner->search_string[0] = '\0';
  ED_area_tag_redraw(CTX_wm_area(C));
  return OPERATOR_FINISHED;
}

void OUTLINER_OT_clear_filter(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Clear Filter";
  ot->description = "Clear the search filter";
  ot->idname = "OUTLINER_OT_clear_filter";

  /* Callbacks. */
  ot->exec = outliner_clear_filter_exec;
  ot->poll = ED_operator_outliner_active;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Show Active (Outliner) Operator
 * \{ */

void outliner_set_coordinates(const ARegion *region, const SpaceOutliner *space_outliner)
{
  int starty = int(region->v2d.tot.ymax) - UI_UNIT_Y;

  tree_iterator::all_open(*space_outliner, [&](TreeElement *te) {
    /* store coord and continue, we need coordinates for elements outside view too */
    te->xs = 0;
    te->ys = float(starty);
    starty -= UI_UNIT_Y;
  });
}

/** Return true when levels were opened. */
static bool outliner_open_back(TreeElement *te)
{
  TreeStoreElem *tselem;
  bool retval = false;

  for (te = te->parent; te; te = te->parent) {
    tselem = TREESTORE(te);
    if (tselem->flag & TSE_CLOSED) {
      tselem->flag &= ~TSE_CLOSED;
      retval = true;
    }
  }
  return retval;
}

/**
 * \return element representing the active base or bone in the outliner, or null if none exists
 */
static TreeElement *outliner_show_active_get_element(bContext *C,
                                                     SpaceOutliner *space_outliner,
                                                     const Scene *scene,
                                                     ViewLayer *view_layer)
{
  TreeElement *te;

  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *obact = BKE_view_layer_active_object_get(view_layer);

  if (!obact) {
    return nullptr;
  }

  te = outliner_find_id(space_outliner, &space_outliner->tree, &obact->id);

  if (te != nullptr && obact->type == OB_ARMATURE) {
    /* traverse down the bone hierarchy in case of armature */
    TreeElement *te_obact = te;

    if (obact->mode & OB_MODE_POSE) {
      Object *obpose = BKE_object_pose_armature_get(obact);
      bPoseChannel *pchan = BKE_pose_channel_active(obpose, false);
      if (pchan) {
        te = outliner_find_posechannel(&te_obact->subtree, pchan);
      }
    }
    else if (obact->mode & OB_MODE_EDIT) {
      EditBone *ebone = CTX_data_active_bone(C);
      if (ebone) {
        te = outliner_find_editbone(&te_obact->subtree, ebone);
      }
    }
  }

  return te;
}

static void outliner_show_active(SpaceOutliner *space_outliner,
                                 ARegion *region,
                                 TreeElement *te,
                                 ID *id)
{
  /* open up tree to active object/bone */
  if (TREESTORE(te)->id == id) {
    if (outliner_open_back(te)) {
      outliner_set_coordinates(region, space_outliner);
    }
    return;
  }

  LISTBASE_FOREACH (TreeElement *, ten, &te->subtree) {
    outliner_show_active(space_outliner, region, ten, id);
  }
}

static wmOperatorStatus outliner_show_active_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  ARegion *region = CTX_wm_region(C);
  View2D *v2d = &region->v2d;

  TreeElement *active_element = outliner_show_active_get_element(
      C, space_outliner, scene, view_layer);

  if (active_element) {
    ID *id = TREESTORE(active_element)->id;

    /* Expand all elements in the outliner with matching ID */
    LISTBASE_FOREACH (TreeElement *, te, &space_outliner->tree) {
      outliner_show_active(space_outliner, region, te, id);
    }

    /* Also open back from the active_element (only done for the first found occurrence of ID
     * though). */
    outliner_show_active(space_outliner, region, active_element, id);

    /* Center view on first element found */
    int size_y = BLI_rcti_size_y(&v2d->mask) + 1;
    int ytop = (active_element->ys + (size_y / 2));
    int delta_y = ytop - v2d->cur.ymax;

    outliner_scroll_view(space_outliner, region, delta_y);
  }
  else {
    return OPERATOR_CANCELLED;
  }

  ED_region_tag_redraw_no_rebuild(region);

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_show_active(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Show Active";
  ot->idname = "OUTLINER_OT_show_active";
  ot->description =
      "Open up the tree and adjust the view so that the active object is shown centered";

  /* callbacks */
  ot->exec = outliner_show_active_exec;
  ot->poll = ED_operator_region_outliner_active;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Panning (Outliner) Operator
 * \{ */

static wmOperatorStatus outliner_scroll_page_exec(bContext *C, wmOperator *op)
{
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  ARegion *region = CTX_wm_region(C);
  int size_y = BLI_rcti_size_y(&region->v2d.mask) + 1;

  bool up = RNA_boolean_get(op->ptr, "up");

  if (!up) {
    size_y = -size_y;
  }

  outliner_scroll_view(space_outliner, region, size_y);

  ED_region_tag_redraw_no_rebuild(region);

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_scroll_page(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Scroll Page";
  ot->idname = "OUTLINER_OT_scroll_page";
  ot->description = "Scroll page up or down";

  /* callbacks */
  ot->exec = outliner_scroll_page_exec;
  ot->poll = ED_operator_region_outliner_active;

  /* properties */
  prop = RNA_def_boolean(ot->srna, "up", false, "Up", "Scroll up one page");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Show One Level Operator
 * \{ */

/* helper function for Show/Hide one level operator */
static void outliner_openclose_level(ListBase *lb, int curlevel, int level, int open)
{
  LISTBASE_FOREACH (TreeElement *, te, lb) {
    TreeStoreElem *tselem = TREESTORE(te);

    if (open) {
      if (curlevel <= level) {
        tselem->flag &= ~TSE_CLOSED;
      }
    }
    else {
      if (curlevel >= level) {
        tselem->flag |= TSE_CLOSED;
      }
    }

    outliner_openclose_level(&te->subtree, curlevel + 1, level, open);
  }
}

static wmOperatorStatus outliner_one_level_exec(bContext *C, wmOperator *op)
{
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  ARegion *region = CTX_wm_region(C);
  const bool add = RNA_boolean_get(op->ptr, "open");
  int level;

  level = outliner_flag_is_any_test(&space_outliner->tree, TSE_CLOSED, 1);
  if (add == 1) {
    if (level) {
      outliner_openclose_level(&space_outliner->tree, 1, level, 1);
    }
  }
  else {
    if (level == 0) {
      level = outliner_count_levels(&space_outliner->tree, 0);
    }
    if (level) {
      outliner_openclose_level(&space_outliner->tree, 1, level - 1, 0);
    }
  }

  ED_region_tag_redraw(region);

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_show_one_level(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Show/Hide One Level";
  ot->idname = "OUTLINER_OT_show_one_level";
  ot->description = "Expand/collapse all entries by one level";

  /* callbacks */
  ot->exec = outliner_one_level_exec;
  ot->poll = ED_operator_region_outliner_active;

  /* no undo or registry, UI option */

  /* properties */
  prop = RNA_def_boolean(ot->srna, "open", true, "Open", "Expand all entries one level deep");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Show Hierarchy Operator
 * \{ */

/**
 * Helper function for #tree_element_shwo_hierarchy() -
 * recursively checks whether subtrees have any objects.
 */
static int subtree_has_objects(ListBase *lb)
{
  LISTBASE_FOREACH (TreeElement *, te, lb) {
    TreeStoreElem *tselem = TREESTORE(te);
    if ((tselem->type == TSE_SOME_ID) && (te->idcode == ID_OB)) {
      return 1;
    }
    if (subtree_has_objects(&te->subtree)) {
      return 1;
    }
  }
  return 0;
}

/* Helper function for Show Hierarchy operator */
static void tree_element_show_hierarchy(Scene *scene, SpaceOutliner *space_outliner)
{
  /* open all object elems, close others */
  tree_iterator::all_open(*space_outliner, [&](TreeElement *te) {
    TreeStoreElem *tselem = TREESTORE(te);

    if (ELEM(tselem->type,
             TSE_SOME_ID,
             TSE_SCENE_OBJECTS_BASE,
             TSE_VIEW_COLLECTION_BASE,
             TSE_LAYER_COLLECTION))
    {
      if (te->idcode == ID_SCE) {
        if (tselem->id != (ID *)scene) {
          tselem->flag |= TSE_CLOSED;
        }
        else {
          tselem->flag &= ~TSE_CLOSED;
        }
      }
      else if (te->idcode == ID_OB) {
        if (subtree_has_objects(&te->subtree)) {
          tselem->flag &= ~TSE_CLOSED;
        }
        else {
          tselem->flag |= TSE_CLOSED;
        }
      }
    }
    else {
      tselem->flag |= TSE_CLOSED;
    }
  });
}

/* show entire object level hierarchy */
static wmOperatorStatus outliner_show_hierarchy_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  ARegion *region = CTX_wm_region(C);
  Scene *scene = CTX_data_scene(C);

  /* recursively open/close levels */
  tree_element_show_hierarchy(scene, space_outliner);

  ED_region_tag_redraw(region);

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_show_hierarchy(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Show Hierarchy";
  ot->idname = "OUTLINER_OT_show_hierarchy";
  ot->description = "Open all object entries and close all others";

  /* callbacks */
  ot->exec = outliner_show_hierarchy_exec;
  /* TODO: shouldn't be allowed in RNA views... */
  ot->poll = ED_operator_region_outliner_active;

  /* no undo or registry, UI option */
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Animation Internal Utilities
 * \{ */

/**
 * Specialized poll callback for these operators to work in data-blocks view only.
 */
static bool ed_operator_outliner_datablocks_active(bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  if ((area) && (area->spacetype == SPACE_OUTLINER)) {
    SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
    return (space_outliner->outlinevis == SO_DATA_API);
  }
  return false;
}

/* Helper func to extract an RNA path from selected tree element
 * NOTE: the caller must zero-out all values of the pointers that it passes here first, as
 * this function does not do that yet
 */
static void tree_element_to_path(TreeElement *te,
                                 TreeStoreElem *tselem,
                                 ID **id,
                                 char **path,
                                 int *array_index,
                                 short *flag,
                                 short * /*groupmode*/)
{
  ListBase hierarchy = {nullptr, nullptr};
  char *newpath = nullptr;

  /* optimize tricks:
   * - Don't do anything if the selected item is a 'struct', but arrays are allowed
   */
  if (tselem->type == TSE_RNA_STRUCT) {
    return;
  }

  /* Overview of Algorithm:
   * 1. Go up the chain of parents until we find the 'root', taking note of the
   *    levels encountered in reverse-order (i.e. items are added to the start of the list
   *    for more convenient looping later)
   * 2. Walk down the chain, adding from the first ID encountered
   *    (which will become the 'ID' for the KeyingSet Path), and build a
   *    path as we step through the chain
   */

  /* step 1: flatten out hierarchy of parents into a flat chain */
  for (TreeElement *tem = te->parent; tem; tem = tem->parent) {
    LinkData *ld = MEM_callocN<LinkData>("LinkData for tree_element_to_path()");
    ld->data = tem;
    BLI_addhead(&hierarchy, ld);
  }

  /* step 2: step down hierarchy building the path
   * (NOTE: addhead in previous loop was needed so that we can loop like this) */
  LISTBASE_FOREACH (LinkData *, ld, &hierarchy) {
    /* get data */
    TreeElement *tem = (TreeElement *)ld->data;
    TreeElementRNACommon *tem_rna = tree_element_cast<TreeElementRNACommon>(tem);
    PointerRNA ptr = tem_rna->get_pointer_rna();

    /* check if we're looking for first ID, or appending to path */
    if (*id) {
      /* just 'append' property to path
       * - to prevent memory leaks, we must write to newpath not path,
       *   then free old path + swap them.
       */
      if (TreeElementRNAProperty *tem_rna_prop = tree_element_cast<TreeElementRNAProperty>(tem)) {
        PropertyRNA *prop = tem_rna_prop->get_property_rna();

        if (RNA_property_type(prop) == PROP_POINTER) {
          /* for pointer we just append property name */
          newpath = RNA_path_append(*path, &ptr, prop, 0, nullptr);
        }
        else if (RNA_property_type(prop) == PROP_COLLECTION) {
          char buf[128], *name;

          TreeElement *temnext = (TreeElement *)(ld->next->data);
          PointerRNA nextptr = tree_element_cast<TreeElementRNACommon>(temnext)->get_pointer_rna();
          name = RNA_struct_name_get_alloc(&nextptr, buf, sizeof(buf), nullptr);

          if (name) {
            /* if possible, use name as a key in the path */
            newpath = RNA_path_append(*path, nullptr, prop, 0, name);

            if (name != buf) {
              MEM_freeN(name);
            }
          }
          else {
            /* otherwise use index */
            int index = 0;

            LISTBASE_FOREACH (TreeElement *, temsub, &tem->subtree) {
              if (temsub == temnext) {
                break;
              }
              index++;
            }
            newpath = RNA_path_append(*path, nullptr, prop, index, nullptr);
          }

          ld = ld->next;
        }
      }

      if (newpath) {
        if (*path) {
          MEM_freeN(*path);
        }
        *path = newpath;
        newpath = nullptr;
      }
    }
    else {
      /* no ID, so check if entry is RNA-struct,
       * and if that RNA-struct is an ID datablock to extract info from. */
      if (tree_element_cast<TreeElementRNAStruct>(tem)) {
        /* ptr->data not ptr->owner_id seems to be the one we want,
         * since ptr->data is sometimes the owner of this ID? */
        if (RNA_struct_is_ID(ptr.type)) {
          *id = static_cast<ID *>(ptr.data);

          /* clear path */
          if (*path) {
            MEM_freeN(*path);
            path = nullptr;
          }
        }
      }
    }
  }

  /* step 3: if we've got an ID, add the current item to the path */
  if (*id) {
    /* add the active property to the path */
    PropertyRNA *prop = tree_element_cast<TreeElementRNACommon>(te)->get_property_rna();

    /* array checks */
    if (tselem->type == TSE_RNA_ARRAY_ELEM) {
      /* item is part of an array, so must set the array_index */
      *array_index = te->index;
    }
    else if (RNA_property_array_check(prop)) {
      /* entire array was selected, so keyframe all */
      *flag |= KSP_FLAG_WHOLE_ARRAY;
    }

    /* path */
    newpath = RNA_path_append(*path, nullptr, prop, 0, nullptr);
    if (*path) {
      MEM_freeN(*path);
    }
    *path = newpath;
  }

  /* free temp data */
  BLI_freelistN(&hierarchy);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Driver Internal Utilities
 * \{ */

/**
 * Driver Operations
 *
 * These operators are only available in data-browser mode for now,
 * as they depend on having RNA paths and/or hierarchies available.
 */
enum {
  DRIVERS_EDITMODE_ADD = 0,
  DRIVERS_EDITMODE_REMOVE,
} /*eDrivers_EditModes*/;

/* Iterate over tree, finding and working on selected items */
static void do_outliner_drivers_editop(SpaceOutliner *space_outliner,
                                       ReportList *reports,
                                       short mode)
{
  tree_iterator::all_open(*space_outliner, [&](TreeElement *te) {
    TreeStoreElem *tselem = TREESTORE(te);

    /* if item is selected, perform operation */
    if (!(tselem->flag & TSE_SELECTED)) {
      return;
    }

    ID *id = nullptr;
    char *path = nullptr;
    int array_index = 0;
    short flag = 0;
    short groupmode = KSP_GROUP_KSNAME;

    TreeElementRNACommon *te_rna = tree_element_cast<TreeElementRNACommon>(te);
    PointerRNA ptr = te_rna ? te_rna->get_pointer_rna() : PointerRNA_NULL;
    PropertyRNA *prop = te_rna ? te_rna->get_property_rna() : nullptr;

    /* check if RNA-property described by this selected element is an animatable prop */
    if (prop && RNA_property_anim_editable(&ptr, prop)) {
      /* get id + path + index info from the selected element */
      tree_element_to_path(te, tselem, &id, &path, &array_index, &flag, &groupmode);
    }

    /* only if ID and path were set, should we perform any actions */
    if (id && path) {
      short dflags = CREATEDRIVER_WITH_DEFAULT_DVAR;
      int arraylen = 1;

      /* array checks */
      if (flag & KSP_FLAG_WHOLE_ARRAY) {
        /* entire array was selected, so add drivers for all */
        arraylen = RNA_property_array_length(&ptr, prop);
      }
      else {
        arraylen = array_index;
      }

      /* we should do at least one step */
      if (arraylen == array_index) {
        arraylen++;
      }

      /* for each array element we should affect, add driver */
      for (; array_index < arraylen; array_index++) {
        /* action depends on mode */
        switch (mode) {
          case DRIVERS_EDITMODE_ADD: {
            /* add a new driver with the information obtained (only if valid) */
            ANIM_add_driver(reports, id, path, array_index, dflags, DRIVER_TYPE_PYTHON);
            break;
          }
          case DRIVERS_EDITMODE_REMOVE: {
            ANIM_remove_driver(id, path, array_index);
            break;
          }
        }
      }

      /* free path, since it had to be generated */
      MEM_freeN(path);
    }
  });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Driver Add Operator
 * \{ */

static wmOperatorStatus outliner_drivers_addsel_exec(bContext *C, wmOperator *op)
{
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);

  /* check for invalid states */
  if (space_outliner == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* recursively go into tree, adding selected items */
  do_outliner_drivers_editop(space_outliner, op->reports, DRIVERS_EDITMODE_ADD);

  /* send notifiers */
  WM_event_add_notifier(C, NC_ANIMATION | ND_FCURVES_ORDER, nullptr); /* XXX */

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_drivers_add_selected(wmOperatorType *ot)
{
  /* API callbacks. */
  ot->idname = "OUTLINER_OT_drivers_add_selected";
  ot->name = "Add Drivers for Selected";
  ot->description = "Add drivers to selected items";

  /* API callbacks. */
  ot->exec = outliner_drivers_addsel_exec;
  ot->poll = ed_operator_outliner_datablocks_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Driver Remove Operator
 * \{ */

static wmOperatorStatus outliner_drivers_deletesel_exec(bContext *C, wmOperator *op)
{
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);

  /* check for invalid states */
  if (space_outliner == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* recursively go into tree, adding selected items */
  do_outliner_drivers_editop(space_outliner, op->reports, DRIVERS_EDITMODE_REMOVE);

  /* send notifiers */
  WM_event_add_notifier(C, ND_KEYS, nullptr); /* XXX */

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_drivers_delete_selected(wmOperatorType *ot)
{
  /* identifiers */
  ot->idname = "OUTLINER_OT_drivers_delete_selected";
  ot->name = "Delete Drivers for Selected";
  ot->description = "Delete drivers assigned to selected items";

  /* API callbacks. */
  ot->exec = outliner_drivers_deletesel_exec;
  ot->poll = ed_operator_outliner_datablocks_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Keying-Set Internal Utilities
 * \{ */

/**
 * Keying-Set Operations
 *
 * These operators are only available in data-browser mode for now, as
 * they depend on having RNA paths and/or hierarchies available.
 */
enum {
  KEYINGSET_EDITMODE_ADD = 0,
  KEYINGSET_EDITMODE_REMOVE,
} /*eKeyingSet_EditModes*/;

/* Find the 'active' KeyingSet, and add if not found (if adding is allowed). */
/* TODO: should this be an API func? */
static KeyingSet *verify_active_keyingset(Scene *scene, short add)
{
  KeyingSet *ks = nullptr;

  /* sanity check */
  if (scene == nullptr) {
    return nullptr;
  }

  /* try to find one from scene */
  if (scene->active_keyingset > 0) {
    ks = static_cast<KeyingSet *>(BLI_findlink(&scene->keyingsets, scene->active_keyingset - 1));
  }

  /* Add if none found */
  /* XXX the default settings have yet to evolve. */
  if ((add) && (ks == nullptr)) {
    ks = BKE_keyingset_add(&scene->keyingsets, nullptr, nullptr, KEYINGSET_ABSOLUTE, 0);
    scene->active_keyingset = BLI_listbase_count(&scene->keyingsets);
  }

  return ks;
}

/* Iterate over tree, finding and working on selected items */
static void do_outliner_keyingset_editop(SpaceOutliner *space_outliner,
                                         KeyingSet *ks,
                                         const short mode)
{
  tree_iterator::all_open(*space_outliner, [&](TreeElement *te) {
    TreeStoreElem *tselem = TREESTORE(te);

    /* if item is selected, perform operation */
    if (!(tselem->flag & TSE_SELECTED)) {
      return;
    }

    ID *id = nullptr;
    char *path = nullptr;
    int array_index = 0;
    short flag = 0;
    short groupmode = KSP_GROUP_KSNAME;

    /* check if RNA-property described by this selected element is an animatable prop */
    const TreeElementRNACommon *te_rna = tree_element_cast<TreeElementRNACommon>(te);
    if (te_rna) {
      PointerRNA ptr = te_rna->get_pointer_rna();
      if (PropertyRNA *prop = te_rna->get_property_rna()) {
        if (RNA_property_anim_editable(&ptr, prop)) {
          /* get id + path + index info from the selected element */
          tree_element_to_path(te, tselem, &id, &path, &array_index, &flag, &groupmode);
        }
      }
    }

    /* only if ID and path were set, should we perform any actions */
    if (id && path) {
      /* action depends on mode */
      switch (mode) {
        case KEYINGSET_EDITMODE_ADD: {
          /* add a new path with the information obtained (only if valid) */
          /* TODO: what do we do with group name?
           * for now, we don't supply one, and just let this use the KeyingSet name */
          BKE_keyingset_add_path(ks, id, nullptr, path, array_index, flag, groupmode);
          ks->active_path = BLI_listbase_count(&ks->paths);
          break;
        }
        case KEYINGSET_EDITMODE_REMOVE: {
          /* find the relevant path, then remove it from the KeyingSet */
          KS_Path *ksp = BKE_keyingset_find_path(ks, id, nullptr, path, array_index, groupmode);

          if (ksp) {
            /* free path's data */
            BKE_keyingset_free_path(ks, ksp);

            ks->active_path = 0;
          }
          break;
        }
      }

      /* free path, since it had to be generated */
      MEM_freeN(path);
    }
  });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Keying-Set Add Operator
 * \{ */

static wmOperatorStatus outliner_keyingset_additems_exec(bContext *C, wmOperator *op)
{
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  Scene *scene = CTX_data_scene(C);
  KeyingSet *ks = verify_active_keyingset(scene, 1);

  /* check for invalid states */
  if (ks == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "Operation requires an active keying set");
    return OPERATOR_CANCELLED;
  }
  if (space_outliner == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* recursively go into tree, adding selected items */
  do_outliner_keyingset_editop(space_outliner, ks, KEYINGSET_EDITMODE_ADD);

  /* send notifiers */
  WM_event_add_notifier(C, NC_SCENE | ND_KEYINGSET, nullptr);

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_keyingset_add_selected(wmOperatorType *ot)
{
  /* identifiers */
  ot->idname = "OUTLINER_OT_keyingset_add_selected";
  ot->name = "Keying Set Add Selected";
  ot->description = "Add selected items (blue-gray rows) to active Keying Set";

  /* API callbacks. */
  ot->exec = outliner_keyingset_additems_exec;
  ot->poll = ed_operator_outliner_datablocks_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Keying-Set Remove Operator
 * \{ */

static wmOperatorStatus outliner_keyingset_removeitems_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  Scene *scene = CTX_data_scene(C);
  KeyingSet *ks = verify_active_keyingset(scene, 1);

  /* check for invalid states */
  if (space_outliner == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* recursively go into tree, adding selected items */
  do_outliner_keyingset_editop(space_outliner, ks, KEYINGSET_EDITMODE_REMOVE);

  /* send notifiers */
  WM_event_add_notifier(C, NC_SCENE | ND_KEYINGSET, nullptr);

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_keyingset_remove_selected(wmOperatorType *ot)
{
  /* identifiers */
  ot->idname = "OUTLINER_OT_keyingset_remove_selected";
  ot->name = "Keying Set Remove Selected";
  ot->description = "Remove selected items (blue-gray rows) from active Keying Set";

  /* API callbacks. */
  ot->exec = outliner_keyingset_removeitems_exec;
  ot->poll = ed_operator_outliner_datablocks_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Purge Orphan Data-Blocks Operator
 * \{ */

static bool ed_operator_outliner_id_orphans_active(bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  if (area != nullptr && area->spacetype == SPACE_OUTLINER) {
    SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
    return (space_outliner->outlinevis == SO_ID_ORPHANS);
  }
  return true;
}

static void unused_message_gen(std::string &message,
                               const std::array<int, INDEX_ID_MAX> &num_tagged)
{
  bool is_first = true;
  if (num_tagged[INDEX_ID_NULL] == 0) {
    message += IFACE_("None");
    return;
  }

  /* NOTE: Index is looped in reversed order, since typically 'higher level' IDs (like Collections
   * or Objects) have a higher index than 'lower level' ones like object data, materials, etc.
   *
   * It makes more sense to present to the user the deleted numbers of Collections or Objects
   * before the ones for object data or Materials. */
  for (int i = INDEX_ID_MAX - 2; i >= 0; i--) {
    if (num_tagged[i] != 0) {
      message += fmt::format(
          "{}{} {}",
          (is_first) ? "" : ", ",
          num_tagged[i],
          (num_tagged[i] > 1) ?
              IFACE_(BKE_idtype_idcode_to_name_plural(BKE_idtype_index_to_idcode(i))) :
              IFACE_(BKE_idtype_idcode_to_name(BKE_idtype_index_to_idcode(i))));
      is_first = false;
    }
  }
}

static int unused_message_popup_width_compute(bContext *C)
{
  /* Computation of unused data amounts, with all options ON.
   * Used to estimate the maximum required width for the dialog. */
  Main *bmain = CTX_data_main(C);
  LibQueryUnusedIDsData data;
  data.do_local_ids = true;
  data.do_linked_ids = true;
  data.do_recursive = true;
  BKE_lib_query_unused_ids_amounts(bmain, data);

  std::string unused_message;
  const uiStyle *style = UI_style_get_dpi();
  unused_message_gen(unused_message, data.num_local);
  float max_messages_width = BLF_width(
      style->widget.uifont_id, unused_message.c_str(), BLF_DRAW_STR_DUMMY_MAX);

  unused_message = "";
  unused_message_gen(unused_message, data.num_linked);
  max_messages_width = std::max(
      max_messages_width,
      BLF_width(style->widget.uifont_id, unused_message.c_str(), BLF_DRAW_STR_DUMMY_MAX));

  return int(std::max(max_messages_width, 300.0f));
}

static void outliner_orphans_purge_cleanup(bContext *C,
                                           wmOperator *op,
                                           const bool is_abort = false)
{
  if (is_abort) {
    /* In case of abort, ensure that temp tag is cleared in all IDs, since they were not deleted.
     */
    BKE_main_id_tag_all(CTX_data_main(C), ID_TAG_DOIT, false);
  }
  if (op->customdata) {
    MEM_delete(static_cast<LibQueryUnusedIDsData *>(op->customdata));
    op->customdata = nullptr;
  }
}

static bool outliner_orphans_purge_check(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  LibQueryUnusedIDsData &data = *static_cast<LibQueryUnusedIDsData *>(op->customdata);

  data.do_local_ids = RNA_boolean_get(op->ptr, "do_local_ids");
  data.do_linked_ids = RNA_boolean_get(op->ptr, "do_linked_ids");
  data.do_recursive = RNA_boolean_get(op->ptr, "do_recursive");

  BKE_lib_query_unused_ids_amounts(bmain, data);

  /* Always assume count changed, and request a redraw. */
  return true;
}

static wmOperatorStatus outliner_orphans_purge_invoke(bContext *C,
                                                      wmOperator *op,
                                                      const wmEvent * /*event*/)
{
  op->customdata = MEM_new<LibQueryUnusedIDsData>(__func__);

  /* Compute expected amounts of deleted IDs and store them in 'cached' operator properties. */
  outliner_orphans_purge_check(C, op);

  return WM_operator_props_dialog_popup(C,
                                        op,
                                        unused_message_popup_width_compute(C),
                                        IFACE_("Purge Unused Data from This File"),
                                        IFACE_("Delete"));
}

static wmOperatorStatus outliner_orphans_purge_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  ScrArea *area = CTX_wm_area(C);
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);

  if (!op->customdata) {
    op->customdata = MEM_new<LibQueryUnusedIDsData>(__func__);
  }
  LibQueryUnusedIDsData &data = *static_cast<LibQueryUnusedIDsData *>(op->customdata);

  data.do_local_ids = RNA_boolean_get(op->ptr, "do_local_ids");
  data.do_linked_ids = RNA_boolean_get(op->ptr, "do_linked_ids");
  data.do_recursive = RNA_boolean_get(op->ptr, "do_recursive");

  /* Tag all IDs to delete. */
  BKE_lib_query_unused_ids_tag(bmain, ID_TAG_DOIT, data);

  if (data.num_total[INDEX_ID_NULL] == 0) {
    BKE_report(op->reports, RPT_INFO, "No orphaned data-blocks to purge");
    outliner_orphans_purge_cleanup(C, op, true);
    return OPERATOR_CANCELLED;
  }

  if (data.num_total[INDEX_ID_SCE] > 0) {
    BKE_report(op->reports,
               RPT_WARNING,
               "Attempt to delete scenes as part of a purge operation, should never happen");

    SceneReplaceData scene_replace_data;

    /* Get the scene currently expected to become the active scene. */
    Scene *scene_curr = scene_replace_data.active_scene_get(C);
    if (scene_curr && scene_curr->id.tag & ID_TAG_DOIT) {
      Scene *scene_new = BKE_scene_find_replacement(
          *bmain, *scene_curr, [](const Scene &scene) -> bool {
            return (
                /* The candidate scene must not be tagged for deletion. */
                (scene.id.tag & ID_TAG_DOIT) == 0 &&
                /* The candidate scene must be locale, or its library must not be tagged for
                 * deletion. */
                (!scene.id.lib || (scene.id.lib->id.tag & ID_TAG_DOIT) == 0));
          });
      if (!scene_new) {
        BKE_reportf(op->reports,
                    RPT_ERROR,
                    "Cannot find a scene to replace the active purged one '%s'",
                    scene_curr->id.name);
        outliner_orphans_purge_cleanup(C, op, true);
        return OPERATOR_CANCELLED;
      }

      BLI_assert(scene_curr != scene_new);
      BLI_assert((scene_new->id.tag & ID_TAG_DOIT) == 0);
      if (!scene_replace_data.scene_to_delete) {
        scene_replace_data.scene_to_delete = scene_curr;
      }
      else {
        BLI_assert(scene_replace_data.scene_to_delete == CTX_data_scene(C));
      }
      scene_replace_data.scene_to_activate = scene_new;
    }

    BLI_assert(scene_replace_data.is_valid());
    if (scene_replace_data.can_replace()) {
      ED_scene_replace_active_for_deletion(
          *C, *bmain, *scene_replace_data.scene_to_delete, scene_replace_data.scene_to_activate);
    }
  }

  BKE_id_multi_tagged_delete(bmain);

  BKE_reportf(op->reports, RPT_INFO, "Deleted %d data-block(s)", data.num_total[INDEX_ID_NULL]);

  /* XXX: tree management normally happens from draw_outliner(), but when
   *      you're clicking to fast on Delete object from context menu in
   *      outliner several mouse events can be handled in one cycle without
   *      handling notifiers/redraw which leads to deleting the same object twice.
   *      cleanup tree here to prevent such cases. */
  if ((area != nullptr) && (area->spacetype == SPACE_OUTLINER)) {
    outliner_cleanup_tree(space_outliner);
  }

  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_ID | NA_REMOVED, nullptr);
  /* Force full redraw of the UI. */
  WM_main_add_notifier(NC_WINDOW, nullptr);

  outliner_orphans_purge_cleanup(C, op);

  return OPERATOR_FINISHED;
}

static void outliner_orphans_purge_cancel(bContext *C, wmOperator *op)
{
  outliner_orphans_purge_cleanup(C, op, true);
}

static void outliner_orphans_purge_ui(bContext * /*C*/, wmOperator *op)
{
  uiLayout *layout = op->layout;
  PointerRNA *ptr = op->ptr;
  if (!op->customdata) {
    /* This should only happen on 'adjust last operation' case, since `invoke` will not have been
     * called then before showing the UI (the 'redo panel' UI uses WM-stored operator properties
     * and a newly-created operator).
     *
     * Since that operator is not 'registered' for adjusting from undo stack, this should never
     * happen currently. */
    BLI_assert_unreachable();
    op->customdata = MEM_new<LibQueryUnusedIDsData>(__func__);
  }
  LibQueryUnusedIDsData &data = *static_cast<LibQueryUnusedIDsData *>(op->customdata);

  std::string unused_message;
  unused_message_gen(unused_message, data.num_local);
  uiLayout *column = &layout->column(true);
  column->prop(ptr, "do_local_ids", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  uiLayout *row = &column->row(true);
  row->separator(2.67f);
  row->label(unused_message, ICON_NONE);

  unused_message = "";
  unused_message_gen(unused_message, data.num_linked);
  column = &layout->column(true);
  column->prop(ptr, "do_linked_ids", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  row = &column->row(true);
  row->separator(2.67f);
  row->label(unused_message, ICON_NONE);

  layout->prop(ptr, "do_recursive", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

void OUTLINER_OT_orphans_purge(wmOperatorType *ot)
{
  /* identifiers */
  ot->idname = "OUTLINER_OT_orphans_purge";
  ot->name = "Purge All";
  ot->description = "Clear all orphaned data-blocks without any users from the file";

  /* callbacks */
  ot->invoke = outliner_orphans_purge_invoke;
  ot->exec = outliner_orphans_purge_exec;
  ot->cancel = outliner_orphans_purge_cancel;

  ot->poll = ed_operator_outliner_id_orphans_active;
  ot->check = outliner_orphans_purge_check;
  ot->ui = outliner_orphans_purge_ui;

  /* flags */
  /* NOTE: No #OPTYPE_REGISTER, since this operator should not be 'adjustable'. */
  ot->flag = OPTYPE_UNDO;

  /* Actual user-visible settings. */
  RNA_def_boolean(ot->srna,
                  "do_local_ids",
                  true,
                  "Local Data-blocks",
                  "Include unused local data-blocks into deletion");
  RNA_def_boolean(ot->srna,
                  "do_linked_ids",
                  true,
                  "Linked Data-blocks",
                  "Include unused linked data-blocks into deletion");

  RNA_def_boolean(ot->srna,
                  "do_recursive",
                  true,
                  "Recursive Delete",
                  "Recursively check for indirectly unused data-blocks, ensuring that no orphaned "
                  "data-blocks remain after execution");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Manage Orphan Data-Blocks Operator
 * \{ */

static wmOperatorStatus outliner_orphans_manage_invoke(bContext *C,
                                                       wmOperator * /*op*/,
                                                       const wmEvent * /*event*/)
{
  if (WM_window_open_temp(C, IFACE_("Manage Unused Data"), SPACE_OUTLINER, false)) {
    SpaceOutliner *soutline = CTX_wm_space_outliner(C);
    soutline->outlinevis = SO_ID_ORPHANS;
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

void OUTLINER_OT_orphans_manage(wmOperatorType *ot)
{
  /* identifiers */
  ot->idname = "OUTLINER_OT_orphans_manage";
  ot->name = "Manage Unused Data";
  ot->description = "Open a window to manage unused data";

  /* callbacks */
  ot->invoke = outliner_orphans_manage_invoke;

  /* flags */
  ot->flag = OPTYPE_REGISTER;
}

/** \} */

}  // namespace blender::ed::outliner
