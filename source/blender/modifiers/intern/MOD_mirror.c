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

#include "BLI_math.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_library.h"
#include "BKE_library_query.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_deform.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include "MEM_guardedalloc.h"

#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "MOD_modifiertypes.h"

static void initData(ModifierData *md)
{
  MirrorModifierData *mmd = (MirrorModifierData *)md;

  mmd->flag |= (MOD_MIR_AXIS_X | MOD_MIR_VGROUP);
  mmd->tolerance = 0.001;
  mmd->mirror_ob = NULL;
}

static void foreachObjectLink(ModifierData *md, Object *ob, ObjectWalkFunc walk, void *userData)
{
  MirrorModifierData *mmd = (MirrorModifierData *)md;

  walk(userData, ob, &mmd->mirror_ob, IDWALK_CB_NOP);
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  MirrorModifierData *mmd = (MirrorModifierData *)md;
  if (mmd->mirror_ob != NULL) {
    DEG_add_object_relation(ctx->node, mmd->mirror_ob, DEG_OB_COMP_TRANSFORM, "Mirror Modifier");
    DEG_add_modifier_to_transform_relation(ctx->node, "Mirror Modifier");
  }
}

static Mesh *doBiscetOnMirrorPlane(
    MirrorModifierData *mmd, const Mesh *mesh, int axis, float plane_co[3], float plane_no[3])
{
  bool do_bisect_flip_axis = ((axis == 0 && mmd->flag & MOD_MIR_BISECT_FLIP_AXIS_X) ||
                              (axis == 1 && mmd->flag & MOD_MIR_BISECT_FLIP_AXIS_Y) ||
                              (axis == 2 && mmd->flag & MOD_MIR_BISECT_FLIP_AXIS_Z));

  const float bisect_distance = 0.001f;

  Mesh *result;
  BMesh *bm;
  BMIter viter;
  BMVert *v, *v_next;

  bm = BKE_mesh_to_bmesh_ex(mesh,
                            &(struct BMeshCreateParams){0},
                            &(struct BMeshFromMeshParams){
                                .calc_face_normal = true,
                                .cd_mask_extra = {.vmask = CD_MASK_ORIGINDEX,
                                                  .emask = CD_MASK_ORIGINDEX,
                                                  .pmask = CD_MASK_ORIGINDEX},
                            });

  /* Define bisecting plane (aka mirror plane). */
  float plane[4];
  if (!do_bisect_flip_axis) {
    /* That reversed condition is a tad weird, but for some reason that's how you keep
     * the part of the mesh which is on the non-mirrored side when flip option is disabled,
     * think that that is the expected behavior. */
    negate_v3(plane_no);
  }
  plane_from_point_normal_v3(plane, plane_co, plane_no);

  BM_mesh_bisect_plane(bm, plane, false, false, 0, 0, bisect_distance);

  /* Plane definitions for vert killing. */
  float plane_offset[4];
  copy_v3_v3(plane_offset, plane);
  plane_offset[3] = plane[3] - bisect_distance;

  /* Delete verts across the mirror plane. */
  BM_ITER_MESH_MUTABLE (v, v_next, &viter, bm, BM_VERTS_OF_MESH) {
    if (plane_point_side_v3(plane_offset, v->co) > 0.0f) {
      BM_vert_kill(bm, v);
    }
  }

  result = BKE_mesh_from_bmesh_for_eval_nomain(bm, NULL);
  BM_mesh_free(bm);

  return result;
}

