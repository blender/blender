/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup intern_iksolver
 */

#include <stdio.h>

#include "IK_QJacobianSolver.h"

// #include "analyze.h"
IK_QJacobianSolver::IK_QJacobianSolver() {}

IK_QJacobianSolver::~IK_QJacobianSolver()
{

  std::vector<IK_QPoleVectorConstraint *>::iterator con;
  for (con = m_pole_constraints_oneway.begin(); con != m_pole_constraints_oneway.end(); con++)
    delete (*con);

  for (con = m_pole_constraints_twoway.begin(); con != m_pole_constraints_twoway.end(); con++)
    delete (*con);

  std::vector<Affine3d *>::iterator matrix;
  for (matrix = m_root_matrices.begin(); matrix != m_root_matrices.end(); matrix++)
    delete (*matrix);
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

  std::vector<Affine3d *>::iterator matrix;
  for (matrix = m_root_matrices.begin(); matrix != m_root_matrices.end(); matrix++)
    (*matrix)->translation() *= scale;

  std::vector<IK_QPoleVectorConstraint *>::iterator con;
  for (con = m_pole_constraints_oneway.begin(); con != m_pole_constraints_oneway.end(); con++) {
    (*con)->m_pole_pos *= scale;
    (*con)->m_chain_goal_pos *= scale;
    // segments are already scaled above
  }
  for (con = m_pole_constraints_twoway.begin(); con != m_pole_constraints_twoway.end(); con++) {
    (*con)->m_pole_pos *= scale;
    (*con)->m_chain_goal_pos *= scale;
    // segments are already scaled above
  }
}

void IK_QJacobianSolver::AddSegmentList(IK_QSegment *seg)
{
  m_segments.push_back(seg);

  IK_QSegment *child;
  for (child = seg->Child(); child; child = child->Sibling())
    AddSegmentList(child);
}

