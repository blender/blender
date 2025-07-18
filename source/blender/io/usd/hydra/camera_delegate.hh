/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <pxr/imaging/hdx/freeCameraSceneDelegate.h>

struct Camera;
struct ID;
struct Scene;

namespace blender::io::hydra {

class CameraDelegate : public pxr::HdxFreeCameraSceneDelegate {
 public:
  CameraDelegate(pxr::HdRenderIndex *render_index, pxr::SdfPath const &delegate_id);
  ~CameraDelegate() override = default;

  void sync(const Scene *scene);
  void update(const ID *camera);

  pxr::VtValue GetCameraParamValue(pxr::SdfPath const &id, pxr::TfToken const &key) override;

 private:
  const Camera *camera_{nullptr};
};

}  // namespace blender::io::hydra
