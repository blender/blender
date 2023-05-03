/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup intern_iksolver
 */

#include "IK_QTask.h"

// IK_QTask

IK_QTask::IK_QTask(int size, bool primary, bool active, const IK_QSegment *segment)
    : m_size(size), m_primary(primary), m_active(active), m_segment(segment), m_weight(1.0)
{
}

// IK_QPositionTask

IK_QPositionTask::IK_QPositionTask(bool primary,
                                   const IK_QSegment *segment,
                                   const Vector3d &goal,
                                   const IK_QSegment *goalseg,
                                   const IK_QSegment *zero_weight_sentinel)
    : IK_QTask(3, primary, true, segment),
      m_goal(goal),
      m_goalsegment(goalseg),
      m_zero_weight_sentinel(zero_weight_sentinel)
{
  // computing clamping length
  int num;
  const IK_QSegment *seg;

  m_clamp_length = 0.0;
  num = 0;

  for (seg = m_segment; seg; seg = seg->Parent()) {
    m_clamp_length += seg->MaxExtension();
    num++;
  }
  for (seg = m_goalsegment; seg; seg = seg->Parent()) {
    m_clamp_length += seg->MaxExtension();
    num++;
  }

  m_clamp_length /= 2 * num;
}

void IK_QPositionTask::ComputeJacobian(IK_QJacobian &jacobian)
{
  // compute beta
  const Vector3d &pos = m_segment->GlobalEnd();

  Vector3d d_pos = m_goal - pos;
  if (m_goalsegment != NULL) {
    d_pos = m_goalsegment->GlobalEnd() - pos;
    // printf("owner endefffector: (%f, %f, %f)\n", pos[0], pos[1], pos[2]);
    // printf("dpos: (%f, %f, %f)\n", d_pos[0], d_pos[1], d_pos[2]);
    // const Vector3d &goal_pos = m_segment->GlobalEnd();
    // printf("goal pos: (%f, %f, %f)\n", goal_pos[0], goal_pos[1], goal_pos[2]);
  }

  double length = d_pos.norm();

  const double epsilon = 1e-5;
  /* m_clamp_length < epsilon when chain tip (seg) has no parent and its head is the end effector.
   */
  if (length > m_clamp_length && m_clamp_length > epsilon)
    d_pos = (m_clamp_length / length) * d_pos;

  jacobian.SetBetas(m_id, m_size, m_weight * d_pos);

  int i;
  const IK_QSegment *seg;
  float effective_weight = m_weight;
  for (seg = m_segment; seg; seg = seg->Parent()) {
    if (seg == m_zero_weight_sentinel) {
      effective_weight = 0;
    }
    Vector3d p = seg->GlobalStart() - pos;

    for (i = 0; i < seg->NumberOfDoF(); i++) {
      Vector3d axis = seg->Axis(i) * effective_weight;

      if (seg->Translational())
        jacobian.SetDerivatives(m_id, seg->DoFId() + i, axis, 1e2);
      else {
        Vector3d pa = p.cross(axis);
        jacobian.SetDerivatives(m_id, seg->DoFId() + i, pa, 1e0);
      }
    }
  }
}

double IK_QPositionTask::Distance() const
{
  const Vector3d &pos = m_segment->GlobalEnd();
  Vector3d d_pos = m_goal - pos;
  return d_pos.norm();
}

// IK_QOrientationTask

IK_QOrientationTask::IK_QOrientationTask(bool primary,
                                         const IK_QSegment *segment,
                                         const Matrix3d &goal,
                                         const IK_QSegment *goalseg,
                                         const IK_QSegment *zero_weight_sentinel)
    : IK_QTask(3, primary, true, segment),
      m_goal(goal),
      m_distance(0.0),
      m_goalsegment(goalseg),
      m_zero_weight_sentinel(zero_weight_sentinel)
{
}

