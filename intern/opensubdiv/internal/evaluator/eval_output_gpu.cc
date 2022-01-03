// Copyright 2021 Blender Foundation. All rights reserved.
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

#include "internal/evaluator/eval_output_gpu.h"

#include "opensubdiv_evaluator_capi.h"

using OpenSubdiv::Osd::PatchArray;
using OpenSubdiv::Osd::PatchArrayVector;

namespace blender {
namespace opensubdiv {

namespace {

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

}  // namespace

GpuEvalOutput::GpuEvalOutput(const StencilTable *vertex_stencils,
                             const StencilTable *varying_stencils,
                             const vector<const StencilTable *> &all_face_varying_stencils,
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

}  // namespace opensubdiv
}  // namespace blender
