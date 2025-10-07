/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

/* Allow using deprecated functionality for .blend file I/O. */
#define DNA_DEPRECATED_ALLOW

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_kdopbvh.hh"
#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"
#include "BLI_utildefines.h"
#include "BLT_translation.hh"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_cachefile_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "DNA_lattice_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_scene_types.h"
#include "DNA_tracking_types.h"

#include "BKE_action.hh"
#include "BKE_anim_path.h"
#include "BKE_animsys.h"
#include "BKE_armature.hh"
#include "BKE_bvhutils.hh"
#include "BKE_cachefile.hh"
#include "BKE_camera.h"
#include "BKE_constraint.h"
#include "BKE_curve.hh"
#include "BKE_deform.hh"
#include "BKE_displist.h"
#include "BKE_editmesh.hh"
#include "BKE_fcurve_driver.h"
#include "BKE_geometry_set_instances.hh"
#include "BKE_global.hh"
#include "BKE_idprop.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_library.hh"
#include "BKE_mesh_runtime.hh"
#include "BKE_movieclip.h"
#include "BKE_object.hh"
#include "BKE_object_types.hh"
#include "BKE_scene.hh"
#include "BKE_shrinkwrap.hh"
#include "BKE_tracking.h"

#include "BIK_api.h"

#include "RNA_prototypes.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "BLO_read_write.hh"

#include "CLG_log.h"

#ifdef WITH_PYTHON
#  include "BPY_extern.hh"
#endif

#ifdef WITH_ALEMBIC
#  include "ABC_alembic.h"
#endif

#ifdef WITH_USD
#  include "usd.hh"
#endif

/* ---------------------------------------------------------------------------- */
/* Useful macros for testing various common flag combinations */

/* Constraint Target Macros */
#define VALID_CONS_TARGET(ct) ((ct) && (ct->tar))

static CLG_LogRef LOG = {"object.constraint"};

/* ************************ Constraints - General Utilities *************************** */
/* These functions here don't act on any specific constraints, and are therefore should/will
 * not require any of the special function-pointers afforded by the relevant constraint
 * type-info structs.
 */

static void damptrack_do_transform(float matrix[4][4], const float tarvec[3], int track_axis);

static bConstraint *constraint_find_original(Object *ob,
                                             bPoseChannel *pchan,
                                             bConstraint *con,
                                             Object **r_orig_ob);
static bConstraint *constraint_find_original_for_update(bConstraintOb *cob, bConstraint *con);

/* -------------- Naming -------------- */

void BKE_constraint_unique_name(bConstraint *con, ListBase *list)
{
  BLI_uniquename(list, con, DATA_("Const"), '.', offsetof(bConstraint, name), sizeof(con->name));
}

/* ----------------- Evaluation Loop Preparation --------------- */

bConstraintOb *BKE_constraints_make_evalob(
    Depsgraph *depsgraph, Scene *scene, Object *ob, void *subdata, short datatype)
{
  bConstraintOb *cob;

  /* create regardless of whether we have any data! */
  cob = MEM_callocN<bConstraintOb>("bConstraintOb");

  /* NOTE(@ton): For system time, part of de-globalization, code nicer later with local time. */
  cob->scene = scene;
  cob->depsgraph = depsgraph;

  /* based on type of available data */
  switch (datatype) {
    case CONSTRAINT_OBTYPE_OBJECT: {
      /* disregard subdata... calloc should set other values right */
      if (ob) {
        cob->ob = ob;
        cob->type = datatype;

        if (cob->ob->rotmode > 0) {
          /* Should be some kind of Euler order, so use it */
          /* NOTE: Versions <= 2.76 assumed that "default" order
           *       would always get used, so we may seem some rig
           *       breakage as a result. However, this change here
           *       is needed to fix #46599
           */
          cob->rotOrder = ob->rotmode;
        }
        else {
          /* Quaternion/Axis-Angle, so Eulers should just use default order. */
          cob->rotOrder = EULER_ORDER_DEFAULT;
        }
        copy_m4_m4(cob->matrix, ob->object_to_world().ptr());
      }
      else {
        unit_m4(cob->matrix);
      }

      copy_m4_m4(cob->startmat, cob->matrix);
      break;
    }
    case CONSTRAINT_OBTYPE_BONE: {
      /* only set if we have valid bone, otherwise default */
      if (ob && subdata) {
        cob->ob = ob;
        cob->pchan = (bPoseChannel *)subdata;
        cob->type = datatype;

        if (cob->pchan->rotmode > 0) {
          /* should be some type of Euler order */
          cob->rotOrder = cob->pchan->rotmode;
        }
        else {
          /* Quaternion, so eulers should just use default order */
          cob->rotOrder = EULER_ORDER_DEFAULT;
        }

        /* matrix in world-space */
        mul_m4_m4m4(cob->matrix, ob->object_to_world().ptr(), cob->pchan->pose_mat);
      }
      else {
        unit_m4(cob->matrix);
      }

      copy_m4_m4(cob->startmat, cob->matrix);
      break;
    }
    default: /* other types not yet handled */
      unit_m4(cob->matrix);
      unit_m4(cob->startmat);
      break;
  }

  return cob;
}

void BKE_constraints_clear_evalob(bConstraintOb *cob)
{
  float delta[4][4], imat[4][4];

  /* prevent crashes */
  if (cob == nullptr) {
    return;
  }

  /* calculate delta of constraints evaluation */
  invert_m4_m4(imat, cob->startmat);
  /* XXX This would seem to be in wrong order. However, it does not work in 'right' order - would
   *     be nice to understand why pre-multiply is needed here instead of usual post-multiply?
   *     In any case, we *do not get a delta* here (e.g. startmat & matrix having same location,
   *     still gives a 'delta' with non-null translation component :/ ). */
  mul_m4_m4m4(delta, cob->matrix, imat);

  /* copy matrices back to source */
  switch (cob->type) {
    case CONSTRAINT_OBTYPE_OBJECT: {
      /* cob->ob might not exist! */
      if (cob->ob) {
        /* copy new ob-matrix back to owner */
        copy_m4_m4(cob->ob->runtime->object_to_world.ptr(), cob->matrix);

        /* copy inverse of delta back to owner */
        invert_m4_m4(cob->ob->constinv, delta);
      }
      break;
    }
    case CONSTRAINT_OBTYPE_BONE: {
      /* cob->ob or cob->pchan might not exist */
      if (cob->ob && cob->pchan) {
        /* copy new pose-matrix back to owner */
        mul_m4_m4m4(cob->pchan->pose_mat, cob->ob->world_to_object().ptr(), cob->matrix);

        /* copy inverse of delta back to owner */
        invert_m4_m4(cob->pchan->constinv, delta);
      }
      break;
    }
  }

  /* Free temporary struct. */
  MEM_freeN(cob);
}

/* -------------- Space-Conversion API -------------- */

void BKE_constraint_mat_convertspace(Object *ob,
                                     bPoseChannel *pchan,
                                     bConstraintOb *cob,
                                     float mat[4][4],
                                     short from,
                                     short to,
                                     const bool keep_scale)
{
  float diff_mat[4][4];
  float imat[4][4];

  /* Prevent crashes in these unlikely events. */
  if (ob == nullptr || mat == nullptr) {
    return;
  }
  /* optimize trick - check if need to do anything */
  if (from == to) {
    return;
  }

  /* are we dealing with pose-channels or objects */
  if (pchan) {
    /* pose channels */
    switch (from) {
      case CONSTRAINT_SPACE_WORLD: /* ---------- FROM WORLDSPACE ---------- */
      {
        if (to == CONSTRAINT_SPACE_CUSTOM) {
          /* World to custom. */
          BLI_assert(cob);
          invert_m4_m4(imat, cob->space_obj_world_matrix);
          mul_m4_m4m4(mat, imat, mat);
        }
        else {
          /* World to pose. */
          invert_m4_m4(imat, ob->object_to_world().ptr());
          mul_m4_m4m4(mat, imat, mat);

          /* Use pose-space as stepping stone for other spaces. */
          if (ELEM(to,
                   CONSTRAINT_SPACE_LOCAL,
                   CONSTRAINT_SPACE_PARLOCAL,
                   CONSTRAINT_SPACE_OWNLOCAL))
          {
            /* Call self with slightly different values. */
            BKE_constraint_mat_convertspace(
                ob, pchan, cob, mat, CONSTRAINT_SPACE_POSE, to, keep_scale);
          }
        }
        break;
      }
      case CONSTRAINT_SPACE_POSE: /* ---------- FROM POSESPACE ---------- */
      {
        /* pose to local */
        if (to == CONSTRAINT_SPACE_LOCAL) {
          if (pchan->bone) {
            BKE_armature_mat_pose_to_bone(pchan, mat, mat);
          }
        }
        /* pose to owner local */
        else if (to == CONSTRAINT_SPACE_OWNLOCAL) {
          /* pose to local */
          if (pchan->bone) {
            BKE_armature_mat_pose_to_bone(pchan, mat, mat);
          }

          /* local to owner local (recursive) */
          BKE_constraint_mat_convertspace(
              ob, pchan, cob, mat, CONSTRAINT_SPACE_LOCAL, to, keep_scale);
        }
        /* pose to local with parent */
        else if (to == CONSTRAINT_SPACE_PARLOCAL) {
          if (pchan->bone) {
            invert_m4_m4(imat, pchan->bone->arm_mat);
            mul_m4_m4m4(mat, imat, mat);
          }
        }
        else {
          /* Pose to world. */
          mul_m4_m4m4(mat, ob->object_to_world().ptr(), mat);
          /* Use world-space as stepping stone for other spaces. */
          if (to != CONSTRAINT_SPACE_WORLD) {
            /* Call self with slightly different values. */
            BKE_constraint_mat_convertspace(
                ob, pchan, cob, mat, CONSTRAINT_SPACE_WORLD, to, keep_scale);
          }
        }
        break;
      }
      case CONSTRAINT_SPACE_LOCAL: /* ------------ FROM LOCALSPACE --------- */
      {
        /* local to owner local */
        if (to == CONSTRAINT_SPACE_OWNLOCAL) {
          if (pchan->bone) {
            copy_m4_m4(diff_mat, pchan->bone->arm_mat);

            if (cob && cob->pchan && cob->pchan->bone) {
              invert_m4_m4(imat, cob->pchan->bone->arm_mat);
              mul_m4_m4m4(diff_mat, imat, diff_mat);
            }

            zero_v3(diff_mat[3]);
            invert_m4_m4(imat, diff_mat);
            mul_m4_series(mat, diff_mat, mat, imat);
          }
        }
        /* local to pose - do inverse procedure that was done for pose to local */
        else {
          if (pchan->bone) {
            /* We need the:
             *  `posespace_matrix = local_matrix + (parent_posespace_matrix + restpos)`. */
            BKE_armature_mat_bone_to_pose(pchan, mat, mat);
          }

          /* use pose-space as stepping stone for other spaces */
          if (ELEM(to, CONSTRAINT_SPACE_WORLD, CONSTRAINT_SPACE_PARLOCAL, CONSTRAINT_SPACE_CUSTOM))
          {
            /* call self with slightly different values */
            BKE_constraint_mat_convertspace(
                ob, pchan, cob, mat, CONSTRAINT_SPACE_POSE, to, keep_scale);
          }
        }
        break;
      }
      case CONSTRAINT_SPACE_OWNLOCAL: { /* -------------- FROM OWNER LOCAL ---------- */
        /* owner local to local */
        if (pchan->bone) {
          copy_m4_m4(diff_mat, pchan->bone->arm_mat);

          if (cob && cob->pchan && cob->pchan->bone) {
            invert_m4_m4(imat, cob->pchan->bone->arm_mat);
            mul_m4_m4m4(diff_mat, imat, diff_mat);
          }

          zero_v3(diff_mat[3]);
          invert_m4_m4(imat, diff_mat);
          mul_m4_series(mat, imat, mat, diff_mat);
        }

        if (to != CONSTRAINT_SPACE_LOCAL) {
          /* call self with slightly different values */
          BKE_constraint_mat_convertspace(
              ob, pchan, cob, mat, CONSTRAINT_SPACE_LOCAL, to, keep_scale);
        }
        break;
      }
      case CONSTRAINT_SPACE_PARLOCAL: /* -------------- FROM LOCAL WITH PARENT ---------- */
      {
        /* local + parent to pose */
        if (pchan->bone) {
          mul_m4_m4m4(mat, pchan->bone->arm_mat, mat);
        }

        /* use pose-space as stepping stone for other spaces */
        if (ELEM(to,
                 CONSTRAINT_SPACE_WORLD,
                 CONSTRAINT_SPACE_LOCAL,
                 CONSTRAINT_SPACE_OWNLOCAL,
                 CONSTRAINT_SPACE_CUSTOM))
        {
          /* call self with slightly different values */
          BKE_constraint_mat_convertspace(
              ob, pchan, cob, mat, CONSTRAINT_SPACE_POSE, to, keep_scale);
        }
        break;
      }
      case CONSTRAINT_SPACE_CUSTOM: /* -------------- FROM CUSTOM SPACE ---------- */
      {
        /* Custom to world. */
        BLI_assert(cob);
        mul_m4_m4m4(mat, cob->space_obj_world_matrix, mat);

        /* Use world-space as stepping stone for other spaces. */
        if (to != CONSTRAINT_SPACE_WORLD) {
          /* Call self with slightly different values. */
          BKE_constraint_mat_convertspace(
              ob, pchan, cob, mat, CONSTRAINT_SPACE_WORLD, to, keep_scale);
        }
        break;
      }
    }
  }
  else {
    /* objects */
    if (from == CONSTRAINT_SPACE_WORLD) {
      if (to == CONSTRAINT_SPACE_LOCAL) {
        /* Check if object has a parent. */
        if (ob->parent) {
          /* 'subtract' parent's effects from owner. */
          mul_m4_m4m4(diff_mat, ob->parent->object_to_world().ptr(), ob->parentinv);
          invert_m4_m4_safe(imat, diff_mat);
          mul_m4_m4m4(mat, imat, mat);
        }
        else {
          /* Local space in this case will have to be defined as local to the owner's
           * transform-property-rotated axes. So subtract this rotation component.
           */
          /* XXX This is actually an ugly hack, local space of a parent-less object *is* the same
           * as global space! Think what we want actually here is some kind of 'Final Space', i.e
           *     . once transformations are applied - users are often confused about this too,
           *     this is not consistent with bones
           *     local space either... Meh :|
           *     --mont29
           */
          BKE_object_to_mat4(ob, diff_mat);
          if (!keep_scale) {
            normalize_m4(diff_mat);
          }
          zero_v3(diff_mat[3]);

          invert_m4_m4_safe(imat, diff_mat);
          mul_m4_m4m4(mat, imat, mat);
        }
      }
      else if (to == CONSTRAINT_SPACE_CUSTOM) {
        /* 'subtract' custom objects's effects from owner. */
        BLI_assert(cob);
        invert_m4_m4_safe(imat, cob->space_obj_world_matrix);
        mul_m4_m4m4(mat, imat, mat);
      }
    }
    else if (from == CONSTRAINT_SPACE_LOCAL) {
      /* check that object has a parent - otherwise this won't work */
      if (ob->parent) {
        /* 'add' parent's effect back to owner */
        mul_m4_m4m4(diff_mat, ob->parent->object_to_world().ptr(), ob->parentinv);
        mul_m4_m4m4(mat, diff_mat, mat);
      }
      else {
        /* Local space in this case will have to be defined as local to the owner's
         * transform-property-rotated axes. So add back this rotation component.
         */
        /* XXX See comment above for world->local case... */
        BKE_object_to_mat4(ob, diff_mat);
        if (!keep_scale) {
          normalize_m4(diff_mat);
        }
        zero_v3(diff_mat[3]);

        mul_m4_m4m4(mat, diff_mat, mat);
      }
      if (to == CONSTRAINT_SPACE_CUSTOM) {
        /* 'subtract' objects's effects from owner. */
        BLI_assert(cob);
        invert_m4_m4_safe(imat, cob->space_obj_world_matrix);
        mul_m4_m4m4(mat, imat, mat);
      }
    }
    else if (from == CONSTRAINT_SPACE_CUSTOM) {
      /* Custom to world. */
      BLI_assert(cob);
      mul_m4_m4m4(mat, cob->space_obj_world_matrix, mat);

      /* Use world-space as stepping stone for other spaces. */
      if (to != CONSTRAINT_SPACE_WORLD) {
        /* Call self with slightly different values. */
        BKE_constraint_mat_convertspace(
            ob, pchan, cob, mat, CONSTRAINT_SPACE_WORLD, to, keep_scale);
      }
    }
  }
}

/* ------------ General Target Matrix Tools ---------- */

/* function that sets the given matrix based on given vertex group in mesh */
static void contarget_get_mesh_mat(Object *ob, const char *substring, float mat[4][4])
{
  /* when not in EditMode, use the 'final' evaluated mesh, depsgraph
   * ensures we build with CD_MDEFORMVERT layer
   */
  const Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob);
  BMEditMesh *em = BKE_editmesh_from_object(ob);
  float plane[3];
  float imat[3][3], tmat[3][3];
  const int defgroup = BKE_object_defgroup_name_index(ob, substring);

  /* initialize target matrix using target matrix */
  copy_m4_m4(mat, ob->object_to_world().ptr());

  /* get index of vertex group */
  if (defgroup == -1) {
    return;
  }

  float vec[3] = {0.0f, 0.0f, 0.0f};
  float normal[3] = {0.0f, 0.0f, 0.0f};
  float weightsum = 0.0f;
  if (em) {
    if (CustomData_has_layer(&em->bm->vdata, CD_MDEFORMVERT)) {
      BMVert *v;
      BMIter iter;

      BM_ITER_MESH (v, &iter, em->bm, BM_VERTS_OF_MESH) {
        MDeformVert *dv = static_cast<MDeformVert *>(
            CustomData_bmesh_get(&em->bm->vdata, v->head.data, CD_MDEFORMVERT));
        MDeformWeight *dw = BKE_defvert_find_index(dv, defgroup);

        if (dw && dw->weight > 0.0f) {
          madd_v3_v3fl(vec, v->co, dw->weight);
          madd_v3_v3fl(normal, v->no, dw->weight);
          weightsum += dw->weight;
        }
      }
    }
  }
  else if (mesh_eval) {
    const blender::Span<blender::float3> positions = mesh_eval->vert_positions();
    const blender::Span<blender::float3> vert_normals = mesh_eval->vert_normals();
    const blender::Span<MDeformVert> dverts = mesh_eval->deform_verts();
    /* check that dvert is a valid pointers (just in case) */
    if (!dverts.is_empty()) {
      /* get the average of all verts with that are in the vertex-group */
      for (const int i : positions.index_range()) {
        const MDeformVert *dv = &dverts[i];
        const MDeformWeight *dw = BKE_defvert_find_index(dv, defgroup);

        if (dw && dw->weight > 0.0f) {
          madd_v3_v3fl(vec, positions[i], dw->weight);
          madd_v3_v3fl(normal, vert_normals[i], dw->weight);
          weightsum += dw->weight;
        }
      }
    }
  }
  else {
    /* No valid edit or evaluated mesh, just abort. */
    return;
  }

  /* calculate averages of normal and coordinates */
  if (weightsum > 0) {
    mul_v3_fl(vec, 1.0f / weightsum);
    mul_v3_fl(normal, 1.0f / weightsum);
  }

  /* derive the rotation from the average normal:
   * - code taken from transform_gizmo.c,
   *   calc_gizmo_stats, V3D_ORIENT_NORMAL case */

  /* We need the transpose of the inverse for a normal. */
  copy_m3_m4(imat, ob->object_to_world().ptr());

  invert_m3_m3(tmat, imat);
  transpose_m3(tmat);
  mul_m3_v3(tmat, normal);

  normalize_v3(normal);
  copy_v3_v3(plane, tmat[1]);

  cross_v3_v3v3(mat[0], normal, plane);
  if (len_squared_v3(mat[0]) < square_f(1e-3f)) {
    copy_v3_v3(plane, tmat[0]);
    cross_v3_v3v3(mat[0], normal, plane);
  }

  copy_v3_v3(mat[2], normal);
  cross_v3_v3v3(mat[1], mat[2], mat[0]);

  normalize_m4(mat);

  /* apply the average coordinate as the new location */
  mul_v3_m4v3(mat[3], ob->object_to_world().ptr(), vec);
}

/* function that sets the given matrix based on given vertex group in lattice */
static void contarget_get_lattice_mat(Object *ob, const char *substring, float mat[4][4])
{
  Lattice *lt = (Lattice *)ob->data;

  DispList *dl = ob->runtime->curve_cache ?
                     BKE_displist_find(&ob->runtime->curve_cache->disp, DL_VERTS) :
                     nullptr;
  const float *co = dl ? dl->verts : nullptr;
  BPoint *bp = lt->def;

  MDeformVert *dv = lt->dvert;
  int tot_verts = lt->pntsu * lt->pntsv * lt->pntsw;
  float vec[3] = {0.0f, 0.0f, 0.0f}, tvec[3];
  int grouped = 0;
  int i, n;
  const int defgroup = BKE_object_defgroup_name_index(ob, substring);

  /* initialize target matrix using target matrix */
  copy_m4_m4(mat, ob->object_to_world().ptr());

  /* get index of vertex group */
  if (defgroup == -1) {
    return;
  }
  if (dv == nullptr) {
    return;
  }

  /* 1. Loop through control-points checking if in nominated vertex-group.
   * 2. If it is, add it to vec to find the average point.
   */
  for (i = 0; i < tot_verts; i++, dv++) {
    for (n = 0; n < dv->totweight; n++) {
      MDeformWeight *dw = BKE_defvert_find_index(dv, defgroup);
      if (dw && dw->weight > 0.0f) {
        /* copy coordinates of point to temporary vector, then add to find average */
        memcpy(tvec, co ? co : bp->vec, sizeof(float[3]));

        add_v3_v3(vec, tvec);
        grouped++;
      }
    }

    /* advance pointer to coordinate data */
    if (co) {
      co += 3;
    }
    else {
      bp++;
    }
  }

  /* find average location, then multiply by ob->object_to_world().ptr() to find world-space
   * location */
  if (grouped) {
    mul_v3_fl(vec, 1.0f / grouped);
  }
  mul_v3_m4v3(tvec, ob->object_to_world().ptr(), vec);

  /* copy new location to matrix */
  copy_v3_v3(mat[3], tvec);
}

/* generic function to get the appropriate matrix for most target cases */
/* The cases where the target can be object data have not been implemented */
static void constraint_target_to_mat4(Object *ob,
                                      const char *substring,
                                      bConstraintOb *cob,
                                      float mat[4][4],
                                      short from,
                                      short to,
                                      short flag,
                                      float headtail)
{
  /* Case OBJECT */
  if (substring[0] == '\0') {
    copy_m4_m4(mat, ob->object_to_world().ptr());
    BKE_constraint_mat_convertspace(ob, nullptr, cob, mat, from, to, false);
  }
  /* Case VERTEXGROUP */
  /* Current method just takes the average location of all the points in the
   * VertexGroup, and uses that as the location value of the targets. Where
   * possible, the orientation will also be calculated, by calculating an
   * 'average' vertex normal, and deriving the rotation from that.
   *
   * NOTE: EditMode is not currently supported, and will most likely remain that
   *       way as constraints can only really affect things on object/bone level.
   */
  else if (ob->type == OB_MESH) {
    contarget_get_mesh_mat(ob, substring, mat);
    BKE_constraint_mat_convertspace(ob, nullptr, cob, mat, from, to, false);
  }
  else if (ob->type == OB_LATTICE) {
    contarget_get_lattice_mat(ob, substring, mat);
    BKE_constraint_mat_convertspace(ob, nullptr, cob, mat, from, to, false);
  }
  /* Case BONE */
  else {
    bPoseChannel *pchan;

    pchan = BKE_pose_channel_find_name(ob->pose, substring);
    if (pchan) {
      /* Multiply the PoseSpace accumulation/final matrix for this
       * PoseChannel by the Armature Object's Matrix to get a world-space matrix.
       */
      bool is_bbone = (pchan->bone) && (pchan->bone->segments > 1) &&
                      (flag & CONSTRAINT_BBONE_SHAPE);
      bool full_bbone = (flag & CONSTRAINT_BBONE_SHAPE_FULL) != 0;

      if (headtail < 0.000001f && !(is_bbone && full_bbone)) {
        /* skip length interpolation if set to head */
        mul_m4_m4m4(mat, ob->object_to_world().ptr(), pchan->pose_mat);
      }
      else if (is_bbone && pchan->bone->segments == pchan->runtime.bbone_segments) {
        /* use point along bbone */
        Mat4 *bbone = pchan->runtime.bbone_pose_mats;
        float tempmat[4][4];
        float loc[3], fac;
        int index;

        /* figure out which segment(s) the headtail value falls in */
        BKE_pchan_bbone_deform_clamp_segment_index(pchan, headtail, &index, &fac);

        /* apply full transformation of the segment if requested */
        if (full_bbone) {
          interp_m4_m4m4(tempmat, bbone[index].mat, bbone[index + 1].mat, fac);

          mul_m4_m4m4(tempmat, pchan->pose_mat, tempmat);
        }
        /* only interpolate location */
        else {
          interp_v3_v3v3(loc, bbone[index].mat[3], bbone[index + 1].mat[3], fac);

          copy_m4_m4(tempmat, pchan->pose_mat);
          mul_v3_m4v3(tempmat[3], pchan->pose_mat, loc);
        }

        mul_m4_m4m4(mat, ob->object_to_world().ptr(), tempmat);
      }
      else {
        float tempmat[4][4], loc[3];

        /* interpolate along length of bone */
        interp_v3_v3v3(loc, pchan->pose_head, pchan->pose_tail, headtail);

        /* use interpolated distance for subtarget */
        copy_m4_m4(tempmat, pchan->pose_mat);
        copy_v3_v3(tempmat[3], loc);

        mul_m4_m4m4(mat, ob->object_to_world().ptr(), tempmat);
      }
    }
    else {
      copy_m4_m4(mat, ob->object_to_world().ptr());
    }

    /* convert matrix space as required */
    BKE_constraint_mat_convertspace(ob, pchan, cob, mat, from, to, false);
  }
}

/* ************************* Specific Constraints ***************************** */
/* Each constraint defines a set of functions, which will be called at the appropriate
 * times. In addition to this, each constraint should have a type-info struct, where
 * its functions are attached for use.
 */

/* Template for type-info data:
 * - make a copy of this when creating new constraints, and just change the functions
 *   pointed to as necessary
 * - although the naming of functions doesn't matter, it would help for code
 *   readability, to follow the same naming convention as is presented here
 * - any functions that a constraint doesn't need to define, don't define
 *   for such cases, just use nullptr
 * - these should be defined after all the functions have been defined, so that
 *   forward-definitions/prototypes don't need to be used!
 * - keep this copy #if-def'd so that future constraints can get based off this
 */
#if 0
static bConstraintTypeInfo CTI_CONSTRNAME = {
    /*type*/ CONSTRAINT_TYPE_CONSTRNAME,
    /*size*/ sizeof(bConstrNameConstraint),
    /*name*/ "ConstrName",
    /*struct_name*/ "bConstrNameConstraint",
    /*free_data*/ constrname_free,
    /*id_looper*/ constrname_id_looper,
    /*copy_data*/ constrname_copy,
    /*new_data*/ constrname_new_data,
    /*get_constraint_targets*/ constrname_get_tars,
    /*flush_constraint_targets*/ constrname_flush_tars,
    /*get_target_matrix*/ constrname_get_tarmat,
    /*evaluate_constraint*/ constrname_evaluate,
};
#endif

static inline void unit_ct_matrix_nullsafe(bConstraintTarget *ct)
{
  if (ct) {
    unit_m4(ct->matrix);
  }
}

/* This function should be used for the get_target_matrix member of all
 * constraints that are not picky about what happens to their target matrix.
 *
 * \returns whether the constraint has a valid target.
 */
static bool default_get_tarmat(Depsgraph * /*depsgraph*/,
                               bConstraint *con,
                               bConstraintOb *cob,
                               bConstraintTarget *ct,
                               float /*ctime*/)
{
  if (!VALID_CONS_TARGET(ct)) {
    unit_ct_matrix_nullsafe(ct);
    return false;
  }

  constraint_target_to_mat4(ct->tar,
                            ct->subtarget,
                            cob,
                            ct->matrix,
                            CONSTRAINT_SPACE_WORLD,
                            ct->space,
                            con->flag,
                            con->headtail);
  return true;
}

/* This is a variant that extracts full transformation from B-Bone segments.
 */
static bool default_get_tarmat_full_bbone(Depsgraph * /*depsgraph*/,
                                          bConstraint *con,
                                          bConstraintOb *cob,
                                          bConstraintTarget *ct,
                                          float /*ctime*/)
{
  if (!VALID_CONS_TARGET(ct)) {
    unit_ct_matrix_nullsafe(ct);
    return false;
  }

  constraint_target_to_mat4(ct->tar,
                            ct->subtarget,
                            cob,
                            ct->matrix,
                            CONSTRAINT_SPACE_WORLD,
                            ct->space,
                            con->flag | CONSTRAINT_BBONE_SHAPE_FULL,
                            con->headtail);
  return true;
}

