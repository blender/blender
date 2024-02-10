/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */
#include "MEM_guardedalloc.h"

#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLT_translation.hh"

#include "BKE_anim_visualization.h"
#include "BKE_report.hh"

#include "GPU_batch.h"

#include "BLO_read_write.hh"

/* ******************************************************************** */
/* Animation Visualization */

void animviz_settings_init(bAnimVizSettings *avs)
{
  /* sanity check */
  if (avs == nullptr) {
    return;
  }

  /* path settings */
  avs->path_bc = avs->path_ac = 10;

  avs->path_sf = 1;   /* XXX: Take from scene instead? */
  avs->path_ef = 250; /* XXX: Take from scene instead? */

  avs->path_viewflag = (MOTIONPATH_VIEW_KFRAS | MOTIONPATH_VIEW_KFNOS);

  avs->path_step = 1;

  avs->path_bakeflag |= MOTIONPATH_BAKE_HEADS;
}

/* ------------------- */

void animviz_free_motionpath_cache(bMotionPath *mpath)
{
  /* sanity check */
  if (mpath == nullptr) {
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
  mpath->points = nullptr;
  mpath->length = 0;
}

void animviz_free_motionpath(bMotionPath *mpath)
{
  /* sanity check */
  if (mpath == nullptr) {
    return;
  }

  /* free the cache first */
  animviz_free_motionpath_cache(mpath);

  /* now the instance itself */
  MEM_freeN(mpath);
}

/* ------------------- */

bMotionPath *animviz_copy_motionpath(const bMotionPath *mpath_src)
{
  bMotionPath *mpath_dst;

  if (mpath_src == nullptr) {
    return nullptr;
  }

  mpath_dst = static_cast<bMotionPath *>(MEM_dupallocN(mpath_src));
  mpath_dst->points = static_cast<bMotionPathVert *>(MEM_dupallocN(mpath_src->points));

  /* should get recreated on draw... */
  mpath_dst->points_vbo = nullptr;
  mpath_dst->batch_line = nullptr;
  mpath_dst->batch_points = nullptr;

  return mpath_dst;
}

/* ------------------- */

bMotionPath *animviz_verify_motionpaths(ReportList *reports,
                                        Scene *scene,
                                        Object *ob,
                                        bPoseChannel *pchan)
{
  bAnimVizSettings *avs;
  bMotionPath *mpath, **dst;

  /* sanity checks */
  if (ELEM(nullptr, scene, ob)) {
    return nullptr;
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

  /* Avoid 0 size allocations. */
  if (avs->path_sf >= avs->path_ef) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Motion path frame extents invalid for %s (%d to %d)%s",
                (pchan) ? pchan->name : ob->id.name,
                avs->path_sf,
                avs->path_ef,
                (avs->path_sf == avs->path_ef) ? RPT_(", cannot have single-frame paths") : "");
    return nullptr;
  }

  const int expected_length = avs->path_ef - avs->path_sf;
  BLI_assert(expected_length > 0); /* Because the `if` above. */

  /* If there is already a motionpath, just return that, provided its settings
   * are ok (saves extra free+alloc). */
  if (*dst != nullptr) {
    mpath = *dst;

    if (avs->path_bakeflag & MOTIONPATH_BAKE_CAMERA_SPACE) {
      mpath->flag |= MOTIONPATH_FLAG_BAKE_CAMERA;
    }
    else {
      mpath->flag &= ~MOTIONPATH_FLAG_BAKE_CAMERA;
    }

    /* Only reuse a path if it was already a valid path, and of the expected length. */
    if (mpath->start_frame != mpath->end_frame && mpath->length == expected_length) {
      mpath->start_frame = avs->path_sf;
      mpath->end_frame = avs->path_ef;
      return mpath;
    }

    /* Clear the existing cache, to allocate a new one below. */
    animviz_free_motionpath_cache(mpath);
  }
  else {
    mpath = static_cast<bMotionPath *>(MEM_callocN(sizeof(bMotionPath), "bMotionPath"));
    *dst = mpath;
  }

  /* Copy mpath settings from the viz settings. */
  mpath->start_frame = avs->path_sf;
  mpath->end_frame = avs->path_ef;
  mpath->length = expected_length;

  if (avs->path_bakeflag & MOTIONPATH_BAKE_HEADS) {
    mpath->flag |= MOTIONPATH_FLAG_BHEAD;
  }
  else {
    mpath->flag &= ~MOTIONPATH_FLAG_BHEAD;
  }

  if (avs->path_bakeflag & MOTIONPATH_BAKE_CAMERA_SPACE) {
    mpath->flag |= MOTIONPATH_FLAG_BAKE_CAMERA;
  }
  else {
    mpath->flag &= ~MOTIONPATH_FLAG_BAKE_CAMERA;
  }

  /* Set default custom values (RGB). */
  mpath->color[0] = 1.0;
  mpath->color[1] = 0.0;
  mpath->color[2] = 0.0;

  mpath->line_thickness = 2;
  mpath->flag |= MOTIONPATH_FLAG_LINES;

  /* Allocate a cache. */
  mpath->points = static_cast<bMotionPathVert *>(
      MEM_callocN(sizeof(bMotionPathVert) * mpath->length, "bMotionPathVerts"));

  /* Tag viz settings as currently having some path(s) which use it. */
  avs->path_bakeflag |= MOTIONPATH_BAKE_HAS_PATHS;

  return mpath;
}

void animviz_motionpath_blend_write(BlendWriter *writer, bMotionPath *mpath)
{
  /* sanity checks */
  if (mpath == nullptr) {
    return;
  }

  /* firstly, just write the motionpath struct */
  BLO_write_struct(writer, bMotionPath, mpath);

  /* now write the array of data */
  BLO_write_struct_array(writer, bMotionPathVert, mpath->length, mpath->points);
}

void animviz_motionpath_blend_read_data(BlendDataReader *reader, bMotionPath *mpath)
{
  /* sanity check */
  if (mpath == nullptr) {
    return;
  }

  /* relink points cache */
  BLO_read_data_address(reader, &mpath->points);

  mpath->points_vbo = nullptr;
  mpath->batch_line = nullptr;
  mpath->batch_points = nullptr;
}
