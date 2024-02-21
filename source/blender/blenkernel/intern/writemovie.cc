/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * Functions for writing movie files.
 * \ingroup bke
 */

#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"

#include "BLI_utildefines.h"

#include "BKE_report.hh"

#ifdef WITH_FFMPEG
#  include "BKE_writeffmpeg.hh"
#endif

#include "BKE_writemovie.hh"

static bool start_stub(void * /*context_v*/,
                       const Scene * /*scene*/,
                       RenderData * /*rd*/,
                       int /*rectx*/,
                       int /*recty*/,
                       ReportList * /*reports*/,
                       bool /*preview*/,
                       const char * /*suffix*/)
{
  return false;
}

static void end_stub(void * /*context_v*/) {}

static bool append_stub(void * /*context_v*/,
                        RenderData * /*rd*/,
                        int /*start_frame*/,
                        int /*frame*/,
                        const ImBuf * /*image*/,
                        const char * /*suffix*/,
                        ReportList * /*reports*/)
{
  return false;
}

static void *context_create_stub()
{
  return nullptr;
}

static void context_free_stub(void * /*context_v*/) {}

bMovieHandle *BKE_movie_handle_get(const char imtype)
{
  static bMovieHandle mh = {nullptr};
  /* Stub callbacks in case ffmpeg is not compiled in. */
  mh.start_movie = start_stub;
  mh.append_movie = append_stub;
  mh.end_movie = end_stub;
  mh.get_movie_path = nullptr;
  mh.context_create = context_create_stub;
  mh.context_free = context_free_stub;

#ifdef WITH_FFMPEG
  if (ELEM(imtype,
           R_IMF_IMTYPE_AVIRAW,
           R_IMF_IMTYPE_AVIJPEG,
           R_IMF_IMTYPE_FFMPEG,
           R_IMF_IMTYPE_H264,
           R_IMF_IMTYPE_XVID,
           R_IMF_IMTYPE_THEORA,
           R_IMF_IMTYPE_AV1))
  {
    mh.start_movie = BKE_ffmpeg_start;
    mh.append_movie = BKE_ffmpeg_append;
    mh.end_movie = BKE_ffmpeg_end;
    mh.get_movie_path = BKE_ffmpeg_filepath_get;
    mh.context_create = BKE_ffmpeg_context_create;
    mh.context_free = BKE_ffmpeg_context_free;
  }
#else
  (void)imtype;
#endif

  return (mh.append_movie != append_stub) ? &mh : nullptr;
}

void BKE_movie_filepath_get(char filepath[/*FILE_MAX*/ 1024],
                            const RenderData *rd,
                            bool preview,
                            const char *suffix)
{
  bMovieHandle *mh = BKE_movie_handle_get(rd->im_format.imtype);
  if (mh && mh->get_movie_path) {
    mh->get_movie_path(filepath, rd, preview, suffix);
  }
  else {
    filepath[0] = '\0';
  }
}
