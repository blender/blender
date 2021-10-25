// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2015 Google Inc. All rights reserved.
// http://ceres-solver.org/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name of Google Inc. nor the names of its contributors may be
//   used to endorse or promote products derived from this software without
//   specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Author: sameeragarwal@google.com (Sameer Agarwal)

#include "ceres/covariance_impl.h"

#ifdef CERES_USE_OPENMP
#include <omp.h>
#endif

#include <algorithm>
#include <cstdlib>
#include <numeric>
#include <sstream>
#include <utility>
#include <vector>

#include "Eigen/SparseCore"
#include "Eigen/SparseQR"
#include "Eigen/SVD"

#include "ceres/collections_port.h"
#include "ceres/compressed_col_sparse_matrix_utils.h"
#include "ceres/compressed_row_sparse_matrix.h"
#include "ceres/covariance.h"
#include "ceres/crs_matrix.h"
#include "ceres/internal/eigen.h"
#include "ceres/map_util.h"
#include "ceres/parameter_block.h"
#include "ceres/problem_impl.h"
#include "ceres/residual_block.h"
#include "ceres/suitesparse.h"
#include "ceres/wall_time.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {

using std::make_pair;
using std::map;
using std::pair;
using std::sort;
using std::swap;
using std::vector;

typedef vector<pair<const double*, const double*> > CovarianceBlocks;

CovarianceImpl::CovarianceImpl(const Covariance::Options& options)
    : options_(options),
      is_computed_(false),
      is_valid_(false) {
#ifndef CERES_USE_OPENMP
  if (options_.num_threads > 1) {
    LOG(WARNING)
        << "OpenMP support is not compiled into this binary; "
        << "only options.num_threads = 1 is supported. Switching "
        << "to single threaded mode.";
    options_.num_threads = 1;
  }
#endif
  evaluate_options_.num_threads = options_.num_threads;
  evaluate_options_.apply_loss_function = options_.apply_loss_function;
}

CovarianceImpl::~CovarianceImpl() {
}

template <typename T> void CheckForDuplicates(vector<T> blocks) {
  sort(blocks.begin(), blocks.end());
  typename vector<T>::iterator it =
      std::adjacent_find(blocks.begin(), blocks.end());
  if (it != blocks.end()) {
    // In case there are duplicates, we search for their location.
    map<T, vector<int> > blocks_map;
    for (int i = 0; i < blocks.size(); ++i) {
      blocks_map[blocks[i]].push_back(i);
    }

    std::ostringstream duplicates;
    while (it != blocks.end()) {
      duplicates << "(";
      for (int i = 0; i < blocks_map[*it].size() - 1; ++i) {
        duplicates << blocks_map[*it][i] << ", ";
      }
      duplicates << blocks_map[*it].back() << ")";
      it = std::adjacent_find(it + 1, blocks.end());
      if (it < blocks.end()) {
        duplicates << " and ";
      }
    }

    LOG(FATAL) << "Covariance::Compute called with duplicate blocks at "
               << "indices " << duplicates.str();
  }
}

bool CovarianceImpl::Compute(const CovarianceBlocks& covariance_blocks,
                             ProblemImpl* problem) {
  CheckForDuplicates<pair<const double*, const double*> >(covariance_blocks);
  problem_ = problem;
  parameter_block_to_row_index_.clear();
  covariance_matrix_.reset(NULL);
  is_valid_ = (ComputeCovarianceSparsity(covariance_blocks, problem) &&
               ComputeCovarianceValues());
  is_computed_ = true;
  return is_valid_;
}

bool CovarianceImpl::Compute(const vector<const double*>& parameter_blocks,
                             ProblemImpl* problem) {
  CheckForDuplicates<const double*>(parameter_blocks);
  CovarianceBlocks covariance_blocks;
  for (int i = 0; i < parameter_blocks.size(); ++i) {
    for (int j = i; j < parameter_blocks.size(); ++j) {
      covariance_blocks.push_back(make_pair(parameter_blocks[i],
                                            parameter_blocks[j]));
    }
  }

  return Compute(covariance_blocks, problem);
}

