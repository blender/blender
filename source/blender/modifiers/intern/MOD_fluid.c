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

#include <stddef.h>

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

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_physics.h"
#include "DEG_depsgraph_query.h"

#include "MOD_modifiertypes.h"
#include "MOD_ui_common.h"

static void initData(ModifierData *md)
{
  FluidModifierData *fmd = (FluidModifierData *)md;

  fmd->domain = NULL;
  fmd->flow = NULL;
  fmd->effector = NULL;
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

static void requiredDataMask(Object *UNUSED(ob),
                             ModifierData *md,
                             CustomData_MeshMasks *r_cddata_masks)
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

typedef struct FluidIsolationData {
  Depsgraph *depsgraph;
  Object *object;
  Mesh *mesh;
  FluidModifierData *fmd;

  Mesh *result;
} FluidIsolationData;

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

static bool dependsOnTime(struct Scene *UNUSED(scene),
                          ModifierData *UNUSED(md),
                          const int UNUSED(dag_eval_mode))
{
  return true;
}

static bool is_flow_cb(Object *UNUSED(ob), ModifierData *md)
{
  FluidModifierData *fmd = (FluidModifierData *)md;
  return (fmd->type & MOD_FLUID_TYPE_FLOW) && fmd->flow;
}

static bool is_coll_cb(Object *UNUSED(ob), ModifierData *md)
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

    if (fmd->domain->guide_parent != NULL) {
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
      walk(userData, ob, (ID **)&fmd->domain->effector_weights->group, IDWALK_CB_NOP);
    }
  }

  if (fmd->type == MOD_FLUID_TYPE_FLOW && fmd->flow) {
    walk(userData, ob, (ID **)&fmd->flow->noise_texture, IDWALK_CB_USER);
  }
}

static void panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, NULL);

  uiItemL(layout, IFACE_("Settings are inside the Physics tab"), ICON_NONE);

  modifier_panel_end(layout, ptr);
}

static void panelRegister(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_Fluid, panel_draw);
}

ModifierTypeInfo modifierType_Fluid = {
    /* name */ "Fluid",
    /* structName */ "FluidModifierData",
    /* structSize */ sizeof(FluidModifierData),
    /* srna */ &RNA_FluidModifier,
    /* type */ eModifierTypeType_Constructive,
    /* flags */ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_Single,
    /* icon */ ICON_MOD_FLUIDSIM,

    /* copyData */ copyData,

    /* deformVerts */ NULL,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* modifyMesh */ modifyMesh,
    /* modifyHair */ NULL,
    /* modifyGeometrySet */ NULL,

    /* initData */ initData,
    /* requiredDataMask */ requiredDataMask,
    /* freeData */ freeData,
    /* isDisabled */ NULL,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ dependsOnTime,
    /* dependsOnNormals */ NULL,
    /* foreachIDLink */ foreachIDLink,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
    /* panelRegister */ panelRegister,
    /* blendWrite */ NULL,
    /* blendRead */ NULL,
};
