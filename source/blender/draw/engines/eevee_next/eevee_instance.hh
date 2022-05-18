/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation.
 */

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

#include "eevee_camera.hh"
#include "eevee_material.hh"
#include "eevee_pipeline.hh"
#include "eevee_shader.hh"
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

 public:
  ShaderModule &shaders;
  SyncModule sync;
  MaterialModule materials;
  PipelineModule pipelines;
  VelocityModule velocity;
  Camera camera;
  MainView main_view;
  World world;

  /** Input data. */
  Depsgraph *depsgraph;
  /** Evaluated IDs. */
  Scene *scene;
  ViewLayer *view_layer;
  Object *camera_eval_object;
  Object *camera_orig_object;
  /** Only available when rendering for final render. */
  const RenderLayer *render_layer;
  RenderEngine *render;
  /** Only available when rendering for viewport. */
  const DRWView *drw_view;
  const View3D *v3d;
  const RegionView3D *rv3d;

  /* Info string displayed at the top of the render / viewport. */
  std::string info = "";

 public:
  Instance()
      : shaders(*ShaderModule::module_get()),
        sync(*this),
        materials(*this),
        pipelines(*this),
        velocity(*this),
        camera(*this),
        main_view(*this),
        world(*this){};
  ~Instance(){};

  void init(const int2 &output_res,
            const rcti *output_rect,
            RenderEngine *render,
            Depsgraph *depsgraph,
            const LightProbe *light_probe_ = nullptr,
            Object *camera_object = nullptr,
            const RenderLayer *render_layer = nullptr,
            const DRWView *drw_view = nullptr,
            const View3D *v3d = nullptr,
            const RegionView3D *rv3d = nullptr);

  void begin_sync();
  void object_sync(Object *ob);
  void end_sync();

  void render_sync();
  void render_frame(RenderLayer *render_layer, const char *view_name);

  void draw_viewport(DefaultFramebufferList *dfbl);

  bool is_viewport(void)
  {
    return !DRW_state_is_scene_render();
  }

  bool use_scene_lights(void) const
  {
    return (!v3d) ||
           ((v3d->shading.type == OB_MATERIAL) &&
            (v3d->shading.flag & V3D_SHADING_SCENE_LIGHTS)) ||
           ((v3d->shading.type == OB_RENDER) &&
            (v3d->shading.flag & V3D_SHADING_SCENE_LIGHTS_RENDER));
  }

  /* Light the scene using the selected HDRI in the viewport shading pop-over. */
  bool use_studio_light(void) const
  {
    return (v3d) && (((v3d->shading.type == OB_MATERIAL) &&
                      ((v3d->shading.flag & V3D_SHADING_SCENE_WORLD) == 0)) ||
                     ((v3d->shading.type == OB_RENDER) &&
                      ((v3d->shading.flag & V3D_SHADING_SCENE_WORLD_RENDER) == 0)));
  }

 private:
  void render_sample();

  void mesh_sync(Object *ob, ObjectHandle &ob_handle);

  void update_eval_members();

  void set_time(float time);
};

}  // namespace blender::eevee
