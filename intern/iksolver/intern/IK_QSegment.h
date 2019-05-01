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
 * Original author: Laurence
 */

/** \file
 * \ingroup iksolver
 */

#pragma once

#include "IK_Math.h"
#include "IK_QJacobian.h"

#include <vector>

/**
 * An IK_Qsegment encodes information about a segments
 * local coordinate system.
 *
 * These segments always point along the y-axis.
 *
 * Here we define the local coordinates of a joint as
 * local_transform =
 * translate(tr1) * rotation(A) * rotation(q) * translate(0,length,0)
 * You can read this as:
 * - first translate by (0,length,0)
 * - multiply by the rotation matrix derived from the current
 * angle parameterization q.
 * - multiply by the user defined matrix representing the rest
 * position of the bone.
 * - translate by the used defined translation (tr1)
 * The ordering of these transformations is vital, you must
 * use exactly the same transformations when displaying the segments
 */

class IK_QSegment {
 public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW
  virtual ~IK_QSegment();

  // start: a user defined translation
  // rest_basis: a user defined rotation
  // basis: a user defined rotation
  // length: length of this segment

  void SetTransform(const Vector3d &start,
                    const Matrix3d &rest_basis,
                    const Matrix3d &basis,
                    const double length);

  // tree structure access
  void SetParent(IK_QSegment *parent);

  IK_QSegment *Child() const
  {
    return m_child;
  }

  IK_QSegment *Sibling() const
  {
    return m_sibling;
  }

  IK_QSegment *Parent() const
  {
    return m_parent;
  }

  // for combining two joints into one from the interface
  void SetComposite(IK_QSegment *seg);

  IK_QSegment *Composite() const
  {
    return m_composite;
  }

  // number of degrees of freedom
  int NumberOfDoF() const
  {
    return m_num_DoF;
  }

  // unique id for this segment, for identification in the jacobian
  int DoFId() const
  {
    return m_DoF_id;
  }

  void SetDoFId(int dof_id)
  {
    m_DoF_id = dof_id;
  }

  // the max distance of the end of this bone from the local origin.
  const double MaxExtension() const
  {
    return m_max_extension;
  }

  // the change in rotation and translation w.r.t. the rest pose
  Matrix3d BasisChange() const;
  Vector3d TranslationChange() const;

  // the start and end of the segment
  const Vector3d GlobalStart() const
  {
    return m_global_start;
  }

  const Vector3d GlobalEnd() const
  {
    return m_global_transform.translation();
  }

  // the global transformation at the end of the segment
  const Affine3d &GlobalTransform() const
  {
    return m_global_transform;
  }

  // is a translational segment?
  bool Translational() const
  {
    return m_translational;
  }

  // locking (during inner clamping loop)
  bool Locked(int dof) const
  {
    return m_locked[dof];
  }

  void UnLock()
  {
    m_locked[0] = m_locked[1] = m_locked[2] = false;
  }

  // per dof joint weighting
  double Weight(int dof) const
  {
    return m_weight[dof];
  }

  void ScaleWeight(int dof, double scale)
  {
    m_weight[dof] *= scale;
  }

  // recursively update the global coordinates of this segment, 'global'
  // is the global transformation from the parent segment
  void UpdateTransform(const Affine3d &global);

  // get axis from rotation matrix for derivative computation
  virtual Vector3d Axis(int dof) const = 0;

  // update the angles using the dTheta's computed using the jacobian matrix
  virtual bool UpdateAngle(const IK_QJacobian &, Vector3d &, bool *) = 0;
  virtual void Lock(int, IK_QJacobian &, Vector3d &)
  {
  }
  virtual void UpdateAngleApply() = 0;

  // set joint limits
  virtual void SetLimit(int, double, double)
  {
  }

  // set joint weights (per axis)
  virtual void SetWeight(int, double)
  {
  }

  virtual void SetBasis(const Matrix3d &basis)
  {
    m_basis = basis;
  }

  // functions needed for pole vector constraint
  void PrependBasis(const Matrix3d &mat);
  void Reset();

  // scale
  virtual void Scale(double scale);

 protected:
  // num_DoF: number of degrees of freedom
  IK_QSegment(int num_DoF, bool translational);

  // remove child as a child of this segment
  void RemoveChild(IK_QSegment *child);

  // tree structure variables
  IK_QSegment *m_parent;
  IK_QSegment *m_child;
  IK_QSegment *m_sibling;
  IK_QSegment *m_composite;

  // full transform =
  // start * rest_basis * basis * translation
  Vector3d m_start;
  Matrix3d m_rest_basis;
  Matrix3d m_basis;
  Vector3d m_translation;

  // original basis
  Matrix3d m_orig_basis;
  Vector3d m_orig_translation;

  // maximum extension of this segment
  double m_max_extension;

