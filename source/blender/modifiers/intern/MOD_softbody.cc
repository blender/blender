/* SPDX-FileCopyrightText: 2005 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstdio>

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

#include "MOD_modifiertypes.hh"
#include "MOD_ui_common.hh"

static void deformVerts(ModifierData * /*md*/,
                        const ModifierEvalContext *ctx,
                        Mesh * /*mesh*/,
                        float (*vertexCos)[3],
                        int verts_num)
{
  Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
  sbObjectStep(
      ctx->depsgraph, scene, ctx->object, DEG_get_ctime(ctx->depsgraph), vertexCos, verts_num);
}

static bool dependsOnTime(Scene * /*scene*/, ModifierData * /*md*/)
{
  return true;
}

static void updateDepsgraph(ModifierData * /*md*/, const ModifierUpdateDepsgraphContext *ctx)
{
  if (ctx->object->soft) {
    /* Actual code uses ccd_build_deflector_hash */
    DEG_add_collision_relations(ctx->node,
                                ctx->object,
                                ctx->object->soft->collision_group,
                                eModifierType_Collision,
                                nullptr,
                                "Softbody Collision");
    DEG_add_forcefield_relations(
        ctx->node, ctx->object, ctx->object->soft->effector_weights, true, 0, "Softbody Field");
  }
  /* We need own transformation as well. */
  DEG_add_depends_on_transform_relation(ctx->node, "SoftBody Modifier");
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
  modifier_panel_register(region_type, eModifierType_Softbody, panel_draw);
}

ModifierTypeInfo modifierType_Softbody = {
    /*idname*/ "Softbody",
    /*name*/ N_("Softbody"),
    /*structName*/ "SoftbodyModifierData",
    /*structSize*/ sizeof(SoftbodyModifierData),
    /*srna*/ &RNA_SoftBodyModifier,
    /*type*/ eModifierTypeType_OnlyDeform,
    /*flags*/ eModifierTypeFlag_AcceptsCVs | eModifierTypeFlag_AcceptsVertexCosOnly |
        eModifierTypeFlag_RequiresOriginalData | eModifierTypeFlag_Single |
        eModifierTypeFlag_UsesPointCache,
    /*icon*/ ICON_MOD_SOFT,

    /*copyData*/ nullptr,

    /*deformVerts*/ deformVerts,
    /*deformMatrices*/ nullptr,
    /*deformVertsEM*/ nullptr,
    /*deformMatricesEM*/ nullptr,
    /*modifyMesh*/ nullptr,
    /*modifyGeometrySet*/ nullptr,

    /*initData*/ nullptr,
    /*requiredDataMask*/ nullptr,
    /*freeData*/ nullptr,
    /*isDisabled*/ nullptr,
    /*updateDepsgraph*/ updateDepsgraph,
    /*dependsOnTime*/ dependsOnTime,
    /*dependsOnNormals*/ nullptr,
    /*foreachIDLink*/ nullptr,
    /*foreachTexLink*/ nullptr,
    /*freeRuntimeData*/ nullptr,
    /*panelRegister*/ panelRegister,
    /*blendWrite*/ nullptr,
    /*blendRead*/ nullptr,
};
