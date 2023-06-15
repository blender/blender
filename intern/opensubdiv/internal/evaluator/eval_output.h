/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Sergey Sharybin. */

#ifndef OPENSUBDIV_EVAL_OUTPUT_H_
#define OPENSUBDIV_EVAL_OUTPUT_H_

#include <opensubdiv/osd/cpuPatchTable.h>
#include <opensubdiv/osd/glPatchTable.h>
#include <opensubdiv/osd/mesh.h>
#include <opensubdiv/osd/types.h>

#include "internal/base/type.h"
#include "internal/evaluator/evaluator_impl.h"

#include "opensubdiv_evaluator_capi.h"

using OpenSubdiv::Far::PatchTable;
using OpenSubdiv::Far::StencilTable;
using OpenSubdiv::Osd::BufferDescriptor;
using OpenSubdiv::Osd::CpuPatchTable;
using OpenSubdiv::Osd::GLPatchTable;
using OpenSubdiv::Osd::PatchCoord;

namespace blender {
namespace opensubdiv {

// Base class for the implementation of the evaluators.
class EvalOutputAPI::EvalOutput {
 public:
  virtual ~EvalOutput() = default;

  virtual void updateSettings(const OpenSubdiv_EvaluatorSettings *settings) = 0;

  virtual void updateData(const float *src, int start_vertex, int num_vertices) = 0;

  virtual void updateVaryingData(const float *src, int start_vertex, int num_vertices) = 0;

  virtual void updateVertexData(const float *src, int start_vertex, int num_vertices) = 0;

  virtual void updateFaceVaryingData(const int face_varying_channel,
                                     const float *src,
                                     int start_vertex,
                                     int num_vertices) = 0;

  virtual void refine() = 0;

  // NOTE: P must point to a memory of at least float[3]*num_patch_coords.
  virtual void evalPatches(const PatchCoord *patch_coord,
                           const int num_patch_coords,
                           float *P) = 0;

  // NOTE: P, dPdu, dPdv must point to a memory of at least float[3]*num_patch_coords.
  virtual void evalPatchesWithDerivatives(const PatchCoord *patch_coord,
                                          const int num_patch_coords,
                                          float *P,
                                          float *dPdu,
                                          float *dPdv) = 0;

  // NOTE: varying must point to a memory of at least float[3]*num_patch_coords.
  virtual void evalPatchesVarying(const PatchCoord *patch_coord,
                                  const int num_patch_coords,
                                  float *varying) = 0;

  // NOTE: vertex_data must point to a memory of at least float*num_vertex_data.
  virtual void evalPatchesVertexData(const PatchCoord *patch_coord,
                                     const int num_patch_coords,
                                     float *vertex_data) = 0;

  virtual void evalPatchesFaceVarying(const int face_varying_channel,
                                      const PatchCoord *patch_coord,
                                      const int num_patch_coords,
                                      float face_varying[2]) = 0;

  // The following interfaces are dependant on the actual evaluator type (CPU, OpenGL, etc.) which
  // have slightly different APIs to access patch arrays, as well as different types for their
  // data structure. They need to be overridden in the specific instances of the EvalOutput derived
  // classes if needed, while the interfaces above are overridden through VolatileEvalOutput.

  virtual void fillPatchArraysBuffer(OpenSubdiv_Buffer * /*patch_arrays_buffer*/) {}

  virtual void wrapPatchIndexBuffer(OpenSubdiv_Buffer * /*patch_index_buffer*/) {}

  virtual void wrapPatchParamBuffer(OpenSubdiv_Buffer * /*patch_param_buffer*/) {}

  virtual void wrapSrcBuffer(OpenSubdiv_Buffer * /*src_buffer*/) {}

  virtual void wrapSrcVertexDataBuffer(OpenSubdiv_Buffer * /*src_buffer*/) {}

  virtual void fillFVarPatchArraysBuffer(const int /*face_varying_channel*/,
                                         OpenSubdiv_Buffer * /*patch_arrays_buffer*/)
  {
  }

  virtual void wrapFVarPatchIndexBuffer(const int /*face_varying_channel*/,
                                        OpenSubdiv_Buffer * /*patch_index_buffer*/)
  {
  }

  virtual void wrapFVarPatchParamBuffer(const int /*face_varying_channel*/,
                                        OpenSubdiv_Buffer * /*patch_param_buffer*/)
  {
  }

  virtual void wrapFVarSrcBuffer(const int /*face_varying_channel*/,
                                 OpenSubdiv_Buffer * /*src_buffer*/)
  {
  }

