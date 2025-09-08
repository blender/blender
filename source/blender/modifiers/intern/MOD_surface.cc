/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_bvhutils.hh"
#include "BKE_lib_id.hh"
#include "BKE_mesh.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_prototypes.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "MOD_modifiertypes.hh"
#include "MOD_ui_common.hh"

#include "MEM_guardedalloc.h"

static void init_data(ModifierData *md)
{
  SurfaceModifierData *surmd = (SurfaceModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(surmd, modifier));

  MEMCPY_STRUCT_AFTER(surmd, DNA_struct_default_get(SurfaceModifierData), modifier);
}

static void copy_data(const ModifierData *md_src, ModifierData *md_dst, const int flag)
{
  SurfaceModifierData *surmd_dst = (SurfaceModifierData *)md_dst;

  BKE_modifier_copydata_generic(md_src, md_dst, flag);

  surmd_dst->runtime = SurfaceModifierData_Runtime{};
}

static void free_data(ModifierData *md)
{
  SurfaceModifierData *surmd = (SurfaceModifierData *)md;

  if (surmd) {
    MEM_SAFE_DELETE(surmd->runtime.bvhtree);

    if (surmd->runtime.mesh) {
      BKE_id_free(nullptr, surmd->runtime.mesh);
      surmd->runtime.mesh = nullptr;
    }

    MEM_SAFE_FREE(surmd->runtime.vert_positions_prev);

    MEM_SAFE_FREE(surmd->runtime.vert_velocities);
  }
}

static bool depends_on_time(Scene * /*scene*/, ModifierData * /*md*/)
{
  return true;
}

static void deform_verts(ModifierData *md,
                         const ModifierEvalContext *ctx,
                         Mesh *mesh,
                         blender::MutableSpan<blender::float3> positions)
{
  SurfaceModifierData *surmd = (SurfaceModifierData *)md;
  const int cfra = int(DEG_get_ctime(ctx->depsgraph));

  /* Free mesh and BVH cache. */
  MEM_SAFE_DELETE(surmd->runtime.bvhtree);
  if (surmd->runtime.mesh) {
    BKE_id_free(nullptr, surmd->runtime.mesh);
    surmd->runtime.mesh = nullptr;
  }

  if (mesh) {
    surmd->runtime.mesh = BKE_mesh_copy_for_eval(*mesh);
  }

  if (!ctx->object->pd) {
    printf("SurfaceModifier deform_verts: Should not happen!\n");
    return;
  }

  if (surmd->runtime.mesh) {
    uint mesh_verts_num = 0, i = 0;
    int init = 0;

    surmd->runtime.mesh->vert_positions_for_write().copy_from(positions);
    surmd->runtime.mesh->tag_positions_changed();

    mesh_verts_num = surmd->runtime.mesh->verts_num;

    if ((mesh_verts_num != surmd->runtime.verts_num) ||
        (surmd->runtime.vert_positions_prev == nullptr) ||
        (surmd->runtime.vert_velocities == nullptr) || (cfra != surmd->runtime.cfra_prev + 1))
    {

      MEM_SAFE_FREE(surmd->runtime.vert_positions_prev);
      MEM_SAFE_FREE(surmd->runtime.vert_velocities);

      surmd->runtime.vert_positions_prev = MEM_calloc_arrayN<float[3]>(mesh_verts_num, __func__);
      surmd->runtime.vert_velocities = MEM_calloc_arrayN<float[3]>(mesh_verts_num, __func__);

      surmd->runtime.verts_num = mesh_verts_num;

      init = 1;
    }

    /* convert to global coordinates and calculate velocity */
    blender::MutableSpan<blender::float3> positions =
        surmd->runtime.mesh->vert_positions_for_write();
    for (i = 0; i < mesh_verts_num; i++) {
      float *vec = positions[i];
      mul_m4_v3(ctx->object->object_to_world().ptr(), vec);

      if (init) {
        zero_v3(surmd->runtime.vert_velocities[i]);
      }
      else {
        sub_v3_v3v3(surmd->runtime.vert_velocities[i], vec, surmd->runtime.vert_positions_prev[i]);
      }

      copy_v3_v3(surmd->runtime.vert_positions_prev[i], vec);
    }

    surmd->runtime.cfra_prev = cfra;

    const bool has_face = surmd->runtime.mesh->faces_num > 0;
    const bool has_edge = surmd->runtime.mesh->edges_num > 0;
    if (has_face) {
      surmd->runtime.bvhtree = MEM_new<blender::bke::BVHTreeFromMesh>(
          __func__, surmd->runtime.mesh->bvh_corner_tris());
    }
    else if (has_edge) {
      surmd->runtime.bvhtree = MEM_new<blender::bke::BVHTreeFromMesh>(
          __func__, surmd->runtime.mesh->bvh_edges());
    }
  }
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
  modifier_panel_register(region_type, eModifierType_Surface, panel_draw);
}

static void blend_read(BlendDataReader * /*reader*/, ModifierData *md)
{
  SurfaceModifierData *surmd = (SurfaceModifierData *)md;

  surmd->runtime = SurfaceModifierData_Runtime{};
}

ModifierTypeInfo modifierType_Surface = {
    /*idname*/ "Surface",
    /*name*/ N_("Surface"),
    /*struct_name*/ "SurfaceModifierData",
    /*struct_size*/ sizeof(SurfaceModifierData),
    /*srna*/ &RNA_SurfaceModifier,
    /*type*/ ModifierTypeType::OnlyDeform,
    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_AcceptsCVs |
        eModifierTypeFlag_NoUserAdd,
    /*icon*/ ICON_MOD_PHYSICS,

    /*copy_data*/ copy_data,

    /*deform_verts*/ deform_verts,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ nullptr,
    /*modify_geometry_set*/ nullptr,

    /*init_data*/ init_data,
    /*required_data_mask*/ nullptr,
    /*free_data*/ free_data,
    /*is_disabled*/ nullptr,
    /*update_depsgraph*/ nullptr,
    /*depends_on_time*/ depends_on_time,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ nullptr,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ panel_register,
    /*blend_write*/ nullptr,
    /*blend_read*/ blend_read,
    /*foreach_cache*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
};
