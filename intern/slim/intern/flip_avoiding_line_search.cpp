/* SPDX-FileCopyrightText: 2016 Michael Rabinovich
 *                         2023 Blender Authors
 *
 * SPDX-License-Identifier: MPL-2.0 */

/** \file
 * \ingroup intern_slim
 */

#include "flip_avoiding_line_search.h"

#include <Eigen/Dense>
#include <vector>

namespace slim {

/* Implement a bisection linesearch to minimize a mesh-based energy on vertices given at 'x' at a
 * search direction 'd', with initial step size. Stops when a point with lower energy is found, or
 * after maximal iterations have been reached.
 *
 * Inputs:
 *   x  						#X by dim list of variables
 *   d  						#X by dim list of a given search direction
 *   step_size  			initial step size
 *   energy       			A function to compute the mesh-based energy (return an energy that is
 *   bigger than 0) cur_energy(OPTIONAL)     The energy at the given point. Helps save redundant
 *   computations.
 *							This is optional. If not specified, the function will compute it.
 * Outputs:
 *		x  						#X by dim list of variables at the new location
 * Returns the energy at the new point 'x'.
 */
static inline double line_search(Eigen::MatrixXd &x,
                                 const Eigen::MatrixXd &d,
                                 double step_size,
                                 std::function<double(Eigen::MatrixXd &)> energy,
                                 double cur_energy = -1)
{
  double old_energy;
  if (cur_energy > 0) {
    old_energy = cur_energy;
  }
  else {
    old_energy = energy(x); /* No energy was given -> need to compute the current energy. */
  }
  double new_energy = old_energy;
  int cur_iter = 0;
  int MAX_STEP_SIZE_ITER = 12;

  while (new_energy >= old_energy && cur_iter < MAX_STEP_SIZE_ITER) {
    Eigen::MatrixXd new_x = x + step_size * d;

    double cur_e = energy(new_x);
    if (cur_e >= old_energy) {
      step_size /= 2;
    }
    else {
      x = new_x;
      new_energy = cur_e;
    }
    cur_iter++;
  }
  return new_energy;
}

static inline double get_smallest_pos_quad_zero(double a, double b, double c)
{
  using namespace std;
  double t1, t2;
  if (a != 0) {
    double delta_in = pow(b, 2) - 4 * a * c;
    if (delta_in < 0) {
      return INFINITY;
    }
    double delta = sqrt(delta_in);
    t1 = (-b + delta) / (2 * a);
    t2 = (-b - delta) / (2 * a);
  }
  else {
    t1 = t2 = -b / c;
  }

  if (!std::isfinite(t1) || !std::isfinite(t2)) {
    throw SlimFailedException();
  }

  double tmp_n = min(t1, t2);
  t1 = max(t1, t2);
  t2 = tmp_n;
  if (t1 == t2) {
    return INFINITY; /* Means the orientation flips twice = doesn't flip. */
  }
  /* Return the smallest negative root if it exists, otherwise return infinity. */
  if (t1 > 0) {
    if (t2 > 0) {
      return t2;
    }
    else {
      return t1;
    }
  }
  else {
    return INFINITY;
  }
}

static inline double get_min_pos_root_2D(const Eigen::MatrixXd &uv,
                                         const Eigen::MatrixXi &F,
                                         Eigen::MatrixXd &d,
                                         int f)
{
  using namespace std;
  /* Finding the smallest timestep t s.t a triangle get degenerated (<=> det = 0). */
  int v1 = F(f, 0);
  int v2 = F(f, 1);
  int v3 = F(f, 2);
  /* Get quadratic coefficients (ax^2 + b^x + c). */
  const double &U11 = uv(v1, 0);
  const double &U12 = uv(v1, 1);
  const double &U21 = uv(v2, 0);
  const double &U22 = uv(v2, 1);
  const double &U31 = uv(v3, 0);
  const double &U32 = uv(v3, 1);

  const double &V11 = d(v1, 0);
  const double &V12 = d(v1, 1);
  const double &V21 = d(v2, 0);
  const double &V22 = d(v2, 1);
  const double &V31 = d(v3, 0);
  const double &V32 = d(v3, 1);

  double a = V11 * V22 - V12 * V21 - V11 * V32 + V12 * V31 + V21 * V32 - V22 * V31;
  double b = U11 * V22 - U12 * V21 - U21 * V12 + U22 * V11 - U11 * V32 + U12 * V31 + U31 * V12 -
             U32 * V11 + U21 * V32 - U22 * V31 - U31 * V22 + U32 * V21;
  double c = U11 * U22 - U12 * U21 - U11 * U32 + U12 * U31 + U21 * U32 - U22 * U31;

  return get_smallest_pos_quad_zero(a, b, c);
}

static inline double compute_max_step_from_singularities(const Eigen::MatrixXd &uv,
                                                         const Eigen::MatrixXi &F,
                                                         Eigen::MatrixXd &d)
{
  using namespace std;
  double max_step = INFINITY;

  /* The if statement is outside the for loops to avoid branching/ease parallelizing. */
  for (int f = 0; f < F.rows(); f++) {
    double min_positive_root = get_min_pos_root_2D(uv, F, d, f);
    max_step = min(max_step, min_positive_root);
  }
  return max_step;
}

inline double flip_avoiding_line_search(const Eigen::MatrixXi F,
                                        Eigen::MatrixXd &cur_v,
                                        Eigen::MatrixXd &dst_v,
                                        std::function<double(Eigen::MatrixXd &)> energy,
                                        double cur_energy)
{
  using namespace std;

  Eigen::MatrixXd d = dst_v - cur_v;
  double min_step_to_singularity = compute_max_step_from_singularities(cur_v, F, d);
  double max_step_size = min(1., min_step_to_singularity * 0.8);
  return line_search(cur_v, d, max_step_size, energy, cur_energy);
}

}  // namespace slim
