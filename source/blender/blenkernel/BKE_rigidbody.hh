/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2013 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup bke
 * \brief API for Blender-side Rigid Body stuff
 */

#pragma once

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

  blender::Map<UID, BodyPointer> map_;
};