bool CovarianceImpl::GetCovarianceBlockInTangentOrAmbientSpace(
    const double* original_parameter_block1,
    const double* original_parameter_block2,
    bool lift_covariance_to_ambient_space,
    double* covariance_block) const {
  CHECK(is_computed_)
      << "Covariance::GetCovarianceBlock called before Covariance::Compute";
  CHECK(is_valid_)
      << "Covariance::GetCovarianceBlock called when Covariance::Compute "
      << "returned false.";

  // If either of the two parameter blocks is constant, then the
  // covariance block is also zero.
  if (constant_parameter_blocks_.count(original_parameter_block1) > 0 ||
      constant_parameter_blocks_.count(original_parameter_block2) > 0) {
    const ProblemImpl::ParameterMap& parameter_map = problem_->parameter_map();
    ParameterBlock* block1 =
        FindOrDie(parameter_map,
                  const_cast<double*>(original_parameter_block1));

    ParameterBlock* block2 =
        FindOrDie(parameter_map,
                  const_cast<double*>(original_parameter_block2));

    const int block1_size = block1->Size();
    const int block2_size = block2->Size();
    const int block1_local_size = block1->LocalSize();
    const int block2_local_size = block2->LocalSize();
    if (!lift_covariance_to_ambient_space) {
      MatrixRef(covariance_block, block1_local_size, block2_local_size)
          .setZero();
    } else {
      MatrixRef(covariance_block, block1_size, block2_size).setZero();
    }
    return true;
  }

  const double* parameter_block1 = original_parameter_block1;
  const double* parameter_block2 = original_parameter_block2;
  const bool transpose = parameter_block1 > parameter_block2;
  if (transpose) {
    swap(parameter_block1, parameter_block2);
  }

  // Find where in the covariance matrix the block is located.
  const int row_begin =
      FindOrDie(parameter_block_to_row_index_, parameter_block1);
  const int col_begin =
      FindOrDie(parameter_block_to_row_index_, parameter_block2);
  const int* rows = covariance_matrix_->rows();
  const int* cols = covariance_matrix_->cols();
  const int row_size = rows[row_begin + 1] - rows[row_begin];
  const int* cols_begin = cols + rows[row_begin];

  // The only part that requires work is walking the compressed column
  // vector to determine where the set of columns correspnding to the
  // covariance block begin.
  int offset = 0;
  while (cols_begin[offset] != col_begin && offset < row_size) {
    ++offset;
  }

  if (offset == row_size) {
    LOG(ERROR) << "Unable to find covariance block for "
               << original_parameter_block1 << " "
               << original_parameter_block2;
    return false;
  }

  const ProblemImpl::ParameterMap& parameter_map = problem_->parameter_map();
  ParameterBlock* block1 =
      FindOrDie(parameter_map, const_cast<double*>(parameter_block1));
  ParameterBlock* block2 =
      FindOrDie(parameter_map, const_cast<double*>(parameter_block2));
  const LocalParameterization* local_param1 = block1->local_parameterization();
  const LocalParameterization* local_param2 = block2->local_parameterization();
  const int block1_size = block1->Size();
  const int block1_local_size = block1->LocalSize();
  const int block2_size = block2->Size();
  const int block2_local_size = block2->LocalSize();

  ConstMatrixRef cov(covariance_matrix_->values() + rows[row_begin],
                     block1_size,
                     row_size);

  // Fast path when there are no local parameterizations or if the
  // user does not want it lifted to the ambient space.
  if ((local_param1 == NULL && local_param2 == NULL) ||
      !lift_covariance_to_ambient_space) {
    if (transpose) {
      MatrixRef(covariance_block, block2_local_size, block1_local_size) =
          cov.block(0, offset, block1_local_size,
                    block2_local_size).transpose();
    } else {
      MatrixRef(covariance_block, block1_local_size, block2_local_size) =
          cov.block(0, offset, block1_local_size, block2_local_size);
    }
    return true;
  }

  // If local parameterizations are used then the covariance that has
  // been computed is in the tangent space and it needs to be lifted
  // back to the ambient space.
  //
  // This is given by the formula
  //
  //  C'_12 = J_1 C_12 J_2'
  //
  // Where C_12 is the local tangent space covariance for parameter
  // blocks 1 and 2. J_1 and J_2 are respectively the local to global
  // jacobians for parameter blocks 1 and 2.
  //
  // See Result 5.11 on page 142 of Hartley & Zisserman (2nd Edition)
  // for a proof.
  //
  // TODO(sameeragarwal): Add caching of local parameterization, so
  // that they are computed just once per parameter block.
  Matrix block1_jacobian(block1_size, block1_local_size);
  if (local_param1 == NULL) {
    block1_jacobian.setIdentity();
  } else {
    local_param1->ComputeJacobian(parameter_block1, block1_jacobian.data());
  }

  Matrix block2_jacobian(block2_size, block2_local_size);
  // Fast path if the user is requesting a diagonal block.
  if (parameter_block1 == parameter_block2) {
    block2_jacobian = block1_jacobian;
  } else {
    if (local_param2 == NULL) {
      block2_jacobian.setIdentity();
    } else {
      local_param2->ComputeJacobian(parameter_block2, block2_jacobian.data());
    }
  }

  if (transpose) {
    MatrixRef(covariance_block, block2_size, block1_size) =
        block2_jacobian *
        cov.block(0, offset, block1_local_size, block2_local_size).transpose() *
        block1_jacobian.transpose();
  } else {
    MatrixRef(covariance_block, block1_size, block2_size) =
        block1_jacobian *
        cov.block(0, offset, block1_local_size, block2_local_size) *
        block2_jacobian.transpose();
  }

  return true;
}

