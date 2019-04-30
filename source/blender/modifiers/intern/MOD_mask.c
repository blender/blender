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

#include "BLI_listbase.h"
#include "BLI_ghash.h"

#include "DNA_armature_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BKE_action.h" /* BKE_pose_channel_find_name */
#include "BKE_customdata.h"
#include "BKE_library_query.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_deform.h"

#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "MOD_modifiertypes.h"

#include "BLI_strict_flags.h"

static void requiredDataMask(Object *UNUSED(ob),
                             ModifierData *UNUSED(md),
                             CustomData_MeshMasks *r_cddata_masks)
{
  r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
}

static void foreachObjectLink(ModifierData *md, Object *ob, ObjectWalkFunc walk, void *userData)
{
  MaskModifierData *mmd = (MaskModifierData *)md;
  walk(userData, ob, &mmd->ob_arm, IDWALK_CB_NOP);
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  MaskModifierData *mmd = (MaskModifierData *)md;
  if (mmd->ob_arm) {
    bArmature *arm = (bArmature *)mmd->ob_arm->data;
    /* Tag relationship in depsgraph, but also on the armature. */
    /* TODO(sergey): Is it a proper relation here? */
    DEG_add_object_relation(ctx->node, mmd->ob_arm, DEG_OB_COMP_TRANSFORM, "Mask Modifier");
    arm->flag |= ARM_HAS_VIZ_DEPS;
    DEG_add_modifier_to_transform_relation(ctx->node, "Mask Modifier");
  }
}

