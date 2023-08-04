/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edscene
 */

#include <cstdio>
#include <cstring>

#include "BLI_compiler_attrs.h"
#include "BLI_listbase.h"
#include "BLI_string.h"

#include "DNA_sequence_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_report.h"
#include "BKE_scene.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "BLT_translation.h"

#include "ED_object.h"
#include "ED_render.h"
#include "ED_scene.h"
#include "ED_screen.hh"
#include "ED_util.hh"

#include "SEQ_relations.h"
#include "SEQ_select.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.hh"
#include "WM_types.hh"

/* -------------------------------------------------------------------- */
/** \name Scene Utilities
 * \{ */

static Scene *scene_add(Main *bmain, Scene *scene_old, eSceneCopyMethod method)
{
  Scene *scene_new = nullptr;
  if (method == SCE_COPY_NEW) {
    scene_new = BKE_scene_add(bmain, DATA_("Scene"));
  }
  else { /* different kinds of copying */
    /* We are going to deep-copy collections, objects and various object data, we need to have
     * up-to-date obdata for that. */
    if (method == SCE_COPY_FULL) {
      ED_editors_flush_edits(bmain);
    }

    scene_new = BKE_scene_duplicate(bmain, scene_old, method);
  }

  return scene_new;
}

Scene *ED_scene_sequencer_add(Main *bmain,
                              bContext *C,
                              eSceneCopyMethod method,
                              const bool assign_strip)
{
  Sequence *seq = nullptr;
  Scene *scene_active = CTX_data_scene(C);
  Scene *scene_strip = nullptr;
  /* Sequencer need to use as base the scene defined in the strip, not the main scene. */
  Editing *ed = scene_active->ed;
  if (ed) {
    seq = ed->act_seq;
    if (seq && seq->scene) {
      scene_strip = seq->scene;
    }
  }

  /* If no scene assigned to the strip, only NEW scene mode is logic. */
  if (scene_strip == nullptr) {
    method = SCE_COPY_NEW;
  }

  Scene *scene_new = scene_add(bmain, scene_strip, method);

  /* If don't need assign the scene to the strip, nothing else to do. */
  if (!assign_strip) {
    return scene_new;
  }

  /* As the scene is created in sequencer, do not set the new scene as active.
   * This is useful for story-boarding where we want to keep actual scene active.
   * The new scene is linked to the active strip and the viewport updated. */
  if (scene_new && seq) {
    seq->scene = scene_new;
    /* Do a refresh of the sequencer data. */
    SEQ_relations_invalidate_cache_raw(scene_active, seq);
    DEG_id_tag_update(&scene_active->id, ID_RECALC_AUDIO | ID_RECALC_SEQUENCER_STRIPS);
    DEG_relations_tag_update(bmain);
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene_active);
  WM_event_add_notifier(C, NC_SCENE | ND_SCENEBROWSE, scene_active);

  return scene_new;
}

Scene *ED_scene_add(Main *bmain, bContext *C, wmWindow *win, eSceneCopyMethod method)
{
  Scene *scene_old = WM_window_get_active_scene(win);
  Scene *scene_new = scene_add(bmain, scene_old, method);

  WM_window_set_active_scene(bmain, C, win, scene_new);

  WM_event_add_notifier(C, NC_SCENE | ND_SCENEBROWSE, scene_new);

  return scene_new;
}

bool ED_scene_delete(bContext *C, Main *bmain, Scene *scene)
{
  Scene *scene_new;

  /* kill running jobs */
  wmWindowManager *wm = static_cast<wmWindowManager *>(bmain->wm.first);
  WM_jobs_kill_type(wm, scene, WM_JOB_TYPE_ANY);

  if (scene->id.prev) {
    scene_new = static_cast<Scene *>(scene->id.prev);
  }
  else if (scene->id.next) {
    scene_new = static_cast<Scene *>(scene->id.next);
  }
  else {
    return false;
  }

  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    if (win->parent != nullptr) { /* We only care about main windows here... */
      continue;
    }
    if (win->scene == scene) {
      WM_window_set_active_scene(bmain, C, win, scene_new);
    }
  }

  BKE_id_delete(bmain, scene);

  return true;
}