bool CovarianceImpl::GetCovarianceMatrixInTangentOrAmbientSpace(
    const vector<const double*>& parameters,
    bool lift_covariance_to_ambient_space,
    double* covariance_matrix) const {
  CHECK(is_computed_)
      << "Covariance::GetCovarianceMatrix called before Covariance::Compute";
  CHECK(is_valid_)
      << "Covariance::GetCovarianceMatrix called when Covariance::Compute "
      << "returned false.";

  const ProblemImpl::ParameterMap& parameter_map = problem_->parameter_map();
  // For OpenMP compatibility we need to define these vectors in advance
  const int num_parameters = parameters.size();
  vector<int> parameter_sizes;
  vector<int> cum_parameter_size;
  parameter_sizes.reserve(num_parameters);
  cum_parameter_size.resize(num_parameters + 1);
  cum_parameter_size[0] = 0;
  for (int i = 0; i < num_parameters; ++i) {
    ParameterBlock* block =
        FindOrDie(parameter_map, const_cast<double*>(parameters[i]));
    if (lift_covariance_to_ambient_space) {
      parameter_sizes.push_back(block->Size());
    } else {
      parameter_sizes.push_back(block->LocalSize());
    }
  }
  std::partial_sum(parameter_sizes.begin(), parameter_sizes.end(),
                   cum_parameter_size.begin() + 1);
  const int max_covariance_block_size =
      *std::max_element(parameter_sizes.begin(), parameter_sizes.end());
  const int covariance_size = cum_parameter_size.back();

  // Assemble the blocks in the covariance matrix.
  MatrixRef covariance(covariance_matrix, covariance_size, covariance_size);
  const int num_threads = options_.num_threads;
  scoped_array<double> workspace(
      new double[num_threads * max_covariance_block_size *
                 max_covariance_block_size]);

  bool success = true;

// The collapse() directive is only supported in OpenMP 3.0 and higher. OpenMP
// 3.0 was released in May 2008 (hence the version number).
#if _OPENMP >= 200805
#  pragma omp parallel for num_threads(num_threads) schedule(dynamic) collapse(2)
#else
#  pragma omp parallel for num_threads(num_threads) schedule(dynamic)
#endif
  for (int i = 0; i < num_parameters; ++i) {
    for (int j = 0; j < num_parameters; ++j) {
      // The second loop can't start from j = i for compatibility with OpenMP
      // collapse command. The conditional serves as a workaround
      if (j >= i) {
        int covariance_row_idx = cum_parameter_size[i];
        int covariance_col_idx = cum_parameter_size[j];
        int size_i = parameter_sizes[i];
        int size_j = parameter_sizes[j];
#ifdef CERES_USE_OPENMP
        int thread_id = omp_get_thread_num();
#else
        int thread_id = 0;
#endif
        double* covariance_block =
            workspace.get() +
            thread_id * max_covariance_block_size * max_covariance_block_size;
        if (!GetCovarianceBlockInTangentOrAmbientSpace(
                parameters[i], parameters[j], lift_covariance_to_ambient_space,
                covariance_block)) {
          success = false;
        }

        covariance.block(covariance_row_idx, covariance_col_idx,
                         size_i, size_j) =
            MatrixRef(covariance_block, size_i, size_j);

        if (i != j) {
          covariance.block(covariance_col_idx, covariance_row_idx,
                           size_j, size_i) =
              MatrixRef(covariance_block, size_i, size_j).transpose();

        }
      }
    }
  }
  return success;
}

