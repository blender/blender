/* SPDX-FileCopyrightText: 2023-2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#pragma once

#include "BLI_vector.hh"
#include "IMB_imbuf_enums.h"
#include "MOV_enums.hh"

struct MovieReader;

/*
 * separate animation index files to solve the following problems:
 *
 * a) different time-codes within one file (like DTS/PTS, Time-code-Track,
 *    "implicit" time-codes within DV-files and HDV-files etc.)
 * b) seeking difficulties within FFMPEG for files with timestamp holes
 * c) broken files that miss several frames / have varying frame-rates
 * d) use proxies accordingly
 *
 * ... we need index files, that provide us with
 *
 * the binary(!) position, where we have to seek into the file *and*
 * the continuous frame number (ignoring the holes) starting from the
 * beginning of the file, so that we know, which proxy frame to serve.
 *
 * This index has to be only built once for a file and is written into
 * the BL_proxy directory structure for later reuse in different blender files.
 */

struct MovieIndexFrame {
  int frameno;
  uint64_t seek_pos_pts;
  uint64_t seek_pos_dts;
  uint64_t pts;
};

struct MovieIndex {
  char filepath[1024];

  blender::Vector<MovieIndexFrame> entries;

  uint64_t get_seek_pos_pts(int frame_index) const;
  uint64_t get_seek_pos_dts(int frame_index) const;

  int get_frame_index(int frameno) const;
  uint64_t get_pts(int frame_index) const;
  int get_duration() const;
};

MovieReader *movie_open_proxy(MovieReader *anim, IMB_Proxy_Size preview_size);
const MovieIndex *movie_open_index(MovieReader *anim, IMB_Timecode_Type tc);
