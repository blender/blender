/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * Functions for writing AVI-format files.
 * Added interface for generic movie support (ton)
 * \ingroup bke
 */

#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"

#include "BLI_utildefines.h"

#include "BKE_report.h"
#ifdef WITH_AVI
#  include "BLI_blenlib.h"

#  include "BKE_main.hh"
#endif

#include "BKE_writeavi.h"

/* ********************** general blender movie support ***************************** */

static int start_stub(void * /*context_v*/,
                      const Scene * /*scene*/,
                      RenderData * /*rd*/,
                      int /*rectx*/,
                      int /*recty*/,
                      ReportList * /*reports*/,
                      bool /*preview*/,
                      const char * /*suffix*/)
{
  return 0;
}

static void end_stub(void * /*context_v*/) {}

static int append_stub(void * /*context_v*/,
                       RenderData * /*rd*/,
                       int /*start_frame*/,
                       int /*frame*/,
                       int * /*pixels*/,
                       int /*rectx*/,
                       int /*recty*/,
                       const char * /*suffix*/,
                       ReportList * /*reports*/)
{
  return 0;
}

static void *context_create_stub()
{
  return nullptr;
}

static void context_free_stub(void * /*context_v*/) {}

#ifdef WITH_AVI
#  include "AVI_avi.h"

/* callbacks */
static int start_avi(void *context_v,
                     const Scene *scene,
                     RenderData *rd,
                     int rectx,
                     int recty,
                     ReportList *reports,
                     bool preview,
                     const char *suffix);
static void end_avi(void *context_v);
static int append_avi(void *context_v,
                      RenderData *rd,
                      int start_frame,
                      int frame,
                      int *pixels,
                      int rectx,
                      int recty,
                      const char *suffix,
                      ReportList *reports);
static void filepath_avi(char filepath[FILE_MAX],
                         const RenderData *rd,
                         bool preview,
                         const char *suffix);
static void *context_create_avi(void);
static void context_free_avi(void *context_v);
#endif /* WITH_AVI */

#ifdef WITH_FFMPEG
#  include "BKE_writeffmpeg.hh"
#endif

