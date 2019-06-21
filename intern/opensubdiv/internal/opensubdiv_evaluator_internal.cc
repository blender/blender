// Copyright 2018 Blender Foundation. All rights reserved.
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

#include "internal/opensubdiv_evaluator_internal.h"

#include <cassert>
#include <cstdio>

#ifdef _MSC_VER
#  include <iso646.h>
#endif

#include <opensubdiv/far/patchMap.h>
#include <opensubdiv/far/patchTable.h>
#include <opensubdiv/far/patchTableFactory.h>
#include <opensubdiv/osd/cpuEvaluator.h>
#include <opensubdiv/osd/cpuPatchTable.h>
#include <opensubdiv/osd/cpuVertexBuffer.h>
#include <opensubdiv/osd/mesh.h>
#include <opensubdiv/osd/types.h>
#include <opensubdiv/version.h>

#include "MEM_guardedalloc.h"

#include "internal/opensubdiv_topology_refiner_internal.h"
#include "internal/opensubdiv_util.h"
#include "internal/opensubdiv_util.h"
#include "opensubdiv_topology_refiner_capi.h"

using OpenSubdiv::Far::PatchMap;
using OpenSubdiv::Far::PatchTable;
using OpenSubdiv::Far::PatchTableFactory;
using OpenSubdiv::Far::StencilTable;
using OpenSubdiv::Far::StencilTableFactory;
using OpenSubdiv::Far::TopologyRefiner;
using OpenSubdiv::Osd::BufferDescriptor;
using OpenSubdiv::Osd::CpuEvaluator;
using OpenSubdiv::Osd::CpuPatchTable;
using OpenSubdiv::Osd::CpuVertexBuffer;
using OpenSubdiv::Osd::PatchCoord;

// TODO(sergey): Remove after official requirement bump for OSD version.
#if OPENSUBDIV_VERSION_NUMBER >= 30200
#  define OPENSUBDIV_HAS_FVAR_EVALUATION
#else
#  undef OPENSUBDIV_HAS_FVAR_EVALUATION
#endif

namespace opensubdiv_capi {

namespace {

// Helper class to wrap numerous of patch coordinates into a buffer.
// Used to pass coordinates to the CPU evaluator. Other evaluators are not
// supported.
class PatchCoordBuffer : public vector<PatchCoord> {
 public:
  static PatchCoordBuffer *Create(int size)
  {
    PatchCoordBuffer *buffer = new PatchCoordBuffer();
    buffer->resize(size);
    return buffer;
  }

  PatchCoord *BindCpuBuffer()
  {
    return reinterpret_cast<PatchCoord *>(&(*this)[0]);
  }

  int GetNumVertices()
  {
    return size();
  }

  void UpdateData(const PatchCoord *patch_coords, int num_patch_coords)
  {
    memcpy(&(*this)[0],
           reinterpret_cast<const void *>(patch_coords),
           sizeof(PatchCoord) * num_patch_coords);
  }
};

// Helper class to wrap single of patch coord into a buffer. Used to pass
// coordinates to the CPU evaluator. Other evaluators are not supported.
class SinglePatchCoordBuffer {
 public:
  static SinglePatchCoordBuffer *Create()
  {
    return new SinglePatchCoordBuffer();
  }

  SinglePatchCoordBuffer()
  {
  }

  explicit SinglePatchCoordBuffer(const PatchCoord &patch_coord) : patch_coord_(patch_coord)
  {
  }

  PatchCoord *BindCpuBuffer()
  {
    return &patch_coord_;
  }

  int GetNumVertices()
  {
    return 1;
  }

  void UpdateData(const PatchCoord &patch_coord)
  {
    patch_coord_ = patch_coord;
  }

