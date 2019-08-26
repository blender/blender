#ifndef MERGE_VERTEX_H_
#define MERGE_VERTEX_H_

#include <Eigen/Core>

namespace qflow {

using namespace Eigen;

void merge_close(MatrixXd& V, MatrixXi& F, double threshold);

} // namespace qflow

#endif