  virtual bool hasVertexData() const
  {
    return false;
  }
};

namespace {

// Buffer which implements API required by OpenSubdiv and uses an existing memory as an underlying
// storage.
template<typename T> class RawDataWrapperBuffer {
 public:
  RawDataWrapperBuffer(T *data) : data_(data) {}

  T *BindCpuBuffer()
  {
    return data_;
  }

  int BindVBO()
  {
    return 0;
  }

  // TODO(sergey): Support UpdateData().

 protected:
  T *data_;
};

template<typename T> class RawDataWrapperVertexBuffer : public RawDataWrapperBuffer<T> {
 public:
  RawDataWrapperVertexBuffer(T *data, int num_vertices)
      : RawDataWrapperBuffer<T>(data), num_vertices_(num_vertices)
  {
  }

  int GetNumVertices()
  {
    return num_vertices_;
  }

 protected:
  int num_vertices_;
};

class ConstPatchCoordWrapperBuffer : public RawDataWrapperVertexBuffer<const PatchCoord> {
 public:
  ConstPatchCoordWrapperBuffer(const PatchCoord *data, int num_vertices)
      : RawDataWrapperVertexBuffer(data, num_vertices)
  {
  }
};
}  // namespace

// Discriminators used in FaceVaryingVolatileEval in order to detect whether we are using adaptive
// patches as the CPU and OpenGL PatchTable have different APIs.
bool is_adaptive(CpuPatchTable *patch_table);
bool is_adaptive(GLPatchTable *patch_table);

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
    // in and out points to same buffer so output is put directly after coarse vertices, needed in
    // adaptive mode
    EVALUATOR::EvalStencils(src_face_varying_data_,
                            src_face_varying_desc_,
                            src_face_varying_data_,
                            dst_face_varying_desc,
                            face_varying_stencils_,
                            eval_instance,
                            device_context_);
  }

  // NOTE: face_varying must point to a memory of at least float[2]*num_patch_coords.
  void evalPatches(const PatchCoord *patch_coord, const int num_patch_coords, float *face_varying)
  {
    RawDataWrapperBuffer<float> face_varying_data(face_varying);
    BufferDescriptor face_varying_desc(0, 2, 2);
    ConstPatchCoordWrapperBuffer patch_coord_buffer(patch_coord, num_patch_coords);
    const EVALUATOR *eval_instance = OpenSubdiv::Osd::GetEvaluator<EVALUATOR>(
        evaluator_cache_, src_face_varying_desc_, face_varying_desc, device_context_);

    BufferDescriptor src_desc = get_src_varying_desc();

    EVALUATOR::EvalPatchesFaceVarying(src_face_varying_data_,
                                      src_desc,
                                      &face_varying_data,
                                      face_varying_desc,
                                      patch_coord_buffer.GetNumVertices(),
                                      &patch_coord_buffer,
                                      patch_table_,
                                      face_varying_channel_,
                                      eval_instance,
                                      device_context_);
  }

  EVAL_VERTEX_BUFFER *getSrcBuffer() const
  {
    return src_face_varying_data_;
  }

  int getFVarSrcBufferOffset() const
  {
    BufferDescriptor src_desc = get_src_varying_desc();
    return src_desc.offset;
  }

  PATCH_TABLE *getPatchTable() const
  {
    return patch_table_;
  }

