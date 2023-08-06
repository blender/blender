/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <pxr/base/tf/hashmap.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/sdf/path.h>

#include "BKE_light.h"

#include "object.h"

namespace blender::io::hydra {

class InstancerData;

class LightData : public ObjectData {
  friend InstancerData;

 protected:
  std::map<pxr::TfToken, pxr::VtValue> data_;
  pxr::TfToken prim_type_;

 public:
  LightData(HydraSceneDelegate *scene_delegate, const Object *object, pxr::SdfPath const &prim_id);

  void init() override;
  void insert() override;
  void remove() override;
  void update() override;

  pxr::VtValue get_data(pxr::TfToken const &key) const override;

 protected:
  pxr::TfToken prim_type(const Light *light);
};

}  // namespace blender::io::hydra
