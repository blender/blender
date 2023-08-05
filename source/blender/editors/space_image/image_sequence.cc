/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spimage
 */

#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_fileops.h"
#include "BLI_fileops_types.h"
#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_image_types.h"
#include "DNA_windowmanager_types.h"

#include "RNA_access.h"

#include "BKE_image.h"
#include "BKE_main.h"

#include "ED_image.hh"

struct ImageFrame {
  ImageFrame *next, *prev;
  int framenr;
};

/**
 * Get a list of frames from the list of image files matching the first file name sequence pattern.
 * The files and directory are read from standard file-select operator properties.
 *
 * The output is a list of frame ranges, each containing a list of frames with matching names.
 */
static void image_sequence_get_frame_ranges(wmOperator *op, ListBase *ranges)
{
  char dir[FILE_MAXDIR];
  const bool do_frame_range = RNA_boolean_get(op->ptr, "use_sequence_detection");
  ImageFrameRange *range = nullptr;
  int range_first_frame = 0;
  /* Track when a new series of files are found that aren't compatible with the previous file. */
  char base_head[FILE_MAX], base_tail[FILE_MAX];

  RNA_string_get(op->ptr, "directory", dir);
  RNA_BEGIN (op->ptr, itemptr, "files") {
    char head[FILE_MAX], tail[FILE_MAX];
    ushort digits;
    char *filename = RNA_string_get_alloc(&itemptr, "name", nullptr, 0, nullptr);
    ImageFrame *frame = static_cast<ImageFrame *>(MEM_callocN(sizeof(ImageFrame), "image_frame"));

    /* use the first file in the list as base filename */
    frame->framenr = BLI_path_sequence_decode(
        filename, head, sizeof(head), tail, sizeof(tail), &digits);

    /* still in the same sequence */
    if (do_frame_range && (range != nullptr) && STREQLEN(base_head, head, FILE_MAX) &&
        STREQLEN(base_tail, tail, FILE_MAX))
    {
      /* Set filepath to first frame in the range. */
      if (frame->framenr < range_first_frame) {
        BLI_path_join(range->filepath, sizeof(range->filepath), dir, filename);
        range_first_frame = frame->framenr;
      }
    }
    else {
      /* start a new frame range */
      range = static_cast<ImageFrameRange *>(MEM_callocN(sizeof(*range), __func__));
      BLI_path_join(range->filepath, sizeof(range->filepath), dir, filename);
      BLI_addtail(ranges, range);

      STRNCPY(base_head, head);
      STRNCPY(base_tail, tail);

      range_first_frame = frame->framenr;
    }

    BLI_addtail(&range->frames, frame);
    MEM_freeN(filename);
  }
  RNA_END;
}

static int image_cmp_frame(const void *a, const void *b)
{
  const ImageFrame *frame_a = static_cast<const ImageFrame *>(a);
  const ImageFrame *frame_b = static_cast<const ImageFrame *>(b);

  if (frame_a->framenr < frame_b->framenr) {
    return -1;
  }
  if (frame_a->framenr > frame_b->framenr) {
    return 1;
  }
  return 0;
}

/**
 * From a list of frames, compute the start (offset) and length of the sequence
 * of contiguous frames. If `detect_udim` is set, it will return UDIM tiles as well.
 */
static void image_detect_frame_range(ImageFrameRange *range, const bool detect_udim)
{
  /* UDIM */
  if (detect_udim) {
    int udim_start, udim_range;
    range->udims_detected = BKE_image_get_tile_info(
        range->filepath, &range->udim_tiles, &udim_start, &udim_range);

    if (range->udims_detected) {
      range->offset = udim_start;
      range->length = udim_range;
      return;
    }
  }

  /* Image Sequence */
  BLI_listbase_sort(&range->frames, image_cmp_frame);

  ImageFrame *frame = static_cast<ImageFrame *>(range->frames.first);
  if (frame != nullptr) {
    int frame_curr = frame->framenr;
    range->offset = frame_curr;

    while (frame != nullptr && (frame->framenr == frame_curr)) {
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

ListBase ED_image_filesel_detect_sequences(Main *bmain, wmOperator *op, const bool detect_udim)
{
  ListBase ranges;
  BLI_listbase_clear(&ranges);

  char filepath[FILE_MAX];
  RNA_string_get(op->ptr, "filepath", filepath);

  /* File browser. */
  if (RNA_struct_property_is_set(op->ptr, "directory") &&
      RNA_struct_property_is_set(op->ptr, "files"))
  {
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
    ImageFrameRange *range = static_cast<ImageFrameRange *>(MEM_callocN(sizeof(*range), __func__));
    BLI_addtail(&ranges, range);

    STRNCPY(range->filepath, filepath);
    image_detect_frame_range(range, detect_udim);
  }

  return ranges;
}
