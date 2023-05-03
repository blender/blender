/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup intern_iksolver
 */

#include "../extern/IK_solver.h"

#include "IK_QJacobianSolver.h"
#include "IK_QSegment.h"
#include "IK_QTask.h"

#include <list>
using namespace std;

class IK_QSolver {
 public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW
  IK_QSolver() {}

  IK_QJacobianSolver solver;
  IK_QSegment **roots;
  int root_count;
  std::list<IK_QTask *> tasks;
};

// FIXME: locks still result in small "residual" changes to the locked axes...
static IK_QSegment *CreateSegment(int flag, bool translate, bool is_stretch)
{
  int ndof = 0;
  ndof += (flag & IK_XDOF) ? 1 : 0;
  ndof += (flag & IK_YDOF) ? 1 : 0;
  ndof += (flag & IK_ZDOF) ? 1 : 0;

  IK_QSegment *seg;

  /* A segment is always created to simplify reading ik solver output. Specific
   * segments can be initialized such that their basis change is useful.
   * IK_QNullSegments (i.e. zero DOF, completely locked segments) allow us to
   * apply restpose space's transform resets to the basis without having the IK
   * solver modify or even consider the segment for calculations, so null
   * segments do not negatively affect solver stability. */
  if (ndof == 0)
    return new IK_QNullSegment(translate, is_stretch);

  else if (ndof == 1) {
    int axis;

    if (flag & IK_XDOF)
      axis = 0;
    else if (flag & IK_YDOF)
      axis = 1;
    else
      axis = 2;

    if (translate)
      seg = new IK_QTranslateSegment(axis, is_stretch);
    else
      seg = new IK_QRevoluteSegment(axis);
  }
  else if (ndof == 2) {
    int axis1, axis2;

    if (flag & IK_XDOF) {
      axis1 = 0;
      axis2 = (flag & IK_YDOF) ? 1 : 2;
    }
    else {
      axis1 = 1;
      axis2 = 2;
    }

    if (translate)
      seg = new IK_QTranslateSegment(axis1, axis2);
    else {
      if (axis1 + axis2 == 2)
        seg = new IK_QSwingSegment();
      else
        seg = new IK_QElbowSegment((axis1 == 0) ? 0 : 2);
    }
  }
  else {
    if (translate)
      seg = new IK_QTranslateSegment();
    else
      seg = new IK_QSphericalSegment();
  }

  return seg;
}

IK_Segment *IK_CreateSegment(int flag, char *cname)
{
  IK_QSegment *rot = CreateSegment(flag, false, false);
  IK_QSegment *trans = CreateSegment(flag >> 3, true, false);
  IK_QSegment *stretch = CreateSegment(flag >> 6, true, true);

  trans->SetComposite(rot);
  rot->SetParent(trans);

  rot->SetComposite(stretch);
  stretch->SetParent(rot);

  trans->m_cname = rot->m_cname = stretch->m_cname = cname;

  return trans;
}

void IK_FreeSegment(IK_Segment *seg)
{
  IK_QSegment *qseg = (IK_QSegment *)seg;

  IK_QSegment *composite = qseg->Composite();
  if (composite) {
    IK_QSegment *composite1 = composite->Composite();
    if (composite1) {
      delete composite1;
    }
    delete composite;
  }
  delete qseg;
}

void IK_SetParent(IK_Segment *seg, IK_Segment *parent)
{
  IK_QSegment *qseg = (IK_QSegment *)seg;
  IK_QSegment *qparent_tip = (IK_QSegment *)parent;
  if (qparent_tip) {
    qparent_tip = qparent_tip->Composite_Tip();
  }
  qseg->SetParent(qparent_tip);
}