/* This following macro should be used for all standard single-target *_get_tars functions
 * to save typing and reduce maintenance woes.
 * (Hopefully all compilers will be happy with the lines with just a space on them.
 * Those are really just to help this code easier to read).
 */
/* TODO: cope with getting rotation order... */
#define SINGLETARGET_GET_TARS(con, datatar, datasubtarget, ct, list) \
  { \
    ct = MEM_callocN<bConstraintTarget>("tempConstraintTarget"); \
\
    ct->tar = datatar; \
    STRNCPY_UTF8(ct->subtarget, datasubtarget); \
    ct->space = con->tarspace; \
    ct->flag = CONSTRAINT_TAR_TEMP; \
\
    if (ct->tar) { \
      if ((ct->tar->type == OB_ARMATURE) && (ct->subtarget[0])) { \
        bPoseChannel *pchan = BKE_pose_channel_find_name(ct->tar->pose, ct->subtarget); \
        ct->type = CONSTRAINT_OBTYPE_BONE; \
        ct->rotOrder = (pchan) ? (pchan->rotmode) : int(EULER_ORDER_DEFAULT); \
      } \
      else if (OB_TYPE_SUPPORT_VGROUP(ct->tar->type) && (ct->subtarget[0])) { \
        ct->type = CONSTRAINT_OBTYPE_VERT; \
        ct->rotOrder = EULER_ORDER_DEFAULT; \
      } \
      else { \
        ct->type = CONSTRAINT_OBTYPE_OBJECT; \
        ct->rotOrder = ct->tar->rotmode; \
      } \
    } \
\
    BLI_addtail(list, ct); \
  } \
  (void)0

/* This following macro should be used for all standard single-target *_get_tars functions
 * to save typing and reduce maintenance woes. It does not do the subtarget related operations
 * (Hopefully all compilers will be happy with the lines with just a space on them. Those are
 * really just to help this code easier to read)
 */
/* TODO: cope with getting rotation order... */
#define SINGLETARGETNS_GET_TARS(con, datatar, ct, list) \
  { \
    ct = MEM_callocN<bConstraintTarget>("tempConstraintTarget"); \
\
    ct->tar = datatar; \
    ct->space = con->tarspace; \
    ct->flag = CONSTRAINT_TAR_TEMP; \
\
    if (ct->tar) { \
      ct->type = CONSTRAINT_OBTYPE_OBJECT; \
    } \
    BLI_addtail(list, ct); \
  } \
  (void)0

/* This following macro should be used for all standard single-target *_flush_tars functions
 * to save typing and reduce maintenance woes.
 * NOTE: the pointer to ct will be changed to point to the next in the list (as it gets removed)
 * (Hopefully all compilers will be happy with the lines with just a space on them. Those are
 *  really just to help this code easier to read)
 */
#define SINGLETARGET_FLUSH_TARS(con, datatar, datasubtarget, ct, list, no_copy) \
  { \
    if (ct) { \
      bConstraintTarget *ctn = ct->next; \
      if (no_copy == 0) { \
        datatar = ct->tar; \
        STRNCPY_UTF8(datasubtarget, ct->subtarget); \
        con->tarspace = char(ct->space); \
      } \
\
      BLI_freelinkN(list, ct); \
      ct = ctn; \
    } \
  } \
  (void)0

/* This following macro should be used for all standard single-target *_flush_tars functions
 * to save typing and reduce maintenance woes. It does not do the subtarget related operations.
 * NOTE: the pointer to ct will be changed to point to the next in the list (as it gets removed)
 * (Hopefully all compilers will be happy with the lines with just a space on them. Those are
 * really just to help this code easier to read)
 */
#define SINGLETARGETNS_FLUSH_TARS(con, datatar, ct, list, no_copy) \
  { \
    if (ct) { \
      bConstraintTarget *ctn = ct->next; \
      if (no_copy == 0) { \
        datatar = ct->tar; \
        con->tarspace = char(ct->space); \
      } \
\
      BLI_freelinkN(list, ct); \
      ct = ctn; \
    } \
  } \
  (void)0

static bool is_custom_space_needed(bConstraint *con)
{
  return con->ownspace == CONSTRAINT_SPACE_CUSTOM || con->tarspace == CONSTRAINT_SPACE_CUSTOM;
}

/* --------- ChildOf Constraint ------------ */

static void childof_new_data(void *cdata)
{
  bChildOfConstraint *data = (bChildOfConstraint *)cdata;

  data->flag = (CHILDOF_LOCX | CHILDOF_LOCY | CHILDOF_LOCZ | CHILDOF_ROTX | CHILDOF_ROTY |
                CHILDOF_ROTZ | CHILDOF_SIZEX | CHILDOF_SIZEY | CHILDOF_SIZEZ |
                CHILDOF_SET_INVERSE);
  unit_m4(data->invmat);
}

static void childof_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
  bChildOfConstraint *data = static_cast<bChildOfConstraint *>(con->data);

  /* target only */
  func(con, (ID **)&data->tar, false, userdata);
}

static int childof_get_tars(bConstraint *con, ListBase *list)
{
  if (con && list) {
    bChildOfConstraint *data = static_cast<bChildOfConstraint *>(con->data);
    bConstraintTarget *ct;

    /* standard target-getting macro for single-target constraints */
    SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list);

    return 1;
  }

  return 0;
}

static void childof_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
  if (con && list) {
    bChildOfConstraint *data = static_cast<bChildOfConstraint *>(con->data);
    bConstraintTarget *ct = static_cast<bConstraintTarget *>(list->first);

    /* the following macro is used for all standard single-target constraints */
    SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, no_copy);
  }
}

static void childof_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
  bChildOfConstraint *data = static_cast<bChildOfConstraint *>(con->data);
  bConstraintTarget *ct = static_cast<bConstraintTarget *>(targets->first);

  /* Only evaluate if there is a target.
   *
   * NOTE: we're setting/unsetting the CONSTRAINT_SPACEONCE flag here because:
   *
   * 1. It's only used by the Child Of constraint anyway.
   * 2. It's only used to affect the steps taken immediately after this function
   *    returns, and this way we ensure it's always set correctly for that.
   *
   * It was previously set in other places which resulted in bugs like #116567.
   * In the future we should ideally move to a different approach entirely. */
  if (!VALID_CONS_TARGET(ct)) {
    con->flag &= ~CONSTRAINT_SPACEONCE;
    return;
  }
  con->flag |= CONSTRAINT_SPACEONCE;

  float parmat[4][4];
  float inverse_matrix[4][4];
  /* Simple matrix parenting. */
  if ((data->flag & CHILDOF_ALL) == CHILDOF_ALL) {
    copy_m4_m4(parmat, ct->matrix);
    copy_m4_m4(inverse_matrix, data->invmat);
  }
  /* Filter the parent matrix by channel. */
  else {
    float loc[3], eul[3], size[3];
    float loco[3], eulo[3], sizeo[3];

    /* extract components of both matrices */
    copy_v3_v3(loc, ct->matrix[3]);
    mat4_to_eulO(eul, ct->rotOrder, ct->matrix);
    mat4_to_size(size, ct->matrix);

    copy_v3_v3(loco, data->invmat[3]);
    mat4_to_eulO(eulo, cob->rotOrder, data->invmat);
    mat4_to_size(sizeo, data->invmat);

    /* Reset the locked channels to their no-op values. */
    if (!(data->flag & CHILDOF_LOCX)) {
      loc[0] = loco[0] = 0.0f;
    }
    if (!(data->flag & CHILDOF_LOCY)) {
      loc[1] = loco[1] = 0.0f;
    }
    if (!(data->flag & CHILDOF_LOCZ)) {
      loc[2] = loco[2] = 0.0f;
    }
    if (!(data->flag & CHILDOF_ROTX)) {
      eul[0] = eulo[0] = 0.0f;
    }
    if (!(data->flag & CHILDOF_ROTY)) {
      eul[1] = eulo[1] = 0.0f;
    }
    if (!(data->flag & CHILDOF_ROTZ)) {
      eul[2] = eulo[2] = 0.0f;
    }
    if (!(data->flag & CHILDOF_SIZEX)) {
      size[0] = sizeo[0] = 1.0f;
    }
    if (!(data->flag & CHILDOF_SIZEY)) {
      size[1] = sizeo[1] = 1.0f;
    }
    if (!(data->flag & CHILDOF_SIZEZ)) {
      size[2] = sizeo[2] = 1.0f;
    }

    /* Construct the new matrices given the disabled channels. */
    loc_eulO_size_to_mat4(parmat, loc, eul, size, ct->rotOrder);
    loc_eulO_size_to_mat4(inverse_matrix, loco, eulo, sizeo, cob->rotOrder);
  }

  /* If requested, compute the inverse matrix from the computed parent matrix. */
  if (data->flag & CHILDOF_SET_INVERSE) {
    invert_m4_m4(data->invmat, parmat);
    if (cob->pchan != nullptr) {
      mul_m4_series(data->invmat, data->invmat, cob->ob->object_to_world().ptr());
    }

    copy_m4_m4(inverse_matrix, data->invmat);

    data->flag &= ~CHILDOF_SET_INVERSE;

    /* Write the computed matrix back to the master copy if in copy-on-eval evaluation. */
    bConstraint *orig_con = constraint_find_original_for_update(cob, con);

    if (orig_con != nullptr) {
      bChildOfConstraint *orig_data = static_cast<bChildOfConstraint *>(orig_con->data);

      copy_m4_m4(orig_data->invmat, data->invmat);
      orig_data->flag &= ~CHILDOF_SET_INVERSE;
    }
  }

  /* Multiply together the target (parent) matrix, parent inverse,
   * and the owner transform matrix to get the effect of this constraint
   * (i.e.  owner is 'parented' to parent). */
  float orig_cob_matrix[4][4];
  copy_m4_m4(orig_cob_matrix, cob->matrix);
  mul_m4_series(cob->matrix, parmat, inverse_matrix, orig_cob_matrix);

  /* Without this, changes to scale and rotation can change location
   * of a parentless bone or a disconnected bone. Even though its set
   * to zero above. */
  if (!(data->flag & CHILDOF_LOCX)) {
    cob->matrix[3][0] = orig_cob_matrix[3][0];
  }
  if (!(data->flag & CHILDOF_LOCY)) {
    cob->matrix[3][1] = orig_cob_matrix[3][1];
  }
  if (!(data->flag & CHILDOF_LOCZ)) {
    cob->matrix[3][2] = orig_cob_matrix[3][2];
  }
}

/* XXX NOTE: con->flag should be CONSTRAINT_SPACEONCE for bone-childof, patched in `readfile.cc`.
 */
static bConstraintTypeInfo CTI_CHILDOF = {
    /*type*/ CONSTRAINT_TYPE_CHILDOF,
    /*size*/ sizeof(bChildOfConstraint),
    /*name*/ N_("Child Of"),
    /*struct_name*/ "bChildOfConstraint",
    /*free_data*/ nullptr,
    /*id_looper*/ childof_id_looper,
    /*copy_data*/ nullptr,
    /*new_data*/ childof_new_data,
    /*get_constraint_targets*/ childof_get_tars,
    /*flush_constraint_targets*/ childof_flush_tars,
    /*get_target_matrix*/ default_get_tarmat,
    /*evaluate_constraint*/ childof_evaluate,
};

/* -------- TrackTo Constraint ------- */

static void trackto_new_data(void *cdata)
{
  bTrackToConstraint *data = (bTrackToConstraint *)cdata;

  data->reserved1 = TRACK_nZ;
  data->reserved2 = UP_Y;
}

static void trackto_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
  bTrackToConstraint *data = static_cast<bTrackToConstraint *>(con->data);

  /* target only */
  func(con, (ID **)&data->tar, false, userdata);
}

static int trackto_get_tars(bConstraint *con, ListBase *list)
{
  if (con && list) {
    bTrackToConstraint *data = static_cast<bTrackToConstraint *>(con->data);
    bConstraintTarget *ct;

    /* standard target-getting macro for single-target constraints */
    SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list);

    return 1;
  }

  return 0;
}

static void trackto_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
  if (con && list) {
    bTrackToConstraint *data = static_cast<bTrackToConstraint *>(con->data);
    bConstraintTarget *ct = static_cast<bConstraintTarget *>(list->first);

    /* the following macro is used for all standard single-target constraints */
    SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, no_copy);
  }
}

static int basis_cross(int n, int m)
{
  switch (n - m) {
    case 1:
    case -2:
      return 1;

    case -1:
    case 2:
      return -1;

    default:
      return 0;
  }
}

static void vectomat(const float vec[3],
                     const float target_up[3],
                     short axis,
                     short upflag,
                     short flags,
                     float m[3][3])
{
  float n[3];
  float u[3]; /* vector specifying the up axis */
  float proj[3];
  float right[3];
  float neg = -1;
  int right_index;

  if (normalize_v3_v3(n, vec) == 0.0f) {
    n[0] = 0.0f;
    n[1] = 0.0f;
    n[2] = 1.0f;
  }
  if (axis > 2) {
    axis -= 3;
  }
  else {
    negate_v3(n);
  }

  /* n specifies the transformation of the track axis */
  if (flags & TARGET_Z_UP) {
    /* target Z axis is the global up axis */
    copy_v3_v3(u, target_up);
  }
  else {
    /* world Z axis is the global up axis */
    u[0] = 0;
    u[1] = 0;
    u[2] = 1;
  }

  /* NOTE: even though 'n' is normalized, don't use 'project_v3_v3v3_normalized' below
   * because precision issues cause a problem in near degenerate states, see: #53455. */

  /* project the up vector onto the plane specified by n */
  project_v3_v3v3(proj, u, n); /* first u onto n... */
  sub_v3_v3v3(proj, u, proj);  /* then onto the plane */
  /* proj specifies the transformation of the up axis */

  if (normalize_v3(proj) == 0.0f) { /* degenerate projection */
    proj[0] = 0.0f;
    proj[1] = 1.0f;
    proj[2] = 0.0f;
  }

  /* Normalized cross product of n and proj specifies transformation of the right axis */
  cross_v3_v3v3(right, proj, n);
  normalize_v3(right);

  if (axis != upflag) {
    right_index = 3 - axis - upflag;
    neg = float(basis_cross(axis, upflag));

    /* account for up direction, track direction */
    m[right_index][0] = neg * right[0];
    m[right_index][1] = neg * right[1];
    m[right_index][2] = neg * right[2];

    copy_v3_v3(m[upflag], proj);

    copy_v3_v3(m[axis], n);
  }
  /* identity matrix - don't do anything if the two axes are the same */
  else {
    unit_m3(m);
  }
}

static void trackto_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
  bTrackToConstraint *data = static_cast<bTrackToConstraint *>(con->data);
  bConstraintTarget *ct = static_cast<bConstraintTarget *>(targets->first);

  if (VALID_CONS_TARGET(ct)) {
    float size[3], vec[3];
    float totmat[3][3];

    /* Get size property, since ob->scale is only the object's own relative size,
     * not its global one. */
    mat4_to_size(size, cob->matrix);

    /* Clear the object's rotation */
    cob->matrix[0][0] = size[0];
    cob->matrix[0][1] = 0;
    cob->matrix[0][2] = 0;
    cob->matrix[1][0] = 0;
    cob->matrix[1][1] = size[1];
    cob->matrix[1][2] = 0;
    cob->matrix[2][0] = 0;
    cob->matrix[2][1] = 0;
    cob->matrix[2][2] = size[2];

    /* NOTE(@joshualung): `targetmat[2]` instead of `ownermat[2]` is passed to #vectomat
     * for backwards compatibility it seems. */
    sub_v3_v3v3(vec, cob->matrix[3], ct->matrix[3]);
    vectomat(vec,
             ct->matrix[2],
             std::clamp<short>(data->reserved1, 0, 5),
             std::clamp<short>(data->reserved2, 0, 2),
             data->flags,
             totmat);

    mul_m4_m3m4(cob->matrix, totmat, cob->matrix);
  }
}

static bConstraintTypeInfo CTI_TRACKTO = {
    /*type*/ CONSTRAINT_TYPE_TRACKTO,
    /*size*/ sizeof(bTrackToConstraint),
    /*name*/ N_("Track To"),
    /*struct_name*/ "bTrackToConstraint",
    /*free_data*/ nullptr,
    /*id_looper*/ trackto_id_looper,
    /*copy_data*/ nullptr,
    /*new_data*/ trackto_new_data,
    /*get_constraint_targets*/ trackto_get_tars,
    /*flush_constraint_targets*/ trackto_flush_tars,
    /*get_target_matrix*/ default_get_tarmat,
    /*evaluate_constraint*/ trackto_evaluate,
};

/* --------- Inverse-Kinematics --------- */

static void kinematic_new_data(void *cdata)
{
  bKinematicConstraint *data = (bKinematicConstraint *)cdata;

  data->weight = 1.0f;
  data->orientweight = 1.0f;
  data->iterations = 500;
  data->dist = 1.0f;
  data->flag = CONSTRAINT_IK_TIP | CONSTRAINT_IK_STRETCH | CONSTRAINT_IK_POS;
}

static void kinematic_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
  bKinematicConstraint *data = static_cast<bKinematicConstraint *>(con->data);

  /* chain target */
  func(con, (ID **)&data->tar, false, userdata);

  /* poletarget */
  func(con, (ID **)&data->poletar, false, userdata);
}

static int kinematic_get_tars(bConstraint *con, ListBase *list)
{
  if (con && list) {
    bKinematicConstraint *data = static_cast<bKinematicConstraint *>(con->data);
    bConstraintTarget *ct;

    /* standard target-getting macro for single-target constraints is used twice here */
    SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list);
    SINGLETARGET_GET_TARS(con, data->poletar, data->polesubtarget, ct, list);

    return 2;
  }

  return 0;
}

static void kinematic_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
  if (con && list) {
    bKinematicConstraint *data = static_cast<bKinematicConstraint *>(con->data);
    bConstraintTarget *ct = static_cast<bConstraintTarget *>(list->first);

    /* the following macro is used for all standard single-target constraints */
    SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, no_copy);
    SINGLETARGET_FLUSH_TARS(con, data->poletar, data->polesubtarget, ct, list, no_copy);
  }
}

static bool kinematic_get_tarmat(Depsgraph * /*depsgraph*/,
                                 bConstraint *con,
                                 bConstraintOb *cob,
                                 bConstraintTarget *ct,
                                 float /*ctime*/)
{
  bKinematicConstraint *data = static_cast<bKinematicConstraint *>(con->data);

  if (VALID_CONS_TARGET(ct)) {
    constraint_target_to_mat4(ct->tar,
                              ct->subtarget,
                              cob,
                              ct->matrix,
                              CONSTRAINT_SPACE_WORLD,
                              ct->space,
                              con->flag,
                              con->headtail);
    return true;
  }

  if (!ct) {
    return false;
  }
  if ((data->flag & CONSTRAINT_IK_AUTO) == 0) {
    unit_m4(ct->matrix);
    return false;
  }

  Object *ob = cob->ob;
  if (ob == nullptr) {
    unit_m4(ct->matrix);
    return false;
  }

  float vec[3];
  /* move grabtarget into world space */
  mul_v3_m4v3(vec, ob->object_to_world().ptr(), data->grabtarget);
  copy_m4_m4(ct->matrix, ob->object_to_world().ptr());
  copy_v3_v3(ct->matrix[3], vec);

  return true;
}

static bConstraintTypeInfo CTI_KINEMATIC = {
    /*type*/ CONSTRAINT_TYPE_KINEMATIC,
    /*size*/ sizeof(bKinematicConstraint),
    /*name*/ N_("IK"),
    /*struct_name*/ "bKinematicConstraint",
    /*free_data*/ nullptr,
    /*id_looper*/ kinematic_id_looper,
    /*copy_data*/ nullptr,
    /*new_data*/ kinematic_new_data,
    /*get_constraint_targets*/ kinematic_get_tars,
    /*flush_constraint_targets*/ kinematic_flush_tars,
    /*get_target_matrix*/ kinematic_get_tarmat,
    /*evaluate_constraint*/ nullptr,
};

/* -------- Follow-Path Constraint ---------- */

static void followpath_new_data(void *cdata)
{
  bFollowPathConstraint *data = (bFollowPathConstraint *)cdata;

  data->trackflag = TRACK_Y;
  data->upflag = UP_Z;
  data->offset = 0;
  data->followflag = 0;
}

static void followpath_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
  bFollowPathConstraint *data = static_cast<bFollowPathConstraint *>(con->data);

  /* target only */
  func(con, (ID **)&data->tar, false, userdata);
}

static int followpath_get_tars(bConstraint *con, ListBase *list)
{
  if (con && list) {
    bFollowPathConstraint *data = static_cast<bFollowPathConstraint *>(con->data);
    bConstraintTarget *ct;

    /* Standard target-getting macro for single-target constraints without sub-targets. */
    SINGLETARGETNS_GET_TARS(con, data->tar, ct, list);

    return 1;
  }

  return 0;
}

static void followpath_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
  if (con && list) {
    bFollowPathConstraint *data = static_cast<bFollowPathConstraint *>(con->data);
    bConstraintTarget *ct = static_cast<bConstraintTarget *>(list->first);

    /* the following macro is used for all standard single-target constraints */
    SINGLETARGETNS_FLUSH_TARS(con, data->tar, ct, list, no_copy);
  }
}

static bool followpath_get_tarmat(Depsgraph * /*depsgraph*/,
                                  bConstraint *con,
                                  bConstraintOb * /*cob*/,
                                  bConstraintTarget *ct,
                                  float /*ctime*/)
{
  bFollowPathConstraint *data = static_cast<bFollowPathConstraint *>(con->data);

  if (!VALID_CONS_TARGET(ct) || ct->tar->type != OB_CURVES_LEGACY) {
    unit_ct_matrix_nullsafe(ct);
    return false;
  }

  Curve *cu = static_cast<Curve *>(ct->tar->data);
  float vec[4], radius;
  float curvetime;

  unit_m4(ct->matrix);

  /* NOTE: when creating constraints that follow path, the curve gets the CU_PATH set now,
   * currently for paths to work it needs to go through the bevlist/displist system (ton)
   */

  if (ct->tar->runtime->curve_cache == nullptr ||
      ct->tar->runtime->curve_cache->anim_path_accum_length == nullptr)
  {
    return false;
  }

  float quat[4];
  if (data->followflag & FOLLOWPATH_STATIC) {
    /* fixed position along curve */
    curvetime = data->offset_fac;
  }
  else {
    /* animated position along curve depending on time */
    curvetime = cu->ctime - data->offset;

    /* ctime is now a proper var setting of Curve which gets set by Animato like any other var
     * that's animated, but this will only work if it actually is animated...
     *
     * we divide the curvetime calculated in the previous step by the length of the path,
     * to get a time factor. */
    curvetime /= cu->pathlen;

    Nurb *nu = static_cast<Nurb *>(cu->nurb.first);
    if (!(nu && nu->flagu & CU_NURB_CYCLIC) && cu->flag & CU_PATH_CLAMP) {
      /* If curve is not cyclic, clamp to the begin/end points if the curve clamp option is on.
       */
      CLAMP(curvetime, 0.0f, 1.0f);
    }
  }

  if (!BKE_where_on_path(ct->tar,
                         curvetime,
                         vec,
                         nullptr,
                         (data->followflag & FOLLOWPATH_FOLLOW) ? quat : nullptr,
                         &radius,
                         nullptr))
  {
    return false;
  }

  float totmat[4][4];
  unit_m4(totmat);

  if (data->followflag & FOLLOWPATH_FOLLOW) {
    quat_apply_track(
        quat, std::clamp<short>(data->trackflag, 0, 5), std::clamp<short>(data->upflag, 0, 2));
    quat_to_mat4(totmat, quat);
  }

  if (data->followflag & FOLLOWPATH_RADIUS) {
    float tmat[4][4], rmat[4][4];
    scale_m4_fl(tmat, radius);
    mul_m4_m4m4(rmat, tmat, totmat);
    copy_m4_m4(totmat, rmat);
  }

  copy_v3_v3(totmat[3], vec);

  mul_m4_m4m4(ct->matrix, ct->tar->object_to_world().ptr(), totmat);
  return true;
}

static void followpath_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
  bConstraintTarget *ct = static_cast<bConstraintTarget *>(targets->first);

  /* only evaluate if there is a target */
  if (VALID_CONS_TARGET(ct)) {
    float obmat[4][4];
    float size[3];
    bFollowPathConstraint *data = static_cast<bFollowPathConstraint *>(con->data);

    /* get Object transform (loc/rot/size) to determine transformation from path */
    /* TODO: this used to be local at one point, but is probably more useful as-is */
    copy_m4_m4(obmat, cob->matrix);

    /* get scaling of object before applying constraint */
    mat4_to_size(size, cob->matrix);

    /* apply targetmat - containing location on path, and rotation */
    mul_m4_m4m4(cob->matrix, ct->matrix, obmat);

    /* un-apply scaling caused by path */
    if ((data->followflag & FOLLOWPATH_RADIUS) == 0) {
      /* XXX(@ideasman42): Assume that scale correction means that radius
       * will have some scale error in it. */
      float obsize[3];

      mat4_to_size(obsize, cob->matrix);
      if (obsize[0]) {
        mul_v3_fl(cob->matrix[0], size[0] / obsize[0]);
      }
      if (obsize[1]) {
        mul_v3_fl(cob->matrix[1], size[1] / obsize[1]);
      }
      if (obsize[2]) {
        mul_v3_fl(cob->matrix[2], size[2] / obsize[2]);
      }
    }
  }
}

static bConstraintTypeInfo CTI_FOLLOWPATH = {
    /*type*/ CONSTRAINT_TYPE_FOLLOWPATH,
    /*size*/ sizeof(bFollowPathConstraint),
    /*name*/ N_("Follow Path"),
    /*struct_name*/ "bFollowPathConstraint",
    /*free_data*/ nullptr,
    /*id_looper*/ followpath_id_looper,
    /*copy_data*/ nullptr,
    /*new_data*/ followpath_new_data,
    /*get_constraint_targets*/ followpath_get_tars,
    /*flush_constraint_targets*/ followpath_flush_tars,
    /*get_target_matrix*/ followpath_get_tarmat,
    /*evaluate_constraint*/ followpath_evaluate,
};

/* --------- Limit Location --------- */

static void loclimit_evaluate(bConstraint *con, bConstraintOb *cob, ListBase * /*targets*/)
{
  bLocLimitConstraint *data = static_cast<bLocLimitConstraint *>(con->data);

  if (data->flag & LIMIT_XMIN) {
    cob->matrix[3][0] = std::max(cob->matrix[3][0], data->xmin);
  }
  if (data->flag & LIMIT_XMAX) {
    cob->matrix[3][0] = std::min(cob->matrix[3][0], data->xmax);
  }
  if (data->flag & LIMIT_YMIN) {
    cob->matrix[3][1] = std::max(cob->matrix[3][1], data->ymin);
  }
  if (data->flag & LIMIT_YMAX) {
    cob->matrix[3][1] = std::min(cob->matrix[3][1], data->ymax);
  }
  if (data->flag & LIMIT_ZMIN) {
    cob->matrix[3][2] = std::max(cob->matrix[3][2], data->zmin);
  }
  if (data->flag & LIMIT_ZMAX) {
    cob->matrix[3][2] = std::min(cob->matrix[3][2], data->zmax);
  }
}

static bConstraintTypeInfo CTI_LOCLIMIT = {
    /*type*/ CONSTRAINT_TYPE_LOCLIMIT,
    /*size*/ sizeof(bLocLimitConstraint),
    /*name*/ N_("Limit Location"),
    /*struct_name*/ "bLocLimitConstraint",
    /*free_data*/ nullptr,
    /*id_looper*/ nullptr,
    /*copy_data*/ nullptr,
    /*new_data*/ nullptr,
    /*get_constraint_targets*/ nullptr,
    /*flush_constraint_targets*/ nullptr,
    /*get_target_matrix*/ nullptr,
    /*evaluate_constraint*/ loclimit_evaluate,
};

/* -------- Limit Rotation --------- */

/**
 * Wraps a number to be in [-PI, +PI].
 */
static inline float wrap_rad_angle(const float angle)
{
  const float b = angle * (0.5 / M_PI) + 0.5;
  return ((b - std::floor(b)) - 0.5) * (2.0 * M_PI);
}

/**
 * Clamps an angle between min and max.
 *
 * All angles are in radians.
 *
 * This function treats angles as existing in a looping (cyclic) space, and is therefore
 * specifically not equivalent to a simple `clamp(angle, min, max)`. `min` and `max` are treated as
 * a directed range on the unit circle and `angle` is treated as a point on the unit circle.
 * `angle` is then clamped to be within the directed range defined by `min` and `max`.
 */
