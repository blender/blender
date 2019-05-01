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

#include <stdio.h>

#include "IK_QJacobianSolver.h"

//#include "analyze.h"
IK_QJacobianSolver::IK_QJacobianSolver()
{
  m_poleconstraint = false;
  m_getpoleangle = false;
  m_rootmatrix.setIdentity();
}

double IK_QJacobianSolver::ComputeScale()
{
  std::vector<IK_QSegment *>::iterator seg;
  double length = 0.0f;

  for (seg = m_segments.begin(); seg != m_segments.end(); seg++)
    length += (*seg)->MaxExtension();

  if (length == 0.0)
    return 1.0;
  else
    return 1.0 / length;
}

void IK_QJacobianSolver::Scale(double scale, std::list<IK_QTask *> &tasks)
{
  std::list<IK_QTask *>::iterator task;
  std::vector<IK_QSegment *>::iterator seg;

  for (task = tasks.begin(); task != tasks.end(); task++)
    (*task)->Scale(scale);

  for (seg = m_segments.begin(); seg != m_segments.end(); seg++)
    (*seg)->Scale(scale);

  m_rootmatrix.translation() *= scale;
  m_goal *= scale;
  m_polegoal *= scale;
}

void IK_QJacobianSolver::AddSegmentList(IK_QSegment *seg)
{
  m_segments.push_back(seg);

  IK_QSegment *child;
  for (child = seg->Child(); child; child = child->Sibling())
    AddSegmentList(child);
}

bool IK_QJacobianSolver::Setup(IK_QSegment *root, std::list<IK_QTask *> &tasks)
{
  m_segments.clear();
  AddSegmentList(root);

  // assign each segment a unique id for the jacobian
  std::vector<IK_QSegment *>::iterator seg;
  int num_dof = 0;

  for (seg = m_segments.begin(); seg != m_segments.end(); seg++) {
    (*seg)->SetDoFId(num_dof);
    num_dof += (*seg)->NumberOfDoF();
  }

  if (num_dof == 0)
    return false;

  // compute task id's and assing weights to task
  int primary_size = 0, primary = 0;
  int secondary_size = 0, secondary = 0;
  double primary_weight = 0.0, secondary_weight = 0.0;
  std::list<IK_QTask *>::iterator task;

  for (task = tasks.begin(); task != tasks.end(); task++) {
    IK_QTask *qtask = *task;

    if (qtask->Primary()) {
      qtask->SetId(primary_size);
      primary_size += qtask->Size();
      primary_weight += qtask->Weight();
      primary++;
    }
    else {
      qtask->SetId(secondary_size);
      secondary_size += qtask->Size();
      secondary_weight += qtask->Weight();
      secondary++;
    }
  }

  if (primary_size == 0 || FuzzyZero(primary_weight))
    return false;

  m_secondary_enabled = (secondary > 0);

  // rescale weights of tasks to sum up to 1
  double primary_rescale = 1.0 / primary_weight;
  double secondary_rescale;
  if (FuzzyZero(secondary_weight))
    secondary_rescale = 0.0;
  else
    secondary_rescale = 1.0 / secondary_weight;

  for (task = tasks.begin(); task != tasks.end(); task++) {
    IK_QTask *qtask = *task;

    if (qtask->Primary())
      qtask->SetWeight(qtask->Weight() * primary_rescale);
    else
      qtask->SetWeight(qtask->Weight() * secondary_rescale);
  }

  // set matrix sizes
  m_jacobian.ArmMatrices(num_dof, primary_size);
  if (secondary > 0)
    m_jacobian_sub.ArmMatrices(num_dof, secondary_size);

  // set dof weights
  int i;

  for (seg = m_segments.begin(); seg != m_segments.end(); seg++)
    for (i = 0; i < (*seg)->NumberOfDoF(); i++)
      m_jacobian.SetDoFWeight((*seg)->DoFId() + i, (*seg)->Weight(i));

  return true;
}

void IK_QJacobianSolver::SetPoleVectorConstraint(
    IK_QSegment *tip, Vector3d &goal, Vector3d &polegoal, float poleangle, bool getangle)
{
  m_poleconstraint = true;
  m_poletip = tip;
  m_goal = goal;
  m_polegoal = polegoal;
  m_poleangle = (getangle) ? 0.0f : poleangle;
  m_getpoleangle = getangle;
}

