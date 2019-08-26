//
//  post-solver.hpp
//  Parametrize
//
//  Created by Jingwei on 2/5/18.
//

#ifndef post_solver_h
#define post_solver_h

#include <Eigen/Core>
#include <vector>
#include "disajoint-tree.hpp"

namespace qflow {

using namespace Eigen;

/*
 * TODO: Optimize O_quad, and possibly N_quad
 * Input:
 *  O_quad[i]: initialized i-th vertex position of the quad mesh
 *  N_quad[i]: initialized i-th vertex normal of the quad mesh
 *  Q_quad[i]: initialized i-th vertex orientation of the quad mesh, guaranteed to be orthogonal to
 *             N_quad[i]
 *  F_quad[i]: 4 vertex index of the i-th quad face
 *
 *  Concept: i-th directed edge is the (i%4)-th edge of the (i/4)-th face of the quad mesh
 *    V2E_quad[i]: one directed edge from i-th vertex of the quad mesh
 *    E2E_quad[i]: the reverse directed edge's index of the i-th directed edge of the quad mesh
 *
 *  V.col(i): i-th vertex position of the triangle mesh
 *  N.col(i): i-th vertex normal of the triangle mesh
 *  Q.col(i): i-th vertex orientation of the triangle mesh, guaranteed to be orthogonal to N.col(i)
 *  O.col(i): "quad position" associated with the i-th vertex in the triangle mesh (see InstantMesh
 *            position field)
 *  F.col(i): i-th triangle of the triangle mesh
 *
 *  V2E[i]: one directed edge from the i-th vertex of the triangle mesh
 *  E2E[i]: the reverse directed edge's index of the i-th directed edge of the triangle mesh
 *
 *  j = disajoint_tree.Index(i)
 *      the j-th vertex of the quad mesh is corresponding to the i-th vertex of the triangle mesh
 *      the relation is one-to-multiple
 *      O_quad can be viewed as an average of corresponding O
 *      N_quad can be viewed as an average of corresponding N
 *      Q_quad can be viewed as aggregation of corresponding Q
 *          Method that aggregates qi to qj with weights wi and wj:
 *              value = compat_orientation_extrinsic_4(qj, nj, qi, ni)
 *              result = (value.first * wj + value.second * wi).normalized()
 *
 * Output:
 *  Optimized O_quad, (possibly N_quad)
 */
void optimize_quad_positions(std::vector<Vector3d>& O_quad, std::vector<Vector3d>& N_quad,
                             std::vector<Vector3d>& Q_quad, std::vector<Vector4i>& F_quad,
                             VectorXi& V2E_quad, std::vector<int>& E2E_quad, MatrixXd& V, MatrixXd& N,
                             MatrixXd& Q, MatrixXd& O, MatrixXi& F, VectorXi& V2E, VectorXi& E2E,
                             DisajointTree& disajoint_tree, double reference_length,
                             bool just_serialize = true);

} // namespace qflow

#endif /* post_solver_h */
