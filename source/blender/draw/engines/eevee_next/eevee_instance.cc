/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation.
 */

/** \file
 * \ingroup eevee
 *
 * An instance contains all structures needed to do a complete render.
 */

#include "BKE_global.h"
#include "BKE_object.h"
#include "BLI_rect.h"
#include "DEG_depsgraph_query.h"
#include "DNA_ID.h"
#include "DNA_lightprobe_types.h"
#include "DNA_modifier_types.h"

#include "eevee_instance.hh"

namespace blender::eevee {

/* -------------------------------------------------------------------- */
/** \name Initialization
 *
 * Initialization functions need to be called once at the start of a frame.
 * Active camera, render extent and enabled render passes are immutable until next init.
 * This takes care of resizing output buffers and view in case a parameter changed.
 * IMPORTANT: xxx.init() functions are NOT meant to acquire and allocate DRW resources.
 * Any attempt to do so will likely produce use after free situations.
 * \{ */

void Instance::init(const int2 &output_res,
                    const rcti *output_rect,
                    RenderEngine *render_,
                    Depsgraph *depsgraph_,
                    const LightProbe *light_probe_,
                    Object *camera_object_,
                    const RenderLayer *render_layer_,
                    const DRWView *drw_view_,
                    const View3D *v3d_,
                    const RegionView3D *rv3d_)
{
  UNUSED_VARS(light_probe_, camera_object_, output_rect);
  render = render_;
  depsgraph = depsgraph_;
  render_layer = render_layer_;
  drw_view = drw_view_;
  v3d = v3d_;
  rv3d = rv3d_;

  update_eval_members();

  main_view.init(output_res);
}

void Instance::update_eval_members()
{
  scene = DEG_get_evaluated_scene(depsgraph);
  view_layer = DEG_get_evaluated_view_layer(depsgraph);
  // camera_eval_object = (camera_orig_object) ?
  //                          DEG_get_evaluated_object(depsgraph, camera_orig_object) :
  //                          nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sync
 *
 * Sync will gather data from the scene that can change over a time step (i.e: motion steps).
 * IMPORTANT: xxx.sync() functions area responsible for creating DRW resources (i.e: DRWView) as
 * well as querying temp texture pool. All DRWPasses should be ready by the end end_sync().
 * \{ */

void Instance::begin_sync()
{
  materials.begin_sync();

  pipelines.sync();
  main_view.sync();
  world.sync();
}

void Instance::object_sync(Object *ob)
{
  const bool is_renderable_type = ELEM(ob->type, OB_CURVES, OB_GPENCIL, OB_MESH);
  const int ob_visibility = DRW_object_visibility_in_active_context(ob);
  const bool partsys_is_visible = (ob_visibility & OB_VISIBLE_PARTICLES) != 0 &&
                                  (ob->type == OB_MESH);
  const bool object_is_visible = DRW_object_is_renderable(ob) &&
                                 (ob_visibility & OB_VISIBLE_SELF) != 0;

  if (!is_renderable_type || (!partsys_is_visible && !object_is_visible)) {
    return;
  }

  ObjectHandle &ob_handle = sync.sync_object(ob);

  if (partsys_is_visible && ob != DRW_context_state_get()->object_edit) {
    LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
      if (md->type == eModifierType_ParticleSystem) {
        sync.sync_curves(ob, ob_handle, md);
      }
    }
  }

  if (object_is_visible) {
    switch (ob->type) {
      case OB_LAMP:
        break;
      case OB_MESH:
      case OB_CURVES_LEGACY:
      case OB_SURF:
      case OB_FONT:
      case OB_MBALL: {
        sync.sync_mesh(ob, ob_handle);
        break;
      }
      case OB_VOLUME:
        break;
      case OB_CURVES:
        sync.sync_curves(ob, ob_handle);
        break;
      case OB_GPENCIL:
        sync.sync_gpencil(ob, ob_handle);
        break;
      default:
        break;
    }
  }

  ob_handle.reset_recalc_flag();
}

void Instance::end_sync()
{
}

void Instance::render_sync()
{
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Rendering
 * \{ */

/**
 * Conceptually renders one sample per pixel.
 * Everything based on random sampling should be done here (i.e: DRWViews jitter)
 **/
void Instance::render_sample()
{
  main_view.render();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Interface
 * \{ */

void Instance::render_frame(RenderLayer *render_layer, const char *view_name)
{
  UNUSED_VARS(render_layer, view_name);
}

void Instance::draw_viewport(DefaultFramebufferList *dfbl)
{
  UNUSED_VARS(dfbl);
  render_sample();
}

/** \} */

}  // namespace blender::eevee
