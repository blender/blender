/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_fluid_types.h"

#include "volume.h"

namespace blender::io::hydra {

class VolumeModifierData : public VolumeData {

 public:
  VolumeModifierData(HydraSceneDelegate *scene_delegate,
                     const Object *object,
                     pxr::SdfPath const &prim_id);
  static bool is_volume_modifier(const Object *object);

  void init() override;
  void update() override;

 protected:
  void write_transform() override;

 private:
  std::string get_cached_file_path(std::string directory, int frame);

  const FluidModifierData *modifier_;
};

}  // namespace blender::io::hydra
