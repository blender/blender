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

#include "BLI_utildefines.h"

#include "BLI_math.h"

#include "BLT_translation.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "MEM_guardedalloc.h"

#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_mesh.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "DEG_depsgraph_query.h"

#include "bmesh.h"
#include "bmesh_tools.h"

// #define USE_TIMEIT

#ifdef USE_TIMEIT
#  include "PIL_time.h"
#  include "PIL_time_utildefines.h"
#endif

#include "MOD_ui_common.h"
#include "MOD_util.h"

static void initData(ModifierData *md)
{
  DecimateModifierData *dmd = (DecimateModifierData *)md;

  dmd->percent = 1.0;
  dmd->angle = DEG2RADF(5.0f);
  dmd->defgrp_factor = 1.0;
}

static void requiredDataMask(Object *UNUSED(ob),
                             ModifierData *md,
                             CustomData_MeshMasks *r_cddata_masks)
{
  DecimateModifierData *dmd = (DecimateModifierData *)md;

  /* ask for vertexgroups if we need them */
  if (dmd->defgrp_name[0] != '\0' && (dmd->defgrp_factor > 0.0f)) {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }
}

static DecimateModifierData *getOriginalModifierData(const DecimateModifierData *dmd,
                                                     const ModifierEvalContext *ctx)
{
  Object *ob_orig = DEG_get_original_object(ctx->object);
  return (DecimateModifierData *)BKE_modifiers_findby_name(ob_orig, dmd->modifier.name);
}

static void updateFaceCount(const ModifierEvalContext *ctx,
                            DecimateModifierData *dmd,
                            int face_count)
{
  dmd->face_count = face_count;

  if (DEG_is_active(ctx->depsgraph)) {
    /* update for display only */
    DecimateModifierData *dmd_orig = getOriginalModifierData(dmd, ctx);
    dmd_orig->face_count = face_count;
  }
}

