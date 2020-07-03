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

static Mesh *modifyMesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *me)
{
#ifndef WITH_FLUID
  UNUSED_VARS(md, ctx);
  return me;
#else
  FluidModifierData *fmd = (FluidModifierData *)md;
  Mesh *result = NULL;

  if (ctx->flag & MOD_APPLY_ORCO) {
    return me;
  }

  Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);

  result = BKE_fluid_modifier_do(fmd, ctx->depsgraph, scene, ctx->object, me);
  return result ? result : me;
#endif /* WITH_FLUID */
}

static bool dependsOnTime(ModifierData *UNUSED(md))
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

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  uiItemL(layout, IFACE_("Settings are inside the Physics tab"), ICON_NONE);

  modifier_panel_end(layout, &ptr);
}

static void panelRegister(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_Fluid, panel_draw);
}

ModifierTypeInfo modifierType_Fluid = {
    /* name */ "Fluid",
    /* structName */ "FluidModifierData",
    /* structSize */ sizeof(FluidModifierData),
    /* type */ eModifierTypeType_Constructive,
    /* flags */ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_Single,

    /* copyData */ copyData,

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
    /* freeData */ freeData,
    /* isDisabled */ NULL,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ dependsOnTime,
    /* dependsOnNormals */ NULL,
    /* foreachObjectLink */ NULL,
    /* foreachIDLink */ foreachIDLink,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
    /* panelRegister */ panelRegister,
    /* blendWrite */ NULL,
    /* blendRead */ NULL,
};
