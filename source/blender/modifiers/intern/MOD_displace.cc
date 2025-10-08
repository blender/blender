/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BLI_utildefines.h"

#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_task.h"

#include "BLT_translation.hh"

#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_texture_types.h"

#include "BKE_deform.hh"
#include "BKE_image.hh"
#include "BKE_lib_query.hh"
#include "BKE_mesh.hh"
#include "BKE_modifier.hh"
#include "BKE_texture.h"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "MEM_guardedalloc.h"

#include "MOD_ui_common.hh"
#include "MOD_util.hh"

#include "RE_texture.h"

/* Displace */

static void init_data(ModifierData *md)
{
  DisplaceModifierData *dmd = (DisplaceModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(dmd, modifier));

  MEMCPY_STRUCT_AFTER(dmd, DNA_struct_default_get(DisplaceModifierData), modifier);
}

static void required_data_mask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
{
  DisplaceModifierData *dmd = (DisplaceModifierData *)md;

  /* Ask for vertex-groups if we need them. */
  if (dmd->defgrp_name[0] != '\0') {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }

  /* ask for UV coordinates if we need them */
  if (dmd->texmapping == MOD_DISP_MAP_UV) {
    r_cddata_masks->fmask |= CD_MASK_MTFACE;
  }
}

static bool depends_on_time(Scene * /*scene*/, ModifierData *md)
{
  DisplaceModifierData *dmd = (DisplaceModifierData *)md;

  if (dmd->texture) {
    return BKE_texture_dependsOnTime(dmd->texture);
  }

  return false;
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  DisplaceModifierData *dmd = (DisplaceModifierData *)md;

  walk(user_data, ob, (ID **)&dmd->texture, IDWALK_CB_USER);
  walk(user_data, ob, (ID **)&dmd->map_object, IDWALK_CB_NOP);
}

static void foreach_tex_link(ModifierData *md, Object *ob, TexWalkFunc walk, void *user_data)
{
  PointerRNA ptr = RNA_pointer_create_discrete(&ob->id, &RNA_Modifier, md);
  PropertyRNA *prop = RNA_struct_find_property(&ptr, "texture");
  walk(user_data, ob, md, &ptr, prop);
}

static bool is_disabled(const Scene * /*scene*/, ModifierData *md, bool /*use_render_params*/)
{
  DisplaceModifierData *dmd = (DisplaceModifierData *)md;
  return ((!dmd->texture && dmd->direction == MOD_DISP_DIR_RGB_XYZ) || dmd->strength == 0.0f);
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  DisplaceModifierData *dmd = (DisplaceModifierData *)md;
  bool need_transform_relation = false;

  if (dmd->space == MOD_DISP_SPACE_GLOBAL &&
      ELEM(dmd->direction, MOD_DISP_DIR_X, MOD_DISP_DIR_Y, MOD_DISP_DIR_Z, MOD_DISP_DIR_RGB_XYZ))
  {
    need_transform_relation = true;
  }

  if (dmd->texture != nullptr) {
    DEG_add_generic_id_relation(ctx->node, &dmd->texture->id, "Displace Modifier");

    if (dmd->map_object != nullptr && dmd->texmapping == MOD_DISP_MAP_OBJECT) {
      MOD_depsgraph_update_object_bone_relation(
          ctx->node, dmd->map_object, dmd->map_bone, "Displace Modifier");
      need_transform_relation = true;
    }
    if (dmd->texmapping == MOD_DISP_MAP_GLOBAL) {
      need_transform_relation = true;
    }
  }

  if (need_transform_relation) {
    DEG_add_depends_on_transform_relation(ctx->node, "Displace Modifier");
  }
}

struct DisplaceUserdata {
  /*const*/ DisplaceModifierData *dmd;
  Scene *scene;
  ImagePool *pool;
  const MDeformVert *dvert;
  float weight;
  int defgrp_index;
  int direction;
  bool use_global_direction;
  Tex *tex_target;
  float (*tex_co)[3];
  blender::MutableSpan<blender::float3> positions;
  float local_mat[4][4];
  blender::Span<blender::float3> vert_normals;
};