static float clamp_angle(const float angle, const float min, const float max)
{
  /* If the allowed range exceeds 360 degrees no clamping can occur. */
  if ((max - min) >= (2 * M_PI)) {
    return angle;
  }

  /* Invalid case, just return min. */
  if (max <= min) {
    return min;
  }

  /* Move min and max into a space where `angle == 0.0`, and wrap them to
   * [-PI, +PI] in that space.  This simplifies the cases below, as we can
   * just use 0.0 in place of `angle` and know that everything is in
   * [-PI, +PI]. */
  const float min_wrapped = wrap_rad_angle(min - angle);
  const float max_wrapped = wrap_rad_angle(max - angle);

  /* If the range defined by `min`/`max` doesn't contain the boundary at
   * PI/-PI.  This is the simple case, because it means we can do a simple
   * clamp. */
  if (min_wrapped < max_wrapped) {
    return angle + std::clamp(0.0f, min_wrapped, max_wrapped);
  }

  /* At this point we know that `min_wrapped` >= `max_wrapped`, meaning the boundary is crossed.
   * With that we know that no clamping is needed in the following case. */
  if (max_wrapped >= 0.0 || min_wrapped <= 0.0) {
    return angle;
  }

  /* If zero is outside of the range, we clamp to the closest of `min_wrapped` or `max_wrapped`. */
  if (std::fabs(max_wrapped) < std::fabs(min_wrapped)) {
    return angle + max_wrapped;
  }
  return angle + min_wrapped;
}

static void rotlimit_evaluate(bConstraint *con, bConstraintOb *cob, ListBase * /*targets*/)
{
  bRotLimitConstraint *data = static_cast<bRotLimitConstraint *>(con->data);
  float loc[3];
  float eul[3];
  float size[3];

  /* This constraint is based on euler rotation math, which doesn't work well with shear.
   * The Y axis is chosen as the main one because constraints are most commonly used on bones.
   * This also allows using the constraint to simply remove shear. */
  orthogonalize_m4_stable(cob->matrix, 1, false);

  /* Only do the complex processing if some limits are actually enabled. */
  if (!(data->flag & (LIMIT_XROT | LIMIT_YROT | LIMIT_ZROT))) {
    return;
  }

  /* Select the Euler rotation order, defaulting to the owner value. */
  short rot_order = cob->rotOrder;

  if (data->euler_order != CONSTRAINT_EULER_AUTO) {
    rot_order = data->euler_order;
  }

  /* Decompose the matrix using the specified order. */
  copy_v3_v3(loc, cob->matrix[3]);
  mat4_to_size(size, cob->matrix);

  mat4_to_eulO(eul, rot_order, cob->matrix);

  /* Limit the euler values. */
  if (data->flag & LIMIT_ROT_LEGACY_BEHAVIOR) {
    /* The legacy behavior, which just does a naive clamping of the angles as
     * simple numbers. Since the input angles are always in the range [-180,
     * 180] degrees due to being derived from matrix decomposition, this naive
     * approach causes problems when rotations cross 180 degrees. Specifically,
     * it results in unpredictable and unwanted rotation flips of the
     * constrained objects/bones, especially when the constraint isn't in local
     * space.
     *
     * The correct thing to do is a more sophisticated form of clamping that
     * treats the angles as existing on a continuous loop, which is what the
     * non-legacy behavior further below does. However, for backwards
     * compatibility we are preserving this old behavior behind an option.
     *
     * See issues #117927 and #123105 for additional background. */
    if (data->flag & LIMIT_XROT) {
      eul[0] = clamp_f(eul[0], data->xmin, data->xmax);
    }
    if (data->flag & LIMIT_YROT) {
      eul[1] = clamp_f(eul[1], data->ymin, data->ymax);
    }
    if (data->flag & LIMIT_ZROT) {
      eul[2] = clamp_f(eul[2], data->zmin, data->zmax);
    }
  }
  else {
    /* The correct, non-legacy behavior. */
    if (data->flag & LIMIT_XROT) {
      eul[0] = clamp_angle(eul[0], data->xmin, data->xmax);
    }
    if (data->flag & LIMIT_YROT) {
      eul[1] = clamp_angle(eul[1], data->ymin, data->ymax);
    }
    if (data->flag & LIMIT_ZROT) {
      eul[2] = clamp_angle(eul[2], data->zmin, data->zmax);
    }
  }

  loc_eulO_size_to_mat4(cob->matrix, loc, eul, size, rot_order);
}

static bConstraintTypeInfo CTI_ROTLIMIT = {
    /*type*/ CONSTRAINT_TYPE_ROTLIMIT,
    /*size*/ sizeof(bRotLimitConstraint),
    /*name*/ N_("Limit Rotation"),
    /*struct_name*/ "bRotLimitConstraint",
    /*free_data*/ nullptr,
    /*id_looper*/ nullptr,
    /*copy_data*/ nullptr,
    /*new_data*/ nullptr,
    /*get_constraint_targets*/ nullptr,
    /*flush_constraint_targets*/ nullptr,
    /*get_target_matrix*/ nullptr,
    /*evaluate_constraint*/ rotlimit_evaluate,
};

/* --------- Limit Scale --------- */

static void sizelimit_evaluate(bConstraint *con, bConstraintOb *cob, ListBase * /*targets*/)
{
  bSizeLimitConstraint *data = static_cast<bSizeLimitConstraint *>(con->data);
  float obsize[3], size[3];

  mat4_to_size(size, cob->matrix);

  copy_v3_v3(obsize, size);

  if (data->flag & LIMIT_XMIN) {
    size[0] = std::max(size[0], data->xmin);
  }
  if (data->flag & LIMIT_XMAX) {
    size[0] = std::min(size[0], data->xmax);
  }
  if (data->flag & LIMIT_YMIN) {
    size[1] = std::max(size[1], data->ymin);
  }
  if (data->flag & LIMIT_YMAX) {
    size[1] = std::min(size[1], data->ymax);
  }
  if (data->flag & LIMIT_ZMIN) {
    size[2] = std::max(size[2], data->zmin);
  }
  if (data->flag & LIMIT_ZMAX) {
    size[2] = std::min(size[2], data->zmax);
  }

  if (obsize[0]) {
    mul_v3_fl(cob->matrix[0], size[0] / obsize[0]);
  }
  if (obsize[1]) {
    mul_v3_fl(cob->matrix[1], size[1] / obsize[1]);
  }
  if (obsize[2]) {
    mul_v3_fl(cob->matrix[2], size[2] / obsize[2]);
  }
}

static bConstraintTypeInfo CTI_SIZELIMIT = {
    /*type*/ CONSTRAINT_TYPE_SIZELIMIT,
    /*size*/ sizeof(bSizeLimitConstraint),
    /*name*/ N_("Limit Scale"),
    /*struct_name*/ "bSizeLimitConstraint",
    /*free_data*/ nullptr,
    /*id_looper*/ nullptr,
    /*copy_data*/ nullptr,
    /*new_data*/ nullptr,
    /*get_constraint_targets*/ nullptr,
    /*flush_constraint_targets*/ nullptr,
    /*get_target_matrix*/ nullptr,
    /*evaluate_constraint*/ sizelimit_evaluate,
};

/* ----------- Copy Location ------------- */

static void loclike_new_data(void *cdata)
{
  bLocateLikeConstraint *data = (bLocateLikeConstraint *)cdata;

  data->flag = LOCLIKE_X | LOCLIKE_Y | LOCLIKE_Z;
}

static void loclike_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
  bLocateLikeConstraint *data = static_cast<bLocateLikeConstraint *>(con->data);

  /* target only */
  func(con, (ID **)&data->tar, false, userdata);
}

static int loclike_get_tars(bConstraint *con, ListBase *list)
{
  if (con && list) {
    bLocateLikeConstraint *data = static_cast<bLocateLikeConstraint *>(con->data);
    bConstraintTarget *ct;

    /* standard target-getting macro for single-target constraints */
    SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list);

    return 1;
  }

  return 0;
}

static void loclike_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
  if (con && list) {
    bLocateLikeConstraint *data = static_cast<bLocateLikeConstraint *>(con->data);
    bConstraintTarget *ct = static_cast<bConstraintTarget *>(list->first);

    /* the following macro is used for all standard single-target constraints */
    SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, no_copy);
  }
}

static void loclike_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
  bLocateLikeConstraint *data = static_cast<bLocateLikeConstraint *>(con->data);
  bConstraintTarget *ct = static_cast<bConstraintTarget *>(targets->first);

  if (VALID_CONS_TARGET(ct)) {
    float offset[3] = {0.0f, 0.0f, 0.0f};

    if (data->flag & LOCLIKE_OFFSET) {
      copy_v3_v3(offset, cob->matrix[3]);
    }

    if (data->flag & LOCLIKE_X) {
      cob->matrix[3][0] = ct->matrix[3][0];

      if (data->flag & LOCLIKE_X_INVERT) {
        cob->matrix[3][0] *= -1;
      }
      cob->matrix[3][0] += offset[0];
    }
    if (data->flag & LOCLIKE_Y) {
      cob->matrix[3][1] = ct->matrix[3][1];

      if (data->flag & LOCLIKE_Y_INVERT) {
        cob->matrix[3][1] *= -1;
      }
      cob->matrix[3][1] += offset[1];
    }
    if (data->flag & LOCLIKE_Z) {
      cob->matrix[3][2] = ct->matrix[3][2];

      if (data->flag & LOCLIKE_Z_INVERT) {
        cob->matrix[3][2] *= -1;
      }
      cob->matrix[3][2] += offset[2];
    }
  }
}

static bConstraintTypeInfo CTI_LOCLIKE = {
    /*type*/ CONSTRAINT_TYPE_LOCLIKE,
    /*size*/ sizeof(bLocateLikeConstraint),
    /*name*/ N_("Copy Location"),
    /*struct_name*/ "bLocateLikeConstraint",
    /*free_data*/ nullptr,
    /*id_looper*/ loclike_id_looper,
    /*copy_data*/ nullptr,
    /*new_data*/ loclike_new_data,
    /*get_constraint_targets*/ loclike_get_tars,
    /*flush_constraint_targets*/ loclike_flush_tars,
    /*get_target_matrix*/ default_get_tarmat,
    /*evaluate_constraint*/ loclike_evaluate,
};

/* ----------- Copy Rotation ------------- */

static void rotlike_new_data(void *cdata)
{
  bRotateLikeConstraint *data = (bRotateLikeConstraint *)cdata;

  data->flag = ROTLIKE_X | ROTLIKE_Y | ROTLIKE_Z;
}

static void rotlike_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
  bRotateLikeConstraint *data = static_cast<bRotateLikeConstraint *>(con->data);

  /* target only */
  func(con, (ID **)&data->tar, false, userdata);
}

static int rotlike_get_tars(bConstraint *con, ListBase *list)
{
  if (con && list) {
    bRotateLikeConstraint *data = static_cast<bRotateLikeConstraint *>(con->data);
    bConstraintTarget *ct;

    /* standard target-getting macro for single-target constraints */
    SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list);

    return 1;
  }

  return 0;
}

static void rotlike_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
  if (con && list) {
    bRotateLikeConstraint *data = static_cast<bRotateLikeConstraint *>(con->data);
    bConstraintTarget *ct = static_cast<bConstraintTarget *>(list->first);

    /* the following macro is used for all standard single-target constraints */
    SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, no_copy);
  }
}

static void rotlike_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
  bRotateLikeConstraint *data = static_cast<bRotateLikeConstraint *>(con->data);
  bConstraintTarget *ct = static_cast<bConstraintTarget *>(targets->first);

  if (VALID_CONS_TARGET(ct)) {
    float loc[3], size[3], oldrot[3][3], newrot[3][3];
    float eul[3], obeul[3], defeul[3];

    mat4_to_loc_rot_size(loc, oldrot, size, cob->matrix);

    /* Select the Euler rotation order, defaulting to the owner. */
    short rot_order = cob->rotOrder;

    if (data->euler_order != CONSTRAINT_EULER_AUTO) {
      rot_order = data->euler_order;
    }

    /* To allow compatible rotations, must get both rotations in the order of the owner... */
    mat4_to_eulO(obeul, rot_order, cob->matrix);
    /* We must get compatible eulers from the beginning because
     * some of them can be modified below (see bug #21875).
     * Additionally, since this constraint is based on euler rotation math, it doesn't work well
     * with shear. The Y axis is chosen as the main axis when we orthogonalize the matrix because
     * constraints are used most commonly on bones. */
    float mat[4][4];
    copy_m4_m4(mat, ct->matrix);
    orthogonalize_m4_stable(mat, 1, true);
    mat4_to_compatible_eulO(eul, obeul, rot_order, mat);

    /* Prepare the copied euler rotation. */
    bool legacy_offset = false;

    switch (data->mix_mode) {
      case ROTLIKE_MIX_OFFSET:
        legacy_offset = true;
        copy_v3_v3(defeul, obeul);
        break;

      case ROTLIKE_MIX_REPLACE:
        copy_v3_v3(defeul, obeul);
        break;

      default:
        zero_v3(defeul);
    }

    if ((data->flag & ROTLIKE_X) == 0) {
      eul[0] = defeul[0];
    }
    else {
      if (legacy_offset) {
        rotate_eulO(eul, rot_order, 'X', obeul[0]);
      }

      if (data->flag & ROTLIKE_X_INVERT) {
        eul[0] *= -1;
      }
    }

    if ((data->flag & ROTLIKE_Y) == 0) {
      eul[1] = defeul[1];
    }
    else {
      if (legacy_offset) {
        rotate_eulO(eul, rot_order, 'Y', obeul[1]);
      }

      if (data->flag & ROTLIKE_Y_INVERT) {
        eul[1] *= -1;
      }
    }

    if ((data->flag & ROTLIKE_Z) == 0) {
      eul[2] = defeul[2];
    }
    else {
      if (legacy_offset) {
        rotate_eulO(eul, rot_order, 'Z', obeul[2]);
      }

      if (data->flag & ROTLIKE_Z_INVERT) {
        eul[2] *= -1;
      }
    }

    /* Add the euler components together if needed. */
    if (data->mix_mode == ROTLIKE_MIX_ADD) {
      add_v3_v3(eul, obeul);
    }

    /* Good to make eulers compatible again,
     * since we don't know how much they were changed above. */
    compatible_eul(eul, obeul);
    eulO_to_mat3(newrot, eul, rot_order);

    /* Mix the rotation matrices: */
    switch (data->mix_mode) {
      case ROTLIKE_MIX_REPLACE:
      case ROTLIKE_MIX_OFFSET:
      case ROTLIKE_MIX_ADD:
        break;

      case ROTLIKE_MIX_BEFORE:
        mul_m3_m3m3(newrot, newrot, oldrot);
        break;

      case ROTLIKE_MIX_AFTER:
        mul_m3_m3m3(newrot, oldrot, newrot);
        break;

      default:
        BLI_assert(false);
    }

    loc_rot_size_to_mat4(cob->matrix, loc, newrot, size);
  }
}

static bConstraintTypeInfo CTI_ROTLIKE = {
    /*type*/ CONSTRAINT_TYPE_ROTLIKE,
    /*size*/ sizeof(bRotateLikeConstraint),
    /*name*/ N_("Copy Rotation"),
    /*struct_name*/ "bRotateLikeConstraint",
    /*free_data*/ nullptr,
    /*id_looper*/ rotlike_id_looper,
    /*copy_data*/ nullptr,
    /*new_data*/ rotlike_new_data,
    /*get_constraint_targets*/ rotlike_get_tars,
    /*flush_constraint_targets*/ rotlike_flush_tars,
    /*get_target_matrix*/ default_get_tarmat,
    /*evaluate_constraint*/ rotlike_evaluate,
};

/* ---------- Copy Scale ---------- */

static void sizelike_new_data(void *cdata)
{
  bSizeLikeConstraint *data = (bSizeLikeConstraint *)cdata;

  data->flag = SIZELIKE_X | SIZELIKE_Y | SIZELIKE_Z | SIZELIKE_MULTIPLY;
  data->power = 1.0f;
}

static void sizelike_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
  bSizeLikeConstraint *data = static_cast<bSizeLikeConstraint *>(con->data);

  /* target only */
  func(con, (ID **)&data->tar, false, userdata);
}

static int sizelike_get_tars(bConstraint *con, ListBase *list)
{
  if (con && list) {
    bSizeLikeConstraint *data = static_cast<bSizeLikeConstraint *>(con->data);
    bConstraintTarget *ct;

    /* standard target-getting macro for single-target constraints */
    SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list);

    return 1;
  }

  return 0;
}

static void sizelike_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
  if (con && list) {
    bSizeLikeConstraint *data = static_cast<bSizeLikeConstraint *>(con->data);
    bConstraintTarget *ct = static_cast<bConstraintTarget *>(list->first);

    /* the following macro is used for all standard single-target constraints */
    SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, no_copy);
  }
}

static void sizelike_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
  bSizeLikeConstraint *data = static_cast<bSizeLikeConstraint *>(con->data);
  bConstraintTarget *ct = static_cast<bConstraintTarget *>(targets->first);

  if (VALID_CONS_TARGET(ct)) {
    float obsize[3], size[3];

    mat4_to_size(obsize, cob->matrix);

    /* Compute one uniform scale factor to apply to all three axes. */
    if (data->flag & SIZELIKE_UNIFORM) {
      const int all_axes = SIZELIKE_X | SIZELIKE_Y | SIZELIKE_Z;
      float total = 1.0f;

      /* If all axes are selected, use the determinant. */
      if ((data->flag & all_axes) == all_axes) {
        total = fabsf(mat4_to_volume_scale(ct->matrix));
      }
      /* Otherwise multiply individual values. */
      else {
        mat4_to_size(size, ct->matrix);

        if (data->flag & SIZELIKE_X) {
          total *= size[0];
        }
        if (data->flag & SIZELIKE_Y) {
          total *= size[1];
        }
        if (data->flag & SIZELIKE_Z) {
          total *= size[2];
        }
      }

      copy_v3_fl(size, cbrt(total));
    }
    /* Regular per-axis scaling. */
    else {
      mat4_to_size(size, ct->matrix);
    }

    for (int i = 0; i < 3; i++) {
      size[i] = powf(size[i], data->power);
    }

    if (data->flag & SIZELIKE_OFFSET) {
      /* Scale is a multiplicative quantity, so adding it makes no sense.
       * However, the additive mode has to stay for backward compatibility. */
      if (data->flag & SIZELIKE_MULTIPLY) {
        /* size[i] *= obsize[i] */
        mul_v3_v3(size, obsize);
      }
      else {
        /* 2.7 compatibility mode: size[i] += (obsize[i] - 1.0f) */
        add_v3_v3(size, obsize);
        add_v3_fl(size, -1.0f);
      }
    }

    if ((data->flag & (SIZELIKE_X | SIZELIKE_UNIFORM)) && (obsize[0] != 0)) {
      mul_v3_fl(cob->matrix[0], size[0] / obsize[0]);
    }
    if ((data->flag & (SIZELIKE_Y | SIZELIKE_UNIFORM)) && (obsize[1] != 0)) {
      mul_v3_fl(cob->matrix[1], size[1] / obsize[1]);
    }
    if ((data->flag & (SIZELIKE_Z | SIZELIKE_UNIFORM)) && (obsize[2] != 0)) {
      mul_v3_fl(cob->matrix[2], size[2] / obsize[2]);
    }
  }
}

static bConstraintTypeInfo CTI_SIZELIKE = {
    /*type*/ CONSTRAINT_TYPE_SIZELIKE,
    /*size*/ sizeof(bSizeLikeConstraint),
    /*name*/ N_("Copy Scale"),
    /*struct_name*/ "bSizeLikeConstraint",
    /*free_data*/ nullptr,
    /*id_looper*/ sizelike_id_looper,
    /*copy_data*/ nullptr,
    /*new_data*/ sizelike_new_data,
    /*get_constraint_targets*/ sizelike_get_tars,
    /*flush_constraint_targets*/ sizelike_flush_tars,
    /*get_target_matrix*/ default_get_tarmat,
    /*evaluate_constraint*/ sizelike_evaluate,
};

/* ----------- Copy Transforms ------------- */

static void translike_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
  bTransLikeConstraint *data = static_cast<bTransLikeConstraint *>(con->data);

  /* target only */
  func(con, (ID **)&data->tar, false, userdata);
}

static int translike_get_tars(bConstraint *con, ListBase *list)
{
  if (con && list) {
    bTransLikeConstraint *data = static_cast<bTransLikeConstraint *>(con->data);
    bConstraintTarget *ct;

    /* standard target-getting macro for single-target constraints */
    SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list);

    return 1;
  }

  return 0;
}

static void translike_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
  if (con && list) {
    bTransLikeConstraint *data = static_cast<bTransLikeConstraint *>(con->data);
    bConstraintTarget *ct = static_cast<bConstraintTarget *>(list->first);

    /* the following macro is used for all standard single-target constraints */
    SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, no_copy);
  }
}

static void translike_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
  bTransLikeConstraint *data = static_cast<bTransLikeConstraint *>(con->data);
  bConstraintTarget *ct = static_cast<bConstraintTarget *>(targets->first);

  if (VALID_CONS_TARGET(ct)) {
    float target_mat[4][4];

    copy_m4_m4(target_mat, ct->matrix);

    /* Remove the shear of the target matrix if enabled.
     * Use Y as the axis since it's the natural default for bones. */
    if (data->flag & TRANSLIKE_REMOVE_TARGET_SHEAR) {
      orthogonalize_m4_stable(target_mat, 1, false);
    }

    /* Finally, combine the matrices. */
    switch (data->mix_mode) {
      case TRANSLIKE_MIX_REPLACE:
        copy_m4_m4(cob->matrix, target_mat);
        break;

      /* Simple matrix multiplication. */
      case TRANSLIKE_MIX_BEFORE_FULL:
        mul_m4_m4m4(cob->matrix, target_mat, cob->matrix);
        break;

      case TRANSLIKE_MIX_AFTER_FULL:
        mul_m4_m4m4(cob->matrix, cob->matrix, target_mat);
        break;

      /* Aligned Inherit Scale emulation. */
      case TRANSLIKE_MIX_BEFORE:
        mul_m4_m4m4_aligned_scale(cob->matrix, target_mat, cob->matrix);
        break;

      case TRANSLIKE_MIX_AFTER:
        mul_m4_m4m4_aligned_scale(cob->matrix, cob->matrix, target_mat);
        break;

      /* Fully separate handling of channels. */
      case TRANSLIKE_MIX_BEFORE_SPLIT:
        mul_m4_m4m4_split_channels(cob->matrix, target_mat, cob->matrix);
        break;

      case TRANSLIKE_MIX_AFTER_SPLIT:
        mul_m4_m4m4_split_channels(cob->matrix, cob->matrix, target_mat);
        break;

      default:
        BLI_assert_msg(0, "Unknown Copy Transforms mix mode");
    }
  }
}

static bConstraintTypeInfo CTI_TRANSLIKE = {
    /*type*/ CONSTRAINT_TYPE_TRANSLIKE,
    /*size*/ sizeof(bTransLikeConstraint),
    /*name*/ N_("Copy Transforms"),
    /*struct_name*/ "bTransLikeConstraint",
    /*free_data*/ nullptr,
    /*id_looper*/ translike_id_looper,
    /*copy_data*/ nullptr,
    /*new_data*/ nullptr,
    /*get_constraint_targets*/ translike_get_tars,
    /*flush_constraint_targets*/ translike_flush_tars,
    /*get_target_matrix*/ default_get_tarmat_full_bbone,
    /*evaluate_constraint*/ translike_evaluate,
};

/* ---------- Maintain Volume ---------- */

static void samevolume_new_data(void *cdata)
{
  bSameVolumeConstraint *data = (bSameVolumeConstraint *)cdata;

  data->free_axis = SAMEVOL_Y;
  data->volume = 1.0f;
}

static void samevolume_evaluate(bConstraint *con, bConstraintOb *cob, ListBase * /*targets*/)
{
  bSameVolumeConstraint *data = static_cast<bSameVolumeConstraint *>(con->data);

  float volume = data->volume;
  float fac = 1.0f, total_scale = 1.0f;
  float obsize[3];

  mat4_to_size(obsize, cob->matrix);

  /* calculate normalizing scale factor for non-essential values */
  switch (data->mode) {
    case SAMEVOL_STRICT:
      total_scale = obsize[0] * obsize[1] * obsize[2];
      break;
    case SAMEVOL_UNIFORM:
      total_scale = pow3f(obsize[data->free_axis]);
      break;
    case SAMEVOL_SINGLE_AXIS:
      total_scale = obsize[data->free_axis];
      break;
  }

  if (total_scale != 0) {
    fac = sqrtf(volume / total_scale);
  }

  /* apply scaling factor to the channels not being kept */
  switch (data->free_axis) {
    case SAMEVOL_X:
      mul_v3_fl(cob->matrix[1], fac);
      mul_v3_fl(cob->matrix[2], fac);
      break;
    case SAMEVOL_Y:
      mul_v3_fl(cob->matrix[0], fac);
      mul_v3_fl(cob->matrix[2], fac);
      break;
    case SAMEVOL_Z:
      mul_v3_fl(cob->matrix[0], fac);
      mul_v3_fl(cob->matrix[1], fac);
      break;
  }
}

static bConstraintTypeInfo CTI_SAMEVOL = {
    /*type*/ CONSTRAINT_TYPE_SAMEVOL,
    /*size*/ sizeof(bSameVolumeConstraint),
    /*name*/ N_("Maintain Volume"),
    /*struct_name*/ "bSameVolumeConstraint",
    /*free_data*/ nullptr,
    /*id_looper*/ nullptr,
    /*copy_data*/ nullptr,
    /*new_data*/ samevolume_new_data,
    /*get_constraint_targets*/ nullptr,
    /*flush_constraint_targets*/ nullptr,
    /*get_target_matrix*/ nullptr,
    /*evaluate_constraint*/ samevolume_evaluate,
};

/* ----------- Armature Constraint -------------- */

static void armdef_free(bConstraint *con)
{
  bArmatureConstraint *data = static_cast<bArmatureConstraint *>(con->data);

  /* Target list. */
  BLI_freelistN(&data->targets);
}

static void armdef_copy(bConstraint *con, bConstraint *srccon)
{
  bArmatureConstraint *pcon = (bArmatureConstraint *)con->data;
  bArmatureConstraint *opcon = (bArmatureConstraint *)srccon->data;

  BLI_duplicatelist(&pcon->targets, &opcon->targets);
}

static int armdef_get_tars(bConstraint *con, ListBase *list)
{
  if (con && list) {
    bArmatureConstraint *data = static_cast<bArmatureConstraint *>(con->data);

    *list = data->targets;

    return BLI_listbase_count(&data->targets);
  }

  return 0;
}

static void armdef_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
  bArmatureConstraint *data = static_cast<bArmatureConstraint *>(con->data);

  /* Target list. */
  LISTBASE_FOREACH (bConstraintTarget *, ct, &data->targets) {
    func(con, (ID **)&ct->tar, false, userdata);
  }
}

/* Compute the world space pose matrix of the target bone. */
static bool armdef_get_tarmat(Depsgraph * /*depsgraph*/,
                              bConstraint * /*con*/,
                              bConstraintOb * /*cob*/,
                              bConstraintTarget *ct,
                              float /*ctime*/)
{
  if (!VALID_CONS_TARGET(ct) || ct->tar->type != OB_ARMATURE) {
    unit_ct_matrix_nullsafe(ct);
    return false;
  }

  bPoseChannel *pchan = BKE_pose_channel_find_name(ct->tar->pose, ct->subtarget);
  if (pchan == nullptr) {
    unit_m4(ct->matrix);
    return false;
  }

  mul_m4_m4m4(ct->matrix, ct->tar->object_to_world().ptr(), pchan->pose_mat);
  return true;
}

static void armdef_accumulate_matrix(const float obmat[4][4],
                                     const float iobmat[4][4],
                                     const float basemat[4][4],
                                     const float bonemat[4][4],
                                     const float pivot[3],
                                     const float weight,
                                     float r_sum_mat[4][4],
                                     DualQuat *r_sum_dq)
{
  if (weight == 0.0f) {
    return;
  }

  /* Convert the selected matrix into object space. */
  float mat[4][4];
  mul_m4_series(mat, obmat, bonemat, iobmat);

  /* Accumulate the transformation. */
  if (r_sum_dq != nullptr) {
    float basemat_world[4][4];
    DualQuat tmpdq;

    /* Compute the orthonormal rest matrix in world space. */
    mul_m4_m4m4(basemat_world, obmat, basemat);
    orthogonalize_m4_stable(basemat_world, 1, true);

    mat4_to_dquat(&tmpdq, basemat_world, mat);
    add_weighted_dq_dq_pivot(r_sum_dq, &tmpdq, pivot, weight, true);
  }
  else {
    madd_m4_m4m4fl(r_sum_mat, r_sum_mat, mat, weight);
  }
}

