/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup spimage
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_fileops.h"
#include "BLI_fileops_types.h"
#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_image_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "RNA_access.h"

#include "BKE_image.h"
#include "BKE_main.h"

#include "ED_image.h"

#include "WM_types.h"

typedef struct ImageFrame {
  struct ImageFrame *next, *prev;
  int framenr;
} ImageFrame;

/**
 * Get a list of frames from the list of image files matching the first file
 * name sequence pattern. The files and directory are read from standard
 * fileselect operator properties.
 *
 * The output is a list of frame ranges, each containing a list of frames with matching names.
 */
static void image_sequence_get_frame_ranges(wmOperator *op, ListBase *ranges)
{
  char dir[FILE_MAXDIR];
  const bool do_frame_range = RNA_boolean_get(op->ptr, "use_sequence_detection");
  ImageFrameRange *range = NULL;

  RNA_string_get(op->ptr, "directory", dir);
  RNA_BEGIN (op->ptr, itemptr, "files") {
    char base_head[FILE_MAX], base_tail[FILE_MAX];
    char head[FILE_MAX], tail[FILE_MAX];
    ushort digits;
    char *filename = RNA_string_get_alloc(&itemptr, "name", NULL, 0);
    ImageFrame *frame = MEM_callocN(sizeof(ImageFrame), "image_frame");

    /* use the first file in the list as base filename */
    frame->framenr = BLI_path_sequence_decode(filename, head, tail, &digits);

    /* still in the same sequence */
    if (do_frame_range && (range != NULL) && (STREQLEN(base_head, head, FILE_MAX)) &&
        (STREQLEN(base_tail, tail, FILE_MAX))) {
      /* pass */
    }
    else {
      /* start a new frame range */
      range = MEM_callocN(sizeof(*range), __func__);
      BLI_join_dirfile(range->filepath, sizeof(range->filepath), dir, filename);
      BLI_addtail(ranges, range);

      BLI_strncpy(base_head, head, sizeof(base_head));
      BLI_strncpy(base_tail, tail, sizeof(base_tail));
    }

    BLI_addtail(&range->frames, frame);
    MEM_freeN(filename);
  }
  RNA_END;
}

static int image_cmp_frame(const void *a, const void *b)
{
  const ImageFrame *frame_a = a;
  const ImageFrame *frame_b = b;

  if (frame_a->framenr < frame_b->framenr) {
    return -1;
  }
  if (frame_a->framenr > frame_b->framenr) {
    return 1;
  }
  return 0;
}

/*
 * Checks whether the given filepath refers to a UDIM texture.
 * If yes, the range from 1001 to the highest tile is returned, otherwise 0.
 *
 * If the result is positive, the filepath will be overwritten with that of
 * the 1001 tile.
 *
 * udim_tiles may get filled even if the result ultimately is false!
 */
static int image_get_udim(char *filepath, ListBase *udim_tiles)
{
  char filename[FILE_MAX], dirname[FILE_MAXDIR];
  BLI_split_dirfile(filepath, dirname, filename, sizeof(dirname), sizeof(filename));

  ushort digits;
  char base_head[FILE_MAX], base_tail[FILE_MAX];
  int id = BLI_path_sequence_decode(filename, base_head, base_tail, &digits);

  if (id < 1001 || id >= IMA_UDIM_MAX) {
    return 0;
  }

  bool is_udim = true;
  bool has_primary = false;
  int max_udim = 0;

  struct direntry *dir;
  uint totfile = BLI_filelist_dir_contents(dirname, &dir);
  for (int i = 0; i < totfile; i++) {
    if (!(dir[i].type & S_IFREG)) {
      continue;
    }
    char head[FILE_MAX], tail[FILE_MAX];
    id = BLI_path_sequence_decode(dir[i].relname, head, tail, &digits);

    if (digits > 4 || !(STREQLEN(base_head, head, FILE_MAX)) ||
        !(STREQLEN(base_tail, tail, FILE_MAX))) {
      continue;
    }

    if (id < 1001 || id >= IMA_UDIM_MAX) {
      is_udim = false;
      break;
    }
    if (id == 1001) {
      has_primary = true;
    }

    BLI_addtail(udim_tiles, BLI_genericNodeN(POINTER_FROM_INT(id)));
    max_udim = max_ii(max_udim, id);
  }
  BLI_filelist_free(dir, totfile);

  if (is_udim && has_primary) {
    char primary_filename[FILE_MAX];
    BLI_path_sequence_encode(primary_filename, base_head, base_tail, digits, 1001);
    BLI_join_dirfile(filepath, FILE_MAX, dirname, primary_filename);
    return max_udim - 1000;
  }
  return 0;
}

/**
 * From a list of frames, compute the start (offset) and length of the sequence
 * of contiguous frames. If UDIM is detect, it will return UDIM tiles as well.
 */
static void image_detect_frame_range(ImageFrameRange *range, const bool detect_udim)
{
  /* UDIM */
  if (detect_udim) {
    int len_udim = image_get_udim(range->filepath, &range->udim_tiles);

    if (len_udim > 0) {
      range->offset = 1001;
      range->length = len_udim;
      return;
    }
  }

  /* Image Sequence */
  BLI_listbase_sort(&range->frames, image_cmp_frame);

  ImageFrame *frame = range->frames.first;
  if (frame != NULL) {
    int frame_curr = frame->framenr;
    range->offset = frame_curr;

    while (frame != NULL && (frame->framenr == frame_curr)) {
      frame_curr++;
      frame = frame->next;
    }

    range->length = frame_curr - range->offset;
  }
  else {
    range->length = 1;
    range->offset = 0;
  }
}

/* Used for both images and volume file loading. */
ListBase ED_image_filesel_detect_sequences(Main *bmain, wmOperator *op, const bool detect_udim)
{
  ListBase ranges;
  BLI_listbase_clear(&ranges);

  char filepath[FILE_MAX];
  RNA_string_get(op->ptr, "filepath", filepath);

  /* File browser. */
  if (RNA_struct_property_is_set(op->ptr, "directory") &&
      RNA_struct_property_is_set(op->ptr, "files")) {
    const bool was_relative = BLI_path_is_rel(filepath);

    image_sequence_get_frame_ranges(op, &ranges);
    LISTBASE_FOREACH (ImageFrameRange *, range, &ranges) {
      image_detect_frame_range(range, detect_udim);
      BLI_freelistN(&range->frames);

      if (was_relative) {
        BLI_path_rel(range->filepath, BKE_main_blendfile_path(bmain));
      }
    }
  }
  /* Filepath property for drag & drop etc. */
  else {
    ImageFrameRange *range = MEM_callocN(sizeof(*range), __func__);
    BLI_addtail(&ranges, range);

    BLI_strncpy(range->filepath, filepath, FILE_MAX);
    image_detect_frame_range(range, detect_udim);
  }

  return ranges;
}
