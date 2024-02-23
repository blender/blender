/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edrend
 */

#include <cstdlib>
#include <cstring>

#include "DNA_cachefile_types.h"
#include "DNA_light_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_world_types.h"

#include "DRW_engine.hh"

#include "BLI_listbase.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_icons.h"
#include "BKE_main.hh"
#include "BKE_material.h"
#include "BKE_paint.hh"
#include "BKE_scene.hh"

#include "NOD_composite.hh"

#include "RE_engine.h"
#include "RE_pipeline.h"

#include "ED_node.hh"
#include "ED_node_preview.hh"
#include "ED_paint.hh"
#include "ED_render.hh"
#include "ED_view3d.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "WM_api.hh"

#include <cstdio>

/* -------------------------------------------------------------------- */
/** \name Render Engines
 * \{ */

void ED_render_view3d_update(Depsgraph *depsgraph,
                             wmWindow *window,
                             ScrArea *area,
                             const bool updated)
{
  Main *bmain = DEG_get_bmain(depsgraph);
  Scene *scene = DEG_get_input_scene(depsgraph);
  ViewLayer *view_layer = DEG_get_input_view_layer(depsgraph);

  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    if (region->regiontype != RGN_TYPE_WINDOW) {
      continue;
    }

    View3D *v3d = static_cast<View3D *>(area->spacedata.first);
    RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
    RenderEngine *engine = rv3d->view_render ? RE_view_engine_get(rv3d->view_render) : nullptr;

    /* call update if the scene changed, or if the render engine
     * tagged itself for update (e.g. because it was busy at the
     * time of the last update) */
    if (engine && (updated || (engine->flag & RE_ENGINE_DO_UPDATE))) {
      /* Create temporary context to execute callback in. */
      bContext *C = CTX_create();
      CTX_data_main_set(C, bmain);
      CTX_data_scene_set(C, scene);
      CTX_wm_manager_set(C, static_cast<wmWindowManager *>(bmain->wm.first));
      CTX_wm_window_set(C, window);
      CTX_wm_screen_set(C, WM_window_get_active_screen(window));
      CTX_wm_area_set(C, area);
      CTX_wm_region_set(C, region);

      engine->flag &= ~RE_ENGINE_DO_UPDATE;
      /* NOTE: Important to pass non-updated depsgraph, This is because this function is called
       * from inside dependency graph evaluation. Additionally, if we pass fully evaluated one
       * we will lose updates stored in the graph. */
      engine->type->view_update(engine, C, CTX_data_depsgraph_pointer(C));

      CTX_free(C);
    }

    if (!updated) {
      continue;
    }

    DRWUpdateContext drw_context = {nullptr};
    drw_context.bmain = bmain;
    drw_context.depsgraph = depsgraph;
    drw_context.scene = scene;
    drw_context.view_layer = view_layer;
    drw_context.region = region;
    drw_context.v3d = v3d;
    drw_context.engine_type = ED_view3d_engine_type(scene, v3d->shading.type);
    DRW_notify_view_update(&drw_context);
  }
}

void ED_render_scene_update(const DEGEditorUpdateContext *update_ctx, const bool updated)
{
  Main *bmain = update_ctx->bmain;
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

  /* Do not call if no WM available, see #42688. */
  if (BLI_listbase_is_empty(&bmain->wm)) {
    return;
  }

  recursive_check = true;

  wmWindowManager *wm = static_cast<wmWindowManager *>(bmain->wm.first);
  LISTBASE_FOREACH (wmWindow *, window, &wm->windows) {
    bScreen *screen = WM_window_get_active_screen(window);

    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      if (area->spacetype == SPACE_VIEW3D) {
        ED_render_view3d_update(update_ctx->depsgraph, window, area, updated);
      }
    }
  }

  recursive_check = false;
}

void ED_render_engine_area_exit(Main *bmain, ScrArea *area)
{
  /* clear all render engines in this area */
  wmWindowManager *wm = static_cast<wmWindowManager *>(bmain->wm.first);

  if (area->spacetype != SPACE_VIEW3D) {
    return;
  }

  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    if (region->regiontype != RGN_TYPE_WINDOW || !(region->regiondata)) {
      continue;
    }
    ED_view3d_stop_render_preview(wm, region);
  }
}

