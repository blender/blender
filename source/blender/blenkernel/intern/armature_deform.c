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
 *
 * Deform coordinates by a armature object (used by modifier).
 */

#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "DNA_armature_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_lattice_types.h"
#include "DNA_listBase.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_deform.h"
#include "BKE_lattice.h"

#include "DEG_depsgraph_build.h"

#include "CLG_log.h"

static CLG_LogRef LOG = {"bke.armature_deform"};

/* -------------------------------------------------------------------- */
/** \name Armature Deform Internal Utilities
 * \{ */

/* Add the effect of one bone or B-Bone segment to the accumulated result. */
static void pchan_deform_accumulate(const DualQuat *deform_dq,
                                    const float deform_mat[4][4],
                                    const float co_in[3],
                                    float weight,
                                    float co_accum[3],
                                    DualQuat *dq_accum,
                                    float mat_accum[3][3])
{
  if (weight == 0.0f) {
    return;
  }

  if (dq_accum) {
    BLI_assert(!co_accum);

    add_weighted_dq_dq(dq_accum, deform_dq, weight);
  }
  else {
    float tmp[3];
    mul_v3_m4v3(tmp, deform_mat, co_in);

    sub_v3_v3(tmp, co_in);
    madd_v3_v3fl(co_accum, tmp, weight);

    if (mat_accum) {
      float tmpmat[3][3];
      copy_m3_m4(tmpmat, deform_mat);

      madd_m3_m3m3fl(mat_accum, mat_accum, tmpmat, weight);
    }
  }
}

static void b_bone_deform(const bPoseChannel *pchan,
                          const float co[3],
                          float weight,
                          float vec[3],
                          DualQuat *dq,
                          float defmat[3][3])
{
  const DualQuat *quats = pchan->runtime.bbone_dual_quats;
  const Mat4 *mats = pchan->runtime.bbone_deform_mats;
  const float(*mat)[4] = mats[0].mat;
  float blend, y;
  int index;

  /* Transform co to bone space and get its y component. */
  y = mat[0][1] * co[0] + mat[1][1] * co[1] + mat[2][1] * co[2] + mat[3][1];

  /* Calculate the indices of the 2 affecting b_bone segments. */
  BKE_pchan_bbone_deform_segment_index(pchan, y / pchan->bone->length, &index, &blend);

  pchan_deform_accumulate(
      &quats[index], mats[index + 1].mat, co, weight * (1.0f - blend), vec, dq, defmat);
  pchan_deform_accumulate(
      &quats[index + 1], mats[index + 2].mat, co, weight * blend, vec, dq, defmat);
}

/* using vec with dist to bone b1 - b2 */
float distfactor_to_bone(
    const float vec[3], const float b1[3], const float b2[3], float rad1, float rad2, float rdist)
{
  float dist_sq;
  float bdelta[3];
  float pdelta[3];
  float hsqr, a, l, rad;

  sub_v3_v3v3(bdelta, b2, b1);
  l = normalize_v3(bdelta);

  sub_v3_v3v3(pdelta, vec, b1);

  a = dot_v3v3(bdelta, pdelta);
  hsqr = len_squared_v3(pdelta);

  if (a < 0.0f) {
    /* If we're past the end of the bone, do a spherical field attenuation thing */
    dist_sq = len_squared_v3v3(b1, vec);
    rad = rad1;
  }
  else if (a > l) {
    /* If we're past the end of the bone, do a spherical field attenuation thing */
    dist_sq = len_squared_v3v3(b2, vec);
    rad = rad2;
  }
  else {
    dist_sq = (hsqr - (a * a));

    if (l != 0.0f) {
      rad = a / l;
      rad = rad * rad2 + (1.0f - rad) * rad1;
    }
    else {
      rad = rad1;
    }
  }

  a = rad * rad;
  if (dist_sq < a) {
    return 1.0f;
  }
  else {
    l = rad + rdist;
    l *= l;
    if (rdist == 0.0f || dist_sq >= l) {
      return 0.0f;
    }
    else {
      a = sqrtf(dist_sq) - rad;
      return 1.0f - (a * a) / (rdist * rdist);
    }
  }
}

static float dist_bone_deform(
    bPoseChannel *pchan, float vec[3], DualQuat *dq, float mat[3][3], const float co[3])
{
  Bone *bone = pchan->bone;
  float fac, contrib = 0.0;

  if (bone == NULL) {
    return 0.0f;
  }

  fac = distfactor_to_bone(
      co, bone->arm_head, bone->arm_tail, bone->rad_head, bone->rad_tail, bone->dist);

  if (fac > 0.0f) {
    fac *= bone->weight;
    contrib = fac;
    if (contrib > 0.0f) {
      if (bone->segments > 1 && pchan->runtime.bbone_segments == bone->segments) {
        b_bone_deform(pchan, co, fac, vec, dq, mat);
      }
      else {
        pchan_deform_accumulate(
            &pchan->runtime.deform_dual_quat, pchan->chan_mat, co, fac, vec, dq, mat);
      }
    }
  }

  return contrib;
}

