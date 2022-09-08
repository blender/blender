/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

CCL_NAMESPACE_BEGIN

/* Closure */

enum ClosureTypeOSL {
  OSL_CLOSURE_MUL_ID = -1,
  OSL_CLOSURE_ADD_ID = -2,

  OSL_CLOSURE_NONE_ID = 0,

#define OSL_CLOSURE_STRUCT_BEGIN(Upper, lower) OSL_CLOSURE_##Upper##_ID,
#include "closures_template.h"
};


CCL_NAMESPACE_END