static Mesh *doMirrorOnAxis(MirrorModifierData *mmd,
                            const ModifierEvalContext *UNUSED(ctx),
                            Object *ob,
                            const Mesh *mesh,
                            int axis)
{
  const float tolerance_sq = mmd->tolerance * mmd->tolerance;
  const bool do_vtargetmap = (mmd->flag & MOD_MIR_NO_MERGE) == 0;
  int tot_vtargetmap = 0; /* total merge vertices */

  const bool do_bisect = ((axis == 0 && mmd->flag & MOD_MIR_BISECT_AXIS_X) ||
                          (axis == 1 && mmd->flag & MOD_MIR_BISECT_AXIS_Y) ||
                          (axis == 2 && mmd->flag & MOD_MIR_BISECT_AXIS_Z));

  Mesh *result;
  MVert *mv, *mv_prev;
  MEdge *me;
  MLoop *ml;
  MPoly *mp;
  float mtx[4][4];
  float plane_co[3], plane_no[3];
  int i;
  int a, totshape;
  int *vtargetmap = NULL, *vtmap_a = NULL, *vtmap_b = NULL;

  /* mtx is the mirror transformation */
  unit_m4(mtx);
  mtx[axis][axis] = -1.0f;

  Object *mirror_ob = mmd->mirror_ob;
  if (mirror_ob != NULL) {
    float tmp[4][4];
    float itmp[4][4];

    /* tmp is a transform from coords relative to the object's own origin,
     * to coords relative to the mirror object origin */
    invert_m4_m4(tmp, mirror_ob->obmat);
    mul_m4_m4m4(tmp, tmp, ob->obmat);

    /* itmp is the reverse transform back to origin-relative coordinates */
    invert_m4_m4(itmp, tmp);

    /* combine matrices to get a single matrix that translates coordinates into
     * mirror-object-relative space, does the mirror, and translates back to
     * origin-relative space */
    mul_m4_series(mtx, itmp, mtx, tmp);

    if (do_bisect) {
      copy_v3_v3(plane_co, itmp[3]);
      copy_v3_v3(plane_no, itmp[axis]);
    }
  }
  else if (do_bisect) {
    copy_v3_v3(plane_co, mtx[3]);
    /* Need to negate here, since that axis is inverted (for mirror transform). */
    negate_v3_v3(plane_no, mtx[axis]);
  }

  Mesh *mesh_bisect = NULL;
  if (do_bisect) {
    mesh_bisect = doBiscetOnMirrorPlane(mmd, mesh, axis, plane_co, plane_no);
    mesh = mesh_bisect;
  }

  const int maxVerts = mesh->totvert;
  const int maxEdges = mesh->totedge;
  const int maxLoops = mesh->totloop;
  const int maxPolys = mesh->totpoly;

  result = BKE_mesh_new_nomain_from_template(
      mesh, maxVerts * 2, maxEdges * 2, 0, maxLoops * 2, maxPolys * 2);

  /*copy customdata to original geometry*/
  CustomData_copy_data(&mesh->vdata, &result->vdata, 0, 0, maxVerts);
  CustomData_copy_data(&mesh->edata, &result->edata, 0, 0, maxEdges);
  CustomData_copy_data(&mesh->ldata, &result->ldata, 0, 0, maxLoops);
  CustomData_copy_data(&mesh->pdata, &result->pdata, 0, 0, maxPolys);

  /* Subsurf for eg won't have mesh data in the custom data arrays.
   * now add mvert/medge/mpoly layers. */
  if (!CustomData_has_layer(&mesh->vdata, CD_MVERT)) {
    memcpy(result->mvert, mesh->mvert, sizeof(*result->mvert) * mesh->totvert);
  }
  if (!CustomData_has_layer(&mesh->edata, CD_MEDGE)) {
    memcpy(result->medge, mesh->medge, sizeof(*result->medge) * mesh->totedge);
  }
  if (!CustomData_has_layer(&mesh->pdata, CD_MPOLY)) {
    memcpy(result->mloop, mesh->mloop, sizeof(*result->mloop) * mesh->totloop);
    memcpy(result->mpoly, mesh->mpoly, sizeof(*result->mpoly) * mesh->totpoly);
  }

  /* copy customdata to new geometry,
   * copy from its self because this data may have been created in the checks above */
  CustomData_copy_data(&result->vdata, &result->vdata, 0, maxVerts, maxVerts);
  CustomData_copy_data(&result->edata, &result->edata, 0, maxEdges, maxEdges);
  /* loops are copied later */
  CustomData_copy_data(&result->pdata, &result->pdata, 0, maxPolys, maxPolys);

  if (do_vtargetmap) {
    /* second half is filled with -1 */
    vtargetmap = MEM_malloc_arrayN(maxVerts, 2 * sizeof(int), "MOD_mirror tarmap");

    vtmap_a = vtargetmap;
    vtmap_b = vtargetmap + maxVerts;
  }

  /* mirror vertex coordinates */
  mv_prev = result->mvert;
  mv = mv_prev + maxVerts;
  for (i = 0; i < maxVerts; i++, mv++, mv_prev++) {
    mul_m4_v3(mtx, mv->co);

    if (do_vtargetmap) {
      /* compare location of the original and mirrored vertex, to see if they
       * should be mapped for merging */
      if (UNLIKELY(len_squared_v3v3(mv_prev->co, mv->co) < tolerance_sq)) {
        *vtmap_a = maxVerts + i;
        tot_vtargetmap++;

        /* average location */
        mid_v3_v3v3(mv->co, mv_prev->co, mv->co);
        copy_v3_v3(mv_prev->co, mv->co);
      }
      else {
        *vtmap_a = -1;
      }

      *vtmap_b = -1; /* fill here to avoid 2x loops */

      vtmap_a++;
      vtmap_b++;
    }
  }

  /* handle shape keys */
  totshape = CustomData_number_of_layers(&result->vdata, CD_SHAPEKEY);
  for (a = 0; a < totshape; a++) {
    float(*cos)[3] = CustomData_get_layer_n(&result->vdata, CD_SHAPEKEY, a);
    for (i = maxVerts; i < result->totvert; i++) {
      mul_m4_v3(mtx, cos[i]);
    }
  }

  /* adjust mirrored edge vertex indices */
  me = result->medge + maxEdges;
  for (i = 0; i < maxEdges; i++, me++) {
    me->v1 += maxVerts;
    me->v2 += maxVerts;
  }

  /* adjust mirrored poly loopstart indices, and reverse loop order (normals) */
  mp = result->mpoly + maxPolys;
  ml = result->mloop;
  for (i = 0; i < maxPolys; i++, mp++) {
    MLoop *ml2;
    int j, e;

    /* reverse the loop, but we keep the first vertex in the face the same,
     * to ensure that quads are split the same way as on the other side */
    CustomData_copy_data(
        &result->ldata, &result->ldata, mp->loopstart, mp->loopstart + maxLoops, 1);

    for (j = 1; j < mp->totloop; j++) {
      CustomData_copy_data(&result->ldata,
                           &result->ldata,
                           mp->loopstart + j,
                           mp->loopstart + maxLoops + mp->totloop - j,
                           1);
    }

    ml2 = ml + mp->loopstart + maxLoops;
    e = ml2[0].e;
    for (j = 0; j < mp->totloop - 1; j++) {
      ml2[j].e = ml2[j + 1].e;
    }
    ml2[mp->totloop - 1].e = e;

    mp->loopstart += maxLoops;
  }

  /* adjust mirrored loop vertex and edge indices */
  ml = result->mloop + maxLoops;
  for (i = 0; i < maxLoops; i++, ml++) {
    ml->v += maxVerts;
    ml->e += maxEdges;
  }

  /* handle uvs,
   * let tessface recalc handle updating the MTFace data */
  if (mmd->flag & (MOD_MIR_MIRROR_U | MOD_MIR_MIRROR_V) ||
      (is_zero_v2(mmd->uv_offset_copy) == false)) {
    const bool do_mirr_u = (mmd->flag & MOD_MIR_MIRROR_U) != 0;
    const bool do_mirr_v = (mmd->flag & MOD_MIR_MIRROR_V) != 0;

    const int totuv = CustomData_number_of_layers(&result->ldata, CD_MLOOPUV);

    for (a = 0; a < totuv; a++) {
      MLoopUV *dmloopuv = CustomData_get_layer_n(&result->ldata, CD_MLOOPUV, a);
      int j = maxLoops;
      dmloopuv += j; /* second set of loops only */
      for (; j-- > 0; dmloopuv++) {
        if (do_mirr_u) {
          dmloopuv->uv[0] = 1.0f - dmloopuv->uv[0] + mmd->uv_offset[0];
        }
        if (do_mirr_v) {
          dmloopuv->uv[1] = 1.0f - dmloopuv->uv[1] + mmd->uv_offset[1];
        }
        dmloopuv->uv[0] += mmd->uv_offset_copy[0];
        dmloopuv->uv[1] += mmd->uv_offset_copy[1];
      }
    }
  }

  /* handle custom split normals */
  if (ob->type == OB_MESH && (((Mesh *)ob->data)->flag & ME_AUTOSMOOTH) &&
      CustomData_has_layer(&result->ldata, CD_CUSTOMLOOPNORMAL)) {
    const int totloop = result->totloop;
    const int totpoly = result->totpoly;
    float(*loop_normals)[3] = MEM_calloc_arrayN((size_t)totloop, sizeof(*loop_normals), __func__);
    CustomData *ldata = &result->ldata;
    short(*clnors)[2] = CustomData_get_layer(ldata, CD_CUSTOMLOOPNORMAL);
    MLoopNorSpaceArray lnors_spacearr = {NULL};
    float(*poly_normals)[3] = MEM_mallocN(sizeof(*poly_normals) * totpoly, __func__);

    /* calculate custom normals into loop_normals, then mirror first half into second half */

    BKE_mesh_calc_normals_poly(result->mvert,
                               NULL,
                               result->totvert,
                               result->mloop,
                               result->mpoly,
                               totloop,
                               totpoly,
                               poly_normals,
                               false);

    BKE_mesh_normals_loop_split(result->mvert,
                                result->totvert,
                                result->medge,
                                result->totedge,
                                result->mloop,
                                loop_normals,
                                totloop,
                                result->mpoly,
                                poly_normals,
                                totpoly,
                                true,
                                mesh->smoothresh,
                                &lnors_spacearr,
                                clnors,
                                NULL);

    /* mirroring has to account for loops being reversed in polys in second half */
    mp = result->mpoly;
    for (i = 0; i < maxPolys; i++, mp++) {
      MPoly *mpmirror = result->mpoly + maxPolys + i;
      int j;

      for (j = mp->loopstart; j < mp->loopstart + mp->totloop; j++) {
        int mirrorj = mpmirror->loopstart;
        if (j > mp->loopstart) {
          mirrorj += mpmirror->totloop - (j - mp->loopstart);
        }
        copy_v3_v3(loop_normals[mirrorj], loop_normals[j]);
        loop_normals[mirrorj][axis] = -loop_normals[j][axis];
        BKE_lnor_space_custom_normal_to_data(
            lnors_spacearr.lspacearr[mirrorj], loop_normals[mirrorj], clnors[mirrorj]);
      }
    }

    MEM_freeN(poly_normals);
    MEM_freeN(loop_normals);
    BKE_lnor_spacearr_free(&lnors_spacearr);
  }

  /* handle vgroup stuff */
  if ((mmd->flag & MOD_MIR_VGROUP) && CustomData_has_layer(&result->vdata, CD_MDEFORMVERT)) {
    MDeformVert *dvert = (MDeformVert *)CustomData_get_layer(&result->vdata, CD_MDEFORMVERT) +
                         maxVerts;
    int *flip_map = NULL, flip_map_len = 0;

    flip_map = defgroup_flip_map(ob, &flip_map_len, false);

    if (flip_map) {
      for (i = 0; i < maxVerts; dvert++, i++) {
        /* merged vertices get both groups, others get flipped */
        if (do_vtargetmap && (vtargetmap[i] != -1)) {
          defvert_flip_merged(dvert, flip_map, flip_map_len);
        }
        else {
          defvert_flip(dvert, flip_map, flip_map_len);
        }
      }

      MEM_freeN(flip_map);
    }
  }

  if (do_vtargetmap) {
    /* slow - so only call if one or more merge verts are found,
     * users may leave this on and not realize there is nothing to merge - campbell */
    if (tot_vtargetmap) {
      result = BKE_mesh_merge_verts(
          result, vtargetmap, tot_vtargetmap, MESH_MERGE_VERTS_DUMP_IF_MAPPED);
    }
    MEM_freeN(vtargetmap);
  }

  if (mesh_bisect != NULL) {
    BKE_id_free(NULL, mesh_bisect);
  }

  return result;
}

