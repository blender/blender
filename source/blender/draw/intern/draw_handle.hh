/* SPDX-FileCopyrightText: 2022 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup draw
 *
 * A unique identifier for each object component.
 * It is used to access each component data such as matrices and object attributes.
 * It is valid only for the current draw, it is not persistent.
 *
 * The most significant bit is used to encode if the object needs to invert the front face winding
 * because of its object matrix handedness. This is handy because this means sorting inside
 * #DrawGroup command will put all inverted commands last.
 *
 * Default value of 0 points toward an non-cull-able object with unit bounding box centered at
 * the origin.
 */

#include "draw_shader_shared.h"

struct Object;
struct DupliObject;

namespace blender::draw {

struct ResourceHandle {
  uint raw;

  ResourceHandle() = default;
  ResourceHandle(uint raw_) : raw(raw_){};
  ResourceHandle(uint index, bool inverted_handedness)
  {
    raw = index;
    SET_FLAG_FROM_TEST(raw, inverted_handedness, 0x80000000u);
  }

  bool has_inverted_handedness() const
  {
    return (raw & 0x80000000u) != 0;
  }

  uint resource_index() const
  {
    return (raw & 0x7FFFFFFFu);
  }
};

/* TODO(fclem): Move to somewhere more appropriated after cleaning up the header dependencies. */
struct ObjectRef {
  Object *object;
  /** Dupli object that corresponds to the current object. */
  DupliObject *dupli_object;
  /** Object that created the dupli-list the current object is part of. */
  Object *dupli_parent;
};

};  // namespace blender::draw
