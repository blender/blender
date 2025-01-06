/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "device/device.h"
#include "scene/scene.h"

#include "util/progress.h"

CCL_NAMESPACE_BEGIN

class BakeManager {
 public:
  BakeManager() = default;
  ~BakeManager() = default;

  void set_baking(Scene *scene, const bool use);
  bool get_baking() const;

  void set_use_camera(bool use_camera);

  void set_use_seed(bool use_seed);
  bool get_use_seed() const;

  void device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress &progress);
  void device_free(Device *device, DeviceScene *dscene);

  void tag_update();

  bool need_update() const;

 private:
  bool need_update_ = true;
  bool use_baking_ = false;
  bool use_camera_ = false;
  bool use_seed_ = false;
};

CCL_NAMESPACE_END
