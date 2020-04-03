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
 * \ingroup bke
 */
#include "MEM_guardedalloc.h"

#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLT_translation.h"

#include "BKE_anim_visualization.h"
#include "BKE_report.h"

#include "GPU_batch.h"

/* ******************************************************************** */
/* Animation Visualization */

/* Initialize the default settings for animation visualization */
void animviz_settings_init(bAnimVizSettings *avs)
{
  /* sanity check */
  if (avs == NULL) {
    return;
  }

  /* path settings */
  avs->path_bc = avs->path_ac = 10;

  avs->path_sf = 1;   /* xxx - take from scene instead? */
  avs->path_ef = 250; /* xxx - take from scene instead? */

  avs->path_viewflag = (MOTIONPATH_VIEW_KFRAS | MOTIONPATH_VIEW_KFNOS);

  avs->path_step = 1;

  avs->path_bakeflag |= MOTIONPATH_BAKE_HEADS;
}

/* ------------------- */

/* Free the given motion path's cache */
void animviz_free_motionpath_cache(bMotionPath *mpath)
{
  /* sanity check */
  if (mpath == NULL) {
    return;
  }

  /* free the path if necessary */
  if (mpath->points) {
    MEM_freeN(mpath->points);
  }

  GPU_VERTBUF_DISCARD_SAFE(mpath->points_vbo);
  GPU_BATCH_DISCARD_SAFE(mpath->batch_line);
  GPU_BATCH_DISCARD_SAFE(mpath->batch_points);

  /* reset the relevant parameters */
  mpath->points = NULL;
  mpath->length = 0;
}

/* Free the given motion path instance and its data
 * NOTE: this frees the motion path given!
 */
void animviz_free_motionpath(bMotionPath *mpath)
{
  /* sanity check */
  if (mpath == NULL) {
    return;
  }

  /* free the cache first */
  animviz_free_motionpath_cache(mpath);

  /* now the instance itself */
  MEM_freeN(mpath);
}

/* ------------------- */

/* Make a copy of motionpath data, so that viewing with copy on write works */
bMotionPath *animviz_copy_motionpath(const bMotionPath *mpath_src)
{
  bMotionPath *mpath_dst;

  if (mpath_src == NULL) {
    return NULL;
  }

  mpath_dst = MEM_dupallocN(mpath_src);
  mpath_dst->points = MEM_dupallocN(mpath_src->points);

  /* should get recreated on draw... */
  mpath_dst->points_vbo = NULL;
  mpath_dst->batch_line = NULL;
  mpath_dst->batch_points = NULL;

  return mpath_dst;
}

/* ------------------- */

/**
 * Setup motion paths for the given data.
 * \note Only used when explicitly calculating paths on bones which may/may not be consider already
 *
 * \param scene: Current scene (for frame ranges, etc.)
 * \param ob: Object to add paths for (must be provided)
 * \param pchan: Posechannel to add paths for (optional; if not provided, object-paths are assumed)
 */
bMotionPath *animviz_verify_motionpaths(ReportList *reports,
                                        Scene *scene,
                                        Object *ob,
                                        bPoseChannel *pchan)
{
  bAnimVizSettings *avs;
  bMotionPath *mpath, **dst;

  /* sanity checks */
  if (ELEM(NULL, scene, ob)) {
    return NULL;
  }

  /* get destination data */
  if (pchan) {
    /* paths for posechannel - assume that posechannel belongs to the object */
    avs = &ob->pose->avs;
    dst = &pchan->mpath;
  }
  else {
    /* paths for object */
    avs = &ob->avs;
    dst = &ob->mpath;
  }

  /* avoid 0 size allocs */
  if (avs->path_sf >= avs->path_ef) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Motion path frame extents invalid for %s (%d to %d)%s",
                (pchan) ? pchan->name : ob->id.name,
                avs->path_sf,
                avs->path_ef,
                (avs->path_sf == avs->path_ef) ? TIP_(", cannot have single-frame paths") : "");
    return NULL;
  }

  /* if there is already a motionpath, just return that,
   * provided it's settings are ok (saves extra free+alloc)
   */
  if (*dst != NULL) {
    int expected_length = avs->path_ef - avs->path_sf;

    mpath = *dst;

    /* Path is "valid" if length is valid,
     * but must also be of the same length as is being requested. */
    if ((mpath->start_frame != mpath->end_frame) && (mpath->length > 0)) {
      /* outer check ensures that we have some curve data for this path */
      if (mpath->length == expected_length) {
        /* return/use this as it is already valid length */
        return mpath;
      }
      else {
        /* clear the existing path (as the range has changed), and reallocate below */
        animviz_free_motionpath_cache(mpath);
      }
    }
  }
  else {
    /* create a new motionpath, and assign it */
    mpath = MEM_callocN(sizeof(bMotionPath), "bMotionPath");
    *dst = mpath;
  }

  /* set settings from the viz settings */
  mpath->start_frame = avs->path_sf;
  mpath->end_frame = avs->path_ef;

  mpath->length = mpath->end_frame - mpath->start_frame;

  if (avs->path_bakeflag & MOTIONPATH_BAKE_HEADS) {
    mpath->flag |= MOTIONPATH_FLAG_BHEAD;
  }
  else {
    mpath->flag &= ~MOTIONPATH_FLAG_BHEAD;
  }

  /* set default custom values */
  mpath->color[0] = 1.0; /* Red */
  mpath->color[1] = 0.0;
  mpath->color[2] = 0.0;

  mpath->line_thickness = 2;
  mpath->flag |= MOTIONPATH_FLAG_LINES; /* draw lines by default */

  /* allocate a cache */
  mpath->points = MEM_callocN(sizeof(bMotionPathVert) * mpath->length, "bMotionPathVerts");

  /* tag viz settings as currently having some path(s) which use it */
  avs->path_bakeflag |= MOTIONPATH_BAKE_HAS_PATHS;

  /* return it */
  return mpath;
}
