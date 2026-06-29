/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "light_tasks_delegate.hh"
#include "engine.hh"

#include <pxr/imaging/hd/legacyTaskFactory.h>
#include <pxr/imaging/hd/legacyTaskSchema.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/sceneIndexObserver.h>
#include <pxr/imaging/hd/tokens.h>

namespace blender::render::hydra {

LightTasksDelegate::LightTasksDelegate(pxr::HdRenderIndex *render_index,
                                       pxr::HdRetainedSceneIndexRefPtr task_scene_index,
                                       pxr::SdfPath const &base_id)
    : render_index_(render_index),
      task_scene_index_(std::move(task_scene_index)),
      simple_task_id_(base_id.AppendElementString("simpleTask")),
      skydome_task_id_(base_id.AppendElementString("skydomeTask"))
{
  publish_simple_task();
  publish_skydome_task();

  CLOG_DEBUG(LOG_HYDRA_RENDER, "%s", simple_task_id_.GetText());
  CLOG_DEBUG(LOG_HYDRA_RENDER, "%s", skydome_task_id_.GetText());
}

void LightTasksDelegate::publish_simple_task()
{
  pxr::HdContainerDataSourceHandle task_ds =
      pxr::HdLegacyTaskSchema::Builder()
          .SetFactory(
              pxr::HdRetainedTypedSampledDataSource<pxr::HdLegacyTaskFactorySharedPtr>::New(
                  pxr::HdMakeLegacyTaskFactory<pxr::HdxSimpleLightTask>()))
          .SetParameters(simple_task_params_ds_)
          .Build();

  task_scene_index_->AddPrims({{simple_task_id_,
                                pxr::HdPrimTypeTokens->task,
                                pxr::HdRetainedContainerDataSource::New(
                                    pxr::HdLegacyTaskSchema::GetSchemaToken(), task_ds)}});
}

void LightTasksDelegate::publish_skydome_task()
{
  const pxr::HdRprimCollection collection(pxr::HdTokens->geometry,
                                          pxr::HdReprSelector(pxr::HdReprTokens->smoothHull));
  const pxr::TfTokenVector render_tags = {pxr::HdRenderTagTokens->geometry};

  pxr::HdContainerDataSourceHandle task_ds =
      pxr::HdLegacyTaskSchema::Builder()
          .SetFactory(
              pxr::HdRetainedTypedSampledDataSource<pxr::HdLegacyTaskFactorySharedPtr>::New(
                  pxr::HdMakeLegacyTaskFactory<pxr::HdxSkydomeTask>()))
          .SetParameters(skydome_task_params_ds_)
          .SetCollection(
              pxr::HdRetainedTypedSampledDataSource<pxr::HdRprimCollection>::New(collection))
          .SetRenderTags(
              pxr::HdRetainedTypedSampledDataSource<pxr::TfTokenVector>::New(render_tags))
          .Build();

  task_scene_index_->AddPrims({{skydome_task_id_,
                                pxr::HdPrimTypeTokens->task,
                                pxr::HdRetainedContainerDataSource::New(
                                    pxr::HdLegacyTaskSchema::GetSchemaToken(), task_ds)}});
}

pxr::HdTaskSharedPtr LightTasksDelegate::simple_task()
{
  return render_index_->GetTask(simple_task_id_);
}

pxr::HdTaskSharedPtr LightTasksDelegate::skydome_task()
{
  /* Note that this task is intended to be the first "Render Task",
   * so that the AOV's are properly cleared, however it
   * does not spawn a HdRenderPass. */
  return render_index_->GetTask(skydome_task_id_);
}

void LightTasksDelegate::set_camera(pxr::SdfPath const &camera_id)
{
  if (simple_task_params_.cameraPath == camera_id && skydome_task_params_.camera == camera_id) {
    return;
  }
  simple_task_params_.cameraPath = camera_id;
  skydome_task_params_.camera = camera_id;
  task_scene_index_->DirtyPrims(
      {{simple_task_id_, pxr::HdLegacyTaskSchema::GetParametersLocator()},
       {skydome_task_id_, pxr::HdLegacyTaskSchema::GetParametersLocator()}});
}

void LightTasksDelegate::set_viewport(pxr::GfVec4d const &viewport)
{
  if (skydome_task_params_.viewport == viewport) {
    return;
  }
  skydome_task_params_.viewport = viewport;
  task_scene_index_->DirtyPrims(
      {{skydome_task_id_, pxr::HdLegacyTaskSchema::GetParametersLocator()}});
}

}  // namespace blender::render::hydra