 protected:
  PatchCoord patch_coord_;
};

// Helper class which is aimed to be used in cases when buffer is small enough
// and better to be allocated in stack rather than in heap.
//
// TODO(sergey): Check if bare arrays could be used by CPU evaluator.
template<int element_size, int num_vertices> class StackAllocatedBuffer {
 public:
  static PatchCoordBuffer *Create(int /*size*/)
  {
    // TODO(sergey): Validate that requested size is smaller than static
    // stack memory size.
    return new StackAllocatedBuffer<element_size, num_vertices>();
  }

  float *BindCpuBuffer()
  {
    return &data_[0];
  }

  int GetNumVertices()
  {
    return num_vertices;
  }

  // TODO(sergey): Support UpdateData().
 protected:
  float data_[element_size * num_vertices];
};

template<typename EVAL_VERTEX_BUFFER,
         typename STENCIL_TABLE,
         typename PATCH_TABLE,
         typename EVALUATOR,
         typename DEVICE_CONTEXT = void>
class FaceVaryingVolatileEval {
 public:
  typedef OpenSubdiv::Osd::EvaluatorCacheT<EVALUATOR> EvaluatorCache;

  FaceVaryingVolatileEval(int face_varying_channel,
                          const StencilTable *face_varying_stencils,
                          int face_varying_width,
                          PATCH_TABLE *patch_table,
                          EvaluatorCache *evaluator_cache = NULL,
                          DEVICE_CONTEXT *device_context = NULL)
      : face_varying_channel_(face_varying_channel),
        src_face_varying_desc_(0, face_varying_width, face_varying_width),
        patch_table_(patch_table),
        evaluator_cache_(evaluator_cache),
        device_context_(device_context)
  {
    using OpenSubdiv::Osd::convertToCompatibleStencilTable;
    num_coarse_face_varying_vertices_ = face_varying_stencils->GetNumControlVertices();
    const int num_total_face_varying_vertices = face_varying_stencils->GetNumControlVertices() +
                                                face_varying_stencils->GetNumStencils();
    src_face_varying_data_ = EVAL_VERTEX_BUFFER::Create(
        2, num_total_face_varying_vertices, device_context);
    face_varying_stencils_ = convertToCompatibleStencilTable<STENCIL_TABLE>(face_varying_stencils,
                                                                            device_context_);
  }

  ~FaceVaryingVolatileEval()
  {
    delete src_face_varying_data_;
    delete face_varying_stencils_;
  }

  void updateData(const float *src, int start_vertex, int num_vertices)
  {
    src_face_varying_data_->UpdateData(src, start_vertex, num_vertices, device_context_);
  }

  void refine()
  {
    BufferDescriptor dst_face_varying_desc = src_face_varying_desc_;
    dst_face_varying_desc.offset += num_coarse_face_varying_vertices_ *
                                    src_face_varying_desc_.stride;
    const EVALUATOR *eval_instance = OpenSubdiv::Osd::GetEvaluator<EVALUATOR>(
        evaluator_cache_, src_face_varying_desc_, dst_face_varying_desc, device_context_);
    EVALUATOR::EvalStencils(src_face_varying_data_,
                            src_face_varying_desc_,
                            src_face_varying_data_,
                            dst_face_varying_desc,
                            face_varying_stencils_,
                            eval_instance,
                            device_context_);
  }

  void evalPatch(const PatchCoord &patch_coord, float face_varying[2])
  {
    StackAllocatedBuffer<2, 1> face_varying_data;
    BufferDescriptor face_varying_desc(0, 2, 2);
    SinglePatchCoordBuffer patch_coord_buffer(patch_coord);
    const EVALUATOR *eval_instance = OpenSubdiv::Osd::GetEvaluator<EVALUATOR>(
        evaluator_cache_, src_face_varying_desc_, face_varying_desc, device_context_);
    EVALUATOR::EvalPatchesFaceVarying(src_face_varying_data_,
                                      src_face_varying_desc_,
                                      &face_varying_data,
                                      face_varying_desc,
                                      patch_coord_buffer.GetNumVertices(),
                                      &patch_coord_buffer,
                                      patch_table_,
                                      face_varying_channel_,
                                      eval_instance,
                                      device_context_);
    const float *refined_face_varying = face_varying_data.BindCpuBuffer();
    memcpy(face_varying, refined_face_varying, sizeof(float) * 2);
  }

 protected:
  int face_varying_channel_;

  BufferDescriptor src_face_varying_desc_;

  int num_coarse_face_varying_vertices_;
  EVAL_VERTEX_BUFFER *src_face_varying_data_;
  const STENCIL_TABLE *face_varying_stencils_;