// Determine the sparsity pattern of the covariance matrix based on
// the block pairs requested by the user.
bool CovarianceImpl::ComputeCovarianceSparsity(
    const CovarianceBlocks&  original_covariance_blocks,
    ProblemImpl* problem) {
  EventLogger event_logger("CovarianceImpl::ComputeCovarianceSparsity");

  // Determine an ordering for the parameter block, by sorting the
  // parameter blocks by their pointers.
  vector<double*> all_parameter_blocks;
  problem->GetParameterBlocks(&all_parameter_blocks);
  const ProblemImpl::ParameterMap& parameter_map = problem->parameter_map();
  HashSet<ParameterBlock*> parameter_blocks_in_use;
  vector<ResidualBlock*> residual_blocks;
  problem->GetResidualBlocks(&residual_blocks);

  for (int i = 0; i < residual_blocks.size(); ++i) {
    ResidualBlock* residual_block = residual_blocks[i];
    parameter_blocks_in_use.insert(residual_block->parameter_blocks(),
                                   residual_block->parameter_blocks() +
                                   residual_block->NumParameterBlocks());
  }

  constant_parameter_blocks_.clear();
  vector<double*>& active_parameter_blocks =
      evaluate_options_.parameter_blocks;
  active_parameter_blocks.clear();
  for (int i = 0; i < all_parameter_blocks.size(); ++i) {
    double* parameter_block = all_parameter_blocks[i];
    ParameterBlock* block = FindOrDie(parameter_map, parameter_block);
    if (!block->IsConstant() && (parameter_blocks_in_use.count(block) > 0)) {
      active_parameter_blocks.push_back(parameter_block);
    } else {
      constant_parameter_blocks_.insert(parameter_block);
    }
  }

  std::sort(active_parameter_blocks.begin(), active_parameter_blocks.end());

  // Compute the number of rows.  Map each parameter block to the
  // first row corresponding to it in the covariance matrix using the
  // ordering of parameter blocks just constructed.
  int num_rows = 0;
  parameter_block_to_row_index_.clear();
  for (int i = 0; i < active_parameter_blocks.size(); ++i) {
    double* parameter_block = active_parameter_blocks[i];
    const int parameter_block_size =
        problem->ParameterBlockLocalSize(parameter_block);
    parameter_block_to_row_index_[parameter_block] = num_rows;
    num_rows += parameter_block_size;
  }

  // Compute the number of non-zeros in the covariance matrix.  Along
  // the way flip any covariance blocks which are in the lower
  // triangular part of the matrix.
  int num_nonzeros = 0;
  CovarianceBlocks covariance_blocks;
  for (int i = 0; i <  original_covariance_blocks.size(); ++i) {
    const pair<const double*, const double*>& block_pair =
        original_covariance_blocks[i];
    if (constant_parameter_blocks_.count(block_pair.first) > 0 ||
        constant_parameter_blocks_.count(block_pair.second) > 0) {
      continue;
    }

    int index1 = FindOrDie(parameter_block_to_row_index_, block_pair.first);
    int index2 = FindOrDie(parameter_block_to_row_index_, block_pair.second);
    const int size1 = problem->ParameterBlockLocalSize(block_pair.first);
    const int size2 = problem->ParameterBlockLocalSize(block_pair.second);
    num_nonzeros += size1 * size2;

    // Make sure we are constructing a block upper triangular matrix.
    if (index1 > index2) {
      covariance_blocks.push_back(make_pair(block_pair.second,
                                            block_pair.first));
    } else {
      covariance_blocks.push_back(block_pair);
    }
  }

  if (covariance_blocks.size() == 0) {
    VLOG(2) << "No non-zero covariance blocks found";
    covariance_matrix_.reset(NULL);
    return true;
  }

  // Sort the block pairs. As a consequence we get the covariance
  // blocks as they will occur in the CompressedRowSparseMatrix that
  // will store the covariance.
  sort(covariance_blocks.begin(), covariance_blocks.end());

  // Fill the sparsity pattern of the covariance matrix.
  covariance_matrix_.reset(
      new CompressedRowSparseMatrix(num_rows, num_rows, num_nonzeros));

  int* rows = covariance_matrix_->mutable_rows();
  int* cols = covariance_matrix_->mutable_cols();

  // Iterate over parameter blocks and in turn over the rows of the
  // covariance matrix. For each parameter block, look in the upper
  // triangular part of the covariance matrix to see if there are any
  // blocks requested by the user. If this is the case then fill out a
  // set of compressed rows corresponding to this parameter block.
  //
  // The key thing that makes this loop work is the fact that the
  // row/columns of the covariance matrix are ordered by the pointer
  // values of the parameter blocks. Thus iterating over the keys of
  // parameter_block_to_row_index_ corresponds to iterating over the
  // rows of the covariance matrix in order.
  int i = 0;  // index into covariance_blocks.
  int cursor = 0;  // index into the covariance matrix.
  for (map<const double*, int>::const_iterator it =
           parameter_block_to_row_index_.begin();
       it != parameter_block_to_row_index_.end();
       ++it) {
    const double* row_block =  it->first;
    const int row_block_size = problem->ParameterBlockLocalSize(row_block);
    int row_begin = it->second;

    // Iterate over the covariance blocks contained in this row block
    // and count the number of columns in this row block.
    int num_col_blocks = 0;
    int num_columns = 0;
    for (int j = i; j < covariance_blocks.size(); ++j, ++num_col_blocks) {
      const pair<const double*, const double*>& block_pair =
          covariance_blocks[j];
      if (block_pair.first != row_block) {
        break;
      }
      num_columns += problem->ParameterBlockLocalSize(block_pair.second);
    }

    // Fill out all the compressed rows for this parameter block.
    for (int r = 0; r < row_block_size; ++r) {
      rows[row_begin + r] = cursor;
      for (int c = 0; c < num_col_blocks; ++c) {
        const double* col_block = covariance_blocks[i + c].second;
        const int col_block_size = problem->ParameterBlockLocalSize(col_block);
        int col_begin = FindOrDie(parameter_block_to_row_index_, col_block);
        for (int k = 0; k < col_block_size; ++k) {
          cols[cursor++] = col_begin++;
        }
      }
    }

    i+= num_col_blocks;
  }

  rows[num_rows] = cursor;
  return true;
}

