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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2005 by the Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup modifiers
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BLI_math.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_deform.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"

#include "MOD_util.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include "DEG_depsgraph_query.h"

static void initData(ModifierData *md)
{
  BevelModifierData *bmd = (BevelModifierData *)md;

  bmd->value = 0.1f;
  bmd->res = 1;
  bmd->flags = 0;
  bmd->val_flags = MOD_BEVEL_AMT_OFFSET;
  bmd->lim_flags = 0;
  bmd->e_flags = 0;
  bmd->edge_flags = 0;
  bmd->face_str_mode = MOD_BEVEL_FACE_STRENGTH_NONE;
  bmd->miter_inner = MOD_BEVEL_MITER_SHARP;
  bmd->miter_outer = MOD_BEVEL_MITER_SHARP;
  bmd->spread = 0.1f;
  bmd->mat = -1;
  bmd->profile = 0.5f;
  bmd->bevel_angle = DEG2RADF(30.0f);
  bmd->defgrp_name[0] = '\0';
}

static void copyData(const ModifierData *md_src, ModifierData *md_dst, const int flag)
{
  modifier_copyData_generic(md_src, md_dst, flag);
}

static void requiredDataMask(Object *UNUSED(ob),
                             ModifierData *md,
                             CustomData_MeshMasks *r_cddata_masks)
{
  BevelModifierData *bmd = (BevelModifierData *)md;

  /* ask for vertexgroups if we need them */
  if (bmd->defgrp_name[0] != '\0') {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }
}

/*
 * This calls the new bevel code (added since 2.64)
 */
