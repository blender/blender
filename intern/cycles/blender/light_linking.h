/* SPDX-FileCopyrightText: 2011-2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/types_base.h"

#include <cstdint>

struct Object;

CCL_NAMESPACE_BEGIN

class BlenderLightLink {
 public:
  static uint64_t get_light_set_membership(const ::Object *parent, const ::Object &object);
  static uint get_receiver_light_set(const ::Object *parent, const ::Object &object);

  static uint64_t get_shadow_set_membership(const ::Object *parent, const ::Object &object);
  static uint get_blocker_shadow_set(const ::Object *parent, const ::Object &object);
};

CCL_NAMESPACE_END
