#ifndef DEDGE_H_
#define DEDGE_H_

#include <Eigen/Core>
#include <Eigen/Dense>
#include <vector>

namespace qflow {

using namespace Eigen;

inline int dedge_prev_3(int e) { return (e % 3 == 0) ? e + 2 : e - 1; }
inline int dedge_next_3(int e) { return (e % 3 == 2) ? e - 2 : e + 1; }

bool compute_direct_graph(MatrixXd& V, MatrixXi& F, VectorXi& V2E,
	VectorXi& E2E, VectorXi& boundary, VectorXi& nonManifold);

void compute_direct_graph_quad(std::vector<Vector3d>& V, std::vector<Vector4i>& F, std::vector<int>& V2E,
                               std::vector<int>& E2E, VectorXi& boundary, VectorXi& nonManifold);

void remove_nonmanifold(std::vector<Vector4i> &F, std::vector<Vector3d> &V);

} // namespace qflow

#endif
