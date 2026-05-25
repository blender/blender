/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_string_ref.hh"

#include "NOD_bundle_type_fwd.hh"

namespace blender::nodes::physics_bundles {

class MeshColliderBundle {
 public:
  static constexpr StringRefNull name = "Blender.Collider.Mesh";
  static const FlatBundleTypePtr &get_bundle_type();
};

class CollisionContactsBundle {
 public:
  static constexpr StringRefNull name = "Blender.CollisionContacts";
  static const FlatBundleTypePtr &get_bundle_type();
};

class DampingBundle {
 public:
  static constexpr StringRefNull name = "Blender.Damping";
  static const FlatBundleTypePtr &get_bundle_type();
};

class PinPositionBundle {
 public:
  static constexpr StringRefNull name = "Blender.Constraint.PinPosition";
  static const FlatBundleTypePtr &get_bundle_type();
};

class PinRotationBundle {
 public:
  static constexpr StringRefNull name = "Blender.Constraint.PinRotation";
  static const FlatBundleTypePtr &get_bundle_type();
};

class RodStretchShearBundle {
 public:
  static constexpr StringRefNull name = "Blender.Constraint.RodStretchShear";
  static const FlatBundleTypePtr &get_bundle_type();
};

class RodBendTwistBundle {
 public:
  static constexpr StringRefNull name = "Blender.Constraint.RodBendTwist";
  static const FlatBundleTypePtr &get_bundle_type();
};

class EdgeLengthConstraintBundle {
 public:
  static constexpr StringRefNull name = "Blender.Constraint.EdgeLength";
  static const FlatBundleTypePtr &get_bundle_type();
};

class CrossEdgeLengthConstraintBundle {
 public:
  static constexpr StringRefNull name = "Blender.Constraint.CrossEdgeLength";
  static const FlatBundleTypePtr &get_bundle_type();
};

/* This is currently not used by built-in nodes but is used in essential assets. */
class ForceBundle {
 public:
  static constexpr StringRefNull name = "Blender.Force";
  static const FlatBundleTypePtr &get_bundle_type();
};

}  // namespace blender::nodes::physics_bundles
