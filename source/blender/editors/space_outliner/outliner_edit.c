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

#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_collection_types.h"
#include "DNA_ID.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_material_types.h"

#include "BLI_blenlib.h"
#include "BLI_dynstr.h"
#include "BLI_utildefines.h"
#include "BLI_path_util.h"

#include "BLT_translation.h"

#include "BKE_animsys.h"
#include "BKE_appdir.h"
#include "BKE_blender_copybuffer.h"
#include "BKE_collection.h"
#include "BKE_context.h"
#include "BKE_idcode.h"
#include "BKE_layer.h"
#include "BKE_library.h"
#include "BKE_library_query.h"
#include "BKE_library_remap.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_outliner_treehash.h"
#include "BKE_report.h"
#include "BKE_scene.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "../blenloader/BLO_readfile.h"

#include "ED_object.h"
#include "ED_outliner.h"
#include "ED_screen.h"
#include "ED_select_utils.h"
#include "ED_keyframing.h"
#include "ED_armature.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "GPU_material.h"

#include "outliner_intern.h"

/* ************************************************************** */

/* Highlight --------------------------------------------------- */

static int outliner_highlight_update(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
  /* Drag and drop does own highlighting. */
  wmWindowManager *wm = CTX_wm_manager(C);
  if (wm->drags.first) {
    return OPERATOR_PASS_THROUGH;
  }

  ARegion *ar = CTX_wm_region(C);
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  const float my = UI_view2d_region_to_view_y(&ar->v2d, event->mval[1]);

  TreeElement *hovered_te = outliner_find_item_at_y(soops, &soops->tree, my);
  bool changed = false;

  if (!hovered_te || !(hovered_te->store_elem->flag & TSE_HIGHLIGHTED)) {
    changed = outliner_flag_set(&soops->tree, TSE_HIGHLIGHTED | TSE_DRAG_ANY, false);
    if (hovered_te) {
      hovered_te->store_elem->flag |= TSE_HIGHLIGHTED;
      changed = true;
    }
  }

  if (changed) {
    ED_region_tag_redraw_no_rebuild(ar);
  }

  return OPERATOR_PASS_THROUGH;
}

void OUTLINER_OT_highlight_update(wmOperatorType *ot)
{
  ot->name = "Update Highlight";
  ot->idname = "OUTLINER_OT_highlight_update";
  ot->description = "Update the item highlight based on the current mouse position";

  ot->invoke = outliner_highlight_update;

  ot->poll = ED_operator_outliner_active;
}

/* Toggle Open/Closed ------------------------------------------- */

static int do_outliner_item_openclose(
    bContext *C, SpaceOutliner *soops, TreeElement *te, const bool all, const float mval[2])
{

  if (mval[1] > te->ys && mval[1] < te->ys + UI_UNIT_Y) {
    TreeStoreElem *tselem = TREESTORE(te);

    /* all below close/open? */
    if (all) {
      tselem->flag &= ~TSE_CLOSED;
      outliner_flag_set(
          &te->subtree, TSE_CLOSED, !outliner_flag_is_any_test(&te->subtree, TSE_CLOSED, 1));
    }
    else {
      if (tselem->flag & TSE_CLOSED) {
        tselem->flag &= ~TSE_CLOSED;
      }
      else {
        tselem->flag |= TSE_CLOSED;
      }
    }

    return 1;
  }

  for (te = te->subtree.first; te; te = te->next) {
    if (do_outliner_item_openclose(C, soops, te, all, mval)) {
      return 1;
    }
  }
  return 0;
}

/* event can enterkey, then it opens/closes */
static int outliner_item_openclose(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *ar = CTX_wm_region(C);
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  TreeElement *te;
  float fmval[2];
  const bool all = RNA_boolean_get(op->ptr, "all");

  UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &fmval[0], &fmval[1]);

  for (te = soops->tree.first; te; te = te->next) {
    if (do_outliner_item_openclose(C, soops, te, all, fmval)) {
      break;
    }
  }

  ED_region_tag_redraw(ar);

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_item_openclose(wmOperatorType *ot)
{
  ot->name = "Open/Close";
  ot->idname = "OUTLINER_OT_item_openclose";
  ot->description = "Toggle whether item under cursor is enabled or closed";

  ot->invoke = outliner_item_openclose;

  ot->poll = ED_operator_outliner_active;

  RNA_def_boolean(ot->srna, "all", 1, "All", "Close or open all items");
}

/* -------------------------------------------------------------------- */
/** \name Object Mode Enter/Exit
 * \{ */

static void item_object_mode_enter_exit(bContext *C, ReportList *reports, Object *ob, bool enter)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *obact = OBACT(view_layer);

  if ((ob->type != obact->type) || ID_IS_LINKED(ob->data)) {
    return;
  }
  if (((ob->mode & obact->mode) != 0) == enter) {
    return;
  }

  if (ob == obact) {
    BKE_report(reports, RPT_WARNING, "Active object mode not changed");
    return;
  }

  Base *base = BKE_view_layer_base_find(view_layer, ob);
  if (base == NULL) {
    return;
  }
  Scene *scene = CTX_data_scene(C);
  outliner_object_mode_toggle(C, scene, view_layer, base);
}

void item_object_mode_enter_cb(bContext *C,
                               ReportList *reports,
                               Scene *UNUSED(scene),
                               TreeElement *UNUSED(te),
                               TreeStoreElem *UNUSED(tsep),
                               TreeStoreElem *tselem,
                               void *UNUSED(user_data))
{
  Object *ob = (Object *)tselem->id;
  item_object_mode_enter_exit(C, reports, ob, true);
}

void item_object_mode_exit_cb(bContext *C,
                              ReportList *reports,
                              Scene *UNUSED(scene),
                              TreeElement *UNUSED(te),
                              TreeStoreElem *UNUSED(tsep),
                              TreeStoreElem *tselem,
                              void *UNUSED(user_data))
{
  Object *ob = (Object *)tselem->id;
  item_object_mode_enter_exit(C, reports, ob, false);
}

/** \} */

/* Rename --------------------------------------------------- */

static void do_item_rename(ARegion *ar,
                           TreeElement *te,
                           TreeStoreElem *tselem,
                           ReportList *reports)
{
  bool add_textbut = false;

  /* can't rename rna datablocks entries or listbases */
  if (ELEM(tselem->type,
           TSE_RNA_STRUCT,
           TSE_RNA_PROPERTY,
           TSE_RNA_ARRAY_ELEM,
           TSE_ID_BASE,
           TSE_SCENE_OBJECTS_BASE)) {
    /* do nothing */
  }
  else if (ELEM(tselem->type,
                TSE_ANIM_DATA,
                TSE_NLA,
                TSE_DEFGROUP_BASE,
                TSE_CONSTRAINT_BASE,
                TSE_MODIFIER_BASE,
                TSE_DRIVER_BASE,
                TSE_POSE_BASE,
                TSE_POSEGRP_BASE,
                TSE_R_LAYER_BASE,
                TSE_SCENE_COLLECTION_BASE,
                TSE_VIEW_COLLECTION_BASE)) {
    BKE_report(reports, RPT_WARNING, "Cannot edit builtin name");
  }
  else if (ELEM(tselem->type, TSE_SEQUENCE, TSE_SEQ_STRIP, TSE_SEQUENCE_DUP)) {
    BKE_report(reports, RPT_WARNING, "Cannot edit sequence name");
  }
  else if (outliner_is_collection_tree_element(te)) {
    Collection *collection = outliner_collection_from_tree_element(te);

    if (collection->flag & COLLECTION_IS_MASTER) {
      BKE_report(reports, RPT_WARNING, "Cannot edit name of master collection");
    }
    else {
      add_textbut = true;
    }
  }
  else if (ID_IS_LINKED(tselem->id)) {
    BKE_report(reports, RPT_WARNING, "Cannot edit external library data");
  }
  else if (te->idcode == ID_LI && ((Library *)tselem->id)->parent) {
    BKE_report(reports, RPT_WARNING, "Cannot edit the path of an indirectly linked library");
  }
  else {
    add_textbut = true;
  }

  if (add_textbut) {
    tselem->flag |= TSE_TEXTBUT;
    ED_region_tag_redraw(ar);
  }
}

