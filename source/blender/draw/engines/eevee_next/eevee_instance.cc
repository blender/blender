/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 * An instance contains all structures needed to do a complete render.
 */

#include <sstream>

#include "BKE_global.h"
#include "BKE_object.h"
#include "BLI_rect.h"
#include "DEG_depsgraph_query.h"
#include "DNA_ID.h"
#include "DNA_lightprobe_types.h"
#include "DNA_modifier_types.h"
#include "IMB_imbuf_types.h"
#include "RE_pipeline.h"

#include "eevee_engine.h"
#include "eevee_instance.hh"

#include "DNA_particle_types.h"

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
                    Object *camera_object_,
                    const RenderLayer *render_layer_,
                    const DRWView *drw_view_,
                    const View3D *v3d_,
                    const RegionView3D *rv3d_)
{
  render = render_;
  depsgraph = depsgraph_;
  camera_orig_object = camera_object_;
  render_layer = render_layer_;
  drw_view = drw_view_;
  v3d = v3d_;
  rv3d = rv3d_;
  manager = DRW_manager_get();

  if (assign_if_different(debug_mode, (eDebugMode)G.debug_value)) {
    sampling.reset();
  }

  info = "";

  update_eval_members();

  sampling.init(scene);
  camera.init();
  film.init(output_res, output_rect);
  ambient_occlusion.init();
  velocity.init();
  raytracing.init();
  depth_of_field.init();
  shadows.init();
  motion_blur.init();
  main_view.init();
  /* Irradiance Cache needs reflection probes to be initialized. */
  reflection_probes.init();
  irradiance_cache.init();
  volume.init();
}

void Instance::init_light_bake(Depsgraph *depsgraph, draw::Manager *manager)
{
  this->depsgraph = depsgraph;
  this->manager = manager;
  camera_orig_object = nullptr;
  render = nullptr;
  render_layer = nullptr;
  drw_view = nullptr;
  v3d = nullptr;
  rv3d = nullptr;

  is_light_bake = true;
  debug_mode = (eDebugMode)G.debug_value;
  info = "";

  update_eval_members();

  sampling.init(scene);
  camera.init();
  /* Film isn't used but init to avoid side effects in other module. */
  rcti empty_rect{0, 0, 0, 0};
  film.init(int2(1), &empty_rect);
  velocity.init();
  depth_of_field.init();
  shadows.init();
  main_view.init();
  /* Irradiance Cache needs reflection probes to be initialized. */
  reflection_probes.init();
  irradiance_cache.init();
}

void Instance::set_time(float time)
{
  BLI_assert(render);
  DRW_render_set_time(render, depsgraph, floorf(time), fractf(time));
  update_eval_members();
}

void Instance::update_eval_members()
{
  scene = DEG_get_evaluated_scene(depsgraph);
  view_layer = DEG_get_evaluated_view_layer(depsgraph);
  camera_eval_object = (camera_orig_object) ?
                           DEG_get_evaluated_object(depsgraph, camera_orig_object) :
                           nullptr;
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
  velocity.begin_sync(); /* NOTE: Also syncs camera. */
  lights.begin_sync();
  shadows.begin_sync();
  volume.begin_sync();
  pipelines.begin_sync();
  cryptomatte.begin_sync();
  reflection_probes.begin_sync();
  light_probes.begin_sync();

  gpencil_engine_enabled = false;

  scene_sync();

  depth_of_field.sync();
  raytracing.sync();
  motion_blur.sync();
  hiz_buffer.sync();
  main_view.sync();
  world.sync();
  film.sync();
  render_buffers.sync();
  ambient_occlusion.sync();
  irradiance_cache.sync();
}

void Instance::scene_sync()
{
  SceneHandle &sc_handle = sync.sync_scene(scene);

  sc_handle.reset_recalc_flag();

  /* This refers specifically to the Scene camera that can be accessed
   * via View Layer Attribute nodes, rather than the actual render camera. */
  if (scene->camera != nullptr) {
    ObjectHandle &ob_handle = sync.sync_object(scene->camera);

    ob_handle.reset_recalc_flag();
  }
}

