/* SPDX-FileCopyrightText: 2014 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef LIBMV_C_API_REGION_H_
#define LIBMV_C_API_REGION_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct libmv_Region {
  float min[2];
  float max[2];
} libmv_Region;

#ifdef __cplusplus
}
#endif

#endif  // LIBMV_C_API_REGION_H_
