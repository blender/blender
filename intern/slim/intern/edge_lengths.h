/* SPDX-FileCopyrightText: 2013 Alec Jacobson
 *                         2023 Blender Authors
 *
 * SPDX-License-Identifier: MPL-2.0 */

/** \file
 * \ingroup intern_slim
 */

#pragma once

#include <Eigen/Dense>

namespace slim {
/* Constructs a list of lengths of edges opposite each index in a face
 * (triangle) list
 *
 * Templates:
 *   DerivedV derived from vertex positions matrix type: i.e. MatrixXd
 *   DerivedF derived from face indices matrix type: i.e. MatrixXi
 *   DerivedL derived from edge lengths matrix type: i.e. MatrixXd
 * Inputs:
 *   V  eigen matrix #V by 3
 *   F  #F by 2 list of mesh edges
 *    or
 *   F  #F by 3 list of mesh faces (must be triangles)
 *    or
 *   T  #T by 4 list of mesh elements (must be tets)
 * Outputs:
 *   L  #F by {1|3|6} list of edge lengths
 *     for edges, column of lengths
 *     for triangles, columns correspond to edges [1,2],[2,0],[0,1]
 */
template<typename DerivedV, typename DerivedF, typename DerivedL>
inline void edge_lengths(const Eigen::PlainObjectBase<DerivedV> &V,
                         const Eigen::PlainObjectBase<DerivedF> &F,
                         Eigen::PlainObjectBase<DerivedL> &L);

/* Constructs a list of squared lengths of edges opposite each index in a face
 * (triangle) list
 *
 * Templates:
 *   DerivedV derived from vertex positions matrix type: i.e. MatrixXd
 *   DerivedF derived from face indices matrix type: i.e. MatrixXi
 *   DerivedL derived from edge lengths matrix type: i.e. MatrixXd
 * Inputs:
 *   V  eigen matrix #V by 3
 *   F  #F by 2 list of mesh edges
 *    or
 *   F  #F by 3 list of mesh faces (must be triangles)
 * Outputs:
 *   L  #F by {1|3|6} list of edge lengths squared
 *     for edges, column of lengths
 *     for triangles, columns correspond to edges [1,2],[2,0],[0,1]
 */
template<typename DerivedV, typename DerivedF, typename DerivedL>
inline void squared_edge_lengths(const Eigen::PlainObjectBase<DerivedV> &V,
                                 const Eigen::PlainObjectBase<DerivedF> &F,
                                 Eigen::PlainObjectBase<DerivedL> &L);
}  // namespace slim

#include "edge_lengths.cpp"
