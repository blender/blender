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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_modifier_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_utildefines.h"
#include "BLI_linklist.h"
#include "BLI_math.h"

#include "BKE_crazyspace.h"
#include "BKE_DerivedMesh.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_mesh.h"
#include "BKE_editmesh.h"
#include "BKE_library.h"

#include "DEG_depsgraph_query.h"

BLI_INLINE void tan_calc_quat_v3(float r_quat[4],
                                 const float co_1[3],
                                 const float co_2[3],
                                 const float co_3[3])
{
  float vec_u[3], vec_v[3];
  float nor[3];

  sub_v3_v3v3(vec_u, co_1, co_2);
  sub_v3_v3v3(vec_v, co_1, co_3);

  cross_v3_v3v3(nor, vec_u, vec_v);

  if (normalize_v3(nor) > FLT_EPSILON) {
    const float zero_vec[3] = {0.0f};
    tri_to_quat_ex(r_quat, zero_vec, vec_u, vec_v, nor);
  }
  else {
    unit_qt(r_quat);
  }
}

static void set_crazy_vertex_quat(float r_quat[4],
                                  const float co_1[3],
                                  const float co_2[3],
                                  const float co_3[3],
                                  const float vd_1[3],
                                  const float vd_2[3],
                                  const float vd_3[3])
{
  float q1[4], q2[4];

  tan_calc_quat_v3(q1, co_1, co_2, co_3);
  tan_calc_quat_v3(q2, vd_1, vd_2, vd_3);

  sub_qt_qtqt(r_quat, q2, q1);
}

static int modifiers_disable_subsurf_temporary(Object *ob)
{
  ModifierData *md;
  int disabled = 0;

  for (md = ob->modifiers.first; md; md = md->next) {
    if (md->type == eModifierType_Subsurf) {
      if (md->mode & eModifierMode_OnCage) {
        md->mode ^= eModifierMode_DisableTemporary;
        disabled = 1;
      }
    }
  }

  return disabled;
}

/* disable subsurf temporal, get mapped cos, and enable it */
float (*BKE_crazyspace_get_mapped_editverts(struct Depsgraph *depsgraph,
                                            Scene *scene,
                                            Object *obedit))[3]
{
  Mesh *me = obedit->data;
  Mesh *me_eval;
  float(*vertexcos)[3];
  int nverts = me->edit_mesh->bm->totvert;

  /* disable subsurf temporal, get mapped cos, and enable it */
  if (modifiers_disable_subsurf_temporary(obedit)) {
    /* need to make new derivemesh */
    makeDerivedMesh(depsgraph, scene, obedit, me->edit_mesh, &CD_MASK_BAREMESH);
  }

  /* now get the cage */
  vertexcos = MEM_mallocN(sizeof(*vertexcos) * nverts, "vertexcos map");

  me_eval = editbmesh_get_eval_cage_from_orig(depsgraph, scene, obedit, &CD_MASK_BAREMESH);

  mesh_get_mapped_verts_coords(me_eval, vertexcos, nverts);

  /* set back the flag, no new cage needs to be built, transform does it */
  modifiers_disable_subsurf_temporary(obedit);

  return vertexcos;
}

