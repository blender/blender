/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation.
 */

/** \file
 * \ingroup eevee
 *
 * List of defines that are shared with the GPUShaderCreateInfos. We do this to avoid
 * dragging larger headers into the createInfo pipeline which would cause problems.
 */

#pragma once

/**
   Number of items in a culling batch. Needs to be Power of 2. Must be <= to 65536.
 * Current limiting factor is the sorting phase which is single pass and only sort within a
 * thread-group which maximum size is 1024.
 */
#define CULLING_BATCH_SIZE 1024

/**
 * IMPORTANT: Some data packing are tweaked for these values.
 * Be sure to update them accordingly.
 * SHADOW_TILEMAP_RES max is 32 because of the shared bitmaps used for LOD tagging.
 * It is also limited by the maximum thread group size (1024).
 */
#define SHADOW_TILEMAP_RES 16
#define SHADOW_TILEMAP_LOD 4 /* LOG2(SHADOW_TILEMAP_RES) */
#define SHADOW_TILEMAP_PER_ROW 64
#define SHADOW_PAGE_COPY_GROUP_SIZE 32
#define SHADOW_DEPTH_SCAN_GROUP_SIZE 32
#define SHADOW_AABB_TAG_GROUP_SIZE 64
#define SHADOW_MAX_TILEMAP 4096
#define SHADOW_MAX_PAGE 4096
#define SHADOW_PAGE_PER_ROW 64

#define HIZ_MIP_COUNT 6u
/* Group size is 2x smaller because we simply copy the level 0. */
#define HIZ_GROUP_SIZE 1u << (HIZ_MIP_COUNT - 2u)

#define RAYTRACE_GROUP_SIZE 16
#define RAYTRACE_MAX_TILES (16384 / RAYTRACE_GROUP_SIZE) * (16384 / RAYTRACE_GROUP_SIZE)

/* Minimum visibility size. */
#define LIGHTPROBE_FILTER_VIS_GROUP_SIZE 16
