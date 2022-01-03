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

#ifndef OPENSUBDIV_EVAL_OUTPUT_CPU_H_
#define OPENSUBDIV_EVAL_OUTPUT_CPU_H_

#include "internal/evaluator/eval_output.h"

#include <opensubdiv/osd/cpuEvaluator.h>
#include <opensubdiv/osd/cpuPatchTable.h>
#include <opensubdiv/osd/cpuVertexBuffer.h>

using OpenSubdiv::Far::StencilTable;
using OpenSubdiv::Osd::CpuEvaluator;
using OpenSubdiv::Osd::CpuVertexBuffer;

namespace blender {
namespace opensubdiv {

// Note: Define as a class instead of typedef to make it possible
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

}  // namespace opensubdiv
}  // namespace blender

#endif  // OPENSUBDIV_EVAL_OUTPUT_CPU_H_
