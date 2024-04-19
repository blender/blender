/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Sergey Sharybin. */

#include "internal/evaluator/eval_output_gpu.h"

#include "opensubdiv_evaluator_capi.hh"

using OpenSubdiv::Osd::PatchArray;
using OpenSubdiv::Osd::PatchArrayVector;

namespace blender::opensubdiv {

static void buildPatchArraysBufferFromVector(const PatchArrayVector &patch_arrays,
                                             OpenSubdiv_Buffer *patch_arrays_buffer)
{
  const size_t patch_array_size = sizeof(PatchArray);
  const size_t patch_array_byte_site = patch_array_size * patch_arrays.size();
  patch_arrays_buffer->device_alloc(patch_arrays_buffer, patch_arrays.size());
  patch_arrays_buffer->bind_gpu(patch_arrays_buffer);
  patch_arrays_buffer->device_update(
      patch_arrays_buffer, 0, patch_array_byte_site, &patch_arrays[0]);
}

GpuEvalOutput::GpuEvalOutput(const StencilTable *vertex_stencils,
                             const StencilTable *varying_stencils,
                             const std::vector<const StencilTable *> &all_face_varying_stencils,
                             const int face_varying_width,
                             const PatchTable *patch_table,
                             VolatileEvalOutput::EvaluatorCache *evaluator_cache)
    : VolatileEvalOutput<GLVertexBuffer,
                         GLVertexBuffer,
                         GLStencilTableSSBO,
                         GLPatchTable,
                         GLComputeEvaluator>(vertex_stencils,
                                             varying_stencils,
                                             all_face_varying_stencils,
                                             face_varying_width,
                                             patch_table,
                                             evaluator_cache)
{
}

void GpuEvalOutput::fillPatchArraysBuffer(OpenSubdiv_Buffer *patch_arrays_buffer)
{
  GLPatchTable *patch_table = getPatchTable();
  buildPatchArraysBufferFromVector(patch_table->GetPatchArrays(), patch_arrays_buffer);
}

void GpuEvalOutput::wrapPatchIndexBuffer(OpenSubdiv_Buffer *patch_index_buffer)
{
  GLPatchTable *patch_table = getPatchTable();
  patch_index_buffer->wrap_device_handle(patch_index_buffer, patch_table->GetPatchIndexBuffer());
}

void GpuEvalOutput::wrapPatchParamBuffer(OpenSubdiv_Buffer *patch_param_buffer)
{
  GLPatchTable *patch_table = getPatchTable();
  patch_param_buffer->wrap_device_handle(patch_param_buffer, patch_table->GetPatchParamBuffer());
}

void GpuEvalOutput::wrapSrcBuffer(OpenSubdiv_Buffer *src_buffer)
{
  GLVertexBuffer *vertex_buffer = getSrcBuffer();
  src_buffer->wrap_device_handle(src_buffer, vertex_buffer->BindVBO());
}

void GpuEvalOutput::wrapSrcVertexDataBuffer(OpenSubdiv_Buffer *src_buffer)
{
  GLVertexBuffer *vertex_buffer = getSrcVertexDataBuffer();
  src_buffer->wrap_device_handle(src_buffer, vertex_buffer->BindVBO());
}

void GpuEvalOutput::fillFVarPatchArraysBuffer(const int face_varying_channel,
                                              OpenSubdiv_Buffer *patch_arrays_buffer)
{
  GLPatchTable *patch_table = getFVarPatchTable(face_varying_channel);
  buildPatchArraysBufferFromVector(patch_table->GetFVarPatchArrays(face_varying_channel),
                                   patch_arrays_buffer);
}

void GpuEvalOutput::wrapFVarPatchIndexBuffer(const int face_varying_channel,
                                             OpenSubdiv_Buffer *patch_index_buffer)
{
  GLPatchTable *patch_table = getFVarPatchTable(face_varying_channel);
  patch_index_buffer->wrap_device_handle(
      patch_index_buffer, patch_table->GetFVarPatchIndexBuffer(face_varying_channel));
}

void GpuEvalOutput::wrapFVarPatchParamBuffer(const int face_varying_channel,
                                             OpenSubdiv_Buffer *patch_param_buffer)
{
  GLPatchTable *patch_table = getFVarPatchTable(face_varying_channel);
  patch_param_buffer->wrap_device_handle(
      patch_param_buffer, patch_table->GetFVarPatchParamBuffer(face_varying_channel));
}

void GpuEvalOutput::wrapFVarSrcBuffer(const int face_varying_channel,
                                      OpenSubdiv_Buffer *src_buffer)
{
  GLVertexBuffer *vertex_buffer = getFVarSrcBuffer(face_varying_channel);
  src_buffer->buffer_offset = getFVarSrcBufferOffset(face_varying_channel);
  src_buffer->wrap_device_handle(src_buffer, vertex_buffer->BindVBO());
}

}  // namespace blender::opensubdiv