void IK_SetTransform_TranslationSegment(
    IK_Segment *seg, float start[3], float rest[][3], float initial_location[3], float location[3])
{
  /* Assumes caller knows that seg has translation, which means its also the composite's root. */
  IK_QSegment *qseg = (IK_QSegment *)seg;
  assert(qseg->Translational() && !qseg->Stretchable());

  Vector3d mstart(start[0], start[1], start[2]);
  Vector3d minitial_location(initial_location[0], initial_location[1], initial_location[2]);
  Vector3d mlocation(location[0], location[1], location[2]);

  Matrix3d mrest = CreateMatrix(rest[0][0],
                                rest[1][0],
                                rest[2][0],
                                rest[0][1],
                                rest[1][1],
                                rest[2][1],
                                rest[0][2],
                                rest[1][2],
                                rest[2][2]);

  qseg->SetTransform_Translation(mstart, mrest, minitial_location, mlocation);
}

void IK_SetTransform_RotationSegment(IK_Segment *seg,
                                     float rest[][3],
                                     float initial_basis[][3],
                                     float basis[][3])
{
  /* Assumes caller knows that seg has rotation. */
  IK_QSegment *qseg = (IK_QSegment *)seg;
  if (qseg->Translational()) {
    qseg = qseg->Composite();
  }
  assert(!qseg->Translational());

  Matrix3d mbasis = CreateMatrix(basis[0][0],
                                 basis[1][0],
                                 basis[2][0],
                                 basis[0][1],
                                 basis[1][1],
                                 basis[2][1],
                                 basis[0][2],
                                 basis[1][2],
                                 basis[2][2]);
  Matrix3d mrest = CreateMatrix(rest[0][0],
                                rest[1][0],
                                rest[2][0],
                                rest[0][1],
                                rest[1][1],
                                rest[2][1],
                                rest[0][2],
                                rest[1][2],
                                rest[2][2]);

  Matrix3d minitial_basis = CreateMatrix(initial_basis[0][0],
                                         initial_basis[1][0],
                                         initial_basis[2][0],
                                         initial_basis[0][1],
                                         initial_basis[1][1],
                                         initial_basis[2][1],
                                         initial_basis[0][2],
                                         initial_basis[1][2],
                                         initial_basis[2][2]);

  /** GG: CLEANUP: start and length not needed. can be removed.*/
  Vector3d mstart(0, 0, 0);
  qseg->SetTransform(mstart, mrest, minitial_basis, mbasis, 0.0f);
}

void IK_SetTransform_ExtensionSegment(IK_Segment *seg, float initial_length, float length)
{
  /* Assumes caller knows that seg has extension. */
  IK_QSegment *qseg = ((IK_QSegment *)seg)->Composite_Tip();
  assert(qseg->Translational() && qseg->Stretchable());

  Vector3d mstart(0, 0, 0);
  Vector3d minitial_location(0, initial_length, 0);
  Vector3d mlocation(0, length, 0);

  /** GG: NOTE: CONSIDER: The below puts m_trnslation=zero at pchan tail. Thus bones never scale to
   * zero since the stretch limit is zero. This is why we get different stretch behaviors w/
   * vanilla belnder.
   */
  // qseg->SetTransform_Translation(minitial_location, mstart, mstart);
  /** GG: XXX: CONSIDER: below puts m_translation=zero at pchan head, where zero is stretch min.
   * THis means the below allows bonees to scale to zero.
   *
   * Which one should be used? Or maybe just make extension/stretch limits explicit in UI? so user
   * chooses? Personally, I prefer the above since bones never scale to zero... though the behavior
   * of the above also doesn't necessariyl distribute scale evenly across bones (netiher does the
   * below)... I dunno. it's probably better to not change existing behavior if there's isn't an
   * obvious use or improvement....though the above appears more stable/reliable at larger
   * IKStretch values....
   */

  Matrix3d m3_identity;
  m3_identity.setIdentity();

  qseg->SetTransform_Translation(mstart, m3_identity, minitial_location, mlocation);
}

