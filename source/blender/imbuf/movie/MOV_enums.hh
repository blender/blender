/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup imbuf
 */

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
  FFMPEG_PRESET_H264 = 1,
  FFMPEG_PRESET_THEORA = 2,
  FFMPEG_PRESET_XVID = 3,
  FFMPEG_PRESET_AV1 = 4,
};

/**
 * Time-code files contain timestamps (PTS, DTS) and packet seek position.
 * These values are obtained by decoding each frame in movie stream. Time-code types define how
 * these map to frame index in Blender. This is used when seeking in movie stream. Note, that
 * meaning of terms time-code and record run here has little connection to their actual meaning.
 *
 * NOTE: Keep in sync with #MovieClipProxy.build_tc_flag.
 */
enum IMB_Timecode_Type {
  /** Don't use time-code files at all. Use FFmpeg API to seek to PTS calculated on the fly. */
  IMB_TC_NONE = 0,
  /**
   * TC entries (and therefore frames in movie stream) are mapped to frame index, such that
   * timestamp in Blender matches timestamp in the movie stream. This assumes, that time starts at
   * 0 in both cases.
   *
   * Simplified formula is `frame_index = movie_stream_timestamp * FPS`.
   */
  IMB_TC_RECORD_RUN = 1,
  /**
   * Each TC entry (and therefore frame in movie stream) is mapped to new frame index in Blender.
   *
   * For example: FFmpeg may say, that a frame should be displayed for 0.5 seconds, but this option
   * ignores that and only displays it in one particular frame index in Blender.
   */
  IMB_TC_RECORD_RUN_NO_GAPS = 8,
  IMB_TC_NUM_TYPES = 2,
};
