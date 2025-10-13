/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BKE_geometry_set.hh"
#include "BKE_lib_query.hh"
#include "BKE_modifier.hh"
#include "BKE_texture.h"
#include "BKE_volume.hh"
#include "BKE_volume_grid.hh"
#include "BKE_volume_grid_process.hh"
#include "BKE_volume_openvdb.hh"

#include "BLT_translation.hh"

#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_texture_types.h"

#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_query.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "MOD_ui_common.hh"

#include "RE_texture.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "BLI_math_vector.h"

#ifdef WITH_OPENVDB
#  include <openvdb/openvdb.h>
#  include <openvdb/tools/Interpolation.h>
#  include <openvdb/tools/Morphology.h>
#  include <openvdb/tools/Prune.h>
#  include <openvdb/tools/ValueTransformer.h>
#endif

static void init_data(ModifierData *md)
{
  VolumeDisplaceModifierData *vdmd = reinterpret_cast<VolumeDisplaceModifierData *>(md);
  vdmd->texture = nullptr;
  vdmd->strength = 0.5f;
  copy_v3_fl(vdmd->texture_mid_level, 0.5f);
  vdmd->texture_sample_radius = 1.0f;
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  VolumeDisplaceModifierData *vdmd = reinterpret_cast<VolumeDisplaceModifierData *>(md);
  if (vdmd->texture != nullptr) {
    DEG_add_generic_id_relation(ctx->node, &vdmd->texture->id, "Volume Displace Modifier");
  }
  if (vdmd->texture_map_mode == MOD_VOLUME_DISPLACE_MAP_OBJECT) {
    if (vdmd->texture_map_object != nullptr) {
      DEG_add_object_relation(
          ctx->node, vdmd->texture_map_object, DEG_OB_COMP_TRANSFORM, "Volume Displace Modifier");
    }
  }
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  VolumeDisplaceModifierData *vdmd = reinterpret_cast<VolumeDisplaceModifierData *>(md);
  walk(user_data, ob, (ID **)&vdmd->texture, IDWALK_CB_USER);
  walk(user_data, ob, (ID **)&vdmd->texture_map_object, IDWALK_CB_USER);
}

static void foreach_tex_link(ModifierData *md, Object *ob, TexWalkFunc walk, void *user_data)
{
  PointerRNA ptr = RNA_pointer_create_discrete(&ob->id, &RNA_Modifier, md);
  PropertyRNA *prop = RNA_struct_find_property(&ptr, "texture");
  walk(user_data, ob, md, &ptr, prop);
}

static bool depends_on_time(Scene * /*scene*/, ModifierData *md)
{
  VolumeDisplaceModifierData *vdmd = reinterpret_cast<VolumeDisplaceModifierData *>(md);
  if (vdmd->texture) {
    return BKE_texture_dependsOnTime(vdmd->texture);
  }
  return false;
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);
  VolumeDisplaceModifierData *vdmd = static_cast<VolumeDisplaceModifierData *>(ptr->data);

  layout->use_property_split_set(true);

  uiTemplateID(layout, C, ptr, "texture", "texture.new", nullptr, nullptr);
  layout->prop(ptr, "texture_map_mode", UI_ITEM_NONE, IFACE_("Texture Mapping"), ICON_NONE);

  if (vdmd->texture_map_mode == MOD_VOLUME_DISPLACE_MAP_OBJECT) {
    layout->prop(ptr, "texture_map_object", UI_ITEM_NONE, IFACE_("Object"), ICON_NONE);
  }

  layout->prop(ptr, "strength", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(ptr, "texture_sample_radius", UI_ITEM_NONE, IFACE_("Sample Radius"), ICON_NONE);
  layout->prop(ptr, "texture_mid_level", UI_ITEM_NONE, IFACE_("Mid Level"), ICON_NONE);

  modifier_error_message_draw(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_VolumeDisplace, panel_draw);
}

#ifdef WITH_OPENVDB

static openvdb::Mat4s matrix_to_openvdb(const blender::float4x4 &m)
{
  /* OpenVDB matrices are transposed Blender matrices, i.e. the translation is in the last row
   * instead of in the last column. However, the layout in memory is the same, because OpenVDB
   * matrices are row major (compared to Blender's column major matrices). */
  openvdb::Mat4s new_matrix{m.base_ptr()};
  return new_matrix;
}

