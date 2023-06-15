/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "internal/evaluator/evaluator_cache_impl.h"

#include "internal/evaluator/eval_output_gpu.h"

OpenSubdiv_EvaluatorCacheImpl::OpenSubdiv_EvaluatorCacheImpl() {}

OpenSubdiv_EvaluatorCacheImpl::~OpenSubdiv_EvaluatorCacheImpl()
{
  delete static_cast<blender::opensubdiv::GpuEvalOutput::EvaluatorCache *>(eval_cache);
}

OpenSubdiv_EvaluatorCacheImpl *openSubdiv_createEvaluatorCacheInternal(
    eOpenSubdivEvaluator evaluator_type)
{
  if (evaluator_type != eOpenSubdivEvaluator::OPENSUBDIV_EVALUATOR_GPU) {
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