void item_rename_cb(bContext *C,
                    ReportList *reports,
                    Scene *UNUSED(scene),
                    TreeElement *te,
                    TreeStoreElem *UNUSED(tsep),
                    TreeStoreElem *tselem,
                    void *UNUSED(user_data))
{
  ARegion *ar = CTX_wm_region(C);
  do_item_rename(ar, te, tselem, reports);
}

static int do_outliner_item_rename(ReportList *reports,
                                   ARegion *ar,
                                   TreeElement *te,
                                   const float mval[2])
{
  if (mval[1] > te->ys && mval[1] < te->ys + UI_UNIT_Y) {
    TreeStoreElem *tselem = TREESTORE(te);

    /* click on name */
    if (mval[0] > te->xs + UI_UNIT_X * 2 && mval[0] < te->xend) {
      do_item_rename(ar, te, tselem, reports);
      return 1;
    }
    return 0;
  }

  for (te = te->subtree.first; te; te = te->next) {
    if (do_outliner_item_rename(reports, ar, te, mval)) {
      return 1;
    }
  }
  return 0;
}

static int outliner_item_rename(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *ar = CTX_wm_region(C);
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  TreeElement *te;
  float fmval[2];
  bool changed = false;

  UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &fmval[0], &fmval[1]);

  for (te = soops->tree.first; te; te = te->next) {
    if (do_outliner_item_rename(op->reports, ar, te, fmval)) {
      changed = true;
      break;
    }
  }

  return changed ? OPERATOR_FINISHED : OPERATOR_PASS_THROUGH;
}

void OUTLINER_OT_item_rename(wmOperatorType *ot)
{
  ot->name = "Rename";
  ot->idname = "OUTLINER_OT_item_rename";
  ot->description = "Rename item under cursor";

  ot->invoke = outliner_item_rename;

  ot->poll = ED_operator_outliner_active;
}

/* ID delete --------------------------------------------------- */

static void id_delete(bContext *C, ReportList *reports, TreeElement *te, TreeStoreElem *tselem)
{
  Main *bmain = CTX_data_main(C);
  ID *id = tselem->id;

  BLI_assert(te->idcode != 0 && id != NULL);
  UNUSED_VARS_NDEBUG(te);

  if (te->idcode == ID_LI && ((Library *)id)->parent != NULL) {
    BKE_reportf(reports, RPT_WARNING, "Cannot delete indirectly linked library '%s'", id->name);
    return;
  }
  if (id->tag & LIB_TAG_INDIRECT) {
    BKE_reportf(reports, RPT_WARNING, "Cannot delete indirectly linked id '%s'", id->name);
    return;
  }
  else if (BKE_library_ID_is_indirectly_used(bmain, id) && ID_REAL_USERS(id) <= 1) {
    BKE_reportf(reports,
                RPT_WARNING,
                "Cannot delete id '%s', indirectly used data-blocks need at least one user",
                id->name);
    return;
  }

  BKE_id_delete(bmain, id);

  WM_event_add_notifier(C, NC_WINDOW, NULL);
}

void id_delete_cb(bContext *C,
                  ReportList *reports,
                  Scene *UNUSED(scene),
                  TreeElement *te,
                  TreeStoreElem *UNUSED(tsep),
                  TreeStoreElem *tselem,
                  void *UNUSED(user_data))
{
  id_delete(C, reports, te, tselem);
}

static int outliner_id_delete_invoke_do(bContext *C,
                                        ReportList *reports,
                                        TreeElement *te,
                                        const float mval[2])
{
  if (mval[1] > te->ys && mval[1] < te->ys + UI_UNIT_Y) {
    TreeStoreElem *tselem = TREESTORE(te);

    if (te->idcode != 0 && tselem->id) {
      if (te->idcode == ID_LI && ((Library *)tselem->id)->parent) {
        BKE_reportf(reports,
                    RPT_ERROR_INVALID_INPUT,
                    "Cannot delete indirectly linked library '%s'",
                    ((Library *)tselem->id)->filepath);
        return OPERATOR_CANCELLED;
      }
      id_delete(C, reports, te, tselem);
      return OPERATOR_FINISHED;
    }
  }
  else {
    for (te = te->subtree.first; te; te = te->next) {
      int ret;
      if ((ret = outliner_id_delete_invoke_do(C, reports, te, mval))) {
        return ret;
      }
    }
  }

  return 0;
}

static int outliner_id_delete_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *ar = CTX_wm_region(C);
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  TreeElement *te;
  float fmval[2];

  BLI_assert(ar && soops);

  UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &fmval[0], &fmval[1]);

  for (te = soops->tree.first; te; te = te->next) {
    int ret;

    if ((ret = outliner_id_delete_invoke_do(C, op->reports, te, fmval))) {
      return ret;
    }
  }

  return OPERATOR_CANCELLED;
}

void OUTLINER_OT_id_delete(wmOperatorType *ot)
{
  ot->name = "Delete Data-Block";
  ot->idname = "OUTLINER_OT_id_delete";
  ot->description = "Delete the ID under cursor";

  ot->invoke = outliner_id_delete_invoke;
  ot->poll = ED_operator_outliner_active;
}

/* ID remap --------------------------------------------------- */

