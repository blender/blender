/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <pxr/usd/sdf/path.h>

#include "light.hh"

namespace blender::io::hydra {

class HydraSceneDelegate;

class WorldData : public LightData {
 public:
  WorldData(HydraSceneDelegate *scene_delegate, pxr::SdfPath const &prim_id);

  void init() override;
  void update() override;
};

}  // namespace blender::io::hydra
