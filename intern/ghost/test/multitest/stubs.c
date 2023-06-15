/* SPDX-FileCopyrightText: 2013 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdlib.h>

#include "BLI_utildefines.h"
#include "IMB_imbuf.h"

struct ColorSpace;
struct ImBuf;

void IMB_freeImBuf(struct ImBuf *UNUSED(ibuf)) {}
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

void BKE_material_defaults_free_gpu(void) {}

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
