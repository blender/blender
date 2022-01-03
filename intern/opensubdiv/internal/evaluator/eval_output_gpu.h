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

#ifndef OPENSUBDIV_EVAL_OUTPUT_GPU_H_
#define OPENSUBDIV_EVAL_OUTPUT_GPU_H_

#include "internal/evaluator/eval_output.h"

#include <opensubdiv/osd/glComputeEvaluator.h>
#include <opensubdiv/osd/glPatchTable.h>
#include <opensubdiv/osd/glVertexBuffer.h>

using OpenSubdiv::Osd::GLComputeEvaluator;
using OpenSubdiv::Osd::GLStencilTableSSBO;
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
                const vector<const StencilTable *> &all_face_varying_stencils,
                const int face_varying_width,
                const PatchTable *patch_table,
                EvaluatorCache *evaluator_cache = NULL);

  void fillPatchArraysBuffer(OpenSubdiv_Buffer *patch_arrays_buffer) override;

  void wrapPatchIndexBuffer(OpenSubdiv_Buffer *patch_index_buffer) override;

  void wrapPatchParamBuffer(OpenSubdiv_Buffer *patch_param_buffer) override;

  void wrapSrcBuffer(OpenSubdiv_Buffer *src_buffer) override;

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
