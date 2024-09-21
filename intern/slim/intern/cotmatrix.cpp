/* SPDX-FileCopyrightText: 2013 Alec Jacobson
 *
 * SPDX-License-Identifier: MPL-2.0 */

/** \file
 * \ingroup intern_slim
 */

#include "cotmatrix.h"
#include "edge_lengths.h"

#include <vector>

namespace slim {

/* Inputs:
 *   V  #V by dim list of rest domain positions
 *   F  #F by 3 list of triangle indices into V
 * Outputs:
 *     C  #F by 3 list of 1/2*cotangents corresponding angles
 *       for triangles, columns correspond to edges [1,2],[2,0],[0,1]
 */
template<typename DerivedV, typename DerivedF, typename DerivedC>
static inline void cotmatrix_entries(const Eigen::PlainObjectBase<DerivedV> &V,
                                     const Eigen::PlainObjectBase<DerivedF> &F,
                                     Eigen::PlainObjectBase<DerivedC> &C)
{
  using namespace std;
  using namespace Eigen;
  /* Number of elements. */
  int m = F.rows();

  assert(F.cols() == 3);

  /* Law of cosines + law of sines. */
  /* Compute Squared Edge lenghts. */
  Matrix<typename DerivedC::Scalar, Dynamic, 3> l2;
  squared_edge_lengths(V, F, l2);
  /* Compute Edge lenghts. */
  Matrix<typename DerivedC::Scalar, Dynamic, 3> l;
  l = l2.array().sqrt();

  /* Double area. */
  Matrix<typename DerivedC::Scalar, Dynamic, 1> dblA;
  doublearea(l, dblA);
  /* Cotangents and diagonal entries for element matrices.
   * correctly divided by 4. */
  C.resize(m, 3);
  for (int i = 0; i < m; i++) {
    C(i, 0) = (l2(i, 1) + l2(i, 2) - l2(i, 0)) / dblA(i) / 4.0;
    C(i, 1) = (l2(i, 2) + l2(i, 0) - l2(i, 1)) / dblA(i) / 4.0;
    C(i, 2) = (l2(i, 0) + l2(i, 1) - l2(i, 2)) / dblA(i) / 4.0;
  }
}

template<typename DerivedV, typename DerivedF, typename Scalar>
inline void cotmatrix(const Eigen::PlainObjectBase<DerivedV> &V,
                      const Eigen::PlainObjectBase<DerivedF> &F,
                      Eigen::SparseMatrix<Scalar> &L)
{
  using namespace Eigen;
  using namespace std;

  L.resize(V.rows(), V.rows());
  Matrix<int, Dynamic, 2> edges;
  /* 3 for triangles. */
  assert(F.cols() == 3);

  /* This is important! it could decrease the comptuation time by a factor of 2
   * Laplacian for a closed 2d manifold mesh will have on average 7 entries per
   * row. */
  L.reserve(10 * V.rows());
  edges.resize(3, 2);
  edges << 1, 2, 2, 0, 0, 1;

  /* Gather cotangents. */
  Matrix<Scalar, Dynamic, Dynamic> C;
  cotmatrix_entries(V, F, C);

  vector<Triplet<Scalar>> IJV;
  IJV.reserve(F.rows() * edges.rows() * 4);
  /* Loop over triangles. */
  for (int i = 0; i < F.rows(); i++) {
    /* Loop over edges of element. */
    for (int e = 0; e < edges.rows(); e++) {
      int source = F(i, edges(e, 0));
      int dest = F(i, edges(e, 1));
      IJV.push_back(Triplet<Scalar>(source, dest, C(i, e)));
      IJV.push_back(Triplet<Scalar>(dest, source, C(i, e)));
      IJV.push_back(Triplet<Scalar>(source, source, -C(i, e)));
      IJV.push_back(Triplet<Scalar>(dest, dest, -C(i, e)));
    }
  }
  L.setFromTriplets(IJV.begin(), IJV.end());
}

}  // namespace slim
