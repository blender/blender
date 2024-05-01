/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_bake_items.hh"
#include "BKE_bake_items_serialize.hh"
#include "BKE_curves.hh"
#include "BKE_instances.hh"
#include "BKE_mesh.hh"
#include "BKE_pointcloud.hh"
#include "BKE_volume.hh"
#include "BKE_volume_grid.hh"

#include "BLI_math_matrix_types.hh"

#include "DNA_material_types.h"
#include "DNA_volume_types.h"

namespace blender::bke::bake {

using namespace io::serialize;
using DictionaryValuePtr = std::shared_ptr<DictionaryValue>;

GeometryBakeItem::GeometryBakeItem(GeometrySet geometry) : geometry(std::move(geometry)) {}

static std::unique_ptr<BakeMaterialsList> materials_to_weak_references(
    Material ***materials, short *materials_num, BakeDataBlockMap *data_block_map)
{
  if (*materials_num == 0) {
    return {};
  }
  auto materials_list = std::make_unique<BakeMaterialsList>();
  materials_list->resize(*materials_num);
  for (const int i : materials_list->index_range()) {
    Material *material = (*materials)[i];
    if (material) {
      (*materials_list)[i] = BakeDataBlockID(material->id);
      if (data_block_map) {
        data_block_map->try_add(material->id);
      }
    }
  }

  MEM_SAFE_FREE(*materials);
  *materials_num = 0;

  return materials_list;
}

void GeometryBakeItem::prepare_geometry_for_bake(GeometrySet &main_geometry,
                                                 BakeDataBlockMap *data_block_map)
{
  main_geometry.ensure_owns_all_data();
  main_geometry.modify_geometry_sets([&](GeometrySet &geometry) {
    if (Mesh *mesh = geometry.get_mesh_for_write()) {
      mesh->attributes_for_write().remove_anonymous();
      mesh->runtime->bake_materials = materials_to_weak_references(
          &mesh->mat, &mesh->totcol, data_block_map);
    }
    if (Curves *curves = geometry.get_curves_for_write()) {
      curves->geometry.wrap().attributes_for_write().remove_anonymous();
      curves->geometry.runtime->bake_materials = materials_to_weak_references(
          &curves->mat, &curves->totcol, data_block_map);
    }
    if (PointCloud *pointcloud = geometry.get_pointcloud_for_write()) {
      pointcloud->attributes_for_write().remove_anonymous();
      pointcloud->runtime->bake_materials = materials_to_weak_references(
          &pointcloud->mat, &pointcloud->totcol, data_block_map);
    }
    if (Volume *volume = geometry.get_volume_for_write()) {
      volume->runtime->bake_materials = materials_to_weak_references(
          &volume->mat, &volume->totcol, data_block_map);
    }
    if (bke::Instances *instances = geometry.get_instances_for_write()) {
      instances->attributes_for_write().remove_anonymous();
    }
    geometry.keep_only_during_modify({GeometryComponent::Type::Mesh,
                                      GeometryComponent::Type::Curve,
                                      GeometryComponent::Type::PointCloud,
                                      GeometryComponent::Type::Volume,
                                      GeometryComponent::Type::Instance});
  });
}

static void restore_materials(Material ***materials,
                              short *materials_num,
                              std::unique_ptr<BakeMaterialsList> materials_list,
                              BakeDataBlockMap *data_block_map)
{
  if (!materials_list) {
    return;
  }
  BLI_assert(*materials == nullptr);
  *materials_num = materials_list->size();
  *materials = MEM_cnew_array<Material *>(materials_list->size(), __func__);
  if (!data_block_map) {
    return;
  }

  for (const int i : materials_list->index_range()) {
    const std::optional<BakeDataBlockID> &data_block_id = (*materials_list)[i];
    if (data_block_id) {
      (*materials)[i] = reinterpret_cast<Material *>(
          data_block_map->lookup_or_remember_missing(*data_block_id));
    }
  }
}

void GeometryBakeItem::try_restore_data_blocks(GeometrySet &main_geometry,
                                               BakeDataBlockMap *data_block_map)
{
  main_geometry.modify_geometry_sets([&](GeometrySet &geometry) {
    if (Mesh *mesh = geometry.get_mesh_for_write()) {
      restore_materials(
          &mesh->mat, &mesh->totcol, std::move(mesh->runtime->bake_materials), data_block_map);
    }
    if (Curves *curves = geometry.get_curves_for_write()) {
      restore_materials(&curves->mat,
                        &curves->totcol,
                        std::move(curves->geometry.runtime->bake_materials),
                        data_block_map);
    }
    if (PointCloud *pointcloud = geometry.get_pointcloud_for_write()) {
      restore_materials(&pointcloud->mat,
                        &pointcloud->totcol,
                        std::move(pointcloud->runtime->bake_materials),
                        data_block_map);
    }
    if (Volume *volume = geometry.get_volume_for_write()) {
      restore_materials(&volume->mat,
                        &volume->totcol,
                        std::move(volume->runtime->bake_materials),
                        data_block_map);
    }
  });
}

#ifdef WITH_OPENVDB
VolumeGridBakeItem::VolumeGridBakeItem(std::unique_ptr<GVolumeGrid> grid) : grid(std::move(grid))
{
}

VolumeGridBakeItem::~VolumeGridBakeItem() = default;
#endif

PrimitiveBakeItem::PrimitiveBakeItem(const CPPType &type, const void *value) : type_(type)
{
  value_ = MEM_mallocN_aligned(type.size(), type.alignment(), __func__);
  type.copy_construct(value, value_);
}

PrimitiveBakeItem::~PrimitiveBakeItem()
{
  type_.destruct(value_);
  MEM_freeN(value_);
}

StringBakeItem::StringBakeItem(std::string value) : value_(std::move(value)) {}

BakeStateRef::BakeStateRef(const BakeState &bake_state)
{
  this->items_by_id.reserve(bake_state.items_by_id.size());
  for (auto item : bake_state.items_by_id.items()) {
    this->items_by_id.add_new(item.key, item.value.get());
  }
}

}  // namespace blender::bke::bake