bMovieHandle *BKE_movie_handle_get(const char imtype)
{
  static bMovieHandle mh = {nullptr};
  /* stub callbacks in case none of the movie formats is supported */
  mh.start_movie = start_stub;
  mh.append_movie = append_stub;
  mh.end_movie = end_stub;
  mh.get_movie_path = nullptr;
  mh.context_create = context_create_stub;
  mh.context_free = context_free_stub;

/* set the default handle, as builtin */
#ifdef WITH_AVI
  mh.start_movie = start_avi;
  mh.append_movie = append_avi;
  mh.end_movie = end_avi;
  mh.get_movie_path = filepath_avi;
  mh.context_create = context_create_avi;
  mh.context_free = context_free_avi;
#endif

/* do the platform specific handles */
#ifdef WITH_FFMPEG
  if (ELEM(imtype,
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
#endif

  /* in case all above are disabled */
  (void)imtype;

  return (mh.append_movie != append_stub) ? &mh : nullptr;
}

/* ****************************************************************** */

#ifdef WITH_AVI

static void filepath_avi(char filepath[FILE_MAX],
                         const RenderData *rd,
                         bool preview,
                         const char *suffix)
{
  int sfra, efra;

  if (filepath == nullptr) {
    return;
  }

  if (preview) {
    sfra = rd->psfra;
    efra = rd->pefra;
  }
  else {
    sfra = rd->sfra;
    efra = rd->efra;
  }

  BLI_strncpy(filepath, rd->pic, FILE_MAX);
  BLI_path_abs(filepath, BKE_main_blendfile_path_from_global());

  BLI_file_ensure_parent_dir_exists(filepath);

  if (rd->scemode & R_EXTENSION) {
    if (!BLI_path_extension_check(filepath, ".avi")) {
      BLI_path_frame_range(filepath, FILE_MAX, sfra, efra, 4);
      BLI_strncat(filepath, ".avi", FILE_MAX);
    }
  }
  else {
    if (BLI_path_frame_check_chars(filepath)) {
      BLI_path_frame_range(filepath, FILE_MAX, sfra, efra, 4);
    }
  }

  BLI_path_suffix(filepath, FILE_MAX, suffix, "");
}

static int start_avi(void *context_v,
                     const Scene * /*scene*/,
                     RenderData *rd,
                     int rectx,
                     int recty,
                     ReportList *reports,
                     bool preview,
                     const char *suffix)
{
  int x, y;
  char filepath[FILE_MAX];
  AviFormat format;
  int quality;
  double framerate;
  AviMovie *avi = static_cast<AviMovie *>(context_v);

  filepath_avi(filepath, rd, preview, suffix);

  x = rectx;
  y = recty;

  quality = rd->im_format.quality;
  framerate = double(rd->frs_sec) / double(rd->frs_sec_base);

  if (rd->im_format.imtype != R_IMF_IMTYPE_AVIJPEG) {
    format = AVI_FORMAT_AVI_RGB;
  }
  else {
    format = AVI_FORMAT_MJPEG;
  }

  if (AVI_open_compress(filepath, avi, 1, format) != AVI_ERROR_NONE) {
    BKE_report(reports, RPT_ERROR, "Cannot open or start AVI movie file");
    return 0;
  }

  AVI_set_compress_option(avi, AVI_OPTION_TYPE_MAIN, 0, AVI_OPTION_WIDTH, &x);
  AVI_set_compress_option(avi, AVI_OPTION_TYPE_MAIN, 0, AVI_OPTION_HEIGHT, &y);
  AVI_set_compress_option(avi, AVI_OPTION_TYPE_MAIN, 0, AVI_OPTION_QUALITY, &quality);
  AVI_set_compress_option(avi, AVI_OPTION_TYPE_MAIN, 0, AVI_OPTION_FRAMERATE, &framerate);

  avi->interlace = 0;
  avi->odd_fields = 0;

  printf("Created avi: %s\n", filepath);
  return 1;
}

static int append_avi(void *context_v,
                      RenderData * /*rd*/,
                      int start_frame,
                      int frame,
                      int *pixels,
                      int rectx,
                      int recty,
                      const char * /*suffix*/,
                      ReportList * /*reports*/)
{
  uint *rt1, *rt2, *rectot;
  int x, y;
  char *cp, rt;
  AviMovie *avi = static_cast<AviMovie *>(context_v);

  if (avi == nullptr) {
    return 0;
  }

  /* NOTE(@zr): that LIBAVI free's the buffer (stupid interface). */
  rectot = static_cast<uint *>(MEM_mallocN(rectx * recty * sizeof(int), "rectot"));
  rt1 = rectot;
  rt2 = (uint *)pixels + (recty - 1) * rectx;
  /* Flip Y and convert to ABGR. */
  for (y = 0; y < recty; y++, rt1 += rectx, rt2 -= rectx) {
    memcpy(rt1, rt2, rectx * sizeof(int));

    cp = (char *)rt1;
    for (x = rectx; x > 0; x--) {
      rt = cp[0];
      cp[0] = cp[3];
      cp[3] = rt;
      rt = cp[1];
      cp[1] = cp[2];
      cp[2] = rt;
      cp += 4;
    }
  }

  AVI_write_frame(avi, (frame - start_frame), AVI_FORMAT_RGB32, rectot, rectx * recty * 4);
  //  printf("added frame %3d (frame %3d in avi): ", frame, frame-start_frame);

  return 1;
}

static void end_avi(void *context_v)
{
  AviMovie *avi = static_cast<AviMovie *>(context_v);

  if (avi == nullptr) {
    return;
  }

  AVI_close_compress(avi);
}

static void *context_create_avi()
{
  AviMovie *avi = static_cast<AviMovie *>(MEM_mallocN(sizeof(AviMovie), "avimovie"));
  return avi;
}

static void context_free_avi(void *context_v)
{
  AviMovie *avi = static_cast<AviMovie *>(context_v);
  if (avi) {
    MEM_freeN(avi);
  }
}

#endif /* WITH_AVI */

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
