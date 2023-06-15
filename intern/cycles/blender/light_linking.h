/* SPDX-FileCopyrightText: 2011-2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <cstdint>

#include "MEM_guardedalloc.h"
#include "RNA_blender_cpp.h"

CCL_NAMESPACE_BEGIN

class BlenderLightLink {
 public:
  static uint64_t get_light_set_membership(const BL::Object &parent, const BL::Object &object);
  static uint get_receiver_light_set(const BL::Object &parent, const BL::Object &object);

  static uint64_t get_shadow_set_membership(const BL::Object &parent, const BL::Object &object);
  static uint get_blocker_shadow_set(const BL::Object &parent, const BL::Object &object);
};

CCL_NAMESPACE_END