void IK_SetLimit(IK_Segment *seg, IK_SegmentAxis axis, float lmin, float lmax)
{
  IK_QSegment *qseg = (IK_QSegment *)seg;
  if (axis >= IK_X && axis <= IK_Z) {
    /* Rotational segment always the composite after the translational non-stretch segment. */
    if (qseg->Translational() && qseg->Composite()) {
      qseg = qseg->Composite();
    }
    if (qseg->Translational()) {
      return;
    }
  }
  else if (axis >= IK_TRANS_X && axis <= IK_TRANS_Z) {
    /* Translational segment always composite's root if it exists. */
    if (!qseg->Translational()) {
      return;
    }
    if (axis == IK_TRANS_X)
      axis = IK_X;
    else if (axis == IK_TRANS_Y)
      axis = IK_Y;
    else
      axis = IK_Z;
  }
  else if (axis == IK_EXTENSION_Y) {
    /* Extension segment always composite's tip if it exists. */
    qseg = qseg->Composite_Tip();
    if (!qseg->Stretchable()) {
      return;
    }
    axis = IK_Y;
  }

  qseg->SetLimit(axis, lmin, lmax);
}

void IK_SetStiffness(IK_Segment *seg, IK_SegmentAxis axis, float stiffness)
{
  if (stiffness < 0.0f)
    return;

  /** For trnaslation, maybe rescale weight to 100?*/
  const float epsilon = IK_STRETCH_STIFF_EPS;
  if (stiffness > (1.0 - epsilon))
    stiffness = (1.0 - epsilon);

  IK_QSegment *qseg = (IK_QSegment *)seg;
  double weight = 1.0f - stiffness;

  if (axis >= IK_X && axis <= IK_Z) {
    /* Rotational segment always the composite after the translational non-stretch segment. */
    if (qseg->Translational() && qseg->Composite()) {
      qseg = qseg->Composite();
    }
    if (qseg->Translational()) {
      return;
    }
  }
  else if (axis >= IK_TRANS_X && axis <= IK_TRANS_Z) {
    /* Translational segment always composite's root if it exists. */
    if (!qseg->Translational()) {
      return;
    }
    if (axis == IK_TRANS_X)
      axis = IK_X;
    else if (axis == IK_TRANS_Y)
      axis = IK_Y;
    else
      axis = IK_Z;
  }
  else if (axis == IK_EXTENSION_Y) {
    /* Extension segment always composite's tip if it exists. */
    qseg = qseg->Composite_Tip();
    if (!qseg->Stretchable()) {
      return;
    }
    axis = IK_Y;
  }

  qseg->SetWeight(axis, weight);
}

void IK_GetBasisChange(IK_Segment *seg, float basis_change[][3])
{
  IK_QSegment *qseg = (IK_QSegment *)seg;

  /* Basis constant for non-rotational segments, so we don't really have to ensure qseg is
   * rotational if it doesn't exist. If it does, it's qseg or qseg->Composite(). */
  if (qseg->Translational() && qseg->Composite())
    qseg = qseg->Composite();

  const Matrix3d &change = qseg->BasisChange();

  // convert to blender column major
  basis_change[0][0] = (float)change(0, 0);
  basis_change[1][0] = (float)change(0, 1);
  basis_change[2][0] = (float)change(0, 2);
  basis_change[0][1] = (float)change(1, 0);
  basis_change[1][1] = (float)change(1, 1);
  basis_change[2][1] = (float)change(1, 2);
  basis_change[0][2] = (float)change(2, 0);
  basis_change[1][2] = (float)change(2, 1);
  basis_change[2][2] = (float)change(2, 2);
}

void IK_GetTranslationChange(IK_Segment *seg, float *translation_change)
{
  IK_QSegment *qseg = (IK_QSegment *)seg;

  /* Rotation and Stretch segments return zero. Translational segments are always the composite's
   * root segment if it exists so we don't search the composites for it. */
  if (qseg->Stretchable() || !qseg->Translational()) {
    translation_change[0] = 0;
    translation_change[1] = 0;
    translation_change[2] = 0;
    return;
  }

  const Vector3d &change = qseg->TranslationChange();

  translation_change[0] = (float)change[0];
  translation_change[1] = (float)change[1];
  translation_change[2] = (float)change[2];
}