void Instance::object_sync(Object *ob)
{
  const bool is_renderable_type = ELEM(ob->type,
                                       OB_CURVES,
                                       OB_GPENCIL_LEGACY,
                                       OB_MESH,
                                       OB_POINTCLOUD,
                                       OB_VOLUME,
                                       OB_LAMP,
                                       OB_LIGHTPROBE);
  const int ob_visibility = DRW_object_visibility_in_active_context(ob);
  const bool partsys_is_visible = (ob_visibility & OB_VISIBLE_PARTICLES) != 0 &&
                                  (ob->type == OB_MESH);
  const bool object_is_visible = DRW_object_is_renderable(ob) &&
                                 (ob_visibility & OB_VISIBLE_SELF) != 0;

  if (!is_renderable_type || (!partsys_is_visible && !object_is_visible)) {
    return;
  }

  /* TODO cleanup. */
  ObjectRef ob_ref = DRW_object_ref_get(ob);
  ResourceHandle res_handle = manager->resource_handle(ob_ref);

  ObjectHandle &ob_handle = sync.sync_object(ob);

  if (partsys_is_visible && ob != DRW_context_state_get()->object_edit) {
    auto sync_hair =
        [&](ObjectHandle hair_handle, ModifierData &md, ParticleSystem &particle_sys) {
          ResourceHandle _res_handle = manager->resource_handle(float4x4(ob->object_to_world));
          sync.sync_curves(ob, hair_handle, _res_handle, &md, &particle_sys);
        };
    foreach_hair_particle_handle(ob, ob_handle, sync_hair);
  }

  if (object_is_visible) {
    switch (ob->type) {
      case OB_LAMP:
        lights.sync_light(ob, ob_handle);
        break;
      case OB_MESH:
        if (!sync.sync_sculpt(ob, ob_handle, res_handle, ob_ref)) {
          sync.sync_mesh(ob, ob_handle, res_handle, ob_ref);
        }
        break;
      case OB_POINTCLOUD:
        sync.sync_point_cloud(ob, ob_handle, res_handle, ob_ref);
        break;
      case OB_VOLUME:
        volume.sync_object(ob, ob_handle, res_handle);
        break;
      case OB_CURVES:
        sync.sync_curves(ob, ob_handle, res_handle);
        break;
      case OB_GPENCIL_LEGACY:
        sync.sync_gpencil(ob, ob_handle, res_handle);
        break;
      case OB_LIGHTPROBE:
        sync.sync_light_probe(ob, ob_handle);
        break;
      default:
        break;
    }
  }

  ob_handle.reset_recalc_flag();
}

/* Wrapper to use with DRW_render_object_iter. */
void Instance::object_sync_render(void *instance_,
                                  Object *ob,
                                  RenderEngine *engine,
                                  Depsgraph *depsgraph)
{
  UNUSED_VARS(engine, depsgraph);
  Instance &inst = *reinterpret_cast<Instance *>(instance_);

  if (inst.visibility_collection != nullptr) {
    bool object_part_of_group = BKE_collection_has_object(inst.visibility_collection, ob);
    if (object_part_of_group == inst.visibility_collection_invert) {
      return;
    }
  }
  inst.object_sync(ob);
}

void Instance::end_sync()
{
  velocity.end_sync();
  shadows.end_sync(); /** \note: Needs to be before lights. */
  lights.end_sync();
  sampling.end_sync();
  subsurface.end_sync();
  film.end_sync();
  cryptomatte.end_sync();
  pipelines.end_sync();
  light_probes.end_sync();
  reflection_probes.end_sync();
  volume.end_sync();
}

void Instance::render_sync()
{
  /* TODO: Remove old draw manager calls. */
  DRW_cache_restart();

  manager->begin_sync();

  begin_sync();
  DRW_render_object_iter(this, render, depsgraph, object_sync_render);
  velocity.geometry_steps_fill();
  end_sync();

  manager->end_sync();

  /* TODO: Remove old draw manager calls. */
  DRW_render_instance_buffer_finish();

  DRW_curves_update();
}

