/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup modifiers
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BLI_math.h"

#include "BLT_translation.h"

#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
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
#include "RNA_prototypes.h"

#include "MOD_modifiertypes.h"
#include "MOD_ui_common.h"
#include "MOD_util.h"

static void initData(ModifierData *md)
{
  SmoothModifierData *smd = (SmoothModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(smd, modifier));

  MEMCPY_STRUCT_AFTER(smd, DNA_struct_default_get(SmoothModifierData), modifier);
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

static void requiredDataMask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
{
  SmoothModifierData *smd = (SmoothModifierData *)md;

  /* ask for vertexgroups if we need them */
  if (smd->defgrp_name[0] != '\0') {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }
}

static void smoothModifier_do(
    SmoothModifierData *smd, Object *ob, Mesh *mesh, float (*vertexCos)[3], int verts_num)
{
  if (mesh == NULL) {
    return;
  }

  float(*accumulated_vecs)[3] = MEM_calloc_arrayN(
      (size_t)verts_num, sizeof(*accumulated_vecs), __func__);
  if (!accumulated_vecs) {
    return;
  }

  uint *accumulated_vecs_count = MEM_calloc_arrayN(
      (size_t)verts_num, sizeof(*accumulated_vecs_count), __func__);
  if (!accumulated_vecs_count) {
    MEM_freeN(accumulated_vecs);
    return;
  }

  const float fac_new = smd->fac;
  const float fac_orig = 1.0f - fac_new;
  const bool invert_vgroup = (smd->flag & MOD_SMOOTH_INVERT_VGROUP) != 0;

  const MEdge *medges = BKE_mesh_edges(mesh);
  const int edges_num = mesh->totedge;

  const MDeformVert *dvert;
  int defgrp_index;
  MOD_get_vgroup(ob, mesh, smd->defgrp_name, &dvert, &defgrp_index);

  for (int j = 0; j < smd->repeat; j++) {
    if (j != 0) {
      memset(accumulated_vecs, 0, sizeof(*accumulated_vecs) * (size_t)verts_num);
      memset(accumulated_vecs_count, 0, sizeof(*accumulated_vecs_count) * (size_t)verts_num);
    }

    for (int i = 0; i < edges_num; i++) {
      float fvec[3];
      const uint idx1 = medges[i].v1;
      const uint idx2 = medges[i].v2;

      mid_v3_v3v3(fvec, vertexCos[idx1], vertexCos[idx2]);

      accumulated_vecs_count[idx1]++;
      add_v3_v3(accumulated_vecs[idx1], fvec);

      accumulated_vecs_count[idx2]++;
      add_v3_v3(accumulated_vecs[idx2], fvec);
    }

    const short flag = smd->flag;
    if (dvert) {
      const MDeformVert *dv = dvert;
      for (int i = 0; i < verts_num; i++, dv++) {
        float *vco_orig = vertexCos[i];
        if (accumulated_vecs_count[i] > 0) {
          mul_v3_fl(accumulated_vecs[i], 1.0f / (float)accumulated_vecs_count[i]);
        }
        float *vco_new = accumulated_vecs[i];

        const float f_vgroup = invert_vgroup ? (1.0f - BKE_defvert_find_weight(dv, defgrp_index)) :
                                               BKE_defvert_find_weight(dv, defgrp_index);
        if (f_vgroup <= 0.0f) {
          continue;
        }
        const float f_new = f_vgroup * fac_new;
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
      for (int i = 0; i < verts_num; i++) {
        float *vco_orig = vertexCos[i];
        if (accumulated_vecs_count[i] > 0) {
          mul_v3_fl(accumulated_vecs[i], 1.0f / (float)accumulated_vecs_count[i]);
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
  MEM_freeN(accumulated_vecs_count);
}

static void deformVerts(ModifierData *md,
                        const ModifierEvalContext *ctx,
                        Mesh *mesh,
                        float (*vertexCos)[3],
                        int verts_num)
{
  SmoothModifierData *smd = (SmoothModifierData *)md;
  Mesh *mesh_src = NULL;

  /* mesh_src is needed for vgroups, and taking edges into account. */
  mesh_src = MOD_deform_mesh_eval_get(ctx->object, NULL, mesh, NULL, verts_num, false);

  smoothModifier_do(smd, ctx->object, mesh_src, vertexCos, verts_num);

  if (!ELEM(mesh_src, NULL, mesh)) {
    BKE_id_free(NULL, mesh_src);
  }
}

static void deformVertsEM(ModifierData *md,
                          const ModifierEvalContext *ctx,
                          struct BMEditMesh *editData,
                          Mesh *mesh,
                          float (*vertexCos)[3],
                          int verts_num)
{
  SmoothModifierData *smd = (SmoothModifierData *)md;
  Mesh *mesh_src = NULL;

  /* mesh_src is needed for vgroups, and taking edges into account. */
  mesh_src = MOD_deform_mesh_eval_get(ctx->object, editData, mesh, NULL, verts_num, false);

  /* TODO(@ideasman42): use edit-mode data only (remove this line). */
  BKE_mesh_wrapper_ensure_mdata(mesh_src);

  smoothModifier_do(smd, ctx->object, mesh_src, vertexCos, verts_num);

  if (!ELEM(mesh_src, NULL, mesh)) {
    BKE_id_free(NULL, mesh_src);
  }
}

static void panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *row, *col;
  uiLayout *layout = panel->layout;
  int toggles_flag = UI_ITEM_R_TOGGLE | UI_ITEM_R_FORCE_BLANK_DECORATE;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  uiLayoutSetPropSep(layout, true);

  row = uiLayoutRowWithHeading(layout, true, IFACE_("Axis"));
  uiItemR(row, ptr, "use_x", toggles_flag, NULL, ICON_NONE);
  uiItemR(row, ptr, "use_y", toggles_flag, NULL, ICON_NONE);
  uiItemR(row, ptr, "use_z", toggles_flag, NULL, ICON_NONE);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "factor", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "iterations", 0, NULL, ICON_NONE);

  modifier_vgroup_ui(layout, ptr, &ob_ptr, "vertex_group", "invert_vertex_group", NULL);

  modifier_panel_end(layout, ptr);
}

static void panelRegister(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_Smooth, panel_draw);
}

ModifierTypeInfo modifierType_Smooth = {
    /*name*/ N_("Smooth"),
    /*structName*/ "SmoothModifierData",
    /*structSize*/ sizeof(SmoothModifierData),
    /*srna*/ &RNA_SmoothModifier,
    /*type*/ eModifierTypeType_OnlyDeform,
    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_AcceptsCVs |
        eModifierTypeFlag_SupportsEditmode,
    /*icon*/ ICON_MOD_SMOOTH,

    /*copyData*/ BKE_modifier_copydata_generic,

    /*deformVerts*/ deformVerts,
    /*deformMatrices*/ NULL,
    /*deformVertsEM*/ deformVertsEM,
    /*deformMatricesEM*/ NULL,
    /*modifyMesh*/ NULL,
    /*modifyGeometrySet*/ NULL,

    /*initData*/ initData,
    /*requiredDataMask*/ requiredDataMask,
    /*freeData*/ NULL,
    /*isDisabled*/ isDisabled,
    /*updateDepsgraph*/ NULL,
    /*dependsOnTime*/ NULL,
    /*dependsOnNormals*/ NULL,
    /*foreachIDLink*/ NULL,
    /*foreachTexLink*/ NULL,
    /*freeRuntimeData*/ NULL,
    /*panelRegister*/ panelRegister,
    /*blendWrite*/ NULL,
    /*blendRead*/ NULL,
};
