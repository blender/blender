/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup modifiers
 */

#include "BLI_utildefines.h"

#include "BLI_math.h"

#include "BLT_translation.h"

#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_bvhutils.h"
#include "BKE_context.h"
#include "BKE_lib_id.h"
#include "BKE_mesh.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "MOD_modifiertypes.h"
#include "MOD_ui_common.h"
#include "MOD_util.h"

#include "BLO_read_write.h"

#include "MEM_guardedalloc.h"

static void initData(ModifierData *md)
{
  SurfaceModifierData *surmd = (SurfaceModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(surmd, modifier));

  MEMCPY_STRUCT_AFTER(surmd, DNA_struct_default_get(SurfaceModifierData), modifier);
}

static void copyData(const ModifierData *md_src, ModifierData *md_dst, const int flag)
{
  SurfaceModifierData *surmd_dst = (SurfaceModifierData *)md_dst;

  BKE_modifier_copydata_generic(md_src, md_dst, flag);

  memset(&surmd_dst->runtime, 0, sizeof(surmd_dst->runtime));
}

static void freeData(ModifierData *md)
{
  SurfaceModifierData *surmd = (SurfaceModifierData *)md;

  if (surmd) {
    if (surmd->runtime.bvhtree) {
      free_bvhtree_from_mesh(surmd->runtime.bvhtree);
      MEM_SAFE_FREE(surmd->runtime.bvhtree);
    }

    if (surmd->runtime.mesh) {
      BKE_id_free(NULL, surmd->runtime.mesh);
      surmd->runtime.mesh = NULL;
    }

    MEM_SAFE_FREE(surmd->runtime.vert_positions_prev);

    MEM_SAFE_FREE(surmd->runtime.vert_velocities);
  }
}

static bool dependsOnTime(struct Scene *UNUSED(scene), ModifierData *UNUSED(md))
{
  return true;
}

static void deformVerts(ModifierData *md,
                        const ModifierEvalContext *ctx,
                        Mesh *mesh,
                        float (*vertexCos)[3],
                        int verts_num)
{
  SurfaceModifierData *surmd = (SurfaceModifierData *)md;
  const int cfra = (int)DEG_get_ctime(ctx->depsgraph);

  /* Free mesh and BVH cache. */
  if (surmd->runtime.bvhtree) {
    free_bvhtree_from_mesh(surmd->runtime.bvhtree);
    MEM_SAFE_FREE(surmd->runtime.bvhtree);
  }

  if (surmd->runtime.mesh) {
    BKE_id_free(NULL, surmd->runtime.mesh);
    surmd->runtime.mesh = NULL;
  }

  if (mesh) {
    /* Not possible to use get_mesh() in this case as we'll modify its vertices
     * and get_mesh() would return 'mesh' directly. */
    surmd->runtime.mesh = (Mesh *)BKE_id_copy_ex(NULL, (ID *)mesh, NULL, LIB_ID_COPY_LOCALIZE);
  }
  else {
    surmd->runtime.mesh = MOD_deform_mesh_eval_get(
        ctx->object, NULL, NULL, NULL, verts_num, false);
  }

  if (!ctx->object->pd) {
    printf("SurfaceModifier deformVerts: Should not happen!\n");
    return;
  }

  if (surmd->runtime.mesh) {
    uint mesh_verts_num = 0, i = 0;
    int init = 0;

    BKE_mesh_vert_coords_apply(surmd->runtime.mesh, vertexCos);

    mesh_verts_num = surmd->runtime.mesh->totvert;

    if ((mesh_verts_num != surmd->runtime.verts_num) ||
        (surmd->runtime.vert_positions_prev == NULL) || (surmd->runtime.vert_velocities == NULL) ||
        (cfra != surmd->runtime.cfra_prev + 1)) {

      MEM_SAFE_FREE(surmd->runtime.vert_positions_prev);
      MEM_SAFE_FREE(surmd->runtime.vert_velocities);

      surmd->runtime.vert_positions_prev = MEM_calloc_arrayN(
          mesh_verts_num, sizeof(float[3]), __func__);
      surmd->runtime.vert_velocities = MEM_calloc_arrayN(
          mesh_verts_num, sizeof(float[3]), __func__);

      surmd->runtime.verts_num = mesh_verts_num;

      init = 1;
    }

    /* convert to global coordinates and calculate velocity */
    float(*positions)[3] = BKE_mesh_vert_positions_for_write(surmd->runtime.mesh);
    for (i = 0; i < mesh_verts_num; i++) {
      float *vec = positions[i];
      mul_m4_v3(ctx->object->object_to_world, vec);

      if (init) {
        zero_v3(surmd->runtime.vert_velocities[i]);
      }
      else {
        sub_v3_v3v3(surmd->runtime.vert_velocities[i], vec, surmd->runtime.vert_positions_prev[i]);
      }

      copy_v3_v3(surmd->runtime.vert_positions_prev[i], vec);
    }

    surmd->runtime.cfra_prev = cfra;

    const bool has_poly = surmd->runtime.mesh->totpoly > 0;
    const bool has_edge = surmd->runtime.mesh->totedge > 0;
    if (has_poly || has_edge) {
      surmd->runtime.bvhtree = MEM_callocN(sizeof(BVHTreeFromMesh), "BVHTreeFromMesh");

      if (has_poly) {
        BKE_bvhtree_from_mesh_get(
            surmd->runtime.bvhtree, surmd->runtime.mesh, BVHTREE_FROM_LOOPTRI, 2);
      }
      else if (has_edge) {
        BKE_bvhtree_from_mesh_get(
            surmd->runtime.bvhtree, surmd->runtime.mesh, BVHTREE_FROM_EDGES, 2);
      }
    }
  }
}

static void panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, NULL);

  uiItemL(layout, TIP_("Settings are inside the Physics tab"), ICON_NONE);

  modifier_panel_end(layout, ptr);
}

static void panelRegister(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_Surface, panel_draw);
}

static void blendRead(BlendDataReader *UNUSED(reader), ModifierData *md)
{
  SurfaceModifierData *surmd = (SurfaceModifierData *)md;

  memset(&surmd->runtime, 0, sizeof(surmd->runtime));
}

ModifierTypeInfo modifierType_Surface = {
    /*name*/ N_("Surface"),
    /*structName*/ "SurfaceModifierData",
    /*structSize*/ sizeof(SurfaceModifierData),
    /*srna*/ &RNA_SurfaceModifier,
    /*type*/ eModifierTypeType_OnlyDeform,
    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_AcceptsCVs |
        eModifierTypeFlag_NoUserAdd,
    /*icon*/ ICON_MOD_PHYSICS,

    /*copyData*/ copyData,

    /*deformVerts*/ deformVerts,
    /*deformMatrices*/ NULL,
    /*deformVertsEM*/ NULL,
    /*deformMatricesEM*/ NULL,
    /*modifyMesh*/ NULL,
    /*modifyGeometrySet*/ NULL,

    /*initData*/ initData,
    /*requiredDataMask*/ NULL,
    /*freeData*/ freeData,
    /*isDisabled*/ NULL,
    /*updateDepsgraph*/ NULL,
    /*dependsOnTime*/ dependsOnTime,
    /*dependsOnNormals*/ NULL,
    /*foreachIDLink*/ NULL,
    /*foreachTexLink*/ NULL,
    /*freeRuntimeData*/ NULL,
    /*panelRegister*/ panelRegister,
    /*blendWrite*/ NULL,
    /*blendRead*/ blendRead,
};
