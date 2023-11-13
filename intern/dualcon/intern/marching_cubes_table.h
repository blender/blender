/* SPDX-FileCopyrightText: 2002-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef __MARCHING_CUBES_TABLE_H__
#define __MARCHING_CUBES_TABLE_H__

/* number of configurations */
#define TOTCONF 256

/* maximum number of triangles per configuration */
#define MAX_TRIS 10

/* number of triangles in each configuration */
extern const int marching_cubes_numtri[TOTCONF];

/* table of triangles in each configuration */
extern const int marching_cubes_tris[TOTCONF][MAX_TRIS][3];

#endif