static void displaceModifier_do_task(void *__restrict userdata,
                                     const int iter,
                                     const TaskParallelTLS *__restrict /*tls*/)
{
  DisplaceUserdata *data = (DisplaceUserdata *)userdata;
  DisplaceModifierData *dmd = data->dmd;
  const MDeformVert *dvert = data->dvert;
  const bool invert_vgroup = (dmd->flag & MOD_DISP_INVERT_VGROUP) != 0;
  float weight = data->weight;
  int defgrp_index = data->defgrp_index;
  int direction = data->direction;
  bool use_global_direction = data->use_global_direction;
  float (*tex_co)[3] = data->tex_co;
  blender::MutableSpan<blender::float3> positions = data->positions;

  /* When no texture is used, we fall back to white. */
  const float delta_fixed = 1.0f - dmd->midlevel;

  TexResult texres;
  float strength = dmd->strength;
  float delta;
  float local_vec[3];

  if (dvert) {
    weight = invert_vgroup ? 1.0f - BKE_defvert_find_weight(dvert + iter, defgrp_index) :
                             BKE_defvert_find_weight(dvert + iter, defgrp_index);
    if (weight == 0.0f) {
      return;
    }
  }

  if (data->tex_target) {
    BKE_texture_get_value_ex(data->tex_target, tex_co[iter], &texres, data->pool, false);
    delta = texres.tin - dmd->midlevel;
  }
  else {
    delta = delta_fixed; /* (1.0f - dmd->midlevel) */ /* never changes */
  }

  if (dvert) {
    strength *= weight;
  }

  delta *= strength;
  CLAMP(delta, -10000, 10000);

  switch (direction) {
    case MOD_DISP_DIR_X:
      if (use_global_direction) {
        positions[iter][0] += delta * data->local_mat[0][0];
        positions[iter][1] += delta * data->local_mat[1][0];
        positions[iter][2] += delta * data->local_mat[2][0];
      }
      else {
        positions[iter][0] += delta;
      }
      break;
    case MOD_DISP_DIR_Y:
      if (use_global_direction) {
        positions[iter][0] += delta * data->local_mat[0][1];
        positions[iter][1] += delta * data->local_mat[1][1];
        positions[iter][2] += delta * data->local_mat[2][1];
      }
      else {
        positions[iter][1] += delta;
      }
      break;
    case MOD_DISP_DIR_Z:
      if (use_global_direction) {
        positions[iter][0] += delta * data->local_mat[0][2];
        positions[iter][1] += delta * data->local_mat[1][2];
        positions[iter][2] += delta * data->local_mat[2][2];
      }
      else {
        positions[iter][2] += delta;
      }
      break;
    case MOD_DISP_DIR_RGB_XYZ:
      local_vec[0] = texres.trgba[0] - dmd->midlevel;
      local_vec[1] = texres.trgba[1] - dmd->midlevel;
      local_vec[2] = texres.trgba[2] - dmd->midlevel;
      if (use_global_direction) {
        mul_transposed_mat3_m4_v3(data->local_mat, local_vec);
      }
      mul_v3_fl(local_vec, strength);
      add_v3_v3(positions[iter], local_vec);
      break;
    case MOD_DISP_DIR_NOR:
    case MOD_DISP_DIR_CLNOR:
      madd_v3_v3fl(positions[iter], data->vert_normals[iter], delta);
      break;
  }
}

