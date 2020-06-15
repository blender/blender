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
 * The Original Code is Copyright (C) 2005 by the Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup modifiers
 */

// #ifdef DEBUG_TIME

#include <stdio.h>

#include "BLI_utildefines.h"

#include "BLI_alloca.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"

#include "BLT_translation.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_global.h" /* only to check G.debug */
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "MOD_ui_common.h"
#include "MOD_util.h"

#include "DEG_depsgraph_query.h"

#include "MEM_guardedalloc.h"

#include "bmesh.h"
#include "bmesh_tools.h"
#include "tools/bmesh_intersect.h"

#ifdef DEBUG_TIME
#  include "PIL_time.h"
#  include "PIL_time_utildefines.h"
#endif

static void initData(ModifierData *md)
{
  BooleanModifierData *bmd = (BooleanModifierData *)md;

  bmd->double_threshold = 1e-6f;
  bmd->operation = eBooleanModifierOp_Difference;
}

static bool isDisabled(const struct Scene *UNUSED(scene),
                       ModifierData *md,
                       bool UNUSED(useRenderParams))
{
  BooleanModifierData *bmd = (BooleanModifierData *)md;

  /* The object type check is only needed here in case we have a placeholder
   * object assigned (because the library containing the mesh is missing).
   *
   * In other cases it should be impossible to have a type mismatch.
   */
  return !bmd->object || bmd->object->type != OB_MESH;
}

static void foreachObjectLink(ModifierData *md, Object *ob, ObjectWalkFunc walk, void *userData)
{
  BooleanModifierData *bmd = (BooleanModifierData *)md;

  walk(userData, ob, &bmd->object, IDWALK_CB_NOP);
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  BooleanModifierData *bmd = (BooleanModifierData *)md;
  if (bmd->object != NULL) {
    DEG_add_object_relation(ctx->node, bmd->object, DEG_OB_COMP_TRANSFORM, "Boolean Modifier");
    DEG_add_object_relation(ctx->node, bmd->object, DEG_OB_COMP_GEOMETRY, "Boolean Modifier");
  }
  /* We need own transformation as well. */
  DEG_add_modifier_to_transform_relation(ctx->node, "Boolean Modifier");
}

static Mesh *get_quick_mesh(
    Object *ob_self, Mesh *mesh_self, Object *ob_other, Mesh *mesh_other, int operation)
{
  Mesh *result = NULL;

  if (mesh_self->totpoly == 0 || mesh_other->totpoly == 0) {
    switch (operation) {
      case eBooleanModifierOp_Intersect:
        result = BKE_mesh_new_nomain(0, 0, 0, 0, 0);
        break;

      case eBooleanModifierOp_Union:
        if (mesh_self->totpoly != 0) {
          result = mesh_self;
        }
        else {
          BKE_id_copy_ex(NULL, &mesh_other->id, (ID **)&result, LIB_ID_COPY_LOCALIZE);

          float imat[4][4];
          float omat[4][4];

          invert_m4_m4(imat, ob_self->obmat);
          mul_m4_m4m4(omat, imat, ob_other->obmat);

          const int mverts_len = result->totvert;
          MVert *mv = result->mvert;

          for (int i = 0; i < mverts_len; i++, mv++) {
            mul_m4_v3(omat, mv->co);
          }

          result->runtime.cd_dirty_vert |= CD_MASK_NORMAL;
        }

        break;

      case eBooleanModifierOp_Difference:
        result = mesh_self;
        break;
    }
  }

  return result;
}

/* has no meaning for faces, do this so we can tell which face is which */
#define BM_FACE_TAG BM_ELEM_DRAW

/**
 * Compare selected/unselected.
 */
static int bm_face_isect_pair(BMFace *f, void *UNUSED(user_data))
{
  return BM_elem_flag_test(f, BM_FACE_TAG) ? 1 : 0;
}

