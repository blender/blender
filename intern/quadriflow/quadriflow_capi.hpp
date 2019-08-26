// Copyright 2019 Blender Foundation. All rights reserved.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
// Author: Sebastian Parborg, Pablo Dobarro

#ifndef QUADRIFLOW_CAPI_HPP
#define QUADRIFLOW_CAPI_HPP

#ifdef __cplusplus
extern "C" {
#endif

typedef struct QuadriflowRemeshData {
  float *verts;
  unsigned int *faces;
  int totfaces;
  int totverts;

  float *out_verts;
  unsigned int *out_faces;
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