void IK_GetStretchChange(IK_Segment *seg, float *stretch_change)
{
  /* Stretchable segment always tip. */
  IK_QSegment *qseg = ((IK_QSegment *)seg)->Composite_Tip();

  if (!qseg->Stretchable()) {
    stretch_change[0] = 0;
    stretch_change[1] = 0;
    stretch_change[2] = 0;
  }

  const Vector3d &change = qseg->TranslationChange();

  stretch_change[0] = (float)change[0];
  stretch_change[1] = (float)change[1];
  stretch_change[2] = (float)change[2];
}

IK_Solver *IK_CreateSolver(IK_Segment **roots, int root_count)
{
  if (roots == NULL)
    return NULL;

  IK_QSolver *solver = new IK_QSolver();
  solver->roots = (IK_QSegment **)roots;
  solver->root_count = root_count;

  return (IK_Solver *)solver;
}
static void IK_DEBUG_print_matrices_segment_matrix(IK_QSegment *cur_seg)
{
  Vector3d start = cur_seg->GlobalStart();
  Vector3d end = cur_seg->GlobalEnd();

  Vector3d x_axis(1, 0, 0), y_axis(0, 1, 0), z_axis(0, 0, 1);
  Matrix3d rot = cur_seg->GlobalTransform().linear();
  x_axis = rot * x_axis;
  y_axis = rot * y_axis;
  z_axis = rot * z_axis;
  printf_s("\t\t\tstart: (%.3f, %.3f, %.3f)\n", start[0], start[1], start[2]);
  printf_s("\t\t\tend: (%.3f, %.3f, %.3f)\n", end[0], end[1], end[2]);
  printf_s("\t\t\tx_axis: (%.3f, %.3f, %.3f)\n", x_axis[0], x_axis[1], x_axis[2]);
  printf_s("\t\t\ty_axis: (%.3f, %.3f, %.3f)\n", y_axis[0], y_axis[1], y_axis[2]);
  printf_s("\t\t\tz_axis: (%.3f, %.3f, %.3f)\n", z_axis[0], z_axis[1], z_axis[2]);
}

static void IK_DEBUG_print_matrices_recurse(IK_QSegment *cur_seg)
{
  /* Hardcoded, known composition. */
  IK_QSegment *head_seg = cur_seg;
  IK_QSegment *rot_seg = head_seg->Composite();
  IK_QSegment *tail_seg = rot_seg->Composite();

  if (head_seg->m_cname != nullptr) {
    printf_s("\tname: %s\n", head_seg->m_cname);
  }
  else {
    printf_s("\tname: NULL\n");
  }

  Vector3d head = head_seg->GlobalStart();
  Vector3d tail = tail_seg->GlobalEnd();

  Vector3d x_axis(1, 0, 0), y_axis(0, 1, 0), z_axis(0, 0, 1);
  Matrix3d rot = rot_seg->GlobalTransform().linear();
  x_axis = rot * x_axis;
  y_axis = rot * y_axis;
  z_axis = rot * z_axis;
  printf_s("\t\thead: (%.3f, %.3f, %.3f)\n", head[0], head[1], head[2]);
  printf_s("\t\ttail: (%.3f, %.3f, %.3f)\n", tail[0], tail[1], tail[2]);
  printf_s("\t\tx_axis: (%.3f, %.3f, %.3f)\n", x_axis[0], x_axis[1], x_axis[2]);
  printf_s("\t\ty_axis: (%.3f, %.3f, %.3f)\n", y_axis[0], y_axis[1], y_axis[2]);
  printf_s("\t\tz_axis: (%.3f, %.3f, %.3f)\n", z_axis[0], z_axis[1], z_axis[2]);
  printf_s("\n");
  printf_s("\t\ttranslation seg\n");
  IK_DEBUG_print_matrices_segment_matrix(head_seg);
  printf_s("\t\trotation seg\n");
  IK_DEBUG_print_matrices_segment_matrix(rot_seg);
  printf_s("\t\textension seg\n");
  IK_DEBUG_print_matrices_segment_matrix(tail_seg);

  // update child transforms
  for (IK_QSegment *child_seg = cur_seg->Composite_Tip()->Child(); child_seg;
       child_seg = child_seg->Sibling())
  {
    IK_DEBUG_print_matrices_recurse(child_seg);
  }
}