static int outliner_id_remap_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  SpaceOutliner *soops = CTX_wm_space_outliner(C);

  const short id_type = (short)RNA_enum_get(op->ptr, "id_type");
  ID *old_id = BLI_findlink(which_libbase(CTX_data_main(C), id_type),
                            RNA_enum_get(op->ptr, "old_id"));
  ID *new_id = BLI_findlink(which_libbase(CTX_data_main(C), id_type),
                            RNA_enum_get(op->ptr, "new_id"));

  /* check for invalid states */
  if (soops == NULL) {
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

  if (ID_IS_LINKED(old_id)) {
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

  WM_event_add_notifier(C, NC_WINDOW, NULL);

  return OPERATOR_FINISHED;
}

static bool outliner_id_remap_find_tree_element(bContext *C,
                                                wmOperator *op,
                                                ListBase *tree,
                                                const float y)
{
  TreeElement *te;

  for (te = tree->first; te; te = te->next) {
    if (y > te->ys && y < te->ys + UI_UNIT_Y) {
      TreeStoreElem *tselem = TREESTORE(te);

      if (tselem->type == 0 && tselem->id) {
        printf("found id %s (%p)!\n", tselem->id->name, tselem->id);

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

static int outliner_id_remap_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  ARegion *ar = CTX_wm_region(C);
  float fmval[2];

  if (!RNA_property_is_set(op->ptr, RNA_struct_find_property(op->ptr, "id_type"))) {
    UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &fmval[0], &fmval[1]);

    outliner_id_remap_find_tree_element(C, op, &soops->tree, fmval[1]);
  }

  return WM_operator_props_dialog_popup(C, op, 200, 100);
}

static const EnumPropertyItem *outliner_id_itemf(bContext *C,
                                                 PointerRNA *ptr,
                                                 PropertyRNA *UNUSED(prop),
                                                 bool *r_free)
{
  EnumPropertyItem item_tmp = {0}, *item = NULL;
  int totitem = 0;
  int i = 0;

  short id_type = (short)RNA_enum_get(ptr, "id_type");
  ID *id = which_libbase(CTX_data_main(C), id_type)->first;

  for (; id; id = id->next) {
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
  ot->name = "Outliner ID data Remap";
  ot->idname = "OUTLINER_OT_id_remap";

  /* callbacks */
  ot->invoke = outliner_id_remap_invoke;
  ot->exec = outliner_id_remap_exec;
  ot->poll = ED_operator_outliner_active;

  ot->flag = 0;

  prop = RNA_def_enum(ot->srna, "id_type", rna_enum_id_type_items, ID_OB, "ID Type", "");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_ID);

  prop = RNA_def_enum(ot->srna, "old_id", DummyRNA_NULL_items, 0, "Old ID", "Old ID to replace");
  RNA_def_property_enum_funcs_runtime(prop, NULL, NULL, outliner_id_itemf);
  RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE | PROP_HIDDEN);

  ot->prop = RNA_def_enum(ot->srna,
                          "new_id",
                          DummyRNA_NULL_items,
                          0,
                          "New ID",
                          "New ID to remap all selected IDs' users to");
  RNA_def_property_enum_funcs_runtime(ot->prop, NULL, NULL, outliner_id_itemf);
  RNA_def_property_flag(ot->prop, PROP_ENUM_NO_TRANSLATE);
}

void id_remap_cb(bContext *C,
                 ReportList *UNUSED(reports),
                 Scene *UNUSED(scene),
                 TreeElement *UNUSED(te),
                 TreeStoreElem *UNUSED(tsep),
                 TreeStoreElem *tselem,
                 void *UNUSED(user_data))
{
  wmOperatorType *ot = WM_operatortype_find("OUTLINER_OT_id_remap", false);
  PointerRNA op_props;

  BLI_assert(tselem->id != NULL);

  WM_operator_properties_create_ptr(&op_props, ot);

  RNA_enum_set(&op_props, "id_type", GS(tselem->id->name));
  RNA_enum_set_identifier(C, &op_props, "old_id", tselem->id->name + 2);

  WM_operator_name_call_ptr(C, ot, WM_OP_INVOKE_DEFAULT, &op_props);

  WM_operator_properties_free(&op_props);
}

/* ID copy/Paste ------------------------------------------------------------- */

static int outliner_id_copy_tag(SpaceOutliner *soops, ListBase *tree)
{
  TreeElement *te;
  TreeStoreElem *tselem;
  int num_ids = 0;

  for (te = tree->first; te; te = te->next) {
    tselem = TREESTORE(te);

    /* if item is selected and is an ID, tag it as needing to be copied. */
    if (tselem->flag & TSE_SELECTED && ELEM(tselem->type, 0, TSE_LAYER_COLLECTION)) {
      ID *id = tselem->id;
      if (!(id->tag & LIB_TAG_DOIT)) {
        BKE_copybuffer_tag_ID(tselem->id);
        num_ids++;
      }
    }

    /* go over sub-tree */
    if (TSELEM_OPEN(tselem, soops)) {
      num_ids += outliner_id_copy_tag(soops, &te->subtree);
    }
  }

  return num_ids;
}

static int outliner_id_copy_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  char str[FILE_MAX];

  BKE_copybuffer_begin(bmain);

  const int num_ids = outliner_id_copy_tag(soops, &soops->tree);
  if (num_ids == 0) {
    BKE_report(op->reports, RPT_INFO, "No selected data-blocks to copy");
    return OPERATOR_CANCELLED;
  }

  BLI_make_file_string("/", str, BKE_tempdir_base(), "copybuffer.blend");
  BKE_copybuffer_save(bmain, str, op->reports);

  BKE_reportf(op->reports, RPT_INFO, "Copied %d selected data-blocks", num_ids);

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_id_copy(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Outliner ID Data Copy";
  ot->idname = "OUTLINER_OT_id_copy";
  ot->description = "Selected data-blocks are copied to the clipboard";

  /* callbacks */
  ot->exec = outliner_id_copy_exec;
  ot->poll = ED_operator_outliner_active;

  ot->flag = 0;
}

static int outliner_id_paste_exec(bContext *C, wmOperator *op)
{
  char str[FILE_MAX];
  const short flag = FILE_AUTOSELECT | FILE_ACTIVE_COLLECTION;

  BLI_make_file_string("/", str, BKE_tempdir_base(), "copybuffer.blend");

  const int num_pasted = BKE_copybuffer_paste(C, str, flag, op->reports, 0);
  if (num_pasted == 0) {
    BKE_report(op->reports, RPT_INFO, "No data to paste");
    return OPERATOR_CANCELLED;
  }

  WM_event_add_notifier(C, NC_WINDOW, NULL);

  BKE_reportf(op->reports, RPT_INFO, "%d data-blocks pasted", num_pasted);
  return OPERATOR_FINISHED;
}

void OUTLINER_OT_id_paste(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Outliner ID Data Paste";
  ot->idname = "OUTLINER_OT_id_paste";
  ot->description = "Data-blocks from the clipboard are pasted";

  /* callbacks */
  ot->exec = outliner_id_paste_exec;
  ot->poll = ED_operator_outliner_active;

  ot->flag = 0;
}

/* Library relocate/reload --------------------------------------------------- */

static int lib_relocate(
    bContext *C, TreeElement *te, TreeStoreElem *tselem, wmOperatorType *ot, const bool reload)
{
  PointerRNA op_props;
  int ret = 0;

  BLI_assert(te->idcode == ID_LI && tselem->id != NULL);
  UNUSED_VARS_NDEBUG(te);

  WM_operator_properties_create_ptr(&op_props, ot);

  RNA_string_set(&op_props, "library", tselem->id->name + 2);

  if (reload) {
    Library *lib = (Library *)tselem->id;
    char dir[FILE_MAXDIR], filename[FILE_MAX];

    BLI_split_dirfile(lib->filepath, dir, filename, sizeof(dir), sizeof(filename));

    printf("%s, %s\n", tselem->id->name, lib->filepath);

    /* We assume if both paths in lib are not the same then lib->name was relative... */
    RNA_boolean_set(&op_props, "relative_path", BLI_path_cmp(lib->filepath, lib->name) != 0);

    RNA_string_set(&op_props, "directory", dir);
    RNA_string_set(&op_props, "filename", filename);

    ret = WM_operator_name_call_ptr(C, ot, WM_OP_EXEC_DEFAULT, &op_props);
  }
  else {
    ret = WM_operator_name_call_ptr(C, ot, WM_OP_INVOKE_DEFAULT, &op_props);
  }

  WM_operator_properties_free(&op_props);

  return ret;
}

static int outliner_lib_relocate_invoke_do(
    bContext *C, ReportList *reports, TreeElement *te, const float mval[2], const bool reload)
{
  if (mval[1] > te->ys && mval[1] < te->ys + UI_UNIT_Y) {
    TreeStoreElem *tselem = TREESTORE(te);

    if (te->idcode == ID_LI && tselem->id) {
      if (((Library *)tselem->id)->parent && !reload) {
        BKE_reportf(reports,
                    RPT_ERROR_INVALID_INPUT,
                    "Cannot relocate indirectly linked library '%s'",
                    ((Library *)tselem->id)->filepath);
        return OPERATOR_CANCELLED;
      }
      else {
        wmOperatorType *ot = WM_operatortype_find(
            reload ? "WM_OT_lib_reload" : "WM_OT_lib_relocate", false);

        return lib_relocate(C, te, tselem, ot, reload);
      }
    }
  }
  else {
    for (te = te->subtree.first; te; te = te->next) {
      int ret;
      if ((ret = outliner_lib_relocate_invoke_do(C, reports, te, mval, reload))) {
        return ret;
      }
    }
  }

  return 0;
}

static int outliner_lib_relocate_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *ar = CTX_wm_region(C);
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  TreeElement *te;
  float fmval[2];

  BLI_assert(ar && soops);

  UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &fmval[0], &fmval[1]);

  for (te = soops->tree.first; te; te = te->next) {
    int ret;

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
  ot->poll = ED_operator_outliner_active;
}

/* XXX This does not work with several items
 * (it is only called once in the end, due to the 'deferred'
 * filebrowser invocation through event system...). */
void lib_relocate_cb(bContext *C,
                     ReportList *UNUSED(reports),
                     Scene *UNUSED(scene),
                     TreeElement *te,
                     TreeStoreElem *UNUSED(tsep),
                     TreeStoreElem *tselem,
                     void *UNUSED(user_data))
{
  wmOperatorType *ot = WM_operatortype_find("WM_OT_lib_relocate", false);

  lib_relocate(C, te, tselem, ot, false);
}

static int outliner_lib_reload_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *ar = CTX_wm_region(C);
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  TreeElement *te;
  float fmval[2];

  BLI_assert(ar && soops);

  UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &fmval[0], &fmval[1]);

  for (te = soops->tree.first; te; te = te->next) {
    int ret;

    if ((ret = outliner_lib_relocate_invoke_do(C, op->reports, te, fmval, true))) {
      return ret;
    }
  }

  return OPERATOR_CANCELLED;
}

