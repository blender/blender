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

class IK_QJacobianSolver {
 public:
  IK_QJacobianSolver();
  ~IK_QJacobianSolver() {}

  // setup pole vector constraint
  void SetPoleVectorConstraint(
      IK_QSegment *tip, Vector3d &goal, Vector3d &polegoal, float poleangle, bool getangle);
  float GetPoleAngle()
  {
    return m_poleangle;
  }

  // call setup once before solving, if it fails don't solve
  bool Setup(IK_QSegment *root, std::list<IK_QTask *> &tasks);

  // returns true if converged, false if max number of iterations was used
  bool Solve(IK_QSegment *root,
             std::list<IK_QTask *> tasks,
             const double tolerance,
             const int max_iterations);

 private:
  void AddSegmentList(IK_QSegment *seg);
  bool UpdateAngles(double &norm);
  void ConstrainPoleVector(IK_QSegment *root, std::list<IK_QTask *> &tasks);

  double ComputeScale();
  void Scale(double scale, std::list<IK_QTask *> &tasks);

 private:
  IK_QJacobian m_jacobian;
  IK_QJacobian m_jacobian_sub;

  bool m_secondary_enabled;

  std::vector<IK_QSegment *> m_segments;

  Affine3d m_rootmatrix;

  bool m_poleconstraint;
  bool m_getpoleangle;
  Vector3d m_goal;
  Vector3d m_polegoal;
  float m_poleangle;
  IK_QSegment *m_poletip;
};
