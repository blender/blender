// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2010, 2011, 2012 Google Inc. All rights reserved.
// http://code.google.com/p/ceres-solver/
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

#ifndef CERES_NO_SUITESPARSE

#include "ceres/visibility_based_preconditioner.h"

#include <algorithm>
#include <functional>
#include <iterator>
#include <set>
#include <utility>
#include <vector>
#include "Eigen/Dense"
#include "ceres/block_random_access_sparse_matrix.h"
#include "ceres/block_sparse_matrix.h"
#include "ceres/canonical_views_clustering.h"
#include "ceres/collections_port.h"
#include "ceres/graph.h"
#include "ceres/graph_algorithms.h"
#include "ceres/internal/scoped_ptr.h"
#include "ceres/linear_solver.h"
#include "ceres/schur_eliminator.h"
#include "ceres/single_linkage_clustering.h"
#include "ceres/visibility.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {

// TODO(sameeragarwal): Currently these are magic weights for the
// preconditioner construction. Move these higher up into the Options
// struct and provide some guidelines for choosing them.
//
// This will require some more work on the clustering algorithm and
// possibly some more refactoring of the code.
static const double kCanonicalViewsSizePenaltyWeight = 3.0;
static const double kCanonicalViewsSimilarityPenaltyWeight = 0.0;
static const double kSingleLinkageMinSimilarity = 0.9;

VisibilityBasedPreconditioner::VisibilityBasedPreconditioner(
    const CompressedRowBlockStructure& bs,
    const Preconditioner::Options& options)
    : options_(options),
      num_blocks_(0),
      num_clusters_(0),
      factor_(NULL) {
  CHECK_GT(options_.elimination_groups.size(), 1);
  CHECK_GT(options_.elimination_groups[0], 0);
  CHECK(options_.type == CLUSTER_JACOBI ||
        options_.type == CLUSTER_TRIDIAGONAL)
      << "Unknown preconditioner type: " << options_.type;
  num_blocks_ = bs.cols.size() - options_.elimination_groups[0];
  CHECK_GT(num_blocks_, 0)
      << "Jacobian should have atleast 1 f_block for "
      << "visibility based preconditioning.";

  // Vector of camera block sizes
  block_size_.resize(num_blocks_);
  for (int i = 0; i < num_blocks_; ++i) {
    block_size_[i] = bs.cols[i + options_.elimination_groups[0]].size;
  }

  const time_t start_time = time(NULL);
  switch (options_.type) {
    case CLUSTER_JACOBI:
      ComputeClusterJacobiSparsity(bs);
      break;
    case CLUSTER_TRIDIAGONAL:
      ComputeClusterTridiagonalSparsity(bs);
      break;
    default:
      LOG(FATAL) << "Unknown preconditioner type";
  }
  const time_t structure_time = time(NULL);
  InitStorage(bs);
  const time_t storage_time = time(NULL);
  InitEliminator(bs);
  const time_t eliminator_time = time(NULL);

  // Allocate temporary storage for a vector used during
  // RightMultiply.
  tmp_rhs_ = CHECK_NOTNULL(ss_.CreateDenseVector(NULL,
                                                 m_->num_rows(),
                                                 m_->num_rows()));
  const time_t init_time = time(NULL);
  VLOG(2) << "init time: "
          << init_time - start_time
          << " structure time: " << structure_time - start_time
          << " storage time:" << storage_time - structure_time
          << " eliminator time: " << eliminator_time - storage_time;
}

VisibilityBasedPreconditioner::~VisibilityBasedPreconditioner() {
  if (factor_ != NULL) {
    ss_.Free(factor_);
    factor_ = NULL;
  }
  if (tmp_rhs_ != NULL) {
    ss_.Free(tmp_rhs_);
    tmp_rhs_ = NULL;
  }
}

