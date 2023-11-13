/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef GPU_SHADER_EEVEE_LEGACY_DEFINES
#define GPU_SHADER_EEVEE_LEGACY_DEFINES
#ifdef GPU_SHADER
#  define EEVEE_ENGINE
#endif

/* Minimum UBO is 16384 bytes. */
#define MAX_PROBE 128 /* TODO: find size by dividing UBO max size by probe data size. */
#define MAX_GRID 64   /* TODO: find size by dividing UBO max size by grid data size. */
#define MAX_PLANAR 16 /* TODO: find size by dividing UBO max size by grid data size. */
#define MAX_LIGHT 128 /* TODO: find size by dividing UBO max size by light data size. */
#define MAX_CASCADE_NUM 4
#define MAX_SHADOW 128 /* TODO: Make this depends on #GL_MAX_ARRAY_TEXTURE_LAYERS. */
#define MAX_SHADOW_CASCADE 8
#define MAX_SHADOW_CUBE (MAX_SHADOW - MAX_CASCADE_NUM * MAX_SHADOW_CASCADE)
#define MAX_BLOOM_STEP 16
#define MAX_AOVS 64

/* Motion Blur. */
#define EEVEE_VELOCITY_TILE_SIZE 32

/* Depth of Field. */
#define DOF_TILE_DIVISOR 16
#define DOF_BOKEH_LUT_SIZE 32
#define DOF_GATHER_RING_COUNT 5
#define DOF_DILATE_RING_COUNT 3
#define DOF_FAST_GATHER_COC_ERROR 0.05

#endif
