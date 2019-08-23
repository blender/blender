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
 * List of all gpu_shader_material_*.glsl files used by GLSL materials. These
 * will be parsed to make all functions in them available to use for GPU_link().
 *
 * If a file uses functions from another file, it must be added to the list of
 * dependencies, and be placed after that file in the list. */

#ifndef __GPU_MATERIAL_LIBRARY_H__
#define __GPU_MATERIAL_LIBRARY_H__

typedef struct GPUMaterialLibrary {
  char *code;
  char *dependencies[8];
} GPUMaterialLibrary;

extern char datatoc_gpu_shader_material_glsl[];
extern char datatoc_gpu_shader_material_hash_glsl[];
extern char datatoc_gpu_shader_material_magic_glsl[];
extern char datatoc_gpu_shader_material_white_noise_glsl[];

static GPUMaterialLibrary gpu_material_libraries[] = {
    {datatoc_gpu_shader_material_hash_glsl, {NULL}},
    {datatoc_gpu_shader_material_glsl, {datatoc_gpu_shader_material_hash_glsl, NULL}},
    {datatoc_gpu_shader_material_magic_glsl, {NULL}},
    {datatoc_gpu_shader_material_white_noise_glsl, {datatoc_gpu_shader_material_hash_glsl, NULL}},
    {NULL, {NULL}}};

#endif