static void displaceModifier_do(DisplaceModifierData *dmd,
                                const ModifierEvalContext *ctx,
                                Mesh *mesh,
                                blender::MutableSpan<blender::float3> positions)
{
  Object *ob = ctx->object;
  const MDeformVert *dvert;
  int direction = dmd->direction;
  int defgrp_index;
  float (*tex_co)[3];
  float weight = 1.0f; /* init value unused but some compilers may complain */
  const bool use_global_direction = dmd->space == MOD_DISP_SPACE_GLOBAL;

  if (dmd->texture == nullptr && dmd->direction == MOD_DISP_DIR_RGB_XYZ) {
    return;
  }
  if (dmd->strength == 0.0f) {
    return;
  }

  MOD_get_vgroup(ob, mesh, dmd->defgrp_name, &dvert, &defgrp_index);

  if (defgrp_index >= 0 && dvert == nullptr) {
    /* There is a vertex group, but it has no vertices. */
    return;
  }

  Tex *tex_target = dmd->texture;
  if (tex_target != nullptr) {
    tex_co = MEM_calloc_arrayN<float[3]>(positions.size(), "displaceModifier_do tex_co");
    MOD_get_texture_coords((MappingInfoModifierData *)dmd,
                           ctx,
                           ob,
                           mesh,
                           reinterpret_cast<float (*)[3]>(positions.data()),
                           tex_co);

    MOD_init_texture((MappingInfoModifierData *)dmd, ctx);
  }
  else {
    tex_co = nullptr;
  }

  DisplaceUserdata data = {nullptr};
  data.scene = DEG_get_evaluated_scene(ctx->depsgraph);
  data.dmd = dmd;
  data.dvert = dvert;
  data.weight = weight;
  data.defgrp_index = defgrp_index;
  data.direction = direction;
  data.use_global_direction = use_global_direction;
  data.tex_target = tex_target;
  data.tex_co = tex_co;
  data.positions = positions;
  if (direction == MOD_DISP_DIR_NOR) {
    data.vert_normals = mesh->vert_normals_true();
  }
  else if (direction == MOD_DISP_DIR_CLNOR) {
    data.vert_normals = mesh->vert_normals();
  }
  else if (ELEM(direction, MOD_DISP_DIR_X, MOD_DISP_DIR_Y, MOD_DISP_DIR_Z, MOD_DISP_DIR_RGB_XYZ) &&
           use_global_direction)
  {
    copy_m4_m4(data.local_mat, ob->object_to_world().ptr());
  }
  if (tex_target != nullptr) {
    data.pool = BKE_image_pool_new();
    BKE_texture_fetch_images_for_pool(tex_target, data.pool);
  }
  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = (positions.size() > 512);
  BLI_task_parallel_range(0, positions.size(), &data, displaceModifier_do_task, &settings);

  if (data.pool != nullptr) {
    BKE_image_pool_free(data.pool);
  }

  if (tex_co) {
    MEM_freeN(tex_co);
  }
}

static void deform_verts(ModifierData *md,
                         const ModifierEvalContext *ctx,
                         Mesh *mesh,
                         blender::MutableSpan<blender::float3> positions)
{
  displaceModifier_do((DisplaceModifierData *)md, ctx, mesh, positions);
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  PointerRNA obj_data_ptr = RNA_pointer_get(&ob_ptr, "data");

  PointerRNA texture_ptr = RNA_pointer_get(ptr, "texture");
  bool has_texture = !RNA_pointer_is_null(&texture_ptr);
  int texture_coords = RNA_enum_get(ptr, "texture_coords");

  layout->use_property_split_set(true);

  uiTemplateID(layout, C, ptr, "texture", "texture.new", nullptr, nullptr);

  col = &layout->column(false);
  col->active_set(has_texture);
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
    col->prop_search(ptr, "uv_layer", &obj_data_ptr, "uv_layers", std::nullopt, ICON_GROUP_UVS);
  }

  layout->separator();

  col = &layout->column(false);
  col->prop(ptr, "direction", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  if (ELEM(RNA_enum_get(ptr, "direction"),
           MOD_DISP_DIR_X,
           MOD_DISP_DIR_Y,
           MOD_DISP_DIR_Z,
           MOD_DISP_DIR_RGB_XYZ))
  {
    col->prop(ptr, "space", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  layout->separator();

  col = &layout->column(false);
  col->prop(ptr, "strength", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(ptr, "mid_level", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  modifier_vgroup_ui(col, ptr, &ob_ptr, "vertex_group", "invert_vertex_group", std::nullopt);

  modifier_error_message_draw(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_Displace, panel_draw);
}

ModifierTypeInfo modifierType_Displace = {
    /*idname*/ "Displace",
    /*name*/ N_("Displace"),
    /*struct_name*/ "DisplaceModifierData",
    /*struct_size*/ sizeof(DisplaceModifierData),
    /*srna*/ &RNA_DisplaceModifier,
    /*type*/ ModifierTypeType::OnlyDeform,
    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsEditmode,
    /*icon*/ ICON_MOD_DISPLACE,

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