void IK_QJacobianSolver::ConstrainPoleVector(IK_QSegment *root, std::list<IK_QTask *> &tasks)
{
  // this function will be called before and after solving. calling it before
  // solving gives predictable solutions by rotating towards the solution,
  // and calling it afterwards ensures the solution is exact.

  if (!m_poleconstraint)
    return;

  // disable pole vector constraint in case of multiple position tasks
  std::list<IK_QTask *>::iterator task;
  int positiontasks = 0;

  for (task = tasks.begin(); task != tasks.end(); task++)
    if ((*task)->PositionTask())
      positiontasks++;

  if (positiontasks >= 2) {
    m_poleconstraint = false;
    return;
  }

  // get positions and rotations
  root->UpdateTransform(m_rootmatrix);

  const Vector3d rootpos = root->GlobalStart();
  const Vector3d endpos = m_poletip->GlobalEnd();
  const Matrix3d &rootbasis = root->GlobalTransform().linear();

  // construct "lookat" matrices (like gluLookAt), based on a direction and
  // an up vector, with the direction going from the root to the end effector
  // and the up vector going from the root to the pole constraint position.
  Vector3d dir = normalize(endpos - rootpos);
  Vector3d rootx = rootbasis.col(0);
  Vector3d rootz = rootbasis.col(2);
  Vector3d up = rootx * cos(m_poleangle) + rootz * sin(m_poleangle);

  // in post, don't rotate towards the goal but only correct the pole up
  Vector3d poledir = (m_getpoleangle) ? dir : normalize(m_goal - rootpos);
  Vector3d poleup = normalize(m_polegoal - rootpos);

  Matrix3d mat, polemat;

  mat.row(0) = normalize(dir.cross(up));
  mat.row(1) = mat.row(0).cross(dir);
  mat.row(2) = -dir;

  polemat.row(0) = normalize(poledir.cross(poleup));
  polemat.row(1) = polemat.row(0).cross(poledir);
  polemat.row(2) = -poledir;

  if (m_getpoleangle) {
    // we compute the pole angle that to rotate towards the target
    m_poleangle = angle(mat.row(1), polemat.row(1));

    double dt = rootz.dot(mat.row(1) * cos(m_poleangle) + mat.row(0) * sin(m_poleangle));
    if (dt > 0.0)
      m_poleangle = -m_poleangle;

    // solve again, with the pole angle we just computed
    m_getpoleangle = false;
    ConstrainPoleVector(root, tasks);
  }
  else {
    // now we set as root matrix the difference between the current and
    // desired rotation based on the pole vector constraint. we use
    // transpose instead of inverse because we have orthogonal matrices
    // anyway, and in case of a singular matrix we don't get NaN's.
    Affine3d trans;
    trans.linear() = polemat.transpose() * mat;
    trans.translation() = Vector3d(0, 0, 0);
    m_rootmatrix = trans * m_rootmatrix;
  }
}

bool IK_QJacobianSolver::UpdateAngles(double &norm)
{
  // assing each segment a unique id for the jacobian
  std::vector<IK_QSegment *>::iterator seg;
  IK_QSegment *qseg, *minseg = NULL;
  double minabsdelta = 1e10, absdelta;
  Vector3d delta, mindelta;
  bool locked = false, clamp[3];
  int i, mindof = 0;

  // here we check if any angle limits were violated. angles whose clamped
  // position is the same as it was before, are locked immediate. of the
  // other violation angles the most violating angle is rememberd
  for (seg = m_segments.begin(); seg != m_segments.end(); seg++) {
    qseg = *seg;
    if (qseg->UpdateAngle(m_jacobian, delta, clamp)) {
      for (i = 0; i < qseg->NumberOfDoF(); i++) {
        if (clamp[i] && !qseg->Locked(i)) {
          absdelta = fabs(delta[i]);

          if (absdelta < IK_EPSILON) {
            qseg->Lock(i, m_jacobian, delta);
            locked = true;
          }
          else if (absdelta < minabsdelta) {
            minabsdelta = absdelta;
            mindelta = delta;
            minseg = qseg;
            mindof = i;
          }
        }
      }
    }
  }

  // lock most violating angle
  if (minseg) {
    minseg->Lock(mindof, m_jacobian, mindelta);
    locked = true;

    if (minabsdelta > norm)
      norm = minabsdelta;
  }

  if (locked == false)
    // no locking done, last inner iteration, apply the angles
    for (seg = m_segments.begin(); seg != m_segments.end(); seg++) {
      (*seg)->UnLock();
      (*seg)->UpdateAngleApply();
    }

  // signal if another inner iteration is needed
  return locked;
}

bool IK_QJacobianSolver::Solve(IK_QSegment *root,
                               std::list<IK_QTask *> tasks,
                               const double,
                               const int max_iterations)
{
  float scale = ComputeScale();
  bool solved = false;
  // double dt = analyze_time();

  Scale(scale, tasks);

  ConstrainPoleVector(root, tasks);

  root->UpdateTransform(m_rootmatrix);

  // iterate
  for (int iterations = 0; iterations < max_iterations; iterations++) {
    // update transform
    root->UpdateTransform(m_rootmatrix);

    std::list<IK_QTask *>::iterator task;

    // compute jacobian
    for (task = tasks.begin(); task != tasks.end(); task++) {
      if ((*task)->Primary())
        (*task)->ComputeJacobian(m_jacobian);
      else
        (*task)->ComputeJacobian(m_jacobian_sub);
    }

    double norm = 0.0;

    do {
      // invert jacobian
      try {
        m_jacobian.Invert();
        if (m_secondary_enabled)
          m_jacobian.SubTask(m_jacobian_sub);
      }
      catch (...) {
        fprintf(stderr, "IK Exception\n");
        return false;
      }

      // update angles and check limits
    } while (UpdateAngles(norm));

    // unlock segments again after locking in clamping loop
    std::vector<IK_QSegment *>::iterator seg;
    for (seg = m_segments.begin(); seg != m_segments.end(); seg++)
      (*seg)->UnLock();

    // compute angle update norm
    double maxnorm = m_jacobian.AngleUpdateNorm();
    if (maxnorm > norm)
      norm = maxnorm;

    // check for convergence
    if (norm < 1e-3 && iterations > 10) {
      solved = true;
      break;
    }
  }

  if (m_poleconstraint)
    root->PrependBasis(m_rootmatrix.linear());

  Scale(1.0f / scale, tasks);

  // analyze_add_run(max_iterations, analyze_time()-dt);

  return solved;
}
