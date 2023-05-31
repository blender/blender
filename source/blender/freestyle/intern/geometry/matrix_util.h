/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * The Original Code is:
 *   GXML/Graphite: Geometry and Graphics Programming Library + Utilities
 *   Copyright 2000 Bruno Levy <levy@loria.fr> */

#pragma once

/** \file
 * \ingroup freestyle
 */

#include "../system/FreestyleConfig.h"

namespace Freestyle {

namespace OGF {

namespace MatrixUtil {

/**
 * computes the eigen values and eigen vectors of a semi definite symmetric matrix
 *
 * \param mat: The matrix stored in column symmetric storage, i.e.
 * <pre>
 * matrix = { m11, m12, m22, m13, m23, m33, m14, m24, m34, m44 ... }
 * size = n(n+1)/2
 * </pre>
 *
 * \param eigen_vec: (return) = { v1, v2, v3, ..., vn }
 *   where `vk = vk0, vk1, ..., vkn`
 *     `size = n^2`, must be allocated by caller.
 *
 * \param eigen_val: (return) are in decreasing order
 *     `size = n`,   must be allocated by caller.
 */
void semi_definite_symmetric_eigen(const double *mat, int n, double *eigen_vec, double *eigen_val);

}  // namespace MatrixUtil

}  // namespace OGF

} /* namespace Freestyle */