/* Compute and accumulate transformation for a single target bone. */
static void armdef_accumulate_bone(const bConstraintTarget *ct,
                                   const bPoseChannel *pchan,
                                   const float wco[3],
                                   const bool force_envelope,
                                   float *r_totweight,
                                   float r_sum_mat[4][4],
                                   DualQuat *r_sum_dq)
{
  float iobmat[4][4], co[3];
  const Bone *bone = pchan->bone;
  float weight = ct->weight;

  /* Our object's location in target pose space. */
  invert_m4_m4(iobmat, ct->tar->object_to_world().ptr());
  mul_v3_m4v3(co, iobmat, wco);

  /* Multiply by the envelope weight when appropriate. */
  if (force_envelope || (bone->flag & BONE_MULT_VG_ENV)) {
    weight *= distfactor_to_bone(
        co, bone->arm_head, bone->arm_tail, bone->rad_head, bone->rad_tail, bone->dist);
  }

  /* Find the correct bone transform matrix in world space. */
  if (bone->segments > 1 && bone->segments == pchan->runtime.bbone_segments) {
    const Mat4 *b_bone_mats = pchan->runtime.bbone_deform_mats;
    const Mat4 *b_bone_rest_mats = pchan->runtime.bbone_rest_mats;
    float basemat[4][4];

    /* Blend the matrix. */
    int index;
    float blend;
    BKE_pchan_bbone_deform_segment_index(pchan, co, &index, &blend);

    if (r_sum_dq != nullptr) {
      /* Compute the object space rest matrix of the segment. */
      mul_m4_m4m4(basemat, bone->arm_mat, b_bone_rest_mats[index].mat);
    }

    armdef_accumulate_matrix(ct->tar->object_to_world().ptr(),
                             iobmat,
                             basemat,
                             b_bone_mats[index + 1].mat,
                             wco,
                             weight * (1.0f - blend),
                             r_sum_mat,
                             r_sum_dq);

    if (r_sum_dq != nullptr) {
      /* Compute the object space rest matrix of the segment. */
      mul_m4_m4m4(basemat, bone->arm_mat, b_bone_rest_mats[index + 1].mat);
    }

    armdef_accumulate_matrix(ct->tar->object_to_world().ptr(),
                             iobmat,
                             basemat,
                             b_bone_mats[index + 2].mat,
                             wco,
                             weight * blend,
                             r_sum_mat,
                             r_sum_dq);
  }
  else {
    /* Simple bone. This requires DEG_OPCODE_BONE_DONE dependency due to chan_mat. */
    armdef_accumulate_matrix(ct->tar->object_to_world().ptr(),
                             iobmat,
                             bone->arm_mat,
                             pchan->chan_mat,
                             wco,
                             weight,
                             r_sum_mat,
                             r_sum_dq);
  }

  /* Accumulate the weight. */
  *r_totweight += weight;
}

static void armdef_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
  bArmatureConstraint *data = static_cast<bArmatureConstraint *>(con->data);

  /* Prepare for blending. */
  float sum_mat[4][4] = {};
  DualQuat sum_dq = {};
  float weight = 0.0f;

  DualQuat *pdq = (data->flag & CONSTRAINT_ARMATURE_QUATERNION) ? &sum_dq : nullptr;
  bool use_envelopes = (data->flag & CONSTRAINT_ARMATURE_ENVELOPE) != 0;

  float input_co[3];
  if (cob->pchan && cob->pchan->bone && !(data->flag & CONSTRAINT_ARMATURE_CUR_LOCATION)) {
    /* For constraints on bones, use the rest position to bind b-bone segments
     * and envelopes, to allow safely changing the bone location as if parented. */
    copy_v3_v3(input_co, cob->pchan->bone->arm_head);
    mul_m4_v3(cob->ob->object_to_world().ptr(), input_co);
  }
  else {
    copy_v3_v3(input_co, cob->matrix[3]);
  }

  /* Process all targets. This can't use ct->matrix, as armdef_get_tarmat is not
   * called in solve for efficiency because the constraint needs bone data anyway. */
  LISTBASE_FOREACH (bConstraintTarget *, ct, targets) {
    if (ct->weight <= 0.0f) {
      continue;
    }

    /* Lookup the bone and abort if failed. */
    if (!VALID_CONS_TARGET(ct) || ct->tar->type != OB_ARMATURE) {
      return;
    }

    bPoseChannel *pchan = BKE_pose_channel_find_name(ct->tar->pose, ct->subtarget);

    if (pchan == nullptr || pchan->bone == nullptr) {
      return;
    }

    armdef_accumulate_bone(ct, pchan, input_co, use_envelopes, &weight, sum_mat, pdq);
  }

  /* Compute the final transform. */
  if (weight > 0.0f) {
    if (pdq != nullptr) {
      normalize_dq(pdq, weight);
      dquat_to_mat4(sum_mat, pdq);
    }
    else {
      mul_m4_fl(sum_mat, 1.0f / weight);
    }

    /* Apply the transform to the result matrix. */
    mul_m4_m4m4(cob->matrix, sum_mat, cob->matrix);
  }
}

static bConstraintTypeInfo CTI_ARMATURE = {
    /*type*/ CONSTRAINT_TYPE_ARMATURE,
    /*size*/ sizeof(bArmatureConstraint),
    /*name*/ N_("Armature"),
    /*struct_name*/ "bArmatureConstraint",
    /*free_data*/ armdef_free,
    /*id_looper*/ armdef_id_looper,
    /*copy_data*/ armdef_copy,
    /*new_data*/ nullptr,
    /*get_constraint_targets*/ armdef_get_tars,
    /*flush_constraint_targets*/ nullptr,
    /*get_target_matrix*/ armdef_get_tarmat,
    /*evaluate_constraint*/ armdef_evaluate,
};

/* -------- Action Constraint ----------- */

static void actcon_new_data(void *cdata)
{
  bActionConstraint *data = (bActionConstraint *)cdata;

  /* set type to 20 (Loc X), as 0 is Rot X for backwards compatibility */
  data->type = 20;

  /* Set the mix mode to After Original with anti-shear scale handling. */
  data->mix_mode = ACTCON_MIX_AFTER;
}

static void actcon_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
  bActionConstraint *data = static_cast<bActionConstraint *>(con->data);

  /* target */
  func(con, (ID **)&data->tar, false, userdata);

  /* action */
  func(con, (ID **)&data->act, true, userdata);
}

static int actcon_get_tars(bConstraint *con, ListBase *list)
{
  if (con && list) {
    bActionConstraint *data = static_cast<bActionConstraint *>(con->data);
    bConstraintTarget *ct;

    /* standard target-getting macro for single-target constraints */
    SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list);

    return 1;
  }

  return 0;
}

static void actcon_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
  if (con && list) {
    bActionConstraint *data = static_cast<bActionConstraint *>(con->data);
    bConstraintTarget *ct = static_cast<bConstraintTarget *>(list->first);

    /* the following macro is used for all standard single-target constraints */
    SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, no_copy);
  }
}

static bool actcon_get_tarmat(Depsgraph *depsgraph,
                              bConstraint *con,
                              bConstraintOb *cob,
                              bConstraintTarget *ct,
                              float /*ctime*/)
{
  bActionConstraint *data = static_cast<bActionConstraint *>(con->data);

  /* Initialize return matrix. This needs to happen even when there is no
   * Action, to avoid returning an all-zeroes matrix. */
  unit_m4(ct->matrix);

  if (!data->act) {
    /* Without an Action, this constraint cannot do anything. */
    return false;
  }

  const bool use_eval_time = data->flag & ACTCON_USE_EVAL_TIME;
  if (!VALID_CONS_TARGET(ct) && !use_eval_time) {
    return false;
  }

  float tempmat[4][4], vec[3];
  float s, t;
  short axis;

  /* Skip targets if we're using local float property to set action time */
  if (use_eval_time) {
    s = data->eval_time;
  }
  else {
    /* get the transform matrix of the target */
    constraint_target_to_mat4(ct->tar,
                              ct->subtarget,
                              cob,
                              tempmat,
                              CONSTRAINT_SPACE_WORLD,
                              ct->space,
                              con->flag,
                              con->headtail);

    /* determine where in transform range target is */
    /* data->type is mapped as follows for backwards compatibility:
     * 00,01,02 - rotation (it used to be like this)
     * 10,11,12 - scaling
     * 20,21,22 - location
     */
    if (data->type < 10) {
      /* extract rotation (is in whatever space target should be in) */
      mat4_to_eul(vec, tempmat);
      mul_v3_fl(vec, RAD2DEGF(1.0f)); /* rad -> deg */
      axis = data->type;
    }
    else if (data->type < 20) {
      /* extract scaling (is in whatever space target should be in) */
      mat4_to_size(vec, tempmat);
      axis = data->type - 10;
    }
    else {
      /* extract location */
      copy_v3_v3(vec, tempmat[3]);
      axis = data->type - 20;
    }

    BLI_assert(uint(axis) < 3);

    /* Convert the target's value into a [0, 1] value that's later used to find the Action frame
     * to apply. This compares to the min/max boundary values first, before doing the
     * normalization by the (max-min) range, to get predictable, valid values when that range is
     * zero. */
    const float range = data->max - data->min;
    if ((range == 0.0f) || (ushort(axis) > 2)) {
      s = 0.0f;
    }
    else {
      s = (vec[axis] - data->min) / range;
    }
  }

  CLAMP(s, 0, 1);
  t = (s * (data->end - data->start)) + data->start;
  const AnimationEvalContext anim_eval_context = BKE_animsys_eval_context_construct(depsgraph, t);

  if (G.debug & G_DEBUG) {
    printf("do Action Constraint %s - Ob %s Pchan %s\n",
           con->name,
           cob->ob->id.name + 2,
           (cob->pchan) ? cob->pchan->name : nullptr);
  }

  /* Get the appropriate information from the action */
  if (cob->type == CONSTRAINT_OBTYPE_OBJECT || (data->flag & ACTCON_BONE_USE_OBJECT_ACTION)) {
    Object workob;

    /* evaluate using workob */
    /* FIXME: we don't have any consistent standards on limiting effects on object... */
    what_does_obaction(cob->ob,
                       &workob,
                       nullptr,
                       data->act,
                       data->action_slot_handle,
                       nullptr,
                       &anim_eval_context);
    BKE_object_to_mat4(&workob, ct->matrix);
  }
  else if (cob->type == CONSTRAINT_OBTYPE_BONE) {
    Object workob;
    bPose pose = {{nullptr}};
    bPoseChannel *pchan, *tchan;

    /* make a copy of the bone of interest in the temp pose before evaluating action,
     * so that it can get set - we need to manually copy over a few settings,
     * including rotation order, otherwise this fails. */
    pchan = cob->pchan;

    tchan = BKE_pose_channel_ensure(&pose, pchan->name);
    tchan->rotmode = pchan->rotmode;

    /* evaluate action using workob (it will only set the PoseChannel in question) */
    what_does_obaction(cob->ob,
                       &workob,
                       &pose,
                       data->act,
                       data->action_slot_handle,
                       pchan->name,
                       &anim_eval_context);

    /* convert animation to matrices for use here */
    BKE_pchan_calc_mat(tchan);
    copy_m4_m4(ct->matrix, tchan->chan_mat);

    /* Clean up */
    BKE_pose_free_data(&pose);
  }
  else {
    /* behavior undefined... */
    puts("Error: unknown owner type for Action Constraint");
    return false;
  }

  return true;
}

static void actcon_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
  bActionConstraint *data = static_cast<bActionConstraint *>(con->data);
  bConstraintTarget *ct = static_cast<bConstraintTarget *>(targets->first);

  if (VALID_CONS_TARGET(ct) || data->flag & ACTCON_USE_EVAL_TIME) {
    switch (data->mix_mode) {
      /* Replace the input transformation. */
      case ACTCON_MIX_REPLACE:
        copy_m4_m4(cob->matrix, ct->matrix);
        break;

      /* Simple matrix multiplication. */
      case ACTCON_MIX_BEFORE_FULL:
        mul_m4_m4m4(cob->matrix, ct->matrix, cob->matrix);
        break;

      case ACTCON_MIX_AFTER_FULL:
        mul_m4_m4m4(cob->matrix, cob->matrix, ct->matrix);
        break;

      /* Aligned Inherit Scale emulation. */
      case ACTCON_MIX_BEFORE:
        mul_m4_m4m4_aligned_scale(cob->matrix, ct->matrix, cob->matrix);
        break;

      case ACTCON_MIX_AFTER:
        mul_m4_m4m4_aligned_scale(cob->matrix, cob->matrix, ct->matrix);
        break;

      /* Fully separate handling of channels. */
      case ACTCON_MIX_BEFORE_SPLIT:
        mul_m4_m4m4_split_channels(cob->matrix, ct->matrix, cob->matrix);
        break;

      case ACTCON_MIX_AFTER_SPLIT:
        mul_m4_m4m4_split_channels(cob->matrix, cob->matrix, ct->matrix);
        break;

      default:
        BLI_assert_msg(0, "Unknown Action mix mode");
    }
  }
}

static bConstraintTypeInfo CTI_ACTION = {
    /*type*/ CONSTRAINT_TYPE_ACTION,
    /*size*/ sizeof(bActionConstraint),
    /*name*/ N_("Action"),
    /*struct_name*/ "bActionConstraint",
    /*free_data*/ nullptr,
    /*id_looper*/ actcon_id_looper,
    /*copy_data*/ nullptr,
    /*new_data*/ actcon_new_data,
    /*get_constraint_targets*/ actcon_get_tars,
    /*flush_constraint_targets*/ actcon_flush_tars,
    /*get_target_matrix*/ actcon_get_tarmat,
    /*evaluate_constraint*/ actcon_evaluate,
};

/* --------- Locked Track ---------- */

static void locktrack_new_data(void *cdata)
{
  bLockTrackConstraint *data = (bLockTrackConstraint *)cdata;

  data->trackflag = TRACK_Y;
  data->lockflag = LOCK_Z;
}

static void locktrack_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
  bLockTrackConstraint *data = static_cast<bLockTrackConstraint *>(con->data);

  /* target only */
  func(con, (ID **)&data->tar, false, userdata);
}

static int locktrack_get_tars(bConstraint *con, ListBase *list)
{
  if (con && list) {
    bLockTrackConstraint *data = static_cast<bLockTrackConstraint *>(con->data);
    bConstraintTarget *ct;

    /* the following macro is used for all standard single-target constraints */
    SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list);

    return 1;
  }

  return 0;
}

static void locktrack_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
  if (con && list) {
    bLockTrackConstraint *data = static_cast<bLockTrackConstraint *>(con->data);
    bConstraintTarget *ct = static_cast<bConstraintTarget *>(list->first);

    /* the following macro is used for all standard single-target constraints */
    SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, no_copy);
  }
}

static void locktrack_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
  bLockTrackConstraint *data = static_cast<bLockTrackConstraint *>(con->data);
  bConstraintTarget *ct = static_cast<bConstraintTarget *>(targets->first);

  if (VALID_CONS_TARGET(ct)) {
    float vec[3], vec2[3];
    float totmat[3][3];
    float tmpmat[3][3];
    float invmat[3][3];
    float mdet;

    /* Vector object -> target */
    sub_v3_v3v3(vec, ct->matrix[3], cob->matrix[3]);
    switch (data->lockflag) {
      case LOCK_X: /* LOCK X */
      {
        switch (data->trackflag) {
          case TRACK_Y: /* LOCK X TRACK Y */
          {
            /* Projection of Vector on the plane */
            project_v3_v3v3(vec2, vec, cob->matrix[0]);
            sub_v3_v3v3(totmat[1], vec, vec2);
            normalize_v3(totmat[1]);

            /* the x axis is fixed */
            normalize_v3_v3(totmat[0], cob->matrix[0]);

            /* the z axis gets mapped onto a third orthogonal vector */
            cross_v3_v3v3(totmat[2], totmat[0], totmat[1]);
            break;
          }
          case TRACK_Z: /* LOCK X TRACK Z */
          {
            /* Projection of Vector on the plane */
            project_v3_v3v3(vec2, vec, cob->matrix[0]);
            sub_v3_v3v3(totmat[2], vec, vec2);
            normalize_v3(totmat[2]);

            /* the x axis is fixed */
            normalize_v3_v3(totmat[0], cob->matrix[0]);

            /* the z axis gets mapped onto a third orthogonal vector */
            cross_v3_v3v3(totmat[1], totmat[2], totmat[0]);
            break;
          }
          case TRACK_nY: /* LOCK X TRACK -Y */
          {
            /* Projection of Vector on the plane */
            project_v3_v3v3(vec2, vec, cob->matrix[0]);
            sub_v3_v3v3(totmat[1], vec, vec2);
            normalize_v3(totmat[1]);
            negate_v3(totmat[1]);

            /* the x axis is fixed */
            normalize_v3_v3(totmat[0], cob->matrix[0]);

            /* the z axis gets mapped onto a third orthogonal vector */
            cross_v3_v3v3(totmat[2], totmat[0], totmat[1]);
            break;
          }
          case TRACK_nZ: /* LOCK X TRACK -Z */
          {
            /* Projection of Vector on the plane */
            project_v3_v3v3(vec2, vec, cob->matrix[0]);
            sub_v3_v3v3(totmat[2], vec, vec2);
            normalize_v3(totmat[2]);
            negate_v3(totmat[2]);

            /* the x axis is fixed */
            normalize_v3_v3(totmat[0], cob->matrix[0]);

            /* the z axis gets mapped onto a third orthogonal vector */
            cross_v3_v3v3(totmat[1], totmat[2], totmat[0]);
            break;
          }
          default: {
            unit_m3(totmat);
            break;
          }
        }
        break;
      }
      case LOCK_Y: /* LOCK Y */
      {
        switch (data->trackflag) {
          case TRACK_X: /* LOCK Y TRACK X */
          {
            /* Projection of Vector on the plane */
            project_v3_v3v3(vec2, vec, cob->matrix[1]);
            sub_v3_v3v3(totmat[0], vec, vec2);
            normalize_v3(totmat[0]);

            /* the y axis is fixed */
            normalize_v3_v3(totmat[1], cob->matrix[1]);

            /* the z axis gets mapped onto a third orthogonal vector */
            cross_v3_v3v3(totmat[2], totmat[0], totmat[1]);
            break;
          }
          case TRACK_Z: /* LOCK Y TRACK Z */
          {
            /* Projection of Vector on the plane */
            project_v3_v3v3(vec2, vec, cob->matrix[1]);
            sub_v3_v3v3(totmat[2], vec, vec2);
            normalize_v3(totmat[2]);

            /* the y axis is fixed */
            normalize_v3_v3(totmat[1], cob->matrix[1]);

            /* the z axis gets mapped onto a third orthogonal vector */
            cross_v3_v3v3(totmat[0], totmat[1], totmat[2]);
            break;
          }
          case TRACK_nX: /* LOCK Y TRACK -X */
          {
            /* Projection of Vector on the plane */
            project_v3_v3v3(vec2, vec, cob->matrix[1]);
            sub_v3_v3v3(totmat[0], vec, vec2);
            normalize_v3(totmat[0]);
            negate_v3(totmat[0]);

            /* the y axis is fixed */
            normalize_v3_v3(totmat[1], cob->matrix[1]);

            /* the z axis gets mapped onto a third orthogonal vector */
            cross_v3_v3v3(totmat[2], totmat[0], totmat[1]);
            break;
          }
          case TRACK_nZ: /* LOCK Y TRACK -Z */
          {
            /* Projection of Vector on the plane */
            project_v3_v3v3(vec2, vec, cob->matrix[1]);
            sub_v3_v3v3(totmat[2], vec, vec2);
            normalize_v3(totmat[2]);
            negate_v3(totmat[2]);

            /* the y axis is fixed */
            normalize_v3_v3(totmat[1], cob->matrix[1]);

            /* the z axis gets mapped onto a third orthogonal vector */
            cross_v3_v3v3(totmat[0], totmat[1], totmat[2]);
            break;
          }
          default: {
            unit_m3(totmat);
            break;
          }
        }
        break;
      }
      case LOCK_Z: /* LOCK Z */
      {
        switch (data->trackflag) {
          case TRACK_X: /* LOCK Z TRACK X */
          {
            /* Projection of Vector on the plane */
            project_v3_v3v3(vec2, vec, cob->matrix[2]);
            sub_v3_v3v3(totmat[0], vec, vec2);
            normalize_v3(totmat[0]);

            /* the z axis is fixed */
            normalize_v3_v3(totmat[2], cob->matrix[2]);

            /* the x axis gets mapped onto a third orthogonal vector */
            cross_v3_v3v3(totmat[1], totmat[2], totmat[0]);
            break;
          }
          case TRACK_Y: /* LOCK Z TRACK Y */
          {
            /* Projection of Vector on the plane */
            project_v3_v3v3(vec2, vec, cob->matrix[2]);
            sub_v3_v3v3(totmat[1], vec, vec2);
            normalize_v3(totmat[1]);

            /* the z axis is fixed */
            normalize_v3_v3(totmat[2], cob->matrix[2]);

            /* the x axis gets mapped onto a third orthogonal vector */
            cross_v3_v3v3(totmat[0], totmat[1], totmat[2]);
            break;
          }
          case TRACK_nX: /* LOCK Z TRACK -X */
          {
            /* Projection of Vector on the plane */
            project_v3_v3v3(vec2, vec, cob->matrix[2]);
            sub_v3_v3v3(totmat[0], vec, vec2);
            normalize_v3(totmat[0]);
            negate_v3(totmat[0]);

            /* the z axis is fixed */
            normalize_v3_v3(totmat[2], cob->matrix[2]);

            /* the x axis gets mapped onto a third orthogonal vector */
            cross_v3_v3v3(totmat[1], totmat[2], totmat[0]);
            break;
          }
          case TRACK_nY: /* LOCK Z TRACK -Y */
          {
            /* Projection of Vector on the plane */
            project_v3_v3v3(vec2, vec, cob->matrix[2]);
            sub_v3_v3v3(totmat[1], vec, vec2);
            normalize_v3(totmat[1]);
            negate_v3(totmat[1]);

            /* the z axis is fixed */
            normalize_v3_v3(totmat[2], cob->matrix[2]);

            /* the x axis gets mapped onto a third orthogonal vector */
            cross_v3_v3v3(totmat[0], totmat[1], totmat[2]);
            break;
          }
          default: {
            unit_m3(totmat);
            break;
          }
        }
        break;
      }
      default: {
        unit_m3(totmat);
        break;
      }
    }
    /* Block to keep matrix heading */
    copy_m3_m4(tmpmat, cob->matrix);
    normalize_m3(tmpmat);
    invert_m3_m3(invmat, tmpmat);
    mul_m3_m3m3(tmpmat, totmat, invmat);
    totmat[0][0] = tmpmat[0][0];
    totmat[0][1] = tmpmat[0][1];
    totmat[0][2] = tmpmat[0][2];
    totmat[1][0] = tmpmat[1][0];
    totmat[1][1] = tmpmat[1][1];
    totmat[1][2] = tmpmat[1][2];
    totmat[2][0] = tmpmat[2][0];
    totmat[2][1] = tmpmat[2][1];
    totmat[2][2] = tmpmat[2][2];

    mdet = determinant_m3(totmat[0][0],
                          totmat[0][1],
                          totmat[0][2],
                          totmat[1][0],
                          totmat[1][1],
                          totmat[1][2],
                          totmat[2][0],
                          totmat[2][1],
                          totmat[2][2]);
    if (mdet == 0) {
      unit_m3(totmat);
    }

    /* apply out transformation to the object */
    mul_m4_m3m4(cob->matrix, totmat, cob->matrix);
  }
}

static bConstraintTypeInfo CTI_LOCKTRACK = {
    /*type*/ CONSTRAINT_TYPE_LOCKTRACK,
    /*size*/ sizeof(bLockTrackConstraint),
    /*name*/ N_("Locked Track"),
    /*struct_name*/ "bLockTrackConstraint",
    /*free_data*/ nullptr,
    /*id_looper*/ locktrack_id_looper,
    /*copy_data*/ nullptr,
    /*new_data*/ locktrack_new_data,
    /*get_constraint_targets*/ locktrack_get_tars,
    /*flush_constraint_targets*/ locktrack_flush_tars,
    /*get_target_matrix*/ default_get_tarmat,
    /*evaluate_constraint*/ locktrack_evaluate,
};

/* ---------- Limit Distance Constraint ----------- */

static void distlimit_new_data(void *cdata)
{
  bDistLimitConstraint *data = (bDistLimitConstraint *)cdata;

  data->dist = 0.0f;
}

static void distlimit_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
  bDistLimitConstraint *data = static_cast<bDistLimitConstraint *>(con->data);

  /* target only */
  func(con, (ID **)&data->tar, false, userdata);
}

static int distlimit_get_tars(bConstraint *con, ListBase *list)
{
  if (con && list) {
    bDistLimitConstraint *data = static_cast<bDistLimitConstraint *>(con->data);
    bConstraintTarget *ct;

    /* standard target-getting macro for single-target constraints */
    SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list);

    return 1;
  }

  return 0;
}

static void distlimit_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
  if (con && list) {
    bDistLimitConstraint *data = static_cast<bDistLimitConstraint *>(con->data);
    bConstraintTarget *ct = static_cast<bConstraintTarget *>(list->first);

    /* the following macro is used for all standard single-target constraints */
    SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, no_copy);
  }
}

static void distlimit_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
  bDistLimitConstraint *data = static_cast<bDistLimitConstraint *>(con->data);
  bConstraintTarget *ct = static_cast<bConstraintTarget *>(targets->first);

  /* only evaluate if there is a target */
  if (VALID_CONS_TARGET(ct)) {
    float dvec[3], dist, sfac = 1.0f;
    short clamp_surf = 0;

    /* calculate our current distance from the target */
    dist = len_v3v3(cob->matrix[3], ct->matrix[3]);

    /* set distance (flag is only set when user demands it) */
    if (data->dist == 0) {
      data->dist = dist;

      /* Write the computed distance back to the master copy if in copy-on-eval evaluation. */
      bConstraint *orig_con = constraint_find_original_for_update(cob, con);

      if (orig_con != nullptr) {
        bDistLimitConstraint *orig_data = static_cast<bDistLimitConstraint *>(orig_con->data);

        orig_data->dist = data->dist;
      }
    }

    /* check if we're which way to clamp from, and calculate interpolation factor (if needed) */
    if (data->mode == LIMITDIST_OUTSIDE) {
      /* if inside, then move to surface */
      if (dist <= data->dist) {
        clamp_surf = 1;
        if (dist != 0.0f) {
          sfac = data->dist / dist;
        }
      }
      /* if soft-distance is enabled, start fading once owner is dist+softdist from the target */
      else if (data->flag & LIMITDIST_USESOFT) {
        if (dist <= (data->dist + data->soft)) {
          /* pass */
        }
      }
    }
    else if (data->mode == LIMITDIST_INSIDE) {
      /* if outside, then move to surface */
      if (dist >= data->dist) {
        clamp_surf = 1;
        if (dist != 0.0f) {
          sfac = data->dist / dist;
        }
      }
      /* if soft-distance is enabled, start fading once owner is dist-soft from the target */
      else if (data->flag & LIMITDIST_USESOFT) {
        /* FIXME: there's a problem with "jumping" when this kicks in */
        if (dist >= (data->dist - data->soft)) {
          sfac = (data->soft * (1.0f - expf(-(dist - data->dist) / data->soft)) + data->dist);
          if (dist != 0.0f) {
            sfac /= dist;
          }

          clamp_surf = 1;
        }
      }
    }
    else {
      if (IS_EQF(dist, data->dist) == 0) {
        clamp_surf = 1;
        if (dist != 0.0f) {
          sfac = data->dist / dist;
        }
      }
    }

    /* clamp to 'surface' (i.e. move owner so that dist == data->dist) */
    if (clamp_surf) {
      /* simply interpolate along line formed by target -> owner */
      interp_v3_v3v3(dvec, ct->matrix[3], cob->matrix[3], sfac);

      /* copy new vector onto owner */
      copy_v3_v3(cob->matrix[3], dvec);
    }
  }
}

static bConstraintTypeInfo CTI_DISTLIMIT = {
    /*type*/ CONSTRAINT_TYPE_DISTLIMIT,
    /*size*/ sizeof(bDistLimitConstraint),
    /*name*/ N_("Limit Distance"),
    /*struct_name*/ "bDistLimitConstraint",
    /*free_data*/ nullptr,
    /*id_looper*/ distlimit_id_looper,
    /*copy_data*/ nullptr,
    /*new_data*/ distlimit_new_data,
    /*get_constraint_targets*/ distlimit_get_tars,
    /*flush_constraint_targets*/ distlimit_flush_tars,
    /*get_target_matrix*/ default_get_tarmat,
    /*evaluate_constraint*/ distlimit_evaluate,
};

/* ---------- Stretch To ------------ */

static void stretchto_new_data(void *cdata)
{
  bStretchToConstraint *data = (bStretchToConstraint *)cdata;

  data->volmode = 0;
  data->plane = SWING_Y;
  data->orglength = 0.0;
  data->bulge = 1.0;
  data->bulge_max = 1.0f;
  data->bulge_min = 1.0f;
}

static void stretchto_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
  bStretchToConstraint *data = static_cast<bStretchToConstraint *>(con->data);

  /* target only */
  func(con, (ID **)&data->tar, false, userdata);
}

