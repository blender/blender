/* SPDX-FileCopyrightText: 2004 Bruno Levy
 * SPDX-FileCopyrightText: 2005-2015 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_eigen
 * Sparse linear solver.
 */

#include "linear_solver.h"

#include <Eigen/Sparse>

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <vector>

/* Eigen data structures */

typedef Eigen::SparseMatrix<double, Eigen::ColMajor> EigenSparseMatrix;
typedef Eigen::SparseLU<EigenSparseMatrix> EigenSparseLU;
typedef Eigen::VectorXd EigenVectorX;
typedef Eigen::Triplet<double> EigenTriplet;

/* Linear Solver data structure */

struct LinearSolver {
  struct Coeff {
    Coeff()
    {
      index = 0;
      value = 0.0;
    }

    int index;
    double value;
  };

  struct Variable {
    Variable()
    {
      memset(value, 0, sizeof(value));
      locked = false;
      index = 0;
    }

    double value[4];
    bool locked;
    int index;
    std::vector<Coeff> a;
  };

  enum State { STATE_VARIABLES_CONSTRUCT, STATE_MATRIX_CONSTRUCT, STATE_MATRIX_SOLVED };

  LinearSolver(int num_rows_, int num_variables_, int num_rhs_, bool lsq_)
  {
    assert(num_variables_ > 0);
    assert(num_rhs_ <= 4);

    state = STATE_VARIABLES_CONSTRUCT;
    m = 0;
    n = 0;
    sparseLU = NULL;
    num_variables = num_variables_;
    num_rhs = num_rhs_;
    num_rows = num_rows_;
    least_squares = lsq_;

    variable.resize(num_variables);
  }

  ~LinearSolver()
  {
    delete sparseLU;
  }

  State state;

  int n;
  int m;

  std::vector<EigenTriplet> Mtriplets;
  EigenSparseMatrix M;
  EigenSparseMatrix MtM;
  std::vector<EigenVectorX> b;
  std::vector<EigenVectorX> x;

  EigenSparseLU *sparseLU;

  int num_variables;
  std::vector<Variable> variable;

  int num_rows;
  int num_rhs;

  bool least_squares;
};

LinearSolver *EIG_linear_solver_new(int num_rows, int num_columns, int num_rhs)
{
  return new LinearSolver(num_rows, num_columns, num_rhs, false);
}

LinearSolver *EIG_linear_least_squares_solver_new(int num_rows, int num_columns, int num_rhs)
{
  return new LinearSolver(num_rows, num_columns, num_rhs, true);
}

void EIG_linear_solver_delete(LinearSolver *solver)
{
  delete solver;
}

/* Variables */

void EIG_linear_solver_variable_set(LinearSolver *solver, int rhs, int index, double value)
{
  solver->variable[index].value[rhs] = value;
}

double EIG_linear_solver_variable_get(LinearSolver *solver, int rhs, int index)
{
  return solver->variable[index].value[rhs];
}

void EIG_linear_solver_variable_lock(LinearSolver *solver, int index)
{
  if (!solver->variable[index].locked) {
    assert(solver->state == LinearSolver::STATE_VARIABLES_CONSTRUCT);
    solver->variable[index].locked = true;
  }
}

void EIG_linear_solver_variable_unlock(LinearSolver *solver, int index)
{
  if (solver->variable[index].locked) {
    assert(solver->state == LinearSolver::STATE_VARIABLES_CONSTRUCT);
    solver->variable[index].locked = false;
  }
}

static void linear_solver_variables_to_vector(LinearSolver *solver)
{
  int num_rhs = solver->num_rhs;

  for (int i = 0; i < solver->num_variables; i++) {
    LinearSolver::Variable *v = &solver->variable[i];
    if (!v->locked) {
      for (int j = 0; j < num_rhs; j++)
        solver->x[j][v->index] = v->value[j];
    }
  }
}

static void linear_solver_vector_to_variables(LinearSolver *solver)
{
  int num_rhs = solver->num_rhs;

  for (int i = 0; i < solver->num_variables; i++) {
    LinearSolver::Variable *v = &solver->variable[i];
    if (!v->locked) {
      for (int j = 0; j < num_rhs; j++)
        v->value[j] = solver->x[j][v->index];
    }
  }
}

/* Matrix */

