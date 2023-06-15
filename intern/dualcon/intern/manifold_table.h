/* SPDX-FileCopyrightText: 2002-2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef __MANIFOLD_TABLE_H__
#define __MANIFOLD_TABLE_H__

typedef struct {
  int comps;
  int pairs[12][2];
} ManifoldIndices;

extern const ManifoldIndices manifold_table[256];

#endif /* __MANIFOLD_TABLE_H__ */