 private:
  BufferDescriptor get_src_varying_desc() const
  {
    // src_face_varying_data_ always contains coarse vertices at the beginning.
    // In adaptive mode they are followed by number of blocks for intermediate
    // subdivision levels, and this is what OSD expects in this mode.
    // In non-adaptive mode (generateIntermediateLevels == false),
    // they are followed by max subdivision level, but they break interpolation as OSD
    // expects only one subd level in this buffer.
    // So in non-adaptive mode we put offset into buffer descriptor to skip over coarse vertices.
    BufferDescriptor src_desc = src_face_varying_desc_;
    if (!is_adaptive(patch_table_)) {
      src_desc.offset += num_coarse_face_varying_vertices_ * src_face_varying_desc_.stride;
    }
    return src_desc;
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
class VolatileEvalOutput : public EvalOutputAPI::EvalOutput {
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
      : src_vertex_data_(NULL),
        src_desc_(0, 3, 3),
        src_varying_desc_(0, 3, 3),
        src_vertex_data_desc_(0, 0, 0),
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
    vertex_stencils_ = convertToCompatibleStencilTable<STENCIL_TABLE>(vertex_stencils,
                                                                      device_context_);
    varying_stencils_ = convertToCompatibleStencilTable<STENCIL_TABLE>(varying_stencils,
                                                                       device_context_);

    // Create evaluators for every face varying channel.
    face_varying_evaluators_.reserve(all_face_varying_stencils.size());
    int face_varying_channel = 0;
    for (const StencilTable *face_varying_stencils : all_face_varying_stencils) {
      face_varying_evaluators_.push_back(new FaceVaryingEval(face_varying_channel,
                                                             face_varying_stencils,
                                                             face_varying_width,
                                                             patch_table_,
                                                             evaluator_cache_,
                                                             device_context_));
      ++face_varying_channel;
    }
  }

  ~VolatileEvalOutput() override
  {
    delete src_data_;
    delete src_varying_data_;
    delete src_vertex_data_;
    delete patch_table_;
    delete vertex_stencils_;
    delete varying_stencils_;
    for (FaceVaryingEval *face_varying_evaluator : face_varying_evaluators_) {
      delete face_varying_evaluator;
    }
  }

  void updateSettings(const OpenSubdiv_EvaluatorSettings *settings) override
  {
    // Optionally allocate additional data to be subdivided like vertex coordinates.
    if (settings->num_vertex_data != src_vertex_data_desc_.length) {
      delete src_vertex_data_;
      if (settings->num_vertex_data > 0) {
        src_vertex_data_ = SRC_VERTEX_BUFFER::Create(
            settings->num_vertex_data, src_data_->GetNumVertices(), device_context_);
      }
      else {
        src_vertex_data_ = NULL;
      }
      src_vertex_data_desc_ = BufferDescriptor(
          0, settings->num_vertex_data, settings->num_vertex_data);
    }
  }

  // TODO(sergey): Implement binding API.

  void updateData(const float *src, int start_vertex, int num_vertices) override
  {
    src_data_->UpdateData(src, start_vertex, num_vertices, device_context_);
  }

  void updateVaryingData(const float *src, int start_vertex, int num_vertices) override
  {
    src_varying_data_->UpdateData(src, start_vertex, num_vertices, device_context_);
  }

  void updateVertexData(const float *src, int start_vertex, int num_vertices) override
  {
    src_vertex_data_->UpdateData(src, start_vertex, num_vertices, device_context_);
  }

  void updateFaceVaryingData(const int face_varying_channel,
                             const float *src,
                             int start_vertex,
                             int num_vertices) override
  {
    assert(face_varying_channel >= 0);
    assert(face_varying_channel < face_varying_evaluators_.size());
    face_varying_evaluators_[face_varying_channel]->updateData(src, start_vertex, num_vertices);
  }

  bool hasVaryingData() const
  {
    // return varying_stencils_ != NULL;
    // TODO(sergey): Check this based on actual topology.
    return false;
  }

  bool hasFaceVaryingData() const
  {
    return face_varying_evaluators_.size() != 0;
  }

  bool hasVertexData() const override
  {
    return src_vertex_data_ != nullptr;
  }

  void refine() override
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

    // Evaluate smoothly interpolated vertex data.
    if (src_vertex_data_) {
      BufferDescriptor dst_vertex_data_desc = src_vertex_data_desc_;
      dst_vertex_data_desc.offset += num_coarse_vertices_ * src_vertex_data_desc_.stride;
      const EVALUATOR *eval_instance = OpenSubdiv::Osd::GetEvaluator<EVALUATOR>(
          evaluator_cache_, src_vertex_data_desc_, dst_vertex_data_desc, device_context_);
      EVALUATOR::EvalStencils(src_vertex_data_,
                              src_vertex_data_desc_,
                              src_vertex_data_,
                              dst_vertex_data_desc,
                              vertex_stencils_,
                              eval_instance,
                              device_context_);
    }

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
      for (FaceVaryingEval *face_varying_evaluator : face_varying_evaluators_) {
        face_varying_evaluator->refine();
      }
    }
  }

  // NOTE: P must point to a memory of at least float[3]*num_patch_coords.
  void evalPatches(const PatchCoord *patch_coord, const int num_patch_coords, float *P) override
  {
    RawDataWrapperBuffer<float> P_data(P);
    // TODO(sergey): Support interleaved vertex-varying data.
    BufferDescriptor P_desc(0, 3, 3);
    ConstPatchCoordWrapperBuffer patch_coord_buffer(patch_coord, num_patch_coords);
    const EVALUATOR *eval_instance = OpenSubdiv::Osd::GetEvaluator<EVALUATOR>(
        evaluator_cache_, src_desc_, P_desc, device_context_);
    EVALUATOR::EvalPatches(src_data_,
                           src_desc_,
                           &P_data,
                           P_desc,
                           patch_coord_buffer.GetNumVertices(),
                           &patch_coord_buffer,
                           patch_table_,
                           eval_instance,
                           device_context_);
  }

  // NOTE: P, dPdu, dPdv must point to a memory of at least float[3]*num_patch_coords.
  void evalPatchesWithDerivatives(const PatchCoord *patch_coord,
                                  const int num_patch_coords,
                                  float *P,
                                  float *dPdu,
                                  float *dPdv) override
  {
    assert(dPdu);
    assert(dPdv);
    RawDataWrapperBuffer<float> P_data(P);
    RawDataWrapperBuffer<float> dPdu_data(dPdu), dPdv_data(dPdv);
    // TODO(sergey): Support interleaved vertex-varying data.
    BufferDescriptor P_desc(0, 3, 3);
    BufferDescriptor dpDu_desc(0, 3, 3), pPdv_desc(0, 3, 3);
    ConstPatchCoordWrapperBuffer patch_coord_buffer(patch_coord, num_patch_coords);
    const EVALUATOR *eval_instance = OpenSubdiv::Osd::GetEvaluator<EVALUATOR>(
        evaluator_cache_, src_desc_, P_desc, dpDu_desc, pPdv_desc, device_context_);
    EVALUATOR::EvalPatches(src_data_,
                           src_desc_,
                           &P_data,
                           P_desc,
                           &dPdu_data,
                           dpDu_desc,
                           &dPdv_data,
                           pPdv_desc,
                           patch_coord_buffer.GetNumVertices(),
                           &patch_coord_buffer,
                           patch_table_,
                           eval_instance,
                           device_context_);
  }

  // NOTE: varying must point to a memory of at least float[3]*num_patch_coords.
  void evalPatchesVarying(const PatchCoord *patch_coord,
                          const int num_patch_coords,
                          float *varying) override
  {
    RawDataWrapperBuffer<float> varying_data(varying);
    BufferDescriptor varying_desc(3, 3, 6);
    ConstPatchCoordWrapperBuffer patch_coord_buffer(patch_coord, num_patch_coords);
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
  }

  // NOTE: data must point to a memory of at least float*num_vertex_data.
  void evalPatchesVertexData(const PatchCoord *patch_coord,
                             const int num_patch_coords,
                             float *data) override
  {
    RawDataWrapperBuffer<float> vertex_data(data);
    BufferDescriptor vertex_desc(0, src_vertex_data_desc_.length, src_vertex_data_desc_.length);
    ConstPatchCoordWrapperBuffer patch_coord_buffer(patch_coord, num_patch_coords);
    const EVALUATOR *eval_instance = OpenSubdiv::Osd::GetEvaluator<EVALUATOR>(
        evaluator_cache_, src_vertex_data_desc_, vertex_desc, device_context_);
    EVALUATOR::EvalPatches(src_vertex_data_,
                           src_vertex_data_desc_,
                           &vertex_data,
                           vertex_desc,
                           patch_coord_buffer.GetNumVertices(),
                           &patch_coord_buffer,
                           patch_table_,
                           eval_instance,
                           device_context_);
  }

  void evalPatchesFaceVarying(const int face_varying_channel,
                              const PatchCoord *patch_coord,
                              const int num_patch_coords,
                              float face_varying[2]) override
  {
    assert(face_varying_channel >= 0);
    assert(face_varying_channel < face_varying_evaluators_.size());
    face_varying_evaluators_[face_varying_channel]->evalPatches(
        patch_coord, num_patch_coords, face_varying);
  }

  SRC_VERTEX_BUFFER *getSrcBuffer() const
  {
    return src_data_;
  }

  SRC_VERTEX_BUFFER *getSrcVertexDataBuffer() const
  {
    return src_vertex_data_;
  }

  PATCH_TABLE *getPatchTable() const
  {
    return patch_table_;
  }

  SRC_VERTEX_BUFFER *getFVarSrcBuffer(const int face_varying_channel) const
  {
    return face_varying_evaluators_[face_varying_channel]->getSrcBuffer();
  }

  int getFVarSrcBufferOffset(const int face_varying_channel) const
  {
    return face_varying_evaluators_[face_varying_channel]->getFVarSrcBufferOffset();
  }

  PATCH_TABLE *getFVarPatchTable(const int face_varying_channel) const
  {
    return face_varying_evaluators_[face_varying_channel]->getPatchTable();
  }

 private:
  SRC_VERTEX_BUFFER *src_data_;
  SRC_VERTEX_BUFFER *src_varying_data_;
  SRC_VERTEX_BUFFER *src_vertex_data_;
  PATCH_TABLE *patch_table_;
  BufferDescriptor src_desc_;
  BufferDescriptor src_varying_desc_;
  BufferDescriptor src_vertex_data_desc_;

  int num_coarse_vertices_;

  const STENCIL_TABLE *vertex_stencils_;
  const STENCIL_TABLE *varying_stencils_;

  int face_varying_width_;
  vector<FaceVaryingEval *> face_varying_evaluators_;

  EvaluatorCache *evaluator_cache_;
  DEVICE_CONTEXT *device_context_;
};

}  // namespace opensubdiv
}  // namespace blender

#endif  // OPENSUBDIV_EVAL_OUTPUT_H_