void IK_DEBUG_print_matrices(IK_Segment **_roots,
                             const int root_count,
                             float _prepend_rot[][3],
                             float _prepend_origin[3])
{
  IK_QSegment **roots = (IK_QSegment **)_roots;

  Affine3d solver_mat;
  solver_mat.setIdentity();

  Vector3d origin(_prepend_origin[0], _prepend_origin[1], _prepend_origin[2]);

  // convert from blender column major
  Matrix3d rot = CreateMatrix(_prepend_rot[0][0],
                              _prepend_rot[1][0],
                              _prepend_rot[2][0],
                              _prepend_rot[0][1],
                              _prepend_rot[1][1],
                              _prepend_rot[2][1],
                              _prepend_rot[0][2],
                              _prepend_rot[1][2],
                              _prepend_rot[2][2]);
  solver_mat.translation() = origin;
  solver_mat.linear() = rot;

  for (int i = 0; i < root_count; i++) {
    IK_QSegment *root_seg = roots[i];
    if (root_seg == nullptr) {
      continue;
    }
    root_seg->UpdateTransform_Root(solver_mat);
  }

  for (int i = 0; i < root_count; i++) {
    printf_s("-----------root[%d]-------------\n", i);
    IK_QSegment *root_seg = roots[i];
    if (root_seg == nullptr) {
      printf_s("\t<!><!><!>root is NULL <!><!><!>\n");
      continue;
    }

    IK_DEBUG_print_matrices_recurse(root_seg);
  }
}

void IK_FreeSolver(IK_Solver *solver)
{
  if (solver == NULL)
    return;

  IK_QSolver *qsolver = (IK_QSolver *)solver;
  std::list<IK_QTask *> &tasks = qsolver->tasks;
  std::list<IK_QTask *>::iterator task;

  for (task = tasks.begin(); task != tasks.end(); task++)
    delete (*task);

  delete qsolver;
}

void IK_SolverAddGoal(IK_Solver *solver,
                      IK_Segment *tip,
                      float goal[3],
                      float weight,
                      const bool use_tip_composite_tip,
                      IK_Segment *goalseg,
                      const bool use_goal_composite_tip,
                      IK_Segment *zero_weight_sentinel_seg)
{
  if (solver == NULL || tip == NULL)
    return;

  IK_QSolver *qsolver = (IK_QSolver *)solver;
  IK_QSegment *qtip = (IK_QSegment *)tip;

  if (use_tip_composite_tip && qtip->Composite())
    qtip = qtip->Composite_Tip();

  Vector3d pos(goal[0], goal[1], goal[2]);

  IK_QSegment *qgoal = (IK_QSegment *)goalseg;
  if (qgoal && use_goal_composite_tip && qgoal->Composite())
    qgoal = qgoal->Composite_Tip();

  IK_QSegment *qzero_weight_sentinel_seg = (IK_QSegment *)zero_weight_sentinel_seg;
  if (qzero_weight_sentinel_seg && qzero_weight_sentinel_seg->Composite()) {
    qzero_weight_sentinel_seg = qzero_weight_sentinel_seg->Composite_Tip();
  }
  IK_QTask *ee = new IK_QPositionTask(true, qtip, pos, qgoal, qzero_weight_sentinel_seg);
  ee->SetWeight(weight);
  qsolver->tasks.push_back(ee);
}

