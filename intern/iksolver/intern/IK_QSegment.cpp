/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup iksolver
 */

#include "IK_QSegment.h"

// IK_QSegment

IK_QSegment::IK_QSegment(int num_DoF, bool translational)
    : m_parent(NULL),
      m_child(NULL),
      m_sibling(NULL),
      m_composite(NULL),
      m_num_DoF(num_DoF),
      m_translational(translational)
{
  m_locked[0] = m_locked[1] = m_locked[2] = false;
  m_weight[0] = m_weight[1] = m_weight[2] = 1.0;

  m_max_extension = 0.0;

  m_start = Vector3d(0, 0, 0);
  m_rest_basis.setIdentity();
  m_basis.setIdentity();
  m_translation = Vector3d(0, 0, 0);

  m_orig_basis = m_basis;
  m_orig_translation = m_translation;
}

void IK_QSegment::Reset()
{
  m_locked[0] = m_locked[1] = m_locked[2] = false;

  m_basis = m_orig_basis;
  m_translation = m_orig_translation;
  SetBasis(m_basis);

  for (IK_QSegment *seg = m_child; seg; seg = seg->m_sibling)
    seg->Reset();
}

void IK_QSegment::SetTransform(const Vector3d &start,
                               const Matrix3d &rest_basis,
                               const Matrix3d &basis,
                               const double length)
{
  m_max_extension = start.norm() + length;

  m_start = start;
  m_rest_basis = rest_basis;

  m_orig_basis = basis;
  SetBasis(basis);

  m_translation = Vector3d(0, length, 0);
  m_orig_translation = m_translation;
}

Matrix3d IK_QSegment::BasisChange() const
{
  return m_orig_basis.transpose() * m_basis;
}

Vector3d IK_QSegment::TranslationChange() const
{
  return m_translation - m_orig_translation;
}

IK_QSegment::~IK_QSegment()
{
  if (m_parent)
    m_parent->RemoveChild(this);

  for (IK_QSegment *seg = m_child; seg; seg = seg->m_sibling)
    seg->m_parent = NULL;
}

void IK_QSegment::SetParent(IK_QSegment *parent)
{
  if (m_parent == parent)
    return;

  if (m_parent)
    m_parent->RemoveChild(this);

  if (parent) {
    m_sibling = parent->m_child;
    parent->m_child = this;
  }

  m_parent = parent;
}

void IK_QSegment::SetComposite(IK_QSegment *seg)
{
  m_composite = seg;
}

void IK_QSegment::RemoveChild(IK_QSegment *child)
{
  if (m_child == NULL)
    return;
  else if (m_child == child)
    m_child = m_child->m_sibling;
  else {
    IK_QSegment *seg = m_child;

    while (seg->m_sibling != child)
      seg = seg->m_sibling;

    if (child == seg->m_sibling)
      seg->m_sibling = child->m_sibling;
  }
}

void IK_QSegment::UpdateTransform(const Affine3d &global)
{
  // compute the global transform at the end of the segment
  m_global_start = global.translation() + global.linear() * m_start;

  m_global_transform.translation() = m_global_start;
  m_global_transform.linear() = global.linear() * m_rest_basis * m_basis;
  m_global_transform.translate(m_translation);

  // update child transforms
  for (IK_QSegment *seg = m_child; seg; seg = seg->m_sibling)
    seg->UpdateTransform(m_global_transform);
}

void IK_QSegment::PrependBasis(const Matrix3d &mat)
{
  m_basis = m_rest_basis.inverse() * mat * m_rest_basis * m_basis;
}

void IK_QSegment::Scale(double scale)
{
  m_start *= scale;
  m_translation *= scale;
  m_orig_translation *= scale;
  m_global_start *= scale;
  m_global_transform.translation() *= scale;
  m_max_extension *= scale;
}

// IK_QSphericalSegment

IK_QSphericalSegment::IK_QSphericalSegment()
    : IK_QSegment(3, false), m_limit_x(false), m_limit_y(false), m_limit_z(false)
{
}