static int stretchto_get_tars(bConstraint *con, ListBase *list)
{
  if (con && list) {
    bStretchToConstraint *data = static_cast<bStretchToConstraint *>(con->data);
    bConstraintTarget *ct;

    /* standard target-getting macro for single-target constraints */
    SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list);

    return 1;
  }

  return 0;
}

static void stretchto_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
  if (con && list) {
    bStretchToConstraint *data = static_cast<bStretchToConstraint *>(con->data);
    bConstraintTarget *ct = static_cast<bConstraintTarget *>(list->first);

    /* the following macro is used for all standard single-target constraints */
    SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, no_copy);
  }
}

static void stretchto_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
  bStretchToConstraint *data = static_cast<bStretchToConstraint *>(con->data);
  bConstraintTarget *ct = static_cast<bConstraintTarget *>(targets->first);

  /* only evaluate if there is a target */
  if (VALID_CONS_TARGET(ct)) {
    float size[3], scale[3], vec[3], xx[3], zz[3], orth[3];
    float dist, bulge;

    /* Remove shear if using the Damped Track mode; the other modes
     * do it as a side effect, which is relied on by rigs. */
    if (data->plane == SWING_Y) {
      orthogonalize_m4_stable(cob->matrix, 1, false);
    }

    /* store scaling before destroying obmat */
    normalize_m4_ex(cob->matrix, size);

    /* store X orientation before destroying obmat */
    copy_v3_v3(xx, cob->matrix[0]);

    /* store Z orientation before destroying obmat */
    copy_v3_v3(zz, cob->matrix[2]);

    /* Compute distance and direction to target. */
    sub_v3_v3v3(vec, ct->matrix[3], cob->matrix[3]);

    dist = normalize_v3(vec);

    /* Only Y constrained object axis scale should be used, to keep same length when scaling it.
     * Use safe divide to avoid creating a matrix with NAN values, see: #141612. */
    dist = blender::math::safe_divide(dist, size[1]);

    /* data->orglength==0 occurs on first run, and after 'R' button is clicked */
    if (data->orglength == 0) {
      data->orglength = dist;

      /* Write the computed length back to the master copy if in copy-on-eval evaluation. */
      bConstraint *orig_con = constraint_find_original_for_update(cob, con);

      if (orig_con != nullptr) {
        bStretchToConstraint *orig_data = static_cast<bStretchToConstraint *>(orig_con->data);

        orig_data->orglength = data->orglength;
      }
    }

    scale[1] = dist / data->orglength;

    bulge = powf(data->orglength / dist, data->bulge);

    if (bulge > 1.0f) {
      if (data->flag & STRETCHTOCON_USE_BULGE_MAX) {
        float bulge_max = max_ff(data->bulge_max, 1.0f);
        float hard = min_ff(bulge, bulge_max);

        float range = bulge_max - 1.0f;
        float scale_fac = (range > 0.0f) ? 1.0f / range : 0.0f;
        float soft = 1.0f + range * atanf((bulge - 1.0f) * scale_fac) / float(M_PI_2);

        bulge = interpf(soft, hard, data->bulge_smooth);
      }
    }
    if (bulge < 1.0f) {
      if (data->flag & STRETCHTOCON_USE_BULGE_MIN) {
        float bulge_min = std::clamp(data->bulge_min, 0.0f, 1.0f);
        float hard = max_ff(bulge, bulge_min);

        float range = 1.0f - bulge_min;
        float scale_fac = (range > 0.0f) ? 1.0f / range : 0.0f;
        float soft = 1.0f - range * atanf((1.0f - bulge) * scale_fac) / float(M_PI_2);

        bulge = interpf(soft, hard, data->bulge_smooth);
      }
    }

    switch (data->volmode) {
      /* volume preserving scaling */
      case VOLUME_XZ:
        scale[0] = sqrtf(bulge);
        scale[2] = scale[0];
        break;
      case VOLUME_X:
        scale[0] = bulge;
        scale[2] = 1.0;
        break;
      case VOLUME_Z:
        scale[0] = 1.0;
        scale[2] = bulge;
        break;
      /* don't care for volume */
      case NO_VOLUME:
        scale[0] = 1.0;
        scale[2] = 1.0;
        break;
      default: /* Should not happen, but in case. */
        return;
    } /* switch (data->volmode) */

    /* Compute final scale. */
    mul_v3_v3(size, scale);

    switch (data->plane) {
      case SWING_Y:
        /* Point the Y axis using Damped Track math. */
        damptrack_do_transform(cob->matrix, vec, TRACK_Y);
        break;
      case PLANE_X:
        /* New Y aligns object target connection. */
        copy_v3_v3(cob->matrix[1], vec);

        /* Build new Z vector. */
        /* Orthogonal to "new Y" "old X! plane. */
        cross_v3_v3v3(orth, xx, vec);
        normalize_v3(orth);

        /* New Z. */
        copy_v3_v3(cob->matrix[2], orth);

        /* We decided to keep X plane. */
        cross_v3_v3v3(xx, vec, orth);
        normalize_v3_v3(cob->matrix[0], xx);
        break;
      case PLANE_Z:
        /* New Y aligns object target connection. */
        copy_v3_v3(cob->matrix[1], vec);

        /* Build new X vector. */
        /* Orthogonal to "new Y" "old Z! plane. */
        cross_v3_v3v3(orth, zz, vec);
        normalize_v3(orth);

        /* New X. */
        negate_v3_v3(cob->matrix[0], orth);

        /* We decided to keep Z. */
        cross_v3_v3v3(zz, vec, orth);
        normalize_v3_v3(cob->matrix[2], zz);
        break;
    } /* switch (data->plane) */

    rescale_m4(cob->matrix, size);
  }
}

static bConstraintTypeInfo CTI_STRETCHTO = {
    /*type*/ CONSTRAINT_TYPE_STRETCHTO,
    /*size*/ sizeof(bStretchToConstraint),
    /*name*/ N_("Stretch To"),
    /*struct_name*/ "bStretchToConstraint",
    /*free_data*/ nullptr,
    /*id_looper*/ stretchto_id_looper,
    /*copy_data*/ nullptr,
    /*new_data*/ stretchto_new_data,
    /*get_constraint_targets*/ stretchto_get_tars,
    /*flush_constraint_targets*/ stretchto_flush_tars,
    /*get_target_matrix*/ default_get_tarmat,
    /*evaluate_constraint*/ stretchto_evaluate,
};

/* ---------- Floor ------------ */

static void minmax_new_data(void *cdata)
{
  bMinMaxConstraint *data = (bMinMaxConstraint *)cdata;

  data->minmaxflag = TRACK_Z;
  data->offset = 0.0f;
  data->flag = 0;
}

static void minmax_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
  bMinMaxConstraint *data = static_cast<bMinMaxConstraint *>(con->data);

  /* target only */
  func(con, (ID **)&data->tar, false, userdata);
}

static int minmax_get_tars(bConstraint *con, ListBase *list)
{
  if (con && list) {
    bMinMaxConstraint *data = static_cast<bMinMaxConstraint *>(con->data);
    bConstraintTarget *ct;

    /* standard target-getting macro for single-target constraints */
    SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list);

    return 1;
  }

  return 0;
}

static void minmax_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
  if (con && list) {
    bMinMaxConstraint *data = static_cast<bMinMaxConstraint *>(con->data);
    bConstraintTarget *ct = static_cast<bConstraintTarget *>(list->first);

    /* the following macro is used for all standard single-target constraints */
    SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, no_copy);
  }
}

static void minmax_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
  bMinMaxConstraint *data = static_cast<bMinMaxConstraint *>(con->data);
  bConstraintTarget *ct = static_cast<bConstraintTarget *>(targets->first);

  /* only evaluate if there is a target */
  if (VALID_CONS_TARGET(ct)) {
    float obmat[4][4], imat[4][4], tarmat[4][4], tmat[4][4];
    float val1, val2;
    int index;

    copy_m4_m4(obmat, cob->matrix);
    copy_m4_m4(tarmat, ct->matrix);

    if (data->flag & MINMAX_USEROT) {
      /* Take rotation of target into account by doing the transaction in target's local-space. */
      invert_m4_m4(imat, tarmat);
      mul_m4_m4m4(tmat, imat, obmat);
      copy_m4_m4(obmat, tmat);
      unit_m4(tarmat);
    }

    switch (data->minmaxflag) {
      case TRACK_Z:
        val1 = tarmat[3][2];
        val2 = obmat[3][2] - data->offset;
        index = 2;
        break;
      case TRACK_Y:
        val1 = tarmat[3][1];
        val2 = obmat[3][1] - data->offset;
        index = 1;
        break;
      case TRACK_X:
        val1 = tarmat[3][0];
        val2 = obmat[3][0] - data->offset;
        index = 0;
        break;
      case TRACK_nZ:
        val2 = tarmat[3][2];
        val1 = obmat[3][2] - data->offset;
        index = 2;
        break;
      case TRACK_nY:
        val2 = tarmat[3][1];
        val1 = obmat[3][1] - data->offset;
        index = 1;
        break;
      case TRACK_nX:
        val2 = tarmat[3][0];
        val1 = obmat[3][0] - data->offset;
        index = 0;
        break;
      default:
        return;
    }

    if (val1 > val2) {
      obmat[3][index] = tarmat[3][index] + data->offset;
      if (data->flag & MINMAX_USEROT) {
        /* Get out of local-space. */
        mul_m4_m4m4(tmat, ct->matrix, obmat);
        copy_m4_m4(cob->matrix, tmat);
      }
      else {
        copy_v3_v3(cob->matrix[3], obmat[3]);
      }
    }
  }
}

static bConstraintTypeInfo CTI_MINMAX = {
    /*type*/ CONSTRAINT_TYPE_MINMAX,
    /*size*/ sizeof(bMinMaxConstraint),
    /*name*/ N_("Floor"),
    /*struct_name*/ "bMinMaxConstraint",
    /*free_data*/ nullptr,
    /*id_looper*/ minmax_id_looper,
    /*copy_data*/ nullptr,
    /*new_data*/ minmax_new_data,
    /*get_constraint_targets*/ minmax_get_tars,
    /*flush_constraint_targets*/ minmax_flush_tars,
    /*get_target_matrix*/ default_get_tarmat,
    /*evaluate_constraint*/ minmax_evaluate,
};

/* -------- Clamp To ---------- */

static void clampto_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
  bClampToConstraint *data = static_cast<bClampToConstraint *>(con->data);

  /* target only */
  func(con, (ID **)&data->tar, false, userdata);
}

static int clampto_get_tars(bConstraint *con, ListBase *list)
{
  if (con && list) {
    bClampToConstraint *data = static_cast<bClampToConstraint *>(con->data);
    bConstraintTarget *ct;

    /* Standard target-getting macro for single-target constraints without sub-targets. */
    SINGLETARGETNS_GET_TARS(con, data->tar, ct, list);

    return 1;
  }

  return 0;
}

static void clampto_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
  if (con && list) {
    bClampToConstraint *data = static_cast<bClampToConstraint *>(con->data);
    bConstraintTarget *ct = static_cast<bConstraintTarget *>(list->first);

    /* the following macro is used for all standard single-target constraints */
    SINGLETARGETNS_FLUSH_TARS(con, data->tar, ct, list, no_copy);
  }
}

static bool clampto_get_tarmat(Depsgraph * /*depsgraph*/,
                               bConstraint * /*con*/,
                               bConstraintOb * /*cob*/,
                               bConstraintTarget *ct,
                               float /*ctime*/)
{
  /* technically, this isn't really needed for evaluation, but we don't know what else
   * might end up calling this...
   */
  unit_ct_matrix_nullsafe(ct);
  return false;
}

static void clampto_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
  using namespace blender;
  bClampToConstraint *data = static_cast<bClampToConstraint *>(con->data);
  bConstraintTarget *ct = static_cast<bConstraintTarget *>(targets->first);

  /* only evaluate if there is a target and it is a curve */
  if (VALID_CONS_TARGET(ct) && (ct->tar->type == OB_CURVES_LEGACY)) {
    float obmat[4][4], ownLoc[3];
    float curveMin[3], curveMax[3];
    float targetMatrix[4][4];

    copy_m4_m4(obmat, cob->matrix);
    copy_v3_v3(ownLoc, obmat[3]);

    unit_m4(targetMatrix);
    INIT_MINMAX(curveMin, curveMax);
    if (const std::optional<Bounds<float3>> bounds = BKE_object_boundbox_get(ct->tar)) {
      copy_v3_v3(curveMin, bounds->min);
      copy_v3_v3(curveMax, bounds->max);
    }

    /* Get target-matrix. */
    if (data->tar->runtime->curve_cache && data->tar->runtime->curve_cache->anim_path_accum_length)
    {
      float vec[4], totmat[4][4];
      float curvetime;
      short clamp_axis;

      /* find best position on curve */
      /* 1. determine which axis to sample on? */
      if (data->flag == CLAMPTO_AUTO) {
        float size[3];
        sub_v3_v3v3(size, curveMax, curveMin);

        /* find axis along which the bounding box has the greatest
         * extent. Otherwise, default to the x-axis, as that is quite
         * frequently used.
         */
        if ((size[2] > size[0]) && (size[2] > size[1])) {
          clamp_axis = CLAMPTO_Z - 1;
        }
        else if ((size[1] > size[0]) && (size[1] > size[2])) {
          clamp_axis = CLAMPTO_Y - 1;
        }
        else {
          clamp_axis = CLAMPTO_X - 1;
        }
      }
      else {
        clamp_axis = data->flag - 1;
      }

      /* 2. determine position relative to curve on a 0-1 scale based on bounding box */
      if (data->flag2 & CLAMPTO_CYCLIC) {
        /* cyclic, so offset within relative bounding box is used */
        float len = (curveMax[clamp_axis] - curveMin[clamp_axis]);
        float offset;

        /* check to make sure len is not so close to zero that it'll cause errors */
        if (IS_EQF(len, 0.0f) == false) {
          /* find bounding-box range where target is located */
          if (ownLoc[clamp_axis] < curveMin[clamp_axis]) {
            /* bounding-box range is before */
            offset = curveMin[clamp_axis] -
                     ceilf((curveMin[clamp_axis] - ownLoc[clamp_axis]) / len) * len;

            /* Now, we calculate as per normal,
             * except using offset instead of curveMin[clamp_axis]. */
            curvetime = (ownLoc[clamp_axis] - offset) / (len);
          }
          else if (ownLoc[clamp_axis] > curveMax[clamp_axis]) {
            /* bounding-box range is after */
            offset = curveMax[clamp_axis] +
                     int((ownLoc[clamp_axis] - curveMax[clamp_axis]) / len) * len;

            /* Now, we calculate as per normal,
             * except using offset instead of curveMax[clamp_axis]. */
            curvetime = (ownLoc[clamp_axis] - offset) / (len);
          }
          else {
            /* as the location falls within bounds, just calculate */
            curvetime = (ownLoc[clamp_axis] - curveMin[clamp_axis]) / (len);
          }
        }
        else {
          /* as length is close to zero, curvetime by default should be 0 (i.e. the start) */
          curvetime = 0.0f;
        }
      }
      else {
        /* no cyclic, so position is clamped to within the bounding box */
        if (ownLoc[clamp_axis] <= curveMin[clamp_axis]) {
          curvetime = 0.0f;
        }
        else if (ownLoc[clamp_axis] >= curveMax[clamp_axis]) {
          curvetime = 1.0f;
        }
        else if (IS_EQF((curveMax[clamp_axis] - curveMin[clamp_axis]), 0.0f) == false) {
          curvetime = (ownLoc[clamp_axis] - curveMin[clamp_axis]) /
                      (curveMax[clamp_axis] - curveMin[clamp_axis]);
        }
        else {
          curvetime = 0.0f;
        }
      }

      /* 3. position on curve */
      if (BKE_where_on_path(ct->tar, curvetime, vec, nullptr, nullptr, nullptr, nullptr)) {
        unit_m4(totmat);
        copy_v3_v3(totmat[3], vec);

        mul_m4_m4m4(targetMatrix, ct->tar->object_to_world().ptr(), totmat);
      }
    }

    /* obtain final object position */
    copy_v3_v3(cob->matrix[3], targetMatrix[3]);
  }
}

static bConstraintTypeInfo CTI_CLAMPTO = {
    /*type*/ CONSTRAINT_TYPE_CLAMPTO,
    /*size*/ sizeof(bClampToConstraint),
    /*name*/ N_("Clamp To"),
    /*struct_name*/ "bClampToConstraint",
    /*free_data*/ nullptr,
    /*id_looper*/ clampto_id_looper,
    /*copy_data*/ nullptr,
    /*new_data*/ nullptr,
    /*get_constraint_targets*/ clampto_get_tars,
    /*flush_constraint_targets*/ clampto_flush_tars,
    /*get_target_matrix*/ clampto_get_tarmat,
    /*evaluate_constraint*/ clampto_evaluate,
};

/* ---------- Transform Constraint ----------- */

static void transform_new_data(void *cdata)
{
  bTransformConstraint *data = (bTransformConstraint *)cdata;

  data->map[0] = 0;
  data->map[1] = 1;
  data->map[2] = 2;

  for (int i = 0; i < 3; i++) {
    data->from_min_scale[i] = data->from_max_scale[i] = 1.0f;
    data->to_min_scale[i] = data->to_max_scale[i] = 1.0f;
  }
}

static void transform_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
  bTransformConstraint *data = static_cast<bTransformConstraint *>(con->data);

  /* target only */
  func(con, (ID **)&data->tar, false, userdata);
}

static int transform_get_tars(bConstraint *con, ListBase *list)
{
  if (con && list) {
    bTransformConstraint *data = static_cast<bTransformConstraint *>(con->data);
    bConstraintTarget *ct;

    /* standard target-getting macro for single-target constraints */
    SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list);

    return 1;
  }

  return 0;
}

static void transform_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
  if (con && list) {
    bTransformConstraint *data = static_cast<bTransformConstraint *>(con->data);
    bConstraintTarget *ct = static_cast<bConstraintTarget *>(list->first);

    /* the following macro is used for all standard single-target constraints */
    SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, no_copy);
  }
}

static void transform_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
  bTransformConstraint *data = static_cast<bTransformConstraint *>(con->data);
  bConstraintTarget *ct = static_cast<bConstraintTarget *>(targets->first);

  /* only evaluate if there is a target */
  if (VALID_CONS_TARGET(ct)) {
    float *from_min, *from_max, *to_min, *to_max;
    float loc[3], rot[3][3], oldeul[3], size[3];
    float newloc[3], newrot[3][3], neweul[3], newsize[3];
    float dbuf[4], sval[3];
    float *const dvec = dbuf + 1;

    /* obtain target effect */
    switch (data->from) {
      case TRANS_SCALE:
        mat4_to_size(dvec, ct->matrix);

        if (is_negative_m4(ct->matrix)) {
          /* Bug-fix #27886: (this is a limitation that riggers will have to live with for now).
           * We can't be sure which axis/axes are negative,
           * though we know that something is negative.
           * Assume we don't care about negativity of separate axes. */
          negate_v3(dvec);
        }
        from_min = data->from_min_scale;
        from_max = data->from_max_scale;
        break;
      case TRANS_ROTATION:
        BKE_driver_target_matrix_to_rot_channels(
            ct->matrix, cob->rotOrder, data->from_rotation_mode, -1, true, dbuf);
        from_min = data->from_min_rot;
        from_max = data->from_max_rot;
        break;
      case TRANS_LOCATION:
      default:
        copy_v3_v3(dvec, ct->matrix[3]);
        from_min = data->from_min;
        from_max = data->from_max;
        break;
    }

    /* Select the output Euler rotation order, defaulting to the owner. */
    short rot_order = cob->rotOrder;

    if (data->to == TRANS_ROTATION && data->to_euler_order != CONSTRAINT_EULER_AUTO) {
      rot_order = data->to_euler_order;
    }

    /* extract components of owner's matrix */
    mat4_to_loc_rot_size(loc, rot, size, cob->matrix);

    /* determine where in range current transforms lie */
    if (data->expo) {
      for (int i = 0; i < 3; i++) {
        if (from_max[i] - from_min[i]) {
          sval[i] = (dvec[i] - from_min[i]) / (from_max[i] - from_min[i]);
        }
        else {
          sval[i] = 0.0f;
        }
      }
    }
    else {
      /* clamp transforms out of range */
      for (int i = 0; i < 3; i++) {
        CLAMP(dvec[i], from_min[i], from_max[i]);
        if (from_max[i] - from_min[i]) {
          sval[i] = (dvec[i] - from_min[i]) / (from_max[i] - from_min[i]);
        }
        else {
          sval[i] = 0.0f;
        }
      }
    }

    /* apply transforms */
    switch (data->to) {
      case TRANS_SCALE:
        to_min = data->to_min_scale;
        to_max = data->to_max_scale;
        for (int i = 0; i < 3; i++) {
          newsize[i] = to_min[i] + (sval[int(data->map[i])] * (to_max[i] - to_min[i]));
        }
        switch (data->mix_mode_scale) {
          case TRANS_MIXSCALE_MULTIPLY:
            mul_v3_v3(size, newsize);
            break;
          case TRANS_MIXSCALE_REPLACE:
          default:
            copy_v3_v3(size, newsize);
            break;
        }
        break;
      case TRANS_ROTATION:
        to_min = data->to_min_rot;
        to_max = data->to_max_rot;
        for (int i = 0; i < 3; i++) {
          neweul[i] = to_min[i] + (sval[int(data->map[i])] * (to_max[i] - to_min[i]));
        }
        switch (data->mix_mode_rot) {
          case TRANS_MIXROT_REPLACE:
            eulO_to_mat3(rot, neweul, rot_order);
            break;
          case TRANS_MIXROT_BEFORE:
            eulO_to_mat3(newrot, neweul, rot_order);
            mul_m3_m3m3(rot, newrot, rot);
            break;
          case TRANS_MIXROT_AFTER:
            eulO_to_mat3(newrot, neweul, rot_order);
            mul_m3_m3m3(rot, rot, newrot);
            break;
          case TRANS_MIXROT_ADD:
          default:
            mat3_to_eulO(oldeul, rot_order, rot);
            add_v3_v3(neweul, oldeul);
            eulO_to_mat3(rot, neweul, rot_order);
            break;
        }
        break;
      case TRANS_LOCATION:
      default:
        to_min = data->to_min;
        to_max = data->to_max;
        for (int i = 0; i < 3; i++) {
          newloc[i] = (to_min[i] + (sval[int(data->map[i])] * (to_max[i] - to_min[i])));
        }
        switch (data->mix_mode_loc) {
          case TRANS_MIXLOC_REPLACE:
            copy_v3_v3(loc, newloc);
            break;
          case TRANS_MIXLOC_ADD:
          default:
            add_v3_v3(loc, newloc);
            break;
        }
        break;
    }

    /* apply to matrix */
    loc_rot_size_to_mat4(cob->matrix, loc, rot, size);
  }
}

static bConstraintTypeInfo CTI_TRANSFORM = {
    /*type*/ CONSTRAINT_TYPE_TRANSFORM,
    /*size*/ sizeof(bTransformConstraint),
    /*name*/ N_("Transformation"),
    /*struct_name*/ "bTransformConstraint",
    /*free_data*/ nullptr,
    /*id_looper*/ transform_id_looper,
    /*copy_data*/ nullptr,
    /*new_data*/ transform_new_data,
    /*get_constraint_targets*/ transform_get_tars,
    /*flush_constraint_targets*/ transform_flush_tars,
    /*get_target_matrix*/ default_get_tarmat,
    /*evaluate_constraint*/ transform_evaluate,
};

/* ---------- Shrinkwrap Constraint ----------- */

static void shrinkwrap_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
  bShrinkwrapConstraint *data = static_cast<bShrinkwrapConstraint *>(con->data);

  /* target only */
  func(con, (ID **)&data->target, false, userdata);
}

static void shrinkwrap_new_data(void *cdata)
{
  bShrinkwrapConstraint *data = (bShrinkwrapConstraint *)cdata;

  data->projAxis = OB_POSZ;
  data->projAxisSpace = CONSTRAINT_SPACE_LOCAL;
}

static int shrinkwrap_get_tars(bConstraint *con, ListBase *list)
{
  if (con && list) {
    bShrinkwrapConstraint *data = static_cast<bShrinkwrapConstraint *>(con->data);
    bConstraintTarget *ct;

    SINGLETARGETNS_GET_TARS(con, data->target, ct, list);

    return 1;
  }

  return 0;
}

static void shrinkwrap_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
  if (con && list) {
    bShrinkwrapConstraint *data = static_cast<bShrinkwrapConstraint *>(con->data);
    bConstraintTarget *ct = static_cast<bConstraintTarget *>(list->first);

    SINGLETARGETNS_FLUSH_TARS(con, data->target, ct, list, no_copy);
  }
}

static bool shrinkwrap_get_tarmat(Depsgraph * /*depsgraph*/,
                                  bConstraint *con,
                                  bConstraintOb *cob,
                                  bConstraintTarget *ct,
                                  float /*ctime*/)
{
  bShrinkwrapConstraint *scon = (bShrinkwrapConstraint *)con->data;

  if (!VALID_CONS_TARGET(ct) || ct->tar->type != OB_MESH) {
    return false;
  }

  bool fail = false;
  float co[3] = {0.0f, 0.0f, 0.0f};
  bool track_normal = false;
  float track_no[3] = {0.0f, 0.0f, 0.0f};

  SpaceTransform transform;
  Mesh *target_eval = BKE_object_get_evaluated_mesh(ct->tar);

  copy_m4_m4(ct->matrix, cob->matrix);

  bool do_track_normal = (scon->flag & CON_SHRINKWRAP_TRACK_NORMAL) != 0;
  ShrinkwrapTreeData tree;

  if (!BKE_shrinkwrap_init_tree(
          &tree, target_eval, scon->shrinkType, scon->shrinkMode, do_track_normal))
  {
    return false;
  }

  BLI_space_transform_from_matrices(&transform, cob->matrix, ct->tar->object_to_world().ptr());

  switch (scon->shrinkType) {
    case MOD_SHRINKWRAP_NEAREST_SURFACE:
    case MOD_SHRINKWRAP_NEAREST_VERTEX:
    case MOD_SHRINKWRAP_TARGET_PROJECT: {
      BVHTreeNearest nearest;

      nearest.index = -1;
      nearest.dist_sq = FLT_MAX;

      BLI_space_transform_apply(&transform, co);

      BKE_shrinkwrap_find_nearest_surface(&tree, &nearest, co, scon->shrinkType);

      if (nearest.index < 0) {
        fail = true;
        break;
      }

      if (scon->shrinkType != MOD_SHRINKWRAP_NEAREST_VERTEX) {
        if (do_track_normal) {
          track_normal = true;
          BKE_shrinkwrap_compute_smooth_normal(
              &tree, nullptr, nearest.index, nearest.co, nearest.no, track_no);
          BLI_space_transform_invert_normal(&transform, track_no);
        }

        BKE_shrinkwrap_snap_point_to_surface(&tree,
                                             nullptr,
                                             scon->shrinkMode,
                                             nearest.index,
                                             nearest.co,
                                             nearest.no,
                                             scon->dist,
                                             co,
                                             co);
      }
      else {
        const float dist = len_v3v3(co, nearest.co);

        if (dist != 0.0f) {
          interp_v3_v3v3(
              co, co, nearest.co, (dist - scon->dist) / dist); /* linear interpolation */
        }
      }

      BLI_space_transform_invert(&transform, co);
      break;
    }
    case MOD_SHRINKWRAP_PROJECT: {
      BVHTreeRayHit hit;

      float mat[4][4];
      float no[3] = {0.0f, 0.0f, 0.0f};

      /* TODO: should use FLT_MAX.. but normal projection doesn't yet supports it. */
      hit.index = -1;
      hit.dist = (scon->projLimit == 0.0f) ? BVH_RAYCAST_DIST_MAX : scon->projLimit;

      switch (scon->projAxis) {
        case OB_POSX:
        case OB_POSY:
        case OB_POSZ:
          no[scon->projAxis - OB_POSX] = 1.0f;
          break;
        case OB_NEGX:
        case OB_NEGY:
        case OB_NEGZ:
          no[scon->projAxis - OB_NEGX] = -1.0f;
          break;
      }

      /* Transform normal into requested space */
      /* Note that in this specific case, we need to keep scaling in non-parented 'local2world'
       * object case, because SpaceTransform also takes it into account when handling normals.
       * See #42447. */
      unit_m4(mat);
      BKE_constraint_mat_convertspace(
          cob->ob, cob->pchan, cob, mat, CONSTRAINT_SPACE_LOCAL, scon->projAxisSpace, true);
      invert_m4(mat);
      mul_mat3_m4_v3(mat, no);

      if (normalize_v3(no) < FLT_EPSILON) {
        fail = true;
        break;
      }

      char cull_mode = scon->flag & CON_SHRINKWRAP_PROJECT_CULL_MASK;

      BKE_shrinkwrap_project_normal(cull_mode, co, no, 0.0f, &transform, &tree, &hit);

      if (scon->flag & CON_SHRINKWRAP_PROJECT_OPPOSITE) {
        float inv_no[3];
        negate_v3_v3(inv_no, no);

        if ((scon->flag & CON_SHRINKWRAP_PROJECT_INVERT_CULL) && (cull_mode != 0)) {
          cull_mode ^= CON_SHRINKWRAP_PROJECT_CULL_MASK;
        }

        BKE_shrinkwrap_project_normal(cull_mode, co, inv_no, 0.0f, &transform, &tree, &hit);
      }

      if (hit.index < 0) {
        fail = true;
        break;
      }

      if (do_track_normal) {
        track_normal = true;
        BKE_shrinkwrap_compute_smooth_normal(
            &tree, &transform, hit.index, hit.co, hit.no, track_no);
      }

      BKE_shrinkwrap_snap_point_to_surface(
          &tree, &transform, scon->shrinkMode, hit.index, hit.co, hit.no, scon->dist, co, co);
      break;
    }
  }

  BKE_shrinkwrap_free_tree(&tree);

  if (fail) {
    /* Don't move the point */
    zero_v3(co);
  }

  /* co is in local object coordinates, change it to global and update target position */
  mul_m4_v3(cob->matrix, co);
  copy_v3_v3(ct->matrix[3], co);

  if (track_normal) {
    mul_mat3_m4_v3(cob->matrix, track_no);
    damptrack_do_transform(ct->matrix, track_no, scon->trackAxis);
  }

  return true;
}

