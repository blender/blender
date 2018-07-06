/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * The Original Code is Copyright (C) 2006-2007 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_STUDIOLIGHT_H__
#define __BKE_STUDIOLIGHT_H__

/** \file BKE_studiolight.h
 *  \ingroup bke
 *
 * Studio lighting for the 3dview
 */

#include "BKE_context.h"

#include "BLI_sys_types.h"

#include "DNA_space_types.h"

#include "IMB_imbuf_types.h"

/*
 * These defines are the indexes in the StudioLight.diffuse_light
 * X_POS means the light that is traveling towards the positive X
 * So Light direction.
 */
#define STUDIOLIGHT_X_POS 0
#define STUDIOLIGHT_X_NEG 1
#define STUDIOLIGHT_Y_POS 2
#define STUDIOLIGHT_Y_NEG 3
#define STUDIOLIGHT_Z_POS 4
#define STUDIOLIGHT_Z_NEG 5

#define STUDIOLIGHT_ICON_ID_TYPE_RADIANCE       (1 << 0)
#define STUDIOLIGHT_ICON_ID_TYPE_IRRADIANCE     (1 << 1)
#define STUDIOLIGHT_ICON_ID_TYPE_MATCAP         (1 << 2)
#define STUDIOLIGHT_ICON_ID_TYPE_MATCAP_FLIPPED (1 << 3)

#define STUDIOLIGHT_ICON_SIZE 96

#define STUDIOLIGHT_SPHERICAL_HARMONICS_LEVEL 2
#define STUDIOLIGHT_SPHERICAL_HARMONICS_MAX_COMPONENTS 18

#if STUDIOLIGHT_SPHERICAL_HARMONICS_LEVEL == 0
#  define STUDIOLIGHT_SPHERICAL_HARMONICS_COMPONENTS 1
#endif

#if STUDIOLIGHT_SPHERICAL_HARMONICS_LEVEL == 1
#  define STUDIOLIGHT_SPHERICAL_HARMONICS_COMPONENTS 4
#endif

#if STUDIOLIGHT_SPHERICAL_HARMONICS_LEVEL == 2
#  define STUDIOLIGHT_SPHERICAL_HARMONICS_COMPONENTS 9
#  define STUDIOLIGHT_SPHERICAL_HARMONICS_WINDOWING_TARGET_LAMPLACIAN 10.0f
#endif

#if STUDIOLIGHT_SPHERICAL_HARMONICS_LEVEL == 4
#  define STUDIOLIGHT_SPHERICAL_HARMONICS_COMPONENTS 18
#endif
struct GPUTexture;
struct StudioLight;

/* StudioLight.flag */
enum StudioLightFlag {
	STUDIOLIGHT_SPHERICAL_HARMONICS_COEFFICIENTS_CALCULATED = (1 << 0),
	STUDIOLIGHT_LIGHT_DIRECTION_CALCULATED                  = (1 << 1),
	STUDIOLIGHT_INTERNAL                                    = (1 << 2),
	STUDIOLIGHT_EXTERNAL_FILE                               = (1 << 3),
	STUDIOLIGHT_USER_DEFINED                                = (1 << 12),
	STUDIOLIGHT_ORIENTATION_CAMERA                          = (1 << 4),
	STUDIOLIGHT_ORIENTATION_WORLD                           = (1 << 5),
	STUDIOLIGHT_ORIENTATION_VIEWNORMAL                      = (1 << 6),
	STUDIOLIGHT_EXTERNAL_IMAGE_LOADED                       = (1 << 7),
	STUDIOLIGHT_EQUIRECTANGULAR_IRRADIANCE_IMAGE_CALCULATED = (1 << 8),
	STUDIOLIGHT_EQUIRECTANGULAR_RADIANCE_GPUTEXTURE         = (1 << 9),
	STUDIOLIGHT_EQUIRECTANGULAR_IRRADIANCE_GPUTEXTURE       = (1 << 10),
	STUDIOLIGHT_RADIANCE_BUFFERS_CALCULATED                 = (1 << 11),
	STUDIOLIGHT_UI_EXPANDED                                 = (1 << 13),
} StudioLightFlag;

#define STUDIOLIGHT_FLAG_ALL (STUDIOLIGHT_INTERNAL | STUDIOLIGHT_EXTERNAL_FILE)
#define STUDIOLIGHT_FLAG_ORIENTATIONS (STUDIOLIGHT_ORIENTATION_CAMERA | STUDIOLIGHT_ORIENTATION_WORLD | STUDIOLIGHT_ORIENTATION_VIEWNORMAL)
#define STUDIOLIGHT_ORIENTATIONS_MATERIAL_MODE (STUDIOLIGHT_ORIENTATION_WORLD)
#define STUDIOLIGHT_ORIENTATIONS_SOLID (STUDIOLIGHT_INTERNAL | STUDIOLIGHT_ORIENTATION_CAMERA | STUDIOLIGHT_ORIENTATION_WORLD)

typedef void StudioLightFreeFunction(struct StudioLight *, void *data);

typedef struct StudioLight {
	struct StudioLight *next, *prev;

	int index;
	int flag;
	char name[FILE_MAXFILE];
	char path[FILE_MAX];
	char *path_irr_cache;
	char *path_sh_cache;
	int icon_id_irradiance;
	int icon_id_radiance;
	int icon_id_matcap;
	int icon_id_matcap_flipped;
	float spherical_harmonics_coefs[STUDIOLIGHT_SPHERICAL_HARMONICS_COMPONENTS][3];
	float light_direction[3];
	ImBuf *equirectangular_radiance_buffer;
	ImBuf *equirectangular_irradiance_buffer;
	ImBuf *radiance_cubemap_buffers[6];
	struct GPUTexture *equirectangular_radiance_gputexture;
	struct GPUTexture *equirectangular_irradiance_gputexture;
	float *gpu_matcap_3components; /* 3 channel buffer for GPU_R11F_G11F_B10F */

	/*
	 * Free function to clean up the running icons previews (wmJob) the usage is in
	 * interface_icons. Please be aware that this was build to handle only one free function
	 * that cleans up all icons. just to keep the code simple.
	 */
	StudioLightFreeFunction *free_function;
	void *free_function_data;
} StudioLight;

void BKE_studiolight_init(void);
void BKE_studiolight_free(void);
struct StudioLight *BKE_studiolight_find(const char *name, int flag);
struct StudioLight *BKE_studiolight_findindex(int index, int flag);
struct StudioLight *BKE_studiolight_find_first(int flag);
void BKE_studiolight_preview(uint *icon_buffer, StudioLight *sl, int icon_id_type);
struct ListBase *BKE_studiolight_listbase(void);
void BKE_studiolight_ensure_flag(StudioLight *sl, int flag);
void BKE_studiolight_refresh(void);
StudioLight *BKE_studiolight_new(const char *path, int orientation);
void BKE_studiolight_remove(StudioLight *sl);
void BKE_studiolight_set_free_function(StudioLight *sl, StudioLightFreeFunction *free_function, void *data);
void BKE_studiolight_unset_icon_id(StudioLight *sl, int icon_id);

#endif /*  __BKE_STUDIOLIGHT_H__ */
