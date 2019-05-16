/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edrend
 */

#include <stdlib.h>
#include <string.h>

#include "DNA_light_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_workspace_types.h"
#include "DNA_world_types.h"
#include "DNA_windowmanager_types.h"

#include "DRW_engine.h"

#include "BLI_listbase.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_icons.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_node.h"
#include "BKE_paint.h"
#include "BKE_scene.h"
#include "BKE_workspace.h"

#include "RE_engine.h"
#include "RE_pipeline.h"

#include "ED_node.h"
#include "ED_render.h"
#include "ED_view3d.h"

#include "DEG_depsgraph.h"

#include "WM_api.h"

#include "render_intern.h"  // own include

/***************************** Render Engines ********************************/

void ED_render_scene_update(const DEGEditorUpdateContext *update_ctx, int updated)
{
  /* viewport rendering update on data changes, happens after depsgraph
   * updates if there was any change. context is set to the 3d view */
  Main *bmain = update_ctx->bmain;
  Scene *scene = update_ctx->scene;
  ViewLayer *view_layer = update_ctx->view_layer;
  bContext *C;
  wmWindowManager *wm;
  wmWindow *win;
  static bool recursive_check = false;

  /* don't do this render engine update if we're updating the scene from
   * other threads doing e.g. rendering or baking jobs */
  if (!BLI_thread_is_main()) {
    return;
  }

  /* don't call this recursively for frame updates */
  if (recursive_check) {
    return;
  }

  /* Do not call if no WM available, see T42688. */
  if (BLI_listbase_is_empty(&bmain->wm)) {
    return;
  }

  recursive_check = true;

  C = CTX_create();
  CTX_data_main_set(C, bmain);
  CTX_data_scene_set(C, scene);

  CTX_wm_manager_set(C, bmain->wm.first);
  wm = bmain->wm.first;

  for (win = wm->windows.first; win; win = win->next) {
    bScreen *sc = WM_window_get_active_screen(win);
    ScrArea *sa;
    ARegion *ar;

    CTX_wm_window_set(C, win);

    for (sa = sc->areabase.first; sa; sa = sa->next) {
      if (sa->spacetype != SPACE_VIEW3D) {
        continue;
      }
      View3D *v3d = sa->spacedata.first;
      for (ar = sa->regionbase.first; ar; ar = ar->next) {
        if (ar->regiontype != RGN_TYPE_WINDOW) {
          continue;
        }
        RegionView3D *rv3d = ar->regiondata;
        RenderEngine *engine = rv3d->render_engine;
        /* call update if the scene changed, or if the render engine
         * tagged itself for update (e.g. because it was busy at the
         * time of the last update) */
        if (engine && (updated || (engine->flag & RE_ENGINE_DO_UPDATE))) {

          CTX_wm_screen_set(C, sc);
          CTX_wm_area_set(C, sa);
          CTX_wm_region_set(C, ar);

          engine->flag &= ~RE_ENGINE_DO_UPDATE;
          /* NOTE: Important to pass non-updated depsgraph, This is because this function is called
           * from inside dependency graph evaluation. Additionally, if we pass fully evaluated one
           * we will loose updates stored in the graph. */
          engine->type->view_update(engine, C, CTX_data_depsgraph(C));
        }
        else {
          RenderEngineType *engine_type = ED_view3d_engine_type(scene, v3d->shading.type);
          if (updated) {
            DRW_notify_view_update((&(DRWUpdateContext){
                .bmain = bmain,
                .depsgraph = update_ctx->depsgraph,
                .scene = scene,
                .view_layer = view_layer,
                .ar = ar,
                .v3d = (View3D *)sa->spacedata.first,
                .engine_type = engine_type,
            }));
          }
        }
      }
    }
  }

  CTX_free(C);

  recursive_check = false;
}

void ED_render_engine_area_exit(Main *bmain, ScrArea *sa)
{
  /* clear all render engines in this area */
  ARegion *ar;
  wmWindowManager *wm = bmain->wm.first;

  if (sa->spacetype != SPACE_VIEW3D) {
    return;
  }

  for (ar = sa->regionbase.first; ar; ar = ar->next) {
    if (ar->regiontype != RGN_TYPE_WINDOW || !(ar->regiondata)) {
      continue;
    }
    ED_view3d_stop_render_preview(wm, ar);
  }
}

