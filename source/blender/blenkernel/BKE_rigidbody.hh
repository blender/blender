/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2013 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup bke
 * \brief API for Blender-side Rigid Body stuff
 */

#pragma once

namespace blender::particles {

static const char *id_attribute_name = "id";
static const char *shape_index_attribute_name = "shape_index";
static const char *pos_attribute_name = "position";
static const char *rot_attribute_name = "rotation";

}  // namespace blender::particles

struct RigidBodyMap {
  using UID = int;

  enum BodyFlag {
    /* Body is used by data in Blender, used for removing bodies that are no longer needed */
    Used = (1 << 0),
  };

  struct BodyPointer {
    int flag;
    struct rbRigidBody *body;
  };

  struct ShapePointer {
    struct ID *id_key;
    struct rbCollisionShape *shape;
    int users;
  };

  blender::Map<UID, BodyPointer> body_map_;
  blender::Vector<ShapePointer> shape_list_;

  void clear(struct RigidBodyWorld *rbw);

  void add_shape(struct ID *id_key, struct rbCollisionShape *shape);
  void clear_shapes();
};
