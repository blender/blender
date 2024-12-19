/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup imbuf
 */

struct ImBuf;
struct ImbMovieWriter;
struct RenderData;
struct ReportList;
struct Scene;

ImbMovieWriter *IMB_movie_write_begin(const char imtype,
                                      const Scene *scene,
                                      RenderData *rd,
                                      int rectx,
                                      int recty,
                                      ReportList *reports,
                                      bool preview,
                                      const char *suffix);
bool IMB_movie_write_append(ImbMovieWriter *writer,
                            RenderData *rd,
                            int start_frame,
                            int frame,
                            const ImBuf *image,
                            const char *suffix,
                            ReportList *reports);
void IMB_movie_write_end(ImbMovieWriter *writer);

/**
 * \note Similar to #BKE_image_path_from_imformat()
 */
void IMB_movie_filepath_get(char filepath[/*FILE_MAX*/ 1024],
                            const RenderData *rd,
                            bool preview,
                            const char *suffix);
