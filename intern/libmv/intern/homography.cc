/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "intern/homography.h"
#include "intern/utildefines.h"
#include "libmv/logging/logging.h"
#include "libmv/multiview/homography.h"

void libmv_homography2DFromCorrespondencesEuc(/* const */ double (*x1)[2],
                                              /* const */ double (*x2)[2],
                                              int num_points,
                                              double H[3][3]) {
  libmv::Mat x1_mat, x2_mat;
  libmv::Mat3 H_mat;

  x1_mat.resize(2, num_points);
  x2_mat.resize(2, num_points);

  for (int i = 0; i < num_points; i++) {
    x1_mat.col(i) = libmv::Vec2(x1[i][0], x1[i][1]);
    x2_mat.col(i) = libmv::Vec2(x2[i][0], x2[i][1]);
  }

  LG << "x1: " << x1_mat;
  LG << "x2: " << x2_mat;

  libmv::EstimateHomographyOptions options;
  libmv::EstimateHomography2DFromCorrespondences(
      x1_mat, x2_mat, options, &H_mat);

  LG << "H: " << H_mat;

  memcpy(H, H_mat.data(), 9 * sizeof(double));
}
