/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup imbuf
 */

struct ImBuf;
struct MovieWriter;
struct RenderData;
struct ReportList;
struct Scene;

MovieWriter *MOV_write_begin(const char imtype,
                             const Scene *scene,
                             const RenderData *rd,
                             int rectx,
                             int recty,
                             ReportList *reports,
                             bool preview,
                             const char *suffix);
bool MOV_write_append(MovieWriter *writer,
                      const Scene *scene,
                      const RenderData *rd,
                      int start_frame,
                      int frame,
                      const ImBuf *image,
                      const char *suffix,
                      ReportList *reports);
void MOV_write_end(MovieWriter *writer);

/**
 * \note Similar to #BKE_image_path_from_imformat()
 */
void MOV_filepath_from_settings(char filepath[/*FILE_MAX*/ 1024],
                                const Scene *scene,
                                const RenderData *rd,
                                bool preview,
                                const char *suffix,
                                ReportList *reports);