void OUTLINER_OT_lib_reload(wmOperatorType *ot)
{
  ot->name = "Reload Library";
  ot->idname = "OUTLINER_OT_lib_reload";
  ot->description = "Reload the library under cursor";

  ot->invoke = outliner_lib_reload_invoke;
  ot->poll = ED_operator_outliner_active;
}

void lib_reload_cb(bContext *C,
                   ReportList *UNUSED(reports),
                   Scene *UNUSED(scene),
                   TreeElement *te,
                   TreeStoreElem *UNUSED(tsep),
                   TreeStoreElem *tselem,
                   void *UNUSED(user_data))
{
  wmOperatorType *ot = WM_operatortype_find("WM_OT_lib_reload", false);

  lib_relocate(C, te, tselem, ot, true);
}

/* ************************************************************** */
/* Setting Toggling Operators */

/* =============================================== */
/* Toggling Utilities (Exported) */

/* Apply Settings ------------------------------- */

static int outliner_count_levels(ListBase *lb, const int curlevel)
{
  TreeElement *te;
  int level = curlevel, lev;

  for (te = lb->first; te; te = te->next) {

    lev = outliner_count_levels(&te->subtree, curlevel + 1);
    if (lev > level) {
      level = lev;
    }
  }
  return level;
}

int outliner_flag_is_any_test(ListBase *lb, short flag, const int curlevel)
{
  TreeElement *te;
  TreeStoreElem *tselem;
  int level;

  for (te = lb->first; te; te = te->next) {
    tselem = TREESTORE(te);
    if (tselem->flag & flag) {
      return curlevel;
    }

    level = outliner_flag_is_any_test(&te->subtree, flag, curlevel + 1);
    if (level) {
      return level;
    }
  }
  return 0;
}

/**
 * Set or unset \a flag for all outliner elements in \a lb and sub-trees.
 * \return if any flag was modified.
 */
bool outliner_flag_set(ListBase *lb, short flag, short set)
{
  TreeElement *te;
  TreeStoreElem *tselem;
  bool changed = false;
  bool has_flag;

  for (te = lb->first; te; te = te->next) {
    tselem = TREESTORE(te);
    has_flag = (tselem->flag & flag);
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
    changed |= outliner_flag_set(&te->subtree, flag, set);
  }

  return changed;
}

bool outliner_flag_flip(ListBase *lb, short flag)
{
  TreeElement *te;
  TreeStoreElem *tselem;
  bool changed = false;

  for (te = lb->first; te; te = te->next) {
    tselem = TREESTORE(te);
    tselem->flag ^= flag;
    changed |= outliner_flag_flip(&te->subtree, flag);
  }

  return changed;
}

/* Restriction Columns ------------------------------- */

/* same check needed for both object operation and restrict column button func
 * return 0 when in edit mode (cannot restrict view or select)
 * otherwise return 1 */
int common_restrict_check(bContext *C, Object *ob)
{
  /* Don't allow hide an object in edit mode,
   * check the bug #22153 and #21609, #23977
   */
  Object *obedit = CTX_data_edit_object(C);
  if (obedit && obedit == ob) {
    /* found object is hidden, reset */
    if (ob->restrictflag & OB_RESTRICT_VIEWPORT) {
      ob->restrictflag &= ~OB_RESTRICT_VIEWPORT;
    }
    /* found object is unselectable, reset */
    if (ob->restrictflag & OB_RESTRICT_SELECT) {
      ob->restrictflag &= ~OB_RESTRICT_SELECT;
    }
    return 0;
  }

  return 1;
}

/* =============================================== */
/* Outliner setting toggles */

/* Toggle Expanded (Outliner) ---------------------------------------- */

static int outliner_toggle_expanded_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  ARegion *ar = CTX_wm_region(C);

  if (outliner_flag_is_any_test(&soops->tree, TSE_CLOSED, 1)) {
    outliner_flag_set(&soops->tree, TSE_CLOSED, 0);
  }
  else {
    outliner_flag_set(&soops->tree, TSE_CLOSED, 1);
  }

  ED_region_tag_redraw(ar);

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
  ot->poll = ED_operator_outliner_active;

  /* no undo or registry, UI option */
}

/* Toggle Selected (Outliner) ---------------------------------------- */

