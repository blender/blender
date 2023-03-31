// Copyright 2021 Blender Foundation
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

#include "internal/evaluator/eval_output.h"

namespace blender {
namespace opensubdiv {

bool is_adaptive(CpuPatchTable *patch_table)
{
  return patch_table->GetPatchArrayBuffer()[0].GetDescriptor().IsAdaptive();
}

bool is_adaptive(GLPatchTable *patch_table)
{
  return patch_table->GetPatchArrays()[0].GetDescriptor().IsAdaptive();
}

}  // namespace opensubdiv
}  // namespace blender