Vector3d IK_QSphericalSegment::Axis(int dof) const
{
  return m_global_transform.linear().col(dof);
}

void IK_QSphericalSegment::SetLimit(int axis, double lmin, double lmax)
{
  if (lmin > lmax)
    return;

  if (axis == 1) {
    lmin = Clamp(lmin, -M_PI, M_PI);
    lmax = Clamp(lmax, -M_PI, M_PI);

    m_min_y = lmin;
    m_max_y = lmax;

    m_limit_y = true;
  }
  else {
    // clamp and convert to axis angle parameters
    lmin = Clamp(lmin, -M_PI, M_PI);
    lmax = Clamp(lmax, -M_PI, M_PI);

    lmin = sin(lmin * 0.5);
    lmax = sin(lmax * 0.5);

    if (axis == 0) {
      m_min[0] = -lmax;
      m_max[0] = -lmin;
      m_limit_x = true;
    }
    else if (axis == 2) {
      m_min[1] = -lmax;
      m_max[1] = -lmin;
      m_limit_z = true;
    }
  }
}

void IK_QSphericalSegment::SetWeight(int axis, double weight)
{
  m_weight[axis] = weight;
}

bool IK_QSphericalSegment::UpdateAngle(const IK_QJacobian &jacobian, Vector3d &delta, bool *clamp)
{
  if (m_locked[0] && m_locked[1] && m_locked[2])
    return false;

  Vector3d dq;
  dq.x() = jacobian.AngleUpdate(m_DoF_id);
  dq.y() = jacobian.AngleUpdate(m_DoF_id + 1);
  dq.z() = jacobian.AngleUpdate(m_DoF_id + 2);

  // Directly update the rotation matrix, with Rodrigues' rotation formula,
  // to avoid singularities and allow smooth integration.

  double theta = dq.norm();

  if (!FuzzyZero(theta)) {
    Vector3d w = dq * (1.0 / theta);

    double sine = sin(theta);
    double cosine = cos(theta);
    double cosineInv = 1 - cosine;

    double xsine = w.x() * sine;
    double ysine = w.y() * sine;
    double zsine = w.z() * sine;

    double xxcosine = w.x() * w.x() * cosineInv;
    double xycosine = w.x() * w.y() * cosineInv;
    double xzcosine = w.x() * w.z() * cosineInv;
    double yycosine = w.y() * w.y() * cosineInv;
    double yzcosine = w.y() * w.z() * cosineInv;
    double zzcosine = w.z() * w.z() * cosineInv;

    Matrix3d M = CreateMatrix(cosine + xxcosine,
                              -zsine + xycosine,
                              ysine + xzcosine,
                              zsine + xycosine,
                              cosine + yycosine,
                              -xsine + yzcosine,
                              -ysine + xzcosine,
                              xsine + yzcosine,
                              cosine + zzcosine);

    m_new_basis = m_basis * M;
  }
  else
    m_new_basis = m_basis;

  if (m_limit_y == false && m_limit_x == false && m_limit_z == false)
    return false;

  Vector3d a = SphericalRangeParameters(m_new_basis);

  if (m_locked[0])
    a.x() = m_locked_ax;
  if (m_locked[1])
    a.y() = m_locked_ay;
  if (m_locked[2])
    a.z() = m_locked_az;

  double ax = a.x(), ay = a.y(), az = a.z();

  clamp[0] = clamp[1] = clamp[2] = false;

  if (m_limit_y) {
    if (a.y() > m_max_y) {
      ay = m_max_y;
      clamp[1] = true;
    }
    else if (a.y() < m_min_y) {
      ay = m_min_y;
      clamp[1] = true;
    }
  }

  if (m_limit_x && m_limit_z) {
    if (EllipseClamp(ax, az, m_min, m_max))
      clamp[0] = clamp[2] = true;
  }
  else if (m_limit_x) {
    if (ax < m_min[0]) {
      ax = m_min[0];
      clamp[0] = true;
    }
    else if (ax > m_max[0]) {
      ax = m_max[0];
      clamp[0] = true;
    }
  }
  else if (m_limit_z) {
    if (az < m_min[1]) {
      az = m_min[1];
      clamp[2] = true;
    }
    else if (az > m_max[1]) {
      az = m_max[1];
      clamp[2] = true;
    }
  }

  if (clamp[0] == false && clamp[1] == false && clamp[2] == false) {
    if (m_locked[0] || m_locked[1] || m_locked[2])
      m_new_basis = ComputeSwingMatrix(ax, az) * ComputeTwistMatrix(ay);
    return false;
  }

  m_new_basis = ComputeSwingMatrix(ax, az) * ComputeTwistMatrix(ay);

  delta = MatrixToAxisAngle(m_basis.transpose() * m_new_basis);

  if (!(m_locked[0] || m_locked[2]) && (clamp[0] || clamp[2])) {
    m_locked_ax = ax;
    m_locked_az = az;
  }

  if (!m_locked[1] && clamp[1])
    m_locked_ay = ay;

  return true;
}