static void shrinkwrap_evaluate(bConstraint * /*con*/, bConstraintOb *cob, ListBase *targets)
{
  bConstraintTarget *ct = static_cast<bConstraintTarget *>(targets->first);

  /* only evaluate if there is a target */
  if (VALID_CONS_TARGET(ct)) {
    copy_m4_m4(cob->matrix, ct->matrix);
  }
}

static bConstraintTypeInfo CTI_SHRINKWRAP = {
    /*type*/ CONSTRAINT_TYPE_SHRINKWRAP,
    /*size*/ sizeof(bShrinkwrapConstraint),
    /*name*/ N_("Shrinkwrap"),
    /*struct_name*/ "bShrinkwrapConstraint",
    /*free_data*/ nullptr,
    /*id_looper*/ shrinkwrap_id_looper,
    /*copy_data*/ nullptr,
    /*new_data*/ shrinkwrap_new_data,
    /*get_constraint_targets*/ shrinkwrap_get_tars,
    /*flush_constraint_targets*/ shrinkwrap_flush_tars,
    /*get_target_matrix*/ shrinkwrap_get_tarmat,
    /*evaluate_constraint*/ shrinkwrap_evaluate,
};

/* --------- Damped Track ---------- */

static void damptrack_new_data(void *cdata)
{
  bDampTrackConstraint *data = (bDampTrackConstraint *)cdata;

  data->trackflag = TRACK_Y;
}

static void damptrack_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
  bDampTrackConstraint *data = static_cast<bDampTrackConstraint *>(con->data);

  /* target only */
  func(con, (ID **)&data->tar, false, userdata);
}

static int damptrack_get_tars(bConstraint *con, ListBase *list)
{
  if (con && list) {
    bDampTrackConstraint *data = static_cast<bDampTrackConstraint *>(con->data);
    bConstraintTarget *ct;

    /* the following macro is used for all standard single-target constraints */
    SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list);

    return 1;
  }

  return 0;
}

static void damptrack_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
  if (con && list) {
    bDampTrackConstraint *data = static_cast<bDampTrackConstraint *>(con->data);
    bConstraintTarget *ct = static_cast<bConstraintTarget *>(list->first);

    /* the following macro is used for all standard single-target constraints */
    SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, no_copy);
  }
}

/* array of direction vectors for the tracking flags */
static const float track_dir_vecs[6][3] = {
    {+1, 0, 0},
    {0, +1, 0},
    {0, 0, +1}, /* TRACK_X,  TRACK_Y,  TRACK_Z */
    {-1, 0, 0},
    {0, -1, 0},
    {0, 0, -1} /* TRACK_NX, TRACK_NY, TRACK_NZ */
};

static void damptrack_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
  bDampTrackConstraint *data = static_cast<bDampTrackConstraint *>(con->data);
  bConstraintTarget *ct = static_cast<bConstraintTarget *>(targets->first);

  if (VALID_CONS_TARGET(ct)) {
    float tarvec[3];

    /* find the (unit) direction vector going from the owner to the target */
    sub_v3_v3v3(tarvec, ct->matrix[3], cob->matrix[3]);

    damptrack_do_transform(cob->matrix, tarvec, data->trackflag);
  }
}

static void damptrack_do_transform(float matrix[4][4], const float tarvec_in[3], int track_axis)
{
  using namespace blender;
  /* find the (unit) direction vector going from the owner to the target */
  float3 tarvec;

  if (normalize_v3_v3(tarvec, tarvec_in) != 0.0f) {
    float3 obvec, obloc;
    float3 raxis;
    float rangle;
    float rmat[3][3], tmat[4][4];

    /* find the (unit) direction that the axis we're interested in currently points
     * - mul_mat3_m4_v3() only takes the 3x3 (rotation+scaling) components of the 4x4 matrix
     * - the normalization step at the end should take care of any unwanted scaling
     *   left over in the 3x3 matrix we used
     */
    copy_v3_v3(obvec, track_dir_vecs[track_axis]);
    mul_mat3_m4_v3(matrix, obvec);

    if (normalize_v3(obvec) == 0.0f) {
      /* exceptional case - just use the track vector as appropriate */
      copy_v3_v3(obvec, track_dir_vecs[track_axis]);
    }

    copy_v3_v3(obloc, matrix[3]);

    /* determine the axis-angle rotation, which represents the smallest possible rotation
     * between the two rotation vectors (i.e. the 'damping' referred to in the name)
     * - we take this to be the rotation around the normal axis/vector to the plane defined
     *   by the current and destination vectors, which will 'map' the current axis to the
     *   destination vector
     * - the min/max wrappers around (obvec . tarvec) result (stored temporarily in rangle)
     *   are used to ensure that the smallest angle is chosen
     */
    raxis = math::cross_high_precision(obvec, tarvec);

    rangle = dot_v3v3(obvec, tarvec);
    rangle = acosf(max_ff(-1.0f, min_ff(1.0f, rangle)));

    /* construct rotation matrix from the axis-angle rotation found above
     * - this call takes care to make sure that the axis provided is a unit vector first
     */
    float norm = normalize_v3(raxis);

    if (norm < FLT_EPSILON) {
      /* if dot product is nonzero, while cross is zero, we have two opposite vectors!
       * - this is an ambiguity in the math that needs to be resolved arbitrarily,
       *   or there will be a case where damped track strangely does nothing
       * - to do that, rotate around a different local axis
       */
      float tmpvec[3];

      if (fabsf(rangle) < M_PI - 0.01f) {
        return;
      }

      rangle = M_PI;
      copy_v3_v3(tmpvec, track_dir_vecs[(track_axis + 1) % 6]);
      mul_mat3_m4_v3(matrix, tmpvec);
      cross_v3_v3v3(raxis, obvec, tmpvec);

      if (normalize_v3(raxis) == 0.0f) {
        return;
      }
    }
    else if (norm < 0.1f) {
      /* Near 0 and Pi `arcsin` has way better precision than `arccos`. */
      rangle = (rangle > M_PI_2) ? M_PI - asinf(norm) : asinf(norm);
    }

    axis_angle_normalized_to_mat3(rmat, raxis, rangle);

    /* rotate the owner in the way defined by this rotation matrix, then reapply the location since
     * we may have destroyed that in the process of multiplying the matrix
     */
    unit_m4(tmat);
    mul_m4_m3m4(tmat, rmat, matrix); /* m1, m3, m2 */

    copy_m4_m4(matrix, tmat);
    copy_v3_v3(matrix[3], obloc);
  }
}

static bConstraintTypeInfo CTI_DAMPTRACK = {
    /*type*/ CONSTRAINT_TYPE_DAMPTRACK,
    /*size*/ sizeof(bDampTrackConstraint),
    /*name*/ N_("Damped Track"),
    /*struct_name*/ "bDampTrackConstraint",
    /*free_data*/ nullptr,
    /*id_looper*/ damptrack_id_looper,
    /*copy_data*/ nullptr,
    /*new_data*/ damptrack_new_data,
    /*get_constraint_targets*/ damptrack_get_tars,
    /*flush_constraint_targets*/ damptrack_flush_tars,
    /*get_target_matrix*/ default_get_tarmat,
    /*evaluate_constraint*/ damptrack_evaluate,
};

/* ----------- Spline IK ------------ */

static void splineik_free(bConstraint *con)
{
  bSplineIKConstraint *data = static_cast<bSplineIKConstraint *>(con->data);

  /* binding array */
  MEM_SAFE_FREE(data->points);
}

static void splineik_copy(bConstraint *con, bConstraint *srccon)
{
  bSplineIKConstraint *src = static_cast<bSplineIKConstraint *>(srccon->data);
  bSplineIKConstraint *dst = static_cast<bSplineIKConstraint *>(con->data);

  /* copy the binding array */
  dst->points = static_cast<float *>(MEM_dupallocN(src->points));
}

static void splineik_new_data(void *cdata)
{
  bSplineIKConstraint *data = (bSplineIKConstraint *)cdata;

  data->chainlen = 1;
  data->bulge = 1.0;
  data->bulge_max = 1.0f;
  data->bulge_min = 1.0f;

  data->yScaleMode = CONSTRAINT_SPLINEIK_YS_FIT_CURVE;
  data->flag = CONSTRAINT_SPLINEIK_USE_ORIGINAL_SCALE;
}

static void splineik_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
  bSplineIKConstraint *data = static_cast<bSplineIKConstraint *>(con->data);

  /* target only */
  func(con, (ID **)&data->tar, false, userdata);
}

static int splineik_get_tars(bConstraint *con, ListBase *list)
{
  if (con && list) {
    bSplineIKConstraint *data = static_cast<bSplineIKConstraint *>(con->data);
    bConstraintTarget *ct;

    /* Standard target-getting macro for single-target constraints without sub-targets. */
    SINGLETARGETNS_GET_TARS(con, data->tar, ct, list);

    return 1;
  }

  return 0;
}

static void splineik_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
  if (con && list) {
    bSplineIKConstraint *data = static_cast<bSplineIKConstraint *>(con->data);
    bConstraintTarget *ct = static_cast<bConstraintTarget *>(list->first);

    /* the following macro is used for all standard single-target constraints */
    SINGLETARGETNS_FLUSH_TARS(con, data->tar, ct, list, no_copy);
  }
}

static bool splineik_get_tarmat(Depsgraph * /*depsgraph*/,
                                bConstraint * /*con*/,
                                bConstraintOb * /*cob*/,
                                bConstraintTarget *ct,
                                float /*ctime*/)
{
  /* technically, this isn't really needed for evaluation, but we don't know what else
   * might end up calling this...
   */
  unit_ct_matrix_nullsafe(ct);
  return false;
}

static bConstraintTypeInfo CTI_SPLINEIK = {
    /*type*/ CONSTRAINT_TYPE_SPLINEIK,
    /*size*/ sizeof(bSplineIKConstraint),
    /*name*/ N_("Spline IK"),
    /*struct_name*/ "bSplineIKConstraint",
    /*free_data*/ splineik_free,
    /*id_looper*/ splineik_id_looper,
    /*copy_data*/ splineik_copy,
    /*new_data*/ splineik_new_data,
    /*get_constraint_targets*/ splineik_get_tars,
    /*flush_constraint_targets*/ splineik_flush_tars,
    /*get_target_matrix*/ splineik_get_tarmat,
    /*evaluate_constraint*/ nullptr,
};

/* ----------- Pivot ------------- */

static void pivotcon_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
  bPivotConstraint *data = static_cast<bPivotConstraint *>(con->data);

  /* target only */
  func(con, (ID **)&data->tar, false, userdata);
}

static int pivotcon_get_tars(bConstraint *con, ListBase *list)
{
  if (con && list) {
    bPivotConstraint *data = static_cast<bPivotConstraint *>(con->data);
    bConstraintTarget *ct;

    /* standard target-getting macro for single-target constraints */
    SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list);

    return 1;
  }

  return 0;
}

static void pivotcon_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
  if (con && list) {
    bPivotConstraint *data = static_cast<bPivotConstraint *>(con->data);
    bConstraintTarget *ct = static_cast<bConstraintTarget *>(list->first);

    /* the following macro is used for all standard single-target constraints */
    SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, no_copy);
  }
}

static void pivotcon_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
  bPivotConstraint *data = static_cast<bPivotConstraint *>(con->data);
  bConstraintTarget *ct = static_cast<bConstraintTarget *>(targets->first);

  float pivot[3], vec[3];
  float rotMat[3][3];

  /* pivot correction */
  float axis[3], angle;

  const int rot_axis = std::clamp(
      int(data->rotAxis), int(PIVOTCON_AXIS_NONE), int(PIVOTCON_AXIS_Z));

  /* firstly, check if pivoting should take place based on the current rotation */
  if (rot_axis != PIVOTCON_AXIS_NONE) {

    float rot[3];

    /* extract euler-rotation of target */
    mat4_to_eulO(rot, cob->rotOrder, cob->matrix);

    /* check which range might be violated */
    if (rot_axis < PIVOTCON_AXIS_X) {
      /* Negative rotations (`rot_axis = 0 -> 2`). */
      if (rot[rot_axis] > 0.0f) {
        return;
      }
    }
    else {
      /* Positive rotations (`rot_axis = 3 -> 5`). */
      if (rot[rot_axis - PIVOTCON_AXIS_X] < 0.0f) {
        return;
      }
    }
  }

  /* Find the pivot-point to use. */
  if (VALID_CONS_TARGET(ct)) {
    /* apply offset to target location */
    add_v3_v3v3(pivot, ct->matrix[3], data->offset);
  }
  else {
    /* no targets to worry about... */
    if ((data->flag & PIVOTCON_FLAG_OFFSET_ABS) == 0) {
      /* offset is relative to owner */
      add_v3_v3v3(pivot, cob->matrix[3], data->offset);
    }
    else {
      /* directly use the 'offset' specified as an absolute position instead */
      copy_v3_v3(pivot, data->offset);
    }
  }

  /* get rotation matrix representing the rotation of the owner */
  /* TODO: perhaps we might want to include scaling based on the pivot too? */
  copy_m3_m4(rotMat, cob->matrix);
  normalize_m3(rotMat);

  /* correct the pivot by the rotation axis otherwise the pivot translates when it shouldn't */
  mat3_normalized_to_axis_angle(axis, &angle, rotMat);
  if (angle) {
    float dvec[3];
    sub_v3_v3v3(vec, pivot, cob->matrix[3]);
    project_v3_v3v3(dvec, vec, axis);
    sub_v3_v3(pivot, dvec);
  }

  /* perform the pivoting... */
  /* 1. take the vector from owner to the pivot */
  sub_v3_v3v3(vec, cob->matrix[3], pivot);
  /* 2. rotate this vector by the rotation of the object... */
  mul_m3_v3(rotMat, vec);
  /* 3. make the rotation in terms of the pivot now */
  add_v3_v3v3(cob->matrix[3], pivot, vec);
}

static bConstraintTypeInfo CTI_PIVOT = {
    /*type*/ CONSTRAINT_TYPE_PIVOT,
    /*size*/ sizeof(bPivotConstraint),
    /*name*/ N_("Pivot"),
    /*struct_name*/ "bPivotConstraint",
    /*free_data*/ nullptr,
    /*id_looper*/ pivotcon_id_looper,
    /*copy_data*/ nullptr,
    /*new_data*/ nullptr,
    /* XXX: might be needed to get 'normal' pivot behavior. */
    /*get_constraint_targets*/ pivotcon_get_tars,
    /*flush_constraint_targets*/ pivotcon_flush_tars,
    /*get_target_matrix*/ default_get_tarmat,
    /*evaluate_constraint*/ pivotcon_evaluate,
};

/* ----------- Follow Track ------------- */

static void followtrack_new_data(void *cdata)
{
  bFollowTrackConstraint *data = (bFollowTrackConstraint *)cdata;

  data->clip = nullptr;
  data->flag |= FOLLOWTRACK_ACTIVECLIP;
}

static void followtrack_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
  bFollowTrackConstraint *data = static_cast<bFollowTrackConstraint *>(con->data);

  func(con, (ID **)&data->clip, true, userdata);
  func(con, (ID **)&data->camera, false, userdata);
  func(con, (ID **)&data->depth_ob, false, userdata);
}

static MovieClip *followtrack_tracking_clip_get(bConstraint *con, bConstraintOb *cob)
{
  bFollowTrackConstraint *data = static_cast<bFollowTrackConstraint *>(con->data);

  if (data->flag & FOLLOWTRACK_ACTIVECLIP) {
    Scene *scene = cob->scene;
    return scene->clip;
  }

  return data->clip;
}

static MovieTrackingObject *followtrack_tracking_object_get(bConstraint *con, bConstraintOb *cob)
{
  MovieClip *clip = followtrack_tracking_clip_get(con, cob);
  MovieTracking *tracking = &clip->tracking;
  bFollowTrackConstraint *data = static_cast<bFollowTrackConstraint *>(con->data);

  if (data->object[0]) {
    return BKE_tracking_object_get_named(tracking, data->object);
  }
  return BKE_tracking_object_get_camera(tracking);
}

static Object *followtrack_camera_object_get(bConstraint *con, bConstraintOb *cob)
{
  bFollowTrackConstraint *data = static_cast<bFollowTrackConstraint *>(con->data);

  if (data->camera == nullptr) {
    Scene *scene = cob->scene;
    return scene->camera;
  }

  return data->camera;
}

struct FollowTrackContext {
  int flag;
  int frame_method;

  Depsgraph *depsgraph;
  Scene *scene;

  MovieClip *clip;
  Object *camera_object;
  Object *depth_object;

  MovieTracking *tracking;
  MovieTrackingObject *tracking_object;
  MovieTrackingTrack *track;

  float depsgraph_time;
  float clip_frame;
};

static bool followtrack_context_init(FollowTrackContext *context,
                                     bConstraint *con,
                                     bConstraintOb *cob)
{
  bFollowTrackConstraint *data = static_cast<bFollowTrackConstraint *>(con->data);

  context->flag = data->flag;
  context->frame_method = data->frame_method;

  context->depsgraph = cob->depsgraph;
  context->scene = cob->scene;

  context->clip = followtrack_tracking_clip_get(con, cob);
  context->camera_object = followtrack_camera_object_get(con, cob);
  if (context->clip == nullptr || context->camera_object == nullptr) {
    return false;
  }
  context->depth_object = data->depth_ob;

  context->tracking = &context->clip->tracking;
  context->tracking_object = followtrack_tracking_object_get(con, cob);
  if (context->tracking_object == nullptr) {
    return false;
  }

  context->track = BKE_tracking_object_find_track_with_name(context->tracking_object, data->track);
  if (context->track == nullptr) {
    return false;
  }

  context->depsgraph_time = DEG_get_ctime(context->depsgraph);
  context->clip_frame = BKE_movieclip_remap_scene_to_clip_frame(context->clip,
                                                                context->depsgraph_time);

  return true;
}

static void followtrack_evaluate_using_3d_position_object(FollowTrackContext *context,
                                                          bConstraintOb *cob)
{
  Object *camera_object = context->camera_object;
  MovieTracking *tracking = context->tracking;
  MovieTrackingTrack *track = context->track;
  MovieTrackingObject *tracking_object = context->tracking_object;

  /* Matrix of the object which is being solved prior to this constraint. */
  float obmat[4][4];
  copy_m4_m4(obmat, cob->matrix);

  /* Object matrix of the camera. */
  float camera_obmat[4][4];
  copy_m4_m4(camera_obmat, camera_object->object_to_world().ptr());

  /* Calculate inverted matrix of the solved camera at the current time. */
  float reconstructed_camera_mat[4][4];
  BKE_tracking_camera_get_reconstructed_interpolate(
      tracking, tracking_object, context->clip_frame, reconstructed_camera_mat);
  float reconstructed_camera_mat_inv[4][4];
  invert_m4_m4(reconstructed_camera_mat_inv, reconstructed_camera_mat);

  mul_m4_series(cob->matrix, obmat, camera_obmat, reconstructed_camera_mat_inv);
  translate_m4(cob->matrix, track->bundle_pos[0], track->bundle_pos[1], track->bundle_pos[2]);
}

static void followtrack_evaluate_using_3d_position_camera(FollowTrackContext *context,
                                                          bConstraintOb *cob)
{
  Object *camera_object = context->camera_object;
  MovieTrackingTrack *track = context->track;

  /* Matrix of the object which is being solved prior to this constraint. */
  float obmat[4][4];
  copy_m4_m4(obmat, cob->matrix);

  float reconstructed_camera_mat[4][4];
  BKE_tracking_get_camera_object_matrix(camera_object, reconstructed_camera_mat);

  mul_m4_m4m4(cob->matrix, obmat, reconstructed_camera_mat);
  translate_m4(cob->matrix, track->bundle_pos[0], track->bundle_pos[1], track->bundle_pos[2]);
}

static void followtrack_evaluate_using_3d_position(FollowTrackContext *context, bConstraintOb *cob)
{
  MovieTrackingTrack *track = context->track;
  if ((track->flag & TRACK_HAS_BUNDLE) == 0) {
    return;
  }

  if ((context->tracking_object->flag & TRACKING_OBJECT_CAMERA) == 0) {
    followtrack_evaluate_using_3d_position_object(context, cob);
    return;
  }

  followtrack_evaluate_using_3d_position_camera(context, cob);
}

/* Apply undistortion if it is enabled in constraint settings. */
static void followtrack_undistort_if_needed(FollowTrackContext *context,
                                            const int clip_width,
                                            const int clip_height,
                                            float marker_position[2])
{
  if ((context->flag & FOLLOWTRACK_USE_UNDISTORTION) == 0) {
    return;
  }

  /* Undistortion need to happen in pixel space. */
  marker_position[0] *= clip_width;
  marker_position[1] *= clip_height;

  BKE_tracking_undistort_v2(
      context->tracking, clip_width, clip_height, marker_position, marker_position);

  /* Normalize pixel coordinates back. */
  marker_position[0] /= clip_width;
  marker_position[1] /= clip_height;
}

/* Modify the marker position matching the frame fitting method. */
static void followtrack_fit_frame(FollowTrackContext *context,
                                  const int clip_width,
                                  const int clip_height,
                                  float marker_position[2])
{
  if (context->frame_method == FOLLOWTRACK_FRAME_STRETCH) {
    return;
  }

  Scene *scene = context->scene;
  MovieClip *clip = context->clip;

  /* apply clip display aspect */
  const float w_src = clip_width * clip->aspx;
  const float h_src = clip_height * clip->aspy;

  const float w_dst = scene->r.xsch * scene->r.xasp;
  const float h_dst = scene->r.ysch * scene->r.yasp;

  const float asp_src = w_src / h_src;
  const float asp_dst = w_dst / h_dst;

  if (fabsf(asp_src - asp_dst) < FLT_EPSILON) {
    return;
  }

  if ((asp_src > asp_dst) == (context->frame_method == FOLLOWTRACK_FRAME_CROP)) {
    /* fit X */
    float div = asp_src / asp_dst;
    float cent = float(clip_width) / 2.0f;

    marker_position[0] = (((marker_position[0] * clip_width - cent) * div) + cent) / clip_width;
  }
  else {
    /* fit Y */
    float div = asp_dst / asp_src;
    float cent = float(clip_height) / 2.0f;

    marker_position[1] = (((marker_position[1] * clip_height - cent) * div) + cent) / clip_height;
  }
}

/* Effectively this is a Z-depth of the object form the movie clip camera.
 * The idea is to preserve this depth while moving the object in 2D. */
static float followtrack_distance_from_viewplane_get(FollowTrackContext *context,
                                                     bConstraintOb *cob)
{
  Object *camera_object = context->camera_object;

  float camera_matrix[4][4];
  BKE_object_where_is_calc_mat4(camera_object, camera_matrix);

  const float z_axis[3] = {0.0f, 0.0f, 1.0f};

  /* Direction of camera's local Z axis in the world space. */
  float camera_axis[3];
  mul_v3_mat3_m4v3(camera_axis, camera_matrix, z_axis);

  /* Distance to projection plane. */
  float vec[3];
  copy_v3_v3(vec, cob->matrix[3]);
  sub_v3_v3(vec, camera_matrix[3]);

  float projection[3];
  project_v3_v3v3(projection, vec, camera_axis);

  return len_v3(projection);
}

/* For the evaluated constraint object project it to the surface of the depth object. */
static void followtrack_project_to_depth_object_if_needed(FollowTrackContext *context,
                                                          bConstraintOb *cob)
{
  if (context->depth_object == nullptr) {
    return;
  }

  Object *depth_object = context->depth_object;
  const Mesh *depth_mesh = BKE_object_get_evaluated_mesh(depth_object);
  if (depth_mesh == nullptr) {
    return;
  }

  float depth_object_mat_inv[4][4];
  invert_m4_m4(depth_object_mat_inv, depth_object->object_to_world().ptr());

  float ray_start[3], ray_end[3];
  mul_v3_m4v3(
      ray_start, depth_object_mat_inv, context->camera_object->object_to_world().location());
  mul_v3_m4v3(ray_end, depth_object_mat_inv, cob->matrix[3]);

  float ray_direction[3];
  sub_v3_v3v3(ray_direction, ray_end, ray_start);
  normalize_v3(ray_direction);

  blender::bke::BVHTreeFromMesh tree_data = depth_mesh->bvh_corner_tris();

  BVHTreeRayHit hit;
  hit.dist = BVH_RAYCAST_DIST_MAX;
  hit.index = -1;

  const int result = BLI_bvhtree_ray_cast(tree_data.tree,
                                          ray_start,
                                          ray_direction,
                                          0.0f,
                                          &hit,
                                          tree_data.raycast_callback,
                                          &tree_data);

  if (result != -1) {
    mul_v3_m4v3(cob->matrix[3], depth_object->object_to_world().ptr(), hit.co);
  }
}

static void followtrack_evaluate_using_2d_position(FollowTrackContext *context, bConstraintOb *cob)
{
  Scene *scene = context->scene;
  MovieClip *clip = context->clip;
  MovieTrackingTrack *track = context->track;
  Object *camera_object = context->camera_object;
  const float clip_frame = context->clip_frame;
  const float aspect = (scene->r.xsch * scene->r.xasp) / (scene->r.ysch * scene->r.yasp);

  const float object_depth = followtrack_distance_from_viewplane_get(context, cob);
  if (object_depth < FLT_EPSILON) {
    return;
  }

  int clip_width, clip_height;
  BKE_movieclip_get_size(clip, nullptr, &clip_width, &clip_height);

  float marker_position[2];
  BKE_tracking_marker_get_subframe_position(track, clip_frame, marker_position);

  followtrack_undistort_if_needed(context, clip_width, clip_height, marker_position);
  followtrack_fit_frame(context, clip_width, clip_height, marker_position);

  float rmat[4][4];
  CameraParams params;
  BKE_camera_params_init(&params);
  BKE_camera_params_from_object(&params, camera_object);

  if (params.is_ortho) {
    float vec[3];
    vec[0] = params.ortho_scale * (marker_position[0] - 0.5f + params.shiftx);
    vec[1] = params.ortho_scale * (marker_position[1] - 0.5f + params.shifty);
    vec[2] = -object_depth;

    if (aspect > 1.0f) {
      vec[1] /= aspect;
    }
    else {
      vec[0] *= aspect;
    }

    float disp[3];
    mul_v3_m4v3(disp, camera_object->object_to_world().ptr(), vec);

    copy_m4_m4(rmat, camera_object->object_to_world().ptr());
    zero_v3(rmat[3]);
    mul_m4_m4m4(cob->matrix, cob->matrix, rmat);

    copy_v3_v3(cob->matrix[3], disp);
  }
  else {
    const float d = (object_depth * params.sensor_x) / (2.0f * params.lens);

    float vec[3];
    vec[0] = d * (2.0f * (marker_position[0] + params.shiftx) - 1.0f);
    vec[1] = d * (2.0f * (marker_position[1] + params.shifty) - 1.0f);
    vec[2] = -object_depth;

    if (aspect > 1.0f) {
      vec[1] /= aspect;
    }
    else {
      vec[0] *= aspect;
    }

    float disp[3];
    mul_v3_m4v3(disp, camera_object->object_to_world().ptr(), vec);

    /* apply camera rotation so Z-axis would be co-linear */
    copy_m4_m4(rmat, camera_object->object_to_world().ptr());
    zero_v3(rmat[3]);
    mul_m4_m4m4(cob->matrix, cob->matrix, rmat);

    copy_v3_v3(cob->matrix[3], disp);
  }

  followtrack_project_to_depth_object_if_needed(context, cob);
}

static void followtrack_evaluate(bConstraint *con, bConstraintOb *cob, ListBase * /*targets*/)
{
  FollowTrackContext context;
  if (!followtrack_context_init(&context, con, cob)) {
    return;
  }

  bFollowTrackConstraint *data = static_cast<bFollowTrackConstraint *>(con->data);
  if (data->flag & FOLLOWTRACK_USE_3D_POSITION) {
    followtrack_evaluate_using_3d_position(&context, cob);
    return;
  }

  followtrack_evaluate_using_2d_position(&context, cob);
}

