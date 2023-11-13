/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#define WB_RESOLVE_GROUP_SIZE 8

/* Resources bind slots. */

/* Textures. */
/* Slot 0 is reserved by draw_hair_new. */
#define WB_MATCAP_SLOT 1
#define WB_TEXTURE_SLOT 2
#define WB_TILE_ARRAY_SLOT 3
#define WB_TILE_DATA_SLOT 4
#define WB_CURVES_UV_SLOT 5
#define WB_CURVES_COLOR_SLOT 6

/* UBOs (Storage buffers in Workbench Next). */
#define WB_MATERIAL_SLOT 0
#define WB_WORLD_SLOT 1
