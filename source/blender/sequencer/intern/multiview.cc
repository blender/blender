/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2003-2009 Blender Authors
 * SPDX-FileCopyrightText: 2005-2006 Peter Schlaile <peter [at] schlaile [dot] de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "DNA_scene_types.h"

#include "BLI_string.h"

#include "BKE_scene.hh"

#include "IMB_imbuf.hh"

#include "multiview.hh"

void seq_anim_add_suffix(Scene *scene, ImBufAnim *anim, const int view_id)
{
  const char *suffix = BKE_scene_multiview_view_id_suffix_get(&scene->r, view_id);
  IMB_suffix_anim(anim, suffix);
}

int seq_num_files(Scene *scene, char views_format, const bool is_multiview)
{
  if (!is_multiview) {
    return 1;
  }
  if (views_format == R_IMF_VIEWS_STEREO_3D) {
    return 1;
  }
  /* R_IMF_VIEWS_INDIVIDUAL */

  return BKE_scene_multiview_num_views_get(&scene->r);
}

void seq_multiview_name(Scene *scene,
                        const int view_id,
                        const char *prefix,
                        const char *ext,
                        char *r_path,
                        size_t r_size)
{
  const char *suffix = BKE_scene_multiview_view_id_suffix_get(&scene->r, view_id);
  BLI_assert(ext != nullptr && suffix != nullptr && prefix != nullptr);
  BLI_snprintf(r_path, r_size, "%s%s%s", prefix, suffix, ext);
}
