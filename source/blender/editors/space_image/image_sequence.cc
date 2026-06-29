/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spimage
 */

#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.hh"
#include "BLI_path_utils.hh"
#include "BLI_string.hh"
#include "BLI_utildefines.hh"

#include "DNA_windowmanager_types.h"

#include "RNA_access.hh"

#include "BKE_image.hh"

#include "ED_image.hh"

namespace blender {

/**
 * Get a list of frames from the list of image files matching the first file name sequence pattern.
 * The files and directory are read from standard file-select operator properties.
 *
 * The output is a list of frame ranges, each containing a list of frames with matching names.
 *
 * \param blendfile_path: Blend file path, used to expand the `directory`.
 * \param root_path: When relative is enabled, the path will be made relative to this directory.
 */
static void image_sequence_get_frame_ranges(StringRefNull blendfile_path,
                                            StringRefNull root_path,
                                            wmOperator *op,
                                            ListBaseT<ImageFrameRange> *ranges,
                                            bool *r_was_relative)
{
  char dir[FILE_MAX];
  const bool do_frame_range = RNA_boolean_get(op->ptr, "use_sequence_detection");
  ImageFrameRange *range = nullptr;
  int range_first_frame = 0;
  /* Track when a new series of files are found that aren't compatible with the previous file. */
  char base_head[FILE_MAX], base_tail[FILE_MAX];

  RNA_string_get(op->ptr, "directory", dir);
  /* Make absolute so we can be sure a relative path is always `root_path` relative. */
  BLI_path_abs(dir, blendfile_path.c_str());
  /* Operators using `ED_image_filesel_detect_sequences` should have a `relative_path` option. */
  BLI_assert(RNA_struct_find_property(op->ptr, "relative_path"));
  if (RNA_boolean_get(op->ptr, "relative_path")) {
    BLI_path_rel(dir, root_path.c_str());
  }

  RNA_BEGIN (op->ptr, itemptr, "files") {
    char head[FILE_MAX], tail[FILE_MAX];
    ushort digits;
    char *filename = RNA_string_get_alloc(&itemptr, "name", nullptr, 0, nullptr);
    ImageFrame *frame = MEM_new_zeroed<ImageFrame>("image_frame");

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
      range = MEM_new_zeroed<ImageFrameRange>(__func__);
      BLI_path_join(range->filepath, sizeof(range->filepath), dir, filename);
      BLI_addtail(ranges, range);

      STRNCPY(base_head, head);
      STRNCPY(base_tail, tail);

      range_first_frame = frame->framenr;
    }

    BLI_addtail(&range->frames, frame);
    MEM_delete(filename);
  }
  RNA_END;

  *r_was_relative = BLI_path_is_rel(dir);
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
  /* UDIM detection relies on the paths resolving on the file-system (being absolute). */
  BLI_assert(!BLI_path_is_rel(range->filepath));

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

  ImageFrame *frame_last = static_cast<ImageFrame *>(range->frames.last);
  if (frame_last != nullptr) {
    range->max_framenr = frame_last->framenr;
  }
}

ListBaseT<ImageFrameRange> ED_image_filesel_detect_sequences(StringRefNull blendfile_path,
                                                             StringRefNull root_path,
                                                             wmOperator *op,
                                                             const bool detect_udim)
{
  ListBaseT<ImageFrameRange> ranges;
  ranges.clear_no_delete();

  bool was_relative = false;
  StringRefNull base_path = blendfile_path;

  /* File browser. */
  if (RNA_struct_property_is_set(op->ptr, "directory") &&
      RNA_struct_property_is_set(op->ptr, "files"))
  {
    image_sequence_get_frame_ranges(blendfile_path, root_path, op, &ranges, &was_relative);
    /* The `root_path` will be used as the blend-file path, if it is relative. */
    base_path = root_path;
  }
  /* File-path property for drag & drop etc. */
  else {
    char filepath[FILE_MAX];
    RNA_string_get(op->ptr, "filepath", filepath);

    /* Treat the file-path as a single selected file, equivalent to the `directory` & `files`. */
    const char *filename = BLI_path_basename(filepath);
    if (filename[0]) {
      ImageFrameRange *range = MEM_new_zeroed<ImageFrameRange>(__func__);
      BLI_addtail(&ranges, range);

      STRNCPY(range->filepath, filepath);

      /* Add a single frame so callers building per-frame data have a frame to load.
       * The frame number is decoded from the file-part (matching the `files` case). */
      char head[FILE_MAX], tail[FILE_MAX];
      ushort digits;
      ImageFrame *frame = MEM_new_zeroed<ImageFrame>("image_frame");
      frame->framenr = BLI_path_sequence_decode(
          filename, head, sizeof(head), tail, sizeof(tail), &digits);
      BLI_addtail(&range->frames, frame);

      was_relative = BLI_path_is_rel(filepath);
    }
  }

  for (ImageFrameRange &range : ranges) {
    /* Expand the path if necessary so UDIM files can be resolved. */
    if (was_relative) {
      BLI_path_abs(range.filepath, base_path.c_str());
    }
    image_detect_frame_range(&range, detect_udim);
    if (was_relative) {
      BLI_path_rel(range.filepath, root_path.c_str());
    }
  }

  return ranges;
}

}  // namespace blender