  // accumulated transformations starting from root
  Vector3d m_global_start;
  Affine3d m_global_transform;

  // number degrees of freedom, (first) id of this segments DOF's
  int m_num_DoF, m_DoF_id;

  bool m_locked[3];
  bool m_translational;
  double m_weight[3];
};

class IK_QSphericalSegment : public IK_QSegment {
 public:
  IK_QSphericalSegment();

  Vector3d Axis(int dof) const;

  bool UpdateAngle(const IK_QJacobian &jacobian, Vector3d &delta, bool *clamp);
  void Lock(int dof, IK_QJacobian &jacobian, Vector3d &delta);
  void UpdateAngleApply();

  bool ComputeClampRotation(Vector3d &clamp);

  void SetLimit(int axis, double lmin, double lmax);
  void SetWeight(int axis, double weight);

 private:
  Matrix3d m_new_basis;
  bool m_limit_x, m_limit_y, m_limit_z;
  double m_min[2], m_max[2];
  double m_min_y, m_max_y, m_max_x, m_max_z, m_offset_x, m_offset_z;
  double m_locked_ax, m_locked_ay, m_locked_az;
};

class IK_QNullSegment : public IK_QSegment {
 public:
  IK_QNullSegment();

  bool UpdateAngle(const IK_QJacobian &, Vector3d &, bool *)
  {
    return false;
  }
  void UpdateAngleApply()
  {
  }

  Vector3d Axis(int) const
  {
    return Vector3d(0, 0, 0);
  }
  void SetBasis(const Matrix3d &)
  {
    m_basis.setIdentity();
  }
};

class IK_QRevoluteSegment : public IK_QSegment {
 public:
  // axis: the axis of the DoF, in range 0..2
  IK_QRevoluteSegment(int axis);

  Vector3d Axis(int dof) const;

  bool UpdateAngle(const IK_QJacobian &jacobian, Vector3d &delta, bool *clamp);
  void Lock(int dof, IK_QJacobian &jacobian, Vector3d &delta);
  void UpdateAngleApply();

  void SetLimit(int axis, double lmin, double lmax);
  void SetWeight(int axis, double weight);
  void SetBasis(const Matrix3d &basis);

 private:
  int m_axis;
  double m_angle, m_new_angle;
  bool m_limit;
  double m_min, m_max;
};

class IK_QSwingSegment : public IK_QSegment {
 public:
  // XZ DOF, uses one direct rotation
  IK_QSwingSegment();

  Vector3d Axis(int dof) const;

  bool UpdateAngle(const IK_QJacobian &jacobian, Vector3d &delta, bool *clamp);
  void Lock(int dof, IK_QJacobian &jacobian, Vector3d &delta);
  void UpdateAngleApply();

  void SetLimit(int axis, double lmin, double lmax);
  void SetWeight(int axis, double weight);
  void SetBasis(const Matrix3d &basis);

 private:
  Matrix3d m_new_basis;
  bool m_limit_x, m_limit_z;
  double m_min[2], m_max[2];
  double m_max_x, m_max_z, m_offset_x, m_offset_z;
};

class IK_QElbowSegment : public IK_QSegment {
 public:
  // XY or ZY DOF, uses two sequential rotations: first rotate around
  // X or Z, then rotate around Y (twist)
  IK_QElbowSegment(int axis);

  Vector3d Axis(int dof) const;

  bool UpdateAngle(const IK_QJacobian &jacobian, Vector3d &delta, bool *clamp);
  void Lock(int dof, IK_QJacobian &jacobian, Vector3d &delta);
  void UpdateAngleApply();

  void SetLimit(int axis, double lmin, double lmax);
  void SetWeight(int axis, double weight);
  void SetBasis(const Matrix3d &basis);

 private:
  int m_axis;

  double m_twist, m_angle, m_new_twist, m_new_angle;
  double m_cos_twist, m_sin_twist;

  bool m_limit, m_limit_twist;
  double m_min, m_max, m_min_twist, m_max_twist;
};

class IK_QTranslateSegment : public IK_QSegment {
 public:
  // 1DOF, 2DOF or 3DOF translational segments
  IK_QTranslateSegment(int axis1);
  IK_QTranslateSegment(int axis1, int axis2);
  IK_QTranslateSegment();

  Vector3d Axis(int dof) const;

  bool UpdateAngle(const IK_QJacobian &jacobian, Vector3d &delta, bool *clamp);
  void Lock(int, IK_QJacobian &, Vector3d &);
  void UpdateAngleApply();

  void SetWeight(int axis, double weight);
  void SetLimit(int axis, double lmin, double lmax);

  void Scale(double scale);

 private:
  int m_axis[3];
  bool m_axis_enabled[3], m_limit[3];
  Vector3d m_new_translation;
  double m_min[3], m_max[3];
};