// Determine the sparsity structure of the CLUSTER_JACOBI
// preconditioner. It clusters cameras using their scene
// visibility. The clusters form the diagonal blocks of the
// preconditioner matrix.
void VisibilityBasedPreconditioner::ComputeClusterJacobiSparsity(
    const CompressedRowBlockStructure& bs) {
  vector<set<int> > visibility;
  ComputeVisibility(bs, options_.elimination_groups[0], &visibility);
  CHECK_EQ(num_blocks_, visibility.size());
  ClusterCameras(visibility);
  cluster_pairs_.clear();
  for (int i = 0; i < num_clusters_; ++i) {
    cluster_pairs_.insert(make_pair(i, i));
  }
}

// Determine the sparsity structure of the CLUSTER_TRIDIAGONAL
// preconditioner. It clusters cameras using using the scene
// visibility and then finds the strongly interacting pairs of
// clusters by constructing another graph with the clusters as
// vertices and approximating it with a degree-2 maximum spanning
// forest. The set of edges in this forest are the cluster pairs.
void VisibilityBasedPreconditioner::ComputeClusterTridiagonalSparsity(
    const CompressedRowBlockStructure& bs) {
  vector<set<int> > visibility;
  ComputeVisibility(bs, options_.elimination_groups[0], &visibility);
  CHECK_EQ(num_blocks_, visibility.size());
  ClusterCameras(visibility);

  // Construct a weighted graph on the set of clusters, where the
  // edges are the number of 3D points/e_blocks visible in both the
  // clusters at the ends of the edge. Return an approximate degree-2
  // maximum spanning forest of this graph.
  vector<set<int> > cluster_visibility;
  ComputeClusterVisibility(visibility, &cluster_visibility);
  scoped_ptr<Graph<int> > cluster_graph(
      CHECK_NOTNULL(CreateClusterGraph(cluster_visibility)));
  scoped_ptr<Graph<int> > forest(
      CHECK_NOTNULL(Degree2MaximumSpanningForest(*cluster_graph)));
  ForestToClusterPairs(*forest, &cluster_pairs_);
}

// Allocate storage for the preconditioner matrix.
void VisibilityBasedPreconditioner::InitStorage(
    const CompressedRowBlockStructure& bs) {
  ComputeBlockPairsInPreconditioner(bs);
  m_.reset(new BlockRandomAccessSparseMatrix(block_size_, block_pairs_));
}

// Call the canonical views algorithm and cluster the cameras based on
// their visibility sets. The visibility set of a camera is the set of
// e_blocks/3D points in the scene that are seen by it.
//
// The cluster_membership_ vector is updated to indicate cluster
// memberships for each camera block.
void VisibilityBasedPreconditioner::ClusterCameras(
    const vector<set<int> >& visibility) {
  scoped_ptr<Graph<int> > schur_complement_graph(
      CHECK_NOTNULL(CreateSchurComplementGraph(visibility)));

  HashMap<int, int> membership;

  if (options_.visibility_clustering_type == CANONICAL_VIEWS) {
    vector<int> centers;
    CanonicalViewsClusteringOptions clustering_options;
    clustering_options.size_penalty_weight =
        kCanonicalViewsSizePenaltyWeight;
    clustering_options.similarity_penalty_weight =
        kCanonicalViewsSimilarityPenaltyWeight;
    ComputeCanonicalViewsClustering(clustering_options,
                                    *schur_complement_graph,
                                    &centers,
                                    &membership);
    num_clusters_ = centers.size();
  } else if (options_.visibility_clustering_type == SINGLE_LINKAGE) {
    SingleLinkageClusteringOptions clustering_options;
    clustering_options.min_similarity =
        kSingleLinkageMinSimilarity;
    num_clusters_ = ComputeSingleLinkageClustering(clustering_options,
                                                   *schur_complement_graph,
                                                   &membership);
  } else {
    LOG(FATAL) << "Unknown visibility clustering algorithm.";
  }

  CHECK_GT(num_clusters_, 0);
  VLOG(2) << "num_clusters: " << num_clusters_;
  FlattenMembershipMap(membership, &cluster_membership_);
}

