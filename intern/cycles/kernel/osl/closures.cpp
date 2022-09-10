/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted from Open Shading Language
 * Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.
 * All Rights Reserved.
 *
 * Modifications Copyright 2011-2022 Blender Foundation. */

#include <OSL/genclosure.h>
#include <OSL/oslclosure.h>

#include "kernel/types.h"

#include "kernel/osl/globals.h"
#include "kernel/osl/services.h"

#include "util/math.h"
#include "util/param.h"

#include "kernel/device/cpu/compat.h"
#include "kernel/device/cpu/globals.h"

#include "kernel/osl/closures_setup.h"
#include "kernel/osl/types.h"

CCL_NAMESPACE_BEGIN

/* Registration */

#define OSL_CLOSURE_STRUCT_BEGIN(Upper, lower) \
  static OSL::ClosureParam *osl_closure_##lower##_params() \
  { \
    static OSL::ClosureParam params[] = {
#define OSL_CLOSURE_STRUCT_END(Upper, lower) \
  CLOSURE_STRING_KEYPARAM(Upper##Closure, label, "label"), CLOSURE_FINISH_PARAM(Upper##Closure) \
  } \
  ; \
  return params; \
  }
#define OSL_CLOSURE_STRUCT_MEMBER(Upper, TYPE, type, name, key) \
  CLOSURE_##TYPE##_KEYPARAM(Upper##Closure, name, key),
#define OSL_CLOSURE_STRUCT_ARRAY_MEMBER(Upper, TYPE, type, name, key, size) \
  CLOSURE_##TYPE##_ARRAY_PARAM(Upper##Closure, name, size),

#include "closures_template.h"

void OSLRenderServices::register_closures(OSL::ShadingSystem *ss)
{
#define OSL_CLOSURE_STRUCT_BEGIN(Upper, lower) \
  ss->register_closure( \
      #lower, OSL_CLOSURE_##Upper##_ID, osl_closure_##lower##_params(), nullptr, nullptr);

#include "closures_template.h"
}

CCL_NAMESPACE_END
