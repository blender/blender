/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup modifiers
 */

#include <stdio.h>

#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_mesh_types.h"
#include "DNA_object_force_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_layer.h"
#include "BKE_particle.h"
#include "BKE_screen.h"
#include "BKE_softbody.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_physics.h"
#include "DEG_depsgraph_query.h"

#include "MOD_modifiertypes.h"
#include "MOD_ui_common.h"

static void deformVerts(ModifierData *UNUSED(md),
                        const ModifierEvalContext *ctx,
                        Mesh *UNUSED(derivedData),
                        float (*vertexCos)[3],
                        int verts_num)
{
  Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
  sbObjectStep(
      ctx->depsgraph, scene, ctx->object, DEG_get_ctime(ctx->depsgraph), vertexCos, verts_num);
}

static bool dependsOnTime(struct Scene *UNUSED(scene), ModifierData *UNUSED(md))
{
  return true;
}

static void updateDepsgraph(ModifierData *UNUSED(md), const ModifierUpdateDepsgraphContext *ctx)
{
  if (ctx->object->soft) {
    /* Actual code uses ccd_build_deflector_hash */
    DEG_add_collision_relations(ctx->node,
                                ctx->object,
                                ctx->object->soft->collision_group,
                                eModifierType_Collision,
                                NULL,
                                "Softbody Collision");
    DEG_add_forcefield_relations(
        ctx->node, ctx->object, ctx->object->soft->effector_weights, true, 0, "Softbody Field");
  }
  /* We need own transformation as well. */
  DEG_add_depends_on_transform_relation(ctx->node, "SoftBody Modifier");
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
  modifier_panel_register(region_type, eModifierType_Softbody, panel_draw);
}

ModifierTypeInfo modifierType_Softbody = {
    /*name*/ N_("Softbody"),
    /*structName*/ "SoftbodyModifierData",
    /*structSize*/ sizeof(SoftbodyModifierData),
    /*srna*/ &RNA_SoftBodyModifier,
    /*type*/ eModifierTypeType_OnlyDeform,
    /*flags*/ eModifierTypeFlag_AcceptsCVs | eModifierTypeFlag_AcceptsVertexCosOnly |
        eModifierTypeFlag_RequiresOriginalData | eModifierTypeFlag_Single |
        eModifierTypeFlag_UsesPointCache,
    /*icon*/ ICON_MOD_SOFT,

    /*copyData*/ NULL,

    /*deformVerts*/ deformVerts,
    /*deformMatrices*/ NULL,
    /*deformVertsEM*/ NULL,
    /*deformMatricesEM*/ NULL,
    /*modifyMesh*/ NULL,
    /*modifyGeometrySet*/ NULL,

    /*initData*/ NULL,
    /*requiredDataMask*/ NULL,
    /*freeData*/ NULL,
    /*isDisabled*/ NULL,
    /*updateDepsgraph*/ updateDepsgraph,
    /*dependsOnTime*/ dependsOnTime,
    /*dependsOnNormals*/ NULL,
    /*foreachIDLink*/ NULL,
    /*foreachTexLink*/ NULL,
    /*freeRuntimeData*/ NULL,
    /*panelRegister*/ panelRegister,
    /*blendWrite*/ NULL,
    /*blendRead*/ NULL,
};
