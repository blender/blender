/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup intern_iksolver
 */

#pragma once

/**
 * @author Laurence Bourn
 * @date 28/6/2001
 */

#include <list>
#include <vector>

#include "IK_Math.h"
#include "IK_QJacobian.h"
#include "IK_QSegment.h"
#include "IK_QTask.h"

typedef struct IK_QPoleVectorConstraint {
  int m_chain_root_index;
  IK_QSegment *m_chain_tip_seg;
  /* Not used when m_goal_segment exists. */
  Vector3d m_chain_goal_pos;
  /* NULL is valid. */
  IK_QSegment *m_chain_goal_seg;
  float m_pole_angle;
  Vector3d m_pole_pos;
} IK_QPoleVectorConstraint;

class IK_QJacobianSolver {
 public:
  IK_QJacobianSolver();
  ~IK_QJacobianSolver();

  // setup pole vector constraint
  void AddPoleVectorConstraint(int root_index,
                               IK_QSegment *tip,
                               Vector3d &goal,
                               Vector3d &polegoal,
                               float poleangle,
                               IK_QSegment *m_goal_segment);

  // call setup once before solving, if it fails don't solve
  bool Setup(IK_QSegment **roots, const int root_count, std::list<IK_QTask *> &tasks);

  // returns true if converged, false if max number of iterations was used
  bool Solve(IK_QSegment **roots,
             const int root_count,
             std::list<IK_QTask *> tasks,
             const double tolerance,
             const int max_iterations);

 private:
  void AddSegmentList(IK_QSegment *seg);
  bool UpdateAngles(double &norm);
  void ConstrainPoleVector_OneWays(IK_QSegment **roots, int root_count);
  void ConstrainPoleVector_TwoWays(IK_QSegment **roots, int root_count);

  double ComputeScale();
  void Scale(double scale, std::list<IK_QTask *> &tasks);

 private:
  IK_QJacobian m_jacobian;
  IK_QJacobian m_jacobian_sub;

  bool m_secondary_enabled;

  std::vector<IK_QSegment *> m_segments;

  std::vector<Affine3d *> m_root_matrices;
  std::vector<IK_QPoleVectorConstraint *> m_pole_constraints_oneway;
  std::vector<IK_QPoleVectorConstraint *> m_pole_constraints_twoway;
};
