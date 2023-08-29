/* SPDX-FileCopyrightText: 2000 `Bruno Levy <levy@loria.fr>`
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * The Original Code is:
 * - OGF/Graphite: Geometry and Graphics Programming Library + Utilities.
 */

/** \file
 * \ingroup freestyle
 */

#include "normal_cycle.h"
#include "matrix_util.h"

namespace Freestyle::OGF {

//_________________________________________________________

void NormalCycle::begin()
{
  M_[0] = M_[1] = M_[2] = M_[3] = M_[4] = M_[5] = 0;
}

void NormalCycle::end()
{
  double eigen_vectors[9];
  MatrixUtil::semi_definite_symmetric_eigen(M_, 3, eigen_vectors, eigen_value_);

  axis_[0] = Vec3r(eigen_vectors[0], eigen_vectors[1], eigen_vectors[2]);

  axis_[1] = Vec3r(eigen_vectors[3], eigen_vectors[4], eigen_vectors[5]);

  axis_[2] = Vec3r(eigen_vectors[6], eigen_vectors[7], eigen_vectors[8]);

  // Normalize the eigen vectors
  for (int i = 0; i < 3; i++) {
    axis_[i].normalize();
  }

  // Sort the eigen vectors
  i_[0] = 0;
  i_[1] = 1;
  i_[2] = 2;

  double l0 = ::fabs(eigen_value_[0]);
  double l1 = ::fabs(eigen_value_[1]);
  double l2 = ::fabs(eigen_value_[2]);

  if (l1 > l0) {
    ogf_swap(l0, l1);
    ogf_swap(i_[0], i_[1]);
  }
  if (l2 > l1) {
    ogf_swap(l1, l2);
    ogf_swap(i_[1], i_[2]);
  }
  if (l1 > l0) {
    ogf_swap(l0, l1);
    ogf_swap(i_[0], i_[1]);
  }
}

//_________________________________________________________

}  // namespace Freestyle::OGF
