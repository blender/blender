/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "BLI_math.h"
#include "BLI_math_matrix.hh"

#include "BKE_bvhutils.h"
#include "BKE_mesh.hh"
#include "BKE_object.h"
#include "BKE_tracking.h"

#include "ED_transform_snap_object_context.h"

#include "transform_snap_object.hh"

eSnapMode snapCamera(SnapObjectContext *sctx,
                     Object *object,
                     const float obmat[4][4],
                     eSnapMode snap_to_flag)
{
  eSnapMode retval = SCE_SNAP_MODE_NONE;

  if (!(sctx->runtime.snap_to_flag & SCE_SNAP_MODE_VERTEX)) {
    return retval;
  }

  Scene *scene = sctx->scene;

  MovieClip *clip = BKE_object_movieclip_get(scene, object, false);
  if (clip == nullptr) {
    return snap_object_center(sctx, object, obmat, snap_to_flag);
  }

  if (object->transflag & OB_DUPLI) {
    return retval;
  }

  float orig_camera_mat[4][4], orig_camera_imat[4][4], imat[4][4];
  BKE_tracking_get_camera_object_matrix(object, orig_camera_mat);

  invert_m4_m4(orig_camera_imat, orig_camera_mat);
  invert_m4_m4(imat, obmat);
  Nearest2dUserData nearest2d(sctx, sctx->ret.dist_px_sq);

  MovieTracking *tracking = &clip->tracking;
  LISTBASE_FOREACH (MovieTrackingObject *, tracking_object, &tracking->objects) {
    float reconstructed_camera_mat[4][4], reconstructed_camera_imat[4][4];
    const float(*vertex_obmat)[4];

    if ((tracking_object->flag & TRACKING_OBJECT_CAMERA) == 0) {
      BKE_tracking_camera_get_reconstructed_interpolate(
          tracking, tracking_object, scene->r.cfra, reconstructed_camera_mat);

      invert_m4_m4(reconstructed_camera_imat, reconstructed_camera_mat);
    }

    LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
      float bundle_pos[3];

      if ((track->flag & TRACK_HAS_BUNDLE) == 0) {
        continue;
      }

      copy_v3_v3(bundle_pos, track->bundle_pos);
      if (tracking_object->flag & TRACKING_OBJECT_CAMERA) {
        vertex_obmat = orig_camera_mat;
      }
      else {
        mul_m4_v3(reconstructed_camera_imat, bundle_pos);
        vertex_obmat = obmat;
      }

      mul_m4_v3(vertex_obmat, bundle_pos);
      if (nearest2d.snap_point(bundle_pos)) {
        retval = SCE_SNAP_MODE_VERTEX;
      }
    }
  }

  if (retval) {
    copy_v3_v3(sctx->ret.loc, nearest2d.nearest_point.co);

    sctx->ret.dist_px_sq = nearest2d.nearest_point.dist_sq;
    sctx->ret.index = nearest2d.nearest_point.index;
    return retval;
  }

  return SCE_SNAP_MODE_NONE;
}
