/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Sergey Sharybin. */

#include "internal/evaluator/eval_output_gpu.h"

#include "opensubdiv_evaluator.hh"

using OpenSubdiv::Osd::PatchArray;
using OpenSubdiv::Osd::PatchArrayVector;

namespace blender::opensubdiv {

static void buildPatchArraysBufferFromVector(const PatchArrayVector &patch_arrays,
                                             blender::gpu::VertBuf *patch_arrays_buffer)
{
  const size_t patch_array_size = sizeof(PatchArray);
  const size_t patch_array_byte_site = patch_array_size * patch_arrays.size();
  GPU_vertbuf_data_alloc(*patch_arrays_buffer, patch_arrays.size());
  GPU_vertbuf_use(patch_arrays_buffer);
  GPU_vertbuf_update_sub(patch_arrays_buffer, 0, patch_array_byte_site, patch_arrays.data());
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

void GpuEvalOutput::fillPatchArraysBuffer(blender::gpu::VertBuf *patch_arrays_buffer)
{
  GLPatchTable *patch_table = getPatchTable();
  buildPatchArraysBufferFromVector(patch_table->GetPatchArrays(), patch_arrays_buffer);
}

void GpuEvalOutput::wrapPatchIndexBuffer(blender::gpu::VertBuf *patch_index_buffer)
{
  GLPatchTable *patch_table = getPatchTable();
  GPU_vertbuf_wrap_handle(patch_index_buffer, patch_table->GetPatchIndexBuffer());
}

void GpuEvalOutput::wrapPatchParamBuffer(blender::gpu::VertBuf *patch_param_buffer)
{
  GLPatchTable *patch_table = getPatchTable();
  GPU_vertbuf_wrap_handle(patch_param_buffer, patch_table->GetPatchParamBuffer());
}

void GpuEvalOutput::wrapSrcBuffer(blender::gpu::VertBuf *src_buffer)
{
  GLVertexBuffer *vertex_buffer = getSrcBuffer();
  GPU_vertbuf_wrap_handle(src_buffer, vertex_buffer->BindVBO());
}

void GpuEvalOutput::wrapSrcVertexDataBuffer(blender::gpu::VertBuf *src_buffer)
{
  GLVertexBuffer *vertex_buffer = getSrcVertexDataBuffer();
  GPU_vertbuf_wrap_handle(src_buffer, vertex_buffer->BindVBO());
}

void GpuEvalOutput::fillFVarPatchArraysBuffer(const int face_varying_channel,
                                              blender::gpu::VertBuf *patch_arrays_buffer)
{
  GLPatchTable *patch_table = getFVarPatchTable(face_varying_channel);
  buildPatchArraysBufferFromVector(patch_table->GetFVarPatchArrays(face_varying_channel),
                                   patch_arrays_buffer);
}

void GpuEvalOutput::wrapFVarPatchIndexBuffer(const int face_varying_channel,
                                             blender::gpu::VertBuf *patch_index_buffer)
{
  GLPatchTable *patch_table = getFVarPatchTable(face_varying_channel);
  GPU_vertbuf_wrap_handle(patch_index_buffer,
                          patch_table->GetFVarPatchIndexBuffer(face_varying_channel));
}

void GpuEvalOutput::wrapFVarPatchParamBuffer(const int face_varying_channel,
                                             blender::gpu::VertBuf *patch_param_buffer)
{
  GLPatchTable *patch_table = getFVarPatchTable(face_varying_channel);
  GPU_vertbuf_wrap_handle(patch_param_buffer,
                          patch_table->GetFVarPatchParamBuffer(face_varying_channel));
}

void GpuEvalOutput::wrapFVarSrcBuffer(const int face_varying_channel,
                                      blender::gpu::VertBuf *src_buffer)
{
  GLVertexBuffer *vertex_buffer = getFVarSrcBuffer(face_varying_channel);
  GPU_vertbuf_wrap_handle(src_buffer, vertex_buffer->BindVBO());
}

}  // namespace blender::opensubdiv
