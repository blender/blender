/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 *
 * Parsing of and code generation using GLSL shaders in gpu/shaders/material. */

#pragma once

#include "GPU_material.h"

#define MAX_FUNCTION_NAME 64
#define MAX_PARAMETER 36

struct GSet;

typedef struct GPUMaterialLibrary {
  char *code;
  struct GPUMaterialLibrary *dependencies[8];
} GPUMaterialLibrary;

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
  GPUMaterialLibrary *library;
} GPUFunction;

/* Module */

void gpu_material_library_init(void);
void gpu_material_library_exit(void);

/* Code Generation */

GPUFunction *gpu_material_library_use_function(struct GSet *used_libraries, const char *name);
char *gpu_material_library_generate_code(struct GSet *used_libraries, const char *frag_lib);

/* Code Parsing */

const char *gpu_str_skip_token(const char *str, char *token, int max);
const char *gpu_data_type_to_string(eGPUType type);