bool IK_QJacobianSolver::Setup(IK_QSegment **roots,
                               const int root_count,
                               std::list<IK_QTask *> &tasks)
{
  m_segments.clear();
  for (int r = 0; r < root_count; r++)
    AddSegmentList(roots[r]);

  for (int r = 0; r < root_count; r++) {
    Affine3d *root_matrix = new Affine3d();
    root_matrix->setIdentity();
    m_root_matrices.push_back(root_matrix);
  }

  // assign each segment a unique id for the jacobian
  std::vector<IK_QSegment *>::iterator seg;
  int num_dof = 0;

  for (seg = m_segments.begin(); seg != m_segments.end(); seg++) {
    (*seg)->SetDoFId(num_dof);
    num_dof += (*seg)->NumberOfDoF();
  }

  if (num_dof == 0)
    return false;

  // compute task ids and assign weights to task
  int primary_size = 0;
  int secondary_size = 0, secondary = 0;
  double primary_weight = 0.0, secondary_weight = 0.0;
  std::list<IK_QTask *>::iterator task;

  for (task = tasks.begin(); task != tasks.end(); task++) {
    IK_QTask *qtask = *task;

    if (qtask->Primary()) {
      qtask->SetId(primary_size);
      primary_size += qtask->Size();
      primary_weight += qtask->Weight();
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

void IK_QJacobianSolver::AddPoleVectorConstraint(int root_index,
                                                 IK_QSegment *tip,
                                                 Vector3d &goal,
                                                 Vector3d &polegoal,
                                                 float poleangle,
                                                 IK_QSegment *m_goal_segment)
{
  IK_QPoleVectorConstraint *con = new IK_QPoleVectorConstraint();
  con->m_chain_root_index = root_index;
  con->m_chain_tip_seg = tip;
  con->m_chain_goal_pos = goal;
  con->m_pole_pos = polegoal;
  con->m_pole_angle = poleangle;
  con->m_chain_goal_seg = m_goal_segment;

  if (m_goal_segment == nullptr) {
    m_pole_constraints_oneway.push_back(con);
  }
  else {
    m_pole_constraints_twoway.push_back(con);
  }
}

void IK_QJacobianSolver::ConstrainPoleVector_OneWays(IK_QSegment **roots, int root_count)
{
  // Assumes one constraint per root. Otherwise, undefined behavior. NOTE: maybe as simple as
  // evaluating this per ik solve iter.

  /** GG: TODO: figure out proper math to support ik pole on 2wayIK's target
   * chain. It will constrain target's tip to target's root. */

  // this function will be called before and after solving. calling it before
  // solving gives predictable solutions by rotating towards the solution,
  // and calling it afterwards ensures the solution is exact.

  /* In case of multiple constraints on the same root, maybe we should constrain
   * the next child segment?*/
  /** We can't twist the posetree while keeping 3 non-colinear points unchanged.
   * GG: CONSIDER: For a single 2way posetree w/o goalRot, we can either twist either chains (2
   * poles) through their tips or twist through both chain roots.*/

  // get positions and rotations

  std::vector<IK_QPoleVectorConstraint *>::iterator iter;
  for (iter = m_pole_constraints_oneway.begin(); iter != m_pole_constraints_oneway.end(); iter++) {
    IK_QPoleVectorConstraint *con = *iter;

    const int root_index = con->m_chain_root_index;
    if (root_index < 0 || root_index >= root_count) {
      assert(false);
      continue;
    }
    if (root_index >= m_root_matrices.size()) {
      assert(false);
      continue;
    }

    IK_QSegment *root = roots[root_index];
    Affine3d *root_pre_matrix_ptr = m_root_matrices[root_index];
    root->UpdateTransform_Root(*root_pre_matrix_ptr);

    const Vector3d rootpos = root->GlobalStart();
    const Vector3d endpos = con->m_chain_tip_seg->GlobalEnd();
    const Matrix3d &rootbasis = root->Composite()->GlobalTransform().linear();

    float pole_angle = con->m_pole_angle;
    // construct "lookat" matrices (like gluLookAt), based on a direction and
    // an up vector, with the direction going from the root to the end effector
    // and the up vector going from the root to the pole constraint position.
    Vector3d dir = normalize(endpos - rootpos);
    Vector3d rootx = rootbasis.col(0);
    Vector3d rootz = rootbasis.col(2);
    Vector3d up = rootx * cos(pole_angle) + rootz * sin(pole_angle);

    Vector3d goal_pos = con->m_chain_goal_pos;
    const Vector3d pole_pos = con->m_pole_pos;
    // in post, don't rotate towards the goal but only correct the pole up
    Vector3d poledir = normalize(goal_pos - rootpos);
    Vector3d poleup = normalize(pole_pos - rootpos);

    Matrix3d mat, polemat;

    mat.row(0) = normalize(dir.cross(up));
    mat.row(1) = mat.row(0).cross(dir);
    mat.row(2) = -dir;

    polemat.row(0) = normalize(poledir.cross(poleup));
    polemat.row(1) = polemat.row(0).cross(poledir);
    polemat.row(2) = -poledir;

    // now we set as root matrix the difference between the current and
    // desired rotation based on the pole vector constraint. we use
    // transpose instead of inverse because we have orthogonal matrices
    // anyway, and in case of a singular matrix we don't get NaN's.
    Affine3d trans;
    trans.linear() = polemat.transpose() * mat;
    trans.translation() = Vector3d(0, 0, 0);
    (*root_pre_matrix_ptr) = trans * (*root_pre_matrix_ptr);
  }
}
void IK_QJacobianSolver::ConstrainPoleVector_TwoWays(IK_QSegment **roots, int root_count)
{
  // Assumes one constraint per root. Otherwise, undefined behavior. NOTE: maybe as simple as
  // evaluating this per ik solve iter. This algo differs from .._OneWays() in that this
  // only twists the root, it does not align the axis of (end effector to root) with the axis of
  // (goal to root).

  /**
   *
   * GG: TODO: no need for m_goal_segment ref? not used.
   */
  std::vector<IK_QPoleVectorConstraint *>::iterator iter;
  for (iter = m_pole_constraints_twoway.begin(); iter != m_pole_constraints_twoway.end(); iter++) {
    IK_QPoleVectorConstraint *con = *iter;

    const int root_index = con->m_chain_root_index;
    if (root_index < 0 || root_index >= root_count) {
      assert(false);
      continue;
    }
    if (root_index >= m_root_matrices.size()) {
      assert(false);
      continue;
    }

    IK_QSegment *root = roots[root_index];
    Affine3d *root_pre_matrix_ptr = m_root_matrices[root_index];
    root->UpdateTransform_Root(*root_pre_matrix_ptr);

    const Vector3d rootpos = root->GlobalStart();
    const Vector3d endpos = con->m_chain_tip_seg->GlobalEnd();
    const Matrix3d &rootbasis = root->Composite()->GlobalTransform().linear();

    float pole_angle = con->m_pole_angle;
    // construct "lookat" matrices (like gluLookAt), based on a direction and
    // an up vector, with the direction going from the root to the end effector
    // and the up vector going from the root to the pole constraint position.
    Vector3d dir = normalize(endpos - rootpos);
    Vector3d rootx = rootbasis.col(0);
    Vector3d rootz = rootbasis.col(2);
    Vector3d up = rootx * cos(pole_angle) + rootz * sin(pole_angle);

    // Vector3d goal_pos;
    // {
    //   IK_QSegment *chain_goal_root_seg;
    //   for (chain_goal_root_seg = con->m_chain_goal_seg;
    //        chain_goal_root_seg && chain_goal_root_seg->Parent();
    //        chain_goal_root_seg = chain_goal_root_seg->Parent())
    //   {
    //   }

    //   int root_index_in_solver = -1;
    //   for (int r = 0; r < root_count; r++) {
    //     if (chain_goal_root_seg != roots[r]) {
    //       continue;
    //     }
    //     root_index_in_solver = r;
    //     break;
    //   }
    //   assert(root_index_in_solver != -1);
    //   Affine3d *goal_pre_matrix_ptr = m_root_matrices[root_index_in_solver];
    //   chain_goal_root_seg->UpdateTransform_Root(*goal_pre_matrix_ptr);

    //   goal_pos = con->m_chain_goal_seg->GlobalStart();
    // }

    // // in post, don't rotate towards the goal but only correct the pole up
    // Vector3d poledir = normalize(goal_pos - rootpos);
    const Vector3d pole_pos = con->m_pole_pos;
    Vector3d poleup = normalize(pole_pos - rootpos);

    Matrix3d mat, polemat;

    mat.row(0) = normalize(dir.cross(up));
    mat.row(1) = mat.row(0).cross(dir);
    mat.row(2) = -dir;

    polemat.row(0) = normalize(dir.cross(poleup));
    polemat.row(1) = polemat.row(0).cross(dir);
    polemat.row(2) = -dir;

    // now we set as root matrix the difference between the current and
    // desired rotation based on the pole vector constraint. we use
    // transpose instead of inverse because we have orthogonal matrices
    // anyway, and in case of a singular matrix we don't get NaN's.
    Affine3d trans;
    trans.linear() = polemat.transpose() * mat;
    trans.translation() = Vector3d(0, 0, 0);
    (*root_pre_matrix_ptr) = trans * (*root_pre_matrix_ptr);
  }
}

bool IK_QJacobianSolver::UpdateAngles(double &norm)
{
  // assign each segment a unique id for the jacobian
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

bool IK_QJacobianSolver::Solve(IK_QSegment **roots,
                               const int root_count,
                               std::list<IK_QTask *> tasks,
                               const double,
                               const int max_iterations)
{
  float scale = ComputeScale();
  // scale = 1;
  bool solved = false;
  // double dt = analyze_time();

  Scale(scale, tasks);

  ConstrainPoleVector_OneWays(roots, root_count);
  ConstrainPoleVector_TwoWays(roots, root_count);

  for (int r = 0; r < root_count; r++) {
    int max_recursion_depth = roots[r]->UpdateTransform_Root(*m_root_matrices[r]);
    // printf("deepeest recursion_depth %d\n", max_recursion_depth);
    if (max_recursion_depth > 50) {
      printf("!!!!!deepest recursion_depth %d\n", max_recursion_depth);
    }
  }

  // iterate
  for (int iterations = 0; iterations < max_iterations; iterations++) {
    // update transform

    /* Poles on twoway chains need to be frequently evaluated to ensure the constrained root's pole
     * plane always includes the pole. One ways don't have this additional computation just to
     * match vanilla blender behavior, which works fine. */
    ConstrainPoleVector_TwoWays(roots, root_count);
    for (int r = 0; r < root_count; r++) {
      int max_recursion_depth = roots[r]->UpdateTransform_Root(*m_root_matrices[r]);
      // printf("deepeest recursion_depth %d\n", max_recursion_depth);
      if (max_recursion_depth > 50) {
        printf("!!!!!deepest recursion_depth %d\n", max_recursion_depth);
      }
    }

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

  for (int r = 0; r < root_count; r++)
    roots[r]->PrependBasis(m_root_matrices[r]->linear());

  Scale(1.0f / scale, tasks);

  // analyze_add_run(max_iterations, analyze_time()-dt);

  return solved;
}
