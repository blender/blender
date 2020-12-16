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
 * Generate shader code from the intermediate node graph.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct GPUMaterial;
struct GPUNodeGraph;
struct GPUShader;

typedef struct GPUPass {
  struct GPUPass *next;

  struct GPUShader *shader;
  char *fragmentcode;
  char *geometrycode;
  char *vertexcode;
  char *defines;
  uint refcount; /* Orphaned GPUPasses gets freed by the garbage collector. */
  uint32_t hash; /* Identity hash generated from all GLSL code. */
  bool compiled; /* Did we already tried to compile the attached GPUShader. */
} GPUPass;

/* Pass */

GPUPass *GPU_generate_pass(struct GPUMaterial *material,
                           struct GPUNodeGraph *graph,
                           const char *vert_code,
                           const char *geom_code,
                           const char *frag_lib,
                           const char *defines);
struct GPUShader *GPU_pass_shader_get(GPUPass *pass);
bool GPU_pass_compile(GPUPass *pass, const char *shname);
void GPU_pass_release(GPUPass *pass);

/* Module */

void gpu_codegen_init(void);
void gpu_codegen_exit(void);

#ifdef __cplusplus
}
#endif
