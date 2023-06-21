/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "BLI_math.h"
#include "BLI_math_matrix.hh"

#include "BKE_armature.h"
#include "BKE_bvhutils.h"
#include "BKE_mesh.hh"
#include "DNA_armature_types.h"

#include "ED_transform_snap_object_context.h"

#include "transform_snap_object.hh"

using blender::float4x4;

eSnapMode snapArmature(SnapObjectContext *sctx,
                       Object *ob_eval,
                       const float obmat[4][4],
                       bool is_object_active)
{
  eSnapMode retval = SCE_SNAP_MODE_NONE;

  if (sctx->runtime.snap_to_flag == SCE_SNAP_MODE_FACE) {
    /* Currently only edge and vert. */
    return retval;
  }

  Nearest2dUserData nearest2d(sctx, sctx->ret.dist_px_sq, float4x4(obmat));

  bArmature *arm = static_cast<bArmature *>(ob_eval->data);
  const bool is_editmode = arm->edbo != nullptr;

  if (is_editmode == false) {
    const BoundBox *bb = BKE_armature_boundbox_get(ob_eval);
    if (bb && !nearest2d.snap_boundbox(bb->vec[0], bb->vec[6])) {
      return retval;
    }
  }

  nearest2d.clip_planes_get(sctx, float4x4(obmat));

  const bool is_posemode = is_object_active && (ob_eval->mode & OB_MODE_POSE);
  const bool skip_selected = (is_editmode || is_posemode) &&
                             (sctx->runtime.params.snap_target_select &
                              SCE_SNAP_TARGET_NOT_SELECTED);

  if (arm->edbo) {
    LISTBASE_FOREACH (EditBone *, eBone, arm->edbo) {
      if (eBone->layer & arm->layer) {
        if (eBone->flag & BONE_HIDDEN_A) {
          /* Skip hidden bones. */
          continue;
        }

        const bool is_selected = (eBone->flag & (BONE_ROOTSEL | BONE_TIPSEL)) != 0;
        if (is_selected && skip_selected) {
          continue;
        }
        bool has_vert_snap = false;

        if (sctx->runtime.snap_to_flag & SCE_SNAP_MODE_VERTEX) {
          has_vert_snap |= nearest2d.snap_point(eBone->head);
          has_vert_snap |= nearest2d.snap_point(eBone->tail);
          if (has_vert_snap) {
            retval = SCE_SNAP_MODE_VERTEX;
          }
        }
        if (!has_vert_snap && sctx->runtime.snap_to_flag & SCE_SNAP_MODE_EDGE) {
          if (nearest2d.snap_edge(eBone->head, eBone->tail)) {
            retval = SCE_SNAP_MODE_EDGE;
          }
        }
      }
    }
  }
  else if (ob_eval->pose && ob_eval->pose->chanbase.first) {
    LISTBASE_FOREACH (bPoseChannel *, pchan, &ob_eval->pose->chanbase) {
      Bone *bone = pchan->bone;
      if (!bone || (bone->flag & (BONE_HIDDEN_P | BONE_HIDDEN_PG))) {
        /* Skip hidden bones. */
        continue;
      }

      const bool is_selected = (bone->flag & (BONE_SELECTED | BONE_ROOTSEL | BONE_TIPSEL)) != 0;
      if (is_selected && skip_selected) {
        continue;
      }

      bool has_vert_snap = false;
      const float *head_vec = pchan->pose_head;
      const float *tail_vec = pchan->pose_tail;

      if (sctx->runtime.snap_to_flag & SCE_SNAP_MODE_VERTEX) {
        has_vert_snap |= nearest2d.snap_point(head_vec);
        has_vert_snap |= nearest2d.snap_point(tail_vec);
        if (has_vert_snap) {
          retval = SCE_SNAP_MODE_VERTEX;
        }
      }
      if (!has_vert_snap && sctx->runtime.snap_to_flag & SCE_SNAP_MODE_EDGE) {
        if (nearest2d.snap_edge(head_vec, tail_vec)) {
          retval = SCE_SNAP_MODE_EDGE;
        }
      }
    }
  }

  if (retval) {
    mul_v3_m4v3(sctx->ret.loc, obmat, nearest2d.nearest_point.co);

    sctx->ret.dist_px_sq = nearest2d.nearest_point.dist_sq;
    sctx->ret.index = nearest2d.nearest_point.index;
    return retval;
  }

  return SCE_SNAP_MODE_NONE;
}
