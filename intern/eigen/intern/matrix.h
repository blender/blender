/* SPDX-FileCopyrightText: 2015 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_eigen
 */

#ifndef __EIGEN3_MATRIX_C_API_H__
#define __EIGEN3_MATRIX_C_API_H__

#ifdef __cplusplus
extern "C" {
#endif

bool EIG_invert_m4_m4(float inverse[4][4], const float matrix[4][4]);

#ifdef __cplusplus
}
#endif

#endif /* __EIGEN3_MATRIX_C_API_H__ */
