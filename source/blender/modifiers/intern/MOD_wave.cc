/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BLI_math_matrix.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_texture_types.h"

#include "BKE_deform.hh"
#include "BKE_editmesh.hh"
#include "BKE_lib_query.hh"
#include "BKE_texture.h"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "MEM_guardedalloc.h"

#include "RE_texture.h"

#include "MOD_modifiertypes.hh"
#include "MOD_ui_common.hh"
#include "MOD_util.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

static void init_data(ModifierData *md)
{
  WaveModifierData *wmd = (WaveModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(wmd, modifier));

  MEMCPY_STRUCT_AFTER(wmd, DNA_struct_default_get(WaveModifierData), modifier);
}

static bool depends_on_time(Scene * /*scene*/, ModifierData * /*md*/)
{
  return true;
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  WaveModifierData *wmd = (WaveModifierData *)md;

  walk(user_data, ob, (ID **)&wmd->texture, IDWALK_CB_USER);
  walk(user_data, ob, (ID **)&wmd->objectcenter, IDWALK_CB_NOP);
  walk(user_data, ob, (ID **)&wmd->map_object, IDWALK_CB_NOP);
}

static void foreach_tex_link(ModifierData *md, Object *ob, TexWalkFunc walk, void *user_data)
{
  PointerRNA ptr = RNA_pointer_create_discrete(&ob->id, &RNA_Modifier, md);
  PropertyRNA *prop = RNA_struct_find_property(&ptr, "texture");
  walk(user_data, ob, md, &ptr, prop);
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  WaveModifierData *wmd = (WaveModifierData *)md;
  bool need_transform_relation = false;

  if (wmd->objectcenter != nullptr) {
    DEG_add_object_relation(ctx->node, wmd->objectcenter, DEG_OB_COMP_TRANSFORM, "Wave Modifier");
    need_transform_relation = true;
  }

  if (wmd->texture != nullptr) {
    DEG_add_generic_id_relation(ctx->node, &wmd->texture->id, "Wave Modifier");

    if ((wmd->texmapping == MOD_DISP_MAP_OBJECT) && wmd->map_object != nullptr) {
      MOD_depsgraph_update_object_bone_relation(
          ctx->node, wmd->map_object, wmd->map_bone, "Wave Modifier");
      need_transform_relation = true;
    }
    else if (wmd->texmapping == MOD_DISP_MAP_GLOBAL) {
      need_transform_relation = true;
    }
  }

  if (need_transform_relation) {
    DEG_add_depends_on_transform_relation(ctx->node, "Wave Modifier");
  }
}

static void required_data_mask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
{
  WaveModifierData *wmd = (WaveModifierData *)md;

  /* ask for UV coordinates if we need them */
  if (wmd->texture && wmd->texmapping == MOD_DISP_MAP_UV) {
    r_cddata_masks->fmask |= CD_MASK_MTFACE;
  }

  /* Ask for vertex-groups if we need them. */
  if (wmd->defgrp_name[0] != '\0') {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }
}

