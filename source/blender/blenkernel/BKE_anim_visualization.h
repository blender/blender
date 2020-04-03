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
#ifndef __BKE_ANIM_VISUALIZATION_H__
#define __BKE_ANIM_VISUALIZATION_H__

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct Object;
struct ReportList;
struct Scene;
struct bAnimVizSettings;
struct bMotionPath;
struct bPoseChannel;

/* ---------------------------------------------------- */
/* Animation Visualization */

void animviz_settings_init(struct bAnimVizSettings *avs);

struct bMotionPath *animviz_copy_motionpath(const struct bMotionPath *mpath_src);

void animviz_free_motionpath_cache(struct bMotionPath *mpath);
void animviz_free_motionpath(struct bMotionPath *mpath);

struct bMotionPath *animviz_verify_motionpaths(struct ReportList *reports,
                                               struct Scene *scene,
                                               struct Object *ob,
                                               struct bPoseChannel *pchan);

#ifdef __cplusplus
}
#endif

#endif
