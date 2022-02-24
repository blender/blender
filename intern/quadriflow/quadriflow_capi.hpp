/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2019 Blender Foundation. All rights reserved. */

#ifndef QUADRIFLOW_CAPI_HPP
#define QUADRIFLOW_CAPI_HPP

#ifdef __cplusplus
extern "C" {
#endif

enum { QFLOW_CONSTRAINED = 1 };

typedef struct QuadriflowFace {
  int v[3];
  char eflag[3];
} QuadriflowFace;

typedef struct QuadriflowRemeshData {
  float *verts;
  QuadriflowFace *faces;
  int totfaces;
  int totverts;

  float *out_verts;
  int *out_faces;
  int out_totverts;
  int out_totfaces;

  int target_faces;
  bool preserve_sharp;
  bool preserve_boundary;
  bool adaptive_scale;
  bool minimum_cost_flow;
  bool aggresive_sat;
  int rng_seed;
} QuadriflowRemeshData;

void QFLOW_quadriflow_remesh(QuadriflowRemeshData *qrd,
                             void (*update_cb)(void *, float progress, int *cancel),
                             void *update_cb_data);

#ifdef __cplusplus
}
#endif

#endif  // QUADRIFLOW_CAPI_HPP
