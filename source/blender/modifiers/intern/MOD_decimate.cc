/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "MEM_guardedalloc.h"

#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_mesh.hh"
#include "BKE_screen.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "DEG_depsgraph_query.h"

#include "bmesh.h"
#include "bmesh_tools.h"

// #define USE_TIMEIT

#ifdef USE_TIMEIT
#  include "PIL_time.h"
#  include "PIL_time_utildefines.h"
#endif

#include "MOD_ui_common.hh"
#include "MOD_util.hh"

static void init_data(ModifierData *md)
{
  DecimateModifierData *dmd = (DecimateModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(dmd, modifier));

  MEMCPY_STRUCT_AFTER(dmd, DNA_struct_default_get(DecimateModifierData), modifier);
}

static void required_data_mask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
{
  DecimateModifierData *dmd = (DecimateModifierData *)md;

  /* Ask for vertex-groups if we need them. */
  if (dmd->defgrp_name[0] != '\0' && (dmd->defgrp_factor > 0.0f)) {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }
}

static DecimateModifierData *getOriginalModifierData(const DecimateModifierData *dmd,
                                                     const ModifierEvalContext *ctx)
{
  Object *ob_orig = DEG_get_original_object(ctx->object);
  return (DecimateModifierData *)BKE_modifiers_findby_name(ob_orig, dmd->modifier.name);
}

static void updateFaceCount(const ModifierEvalContext *ctx,
                            DecimateModifierData *dmd,
                            int face_count)
{
  dmd->face_count = face_count;

  if (DEG_is_active(ctx->depsgraph)) {
    /* update for display only */
    DecimateModifierData *dmd_orig = getOriginalModifierData(dmd, ctx);
    dmd_orig->face_count = face_count;
  }
}