void ED_render_engine_changed(Main *bmain, const bool update_scene_data)
{
  /* on changing the render engine type, clear all running render engines */
  for (bScreen *screen = static_cast<bScreen *>(bmain->screens.first); screen;
       screen = static_cast<bScreen *>(screen->id.next))
  {
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      ED_render_engine_area_exit(bmain, area);
    }
  }
  /* Invalidate all shader previews. */
  blender::ed::space_node::stop_preview_job(*static_cast<wmWindowManager *>(bmain->wm.first));
  LISTBASE_FOREACH (Material *, ma, &bmain->materials) {
    BKE_material_make_node_previews_dirty(ma);
  }
  RE_FreePersistentData(nullptr);
  /* Inform all render engines and draw managers. */
  DEGEditorUpdateContext update_ctx = {nullptr};
  update_ctx.bmain = bmain;
  for (Scene *scene = static_cast<Scene *>(bmain->scenes.first); scene;
       scene = static_cast<Scene *>(scene->id.next))
  {
    update_ctx.scene = scene;
    LISTBASE_FOREACH (ViewLayer *, view_layer, &scene->view_layers) {
      /* TDODO(sergey): Iterate over depsgraphs instead? */
      update_ctx.depsgraph = BKE_scene_ensure_depsgraph(bmain, scene, view_layer);
      update_ctx.view_layer = view_layer;
      ED_render_id_flush_update(&update_ctx, &scene->id);
    }
    if (scene->nodetree && update_scene_data) {
      ntreeCompositUpdateRLayers(scene->nodetree);
    }
  }

  /* Update #CacheFiles to ensure that procedurals are properly taken into account. */
  LISTBASE_FOREACH (CacheFile *, cachefile, &bmain->cachefiles) {
    /* Only update cache-files which are set to use a render procedural.
     * We do not use #BKE_cachefile_uses_render_procedural here as we need to update regardless of
     * the current engine or its settings. */
    if (cachefile->use_render_procedural) {
      DEG_id_tag_update(&cachefile->id, ID_RECALC_SYNC_TO_EVAL);
      /* Rebuild relations so that modifiers are reconnected to or disconnected from the
       * cache-file. */
      DEG_relations_tag_update(bmain);
    }
  }
}

void ED_render_view_layer_changed(Main *bmain, bScreen *screen)
{
  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    ED_render_engine_area_exit(bmain, area);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Updates
 *
 * #ED_render_id_flush_update gets called from #DEG_id_tag_update,
 * to do editor level updates when the ID changes.
 * When these ID blocks are in the dependency graph,
 * we can get rid of the manual dependency checks.
 * \{ */

static void material_changed(Main * /*bmain*/, Material *ma)
{
  /* icons */
  BKE_icon_changed(BKE_icon_id_ensure(&ma->id));
}

static void lamp_changed(Main * /*bmain*/, Light *la)
{
  /* icons */
  BKE_icon_changed(BKE_icon_id_ensure(&la->id));
}

static void texture_changed(Main *bmain, Tex *tex)
{
  Scene *scene;

  /* icons */
  BKE_icon_changed(BKE_icon_id_ensure(&tex->id));

  for (scene = static_cast<Scene *>(bmain->scenes.first); scene;
       scene = static_cast<Scene *>(scene->id.next))
  {
    /* paint overlays */
    LISTBASE_FOREACH (ViewLayer *, view_layer, &scene->view_layers) {
      BKE_paint_invalidate_overlay_tex(scene, view_layer, tex);
    }
    /* find compositing nodes */
    if (scene->use_nodes && scene->nodetree) {
      LISTBASE_FOREACH (bNode *, node, &scene->nodetree->nodes) {
        if (node->id == &tex->id) {
          ED_node_tag_update_id(&scene->id);
        }
      }
    }
  }
}

static void world_changed(Main * /*bmain*/, World *wo)
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
  for (tex = static_cast<Tex *>(bmain->textures.first); tex;
       tex = static_cast<Tex *>(tex->id.next))
  {
    if (tex->type == TEX_IMAGE && tex->ima == ima) {
      texture_changed(bmain, tex);
    }
  }
}

static void scene_changed(Main *bmain, Scene *scene)
{
  Object *ob;

  /* glsl */
  for (ob = static_cast<Object *>(bmain->objects.first); ob;
       ob = static_cast<Object *>(ob->id.next))
  {
    if (ob->mode & OB_MODE_TEXTURE_PAINT) {
      BKE_texpaint_slots_refresh_object(scene, ob);
      ED_paint_proj_mesh_data_check(scene, ob, nullptr, nullptr, nullptr, nullptr);
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

/** \} */
