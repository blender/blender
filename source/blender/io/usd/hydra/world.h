/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <map>

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/tf/staticTokens.h>
#include <pxr/base/vt/value.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/sdf/path.h>

#include "DNA_view3d_types.h"
#include "DNA_world_types.h"

#include "light.h"

namespace blender::io::hydra {

class WorldData : public LightData {
 public:
  WorldData(HydraSceneDelegate *scene_delegate, pxr::SdfPath const &prim_id);

  void init() override;
  void update() override;

 protected:
  void write_transform() override;
};

}  // namespace blender::io::hydra
