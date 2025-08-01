/* SPDX-FileCopyrightText: 2006-2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 *
 * Studio lighting for the 3dview
 */

#include "BLI_sys_types.h"

#include "BLI_path_utils.hh"

#include "DNA_userdef_types.h" /* for #SolidLight */

struct ImBuf;

#define STUDIOLIGHT_ICON_ID_TYPE_RADIANCE (1 << 0)
#define STUDIOLIGHT_ICON_ID_TYPE_IRRADIANCE (1 << 1)
#define STUDIOLIGHT_ICON_ID_TYPE_MATCAP (1 << 2)
#define STUDIOLIGHT_ICON_ID_TYPE_MATCAP_FLIPPED (1 << 3)

#define STUDIOLIGHT_MAX_LIGHT 4

#define STUDIOLIGHT_ICON_SIZE 96

namespace blender::gpu {
class Texture;
}  // namespace blender::gpu
struct StudioLight;

/** #StudioLight.flag */
enum StudioLightFlag {
  STUDIOLIGHT_INTERNAL = (1 << 0),
  STUDIOLIGHT_EXTERNAL_FILE = (1 << 1),
  STUDIOLIGHT_TYPE_STUDIO = (1 << 2),
  STUDIOLIGHT_TYPE_WORLD = (1 << 3),
  STUDIOLIGHT_TYPE_MATCAP = (1 << 4),
  STUDIOLIGHT_EXTERNAL_IMAGE_LOADED = (1 << 5),
  /** GPU Texture used for lookdev mode. */
  STUDIOLIGHT_EQUIRECT_RADIANCE_GPUTEXTURE = (1 << 6),
  STUDIOLIGHT_USER_DEFINED = (1 << 7),

  STUDIOLIGHT_MATCAP_DIFFUSE_GPUTEXTURE = (1 << 8),
  STUDIOLIGHT_MATCAP_SPECULAR_GPUTEXTURE = (1 << 9),
  /* Is set for studio lights and matcaps with specular highlight pass. */
  STUDIOLIGHT_SPECULAR_HIGHLIGHT_PASS = (1 << 10),
};

#define STUDIOLIGHT_FLAG_ALL (STUDIOLIGHT_INTERNAL | STUDIOLIGHT_EXTERNAL_FILE)
#define STUDIOLIGHT_FLAG_ORIENTATIONS \
  (STUDIOLIGHT_TYPE_STUDIO | STUDIOLIGHT_TYPE_WORLD | STUDIOLIGHT_TYPE_MATCAP)
#define STUDIOLIGHT_ORIENTATIONS_MATERIAL_MODE (STUDIOLIGHT_TYPE_WORLD)
#define STUDIOLIGHT_ORIENTATIONS_SOLID (STUDIOLIGHT_INTERNAL | STUDIOLIGHT_TYPE_STUDIO)

typedef void StudioLightFreeFunction(struct StudioLight *, void *data);

typedef struct StudioLightImage {
  struct ImBuf *ibuf;
  blender::gpu::Texture *gputexture;
} StudioLightImage;

typedef struct StudioLight {
  struct StudioLight *next, *prev;

  int index;
  int flag;
  char name[FILE_MAXFILE];
  char filepath[FILE_MAX];
  int icon_id_irradiance;
  int icon_id_radiance;
  int icon_id_matcap;
  int icon_id_matcap_flipped;
  StudioLightImage matcap_diffuse;
  StudioLightImage matcap_specular;
  struct ImBuf *equirect_radiance_buffer;
  blender::gpu::Texture *equirect_radiance_gputexture;
  SolidLight light[STUDIOLIGHT_MAX_LIGHT];
  float light_ambient[3];

  /*
   * Free function to clean up the running icons previews (wmJob) the usage is in
   * interface_icons. Please be aware that this was build to handle only one free function
   * that cleans up all icons. just to keep the code simple.
   */
  StudioLightFreeFunction *free_function;
  void *free_function_data;
} StudioLight;

/* API */

void BKE_studiolight_init(void);
void BKE_studiolight_free(void);
void BKE_studiolight_default(SolidLight lights[4], float light_ambient[3]);
struct StudioLight *BKE_studiolight_find(const char *name, int flag);
struct StudioLight *BKE_studiolight_findindex(int index, int flag);
struct StudioLight *BKE_studiolight_find_default(int flag);
void BKE_studiolight_preview(uint *icon_buffer, StudioLight *sl, int icon_id_type);
struct ListBase *BKE_studiolight_listbase(void);
/**
 * Ensure state of studio-lights.
 */
void BKE_studiolight_ensure_flag(StudioLight *sl, int flag);
void BKE_studiolight_refresh(void);
StudioLight *BKE_studiolight_load(const char *filepath, int type);
StudioLight *BKE_studiolight_create(const char *filepath,
                                    const SolidLight light[4],
                                    const float light_ambient[3]);
/**
 * Only useful for workbench while editing the user-preferences.
 */
StudioLight *BKE_studiolight_studio_edit_get(void);
void BKE_studiolight_remove(StudioLight *sl);
void BKE_studiolight_set_free_function(StudioLight *sl,
                                       StudioLightFreeFunction *free_function,
                                       void *data);
void BKE_studiolight_unset_icon_id(StudioLight *sl, int icon_id);
