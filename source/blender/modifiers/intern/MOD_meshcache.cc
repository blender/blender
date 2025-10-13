/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BLI_utildefines.h"

#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"

#include "BLT_translation.hh"

#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_deform.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_mesh.hh"
#include "BKE_scene.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "DEG_depsgraph_query.hh"

#include "MEM_guardedalloc.h"

#include "MOD_meshcache_util.hh" /* utility functions */
#include "MOD_modifiertypes.hh"
#include "MOD_ui_common.hh"
#include "MOD_util.hh"

static void init_data(ModifierData *md)
{
  MeshCacheModifierData *mcmd = (MeshCacheModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(mcmd, modifier));

  MEMCPY_STRUCT_AFTER(mcmd, DNA_struct_default_get(MeshCacheModifierData), modifier);
}

static bool depends_on_time(Scene * /*scene*/, ModifierData *md)
{
  MeshCacheModifierData *mcmd = (MeshCacheModifierData *)md;
  return (mcmd->play_mode == MOD_MESHCACHE_PLAY_CFEA);
}

static bool is_disabled(const Scene * /*scene*/, ModifierData *md, bool /*use_render_params*/)
{
  MeshCacheModifierData *mcmd = (MeshCacheModifierData *)md;

  /* leave it up to the modifier to check the file is valid on calculation */
  return (mcmd->factor <= 0.0f) || (mcmd->filepath[0] == '\0');
}