// Compute the block sparsity structure of the Schur complement
// matrix. For each pair of cameras contributing a non-zero cell to
// the schur complement, determine if that cell is present in the
// preconditioner or not.
//
// A pair of cameras contribute a cell to the preconditioner if they
// are part of the same cluster or if the the two clusters that they
// belong have an edge connecting them in the degree-2 maximum
// spanning forest.
//
// For example, a camera pair (i,j) where i belonges to cluster1 and
// j belongs to cluster2 (assume that cluster1 < cluster2).
//
// The cell corresponding to (i,j) is present in the preconditioner
// if cluster1 == cluster2 or the pair (cluster1, cluster2) were
// connected by an edge in the degree-2 maximum spanning forest.
//
// Since we have already expanded the forest into a set of camera
// pairs/edges, including self edges, the check can be reduced to
// checking membership of (cluster1, cluster2) in cluster_pairs_.
void VisibilityBasedPreconditioner::ComputeBlockPairsInPreconditioner(
    const CompressedRowBlockStructure& bs) {
  block_pairs_.clear();
  for (int i = 0; i < num_blocks_; ++i) {
    block_pairs_.insert(make_pair(i, i));
  }

  int r = 0;
  const int num_row_blocks = bs.rows.size();
  const int num_eliminate_blocks = options_.elimination_groups[0];

  // Iterate over each row of the matrix. The block structure of the
  // matrix is assumed to be sorted in order of the e_blocks/point
  // blocks. Thus all row blocks containing an e_block/point occur
  // contiguously. Further, if present, an e_block is always the first
  // parameter block in each row block.  These structural assumptions
  // are common to all Schur complement based solvers in Ceres.
  //
  // For each e_block/point block we identify the set of cameras
  // seeing it. The cross product of this set with itself is the set
  // of non-zero cells contibuted by this e_block.
  //
  // The time complexity of this is O(nm^2) where, n is the number of
  // 3d points and m is the maximum number of cameras seeing any
  // point, which for most scenes is a fairly small number.
  while (r < num_row_blocks) {
    int e_block_id = bs.rows[r].cells.front().block_id;
    if (e_block_id >= num_eliminate_blocks) {
      // Skip the rows whose first block is an f_block.
      break;
    }

    set<int> f_blocks;
    for (; r < num_row_blocks; ++r) {
      const CompressedRow& row = bs.rows[r];
      if (row.cells.front().block_id != e_block_id) {
        break;
      }

      // Iterate over the blocks in the row, ignoring the first block
      // since it is the one to be eliminated and adding the rest to
      // the list of f_blocks associated with this e_block.
      for (int c = 1; c < row.cells.size(); ++c) {
        const Cell& cell = row.cells[c];
        const int f_block_id = cell.block_id - num_eliminate_blocks;
        CHECK_GE(f_block_id, 0);
        f_blocks.insert(f_block_id);
      }
    }

    for (set<int>::const_iterator block1 = f_blocks.begin();
         block1 != f_blocks.end();
         ++block1) {
      set<int>::const_iterator block2 = block1;
      ++block2;
      for (; block2 != f_blocks.end(); ++block2) {
        if (IsBlockPairInPreconditioner(*block1, *block2)) {
          block_pairs_.insert(make_pair(*block1, *block2));
        }
      }
    }
  }

  // The remaining rows which do not contain any e_blocks.
  for (; r < num_row_blocks; ++r) {
    const CompressedRow& row = bs.rows[r];
    CHECK_GE(row.cells.front().block_id, num_eliminate_blocks);
    for (int i = 0; i < row.cells.size(); ++i) {
      const int block1 = row.cells[i].block_id - num_eliminate_blocks;
      for (int j = 0; j < row.cells.size(); ++j) {
        const int block2 = row.cells[j].block_id - num_eliminate_blocks;
        if (block1 <= block2) {
          if (IsBlockPairInPreconditioner(block1, block2)) {
            block_pairs_.insert(make_pair(block1, block2));
          }
        }
      }
    }
  }

  VLOG(1) << "Block pair stats: " << block_pairs_.size();
}

