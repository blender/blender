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

/* DOUBLEAREA computes twice the area for each input triangle
 *
 * Templates:
 *   DerivedV  derived type of eigen matrix for V (e.g. derived from
 *     MatrixXd)
 *   DerivedF  derived type of eigen matrix for F (e.g. derived from
 *     MatrixXi)
 *   DeriveddblA  derived type of eigen matrix for dblA (e.g. derived from
 *     MatrixXd)
 * Inputs:
 *   V  #V by dim list of mesh vertex positions
 *   F  #F by simplex_size list of mesh faces (must be triangles)
 * Outputs:
 *   dblA  #F list of triangle double areas (SIGNED only for 2D input)
 *
 * Known bug: For dim==3 complexity is O(#V + #F)!! Not just O(#F). This is a big deal
 * if you have 1million unreferenced vertices and 1 face
 */
template<typename DerivedV, typename DerivedF, typename DeriveddblA>
inline void doublearea(const Eigen::PlainObjectBase<DerivedV> &V,
                       const Eigen::PlainObjectBase<DerivedF> &F,
                       Eigen::PlainObjectBase<DeriveddblA> &dblA);

/* Same as above but use instrinsic edge lengths rather than (V,F) mesh
 * Inputs:
 *   l  #F by dim list of edge lengths using
 *     for triangles, columns correspond to edges 23,31,12
 * Outputs:
 *   dblA  #F list of triangle double areas
 */
template<typename Derivedl, typename DeriveddblA>
inline void doublearea(const Eigen::PlainObjectBase<Derivedl> &l,
                       Eigen::PlainObjectBase<DeriveddblA> &dblA);

}  // namespace slim

#include "doublearea.cpp"
