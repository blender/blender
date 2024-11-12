/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "DNA_scene_types.h"

/** \file
 * \ingroup bke
 */

struct Image;
struct ImageUser;
struct Main;
struct RenderResult;
struct ReportList;
struct Scene;

/* Image datablock saving. */

struct ImageSaveOptions {
  /* Context within which image is saved. */
  Main *bmain;
  Scene *scene;

  /* Format and absolute file path. */
  ImageFormatData im_format;
  char filepath[1024]; /* 1024 = FILE_MAX */

  /* Options. */
  bool relative;
  bool save_copy;
  bool save_as_render;
  bool do_newpath;

  /* Keep track of previous values for auto updates in UI. */
  bool prev_save_as_render;
  int prev_imtype;
};

bool BKE_image_save_options_init(ImageSaveOptions *opts,
                                 Main *bmain,
                                 Scene *scene,
                                 Image *ima,
                                 ImageUser *iuser,
                                 const bool guess_path,
                                 const bool save_as_render);
void BKE_image_save_options_update(ImageSaveOptions *opts, const Image *image);
void BKE_image_save_options_free(ImageSaveOptions *opts);

bool BKE_image_save(
    ReportList *reports, Main *bmain, Image *ima, ImageUser *iuser, const ImageSaveOptions *opts);

/* Render saving. */

/**
 * Save single or multi-layer OpenEXR files from the render result.
 * Optionally saves only a specific view or layer.
 */
bool BKE_image_render_write_exr(ReportList *reports,
                                const RenderResult *rr,
                                const char *filepath,
                                const ImageFormatData *imf,
                                const bool save_as_render,
                                const char *view,
                                int layer);

/**
 * \param filepath_basis: May be used as-is, or used as a basis for multi-view images.
 * \param format: The image format to use for saving, if null, the scene format will be used.
 */
bool BKE_image_render_write(ReportList *reports,
                            RenderResult *rr,
                            const Scene *scene,
                            const bool stamp,
                            const char *filepath_basis,
                            const ImageFormatData *format = nullptr,
                            bool save_as_render = true);
