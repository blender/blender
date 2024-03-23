/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Parsing of and code generation using GLSL shaders in gpu/shaders/material. */

#pragma once

#include "GPU_material.hh"

#define MAX_FUNCTION_NAME 64
#define MAX_PARAMETER 36

struct GSet;

enum GPUFunctionQual {
  FUNCTION_QUAL_IN,
  FUNCTION_QUAL_OUT,
  FUNCTION_QUAL_INOUT,
};

struct GPUFunction {
  char name[MAX_FUNCTION_NAME];
  eGPUType paramtype[MAX_PARAMETER];
  GPUFunctionQual paramqual[MAX_PARAMETER];
  int totparam;
  /* TODO(@fclem): Clean that void pointer. */
  void *source; /* GPUSource */
};

GPUFunction *gpu_material_library_use_function(GSet *used_libraries, const char *name);
