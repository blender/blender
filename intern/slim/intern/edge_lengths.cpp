/* SPDX-FileCopyrightText: 2013 Alec Jacobson
 *                         2023 Blender Authors
 *
 * SPDX-License-Identifier: MPL-2.0 */

/** \file
 * \ingroup intern_slim
 */

#include "BLI_task.hh"

#include "edge_lengths.h"

namespace slim {

template<typename DerivedV, typename DerivedF, typename DerivedL>
inline void edge_lengths(const Eigen::PlainObjectBase<DerivedV> &V,
                         const Eigen::PlainObjectBase<DerivedF> &F,
                         Eigen::PlainObjectBase<DerivedL> &L)
{
  squared_edge_lengths(V, F, L);
  L = L.array().sqrt().eval();
}

template<typename DerivedV, typename DerivedF, typename DerivedL>
inline void squared_edge_lengths(const Eigen::PlainObjectBase<DerivedV> &V,
                                 const Eigen::PlainObjectBase<DerivedF> &F,
                                 Eigen::PlainObjectBase<DerivedL> &L)
{
  using namespace std;
  const int m = F.rows();
  assert(F.cols() == 3);

  L.resize(m, 3);

  /* Loop over faces. */
  using namespace blender;
  threading::parallel_for(IndexRange(m), 1000, [&V, &F, &L](const IndexRange range) {
    for (const int i : range) {
      L(i, 0) = (V.row(F(i, 1)) - V.row(F(i, 2))).squaredNorm();
      L(i, 1) = (V.row(F(i, 2)) - V.row(F(i, 0))).squaredNorm();
      L(i, 2) = (V.row(F(i, 0)) - V.row(F(i, 1))).squaredNorm();
    }
  });
}

}  // namespace slim