void IK_QSphericalSegment::Lock(int dof, IK_QJacobian &jacobian, Vector3d &delta)
{
  if (dof == 1) {
    m_locked[1] = true;
    jacobian.Lock(m_DoF_id + 1, delta[1]);
  }
  else {
    m_locked[0] = m_locked[2] = true;
    jacobian.Lock(m_DoF_id, delta[0]);
    jacobian.Lock(m_DoF_id + 2, delta[2]);
  }
}

void IK_QSphericalSegment::UpdateAngleApply()
{
  m_basis = m_new_basis;
}

// IK_QNullSegment

IK_QNullSegment::IK_QNullSegment() : IK_QSegment(0, false)
{
}

// IK_QRevoluteSegment

IK_QRevoluteSegment::IK_QRevoluteSegment(int axis)
    : IK_QSegment(1, false), m_axis(axis), m_angle(0.0), m_limit(false)
{
}

void IK_QRevoluteSegment::SetBasis(const Matrix3d &basis)
{
  if (m_axis == 1) {
    m_angle = ComputeTwist(basis);
    m_basis = ComputeTwistMatrix(m_angle);
  }
  else {
    m_angle = EulerAngleFromMatrix(basis, m_axis);
    m_basis = RotationMatrix(m_angle, m_axis);
  }
}

Vector3d IK_QRevoluteSegment::Axis(int) const
{
  return m_global_transform.linear().col(m_axis);
}

bool IK_QRevoluteSegment::UpdateAngle(const IK_QJacobian &jacobian, Vector3d &delta, bool *clamp)
{
  if (m_locked[0])
    return false;

  m_new_angle = m_angle + jacobian.AngleUpdate(m_DoF_id);

  clamp[0] = false;

  if (m_limit == false)
    return false;

  if (m_new_angle > m_max)
    delta[0] = m_max - m_angle;
  else if (m_new_angle < m_min)
    delta[0] = m_min - m_angle;
  else
    return false;

  clamp[0] = true;
  m_new_angle = m_angle + delta[0];

  return true;
}

void IK_QRevoluteSegment::Lock(int, IK_QJacobian &jacobian, Vector3d &delta)
{
  m_locked[0] = true;
  jacobian.Lock(m_DoF_id, delta[0]);
}

void IK_QRevoluteSegment::UpdateAngleApply()
{
  m_angle = m_new_angle;
  m_basis = RotationMatrix(m_angle, m_axis);
}

void IK_QRevoluteSegment::SetLimit(int axis, double lmin, double lmax)
{
  if (lmin > lmax || m_axis != axis)
    return;

  // clamp and convert to axis angle parameters
  lmin = Clamp(lmin, -M_PI, M_PI);
  lmax = Clamp(lmax, -M_PI, M_PI);

  m_min = lmin;
  m_max = lmax;

  m_limit = true;
}

void IK_QRevoluteSegment::SetWeight(int axis, double weight)
{
  if (axis == m_axis)
    m_weight[0] = weight;
}

// IK_QSwingSegment

