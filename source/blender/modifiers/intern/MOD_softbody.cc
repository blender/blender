/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BLT_translation.hh"

#include "DNA_object_force_types.h"
#include "DNA_screen_types.h"

#include "BKE_softbody.h"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_prototypes.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_physics.hh"
#include "DEG_depsgraph_query.hh"

#include "MOD_modifiertypes.hh"
#include "MOD_ui_common.hh"

static void deform_verts(ModifierData * /*md*/,
                         const ModifierEvalContext *ctx,
                         Mesh * /*mesh*/,
                         blender::MutableSpan<blender::float3> positions)
{
  Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
  sbObjectStep(ctx->depsgraph,
               scene,
               ctx->object,
               DEG_get_ctime(ctx->depsgraph),
               reinterpret_cast<float (*)[3]>(positions.data()),
               positions.size());
}

static bool depends_on_time(Scene * /*scene*/, ModifierData * /*md*/)
{
  return true;
}

static void update_depsgraph(ModifierData * /*md*/, const ModifierUpdateDepsgraphContext *ctx)
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
  /* We need our own transformation as well. */
  DEG_add_depends_on_transform_relation(ctx->node, "SoftBody Modifier");
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
  modifier_panel_register(region_type, eModifierType_Softbody, panel_draw);
}

ModifierTypeInfo modifierType_Softbody = {
    /*idname*/ "Softbody",
    /*name*/ N_("Softbody"),
    /*struct_name*/ "SoftbodyModifierData",
    /*struct_size*/ sizeof(SoftbodyModifierData),
    /*srna*/ &RNA_SoftBodyModifier,
    /*type*/ ModifierTypeType::OnlyDeform,
    /*flags*/ eModifierTypeFlag_AcceptsCVs | eModifierTypeFlag_AcceptsVertexCosOnly |
        eModifierTypeFlag_RequiresOriginalData | eModifierTypeFlag_Single |
        eModifierTypeFlag_UsesPointCache,
    /*icon*/ ICON_MOD_SOFT,

    /*copy_data*/ nullptr,

    /*deform_verts*/ deform_verts,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ nullptr,
    /*modify_geometry_set*/ nullptr,

    /*init_data*/ nullptr,
    /*required_data_mask*/ nullptr,
    /*free_data*/ nullptr,
    /*is_disabled*/ nullptr,
    /*update_depsgraph*/ update_depsgraph,
    /*depends_on_time*/ depends_on_time,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ nullptr,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ panel_register,
    /*blend_write*/ nullptr,
    /*blend_read*/ nullptr,
    /*foreach_cache*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
};