bool CovarianceImpl::ComputeCovarianceValues() {
  switch (options_.algorithm_type) {
    case DENSE_SVD:
      return ComputeCovarianceValuesUsingDenseSVD();
    case SUITE_SPARSE_QR:
#ifndef CERES_NO_SUITESPARSE
      return ComputeCovarianceValuesUsingSuiteSparseQR();
#else
      LOG(ERROR) << "SuiteSparse is required to use the "
                 << "SUITE_SPARSE_QR algorithm.";
      return false;
#endif
    case EIGEN_SPARSE_QR:
      return ComputeCovarianceValuesUsingEigenSparseQR();
    default:
      LOG(ERROR) << "Unsupported covariance estimation algorithm type: "
                 << CovarianceAlgorithmTypeToString(options_.algorithm_type);
      return false;
  }
  return false;
}

bool CovarianceImpl::ComputeCovarianceValuesUsingSuiteSparseQR() {
  EventLogger event_logger(
      "CovarianceImpl::ComputeCovarianceValuesUsingSparseQR");

#ifndef CERES_NO_SUITESPARSE
  if (covariance_matrix_.get() == NULL) {
    // Nothing to do, all zeros covariance matrix.
    return true;
  }

  CRSMatrix jacobian;
  problem_->Evaluate(evaluate_options_, NULL, NULL, NULL, &jacobian);
  event_logger.AddEvent("Evaluate");

  // Construct a compressed column form of the Jacobian.
  const int num_rows = jacobian.num_rows;
  const int num_cols = jacobian.num_cols;
  const int num_nonzeros = jacobian.values.size();

  vector<SuiteSparse_long> transpose_rows(num_cols + 1, 0);
  vector<SuiteSparse_long> transpose_cols(num_nonzeros, 0);
  vector<double> transpose_values(num_nonzeros, 0);

  for (int idx = 0; idx < num_nonzeros; ++idx) {
    transpose_rows[jacobian.cols[idx] + 1] += 1;
  }

  for (int i = 1; i < transpose_rows.size(); ++i) {
    transpose_rows[i] += transpose_rows[i - 1];
  }

  for (int r = 0; r < num_rows; ++r) {
    for (int idx = jacobian.rows[r]; idx < jacobian.rows[r + 1]; ++idx) {
      const int c = jacobian.cols[idx];
      const int transpose_idx = transpose_rows[c];
      transpose_cols[transpose_idx] = r;
      transpose_values[transpose_idx] = jacobian.values[idx];
      ++transpose_rows[c];
    }
  }

  for (int i = transpose_rows.size() - 1; i > 0 ; --i) {
    transpose_rows[i] = transpose_rows[i - 1];
  }
  transpose_rows[0] = 0;

  cholmod_sparse cholmod_jacobian;
  cholmod_jacobian.nrow = num_rows;
  cholmod_jacobian.ncol = num_cols;
  cholmod_jacobian.nzmax = num_nonzeros;
  cholmod_jacobian.nz = NULL;
  cholmod_jacobian.p = reinterpret_cast<void*>(&transpose_rows[0]);
  cholmod_jacobian.i = reinterpret_cast<void*>(&transpose_cols[0]);
  cholmod_jacobian.x = reinterpret_cast<void*>(&transpose_values[0]);
  cholmod_jacobian.z = NULL;
  cholmod_jacobian.stype = 0;  // Matrix is not symmetric.
  cholmod_jacobian.itype = CHOLMOD_LONG;
  cholmod_jacobian.xtype = CHOLMOD_REAL;
  cholmod_jacobian.dtype = CHOLMOD_DOUBLE;
  cholmod_jacobian.sorted = 1;
  cholmod_jacobian.packed = 1;

  cholmod_common cc;
  cholmod_l_start(&cc);

  cholmod_sparse* R = NULL;
  SuiteSparse_long* permutation = NULL;

  // Compute a Q-less QR factorization of the Jacobian. Since we are
  // only interested in inverting J'J = R'R, we do not need Q. This
  // saves memory and gives us R as a permuted compressed column
  // sparse matrix.
  //
  // TODO(sameeragarwal): Currently the symbolic factorization and the
  // numeric factorization is done at the same time, and this does not
  // explicitly account for the block column and row structure in the
  // matrix. When using AMD, we have observed in the past that
  // computing the ordering with the block matrix is significantly
  // more efficient, both in runtime as well as the quality of
  // ordering computed. So, it maybe worth doing that analysis
  // separately.
  const SuiteSparse_long rank =
      SuiteSparseQR<double>(SPQR_ORDERING_BESTAMD,
                            SPQR_DEFAULT_TOL,
                            cholmod_jacobian.ncol,
                            &cholmod_jacobian,
                            &R,
                            &permutation,
                            &cc);
  event_logger.AddEvent("Numeric Factorization");
  CHECK_NOTNULL(permutation);
  CHECK_NOTNULL(R);

  if (rank < cholmod_jacobian.ncol) {
    LOG(ERROR) << "Jacobian matrix is rank deficient. "
               << "Number of columns: " << cholmod_jacobian.ncol
               << " rank: " << rank;
    free(permutation);
    cholmod_l_free_sparse(&R, &cc);
    cholmod_l_finish(&cc);
    return false;
  }

  vector<int> inverse_permutation(num_cols);
  for (SuiteSparse_long i = 0; i < num_cols; ++i) {
    inverse_permutation[permutation[i]] = i;
  }

  const int* rows = covariance_matrix_->rows();
  const int* cols = covariance_matrix_->cols();
  double* values = covariance_matrix_->mutable_values();

  // The following loop exploits the fact that the i^th column of A^{-1}
  // is given by the solution to the linear system
  //
  //  A x = e_i
  //
  // where e_i is a vector with e(i) = 1 and all other entries zero.
  //
  // Since the covariance matrix is symmetric, the i^th row and column
  // are equal.
  const int num_threads = options_.num_threads;
  scoped_array<double> workspace(new double[num_threads * num_cols]);

#pragma omp parallel for num_threads(num_threads) schedule(dynamic)
  for (int r = 0; r < num_cols; ++r) {
    const int row_begin = rows[r];
    const int row_end = rows[r + 1];
    if (row_end == row_begin) {
      continue;
    }

#  ifdef CERES_USE_OPENMP
    int thread_id = omp_get_thread_num();
#  else
    int thread_id = 0;
#  endif

    double* solution = workspace.get() + thread_id * num_cols;
    SolveRTRWithSparseRHS<SuiteSparse_long>(
        num_cols,
        static_cast<SuiteSparse_long*>(R->i),
        static_cast<SuiteSparse_long*>(R->p),
        static_cast<double*>(R->x),
        inverse_permutation[r],
        solution);
    for (int idx = row_begin; idx < row_end; ++idx) {
     const int c = cols[idx];
     values[idx] = solution[inverse_permutation[c]];
    }
  }

  free(permutation);
  cholmod_l_free_sparse(&R, &cc);
  cholmod_l_finish(&cc);
  event_logger.AddEvent("Inversion");
  return true;

#else  // CERES_NO_SUITESPARSE

  return false;

#endif  // CERES_NO_SUITESPARSE
}

