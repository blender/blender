/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_slim
 */

#include "uv_initializer.h"
#include "cotmatrix.h"

#include <Eigen/SparseLU>

namespace slim {

static double compute_angle(const Eigen::Vector3d &a, const Eigen::Vector3d &b)
{
  return acos(a.dot(b) / (a.norm() * b.norm()));
}

static void find_vertex_to_opposite_angles_correspondence(
    const Eigen::MatrixXi &f,
    const Eigen::MatrixXd &v,
    Eigen::SparseMatrix<double> &vertex_to_face_indices)
{

  typedef Eigen::Triplet<double> t;
  std::vector<t> coefficients;

  for (int i = 0; i < f.rows(); i++) {

    int vertex_index1 = f(i, 0);
    int vertex_index2 = f(i, 1);
    int vertex_index3 = f(i, 2);

    double angle1 = compute_angle(v.row(vertex_index2) - v.row(vertex_index1),
                                  v.row(vertex_index3) - v.row(vertex_index1));
    double angle2 = compute_angle(v.row(vertex_index3) - v.row(vertex_index2),
                                  v.row(vertex_index1) - v.row(vertex_index2));
    double angle3 = compute_angle(v.row(vertex_index1) - v.row(vertex_index3),
                                  v.row(vertex_index2) - v.row(vertex_index3));

    coefficients.push_back(t(vertex_index1, 2 * vertex_index2, angle3));
    coefficients.push_back(t(vertex_index1, 2 * vertex_index3 + 1, angle2));

    coefficients.push_back(t(vertex_index2, 2 * vertex_index1 + 1, angle3));
    coefficients.push_back(t(vertex_index2, 2 * vertex_index3, angle1));

    coefficients.push_back(t(vertex_index3, 2 * vertex_index1, angle2));
    coefficients.push_back(t(vertex_index3, 2 * vertex_index2 + 1, angle1));
  }

  vertex_to_face_indices.setFromTriplets(coefficients.begin(), coefficients.end());
}

static void find_vertex_to_its_angles_correspondence(
    const Eigen::MatrixXi &f,
    const Eigen::MatrixXd &v,
    Eigen::SparseMatrix<double> &vertex_to_face_indices)
{

  typedef Eigen::Triplet<double> t;
  std::vector<t> coefficients;

  for (int i = 0; i < f.rows(); i++) {

    int vertex_index1 = f(i, 0);
    int vertex_index2 = f(i, 1);
    int vertex_index3 = f(i, 2);

    double angle1 = compute_angle(v.row(vertex_index2) - v.row(vertex_index1),
                                  v.row(vertex_index3) - v.row(vertex_index1));
    double angle2 = compute_angle(v.row(vertex_index3) - v.row(vertex_index2),
                                  v.row(vertex_index1) - v.row(vertex_index2));
    double angle3 = compute_angle(v.row(vertex_index1) - v.row(vertex_index3),
                                  v.row(vertex_index2) - v.row(vertex_index3));

    coefficients.push_back(t(vertex_index1, 2 * vertex_index2, angle1));
    coefficients.push_back(t(vertex_index1, 2 * vertex_index3 + 1, angle1));

    coefficients.push_back(t(vertex_index2, 2 * vertex_index1 + 1, angle2));
    coefficients.push_back(t(vertex_index2, 2 * vertex_index3, angle2));

    coefficients.push_back(t(vertex_index3, 2 * vertex_index1, angle3));
    coefficients.push_back(t(vertex_index3, 2 * vertex_index2 + 1, angle3));
  }

  vertex_to_face_indices.setFromTriplets(coefficients.begin(), coefficients.end());
}

/* Implementation of different fixed-border parameterizations, mean value coordinates, harmonic,
 * tutte. */
void convex_border_parameterization(const Eigen::MatrixXi &f,
                                    const Eigen::MatrixXd &v,
                                    const Eigen::MatrixXi &e,
                                    const Eigen::VectorXd &el,
                                    const Eigen::VectorXi &bnd,
                                    const Eigen::MatrixXd &bnd_uv,
                                    Eigen::MatrixXd &uv,
                                    Method method)
{
  int verts_num = uv.rows();
  int edges_num = e.rows();

  Eigen::SparseMatrix<double> vertex_to_angles(verts_num, verts_num * 2);

  switch (method) {
    case HARMONIC:
      find_vertex_to_opposite_angles_correspondence(f, v, vertex_to_angles);
      break;
    case MVC:
      find_vertex_to_its_angles_correspondence(f, v, vertex_to_angles);
      break;
    case TUTTE:
      break;
  }

  int n_unknowns = verts_num - bnd.size();
  int n_knowns = bnd.size();

  Eigen::SparseMatrix<double> aint(n_unknowns, n_unknowns);
  Eigen::SparseMatrix<double> abnd(n_unknowns, n_knowns);
  Eigen::VectorXd z(n_knowns);

  std::vector<Eigen::Triplet<double>> int_triplet_vector;
  std::vector<Eigen::Triplet<double>> bnd_triplet_vector;

  int rowindex;
  int columnindex;
  double edge_weight, edge_length;
  Eigen::RowVector2i edge;

  int first_vertex, second_vertex;

  for (int e_idx = 0; e_idx < edges_num; e_idx++) {
    edge = e.row(e_idx);
    edge_length = el(e_idx);
    first_vertex = edge(0);
    second_vertex = edge(1);

    if (first_vertex >= n_knowns) {
      /* Into aint. */
      rowindex = first_vertex - n_knowns;

      double angle1 = vertex_to_angles.coeff(first_vertex, 2 * second_vertex);
      double angle2 = vertex_to_angles.coeff(first_vertex, 2 * second_vertex + 1);

      switch (method) {
        case HARMONIC:
          edge_weight = 1 / tan(angle1) + 1 / tan(angle2);
          break;
        case MVC:
          edge_weight = tan(angle1 / 2) + tan(angle2 / 2);
          edge_weight /= edge_length;
          break;
        case TUTTE:
          edge_weight = 1;
          break;
      }

      int_triplet_vector.push_back(Eigen::Triplet<double>(rowindex, rowindex, edge_weight));

      if (second_vertex >= n_knowns) {
        /* Also an unknown point in the interior. */
        columnindex = second_vertex - n_knowns;

        int_triplet_vector.push_back(Eigen::Triplet<double>(rowindex, columnindex, -edge_weight));
      }
      else {
        /* Known point on the border. */
        columnindex = second_vertex;
        bnd_triplet_vector.push_back(Eigen::Triplet<double>(rowindex, columnindex, edge_weight));
      }
    }
  }

  aint.setFromTriplets(int_triplet_vector.begin(), int_triplet_vector.end());
  aint.makeCompressed();

  abnd.setFromTriplets(bnd_triplet_vector.begin(), bnd_triplet_vector.end());
  abnd.makeCompressed();

  for (int i = 0; i < n_unknowns; i++) {
    double factor = aint.coeff(i, i);
    aint.row(i) /= factor;
    abnd.row(i) /= factor;
  }

  Eigen::SparseLU<Eigen::SparseMatrix<double>> solver;
  solver.compute(aint);

  for (int i = 0; i < 2; i++) {

    for (int zindex = 0; zindex < n_knowns; zindex++) {
      z(zindex) = bnd_uv(bnd(zindex), i);
    }

    Eigen::VectorXd b = abnd * z;

    Eigen::VectorXd uvs;
    uvs = solver.solve(b);

    Eigen::VectorXd boundary = bnd_uv.col(i);
    Eigen::VectorXd interior = uvs;

    uv.col(i) << boundary, interior;
  }
}

void mvc(const Eigen::MatrixXi &f,
         const Eigen::MatrixXd &v,
         const Eigen::MatrixXi &e,
         const Eigen::VectorXd &el,
         const Eigen::VectorXi &bnd,
         const Eigen::MatrixXd &bnd_uv,
         Eigen::MatrixXd &uv)
{

  convex_border_parameterization(f, v, e, el, bnd, bnd_uv, uv, Method::MVC);
}

void harmonic(const Eigen::MatrixXi &f,
              const Eigen::MatrixXd &v,
              const Eigen::MatrixXi &e,
              const Eigen::VectorXd &el,
              const Eigen::VectorXi &bnd,
              const Eigen::MatrixXd &bnd_uv,
              Eigen::MatrixXd &uv)
{

  convex_border_parameterization(f, v, e, el, bnd, bnd_uv, uv, Method::HARMONIC);
}

void tutte(const Eigen::MatrixXi &f,
           const Eigen::MatrixXd &v,
           const Eigen::MatrixXi &e,
           const Eigen::VectorXd &el,
           const Eigen::VectorXi &bnd,
           const Eigen::MatrixXd &bnd_uv,
           Eigen::MatrixXd &uv)
{

  convex_border_parameterization(f, v, e, el, bnd, bnd_uv, uv, Method::TUTTE);
}

void map_vertices_to_convex_border(Eigen::MatrixXd &vertex_positions)
{
  double pi = atan(1) * 4;
  int boundary_vertices_num = vertex_positions.rows();
  double x, y;
  double angle = 2 * pi / boundary_vertices_num;

  for (int i = 0; i < boundary_vertices_num; i++) {
    x = cos(angle * i);
    y = sin(angle * i);
    vertex_positions(i, 0) = (x * 0.5) + 0.5;
    vertex_positions(i, 1) = (y * 0.5) + 0.5;
  }
}

static void get_flips(const Eigen::MatrixXi &f,
                      const Eigen::MatrixXd &uv,
                      std::vector<int> &flip_idx)
{
  flip_idx.resize(0);
  for (int i = 0; i < f.rows(); i++) {

    Eigen::Vector2d v1_n = uv.row(f(i, 0));
    Eigen::Vector2d v2_n = uv.row(f(i, 1));
    Eigen::Vector2d v3_n = uv.row(f(i, 2));

    Eigen::MatrixXd t2_homo(3, 3);
    t2_homo.col(0) << v1_n(0), v1_n(1), 1;
    t2_homo.col(1) << v2_n(0), v2_n(1), 1;
    t2_homo.col(2) << v3_n(0), v3_n(1), 1;
    double det = t2_homo.determinant();
    assert(det == det);
    if (det < 0) {
      flip_idx.push_back(i);
    }
  }
}

int count_flips(const Eigen::MatrixXi &f, const Eigen::MatrixXd &uv)
{

  std::vector<int> flip_idx;
  get_flips(f, uv, flip_idx);

  return flip_idx.size();
}

}  // namespace slim