// Initialize the SchurEliminator.
void VisibilityBasedPreconditioner::InitEliminator(
    const CompressedRowBlockStructure& bs) {
  LinearSolver::Options eliminator_options;
  eliminator_options.elimination_groups = options_.elimination_groups;
  eliminator_options.num_threads = options_.num_threads;
  eliminator_options.e_block_size = options_.e_block_size;
  eliminator_options.f_block_size = options_.f_block_size;
  eliminator_options.row_block_size = options_.row_block_size;
  eliminator_.reset(SchurEliminatorBase::Create(eliminator_options));
  eliminator_->Init(eliminator_options.elimination_groups[0], &bs);
}

// Update the values of the preconditioner matrix and factorize it.
bool VisibilityBasedPreconditioner::UpdateImpl(const BlockSparseMatrix& A,
                                               const double* D) {
  const time_t start_time = time(NULL);
  const int num_rows = m_->num_rows();
  CHECK_GT(num_rows, 0);

  // We need a dummy rhs vector and a dummy b vector since the Schur
  // eliminator combines the computation of the reduced camera matrix
  // with the computation of the right hand side of that linear
  // system.
  //
  // TODO(sameeragarwal): Perhaps its worth refactoring the
  // SchurEliminator::Eliminate function to allow NULL for the rhs. As
  // of now it does not seem to be worth the effort.
  Vector rhs = Vector::Zero(m_->num_rows());
  Vector b = Vector::Zero(A.num_rows());

  // Compute a subset of the entries of the Schur complement.
  eliminator_->Eliminate(&A, b.data(), D, m_.get(), rhs.data());

  // Try factorizing the matrix. For CLUSTER_JACOBI, this should
  // always succeed modulo some numerical/conditioning problems. For
  // CLUSTER_TRIDIAGONAL, in general the preconditioner matrix as
  // constructed is not positive definite. However, we will go ahead
  // and try factorizing it. If it works, great, otherwise we scale
  // all the cells in the preconditioner corresponding to the edges in
  // the degree-2 forest and that guarantees positive
  // definiteness. The proof of this fact can be found in Lemma 1 in
  // "Visibility Based Preconditioning for Bundle Adjustment".
  //
  // Doing the factorization like this saves us matrix mass when
  // scaling is not needed, which is quite often in our experience.
  LinearSolverTerminationType status = Factorize();

  if (status == LINEAR_SOLVER_FATAL_ERROR) {
    return false;
  }

  // The scaling only affects the tri-diagonal case, since
  // ScaleOffDiagonalBlocks only pays attenion to the cells that
  // belong to the edges of the degree-2 forest. In the CLUSTER_JACOBI
  // case, the preconditioner is guaranteed to be positive
  // semidefinite.
  if (status == LINEAR_SOLVER_FAILURE && options_.type == CLUSTER_TRIDIAGONAL) {
    VLOG(1) << "Unscaled factorization failed. Retrying with off-diagonal "
            << "scaling";
    ScaleOffDiagonalCells();
    status = Factorize();
  }

  VLOG(2) << "Compute time: " << time(NULL) - start_time;
  return (status == LINEAR_SOLVER_SUCCESS);
}

// Consider the preconditioner matrix as meta-block matrix, whose
// blocks correspond to the clusters. Then cluster pairs corresponding
// to edges in the degree-2 forest are off diagonal entries of this
// matrix. Scaling these off-diagonal entries by 1/2 forces this
// matrix to be positive definite.
void VisibilityBasedPreconditioner::ScaleOffDiagonalCells() {
  for (set< pair<int, int> >::const_iterator it = block_pairs_.begin();
       it != block_pairs_.end();
       ++it) {
    const int block1 = it->first;
    const int block2 = it->second;
    if (!IsBlockPairOffDiagonal(block1, block2)) {
      continue;
    }

    int r, c, row_stride, col_stride;
    CellInfo* cell_info = m_->GetCell(block1, block2,
                                      &r, &c,
                                      &row_stride, &col_stride);
    CHECK(cell_info != NULL)
        << "Cell missing for block pair (" << block1 << "," << block2 << ")"
        << " cluster pair (" << cluster_membership_[block1]
        << " " << cluster_membership_[block2] << ")";

    // Ah the magic of tri-diagonal matrices and diagonal
    // dominance. See Lemma 1 in "Visibility Based Preconditioning
    // For Bundle Adjustment".
    MatrixRef m(cell_info->values, row_stride, col_stride);
    m.block(r, c, block_size_[block1], block_size_[block2]) *= 0.5;
  }
}

