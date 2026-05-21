/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/types.h"

#include "graph/node.h"

CCL_NAMESPACE_BEGIN

class Device;
class DeviceScene;
class Scene;

class SceneAttributes : public Node {
 public:
  NODE_DECLARE

  NODE_SOCKET_API(float, time)
  NODE_SOCKET_API(float, frame)

  enum : uint32_t {
    /* Tag everything in the manager for an update. */
    UPDATE_ALL = ~0u,

    UPDATE_NONE = 0u,
  };

  SceneAttributes();
  ~SceneAttributes() override;

  void device_update(Device *device, DeviceScene *dscene, Scene *scene);
  void device_free(Device *device, DeviceScene *dscene, bool force_free = false);

  void tag_update(Scene *scene, const uint32_t flag);

  bool is_modified() const;
  void clear_modified();
};

CCL_NAMESPACE_END
