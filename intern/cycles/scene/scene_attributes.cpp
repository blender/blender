/* SPDX-FileCopyrightText: 2026 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "scene/scene.h"

CCL_NAMESPACE_BEGIN

NODE_DEFINE(SceneAttributes)
{
  NodeType *type = NodeType::add("scene_attributes", create);

  SOCKET_FLOAT(time, "Time", 0);
  SOCKET_FLOAT(frame, "Frame", 1);

  return type;
}

SceneAttributes::SceneAttributes() : Node(get_node_type()) {}

SceneAttributes::~SceneAttributes() = default;

void SceneAttributes::device_update(Device * /*device*/, DeviceScene *dscene, Scene * /*scene*/)
{
  if (!is_modified()) {
    return;
  }
  dscene->data.scene_time.time = time;
  dscene->data.scene_time.frame = frame;

  clear_modified();
}

bool SceneAttributes::is_modified() const
{
  return Node::is_modified();
}

void SceneAttributes::clear_modified()
{
  Node::clear_modified();
}

void SceneAttributes::device_free(Device * /*unused*/,
                                  DeviceScene * /*udscene*/,
                                  bool /*force_free*/)
{
}

void SceneAttributes::tag_update(Scene * /*scene*/, const uint32_t flag)
{
  if (flag == UPDATE_ALL) {
    tag_modified();
  }
}

CCL_NAMESPACE_END
