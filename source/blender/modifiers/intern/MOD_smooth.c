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

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BLI_math.h"

#include "BLT_translation.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_editmesh.h"
#include "BKE_lib_id.h"
#include "BKE_mesh.h"
#include "BKE_mesh_wrapper.h"
#include "BKE_particle.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "MOD_modifiertypes.h"
#include "MOD_ui_common.h"
#include "MOD_util.h"

static void initData(ModifierData *md)
{
  SmoothModifierData *smd = (SmoothModifierData *)md;

  smd->fac = 0.5f;
  smd->repeat = 1;
  smd->flag = MOD_SMOOTH_X | MOD_SMOOTH_Y | MOD_SMOOTH_Z;
  smd->defgrp_name[0] = '\0';
}

static bool isDisabled(const struct Scene *UNUSED(scene),
                       ModifierData *md,
                       bool UNUSED(useRenderParams))
{
  SmoothModifierData *smd = (SmoothModifierData *)md;

  const short flag = smd->flag & (MOD_SMOOTH_X | MOD_SMOOTH_Y | MOD_SMOOTH_Z);

  /* disable if modifier is off for X, Y and Z or if factor is 0 */
  if (smd->fac == 0.0f || flag == 0) {
    return true;
  }

  return false;
}

static void requiredDataMask(Object *UNUSED(ob),
                             ModifierData *md,
                             CustomData_MeshMasks *r_cddata_masks)
{
  SmoothModifierData *smd = (SmoothModifierData *)md;

  /* ask for vertexgroups if we need them */
  if (smd->defgrp_name[0] != '\0') {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }
}

static void smoothModifier_do(
    SmoothModifierData *smd, Object *ob, Mesh *mesh, float (*vertexCos)[3], int numVerts)
{
  if (mesh == NULL) {
    return;
  }

  float(*accumulated_vecs)[3] = MEM_calloc_arrayN(
      (size_t)numVerts, sizeof(*accumulated_vecs), __func__);
  if (!accumulated_vecs) {
    return;
  }

  uint *num_accumulated_vecs = MEM_calloc_arrayN(
      (size_t)numVerts, sizeof(*num_accumulated_vecs), __func__);
  if (!num_accumulated_vecs) {
    if (accumulated_vecs) {
      MEM_freeN(accumulated_vecs);
    }
    return;
  }

  const float fac_new = smd->fac;
  const float fac_orig = 1.0f - fac_new;
  const bool invert_vgroup = (smd->flag & MOD_SMOOTH_INVERT_VGROUP) != 0;

  MEdge *medges = mesh->medge;
  const int num_edges = mesh->totedge;

  MDeformVert *dvert;
  int defgrp_index;
  MOD_get_vgroup(ob, mesh, smd->defgrp_name, &dvert, &defgrp_index);

  for (int j = 0; j < smd->repeat; j++) {
    if (j != 0) {
      memset(accumulated_vecs, 0, sizeof(*accumulated_vecs) * (size_t)numVerts);
      memset(num_accumulated_vecs, 0, sizeof(*num_accumulated_vecs) * (size_t)numVerts);
    }

    for (int i = 0; i < num_edges; i++) {
      float fvec[3];
      const uint idx1 = medges[i].v1;
      const uint idx2 = medges[i].v2;

      mid_v3_v3v3(fvec, vertexCos[idx1], vertexCos[idx2]);

      num_accumulated_vecs[idx1]++;
      add_v3_v3(accumulated_vecs[idx1], fvec);

      num_accumulated_vecs[idx2]++;
      add_v3_v3(accumulated_vecs[idx2], fvec);
    }

    const short flag = smd->flag;
    if (dvert) {
      MDeformVert *dv = dvert;
      for (int i = 0; i < numVerts; i++, dv++) {
        float *vco_orig = vertexCos[i];
        if (num_accumulated_vecs[0] > 0) {
          mul_v3_fl(accumulated_vecs[i], 1.0f / (float)num_accumulated_vecs[i]);
        }
        float *vco_new = accumulated_vecs[i];

        const float f_new = invert_vgroup ?
                                (1.0f - BKE_defvert_find_weight(dv, defgrp_index)) * fac_new :
                                BKE_defvert_find_weight(dv, defgrp_index) * fac_new;
        if (f_new <= 0.0f) {
          continue;
        }
        const float f_orig = 1.0f - f_new;

        if (flag & MOD_SMOOTH_X) {
          vco_orig[0] = f_orig * vco_orig[0] + f_new * vco_new[0];
        }
        if (flag & MOD_SMOOTH_Y) {
          vco_orig[1] = f_orig * vco_orig[1] + f_new * vco_new[1];
        }
        if (flag & MOD_SMOOTH_Z) {
          vco_orig[2] = f_orig * vco_orig[2] + f_new * vco_new[2];
        }
      }
    }
    else { /* no vertex group */
      for (int i = 0; i < numVerts; i++) {
        float *vco_orig = vertexCos[i];
        if (num_accumulated_vecs[0] > 0) {
          mul_v3_fl(accumulated_vecs[i], 1.0f / (float)num_accumulated_vecs[i]);
        }
        float *vco_new = accumulated_vecs[i];

        if (flag & MOD_SMOOTH_X) {
          vco_orig[0] = fac_orig * vco_orig[0] + fac_new * vco_new[0];
        }
        if (flag & MOD_SMOOTH_Y) {
          vco_orig[1] = fac_orig * vco_orig[1] + fac_new * vco_new[1];
        }
        if (flag & MOD_SMOOTH_Z) {
          vco_orig[2] = fac_orig * vco_orig[2] + fac_new * vco_new[2];
        }
      }
    }
  }

  MEM_freeN(accumulated_vecs);
  MEM_freeN(num_accumulated_vecs);
}

