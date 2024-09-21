/* SPDX-FileCopyrightText: 2016 Michael Rabinovich
 *                         2023 Blender Authors
 *
 * SPDX-License-Identifier: MPL-2.0 */

/** \file
 * \ingroup intern_slim
 */

#pragma once

#include <Eigen/Dense>

namespace slim {

/* A bisection line search for a mesh based energy that avoids triangle flips as suggested in
 * "Bijective Parameterization with Free Boundaries" (Smith J. and Schaefer S., 2015).
 *
 * The user specifies an initial vertices position (that has no flips) and target one (that my have
 * flipped triangles). This method first computes the largest step in direction of the destination
 * vertices that does not incur flips, and then minimizes a given energy using this maximal step
 * and a bisection linesearch (see igl::line_search).
 *
 * Supports triangle meshes.
 *
 * Inputs:
 *   F  #F by 3 				list of mesh faces
 *   cur_v  						#V by dim list of variables
 *   dst_v  						#V by dim list of target vertices. This mesh may have flipped triangles
 *   energy       			    A function to compute the mesh-based energy (return an energy that is
 *   bigger than 0) cur_energy(OPTIONAL)         The energy at the given point. Helps save
 *   redundant computations.
 *							    This is optional. If not specified, the function will compute it.
 * Outputs:
 *		cur_v  						#V by dim list of variables at the new location
 * Returns the energy at the new point.
 */
inline double flip_avoiding_line_search(const Eigen::MatrixXi F,
                                        Eigen::MatrixXd &cur_v,
                                        Eigen::MatrixXd &dst_v,
                                        std::function<double(Eigen::MatrixXd &)> energy,
                                        double cur_energy = -1);

}  // namespace slim

#include "flip_avoiding_line_search.cpp"
