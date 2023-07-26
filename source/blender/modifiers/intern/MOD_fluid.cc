/* SPDX-FileCopyrightText: 2005 Blender Foundation
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

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_physics.h"
#include "DEG_depsgraph_query.h"

#include "MOD_modifiertypes.hh"
#include "MOD_ui_common.hh"

static void initData(ModifierData *md)
{
  FluidModifierData *fmd = (FluidModifierData *)md;

  fmd->domain = nullptr;
  fmd->flow = nullptr;
  fmd->effector = nullptr;
  fmd->type = 0;
  fmd->time = -1;
}

static void copyData(const ModifierData *md, ModifierData *target, const int flag)
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

static void freeData(ModifierData *md)
{
#ifndef WITH_FLUID
  UNUSED_VARS(md);
#else
  FluidModifierData *fmd = (FluidModifierData *)md;

  BKE_fluid_modifier_free(fmd);
#endif /* WITH_FLUID */
}

static void requiredDataMask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
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

static Mesh *modifyMesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *me)
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

static bool dependsOnTime(Scene * /*scene*/, ModifierData * /*md*/)
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

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
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

static void foreachIDLink(ModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  FluidModifierData *fmd = (FluidModifierData *)md;

  if (fmd->type == MOD_FLUID_TYPE_DOMAIN && fmd->domain) {
    walk(userData, ob, (ID **)&fmd->domain->effector_group, IDWALK_CB_NOP);
    walk(userData, ob, (ID **)&fmd->domain->fluid_group, IDWALK_CB_NOP);
    walk(userData, ob, (ID **)&fmd->domain->force_group, IDWALK_CB_NOP);

    if (fmd->domain->guide_parent) {
      walk(userData, ob, (ID **)&fmd->domain->guide_parent, IDWALK_CB_NOP);
    }

    if (fmd->domain->effector_weights) {
      walk(userData, ob, (ID **)&fmd->domain->effector_weights->group, IDWALK_CB_USER);
    }
  }

  if (fmd->type == MOD_FLUID_TYPE_FLOW && fmd->flow) {
    walk(userData, ob, (ID **)&fmd->flow->noise_texture, IDWALK_CB_USER);
  }
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  uiItemL(layout, TIP_("Settings are inside the Physics tab"), ICON_NONE);

  modifier_panel_end(layout, ptr);
}

static void panelRegister(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_Fluid, panel_draw);
}

ModifierTypeInfo modifierType_Fluid = {
    /*idname*/ "Fluid",
    /*name*/ N_("Fluid"),
    /*structName*/ "FluidModifierData",
    /*structSize*/ sizeof(FluidModifierData),
    /*srna*/ &RNA_FluidModifier,
    /*type*/ eModifierTypeType_Constructive,
    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_Single,
    /*icon*/ ICON_MOD_FLUIDSIM,

    /*copyData*/ copyData,

    /*deformVerts*/ nullptr,
    /*deformMatrices*/ nullptr,
    /*deformVertsEM*/ nullptr,
    /*deformMatricesEM*/ nullptr,
    /*modifyMesh*/ modifyMesh,
    /*modifyGeometrySet*/ nullptr,

    /*initData*/ initData,
    /*requiredDataMask*/ requiredDataMask,
    /*freeData*/ freeData,
    /*isDisabled*/ nullptr,
    /*updateDepsgraph*/ updateDepsgraph,
    /*dependsOnTime*/ dependsOnTime,
    /*dependsOnNormals*/ nullptr,
    /*foreachIDLink*/ foreachIDLink,
    /*foreachTexLink*/ nullptr,
    /*freeRuntimeData*/ nullptr,
    /*panelRegister*/ panelRegister,
    /*blendWrite*/ nullptr,
    /*blendRead*/ nullptr,
};
