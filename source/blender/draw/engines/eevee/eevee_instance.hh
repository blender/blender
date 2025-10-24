/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 * An renderer instance that contains all data to render a full frame.
 */

#pragma once

#include <fmt/format.h>

#include "CLG_log.h"

#include "BLI_string.h"

#include "BLT_translation.hh"

#include "BKE_object.hh"

#include "DEG_depsgraph.hh"

#include "DNA_lightprobe_types.h"

#include "DRW_render.hh"

#include "eevee_ambient_occlusion.hh"
#include "eevee_camera.hh"
#include "eevee_cryptomatte.hh"
#include "eevee_debug_shared.hh"
#include "eevee_depth_of_field.hh"
#include "eevee_film.hh"
#include "eevee_gbuffer.hh"
#include "eevee_hizbuffer.hh"
#include "eevee_light.hh"
#include "eevee_lightprobe.hh"
#include "eevee_lightprobe_planar.hh"
#include "eevee_lightprobe_sphere.hh"
#include "eevee_lightprobe_volume.hh"
#include "eevee_lookdev.hh"
#include "eevee_material.hh"
#include "eevee_motion_blur.hh"
#include "eevee_pipeline.hh"
#include "eevee_raytrace.hh"
#include "eevee_renderbuffers.hh"
#include "eevee_sampling.hh"
#include "eevee_shader.hh"
#include "eevee_shadow.hh"
#include "eevee_subsurface.hh"
#include "eevee_sync.hh"
#include "eevee_view.hh"
#include "eevee_volume.hh"
#include "eevee_world.hh"

namespace blender::eevee {

using UniformDataBuf = draw::UniformBuffer<UniformData>;

/* Combines data from several modules to avoid wasting binding slots. */
struct UniformDataModule {
  UniformDataBuf data = {"UniformDataBuf"};

  void push_update()
  {
    data.push_update();
  }

  template<typename PassType> void bind_resources(PassType &pass)
  {
    pass.bind_ubo(UNIFORM_BUF_SLOT, &data);
  }
};

/**
 * \class Instance
 * \brief A running instance of the engine.
 */
class Instance : public DrawEngine {
  friend VelocityModule;
  friend MotionBlurModule;

  /** Debug scopes. */
  static void *debug_scope_render_sample;
  static void *debug_scope_irradiance_setup;
  static void *debug_scope_irradiance_sample;

  uint64_t depsgraph_last_update_ = 0;
  bool overlays_enabled_ = false;
  bool skip_render_ = false;

  /** Info string displayed at the top of the render / viewport, or the console when baking. */
  std::string info_ = "";

 public:
  ShaderModule &shaders;
  SyncModule sync;
  UniformDataModule uniform_data;
  MaterialModule materials;
  SubsurfaceModule subsurface;
  PipelineModule pipelines;
  ShadowModule shadows;
  LightModule lights;
  AmbientOcclusion ambient_occlusion;
  RayTraceModule raytracing;
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
  LookdevView lookdev_view;
  LookdevModule lookdev;
  SphereProbeModule sphere_probes;
  PlanarProbeModule planar_probes;
  VolumeProbeModule volume_probes;
  LightProbeModule light_probes;
  VolumeModule volume;

  static CLG_LogRef log;

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
  const View *drw_view = nullptr;
  const View3D *v3d;
  const RegionView3D *rv3d;

  const DRWContext *draw_ctx = nullptr;

  /** True if the instance is created for light baking. */
  bool is_light_bake = false;
  /** True if the instance is created for either viewport image render or final image render. */
  bool is_image_render = false;
  /** True if the instance is created only for viewport image render. */
  bool is_viewport_image_render = false;
  /** True if current viewport is drawn during playback. */
  bool is_playback = false;
  /** True if current viewport is drawn during navigation operator. */
  bool is_navigating = false;
  /** True if current viewport is drawn during painting operator. */
  bool is_painting = false;
  /** True if current viewport is drawn during transforming operator. */
  bool is_transforming = false;
  /** True if viewport compositor is enabled when drawing with this instance. */
  bool is_viewport_compositor_enabled = false;
  /** True if overlays need to be displayed (only for viewport). */
  bool draw_overlays = false;

  ShaderGroups loaded_shaders = ShaderGroups(0);
  ShaderGroups needed_shaders = ShaderGroups(0);

  /** View-layer overrides. */
  bool use_surfaces = true;
  bool use_curves = true;
  bool use_volumes = true;

  /** Debug mode from debug value. */
  eDebugMode debug_mode = eDebugMode::DEBUG_NONE;

