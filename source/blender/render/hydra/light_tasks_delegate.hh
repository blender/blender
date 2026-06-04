/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <pxr/imaging/hd/dataSource.h>
#include <pxr/imaging/hd/renderIndex.h>
#include <pxr/imaging/hd/retainedSceneIndex.h>
#include <pxr/imaging/hd/task.h>
#include <pxr/imaging/hdx/simpleLightTask.h>
#include <pxr/imaging/hdx/skydomeTask.h>

#include "render_task_delegate.hh"

namespace blender::render::hydra {

/* Registers a HdxSimpleLightTask and HdxSkydomeTask with the render index. */

class SimpleLightTaskParamsDataSource final
    : public pxr::HdTypedSampledDataSource<pxr::HdxSimpleLightTaskParams> {
 public:
  HD_DECLARE_DATASOURCE(SimpleLightTaskParamsDataSource);

  pxr::HdxSimpleLightTaskParams params;

  pxr::VtValue GetValue(Time /*t*/) override
  {
    return pxr::VtValue(params);
  }
  pxr::HdxSimpleLightTaskParams GetTypedValue(Time /*t*/) override
  {
    return params;
  }
  bool GetContributingSampleTimesForInterval(Time /*start*/,
                                             Time /*end*/,
                                             std::vector<Time> * /*sampleTimes*/) override
  {
    return false;
  }
};

class LightTasksDelegate {
 public:
  LightTasksDelegate(pxr::HdRenderIndex *render_index,
                     pxr::HdRetainedSceneIndexRefPtr task_scene_index,
                     pxr::SdfPath const &base_id);

  pxr::HdTaskSharedPtr simple_task();
  pxr::HdTaskSharedPtr skydome_task();
  void set_camera(pxr::SdfPath const &camera_id);
  void set_viewport(pxr::GfVec4d const &viewport);

 private:
  void publish_simple_task();
  void publish_skydome_task();

  pxr::HdRenderIndex *render_index_ = nullptr;
  pxr::HdRetainedSceneIndexRefPtr task_scene_index_;
  pxr::SdfPath simple_task_id_;
  pxr::SdfPath skydome_task_id_;
  SimpleLightTaskParamsDataSource::Handle simple_task_params_ds_ =
      SimpleLightTaskParamsDataSource::New();
  RenderTaskParamsDataSource::Handle skydome_task_params_ds_ = RenderTaskParamsDataSource::New();
  pxr::HdxSimpleLightTaskParams &simple_task_params_ = simple_task_params_ds_->params;
  pxr::HdxRenderTaskParams &skydome_task_params_ = skydome_task_params_ds_->params;
};

}  // namespace blender::render::hydra
