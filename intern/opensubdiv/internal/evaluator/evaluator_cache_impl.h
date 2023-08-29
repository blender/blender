/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef OPENSUBDIV_EVALUATOR_CACHE_IMPL_H_
#define OPENSUBDIV_EVALUATOR_CACHE_IMPL_H_

#include "internal/base/memory.h"

#include "opensubdiv_capi_type.h"

struct OpenSubdiv_EvaluatorCacheImpl {
 public:
  OpenSubdiv_EvaluatorCacheImpl();
  ~OpenSubdiv_EvaluatorCacheImpl();

  void *eval_cache;
  MEM_CXX_CLASS_ALLOC_FUNCS("OpenSubdiv_EvaluatorCacheImpl");
};

OpenSubdiv_EvaluatorCacheImpl *openSubdiv_createEvaluatorCacheInternal(
    eOpenSubdivEvaluator evaluator_type);

void openSubdiv_deleteEvaluatorCacheInternal(OpenSubdiv_EvaluatorCacheImpl *evaluator_cache);

#endif  // OPENSUBDIV_EVALUATOR_CACHE_IMPL_H_