static void meshcache_do(MeshCacheModifierData *mcmd,
                         Scene *scene,
                         Object *ob,
                         Mesh *mesh,
                         float (*vertexCos_Real)[3],
                         int verts_num)
{
  const bool use_factor = mcmd->factor < 1.0f;
  int influence_group_index;
  const MDeformVert *dvert;
  MOD_get_vgroup(ob, mesh, mcmd->defgrp_name, &dvert, &influence_group_index);

  float (*vertexCos_Store)[3] = (use_factor || influence_group_index != -1 ||
                                 (mcmd->deform_mode == MOD_MESHCACHE_DEFORM_INTEGRATE)) ?
                                    MEM_malloc_arrayN<float[3]>(size_t(verts_num), __func__) :
                                    nullptr;
  float (*vertexCos)[3] = vertexCos_Store ? vertexCos_Store : vertexCos_Real;

  const float fps = scene->frames_per_second();

  char filepath[FILE_MAX];
  const char *err_str = nullptr;
  bool ok;

  float time;

  /* -------------------------------------------------------------------- */
  /* Interpret Time (the reading functions also do some of this). */
  if (mcmd->play_mode == MOD_MESHCACHE_PLAY_CFEA) {
    const float ctime = BKE_scene_ctime_get(scene);

    switch (mcmd->time_mode) {
      case MOD_MESHCACHE_TIME_FRAME: {
        time = ctime;
        break;
      }
      case MOD_MESHCACHE_TIME_SECONDS: {
        time = ctime / fps;
        break;
      }
      case MOD_MESHCACHE_TIME_FACTOR:
      default: {
        time = ctime / fps;
        break;
      }
    }

    /* apply offset and scale */
    time = (mcmd->frame_scale * time) - mcmd->frame_start;
  }
  else { /* `if (mcmd->play_mode == MOD_MESHCACHE_PLAY_EVAL) {`. */
    switch (mcmd->time_mode) {
      case MOD_MESHCACHE_TIME_FRAME: {
        time = mcmd->eval_frame;
        break;
      }
      case MOD_MESHCACHE_TIME_SECONDS: {
        time = mcmd->eval_time;
        break;
      }
      case MOD_MESHCACHE_TIME_FACTOR:
      default: {
        time = mcmd->eval_factor;
        break;
      }
    }
  }

  /* -------------------------------------------------------------------- */
  /* Read the File (or error out when the file is bad) */

  /* would be nice if we could avoid doing this _every_ frame */
  STRNCPY(filepath, mcmd->filepath);
  BLI_path_abs(filepath, ID_BLEND_PATH_FROM_GLOBAL((ID *)ob));

  switch (mcmd->type) {
    case MOD_MESHCACHE_TYPE_MDD:
      ok = MOD_meshcache_read_mdd_times(
          filepath, vertexCos, verts_num, mcmd->interp, time, fps, mcmd->time_mode, &err_str);
      break;
    case MOD_MESHCACHE_TYPE_PC2:
      ok = MOD_meshcache_read_pc2_times(
          filepath, vertexCos, verts_num, mcmd->interp, time, fps, mcmd->time_mode, &err_str);
      break;
    default:
      ok = false;
      break;
  }

  /* -------------------------------------------------------------------- */
  /* tricky shape key integration (slow!) */
  if (mcmd->deform_mode == MOD_MESHCACHE_DEFORM_INTEGRATE) {
    Mesh *mesh = static_cast<Mesh *>(ob->data);

    /* we could support any object type */
    if (UNLIKELY(ob->type != OB_MESH)) {
      BKE_modifier_set_error(ob, &mcmd->modifier, "'Integrate' only valid for Mesh objects");
    }
    else if (UNLIKELY(mesh->verts_num != verts_num)) {
      BKE_modifier_set_error(ob, &mcmd->modifier, "'Integrate' original mesh vertex mismatch");
    }
    else if (UNLIKELY(mesh->faces_num == 0)) {
      BKE_modifier_set_error(ob, &mcmd->modifier, "'Integrate' requires faces");
    }
    else {
      float (*vertexCos_New)[3] = MEM_malloc_arrayN<float[3]>(size_t(verts_num), __func__);

      BKE_mesh_calc_relative_deform(
          mesh->face_offsets().data(),
          mesh->faces_num,
          mesh->corner_verts().data(),
          mesh->verts_num,
          /* From the original Mesh. */
          reinterpret_cast<const float (*)[3]>(mesh->vert_positions().data()),
          /* the input we've been given (shape keys!) */
          const_cast<const float (*)[3]>(vertexCos_Real),
          /* The result of this modifier. */
          const_cast<const float (*)[3]>(vertexCos),
          /* The result of this function. */
          vertexCos_New);

      /* write the corrected locations back into the result */
      memcpy(vertexCos, vertexCos_New, sizeof(*vertexCos) * verts_num);

      MEM_freeN(vertexCos_New);
    }
  }

  /* -------------------------------------------------------------------- */
  /* Apply the transformation matrix (if needed) */
  if (UNLIKELY(err_str)) {
    BKE_modifier_set_error(ob, &mcmd->modifier, "%s", err_str);
  }
  else if (ok) {
    bool use_matrix = false;
    float mat[3][3];
    unit_m3(mat);

    if (mat3_from_axis_conversion(mcmd->forward_axis, mcmd->up_axis, 1, 2, mat)) {
      use_matrix = true;
    }

    if (mcmd->flip_axis) {
      float tmat[3][3];
      unit_m3(tmat);
      if (mcmd->flip_axis & MOD_MESHCACHE_FLIP_AXIS_X) {
        tmat[0][0] = -1.0f;
      }
      if (mcmd->flip_axis & MOD_MESHCACHE_FLIP_AXIS_Y) {
        tmat[1][1] = -1.0f;
      }
      if (mcmd->flip_axis & MOD_MESHCACHE_FLIP_AXIS_Z) {
        tmat[2][2] = -1.0f;
      }
      mul_m3_m3m3(mat, tmat, mat);

      use_matrix = true;
    }

    if (use_matrix) {
      int i;
      for (i = 0; i < verts_num; i++) {
        mul_m3_v3(mat, vertexCos[i]);
      }
    }
  }

  if (vertexCos_Store) {
    if (ok) {
      if (influence_group_index != -1) {
        const float global_factor = (mcmd->flag & MOD_MESHCACHE_INVERT_VERTEX_GROUP) ?
                                        -mcmd->factor :
                                        mcmd->factor;
        const float global_offset = (mcmd->flag & MOD_MESHCACHE_INVERT_VERTEX_GROUP) ?
                                        mcmd->factor :
                                        0.0f;
        if (!mesh->deform_verts().is_empty()) {
          for (int i = 0; i < verts_num; i++) {
            /* For each vertex, compute its blending factor between the mesh cache (for `fac = 0`)
             * and the former position of the vertex (for `fac = 1`). */
            const MDeformVert *currentIndexDVert = dvert + i;
            const float local_vertex_fac = global_offset +
                                           BKE_defvert_find_weight(currentIndexDVert,
                                                                   influence_group_index) *
                                               global_factor;
            interp_v3_v3v3(
                vertexCos_Real[i], vertexCos_Real[i], vertexCos_Store[i], local_vertex_fac);
          }
        }
      }
      else if (use_factor) {
        /* Influence_group_index is -1. */
        interp_vn_vn(*vertexCos_Real, *vertexCos_Store, mcmd->factor, verts_num * 3);
      }
      else {
        memcpy(vertexCos_Real, vertexCos_Store, sizeof(*vertexCos_Store) * verts_num);
      }
    }

    MEM_freeN(vertexCos_Store);
  }
}

