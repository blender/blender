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
 */

/** \file
 * \ingroup edscene
 */

#include <stdio.h>
#include <string.h>

#include "BLI_compiler_attrs.h"
#include "BLI_listbase.h"
#include "BLI_string.h"

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
#include "ED_screen.h"
#include "ED_util.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

Scene *ED_scene_add(Main *bmain, bContext *C, wmWindow *win, eSceneCopyMethod method)
{
  Scene *scene_new;

  if (method == SCE_COPY_NEW) {
    scene_new = BKE_scene_add(bmain, DATA_("Scene"));
  }
  else { /* different kinds of copying */
    Scene *scene_old = WM_window_get_active_scene(win);

    /* We are going to deep-copy collections, objects and various object data, we need to have
     * up-to-date obdata for that. */
    if (method == SCE_COPY_FULL) {
      ED_editors_flush_edits(bmain);
    }

    scene_new = BKE_scene_copy(bmain, scene_old, method);
  }

  WM_window_set_active_scene(bmain, C, win, scene_new);

  WM_event_add_notifier(C, NC_SCENE | ND_SCENEBROWSE, scene_new);

  return scene_new;
}

/**
 * \note Only call outside of area/region loops
 * \return true if successful
 */
bool ED_scene_delete(bContext *C, Main *bmain, Scene *scene)
{
  Scene *scene_new;

  /* kill running jobs */
  wmWindowManager *wm = bmain->wm.first;
  WM_jobs_kill_type(wm, scene, WM_JOB_TYPE_ANY);

  if (scene->id.prev) {
    scene_new = scene->id.prev;
  }
  else if (scene->id.next) {
    scene_new = scene->id.next;
  }
  else {
    return false;
  }

  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    if (win->parent != NULL) { /* We only care about main windows here... */
      continue;
    }
    if (win->scene == scene) {
      WM_window_set_active_scene(bmain, C, win, scene_new);
    }
  }

  BKE_id_delete(bmain, scene);

  return true;
}

/* Depsgraph updates after scene becomes active in a window. */
void ED_scene_change_update(Main *bmain, Scene *scene, ViewLayer *layer)
{
  Depsgraph *depsgraph = BKE_scene_get_depsgraph(bmain, scene, layer, true);

  BKE_scene_set_background(bmain, scene);
  DEG_graph_relations_update(depsgraph, bmain, scene, layer);
  DEG_on_visible_update(bmain, false);

  ED_render_engine_changed(bmain);
  ED_update_for_newframe(bmain, depsgraph);
}

static bool view_layer_remove_poll(const Scene *scene, const ViewLayer *layer)
{
  const int act = BLI_findindex(&scene->view_layers, layer);

  if (act == -1) {
    return false;
  }
  else if ((scene->view_layers.first == scene->view_layers.last) &&
           (scene->view_layers.first == layer)) {
    /* ensure 1 layer is kept */
    return false;
  }

  return true;
}

static void view_layer_remove_unset_nodetrees(const Main *bmain, Scene *scene, ViewLayer *layer)
{
  int act_layer_index = BLI_findindex(&scene->view_layers, layer);

  for (Scene *sce = bmain->scenes.first; sce; sce = sce->id.next) {
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

  /* We need to unset nodetrees before removing the layer, otherwise its index will be -1. */
  view_layer_remove_unset_nodetrees(bmain, scene, layer);

  BLI_remlink(&scene->view_layers, layer);
  BLI_assert(BLI_listbase_is_empty(&scene->view_layers) == false);

  /* Remove from windows. */
  wmWindowManager *wm = bmain->wm.first;
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    if (win->scene == scene && STREQ(win->view_layer_name, layer->name)) {
      ViewLayer *first_layer = BKE_view_layer_default_view(scene);
      STRNCPY(win->view_layer_name, first_layer->name);
    }
  }

  BKE_scene_free_view_layer_depsgraph(scene, layer);

  BKE_view_layer_free(layer);

  DEG_id_tag_update(&scene->id, 0);
  DEG_relations_tag_update(bmain);
  WM_main_add_notifier(NC_SCENE | ND_LAYER | NA_REMOVED, scene);

  return true;
}

static int scene_new_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  wmWindow *win = CTX_wm_window(C);
  int type = RNA_enum_get(op->ptr, "type");

  ED_scene_add(bmain, C, win, type);

  return OPERATOR_FINISHED;
}

static void SCENE_OT_new(wmOperatorType *ot)
{
  static EnumPropertyItem type_items[] = {
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
      {0, NULL, 0, NULL, NULL},
  };

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
  ot->prop = RNA_def_enum(ot->srna, "type", type_items, 0, "Type", "");
}

static bool scene_delete_poll(bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  return (scene->id.prev || scene->id.next);
}

static int scene_delete_exec(bContext *C, wmOperator *UNUSED(op))
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

void ED_operatortypes_scene(void)
{
  WM_operatortype_append(SCENE_OT_new);
  WM_operatortype_append(SCENE_OT_delete);
}