void BKE_crazyspace_set_quats_editmesh(BMEditMesh *em,
                                       float (*origcos)[3],
                                       float (*mappedcos)[3],
                                       float (*quats)[4],
                                       const bool use_select)
{
  BMFace *f;
  BMIter iter;
  int index;

  {
    BMVert *v;
    BM_ITER_MESH_INDEX (v, &iter, em->bm, BM_VERTS_OF_MESH, index) {
      BM_elem_flag_disable(v, BM_ELEM_TAG);
      BM_elem_index_set(v, index); /* set_inline */
    }
    em->bm->elem_index_dirty &= ~BM_VERT;
  }

  BM_ITER_MESH (f, &iter, em->bm, BM_FACES_OF_MESH) {
    BMLoop *l_iter, *l_first;

    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      if (BM_elem_flag_test(l_iter->v, BM_ELEM_HIDDEN) ||
          BM_elem_flag_test(l_iter->v, BM_ELEM_TAG) ||
          (use_select && !BM_elem_flag_test(l_iter->v, BM_ELEM_SELECT))) {
        continue;
      }

      if (!BM_elem_flag_test(l_iter->v, BM_ELEM_TAG)) {
        const float *co_prev, *co_curr, *co_next; /* orig */
        const float *vd_prev, *vd_curr, *vd_next; /* deform */

        const int i_prev = BM_elem_index_get(l_iter->prev->v);
        const int i_curr = BM_elem_index_get(l_iter->v);
        const int i_next = BM_elem_index_get(l_iter->next->v);

        /* retrieve mapped coordinates */
        vd_prev = mappedcos[i_prev];
        vd_curr = mappedcos[i_curr];
        vd_next = mappedcos[i_next];

        if (origcos) {
          co_prev = origcos[i_prev];
          co_curr = origcos[i_curr];
          co_next = origcos[i_next];
        }
        else {
          co_prev = l_iter->prev->v->co;
          co_curr = l_iter->v->co;
          co_next = l_iter->next->v->co;
        }

        set_crazy_vertex_quat(quats[i_curr], co_curr, co_next, co_prev, vd_curr, vd_next, vd_prev);

        BM_elem_flag_enable(l_iter->v, BM_ELEM_TAG);
      }
    } while ((l_iter = l_iter->next) != l_first);
  }
}

void BKE_crazyspace_set_quats_mesh(Mesh *me,
                                   float (*origcos)[3],
                                   float (*mappedcos)[3],
                                   float (*quats)[4])
{
  int i;
  MVert *mvert;
  MLoop *mloop;
  MPoly *mp;

  mvert = me->mvert;
  for (i = 0; i < me->totvert; i++, mvert++) {
    mvert->flag &= ~ME_VERT_TMP_TAG;
  }

  /* first store two sets of tangent vectors in vertices, we derive it just from the face-edges */
  mvert = me->mvert;
  mp = me->mpoly;
  mloop = me->mloop;

  for (i = 0; i < me->totpoly; i++, mp++) {
    MLoop *ml_prev, *ml_curr, *ml_next;
    int j;

    ml_next = &mloop[mp->loopstart];
    ml_curr = &ml_next[mp->totloop - 1];
    ml_prev = &ml_next[mp->totloop - 2];

    for (j = 0; j < mp->totloop; j++) {
      if ((mvert[ml_curr->v].flag & ME_VERT_TMP_TAG) == 0) {
        const float *co_prev, *co_curr, *co_next; /* orig */
        const float *vd_prev, *vd_curr, *vd_next; /* deform */

        /* retrieve mapped coordinates */
        vd_prev = mappedcos[ml_prev->v];
        vd_curr = mappedcos[ml_curr->v];
        vd_next = mappedcos[ml_next->v];

        if (origcos) {
          co_prev = origcos[ml_prev->v];
          co_curr = origcos[ml_curr->v];
          co_next = origcos[ml_next->v];
        }
        else {
          co_prev = mvert[ml_prev->v].co;
          co_curr = mvert[ml_curr->v].co;
          co_next = mvert[ml_next->v].co;
        }

        set_crazy_vertex_quat(
            quats[ml_curr->v], co_curr, co_next, co_prev, vd_curr, vd_next, vd_prev);

        mvert[ml_curr->v].flag |= ME_VERT_TMP_TAG;
      }

      ml_prev = ml_curr;
      ml_curr = ml_next;
      ml_next++;
    }
  }
}

/** returns an array of deform matrices for crazyspace correction, and the
 * number of modifiers left */
