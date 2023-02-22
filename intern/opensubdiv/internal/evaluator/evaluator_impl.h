// Copyright 2018 Blender Foundation
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

#ifndef OPENSUBDIV_EVALUATOR_IMPL_H_
#define OPENSUBDIV_EVALUATOR_IMPL_H_

#ifdef _MSC_VER
#  include <iso646.h>
#endif

#include <opensubdiv/far/patchMap.h>
#include <opensubdiv/far/patchTable.h>

#include "internal/base/memory.h"

#include "opensubdiv_capi_type.h"

struct OpenSubdiv_Buffer;
struct OpenSubdiv_EvaluatorCacheImpl;
struct OpenSubdiv_EvaluatorSettings;
struct OpenSubdiv_PatchCoord;
struct OpenSubdiv_TopologyRefiner;

namespace blender {
namespace opensubdiv {

class PatchMap;

// Wrapper around implementation, which defines API which we are capable to
// provide over the implementation.
//
// TODO(sergey):  It is almost the same as C-API object, so ideally need to
// merge them somehow, but how to do this and keep files with all the templates
// and such separate?
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
  void evaluateVertexData(const int ptes_face_index, float face_u, float face_v, float data[]);

  // Evaluate varying data at a given bilinear coordinate of given ptex face.
  void evaluateVarying(const int ptes_face_index, float face_u, float face_v, float varying[3]);

  // Evaluate facee-varying data at a given bilinear coordinate of given
  // ptex face.
  void evaluateFaceVarying(const int face_varying_channel,
                           const int ptes_face_index,
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
  void getPatchMap(OpenSubdiv_Buffer *patch_map_handles,
                   OpenSubdiv_Buffer *patch_map_quadtree,
                   int *min_patch_face,
                   int *max_patch_face,
                   int *max_depth,
                   int *patches_are_triangular);

  // Copy the patch arrays buffer used by OpenSubDiv for the source data to the given buffer.
  void fillPatchArraysBuffer(OpenSubdiv_Buffer *patch_arrays_buffer);

  // Wrap the patch index buffer used by OpenSubDiv for the source data with the given buffer.
  void wrapPatchIndexBuffer(OpenSubdiv_Buffer *patch_index_buffer);

  // Wrap the patch param buffer used by OpenSubDiv for the source data with the given buffer.
  void wrapPatchParamBuffer(OpenSubdiv_Buffer *patch_param_buffer);

  // Wrap the buffer used by OpenSubDiv for the source data with the given buffer.
  void wrapSrcBuffer(OpenSubdiv_Buffer *src_buffer);

  // Wrap the buffer used by OpenSubDiv for the extra source data with the given buffer.
  void wrapSrcVertexDataBuffer(OpenSubdiv_Buffer *src_buffer);

  // Copy the patch arrays buffer used by OpenSubDiv for the face varying channel with the given
  // buffer.
  void fillFVarPatchArraysBuffer(const int face_varying_channel,
                                 OpenSubdiv_Buffer *patch_arrays_buffer);

  // Wrap the patch index buffer used by OpenSubDiv for the face varying channel with the given
  // buffer.
  void wrapFVarPatchIndexBuffer(const int face_varying_channel,
                                OpenSubdiv_Buffer *patch_index_buffer);

  // Wrap the patch param buffer used by OpenSubDiv for the face varying channel with the given
  // buffer.
  void wrapFVarPatchParamBuffer(const int face_varying_channel,
                                OpenSubdiv_Buffer *patch_param_buffer);

  // Wrap thebuffer used by OpenSubDiv for the face varying channel with the given buffer.
  void wrapFVarSrcBuffer(const int face_varying_channel, OpenSubdiv_Buffer *src_buffer);

  // Return true if source vertex data has been set.
  bool hasVertexData() const;

 protected:
  PatchMap *patch_map_;
  EvalOutput *implementation_;
};

}  // namespace opensubdiv
}  // namespace blender

struct OpenSubdiv_EvaluatorImpl {
 public:
  OpenSubdiv_EvaluatorImpl();
  ~OpenSubdiv_EvaluatorImpl();

  blender::opensubdiv::EvalOutputAPI *eval_output;
  const blender::opensubdiv::PatchMap *patch_map;
  const OpenSubdiv::Far::PatchTable *patch_table;

  MEM_CXX_CLASS_ALLOC_FUNCS("OpenSubdiv_EvaluatorImpl");
};

OpenSubdiv_EvaluatorImpl *openSubdiv_createEvaluatorInternal(
    struct OpenSubdiv_TopologyRefiner *topology_refiner,
    eOpenSubdivEvaluator evaluator_type,
    OpenSubdiv_EvaluatorCacheImpl *evaluator_cache_descr);

void openSubdiv_deleteEvaluatorInternal(OpenSubdiv_EvaluatorImpl *evaluator);

#endif  // OPENSUBDIV_EVALUATOR_IMPL_H_
