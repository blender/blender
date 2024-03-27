/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "engine.hh"

#include <pxr/base/plug/plugin.h>
#include <pxr/base/plug/registry.h>
#include <pxr/imaging/hd/rendererPluginRegistry.h>
#include <pxr/imaging/hdSt/renderDelegate.h>
#include <pxr/imaging/hgi/tokens.h>
#include <pxr/usd/usdGeom/tokens.h>

#include "BLI_path_util.h"

#include "BKE_context.hh"

#include "GPU_context.hh"

#include "DEG_depsgraph_query.hh"

#include "RE_engine.h"

#include "CLG_log.h"

namespace blender::render::hydra {

CLG_LOGREF_DECLARE_GLOBAL(LOG_HYDRA_RENDER, "hydra.render");

Engine::Engine(RenderEngine *bl_engine, const std::string &render_delegate_name)
    : render_delegate_name_(render_delegate_name), bl_engine_(bl_engine)
{
  pxr::HdRendererPluginRegistry &registry = pxr::HdRendererPluginRegistry::GetInstance();

  pxr::TF_PY_ALLOW_THREADS_IN_SCOPE();

  if (GPU_backend_get_type() == GPU_BACKEND_VULKAN) {
    BLI_setenv("HGI_ENABLE_VULKAN", "1");
  }

  pxr::HdDriverVector hd_drivers;
  if (bl_engine->type->flag & RE_USE_GPU_CONTEXT) {
    hgi_ = pxr::Hgi::CreatePlatformDefaultHgi();
    hgi_driver_.name = pxr::HgiTokens->renderDriver;
    hgi_driver_.driver = pxr::VtValue(hgi_.get());

    hd_drivers.push_back(&hgi_driver_);
  }
  render_delegate_ = registry.CreateRenderDelegate(pxr::TfToken(render_delegate_name_));

  if (!render_delegate_) {
    throw std::runtime_error("Cannot create render delegate: " + render_delegate_name_);
  }

  render_index_.reset(pxr::HdRenderIndex::New(render_delegate_.Get(), hd_drivers));
  free_camera_delegate_ = std::make_unique<pxr::HdxFreeCameraSceneDelegate>(
      render_index_.get(), pxr::SdfPath::AbsoluteRootPath().AppendElementString("freeCamera"));

  if (bl_engine->type->flag & RE_USE_GPU_CONTEXT && GPU_backend_get_type() == GPU_BACKEND_OPENGL) {
    render_task_delegate_ = std::make_unique<GPURenderTaskDelegate>(
        render_index_.get(), pxr::SdfPath::AbsoluteRootPath().AppendElementString("renderTask"));
  }
  else {
    render_task_delegate_ = std::make_unique<RenderTaskDelegate>(
        render_index_.get(), pxr::SdfPath::AbsoluteRootPath().AppendElementString("renderTask"));
  }
  render_task_delegate_->set_camera(free_camera_delegate_->GetCameraId());

  if (render_delegate_name_ == "HdStormRendererPlugin") {
    light_tasks_delegate_ = std::make_unique<LightTasksDelegate>(
        render_index_.get(), pxr::SdfPath::AbsoluteRootPath().AppendElementString("lightTasks"));
    light_tasks_delegate_->set_camera(free_camera_delegate_->GetCameraId());
  }

  engine_ = std::make_unique<pxr::HdEngine>();
}

void Engine::sync(Depsgraph *depsgraph, bContext *context)
{
  depsgraph_ = depsgraph;
  context_ = context;
  scene_ = DEG_get_evaluated_scene(depsgraph);

  if (scene_->hydra.export_method == SCE_HYDRA_EXPORT_HYDRA) {
    /* Fast path. */
    usd_scene_delegate_.reset();

    if (!hydra_scene_delegate_) {
      pxr::SdfPath scene_path = pxr::SdfPath::AbsoluteRootPath().AppendElementString("scene");
      hydra_scene_delegate_ = std::make_unique<io::hydra::HydraSceneDelegate>(render_index_.get(),
                                                                              scene_path);
      hydra_scene_delegate_->use_materialx = bl_engine_->type->flag & RE_USE_MATERIALX;
    }
    hydra_scene_delegate_->populate(depsgraph, context ? CTX_wm_view3d(context) : nullptr);
  }
  else {
    /* Slow USD export for reference. */
    if (hydra_scene_delegate_) {
      /* Freeing the Hydra scene delegate crashes as something internal to USD
       * still holds a pointer to it, only clear it instead. */
      hydra_scene_delegate_->clear();
    }

    if (!usd_scene_delegate_) {
      pxr::SdfPath scene_path = pxr::SdfPath::AbsoluteRootPath().AppendElementString("usd_scene");
      usd_scene_delegate_ = std::make_unique<io::hydra::USDSceneDelegate>(render_index_.get(),
                                                                          scene_path);
    }
    usd_scene_delegate_->populate(depsgraph);
  }
}

void Engine::set_render_setting(const std::string &key, const pxr::VtValue &val)
{
  render_delegate_->SetRenderSetting(pxr::TfToken(key), val);
}

float Engine::renderer_percent_done()
{
  pxr::VtDictionary render_stats = render_delegate_->GetRenderStats();
  auto it = render_stats.find("percentDone");
  if (it == render_stats.end()) {
    return 0.0f;
  }
  return float(it->second.UncheckedGet<double>());
}

pxr::HdTaskSharedPtrVector Engine::tasks()
{
  pxr::HdTaskSharedPtrVector res;
  if (light_tasks_delegate_) {
    if (scene_->r.alphamode != R_ALPHAPREMUL) {
#ifndef __APPLE__
      /* TODO: Temporary disable skydome task for MacOS due to crash with error:
       * Failed to created pipeline state, error depthAttachmentPixelFormat is not valid
       * and shader writes to depth */
      res.push_back(light_tasks_delegate_->skydome_task());
#endif
    }
    res.push_back(light_tasks_delegate_->simple_task());
  }
  res.push_back(render_task_delegate_->task());
  return res;
}

}  // namespace blender::render::hydra