void IK_SolverAddGoalOrientation(IK_Solver *solver,
                                 IK_Segment *tip,
                                 float goal[][3],
                                 float weight,
                                 IK_Segment *goalseg,
                                 IK_Segment *zero_weight_sentinel_seg)
{
  if (solver == NULL || tip == NULL)
    return;

  IK_QSolver *qsolver = (IK_QSolver *)solver;
  IK_QSegment *qtip = ((IK_QSegment *)tip)->Composite_Tip();

  // convert from blender column major
  Matrix3d rot = CreateMatrix(goal[0][0],
                              goal[1][0],
                              goal[2][0],
                              goal[0][1],
                              goal[1][1],
                              goal[2][1],
                              goal[0][2],
                              goal[1][2],
                              goal[2][2]);

  IK_QSegment *qgoalseg = (IK_QSegment *)goalseg;
  if (qgoalseg != NULL) {
    qgoalseg = qgoalseg->Composite();
    assert(!qgoalseg->Translational());
  }

  IK_QSegment *qzero_weight_sentinel_seg = (IK_QSegment *)zero_weight_sentinel_seg;
  if (qzero_weight_sentinel_seg && qzero_weight_sentinel_seg->Composite()) {
    qzero_weight_sentinel_seg = qzero_weight_sentinel_seg->Composite_Tip();
  }
  IK_QTask *orient = new IK_QOrientationTask(true, qtip, rot, qgoalseg, qzero_weight_sentinel_seg);
  orient->SetWeight(weight);
  qsolver->tasks.push_back(orient);
}

void IK_SolverAddPoleVectorConstraint(IK_Solver *solver,
                                      int root_index,
                                      IK_Segment *tip,
                                      const bool use_tip_composite_tip,
                                      float goal[3],
                                      float polegoal[3],
                                      float poleangle,
                                      IK_Segment *goalseg,
                                      const bool use_goal_composite_tip)
{
  if (solver == NULL || tip == NULL)
    return;

  IK_QSolver *qsolver = (IK_QSolver *)solver;
  IK_QSegment *qtip = ((IK_QSegment *)tip);

  if (use_tip_composite_tip && qtip->Composite())
    qtip = qtip->Composite_Tip();

  Vector3d qgoal(goal[0], goal[1], goal[2]);
  Vector3d qpolegoal(polegoal[0], polegoal[1], polegoal[2]);

  IK_QSegment *q_goalseg = (IK_QSegment *)goalseg;
  if (q_goalseg && use_goal_composite_tip && q_goalseg->Composite())
    q_goalseg = q_goalseg->Composite_Tip();

  qsolver->solver.AddPoleVectorConstraint(
      root_index, qtip, qgoal, qpolegoal, poleangle, q_goalseg);
}

#if 0
static void IK_SolverAddCenterOfMass(IK_Solver *solver,
                                     IK_Segment *root,
                                     float goal[3],
                                     float weight)
{
  if (solver == NULL || root == NULL)
    return;

  IK_QSolver *qsolver = (IK_QSolver *)solver;
  IK_QSegment *qroot = (IK_QSegment *)root;

  // convert from blender column major
  Vector3d center(goal);

  IK_QTask *com = new IK_QCenterOfMassTask(true, qroot, center);
  com->SetWeight(weight);
  qsolver->tasks.push_back(com);
}
#endif

int IK_Solve(IK_Solver *solver, float tolerance, int max_iterations)
{
  if (solver == NULL)
    return 0;

  IK_QSolver *qsolver = (IK_QSolver *)solver;

  IK_QSegment **roots = qsolver->roots;
  const int root_count = qsolver->root_count;
  IK_QJacobianSolver &jacobian = qsolver->solver;
  std::list<IK_QTask *> &tasks = qsolver->tasks;
  double tol = tolerance;

  if (!jacobian.Setup(roots, root_count, tasks))
    return 0;

  bool result = jacobian.Solve(roots, root_count, tasks, tol, max_iterations);

  return ((result) ? 1 : 0);
}