IK_QSwingSegment::IK_QSwingSegment() : IK_QSegment(2, false), m_limit_x(false), m_limit_z(false)
{
}

void IK_QSwingSegment::SetBasis(const Matrix3d &basis)
{
  m_basis = basis;
  RemoveTwist(m_basis);
}

Vector3d IK_QSwingSegment::Axis(int dof) const
{
  return m_global_transform.linear().col((dof == 0) ? 0 : 2);
}

bool IK_QSwingSegment::UpdateAngle(const IK_QJacobian &jacobian, Vector3d &delta, bool *clamp)
{
  if (m_locked[0] && m_locked[1])
    return false;

  Vector3d dq;
  dq.x() = jacobian.AngleUpdate(m_DoF_id);
  dq.y() = 0.0;
  dq.z() = jacobian.AngleUpdate(m_DoF_id + 1);

  // Directly update the rotation matrix, with Rodrigues' rotation formula,
  // to avoid singularities and allow smooth integration.

  double theta = dq.norm();

  if (!FuzzyZero(theta)) {
    Vector3d w = dq * (1.0 / theta);

    double sine = sin(theta);
    double cosine = cos(theta);
    double cosineInv = 1 - cosine;

    double xsine = w.x() * sine;
    double zsine = w.z() * sine;

    double xxcosine = w.x() * w.x() * cosineInv;
    double xzcosine = w.x() * w.z() * cosineInv;
    double zzcosine = w.z() * w.z() * cosineInv;

    Matrix3d M = CreateMatrix(cosine + xxcosine,
                              -zsine,
                              xzcosine,
                              zsine,
                              cosine,
                              -xsine,
                              xzcosine,
                              xsine,
                              cosine + zzcosine);

    m_new_basis = m_basis * M;

    RemoveTwist(m_new_basis);
  }
  else
    m_new_basis = m_basis;

  if (m_limit_x == false && m_limit_z == false)
    return false;

  Vector3d a = SphericalRangeParameters(m_new_basis);
  double ax = 0, az = 0;

  clamp[0] = clamp[1] = false;

  if (m_limit_x && m_limit_z) {
    ax = a.x();
    az = a.z();

    if (EllipseClamp(ax, az, m_min, m_max))
      clamp[0] = clamp[1] = true;
  }
  else if (m_limit_x) {
    if (ax < m_min[0]) {
      ax = m_min[0];
      clamp[0] = true;
    }
    else if (ax > m_max[0]) {
      ax = m_max[0];
      clamp[0] = true;
    }
  }
  else if (m_limit_z) {
    if (az < m_min[1]) {
      az = m_min[1];
      clamp[1] = true;
    }
    else if (az > m_max[1]) {
      az = m_max[1];
      clamp[1] = true;
    }
  }

  if (clamp[0] == false && clamp[1] == false)
    return false;

  m_new_basis = ComputeSwingMatrix(ax, az);

  delta = MatrixToAxisAngle(m_basis.transpose() * m_new_basis);
  delta[1] = delta[2];
  delta[2] = 0.0;

  return true;
}

void IK_QSwingSegment::Lock(int, IK_QJacobian &jacobian, Vector3d &delta)
{
  m_locked[0] = m_locked[1] = true;
  jacobian.Lock(m_DoF_id, delta[0]);
  jacobian.Lock(m_DoF_id + 1, delta[1]);
}

void IK_QSwingSegment::UpdateAngleApply()
{
  m_basis = m_new_basis;
}

void IK_QSwingSegment::SetLimit(int axis, double lmin, double lmax)
{
  if (lmin > lmax)
    return;

  // clamp and convert to axis angle parameters
  lmin = Clamp(lmin, -M_PI, M_PI);
  lmax = Clamp(lmax, -M_PI, M_PI);

  lmin = sin(lmin * 0.5);
  lmax = sin(lmax * 0.5);

  // put center of ellispe in the middle between min and max
  double offset = 0.5 * (lmin + lmax);
  // lmax = lmax - offset;

  if (axis == 0) {
    m_min[0] = -lmax;
    m_max[0] = -lmin;

    m_limit_x = true;
    m_offset_x = offset;
    m_max_x = lmax;
  }
  else if (axis == 2) {
    m_min[1] = -lmax;
    m_max[1] = -lmin;

    m_limit_z = true;
    m_offset_z = offset;
    m_max_z = lmax;
  }
}

