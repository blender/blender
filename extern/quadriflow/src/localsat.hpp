#ifndef __LOCAL_SAT_H
#define __LOCAL_SAT_H

#include <Eigen/Core>
#include <vector>

namespace qflow {

using namespace Eigen;

enum class SolverStatus {
    Sat,
    Unsat,
    Timeout,
};

SolverStatus SolveSatProblem(int n_variable, std::vector<int> &value,
                             const std::vector<bool> flexible,  // NOQA
                             const std::vector<Vector3i> &variable_eq,
                             const std::vector<Vector3i> &constant_eq,
                             const std::vector<Vector4i> &variable_ge,
                             const std::vector<Vector2i> &constant_ge,
                             int timeout = 8);

void ExportLocalSat(std::vector<Vector2i> &edge_diff, const std::vector<Vector3i> &face_edgeIds,
                    const std::vector<Vector3i> &face_edgeOrients, const MatrixXi &F,
                    const VectorXi &V2E, const VectorXi &E2E);

} // namespace qflow

#endif