void ED_scene_change_update(Main *bmain, Scene *scene, ViewLayer *layer)
{
  Depsgraph *depsgraph = BKE_scene_ensure_depsgraph(bmain, scene, layer);

  BKE_scene_set_background(bmain, scene);
  DEG_graph_relations_update(depsgraph);
  DEG_tag_on_visible_update(bmain, false);

  ED_render_engine_changed(bmain, false);
  ED_update_for_newframe(bmain, depsgraph);
}

static bool view_layer_remove_poll(const Scene *scene, const ViewLayer *layer)
{
  const int act = BLI_findindex(&scene->view_layers, layer);

  if (act == -1) {
    return false;
  }
  if ((scene->view_layers.first == scene->view_layers.last) && (scene->view_layers.first == layer))
  {
    /* ensure 1 layer is kept */
    return false;
  }

  return true;
}

static void view_layer_remove_unset_nodetrees(const Main *bmain, Scene *scene, ViewLayer *layer)
{
  int act_layer_index = BLI_findindex(&scene->view_layers, layer);

  for (Scene *sce = static_cast<Scene *>(bmain->scenes.first); sce;
       sce = static_cast<Scene *>(sce->id.next))
  {
    if (sce->nodetree) {
      BKE_nodetree_remove_layer_n(sce->nodetree, scene, act_layer_index);
    }
  }
}

bool ED_scene_view_layer_delete(Main *bmain, Scene *scene, ViewLayer *layer, ReportList *reports)
{
  if (view_layer_remove_poll(scene, layer) == false) {
    if (reports) {
      BKE_reportf(reports,
                  RPT_ERROR,
                  "View layer '%s' could not be removed from scene '%s'",
                  layer->name,
                  scene->id.name + 2);
    }

    return false;
  }

  /* We need to unset node-trees before removing the layer, otherwise its index will be -1. */
  view_layer_remove_unset_nodetrees(bmain, scene, layer);

  BLI_remlink(&scene->view_layers, layer);
  BLI_assert(BLI_listbase_is_empty(&scene->view_layers) == false);

  /* Remove from windows. */
  wmWindowManager *wm = static_cast<wmWindowManager *>(bmain->wm.first);
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    if (win->scene == scene && STREQ(win->view_layer_name, layer->name)) {
      ViewLayer *first_layer = BKE_view_layer_default_view(scene);
      STRNCPY(win->view_layer_name, first_layer->name);
    }
  }

  BKE_scene_free_view_layer_depsgraph(scene, layer);

  BKE_view_layer_free(layer);

  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);
  DEG_relations_tag_update(bmain);
  WM_main_add_notifier(NC_SCENE | ND_LAYER | NA_REMOVED, scene);

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Scene New Operator
 * \{ */

static int scene_new_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  wmWindow *win = CTX_wm_window(C);
  int type = RNA_enum_get(op->ptr, "type");

  ED_scene_add(bmain, C, win, eSceneCopyMethod(type));

  return OPERATOR_FINISHED;
}

static EnumPropertyItem scene_new_items[] = {
    {SCE_COPY_NEW, "NEW", 0, "New", "Add a new, empty scene with default settings"},
    {SCE_COPY_EMPTY,
     "EMPTY",
     0,
     "Copy Settings",
     "Add a new, empty scene, and copy settings from the current scene"},
    {SCE_COPY_LINK_COLLECTION,
     "LINK_COPY",
     0,
     "Linked Copy",
     "Link in the collections from the current scene (shallow copy)"},
    {SCE_COPY_FULL, "FULL_COPY", 0, "Full Copy", "Make a full copy of the current scene"},
    {0, nullptr, 0, nullptr, nullptr},
};

