// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2023 Google Inc. All rights reserved.
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

#include "ceres/reorder_program.h"

#include <algorithm>
#include <map>
#include <memory>
#include <numeric>
#include <set>
#include <string>
#include <vector>

#include "Eigen/SparseCore"
#include "ceres/internal/config.h"
#include "ceres/internal/export.h"
#include "ceres/ordered_groups.h"
#include "ceres/parameter_block.h"
#include "ceres/parameter_block_ordering.h"
#include "ceres/problem_impl.h"
#include "ceres/program.h"
#include "ceres/residual_block.h"
#include "ceres/solver.h"
#include "ceres/suitesparse.h"
#include "ceres/triplet_sparse_matrix.h"
#include "ceres/types.h"

#ifdef CERES_USE_EIGEN_SPARSE

#ifndef CERES_NO_EIGEN_METIS
#include <iostream>  // Need this because MetisSupport refers to std::cerr.

#include "Eigen/MetisSupport"
#endif

#include "Eigen/OrderingMethods"
#endif

#include "glog/logging.h"

namespace ceres::internal {

namespace {

// Find the minimum index of any parameter block to the given
// residual.  Parameter blocks that have indices greater than
// size_of_first_elimination_group are considered to have an index
// equal to size_of_first_elimination_group.
static int MinParameterBlock(const ResidualBlock* residual_block,
                             int size_of_first_elimination_group) {
  int min_parameter_block_position = size_of_first_elimination_group;
  for (int i = 0; i < residual_block->NumParameterBlocks(); ++i) {
    ParameterBlock* parameter_block = residual_block->parameter_blocks()[i];
    if (!parameter_block->IsConstant()) {
      CHECK_NE(parameter_block->index(), -1)
          << "Did you forget to call Program::SetParameterOffsetsAndIndex()? "
          << "This is a Ceres bug; please contact the developers!";
      min_parameter_block_position =
          std::min(parameter_block->index(), min_parameter_block_position);
    }
  }
  return min_parameter_block_position;
}

Eigen::SparseMatrix<int> CreateBlockJacobian(
    const TripletSparseMatrix& block_jacobian_transpose) {
  using SparseMatrix = Eigen::SparseMatrix<int>;
  using Triplet = Eigen::Triplet<int>;

  const int* rows = block_jacobian_transpose.rows();
  const int* cols = block_jacobian_transpose.cols();
  int num_nonzeros = block_jacobian_transpose.num_nonzeros();
  std::vector<Triplet> triplets;
  triplets.reserve(num_nonzeros);
  for (int i = 0; i < num_nonzeros; ++i) {
    triplets.emplace_back(cols[i], rows[i], 1);
  }

  SparseMatrix block_jacobian(block_jacobian_transpose.num_cols(),
                              block_jacobian_transpose.num_rows());
  block_jacobian.setFromTriplets(triplets.begin(), triplets.end());
  return block_jacobian;
}

void OrderingForSparseNormalCholeskyUsingSuiteSparse(
    const LinearSolverOrderingType linear_solver_ordering_type,
    const TripletSparseMatrix& tsm_block_jacobian_transpose,
    const std::vector<ParameterBlock*>& parameter_blocks,
    const ParameterBlockOrdering& parameter_block_ordering,
    int* ordering) {
#ifdef CERES_NO_SUITESPARSE
  // "Void"ing values to avoid compiler warnings about unused parameters
  (void)linear_solver_ordering_type;
  (void)tsm_block_jacobian_transpose;
  (void)parameter_blocks;
  (void)parameter_block_ordering;
  (void)ordering;
  LOG(FATAL) << "Congratulations, you found a Ceres bug! "
             << "Please report this error to the developers.";
#else
  SuiteSparse ss;
  cholmod_sparse* block_jacobian_transpose = ss.CreateSparseMatrix(
      const_cast<TripletSparseMatrix*>(&tsm_block_jacobian_transpose));

  if (linear_solver_ordering_type == ceres::AMD) {
    if (parameter_block_ordering.NumGroups() <= 1) {
      // The user did not supply a useful ordering so just go ahead
      // and use AMD.
      ss.Ordering(block_jacobian_transpose, OrderingType::AMD, ordering);
    } else {
      // The user supplied an ordering, so use CAMD.
      std::vector<int> constraints;
      constraints.reserve(parameter_blocks.size());
      for (auto* parameter_block : parameter_blocks) {
        constraints.push_back(parameter_block_ordering.GroupId(
            parameter_block->mutable_user_state()));
      }

      // Renumber the entries of constraints to be contiguous integers
      // as CAMD requires that the group ids be in the range [0,
      // parameter_blocks.size() - 1].
      MapValuesToContiguousRange(constraints.size(), constraints.data());
      ss.ConstrainedApproximateMinimumDegreeOrdering(
          block_jacobian_transpose, constraints.data(), ordering);
    }
  } else if (linear_solver_ordering_type == ceres::NESDIS) {
    // If nested dissection is chosen as an ordering algorithm, then
    // ignore any user provided linear_solver_ordering.
    CHECK(SuiteSparse::IsNestedDissectionAvailable())
        << "Congratulations, you found a Ceres bug! "
        << "Please report this error to the developers.";
    ss.Ordering(block_jacobian_transpose, OrderingType::NESDIS, ordering);
  } else {
    LOG(FATAL) << "Congratulations, you found a Ceres bug! "
               << "Please report this error to the developers.";
  }

  ss.Free(block_jacobian_transpose);
#endif  // CERES_NO_SUITESPARSE
}

void OrderingForSparseNormalCholeskyUsingEigenSparse(
    const LinearSolverOrderingType linear_solver_ordering_type,
    const TripletSparseMatrix& tsm_block_jacobian_transpose,
    int* ordering) {
#ifndef CERES_USE_EIGEN_SPARSE
  LOG(FATAL) << "SPARSE_NORMAL_CHOLESKY cannot be used with EIGEN_SPARSE "
                "because Ceres was not built with support for "
                "Eigen's SimplicialLDLT decomposition. "
                "This requires enabling building with -DEIGENSPARSE=ON.";
#else

  // TODO(sameeragarwal): This conversion from a TripletSparseMatrix
  // to a Eigen::Triplet matrix is unfortunate, but unavoidable for
  // now. It is not a significant performance penalty in the grand
  // scheme of things. The right thing to do here would be to get a
  // compressed row sparse matrix representation of the jacobian and
  // go from there. But that is a project for another day.
  using SparseMatrix = Eigen::SparseMatrix<int>;

  const SparseMatrix block_jacobian =
      CreateBlockJacobian(tsm_block_jacobian_transpose);
  const SparseMatrix block_hessian =
      block_jacobian.transpose() * block_jacobian;

  Eigen::PermutationMatrix<Eigen::Dynamic, Eigen::Dynamic, int> perm;
  if (linear_solver_ordering_type == ceres::AMD) {
    Eigen::AMDOrdering<int> amd_ordering;
    amd_ordering(block_hessian, perm);
  } else {
#ifndef CERES_NO_EIGEN_METIS
    Eigen::MetisOrdering<int> metis_ordering;
    metis_ordering(block_hessian, perm);
#else
    perm.setIdentity(block_hessian.rows());
#endif
  }

  for (int i = 0; i < block_hessian.rows(); ++i) {
    ordering[i] = perm.indices()[i];
  }
#endif  // CERES_USE_EIGEN_SPARSE
}

}  // namespace

bool ApplyOrdering(const ProblemImpl::ParameterMap& parameter_map,
                   const ParameterBlockOrdering& ordering,
                   Program* program,
                   std::string* error) {
  const int num_parameter_blocks = program->NumParameterBlocks();
  if (ordering.NumElements() != num_parameter_blocks) {
    *error = StringPrintf(
        "User specified ordering does not have the same "
        "number of parameters as the problem. The problem"
        "has %d blocks while the ordering has %d blocks.",
        num_parameter_blocks,
        ordering.NumElements());
    return false;
  }

  std::vector<ParameterBlock*>* parameter_blocks =
      program->mutable_parameter_blocks();
  parameter_blocks->clear();

  // TODO(sameeragarwal): Investigate whether this should be a set or an
  // unordered_set.
  const std::map<int, std::set<double*>>& groups = ordering.group_to_elements();
  for (const auto& p : groups) {
    const std::set<double*>& group = p.second;
    for (double* parameter_block_ptr : group) {
      auto it = parameter_map.find(parameter_block_ptr);
      if (it == parameter_map.end()) {
        *error = StringPrintf(
            "User specified ordering contains a pointer "
            "to a double that is not a parameter block in "
            "the problem. The invalid double is in group: %d",
            p.first);
        return false;
      }
      parameter_blocks->push_back(it->second);
    }
  }
  return true;
}

bool LexicographicallyOrderResidualBlocks(
    const int size_of_first_elimination_group,
    Program* program,
    std::string* /*error*/) {
  CHECK_GE(size_of_first_elimination_group, 1)
      << "Congratulations, you found a Ceres bug! Please report this error "
      << "to the developers.";

  // Create a histogram of the number of residuals for each E block. There is an
  // extra bucket at the end to catch all non-eliminated F blocks.
  std::vector<int> residual_blocks_per_e_block(size_of_first_elimination_group +
                                               1);
  std::vector<ResidualBlock*>* residual_blocks =
      program->mutable_residual_blocks();
  std::vector<int> min_position_per_residual(residual_blocks->size());
  for (int i = 0; i < residual_blocks->size(); ++i) {
    ResidualBlock* residual_block = (*residual_blocks)[i];
    int position =
        MinParameterBlock(residual_block, size_of_first_elimination_group);
    min_position_per_residual[i] = position;
    DCHECK_LE(position, size_of_first_elimination_group);
    residual_blocks_per_e_block[position]++;
  }

  // Run a cumulative sum on the histogram, to obtain offsets to the start of
  // each histogram bucket (where each bucket is for the residuals for that
  // E-block).
  std::vector<int> offsets(size_of_first_elimination_group + 1);
  std::partial_sum(residual_blocks_per_e_block.begin(),
                   residual_blocks_per_e_block.end(),
                   offsets.begin());
  CHECK_EQ(offsets.back(), residual_blocks->size())
      << "Congratulations, you found a Ceres bug! Please report this error "
      << "to the developers.";

  CHECK(find(residual_blocks_per_e_block.begin(),
             residual_blocks_per_e_block.end() - 1,
             0) == residual_blocks_per_e_block.end() - 1)
      << "Congratulations, you found a Ceres bug! Please report this error "
      << "to the developers.";

  // Fill in each bucket with the residual blocks for its corresponding E block.
  // Each bucket is individually filled from the back of the bucket to the front
  // of the bucket. The filling order among the buckets is dictated by the
  // residual blocks. This loop uses the offsets as counters; subtracting one
  // from each offset as a residual block is placed in the bucket. When the
  // filling is finished, the offset pointers should have shifted down one
  // entry (this is verified below).
  std::vector<ResidualBlock*> reordered_residual_blocks(
      (*residual_blocks).size(), static_cast<ResidualBlock*>(nullptr));
  for (int i = 0; i < residual_blocks->size(); ++i) {
    int bucket = min_position_per_residual[i];

    // Decrement the cursor, which should now point at the next empty position.
    offsets[bucket]--;

    // Sanity.
    CHECK(reordered_residual_blocks[offsets[bucket]] == nullptr)
        << "Congratulations, you found a Ceres bug! Please report this error "
        << "to the developers.";

    reordered_residual_blocks[offsets[bucket]] = (*residual_blocks)[i];
  }

  // Sanity check #1: The difference in bucket offsets should match the
  // histogram sizes.
  for (int i = 0; i < size_of_first_elimination_group; ++i) {
    CHECK_EQ(residual_blocks_per_e_block[i], offsets[i + 1] - offsets[i])
        << "Congratulations, you found a Ceres bug! Please report this error "
        << "to the developers.";
  }
  // Sanity check #2: No nullptr's left behind.
  for (auto* residual_block : reordered_residual_blocks) {
    CHECK(residual_block != nullptr)
        << "Congratulations, you found a Ceres bug! Please report this error "
        << "to the developers.";
  }

  // Now that the residuals are collected by E block, swap them in place.
  swap(*program->mutable_residual_blocks(), reordered_residual_blocks);
  return true;
}

// Pre-order the columns corresponding to the Schur complement if
// possible.
static void ReorderSchurComplementColumnsUsingSuiteSparse(
    const ParameterBlockOrdering& parameter_block_ordering, Program* program) {
#ifdef CERES_NO_SUITESPARSE
  // "Void"ing values to avoid compiler warnings about unused parameters
  (void)parameter_block_ordering;
  (void)program;
#else
  SuiteSparse ss;
  std::vector<int> constraints;
  std::vector<ParameterBlock*>& parameter_blocks =
      *(program->mutable_parameter_blocks());

  for (auto* parameter_block : parameter_blocks) {
    constraints.push_back(parameter_block_ordering.GroupId(
        parameter_block->mutable_user_state()));
  }

  // Renumber the entries of constraints to be contiguous integers as
  // CAMD requires that the group ids be in the range [0,
  // parameter_blocks.size() - 1].
  MapValuesToContiguousRange(constraints.size(), constraints.data());

  // Compute a block sparse presentation of J'.
  std::unique_ptr<TripletSparseMatrix> tsm_block_jacobian_transpose(
      program->CreateJacobianBlockSparsityTranspose());

  cholmod_sparse* block_jacobian_transpose =
      ss.CreateSparseMatrix(tsm_block_jacobian_transpose.get());

  std::vector<int> ordering(parameter_blocks.size(), 0);
  ss.ConstrainedApproximateMinimumDegreeOrdering(
      block_jacobian_transpose, constraints.data(), ordering.data());
  ss.Free(block_jacobian_transpose);

  const std::vector<ParameterBlock*> parameter_blocks_copy(parameter_blocks);
  for (int i = 0; i < program->NumParameterBlocks(); ++i) {
    parameter_blocks[i] = parameter_blocks_copy[ordering[i]];
  }

  program->SetParameterOffsetsAndIndex();
#endif
}

static void ReorderSchurComplementColumnsUsingEigen(
    LinearSolverOrderingType ordering_type,
    const int size_of_first_elimination_group,
    const ProblemImpl::ParameterMap& /*parameter_map*/,
    Program* program) {
#if defined(CERES_USE_EIGEN_SPARSE)
  std::unique_ptr<TripletSparseMatrix> tsm_block_jacobian_transpose(
      program->CreateJacobianBlockSparsityTranspose());
  using SparseMatrix = Eigen::SparseMatrix<int>;
  const SparseMatrix block_jacobian =
      CreateBlockJacobian(*tsm_block_jacobian_transpose);
  const int num_rows = block_jacobian.rows();
  const int num_cols = block_jacobian.cols();

  // Vertically partition the jacobian in parameter blocks of type E
  // and F.
  const SparseMatrix E =
      block_jacobian.block(0, 0, num_rows, size_of_first_elimination_group);
  const SparseMatrix F =
      block_jacobian.block(0,
                           size_of_first_elimination_group,
                           num_rows,
                           num_cols - size_of_first_elimination_group);

  // Block sparsity pattern of the schur complement.
  const SparseMatrix block_schur_complement =
      F.transpose() * F - F.transpose() * E * E.transpose() * F;

  Eigen::PermutationMatrix<Eigen::Dynamic, Eigen::Dynamic, int> perm;
  if (ordering_type == ceres::AMD) {
    Eigen::AMDOrdering<int> amd_ordering;
    amd_ordering(block_schur_complement, perm);
  } else {
#ifndef CERES_NO_EIGEN_METIS
    Eigen::MetisOrdering<int> metis_ordering;
    metis_ordering(block_schur_complement, perm);
#else
    perm.setIdentity(block_schur_complement.rows());
#endif
  }

  const std::vector<ParameterBlock*>& parameter_blocks =
      program->parameter_blocks();
  std::vector<ParameterBlock*> ordering(num_cols);

  // The ordering of the first size_of_first_elimination_group does
  // not matter, so we preserve the existing ordering.
  for (int i = 0; i < size_of_first_elimination_group; ++i) {
    ordering[i] = parameter_blocks[i];
  }

  // For the rest of the blocks, use the ordering computed using AMD.
  for (int i = 0; i < block_schur_complement.cols(); ++i) {
    ordering[size_of_first_elimination_group + i] =
        parameter_blocks[size_of_first_elimination_group + perm.indices()[i]];
  }

  swap(*program->mutable_parameter_blocks(), ordering);
  program->SetParameterOffsetsAndIndex();
#endif
}

bool ReorderProgramForSchurTypeLinearSolver(
    const LinearSolverType linear_solver_type,
    const SparseLinearAlgebraLibraryType sparse_linear_algebra_library_type,
    const LinearSolverOrderingType linear_solver_ordering_type,
    const ProblemImpl::ParameterMap& parameter_map,
    ParameterBlockOrdering* parameter_block_ordering,
    Program* program,
    std::string* error) {
  if (parameter_block_ordering->NumElements() !=
      program->NumParameterBlocks()) {
    *error = StringPrintf(
        "The program has %d parameter blocks, but the parameter block "
        "ordering has %d parameter blocks.",
        program->NumParameterBlocks(),
        parameter_block_ordering->NumElements());
    return false;
  }

  if (parameter_block_ordering->NumGroups() == 1) {
    // If the user supplied an parameter_block_ordering with just one
    // group, it is equivalent to the user supplying nullptr as an
    // parameter_block_ordering. Ceres is completely free to choose the
    // parameter block ordering as it sees fit. For Schur type solvers,
    // this means that the user wishes for Ceres to identify the
    // e_blocks, which we do by computing a maximal independent set.
    std::vector<ParameterBlock*> schur_ordering;
    const int size_of_first_elimination_group =
        ComputeStableSchurOrdering(*program, &schur_ordering);

    CHECK_EQ(schur_ordering.size(), program->NumParameterBlocks())
        << "Congratulations, you found a Ceres bug! Please report this error "
        << "to the developers.";

    // Update the parameter_block_ordering object.
    for (int i = 0; i < schur_ordering.size(); ++i) {
      double* parameter_block = schur_ordering[i]->mutable_user_state();
      const int group_id = (i < size_of_first_elimination_group) ? 0 : 1;
      parameter_block_ordering->AddElementToGroup(parameter_block, group_id);
    }

    // We could call ApplyOrdering but this is cheaper and
    // simpler.
    swap(*program->mutable_parameter_blocks(), schur_ordering);
  } else {
    // The user provided an ordering with more than one elimination
    // group.

    // Verify that the first elimination group is an independent set.

    // TODO(sameeragarwal): Investigate if this should be a set or an
    // unordered_set.
    const std::set<double*>& first_elimination_group =
        parameter_block_ordering->group_to_elements().begin()->second;
    if (!program->IsParameterBlockSetIndependent(first_elimination_group)) {
      *error = StringPrintf(
          "The first elimination group in the parameter block "
          "ordering of size %zd is not an independent set",
          first_elimination_group.size());
      return false;
    }

    if (!ApplyOrdering(
            parameter_map, *parameter_block_ordering, program, error)) {
      return false;
    }
  }

  program->SetParameterOffsetsAndIndex();

  const int size_of_first_elimination_group =
      parameter_block_ordering->group_to_elements().begin()->second.size();

  if (linear_solver_type == SPARSE_SCHUR) {
    if (sparse_linear_algebra_library_type == SUITE_SPARSE &&
        linear_solver_ordering_type == ceres::AMD) {
      // Preordering support for schur complement only works with AMD
      // for now, since we are using CAMD.
      //
      // TODO(sameeragarwal): It maybe worth adding pre-ordering support for
      // nested dissection too.
      ReorderSchurComplementColumnsUsingSuiteSparse(*parameter_block_ordering,
                                                    program);
    } else if (sparse_linear_algebra_library_type == EIGEN_SPARSE) {
      ReorderSchurComplementColumnsUsingEigen(linear_solver_ordering_type,
                                              size_of_first_elimination_group,
                                              parameter_map,
                                              program);
    }
  }

  // Schur type solvers also require that their residual blocks be
  // lexicographically ordered.
  return LexicographicallyOrderResidualBlocks(
      size_of_first_elimination_group, program, error);
}

bool ReorderProgramForSparseCholesky(
    const SparseLinearAlgebraLibraryType sparse_linear_algebra_library_type,
    const LinearSolverOrderingType linear_solver_ordering_type,
    const ParameterBlockOrdering& parameter_block_ordering,
    int start_row_block,
    Program* program,
    std::string* error) {
  if (parameter_block_ordering.NumElements() != program->NumParameterBlocks()) {
    *error = StringPrintf(
        "The program has %d parameter blocks, but the parameter block "
        "ordering has %d parameter blocks.",
        program->NumParameterBlocks(),
        parameter_block_ordering.NumElements());
    return false;
  }

  // Compute a block sparse presentation of J'.
  std::unique_ptr<TripletSparseMatrix> tsm_block_jacobian_transpose(
      program->CreateJacobianBlockSparsityTranspose(start_row_block));

  std::vector<int> ordering(program->NumParameterBlocks(), 0);
  std::vector<ParameterBlock*>& parameter_blocks =
      *(program->mutable_parameter_blocks());

  if (sparse_linear_algebra_library_type == SUITE_SPARSE) {
    OrderingForSparseNormalCholeskyUsingSuiteSparse(
        linear_solver_ordering_type,
        *tsm_block_jacobian_transpose,
        parameter_blocks,
        parameter_block_ordering,
        ordering.data());
  } else if (sparse_linear_algebra_library_type == ACCELERATE_SPARSE) {
    // Accelerate does not provide a function to perform reordering without
    // performing a full symbolic factorisation.  As such, we have nothing
    // to gain from trying to reorder the problem here, as it will happen
    // in AppleAccelerateCholesky::Factorize() (once) and reordering here
    // would involve performing two symbolic factorisations instead of one
    // which would have a negative overall impact on performance.
    return true;

  } else if (sparse_linear_algebra_library_type == EIGEN_SPARSE) {
    OrderingForSparseNormalCholeskyUsingEigenSparse(
        linear_solver_ordering_type,
        *tsm_block_jacobian_transpose,
        ordering.data());
  }

  // Apply ordering.
  const std::vector<ParameterBlock*> parameter_blocks_copy(parameter_blocks);
  for (int i = 0; i < program->NumParameterBlocks(); ++i) {
    parameter_blocks[i] = parameter_blocks_copy[ordering[i]];
  }

  program->SetParameterOffsetsAndIndex();
  return true;
}

int ReorderResidualBlocksByPartition(
    const std::unordered_set<ResidualBlockId>& bottom_residual_blocks,
    Program* program) {
  auto residual_blocks = program->mutable_residual_blocks();
  auto it = std::partition(residual_blocks->begin(),
                           residual_blocks->end(),
                           [&bottom_residual_blocks](ResidualBlock* r) {
                             return bottom_residual_blocks.count(r) == 0;
                           });
  return it - residual_blocks->begin();
}

bool AreJacobianColumnsOrdered(
    const LinearSolverType linear_solver_type,
    const PreconditionerType preconditioner_type,
    const SparseLinearAlgebraLibraryType sparse_linear_algebra_library_type,
    const LinearSolverOrderingType linear_solver_ordering_type) {
  if (sparse_linear_algebra_library_type == SUITE_SPARSE) {
    if (linear_solver_type == SPARSE_NORMAL_CHOLESKY ||
        (linear_solver_type == CGNR && preconditioner_type == SUBSET)) {
      return true;
    }
    if (linear_solver_type == SPARSE_SCHUR &&
        linear_solver_ordering_type == ceres::AMD) {
      return true;
    }
    return false;
  }

  if (sparse_linear_algebra_library_type == ceres::EIGEN_SPARSE) {
    if (linear_solver_type == SPARSE_NORMAL_CHOLESKY ||
        linear_solver_type == SPARSE_SCHUR ||
        (linear_solver_type == CGNR && preconditioner_type == SUBSET)) {
      return true;
    }
    return false;
  }

  if (sparse_linear_algebra_library_type == ceres::ACCELERATE_SPARSE) {
    // Apple's accelerate framework does not allow direct access to
    // ordering algorithms, so jacobian columns are never pre-ordered.
    return false;
  }

  return false;
}

}  // namespace ceres::internal
