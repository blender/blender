/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __BACKGROUND_H__
#define __BACKGROUND_H__

#include "graph/node.h"

#include "util/types.h"

CCL_NAMESPACE_BEGIN

class Device;
class DeviceScene;
class Scene;
class Shader;

class Background : public Node {
 public:
  NODE_DECLARE

  NODE_SOCKET_API(bool, use_shader)

  NODE_SOCKET_API(uint, visibility)
  NODE_SOCKET_API(Shader *, shader)

  NODE_SOCKET_API(bool, transparent)
  NODE_SOCKET_API(bool, transparent_glass)
  NODE_SOCKET_API(float, transparent_roughness_threshold)

  NODE_SOCKET_API(float, volume_step_size)

  NODE_SOCKET_API(ustring, lightgroup)

  Background();
  ~Background();

  void device_update(Device *device, DeviceScene *dscene, Scene *scene);
  void device_free(Device *device, DeviceScene *dscene);

  void tag_update(Scene *scene);

  Shader *get_shader(const Scene *scene);
};

CCL_NAMESPACE_END

#endif /* __BACKGROUND_H__ */
