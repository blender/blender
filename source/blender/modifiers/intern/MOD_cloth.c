/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup modifiers
 */

#include <string.h>

#include "BLI_utildefines.h"

#include "BLI_listbase.h"

#include "BLT_translation.h"

#include "DNA_cloth_types.h"
#include "DNA_defaults.h"
#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_force_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "MEM_guardedalloc.h"

#include "BKE_cloth.h"
#include "BKE_context.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_key.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_pointcache.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "DEG_depsgraph_physics.h"
#include "DEG_depsgraph_query.h"

#include "MOD_ui_common.h"
#include "MOD_util.h"

static void initData(ModifierData *md)
{
  ClothModifierData *clmd = (ClothModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(clmd, modifier));

  MEMCPY_STRUCT_AFTER(clmd, DNA_struct_default_get(ClothModifierData), modifier);
  clmd->sim_parms = DNA_struct_default_alloc(ClothSimSettings);
  clmd->coll_parms = DNA_struct_default_alloc(ClothCollSettings);

  clmd->point_cache = BKE_ptcache_add(&clmd->ptcaches);

  /* check for alloc failing */
  if (!clmd->sim_parms || !clmd->coll_parms || !clmd->point_cache) {
    return;
  }

  if (!clmd->sim_parms->effector_weights) {
    clmd->sim_parms->effector_weights = BKE_effector_add_weights(NULL);
  }

  if (clmd->point_cache) {
    clmd->point_cache->step = 1;
  }
}

static void deformVerts(ModifierData *md,
                        const ModifierEvalContext *ctx,
                        Mesh *mesh,
                        float (*vertexCos)[3],
                        int verts_num)
{
  Mesh *mesh_src;
  ClothModifierData *clmd = (ClothModifierData *)md;
  Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);

  /* check for alloc failing */
  if (!clmd->sim_parms || !clmd->coll_parms) {
    initData(md);

    if (!clmd->sim_parms || !clmd->coll_parms) {
      return;
    }
  }

  if (mesh == NULL) {
    mesh_src = MOD_deform_mesh_eval_get(ctx->object, NULL, NULL, NULL, verts_num, false);
  }
  else {
    /* Not possible to use get_mesh() in this case as we'll modify its vertices
     * and get_mesh() would return 'mesh' directly. */
    mesh_src = (Mesh *)BKE_id_copy_ex(NULL, (ID *)mesh, NULL, LIB_ID_COPY_LOCALIZE);
  }

  /* TODO(sergey): For now it actually duplicates logic from DerivedMesh.cc
   * and needs some more generic solution. But starting experimenting with
   * this so close to the release is not that nice..
   *
   * Also hopefully new cloth system will arrive soon..
   */
  if (mesh == NULL && clmd->sim_parms->shapekey_rest) {
    KeyBlock *kb = BKE_keyblock_from_key(BKE_key_from_object(ctx->object),
                                         clmd->sim_parms->shapekey_rest);
    if (kb && kb->data != NULL) {
      float(*layerorco)[3];
      if (!(layerorco = CustomData_get_layer_for_write(
                &mesh_src->vdata, CD_CLOTH_ORCO, mesh_src->totvert))) {
        layerorco = CustomData_add_layer(
            &mesh_src->vdata, CD_CLOTH_ORCO, CD_SET_DEFAULT, NULL, mesh_src->totvert);
      }

      memcpy(layerorco, kb->data, sizeof(float[3]) * verts_num);
    }
  }

  BKE_mesh_vert_coords_apply(mesh_src, vertexCos);

  clothModifier_do(clmd, ctx->depsgraph, scene, ctx->object, mesh_src, vertexCos);

  BKE_id_free(NULL, mesh_src);
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  ClothModifierData *clmd = (ClothModifierData *)md;
  if (clmd != NULL) {
    if (clmd->coll_parms->flags & CLOTH_COLLSETTINGS_FLAG_ENABLED) {
      DEG_add_collision_relations(ctx->node,
                                  ctx->object,
                                  clmd->coll_parms->group,
                                  eModifierType_Collision,
                                  NULL,
                                  "Cloth Collision");
    }
    DEG_add_forcefield_relations(
        ctx->node, ctx->object, clmd->sim_parms->effector_weights, true, 0, "Cloth Field");
  }
  DEG_add_depends_on_transform_relation(ctx->node, "Cloth Modifier");
}

static void requiredDataMask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
{
  ClothModifierData *clmd = (ClothModifierData *)md;

  if (cloth_uses_vgroup(clmd)) {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }

  if (clmd->sim_parms->shapekey_rest != 0) {
    r_cddata_masks->vmask |= CD_MASK_CLOTH_ORCO;
  }
}

