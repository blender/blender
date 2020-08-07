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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */
#pragma once

#include "DNA_scene_types.h"

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct Image;
struct Main;
struct ReportList;
struct Scene;

typedef struct ImageSaveOptions {
  /* Context within which image is saved. */
  struct Main *bmain;
  struct Scene *scene;

  /* Format and absolute file path. */
  struct ImageFormatData im_format;
  char filepath[1024]; /* 1024 = FILE_MAX */

  /* Options. */
  bool relative;
  bool save_copy;
  bool save_as_render;
  bool do_newpath;
} ImageSaveOptions;

void BKE_image_save_options_init(struct ImageSaveOptions *opts,
                                 struct Main *bmain,
                                 struct Scene *scene);
bool BKE_image_save(struct ReportList *reports,
                    struct Main *bmain,
                    struct Image *ima,
                    struct ImageUser *iuser,
                    struct ImageSaveOptions *opts);

#ifdef __cplusplus
}
#endif
