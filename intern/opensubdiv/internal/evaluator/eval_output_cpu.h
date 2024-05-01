/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Sergey Sharybin. */

#ifndef OPENSUBDIV_EVAL_OUTPUT_CPU_H_
#define OPENSUBDIV_EVAL_OUTPUT_CPU_H_

#include "internal/evaluator/eval_output.h"

#include <opensubdiv/osd/cpuEvaluator.h>
#include <opensubdiv/osd/cpuPatchTable.h>
#include <opensubdiv/osd/cpuVertexBuffer.h>

using OpenSubdiv::Far::StencilTable;
using OpenSubdiv::Osd::CpuEvaluator;
using OpenSubdiv::Osd::CpuVertexBuffer;

namespace blender::opensubdiv {

// NOTE: Define as a class instead of typedef to make it possible
// to have anonymous class in opensubdiv_evaluator_internal.h
class CpuEvalOutput : public VolatileEvalOutput<CpuVertexBuffer,
                                                CpuVertexBuffer,
                                                StencilTable,
                                                CpuPatchTable,
                                                CpuEvaluator> {
 public:
  CpuEvalOutput(const StencilTable *vertex_stencils,
                const StencilTable *varying_stencils,
                const std::vector<const StencilTable *> &all_face_varying_stencils,
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

}  // namespace blender::opensubdiv

#endif  // OPENSUBDIV_EVAL_OUTPUT_CPU_H_
