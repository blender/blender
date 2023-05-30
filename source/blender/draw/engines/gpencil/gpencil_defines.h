/* SPDX-License-Identifier: GPL-2.0-or-later */

#define GPENCIL_MATERIAL_BUFFER_LEN 256

#define GPENCIL_LIGHT_BUFFER_LEN 128

/* High bits are used to pass material ID to fragment shader. */
#define GPENCIl_MATID_SHIFT 16u

/* Textures */
#define GPENCIL_SCENE_DEPTH_TEX_SLOT 2
#define GPENCIL_MASK_TEX_SLOT 3
#define GPENCIL_FILL_TEX_SLOT 4
#define GPENCIL_STROKE_TEX_SLOT 5
/* SSBOs */
#define GPENCIL_OBJECT_SLOT 0
#define GPENCIL_LAYER_SLOT 1
#define GPENCIL_MATERIAL_SLOT 2
#define GPENCIL_LIGHT_SLOT 3
/* UBOs */
#define GPENCIL_SCENE_SLOT 2