void IK_QSwingSegment::SetWeight(int axis, double weight)
{
  if (axis == 0)
    m_weight[0] = weight;
  else if (axis == 2)
    m_weight[1] = weight;
}

// IK_QElbowSegment

IK_QElbowSegment::IK_QElbowSegment(int axis)
    : IK_QSegment(2, false),
      m_axis(axis),
      m_twist(0.0),
      m_angle(0.0),
      m_cos_twist(1.0),
      m_sin_twist(0.0),
      m_limit(false),
      m_limit_twist(false)
{
}

void IK_QElbowSegment::SetBasis(const Matrix3d &basis)
{
  m_basis = basis;

  m_twist = ComputeTwist(m_basis);
  RemoveTwist(m_basis);
  m_angle = EulerAngleFromMatrix(basis, m_axis);

  m_basis = RotationMatrix(m_angle, m_axis) * ComputeTwistMatrix(m_twist);
}

Vector3d IK_QElbowSegment::Axis(int dof) const
{
  if (dof == 0) {
    Vector3d v;
    if (m_axis == 0)
      v = Vector3d(m_cos_twist, 0, m_sin_twist);
    else
      v = Vector3d(-m_sin_twist, 0, m_cos_twist);

    return m_global_transform.linear() * v;
  }
  else
    return m_global_transform.linear().col(1);
}

bool IK_QElbowSegment::UpdateAngle(const IK_QJacobian &jacobian, Vector3d &delta, bool *clamp)
{
  if (m_locked[0] && m_locked[1])
    return false;

  clamp[0] = clamp[1] = false;

  if (!m_locked[0]) {
    m_new_angle = m_angle + jacobian.AngleUpdate(m_DoF_id);

    if (m_limit) {
      if (m_new_angle > m_max) {
        delta[0] = m_max - m_angle;
        m_new_angle = m_max;
        clamp[0] = true;
      }
      else if (m_new_angle < m_min) {
        delta[0] = m_min - m_angle;
        m_new_angle = m_min;
        clamp[0] = true;
      }
    }
  }

  if (!m_locked[1]) {
    m_new_twist = m_twist + jacobian.AngleUpdate(m_DoF_id + 1);

    if (m_limit_twist) {
      if (m_new_twist > m_max_twist) {
        delta[1] = m_max_twist - m_twist;
        m_new_twist = m_max_twist;
        clamp[1] = true;
      }
      else if (m_new_twist < m_min_twist) {
        delta[1] = m_min_twist - m_twist;
        m_new_twist = m_min_twist;
        clamp[1] = true;
      }
    }
  }

  return (clamp[0] || clamp[1]);
}

void IK_QElbowSegment::Lock(int dof, IK_QJacobian &jacobian, Vector3d &delta)
{
  if (dof == 0) {
    m_locked[0] = true;
    jacobian.Lock(m_DoF_id, delta[0]);
  }
  else {
    m_locked[1] = true;
    jacobian.Lock(m_DoF_id + 1, delta[1]);
  }
}

void IK_QElbowSegment::UpdateAngleApply()
{
  m_angle = m_new_angle;
  m_twist = m_new_twist;

  m_sin_twist = sin(m_twist);
  m_cos_twist = cos(m_twist);

  Matrix3d A = RotationMatrix(m_angle, m_axis);
  Matrix3d T = RotationMatrix(m_sin_twist, m_cos_twist, 1);

  m_basis = A * T;
}

void IK_QElbowSegment::SetLimit(int axis, double lmin, double lmax)
{
  if (lmin > lmax)
    return;

  // clamp and convert to axis angle parameters
  lmin = Clamp(lmin, -M_PI, M_PI);
  lmax = Clamp(lmax, -M_PI, M_PI);

  if (axis == 1) {
    m_min_twist = lmin;
    m_max_twist = lmax;
    m_limit_twist = true;
  }
  else if (axis == m_axis) {
    m_min = lmin;
    m_max = lmax;
    m_limit = true;
  }
}

