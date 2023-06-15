/* SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *  */

/** \file
 * \ingroup draw
 *
 * List of defines that are shared with the GPUShaderCreateInfos. We do this to avoid
 * dragging larger headers into the createInfo pipeline which would cause problems.
 */

#pragma once

#define DRW_VIEW_UBO_SLOT 0
#define DRW_VIEW_CULLING_UBO_SLOT 1
#define DRW_CLIPPING_UBO_SLOT 2

#define DRW_LAYER_ATTR_UBO_SLOT 3

#define DRW_RESOURCE_ID_SLOT 11
#define DRW_OBJ_MAT_SLOT 10
#define DRW_OBJ_INFOS_SLOT 9
#define DRW_OBJ_ATTR_SLOT 8

#define DRW_DEBUG_PRINT_SLOT 15
#define DRW_DEBUG_DRAW_SLOT 14

#define DRW_COMMAND_GROUP_SIZE 64
#define DRW_FINALIZE_GROUP_SIZE 64
/* Must be multiple of 32. Set to 32 for shader simplicity. */
#define DRW_VISIBILITY_GROUP_SIZE 32
