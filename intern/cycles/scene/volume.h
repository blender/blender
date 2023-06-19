/* SPDX-FileCopyrightText: 2020-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "graph/node.h"

#include "scene/mesh.h"

CCL_NAMESPACE_BEGIN

class Volume : public Mesh {
 public:
  NODE_DECLARE

  Volume();

  NODE_SOCKET_API(float, clipping)
  NODE_SOCKET_API(float, step_size)
  NODE_SOCKET_API(bool, object_space)
  NODE_SOCKET_API(float, velocity_scale)

  virtual void clear(bool preserve_shaders = false) override;
};

CCL_NAMESPACE_END
