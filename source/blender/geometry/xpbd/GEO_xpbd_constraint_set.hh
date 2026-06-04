/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_index_mask.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "GEO_xpbd_constraint_coloring.hh"
#include "GEO_xpbd_constraint_set_params.hh"
#include "GEO_xpbd_updater_gauss_seidel.hh"
#include "GEO_xpbd_updater_velocity.hh"

namespace blender::xpbd {

/**
 * Base class for constraint evaluators. It evaluate a batch of constraints and writes back the
 * results using a passed in "updater".
 *
 * Use #TemplatedConstraintSet to instantiate the constraint evaluation for each updater
 * automatically. This avoids having to implement separate Jacobian and Gauss Seidel code paths for
 * such constraints.
 */
class ConstraintSet {
 protected:
  int constraints_num_;
  Vector<int> affected_geo_indices_;

 public:
  ConstraintSet(int constraints_num, Vector<int> affected_geo_indices)
      : constraints_num_(constraints_num), affected_geo_indices_(std::move(affected_geo_indices))
  {
  }

  virtual ~ConstraintSet() = default;

  virtual StringRefNull debug_name() const = 0;
  virtual void solve_sequential(const ConstraintSetParams &params,
                                GaussSeidelUpdater &updater,
                                const IndexMask &mask) = 0;
  virtual void reset_forces() = 0;
  virtual ConstraintColoring color_constraints(LinearAllocator<> &memory) const = 0;

  void solve_sequential_all(const ConstraintSetParams &params, GaussSeidelUpdater &updater)
  {
    this->solve_sequential(params, updater, IndexMask(constraints_num_));
  }

  Span<int> get_affected_geo_indices() const
  {
    return affected_geo_indices_;
  }
};

class VelocityConstraintSet {
 protected:
  Vector<int> affected_geo_indices_;

 public:
  VelocityConstraintSet(Vector<int> affected_geo_indices)
      : affected_geo_indices_(std::move(affected_geo_indices))
  {
  }
  virtual ~VelocityConstraintSet() = default;

  virtual void reset_forces() = 0;
  virtual void solve_sequential(const ConstraintSetParams &params, VelocityUpdater &updater) = 0;

  Span<int> get_affected_geo_indices() const
  {
    return affected_geo_indices_;
  }
};

}  // namespace blender::xpbd