template<typename GridType> struct DisplaceOp {
  /* Has to be copied for each thread. */
  typename GridType::ConstAccessor accessor;
  const openvdb::Mat4s index_to_texture;

  Tex *texture;
  const double strength;
  const openvdb::Vec3d texture_mid_level;

  void operator()(const typename GridType::ValueOnIter &iter) const
  {
    const openvdb::Coord coord = iter.getCoord();
    const openvdb::Vec3d displace_vector = this->compute_displace_vector(coord);
    /* Subtract vector because that makes the result more similar to advection and the mesh
     * displace modifier. */
    const openvdb::Vec3d sample_coord = coord.asVec3d() - displace_vector;
    const auto new_value = openvdb::tools::BoxSampler::sample(this->accessor, sample_coord);
    iter.setValue(new_value);
  }

  openvdb::Vec3d compute_displace_vector(const openvdb::Coord &coord) const
  {
    if (this->texture != nullptr) {
      const openvdb::Vec3f texture_pos = coord.asVec3s() * this->index_to_texture;
      const openvdb::Vec3d texture_value = this->evaluate_texture(texture_pos);
      const openvdb::Vec3d displacement = (texture_value - this->texture_mid_level) *
                                          this->strength;
      return displacement;
    }
    return openvdb::Vec3d{0, 0, 0};
  }

  openvdb::Vec3d evaluate_texture(const openvdb::Vec3f &pos) const
  {
    TexResult texture_result = {0};
    BKE_texture_get_value(this->texture, const_cast<float *>(pos.asV()), &texture_result, false);
    return {texture_result.trgba[0], texture_result.trgba[1], texture_result.trgba[2]};
  }
};

static float get_max_voxel_side_length(const openvdb::GridBase &grid)
{
  const openvdb::Vec3d voxel_size = grid.voxelSize();
  const float max_voxel_side_length = std::max({voxel_size[0], voxel_size[1], voxel_size[2]});
  return max_voxel_side_length;
}

struct DisplaceGridOp {
  /* This is the grid that will be displaced. The output is copied back to the original grid. */
  openvdb::GridBase &base_grid;

  VolumeDisplaceModifierData &vdmd;
  const ModifierEvalContext &ctx;

  template<typename GridType> void operator()()
  {
    if constexpr (blender::
                      is_same_any_v<GridType, openvdb::points::PointDataGrid, openvdb::MaskGrid>)
    {
      /* We don't support displacing these grid types yet. */
      return;
    }
    else {
      this->displace_grid<GridType>();
    }
  }

  template<typename GridType> void displace_grid()
  {
    GridType &grid = static_cast<GridType &>(base_grid);

    /* Make a copy of the original grid to work on. This will replace the original grid. */
    typename GridType::Ptr temp_grid = grid.deepCopy();

    /* Dilate grid, because the currently inactive cells might become active during the displace
     * operation. The quality of the approximation of this has a big impact on performance. */
    const float max_voxel_side_length = get_max_voxel_side_length(grid);
    const float sample_radius = vdmd.texture_sample_radius * std::abs(vdmd.strength) /
                                max_voxel_side_length / 2.0f;
    openvdb::tools::dilateActiveValues(temp_grid->tree(),
                                       int(std::ceil(sample_radius)),
                                       openvdb::tools::NN_FACE_EDGE,
                                       openvdb::tools::EXPAND_TILES);

    const openvdb::Mat4s index_to_texture = this->get_index_to_texture_transform();

    /* Construct the operator that will be executed on every cell of the dilated grid. */
    DisplaceOp<GridType> displace_op{grid.getConstAccessor(),
                                     index_to_texture,
                                     vdmd.texture,
                                     vdmd.strength / max_voxel_side_length,
                                     openvdb::Vec3d{vdmd.texture_mid_level}};

    /* Run the operator. This is multi-threaded. It is important that the operator is not shared
     * between the threads, because it contains a non-thread-safe accessor for the old grid. */
    openvdb::tools::foreach(temp_grid->beginValueOn(),
                            displace_op,
                            true,
                            /* Disable sharing of the operator. */
                            false);

    /* It is likely that we produced too many active cells. Those are removed here, to avoid
     * slowing down subsequent operations. */
    typename GridType::ValueType prune_tolerance{0};
    openvdb::tools::deactivate(*temp_grid, temp_grid->background(), prune_tolerance);
    blender::bke::volume_grid::prune_inactive(*temp_grid);

    /* Overwrite the old volume grid with the new grid. */
    grid.clear();
    grid.merge(*temp_grid);
  }

