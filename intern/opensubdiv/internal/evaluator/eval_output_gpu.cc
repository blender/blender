/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Sergey Sharybin. */

#include "internal/evaluator/eval_output_gpu.h"

#include "opensubdiv_evaluator.hh"

#include "gpu_patch_table.hh"

using OpenSubdiv::Osd::PatchArray;
using OpenSubdiv::Osd::PatchArrayVector;

namespace blender::opensubdiv {

static GPUStorageBuf *create_patch_array_buffer(const PatchArrayVector &patch_arrays)
{
  const size_t patch_array_size = sizeof(PatchArray);
  const size_t patch_array_byte_size = patch_array_size * patch_arrays.size();
  GPUStorageBuf *storage_buf = GPU_storagebuf_create_ex(
      patch_array_byte_size, patch_arrays.data(), GPU_USAGE_STATIC, "osd_patch_array");
  return storage_buf;
}

GpuEvalOutput::GpuEvalOutput(const StencilTable *vertex_stencils,
                             const StencilTable *varying_stencils,
                             const std::vector<const StencilTable *> &all_face_varying_stencils,
                             const int face_varying_width,
                             const PatchTable *patch_table,
                             VolatileEvalOutput::EvaluatorCache *evaluator_cache)
    : VolatileEvalOutput<GPUVertexBuffer,
                         GPUVertexBuffer,
                         GPUStencilTableSSBO,
                         GPUPatchTable,
                         GPUComputeEvaluator>(vertex_stencils,
                                              varying_stencils,
                                              all_face_varying_stencils,
                                              face_varying_width,
                                              patch_table,
                                              evaluator_cache)
{
}

GPUStorageBuf *GpuEvalOutput::create_patch_arrays_buf()
{
  GPUPatchTable *patch_table = getPatchTable();
  return create_patch_array_buffer(patch_table->GetPatchArrays());
}

GPUStorageBuf *GpuEvalOutput::create_face_varying_patch_array_buf(const int face_varying_channel)
{
  GPUPatchTable *patch_table = getFVarPatchTable(face_varying_channel);
  return create_patch_array_buffer(patch_table->GetFVarPatchArrays(face_varying_channel));
}

}  // namespace blender::opensubdiv