// Compute the sparse Cholesky factorization of the preconditioner
// matrix.
LinearSolverTerminationType VisibilityBasedPreconditioner::Factorize() {
  // Extract the TripletSparseMatrix that is used for actually storing
  // S and convert it into a cholmod_sparse object.
  cholmod_sparse* lhs = ss_.CreateSparseMatrix(
      down_cast<BlockRandomAccessSparseMatrix*>(
          m_.get())->mutable_matrix());

  // The matrix is symmetric, and the upper triangular part of the
  // matrix contains the values.
  lhs->stype = 1;

  // TODO(sameeragarwal): Refactor to pipe this up and out.
  string status;

  // Symbolic factorization is computed if we don't already have one handy.
  if (factor_ == NULL) {
    factor_ = ss_.BlockAnalyzeCholesky(lhs, block_size_, block_size_, &status);
  }

  const LinearSolverTerminationType termination_type =
      (factor_ != NULL)
      ? ss_.Cholesky(lhs, factor_, &status)
      : LINEAR_SOLVER_FATAL_ERROR;

  ss_.Free(lhs);
  return termination_type;
}

void VisibilityBasedPreconditioner::RightMultiply(const double* x,
                                                  double* y) const {
  CHECK_NOTNULL(x);
  CHECK_NOTNULL(y);
  SuiteSparse* ss = const_cast<SuiteSparse*>(&ss_);

  const int num_rows = m_->num_rows();
  memcpy(CHECK_NOTNULL(tmp_rhs_)->x, x, m_->num_rows() * sizeof(*x));
  // TODO(sameeragarwal): Better error handling.
  string status;
  cholmod_dense* solution =
      CHECK_NOTNULL(ss->Solve(factor_, tmp_rhs_, &status));
  memcpy(y, solution->x, sizeof(*y) * num_rows);
  ss->Free(solution);
}

int VisibilityBasedPreconditioner::num_rows() const {
  return m_->num_rows();
}

// Classify camera/f_block pairs as in and out of the preconditioner,
// based on whether the cluster pair that they belong to is in the
// preconditioner or not.
bool VisibilityBasedPreconditioner::IsBlockPairInPreconditioner(
    const int block1,
    const int block2) const {
  int cluster1 = cluster_membership_[block1];
  int cluster2 = cluster_membership_[block2];
  if (cluster1 > cluster2) {
    std::swap(cluster1, cluster2);
  }
  return (cluster_pairs_.count(make_pair(cluster1, cluster2)) > 0);
}

bool VisibilityBasedPreconditioner::IsBlockPairOffDiagonal(
    const int block1,
    const int block2) const {
  return (cluster_membership_[block1] != cluster_membership_[block2]);
}

// Convert a graph into a list of edges that includes self edges for
// each vertex.
void VisibilityBasedPreconditioner::ForestToClusterPairs(
    const Graph<int>& forest,
    HashSet<pair<int, int> >* cluster_pairs) const {
  CHECK_NOTNULL(cluster_pairs)->clear();
  const HashSet<int>& vertices = forest.vertices();
  CHECK_EQ(vertices.size(), num_clusters_);

  // Add all the cluster pairs corresponding to the edges in the
  // forest.
  for (HashSet<int>::const_iterator it1 = vertices.begin();
       it1 != vertices.end();
       ++it1) {
    const int cluster1 = *it1;
    cluster_pairs->insert(make_pair(cluster1, cluster1));
    const HashSet<int>& neighbors = forest.Neighbors(cluster1);
    for (HashSet<int>::const_iterator it2 = neighbors.begin();
         it2 != neighbors.end();
         ++it2) {
      const int cluster2 = *it2;
      if (cluster1 < cluster2) {
        cluster_pairs->insert(make_pair(cluster1, cluster2));
      }
    }
  }
}

