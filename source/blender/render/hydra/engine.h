/* SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation */

#pragma once

#include <memory>
#include <string>

#include <pxr/imaging/hd/driver.h>
#include <pxr/imaging/hd/engine.h>
#include <pxr/imaging/hd/pluginRenderDelegateUniqueHandle.h>
#include <pxr/imaging/hdx/freeCameraSceneDelegate.h>
#include <pxr/imaging/hgi/hgi.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usdImaging/usdImaging/delegate.h>

#include "hydra/hydra_scene_delegate.h"
#include "hydra/settings.h"
#include "hydra/usd_scene_delegate.h"

#include "light_tasks_delegate.h"
#include "render_task_delegate.h"

struct bContext;
struct RenderEngine;
struct CLG_LogRef;

namespace blender::render::hydra {

extern struct CLG_LogRef *LOG_HYDRA_RENDER;

class Engine {
 protected:
  std::string render_delegate_name_;
  RenderEngine *bl_engine_ = nullptr;
  Depsgraph *depsgraph_ = nullptr;
  bContext *context_ = nullptr;
  Scene *scene_ = nullptr;

  /* The order is important due to deletion order */
  pxr::HgiUniquePtr hgi_;
  pxr::HdDriver hgi_driver_;
  pxr::HdPluginRenderDelegateUniqueHandle render_delegate_;
  std::unique_ptr<pxr::HdRenderIndex> render_index_;

  std::unique_ptr<io::hydra::HydraSceneDelegate> hydra_scene_delegate_;
  std::unique_ptr<io::hydra::USDSceneDelegate> usd_scene_delegate_;

  std::unique_ptr<RenderTaskDelegate> render_task_delegate_;
  std::unique_ptr<pxr::HdxFreeCameraSceneDelegate> free_camera_delegate_;
  std::unique_ptr<LightTasksDelegate> light_tasks_delegate_;
  std::unique_ptr<pxr::HdEngine> engine_;

 public:
  Engine(RenderEngine *bl_engine, const std::string &render_delegate_name);
  virtual ~Engine() = default;

  void sync(Depsgraph *depsgraph, bContext *context);
  virtual void render() = 0;

  virtual void set_render_setting(const std::string &key, const pxr::VtValue &val);

 protected:
  float renderer_percent_done();
  pxr::HdTaskSharedPtrVector tasks();
  virtual void notify_status(float progress,
                             const std::string &title,
                             const std::string &info) = 0;
};

}  // namespace blender::render::hydra
