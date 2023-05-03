/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup intern_iksolver
 */

/**
 * \page IK - Blender inverse kinematics module.
 *
 * \section about About the IK module
 *
 * This module allows you to create segments and form them into
 * tree. You can then define a goal points that the end of a given
 * segment should attempt to reach - an inverse kinematic problem.
 * This module will then modify the segments in the tree in order
 * to get the as near as possible to the goal. This solver uses an
 * inverse jacobian method to find a solution.
 *
 * \section issues Known issues with this IK solver.
 *
 * - There is currently no support for joint constraints in the
 * solver. This is within the realms of possibility - please ask
 * if this functionality is required.
 * - The solver is slow, inverse jacobian methods in general give
 * 'smooth' solutions and the method is also very flexible, it
 * does not rely on specific angle parameterization and can be
 * extended to deal with different joint types and joint
 * constraints. However it is not suitable for real time use.
 * Other algorithms exist which are more suitable for real-time
 * applications, please ask if this functionality is required.
 *
 * \section dependencies Dependencies
 *
 * This module only depends on Moto.
 */

#ifndef __IK_SOLVER_H__
#define __IK_SOLVER_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Typical order of calls for solving an IK problem:
 *
 * - create number of IK_Segment's and set their parents and transforms
 * - create an IK_Solver
 * - set a number of goals for the IK_Solver to solve
 * - call IK_Solve
 * - free the IK_Solver
 * - get basis and translation changes from segments
 * - free all segments
 */

/**
 * IK_Segment defines a single segment of an IK tree.
 * - Individual segments are always defined in local coordinates.
 * - The segment is assumed to be oriented in the local
 *   y-direction.
 * - start is the start of the segment relative to the end
 *   of the parent segment.
 * - rest_basis is a column major matrix defineding the rest
 *   position (w.r.t. which the limits are defined), must
 *   be a pure rotation
 * - basis is a column major matrix defining the current change
 *   from the rest basis, must be a pure rotation
 * - length is the length of the bone.
 *
 * - basis_change and translation_change respectively define
 *   the change in rotation or translation. basis_change is a
 *   column major 3x3 matrix.
 *
 * The local transformation is then defined as:
 * start * rest_basis * basis * basis_change * translation_change * translate(0,length,0)
 */

typedef void IK_Segment;

enum IK_SegmentFlag {
  IK_XDOF = 1,
  IK_YDOF = 2,
  IK_ZDOF = 4,
  IK_TRANS_XDOF = 8,
  IK_TRANS_YDOF = 16,
  IK_TRANS_ZDOF = 32,
  /* Solver does not support scale, so we call it extension instead, which is a location type of
     data. */
  IK_EXTENSION_YDOF = 128,
};

typedef enum IK_SegmentAxis {
  IK_X = 0,
  IK_Y = 1,
  IK_Z = 2,
  IK_TRANS_X = 3,
  IK_TRANS_Y = 4,
  IK_TRANS_Z = 5,
  IK_EXTENSION_Y = 7,
} IK_SegmentAxis;

extern IK_Segment *IK_CreateSegment(int flag, char *name);
extern void IK_FreeSegment(IK_Segment *seg);

extern void IK_SetParent(IK_Segment *seg, IK_Segment *parent);
extern void IK_SetTransform_TranslationSegment(IK_Segment *seg,
                                               float start[3],
                                               float rest[][3],
                                               float initial_location[3],
                                               float location[3]);
extern void IK_SetTransform_RotationSegment(IK_Segment *seg,
                                            float rest[][3],
                                            float initial_basis[][3],
                                            float basis[][3]);
extern void IK_SetTransform_ExtensionSegment(IK_Segment *seg, float initial_length, float length);
extern void IK_SetLimit(IK_Segment *seg, IK_SegmentAxis axis, float lmin, float lmax);
extern void IK_SetStiffness(IK_Segment *seg, IK_SegmentAxis axis, float stiffness);

extern void IK_GetBasisChange(IK_Segment *seg, float basis_change[][3]);
extern void IK_GetTranslationChange(IK_Segment *seg, float *translation_change);
extern void IK_GetStretchChange(IK_Segment *seg, float *stretch_change);

/**
 * An IK_Solver must be created to be able to execute the solver.
 *
 * An arbitray number of goals can be created, stating that a given
 * end effector must have a given position or rotation. If multiple
 * goals are specified, they can be weighted (range 0..1) to get
 * some control over their importance.
 *
 * IK_Solve will execute the solver, that will run until either the
 * system converges, or a maximum number of iterations is reached.
 * It returns 1 if the system converged, 0 otherwise.
 */

typedef void IK_Solver;

IK_Solver *IK_CreateSolver(IK_Segment **roots, int root_count);
void IK_DEBUG_print_matrices(IK_Segment **_roots,
                             const int root_count,
                             float prepend_rot[][3],
                             float preprend_origin[3]);
void IK_FreeSolver(IK_Solver *solver);

void IK_SolverAddGoal(IK_Solver *solver,
                      IK_Segment *tip,
                      float goal[3],
                      float weight,
                      bool use_tip_composite_tip,
                      IK_Segment *goalseg,
                      bool use_goal_composite_tip,
                      IK_Segment *zero_weight_sentinel_seg);
void IK_SolverAddGoalOrientation(IK_Solver *solver,
                                 IK_Segment *tip,
                                 float goal[][3],
                                 float weight,
                                 IK_Segment *goalseg,
                                 IK_Segment *zero_weight_sentinel_seg);
void IK_SolverAddPoleVectorConstraint(IK_Solver *solver,
                                      int root_index,
                                      IK_Segment *tip,
                                      const bool use_tip_composite_tip,
                                      float goal[3],
                                      float polegoal[3],
                                      float poleangle,
                                      IK_Segment *goalseg,
                                      const bool use_goal_composite_tip);

int IK_Solve(IK_Solver *solver, float tolerance, int max_iterations);

#define IK_STRETCH_STIFF_EPS 0.01f

#ifdef __cplusplus
}
#endif

#endif  // __IK_SOLVER_H__