int BKE_crazyspace_get_first_deform_matrices_editbmesh(struct Depsgraph *depsgraph,
                                                       Scene *scene,
                                                       Object *ob,
                                                       BMEditMesh *em,
                                                       float (**deformmats)[3][3],
                                                       float (**deformcos)[3])
{
  ModifierData *md;
  Mesh *me;
  int i, a, numleft = 0, numVerts = 0;
  int cageIndex = modifiers_getCageIndex(scene, ob, NULL, 1);
  float(*defmats)[3][3] = NULL, (*deformedVerts)[3] = NULL;
  VirtualModifierData virtualModifierData;
  ModifierEvalContext mectx = {depsgraph, ob, 0};

  modifiers_clearErrors(ob);

  me = NULL;
  md = modifiers_getVirtualModifierList(ob, &virtualModifierData);

  /* compute the deformation matrices and coordinates for the first
   * modifiers with on cage editing that are enabled and support computing
   * deform matrices */
  for (i = 0; md && i <= cageIndex; i++, md = md->next) {
    const ModifierTypeInfo *mti = modifierType_getInfo(md->type);

    if (!editbmesh_modifier_is_enabled(scene, md, me != NULL)) {
      continue;
    }

    if (mti->type == eModifierTypeType_OnlyDeform && mti->deformMatricesEM) {
      if (!defmats) {
        const int required_mode = eModifierMode_Realtime | eModifierMode_Editmode;
        CustomData_MeshMasks data_mask = CD_MASK_BAREMESH;
        CDMaskLink *datamasks = modifiers_calcDataMasks(
            scene, ob, md, &data_mask, required_mode, NULL, NULL);
        data_mask = datamasks->mask;
        BLI_linklist_free((LinkNode *)datamasks, NULL);

        me = BKE_mesh_from_editmesh_with_coords_thin_wrap(em, &data_mask, NULL);
        deformedVerts = editbmesh_get_vertex_cos(em, &numVerts);
        defmats = MEM_mallocN(sizeof(*defmats) * numVerts, "defmats");

        for (a = 0; a < numVerts; a++) {
          unit_m3(defmats[a]);
        }
      }
      mti->deformMatricesEM(md, &mectx, em, me, deformedVerts, defmats, numVerts);
    }
    else {
      break;
    }
  }

  for (; md && i <= cageIndex; md = md->next, i++) {
    if (editbmesh_modifier_is_enabled(scene, md, me != NULL) &&
        modifier_isCorrectableDeformed(md)) {
      numleft++;
    }
  }

  if (me) {
    BKE_id_free(NULL, me);
  }

  *deformmats = defmats;
  *deformcos = deformedVerts;

  return numleft;
}

/**
 * Crazyspace evaluation needs to have an object which has all the fields
 * evaluated, but the mesh data being at undeformed state. This way it can
 * re-apply modifiers and also have proper pointers to key data blocks.
 *
 * Similar to #BKE_object_eval_reset(), but does not modify the actual evaluated object.
 */
static void crazyspace_init_object_for_eval(struct Depsgraph *depsgraph,
                                            Object *object,
                                            Object *object_crazy)
{
  Object *object_eval = DEG_get_evaluated_object(depsgraph, object);
  *object_crazy = *object_eval;
  if (object_crazy->runtime.mesh_orig != NULL) {
    object_crazy->data = object_crazy->runtime.mesh_orig;
  }
}