  // NOTE: We reference this, do not own it.
  PATCH_TABLE *patch_table_;

  EvaluatorCache *evaluator_cache_;
  DEVICE_CONTEXT *device_context_;
};

// Volatile evaluator which can be used from threads.
//
// TODO(sergey): Make it possible to evaluate coordinates in chunks.
// TODO(sergey): Make it possible to evaluate multiple face varying layers.
//               (or maybe, it's cheap to create new evaluator for existing
//               topology to evaluate all needed face varying layers?)
template<typename SRC_VERTEX_BUFFER,
         typename EVAL_VERTEX_BUFFER,
         typename STENCIL_TABLE,
         typename PATCH_TABLE,
         typename EVALUATOR,
         typename DEVICE_CONTEXT = void>
class VolatileEvalOutput {
 public:
  typedef OpenSubdiv::Osd::EvaluatorCacheT<EVALUATOR> EvaluatorCache;
  typedef FaceVaryingVolatileEval<EVAL_VERTEX_BUFFER,
                                  STENCIL_TABLE,
                                  PATCH_TABLE,
                                  EVALUATOR,
                                  DEVICE_CONTEXT>
      FaceVaryingEval;

  VolatileEvalOutput(const StencilTable *vertex_stencils,
                     const StencilTable *varying_stencils,
                     const vector<const StencilTable *> &all_face_varying_stencils,
                     const int face_varying_width,
                     const PatchTable *patch_table,
                     EvaluatorCache *evaluator_cache = NULL,
                     DEVICE_CONTEXT *device_context = NULL)
      : src_desc_(0, 3, 3),
        src_varying_desc_(0, 3, 3),
        face_varying_width_(face_varying_width),
        evaluator_cache_(evaluator_cache),
        device_context_(device_context)
  {
    // Total number of vertices = coarse points + refined points + local points.
    int num_total_vertices = vertex_stencils->GetNumControlVertices() +
                             vertex_stencils->GetNumStencils();
    num_coarse_vertices_ = vertex_stencils->GetNumControlVertices();
    using OpenSubdiv::Osd::convertToCompatibleStencilTable;
    src_data_ = SRC_VERTEX_BUFFER::Create(3, num_total_vertices, device_context_);
    src_varying_data_ = SRC_VERTEX_BUFFER::Create(3, num_total_vertices, device_context_);
    patch_table_ = PATCH_TABLE::Create(patch_table, device_context_);
    patch_coords_ = NULL;
    vertex_stencils_ = convertToCompatibleStencilTable<STENCIL_TABLE>(vertex_stencils,
                                                                      device_context_);
    varying_stencils_ = convertToCompatibleStencilTable<STENCIL_TABLE>(varying_stencils,
                                                                       device_context_);
    // Create evaluators for every face varying channel.
    face_varying_evaluators.reserve(all_face_varying_stencils.size());
    int face_varying_channel = 0;
    foreach (const StencilTable *face_varying_stencils, all_face_varying_stencils) {
      face_varying_evaluators.push_back(new FaceVaryingEval(face_varying_channel,
                                                            face_varying_stencils,
                                                            face_varying_width,
                                                            patch_table_,
                                                            evaluator_cache_,
                                                            device_context_));
      ++face_varying_channel;
    }
  }

  ~VolatileEvalOutput()
  {
    delete src_data_;
    delete src_varying_data_;
    delete patch_table_;
    delete vertex_stencils_;
    delete varying_stencils_;
    foreach (FaceVaryingEval *face_varying_evaluator, face_varying_evaluators) {
      delete face_varying_evaluator;
    }
  }

  // TODO(sergey): Implement binding API.

  void updateData(const float *src, int start_vertex, int num_vertices)
  {
    src_data_->UpdateData(src, start_vertex, num_vertices, device_context_);
  }

  void updateVaryingData(const float *src, int start_vertex, int num_vertices)
  {
    src_varying_data_->UpdateData(src, start_vertex, num_vertices, device_context_);
  }