void IK_QOrientationTask::ComputeJacobian(IK_QJacobian &jacobian)
{
  /** GG: NOTE: I wonder why there isn't a clamped rotation delta like there is for postiion
   * goal?*/
  // compute betas
  const Matrix3d &rot = m_segment->GlobalTransform().linear();

  Matrix3d d_rotm = (m_goal * rot.transpose()).transpose();
  if (m_goalsegment != NULL) {
    d_rotm = (m_goalsegment->GlobalTransform().linear() * rot.transpose()).transpose();
  }

  Vector3d d_rot;
  d_rot = -0.5 * Vector3d(d_rotm(2, 1) - d_rotm(1, 2),
                          d_rotm(0, 2) - d_rotm(2, 0),
                          d_rotm(1, 0) - d_rotm(0, 1));

  m_distance = d_rot.norm();

  jacobian.SetBetas(m_id, m_size, m_weight * d_rot);

  // compute derivatives
  int i;
  const IK_QSegment *seg;
  float effective_weight = m_weight;
  for (seg = m_segment; seg; seg = seg->Parent()) {
    if (seg == m_zero_weight_sentinel) {
      effective_weight = 0;
    }
    for (i = 0; i < seg->NumberOfDoF(); i++) {

      if (seg->Translational())
        /** GG: Q: Why even have jacobian entries for translation segments? Does that not
         * introduce numeric instability or unnecessary computations?
         * A: The jacobian is used for all tasks, including position tasks, so
         * the entries will exist and must be initialized anyways. Though, it is
         * redundant since we know it'll always be zero for translation segments
         * and never change.
         */
        jacobian.SetDerivatives(m_id, seg->DoFId() + i, Vector3d(0, 0, 0), 1e2);
      else {
        /** GG: NOTE: so beta is an axis+angle, so maybe that's why inputs are axis+angles too? I
         * wonder if there is not explicit conversion for handling jacobain for orientation task
         * vs position task  because maybe it's implicit? That the derivatives provided are
         * associated w/ a unit change in input, therefore, there is no need to explcitly convert
         * anything? I.e. even if the beta was in Euler angles, as long as we provide Euler
         * derivatives, then no further explicit conversion is needed?
         */
        Vector3d axis = seg->Axis(i) * effective_weight;
        jacobian.SetDerivatives(m_id, seg->DoFId() + i, axis, 1e0);
      }
    }
  }
}

// IK_QCenterOfMassTask
// NOTE: implementation not finished!

IK_QCenterOfMassTask::IK_QCenterOfMassTask(bool primary,
                                           const IK_QSegment *segment,
                                           const Vector3d &goal_center)
    : IK_QTask(3, primary, true, segment), m_goal_center(goal_center)
{
  m_total_mass_inv = ComputeTotalMass(m_segment);
  if (!FuzzyZero(m_total_mass_inv))
    m_total_mass_inv = 1.0 / m_total_mass_inv;
}

double IK_QCenterOfMassTask::ComputeTotalMass(const IK_QSegment *segment)
{
  double mass = /*seg->Mass()*/ 1.0;

  const IK_QSegment *seg;
  for (seg = segment->Child(); seg; seg = seg->Sibling())
    mass += ComputeTotalMass(seg);

  return mass;
}

Vector3d IK_QCenterOfMassTask::ComputeCenter(const IK_QSegment *segment)
{
  Vector3d center = /*seg->Mass()**/ segment->GlobalStart();

  const IK_QSegment *seg;
  for (seg = segment->Child(); seg; seg = seg->Sibling())
    center += ComputeCenter(seg);

  return center;
}

void IK_QCenterOfMassTask::JacobianSegment(IK_QJacobian &jacobian,
                                           Vector3d &center,
                                           const IK_QSegment *segment)
{
  int i;
  Vector3d p = center - segment->GlobalStart();

  for (i = 0; i < segment->NumberOfDoF(); i++) {
    Vector3d axis = segment->Axis(i) * m_weight;
    axis *= /*segment->Mass()**/ m_total_mass_inv;

    if (segment->Translational())
      jacobian.SetDerivatives(m_id, segment->DoFId() + i, axis, 1e2);
    else {
      Vector3d pa = axis.cross(p);
      jacobian.SetDerivatives(m_id, segment->DoFId() + i, pa, 1e0);
    }
  }

  const IK_QSegment *seg;
  for (seg = segment->Child(); seg; seg = seg->Sibling())
    JacobianSegment(jacobian, center, seg);
}

void IK_QCenterOfMassTask::ComputeJacobian(IK_QJacobian &jacobian)
{
  Vector3d center = ComputeCenter(m_segment) * m_total_mass_inv;

  // compute beta
  Vector3d d_pos = m_goal_center - center;

  m_distance = d_pos.norm();

#if 0
  if (m_distance > m_clamp_length)
    d_pos = (m_clamp_length / m_distance) * d_pos;
#endif

  jacobian.SetBetas(m_id, m_size, m_weight * d_pos);

  // compute derivatives
  JacobianSegment(jacobian, center, m_segment);
}

double IK_QCenterOfMassTask::Distance() const
{
  return m_distance;
}
