/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Sergey Sharybin. */

#ifndef OPENSUBDIV_EVAL_OUTPUT_GPU_H_
#define OPENSUBDIV_EVAL_OUTPUT_GPU_H_

#include "internal/evaluator/eval_output.h"
#include "internal/evaluator/gl_compute_evaluator.h"

#include <opensubdiv/osd/glPatchTable.h>
#include <opensubdiv/osd/glVertexBuffer.h>

using OpenSubdiv::Osd::GLVertexBuffer;

namespace blender {
namespace opensubdiv {

class GpuEvalOutput : public VolatileEvalOutput<GLVertexBuffer,
                                                GLVertexBuffer,
                                                GLStencilTableSSBO,
                                                GLPatchTable,
                                                GLComputeEvaluator> {
 public:
  GpuEvalOutput(const StencilTable *vertex_stencils,
                const StencilTable *varying_stencils,
                const std::vector<const StencilTable *> &all_face_varying_stencils,
                const int face_varying_width,
                const PatchTable *patch_table,
                EvaluatorCache *evaluator_cache = NULL);

  void fillPatchArraysBuffer(OpenSubdiv_Buffer *patch_arrays_buffer) override;

  void wrapPatchIndexBuffer(OpenSubdiv_Buffer *patch_index_buffer) override;

  void wrapPatchParamBuffer(OpenSubdiv_Buffer *patch_param_buffer) override;

  void wrapSrcBuffer(OpenSubdiv_Buffer *src_buffer) override;

  void wrapSrcVertexDataBuffer(OpenSubdiv_Buffer *src_buffer) override;

  void fillFVarPatchArraysBuffer(const int face_varying_channel,
                                 OpenSubdiv_Buffer *patch_arrays_buffer) override;

  void wrapFVarPatchIndexBuffer(const int face_varying_channel,
                                OpenSubdiv_Buffer *patch_index_buffer) override;

  void wrapFVarPatchParamBuffer(const int face_varying_channel,
                                OpenSubdiv_Buffer *patch_param_buffer) override;

  void wrapFVarSrcBuffer(const int face_varying_channel, OpenSubdiv_Buffer *src_buffer) override;
};

}  // namespace opensubdiv
}  // namespace blender

#endif  // OPENSUBDIV_EVAL_OUTPUT_GPU_H_
