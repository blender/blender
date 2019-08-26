#include <Eigen/Core>
#include <Eigen/Dense>

#include "parametrizer.hpp"
using namespace Eigen;

namespace qflow {

void subdivide(MatrixXi &F, MatrixXd &V, VectorXd& rho, VectorXi &V2E, VectorXi &E2E, VectorXi &boundary,
               VectorXi &nonmanifold, double maxLength);

void subdivide_edgeDiff(MatrixXi &F, MatrixXd &V, MatrixXd &N, MatrixXd &Q, MatrixXd &O, MatrixXd* S,
                    VectorXi &V2E, VectorXi &E2E, VectorXi &boundary, VectorXi &nonmanifold,
                    std::vector<Vector2i> &edge_diff, std::vector<DEdge> &edge_values,
                    std::vector<Vector3i> &face_edgeOrients, std::vector<Vector3i> &face_edgeIds,
                    std::vector<int>& sharp_edges, std::map<int, int> &singularities, int max_len);
} // namespace qflow
