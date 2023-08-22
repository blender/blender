/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstddef>

#include "MEM_guardedalloc.h"

#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_collection_types.h"
#include "DNA_fluid_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_force_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_fluid.h"
#include "BKE_layer.h"
#include "BKE_lib_query.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_physics.h"
#include "DEG_depsgraph_query.h"

#include "MOD_modifiertypes.hh"
#include "MOD_ui_common.hh"

static void init_data(ModifierData *md)
{
  FluidModifierData *fmd = (FluidModifierData *)md;

  fmd->domain = nullptr;
  fmd->flow = nullptr;
  fmd->effector = nullptr;
  fmd->type = 0;
  fmd->time = -1;
}

static void copy_data(const ModifierData *md, ModifierData *target, const int flag)
{
#ifndef WITH_FLUID
  UNUSED_VARS(md, target, flag);
#else
  const FluidModifierData *fmd = (const FluidModifierData *)md;
  FluidModifierData *tfmd = (FluidModifierData *)target;

  BKE_fluid_modifier_free(tfmd);
  BKE_fluid_modifier_copy(fmd, tfmd, flag);
#endif /* WITH_FLUID */
}

static void free_data(ModifierData *md)
{
#ifndef WITH_FLUID
  UNUSED_VARS(md);
#else
  FluidModifierData *fmd = (FluidModifierData *)md;

  BKE_fluid_modifier_free(fmd);
#endif /* WITH_FLUID */
}

static void required_data_mask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
{
  FluidModifierData *fmd = (FluidModifierData *)md;

  if (fmd && (fmd->type & MOD_FLUID_TYPE_FLOW) && fmd->flow) {
    if (fmd->flow->source == FLUID_FLOW_SOURCE_MESH) {
      /* vertex groups */
      if (fmd->flow->vgroup_density) {
        r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
      }
      /* uv layer */
      if (fmd->flow->texture_type == FLUID_FLOW_TEXTURE_MAP_UV) {
        r_cddata_masks->fmask |= CD_MASK_MTFACE;
      }
    }
  }
}

struct FluidIsolationData {
  Depsgraph *depsgraph;
  Object *object;
  Mesh *mesh;
  FluidModifierData *fmd;

  Mesh *result;
};

#ifdef WITH_FLUID
static void fluid_modifier_do_isolated(void *userdata)
{
  FluidIsolationData *isolation_data = (FluidIsolationData *)userdata;

  Scene *scene = DEG_get_evaluated_scene(isolation_data->depsgraph);

  Mesh *result = BKE_fluid_modifier_do(isolation_data->fmd,
                                       isolation_data->depsgraph,
                                       scene,
                                       isolation_data->object,
                                       isolation_data->mesh);
  isolation_data->result = result ? result : isolation_data->mesh;
}
#endif /* WITH_FLUID */

static Mesh *modify_mesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *me)
{
#ifndef WITH_FLUID
  UNUSED_VARS(md, ctx);
  return me;
#else
  FluidModifierData *fmd = (FluidModifierData *)md;

  if (ctx->flag & MOD_APPLY_ORCO) {
    return me;
  }

  /* Isolate execution of Mantaflow when running from dependency graph. The reason for this is
   * because Mantaflow uses TBB to parallel its own computation which without isolation will start
   * stealing tasks from dependency graph. Stealing tasks from the dependency graph might cause
   * a recursive lock when Python drivers are used (because Mantaflow is interfaced via Python as
   * well. */
  FluidIsolationData isolation_data;
  isolation_data.depsgraph = ctx->depsgraph;
  isolation_data.object = ctx->object;
  isolation_data.mesh = me;
  isolation_data.fmd = fmd;
  BLI_task_isolate(fluid_modifier_do_isolated, &isolation_data);

  return isolation_data.result;
#endif /* WITH_FLUID */
}

static bool depends_on_time(Scene * /*scene*/, ModifierData * /*md*/)
{
  return true;
}

static bool is_flow_cb(Object * /*ob*/, ModifierData *md)
{
  FluidModifierData *fmd = (FluidModifierData *)md;
  return (fmd->type & MOD_FLUID_TYPE_FLOW) && fmd->flow;
}

static bool is_coll_cb(Object * /*ob*/, ModifierData *md)
{
  FluidModifierData *fmd = (FluidModifierData *)md;
  return (fmd->type & MOD_FLUID_TYPE_EFFEC) && fmd->effector;
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  FluidModifierData *fmd = (FluidModifierData *)md;

  if (fmd && (fmd->type & MOD_FLUID_TYPE_DOMAIN) && fmd->domain) {
    DEG_add_collision_relations(ctx->node,
                                ctx->object,
                                fmd->domain->fluid_group,
                                eModifierType_Fluid,
                                is_flow_cb,
                                "Fluid Flow");
    DEG_add_collision_relations(ctx->node,
                                ctx->object,
                                fmd->domain->effector_group,
                                eModifierType_Fluid,
                                is_coll_cb,
                                "Fluid Effector");
    DEG_add_forcefield_relations(ctx->node,
                                 ctx->object,
                                 fmd->domain->effector_weights,
                                 true,
                                 PFIELD_FLUIDFLOW,
                                 "Fluid Force Field");

    if (fmd->domain->guide_parent != nullptr) {
      DEG_add_object_relation(
          ctx->node, fmd->domain->guide_parent, DEG_OB_COMP_TRANSFORM, "Fluid Guiding Object");
      DEG_add_object_relation(
          ctx->node, fmd->domain->guide_parent, DEG_OB_COMP_GEOMETRY, "Fluid Guiding Object");
    }
  }
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  FluidModifierData *fmd = (FluidModifierData *)md;

  if (fmd->type == MOD_FLUID_TYPE_DOMAIN && fmd->domain) {
    walk(user_data, ob, (ID **)&fmd->domain->effector_group, IDWALK_CB_NOP);
    walk(user_data, ob, (ID **)&fmd->domain->fluid_group, IDWALK_CB_NOP);
    walk(user_data, ob, (ID **)&fmd->domain->force_group, IDWALK_CB_NOP);

    if (fmd->domain->guide_parent) {
      walk(user_data, ob, (ID **)&fmd->domain->guide_parent, IDWALK_CB_NOP);
    }

    if (fmd->domain->effector_weights) {
      walk(user_data, ob, (ID **)&fmd->domain->effector_weights->group, IDWALK_CB_USER);
    }
  }

  if (fmd->type == MOD_FLUID_TYPE_FLOW && fmd->flow) {
    walk(user_data, ob, (ID **)&fmd->flow->noise_texture, IDWALK_CB_USER);
  }
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  uiItemL(layout, TIP_("Settings are inside the Physics tab"), ICON_NONE);

  modifier_panel_end(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_Fluid, panel_draw);
}

ModifierTypeInfo modifierType_Fluid = {
    /*idname*/ "Fluid",
    /*name*/ N_("Fluid"),
    /*struct_name*/ "FluidModifierData",
    /*struct_size*/ sizeof(FluidModifierData),
    /*srna*/ &RNA_FluidModifier,
    /*type*/ eModifierTypeType_Constructive,
    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_Single,
    /*icon*/ ICON_MOD_FLUIDSIM,

    /*copy_data*/ copy_data,

    /*deform_verts*/ nullptr,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ modify_mesh,
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
};