// The visibilty set of a cluster is the union of the visibilty sets
// of all its cameras. In other words, the set of points visible to
// any camera in the cluster.
void VisibilityBasedPreconditioner::ComputeClusterVisibility(
    const vector<set<int> >& visibility,
    vector<set<int> >* cluster_visibility) const {
  CHECK_NOTNULL(cluster_visibility)->resize(0);
  cluster_visibility->resize(num_clusters_);
  for (int i = 0; i < num_blocks_; ++i) {
    const int cluster_id = cluster_membership_[i];
    (*cluster_visibility)[cluster_id].insert(visibility[i].begin(),
                                             visibility[i].end());
  }
}

// Construct a graph whose vertices are the clusters, and the edge
// weights are the number of 3D points visible to cameras in both the
// vertices.
Graph<int>* VisibilityBasedPreconditioner::CreateClusterGraph(
    const vector<set<int> >& cluster_visibility) const {
  Graph<int>* cluster_graph = new Graph<int>;

  for (int i = 0; i < num_clusters_; ++i) {
    cluster_graph->AddVertex(i);
  }

  for (int i = 0; i < num_clusters_; ++i) {
    const set<int>& cluster_i = cluster_visibility[i];
    for (int j = i+1; j < num_clusters_; ++j) {
      vector<int> intersection;
      const set<int>& cluster_j = cluster_visibility[j];
      set_intersection(cluster_i.begin(), cluster_i.end(),
                       cluster_j.begin(), cluster_j.end(),
                       back_inserter(intersection));

      if (intersection.size() > 0) {
        // Clusters interact strongly when they share a large number
        // of 3D points. The degree-2 maximum spanning forest
        // alorithm, iterates on the edges in decreasing order of
        // their weight, which is the number of points shared by the
        // two cameras that it connects.
        cluster_graph->AddEdge(i, j, intersection.size());
      }
    }
  }
  return cluster_graph;
}

// Canonical views clustering returns a HashMap from vertices to
// cluster ids. Convert this into a flat array for quick lookup. It is
// possible that some of the vertices may not be associated with any
// cluster. In that case, randomly assign them to one of the clusters.
//
// The cluster ids can be non-contiguous integers. So as we flatten
// the membership_map, we also map the cluster ids to a contiguous set
// of integers so that the cluster ids are in [0, num_clusters_).
void VisibilityBasedPreconditioner::FlattenMembershipMap(
    const HashMap<int, int>& membership_map,
    vector<int>* membership_vector) const {
  CHECK_NOTNULL(membership_vector)->resize(0);
  membership_vector->resize(num_blocks_, -1);

  HashMap<int, int> cluster_id_to_index;
  // Iterate over the cluster membership map and update the
  // cluster_membership_ vector assigning arbitrary cluster ids to
  // the few cameras that have not been clustered.
  for (HashMap<int, int>::const_iterator it = membership_map.begin();
       it != membership_map.end();
       ++it) {
    const int camera_id = it->first;
    int cluster_id = it->second;

    // If the view was not clustered, randomly assign it to one of the
    // clusters. This preserves the mathematical correctness of the
    // preconditioner. If there are too many views which are not
    // clustered, it may lead to some quality degradation though.
    //
    // TODO(sameeragarwal): Check if a large number of views have not
    // been clustered and deal with it?
    if (cluster_id == -1) {
      cluster_id = camera_id % num_clusters_;
    }

    const int index = FindWithDefault(cluster_id_to_index,
                                      cluster_id,
                                      cluster_id_to_index.size());

    if (index == cluster_id_to_index.size()) {
      cluster_id_to_index[cluster_id] = index;
    }

    CHECK_LT(index, num_clusters_);
    membership_vector->at(camera_id) = index;
  }
}

}  // namespace internal
}  // namespace ceres

#endif  // CERES_NO_SUITESPARSE
