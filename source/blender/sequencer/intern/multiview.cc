/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2003-2009 Blender Authors
 * SPDX-FileCopyrightText: 2005-2006 Peter Schlaile <peter [at] schlaile [dot] de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "DNA_scene_types.h"

#include "BLI_string.hh"

#include "BKE_scene.hh"

#include "MOV_read.hh"

#include "multiview.hh"

namespace blender::seq {

void seq_anim_add_suffix(Scene *scene, MovieReader *anim, const int view_id)
{
  const char *suffix = BKE_scene_multiview_view_id_suffix_get(&scene->r, view_id);
  MOV_set_multiview_suffix(anim, suffix);
}

int seq_multiview_num_files_get(const Scene *scene, eImageFormat_ViewsFormat views_format)
{
  switch (views_format) {
    case R_IMF_VIEWS_MULTIVIEW:
      /* Unsupported currently, pass through. */
    case R_IMF_VIEWS_STEREO_3D:
      return 1; /* Single lossy combined image. */
    case R_IMF_VIEWS_INDIVIDUAL:
      return BKE_scene_multiview_num_views_get(&scene->r);
  }
  BLI_assert_unreachable();
  return 1;
}

void seq_multiview_name(const Scene *scene,
                        const int view_id,
                        const char *prefix,
                        const char *ext,
                        char *r_path,
                        size_t r_path_maxncpy)
{
  const char *suffix = BKE_scene_multiview_view_id_suffix_get(&scene->r, view_id);
  BLI_assert(ext != nullptr && suffix != nullptr && prefix != nullptr);
  BLI_snprintf(r_path, r_path_maxncpy, "%s%s%s", prefix, suffix, ext);
}

bool seq_multiview_view_filepath_get(const Scene &scene,
                                     const char *filepath,
                                     const int view_id,
                                     char *r_filepath,
                                     const size_t r_filepath_maxncpy,
                                     const char **r_suffix)
{
  char prefix[FILE_MAX];
  const char *ext = nullptr;
  BKE_scene_multiview_view_prefix_get(&scene, filepath, prefix, &ext);
  if (prefix[0] == '\0') {
    return false;
  }

  const char *suffix = BKE_scene_multiview_view_id_suffix_get(&scene.r, view_id);
  if (r_suffix != nullptr) {
    *r_suffix = suffix;
  }
  BLI_snprintf(r_filepath, r_filepath_maxncpy, "%s%s%s", prefix, suffix, ext);
  return true;
}

}  // namespace blender::seq