void ED_render_engine_changed(Main *bmain)
{
  /* on changing the render engine type, clear all running render engines */
  for (bScreen *sc = bmain->screens.first; sc; sc = sc->id.next) {
    for (ScrArea *sa = sc->areabase.first; sa; sa = sa->next) {
      ED_render_engine_area_exit(bmain, sa);
    }
  }
  RE_FreePersistentData();
  /* Inform all render engines and draw managers. */
  DEGEditorUpdateContext update_ctx = {NULL};
  update_ctx.bmain = bmain;
  for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
    update_ctx.scene = scene;
    LISTBASE_FOREACH (ViewLayer *, view_layer, &scene->view_layers) {
      /* TDODO(sergey): Iterate over depsgraphs instead? */
      update_ctx.depsgraph = BKE_scene_get_depsgraph(scene, view_layer, true);
      update_ctx.view_layer = view_layer;
      ED_render_id_flush_update(&update_ctx, &scene->id);
    }
    if (scene->nodetree) {
      ntreeCompositUpdateRLayers(scene->nodetree);
    }
  }
}

void ED_render_view_layer_changed(Main *bmain, bScreen *sc)
{
  for (ScrArea *sa = sc->areabase.first; sa; sa = sa->next) {
    ED_render_engine_area_exit(bmain, sa);
  }
}

/***************************** Updates ***********************************
 * ED_render_id_flush_update gets called from DEG_id_tag_update, to do   *
 * editor level updates when the ID changes. when these ID blocks are in *
 * the dependency graph, we can get rid of the manual dependency checks  */

static void material_changed(Main *UNUSED(bmain), Material *ma)
{
  /* icons */
  BKE_icon_changed(BKE_icon_id_ensure(&ma->id));
}

static void lamp_changed(Main *UNUSED(bmain), Light *la)
{
  /* icons */
  BKE_icon_changed(BKE_icon_id_ensure(&la->id));
}

static void texture_changed(Main *bmain, Tex *tex)
{
  Scene *scene;
  ViewLayer *view_layer;
  bNode *node;

  /* icons */
  BKE_icon_changed(BKE_icon_id_ensure(&tex->id));

  for (scene = bmain->scenes.first; scene; scene = scene->id.next) {
    /* paint overlays */
    for (view_layer = scene->view_layers.first; view_layer; view_layer = view_layer->next) {
      BKE_paint_invalidate_overlay_tex(scene, view_layer, tex);
    }
    /* find compositing nodes */
    if (scene->use_nodes && scene->nodetree) {
      for (node = scene->nodetree->nodes.first; node; node = node->next) {
        if (node->id == &tex->id) {
          ED_node_tag_update_id(&scene->id);
        }
      }
    }
  }
}

static void world_changed(Main *UNUSED(bmain), World *wo)
{
  /* icons */
  BKE_icon_changed(BKE_icon_id_ensure(&wo->id));
}

static void image_changed(Main *bmain, Image *ima)
{
  Tex *tex;

  /* icons */
  BKE_icon_changed(BKE_icon_id_ensure(&ima->id));

  /* textures */
  for (tex = bmain->textures.first; tex; tex = tex->id.next) {
    if (tex->type == TEX_IMAGE && tex->ima == ima) {
      texture_changed(bmain, tex);
    }
  }
}

static void scene_changed(Main *bmain, Scene *scene)
{
  Object *ob;

  /* glsl */
  for (ob = bmain->objects.first; ob; ob = ob->id.next) {
    if (ob->mode & OB_MODE_TEXTURE_PAINT) {
      BKE_texpaint_slots_refresh_object(scene, ob);
      BKE_paint_proj_mesh_data_check(scene, ob, NULL, NULL, NULL, NULL);
    }
  }
}

void ED_render_id_flush_update(const DEGEditorUpdateContext *update_ctx, ID *id)
{
  /* this can be called from render or baking thread when a python script makes
   * changes, in that case we don't want to do any editor updates, and making
   * GPU changes is not possible because OpenGL only works in the main thread */
  if (!BLI_thread_is_main()) {
    return;
  }
  Main *bmain = update_ctx->bmain;
  /* Internal ID update handlers. */
  switch (GS(id->name)) {
    case ID_MA:
      material_changed(bmain, (Material *)id);
      break;
    case ID_TE:
      texture_changed(bmain, (Tex *)id);
      break;
    case ID_WO:
      world_changed(bmain, (World *)id);
      break;
    case ID_LA:
      lamp_changed(bmain, (Light *)id);
      break;
    case ID_IM:
      image_changed(bmain, (Image *)id);
      break;
    case ID_SCE:
      scene_changed(bmain, (Scene *)id);
      break;
    default:
      break;
  }
}
