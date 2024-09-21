/* SPDX-FileCopyrightText: 2014 Alec Jacobson
 *                         2023 Blender Authors
 *
 * SPDX-License-Identifier: MPL-2.0 */

/** \file
 * \ingroup intern_slim
 */

#pragma once

#include <Eigen/Dense>
#include <Eigen/Sparse>

namespace slim {

/* Constructs the cotangent stiffness matrix (discrete laplacian) for a given
 * mesh (V,F).
 *
 * Templates:
 *   DerivedV  derived type of eigen matrix for V (e.g. derived from
 *     MatrixXd)
 *   DerivedF  derived type of eigen matrix for F (e.g. derived from
 *     MatrixXi)
 *   Scalar  scalar type for eigen sparse matrix (e.g. double)
 * Inputs:
 *   V  #V by dim list of mesh vertex positions
 *   F  #F by simplex_size list of mesh faces (must be triangles)
 * Outputs:
 *   L  #V by #V cotangent matrix, each row i corresponding to V(i,:)
 *
 * Note: This Laplacian uses the convention that diagonal entries are
 * **minus** the sum of off-diagonal entries. The diagonal entries are
 * therefore in general negative and the matrix is **negative** semi-definite
 * (immediately, -L is **positive** semi-definite)
 */
template<typename DerivedV, typename DerivedF, typename Scalar>
inline void cotmatrix(const Eigen::PlainObjectBase<DerivedV> &V,
                      const Eigen::PlainObjectBase<DerivedF> &F,
                      Eigen::SparseMatrix<Scalar> &L);

}  // namespace slim

#include "cotmatrix.cpp"
