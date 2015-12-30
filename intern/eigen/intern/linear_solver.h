/*
 *  Sparse linear solver.
 *  Copyright (C) 2004 Bruno Levy
 *  Copyright (C) 2005-2015 Blender Foundation
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  If you modify this software, you should include a notice giving the
 *  name of the person performing the modification, the date of modification,
 *  and the reason for such modification.
 */

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Solvers for Ax = b and AtAx = Atb */

typedef struct LinearSolver LinearSolver;

LinearSolver *EIG_linear_solver_new(
	int num_rows,
	int num_columns,
	int num_right_hand_sides);

LinearSolver *EIG_linear_least_squares_solver_new(
	int num_rows,
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