  void updateFaceVaryingData(const int face_varying_channel,
                             const float *src,
                             int start_vertex,
                             int num_vertices)
  {
    assert(face_varying_channel >= 0);
    assert(face_varying_channel < face_varying_evaluators.size());
    face_varying_evaluators[face_varying_channel]->updateData(src, start_vertex, num_vertices);
  }

  bool hasVaryingData() const
  {
    // return varying_stencils_ != NULL;
    // TODO(sergey): Check this based on actual topology.
    return false;
  }

  bool hasFaceVaryingData() const
  {
    return face_varying_evaluators.size() != 0;
  }

  void refine()
  {
    // Evaluate vertex positions.
    BufferDescriptor dst_desc = src_desc_;
    dst_desc.offset += num_coarse_vertices_ * src_desc_.stride;
    const EVALUATOR *eval_instance = OpenSubdiv::Osd::GetEvaluator<EVALUATOR>(
        evaluator_cache_, src_desc_, dst_desc, device_context_);
    EVALUATOR::EvalStencils(src_data_,
                            src_desc_,
                            src_data_,
                            dst_desc,
                            vertex_stencils_,
                            eval_instance,
                            device_context_);
    // Evaluate varying data.
    if (hasVaryingData()) {
      BufferDescriptor dst_varying_desc = src_varying_desc_;
      dst_varying_desc.offset += num_coarse_vertices_ * src_varying_desc_.stride;
      eval_instance = OpenSubdiv::Osd::GetEvaluator<EVALUATOR>(
          evaluator_cache_, src_varying_desc_, dst_varying_desc, device_context_);
      EVALUATOR::EvalStencils(src_varying_data_,
                              src_varying_desc_,
                              src_varying_data_,
                              dst_varying_desc,
                              varying_stencils_,
                              eval_instance,
                              device_context_);
    }
    // Evaluate face-varying data.
    if (hasFaceVaryingData()) {
      foreach (FaceVaryingEval *face_varying_evaluator, face_varying_evaluators) {
        face_varying_evaluator->refine();
      }
    }
  }

  void evalPatchCoord(const PatchCoord &patch_coord, float P[3])
  {
    StackAllocatedBuffer<6, 1> vertex_data;
    // TODO(sergey): Varying data is interleaved in vertex array, so need to
    // adjust stride if there is a varying data.
    // BufferDescriptor vertex_desc(0, 3, 6);
    BufferDescriptor vertex_desc(0, 3, 3);
    SinglePatchCoordBuffer patch_coord_buffer(patch_coord);
    const EVALUATOR *eval_instance = OpenSubdiv::Osd::GetEvaluator<EVALUATOR>(
        evaluator_cache_, src_desc_, vertex_desc, device_context_);
    EVALUATOR::EvalPatches(src_data_,
                           src_desc_,
                           &vertex_data,
                           vertex_desc,
                           patch_coord_buffer.GetNumVertices(),
                           &patch_coord_buffer,
                           patch_table_,
                           eval_instance,
                           device_context_);
    const float *refined_vertices = vertex_data.BindCpuBuffer();
    memcpy(P, refined_vertices, sizeof(float) * 3);
  }

  void evalPatchesWithDerivatives(const PatchCoord &patch_coord,
                                  float P[3],
                                  float dPdu[3],
                                  float dPdv[3])
  {
    StackAllocatedBuffer<6, 1> vertex_data, derivatives;
    // TODO(sergey): Varying data is interleaved in vertex array, so need to
    // adjust stride if there is a varying data.
    // BufferDescriptor vertex_desc(0, 3, 6);
    BufferDescriptor vertex_desc(0, 3, 3);
    BufferDescriptor du_desc(0, 3, 6), dv_desc(3, 3, 6);
    SinglePatchCoordBuffer patch_coord_buffer(patch_coord);
    const EVALUATOR *eval_instance = OpenSubdiv::Osd::GetEvaluator<EVALUATOR>(
        evaluator_cache_, src_desc_, vertex_desc, du_desc, dv_desc, device_context_);
    EVALUATOR::EvalPatches(src_data_,
                           src_desc_,
                           &vertex_data,
                           vertex_desc,
                           &derivatives,
                           du_desc,
                           &derivatives,
                           dv_desc,
                           patch_coord_buffer.GetNumVertices(),
                           &patch_coord_buffer,
                           patch_table_,
                           eval_instance,
                           device_context_);
    const float *refined_vertices = vertex_data.BindCpuBuffer();
    memcpy(P, refined_vertices, sizeof(float) * 3);
    if (dPdu != NULL || dPdv != NULL) {
      const float *refined_derivatives = derivatives.BindCpuBuffer();
      if (dPdu != NULL) {
        memcpy(dPdu, refined_derivatives, sizeof(float) * 3);
      }
      if (dPdv != NULL) {
        memcpy(dPdv, refined_derivatives + 3, sizeof(float) * 3);
      }
    }
  }

