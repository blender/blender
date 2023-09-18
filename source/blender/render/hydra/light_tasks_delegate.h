/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <pxr/imaging/hd/sceneDelegate.h>
#include <pxr/imaging/hdx/simpleLightTask.h>
#include <pxr/imaging/hdx/skydomeTask.h>

namespace blender::render::hydra {

class LightTasksDelegate : public pxr::HdSceneDelegate {
 public:
  LightTasksDelegate(pxr::HdRenderIndex *parentIndex, pxr::SdfPath const &delegate_id);
  ~LightTasksDelegate() override = default;

  /* Delegate methods */
  pxr::VtValue Get(pxr::SdfPath const &id, pxr::TfToken const &key) override;

  pxr::HdTaskSharedPtr simple_task();
  pxr::HdTaskSharedPtr skydome_task();
  void set_camera(pxr::SdfPath const &camera_id);
  void set_viewport(pxr::GfVec4d const &viewport);

 private:
  pxr::SdfPath simple_task_id_;
  pxr::SdfPath skydome_task_id_;
  pxr::HdxSimpleLightTaskParams simple_task_params_;
  pxr::HdxRenderTaskParams skydome_task_params_;
};

}  // namespace blender::render::hydra
