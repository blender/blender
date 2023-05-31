/* SPDX-FileCopyrightText: 2018 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include "BKE_multires.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

BLI_INLINE void BKE_multires_construct_tangent_matrix(float tangent_matrix[3][3],
                                                      const float dPdu[3],
                                                      const float dPdv[3],
                                                      const int corner)
{
  if (corner == 0) {
    copy_v3_v3(tangent_matrix[0], dPdv);
    copy_v3_v3(tangent_matrix[1], dPdu);
    mul_v3_fl(tangent_matrix[0], -1.0f);
    mul_v3_fl(tangent_matrix[1], -1.0f);
  }
  else if (corner == 1) {
    copy_v3_v3(tangent_matrix[0], dPdu);
    copy_v3_v3(tangent_matrix[1], dPdv);
    mul_v3_fl(tangent_matrix[1], -1.0f);
  }
  else if (corner == 2) {
    copy_v3_v3(tangent_matrix[0], dPdv);
    copy_v3_v3(tangent_matrix[1], dPdu);
  }
  else if (corner == 3) {
    copy_v3_v3(tangent_matrix[0], dPdu);
    copy_v3_v3(tangent_matrix[1], dPdv);
    mul_v3_fl(tangent_matrix[0], -1.0f);
  }
  else {
    BLI_assert_msg(0, "Unhandled corner index");
  }
  cross_v3_v3v3(tangent_matrix[2], dPdu, dPdv);
  normalize_v3(tangent_matrix[0]);
  normalize_v3(tangent_matrix[1]);
  normalize_v3(tangent_matrix[2]);
}
