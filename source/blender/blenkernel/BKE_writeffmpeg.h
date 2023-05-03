/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

#pragma once

/** \file
 * \ingroup bke
 */

#ifdef WITH_FFMPEG

#  ifdef __cplusplus
extern "C" {
#  endif

enum {
  FFMPEG_MPEG1 = 0,
  FFMPEG_MPEG2 = 1,
  FFMPEG_MPEG4 = 2,
  FFMPEG_AVI = 3,
  FFMPEG_MOV = 4,
  FFMPEG_DV = 5,
  FFMPEG_H264 = 6,
  FFMPEG_XVID = 7,
  FFMPEG_FLV = 8,
  FFMPEG_MKV = 9,
  FFMPEG_OGG = 10,
  FFMPEG_INVALID = 11,
  FFMPEG_WEBM = 12,
  FFMPEG_AV1 = 13,
};

enum {
  FFMPEG_PRESET_NONE = 0,
  FFMPEG_PRESET_DVD = 1,
  FFMPEG_PRESET_SVCD = 2,
  FFMPEG_PRESET_VCD = 3,
  FFMPEG_PRESET_DV = 4,
  FFMPEG_PRESET_H264 = 5,
  FFMPEG_PRESET_THEORA = 6,
  FFMPEG_PRESET_XVID = 7,
  FFMPEG_PRESET_AV1 = 8,
};

struct RenderData;
struct ReportList;
struct Scene;

int BKE_ffmpeg_start(void *context_v,
                     const struct Scene *scene,
                     struct RenderData *rd,
                     int rectx,
                     int recty,
                     struct ReportList *reports,
                     bool preview,
                     const char *suffix);
void BKE_ffmpeg_end(void *context_v);
int BKE_ffmpeg_append(void *context_v,
                      struct RenderData *rd,
                      int start_frame,
                      int frame,
                      int *pixels,
                      int rectx,
                      int recty,
                      const char *suffix,
                      struct ReportList *reports);
void BKE_ffmpeg_filepath_get(char *filepath,
                             const struct RenderData *rd,
                             bool preview,
                             const char *suffix);

void BKE_ffmpeg_preset_set(struct RenderData *rd, int preset);
void BKE_ffmpeg_image_type_verify(struct RenderData *rd, const struct ImageFormatData *imf);
bool BKE_ffmpeg_alpha_channel_is_supported(const struct RenderData *rd);

void *BKE_ffmpeg_context_create(void);
void BKE_ffmpeg_context_free(void *context_v);

#  ifdef __cplusplus
}
#  endif

#endif
