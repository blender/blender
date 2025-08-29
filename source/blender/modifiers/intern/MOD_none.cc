/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "MOD_modifiertypes.hh"

#include "UI_resources.hh"

#include "RNA_prototypes.hh"

/* We only need to define is_disabled; because it always returns 1,
 * no other functions will be called
 */

static bool is_disabled(const Scene * /*scene*/, ModifierData * /*md*/, bool /*use_render_params*/)
{
  return true;
}

ModifierTypeInfo modifierType_None = {
    /*idname*/ "None",
    /*name*/ "None",
    /*struct_name*/ "ModifierData",
    /*struct_size*/ sizeof(ModifierData),
    /*srna*/ &RNA_Modifier,
    /*type*/ ModifierTypeType::None,
    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_AcceptsCVs,
    /*icon*/ ICON_NONE,

    /*copy_data*/ nullptr,

    /*deform_verts*/ nullptr,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ nullptr,
    /*modify_geometry_set*/ nullptr,

    /*init_data*/ nullptr,
    /*required_data_mask*/ nullptr,
    /*free_data*/ nullptr,
    /*is_disabled*/ is_disabled,
    /*update_depsgraph*/ nullptr,
    /*depends_on_time*/ nullptr,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ nullptr,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ nullptr,
    /*blend_write*/ nullptr,
    /*blend_read*/ nullptr,
    /*foreach_cache*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
};