  void evalPatchVarying(const PatchCoord &patch_coord, float varying[3])
  {
    StackAllocatedBuffer<6, 1> varying_data;
    BufferDescriptor varying_desc(3, 3, 6);
    SinglePatchCoordBuffer patch_coord_buffer(patch_coord);
    const EVALUATOR *eval_instance = OpenSubdiv::Osd::GetEvaluator<EVALUATOR>(
        evaluator_cache_, src_varying_desc_, varying_desc, device_context_);
    EVALUATOR::EvalPatchesVarying(src_varying_data_,
                                  src_varying_desc_,
                                  &varying_data,
                                  varying_desc,
                                  patch_coord_buffer.GetNumVertices(),
                                  &patch_coord_buffer,
                                  patch_table_,
                                  eval_instance,
                                  device_context_);
    const float *refined_varying = varying_data.BindCpuBuffer();
    memcpy(varying, refined_varying, sizeof(float) * 3);
  }

  void evalPatchFaceVarying(const int face_varying_channel,
                            const PatchCoord &patch_coord,
                            float face_varying[2])
  {
    assert(face_varying_channel >= 0);
    assert(face_varying_channel < face_varying_evaluators.size());
    face_varying_evaluators[face_varying_channel]->evalPatch(patch_coord, face_varying);
  }

 private:
  SRC_VERTEX_BUFFER *src_data_;
  SRC_VERTEX_BUFFER *src_varying_data_;
  PatchCoordBuffer *patch_coords_;
  PATCH_TABLE *patch_table_;
  BufferDescriptor src_desc_;
  BufferDescriptor src_varying_desc_;

  int num_coarse_vertices_;

  const STENCIL_TABLE *vertex_stencils_;
  const STENCIL_TABLE *varying_stencils_;

  int face_varying_width_;
  vector<FaceVaryingEval *> face_varying_evaluators;

