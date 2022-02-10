/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */
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
