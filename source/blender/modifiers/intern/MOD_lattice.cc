/* SPDX-FileCopyrightText: 2005 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstring>

#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_defaults.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_lattice.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_wrapper.hh"
#include "BKE_modifier.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "DEG_depsgraph_query.h"

#include "MEM_guardedalloc.h"

#include "MOD_ui_common.hh"
#include "MOD_util.hh"

static void init_data(ModifierData *md)
{
  LatticeModifierData *lmd = (LatticeModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(lmd, modifier));

  MEMCPY_STRUCT_AFTER(lmd, DNA_struct_default_get(LatticeModifierData), modifier);
}

static void required_data_mask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
{
  LatticeModifierData *lmd = (LatticeModifierData *)md;

  /* Ask for vertex-groups if we need them. */
  if (lmd->name[0] != '\0') {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }
}

/**
 * The object type check is only needed here in case we have a placeholder
 * Object assigned (because the library containing the lattice is missing).
 * In other cases it should be impossible to have a type mismatch.
 */
static bool is_disabled(LatticeModifierData *lmd)
{
  return !lmd->object || lmd->object->type != OB_LATTICE;
}

static bool is_disabled(const Scene * /*scene*/, ModifierData *md, bool /*use_render_params*/)
{
  LatticeModifierData *lmd = (LatticeModifierData *)md;
  return is_disabled(lmd);
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  LatticeModifierData *lmd = (LatticeModifierData *)md;

  walk(user_data, ob, (ID **)&lmd->object, IDWALK_CB_NOP);
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  LatticeModifierData *lmd = (LatticeModifierData *)md;
  if (is_disabled(lmd)) {
    return;
  }

  DEG_add_object_relation(ctx->node, lmd->object, DEG_OB_COMP_GEOMETRY, "Lattice Modifier");
  DEG_add_object_relation(ctx->node, lmd->object, DEG_OB_COMP_TRANSFORM, "Lattice Modifier");
  DEG_add_depends_on_transform_relation(ctx->node, "Lattice Modifier");
}

static void deform_verts(ModifierData *md,
                         const ModifierEvalContext *ctx,
                         Mesh *mesh,
                         float (*vertexCos)[3],
                         int verts_num)
{
  LatticeModifierData *lmd = (LatticeModifierData *)md;

  MOD_previous_vcos_store(md, vertexCos); /* if next modifier needs original vertices */

  BKE_lattice_deform_coords_with_mesh(
      lmd->object, ctx->object, vertexCos, verts_num, lmd->flag, lmd->name, lmd->strength, mesh);
}

static void deform_verts_EM(ModifierData *md,
                            const ModifierEvalContext *ctx,
                            BMEditMesh *em,
                            Mesh *mesh,
                            float (*vertexCos)[3],
                            int verts_num)
{
  if (mesh->runtime->wrapper_type == ME_WRAPPER_TYPE_MDATA) {
    deform_verts(md, ctx, mesh, vertexCos, verts_num);
    return;
  }

  LatticeModifierData *lmd = (LatticeModifierData *)md;

  MOD_previous_vcos_store(md, vertexCos); /* if next modifier needs original vertices */

  BKE_lattice_deform_coords_with_editmesh(
      lmd->object, ctx->object, vertexCos, verts_num, lmd->flag, lmd->name, lmd->strength, em);
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "object", UI_ITEM_NONE, nullptr, ICON_NONE);

  modifier_vgroup_ui(layout, ptr, &ob_ptr, "vertex_group", "invert_vertex_group", nullptr);

  uiItemR(layout, ptr, "strength", UI_ITEM_R_SLIDER, nullptr, ICON_NONE);

  modifier_panel_end(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_Lattice, panel_draw);
}

ModifierTypeInfo modifierType_Lattice = {
    /*idname*/ "Lattice",
    /*name*/ N_("Lattice"),
    /*struct_name*/ "LatticeModifierData",
    /*struct_size*/ sizeof(LatticeModifierData),
    /*srna*/ &RNA_LatticeModifier,
    /*type*/ eModifierTypeType_OnlyDeform,
    /*flags*/ eModifierTypeFlag_AcceptsCVs | eModifierTypeFlag_AcceptsVertexCosOnly |
        eModifierTypeFlag_SupportsEditmode,
    /*icon*/ ICON_MOD_LATTICE,

    /*copy_data*/ BKE_modifier_copydata_generic,

    /*deform_verts*/ deform_verts,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ deform_verts_EM,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ nullptr,
    /*modify_geometry_set*/ nullptr,

    /*init_data*/ init_data,
    /*required_data_mask*/ required_data_mask,
    /*free_data*/ nullptr,
    /*is_disabled*/ is_disabled,
    /*update_depsgraph*/ update_depsgraph,
    /*depends_on_time*/ nullptr,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ foreach_ID_link,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ panel_register,
    /*blend_write*/ nullptr,
    /*blend_read*/ nullptr,
};
