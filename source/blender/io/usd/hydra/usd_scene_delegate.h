/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <string>

#include <pxr/imaging/hd/renderIndex.h>
#include <pxr/usdImaging/usdImaging/delegate.h>

struct Depsgraph;

namespace blender::io::hydra {

/* Populate Hydra render index using USD file export, for testing. */
class USDSceneDelegate {
 private:
  pxr::HdRenderIndex *render_index_;
  pxr::SdfPath const delegate_id_;
  pxr::UsdStageRefPtr stage_;
  std::unique_ptr<pxr::UsdImagingDelegate> delegate_;

  std::string temp_dir_;
  std::string temp_file_;

 public:
  USDSceneDelegate(pxr::HdRenderIndex *render_index, pxr::SdfPath const &delegate_id);
  ~USDSceneDelegate();

  void populate(Depsgraph *depsgraph);
};

}  // namespace blender::io::hydra
