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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 */

#include <stdlib.h>

#include "BLI_utildefines.h"
#include "IMB_imbuf.h"

struct ColorSpace;
struct ImBuf;

void IMB_freeImBuf(struct ImBuf *UNUSED(ibuf))
{
}
void IMB_colormanagement_display_to_scene_linear_v3(float UNUSED(pixel[3]),
                                                    struct ColorManagedDisplay *UNUSED(display))
{
}

bool IMB_colormanagement_space_is_scene_linear(struct ColorSpace *colorspace)
{
  return false;
}

bool IMB_colormanagement_space_is_data(struct ColorSpace *colorspace)
{
  return false;
}

void BKE_material_defaults_free_gpu(void)
{
}

/* Variables. */
int G;

/* Functions which aren't called. */
void *BKE_image_free_buffers = NULL;
void *BKE_image_get_tile = NULL;
void *BKE_image_get_tile_from_iuser = NULL;
void *BKE_tempdir_session = NULL;
void *DRW_deferred_shader_remove = NULL;
void *datatoc_common_view_lib_glsl = NULL;
void *ntreeFreeLocalTree = NULL;
void *ntreeGPUMaterialNodes = NULL;
void *ntreeLocalize = NULL;