static void copyData(const ModifierData *md, ModifierData *target, const int flag)
{
  const ClothModifierData *clmd = (const ClothModifierData *)md;
  ClothModifierData *tclmd = (ClothModifierData *)target;

  if (tclmd->sim_parms) {
    if (tclmd->sim_parms->effector_weights) {
      MEM_freeN(tclmd->sim_parms->effector_weights);
    }
    MEM_freeN(tclmd->sim_parms);
  }

  if (tclmd->coll_parms) {
    MEM_freeN(tclmd->coll_parms);
  }

  BKE_ptcache_free_list(&tclmd->ptcaches);
  if (flag & LIB_ID_COPY_SET_COPIED_ON_WRITE) {
    /* Share the cache with the original object's modifier. */
    tclmd->modifier.flag |= eModifierFlag_SharedCaches;
    tclmd->ptcaches = clmd->ptcaches;
    tclmd->point_cache = clmd->point_cache;
  }
  else {
    const int clmd_point_cache_index = BLI_findindex(&clmd->ptcaches, clmd->point_cache);
    BKE_ptcache_copy_list(&tclmd->ptcaches, &clmd->ptcaches, flag);
    tclmd->point_cache = BLI_findlink(&tclmd->ptcaches, clmd_point_cache_index);
  }

  tclmd->sim_parms = MEM_dupallocN(clmd->sim_parms);
  if (clmd->sim_parms->effector_weights) {
    tclmd->sim_parms->effector_weights = MEM_dupallocN(clmd->sim_parms->effector_weights);
  }
  tclmd->coll_parms = MEM_dupallocN(clmd->coll_parms);
  tclmd->clothObject = NULL;
  tclmd->hairdata = NULL;
  tclmd->solver_result = NULL;
}

static bool dependsOnTime(struct Scene *UNUSED(scene), ModifierData *UNUSED(md))
{
  return true;
}

static void freeData(ModifierData *md)
{
  ClothModifierData *clmd = (ClothModifierData *)md;

  if (clmd) {
    if (G.debug & G_DEBUG_SIMDATA) {
      printf("clothModifier_freeData\n");
    }

    cloth_free_modifier_extern(clmd);

    if (clmd->sim_parms) {
      if (clmd->sim_parms->effector_weights) {
        MEM_freeN(clmd->sim_parms->effector_weights);
      }
      MEM_freeN(clmd->sim_parms);
    }
    if (clmd->coll_parms) {
      MEM_freeN(clmd->coll_parms);
    }

    if (md->flag & eModifierFlag_SharedCaches) {
      BLI_listbase_clear(&clmd->ptcaches);
    }
    else {
      BKE_ptcache_free_list(&clmd->ptcaches);
    }
    clmd->point_cache = NULL;

    if (clmd->hairdata) {
      MEM_freeN(clmd->hairdata);
    }

    if (clmd->solver_result) {
      MEM_freeN(clmd->solver_result);
    }
  }
}

static void foreachIDLink(ModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  ClothModifierData *clmd = (ClothModifierData *)md;

  if (clmd->coll_parms) {
    walk(userData, ob, (ID **)&clmd->coll_parms->group, IDWALK_CB_NOP);
  }

  if (clmd->sim_parms && clmd->sim_parms->effector_weights) {
    walk(userData, ob, (ID **)&clmd->sim_parms->effector_weights->group, IDWALK_CB_USER);
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
  modifier_panel_register(region_type, eModifierType_Cloth, panel_draw);
}

ModifierTypeInfo modifierType_Cloth = {
    /*name*/ N_("Cloth"),
    /*structName*/ "ClothModifierData",
    /*structSize*/ sizeof(ClothModifierData),
    /*srna*/ &RNA_ClothModifier,
    /*type*/ eModifierTypeType_OnlyDeform,
    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_UsesPointCache |
        eModifierTypeFlag_Single,
    /*icon*/ ICON_MOD_CLOTH,

    /*copyData*/ copyData,

    /*deformVerts*/ deformVerts,
    /*deformMatrices*/ NULL,
    /*deformVertsEM*/ NULL,
    /*deformMatricesEM*/ NULL,
    /*modifyMesh*/ NULL,
    /*modifyGeometrySet*/ NULL,

    /*initData*/ initData,
    /*requiredDataMask*/ requiredDataMask,
    /*freeData*/ freeData,
    /*isDisabled*/ NULL,
    /*updateDepsgraph*/ updateDepsgraph,
    /*dependsOnTime*/ dependsOnTime,
    /*dependsOnNormals*/ NULL,
    /*foreachIDLink*/ foreachIDLink,
    /*foreachTexLink*/ NULL,
    /*freeRuntimeData*/ NULL,
    /*panelRegister*/ panelRegister,
    /*blendWrite*/ NULL,
    /*blendRead*/ NULL,
};