static Mesh *modifyMesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  BooleanModifierData *bmd = (BooleanModifierData *)md;
  Mesh *result = mesh;

  Mesh *mesh_other;

  if (bmd->object == NULL) {
    return result;
  }

  Object *other = bmd->object;
  mesh_other = BKE_modifier_get_evaluated_mesh_from_evaluated_object(other, false);
  if (mesh_other) {
    Object *object = ctx->object;

    /* when one of objects is empty (has got no faces) we could speed up
     * calculation a bit returning one of objects' derived meshes (or empty one)
     * Returning mesh is depended on modifiers operation (sergey) */
    result = get_quick_mesh(object, mesh, other, mesh_other, bmd->operation);

    if (result == NULL) {
      const bool is_flip = (is_negative_m4(object->obmat) != is_negative_m4(other->obmat));

      BMesh *bm;
      const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(mesh, mesh_other);

#ifdef DEBUG_TIME
      TIMEIT_START(boolean_bmesh);
#endif
      bm = BM_mesh_create(&allocsize,
                          &((struct BMeshCreateParams){
                              .use_toolflags = false,
                          }));

      BM_mesh_bm_from_me(bm,
                         mesh_other,
                         &((struct BMeshFromMeshParams){
                             .calc_face_normal = true,
                         }));

      if (UNLIKELY(is_flip)) {
        const int cd_loop_mdisp_offset = CustomData_get_offset(&bm->ldata, CD_MDISPS);
        BMIter iter;
        BMFace *efa;
        BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
          BM_face_normal_flip_ex(bm, efa, cd_loop_mdisp_offset, true);
        }
      }

      BM_mesh_bm_from_me(bm,
                         mesh,
                         &((struct BMeshFromMeshParams){
                             .calc_face_normal = true,
                         }));

      /* main bmesh intersection setup */
      {
        /* create tessface & intersect */
        const int looptris_tot = poly_to_tri_count(bm->totface, bm->totloop);
        int tottri;
        BMLoop *(*looptris)[3];

        looptris = MEM_malloc_arrayN(looptris_tot, sizeof(*looptris), __func__);

        BM_mesh_calc_tessellation_beauty(bm, looptris, &tottri);

        /* postpone this until after tessellating
         * so we can use the original normals before the vertex are moved */
        {
          BMIter iter;
          int i;
          const int i_verts_end = mesh_other->totvert;
          const int i_faces_end = mesh_other->totpoly;

          float imat[4][4];
          float omat[4][4];

          invert_m4_m4(imat, object->obmat);
          mul_m4_m4m4(omat, imat, other->obmat);

          BMVert *eve;
          i = 0;
          BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
            mul_m4_v3(omat, eve->co);
            if (++i == i_verts_end) {
              break;
            }
          }

          /* we need face normals because of 'BM_face_split_edgenet'
           * we could calculate on the fly too (before calling split). */
          {
            float nmat[3][3];
            copy_m3_m4(nmat, omat);
            invert_m3(nmat);

            if (UNLIKELY(is_flip)) {
              negate_m3(nmat);
            }

            const short ob_src_totcol = other->totcol;
            short *material_remap = BLI_array_alloca(material_remap,
                                                     ob_src_totcol ? ob_src_totcol : 1);

            /* Using original (not evaluated) object here since we are writing to it. */
            /* XXX Pretty sure comment above is fully wrong now with CoW & co ? */
            BKE_object_material_remap_calc(ctx->object, other, material_remap);

            BMFace *efa;
            i = 0;
            BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
              mul_transposed_m3_v3(nmat, efa->no);
              normalize_v3(efa->no);

              /* Temp tag to test which side split faces are from. */
              BM_elem_flag_enable(efa, BM_FACE_TAG);

              /* remap material */
              if (LIKELY(efa->mat_nr < ob_src_totcol)) {
                efa->mat_nr = material_remap[efa->mat_nr];
              }

              if (++i == i_faces_end) {
                break;
              }
            }
          }
        }

        /* not needed, but normals for 'dm' will be invalid,
         * currently this is ok for 'BM_mesh_intersect' */
        // BM_mesh_normals_update(bm);

        bool use_separate = false;
        bool use_dissolve = true;
        bool use_island_connect = true;

        /* change for testing */
        if (G.debug & G_DEBUG) {
          use_separate = (bmd->bm_flag & eBooleanModifierBMeshFlag_BMesh_Separate) != 0;
          use_dissolve = (bmd->bm_flag & eBooleanModifierBMeshFlag_BMesh_NoDissolve) == 0;
          use_island_connect = (bmd->bm_flag & eBooleanModifierBMeshFlag_BMesh_NoConnectRegions) ==
                               0;
        }

        BM_mesh_intersect(bm,
                          looptris,
                          tottri,
                          bm_face_isect_pair,
                          NULL,
                          false,
                          use_separate,
                          use_dissolve,
                          use_island_connect,
                          false,
                          false,
                          bmd->operation,
                          bmd->double_threshold);

        MEM_freeN(looptris);
      }

      result = BKE_mesh_from_bmesh_for_eval_nomain(bm, NULL, mesh);

      BM_mesh_free(bm);

      result->runtime.cd_dirty_vert |= CD_MASK_NORMAL;

#ifdef DEBUG_TIME
      TIMEIT_END(boolean_bmesh);
#endif
    }

    /* if new mesh returned, return it; otherwise there was
     * an error, so delete the modifier object */
    if (result == NULL) {
      BKE_modifier_set_error(md, "Cannot execute boolean operation");
    }
  }

  return result;
}

static void requiredDataMask(Object *UNUSED(ob),
                             ModifierData *UNUSED(md),
                             CustomData_MeshMasks *r_cddata_masks)
{
  r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  r_cddata_masks->emask |= CD_MASK_MEDGE;
  r_cddata_masks->fmask |= CD_MASK_MTFACE;
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, &ptr, "operation", 0, NULL, ICON_NONE);
  uiItemR(layout, &ptr, "object", 0, NULL, ICON_NONE);
  uiItemR(layout, &ptr, "double_threshold", 0, NULL, ICON_NONE);

  if (G.debug) {
    uiLayout *col = uiLayoutColumn(layout, true);
    uiItemR(col, &ptr, "debug_options", 0, NULL, ICON_NONE);
  }

  modifier_panel_end(layout, &ptr);
}

static void panelRegister(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_Boolean, panel_draw);
}

ModifierTypeInfo modifierType_Boolean = {
    /* name */ "Boolean",
    /* structName */ "BooleanModifierData",
    /* structSize */ sizeof(BooleanModifierData),
    /* type */ eModifierTypeType_Nonconstructive,
    /* flags */ eModifierTypeFlag_AcceptsMesh,

    /* copyData */ BKE_modifier_copydata_generic,

    /* deformVerts */ NULL,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* modifyMesh */ modifyMesh,
    /* modifyHair */ NULL,
    /* modifyPointCloud */ NULL,
    /* modifyVolume */ NULL,

    /* initData */ initData,
    /* requiredDataMask */ requiredDataMask,
    /* freeData */ NULL,
    /* isDisabled */ isDisabled,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ NULL,
    /* foreachObjectLink */ foreachObjectLink,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
    /* panelRegister */ panelRegister,
    /* blendWrite */ NULL,
    /* blendRead */ NULL,
};
