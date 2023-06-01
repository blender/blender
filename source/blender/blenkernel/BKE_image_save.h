/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "DNA_scene_types.h"

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct Image;
struct ImageUser;
struct Main;
struct RenderResult;
struct ReportList;
struct Scene;

/* Image datablock saving. */

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

  /* Keep track of previous values for auto updates in UI. */
  bool prev_save_as_render;
  int prev_imtype;
} ImageSaveOptions;

bool BKE_image_save_options_init(ImageSaveOptions *opts,
                                 struct Main *bmain,
                                 struct Scene *scene,
                                 struct Image *ima,
                                 struct ImageUser *iuser,
                                 const bool guess_path,
                                 const bool save_as_render);
void BKE_image_save_options_update(struct ImageSaveOptions *opts, const struct Image *ima);
void BKE_image_save_options_free(struct ImageSaveOptions *opts);

bool BKE_image_save(struct ReportList *reports,
                    struct Main *bmain,
                    struct Image *ima,
                    struct ImageUser *iuser,
                    const struct ImageSaveOptions *opts);

/* Render saving. */

/**
 * Save single or multi-layer OpenEXR files from the render result.
 * Optionally saves only a specific view or layer.
 */
bool BKE_image_render_write_exr(struct ReportList *reports,
                                const struct RenderResult *rr,
                                const char *filepath,
                                const struct ImageFormatData *imf,
                                const bool save_as_render,
                                const char *view,
                                int layer);

/**
 * \param filepath_basis: May be used as-is, or used as a basis for multi-view images.
 */
bool BKE_image_render_write(struct ReportList *reports,
                            struct RenderResult *rr,
                            const struct Scene *scene,
                            const bool stamp,
                            const char *filepath_basis);

#ifdef __cplusplus
}
#endif
