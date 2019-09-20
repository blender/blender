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
 */

/** \file
 * \ingroup modifiers
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_modifier.h"
#include "BKE_mesh.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include "MOD_modifiertypes.h"

static Mesh *triangulate_mesh(Mesh *mesh,
                              const int quad_method,
                              const int ngon_method,
                              const int min_vertices,
                              const int flag)
{
  Mesh *result;
  BMesh *bm;
  int total_edges, i;
  MEdge *me;
  CustomData_MeshMasks cddata_masks = {
      .vmask = CD_MASK_ORIGINDEX, .emask = CD_MASK_ORIGINDEX, .pmask = CD_MASK_ORIGINDEX};

  bool keep_clnors = (flag & MOD_TRIANGULATE_KEEP_CUSTOMLOOP_NORMALS) != 0;

  if (keep_clnors) {
    BKE_mesh_calc_normals_split(mesh);
    /* We need that one to 'survive' to/from BMesh conversions. */
    CustomData_clear_layer_flag(&mesh->ldata, CD_NORMAL, CD_FLAG_TEMPORARY);
    cddata_masks.lmask |= CD_MASK_NORMAL;
  }

  bm = BKE_mesh_to_bmesh_ex(mesh,
                            &((struct BMeshCreateParams){0}),
                            &((struct BMeshFromMeshParams){
                                .calc_face_normal = true,
                                .cd_mask_extra = cddata_masks,
                            }));

  BM_mesh_triangulate(bm, quad_method, ngon_method, min_vertices, false, NULL, NULL, NULL);

  result = BKE_mesh_from_bmesh_for_eval_nomain(bm, &cddata_masks);
  BM_mesh_free(bm);

  if (keep_clnors) {
    float(*lnors)[3] = CustomData_get_layer(&result->ldata, CD_NORMAL);
    BLI_assert(lnors != NULL);

    BKE_mesh_set_custom_normals(result, lnors);

    /* Do some cleanup, we do not want those temp data to stay around. */
    CustomData_set_layer_flag(&mesh->ldata, CD_NORMAL, CD_FLAG_TEMPORARY);
    CustomData_set_layer_flag(&result->ldata, CD_NORMAL, CD_FLAG_TEMPORARY);
  }

  total_edges = result->totedge;
  me = result->medge;

  /* force drawing of all edges (seems to be omitted in CDDM_from_bmesh) */
  for (i = 0; i < total_edges; i++, me++) {
    me->flag |= ME_EDGEDRAW | ME_EDGERENDER;
  }

  result->runtime.cd_dirty_vert |= CD_MASK_NORMAL;

  return result;
}

static void initData(ModifierData *md)
{
  TriangulateModifierData *tmd = (TriangulateModifierData *)md;

  /* Enable in editmode by default */
  md->mode |= eModifierMode_Editmode;
  tmd->quad_method = MOD_TRIANGULATE_QUAD_SHORTEDGE;
  tmd->ngon_method = MOD_TRIANGULATE_NGON_BEAUTY;
  tmd->min_vertices = 4;
}

static Mesh *applyModifier(ModifierData *md, const ModifierEvalContext *UNUSED(ctx), Mesh *mesh)
{
  TriangulateModifierData *tmd = (TriangulateModifierData *)md;
  Mesh *result;
  if (!(result = triangulate_mesh(
            mesh, tmd->quad_method, tmd->ngon_method, tmd->min_vertices, tmd->flag))) {
    return mesh;
  }

  return result;
}

ModifierTypeInfo modifierType_Triangulate = {
    /* name */ "Triangulate",
    /* structName */ "TriangulateModifierData",
    /* structSize */ sizeof(TriangulateModifierData),
    /* type */ eModifierTypeType_Constructive,
    /* flags */ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsEditmode |
        eModifierTypeFlag_SupportsMapping | eModifierTypeFlag_EnableInEditmode |
        eModifierTypeFlag_AcceptsCVs,

    /* copyData */ modifier_copyData_generic,

    /* deformVerts */ NULL,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* applyModifier */ applyModifier,

    /* initData */ initData,
    /* requiredDataMask */ NULL,  // requiredDataMask,
    /* freeData */ NULL,
    /* isDisabled */ NULL,
    /* updateDepsgraph */ NULL,
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ NULL,
    /* foreachObjectLink */ NULL,
    /* foreachIDLink */ NULL,
    /* freeRuntimeData */ NULL,
};
