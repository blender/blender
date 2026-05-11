/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <string>

#include <pxr/imaging/hd/renderIndex.h>
#include <pxr/imaging/hd/sceneIndex.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usdImaging/usdImaging/stageSceneIndex.h>

namespace blender {

struct Depsgraph;

namespace io::hydra {

/* Populate Hydra render index using USD file export, for testing. */
class USDSceneIndex {
 private:
  pxr::HdRenderIndex *render_index_;
  pxr::SdfPath const delegate_id_;
  pxr::UsdStageRefPtr stage_;
  pxr::UsdImagingStageSceneIndexRefPtr stage_scene_index_;
  pxr::HdSceneIndexBaseRefPtr final_scene_index_;

  std::string temp_dir_;
  std::string temp_file_;

  bool use_materialx = true;

 public:
  USDSceneIndex(pxr::HdRenderIndex *render_index,
                pxr::SdfPath const &delegate_id,
                bool use_materialx);
  ~USDSceneIndex();

  void populate(Depsgraph *depsgraph);
};

}  // namespace io::hydra
}  // namespace blender
