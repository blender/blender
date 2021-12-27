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

#include "internal/evaluator/evaluator_cache_impl.h"

#include "internal/evaluator/eval_output_gpu.h"

OpenSubdiv_EvaluatorCacheImpl::OpenSubdiv_EvaluatorCacheImpl()
{
}

OpenSubdiv_EvaluatorCacheImpl::~OpenSubdiv_EvaluatorCacheImpl()
{
  delete static_cast<blender::opensubdiv::GpuEvalOutput::EvaluatorCache *>(eval_cache);
}

OpenSubdiv_EvaluatorCacheImpl *openSubdiv_createEvaluatorCacheInternal(
    eOpenSubdivEvaluator evaluator_type)
{
  if (evaluator_type != eOpenSubdivEvaluator::OPENSUBDIV_EVALUATOR_GLSL_COMPUTE) {
    return nullptr;
  }
  OpenSubdiv_EvaluatorCacheImpl *evaluator_cache;
  evaluator_cache = new OpenSubdiv_EvaluatorCacheImpl;
  blender::opensubdiv::GpuEvalOutput::EvaluatorCache *eval_cache;
  eval_cache = new blender::opensubdiv::GpuEvalOutput::EvaluatorCache();
  evaluator_cache->eval_cache = eval_cache;
  return evaluator_cache;
}

void openSubdiv_deleteEvaluatorCacheInternal(OpenSubdiv_EvaluatorCacheImpl *evaluator_cache)
{
  delete evaluator_cache;
}