bool Instance::do_probe_sync() const
{
  if (materials.queued_shaders_count > 0) {
    return false;
  }
  if (!reflection_probes.update_probes_this_sample_) {
    return false;
  }
  return true;
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
  if (sampling.finished_viewport()) {
    film.display();
    return;
  }

  /* Motion blur may need to do re-sync after a certain number of sample. */
  if (!is_viewport() && sampling.do_render_sync()) {
    render_sync();
  }

  sampling.step();

  capture_view.render_world();
  capture_view.render_probes();

  main_view.render();

  motion_blur.step();
}

void Instance::render_read_result(RenderLayer *render_layer, const char *view_name)
{
  eViewLayerEEVEEPassType pass_bits = film.enabled_passes_get();
  for (auto i : IndexRange(EEVEE_RENDER_PASS_MAX_BIT)) {
    eViewLayerEEVEEPassType pass_type = eViewLayerEEVEEPassType(pass_bits & (1 << i));
    if (pass_type == 0) {
      continue;
    }

    Vector<std::string> pass_names = Film::pass_to_render_pass_names(pass_type, view_layer);
    for (int64_t pass_offset : IndexRange(pass_names.size())) {
      RenderPass *rp = RE_pass_find_by_name(
          render_layer, pass_names[pass_offset].c_str(), view_name);
      if (!rp) {
        continue;
      }
      float *result = film.read_pass(pass_type, pass_offset);

      if (result) {
        BLI_mutex_lock(&render->update_render_passes_mutex);
        /* WORKAROUND: We use texture read to avoid using a frame-buffer to get the render result.
         * However, on some implementation, we need a buffer with a few extra bytes for the read to
         * happen correctly (see #GLTexture::read()). So we need a custom memory allocation. */
        /* Avoid `memcpy()`, replace the pointer directly. */
        RE_pass_set_buffer_data(rp, result);
        BLI_mutex_unlock(&render->update_render_passes_mutex);
      }
    }
  }

  /* AOVs. */
  LISTBASE_FOREACH (ViewLayerAOV *, aov, &view_layer->aovs) {
    if ((aov->flag & AOV_CONFLICT) != 0) {
      continue;
    }
    RenderPass *rp = RE_pass_find_by_name(render_layer, aov->name, view_name);
    if (!rp) {
      continue;
    }
    float *result = film.read_aov(aov);

    if (result) {
      BLI_mutex_lock(&render->update_render_passes_mutex);
      /* WORKAROUND: We use texture read to avoid using a frame-buffer to get the render result.
       * However, on some implementation, we need a buffer with a few extra bytes for the read to
       * happen correctly (see GLTexture::read()). So we need a custom memory allocation. */
      /* Avoid memcpy(), replace the pointer directly. */
      RE_pass_set_buffer_data(rp, result);
      BLI_mutex_unlock(&render->update_render_passes_mutex);
    }
  }

  /* The vector pass is initialized to weird values. Set it to neutral value if not rendered. */
  if ((pass_bits & EEVEE_RENDER_PASS_VECTOR) == 0) {
    for (std::string vector_pass_name :
         Film::pass_to_render_pass_names(EEVEE_RENDER_PASS_VECTOR, view_layer))
    {
      RenderPass *vector_rp = RE_pass_find_by_name(
          render_layer, vector_pass_name.c_str(), view_name);
      if (vector_rp) {
        memset(vector_rp->ibuf->float_buffer.data,
               0,
               sizeof(float) * 4 * vector_rp->rectx * vector_rp->recty);
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Interface
 * \{ */

void Instance::render_frame(RenderLayer *render_layer, const char *view_name)
{
  /* TODO(jbakker): should we check on the subtype as well? Now it also populates even when there
   * are other light probes in the scene. */
  if (DEG_id_type_any_exists(this->depsgraph, ID_LP)) {
    reflection_probes.update_probes_next_sample_ = true;
  }

  while (!sampling.finished()) {
    this->render_sample();

    /* TODO(fclem) print progression. */
#if 0
    /* TODO(fclem): Does not currently work. But would be better to just display to 2D view like
     * cycles does. */
    if (G.background == false && first_read) {
      /* Allow to preview the first sample. */
      /* TODO(fclem): Might want to not do this during animation render to avoid too much stall. */
      this->render_read_result(render_layer, view_name);
      first_read = false;
      DRW_render_context_disable(render->re);
      /* Allow the 2D viewport to grab the ticket mutex to display the render. */
      DRW_render_context_enable(render->re);
    }
#endif
  }

  this->film.cryptomatte_sort();

  this->render_read_result(render_layer, view_name);
}

void Instance::draw_viewport(DefaultFramebufferList *dfbl)
{
  UNUSED_VARS(dfbl);
  render_sample();
  velocity.step_swap();

  /* Do not request redraw during viewport animation to lock the frame-rate to the animation
   * playback rate. This is in order to preserve motion blur aspect and also to avoid TAA reset
   * that can show flickering. */
  if (!sampling.finished_viewport() && !DRW_state_is_playback()) {
    DRW_viewport_request_redraw();
  }

  if (materials.queued_shaders_count > 0) {
    std::stringstream ss;
    ss << "Compiling Shaders (" << materials.queued_shaders_count << " remaining)";
    info = ss.str();
  }
}

void Instance::store_metadata(RenderResult *render_result)
{
  cryptomatte.store_metadata(render_result);
}

void Instance::update_passes(RenderEngine *engine, Scene *scene, ViewLayer *view_layer)
{
  RE_engine_register_pass(engine, scene, view_layer, RE_PASSNAME_COMBINED, 4, "RGBA", SOCK_RGBA);

#define CHECK_PASS_LEGACY(name, type, channels, chanid) \
  if (view_layer->passflag & (SCE_PASS_##name)) { \
    RE_engine_register_pass( \
        engine, scene, view_layer, RE_PASSNAME_##name, channels, chanid, type); \
  } \
  ((void)0)
#define CHECK_PASS_EEVEE(name, type, channels, chanid) \
  if (view_layer->eevee.render_passes & (EEVEE_RENDER_PASS_##name)) { \
    RE_engine_register_pass( \
        engine, scene, view_layer, RE_PASSNAME_##name, channels, chanid, type); \
  } \
  ((void)0)

  CHECK_PASS_LEGACY(Z, SOCK_FLOAT, 1, "Z");
  CHECK_PASS_LEGACY(MIST, SOCK_FLOAT, 1, "Z");
  CHECK_PASS_LEGACY(NORMAL, SOCK_VECTOR, 3, "XYZ");
  CHECK_PASS_LEGACY(DIFFUSE_DIRECT, SOCK_RGBA, 3, "RGB");
  CHECK_PASS_LEGACY(DIFFUSE_COLOR, SOCK_RGBA, 3, "RGB");
  CHECK_PASS_LEGACY(GLOSSY_DIRECT, SOCK_RGBA, 3, "RGB");
  CHECK_PASS_LEGACY(GLOSSY_COLOR, SOCK_RGBA, 3, "RGB");
  CHECK_PASS_EEVEE(VOLUME_LIGHT, SOCK_RGBA, 3, "RGB");
  CHECK_PASS_LEGACY(EMIT, SOCK_RGBA, 3, "RGB");
  CHECK_PASS_LEGACY(ENVIRONMENT, SOCK_RGBA, 3, "RGB");
  CHECK_PASS_LEGACY(SHADOW, SOCK_RGBA, 3, "RGB");
  CHECK_PASS_LEGACY(AO, SOCK_RGBA, 3, "RGB");

  LISTBASE_FOREACH (ViewLayerAOV *, aov, &view_layer->aovs) {
    if ((aov->flag & AOV_CONFLICT) != 0) {
      continue;
    }
    switch (aov->type) {
      case AOV_TYPE_COLOR:
        RE_engine_register_pass(engine, scene, view_layer, aov->name, 4, "RGBA", SOCK_RGBA);
        break;
      case AOV_TYPE_VALUE:
        RE_engine_register_pass(engine, scene, view_layer, aov->name, 1, "X", SOCK_FLOAT);
        break;
      default:
        break;
    }
  }

  /* NOTE: Name channels lowercase `rgba` so that compression rules check in OpenEXR DWA code uses
   * lossless compression. Reportedly this naming is the only one which works good from the
   * interoperability point of view. Using `xyzw` naming is not portable. */
  auto register_cryptomatte_passes = [&](eViewLayerCryptomatteFlags cryptomatte_layer,
                                         eViewLayerEEVEEPassType eevee_pass) {
    if (view_layer->cryptomatte_flag & cryptomatte_layer) {
      for (std::string pass_name : Film::pass_to_render_pass_names(eevee_pass, view_layer)) {
        RE_engine_register_pass(
            engine, scene, view_layer, pass_name.c_str(), 4, "rgba", SOCK_RGBA);
      }
    }
  };
  register_cryptomatte_passes(VIEW_LAYER_CRYPTOMATTE_OBJECT, EEVEE_RENDER_PASS_CRYPTOMATTE_OBJECT);
  register_cryptomatte_passes(VIEW_LAYER_CRYPTOMATTE_ASSET, EEVEE_RENDER_PASS_CRYPTOMATTE_ASSET);
  register_cryptomatte_passes(VIEW_LAYER_CRYPTOMATTE_MATERIAL,
                              EEVEE_RENDER_PASS_CRYPTOMATTE_MATERIAL);
}

void Instance::light_bake_irradiance(
    Object &probe,
    FunctionRef<void()> context_enable,
    FunctionRef<void()> context_disable,
    FunctionRef<bool()> stop,
    FunctionRef<void(LightProbeGridCacheFrame *, float progress)> result_update)
{
  BLI_assert(is_baking());

  auto custom_pipeline_wrapper = [&](FunctionRef<void()> callback) {
    context_enable();
    DRW_custom_pipeline_begin(&draw_engine_eevee_next_type, depsgraph);
    callback();
    DRW_custom_pipeline_end();
    context_disable();
  };

  auto context_wrapper = [&](FunctionRef<void()> callback) {
    context_enable();
    callback();
    context_disable();
  };

  irradiance_cache.bake.init(probe);

  custom_pipeline_wrapper([&]() {
    const ::LightProbe *light_probe = static_cast<const ::LightProbe *>(probe.data);

    visibility_collection = light_probe->visibility_grp;
    visibility_collection_invert = (light_probe->flag & LIGHTPROBE_FLAG_INVERT_GROUP) != 0;

    manager->begin_sync();
    render_sync();
    manager->end_sync();

    capture_view.render_world();

    irradiance_cache.bake.surfels_create(probe);
    irradiance_cache.bake.surfels_lights_eval();

    irradiance_cache.bake.clusters_build();
    irradiance_cache.bake.irradiance_offset();
  });

  sampling.init(probe);
  while (!sampling.finished()) {
    context_wrapper([&]() {
      /* Batch ray cast by pack of 16. Avoids too much overhead of the update function & context
       * switch. */
      /* TODO(fclem): Could make the number of iteration depend on the computation time. */
      for (int i = 0; i < 16 && !sampling.finished(); i++) {
        sampling.step();

        irradiance_cache.bake.raylists_build();
        irradiance_cache.bake.propagate_light();
        irradiance_cache.bake.irradiance_capture();
      }

      if (sampling.finished()) {
        /* TODO(fclem): Dilation, filter etc... */
        // irradiance_cache.bake.irradiance_finalize();
      }

      LightProbeGridCacheFrame *cache_frame;
      if (sampling.finished()) {
        cache_frame = irradiance_cache.bake.read_result_packed();
      }
      else {
        /* TODO(fclem): Only do this read-back if needed. But it might be tricky to know when. */
        cache_frame = irradiance_cache.bake.read_result_unpacked();
      }

      float progress = sampling.sample_index() / float(sampling.sample_count());
      result_update(cache_frame, progress);
    });

    if (stop()) {
      return;
    }
  }
}

/** \} */

}  // namespace blender::eevee
