/* SPDX-FileCopyrightText: 2011-2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/types_base.h"

#include <cstdint>

namespace blender {
struct Object;
}

CCL_NAMESPACE_BEGIN

class BlenderLightLink {
 public:
  static uint64_t get_light_set_membership(const blender::Object *parent,
                                           const blender::Object &object);
  static uint get_receiver_light_set(const blender::Object *parent, const blender::Object &object);

  static uint64_t get_shadow_set_membership(const blender::Object *parent,
                                            const blender::Object &object);
  static uint get_blocker_shadow_set(const blender::Object *parent, const blender::Object &object);
};

CCL_NAMESPACE_END
