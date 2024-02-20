/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

/* generic blender movie support, could move to own module */

struct RenderData;
struct ReportList;
struct Scene;

struct bMovieHandle {
  int (*start_movie)(void *context_v,
                     const Scene *scene,
                     RenderData *rd,
                     int rectx,
                     int recty,
                     ReportList *reports,
                     bool preview,
                     const char *suffix);
  int (*append_movie)(void *context_v,
                      RenderData *rd,
                      int start_frame,
                      int frame,
                      int *pixels,
                      int rectx,
                      int recty,
                      const char *suffix,
                      ReportList *reports);
  void (*end_movie)(void *context_v);

  /* Optional function. */
  void (*get_movie_path)(char filepath[/*FILE_MAX*/ 1024],
                         const RenderData *rd,
                         bool preview,
                         const char *suffix);

  void *(*context_create)(void);
  void (*context_free)(void *context_v);
};

bMovieHandle *BKE_movie_handle_get(char imtype);

/**
 * \note Similar to #BKE_image_path_from_imformat()
 */
void BKE_movie_filepath_get(char filepath[/*FILE_MAX*/ 1024],
                            const RenderData *rd,
                            bool preview,
                            const char *suffix);
