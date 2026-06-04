/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <pxr/base/gf/camera.h>
#include <pxr/imaging/hd/renderIndex.h>
#include <pxr/imaging/hd/retainedSceneIndex.h>
#include <pxr/imaging/hdx/freeCameraPrimDataSource.h>

namespace blender {

struct Camera;
struct Scene;

namespace io::hydra {

/* Camera prim conforming to HdCameraSchema and HdXformSchema through a retained
 * scene index. */

class BlenderCameraIDPropertiesDataSource;

class CameraDelegate {
 public:
  CameraDelegate(pxr::HdRenderIndex *render_index, pxr::SdfPath const &camera_id);

  void sync(const Scene *scene);
  void SetCamera(pxr::GfCamera const &camera);
  pxr::SdfPath const &GetCameraId() const
  {
    return camera_id_;
  }

 private:
  pxr::HdRetainedSceneIndexRefPtr camera_scene_index_;
  pxr::SdfPath camera_id_;
  pxr::HdxFreeCameraPrimDataSource::Handle free_camera_ds_;
  std::shared_ptr<BlenderCameraIDPropertiesDataSource> id_properties_ds_;
};

}  // namespace io::hydra
}  // namespace blender