 public:
  Instance()
      : shaders(*ShaderModule::module_get()),
        sync(*this),
        materials(*this),
        subsurface(*this, uniform_data.data.subsurface),
        pipelines(*this, uniform_data.data.pipeline),
        shadows(*this, uniform_data.data.shadow),
        lights(*this),
        ambient_occlusion(*this, uniform_data.data.ao),
        raytracing(*this, uniform_data.data.raytrace),
        velocity(*this),
        motion_blur(*this),
        depth_of_field(*this),
        cryptomatte(*this),
        hiz_buffer(*this, uniform_data.data.hiz),
        sampling(*this, uniform_data.data.clamp),
        camera(*this, uniform_data.data.camera),
        film(*this, uniform_data.data.film),
        render_buffers(*this, uniform_data.data.render_pass),
        main_view(*this),
        capture_view(*this),
        world(*this),
        lookdev_view(*this),
        lookdev(*this),
        sphere_probes(*this),
        planar_probes(*this),
        volume_probes(*this),
        light_probes(*this),
        volume(*this, uniform_data.data.volumes) {};
  ~Instance() {};

  blender::StringRefNull name_get() final
  {
    return "EEVEE";
  }

  /* Render & Viewport. */
  /* TODO(fclem): Split for clarity. */
  void init(const int2 &output_res,
            const rcti *output_rect,
            const rcti *visible_rect,
            RenderEngine *render,
            Depsgraph *depsgraph,
            Object *camera_object = nullptr,
            const RenderLayer *render_layer = nullptr,
            View *drw_view_ = nullptr,
            const View3D *v3d = nullptr,
            const RegionView3D *rv3d = nullptr);

  void init() final;

  void begin_sync() final;
  void object_sync(ObjectRef &ob_ref, Manager &manager) final;
  void end_sync() final;

  bool is_loaded(ShaderGroups groups) const
  {
    return (loaded_shaders & groups) == groups;
  }

  /**
   * Return true when probe pipeline is used during this sample.
   */
  bool do_lightprobe_sphere_sync() const;
  bool do_planar_probe_sync() const;

  /**
   * Return true when probe passes should be loaded.
   * It can be true even if do_<type>_probe_sync() is false due to shaders still being compiled.
   */
  bool needs_lightprobe_sphere_passes() const;
  bool needs_planar_probe_passes() const;

  /* Render. */

  void render_sync();
  void render_frame(RenderEngine *engine, RenderLayer *render_layer, const char *view_name);
  void store_metadata(RenderResult *render_result);

  /* Viewport. */

  void draw_viewport();
  void draw_viewport_image_render();

  void draw(Manager &manager) final;

  /* Light bake. */

  void init_light_bake(Depsgraph *depsgraph, draw::Manager *manager);
  void light_bake_irradiance(
      Object &probe,
      FunctionRef<void()> context_enable,
      FunctionRef<void()> context_disable,
      FunctionRef<bool()> stop,
      FunctionRef<void(LightProbeGridCacheFrame *, float progress)> result_update);

  static void update_passes(RenderEngine *engine, Scene *scene, ViewLayer *view_layer);

  /* Append a new line to the info string. */
  template<typename... Args> void info_append(const char *msg, Args &&...args)
  {
    std::string fmt_msg = fmt::format(fmt::runtime(msg), args...) + "\n";
    /* Don't print the same error twice. */
    if (info_ != fmt_msg && !BLI_str_endswith(info_.c_str(), fmt_msg.c_str())) {
      info_ += fmt_msg;
    }
  }

  /* The same as `info_append`, but `msg` will be translated.
   * NOTE: When calling this function, `msg` should be a string literal. */
  template<typename... Args> void info_append_i18n(const char *msg, Args &&...args)
  {
    std::string fmt_msg = fmt::format(fmt::runtime(RPT_(msg)), args...) + "\n";
    /* Don't print the same error twice. */
    if (info_ != fmt_msg && !BLI_str_endswith(info_.c_str(), fmt_msg.c_str())) {
      info_ += fmt_msg;
    }
  }

  const char *info_get()
  {
    return info_.c_str();
  }

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
    return overlays_enabled_;
  }

  /** True if the grease pencil engine might be running. */
  bool gpencil_engine_enabled() const
  {
    return DEG_id_type_any_exists(depsgraph, ID_GP);
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

  bool use_lookdev_overlay() const
  {
    return (v3d) &&
           ((v3d->shading.type == OB_MATERIAL) && (v3d->overlay.flag & V3D_OVERLAY_LOOK_DEV));
  }

  int get_recalc_flags(const ObjectRef &ob_ref)
  {
    return ob_ref.recalc_flags(depsgraph_last_update_);
  }

  int get_recalc_flags(const ::World &world)
  {
    return world.last_update > depsgraph_last_update_ ? int(ID_RECALC_SHADING) : 0;
  }

 private:
  /**
   * Conceptually renders one sample per pixel.
   * Everything based on random sampling should be done here (i.e: DRWViews jitter)
   */
  void render_sample();
  void render_read_result(RenderLayer *render_layer, const char *view_name);

  void mesh_sync(Object *ob, ObjectHandle &ob_handle);

  void update_eval_members();

  void set_time(float time);

  struct DebugScope {
    void *scope;

    DebugScope(void *&scope_p, const char *name)
    {
      if (scope_p == nullptr) {
        scope_p = GPU_debug_capture_scope_create(name);
      }
      scope = scope_p;
      GPU_debug_capture_scope_begin(scope);
    }

    ~DebugScope()
    {
      GPU_debug_capture_scope_end(scope);
    }
  };
};

}  // namespace blender::eevee
