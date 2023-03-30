/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup intern_iksolver
 */

#pragma once

#include "IK_Math.h"
#include "IK_QJacobian.h"
#include "IK_QSegment.h"

class IK_QTask {
 public:
  IK_QTask(int size, bool primary, bool active, const IK_QSegment *segment);
  virtual ~IK_QTask() {}

  int Id() const
  {
    return m_size;
  }

  void SetId(int id)
  {
    m_id = id;
  }

  int Size() const
  {
    return m_size;
  }

  bool Primary() const
  {
    return m_primary;
  }

  bool Active() const
  {
    return m_active;
  }

  double Weight() const
  {
    return m_weight * m_weight;
  }

  void SetWeight(double weight)
  {
    m_weight = sqrt(weight);
  }

  virtual void ComputeJacobian(IK_QJacobian &jacobian) = 0;

  virtual double Distance() const = 0;

  virtual bool PositionTask() const
  {
    return false;
  }

  virtual void Scale(double) {}

 protected:
  int m_id;
  int m_size;
  bool m_primary;
  bool m_active;
  const IK_QSegment *m_segment;
  double m_weight;
};

class IK_QPositionTask : public IK_QTask {
 public:
  IK_QPositionTask(bool primary, const IK_QSegment *segment, const Vector3d &goal);

  void ComputeJacobian(IK_QJacobian &jacobian);

  double Distance() const;

  bool PositionTask() const
  {
    return true;
  }
  void Scale(double scale)
  {
    m_goal *= scale;
    m_clamp_length *= scale;
  }

 private:
  Vector3d m_goal;
  double m_clamp_length;
};

class IK_QOrientationTask : public IK_QTask {
 public:
  IK_QOrientationTask(bool primary, const IK_QSegment *segment, const Matrix3d &goal);

  double Distance() const
  {
    return m_distance;
  }
  void ComputeJacobian(IK_QJacobian &jacobian);

 private:
  Matrix3d m_goal;
  double m_distance;
};

class IK_QCenterOfMassTask : public IK_QTask {
 public:
  IK_QCenterOfMassTask(bool primary, const IK_QSegment *segment, const Vector3d &center);

  void ComputeJacobian(IK_QJacobian &jacobian);

  double Distance() const;

  void Scale(double scale)
  {
    m_goal_center *= scale;
    m_distance *= scale;
  }

 private:
  double ComputeTotalMass(const IK_QSegment *segment);
  Vector3d ComputeCenter(const IK_QSegment *segment);
  void JacobianSegment(IK_QJacobian &jacobian, Vector3d &center, const IK_QSegment *segment);

  Vector3d m_goal_center;
  double m_total_mass_inv;
  double m_distance;
};
