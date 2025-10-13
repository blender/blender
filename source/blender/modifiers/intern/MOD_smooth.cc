/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "MEM_guardedalloc.h"

#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_screen_types.h"

#include "BKE_deform.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_prototypes.hh"
#include "RNA_types.hh"

#include "MOD_modifiertypes.hh"
#include "MOD_ui_common.hh"
#include "MOD_util.hh"

static void init_data(ModifierData *md)
{
  SmoothModifierData *smd = (SmoothModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(smd, modifier));

  MEMCPY_STRUCT_AFTER(smd, DNA_struct_default_get(SmoothModifierData), modifier);
}

static bool is_disabled(const Scene * /*scene*/, ModifierData *md, bool /*use_render_params*/)
{
  SmoothModifierData *smd = (SmoothModifierData *)md;

  const short flag = smd->flag & (MOD_SMOOTH_X | MOD_SMOOTH_Y | MOD_SMOOTH_Z);

  /* disable if modifier is off for X, Y and Z or if factor is 0 */
  if (smd->fac == 0.0f || flag == 0) {
    return true;
  }

  return false;
}

static void required_data_mask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
{
  SmoothModifierData *smd = (SmoothModifierData *)md;

  /* Ask for vertex-groups if we need them. */
  if (smd->defgrp_name[0] != '\0') {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }
}

static void smoothModifier_do(
    SmoothModifierData *smd, Object *ob, Mesh *mesh, float (*vertexCos)[3], int verts_num)
{
  if (mesh == nullptr) {
    return;
  }

  float (*accumulated_vecs)[3] = MEM_calloc_arrayN<float[3]>(verts_num, __func__);
  if (!accumulated_vecs) {
    return;
  }

  uint *accumulated_vecs_count = MEM_calloc_arrayN<uint>(verts_num, __func__);
  if (!accumulated_vecs_count) {
    MEM_freeN(accumulated_vecs);
    return;
  }

  const float fac_new = smd->fac;
  const float fac_orig = 1.0f - fac_new;
  const bool invert_vgroup = (smd->flag & MOD_SMOOTH_INVERT_VGROUP) != 0;

  const blender::Span<blender::int2> edges = mesh->edges();

  const MDeformVert *dvert;
  int defgrp_index;
  MOD_get_vgroup(ob, mesh, smd->defgrp_name, &dvert, &defgrp_index);

  for (int j = 0; j < smd->repeat; j++) {
    if (j != 0) {
      memset(accumulated_vecs, 0, sizeof(*accumulated_vecs) * size_t(verts_num));
      memset(accumulated_vecs_count, 0, sizeof(*accumulated_vecs_count) * size_t(verts_num));
    }

    for (const int i : edges.index_range()) {
      float fvec[3];
      const uint idx1 = edges[i][0];
      const uint idx2 = edges[i][1];

      mid_v3_v3v3(fvec, vertexCos[idx1], vertexCos[idx2]);

      accumulated_vecs_count[idx1]++;
      add_v3_v3(accumulated_vecs[idx1], fvec);

      accumulated_vecs_count[idx2]++;
      add_v3_v3(accumulated_vecs[idx2], fvec);
    }

    const short flag = smd->flag;
    if (dvert) {
      const MDeformVert *dv = dvert;
      for (int i = 0; i < verts_num; i++, dv++) {
        float *vco_orig = vertexCos[i];
        if (accumulated_vecs_count[i] > 0) {
          mul_v3_fl(accumulated_vecs[i], 1.0f / float(accumulated_vecs_count[i]));
        }
        float *vco_new = accumulated_vecs[i];

        const float f_vgroup = invert_vgroup ? (1.0f - BKE_defvert_find_weight(dv, defgrp_index)) :
                                               BKE_defvert_find_weight(dv, defgrp_index);
        if (f_vgroup <= 0.0f) {
          continue;
        }
        const float f_new = f_vgroup * fac_new;
        const float f_orig = 1.0f - f_new;

        if (flag & MOD_SMOOTH_X) {
          vco_orig[0] = f_orig * vco_orig[0] + f_new * vco_new[0];
        }
        if (flag & MOD_SMOOTH_Y) {
          vco_orig[1] = f_orig * vco_orig[1] + f_new * vco_new[1];
        }
        if (flag & MOD_SMOOTH_Z) {
          vco_orig[2] = f_orig * vco_orig[2] + f_new * vco_new[2];
        }
      }
    }
    else { /* no vertex group */
      for (int i = 0; i < verts_num; i++) {
        float *vco_orig = vertexCos[i];
        if (accumulated_vecs_count[i] > 0) {
          mul_v3_fl(accumulated_vecs[i], 1.0f / float(accumulated_vecs_count[i]));
        }
        float *vco_new = accumulated_vecs[i];

        if (flag & MOD_SMOOTH_X) {
          vco_orig[0] = fac_orig * vco_orig[0] + fac_new * vco_new[0];
        }
        if (flag & MOD_SMOOTH_Y) {
          vco_orig[1] = fac_orig * vco_orig[1] + fac_new * vco_new[1];
        }
        if (flag & MOD_SMOOTH_Z) {
          vco_orig[2] = fac_orig * vco_orig[2] + fac_new * vco_new[2];
        }
      }
    }
  }

  MEM_freeN(accumulated_vecs);
  MEM_freeN(accumulated_vecs_count);
}

static void deform_verts(ModifierData *md,
                         const ModifierEvalContext *ctx,
                         Mesh *mesh,
                         blender::MutableSpan<blender::float3> positions)
{
  SmoothModifierData *smd = (SmoothModifierData *)md;
  smoothModifier_do(
      smd, ctx->object, mesh, reinterpret_cast<float (*)[3]>(positions.data()), positions.size());
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *row, *col;
  uiLayout *layout = panel->layout;
  const eUI_Item_Flag toggles_flag = UI_ITEM_R_TOGGLE | UI_ITEM_R_FORCE_BLANK_DECORATE;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  layout->use_property_split_set(true);

  row = &layout->row(true, IFACE_("Axis"));
  row->prop(ptr, "use_x", toggles_flag, std::nullopt, ICON_NONE);
  row->prop(ptr, "use_y", toggles_flag, std::nullopt, ICON_NONE);
  row->prop(ptr, "use_z", toggles_flag, std::nullopt, ICON_NONE);

  col = &layout->column(false);
  col->prop(ptr, "factor", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(ptr, "iterations", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  modifier_vgroup_ui(layout, ptr, &ob_ptr, "vertex_group", "invert_vertex_group", std::nullopt);

  modifier_error_message_draw(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_Smooth, panel_draw);
}

ModifierTypeInfo modifierType_Smooth = {
    /*idname*/ "Smooth",
    /*name*/ N_("Smooth"),
    /*struct_name*/ "SmoothModifierData",
    /*struct_size*/ sizeof(SmoothModifierData),
    /*srna*/ &RNA_SmoothModifier,
    /*type*/ ModifierTypeType::OnlyDeform,
    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_AcceptsCVs |
        eModifierTypeFlag_SupportsEditmode,
    /*icon*/ ICON_MOD_SMOOTH,

    /*copy_data*/ BKE_modifier_copydata_generic,

    /*deform_verts*/ deform_verts,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ nullptr,
    /*modify_geometry_set*/ nullptr,

    /*init_data*/ init_data,
    /*required_data_mask*/ required_data_mask,
    /*free_data*/ nullptr,
    /*is_disabled*/ is_disabled,
    /*update_depsgraph*/ nullptr,
    /*depends_on_time*/ nullptr,
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
