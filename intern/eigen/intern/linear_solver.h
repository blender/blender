/* SPDX-FileCopyrightText: 2004 Bruno Levy
 * SPDX-FileCopyrightText: 2005-2015 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_eigen
 * Sparse linear solver.
 */

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Solvers for Ax = b and AtAx = Atb */

typedef struct LinearSolver LinearSolver;

LinearSolver *EIG_linear_solver_new(int num_rows, int num_columns, int num_right_hand_sides);

LinearSolver *EIG_linear_least_squares_solver_new(int num_rows,
                                                  int num_columns,
                                                  int num_right_hand_sides);

void EIG_linear_solver_delete(LinearSolver *solver);

/* Variables (x). Any locking must be done before matrix construction. */

void EIG_linear_solver_variable_set(LinearSolver *solver, int rhs, int index, double value);
double EIG_linear_solver_variable_get(LinearSolver *solver, int rhs, int index);
void EIG_linear_solver_variable_lock(LinearSolver *solver, int index);
void EIG_linear_solver_variable_unlock(LinearSolver *solver, int index);

/* Matrix (A) and right hand side (b) */

void EIG_linear_solver_matrix_add(LinearSolver *solver, int row, int col, double value);
void EIG_linear_solver_right_hand_side_add(LinearSolver *solver, int rhs, int index, double value);

/* Solve. Repeated solves are supported, by changing b between solves. */

bool EIG_linear_solver_solve(LinearSolver *solver);

/* Debugging */

void EIG_linear_solver_print_matrix(LinearSolver *solver);

#ifdef __cplusplus
}
#endif
