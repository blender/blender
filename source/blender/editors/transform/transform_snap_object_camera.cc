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

using namespace blender;

eSnapMode snapCamera(SnapObjectContext *sctx,
                     Object *object,
                     const float4x4 &obmat,
                     eSnapMode snap_to_flag)
{
  eSnapMode retval = SCE_SNAP_TO_NONE;

  if (!(sctx->runtime.snap_to_flag & SCE_SNAP_TO_POINT)) {
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

  float4x4 orig_camera_mat;
  BKE_tracking_get_camera_object_matrix(object, orig_camera_mat.ptr());

  SnapData nearest2d(sctx);
  nearest2d.clip_planes_enable(sctx);

  MovieTracking *tracking = &clip->tracking;
  LISTBASE_FOREACH (MovieTrackingObject *, tracking_object, &tracking->objects) {
    float4x4 reconstructed_camera_imat;

    if ((tracking_object->flag & TRACKING_OBJECT_CAMERA) == 0) {
      float4x4 reconstructed_camera_mat;
      BKE_tracking_camera_get_reconstructed_interpolate(
          tracking, tracking_object, scene->r.cfra, reconstructed_camera_mat.ptr());

      reconstructed_camera_imat = math::invert(reconstructed_camera_mat) * obmat;
    }

    LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
      float3 bundle_pos;

      if ((track->flag & TRACK_HAS_BUNDLE) == 0) {
        continue;
      }

      if (tracking_object->flag & TRACKING_OBJECT_CAMERA) {
        bundle_pos = math::transform_point(orig_camera_mat, float3(track->bundle_pos));
      }
      else {
        bundle_pos = math::transform_point(reconstructed_camera_imat, float3(track->bundle_pos));
      }

      if (nearest2d.snap_point(bundle_pos)) {
        retval = SCE_SNAP_TO_POINT;
      }
    }
  }

  if (retval) {
    nearest2d.register_result(sctx, object, static_cast<const ID *>(object->data));
  }
  return retval;
}
