/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

/* generic blender movie support, could move to own module */

struct RenderData;
struct ReportList;
struct Scene;

typedef struct bMovieHandle {
  int (*start_movie)(void *context_v,
                     const struct Scene *scene,
                     struct RenderData *rd,
                     int rectx,
                     int recty,
                     struct ReportList *reports,
                     bool preview,
                     const char *suffix);
  int (*append_movie)(void *context_v,
                      struct RenderData *rd,
                      int start_frame,
                      int frame,
                      int *pixels,
                      int rectx,
                      int recty,
                      const char *suffix,
                      struct ReportList *reports);
  void (*end_movie)(void *context_v);

  /* Optional function. */
  void (*get_movie_path)(char *filepath,
                         const struct RenderData *rd,
                         bool preview,
                         const char *suffix);

  void *(*context_create)(void);
  void (*context_free)(void *context_v);
} bMovieHandle;

bMovieHandle *BKE_movie_handle_get(char imtype);

/**
 * \note Similar to #BKE_image_path_from_imformat()
 */
void BKE_movie_filepath_get(char *filepath,
                            const struct RenderData *rd,
                            bool preview,
                            const char *suffix);

#ifdef __cplusplus
}
#endif