static Mesh *modifyMesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *meshData)
{
  DecimateModifierData *dmd = (DecimateModifierData *)md;
  Mesh *mesh = meshData, *result = NULL;
  BMesh *bm;
  bool calc_face_normal;
  float *vweights = NULL;

#ifdef USE_TIMEIT
  TIMEIT_START(decim);
#endif

  /* set up front so we dont show invalid info in the UI */
  updateFaceCount(ctx, dmd, mesh->totpoly);

  switch (dmd->mode) {
    case MOD_DECIM_MODE_COLLAPSE:
      if (dmd->percent == 1.0f) {
        return mesh;
      }
      calc_face_normal = true;
      break;
    case MOD_DECIM_MODE_UNSUBDIV:
      if (dmd->iter == 0) {
        return mesh;
      }
      calc_face_normal = false;
      break;
    case MOD_DECIM_MODE_DISSOLVE:
      if (dmd->angle == 0.0f) {
        return mesh;
      }
      calc_face_normal = true;
      break;
    default:
      return mesh;
  }

  if (dmd->face_count <= 3) {
    BKE_modifier_set_error(md, "Modifier requires more than 3 input faces");
    return mesh;
  }

  if (dmd->mode == MOD_DECIM_MODE_COLLAPSE) {
    if (dmd->defgrp_name[0] && (dmd->defgrp_factor > 0.0f)) {
      MDeformVert *dvert;
      int defgrp_index;

      MOD_get_vgroup(ctx->object, mesh, dmd->defgrp_name, &dvert, &defgrp_index);

      if (dvert) {
        const uint vert_tot = mesh->totvert;
        uint i;

        vweights = MEM_malloc_arrayN(vert_tot, sizeof(float), __func__);

        if (dmd->flag & MOD_DECIM_FLAG_INVERT_VGROUP) {
          for (i = 0; i < vert_tot; i++) {
            vweights[i] = 1.0f - BKE_defvert_find_weight(&dvert[i], defgrp_index);
          }
        }
        else {
          for (i = 0; i < vert_tot; i++) {
            vweights[i] = BKE_defvert_find_weight(&dvert[i], defgrp_index);
          }
        }
      }
    }
  }

  bm = BKE_mesh_to_bmesh_ex(mesh,
                            &(struct BMeshCreateParams){0},
                            &(struct BMeshFromMeshParams){
                                .calc_face_normal = calc_face_normal,
                                .cd_mask_extra = {.vmask = CD_MASK_ORIGINDEX,
                                                  .emask = CD_MASK_ORIGINDEX,
                                                  .pmask = CD_MASK_ORIGINDEX},
                            });

  switch (dmd->mode) {
    case MOD_DECIM_MODE_COLLAPSE: {
      const bool do_triangulate = (dmd->flag & MOD_DECIM_FLAG_TRIANGULATE) != 0;
      const int symmetry_axis = (dmd->flag & MOD_DECIM_FLAG_SYMMETRY) ? dmd->symmetry_axis : -1;
      const float symmetry_eps = 0.00002f;
      BM_mesh_decimate_collapse(bm,
                                dmd->percent,
                                vweights,
                                dmd->defgrp_factor,
                                do_triangulate,
                                symmetry_axis,
                                symmetry_eps);
      break;
    }
    case MOD_DECIM_MODE_UNSUBDIV: {
      BM_mesh_decimate_unsubdivide(bm, dmd->iter);
      break;
    }
    case MOD_DECIM_MODE_DISSOLVE: {
      const bool do_dissolve_boundaries = (dmd->flag & MOD_DECIM_FLAG_ALL_BOUNDARY_VERTS) != 0;
      BM_mesh_decimate_dissolve(bm, dmd->angle, do_dissolve_boundaries, (BMO_Delimit)dmd->delimit);
      break;
    }
  }

  if (vweights) {
    MEM_freeN(vweights);
  }

  updateFaceCount(ctx, dmd, bm->totface);

  result = BKE_mesh_from_bmesh_for_eval_nomain(bm, NULL, mesh);
  /* make sure we never alloc'd these */
  BLI_assert(bm->vtoolflagpool == NULL && bm->etoolflagpool == NULL && bm->ftoolflagpool == NULL);
  BLI_assert(bm->vtable == NULL && bm->etable == NULL && bm->ftable == NULL);

  BM_mesh_free(bm);

#ifdef USE_TIMEIT
  TIMEIT_END(decim);
#endif

  result->runtime.cd_dirty_vert |= CD_MASK_NORMAL;

  return result;
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *sub, *row;
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  PointerRNA ob_ptr;
  modifier_panel_get_property_pointers(C, panel, &ob_ptr, &ptr);

  uiLayoutSetPropSep(layout, true);

  int decimate_type = RNA_enum_get(&ptr, "decimate_type");
  char count_info[32];
  snprintf(count_info, 32, IFACE_("Face Count: %d"), RNA_int_get(&ptr, "face_count"));

  uiItemR(layout, &ptr, "decimate_type", 0, NULL, ICON_NONE);

  if (decimate_type == MOD_DECIM_MODE_COLLAPSE) {
    uiItemR(layout, &ptr, "ratio", UI_ITEM_R_SLIDER, NULL, ICON_NONE);

    row = uiLayoutRowWithHeading(layout, true, "Symmetry");
    uiLayoutSetPropDecorate(row, false);
    sub = uiLayoutRow(row, true);
    uiItemR(sub, &ptr, "use_symmetry", 0, "", ICON_NONE);
    sub = uiLayoutRow(sub, true);
    uiLayoutSetActive(sub, RNA_boolean_get(&ptr, "use_symmetry"));
    uiItemR(sub, &ptr, "symmetry_axis", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
    uiItemDecoratorR(row, &ptr, "symmetry_axis", 0);

    uiItemR(layout, &ptr, "use_collapse_triangulate", 0, NULL, ICON_NONE);

    modifier_vgroup_ui(layout, &ptr, &ob_ptr, "vertex_group", "invert_vertex_group", NULL);
  }
  else if (decimate_type == MOD_DECIM_MODE_UNSUBDIV) {
    uiItemR(layout, &ptr, "iterations", 0, NULL, ICON_NONE);
  }
  else { /* decimate_type == MOD_DECIM_MODE_DISSOLVE. */
    uiItemR(layout, &ptr, "angle_limit", 0, NULL, ICON_NONE);
    uiItemR(layout, &ptr, "delimit", 0, NULL, ICON_NONE);
    uiItemR(layout, &ptr, "use_dissolve_boundaries", 0, NULL, ICON_NONE);
  }
  uiItemL(layout, count_info, ICON_NONE);

  modifier_panel_end(layout, &ptr);
}

static void panelRegister(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_Decimate, panel_draw);
}

ModifierTypeInfo modifierType_Decimate = {
    /* name */ "Decimate",
    /* structName */ "DecimateModifierData",
    /* structSize */ sizeof(DecimateModifierData),
    /* type */ eModifierTypeType_Nonconstructive,
    /* flags */ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_AcceptsCVs,

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
    /* isDisabled */ NULL,
    /* updateDepsgraph */ NULL,
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ NULL,
    /* foreachObjectLink */ NULL,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
    /* panelRegister */ panelRegister,
    /* blendWrite */ NULL,
    /* blendRead */ NULL,
};
