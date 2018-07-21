// Copyright 2013 Blender Foundation. All rights reserved.
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
// Author: Sergey Sharybin

#ifndef OPENSUBDIV_EVALUATOR_CAPI_H_
#define OPENSUBDIV_EVALUATOR_CAPI_H_

#ifdef __cplusplus
extern "C" {
#endif

struct OpenSubdiv_EvaluatorInternal;
struct OpenSubdiv_TopologyRefiner;

typedef struct OpenSubdiv_Evaluator {
  // Set coarse positions from a continuous array of coordinates.
  void (*setCoarsePositions)(struct OpenSubdiv_Evaluator* evaluator,
                             const float* positions,
                             const int start_vertex_index,
                             const int num_vertices);
  // Set varying data from a continuous array of data.
  void (*setVaryingData)(struct OpenSubdiv_Evaluator* evaluator,
                         const float* varying_data,
                         const int start_vertex_index, const int num_vertices);
  // Set face varying data from a continuous array of data.
  //
  // TODO(sergey): Find a better name for vertex here. It is not the vertex of
  // geometry, but a vertex of UV map.
  void (*setFaceVaryingData)(struct OpenSubdiv_Evaluator* evaluator,
                             const float* face_varying_data,
                             const int start_vertex_index,
                             const int num_vertices);

  // Set coarse vertex position from a continuous memory buffer where
  // first coordinate starts at offset of `start_offset` and there is `stride`
  // bytes between adjacent vertex coordinates.
  void (*setCoarsePositionsFromBuffer)(struct OpenSubdiv_Evaluator* evaluator,
                                       const void* buffer,
                                       const int start_offset,
                                       const int stride,
                                       const int start_vertex_index,
                                       const int num_vertices);
  // Set varying data from a continuous memory buffer where
  // first coordinate starts at offset of `start_offset` and there is `stride`
  // bytes between adjacent vertex coordinates.
  void (*setVaryingDataFromBuffer)(struct OpenSubdiv_Evaluator* evaluator,
                                   const void* buffer,
                                   const int start_offset,
                                   const int stride,
                                   const int start_vertex_index,
                                   const int num_vertices);
  // Set face varying data from a continuous memory buffer where
  // first coordinate starts at offset of `start_offset` and there is `stride`
  // bytes between adjacent vertex coordinates.
  //
  // TODO(sergey): Find a better name for vertex here. It is not the vertex of
  // geometry, but a vertex of UV map.
  void (*setFaceVaryingDataFromBuffer)(struct OpenSubdiv_Evaluator* evaluator,
                                       const void* buffer,
                                       const int start_offset,
                                       const int stride,
                                       const int start_vertex_index,
                                       const int num_vertices);

  // Refine after coarse positions update.
  void (*refine)(struct OpenSubdiv_Evaluator* evaluator);

  // Evaluate given ptex face at given bilinear coordinate.
  // If derivatives are NULL, they will not be evaluated.
  void (*evaluateLimit)(struct OpenSubdiv_Evaluator* evaluator,
                        const int ptex_face_index,
                        float face_u, float face_v,
                        float P[3], float dPdu[3], float dPdv[3]);

  // Evaluate varying data at a given bilinear coordinate of given ptex face.
  void (*evaluateVarying)(struct OpenSubdiv_Evaluator* evaluator,
                          const int ptex_face_index,
                          float face_u, float face_v,
                          float varying[3]);

  // Evaluate face-varying data at a given bilinear coordinate of given
  // ptex face.
  void (*evaluateFaceVarying)(struct OpenSubdiv_Evaluator* evaluator,
                              const int ptex_face_index,
                              float face_u, float face_v,
                              float face_varying[2]);

  // Internal storage for the use in this module only.
  //
  // This is where actual OpenSubdiv's evaluator is living.
  struct OpenSubdiv_EvaluatorInternal* internal;
} OpenSubdiv_Evaluator;

OpenSubdiv_Evaluator* openSubdiv_createEvaluatorFromTopologyRefiner(
    struct OpenSubdiv_TopologyRefiner* topology_refiner);

void openSubdiv_deleteEvaluator(OpenSubdiv_Evaluator* evaluator);

#ifdef __cplusplus
}
#endif

#endif  // OPENSUBDIV_EVALUATOR_CAPI_H_