static int outliner_select_all_exec(bContext *C, wmOperator *op)
{
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  ARegion *ar = CTX_wm_region(C);
  Scene *scene = CTX_data_scene(C);
  int action = RNA_enum_get(op->ptr, "action");
  if (action == SEL_TOGGLE) {
    action = outliner_flag_is_any_test(&soops->tree, TSE_SELECTED, 1) ? SEL_DESELECT : SEL_SELECT;
  }

  switch (action) {
    case SEL_SELECT:
      outliner_flag_set(&soops->tree, TSE_SELECTED, 1);
      break;
    case SEL_DESELECT:
      outliner_flag_set(&soops->tree, TSE_SELECTED, 0);
      break;
    case SEL_INVERT:
      outliner_flag_flip(&soops->tree, TSE_SELECTED);
      break;
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
  ED_region_tag_redraw_no_rebuild(ar);

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

/* ************************************************************** */
/* Hotkey Only Operators */

/* Show Active --------------------------------------------------- */

static void outliner_set_coordinates_element_recursive(SpaceOutliner *soops,
                                                       TreeElement *te,
                                                       int startx,
                                                       int *starty)
{
  TreeStoreElem *tselem = TREESTORE(te);

  /* store coord and continue, we need coordinates for elements outside view too */
  te->xs = (float)startx;
  te->ys = (float)(*starty);
  *starty -= UI_UNIT_Y;

  if (TSELEM_OPEN(tselem, soops)) {
    TreeElement *ten;
    for (ten = te->subtree.first; ten; ten = ten->next) {
      outliner_set_coordinates_element_recursive(soops, ten, startx + UI_UNIT_X, starty);
    }
  }
}

/* to retrieve coordinates with redrawing the entire tree */
void outliner_set_coordinates(ARegion *ar, SpaceOutliner *soops)
{
  TreeElement *te;
  int starty = (int)(ar->v2d.tot.ymax) - UI_UNIT_Y;

  for (te = soops->tree.first; te; te = te->next) {
    outliner_set_coordinates_element_recursive(soops, te, 0, &starty);
  }
}

/* return 1 when levels were opened */
static int outliner_open_back(TreeElement *te)
{
  TreeStoreElem *tselem;
  int retval = 0;

  for (te = te->parent; te; te = te->parent) {
    tselem = TREESTORE(te);
    if (tselem->flag & TSE_CLOSED) {
      tselem->flag &= ~TSE_CLOSED;
      retval = 1;
    }
  }
  return retval;
}

static int outliner_show_active_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceOutliner *so = CTX_wm_space_outliner(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  ARegion *ar = CTX_wm_region(C);
  View2D *v2d = &ar->v2d;

  TreeElement *te;
  int xdelta, ytop;

  Object *obact = OBACT(view_layer);

  if (!obact) {
    return OPERATOR_CANCELLED;
  }

  te = outliner_find_id(so, &so->tree, &obact->id);

  if (te != NULL && obact->type == OB_ARMATURE) {
    /* traverse down the bone hierarchy in case of armature */
    TreeElement *te_obact = te;

    if (obact->mode & OB_MODE_POSE) {
      bPoseChannel *pchan = CTX_data_active_pose_bone(C);
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

  if (te) {
    /* open up tree to active object/bone */
    if (outliner_open_back(te)) {
      outliner_set_coordinates(ar, so);
    }

    /* make te->ys center of view */
    ytop = te->ys + BLI_rcti_size_y(&v2d->mask) / 2;
    if (ytop > 0) {
      ytop = 0;
    }

    v2d->cur.ymax = (float)ytop;
    v2d->cur.ymin = (float)(ytop - BLI_rcti_size_y(&v2d->mask));

    /* make te->xs ==> te->xend center of view */
    xdelta = (int)(te->xs - v2d->cur.xmin);
    v2d->cur.xmin += xdelta;
    v2d->cur.xmax += xdelta;
  }

  ED_region_tag_redraw_no_rebuild(ar);

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_show_active(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Show Active";
  ot->idname = "OUTLINER_OT_show_active";
  ot->description =
      "Open up the tree and adjust the view so that the active Object is shown centered";

  /* callbacks */
  ot->exec = outliner_show_active_exec;
  ot->poll = ED_operator_outliner_active;
}

/* View Panning --------------------------------------------------- */

static int outliner_scroll_page_exec(bContext *C, wmOperator *op)
{
  ARegion *ar = CTX_wm_region(C);
  int dy = BLI_rcti_size_y(&ar->v2d.mask);
  int up = 0;

  if (RNA_boolean_get(op->ptr, "up")) {
    up = 1;
  }

  if (up == 0) {
    dy = -dy;
  }
  ar->v2d.cur.ymin += dy;
  ar->v2d.cur.ymax += dy;

  ED_region_tag_redraw_no_rebuild(ar);

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
  ot->poll = ED_operator_outliner_active;

  /* properties */
  prop = RNA_def_boolean(ot->srna, "up", 0, "Up", "Scroll up one page");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/* Search ------------------------------------------------------- */
// TODO: probably obsolete now with filtering?

#if 0

/* find next element that has this name */
static TreeElement *outliner_find_name(
    SpaceOutliner *soops, ListBase *lb, char *name, int flags, TreeElement *prev, int *prevFound)
{
  TreeElement *te, *tes;

  for (te = lb->first; te; te = te->next) {
    int found = outliner_filter_has_name(te, name, flags);

    if (found) {
      /* name is right, but is element the previous one? */
      if (prev) {
        if ((te != prev) && (*prevFound))
          return te;
        if (te == prev) {
          *prevFound = 1;
        }
      }
      else
        return te;
    }

    tes = outliner_find_name(soops, &te->subtree, name, flags, prev, prevFound);
    if (tes)
      return tes;
  }

  /* nothing valid found */
  return NULL;
}

static void outliner_find_panel(
    Scene *UNUSED(scene), ARegion *ar, SpaceOutliner *soops, int again, int flags)
{
  ReportList *reports = NULL;  // CTX_wm_reports(C);
  TreeElement *te = NULL;
  TreeElement *last_find;
  TreeStoreElem *tselem;
  int ytop, xdelta, prevFound = 0;
  char name[sizeof(soops->search_string)];

  /* get last found tree-element based on stored search_tse */
  last_find = outliner_find_tse(soops, &soops->search_tse);

  /* determine which type of search to do */
  if (again && last_find) {
    /* no popup panel - previous + user wanted to search for next after previous */
    BLI_strncpy(name, soops->search_string, sizeof(name));
    flags = soops->search_flags;

    /* try to find matching element */
    te = outliner_find_name(soops, &soops->tree, name, flags, last_find, &prevFound);
    if (te == NULL) {
      /* no more matches after previous, start from beginning again */
      prevFound = 1;
      te = outliner_find_name(soops, &soops->tree, name, flags, last_find, &prevFound);
    }
  }
  else {
    /* pop up panel - no previous, or user didn't want search after previous */
    name[0] = '\0';
    // XXX      if (sbutton(name, 0, sizeof(name) - 1, "Find: ") && name[0]) {
    //          te = outliner_find_name(soops, &soops->tree, name, flags, NULL, &prevFound);
    //      }
    //      else return; /* XXX RETURN! XXX */
  }

  /* do selection and reveal */
  if (te) {
    tselem = TREESTORE(te);
    if (tselem) {
      /* expand branches so that it will be visible, we need to get correct coordinates */
      if (outliner_open_back(soops, te))
        outliner_set_coordinates(ar, soops);

      /* deselect all visible, and select found element */
      outliner_flag_set(soops, &soops->tree, TSE_SELECTED, 0);
      tselem->flag |= TSE_SELECTED;

      /* make te->ys center of view */
      ytop = (int)(te->ys + BLI_rctf_size_y(&ar->v2d.mask) / 2);
      if (ytop > 0)
        ytop = 0;
      ar->v2d.cur.ymax = (float)ytop;
      ar->v2d.cur.ymin = (float)(ytop - BLI_rctf_size_y(&ar->v2d.mask));

      /* make te->xs ==> te->xend center of view */
      xdelta = (int)(te->xs - ar->v2d.cur.xmin);
      ar->v2d.cur.xmin += xdelta;
      ar->v2d.cur.xmax += xdelta;

      /* store selection */
      soops->search_tse = *tselem;

      BLI_strncpy(soops->search_string, name, sizeof(soops->search_string));
      soops->search_flags = flags;

      /* redraw */
      ED_region_tag_redraw_no_rebuild(ar);
    }
  }
  else {
    /* no tree-element found */
    BKE_reportf(reports, RPT_WARNING, "Not found: %s", name);
  }
}
#endif

/* Show One Level ----------------------------------------------- */

/* helper function for Show/Hide one level operator */
static void outliner_openclose_level(ListBase *lb, int curlevel, int level, int open)
{
  TreeElement *te;
  TreeStoreElem *tselem;

  for (te = lb->first; te; te = te->next) {
    tselem = TREESTORE(te);

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

static int outliner_one_level_exec(bContext *C, wmOperator *op)
{
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  ARegion *ar = CTX_wm_region(C);
  const bool add = RNA_boolean_get(op->ptr, "open");
  int level;

  level = outliner_flag_is_any_test(&soops->tree, TSE_CLOSED, 1);
  if (add == 1) {
    if (level) {
      outliner_openclose_level(&soops->tree, 1, level, 1);
    }
  }
  else {
    if (level == 0) {
      level = outliner_count_levels(&soops->tree, 0);
    }
    if (level) {
      outliner_openclose_level(&soops->tree, 1, level - 1, 0);
    }
  }

  ED_region_tag_redraw(ar);

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
  ot->poll = ED_operator_outliner_active;

  /* no undo or registry, UI option */

  /* properties */
  prop = RNA_def_boolean(ot->srna, "open", 1, "Open", "Expand all entries one level deep");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/* Show Hierarchy ----------------------------------------------- */

/* Helper function for tree_element_shwo_hierarchy() -
 * recursively checks whether subtrees have any objects. */
static int subtree_has_objects(ListBase *lb)
{
  TreeElement *te;
  TreeStoreElem *tselem;

  for (te = lb->first; te; te = te->next) {
    tselem = TREESTORE(te);
    if (tselem->type == 0 && te->idcode == ID_OB) {
      return 1;
    }
    if (subtree_has_objects(&te->subtree)) {
      return 1;
    }
  }
  return 0;
}

/* recursive helper function for Show Hierarchy operator */
static void tree_element_show_hierarchy(Scene *scene, SpaceOutliner *soops, ListBase *lb)
{
  TreeElement *te;
  TreeStoreElem *tselem;

  /* open all object elems, close others */
  for (te = lb->first; te; te = te->next) {
    tselem = TREESTORE(te);

    if (tselem->type == 0) {
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

    if (TSELEM_OPEN(tselem, soops)) {
      tree_element_show_hierarchy(scene, soops, &te->subtree);
    }
  }
}

/* show entire object level hierarchy */
static int outliner_show_hierarchy_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  ARegion *ar = CTX_wm_region(C);
  Scene *scene = CTX_data_scene(C);

  /* recursively open/close levels */
  tree_element_show_hierarchy(scene, soops, &soops->tree);

  ED_region_tag_redraw(ar);

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
  ot->poll = ED_operator_outliner_active;  //  TODO: shouldn't be allowed in RNA views...

  /* no undo or registry, UI option */
}

/* ************************************************************** */
/* ANIMATO OPERATIONS */
/* KeyingSet and Driver Creation - Helper functions */

/* specialized poll callback for these operators to work in Datablocks view only */
static bool ed_operator_outliner_datablocks_active(bContext *C)
{
  ScrArea *sa = CTX_wm_area(C);
  if ((sa) && (sa->spacetype == SPACE_OUTLINER)) {
    SpaceOutliner *so = CTX_wm_space_outliner(C);
    return (so->outlinevis == SO_DATA_API);
  }
  return 0;
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
                                 short *UNUSED(groupmode))
{
  ListBase hierarchy = {NULL, NULL};
  LinkData *ld;
  TreeElement *tem, *temnext, *temsub;
  TreeStoreElem *tse /* , *tsenext */ /* UNUSED */;
  PointerRNA *ptr, *nextptr;
  PropertyRNA *prop;
  char *newpath = NULL;

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
  for (tem = te->parent; tem; tem = tem->parent) {
    ld = MEM_callocN(sizeof(LinkData), "LinkData for tree_element_to_path()");
    ld->data = tem;
    BLI_addhead(&hierarchy, ld);
  }

  /* step 2: step down hierarchy building the path
   * (NOTE: addhead in previous loop was needed so that we can loop like this) */
  for (ld = hierarchy.first; ld; ld = ld->next) {
    /* get data */
    tem = (TreeElement *)ld->data;
    tse = TREESTORE(tem);
    ptr = &tem->rnaptr;
    prop = tem->directdata;

    /* check if we're looking for first ID, or appending to path */
    if (*id) {
      /* just 'append' property to path
       * - to prevent memory leaks, we must write to newpath not path,
       *   then free old path + swap them.
       */
      if (tse->type == TSE_RNA_PROPERTY) {
        if (RNA_property_type(prop) == PROP_POINTER) {
          /* for pointer we just append property name */
          newpath = RNA_path_append(*path, ptr, prop, 0, NULL);
        }
        else if (RNA_property_type(prop) == PROP_COLLECTION) {
          char buf[128], *name;

          temnext = (TreeElement *)(ld->next->data);
          /* tsenext = TREESTORE(temnext); */ /* UNUSED */

          nextptr = &temnext->rnaptr;
          name = RNA_struct_name_get_alloc(nextptr, buf, sizeof(buf), NULL);

          if (name) {
            /* if possible, use name as a key in the path */
            newpath = RNA_path_append(*path, NULL, prop, 0, name);

            if (name != buf) {
              MEM_freeN(name);
            }
          }
          else {
            /* otherwise use index */
            int index = 0;

            for (temsub = tem->subtree.first; temsub; temsub = temsub->next, index++) {
              if (temsub == temnext) {
                break;
              }
            }
            newpath = RNA_path_append(*path, NULL, prop, index, NULL);
          }

          ld = ld->next;
        }
      }

      if (newpath) {
        if (*path) {
          MEM_freeN(*path);
        }
        *path = newpath;
        newpath = NULL;
      }
    }
    else {
      /* no ID, so check if entry is RNA-struct,
       * and if that RNA-struct is an ID datablock to extract info from. */
      if (tse->type == TSE_RNA_STRUCT) {
        /* ptr->data not ptr->id.data seems to be the one we want,
         * since ptr->data is sometimes the owner of this ID? */
        if (RNA_struct_is_ID(ptr->type)) {
          *id = (ID *)ptr->data;

          /* clear path */
          if (*path) {
            MEM_freeN(*path);
            path = NULL;
          }
        }
      }
    }
  }

  /* step 3: if we've got an ID, add the current item to the path */
  if (*id) {
    /* add the active property to the path */
    ptr = &te->rnaptr;
    prop = te->directdata;

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
    newpath = RNA_path_append(*path, NULL, prop, 0, NULL);
    if (*path) {
      MEM_freeN(*path);
    }
    *path = newpath;
  }

  /* free temp data */
  BLI_freelistN(&hierarchy);
}

/* =============================================== */
/* Driver Operations */

/* These operators are only available in databrowser mode for now, as
 * they depend on having RNA paths and/or hierarchies available.
 */
enum {
  DRIVERS_EDITMODE_ADD = 0,
  DRIVERS_EDITMODE_REMOVE,
} /*eDrivers_EditModes*/;

/* Utilities ---------------------------------- */

/* Recursively iterate over tree, finding and working on selected items */
static void do_outliner_drivers_editop(SpaceOutliner *soops,
                                       ListBase *tree,
                                       ReportList *reports,
                                       short mode)
{
  TreeElement *te;
  TreeStoreElem *tselem;

  for (te = tree->first; te; te = te->next) {
    tselem = TREESTORE(te);

    /* if item is selected, perform operation */
    if (tselem->flag & TSE_SELECTED) {
      ID *id = NULL;
      char *path = NULL;
      int array_index = 0;
      short flag = 0;
      short groupmode = KSP_GROUP_KSNAME;

      /* check if RNA-property described by this selected element is an animatable prop */
      if (ELEM(tselem->type, TSE_RNA_PROPERTY, TSE_RNA_ARRAY_ELEM) &&
          RNA_property_animateable(&te->rnaptr, te->directdata)) {
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
          arraylen = RNA_property_array_length(&te->rnaptr, te->directdata);
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
              /* remove driver matching the information obtained (only if valid) */
              ANIM_remove_driver(reports, id, path, array_index, dflags);
              break;
            }
          }
        }

        /* free path, since it had to be generated */
        MEM_freeN(path);
      }
    }

    /* go over sub-tree */
    if (TSELEM_OPEN(tselem, soops)) {
      do_outliner_drivers_editop(soops, &te->subtree, reports, mode);
    }
  }
}

/* Add Operator ---------------------------------- */

static int outliner_drivers_addsel_exec(bContext *C, wmOperator *op)
{
  SpaceOutliner *soutliner = CTX_wm_space_outliner(C);

  /* check for invalid states */
  if (soutliner == NULL) {
    return OPERATOR_CANCELLED;
  }

  /* recursively go into tree, adding selected items */
  do_outliner_drivers_editop(soutliner, &soutliner->tree, op->reports, DRIVERS_EDITMODE_ADD);

  /* send notifiers */
  WM_event_add_notifier(C, NC_ANIMATION | ND_FCURVES_ORDER, NULL);  // XXX

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_drivers_add_selected(wmOperatorType *ot)
{
  /* api callbacks */
  ot->idname = "OUTLINER_OT_drivers_add_selected";
  ot->name = "Add Drivers for Selected";
  ot->description = "Add drivers to selected items";

  /* api callbacks */
  ot->exec = outliner_drivers_addsel_exec;
  ot->poll = ed_operator_outliner_datablocks_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* Remove Operator ---------------------------------- */

static int outliner_drivers_deletesel_exec(bContext *C, wmOperator *op)
{
  SpaceOutliner *soutliner = CTX_wm_space_outliner(C);

  /* check for invalid states */
  if (soutliner == NULL) {
    return OPERATOR_CANCELLED;
  }

  /* recursively go into tree, adding selected items */
  do_outliner_drivers_editop(soutliner, &soutliner->tree, op->reports, DRIVERS_EDITMODE_REMOVE);

  /* send notifiers */
  WM_event_add_notifier(C, ND_KEYS, NULL);  // XXX

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_drivers_delete_selected(wmOperatorType *ot)
{
  /* identifiers */
  ot->idname = "OUTLINER_OT_drivers_delete_selected";
  ot->name = "Delete Drivers for Selected";
  ot->description = "Delete drivers assigned to selected items";

  /* api callbacks */
  ot->exec = outliner_drivers_deletesel_exec;
  ot->poll = ed_operator_outliner_datablocks_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* =============================================== */
/* Keying Set Operations */

/* These operators are only available in databrowser mode for now, as
 * they depend on having RNA paths and/or hierarchies available.
 */
enum {
  KEYINGSET_EDITMODE_ADD = 0,
  KEYINGSET_EDITMODE_REMOVE,
} /*eKeyingSet_EditModes*/;

/* Utilities ---------------------------------- */

/* find the 'active' KeyingSet, and add if not found (if adding is allowed) */
// TODO: should this be an API func?
static KeyingSet *verify_active_keyingset(Scene *scene, short add)
{
  KeyingSet *ks = NULL;

  /* sanity check */
  if (scene == NULL) {
    return NULL;
  }

  /* try to find one from scene */
  if (scene->active_keyingset > 0) {
    ks = BLI_findlink(&scene->keyingsets, scene->active_keyingset - 1);
  }

  /* add if none found */
  // XXX the default settings have yet to evolve
  if ((add) && (ks == NULL)) {
    ks = BKE_keyingset_add(&scene->keyingsets, NULL, NULL, KEYINGSET_ABSOLUTE, 0);
    scene->active_keyingset = BLI_listbase_count(&scene->keyingsets);
  }

  return ks;
}

/* Recursively iterate over tree, finding and working on selected items */
static void do_outliner_keyingset_editop(SpaceOutliner *soops,
                                         KeyingSet *ks,
                                         ListBase *tree,
                                         short mode)
{
  TreeElement *te;
  TreeStoreElem *tselem;

  for (te = tree->first; te; te = te->next) {
    tselem = TREESTORE(te);

    /* if item is selected, perform operation */
    if (tselem->flag & TSE_SELECTED) {
      ID *id = NULL;
      char *path = NULL;
      int array_index = 0;
      short flag = 0;
      short groupmode = KSP_GROUP_KSNAME;

      /* check if RNA-property described by this selected element is an animatable prop */
      if (ELEM(tselem->type, TSE_RNA_PROPERTY, TSE_RNA_ARRAY_ELEM) &&
          RNA_property_animateable(&te->rnaptr, te->directdata)) {
        /* get id + path + index info from the selected element */
        tree_element_to_path(te, tselem, &id, &path, &array_index, &flag, &groupmode);
      }

      /* only if ID and path were set, should we perform any actions */
      if (id && path) {
        /* action depends on mode */
        switch (mode) {
          case KEYINGSET_EDITMODE_ADD: {
            /* add a new path with the information obtained (only if valid) */
            /* TODO: what do we do with group name?
             * for now, we don't supply one, and just let this use the KeyingSet name */
            BKE_keyingset_add_path(ks, id, NULL, path, array_index, flag, groupmode);
            ks->active_path = BLI_listbase_count(&ks->paths);
            break;
          }
          case KEYINGSET_EDITMODE_REMOVE: {
            /* find the relevant path, then remove it from the KeyingSet */
            KS_Path *ksp = BKE_keyingset_find_path(ks, id, NULL, path, array_index, groupmode);

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
    }

    /* go over sub-tree */
    if (TSELEM_OPEN(tselem, soops)) {
      do_outliner_keyingset_editop(soops, ks, &te->subtree, mode);
    }
  }
}

/* Add Operator ---------------------------------- */

static int outliner_keyingset_additems_exec(bContext *C, wmOperator *op)
{
  SpaceOutliner *soutliner = CTX_wm_space_outliner(C);
  Scene *scene = CTX_data_scene(C);
  KeyingSet *ks = verify_active_keyingset(scene, 1);

  /* check for invalid states */
  if (ks == NULL) {
    BKE_report(op->reports, RPT_ERROR, "Operation requires an active keying set");
    return OPERATOR_CANCELLED;
  }
  if (soutliner == NULL) {
    return OPERATOR_CANCELLED;
  }

  /* recursively go into tree, adding selected items */
  do_outliner_keyingset_editop(soutliner, ks, &soutliner->tree, KEYINGSET_EDITMODE_ADD);

  /* send notifiers */
  WM_event_add_notifier(C, NC_SCENE | ND_KEYINGSET, NULL);

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_keyingset_add_selected(wmOperatorType *ot)
{
  /* identifiers */
  ot->idname = "OUTLINER_OT_keyingset_add_selected";
  ot->name = "Keying Set Add Selected";
  ot->description = "Add selected items (blue-gray rows) to active Keying Set";

  /* api callbacks */
  ot->exec = outliner_keyingset_additems_exec;
  ot->poll = ed_operator_outliner_datablocks_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* Remove Operator ---------------------------------- */

static int outliner_keyingset_removeitems_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceOutliner *soutliner = CTX_wm_space_outliner(C);
  Scene *scene = CTX_data_scene(C);
  KeyingSet *ks = verify_active_keyingset(scene, 1);

  /* check for invalid states */
  if (soutliner == NULL) {
    return OPERATOR_CANCELLED;
  }

  /* recursively go into tree, adding selected items */
  do_outliner_keyingset_editop(soutliner, ks, &soutliner->tree, KEYINGSET_EDITMODE_REMOVE);

  /* send notifiers */
  WM_event_add_notifier(C, NC_SCENE | ND_KEYINGSET, NULL);

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_keyingset_remove_selected(wmOperatorType *ot)
{
  /* identifiers */
  ot->idname = "OUTLINER_OT_keyingset_remove_selected";
  ot->name = "Keying Set Remove Selected";
  ot->description = "Remove selected items (blue-gray rows) from active Keying Set";

  /* api callbacks */
  ot->exec = outliner_keyingset_removeitems_exec;
  ot->poll = ed_operator_outliner_datablocks_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ************************************************************** */
/* ORPHANED DATABLOCKS */

static bool ed_operator_outliner_id_orphans_active(bContext *C)
{
  ScrArea *sa = CTX_wm_area(C);
  if ((sa) && (sa->spacetype == SPACE_OUTLINER)) {
    SpaceOutliner *so = CTX_wm_space_outliner(C);
    return (so->outlinevis == SO_ID_ORPHANS);
  }
  return 0;
}

/* Purge Orphans Operator --------------------------------------- */

static void outliner_orphans_purge_tag(ID *id, int *num_tagged)
{
  if (id->us == 0) {
    id->tag |= LIB_TAG_DOIT;
    num_tagged[INDEX_ID_NULL]++;
    num_tagged[BKE_idcode_to_index(GS(id->name))]++;
  }
  else {
    id->tag &= ~LIB_TAG_DOIT;
  }
}

static int outliner_orphans_purge_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(evt))
{
  Main *bmain = CTX_data_main(C);
  int num_tagged[INDEX_ID_MAX] = {0};

  /* Tag all IDs having zero users. */
  ID *id;
  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    outliner_orphans_purge_tag(id, num_tagged);
  }
  FOREACH_MAIN_ID_END;
  RNA_int_set(op->ptr, "num_deleted", num_tagged[INDEX_ID_NULL]);

  if (num_tagged[INDEX_ID_NULL] == 0) {
    BKE_report(op->reports, RPT_INFO, "No orphanned data-blocks to purge");
    return OPERATOR_CANCELLED;
  }

  DynStr *dyn_str = BLI_dynstr_new();
  BLI_dynstr_append(dyn_str, "Purging unused data-blocks (");
  bool is_first = true;
  for (int i = 0; i < INDEX_ID_MAX - 2; i++) {
    if (num_tagged[i] != 0) {
      if (!is_first) {
        BLI_dynstr_append(dyn_str, ", ");
      }
      else {
        is_first = false;
      }
      BLI_dynstr_appendf(dyn_str,
                         "%d %s",
                         num_tagged[i],
                         TIP_(BKE_idcode_to_name_plural(BKE_idcode_from_index(i))));
    }
  }
  BLI_dynstr_append(dyn_str, TIP_("). Click here to proceed..."));

  char *message = BLI_dynstr_get_cstring(dyn_str);
  int ret = WM_operator_confirm_message(C, op, message);

  MEM_freeN(message);
  BLI_dynstr_free(dyn_str);
  return ret;
}

static int outliner_orphans_purge_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  int num_tagged[INDEX_ID_MAX] = {0};

  if ((num_tagged[INDEX_ID_NULL] = RNA_int_get(op->ptr, "num_deleted")) == 0) {
    /* Tag all IDs having zero users. */
    ID *id;
    FOREACH_MAIN_ID_BEGIN (bmain, id) {
      outliner_orphans_purge_tag(id, num_tagged);
    }
    FOREACH_MAIN_ID_END;

    if (num_tagged[INDEX_ID_NULL] == 0) {
      BKE_report(op->reports, RPT_INFO, "No orphanned data-blocks to purge");
      return OPERATOR_CANCELLED;
    }
  }

  BKE_id_multi_tagged_delete(bmain);

  BKE_reportf(op->reports, RPT_INFO, "Deleted %d data-blocks", num_tagged[INDEX_ID_NULL]);

  /* XXX: tree management normally happens from draw_outliner(), but when
   *      you're clicking to fast on Delete object from context menu in
   *      outliner several mouse events can be handled in one cycle without
   *      handling notifiers/redraw which leads to deleting the same object twice.
   *      cleanup tree here to prevent such cases. */
  outliner_cleanup_tree(soops);

  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_ID | NA_EDITED, NULL);
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_OUTLINER, NULL);
  return OPERATOR_FINISHED;
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
  ot->poll = ed_operator_outliner_id_orphans_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  PropertyRNA *prop = RNA_def_int(ot->srna, "num_deleted", 0, 0, INT_MAX, "", "", 0, INT_MAX);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
}
