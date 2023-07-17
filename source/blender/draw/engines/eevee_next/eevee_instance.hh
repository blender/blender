/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 * An renderer instance that contains all data to render a full frame.
 */

#pragma once

#include "BKE_object.h"
#include "DEG_depsgraph.h"
#include "DNA_lightprobe_types.h"
#include "DRW_render.h"

#include "eevee_ambient_occlusion.hh"
#include "eevee_camera.hh"
#include "eevee_cryptomatte.hh"
#include "eevee_depth_of_field.hh"
#include "eevee_film.hh"
#include "eevee_gbuffer.hh"
#include "eevee_hizbuffer.hh"
#include "eevee_irradiance_cache.hh"
#include "eevee_light.hh"
#include "eevee_lightprobe.hh"
#include "eevee_lookdev.hh"
#include "eevee_material.hh"
#include "eevee_motion_blur.hh"
#include "eevee_pipeline.hh"
#include "eevee_reflection_probes.hh"
#include "eevee_renderbuffers.hh"
#include "eevee_sampling.hh"
#include "eevee_shader.hh"
#include "eevee_shadow.hh"
#include "eevee_subsurface.hh"
#include "eevee_sync.hh"
#include "eevee_view.hh"
#include "eevee_world.hh"

namespace blender::eevee {

/**
 * \class Instance
 * \brief A running instance of the engine.
 */
class Instance {
  friend VelocityModule;
  friend MotionBlurModule;

 public:
  ShaderModule &shaders;
  SyncModule sync;
  MaterialModule materials;
  SubsurfaceModule subsurface;
  PipelineModule pipelines;
  ShadowModule shadows;
  LightModule lights;
  AmbientOcclusion ambient_occlusion;
  ReflectionProbeModule reflection_probes;
  VelocityModule velocity;
  MotionBlurModule motion_blur;
  DepthOfField depth_of_field;
  Cryptomatte cryptomatte;
  GBuffer gbuffer;
  HiZBuffer hiz_buffer;
  Sampling sampling;
  Camera camera;
  Film film;
  RenderBuffers render_buffers;
  MainView main_view;
  CaptureView capture_view;
  World world;
  LookdevModule lookdev;
  LightProbeModule light_probes;
  IrradianceCache irradiance_cache;

  /** Input data. */
  Depsgraph *depsgraph;
  Manager *manager;
  /** Evaluated IDs. */
  Scene *scene;
  ViewLayer *view_layer;
  /** Camera object if rendering through a camera. nullptr otherwise. */
  Object *camera_eval_object;
  Object *camera_orig_object;
  /** Only available when rendering for final render. */
  const RenderLayer *render_layer;
  RenderEngine *render;
  /** Only available when rendering for viewport. */
  const DRWView *drw_view;
  const View3D *v3d;
  const RegionView3D *rv3d;

  /** True if the grease pencil engine might be running. */
  bool gpencil_engine_enabled;
  /** True if the instance is created for light baking. */
  bool is_light_bake = false;

  /** Info string displayed at the top of the render / viewport. */
  std::string info = "";
  /** Debug mode from debug value. */
  eDebugMode debug_mode = eDebugMode::DEBUG_NONE;

 public:
  Instance()
      : shaders(*ShaderModule::module_get()),
        sync(*this),
        materials(*this),
        subsurface(*this),
        pipelines(*this),
        shadows(*this),
        lights(*this),
        ambient_occlusion(*this),
        reflection_probes(*this),
        velocity(*this),
        motion_blur(*this),
        depth_of_field(*this),
        cryptomatte(*this),
        hiz_buffer(*this),
        sampling(*this),
        camera(*this),
        film(*this),
        render_buffers(*this),
        main_view(*this),
        capture_view(*this),
        world(*this),
        lookdev(*this),
        light_probes(*this),
        irradiance_cache(*this){};
  ~Instance(){};

  /* Render & Viewport. */
  /* TODO(fclem): Split for clarity. */
  void init(const int2 &output_res,
            const rcti *output_rect,
            RenderEngine *render,
            Depsgraph *depsgraph,
            Object *camera_object = nullptr,
            const RenderLayer *render_layer = nullptr,
            const DRWView *drw_view = nullptr,
            const View3D *v3d = nullptr,
            const RegionView3D *rv3d = nullptr);

  void begin_sync();
  void object_sync(Object *ob);
  void end_sync();

  /**
   * Return true when probe pipeline is used during this sample.
   */
  bool do_probe_sync() const;

  /* Render. */

  void render_sync();
  void render_frame(RenderLayer *render_layer, const char *view_name);
  void store_metadata(RenderResult *render_result);

  /* Viewport. */

  void draw_viewport(DefaultFramebufferList *dfbl);

  /* Light bake. */

  void init_light_bake(Depsgraph *depsgraph, draw::Manager *manager);
  void light_bake_irradiance(
      Object &probe,
      FunctionRef<void()> context_enable,
      FunctionRef<void()> context_disable,
      FunctionRef<bool()> stop,
      FunctionRef<void(LightProbeGridCacheFrame *, float progress)> result_update);

  static void update_passes(RenderEngine *engine, Scene *scene, ViewLayer *view_layer);

  bool is_viewport() const
  {
    return render == nullptr && !is_baking();
  }

  bool is_baking() const
  {
    return is_light_bake;
  }

  bool overlays_enabled() const
  {
    return v3d && ((v3d->flag2 & V3D_HIDE_OVERLAYS) == 0);
  }

  bool use_scene_lights() const
  {
    return (!v3d) ||
           ((v3d->shading.type == OB_MATERIAL) &&
            (v3d->shading.flag & V3D_SHADING_SCENE_LIGHTS)) ||
           ((v3d->shading.type == OB_RENDER) &&
            (v3d->shading.flag & V3D_SHADING_SCENE_LIGHTS_RENDER));
  }

  /* Light the scene using the selected HDRI in the viewport shading pop-over. */
  bool use_studio_light() const
  {
    return (v3d) && (((v3d->shading.type == OB_MATERIAL) &&
                      ((v3d->shading.flag & V3D_SHADING_SCENE_WORLD) == 0)) ||
                     ((v3d->shading.type == OB_RENDER) &&
                      ((v3d->shading.flag & V3D_SHADING_SCENE_WORLD_RENDER) == 0)));
  }

 private:
  static void object_sync_render(void *instance_,
                                 Object *ob,
                                 RenderEngine *engine,
                                 Depsgraph *depsgraph);
  void render_sample();
  void render_read_result(RenderLayer *render_layer, const char *view_name);

  void scene_sync();
  void mesh_sync(Object *ob, ObjectHandle &ob_handle);

  void update_eval_members();

  void set_time(float time);
};

}  // namespace blender::eevee