  EvaluatorCache *evaluator_cache_;
  DEVICE_CONTEXT *device_context_;
};

}  // namespace

// Note: Define as a class instead of typedcef to make it possible
// to have anonymous class in opensubdiv_evaluator_internal.h
class CpuEvalOutput : public VolatileEvalOutput<CpuVertexBuffer,
                                                CpuVertexBuffer,
                                                StencilTable,
                                                CpuPatchTable,
                                                CpuEvaluator> {
 public:
  CpuEvalOutput(const StencilTable *vertex_stencils,
                const StencilTable *varying_stencils,
                const vector<const StencilTable *> &all_face_varying_stencils,
                const int face_varying_width,
                const PatchTable *patch_table,
                EvaluatorCache *evaluator_cache = NULL)
      : VolatileEvalOutput<CpuVertexBuffer,
                           CpuVertexBuffer,
                           StencilTable,
                           CpuPatchTable,
                           CpuEvaluator>(vertex_stencils,
                                         varying_stencils,
                                         all_face_varying_stencils,
                                         face_varying_width,
                                         patch_table,
                                         evaluator_cache)
  {
  }
};

////////////////////////////////////////////////////////////////////////////////
// Evaluator wrapper for anonymous API.

CpuEvalOutputAPI::CpuEvalOutputAPI(CpuEvalOutput *implementation,
                                   OpenSubdiv::Far::PatchMap *patch_map)
    : implementation_(implementation), patch_map_(patch_map)
{
}

CpuEvalOutputAPI::~CpuEvalOutputAPI()
{
  delete implementation_;
}

void CpuEvalOutputAPI::setCoarsePositions(const float *positions,
                                          const int start_vertex_index,
                                          const int num_vertices)
{
  // TODO(sergey): Add sanity check on indices.
  implementation_->updateData(positions, start_vertex_index, num_vertices);
}

void CpuEvalOutputAPI::setVaryingData(const float *varying_data,
                                      const int start_vertex_index,
                                      const int num_vertices)
{
  // TODO(sergey): Add sanity check on indices.
  implementation_->updateVaryingData(varying_data, start_vertex_index, num_vertices);
}

void CpuEvalOutputAPI::setFaceVaryingData(const int face_varying_channel,
                                          const float *face_varying_data,
                                          const int start_vertex_index,
                                          const int num_vertices)
{
  // TODO(sergey): Add sanity check on indices.
  implementation_->updateFaceVaryingData(
      face_varying_channel, face_varying_data, start_vertex_index, num_vertices);
}

void CpuEvalOutputAPI::setCoarsePositionsFromBuffer(const void *buffer,
                                                    const int start_offset,
                                                    const int stride,
                                                    const int start_vertex_index,
                                                    const int num_vertices)
{
  // TODO(sergey): Add sanity check on indices.
  const unsigned char *current_buffer = (unsigned char *)buffer;
  current_buffer += start_offset;
  for (int i = 0; i < num_vertices; ++i) {
    const int current_vertex_index = start_vertex_index + i;
    implementation_->updateData(
        reinterpret_cast<const float *>(current_buffer), current_vertex_index, 1);
    current_buffer += stride;
  }
}

void CpuEvalOutputAPI::setVaryingDataFromBuffer(const void *buffer,
                                                const int start_offset,
                                                const int stride,
                                                const int start_vertex_index,
                                                const int num_vertices)
{
  // TODO(sergey): Add sanity check on indices.
  const unsigned char *current_buffer = (unsigned char *)buffer;
  current_buffer += start_offset;
  for (int i = 0; i < num_vertices; ++i) {
    const int current_vertex_index = start_vertex_index + i;
    implementation_->updateVaryingData(
        reinterpret_cast<const float *>(current_buffer), current_vertex_index, 1);
    current_buffer += stride;
  }
}

void CpuEvalOutputAPI::setFaceVaryingDataFromBuffer(const int face_varying_channel,
                                                    const void *buffer,
                                                    const int start_offset,
                                                    const int stride,
                                                    const int start_vertex_index,
                                                    const int num_vertices)
{
  // TODO(sergey): Add sanity check on indices.
  const unsigned char *current_buffer = (unsigned char *)buffer;
  current_buffer += start_offset;
  for (int i = 0; i < num_vertices; ++i) {
    const int current_vertex_index = start_vertex_index + i;
    implementation_->updateFaceVaryingData(face_varying_channel,
                                           reinterpret_cast<const float *>(current_buffer),
                                           current_vertex_index,
                                           1);
    current_buffer += stride;
  }
}

void CpuEvalOutputAPI::refine()
{
  implementation_->refine();
}

void CpuEvalOutputAPI::evaluateLimit(const int ptex_face_index,
                                     float face_u,
                                     float face_v,
                                     float P[3],
                                     float dPdu[3],
                                     float dPdv[3])
{
  assert(face_u >= 0.0f);
  assert(face_u <= 1.0f);
  assert(face_v >= 0.0f);
  assert(face_v <= 1.0f);
  const PatchTable::PatchHandle *handle = patch_map_->FindPatch(ptex_face_index, face_u, face_v);
  PatchCoord patch_coord(*handle, face_u, face_v);
  if (dPdu != NULL || dPdv != NULL) {
    implementation_->evalPatchesWithDerivatives(patch_coord, P, dPdu, dPdv);
  }
  else {
    implementation_->evalPatchCoord(patch_coord, P);
  }
}

void CpuEvalOutputAPI::evaluateVarying(const int ptex_face_index,
                                       float face_u,
                                       float face_v,
                                       float varying[3])
{
  assert(face_u >= 0.0f);
  assert(face_u <= 1.0f);
  assert(face_v >= 0.0f);
  assert(face_v <= 1.0f);
  const PatchTable::PatchHandle *handle = patch_map_->FindPatch(ptex_face_index, face_u, face_v);
  PatchCoord patch_coord(*handle, face_u, face_v);
  implementation_->evalPatchVarying(patch_coord, varying);
}

void CpuEvalOutputAPI::evaluateFaceVarying(const int face_varying_channel,
                                           const int ptex_face_index,
                                           float face_u,
                                           float face_v,
                                           float face_varying[2])
{
  assert(face_u >= 0.0f);
  assert(face_u <= 1.0f);
  assert(face_v >= 0.0f);
  assert(face_v <= 1.0f);
  const PatchTable::PatchHandle *handle = patch_map_->FindPatch(ptex_face_index, face_u, face_v);
  PatchCoord patch_coord(*handle, face_u, face_v);
  implementation_->evalPatchFaceVarying(face_varying_channel, patch_coord, face_varying);
}

}  // namespace opensubdiv_capi

