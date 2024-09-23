/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Sergey Sharybin. */

#include "internal/evaluator/eval_output.h"

namespace blender::opensubdiv {
bool is_adaptive(const CpuPatchTable *patch_table)
{
  return patch_table->GetPatchArrayBuffer()[0].GetDescriptor().IsAdaptive();
}

bool is_adaptive(const GLPatchTable *patch_table)
{
  return patch_table->GetPatchArrays()[0].GetDescriptor().IsAdaptive();
}

}  // namespace blender::opensubdiv
