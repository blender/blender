/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_customdata_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PointCloud {
  ID id;
  struct AnimData *adt; /* animation data (must be immediately after id) */

  int flag;
  int _pad1[1];

  /* Geometry */
  float (*co)[3];
  float *radius;
  int totpoint;
  int _pad2[1];

  /* Custom Data */
  struct CustomData pdata;
  int attributes_active_index;
  int _pad4;

  /* Material */
  struct Material **mat;
  short totcol;
  short _pad3[3];

  /* Draw Cache */
  void *batch_cache;
} PointCloud;

/* PointCloud.flag */
enum {
  PT_DS_EXPAND = (1 << 0),
};

/* Only one material supported currently. */
#define POINTCLOUD_MATERIAL_NR 1

#ifdef __cplusplus
}
#endif