static Mesh *applyModifier(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  MaskModifierData *mmd = (MaskModifierData *)md;
  Object *ob = ctx->object;
  const bool found_test = (mmd->flag & MOD_MASK_INV) == 0;
  Mesh *result = NULL;
  GHash *vertHash = NULL, *edgeHash, *polyHash;
  GHashIterator gh_iter;
  MDeformVert *dvert, *dv;
  int numPolys = 0, numLoops = 0, numEdges = 0, numVerts = 0;
  int maxVerts, maxEdges, maxPolys;
  int i;

  const MVert *mvert_src;
  const MEdge *medge_src;
  const MPoly *mpoly_src;
  const MLoop *mloop_src;

  MPoly *mpoly_dst;
  MLoop *mloop_dst;
  MEdge *medge_dst;
  MVert *mvert_dst;

  int *loop_mapping;

  dvert = CustomData_get_layer(&mesh->vdata, CD_MDEFORMVERT);
  if (dvert == NULL) {
    return found_test ? BKE_mesh_new_nomain_from_template(mesh, 0, 0, 0, 0, 0) : mesh;
  }

  /* Overview of Method:
   * 1. Get the vertices that are in the vertexgroup of interest.
   * 2. Filter out unwanted geometry (i.e. not in vertexgroup),
   *    by populating mappings with new vs old indices.
   * 3. Make a new mesh containing only the mapping data.
   */

  /* get original number of verts, edges, and faces */
  maxVerts = mesh->totvert;
  maxEdges = mesh->totedge;
  maxPolys = mesh->totpoly;

  /* check if we can just return the original mesh
   * - must have verts and therefore verts assigned to vgroups to do anything useful
   */
  if (!(ELEM(mmd->mode, MOD_MASK_MODE_ARM, MOD_MASK_MODE_VGROUP)) || (maxVerts == 0) ||
      BLI_listbase_is_empty(&ob->defbase)) {
    return mesh;
  }

  /* if mode is to use selected armature bones, aggregate the bone groups */
  if (mmd->mode == MOD_MASK_MODE_ARM) { /* --- using selected bones --- */
    Object *oba = mmd->ob_arm;
    bPoseChannel *pchan;
    bDeformGroup *def;
    bool *bone_select_array;
    int bone_select_tot = 0;
    const int defbase_tot = BLI_listbase_count(&ob->defbase);

    /* check that there is armature object with bones to use, otherwise return original mesh */
    if (ELEM(NULL, oba, oba->pose, ob->defbase.first)) {
      return mesh;
    }

    /* Determine whether each vertexgroup is associated with a selected bone or not:
     * - Each cell is a boolean saying whether bone corresponding to the ith group is selected.
     * - Groups that don't match a bone are treated as not existing
     *   (along with the corresponding ungrouped verts).
     */
    bone_select_array = MEM_malloc_arrayN((size_t)defbase_tot, sizeof(char), "mask array");

    for (i = 0, def = ob->defbase.first; def; def = def->next, i++) {
      pchan = BKE_pose_channel_find_name(oba->pose, def->name);
      if (pchan && pchan->bone && (pchan->bone->flag & BONE_SELECTED)) {
        bone_select_array[i] = true;
        bone_select_tot++;
      }
      else {
        bone_select_array[i] = false;
      }
    }

    /* verthash gives mapping from original vertex indices to the new indices
     * (including selected matches only):
     * key = oldindex, value = newindex
     */
    vertHash = BLI_ghash_int_new_ex("mask vert gh", (unsigned int)maxVerts);

    /* add vertices which exist in vertexgroups into vertHash for filtering
     * - dv = for each vertex, what vertexgroups does it belong to
     * - dw = weight that vertex was assigned to a vertexgroup it belongs to
     */
    for (i = 0, dv = dvert; i < maxVerts; i++, dv++) {
      MDeformWeight *dw = dv->dw;
      bool found = false;
      int j;

      /* check the groups that vertex is assigned to, and see if it was any use */
      for (j = 0; j < dv->totweight; j++, dw++) {
        if (dw->def_nr < defbase_tot) {
          if (bone_select_array[dw->def_nr]) {
            if (dw->weight > mmd->threshold) {
              found = true;
              break;
            }
          }
        }
      }

      if (found_test != found) {
        continue;
      }

      /* add to ghash for verts (numVerts acts as counter for mapping) */
      BLI_ghash_insert(vertHash, POINTER_FROM_INT(i), POINTER_FROM_INT(numVerts));
      numVerts++;
    }

    /* free temp hashes */
    MEM_freeN(bone_select_array);
  }
  else { /* --- Using Nominated VertexGroup only --- */
    int defgrp_index = defgroup_name_index(ob, mmd->vgroup);

    /* if no vgroup (i.e. dverts) found, return the initial mesh */
    if (defgrp_index == -1) {
      return mesh;
    }

    /* hashes for quickly providing a mapping from old to new - use key=oldindex, value=newindex */
    vertHash = BLI_ghash_int_new_ex("mask vert2 bh", (unsigned int)maxVerts);

    /* add vertices which exist in vertexgroup into ghash for filtering */
    for (i = 0, dv = dvert; i < maxVerts; i++, dv++) {
      const bool found = defvert_find_weight(dv, defgrp_index) > mmd->threshold;
      if (found_test != found) {
        continue;
      }

      /* add to ghash for verts (numVerts acts as counter for mapping) */
      BLI_ghash_insert(vertHash, POINTER_FROM_INT(i), POINTER_FROM_INT(numVerts));
      numVerts++;
    }
  }

  /* hashes for quickly providing a mapping from old to new - use key=oldindex, value=newindex */
  edgeHash = BLI_ghash_int_new_ex("mask ed2 gh", (unsigned int)maxEdges);
  polyHash = BLI_ghash_int_new_ex("mask fa2 gh", (unsigned int)maxPolys);

  mvert_src = mesh->mvert;
  medge_src = mesh->medge;
  mpoly_src = mesh->mpoly;
  mloop_src = mesh->mloop;

  /* overalloc, assume all polys are seen */
  loop_mapping = MEM_malloc_arrayN((size_t)maxPolys, sizeof(int), "mask loopmap");

  /* loop over edges and faces, and do the same thing to
   * ensure that they only reference existing verts
   */
  for (i = 0; i < maxEdges; i++) {
    const MEdge *me = &medge_src[i];

    /* only add if both verts will be in new mesh */
    if (BLI_ghash_haskey(vertHash, POINTER_FROM_INT(me->v1)) &&
        BLI_ghash_haskey(vertHash, POINTER_FROM_INT(me->v2))) {
      BLI_ghash_insert(edgeHash, POINTER_FROM_INT(i), POINTER_FROM_INT(numEdges));
      numEdges++;
    }
  }
  for (i = 0; i < maxPolys; i++) {
    const MPoly *mp_src = &mpoly_src[i];
    const MLoop *ml_src = &mloop_src[mp_src->loopstart];
    bool ok = true;
    int j;

    for (j = 0; j < mp_src->totloop; j++, ml_src++) {
      if (!BLI_ghash_haskey(vertHash, POINTER_FROM_INT(ml_src->v))) {
        ok = false;
        break;
      }
    }

    /* all verts must be available */
    if (ok) {
      BLI_ghash_insert(polyHash, POINTER_FROM_INT(i), POINTER_FROM_INT(numPolys));
      loop_mapping[numPolys] = numLoops;
      numPolys++;
      numLoops += mp_src->totloop;
    }
  }

  /* now we know the number of verts, edges and faces,
   * we can create the new (reduced) mesh
   */
  result = BKE_mesh_new_nomain_from_template(mesh, numVerts, numEdges, 0, numLoops, numPolys);

  mpoly_dst = result->mpoly;
  mloop_dst = result->mloop;
  medge_dst = result->medge;
  mvert_dst = result->mvert;

  /* using ghash-iterators, map data into new mesh */
  /* vertices */
  GHASH_ITER (gh_iter, vertHash) {
    const MVert *v_src;
    MVert *v_dst;
    const int i_src = POINTER_AS_INT(BLI_ghashIterator_getKey(&gh_iter));
    const int i_dst = POINTER_AS_INT(BLI_ghashIterator_getValue(&gh_iter));

    v_src = &mvert_src[i_src];
    v_dst = &mvert_dst[i_dst];

    *v_dst = *v_src;
    CustomData_copy_data(&mesh->vdata, &result->vdata, i_src, i_dst, 1);
  }

  /* edges */
  GHASH_ITER (gh_iter, edgeHash) {
    const MEdge *e_src;
    MEdge *e_dst;
    const int i_src = POINTER_AS_INT(BLI_ghashIterator_getKey(&gh_iter));
    const int i_dst = POINTER_AS_INT(BLI_ghashIterator_getValue(&gh_iter));

    e_src = &medge_src[i_src];
    e_dst = &medge_dst[i_dst];

    CustomData_copy_data(&mesh->edata, &result->edata, i_src, i_dst, 1);
    *e_dst = *e_src;
    e_dst->v1 = POINTER_AS_UINT(BLI_ghash_lookup(vertHash, POINTER_FROM_UINT(e_src->v1)));
    e_dst->v2 = POINTER_AS_UINT(BLI_ghash_lookup(vertHash, POINTER_FROM_UINT(e_src->v2)));
  }

  /* faces */
  GHASH_ITER (gh_iter, polyHash) {
    const int i_src = POINTER_AS_INT(BLI_ghashIterator_getKey(&gh_iter));
    const int i_dst = POINTER_AS_INT(BLI_ghashIterator_getValue(&gh_iter));
    const MPoly *mp_src = &mpoly_src[i_src];
    MPoly *mp_dst = &mpoly_dst[i_dst];
    const int i_ml_src = mp_src->loopstart;
    const int i_ml_dst = loop_mapping[i_dst];
    const MLoop *ml_src = &mloop_src[i_ml_src];
    MLoop *ml_dst = &mloop_dst[i_ml_dst];

    CustomData_copy_data(&mesh->pdata, &result->pdata, i_src, i_dst, 1);
    CustomData_copy_data(&mesh->ldata, &result->ldata, i_ml_src, i_ml_dst, mp_src->totloop);

    *mp_dst = *mp_src;
    mp_dst->loopstart = i_ml_dst;
    for (i = 0; i < mp_src->totloop; i++) {
      ml_dst[i].v = POINTER_AS_UINT(BLI_ghash_lookup(vertHash, POINTER_FROM_UINT(ml_src[i].v)));
      ml_dst[i].e = POINTER_AS_UINT(BLI_ghash_lookup(edgeHash, POINTER_FROM_UINT(ml_src[i].e)));
    }
  }

  MEM_freeN(loop_mapping);

  /* why is this needed? - campbell */
  /* recalculate normals */
  result->runtime.cd_dirty_vert |= CD_MASK_NORMAL;

  /* free hashes */
  BLI_ghash_free(vertHash, NULL, NULL);
  BLI_ghash_free(edgeHash, NULL, NULL);
  BLI_ghash_free(polyHash, NULL, NULL);

  /* return the new mesh */
  return result;
}

ModifierTypeInfo modifierType_Mask = {
    /* name */ "Mask",
    /* structName */ "MaskModifierData",
    /* structSize */ sizeof(MaskModifierData),
    /* type */ eModifierTypeType_Nonconstructive,
    /* flags */ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping |
        eModifierTypeFlag_SupportsEditmode,

    /* copyData */ modifier_copyData_generic,

    /* deformVerts */ NULL,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* applyModifier */ applyModifier,

    /* initData */ NULL,
    /* requiredDataMask */ requiredDataMask,
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
