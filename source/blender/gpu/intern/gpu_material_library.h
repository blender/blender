/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 *
 * Parsing of and code generation using GLSL shaders in gpu/shaders/material. */

#pragma once

#include "GPU_material.h"

#define MAX_FUNCTION_NAME 64
#define MAX_PARAMETER 32

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

char *gpu_str_skip_token(char *str, char *token, int max);
const char *gpu_data_type_to_string(const eGPUType type);
