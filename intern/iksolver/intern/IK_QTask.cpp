/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

IK_QPositionTask::IK_QPositionTask(bool primary, const IK_QSegment *segment, const Vector3d &goal)
    : IK_QTask(3, primary, true, segment), m_goal(goal)
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

  m_clamp_length /= 2 * num;
}

void IK_QPositionTask::ComputeJacobian(IK_QJacobian &jacobian)
{
  // compute beta
  const Vector3d &pos = m_segment->GlobalEnd();

  Vector3d d_pos = m_goal - pos;
  double length = d_pos.norm();

  if (length > m_clamp_length) {
    d_pos = (m_clamp_length / length) * d_pos;
  }

  jacobian.SetBetas(m_id, m_size, m_weight * d_pos);

  // compute derivatives
  int i;
  const IK_QSegment *seg;

  for (seg = m_segment; seg; seg = seg->Parent()) {
    Vector3d p = seg->GlobalStart() - pos;

    for (i = 0; i < seg->NumberOfDoF(); i++) {
      Vector3d axis = seg->Axis(i) * m_weight;

      if (seg->Translational()) {
        jacobian.SetDerivatives(m_id, seg->DoFId() + i, axis, 1e2);
      }
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
                                         const Matrix3d &goal)
    : IK_QTask(3, primary, true, segment), m_goal(goal), m_distance(0.0)
{
}

void IK_QOrientationTask::ComputeJacobian(IK_QJacobian &jacobian)
{
  // compute betas
  const Matrix3d &rot = m_segment->GlobalTransform().linear();

  Matrix3d d_rotm = (m_goal * rot.transpose()).transpose();

  Vector3d d_rot;
  d_rot = -0.5 * Vector3d(d_rotm(2, 1) - d_rotm(1, 2),
                          d_rotm(0, 2) - d_rotm(2, 0),
                          d_rotm(1, 0) - d_rotm(0, 1));

  m_distance = d_rot.norm();

  jacobian.SetBetas(m_id, m_size, m_weight * d_rot);

  // compute derivatives
  int i;
  const IK_QSegment *seg;

  for (seg = m_segment; seg; seg = seg->Parent()) {
    for (i = 0; i < seg->NumberOfDoF(); i++) {

      if (seg->Translational()) {
        jacobian.SetDerivatives(m_id, seg->DoFId() + i, Vector3d(0, 0, 0), 1e2);
      }
      else {
        Vector3d axis = seg->Axis(i) * m_weight;
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
  if (!FuzzyZero(m_total_mass_inv)) {
    m_total_mass_inv = 1.0 / m_total_mass_inv;
  }
}

double IK_QCenterOfMassTask::ComputeTotalMass(const IK_QSegment *segment)
{
  double mass = /*seg->Mass()*/ 1.0;

  const IK_QSegment *seg;
  for (seg = segment->Child(); seg; seg = seg->Sibling()) {
    mass += ComputeTotalMass(seg);
  }

  return mass;
}

Vector3d IK_QCenterOfMassTask::ComputeCenter(const IK_QSegment *segment)
{
  Vector3d center = /*seg->Mass()**/ segment->GlobalStart();

  const IK_QSegment *seg;
  for (seg = segment->Child(); seg; seg = seg->Sibling()) {
    center += ComputeCenter(seg);
  }

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

    if (segment->Translational()) {
      jacobian.SetDerivatives(m_id, segment->DoFId() + i, axis, 1e2);
    }
    else {
      Vector3d pa = axis.cross(p);
      jacobian.SetDerivatives(m_id, segment->DoFId() + i, pa, 1e0);
    }
  }

  const IK_QSegment *seg;
  for (seg = segment->Child(); seg; seg = seg->Sibling()) {
    JacobianSegment(jacobian, center, seg);
  }
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