static Mesh *applyModifier(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  Mesh *result;
  BMesh *bm;
  BMIter iter;
  BMEdge *e;
  BMVert *v;
  float weight, weight2;
  int vgroup = -1;
  MDeformVert *dvert = NULL;
  BevelModifierData *bmd = (BevelModifierData *)md;
  const float threshold = cosf(bmd->bevel_angle + 0.000000175f);
  const bool vertex_only = (bmd->flags & MOD_BEVEL_VERT) != 0;
  const bool do_clamp = !(bmd->flags & MOD_BEVEL_OVERLAP_OK);
  const int offset_type = bmd->val_flags;
  const float value = bmd->value;
  const int mat = CLAMPIS(bmd->mat, -1, ctx->object->totcol - 1);
  const bool loop_slide = (bmd->flags & MOD_BEVEL_EVEN_WIDTHS) == 0;
  const bool mark_seam = (bmd->edge_flags & MOD_BEVEL_MARK_SEAM);
  const bool mark_sharp = (bmd->edge_flags & MOD_BEVEL_MARK_SHARP);
  bool harden_normals = (bmd->flags & MOD_BEVEL_HARDEN_NORMALS);
  const int face_strength_mode = bmd->face_str_mode;
  const int miter_outer = bmd->miter_outer;
  const int miter_inner = bmd->miter_inner;
  const float spread = bmd->spread;

  bm = BKE_mesh_to_bmesh_ex(mesh,
                            &(struct BMeshCreateParams){0},
                            &(struct BMeshFromMeshParams){
                                .calc_face_normal = true,
                                .add_key_index = false,
                                .use_shapekey = false,
                                .active_shapekey = 0,
                                /* XXX We probably can use CD_MASK_BAREMESH_ORIGDINDEX here instead
                                 * (also for other modifiers cases)? */
                                .cd_mask_extra = {.vmask = CD_MASK_ORIGINDEX,
                                                  .emask = CD_MASK_ORIGINDEX,
                                                  .pmask = CD_MASK_ORIGINDEX},
                            });

  if ((bmd->lim_flags & MOD_BEVEL_VGROUP) && bmd->defgrp_name[0]) {
    MOD_get_vgroup(ctx->object, mesh, bmd->defgrp_name, &dvert, &vgroup);
  }

  if (vertex_only) {
    BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
      if (!BM_vert_is_manifold(v)) {
        continue;
      }
      if (bmd->lim_flags & MOD_BEVEL_WEIGHT) {
        weight = BM_elem_float_data_get(&bm->vdata, v, CD_BWEIGHT);
        if (weight == 0.0f) {
          continue;
        }
      }
      else if (vgroup != -1) {
        weight = defvert_array_find_weight_safe(dvert, BM_elem_index_get(v), vgroup);
        /* Check is against 0.5 rather than != 0.0 because cascaded bevel modifiers will
         * interpolate weights for newly created vertices, and may cause unexpected "selection" */
        if (weight < 0.5f) {
          continue;
        }
      }
      BM_elem_flag_enable(v, BM_ELEM_TAG);
    }
  }
  else if (bmd->lim_flags & MOD_BEVEL_ANGLE) {
    BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
      /* check for 1 edge having 2 face users */
      BMLoop *l_a, *l_b;
      if (BM_edge_loop_pair(e, &l_a, &l_b)) {
        if (dot_v3v3(l_a->f->no, l_b->f->no) < threshold) {
          BM_elem_flag_enable(e, BM_ELEM_TAG);
          BM_elem_flag_enable(e->v1, BM_ELEM_TAG);
          BM_elem_flag_enable(e->v2, BM_ELEM_TAG);
        }
      }
    }
  }
  else {
    /* crummy, is there a way just to operator on all? - campbell */
    BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
      if (BM_edge_is_manifold(e)) {
        if (bmd->lim_flags & MOD_BEVEL_WEIGHT) {
          weight = BM_elem_float_data_get(&bm->edata, e, CD_BWEIGHT);
          if (weight == 0.0f) {
            continue;
          }
        }
        else if (vgroup != -1) {
          weight = defvert_array_find_weight_safe(dvert, BM_elem_index_get(e->v1), vgroup);
          weight2 = defvert_array_find_weight_safe(dvert, BM_elem_index_get(e->v2), vgroup);
          if (weight < 0.5f || weight2 < 0.5f) {
            continue;
          }
        }
        BM_elem_flag_enable(e, BM_ELEM_TAG);
        BM_elem_flag_enable(e->v1, BM_ELEM_TAG);
        BM_elem_flag_enable(e->v2, BM_ELEM_TAG);
      }
    }
  }

  if (harden_normals && !(((Mesh *)ctx->object->data)->flag & ME_AUTOSMOOTH)) {
    modifier_setError(md, "Enable 'Auto Smooth' option in mesh settings for hardening");
    harden_normals = false;
  }

  BM_mesh_bevel(bm,
                value,
                offset_type,
                bmd->res,
                bmd->profile,
                vertex_only,
                bmd->lim_flags & MOD_BEVEL_WEIGHT,
                do_clamp,
                dvert,
                vgroup,
                mat,
                loop_slide,
                mark_seam,
                mark_sharp,
                harden_normals,
                face_strength_mode,
                miter_outer,
                miter_inner,
                spread,
                mesh->smoothresh);

  result = BKE_mesh_from_bmesh_for_eval_nomain(bm, NULL);

  BLI_assert(bm->vtoolflagpool == NULL && bm->etoolflagpool == NULL &&
             bm->ftoolflagpool == NULL); /* make sure we never alloc'd these */
  BM_mesh_free(bm);

  result->runtime.cd_dirty_vert |= CD_MASK_NORMAL;

  return result;
}

static bool dependsOnNormals(ModifierData *UNUSED(md))
{
  return true;
}

ModifierTypeInfo modifierType_Bevel = {
    /* name */ "Bevel",
    /* structName */ "BevelModifierData",
    /* structSize */ sizeof(BevelModifierData),
    /* type */ eModifierTypeType_Constructive,
    /* flags */ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsEditmode |
        eModifierTypeFlag_EnableInEditmode | eModifierTypeFlag_AcceptsCVs,

    /* copyData */ copyData,

    /* deformVerts */ NULL,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* applyModifier */ applyModifier,

    /* initData */ initData,
    /* requiredDataMask */ requiredDataMask,
    /* freeData */ NULL,
    /* isDisabled */ NULL,
    /* updateDepsgraph */ NULL,
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ dependsOnNormals,
    /* foreachObjectLink */ NULL,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
};
