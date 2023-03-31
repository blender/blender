/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#include "hydra/file_reader.h"
#include "hydra/camera.h"
#include "hydra/render_delegate.h"

#include "util/path.h"
#include "util/unique_ptr.h"

#include "scene/scene.h"

#include <pxr/base/plug/registry.h>
#include <pxr/imaging/hd/dirtyList.h>
#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/imaging/hd/renderIndex.h>
#include <pxr/imaging/hd/rprimCollection.h>
#include <pxr/imaging/hd/task.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/camera.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usdImaging/usdImaging/delegate.h>

HDCYCLES_NAMESPACE_OPEN_SCOPE

/* Dummy task whose only purpose is to provide render tag tokens to the render index. */
class DummyHdTask : public HdTask {
 public:
  DummyHdTask(HdSceneDelegate *delegate, SdfPath const &id)
      : HdTask(id), tags({HdRenderTagTokens->geometry, HdRenderTagTokens->render})
  {
  }

 protected:
  void Sync(HdSceneDelegate *delegate, HdTaskContext *ctx, HdDirtyBits *dirtyBits) override {}

  void Prepare(HdTaskContext *ctx, HdRenderIndex *render_index) override {}

  void Execute(HdTaskContext *ctx) override {}

  const TfTokenVector &GetRenderTags() const override
  {
    return tags;
  }

  TfTokenVector tags;
};

void HdCyclesFileReader::read(Session *session, const char *filepath, const bool use_camera)
{
  /* Initialize USD. */
  PlugRegistry::GetInstance().RegisterPlugins(path_get("usd"));

  /* Open Stage. */
  UsdStageRefPtr stage = UsdStage::Open(filepath);
  if (!stage) {
    fprintf(stderr, "%s read error\n", filepath);
    return;
  }

  /* Init paths. */
  SdfPath root_path = SdfPath::AbsoluteRootPath();
  SdfPath task_path("/_hdCycles/DummyHdTask");

  /* Create render delegate. */
  HdRenderSettingsMap settings_map;
  settings_map.insert(std::make_pair(HdCyclesRenderSettingsTokens->stageMetersPerUnit,
                                     VtValue(UsdGeomGetStageMetersPerUnit(stage))));

  HdCyclesDelegate render_delegate(settings_map, session, true);

  /* Create render index and scene delegate. */
  unique_ptr<HdRenderIndex> render_index(HdRenderIndex::New(&render_delegate, {}));
  unique_ptr<UsdImagingDelegate> scene_delegate = make_unique<UsdImagingDelegate>(
      render_index.get(), root_path);

  /* Add render tags and collection to render index. */
  HdRprimCollection collection(HdTokens->geometry, HdReprSelector(HdReprTokens->smoothHull));
  collection.SetRootPath(root_path);

  render_index->InsertTask<DummyHdTask>(scene_delegate.get(), task_path);

#if PXR_VERSION < 2111
  HdDirtyListSharedPtr dirty_list = std::make_shared<HdDirtyList>(collection,
                                                                  *(render_index.get()));
  render_index->EnqueuePrimsToSync(dirty_list, collection);
#else
  render_index->EnqueueCollectionToSync(collection);
#endif

  /* Create prims. */
  const UsdPrim &stage_root = stage->GetPseudoRoot();
  scene_delegate->Populate(stage_root.GetStage()->GetPrimAtPath(root_path), {});

  /* Sync prims. */
  HdTaskContext task_context;
  HdTaskSharedPtrVector tasks;
  tasks.push_back(render_index->GetTask(task_path));

  render_index->SyncAll(&tasks, &task_context);
  render_delegate.CommitResources(&render_index->GetChangeTracker());

  /* Use first camera in stage.
   * TODO: get camera from UsdRender if available. */
  if (use_camera) {
    for (UsdPrim const &prim : stage->Traverse()) {
      if (prim.IsA<UsdGeomCamera>()) {
        HdSprim *sprim = render_index->GetSprim(HdPrimTypeTokens->camera, prim.GetPath());
        if (sprim) {
          HdCyclesCamera *camera = dynamic_cast<HdCyclesCamera *>(sprim);
          camera->ApplyCameraSettings(render_delegate.GetRenderParam(), session->scene->camera);
          break;
        }
      }
    }
  }
}

HDCYCLES_NAMESPACE_CLOSE_SCOPE
