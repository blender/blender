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

/* Note: match ffmpeg AVCodecID enum values. */
enum IMB_Ffmpeg_Codec_ID {
  FFMPEG_CODEC_ID_NONE = 0,
  FFMPEG_CODEC_ID_MPEG1VIDEO = 1,
  FFMPEG_CODEC_ID_MPEG2VIDEO = 2,
  FFMPEG_CODEC_ID_MPEG4 = 12,
  FFMPEG_CODEC_ID_FLV1 = 21,
  FFMPEG_CODEC_ID_DVVIDEO = 24,
  FFMPEG_CODEC_ID_HUFFYUV = 25,
  FFMPEG_CODEC_ID_H264 = 27,
  FFMPEG_CODEC_ID_THEORA = 30,
  FFMPEG_CODEC_ID_FFV1 = 33,
  FFMPEG_CODEC_ID_QTRLE = 55,
  FFMPEG_CODEC_ID_PNG = 61,
  FFMPEG_CODEC_ID_DNXHD = 99,
  FFMPEG_CODEC_ID_VP9 = 167,
  FFMPEG_CODEC_ID_H265 = 173,
  FFMPEG_CODEC_ID_AV1 = 226,
  FFMPEG_CODEC_ID_PCM_S16LE = 65536,
  FFMPEG_CODEC_ID_MP2 = 86016,
  FFMPEG_CODEC_ID_MP3 = 86017,
  FFMPEG_CODEC_ID_AAC = 86018,
  FFMPEG_CODEC_ID_AC3 = 86019,
  FFMPEG_CODEC_ID_VORBIS = 86021,
  FFMPEG_CODEC_ID_FLAC = 86028,
  FFMPEG_CODEC_ID_OPUS = 86076,
};

/**
 * Time-code files contain timestamps (PTS, DTS) and packet seek position.
 * These values are obtained by decoding each frame in movie stream. Time-code types define how
 * these map to frame index in Blender. This is used when seeking in movie stream. Note, that
 * meaning of terms time-code and record run here has little connection to their actual meaning.
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