static bConstraintTypeInfo CTI_FOLLOWTRACK = {
    /*type*/ CONSTRAINT_TYPE_FOLLOWTRACK,
    /*size*/ sizeof(bFollowTrackConstraint),
    /*name*/ N_("Follow Track"),
    /*struct_name*/ "bFollowTrackConstraint",
    /*free_data*/ nullptr,
    /*id_looper*/ followtrack_id_looper,
    /*copy_data*/ nullptr,
    /*new_data*/ followtrack_new_data,
    /*get_constraint_targets*/ nullptr,
    /*flush_constraint_targets*/ nullptr,
    /*get_target_matrix*/ nullptr,
    /*evaluate_constraint*/ followtrack_evaluate,
};

/* ----------- Camera Solver ------------- */

static void camerasolver_new_data(void *cdata)
{
  bCameraSolverConstraint *data = (bCameraSolverConstraint *)cdata;

  data->clip = nullptr;
  data->flag |= CAMERASOLVER_ACTIVECLIP;
}

static void camerasolver_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
  bCameraSolverConstraint *data = static_cast<bCameraSolverConstraint *>(con->data);

  func(con, (ID **)&data->clip, true, userdata);
}

static void camerasolver_evaluate(bConstraint *con, bConstraintOb *cob, ListBase * /*targets*/)
{
  Depsgraph *depsgraph = cob->depsgraph;
  Scene *scene = cob->scene;
  bCameraSolverConstraint *data = static_cast<bCameraSolverConstraint *>(con->data);
  MovieClip *clip = data->clip;

  if (data->flag & CAMERASOLVER_ACTIVECLIP) {
    clip = scene->clip;
  }

  if (clip) {
    float mat[4][4], obmat[4][4];
    MovieTracking *tracking = &clip->tracking;
    MovieTrackingObject *tracking_object = BKE_tracking_object_get_camera(tracking);
    const float ctime = DEG_get_ctime(depsgraph);
    const float framenr = BKE_movieclip_remap_scene_to_clip_frame(clip, ctime);

    BKE_tracking_camera_get_reconstructed_interpolate(tracking, tracking_object, framenr, mat);

    copy_m4_m4(obmat, cob->matrix);

    mul_m4_m4m4(cob->matrix, obmat, mat);
  }
}

static bConstraintTypeInfo CTI_CAMERASOLVER = {
    /*type*/ CONSTRAINT_TYPE_CAMERASOLVER,
    /*size*/ sizeof(bCameraSolverConstraint),
    /*name*/ N_("Camera Solver"),
    /*struct_name*/ "bCameraSolverConstraint",
    /*free_data*/ nullptr,
    /*id_looper*/ camerasolver_id_looper,
    /*copy_data*/ nullptr,
    /*new_data*/ camerasolver_new_data,
    /*get_constraint_targets*/ nullptr,
    /*flush_constraint_targets*/ nullptr,
    /*get_target_matrix*/ nullptr,
    /*evaluate_constraint*/ camerasolver_evaluate,
};

/* ----------- Object Solver ------------- */

static void objectsolver_new_data(void *cdata)
{
  bObjectSolverConstraint *data = (bObjectSolverConstraint *)cdata;

  data->clip = nullptr;
  data->flag |= OBJECTSOLVER_ACTIVECLIP;
  unit_m4(data->invmat);
}

static void objectsolver_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
  bObjectSolverConstraint *data = static_cast<bObjectSolverConstraint *>(con->data);

  func(con, (ID **)&data->clip, false, userdata);
  func(con, (ID **)&data->camera, false, userdata);
}

static void objectsolver_evaluate(bConstraint *con, bConstraintOb *cob, ListBase * /*targets*/)
{
  Depsgraph *depsgraph = cob->depsgraph;
  Scene *scene = cob->scene;
  bObjectSolverConstraint *data = static_cast<bObjectSolverConstraint *>(con->data);
  MovieClip *clip = data->clip;
  Object *camob = data->camera ? data->camera : scene->camera;

  if (data->flag & OBJECTSOLVER_ACTIVECLIP) {
    clip = scene->clip;
  }
  if (!camob || !clip) {
    return;
  }

  MovieTracking *tracking = &clip->tracking;
  MovieTrackingObject *tracking_object = BKE_tracking_object_get_named(tracking, data->object);
  if (!tracking_object) {
    return;
  }

  float mat[4][4], obmat[4][4], imat[4][4], parmat[4][4];
  float ctime = DEG_get_ctime(depsgraph);
  float framenr = BKE_movieclip_remap_scene_to_clip_frame(clip, ctime);

  BKE_tracking_camera_get_reconstructed_interpolate(tracking, tracking_object, framenr, mat);

  invert_m4_m4(imat, mat);
  mul_m4_m4m4(parmat, camob->object_to_world().ptr(), imat);

  copy_m4_m4(obmat, cob->matrix);

  /* Recalculate the inverse matrix if requested. */
  if (data->flag & OBJECTSOLVER_SET_INVERSE) {
    invert_m4_m4(data->invmat, parmat);

    data->flag &= ~OBJECTSOLVER_SET_INVERSE;

    /* Write the computed matrix back to the master copy if in copy-on-eval evaluation. */
    bConstraint *orig_con = constraint_find_original_for_update(cob, con);

    if (orig_con != nullptr) {
      bObjectSolverConstraint *orig_data = static_cast<bObjectSolverConstraint *>(orig_con->data);

      copy_m4_m4(orig_data->invmat, data->invmat);
      orig_data->flag &= ~OBJECTSOLVER_SET_INVERSE;
    }
  }

  mul_m4_series(cob->matrix, parmat, data->invmat, obmat);
}

static bConstraintTypeInfo CTI_OBJECTSOLVER = {
    /*type*/ CONSTRAINT_TYPE_OBJECTSOLVER,
    /*size*/ sizeof(bObjectSolverConstraint),
    /*name*/ N_("Object Solver"),
    /*struct_name*/ "bObjectSolverConstraint",
    /*free_data*/ nullptr,
    /*id_looper*/ objectsolver_id_looper,
    /*copy_data*/ nullptr,
    /*new_data*/ objectsolver_new_data,
    /*get_constraint_targets*/ nullptr,
    /*flush_constraint_targets*/ nullptr,
    /*get_target_matrix*/ nullptr,
    /*evaluate_constraint*/ objectsolver_evaluate,
};

/* ----------- Transform Cache ------------- */

static void transformcache_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
  bTransformCacheConstraint *data = static_cast<bTransformCacheConstraint *>(con->data);
  func(con, (ID **)&data->cache_file, true, userdata);
}

static void transformcache_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
#if defined(WITH_ALEMBIC) || defined(WITH_USD)
  bTransformCacheConstraint *data = static_cast<bTransformCacheConstraint *>(con->data);
  Scene *scene = cob->scene;

  CacheFile *cache_file = data->cache_file;

  if (!cache_file) {
    return;
  }

  const float frame = DEG_get_ctime(cob->depsgraph);
  const double time = BKE_cachefile_time_offset(
      cache_file, double(frame), scene->frames_per_second());

  if (!data->reader || !STREQ(data->reader_object_path, data->object_path)) {
    STRNCPY(data->reader_object_path, data->object_path);
    BKE_cachefile_reader_open(cache_file, &data->reader, cob->ob, data->object_path);
  }

  switch (cache_file->type) {
    case CACHEFILE_TYPE_ALEMBIC:
#  ifdef WITH_ALEMBIC
      ABC_get_transform(data->reader, cob->matrix, time, cache_file->scale);
#  endif
      break;
    case CACHEFILE_TYPE_USD:
#  ifdef WITH_USD
      blender::io::usd::USD_get_transform(
          data->reader, cob->matrix, time * scene->frames_per_second(), cache_file->scale);
#  endif
      break;
    case CACHE_FILE_TYPE_INVALID:
      break;
  }
#else
  UNUSED_VARS(con, cob);
#endif

  UNUSED_VARS(targets);
}

static void transformcache_copy(bConstraint *con, bConstraint *srccon)
{
  bTransformCacheConstraint *src = static_cast<bTransformCacheConstraint *>(srccon->data);
  bTransformCacheConstraint *dst = static_cast<bTransformCacheConstraint *>(con->data);

  STRNCPY(dst->object_path, src->object_path);
  dst->cache_file = src->cache_file;
  dst->reader = nullptr;
  dst->reader_object_path[0] = '\0';
}

static void transformcache_free(bConstraint *con)
{
  bTransformCacheConstraint *data = static_cast<bTransformCacheConstraint *>(con->data);

  if (data->reader) {
    BKE_cachefile_reader_free(data->cache_file, &data->reader);
    data->reader_object_path[0] = '\0';
  }
}

static void transformcache_new_data(void *cdata)
{
  bTransformCacheConstraint *data = (bTransformCacheConstraint *)cdata;

  data->cache_file = nullptr;
}

static bConstraintTypeInfo CTI_TRANSFORM_CACHE = {
    /*type*/ CONSTRAINT_TYPE_TRANSFORM_CACHE,
    /*size*/ sizeof(bTransformCacheConstraint),
    /*name*/ N_("Transform Cache"),
    /*struct_name*/ "bTransformCacheConstraint",
    /*free_data*/ transformcache_free,
    /*id_looper*/ transformcache_id_looper,
    /*copy_data*/ transformcache_copy,
    /*new_data*/ transformcache_new_data,
    /*get_constraint_targets*/ nullptr,
    /*flush_constraint_targets*/ nullptr,
    /*get_target_matrix*/ nullptr,
    /*evaluate_constraint*/ transformcache_evaluate,
};

/* ---------- Geometry Attribute Constraint ----------- */

static blender::bke::AttrDomain domain_value_to_attribute(const Attribute_Domain domain)
{
  switch (domain) {
    case CON_ATTRIBUTE_DOMAIN_POINT:
      return blender::bke::AttrDomain::Point;
    case CON_ATTRIBUTE_DOMAIN_EDGE:
      return blender::bke::AttrDomain::Edge;
    case CON_ATTRIBUTE_DOMAIN_FACE:
      return blender::bke::AttrDomain::Face;
    case CON_ATTRIBUTE_DOMAIN_FACE_CORNER:
      return blender::bke::AttrDomain::Corner;
    case CON_ATTRIBUTE_DOMAIN_CURVE:
      return blender::bke::AttrDomain::Curve;
    case CON_ATTRIBUTE_DOMAIN_INSTANCE:
      return blender::bke::AttrDomain::Instance;
  }
  BLI_assert_unreachable();
  return blender::bke::AttrDomain::Point;
}

static blender::bke::AttrType type_value_to_attribute(const Attribute_Data_Type data_type)
{
  switch (data_type) {
    case CON_ATTRIBUTE_VECTOR:
      return blender::bke::AttrType::Float3;
    case CON_ATTRIBUTE_QUATERNION:
      return blender::bke::AttrType::Quaternion;
    case CON_ATTRIBUTE_4X4MATRIX:
      return blender::bke::AttrType::Float4x4;
  }
  BLI_assert_unreachable();
  return blender::bke::AttrType::Float3;
}

static void value_attribute_to_matrix(float r_matrix[4][4],
                                      const blender::GPointer value,
                                      const Attribute_Data_Type data_type)
{
  switch (data_type) {
    case CON_ATTRIBUTE_VECTOR:
      copy_v3_v3(r_matrix[3], *value.get<blender::float3>());
      return;
    case CON_ATTRIBUTE_QUATERNION:
      quat_to_mat4(r_matrix, *value.get<blender::float4>());
      return;
    case CON_ATTRIBUTE_4X4MATRIX:
      copy_m4_m4(r_matrix, value.get<blender::float4x4>()->ptr());
      return;
  }
  BLI_assert_unreachable();
}

static bool component_is_available(const blender::bke::GeometrySet &geometry,
                                   const blender::bke::GeometryComponent::Type type,
                                   const blender::bke::AttrDomain domain)
{
  if (const blender::bke::GeometryComponent *component = geometry.get_component(type)) {
    return component->attribute_domain_size(domain) != 0;
  }
  return false;
}

static const blender::bke::GeometryComponent *find_source_component(
    const blender::bke::GeometrySet &geometry, const blender::bke::AttrDomain domain)
{
  /* Choose the other component based on a consistent order, rather than some more complicated
   * heuristic. This is the same order visible in the spreadsheet and used in the ray-cast node. */
  static const blender::Array<blender::bke::GeometryComponent::Type> supported_types = {
      blender::bke::GeometryComponent::Type::Mesh,
      blender::bke::GeometryComponent::Type::PointCloud,
      blender::bke::GeometryComponent::Type::Curve,
      blender::bke::GeometryComponent::Type::Instance,
      blender::bke::GeometryComponent::Type::GreasePencil};
  for (const blender::bke::GeometryComponent::Type src_type : supported_types) {
    if (component_is_available(geometry, src_type, domain)) {
      return geometry.get_component(src_type);
    }
  }

  return nullptr;
}

static void geometry_attribute_free_data(bConstraint *con)
{
  bGeometryAttributeConstraint *data = static_cast<bGeometryAttributeConstraint *>(con->data);
  MEM_SAFE_FREE(data->attribute_name);
}

static void geometry_attribute_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
  bGeometryAttributeConstraint *data = static_cast<bGeometryAttributeConstraint *>(con->data);
  func(con, (ID **)&data->target, false, userdata);
}

static void geometry_attribute_copy_data(bConstraint *con, bConstraint *srccon)
{
  const auto *src = static_cast<bGeometryAttributeConstraint *>(srccon->data);
  auto *dst = static_cast<bGeometryAttributeConstraint *>(con->data);
  dst->attribute_name = BLI_strdup_null(src->attribute_name);
}

static void geometry_attribute_new_data(void *cdata)
{
  bGeometryAttributeConstraint *data = static_cast<bGeometryAttributeConstraint *>(cdata);
  data->attribute_name = BLI_strdup("position");
  data->flags = MIX_LOC | MIX_ROT | MIX_SCALE;
}

static int geometry_attribute_get_tars(bConstraint *con, ListBase *list)
{
  if (!con || !list) {
    return 0;
  }
  bGeometryAttributeConstraint *data = static_cast<bGeometryAttributeConstraint *>(con->data);
  bConstraintTarget *ct;

  SINGLETARGETNS_GET_TARS(con, data->target, ct, list);

  return 1;
}

static void geometry_attribute_flush_tars(bConstraint *con, ListBase *list, const bool no_copy)
{
  if (!con || !list) {
    return;
  }
  bGeometryAttributeConstraint *data = static_cast<bGeometryAttributeConstraint *>(con->data);
  bConstraintTarget *ct = static_cast<bConstraintTarget *>(list->first);

  SINGLETARGETNS_FLUSH_TARS(con, data->target, ct, list, no_copy);
}

static bool geometry_attribute_get_tarmat(Depsgraph * /*depsgraph*/,
                                          bConstraint *con,
                                          bConstraintOb * /*cob*/,
                                          bConstraintTarget *ct,
                                          float /*ctime*/)
{
  using namespace blender;
  const bGeometryAttributeConstraint *acon = static_cast<bGeometryAttributeConstraint *>(
      con->data);

  if (!VALID_CONS_TARGET(ct)) {
    return false;
  }

  unit_m4(ct->matrix);

  const bke::AttrDomain domain = domain_value_to_attribute(
      static_cast<Attribute_Domain>(acon->domain));
  const bke::AttrType sample_data_type = type_value_to_attribute(
      static_cast<Attribute_Data_Type>(acon->data_type));
  const bke::GeometrySet &target_eval = bke::object_get_evaluated_geometry_set(*ct->tar);

  const bke::GeometryComponent *component = find_source_component(target_eval, domain);
  if (component == nullptr) {
    return false;
  }

  const std::optional<bke::AttributeAccessor> optional_attributes = component->attributes();
  if (!optional_attributes.has_value()) {
    return false;
  }

  const bke::AttributeAccessor &attributes = *optional_attributes;
  const GVArray attribute = *attributes.lookup(acon->attribute_name, domain, sample_data_type);

  if (attribute.is_empty()) {
    return false;
  }

  const int index = std::clamp<int>(acon->sample_index, 0, attribute.size() - 1);

  const CPPType &type = attribute.type();
  BUFFER_FOR_CPP_TYPE_VALUE(type, sampled_value);
  attribute.get_to_uninitialized(index, sampled_value);

  value_attribute_to_matrix(ct->matrix,
                            GPointer(type, sampled_value),
                            static_cast<Attribute_Data_Type>(acon->data_type));
  type.destruct(sampled_value);

  return true;
}

static void geometry_attribute_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
  bConstraintTarget *ct = static_cast<bConstraintTarget *>(targets->first);
  const bGeometryAttributeConstraint *data = static_cast<bGeometryAttributeConstraint *>(
      con->data);

  /* Only evaluate if there is a target. */
  if (!VALID_CONS_TARGET(ct)) {
    return;
  }

  float target_mat[4][4];
  if (data->mix_mode == CON_ATTRIBUTE_MIX_REPLACE) {
    copy_m4_m4(target_mat, cob->matrix);
  }
  else {
    unit_m4(target_mat);
  }

  float prev_location[3];
  float prev_rotation[3][3];
  float prev_size[3];
  mat4_to_loc_rot_size(prev_location, prev_rotation, prev_size, target_mat);

  float next_location[3];
  float next_rotation[3][3];
  float next_size[3];
  mat4_to_loc_rot_size(next_location, next_rotation, next_size, ct->matrix);

  switch (data->data_type) {
    case CON_ATTRIBUTE_VECTOR:
      loc_rot_size_to_mat4(target_mat, next_location, prev_rotation, prev_size);
      break;
    case CON_ATTRIBUTE_QUATERNION:
      loc_rot_size_to_mat4(target_mat, prev_location, next_rotation, prev_size);
      break;
    case CON_ATTRIBUTE_4X4MATRIX:
      if ((data->flags & MIX_LOC) && (data->flags & MIX_ROT) && (data->flags & MIX_SCALE)) {
        copy_m4_m4(target_mat, ct->matrix);
      }
      else {
        if (data->flags & MIX_LOC) {
          copy_v3_v3(prev_location, next_location);
        }
        if (data->flags & MIX_ROT) {
          copy_m3_m3(prev_rotation, next_rotation);
        }
        if (data->flags & MIX_SCALE) {
          copy_v3_v3(prev_size, next_size);
        }
        loc_rot_size_to_mat4(target_mat, prev_location, prev_rotation, prev_size);
      }
      break;
  }

  /* Finally, combine the matrices. */
  switch (data->mix_mode) {
    case CON_ATTRIBUTE_MIX_REPLACE:
      copy_m4_m4(cob->matrix, target_mat);
      break;
    /* Simple matrix multiplication. */
    case CON_ATTRIBUTE_MIX_BEFORE_FULL:
      mul_m4_m4m4(cob->matrix, target_mat, cob->matrix);
      break;
    case CON_ATTRIBUTE_MIX_AFTER_FULL:
      mul_m4_m4m4(cob->matrix, cob->matrix, target_mat);
      break;
    /* Fully separate handling of channels. */
    case CON_ATTRIBUTE_MIX_BEFORE_SPLIT:
      mul_m4_m4m4_split_channels(cob->matrix, target_mat, cob->matrix);
      break;
    case CON_ATTRIBUTE_MIX_AFTER_SPLIT:
      mul_m4_m4m4_split_channels(cob->matrix, cob->matrix, target_mat);
      break;
  }

  if (data->apply_target_transform) {
    mul_m4_m4m4(cob->matrix, ct->tar->object_to_world().ptr(), cob->matrix);
  }
}

static bConstraintTypeInfo CTI_ATTRIBUTE = {
    /*type*/ CONSTRAINT_TYPE_GEOMETRY_ATTRIBUTE,
    /*size*/ sizeof(bGeometryAttributeConstraint),
    /*name*/ N_("Geometry Attribute"),
    /*struct_name*/ "bGeometryAttributeConstraint",
    /*free_data*/ geometry_attribute_free_data,
    /*id_looper*/ geometry_attribute_id_looper,
    /*copy_data*/ geometry_attribute_copy_data,
    /*new_data*/ geometry_attribute_new_data,
    /*get_constraint_targets*/ geometry_attribute_get_tars,
    /*flush_constraint_targets*/ geometry_attribute_flush_tars,
    /*get_target_matrix*/ geometry_attribute_get_tarmat,
    /*evaluate_constraint*/ geometry_attribute_evaluate,
};

/* ************************* Constraints Type-Info *************************** */
/* All of the constraints API functions use #bConstraintTypeInfo structs to carry out
 * and operations that involve constraint specific code.
 */

/* These globals only ever get directly accessed in this file */
static bConstraintTypeInfo *constraintsTypeInfo[NUM_CONSTRAINT_TYPES];
static short CTI_INIT = 1; /* when non-zero, the list needs to be updated */

/* This function only gets called when CTI_INIT is non-zero */
static void constraints_init_typeinfo()
{
  constraintsTypeInfo[0] = nullptr;               /* 'Null' Constraint */
  constraintsTypeInfo[1] = &CTI_CHILDOF;          /* ChildOf Constraint */
  constraintsTypeInfo[2] = &CTI_TRACKTO;          /* TrackTo Constraint */
  constraintsTypeInfo[3] = &CTI_KINEMATIC;        /* IK Constraint */
  constraintsTypeInfo[4] = &CTI_FOLLOWPATH;       /* Follow-Path Constraint */
  constraintsTypeInfo[5] = &CTI_ROTLIMIT;         /* Limit Rotation Constraint */
  constraintsTypeInfo[6] = &CTI_LOCLIMIT;         /* Limit Location Constraint */
  constraintsTypeInfo[7] = &CTI_SIZELIMIT;        /* Limit Scale Constraint */
  constraintsTypeInfo[8] = &CTI_ROTLIKE;          /* Copy Rotation Constraint */
  constraintsTypeInfo[9] = &CTI_LOCLIKE;          /* Copy Location Constraint */
  constraintsTypeInfo[10] = &CTI_SIZELIKE;        /* Copy Scale Constraint */
  constraintsTypeInfo[11] = nullptr;              /* Python/Script Constraint: DEPRECATED. */
  constraintsTypeInfo[12] = &CTI_ACTION;          /* Action Constraint */
  constraintsTypeInfo[13] = &CTI_LOCKTRACK;       /* Locked-Track Constraint */
  constraintsTypeInfo[14] = &CTI_DISTLIMIT;       /* Limit Distance Constraint */
  constraintsTypeInfo[15] = &CTI_STRETCHTO;       /* StretchTo Constraint */
  constraintsTypeInfo[16] = &CTI_MINMAX;          /* Floor Constraint */
  constraintsTypeInfo[17] = nullptr;              /* RigidBody Constraint: DEPRECATED. */
  constraintsTypeInfo[18] = &CTI_CLAMPTO;         /* ClampTo Constraint */
  constraintsTypeInfo[19] = &CTI_TRANSFORM;       /* Transformation Constraint */
  constraintsTypeInfo[20] = &CTI_SHRINKWRAP;      /* Shrinkwrap Constraint */
  constraintsTypeInfo[21] = &CTI_DAMPTRACK;       /* Damped TrackTo Constraint */
  constraintsTypeInfo[22] = &CTI_SPLINEIK;        /* Spline IK Constraint */
  constraintsTypeInfo[23] = &CTI_TRANSLIKE;       /* Copy Transforms Constraint */
  constraintsTypeInfo[24] = &CTI_SAMEVOL;         /* Maintain Volume Constraint */
  constraintsTypeInfo[25] = &CTI_PIVOT;           /* Pivot Constraint */
  constraintsTypeInfo[26] = &CTI_FOLLOWTRACK;     /* Follow Track Constraint */
  constraintsTypeInfo[27] = &CTI_CAMERASOLVER;    /* Camera Solver Constraint */
  constraintsTypeInfo[28] = &CTI_OBJECTSOLVER;    /* Object Solver Constraint */
  constraintsTypeInfo[29] = &CTI_TRANSFORM_CACHE; /* Transform Cache Constraint */
  constraintsTypeInfo[30] = &CTI_ARMATURE;        /* Armature Constraint */
  constraintsTypeInfo[31] = &CTI_ATTRIBUTE;       /* Attribute Transform Constraint */
}

const bConstraintTypeInfo *BKE_constraint_typeinfo_from_type(int type)
{
  /* initialize the type-info list? */
  if (CTI_INIT) {
    constraints_init_typeinfo();
    CTI_INIT = 0;
  }

  /* only return for valid types */
  if ((type >= CONSTRAINT_TYPE_NULL) && (type < NUM_CONSTRAINT_TYPES)) {
    /* there shouldn't be any segfaults here... */
    return constraintsTypeInfo[type];
  }

  CLOG_WARN(&LOG, "No valid constraint type-info data available. Type = %i", type);

  return nullptr;
}

const bConstraintTypeInfo *BKE_constraint_typeinfo_get(bConstraint *con)
{
  /* only return typeinfo for valid constraints */
  if (con) {
    return BKE_constraint_typeinfo_from_type(con->type);
  }

  return nullptr;
}

/* ************************* General Constraints API ************************** */
/* The functions here are called by various parts of Blender. Very few (should be none if possible)
 * constraint-specific code should occur here.
 */

/* ---------- Data Management ------- */

/**
 * Helper function for #BKE_constraint_free_data() - unlinks references.
 */
static void con_unlink_refs_cb(bConstraint * /*con*/,
                               ID **idpoin,
                               bool is_reference,
                               void * /*user_data*/)
{
  if (*idpoin && is_reference) {
    id_us_min(*idpoin);
  }
}

/**
 * Helper function to invoke the id_looper callback, including custom space.
 *
 * \param flag: is unused right now, but it's kept as a reminder that new code may need to check
 * flags as well. See enum #LibraryForeachIDFlag in `BKE_lib_query.hh`.
 */
static void con_invoke_id_looper(const bConstraintTypeInfo *cti,
                                 bConstraint *con,
                                 ConstraintIDFunc func,
                                 const int /*flag*/,
                                 void *userdata)
{
  if (cti->id_looper) {
    cti->id_looper(con, func, userdata);
  }

  func(con, (ID **)&con->space_object, false, userdata);
}

void BKE_constraint_free_data_ex(bConstraint *con, bool do_id_user)
{
  if (con->data) {
    const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);

    if (cti) {
      /* perform any special freeing constraint may have */
      if (cti->free_data) {
        cti->free_data(con);
      }

      /* unlink the referenced resources it uses */
      if (do_id_user) {
        con_invoke_id_looper(cti, con, con_unlink_refs_cb, IDWALK_NOP, nullptr);
      }
    }

    /* free constraint data now */
    MEM_freeN(con->data);
  }
}

void BKE_constraint_free_data(bConstraint *con)
{
  BKE_constraint_free_data_ex(con, true);
}

void BKE_constraints_free_ex(ListBase *list, bool do_id_user)
{
  /* Free constraint data and also any extra data */
  LISTBASE_FOREACH (bConstraint *, con, list) {
    BKE_constraint_free_data_ex(con, do_id_user);
  }

  /* Free the whole list */
  BLI_freelistN(list);
}

void BKE_constraints_free(ListBase *list)
{
  BKE_constraints_free_ex(list, true);
}

static bool constraint_remove(ListBase *list, bConstraint *con)
{
  if (con) {
    BKE_constraint_free_data(con);
    BLI_freelinkN(list, con);
    return true;
  }

  return false;
}

bool BKE_constraint_remove_ex(ListBase *list, Object *ob, bConstraint *con)
{
  BKE_animdata_drivers_remove_for_rna_struct(ob->id, RNA_Constraint, con);

  const short type = con->type;
  if (constraint_remove(list, con)) {
    /* ITASC needs to be rebuilt once a constraint is removed #26920. */
    if (ELEM(type, CONSTRAINT_TYPE_KINEMATIC, CONSTRAINT_TYPE_SPLINEIK)) {
      BIK_clear_data(ob->pose);
    }
    return true;
  }

  return false;
}

bool BKE_constraint_apply_for_object(Depsgraph *depsgraph,
                                     Scene *scene,
                                     Object *ob,
                                     bConstraint *con)
{
  if (!con) {
    return false;
  }

  const float ctime = BKE_scene_frame_get(scene);

  /* Do this all in the evaluated domain (e.g. shrinkwrap needs to access evaluated constraint
   * target mesh). */
  Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
  Object *ob_eval = DEG_get_evaluated(depsgraph, ob);
  bConstraint *con_eval = BKE_constraints_find_name(&ob_eval->constraints, con->name);

  bConstraint *new_con = BKE_constraint_duplicate_ex(con_eval, 0, ID_IS_EDITABLE(ob));
  ListBase single_con = {new_con, new_con};

  bConstraintOb *cob = BKE_constraints_make_evalob(
      depsgraph, scene_eval, ob_eval, nullptr, CONSTRAINT_OBTYPE_OBJECT);
  /* Undo the effect of the current constraint stack evaluation. */
  mul_m4_m4m4(cob->matrix, ob_eval->constinv, cob->matrix);

  /* Evaluate single constraint. */
  BKE_constraints_solve(depsgraph, &single_con, cob, ctime);
  /* Copy transforms back. This will leave the object in a bad state
   * as ob->constinv will be wrong until next evaluation. */
  BKE_constraints_clear_evalob(cob);

  /* Free the copied constraint. */
  BKE_constraint_free_data(new_con);
  BLI_freelinkN(&single_con, new_con);

  /* Apply transform from matrix. */
  BKE_object_apply_mat4(ob, ob_eval->object_to_world().ptr(), true, true);

  return true;
}