  openvdb::Mat4s get_index_to_texture_transform() const
  {
    const openvdb::Mat4s index_to_object{
        base_grid.transform().baseMap()->getAffineMap()->getMat4()};

    switch (vdmd.texture_map_mode) {
      case MOD_VOLUME_DISPLACE_MAP_LOCAL: {
        return index_to_object;
      }
      case MOD_VOLUME_DISPLACE_MAP_GLOBAL: {
        const openvdb::Mat4s object_to_world = matrix_to_openvdb(ctx.object->object_to_world());
        return index_to_object * object_to_world;
      }
      case MOD_VOLUME_DISPLACE_MAP_OBJECT: {
        if (vdmd.texture_map_object == nullptr) {
          return index_to_object;
        }
        const openvdb::Mat4s object_to_world = matrix_to_openvdb(ctx.object->object_to_world());
        const openvdb::Mat4s world_to_texture = matrix_to_openvdb(
            vdmd.texture_map_object->world_to_object());
        return index_to_object * object_to_world * world_to_texture;
      }
    }
    BLI_assert(false);
    return {};
  }
};

#endif

static void displace_volume(ModifierData *md, const ModifierEvalContext *ctx, Volume *volume)
{
#ifdef WITH_OPENVDB
  VolumeDisplaceModifierData *vdmd = reinterpret_cast<VolumeDisplaceModifierData *>(md);

  /* Iterate over all grids and displace them one by one. */
  BKE_volume_load(volume, DEG_get_bmain(ctx->depsgraph));
  const int grid_amount = BKE_volume_num_grids(volume);
  for (int grid_index = 0; grid_index < grid_amount; grid_index++) {
    blender::bke::VolumeGridData *volume_grid = BKE_volume_grid_get_for_write(volume, grid_index);
    BLI_assert(volume_grid);

    blender::bke::VolumeTreeAccessToken tree_token;
    openvdb::GridBase &grid = volume_grid->grid_for_write(tree_token);
    VolumeGridType grid_type = volume_grid->grid_type();

    DisplaceGridOp displace_grid_op{grid, *vdmd, *ctx};
    BKE_volume_grid_type_operation(grid_type, displace_grid_op);
    volume_grid->tag_tree_modified();
  }

#else
  UNUSED_VARS(md, volume, ctx);
  BKE_modifier_set_error(ctx->object, md, "Compiled without OpenVDB");
#endif
}

static void modify_geometry_set(ModifierData *md,
                                const ModifierEvalContext *ctx,
                                blender::bke::GeometrySet *geometry_set)
{
  Volume *input_volume = geometry_set->get_volume_for_write();
  if (input_volume != nullptr) {
    displace_volume(md, ctx, input_volume);
  }
}

ModifierTypeInfo modifierType_VolumeDisplace = {
    /*idname*/ "Volume Displace",
    /*name*/ N_("Volume Displace"),
    /*struct_name*/ "VolumeDisplaceModifierData",
    /*struct_size*/ sizeof(VolumeDisplaceModifierData),
    /*srna*/ &RNA_VolumeDisplaceModifier,
    /*type*/ ModifierTypeType::NonGeometrical,
    /*flags*/ static_cast<ModifierTypeFlag>(0),
    /*icon*/ ICON_VOLUME_DATA, /* TODO: Use correct icon. */

    /*copy_data*/ BKE_modifier_copydata_generic,

    /*deform_verts*/ nullptr,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ nullptr,
    /*modify_geometry_set*/ modify_geometry_set,

    /*init_data*/ init_data,
    /*required_data_mask*/ nullptr,
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