void IK_QElbowSegment::SetWeight(int axis, double weight)
{
  if (axis == m_axis)
    m_weight[0] = weight;
  else if (axis == 1)
    m_weight[1] = weight;
}

// IK_QTranslateSegment

IK_QTranslateSegment::IK_QTranslateSegment(int axis1) : IK_QSegment(1, true)
{
  m_axis_enabled[0] = m_axis_enabled[1] = m_axis_enabled[2] = false;
  m_axis_enabled[axis1] = true;

  m_axis[0] = axis1;

  m_limit[0] = m_limit[1] = m_limit[2] = false;
}

IK_QTranslateSegment::IK_QTranslateSegment(int axis1, int axis2) : IK_QSegment(2, true)
{
  m_axis_enabled[0] = m_axis_enabled[1] = m_axis_enabled[2] = false;
  m_axis_enabled[axis1] = true;
  m_axis_enabled[axis2] = true;

  m_axis[0] = axis1;
  m_axis[1] = axis2;

  m_limit[0] = m_limit[1] = m_limit[2] = false;
}

IK_QTranslateSegment::IK_QTranslateSegment() : IK_QSegment(3, true)
{
  m_axis_enabled[0] = m_axis_enabled[1] = m_axis_enabled[2] = true;

  m_axis[0] = 0;
  m_axis[1] = 1;
  m_axis[2] = 2;

  m_limit[0] = m_limit[1] = m_limit[2] = false;
}

Vector3d IK_QTranslateSegment::Axis(int dof) const
{
  return m_global_transform.linear().col(m_axis[dof]);
}

bool IK_QTranslateSegment::UpdateAngle(const IK_QJacobian &jacobian, Vector3d &delta, bool *clamp)
{
  int dof_id = m_DoF_id, dof = 0, i, clamped = false;

  Vector3d dx(0.0, 0.0, 0.0);

  for (i = 0; i < 3; i++) {
    if (!m_axis_enabled[i]) {
      m_new_translation[i] = m_translation[i];
      continue;
    }

    clamp[dof] = false;

    if (!m_locked[dof]) {
      m_new_translation[i] = m_translation[i] + jacobian.AngleUpdate(dof_id);

      if (m_limit[i]) {
        if (m_new_translation[i] > m_max[i]) {
          delta[dof] = m_max[i] - m_translation[i];
          m_new_translation[i] = m_max[i];
          clamped = clamp[dof] = true;
        }
        else if (m_new_translation[i] < m_min[i]) {
          delta[dof] = m_min[i] - m_translation[i];
          m_new_translation[i] = m_min[i];
          clamped = clamp[dof] = true;
        }
      }
    }

    dof_id++;
    dof++;
  }

  return clamped;
}

void IK_QTranslateSegment::UpdateAngleApply()
{
  m_translation = m_new_translation;
}

void IK_QTranslateSegment::Lock(int dof, IK_QJacobian &jacobian, Vector3d &delta)
{
  m_locked[dof] = true;
  jacobian.Lock(m_DoF_id + dof, delta[dof]);
}

void IK_QTranslateSegment::SetWeight(int axis, double weight)
{
  int i;

  for (i = 0; i < m_num_DoF; i++)
    if (m_axis[i] == axis)
      m_weight[i] = weight;
}

void IK_QTranslateSegment::SetLimit(int axis, double lmin, double lmax)
{
  if (lmax < lmin)
    return;

  m_min[axis] = lmin;
  m_max[axis] = lmax;
  m_limit[axis] = true;
}

void IK_QTranslateSegment::Scale(double scale)
{
  int i;

  IK_QSegment::Scale(scale);

  for (i = 0; i < 3; i++) {
    m_min[0] *= scale;
    m_max[1] *= scale;
  }

  m_new_translation *= scale;
}