static Mesh *modify_mesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *meshData)
{
  DecimateModifierData *dmd = (DecimateModifierData *)md;
  Mesh *mesh = meshData, *result = nullptr;
  BMesh *bm;
  bool calc_vert_normal;
  bool calc_face_normal;
  float *vweights = nullptr;

#ifdef USE_TIMEIT
  TIMEIT_START(decim);
#endif

  /* Set up front so we don't show invalid info in the UI. */
  updateFaceCount(ctx, dmd, mesh->faces_num);

  switch (dmd->mode) {
    case MOD_DECIM_MODE_COLLAPSE:
      if (dmd->percent == 1.0f) {
        return mesh;
      }
      calc_face_normal = true;
      calc_vert_normal = true;
      break;
    case MOD_DECIM_MODE_UNSUBDIV:
      if (dmd->iter == 0) {
        return mesh;
      }
      calc_face_normal = false;
      calc_vert_normal = false;
      break;
    case MOD_DECIM_MODE_DISSOLVE:
      if (dmd->angle == 0.0f) {
        return mesh;
      }
      calc_face_normal = true;
      calc_vert_normal = false;
      break;
    default:
      return mesh;
  }

  if (dmd->face_count <= 3) {
    BKE_modifier_set_error(ctx->object, md, "Modifier requires more than 3 input faces");
    return mesh;
  }

  if (dmd->mode == MOD_DECIM_MODE_COLLAPSE) {
    if (dmd->defgrp_name[0] && (dmd->defgrp_factor > 0.0f)) {
      const MDeformVert *dvert;
      int defgrp_index;

      MOD_get_vgroup(ctx->object, mesh, dmd->defgrp_name, &dvert, &defgrp_index);

      if (dvert) {
        const uint vert_tot = mesh->totvert;
        uint i;

        vweights = static_cast<float *>(MEM_malloc_arrayN(vert_tot, sizeof(float), __func__));

        if (dmd->flag & MOD_DECIM_FLAG_INVERT_VGROUP) {
          for (i = 0; i < vert_tot; i++) {
            vweights[i] = 1.0f - BKE_defvert_find_weight(&dvert[i], defgrp_index);
          }
        }
        else {
          for (i = 0; i < vert_tot; i++) {
            vweights[i] = BKE_defvert_find_weight(&dvert[i], defgrp_index);
          }
        }
      }
    }
  }

  BMeshCreateParams create_params{};
  BMeshFromMeshParams convert_params{};
  convert_params.calc_face_normal = calc_face_normal;
  convert_params.calc_vert_normal = calc_vert_normal;
  convert_params.cd_mask_extra.vmask = CD_MASK_ORIGINDEX;
  convert_params.cd_mask_extra.emask = CD_MASK_ORIGINDEX;
  convert_params.cd_mask_extra.pmask = CD_MASK_ORIGINDEX;

  bm = BKE_mesh_to_bmesh_ex(mesh, &create_params, &convert_params);

  switch (dmd->mode) {
    case MOD_DECIM_MODE_COLLAPSE: {
      const bool do_triangulate = (dmd->flag & MOD_DECIM_FLAG_TRIANGULATE) != 0;
      const int symmetry_axis = (dmd->flag & MOD_DECIM_FLAG_SYMMETRY) ? dmd->symmetry_axis : -1;
      const float symmetry_eps = 0.00002f;
      BM_mesh_decimate_collapse(bm,
                                dmd->percent,
                                vweights,
                                dmd->defgrp_factor,
                                do_triangulate,
                                symmetry_axis,
                                symmetry_eps);
      break;
    }
    case MOD_DECIM_MODE_UNSUBDIV: {
      BM_mesh_decimate_unsubdivide(bm, dmd->iter);
      break;
    }
    case MOD_DECIM_MODE_DISSOLVE: {
      const bool do_dissolve_boundaries = (dmd->flag & MOD_DECIM_FLAG_ALL_BOUNDARY_VERTS) != 0;
      BM_mesh_decimate_dissolve(bm, dmd->angle, do_dissolve_boundaries, (BMO_Delimit)dmd->delimit);
      break;
    }
  }

  if (vweights) {
    MEM_freeN(vweights);
  }

  updateFaceCount(ctx, dmd, bm->totface);

  /* make sure we never alloc'd these */
  BLI_assert(bm->vtoolflagpool == nullptr && bm->etoolflagpool == nullptr &&
             bm->ftoolflagpool == nullptr);

  result = BKE_mesh_from_bmesh_for_eval_nomain(bm, nullptr, mesh);

  BM_mesh_free(bm);

#ifdef USE_TIMEIT
  TIMEIT_END(decim);
#endif

  return result;
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *sub, *row;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  int decimate_type = RNA_enum_get(ptr, "decimate_type");
  char count_info[64];
  SNPRINTF(count_info, TIP_("Face Count: %d"), RNA_int_get(ptr, "face_count"));

  uiItemR(layout, ptr, "decimate_type", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);

  uiLayoutSetPropSep(layout, true);

  if (decimate_type == MOD_DECIM_MODE_COLLAPSE) {
    uiItemR(layout, ptr, "ratio", UI_ITEM_R_SLIDER, nullptr, ICON_NONE);

    row = uiLayoutRowWithHeading(layout, true, IFACE_("Symmetry"));
    uiLayoutSetPropDecorate(row, false);
    sub = uiLayoutRow(row, true);
    uiItemR(sub, ptr, "use_symmetry", UI_ITEM_NONE, "", ICON_NONE);
    sub = uiLayoutRow(sub, true);
    uiLayoutSetActive(sub, RNA_boolean_get(ptr, "use_symmetry"));
    uiItemR(sub, ptr, "symmetry_axis", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
    uiItemDecoratorR(row, ptr, "symmetry_axis", 0);

    uiItemR(layout, ptr, "use_collapse_triangulate", UI_ITEM_NONE, nullptr, ICON_NONE);

    modifier_vgroup_ui(layout, ptr, &ob_ptr, "vertex_group", "invert_vertex_group", nullptr);
    sub = uiLayoutRow(layout, true);
    bool has_vertex_group = RNA_string_length(ptr, "vertex_group") != 0;
    uiLayoutSetActive(sub, has_vertex_group);
    uiItemR(sub, ptr, "vertex_group_factor", UI_ITEM_NONE, nullptr, ICON_NONE);
  }
  else if (decimate_type == MOD_DECIM_MODE_UNSUBDIV) {
    uiItemR(layout, ptr, "iterations", UI_ITEM_NONE, nullptr, ICON_NONE);
  }
  else { /* decimate_type == MOD_DECIM_MODE_DISSOLVE. */
    uiItemR(layout, ptr, "angle_limit", UI_ITEM_NONE, nullptr, ICON_NONE);
    uiLayout *col = uiLayoutColumn(layout, false);
    uiItemR(col, ptr, "delimit", UI_ITEM_NONE, nullptr, ICON_NONE);
    uiItemR(layout, ptr, "use_dissolve_boundaries", UI_ITEM_NONE, nullptr, ICON_NONE);
  }
  uiItemL(layout, count_info, ICON_NONE);

  modifier_panel_end(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_Decimate, panel_draw);
}

ModifierTypeInfo modifierType_Decimate = {
    /*idname*/ "Decimate",
    /*name*/ N_("Decimate"),
    /*struct_name*/ "DecimateModifierData",
    /*struct_size*/ sizeof(DecimateModifierData),
    /*srna*/ &RNA_DecimateModifier,
    /*type*/ eModifierTypeType_Nonconstructive,
    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_AcceptsCVs,
    /*icon*/ ICON_MOD_DECIM,

    /*copy_data*/ BKE_modifier_copydata_generic,

    /*deform_verts*/ nullptr,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ modify_mesh,
    /*modify_geometry_set*/ nullptr,

    /*init_data*/ init_data,
    /*required_data_mask*/ required_data_mask,
    /*free_data*/ nullptr,
    /*is_disabled*/ nullptr,
    /*update_depsgraph*/ nullptr,
    /*depends_on_time*/ nullptr,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ nullptr,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ panel_register,
    /*blend_write*/ nullptr,
    /*blend_read*/ nullptr,
};