static Mesh *mirrorModifier__doMirror(MirrorModifierData *mmd,
                                      const ModifierEvalContext *ctx,
                                      Object *ob,
                                      Mesh *mesh)
{
  Mesh *result = mesh;

  /* check which axes have been toggled and mirror accordingly */
  if (mmd->flag & MOD_MIR_AXIS_X) {
    result = doMirrorOnAxis(mmd, ctx, ob, result, 0);
  }
  if (mmd->flag & MOD_MIR_AXIS_Y) {
    Mesh *tmp = result;
    result = doMirrorOnAxis(mmd, ctx, ob, result, 1);
    if (tmp != mesh) {
      /* free intermediate results */
      BKE_id_free(NULL, tmp);
    }
  }
  if (mmd->flag & MOD_MIR_AXIS_Z) {
    Mesh *tmp = result;
    result = doMirrorOnAxis(mmd, ctx, ob, result, 2);
    if (tmp != mesh) {
      /* free intermediate results */
      BKE_id_free(NULL, tmp);
    }
  }

  return result;
}

static Mesh *applyModifier(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  Mesh *result;
  MirrorModifierData *mmd = (MirrorModifierData *)md;

  result = mirrorModifier__doMirror(mmd, ctx, ctx->object, mesh);

  if (result != mesh) {
    result->runtime.cd_dirty_vert |= CD_MASK_NORMAL;
  }
  return result;
}

ModifierTypeInfo modifierType_Mirror = {
    /* name */ "Mirror",
    /* structName */ "MirrorModifierData",
    /* structSize */ sizeof(MirrorModifierData),
    /* type */ eModifierTypeType_Constructive,
    /* flags */ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping |
        eModifierTypeFlag_SupportsEditmode | eModifierTypeFlag_EnableInEditmode |
        eModifierTypeFlag_AcceptsCVs |
        /* this is only the case when 'MOD_MIR_VGROUP' is used */
        eModifierTypeFlag_UsesPreview,

    /* copyData */ modifier_copyData_generic,

    /* deformVerts */ NULL,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* applyModifier */ applyModifier,

    /* initData */ initData,
    /* requiredDataMask */ NULL,
    /* freeData */ NULL,
    /* isDisabled */ NULL,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ NULL,
    /* foreachObjectLink */ foreachObjectLink,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
};
