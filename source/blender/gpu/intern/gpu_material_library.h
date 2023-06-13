/* SPDX-FileCopyrightText: 2005 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Parsing of and code generation using GLSL shaders in gpu/shaders/material. */

#pragma once

#include "GPU_material.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_FUNCTION_NAME 64
#define MAX_PARAMETER 36

struct GSet;

typedef enum {
  FUNCTION_QUAL_IN,
  FUNCTION_QUAL_OUT,
  FUNCTION_QUAL_INOUT,
} GPUFunctionQual;

typedef struct GPUFunction {
  char name[MAX_FUNCTION_NAME];
  eGPUType paramtype[MAX_PARAMETER];
  GPUFunctionQual paramqual[MAX_PARAMETER];
  int totparam;
  /* TODO(@fclem): Clean that void pointer. */
  void *source; /* GPUSource */
} GPUFunction;

GPUFunction *gpu_material_library_use_function(struct GSet *used_libraries, const char *name);

#ifdef __cplusplus
}
#endif
