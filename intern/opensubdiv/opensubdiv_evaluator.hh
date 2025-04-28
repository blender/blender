/* SPDX-FileCopyrightText: 2018 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Sergey Sharybin. */

#ifndef OPENSUBDIV_EVALUATOR_IMPL_H_
#define OPENSUBDIV_EVALUATOR_IMPL_H_

#ifdef _MSC_VER
#  include <iso646.h>
#endif

#include <opensubdiv/far/patchMap.h>
#include <opensubdiv/far/patchTable.h>

#include "opensubdiv_capi_type.hh"

#include "GPU_storage_buffer.hh"

struct OpenSubdiv_EvaluatorCache;
struct OpenSubdiv_EvaluatorSettings;
struct OpenSubdiv_PatchCoord;
namespace blender::gpu {
class VertBuf;
}
namespace blender::opensubdiv {

class TopologyRefinerImpl;
class PatchMap;

// Wrapper around implementation, which defines API which we are capable to
// provide over the implementation.
class EvalOutputAPI {
 public:
  // Anonymous forward declaration of actual evaluator implementation.
  class EvalOutput;

  // NOTE: PatchMap is not owned, only referenced.
  EvalOutputAPI(EvalOutput *implementation, PatchMap *patch_map);

  ~EvalOutputAPI();

  // Set settings for data buffers.
  void setSettings(const OpenSubdiv_EvaluatorSettings *settings);

  // Set coarse positions from a continuous array of coordinates.
  void setCoarsePositions(const float *positions,
                          const int start_vertex_index,
                          const int num_vertices);
  // Set vertex data from a continuous array of data.
  void setVertexData(const float *data, const int start_vertex_index, const int num_vertices);
  // Set varying data from a continuous array of data.
  void setVaryingData(const float *varying_data,
                      const int start_vertex_index,
                      const int num_vertices);
  // Set face varying data from a continuous array of data.
  //
  // TODO(sergey): Find a better name for vertex here. It is not the vertex of
  // geometry, but a vertex of UV map.
  void setFaceVaryingData(const int face_varying_channel,
                          const float *varying_data,
                          const int start_vertex_index,
                          const int num_vertices);

  // Set coarse vertex position from a continuous memory buffer where
  // first coordinate starts at offset of `start_offset` and there is `stride`
  // bytes between adjacent vertex coordinates.
  void setCoarsePositionsFromBuffer(const void *buffer,
                                    const int start_offset,
                                    const int stride,
                                    const int start_vertex_index,
                                    const int num_vertices);
  // Set varying data from a continuous memory buffer where
  // first coordinate starts at offset of `start_offset` and there is `stride`
  // bytes between adjacent vertex coordinates.
  void setVaryingDataFromBuffer(const void *buffer,
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
  void setFaceVaryingDataFromBuffer(const int face_varying_channel,
                                    const void *buffer,
                                    const int start_offset,
                                    const int stride,
                                    const int start_vertex_index,
                                    const int num_vertices);

  // Refine after coarse positions update.
  void refine();

  // Evaluate given ptex face at given bilinear coordinate.
  // If derivatives are NULL, they will not be evaluated.
  void evaluateLimit(const int ptex_face_index,
                     float face_u,
                     float face_v,
                     float P[3],
                     float dPdu[3],
                     float dPdv[3]);

  // Evaluate varying data at a given bilinear coordinate of given ptex face.
  void evaluateVertexData(const int ptex_face_index, float face_u, float face_v, float data[]);

  // Evaluate varying data at a given bilinear coordinate of given ptex face.
  void evaluateVarying(const int ptex_face_index, float face_u, float face_v, float varying[3]);

  // Evaluate facee-varying data at a given bilinear coordinate of given
  // ptex face.
  void evaluateFaceVarying(const int face_varying_channel,
                           const int ptex_face_index,
                           float face_u,
                           float face_v,
                           float face_varying[2]);

  // Batched evaluation of multiple input coordinates.

  // Evaluate given ptex face at given bilinear coordinate.
  // If derivatives are NULL, they will not be evaluated.
  //
  // NOTE: Output arrays must point to a memory of size float[3]*num_patch_coords.
  void evaluatePatchesLimit(const OpenSubdiv_PatchCoord *patch_coords,
                            const int num_patch_coords,
                            float *P,
                            float *dPdu,
                            float *dPdv);

  // Fill the output buffers and variables with data from the PatchMap.
  void getPatchMap(blender::gpu::VertBuf *patch_map_handles,
                   blender::gpu::VertBuf *patch_map_quadtree,
                   int *min_patch_face,
                   int *max_patch_face,
                   int *max_depth,
                   int *patches_are_triangular);

  // Copy the patch arrays buffer used by OpenSubDiv for the source data to the given buffer.
  GPUStorageBuf *create_patch_arrays_buf();

  // Wrap the patch index buffer used by OpenSubDiv for the source data with the given buffer.
  GPUStorageBuf *get_patch_index_buf();

  // Wrap the patch param buffer used by OpenSubDiv for the source data with the given buffer.
  GPUStorageBuf *get_patch_param_buf();

  // Wrap the buffer used by OpenSubDiv for the source data with the given buffer.
  gpu::VertBuf *get_source_buf();

  // Wrap the buffer used by OpenSubDiv for the extra source data with the given buffer.
  gpu::VertBuf *get_source_data_buf();

  // Copy the patch arrays buffer used by OpenSubDiv for the face varying channel with the given
  // buffer.
  GPUStorageBuf *create_face_varying_patch_array_buf(const int face_varying_channel);

  // Wrap the patch index buffer used by OpenSubDiv for the face varying channel with the given
  // buffer.
  GPUStorageBuf *get_face_varying_patch_index_buf(const int face_varying_channel);

  // Wrap the patch param buffer used by OpenSubDiv for the face varying channel with the given
  // buffer.
  GPUStorageBuf *get_face_varying_patch_param_buf(const int face_varying_channel);

  // Wrap thebuffer used by OpenSubDiv for the face varying channel with the given buffer.
  gpu::VertBuf *get_face_varying_source_buf(const int face_varying_channel);
  /** Get the source buffer offset for the given channel. */
  int get_face_varying_source_offset(const int face_varying_channel) const;

  // Return true if source vertex data has been set.
  bool hasVertexData() const;

 protected:
  PatchMap *patch_map_;
  EvalOutput *implementation_;
};

}  // namespace blender::opensubdiv

struct OpenSubdiv_Evaluator {
  blender::opensubdiv::EvalOutputAPI *eval_output;
  const blender::opensubdiv::PatchMap *patch_map;
  const OpenSubdiv::Far::PatchTable *patch_table;

  eOpenSubdivEvaluator type;

  OpenSubdiv_Evaluator();
  ~OpenSubdiv_Evaluator();
};

OpenSubdiv_Evaluator *openSubdiv_createEvaluatorFromTopologyRefiner(
    blender::opensubdiv::TopologyRefinerImpl *topology_refiner,
    eOpenSubdivEvaluator evaluator_type,
    OpenSubdiv_EvaluatorCache *evaluator_cache_descr);

#endif  // OPENSUBDIV_EVALUATOR_IMPL_H_