static void waveModifier_do(WaveModifierData *wmd,
                            const ModifierEvalContext *ctx,
                            Object *ob,
                            Mesh *mesh,
                            float (*vertexCos)[3],
                            int verts_num)
{
  const MDeformVert *dvert;
  int defgrp_index;
  float ctime = DEG_get_ctime(ctx->depsgraph);
  float minfac = float(1.0 / exp(wmd->width * wmd->narrow * wmd->width * wmd->narrow));
  float lifefac = wmd->height;
  float (*tex_co)[3] = nullptr;
  const int wmd_axis = wmd->flag & (MOD_WAVE_X | MOD_WAVE_Y);
  const float falloff = wmd->falloff;
  float falloff_fac = 1.0f; /* when falloff == 0.0f this stays at 1.0f */
  const bool invert_group = (wmd->flag & MOD_WAVE_INVERT_VGROUP) != 0;

  blender::Span<blender::float3> vert_normals;
  if ((wmd->flag & MOD_WAVE_NORM) && (mesh != nullptr)) {
    vert_normals = mesh->vert_normals();
  }

  if (wmd->objectcenter != nullptr) {
    float mat[4][4];
    /* get the control object's location in local coordinates */
    invert_m4_m4(ob->runtime->world_to_object.ptr(), ob->object_to_world().ptr());
    mul_m4_m4m4(mat, ob->world_to_object().ptr(), wmd->objectcenter->object_to_world().ptr());

    wmd->startx = mat[3][0];
    wmd->starty = mat[3][1];
  }

  /* get the index of the deform group */
  MOD_get_vgroup(ob, mesh, wmd->defgrp_name, &dvert, &defgrp_index);

  if (wmd->damp == 0.0f) {
    wmd->damp = 10.0f;
  }

  if (wmd->lifetime != 0.0f) {
    float x = ctime - wmd->timeoffs;

    if (x > wmd->lifetime) {
      lifefac = x - wmd->lifetime;

      if (lifefac > wmd->damp) {
        lifefac = 0.0;
      }
      else {
        lifefac = (wmd->height * (1.0f - sqrtf(lifefac / wmd->damp)));
      }
    }
  }

  Tex *tex_target = wmd->texture;
  if (mesh != nullptr && tex_target != nullptr) {
    tex_co = MEM_malloc_arrayN<float[3]>(size_t(verts_num), __func__);
    MOD_get_texture_coords((MappingInfoModifierData *)wmd, ctx, ob, mesh, vertexCos, tex_co);

    MOD_init_texture((MappingInfoModifierData *)wmd, ctx);
  }

  if (lifefac != 0.0f) {
    /* avoid divide by zero checks within the loop */
    float falloff_inv = falloff != 0.0f ? 1.0f / falloff : 1.0f;
    int i;

    for (i = 0; i < verts_num; i++) {
      float *co = vertexCos[i];
      float x = co[0] - wmd->startx;
      float y = co[1] - wmd->starty;
      float amplit = 0.0f;
      float def_weight = 1.0f;

      /* get weights */
      if (dvert) {
        def_weight = invert_group ? 1.0f - BKE_defvert_find_weight(&dvert[i], defgrp_index) :
                                    BKE_defvert_find_weight(&dvert[i], defgrp_index);

        /* if this vert isn't in the vgroup, don't deform it */
        if (def_weight == 0.0f) {
          continue;
        }
      }

      switch (wmd_axis) {
        case MOD_WAVE_X | MOD_WAVE_Y:
          amplit = sqrtf(x * x + y * y);
          break;
        case MOD_WAVE_X:
          amplit = x;
          break;
        case MOD_WAVE_Y:
          amplit = y;
          break;
      }

      /* this way it makes nice circles */
      amplit -= (ctime - wmd->timeoffs) * wmd->speed;

      if (wmd->flag & MOD_WAVE_CYCL) {
        amplit = fmodf(amplit - wmd->width, 2.0f * wmd->width) + wmd->width;
      }

      if (falloff != 0.0f) {
        float dist = 0.0f;

        switch (wmd_axis) {
          case MOD_WAVE_X | MOD_WAVE_Y:
            dist = sqrtf(x * x + y * y);
            break;
          case MOD_WAVE_X:
            dist = fabsf(x);
            break;
          case MOD_WAVE_Y:
            dist = fabsf(y);
            break;
        }

        falloff_fac = (1.0f - (dist * falloff_inv));
        CLAMP(falloff_fac, 0.0f, 1.0f);
      }

      /* GAUSSIAN */
      if ((falloff_fac != 0.0f) && (amplit > -wmd->width) && (amplit < wmd->width)) {
        amplit = amplit * wmd->narrow;
        amplit = (1.0f / expf(amplit * amplit) - minfac);

        /* Apply texture. */
        if (tex_co) {
          TexResult texres;
          BKE_texture_get_value(tex_target, tex_co[i], &texres, false);
          amplit *= texres.tin;
        }

        /* Apply weight & falloff. */
        amplit *= def_weight * falloff_fac;

        if (!vert_normals.is_empty()) {
          /* move along normals */
          if (wmd->flag & MOD_WAVE_NORM_X) {
            co[0] += (lifefac * amplit) * vert_normals[i][0];
          }
          if (wmd->flag & MOD_WAVE_NORM_Y) {
            co[1] += (lifefac * amplit) * vert_normals[i][1];
          }
          if (wmd->flag & MOD_WAVE_NORM_Z) {
            co[2] += (lifefac * amplit) * vert_normals[i][2];
          }
        }
        else {
          /* move along local z axis */
          co[2] += lifefac * amplit;
        }
      }
    }
  }

  MEM_SAFE_FREE(tex_co);
}

