/* SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation */

#include "light_tasks_delegate.h"
#include "engine.h"

namespace blender::render::hydra {

LightTasksDelegate::LightTasksDelegate(pxr::HdRenderIndex *parent_index,
                                       pxr::SdfPath const &delegate_id)
    : pxr::HdSceneDelegate(parent_index, delegate_id)
{
  simple_task_id_ = GetDelegateID().AppendElementString("simpleTask");
  GetRenderIndex().InsertTask<pxr::HdxSimpleLightTask>(this, simple_task_id_);
  skydome_task_id_ = GetDelegateID().AppendElementString("skydomeTask");
  GetRenderIndex().InsertTask<pxr::HdxSkydomeTask>(this, skydome_task_id_);

  CLOG_INFO(LOG_HYDRA_RENDER, 1, "%s", simple_task_id_.GetText());
  CLOG_INFO(LOG_HYDRA_RENDER, 1, "%s", skydome_task_id_.GetText());
}

pxr::VtValue LightTasksDelegate::Get(pxr::SdfPath const &id, pxr::TfToken const &key)
{
  CLOG_INFO(LOG_HYDRA_RENDER, 3, "%s, %s", id.GetText(), key.GetText());

  if (key == pxr::HdTokens->params) {
    if (id == simple_task_id_) {
      return pxr::VtValue(simple_task_params_);
    }
    else if (id == skydome_task_id_) {
      return pxr::VtValue(skydome_task_params_);
    }
  }
  return pxr::VtValue();
}

pxr::HdTaskSharedPtr LightTasksDelegate::simple_task()
{
  return GetRenderIndex().GetTask(simple_task_id_);
}

pxr::HdTaskSharedPtr LightTasksDelegate::skydome_task()
{
  /* Note that this task is intended to be the first "Render Task",
   * so that the AOV's are properly cleared, however it
   * does not spawn a HdRenderPass. */
  return GetRenderIndex().GetTask(skydome_task_id_);
}

void LightTasksDelegate::set_camera(pxr::SdfPath const &camera_id)
{
  if (simple_task_params_.cameraPath == camera_id) {
    return;
  }
  simple_task_params_.cameraPath = camera_id;
  GetRenderIndex().GetChangeTracker().MarkTaskDirty(simple_task_id_,
                                                    pxr::HdChangeTracker::DirtyParams);
  skydome_task_params_.camera = camera_id;
  GetRenderIndex().GetChangeTracker().MarkTaskDirty(skydome_task_id_,
                                                    pxr::HdChangeTracker::DirtyParams);
}

void LightTasksDelegate::set_viewport(pxr::GfVec4d const &viewport)
{
  if (skydome_task_params_.viewport == viewport) {
    return;
  }
  skydome_task_params_.viewport = viewport;
  GetRenderIndex().GetChangeTracker().MarkTaskDirty(skydome_task_id_,
                                                    pxr::HdChangeTracker::DirtyParams);
}

}  // namespace blender::render::hydra