static void linear_solver_ensure_matrix_construct(LinearSolver *solver)
{
  /* transition to matrix construction if necessary */
  if (solver->state == LinearSolver::STATE_VARIABLES_CONSTRUCT) {
    int n = 0;

    for (int i = 0; i < solver->num_variables; i++) {
      if (solver->variable[i].locked)
        solver->variable[i].index = ~0;
      else
        solver->variable[i].index = n++;
    }

    int m = (solver->num_rows == 0) ? n : solver->num_rows;

    solver->m = m;
    solver->n = n;

    assert(solver->least_squares || m == n);

    /* reserve reasonable estimate */
    solver->Mtriplets.clear();
    solver->Mtriplets.reserve(std::max(m, n) * 3);

    solver->b.resize(solver->num_rhs);
    solver->x.resize(solver->num_rhs);

    for (int i = 0; i < solver->num_rhs; i++) {
      solver->b[i].setZero(m);
      solver->x[i].setZero(n);
    }

    linear_solver_variables_to_vector(solver);

    solver->state = LinearSolver::STATE_MATRIX_CONSTRUCT;
  }
}

void EIG_linear_solver_matrix_add(LinearSolver *solver, int row, int col, double value)
{
  if (solver->state == LinearSolver::STATE_MATRIX_SOLVED)
    return;

  linear_solver_ensure_matrix_construct(solver);

  if (!solver->least_squares && solver->variable[row].locked)
    ;
  else if (solver->variable[col].locked) {
    if (!solver->least_squares)
      row = solver->variable[row].index;

    LinearSolver::Coeff coeff;
    coeff.index = row;
    coeff.value = value;
    solver->variable[col].a.push_back(coeff);
  }
  else {
    if (!solver->least_squares)
      row = solver->variable[row].index;
    col = solver->variable[col].index;

    /* direct insert into matrix is too slow, so use triplets */
    EigenTriplet triplet(row, col, value);
    solver->Mtriplets.push_back(triplet);
  }
}

/* Right hand side */

void EIG_linear_solver_right_hand_side_add(LinearSolver *solver, int rhs, int index, double value)
{
  linear_solver_ensure_matrix_construct(solver);

  if (solver->least_squares) {
    solver->b[rhs][index] += value;
  }
  else if (!solver->variable[index].locked) {
    index = solver->variable[index].index;
    solver->b[rhs][index] += value;
  }
}

/* Solve */

bool EIG_linear_solver_solve(LinearSolver *solver)
{
  /* nothing to solve, perhaps all variables were locked */
  if (solver->m == 0 || solver->n == 0)
    return true;

  bool result = true;

  assert(solver->state != LinearSolver::STATE_VARIABLES_CONSTRUCT);

  if (solver->state == LinearSolver::STATE_MATRIX_CONSTRUCT) {
    /* create matrix from triplets */
    solver->M.resize(solver->m, solver->n);
    solver->M.setFromTriplets(solver->Mtriplets.begin(), solver->Mtriplets.end());
    solver->Mtriplets.clear();

    /* create least squares matrix */
    if (solver->least_squares)
      solver->MtM = solver->M.transpose() * solver->M;

    /* convert M to compressed column format */
    EigenSparseMatrix &M = (solver->least_squares) ? solver->MtM : solver->M;
    M.makeCompressed();

    /* perform sparse LU factorization */
    EigenSparseLU *sparseLU = new EigenSparseLU();
    solver->sparseLU = sparseLU;

    sparseLU->compute(M);
    result = (sparseLU->info() == Eigen::Success);

    solver->state = LinearSolver::STATE_MATRIX_SOLVED;
  }

  if (result) {
    /* solve for each right hand side */
    for (int rhs = 0; rhs < solver->num_rhs; rhs++) {
      /* modify for locked variables */
      EigenVectorX &b = solver->b[rhs];

      for (int i = 0; i < solver->num_variables; i++) {
        LinearSolver::Variable *variable = &solver->variable[i];

        if (variable->locked) {
          std::vector<LinearSolver::Coeff> &a = variable->a;

          for (int j = 0; j < a.size(); j++)
            b[a[j].index] -= a[j].value * variable->value[rhs];
        }
      }

      /* solve */
      if (solver->least_squares) {
        EigenVectorX Mtb = solver->M.transpose() * b;
        solver->x[rhs] = solver->sparseLU->solve(Mtb);
      }
      else {
        EigenVectorX &b = solver->b[rhs];
        solver->x[rhs] = solver->sparseLU->solve(b);
      }

      if (solver->sparseLU->info() != Eigen::Success)
        result = false;
    }

    if (result)
      linear_solver_vector_to_variables(solver);
  }

  /* clear for next solve */
  for (int rhs = 0; rhs < solver->num_rhs; rhs++)
    solver->b[rhs].setZero(solver->m);

  return result;
}

/* Debugging */

void EIG_linear_solver_print_matrix(LinearSolver *solver)
{
  std::cout << "A:" << solver->M << std::endl;

  for (int rhs = 0; rhs < solver->num_rhs; rhs++)
    std::cout << "b " << rhs << ":" << solver->b[rhs] << std::endl;

  if (solver->MtM.rows() && solver->MtM.cols())
    std::cout << "AtA:" << solver->MtM << std::endl;
}