bool CovarianceImpl::ComputeCovarianceValuesUsingDenseSVD() {
  EventLogger event_logger(
      "CovarianceImpl::ComputeCovarianceValuesUsingDenseSVD");
  if (covariance_matrix_.get() == NULL) {
    // Nothing to do, all zeros covariance matrix.
    return true;
  }

  CRSMatrix jacobian;
  problem_->Evaluate(evaluate_options_, NULL, NULL, NULL, &jacobian);
  event_logger.AddEvent("Evaluate");

  Matrix dense_jacobian(jacobian.num_rows, jacobian.num_cols);
  dense_jacobian.setZero();
  for (int r = 0; r < jacobian.num_rows; ++r) {
    for (int idx = jacobian.rows[r]; idx < jacobian.rows[r + 1]; ++idx) {
      const int c = jacobian.cols[idx];
      dense_jacobian(r, c) = jacobian.values[idx];
    }
  }
  event_logger.AddEvent("ConvertToDenseMatrix");

  Eigen::JacobiSVD<Matrix> svd(dense_jacobian,
                               Eigen::ComputeThinU | Eigen::ComputeThinV);

  event_logger.AddEvent("SingularValueDecomposition");

  const Vector singular_values = svd.singularValues();
  const int num_singular_values = singular_values.rows();
  Vector inverse_squared_singular_values(num_singular_values);
  inverse_squared_singular_values.setZero();

  const double max_singular_value = singular_values[0];
  const double min_singular_value_ratio =
      sqrt(options_.min_reciprocal_condition_number);

  const bool automatic_truncation = (options_.null_space_rank < 0);
  const int max_rank = std::min(num_singular_values,
                                num_singular_values - options_.null_space_rank);

  // Compute the squared inverse of the singular values. Truncate the
  // computation based on min_singular_value_ratio and
  // null_space_rank. When either of these two quantities are active,
  // the resulting covariance matrix is a Moore-Penrose inverse
  // instead of a regular inverse.
  for (int i = 0; i < max_rank; ++i) {
    const double singular_value_ratio = singular_values[i] / max_singular_value;
    if (singular_value_ratio < min_singular_value_ratio) {
      // Since the singular values are in decreasing order, if
      // automatic truncation is enabled, then from this point on
      // all values will fail the ratio test and there is nothing to
      // do in this loop.
      if (automatic_truncation) {
        break;
      } else {
        LOG(ERROR) << "Error: Covariance matrix is near rank deficient "
                   << "and the user did not specify a non-zero"
                   << "Covariance::Options::null_space_rank "
                   << "to enable the computation of a Pseudo-Inverse. "
                   << "Reciprocal condition number: "
                   << singular_value_ratio * singular_value_ratio << " "
                   << "min_reciprocal_condition_number: "
                   << options_.min_reciprocal_condition_number;
        return false;
      }
    }

    inverse_squared_singular_values[i] =
        1.0 / (singular_values[i] * singular_values[i]);
  }

  Matrix dense_covariance =
      svd.matrixV() *
      inverse_squared_singular_values.asDiagonal() *
      svd.matrixV().transpose();
  event_logger.AddEvent("PseudoInverse");

  const int num_rows = covariance_matrix_->num_rows();
  const int* rows = covariance_matrix_->rows();
  const int* cols = covariance_matrix_->cols();
  double* values = covariance_matrix_->mutable_values();

  for (int r = 0; r < num_rows; ++r) {
    for (int idx = rows[r]; idx < rows[r + 1]; ++idx) {
      const int c = cols[idx];
      values[idx] = dense_covariance(r, c);
    }
  }
  event_logger.AddEvent("CopyToCovarianceMatrix");
  return true;
}