static void SCENE_OT_new(wmOperatorType *ot)
{

  /* identifiers */
  ot->name = "New Scene";
  ot->description = "Add new scene by type";
  ot->idname = "SCENE_OT_new";

  /* api callbacks */
  ot->exec = scene_new_exec;
  ot->invoke = WM_menu_invoke;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", scene_new_items, SCE_COPY_NEW, "Type", "");
  RNA_def_property_translation_context(ot->prop, BLT_I18NCONTEXT_ID_SCENE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Scene New Sequencer Operator
 * \{ */

static int scene_new_sequencer_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  int type = RNA_enum_get(op->ptr, "type");

  if (ED_scene_sequencer_add(bmain, C, eSceneCopyMethod(type), true) == nullptr) {
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_FINISHED;
}

static bool scene_new_sequencer_poll(bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  const Sequence *seq = SEQ_select_active_get(scene);
  return (seq && (seq->type == SEQ_TYPE_SCENE));
}

static const EnumPropertyItem *scene_new_sequencer_enum_itemf(bContext *C,
                                                              PointerRNA * /*ptr*/,
                                                              PropertyRNA * /*prop*/,
                                                              bool *r_free)
{
  EnumPropertyItem *item = nullptr;
  int totitem = 0;
  uint item_index;

  item_index = RNA_enum_from_value(scene_new_items, SCE_COPY_NEW);
  RNA_enum_item_add(&item, &totitem, &scene_new_items[item_index]);

  bool has_scene_or_no_context = false;
  if (C == nullptr) {
    /* For documentation generation. */
    has_scene_or_no_context = true;
  }
  else {
    Scene *scene = CTX_data_scene(C);
    Sequence *seq = SEQ_select_active_get(scene);
    if (seq && (seq->type == SEQ_TYPE_SCENE) && (seq->scene != nullptr)) {
      has_scene_or_no_context = true;
    }
  }

  if (has_scene_or_no_context) {
    int values[] = {SCE_COPY_EMPTY, SCE_COPY_LINK_COLLECTION, SCE_COPY_FULL};
    for (int i = 0; i < ARRAY_SIZE(values); i++) {
      item_index = RNA_enum_from_value(scene_new_items, values[i]);
      RNA_enum_item_add(&item, &totitem, &scene_new_items[item_index]);
    }
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;
  return item;
}

static void SCENE_OT_new_sequencer(wmOperatorType *ot)
{

  /* identifiers */
  ot->name = "New Scene";
  ot->description = "Add new scene by type in the sequence editor and assign to active strip";
  ot->idname = "SCENE_OT_new_sequencer";

  /* api callbacks */
  ot->exec = scene_new_sequencer_exec;
  ot->invoke = WM_menu_invoke;
  ot->poll = scene_new_sequencer_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", scene_new_items, SCE_COPY_NEW, "Type", "");
  RNA_def_enum_funcs(ot->prop, scene_new_sequencer_enum_itemf);
  RNA_def_property_flag(ot->prop, PROP_ENUM_NO_TRANSLATE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Scene Delete Operator
 * \{ */

static bool scene_delete_poll(bContext *C)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  return BKE_scene_can_be_removed(bmain, scene);
}

static int scene_delete_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);

  if (ED_scene_delete(C, CTX_data_main(C), scene) == false) {
    return OPERATOR_CANCELLED;
  }

  if (G.debug & G_DEBUG) {
    printf("scene delete %p\n", scene);
  }

  WM_event_add_notifier(C, NC_SCENE | NA_REMOVED, scene);

  return OPERATOR_FINISHED;
}

static void SCENE_OT_delete(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete Scene";
  ot->description = "Delete active scene";
  ot->idname = "SCENE_OT_delete";

  /* api callbacks */
  ot->exec = scene_delete_exec;
  ot->poll = scene_delete_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Registration
 * \{ */

void ED_operatortypes_scene()
{
  WM_operatortype_append(SCENE_OT_new);
  WM_operatortype_append(SCENE_OT_delete);
  WM_operatortype_append(SCENE_OT_new_sequencer);
}

/** \} */