static void deform_verts(ModifierData *md,
                         const ModifierEvalContext *ctx,
                         Mesh *mesh,
                         blender::MutableSpan<blender::float3> positions)
{
  WaveModifierData *wmd = (WaveModifierData *)md;
  waveModifier_do(wmd,
                  ctx,
                  ctx->object,
                  mesh,
                  reinterpret_cast<float (*)[3]>(positions.data()),
                  positions.size());
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *sub, *row, *col;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  layout->use_property_split_set(true);

  row = &layout->row(true, IFACE_("Motion"));
  row->prop(
      ptr, "use_x", UI_ITEM_R_TOGGLE | UI_ITEM_R_FORCE_BLANK_DECORATE, std::nullopt, ICON_NONE);
  row->prop(
      ptr, "use_y", UI_ITEM_R_TOGGLE | UI_ITEM_R_FORCE_BLANK_DECORATE, std::nullopt, ICON_NONE);

  layout->prop(ptr, "use_cyclic", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  row = &layout->row(true, IFACE_("Along Normals"));
  row->prop(ptr, "use_normal", UI_ITEM_NONE, "", ICON_NONE);
  sub = &row->row(true);
  sub->active_set(RNA_boolean_get(ptr, "use_normal"));
  sub->prop(ptr, "use_normal_x", UI_ITEM_R_TOGGLE, IFACE_("X"), ICON_NONE);
  sub->prop(ptr, "use_normal_y", UI_ITEM_R_TOGGLE, IFACE_("Y"), ICON_NONE);
  sub->prop(ptr, "use_normal_z", UI_ITEM_R_TOGGLE, IFACE_("Z"), ICON_NONE);

  col = &layout->column(false);
  col->prop(ptr, "falloff_radius", UI_ITEM_NONE, IFACE_("Falloff"), ICON_NONE);
  col->prop(ptr, "height", UI_ITEM_R_SLIDER, std::nullopt, ICON_NONE);
  col->prop(ptr, "width", UI_ITEM_R_SLIDER, std::nullopt, ICON_NONE);
  col->prop(ptr, "narrowness", UI_ITEM_R_SLIDER, std::nullopt, ICON_NONE);

  modifier_vgroup_ui(layout, ptr, &ob_ptr, "vertex_group", "invert_vertex_group", std::nullopt);

  modifier_error_message_draw(layout, ptr);
}

static void position_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  layout->use_property_split_set(true);

  layout->prop(ptr, "start_position_object", UI_ITEM_NONE, IFACE_("Object"), ICON_NONE);

  col = &layout->column(true);
  col->prop(ptr, "start_position_x", UI_ITEM_NONE, IFACE_("Start Position X"), ICON_NONE);
  col->prop(ptr, "start_position_y", UI_ITEM_NONE, IFACE_("Y"), ICON_NONE);
}

static void time_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  layout->use_property_split_set(true);

  col = &layout->column(false);
  col->prop(ptr, "time_offset", UI_ITEM_NONE, IFACE_("Offset"), ICON_NONE);
  col->prop(ptr, "lifetime", UI_ITEM_NONE, IFACE_("Life"), ICON_NONE);
  col->prop(ptr, "damping_time", UI_ITEM_NONE, IFACE_("Damping"), ICON_NONE);
  col->prop(ptr, "speed", UI_ITEM_R_SLIDER, std::nullopt, ICON_NONE);
}

static void texture_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  int texture_coords = RNA_enum_get(ptr, "texture_coords");

  uiTemplateID(layout, C, ptr, "texture", "texture.new", nullptr, nullptr);

  layout->use_property_split_set(true);

  col = &layout->column(false);
  col->prop(ptr, "texture_coords", UI_ITEM_NONE, IFACE_("Coordinates"), ICON_NONE);
  if (texture_coords == MOD_DISP_MAP_OBJECT) {
    col->prop(ptr, "texture_coords_object", UI_ITEM_NONE, IFACE_("Object"), ICON_NONE);
    PointerRNA texture_coords_obj_ptr = RNA_pointer_get(ptr, "texture_coords_object");
    if (!RNA_pointer_is_null(&texture_coords_obj_ptr) &&
        (RNA_enum_get(&texture_coords_obj_ptr, "type") == OB_ARMATURE))
    {
      PointerRNA texture_coords_obj_data_ptr = RNA_pointer_get(&texture_coords_obj_ptr, "data");
      col->prop_search(ptr,
                       "texture_coords_bone",
                       &texture_coords_obj_data_ptr,
                       "bones",
                       IFACE_("Bone"),
                       ICON_NONE);
    }
  }
  else if (texture_coords == MOD_DISP_MAP_UV && RNA_enum_get(&ob_ptr, "type") == OB_MESH) {
    PointerRNA obj_data_ptr = RNA_pointer_get(&ob_ptr, "data");
    col->prop_search(ptr, "uv_layer", &obj_data_ptr, "uv_layers", std::nullopt, ICON_GROUP_UVS);
  }
}

static void panel_register(ARegionType *region_type)
{
  PanelType *panel_type = modifier_panel_register(region_type, eModifierType_Wave, panel_draw);
  modifier_subpanel_register(
      region_type, "position", "Start Position", nullptr, position_panel_draw, panel_type);
  modifier_subpanel_register(region_type, "time", "Time", nullptr, time_panel_draw, panel_type);
  modifier_subpanel_register(
      region_type, "texture", "Texture", nullptr, texture_panel_draw, panel_type);
}

ModifierTypeInfo modifierType_Wave = {
    /*idname*/ "Wave",
    /*name*/ N_("Wave"),
    /*struct_name*/ "WaveModifierData",
    /*struct_size*/ sizeof(WaveModifierData),
    /*srna*/ &RNA_WaveModifier,
    /*type*/ ModifierTypeType::OnlyDeform,
    /*flags*/ eModifierTypeFlag_AcceptsCVs | eModifierTypeFlag_AcceptsVertexCosOnly |
        eModifierTypeFlag_SupportsEditmode,
    /*icon*/ ICON_MOD_WAVE,

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
    /*is_disabled*/ nullptr,
    /*update_depsgraph*/ update_depsgraph,
    /*depends_on_time*/ depends_on_time,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ foreach_ID_link,
    /*foreach_tex_link*/ foreach_tex_link,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ panel_register,
    /*blend_write*/ nullptr,
    /*blend_read*/ nullptr,
    /*foreach_cache*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
};