bool CovarianceImpl::ComputeCovarianceValuesUsingEigenSparseQR() {
  EventLogger event_logger(
      "CovarianceImpl::ComputeCovarianceValuesUsingEigenSparseQR");
  if (covariance_matrix_.get() == NULL) {
    // Nothing to do, all zeros covariance matrix.
    return true;
  }

  CRSMatrix jacobian;
  problem_->Evaluate(evaluate_options_, NULL, NULL, NULL, &jacobian);
  event_logger.AddEvent("Evaluate");

  typedef Eigen::SparseMatrix<double, Eigen::ColMajor> EigenSparseMatrix;

  // Convert the matrix to column major order as required by SparseQR.
  EigenSparseMatrix sparse_jacobian =
      Eigen::MappedSparseMatrix<double, Eigen::RowMajor>(
          jacobian.num_rows, jacobian.num_cols,
          static_cast<int>(jacobian.values.size()),
          jacobian.rows.data(), jacobian.cols.data(), jacobian.values.data());
  event_logger.AddEvent("ConvertToSparseMatrix");

  Eigen::SparseQR<EigenSparseMatrix, Eigen::COLAMDOrdering<int> >
      qr_solver(sparse_jacobian);
  event_logger.AddEvent("QRDecomposition");

  if (qr_solver.info() != Eigen::Success) {
    LOG(ERROR) << "Eigen::SparseQR decomposition failed.";
    return false;
  }

  if (qr_solver.rank() < jacobian.num_cols) {
    LOG(ERROR) << "Jacobian matrix is rank deficient. "
               << "Number of columns: " << jacobian.num_cols
               << " rank: " << qr_solver.rank();
    return false;
  }

  const int* rows = covariance_matrix_->rows();
  const int* cols = covariance_matrix_->cols();
  double* values = covariance_matrix_->mutable_values();

  // Compute the inverse column permutation used by QR factorization.
  Eigen::PermutationMatrix<Eigen::Dynamic, Eigen::Dynamic> inverse_permutation =
      qr_solver.colsPermutation().inverse();

  // The following loop exploits the fact that the i^th column of A^{-1}
  // is given by the solution to the linear system
  //
  //  A x = e_i
  //
  // where e_i is a vector with e(i) = 1 and all other entries zero.
  //
  // Since the covariance matrix is symmetric, the i^th row and column
  // are equal.
  const int num_cols = jacobian.num_cols;
  const int num_threads = options_.num_threads;
  scoped_array<double> workspace(new double[num_threads * num_cols]);

#pragma omp parallel for num_threads(num_threads) schedule(dynamic)
  for (int r = 0; r < num_cols; ++r) {
    const int row_begin = rows[r];
    const int row_end = rows[r + 1];
    if (row_end == row_begin) {
      continue;
    }

#  ifdef CERES_USE_OPENMP
    int thread_id = omp_get_thread_num();
#  else
    int thread_id = 0;
#  endif

    double* solution = workspace.get() + thread_id * num_cols;
    SolveRTRWithSparseRHS<int>(
        num_cols,
        qr_solver.matrixR().innerIndexPtr(),
        qr_solver.matrixR().outerIndexPtr(),
        &qr_solver.matrixR().data().value(0),
        inverse_permutation.indices().coeff(r),
        solution);

    // Assign the values of the computed covariance using the
    // inverse permutation used in the QR factorization.
    for (int idx = row_begin; idx < row_end; ++idx) {
     const int c = cols[idx];
     values[idx] = solution[inverse_permutation.indices().coeff(c)];
    }
  }

  event_logger.AddEvent("Inverse");

  return true;
}

}  // namespace internal
}  // namespace ceres
