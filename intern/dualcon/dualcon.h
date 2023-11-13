/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef __DUALCON_H__
#define __DUALCON_H__

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef float (*DualConCo)[3];

typedef unsigned int (*DualConTri)[3];

typedef unsigned int *DualConLoop;

typedef struct DualConInput {
  DualConLoop mloop;

  DualConCo co;
  int co_stride;
  int totco;

  DualConTri looptri;
  int tri_stride;
  int tottri;

  int loop_stride;

  float min[3], max[3];
} DualConInput;

/* callback for allocating memory for output */
typedef void *(*DualConAllocOutput)(int totvert, int totquad);
/* callback for adding a new vertex to the output */
typedef void (*DualConAddVert)(void *output, const float co[3]);
/* callback for adding a new quad to the output */
typedef void (*DualConAddQuad)(void *output, const int vert_indices[4]);

typedef enum {
  DUALCON_FLOOD_FILL = 1,
} DualConFlags;

typedef enum {
  /* blocky */
  DUALCON_CENTROID,
  /* smooth */
  DUALCON_MASS_POINT,
  /* keeps sharp edges */
  DUALCON_SHARP_FEATURES,
} DualConMode;

/* Usage:
 *
 * The three callback arguments are used for creating the output
 * mesh. The alloc_output callback takes the total number of vertices
 * and faces (quads) that will be in the output. It should allocate
 * and return a structure to hold the output mesh. The add_vert and
 * add_quad callbacks will then be called for each new vertex and
 * quad, and the callback should add the new mesh elements to the
 * structure.
 */
void *dualcon(const DualConInput *input_mesh,
              /* callbacks for output */
              DualConAllocOutput alloc_output,
              DualConAddVert add_vert,
              DualConAddQuad add_quad,

              /* flags and settings to control the remeshing
               * algorithm */
              DualConFlags flags,
              DualConMode mode,
              float threshold,
              float hermite_num,
              float scale,
              int depth);

#ifdef __cplusplus
}
#endif

#endif /* __DUALCON_H__ */
