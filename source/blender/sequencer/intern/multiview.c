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
 *
 * - Blender Foundation, 2003-2009
 * - Peter Schlaile <peter [at] schlaile [dot] de> 2005/2006
 */

/** \file
 * \ingroup bke
 */

#include "DNA_scene_types.h"

#include "BLI_string.h"

#include "BKE_scene.h"

#include "IMB_imbuf.h"

#include "multiview.h"

void seq_anim_add_suffix(Scene *scene, struct anim *anim, const int view_id)
{
  const char *suffix = BKE_scene_multiview_view_id_suffix_get(&scene->r, view_id);
  IMB_suffix_anim(anim, suffix);
}

/* the number of files will vary according to the stereo format */
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
  BLI_assert(ext != NULL && suffix != NULL && prefix != NULL);
  BLI_snprintf(r_path, r_size, "%s%s%s", prefix, suffix, ext);
}
