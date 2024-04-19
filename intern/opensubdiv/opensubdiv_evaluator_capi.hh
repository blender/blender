/* SPDX-FileCopyrightText: 2013 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstdint>  // for uint64_t

#include "opensubdiv_capi_type.hh"

struct OpenSubdiv_EvaluatorCacheImpl;
struct OpenSubdiv_EvaluatorImpl;
struct OpenSubdiv_EvaluatorInternal;
struct OpenSubdiv_PatchCoord;
class OpenSubdiv_TopologyRefiner;

struct OpenSubdiv_EvaluatorSettings {
  // Number of smoothly interpolated vertex data channels.
  int num_vertex_data;
};

// Callback type for doing input/output operations on buffers.
// Useful to abstract GPU buffers.
struct OpenSubdiv_Buffer {
  // Bind the buffer to the GPU.
  void (*bind_gpu)(const OpenSubdiv_Buffer *buffer);

  // Allocate the buffer directly on the host for the given size in bytes. This has to return
  // a pointer to the newly allocated memory.
  void *(*alloc)(const OpenSubdiv_Buffer *buffer, const unsigned int size);

  // Allocate the buffer directly on the device for the given size in bytes.
  void (*device_alloc)(const OpenSubdiv_Buffer *buffer, const unsigned int size);

  // Update the given range of the buffer with new data.
  void (*device_update)(const OpenSubdiv_Buffer *buffer,
                        unsigned int start,
                        unsigned int len,
                        const void *data);

  // Wrap an existing GPU buffer, given its device handle, into the client's buffer type for
  // read-only use.
  void (*wrap_device_handle)(const OpenSubdiv_Buffer *buffer, uint64_t device_ptr);

  // Offset in the buffer where the data starts, if a single buffer is used for multiple data
  // channels.
  int buffer_offset;

  // Pointer to the client buffer data, which is modified or initialized through the various
  // callbacks.
  void *data;
};

struct OpenSubdiv_Evaluator {
  // Set settings for data buffers used.
  void (*setSettings)(OpenSubdiv_Evaluator *evaluator,
                      const OpenSubdiv_EvaluatorSettings *settings);

  // Set coarse positions from a continuous array of coordinates.
  void (*setCoarsePositions)(OpenSubdiv_Evaluator *evaluator,
                             const float *positions,
                             const int start_vertex_index,
                             const int num_vertices);
  // Set vertex data from a continuous array of coordinates.
  void (*setVertexData)(OpenSubdiv_Evaluator *evaluator,
                        const float *data,
                        const int start_vertex_index,
                        const int num_vertices);
  // Set varying data from a continuous array of data.
  void (*setVaryingData)(OpenSubdiv_Evaluator *evaluator,
                         const float *varying_data,
                         const int start_vertex_index,
                         const int num_vertices);
  // Set face varying data from a continuous array of data.
  //
  // TODO(sergey): Find a better name for vertex here. It is not the vertex of
  // geometry, but a vertex of UV map.
  void (*setFaceVaryingData)(OpenSubdiv_Evaluator *evaluator,
                             const int face_varying_channel,
                             const float *face_varying_data,
                             const int start_vertex_index,
                             const int num_vertices);

  // Set coarse vertex position from a continuous memory buffer where
  // first coordinate starts at offset of `start_offset` and there is `stride`
  // bytes between adjacent vertex coordinates.
  void (*setCoarsePositionsFromBuffer)(OpenSubdiv_Evaluator *evaluator,
                                       const void *buffer,
                                       const int start_offset,
                                       const int stride,
                                       const int start_vertex_index,
                                       const int num_vertices);
  // Set varying data from a continuous memory buffer where
  // first coordinate starts at offset of `start_offset` and there is `stride`
  // bytes between adjacent vertex coordinates.
  void (*setVaryingDataFromBuffer)(OpenSubdiv_Evaluator *evaluator,
                                   const void *buffer,
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
  void (*setFaceVaryingDataFromBuffer)(OpenSubdiv_Evaluator *evaluator,
                                       const int face_varying_channel,
                                       const void *buffer,
                                       const int start_offset,
                                       const int stride,
                                       const int start_vertex_index,
                                       const int num_vertices);

  // Refine after coarse positions update.
  void (*refine)(OpenSubdiv_Evaluator *evaluator);

  // Evaluate given ptex face at given bilinear coordinate.
  // If derivatives are NULL, they will not be evaluated.
  void (*evaluateLimit)(OpenSubdiv_Evaluator *evaluator,
                        const int ptex_face_index,
                        float face_u,
                        float face_v,
                        float P[3],
                        float dPdu[3],
                        float dPdv[3]);

  // Evaluate vertex data at a given bilinear coordinate of given ptex face.
  void (*evaluateVertexData)(OpenSubdiv_Evaluator *evaluator,
                             const int ptex_face_index,
                             float face_u,
                             float face_v,
                             float data[]);

  // Evaluate varying data at a given bilinear coordinate of given ptex face.
  void (*evaluateVarying)(OpenSubdiv_Evaluator *evaluator,
                          const int ptex_face_index,
                          float face_u,
                          float face_v,
                          float varying[3]);

  // Evaluate face-varying data at a given bilinear coordinate of given
  // ptex face.
  void (*evaluateFaceVarying)(OpenSubdiv_Evaluator *evaluator,
                              const int face_varying_channel,
                              const int ptex_face_index,
                              float face_u,
                              float face_v,
                              float face_varying[2]);

  // Batched evaluation of multiple input coordinates.

  // Evaluate limit surface.
  // If derivatives are NULL, they will not be evaluated.
  //
  // NOTE: Output arrays must point to a memory of size float[3]*num_patch_coords.
  void (*evaluatePatchesLimit)(OpenSubdiv_Evaluator *evaluator,
                               const OpenSubdiv_PatchCoord *patch_coords,
                               const int num_patch_coords,
                               float *P,
                               float *dPdu,
                               float *dPdv);

  // Copy the patch map to the given buffers, and output some topology information.
  void (*getPatchMap)(OpenSubdiv_Evaluator *evaluator,
                      OpenSubdiv_Buffer *patch_map_handles,
                      OpenSubdiv_Buffer *patch_map_quadtree,
                      int *min_patch_face,
                      int *max_patch_face,
                      int *max_depth,
                      int *patches_are_triangular);

  // Fill the given buffer with data from the evaluator's patch array buffer.
  void (*fillPatchArraysBuffer)(OpenSubdiv_Evaluator *evaluator,
                                OpenSubdiv_Buffer *patch_array_buffer);

  // Fill the given buffer with data from the evaluator's patch index buffer.
  void (*wrapPatchIndexBuffer)(OpenSubdiv_Evaluator *evaluator,
                               OpenSubdiv_Buffer *patch_index_buffer);

  // Fill the given buffer with data from the evaluator's patch parameter buffer.
  void (*wrapPatchParamBuffer)(OpenSubdiv_Evaluator *evaluator,
                               OpenSubdiv_Buffer *patch_param_buffer);

  // Fill the given buffer with data from the evaluator's source buffer.
  void (*wrapSrcBuffer)(OpenSubdiv_Evaluator *evaluator, OpenSubdiv_Buffer *src_buffer);

  // Fill the given buffer with data from the evaluator's extra source buffer.
  void (*wrapSrcVertexDataBuffer)(OpenSubdiv_Evaluator *evaluator, OpenSubdiv_Buffer *src_buffer);

  // Fill the given buffer with data from the evaluator's face varying patch array buffer.
  void (*fillFVarPatchArraysBuffer)(OpenSubdiv_Evaluator *evaluator,
                                    const int face_varying_channel,
                                    OpenSubdiv_Buffer *patch_array_buffer);

  // Fill the given buffer with data from the evaluator's face varying patch index buffer.
  void (*wrapFVarPatchIndexBuffer)(OpenSubdiv_Evaluator *evaluator,
                                   const int face_varying_channel,
                                   OpenSubdiv_Buffer *patch_index_buffer);

  // Fill the given buffer with data from the evaluator's face varying patch parameter buffer.
  void (*wrapFVarPatchParamBuffer)(OpenSubdiv_Evaluator *evaluator,
                                   const int face_varying_channel,
                                   OpenSubdiv_Buffer *patch_param_buffer);

  // Fill the given buffer with data from the evaluator's face varying source buffer.
  void (*wrapFVarSrcBuffer)(OpenSubdiv_Evaluator *evaluator,
                            const int face_varying_channel,
                            OpenSubdiv_Buffer *src_buffer);

  // Return true if the evaluator has source vertex data set.
  bool (*hasVertexData)(OpenSubdiv_Evaluator *evaluator);

  // Implementation of the evaluator.
  OpenSubdiv_EvaluatorImpl *impl;

  // Type of the evaluator.
  eOpenSubdivEvaluator type;
};

struct OpenSubdiv_EvaluatorCache {
  // Implementation of the evaluator cache.
  OpenSubdiv_EvaluatorCacheImpl *impl;
};

OpenSubdiv_Evaluator *openSubdiv_createEvaluatorFromTopologyRefiner(
    OpenSubdiv_TopologyRefiner *topology_refiner,
    eOpenSubdivEvaluator evaluator_type,
    OpenSubdiv_EvaluatorCache *evaluator_cache);

void openSubdiv_deleteEvaluator(OpenSubdiv_Evaluator *evaluator);

OpenSubdiv_EvaluatorCache *openSubdiv_createEvaluatorCache(eOpenSubdivEvaluator evaluator_type);

void openSubdiv_deleteEvaluatorCache(OpenSubdiv_EvaluatorCache *evaluator_cache);

// Return the GLSL source code from the OpenSubDiv library used for patch evaluation.
// This function is not thread-safe.
const char *openSubdiv_getGLSLPatchBasisSource();
