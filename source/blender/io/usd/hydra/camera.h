/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <tuple>

#include <pxr/base/gf/camera.h>
#include <pxr/base/gf/vec2f.h>

struct ARegion;
struct Object;
struct View3D;

namespace blender::io::hydra {

class CameraData {
 private:
  int mode_;
  pxr::GfRange1f clip_range_;
  float focal_length_;
  pxr::GfVec2f sensor_size_;
  pxr::GfMatrix4d transform_;
  pxr::GfVec2f lens_shift_;
  pxr::GfVec2f ortho_size_;
  std::tuple<float, float, int> dof_data_;

 public:
  CameraData(const View3D *v3d, const ARegion *region);
  CameraData(const Object *camera_obj, pxr::GfVec2i res, pxr::GfVec4f tile);

  pxr::GfCamera gf_camera();
  pxr::GfCamera gf_camera(pxr::GfVec4f tile);
};

}  // namespace blender::io::hydra