static void deform_verts(ModifierData *md,
                         const ModifierEvalContext *ctx,
                         Mesh *mesh,
                         blender::MutableSpan<blender::float3> positions)
{
  MeshCacheModifierData *mcmd = (MeshCacheModifierData *)md;
  Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);

  meshcache_do(mcmd,
               scene,
               ctx->object,
               mesh,
               reinterpret_cast<float (*)[3]>(positions.data()),
               positions.size());
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  layout->use_property_split_set(true);

  layout->prop(ptr, "cache_format", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(ptr, "filepath", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  layout->prop(ptr, "factor", UI_ITEM_R_SLIDER, std::nullopt, ICON_NONE);
  layout->prop(ptr, "deform_mode", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(ptr, "interpolation", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  modifier_vgroup_ui(layout, ptr, &ob_ptr, "vertex_group", "invert_vertex_group", std::nullopt);

  modifier_error_message_draw(layout, ptr);
}

static void time_remapping_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  layout->prop(ptr, "time_mode", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);

  layout->use_property_split_set(true);

  layout->prop(ptr, "play_mode", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);

  if (RNA_enum_get(ptr, "play_mode") == MOD_MESHCACHE_PLAY_CFEA) {
    layout->prop(ptr, "frame_start", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    layout->prop(ptr, "frame_scale", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
  else { /* play_mode == MOD_MESHCACHE_PLAY_EVAL */
    int time_mode = RNA_enum_get(ptr, "time_mode");
    if (time_mode == MOD_MESHCACHE_TIME_FRAME) {
      layout->prop(ptr, "eval_frame", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    }
    else if (time_mode == MOD_MESHCACHE_TIME_SECONDS) {
      layout->prop(ptr, "eval_time", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    }
    else { /* time_mode == MOD_MESHCACHE_TIME_FACTOR */
      layout->prop(ptr, "eval_factor", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    }
  }
}

static void axis_mapping_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  layout->use_property_split_set(true);

  col = &layout->column(true);
  col->red_alert_set(RNA_enum_get(ptr, "forward_axis") == RNA_enum_get(ptr, "up_axis"));
  col->prop(ptr, "forward_axis", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(ptr, "up_axis", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  const eUI_Item_Flag toggles_flag = UI_ITEM_R_TOGGLE | UI_ITEM_R_FORCE_BLANK_DECORATE;
  PropertyRNA *prop = RNA_struct_find_property(ptr, "flip_axis");
  uiLayout *row = &col->row(true, IFACE_("Flip Axis"));
  row->prop(ptr, prop, 0, 0, toggles_flag, IFACE_("X"), ICON_NONE);
  row->prop(ptr, prop, 1, 0, toggles_flag, IFACE_("Y"), ICON_NONE);
  row->prop(ptr, prop, 2, 0, toggles_flag, IFACE_("Z"), ICON_NONE);
}

static void panel_register(ARegionType *region_type)
{
  PanelType *panel_type = modifier_panel_register(
      region_type, eModifierType_MeshCache, panel_draw);
  modifier_subpanel_register(region_type,
                             "time_remapping",
                             "Time Remapping",
                             nullptr,
                             time_remapping_panel_draw,
                             panel_type);
  modifier_subpanel_register(
      region_type, "axis_mapping", "Axis Mapping", nullptr, axis_mapping_panel_draw, panel_type);
}

ModifierTypeInfo modifierType_MeshCache = {
    /*idname*/ "MeshCache",
    /*name*/ N_("MeshCache"),
    /*struct_name*/ "MeshCacheModifierData",
    /*struct_size*/ sizeof(MeshCacheModifierData),
    /*srna*/ &RNA_MeshCacheModifier,
    /*type*/ ModifierTypeType::OnlyDeform,
    /*flags*/ eModifierTypeFlag_AcceptsCVs | eModifierTypeFlag_AcceptsVertexCosOnly |
        eModifierTypeFlag_SupportsEditmode,
    /*icon*/ ICON_MOD_MESHDEFORM, /* TODO: Use correct icon. */

    /*copy_data*/ BKE_modifier_copydata_generic,

    /*deform_verts*/ deform_verts,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ nullptr,
    /*modify_geometry_set*/ nullptr,

    /*init_data*/ init_data,
    /*required_data_mask*/ nullptr,
    /*free_data*/ nullptr,
    /*is_disabled*/ is_disabled,
    /*update_depsgraph*/ nullptr,
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