bool BKE_constraint_apply_and_remove_for_object(Depsgraph *depsgraph,
                                                Scene *scene,
                                                ListBase /*bConstraint*/ *constraints,
                                                Object *ob,
                                                bConstraint *con)
{
  if (!BKE_constraint_apply_for_object(depsgraph, scene, ob, con)) {
    return false;
  }

  return BKE_constraint_remove_ex(constraints, ob, con);
}

bool BKE_constraint_apply_for_pose(
    Depsgraph *depsgraph, Scene *scene, Object *ob, bPoseChannel *pchan, bConstraint *con)
{
  if (!con) {
    return false;
  }

  const float ctime = BKE_scene_frame_get(scene);

  /* Do this all in the evaluated domain (e.g. shrinkwrap needs to access evaluated constraint
   * target mesh). */
  Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
  Object *ob_eval = DEG_get_evaluated(depsgraph, ob);
  bPoseChannel *pchan_eval = BKE_pose_channel_find_name(ob_eval->pose, pchan->name);
  bConstraint *con_eval = BKE_constraints_find_name(&pchan_eval->constraints, con->name);

  bConstraint *new_con = BKE_constraint_duplicate_ex(con_eval, 0, ID_IS_EDITABLE(ob));
  ListBase single_con;
  single_con.first = new_con;
  single_con.last = new_con;

  float vec[3];
  copy_v3_v3(vec, pchan_eval->pose_mat[3]);

  bConstraintOb *cob = BKE_constraints_make_evalob(
      depsgraph, scene_eval, ob_eval, pchan_eval, CONSTRAINT_OBTYPE_BONE);
  /* Undo the effects of currently applied constraints. */
  mul_m4_m4m4(cob->matrix, pchan_eval->constinv, cob->matrix);
  /* Evaluate single constraint. */
  BKE_constraints_solve(depsgraph, &single_con, cob, ctime);
  BKE_constraints_clear_evalob(cob);

  /* Free the copied constraint. */
  BKE_constraint_free_data(new_con);
  BLI_freelinkN(&single_con, new_con);

  /* Prevent constraints breaking a chain. */
  if (pchan->bone->flag & BONE_CONNECTED) {
    copy_v3_v3(pchan_eval->pose_mat[3], vec);
  }

  /* Apply transform from matrix. */
  float mat[4][4];
  BKE_armature_mat_pose_to_bone(pchan, pchan_eval->pose_mat, mat);
  BKE_pchan_apply_mat4(pchan, mat, true);

  return true;
}

bool BKE_constraint_apply_and_remove_for_pose(Depsgraph *depsgraph,
                                              Scene *scene,
                                              ListBase /*bConstraint*/ *constraints,
                                              Object *ob,
                                              bConstraint *con,
                                              bPoseChannel *pchan)
{
  if (!BKE_constraint_apply_for_pose(depsgraph, scene, ob, pchan, con)) {
    return false;
  }

  return BKE_constraint_remove_ex(constraints, ob, con);
}

void BKE_constraint_panel_expand(bConstraint *con)
{
  con->ui_expand_flag |= UI_PANEL_DATA_EXPAND_ROOT;
}

/* ......... */

/* Creates a new constraint, initializes its data, and returns it */
static bConstraint *add_new_constraint_internal(const char *name, short type)
{
  bConstraint *con = MEM_callocN<bConstraint>("Constraint");
  const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_from_type(type);
  const char *newName;

  /* Set up a generic constraint data-block. */
  con->type = type;
  con->flag |= CONSTRAINT_OVERRIDE_LIBRARY_LOCAL;
  con->enforce = 1.0f;

  /* Only open the main panel when constraints are created, not the sub-panels. */
  con->ui_expand_flag = UI_PANEL_DATA_EXPAND_ROOT;
  if (ELEM(type, CONSTRAINT_TYPE_ACTION, CONSTRAINT_TYPE_SPLINEIK)) {
    /* Expand the two sub-panels in the cases where the main panel barely has any properties. */
    con->ui_expand_flag |= UI_SUBPANEL_DATA_EXPAND_1 | UI_SUBPANEL_DATA_EXPAND_2;
  }

  /* Determine a basic name, and info */
  if (cti) {
    /* initialize constraint data */
    con->data = MEM_callocN(cti->size, cti->struct_name);

    /* only constraints that change any settings need this */
    if (cti->new_data) {
      cti->new_data(con->data);
    }

    /* if no name is provided, use the type of the constraint as the name */
    newName = (name && name[0]) ? name : DATA_(cti->name);
  }
  else {
    /* if no name is provided, use the generic "Const" name */
    /* NOTE: any constraint type that gets here really shouldn't get added... */
    newName = (name && name[0]) ? name : DATA_("Const");
  }

  /* copy the name */
  STRNCPY_UTF8(con->name, newName);

  /* return the new constraint */
  return con;
}

/* Add a newly created constraint to the constraint list. */
static void add_new_constraint_to_list(Object *ob, bPoseChannel *pchan, bConstraint *con)
{
  ListBase *list;

  /* find the constraint stack - bone or object? */
  list = (pchan) ? (&pchan->constraints) : (&ob->constraints);

  if (list) {
    /* add new constraint to end of list of constraints before ensuring that it has a unique name
     * (otherwise unique-naming code will fail, since it assumes element exists in list)
     */
    BLI_addtail(list, con);
    BKE_constraint_unique_name(con, list);

    /* make this constraint the active one */
    BKE_constraints_active_set(list, con);
  }
}

/* if pchan is not nullptr then assume we're adding a pose constraint */
static bConstraint *add_new_constraint(Object *ob,
                                       bPoseChannel *pchan,
                                       const char *name,
                                       short type)
{
  bConstraint *con;

  /* add the constraint */
  con = add_new_constraint_internal(name, type);

  add_new_constraint_to_list(ob, pchan, con);

  /* set type+owner specific immutable settings */
  /* TODO: does action constraint need anything here - i.e. spaceonce? */
  switch (type) {
    case CONSTRAINT_TYPE_CHILDOF: {
      /* If this constraint is being added to a pose-channel, make sure
       * the constraint gets evaluated in pose-space. */
      if (pchan) {
        con->ownspace = CONSTRAINT_SPACE_POSE;
      }
      break;
    }
    case CONSTRAINT_TYPE_ACTION: {
      /* The Before or Split modes require computing in local space, but
       * for objects the Local space doesn't make sense (#78462, D6095 etc).
       * So only default to Before (Split) if the constraint is on a bone. */
      if (pchan) {
        bActionConstraint *data = static_cast<bActionConstraint *>(con->data);
        data->mix_mode = ACTCON_MIX_BEFORE_SPLIT;
        con->ownspace = CONSTRAINT_SPACE_LOCAL;
      }
      break;
    }
  }

  return con;
}

bool BKE_constraint_target_uses_bbone(bConstraint *con, bConstraintTarget *ct)
{
  if (ct->flag & CONSTRAINT_TAR_CUSTOM_SPACE) {
    return false;
  }

  return (con->flag & CONSTRAINT_BBONE_SHAPE) || (con->type == CONSTRAINT_TYPE_ARMATURE);
}

/* ......... */

bConstraint *BKE_constraint_add_for_pose(Object *ob,
                                         bPoseChannel *pchan,
                                         const char *name,
                                         short type)
{
  if (pchan == nullptr) {
    return nullptr;
  }

  return add_new_constraint(ob, pchan, name, type);
}

bConstraint *BKE_constraint_add_for_object(Object *ob, const char *name, short type)
{
  return add_new_constraint(ob, nullptr, name, type);
}

/* ......... */

void BKE_constraints_id_loop(ListBase *conlist,
                             ConstraintIDFunc func,
                             const int flag,
                             void *userdata)
{
  LISTBASE_FOREACH (bConstraint *, con, conlist) {
    const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);

    if (cti) {
      con_invoke_id_looper(cti, con, func, flag, userdata);
    }
  }
}

/* ......... */

/* helper for BKE_constraints_copy(), to be used for making sure that ID's are valid */
static void con_extern_cb(bConstraint * /*con*/,
                          ID **idpoin,
                          bool /*is_reference*/,
                          void * /*user_data*/)
{
  if (*idpoin && ID_IS_LINKED(*idpoin)) {
    id_lib_extern(*idpoin);
  }
}

/**
 * Helper for #BKE_constraints_copy(),
 * to be used for making sure that user-counts of copied ID's are fixed up.
 */
static void con_fix_copied_refs_cb(bConstraint * /*con*/,
                                   ID **idpoin,
                                   bool is_reference,
                                   void * /*user_data*/)
{
  /* Increment user-count if this is a reference type. */
  if ((*idpoin) && (is_reference)) {
    id_us_plus(*idpoin);
  }
}

/** Copies a single constraint's data (\a dst must already be a shallow copy of \a src). */
static void constraint_copy_data_ex(bConstraint *dst,
                                    bConstraint *src,
                                    const int flag,
                                    const bool do_extern)
{
  const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(src);

  /* make a new copy of the constraint's data */
  dst->data = MEM_dupallocN(dst->data);

  /* only do specific constraints if required */
  if (cti) {
    /* perform custom copying operations if needed */
    if (cti->copy_data) {
      cti->copy_data(dst, src);
    }

    /* Fix user-counts for all referenced data that need it. */
    if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
      con_invoke_id_looper(cti, dst, con_fix_copied_refs_cb, IDWALK_NOP, nullptr);
    }

    /* For proxies we don't want to make external. */
    if (do_extern) {
      /* go over used ID-links for this constraint to ensure that they are valid for proxies */
      con_invoke_id_looper(cti, dst, con_extern_cb, IDWALK_NOP, nullptr);
    }
  }
}

bConstraint *BKE_constraint_duplicate_ex(bConstraint *src, const int flag, const bool do_extern)
{
  bConstraint *dst = static_cast<bConstraint *>(MEM_dupallocN(src));
  constraint_copy_data_ex(dst, src, flag, do_extern);
  dst->next = dst->prev = nullptr;
  return dst;
}

bConstraint *BKE_constraint_copy_for_pose(Object *ob, bPoseChannel *pchan, bConstraint *src)
{
  if (pchan == nullptr) {
    return nullptr;
  }

  bConstraint *new_con = BKE_constraint_duplicate_ex(src, 0, ID_IS_EDITABLE(ob));
  add_new_constraint_to_list(ob, pchan, new_con);
  return new_con;
}

bConstraint *BKE_constraint_copy_for_object(Object *ob, bConstraint *src)
{
  bConstraint *new_con = BKE_constraint_duplicate_ex(src, 0, ID_IS_EDITABLE(ob));
  add_new_constraint_to_list(ob, nullptr, new_con);
  return new_con;
}

void BKE_constraints_copy_ex(ListBase *dst, const ListBase *src, const int flag, bool do_extern)
{
  bConstraint *con, *srccon;

  BLI_listbase_clear(dst);
  BLI_duplicatelist(dst, src);

  for (con = static_cast<bConstraint *>(dst->first),
      srccon = static_cast<bConstraint *>(src->first);
       con && srccon;
       srccon = srccon->next, con = con->next)
  {
    constraint_copy_data_ex(con, srccon, flag, do_extern);
    if ((flag & LIB_ID_COPY_NO_LIB_OVERRIDE_LOCAL_DATA_FLAG) == 0) {
      con->flag |= CONSTRAINT_OVERRIDE_LIBRARY_LOCAL;
    }
  }
}

void BKE_constraints_copy(ListBase *dst, const ListBase *src, bool do_extern)
{
  BKE_constraints_copy_ex(dst, src, 0, do_extern);
}

/* ......... */

bConstraint *BKE_constraints_find_name(ListBase *list, const char *name)
{
  return static_cast<bConstraint *>(BLI_findstring(list, name, offsetof(bConstraint, name)));
}

bConstraint *BKE_constraints_active_get(ListBase *list)
{

  /* search for the first constraint with the 'active' flag set */
  if (list) {
    LISTBASE_FOREACH (bConstraint *, con, list) {
      if (con->flag & CONSTRAINT_ACTIVE) {
        return con;
      }
    }
  }

  /* no active constraint found */
  return nullptr;
}

void BKE_constraints_active_set(ListBase *list, bConstraint *con)
{

  if (list) {
    LISTBASE_FOREACH (bConstraint *, con_iter, list) {
      if (con_iter == con) {
        con_iter->flag |= CONSTRAINT_ACTIVE;
      }
      else {
        con_iter->flag &= ~CONSTRAINT_ACTIVE;
      }
    }
  }
}

static bConstraint *constraint_list_find_from_target(ListBase *constraints, bConstraintTarget *tgt)
{
  LISTBASE_FOREACH (bConstraint *, con, constraints) {
    ListBase *targets = nullptr;

    if (con->type == CONSTRAINT_TYPE_ARMATURE) {
      targets = &((bArmatureConstraint *)con->data)->targets;
    }

    if (targets && BLI_findindex(targets, tgt) != -1) {
      return con;
    }
  }

  return nullptr;
}

bConstraint *BKE_constraint_find_from_target(Object *ob,
                                             bConstraintTarget *tgt,
                                             bPoseChannel **r_pchan)
{
  if (r_pchan != nullptr) {
    *r_pchan = nullptr;
  }

  bConstraint *result = constraint_list_find_from_target(&ob->constraints, tgt);

  if (result != nullptr) {
    return result;
  }

  if (ob->pose != nullptr) {
    LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
      result = constraint_list_find_from_target(&pchan->constraints, tgt);

      if (result != nullptr) {
        if (r_pchan != nullptr) {
          *r_pchan = pchan;
        }

        return result;
      }
    }
  }

  return nullptr;
}

/* Finds the original copy of the constraint based on an evaluated copy. */
static bConstraint *constraint_find_original(Object *ob,
                                             bPoseChannel *pchan,
                                             bConstraint *con,
                                             Object **r_orig_ob)
{
  Object *orig_ob = DEG_get_original(ob);

  if (ELEM(orig_ob, nullptr, ob)) {
    return nullptr;
  }

  /* Find which constraint list to use. */
  ListBase *constraints, *orig_constraints;

  if (pchan != nullptr) {
    bPoseChannel *orig_pchan = pchan->orig_pchan;

    if (orig_pchan == nullptr) {
      return nullptr;
    }

    constraints = &pchan->constraints;
    orig_constraints = &orig_pchan->constraints;
  }
  else {
    constraints = &ob->constraints;
    orig_constraints = &orig_ob->constraints;
  }

  /* Lookup the original constraint by index. */
  int index = BLI_findindex(constraints, con);

  if (index >= 0) {
    bConstraint *orig_con = static_cast<bConstraint *>(BLI_findlink(orig_constraints, index));

    /* Verify it has correct type and name. */
    if (orig_con && orig_con->type == con->type && STREQ(orig_con->name, con->name)) {
      if (r_orig_ob != nullptr) {
        *r_orig_ob = orig_ob;
      }

      return orig_con;
    }
  }

  return nullptr;
}

static bConstraint *constraint_find_original_for_update(bConstraintOb *cob, bConstraint *con)
{
  /* Write the computed distance back to the master copy if in copy-on-eval evaluation. */
  if (!DEG_is_active(cob->depsgraph)) {
    return nullptr;
  }

  Object *orig_ob = nullptr;
  bConstraint *orig_con = constraint_find_original(cob->ob, cob->pchan, con, &orig_ob);

  if (orig_con != nullptr) {
    DEG_id_tag_update(&orig_ob->id, ID_RECALC_SYNC_TO_EVAL | ID_RECALC_TRANSFORM);
  }

  return orig_con;
}

bool BKE_constraint_is_nonlocal_in_liboverride(const Object *ob, const bConstraint *con)
{
  return (ID_IS_OVERRIDE_LIBRARY(ob) &&
          (con == nullptr || (con->flag & CONSTRAINT_OVERRIDE_LIBRARY_LOCAL) == 0));
}

/* -------- Target-Matrix Stuff ------- */

int BKE_constraint_targets_get(bConstraint *con, ListBase *r_targets)
{
  BLI_listbase_clear(r_targets);

  const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);

  if (!cti) {
    return 0;
  }

  int count = 0;

  /* Constraint-specific targets. */
  if (cti->get_constraint_targets) {
    count = cti->get_constraint_targets(con, r_targets);
  }

  /* Add the custom target. */
  if (is_custom_space_needed(con)) {
    bConstraintTarget *ct;
    SINGLETARGET_GET_TARS(con, con->space_object, con->space_subtarget, ct, r_targets);
    ct->space = CONSTRAINT_SPACE_WORLD;
    ct->flag |= CONSTRAINT_TAR_CUSTOM_SPACE;
    count++;
  }

  return count;
}

void BKE_constraint_targets_flush(bConstraint *con, ListBase *targets, bool no_copy)
{
  const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);

  if (!cti) {
    return;
  }

  /* Remove the custom target. */
  bConstraintTarget *ct = (bConstraintTarget *)targets->last;

  if (ct && (ct->flag & CONSTRAINT_TAR_CUSTOM_SPACE)) {
    BLI_assert(is_custom_space_needed(con));

    if (!no_copy) {
      con->space_object = ct->tar;
      STRNCPY_UTF8(con->space_subtarget, ct->subtarget);
    }

    BLI_freelinkN(targets, ct);
  }

  /* Release the constraint-specific targets. */
  if (cti->flush_constraint_targets) {
    cti->flush_constraint_targets(con, targets, no_copy);
  }
}

void BKE_constraint_target_matrix_get(Depsgraph *depsgraph,
                                      Scene *scene,
                                      bConstraint *con,
                                      int index,
                                      short ownertype,
                                      void *ownerdata,
                                      float mat[4][4],
                                      float ctime)
{
  const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);
  ListBase targets = {nullptr, nullptr};
  bConstraintOb *cob;
  bConstraintTarget *ct;

  if (cti && cti->get_constraint_targets) {
    /* make 'constraint-ob' */
    cob = MEM_callocN<bConstraintOb>("tempConstraintOb");
    cob->type = ownertype;
    cob->scene = scene;
    cob->depsgraph = depsgraph;
    switch (ownertype) {
      case CONSTRAINT_OBTYPE_OBJECT: /* it is usually this case */
      {
        cob->ob = (Object *)ownerdata;
        cob->pchan = nullptr;
        if (cob->ob) {
          copy_m4_m4(cob->matrix, cob->ob->object_to_world().ptr());
          copy_m4_m4(cob->startmat, cob->matrix);
        }
        else {
          unit_m4(cob->matrix);
          unit_m4(cob->startmat);
        }
        break;
      }
      case CONSTRAINT_OBTYPE_BONE: /* this may occur in some cases */
      {
        cob->ob = nullptr; /* this might not work at all :/ */
        cob->pchan = (bPoseChannel *)ownerdata;
        if (cob->pchan) {
          copy_m4_m4(cob->matrix, cob->pchan->pose_mat);
          copy_m4_m4(cob->startmat, cob->matrix);
        }
        else {
          unit_m4(cob->matrix);
          unit_m4(cob->startmat);
        }
        break;
      }
    }

    /* Initialize the custom space for use in calculating the matrices. */
    BKE_constraint_custom_object_space_init(cob, con);

    /* get targets - we only need the first one though (and there should only be one) */
    cti->get_constraint_targets(con, &targets);

    /* only calculate the target matrix on the first target */
    ct = static_cast<bConstraintTarget *>(BLI_findlink(&targets, index));

    if (ct) {
      if (cti->get_target_matrix) {
        cti->get_target_matrix(depsgraph, con, cob, ct, ctime);
      }
      copy_m4_m4(mat, ct->matrix);
    }

    /* free targets + 'constraint-ob' */
    if (cti->flush_constraint_targets) {
      cti->flush_constraint_targets(con, &targets, true);
    }
    MEM_freeN(cob);
  }
  else {
    /* invalid constraint - perhaps... */
    unit_m4(mat);
  }
}

void BKE_constraint_targets_for_solving_get(
    Depsgraph *depsgraph, bConstraint *con, bConstraintOb *cob, ListBase *targets, float ctime)
{
  const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);

  if (cti && cti->get_constraint_targets) {
    /* get targets
     * - constraints should use ct->matrix, not directly accessing values
     * - ct->matrix members have not yet been calculated here!
     */
    cti->get_constraint_targets(con, targets);

    /* The Armature constraint doesn't need ct->matrix for evaluate at all. */
    if (ELEM(cti->type, CONSTRAINT_TYPE_ARMATURE)) {
      return;
    }

    /* set matrices
     * - calculate if possible, otherwise just initialize as identity matrix
     */
    if (cti->get_target_matrix) {
      LISTBASE_FOREACH (bConstraintTarget *, ct, targets) {
        cti->get_target_matrix(depsgraph, con, cob, ct, ctime);
      }
    }
    else {
      LISTBASE_FOREACH (bConstraintTarget *, ct, targets) {
        unit_m4(ct->matrix);
      }
    }
  }
}

void BKE_constraint_custom_object_space_init(bConstraintOb *cob, bConstraint *con)
{
  if (con && con->space_object && is_custom_space_needed(con)) {
    /* Basically default_get_tarmat but without the unused parameters. */
    constraint_target_to_mat4(con->space_object,
                              con->space_subtarget,
                              nullptr,
                              cob->space_obj_world_matrix,
                              CONSTRAINT_SPACE_WORLD,
                              CONSTRAINT_SPACE_WORLD,
                              0,
                              0);

    return;
  }

  unit_m4(cob->space_obj_world_matrix);
}

/* ---------- Evaluation ----------- */

void BKE_constraints_solve(Depsgraph *depsgraph,
                           ListBase *conlist,
                           bConstraintOb *cob,
                           float ctime)
{
  float oldmat[4][4];
  float enf;

  /* check that there is a valid constraint object to evaluate */
  if (cob == nullptr) {
    return;
  }

  /* loop over available constraints, solving and blending them */
  LISTBASE_FOREACH (bConstraint *, con, conlist) {
    const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);
    ListBase targets = {nullptr, nullptr};

    /* these we can skip completely (invalid constraints...) */
    if (cti == nullptr) {
      continue;
    }
    if (con->flag & (CONSTRAINT_DISABLE | CONSTRAINT_OFF)) {
      continue;
    }
    /* these constraints can't be evaluated anyway */
    if (cti->evaluate_constraint == nullptr) {
      continue;
    }
    /* influence == 0 should be ignored */
    if (con->enforce == 0.0f) {
      continue;
    }

    /* influence of constraint
     * - value should have been set from animation data already
     */
    enf = con->enforce;

    /* Initialize the custom space for use in calculating the matrices. */
    BKE_constraint_custom_object_space_init(cob, con);

    /* make copy of world-space matrix pre-constraint for use with blending later */
    copy_m4_m4(oldmat, cob->matrix);

    /* move owner matrix into right space */
    BKE_constraint_mat_convertspace(
        cob->ob, cob->pchan, cob, cob->matrix, CONSTRAINT_SPACE_WORLD, con->ownspace, false);

    /* prepare targets for constraint solving */
    BKE_constraint_targets_for_solving_get(depsgraph, con, cob, &targets, ctime);

    /* Solve the constraint and put result in cob->matrix */
    cti->evaluate_constraint(con, cob, &targets);

    /* clear targets after use
     * - this should free temp targets but no data should be copied back
     *   as constraints may have done some nasty things to it...
     */
    if (cti->flush_constraint_targets) {
      cti->flush_constraint_targets(con, &targets, true);
    }

    /* move owner back into world-space for next constraint/other business */
    if ((con->flag & CONSTRAINT_SPACEONCE) == 0) {
      BKE_constraint_mat_convertspace(
          cob->ob, cob->pchan, cob, cob->matrix, con->ownspace, CONSTRAINT_SPACE_WORLD, false);
    }

    /* Interpolate the enforcement, to blend result of constraint into final owner transform
     * - all this happens in world-space to prevent any weirdness creeping in
     *   (#26014 and #25725), since some constraints may not convert the solution back to the input
     *   space before blending but all are guaranteed to end up in good "world-space" result.
     */
    /* NOTE: all kind of stuff here before (caused trouble), much easier to just interpolate,
     * or did I miss something? -jahka (r.32105) */
    if (enf < 1.0f) {
      float solution[4][4];
      copy_m4_m4(solution, cob->matrix);
      interp_m4_m4m4(cob->matrix, oldmat, solution, enf);
    }
  }
}

void BKE_constraint_blend_write(BlendWriter *writer, ListBase *conlist)
{
  LISTBASE_FOREACH (bConstraint *, con, conlist) {
    const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);

    /* Write the specific data */
    if (cti && con->data) {
      /* firstly, just write the plain con->data struct */
      BLO_write_struct_by_name(writer, cti->struct_name, con->data);

      /* do any constraint specific stuff */
      switch (con->type) {
        case CONSTRAINT_TYPE_ARMATURE: {
          bArmatureConstraint *data = static_cast<bArmatureConstraint *>(con->data);

          /* write targets */
          LISTBASE_FOREACH (bConstraintTarget *, ct, &data->targets) {
            BLO_write_struct(writer, bConstraintTarget, ct);
          }

          break;
        }
        case CONSTRAINT_TYPE_SPLINEIK: {
          bSplineIKConstraint *data = static_cast<bSplineIKConstraint *>(con->data);

          /* write points array */
          BLO_write_float_array(writer, data->numpoints, data->points);

          break;
        }
        case CONSTRAINT_TYPE_GEOMETRY_ATTRIBUTE: {
          bGeometryAttributeConstraint *data = static_cast<bGeometryAttributeConstraint *>(
              con->data);
          BLO_write_string(writer, data->attribute_name);
          break;
        }
      }
    }

    /* Write the constraint */
    BLO_write_struct(writer, bConstraint, con);
  }
}

void BKE_constraint_blend_read_data(BlendDataReader *reader, ID *id_owner, ListBase *lb)
{
  BLO_read_struct_list(reader, bConstraint, lb);
  LISTBASE_FOREACH (bConstraint *, con, lb) {
    const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);
    if (cti) {
      con->data = BLO_read_struct_by_name_array(reader, cti->struct_name, 1, con->data);
    }
    else {
      /* No `BLI_assert_unreachable()` here, this code can be reached in some cases, like the
       * deprecated RigidBody constraint. */
      con->data = nullptr;
    }

    /* Patch for error introduced by changing constraints (don't know how). */
    /* NOTE(@ton): If `con->data` type changes, DNA cannot resolve the pointer!. */
    if (con->data == nullptr) {
      con->type = CONSTRAINT_TYPE_NULL;
    }

    /* If linking from a library, clear 'local' library override flag. */
    if (ID_IS_LINKED(id_owner)) {
      con->flag &= ~CONSTRAINT_OVERRIDE_LIBRARY_LOCAL;
    }

    switch (con->type) {
      case CONSTRAINT_TYPE_ARMATURE: {
        bArmatureConstraint *data = static_cast<bArmatureConstraint *>(con->data);

        BLO_read_struct_list(reader, bConstraintTarget, &data->targets);

        break;
      }
      case CONSTRAINT_TYPE_SPLINEIK: {
        bSplineIKConstraint *data = static_cast<bSplineIKConstraint *>(con->data);

        BLO_read_float_array(reader, data->numpoints, &data->points);
        break;
      }
      case CONSTRAINT_TYPE_KINEMATIC: {
        bKinematicConstraint *data = static_cast<bKinematicConstraint *>(con->data);

        con->lin_error = 0.0f;
        con->rot_error = 0.0f;

        /* version patch for runtime flag, was not cleared in some case */
        data->flag &= ~CONSTRAINT_IK_AUTO;
        break;
      }
      case CONSTRAINT_TYPE_TRANSFORM_CACHE: {
        bTransformCacheConstraint *data = static_cast<bTransformCacheConstraint *>(con->data);
        data->reader = nullptr;
        data->reader_object_path[0] = '\0';
        break;
      }
      case CONSTRAINT_TYPE_GEOMETRY_ATTRIBUTE: {
        bGeometryAttributeConstraint *data = static_cast<bGeometryAttributeConstraint *>(
            con->data);
        BLO_read_string(reader, &data->attribute_name);
        break;
      }
    }
  }
}

/* Some static asserts to ensure that the bActionConstraint data is using the expected types for
 * some of the fields. This check is done here instead of in DNA_constraint_types.h to avoid the
 * inclusion of an DNA_anim_types.h in DNA_constraint_types.h just for this assert. */
static_assert(
    std::is_same_v<decltype(ActionSlot::handle), decltype(bActionConstraint::action_slot_handle)>);
static_assert(std::is_same_v<decltype(ActionSlot::identifier),
                             decltype(bActionConstraint::last_slot_identifier)>);