static void pchan_bone_deform(bPoseChannel *pchan,
                              float weight,
                              float vec[3],
                              DualQuat *dq,
                              float mat[3][3],
                              const float co[3],
                              float *contrib)
{
  Bone *bone = pchan->bone;

  if (!weight) {
    return;
  }

  if (bone->segments > 1 && pchan->runtime.bbone_segments == bone->segments) {
    b_bone_deform(pchan, co, weight, vec, dq, mat);
  }
  else {
    pchan_deform_accumulate(
        &pchan->runtime.deform_dual_quat, pchan->chan_mat, co, weight, vec, dq, mat);
  }

  (*contrib) += weight;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Armature Deform #BKE_armature_deform_coords API
 *
 * #BKE_armature_deform_coords and related functions.
 * \{ */

typedef struct ArmatureUserdata {
  Object *ob_arm;
  Object *ob_target;
  const Mesh *me_target;
  float (*vert_coords)[3];
  float (*vert_deform_mats)[3][3];
  float (*vert_coords_prev)[3];

  bool use_envelope;
  bool use_quaternion;
  bool invert_vgroup;
  bool use_dverts;

  int armature_def_nr;

  const MDeformVert *dverts;
  int dverts_len;

  bPoseChannel **pchan_from_defbase;
  int defbase_len;

  float premat[4][4];
  float postmat[4][4];
} ArmatureUserdata;

static void armature_vert_task(void *__restrict userdata,
                               const int i,
                               const TaskParallelTLS *__restrict UNUSED(tls))
{
  const ArmatureUserdata *data = userdata;
  float(*const vert_coords)[3] = data->vert_coords;
  float(*const vert_deform_mats)[3][3] = data->vert_deform_mats;
  float(*const vert_coords_prev)[3] = data->vert_coords_prev;
  const bool use_envelope = data->use_envelope;
  const bool use_quaternion = data->use_quaternion;
  const bool use_dverts = data->use_dverts;
  const int armature_def_nr = data->armature_def_nr;

  const MDeformVert *dvert;
  DualQuat sumdq, *dq = NULL;
  bPoseChannel *pchan;
  float *co, dco[3];
  float sumvec[3], summat[3][3];
  float *vec = NULL, (*smat)[3] = NULL;
  float contrib = 0.0f;
  float armature_weight = 1.0f; /* default to 1 if no overall def group */
  float prevco_weight = 1.0f;   /* weight for optional cached vertexcos */

  if (use_quaternion) {
    memset(&sumdq, 0, sizeof(DualQuat));
    dq = &sumdq;
  }
  else {
    zero_v3(sumvec);
    vec = sumvec;

    if (vert_deform_mats) {
      zero_m3(summat);
      smat = summat;
    }
  }

  if (use_dverts || armature_def_nr != -1) {
    if (data->me_target) {
      BLI_assert(i < data->me_target->totvert);
      if (data->me_target->dvert != NULL) {
        dvert = data->me_target->dvert + i;
      }
      else {
        dvert = NULL;
      }
    }
    else if (data->dverts && i < data->dverts_len) {
      dvert = data->dverts + i;
    }
    else {
      dvert = NULL;
    }
  }
  else {
    dvert = NULL;
  }

  if (armature_def_nr != -1 && dvert) {
    armature_weight = BKE_defvert_find_weight(dvert, armature_def_nr);

    if (data->invert_vgroup) {
      armature_weight = 1.0f - armature_weight;
    }

    /* hackish: the blending factor can be used for blending with vert_coords_prev too */
    if (vert_coords_prev) {
      prevco_weight = armature_weight;
      armature_weight = 1.0f;
    }
  }

  /* check if there's any  point in calculating for this vert */
  if (armature_weight == 0.0f) {
    return;
  }

  /* get the coord we work on */
  co = vert_coords_prev ? vert_coords_prev[i] : vert_coords[i];

  /* Apply the object's matrix */
  mul_m4_v3(data->premat, co);

  if (use_dverts && dvert && dvert->totweight) { /* use weight groups ? */
    const MDeformWeight *dw = dvert->dw;
    int deformed = 0;
    unsigned int j;
    for (j = dvert->totweight; j != 0; j--, dw++) {
      const uint index = dw->def_nr;
      if (index < data->defbase_len && (pchan = data->pchan_from_defbase[index])) {
        float weight = dw->weight;
        Bone *bone = pchan->bone;

        deformed = 1;

        if (bone && bone->flag & BONE_MULT_VG_ENV) {
          weight *= distfactor_to_bone(
              co, bone->arm_head, bone->arm_tail, bone->rad_head, bone->rad_tail, bone->dist);
        }

        pchan_bone_deform(pchan, weight, vec, dq, smat, co, &contrib);
      }
    }
    /* if there are vertexgroups but not groups with bones
     * (like for softbody groups) */
    if (deformed == 0 && use_envelope) {
      for (pchan = data->ob_arm->pose->chanbase.first; pchan; pchan = pchan->next) {
        if (!(pchan->bone->flag & BONE_NO_DEFORM)) {
          contrib += dist_bone_deform(pchan, vec, dq, smat, co);
        }
      }
    }
  }
  else if (use_envelope) {
    for (pchan = data->ob_arm->pose->chanbase.first; pchan; pchan = pchan->next) {
      if (!(pchan->bone->flag & BONE_NO_DEFORM)) {
        contrib += dist_bone_deform(pchan, vec, dq, smat, co);
      }
    }
  }

  /* actually should be EPSILON? weight values and contrib can be like 10e-39 small */
  if (contrib > 0.0001f) {
    if (use_quaternion) {
      normalize_dq(dq, contrib);

      if (armature_weight != 1.0f) {
        copy_v3_v3(dco, co);
        mul_v3m3_dq(dco, (vert_deform_mats) ? summat : NULL, dq);
        sub_v3_v3(dco, co);
        mul_v3_fl(dco, armature_weight);
        add_v3_v3(co, dco);
      }
      else {
        mul_v3m3_dq(co, (vert_deform_mats) ? summat : NULL, dq);
      }

      smat = summat;
    }
    else {
      mul_v3_fl(vec, armature_weight / contrib);
      add_v3_v3v3(co, vec, co);
    }

    if (vert_deform_mats) {
      float pre[3][3], post[3][3], tmpmat[3][3];

      copy_m3_m4(pre, data->premat);
      copy_m3_m4(post, data->postmat);
      copy_m3_m3(tmpmat, vert_deform_mats[i]);

      if (!use_quaternion) { /* quaternion already is scale corrected */
        mul_m3_fl(smat, armature_weight / contrib);
      }

      mul_m3_series(vert_deform_mats[i], post, smat, pre, tmpmat);
    }
  }

  /* always, check above code */
  mul_m4_v3(data->postmat, co);

  /* interpolate with previous modifier position using weight group */
  if (vert_coords_prev) {
    float mw = 1.0f - prevco_weight;
    vert_coords[i][0] = prevco_weight * vert_coords[i][0] + mw * co[0];
    vert_coords[i][1] = prevco_weight * vert_coords[i][1] + mw * co[1];
    vert_coords[i][2] = prevco_weight * vert_coords[i][2] + mw * co[2];
  }
}

static void armature_deform_coords_impl(Object *ob_arm,
                                        Object *ob_target,
                                        float (*vert_coords)[3],
                                        float (*vert_deform_mats)[3][3],
                                        const int vert_coords_len,
                                        const int deformflag,
                                        float (*vert_coords_prev)[3],
                                        const char *defgrp_name,
                                        const Mesh *me_target,
                                        bGPDstroke *gps_target)
{
  bArmature *arm = ob_arm->data;
  bPoseChannel **pchan_from_defbase = NULL;
  const MDeformVert *dverts = NULL;
  bDeformGroup *dg;
  const bool use_envelope = (deformflag & ARM_DEF_ENVELOPE) != 0;
  const bool use_quaternion = (deformflag & ARM_DEF_QUATERNION) != 0;
  const bool invert_vgroup = (deformflag & ARM_DEF_INVERT_VGROUP) != 0;
  int defbase_len = 0;   /* safety for vertexgroup index overflow */
  int i, dverts_len = 0; /* safety for vertexgroup overflow */
  bool use_dverts = false;
  int armature_def_nr;

  /* in editmode, or not an armature */
  if (arm->edbo || (ob_arm->pose == NULL)) {
    return;
  }

  if ((ob_arm->pose->flag & POSE_RECALC) != 0) {
    CLOG_ERROR(&LOG,
               "Trying to evaluate influence of armature '%s' which needs Pose recalc!",
               ob_arm->id.name);
    BLI_assert(0);
  }

  /* get the def_nr for the overall armature vertex group if present */
  armature_def_nr = BKE_object_defgroup_name_index(ob_target, defgrp_name);

  if (ELEM(ob_target->type, OB_MESH, OB_LATTICE, OB_GPENCIL)) {
    defbase_len = BLI_listbase_count(&ob_target->defbase);

    if (ob_target->type == OB_MESH) {
      Mesh *me = ob_target->data;
      dverts = me->dvert;
      if (dverts) {
        dverts_len = me->totvert;
      }
    }
    else if (ob_target->type == OB_LATTICE) {
      Lattice *lt = ob_target->data;
      dverts = lt->dvert;
      if (dverts) {
        dverts_len = lt->pntsu * lt->pntsv * lt->pntsw;
      }
    }
    else if (ob_target->type == OB_GPENCIL) {
      dverts = gps_target->dvert;
      if (dverts) {
        dverts_len = gps_target->totpoints;
      }
    }
  }

  /* get a vertex-deform-index to posechannel array */
  if (deformflag & ARM_DEF_VGROUP) {
    if (ELEM(ob_target->type, OB_MESH, OB_LATTICE, OB_GPENCIL)) {
      /* if we have a Mesh, only use dverts if it has them */
      if (me_target) {
        use_dverts = (me_target->dvert != NULL);
      }
      else if (dverts) {
        use_dverts = true;
      }

      if (use_dverts) {
        pchan_from_defbase = MEM_callocN(sizeof(*pchan_from_defbase) * defbase_len, "defnrToBone");
        /* TODO(sergey): Some considerations here:
         *
         * - Check whether keeping this consistent across frames gives speedup.
         */
        for (i = 0, dg = ob_target->defbase.first; dg; i++, dg = dg->next) {
          pchan_from_defbase[i] = BKE_pose_channel_find_name(ob_arm->pose, dg->name);
          /* exclude non-deforming bones */
          if (pchan_from_defbase[i]) {
            if (pchan_from_defbase[i]->bone->flag & BONE_NO_DEFORM) {
              pchan_from_defbase[i] = NULL;
            }
          }
        }
      }
    }
  }

  ArmatureUserdata data = {
      .ob_arm = ob_arm,
      .ob_target = ob_target,
      .me_target = me_target,
      .vert_coords = vert_coords,
      .vert_deform_mats = vert_deform_mats,
      .vert_coords_prev = vert_coords_prev,
      .use_envelope = use_envelope,
      .use_quaternion = use_quaternion,
      .invert_vgroup = invert_vgroup,
      .use_dverts = use_dverts,
      .armature_def_nr = armature_def_nr,
      .dverts = dverts,
      .dverts_len = dverts_len,
      .pchan_from_defbase = pchan_from_defbase,
      .defbase_len = defbase_len,
  };

  float obinv[4][4];
  invert_m4_m4(obinv, ob_target->obmat);

  mul_m4_m4m4(data.postmat, obinv, ob_arm->obmat);
  invert_m4_m4(data.premat, data.postmat);

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.min_iter_per_thread = 32;
  BLI_task_parallel_range(0, vert_coords_len, &data, armature_vert_task, &settings);

  if (pchan_from_defbase) {
    MEM_freeN(pchan_from_defbase);
  }
}

void BKE_armature_deform_coords_with_gpencil_stroke(Object *ob_arm,
                                                    Object *ob_target,
                                                    float (*vert_coords)[3],
                                                    float (*vert_deform_mats)[3][3],
                                                    int vert_coords_len,
                                                    int deformflag,
                                                    float (*vert_coords_prev)[3],
                                                    const char *defgrp_name,
                                                    bGPDstroke *gps_target)
{
  armature_deform_coords_impl(ob_arm,
                              ob_target,
                              vert_coords,
                              vert_deform_mats,
                              vert_coords_len,
                              deformflag,
                              vert_coords_prev,
                              defgrp_name,
                              NULL,
                              gps_target);
}

void BKE_armature_deform_coords_with_mesh(Object *ob_arm,
                                          Object *ob_target,
                                          float (*vert_coords)[3],
                                          float (*vert_deform_mats)[3][3],
                                          int vert_coords_len,
                                          int deformflag,
                                          float (*vert_coords_prev)[3],
                                          const char *defgrp_name,
                                          const Mesh *me_target)
{
  armature_deform_coords_impl(ob_arm,
                              ob_target,
                              vert_coords,
                              vert_deform_mats,
                              vert_coords_len,
                              deformflag,
                              vert_coords_prev,
                              defgrp_name,
                              me_target,
                              NULL);
}

/** \} */