static void deformVerts(ModifierData *md,
                        const ModifierEvalContext *ctx,
                        Mesh *mesh,
                        float (*vertexCos)[3],
                        int numVerts)
{
  SmoothModifierData *smd = (SmoothModifierData *)md;
  Mesh *mesh_src = NULL;

  /* mesh_src is needed for vgroups, and taking edges into account. */
  mesh_src = MOD_deform_mesh_eval_get(ctx->object, NULL, mesh, NULL, numVerts, false, false);

  smoothModifier_do(smd, ctx->object, mesh_src, vertexCos, numVerts);

  if (!ELEM(mesh_src, NULL, mesh)) {
    BKE_id_free(NULL, mesh_src);
  }
}

static void deformVertsEM(ModifierData *md,
                          const ModifierEvalContext *ctx,
                          struct BMEditMesh *editData,
                          Mesh *mesh,
                          float (*vertexCos)[3],
                          int numVerts)
{
  SmoothModifierData *smd = (SmoothModifierData *)md;
  Mesh *mesh_src = NULL;

  /* mesh_src is needed for vgroups, and taking edges into account. */
  mesh_src = MOD_deform_mesh_eval_get(ctx->object, editData, mesh, NULL, numVerts, false, false);

  /* TODO(campbell): use edit-mode data only (remove this line). */
  BKE_mesh_wrapper_ensure_mdata(mesh_src);

  smoothModifier_do(smd, ctx->object, mesh_src, vertexCos, numVerts);

  if (!ELEM(mesh_src, NULL, mesh)) {
    BKE_id_free(NULL, mesh_src);
  }
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *row, *col;
  uiLayout *layout = panel->layout;
  int toggles_flag = UI_ITEM_R_TOGGLE | UI_ITEM_R_FORCE_BLANK_DECORATE;

  PointerRNA ptr;
  PointerRNA ob_ptr;
  modifier_panel_get_property_pointers(C, panel, &ob_ptr, &ptr);

  uiLayoutSetPropSep(layout, true);

  row = uiLayoutRowWithHeading(layout, true, IFACE_("Axis"));
  uiItemR(row, &ptr, "use_x", toggles_flag, NULL, ICON_NONE);
  uiItemR(row, &ptr, "use_y", toggles_flag, NULL, ICON_NONE);
  uiItemR(row, &ptr, "use_z", toggles_flag, NULL, ICON_NONE);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, &ptr, "factor", 0, NULL, ICON_NONE);
  uiItemR(col, &ptr, "iterations", 0, NULL, ICON_NONE);

  modifier_vgroup_ui(layout, &ptr, &ob_ptr, "vertex_group", "invert_vertex_group", NULL);

  modifier_panel_end(layout, &ptr);
}

static void panelRegister(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_Smooth, panel_draw);
}

ModifierTypeInfo modifierType_Smooth = {
    /* name */ "Smooth",
    /* structName */ "SmoothModifierData",
    /* structSize */ sizeof(SmoothModifierData),
    /* type */ eModifierTypeType_OnlyDeform,
    /* flags */ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_AcceptsCVs |
        eModifierTypeFlag_SupportsEditmode,

    /* copyData */ BKE_modifier_copydata_generic,

    /* deformVerts */ deformVerts,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ deformVertsEM,
    /* deformMatricesEM */ NULL,
    /* modifyMesh */ NULL,
    /* modifyHair */ NULL,
    /* modifyPointCloud */ NULL,
    /* modifyVolume */ NULL,

    /* initData */ initData,
    /* requiredDataMask */ requiredDataMask,
    /* freeData */ NULL,
    /* isDisabled */ isDisabled,
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
