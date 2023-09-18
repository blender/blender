/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * List of defines that are shared with the GPUShaderCreateInfos. We do this to avoid
 * dragging larger headers into the createInfo pipeline which would cause problems.
 */

#pragma once

/* We target hardware with at least 12 UBO slots (Guaranteed by GL 4.3). */
#define DRW_VIEW_UBO_SLOT 11
#define DRW_VIEW_CULLING_UBO_SLOT 10
#define DRW_OBJ_DATA_INFO_UBO_SLOT 9
#define DRW_OBJ_MAT_UBO_SLOT 8
#define DRW_LAYER_ATTR_UBO_SLOT 7
/* Slots 0-6 are reserved for engine use. */
/* TODO(fclem): Legacy. To be removed once we remove the old DRW. */
#define DRW_OBJ_INFOS_UBO_SLOT 6
/* TODO(fclem): Remove in favor of engine-side clipping UBO. */
#define DRW_CLIPPING_UBO_SLOT 5

/* We target hardware with at least 12 SSBO slots (NOT Guaranteed by GL 4.3). */
#define DRW_RESOURCE_ID_SLOT 11
#define DRW_OBJ_MAT_SLOT 10
#define DRW_OBJ_INFOS_SLOT 9
#define DRW_OBJ_ATTR_SLOT 8
/* Slots 0-7 are reserved for engine use. */
/* Debug SSBOs are not counted in the limit [12 - 15+]. */
#define DRW_DEBUG_PRINT_SLOT 15
#define DRW_DEBUG_DRAW_SLOT 14

#define DRW_COMMAND_GROUP_SIZE 64
#define DRW_FINALIZE_GROUP_SIZE 64
/* Must be multiple of 32. Set to 32 for shader simplicity. */
#define DRW_VISIBILITY_GROUP_SIZE 32
