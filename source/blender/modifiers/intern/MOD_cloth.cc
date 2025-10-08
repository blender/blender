/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstring>

#include "BLI_utildefines.h"

#include "BLI_listbase.h"

#include "BLT_translation.hh"

#include "DNA_cloth_types.h"
#include "DNA_defaults.h"
#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_force_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "MEM_guardedalloc.h"

#include "BKE_cloth.hh"
#include "BKE_customdata.hh"
#include "BKE_effect.h"
#include "BKE_global.hh"
#include "BKE_key.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_modifier.hh"
#include "BKE_pointcache.h"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_prototypes.hh"

#include "DEG_depsgraph_physics.hh"
#include "DEG_depsgraph_query.hh"

#include "MOD_ui_common.hh"

static void init_data(ModifierData *md)
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
    clmd->sim_parms->effector_weights = BKE_effector_add_weights(nullptr);
  }

  if (clmd->point_cache) {
    clmd->point_cache->step = 1;
  }
}

static void deform_verts(ModifierData *md,
                         const ModifierEvalContext *ctx,
                         Mesh *mesh,
                         blender::MutableSpan<blender::float3> positions)
{
  ClothModifierData *clmd = (ClothModifierData *)md;
  Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);

  /* check for alloc failing */
  if (!clmd->sim_parms || !clmd->coll_parms) {
    init_data(md);

    if (!clmd->sim_parms || !clmd->coll_parms) {
      return;
    }
  }

  /* TODO(sergey): For now it actually duplicates logic from mesh_data_update.cc
   * and needs some more generic solution. But starting experimenting with
   * this so close to the release is not that nice..
   *
   * Also hopefully new cloth system will arrive soon..
   */
  if (mesh == nullptr && clmd->sim_parms->shapekey_rest) {
    KeyBlock *kb = BKE_keyblock_find_by_index(BKE_key_from_object(ctx->object),
                                              clmd->sim_parms->shapekey_rest);
    if (kb && kb->data != nullptr) {
      float (*layerorco)[3] = static_cast<float (*)[3]>(
          CustomData_get_layer_for_write(&mesh->vert_data, CD_CLOTH_ORCO, mesh->verts_num));
      if (!layerorco) {
        layerorco = static_cast<float (*)[3]>(CustomData_add_layer(
            &mesh->vert_data, CD_CLOTH_ORCO, CD_SET_DEFAULT, mesh->verts_num));
      }

      memcpy(layerorco, kb->data, sizeof(float[3]) * positions.size());
    }
  }

  mesh->vert_positions_for_write().copy_from(positions);
  mesh->tag_positions_changed();

  clothModifier_do(clmd,
                   ctx->depsgraph,
                   scene,
                   ctx->object,
                   mesh,
                   reinterpret_cast<float (*)[3]>(positions.data()));
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  ClothModifierData *clmd = (ClothModifierData *)md;
  if (clmd != nullptr) {
    if (clmd->coll_parms->flags & CLOTH_COLLSETTINGS_FLAG_ENABLED) {
      DEG_add_collision_relations(ctx->node,
                                  ctx->object,
                                  clmd->coll_parms->group,
                                  eModifierType_Collision,
                                  nullptr,
                                  "Cloth Collision");
    }
    DEG_add_forcefield_relations(
        ctx->node, ctx->object, clmd->sim_parms->effector_weights, true, 0, "Cloth Field");
  }
  DEG_add_depends_on_transform_relation(ctx->node, "Cloth Modifier");
}

static void required_data_mask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
{
  ClothModifierData *clmd = (ClothModifierData *)md;

  if (cloth_uses_vgroup(clmd)) {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }

  if (clmd->sim_parms->shapekey_rest != 0) {
    r_cddata_masks->vmask |= CD_MASK_CLOTH_ORCO;
  }
}

static void copy_data(const ModifierData *md, ModifierData *target, const int flag)
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
    tclmd->point_cache = static_cast<PointCache *>(
        BLI_findlink(&tclmd->ptcaches, clmd_point_cache_index));
  }

  tclmd->sim_parms = static_cast<ClothSimSettings *>(MEM_dupallocN(clmd->sim_parms));
  if (clmd->sim_parms->effector_weights) {
    tclmd->sim_parms->effector_weights = static_cast<EffectorWeights *>(
        MEM_dupallocN(clmd->sim_parms->effector_weights));
  }
  tclmd->coll_parms = static_cast<ClothCollSettings *>(MEM_dupallocN(clmd->coll_parms));
  tclmd->clothObject = nullptr;
  tclmd->hairdata = nullptr;
  tclmd->solver_result = nullptr;
}

static bool depends_on_time(Scene * /*scene*/, ModifierData * /*md*/)
{
  return true;
}

static void free_data(ModifierData *md)
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
    clmd->point_cache = nullptr;

    if (clmd->hairdata) {
      MEM_freeN(clmd->hairdata);
    }

    if (clmd->solver_result) {
      MEM_freeN(clmd->solver_result);
    }
  }
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  ClothModifierData *clmd = (ClothModifierData *)md;

  if (clmd->coll_parms) {
    walk(user_data, ob, (ID **)&clmd->coll_parms->group, IDWALK_CB_NOP);
  }

  if (clmd->sim_parms && clmd->sim_parms->effector_weights) {
    walk(user_data, ob, (ID **)&clmd->sim_parms->effector_weights->group, IDWALK_CB_USER);
  }
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  layout->label(RPT_("Settings are inside the Physics tab"), ICON_NONE);

  modifier_error_message_draw(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_Cloth, panel_draw);
}

ModifierTypeInfo modifierType_Cloth = {
    /*idname*/ "Cloth",
    /*name*/ N_("Cloth"),
    /*struct_name*/ "ClothModifierData",
    /*struct_size*/ sizeof(ClothModifierData),
    /*srna*/ &RNA_ClothModifier,
    /*type*/ ModifierTypeType::OnlyDeform,
    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_UsesPointCache |
        eModifierTypeFlag_Single,
    /*icon*/ ICON_MOD_CLOTH,

    /*copy_data*/ copy_data,

    /*deform_verts*/ deform_verts,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ nullptr,
    /*modify_geometry_set*/ nullptr,

    /*init_data*/ init_data,
    /*required_data_mask*/ required_data_mask,
    /*free_data*/ free_data,
    /*is_disabled*/ nullptr,
    /*update_depsgraph*/ update_depsgraph,
    /*depends_on_time*/ depends_on_time,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ foreach_ID_link,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ panel_register,
    /*blend_write*/ nullptr,
    /*blend_read*/ nullptr,
    /*foreach_cache*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
};
