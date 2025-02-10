/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_slim
 */

#pragma once

#include <Eigen/Dense>
#include <Eigen/Sparse>

enum Method { TUTTE, HARMONIC, MVC };

namespace slim {

void convex_border_parameterization(const Eigen::MatrixXi &f,
                                    const Eigen::MatrixXd &v,
                                    const Eigen::MatrixXi &e,
                                    const Eigen::VectorXd &el,
                                    const Eigen::VectorXi &bnd,
                                    const Eigen::MatrixXd &bnd_uv,
                                    Eigen::MatrixXd &UV,
                                    Method method);

void mvc(const Eigen::MatrixXi &f,
         const Eigen::MatrixXd &v,
         const Eigen::MatrixXi &e,
         const Eigen::VectorXd &el,
         const Eigen::VectorXi &bnd,
         const Eigen::MatrixXd &bnd_uv,
         Eigen::MatrixXd &UV);

void harmonic(const Eigen::MatrixXi &f,
              const Eigen::MatrixXd &v,
              const Eigen::MatrixXi &e,
              const Eigen::VectorXd &el,
              const Eigen::VectorXi &bnd,
              const Eigen::MatrixXd &bnd_uv,
              Eigen::MatrixXd &UV);

void tutte(const Eigen::MatrixXi &f,
           const Eigen::MatrixXd &v,
           const Eigen::MatrixXi &e,
           const Eigen::VectorXd &el,
           const Eigen::VectorXi &bnd,
           const Eigen::MatrixXd &bnd_uv,
           Eigen::MatrixXd &UV);

void harmonic(const Eigen::MatrixXd &v,
              const Eigen::MatrixXi &f,
              const Eigen::MatrixXi &B,
              const Eigen::MatrixXd &bnd_uv,
              int power_of_harmonic_operaton,
              Eigen::MatrixXd &UV);

void map_vertices_to_convex_border(Eigen::MatrixXd &vertex_positions);

int count_flips(const Eigen::MatrixXi &f, const Eigen::MatrixXd &uv);

}  // namespace slim
