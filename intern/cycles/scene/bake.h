/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#ifndef __BAKE_H__
#define __BAKE_H__

#include "device/device.h"
#include "scene/scene.h"

#include "util/progress.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

class BakeManager {
 public:
  BakeManager() = default;
  ~BakeManager() = default;

  void set_baking(Scene *scene, const bool use);
  bool get_baking() const;

  void set_use_camera(bool use_camera);

  void device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress &progress);
  void device_free(Device *device, DeviceScene *dscene);

  void tag_update();

  bool need_update() const;

 private:
  bool need_update_ = true;
  bool use_baking_ = false;
  bool use_camera_ = false;
};

CCL_NAMESPACE_END

#endif /* __BAKE_H__ */