OpenSubdiv_EvaluatorInternal::OpenSubdiv_EvaluatorInternal()
    : eval_output(NULL), patch_map(NULL), patch_table(NULL)
{
}

OpenSubdiv_EvaluatorInternal::~OpenSubdiv_EvaluatorInternal()
{
  delete eval_output;
  delete patch_map;
  delete patch_table;
}

OpenSubdiv_EvaluatorInternal *openSubdiv_createEvaluatorInternal(
    OpenSubdiv_TopologyRefiner *topology_refiner)
{
  using opensubdiv_capi::vector;
  TopologyRefiner *refiner = topology_refiner->internal->osd_topology_refiner;
  if (refiner == NULL) {
    // Happens on bad topology.
    return NULL;
  }
  // TODO(sergey): Base this on actual topology.
  const bool has_varying_data = false;
  const int num_face_varying_channels = refiner->GetNumFVarChannels();
  const bool has_face_varying_data = (num_face_varying_channels != 0);
  const int level = topology_refiner->getSubdivisionLevel(topology_refiner);
  const bool is_adaptive = topology_refiner->getIsAdaptive(topology_refiner);
  // Common settings for stencils and patches.
  const bool stencil_generate_intermediate_levels = is_adaptive;
  const bool stencil_generate_offsets = true;
  const bool use_inf_sharp_patch = true;
  // Refine the topology with given settings.
  // TODO(sergey): What if topology is already refined?
  if (is_adaptive) {
    TopologyRefiner::AdaptiveOptions options(level);
    options.considerFVarChannels = has_face_varying_data;
    options.useInfSharpPatch = use_inf_sharp_patch;
    refiner->RefineAdaptive(options);
  }
  else {
    TopologyRefiner::UniformOptions options(level);
    refiner->RefineUniform(options);
  }
  // Generate stencil table to update the bi-cubic patches control vertices
  // after they have been re-posed (both for vertex & varying interpolation).
  //
  // Vertex stencils.
  StencilTableFactory::Options vertex_stencil_options;
  vertex_stencil_options.generateOffsets = stencil_generate_offsets;
  vertex_stencil_options.generateIntermediateLevels = stencil_generate_intermediate_levels;
  const StencilTable *vertex_stencils = StencilTableFactory::Create(*refiner,
                                                                    vertex_stencil_options);
  // Varying stencils.
  //
  // TODO(sergey): Seems currently varying stencils are always required in
  // OpenSubdiv itself.
  const StencilTable *varying_stencils = NULL;
  if (has_varying_data) {
    StencilTableFactory::Options varying_stencil_options;
    varying_stencil_options.generateOffsets = stencil_generate_offsets;
    varying_stencil_options.generateIntermediateLevels = stencil_generate_intermediate_levels;
    varying_stencil_options.interpolationMode = StencilTableFactory::INTERPOLATE_VARYING;
    varying_stencils = StencilTableFactory::Create(*refiner, varying_stencil_options);
  }
  // Face warying stencil.
  vector<const StencilTable *> all_face_varying_stencils;
#ifdef OPENSUBDIV_HAS_FVAR_EVALUATION
  all_face_varying_stencils.reserve(num_face_varying_channels);
  for (int face_varying_channel = 0; face_varying_channel < num_face_varying_channels;
       ++face_varying_channel) {
    StencilTableFactory::Options face_varying_stencil_options;
    face_varying_stencil_options.generateOffsets = stencil_generate_offsets;
    face_varying_stencil_options.generateIntermediateLevels = stencil_generate_intermediate_levels;
    face_varying_stencil_options.interpolationMode = StencilTableFactory::INTERPOLATE_FACE_VARYING;
    face_varying_stencil_options.fvarChannel = face_varying_channel;
    all_face_varying_stencils.push_back(
        StencilTableFactory::Create(*refiner, face_varying_stencil_options));
  }
#endif
  // Generate bi-cubic patch table for the limit surface.
  PatchTableFactory::Options patch_options(level);
  patch_options.SetEndCapType(PatchTableFactory::Options::ENDCAP_GREGORY_BASIS);
  patch_options.useInfSharpPatch = use_inf_sharp_patch;
  patch_options.generateFVarTables = has_face_varying_data;
  patch_options.generateFVarLegacyLinearPatches = false;
  const PatchTable *patch_table = PatchTableFactory::Create(*refiner, patch_options);
  // Append local points stencils.
  // Point stencils.
  const StencilTable *local_point_stencil_table = patch_table->GetLocalPointStencilTable();
  if (local_point_stencil_table != NULL) {
    const StencilTable *table = StencilTableFactory::AppendLocalPointStencilTable(
        *refiner, vertex_stencils, local_point_stencil_table);
    delete vertex_stencils;
    vertex_stencils = table;
  }
  // Varying stencils.
  if (has_varying_data) {
    const StencilTable *local_point_varying_stencil_table =
        patch_table->GetLocalPointVaryingStencilTable();
    if (local_point_varying_stencil_table != NULL) {
      const StencilTable *table = StencilTableFactory::AppendLocalPointStencilTable(
          *refiner, varying_stencils, local_point_varying_stencil_table);
      delete varying_stencils;
      varying_stencils = table;
    }
  }
#ifdef OPENSUBDIV_HAS_FVAR_EVALUATION
  for (int face_varying_channel = 0; face_varying_channel < num_face_varying_channels;
       ++face_varying_channel) {
    const StencilTable *table = StencilTableFactory::AppendLocalPointStencilTableFaceVarying(
        *refiner,
        all_face_varying_stencils[face_varying_channel],
        patch_table->GetLocalPointFaceVaryingStencilTable(face_varying_channel),
        face_varying_channel);
    if (table != NULL) {
      delete all_face_varying_stencils[face_varying_channel];
      all_face_varying_stencils[face_varying_channel] = table;
    }
  }
#endif
  // Create OpenSubdiv's CPU side evaluator.
  // TODO(sergey): Make it possible to use different evaluators.
  opensubdiv_capi::CpuEvalOutput *eval_output = new opensubdiv_capi::CpuEvalOutput(
      vertex_stencils, varying_stencils, all_face_varying_stencils, 2, patch_table);
  OpenSubdiv::Far::PatchMap *patch_map = new PatchMap(*patch_table);
  // Wrap everything we need into an object which we control from our side.
  OpenSubdiv_EvaluatorInternal *evaluator_descr;
  evaluator_descr = OBJECT_GUARDED_NEW(OpenSubdiv_EvaluatorInternal);
  evaluator_descr->eval_output = new opensubdiv_capi::CpuEvalOutputAPI(eval_output, patch_map);
  evaluator_descr->patch_map = patch_map;
  evaluator_descr->patch_table = patch_table;
  // TOOD(sergey): Look into whether we've got duplicated stencils arrays.
  delete vertex_stencils;
  delete varying_stencils;
  foreach (const StencilTable *table, all_face_varying_stencils) {
    delete table;
  }
  return evaluator_descr;
}

void openSubdiv_deleteEvaluatorInternal(OpenSubdiv_EvaluatorInternal *evaluator)
{
  OBJECT_GUARDED_DELETE(evaluator, OpenSubdiv_EvaluatorInternal);
}
