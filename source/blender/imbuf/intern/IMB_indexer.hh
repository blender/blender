/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#pragma once

#ifdef WIN32
#  include <io.h>
#endif

#include "IMB_anim.hh"
#include <stdio.h>
#include <stdlib.h>
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

struct anim_index_entry {
  int frameno;
  uint64_t seek_pos;
  uint64_t seek_pos_pts;
  uint64_t seek_pos_dts;
  uint64_t pts;
};

struct ImBufAnimIndex {
  char filepath[1024];

  int num_entries;
  anim_index_entry *entries;
};

struct anim_index_builder;

struct anim_index_builder {
  FILE *fp;
  char filepath[FILE_MAX];
  char filepath_temp[FILE_MAX];

  void *private_data;

  void (*delete_priv_data)(anim_index_builder *idx);
  void (*proc_frame)(anim_index_builder *idx,
                     unsigned char *buffer,
                     int data_size,
                     anim_index_entry *entry);
};

anim_index_builder *IMB_index_builder_create(const char *filepath);
void IMB_index_builder_add_entry(anim_index_builder *fp,
                                 int frameno,
                                 uint64_t seek_pos,
                                 uint64_t seek_pos_pts,
                                 uint64_t seek_pos_dts,
                                 uint64_t pts);

void IMB_index_builder_proc_frame(anim_index_builder *fp,
                                  unsigned char *buffer,
                                  int data_size,
                                  int frameno,
                                  uint64_t seek_pos,
                                  uint64_t seek_pos_pts,
                                  uint64_t seek_pos_dts,
                                  uint64_t pts);

void IMB_index_builder_finish(anim_index_builder *fp, int rollback);

ImBufAnimIndex *IMB_indexer_open(const char *name);
uint64_t IMB_indexer_get_seek_pos(ImBufAnimIndex *idx, int frame_index);
uint64_t IMB_indexer_get_seek_pos_pts(ImBufAnimIndex *idx, int frame_index);
uint64_t IMB_indexer_get_seek_pos_dts(ImBufAnimIndex *idx, int frame_index);

int IMB_indexer_get_frame_index(ImBufAnimIndex *idx, int frameno);
uint64_t IMB_indexer_get_pts(ImBufAnimIndex *idx, int frame_index);
int IMB_indexer_get_duration(ImBufAnimIndex *idx);

int IMB_indexer_can_scan(ImBufAnimIndex *idx, int old_frame_index, int new_frame_index);

void IMB_indexer_close(ImBufAnimIndex *idx);

void IMB_free_indices(ImBufAnim *anim);

ImBufAnim *IMB_anim_open_proxy(ImBufAnim *anim, IMB_Proxy_Size preview_size);
ImBufAnimIndex *IMB_anim_open_index(ImBufAnim *anim, IMB_Timecode_Type tc);

int IMB_proxy_size_to_array_index(IMB_Proxy_Size pr_size);
int IMB_timecode_to_array_index(IMB_Timecode_Type tc);