int BKE_sculpt_get_first_deform_matrices(struct Depsgraph *depsgraph,
                                         Scene *scene,
                                         Object *object,
                                         float (**deformmats)[3][3],
                                         float (**deformcos)[3])
{
  ModifierData *md;
  Mesh *me_eval;
  int a, numVerts = 0;
  float(*defmats)[3][3] = NULL, (*deformedVerts)[3] = NULL;
  int numleft = 0;
  VirtualModifierData virtualModifierData;
  Object object_eval;
  crazyspace_init_object_for_eval(depsgraph, object, &object_eval);
  MultiresModifierData *mmd = get_multires_modifier(scene, &object_eval, 0);
  const bool has_multires = mmd != NULL && mmd->sculptlvl > 0;
  const ModifierEvalContext mectx = {depsgraph, &object_eval, 0};

  if (has_multires) {
    *deformmats = NULL;
    *deformcos = NULL;
    return numleft;
  }

  me_eval = NULL;

  md = modifiers_getVirtualModifierList(&object_eval, &virtualModifierData);

  for (; md; md = md->next) {
    const ModifierTypeInfo *mti = modifierType_getInfo(md->type);

    if (!modifier_isEnabled(scene, md, eModifierMode_Realtime)) {
      continue;
    }

    if (mti->type == eModifierTypeType_OnlyDeform) {
      if (!defmats) {
        /* NOTE: Evaluated object si re-set to its original undeformed
         * state. */
        Mesh *me = object_eval.data;
        me_eval = BKE_mesh_copy_for_eval(me, true);
        deformedVerts = BKE_mesh_vertexCos_get(me, &numVerts);
        defmats = MEM_callocN(sizeof(*defmats) * numVerts, "defmats");

        for (a = 0; a < numVerts; a++) {
          unit_m3(defmats[a]);
        }
      }

      if (mti->deformMatrices) {
        mti->deformMatrices(md, &mectx, me_eval, deformedVerts, defmats, numVerts);
      }
      else {
        break;
      }
    }
  }

  for (; md; md = md->next) {
    const ModifierTypeInfo *mti = modifierType_getInfo(md->type);

    if (!modifier_isEnabled(scene, md, eModifierMode_Realtime)) {
      continue;
    }

    if (mti->type == eModifierTypeType_OnlyDeform) {
      numleft++;
    }
  }

  if (me_eval) {
    BKE_id_free(NULL, me_eval);
  }

  *deformmats = defmats;
  *deformcos = deformedVerts;

  return numleft;
}

void BKE_crazyspace_build_sculpt(struct Depsgraph *depsgraph,
                                 Scene *scene,
                                 Object *object,
                                 float (**deformmats)[3][3],
                                 float (**deformcos)[3])
{
  int totleft = BKE_sculpt_get_first_deform_matrices(
      depsgraph, scene, object, deformmats, deformcos);

  if (totleft) {
    /* there are deformation modifier which doesn't support deformation matrices
     * calculation. Need additional crazyspace correction */

    float(*deformedVerts)[3] = *deformcos;
    float(*origVerts)[3] = MEM_dupallocN(deformedVerts);
    float(*quats)[4];
    int i, deformed = 0;
    VirtualModifierData virtualModifierData;
    Object object_eval;
    crazyspace_init_object_for_eval(depsgraph, object, &object_eval);
    ModifierData *md = modifiers_getVirtualModifierList(&object_eval, &virtualModifierData);
    const ModifierEvalContext mectx = {depsgraph, &object_eval, 0};
    Mesh *mesh = (Mesh *)object->data;

    for (; md; md = md->next) {
      const ModifierTypeInfo *mti = modifierType_getInfo(md->type);

      if (!modifier_isEnabled(scene, md, eModifierMode_Realtime)) {
        continue;
      }

      if (mti->type == eModifierTypeType_OnlyDeform) {
        /* skip leading modifiers which have been already
         * handled in sculpt_get_first_deform_matrices */
        if (mti->deformMatrices && !deformed) {
          continue;
        }

        mti->deformVerts(md, &mectx, NULL, deformedVerts, mesh->totvert);
        deformed = 1;
      }
    }

    quats = MEM_mallocN(mesh->totvert * sizeof(*quats), "crazy quats");

    BKE_crazyspace_set_quats_mesh(mesh, origVerts, deformedVerts, quats);

    for (i = 0; i < mesh->totvert; i++) {
      float qmat[3][3], tmat[3][3];

      quat_to_mat3(qmat, quats[i]);
      mul_m3_m3m3(tmat, qmat, (*deformmats)[i]);
      copy_m3_m3((*deformmats)[i], tmat);
    }

    MEM_freeN(origVerts);
    MEM_freeN(quats);
  }

  if (*deformmats == NULL) {
    int a, numVerts;
    Mesh *mesh = (Mesh *)object->data;

    *deformcos = BKE_mesh_vertexCos_get(mesh, &numVerts);
    *deformmats = MEM_callocN(sizeof(*(*deformmats)) * numVerts, "defmats");

    for (a = 0; a < numVerts; a++) {
      unit_m3((*deformmats)[a]);
    }
  }
}
