/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_bake_items.hh"
#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_instances.hh"
#include "BKE_mesh.hh"
#include "BKE_pointcloud.hh"
#include "BKE_volume.hh"
#include "BKE_volume_grid.hh"

#include "BLI_memory_counter.hh"
#include "BLI_serialize.hh"

#include "DNA_material_types.h"
#include "DNA_volume_types.h"

#include "NOD_geometry_nodes_list.hh"

namespace blender::bke::bake {

using namespace io::serialize;
using DictionaryValuePtr = std::shared_ptr<DictionaryValue>;

GeometryBakeItem::GeometryBakeItem(GeometrySet geometry) : geometry(std::move(geometry)) {}

void GeometryBakeItem::count_memory(MemoryCounter &memory) const
{
  this->geometry.count_memory(memory);
}

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

static void prepare_geometry_for_bake_recursive(GeometrySet &geometry,
                                                BakeDataBlockMap *data_block_map)
{
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
  if (GreasePencil *grease_pencil = geometry.get_grease_pencil_for_write()) {
    for (GreasePencilDrawingBase *base : grease_pencil->drawings()) {
      if (base->type != GP_DRAWING) {
        continue;
      }
      greasepencil::Drawing &drawing = reinterpret_cast<GreasePencilDrawing *>(base)->wrap();
      drawing.strokes_for_write().attributes_for_write().remove_anonymous();
    }
    grease_pencil->attributes_for_write().remove_anonymous();
    grease_pencil->runtime->bake_materials = materials_to_weak_references(
        &grease_pencil->material_array, &grease_pencil->material_array_num, data_block_map);
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
    instances->ensure_geometry_instances();
    for (bke::InstanceReference &reference : instances->references_for_write()) {
      if (reference.type() == bke::InstanceReference::Type::GeometrySet) {
        prepare_geometry_for_bake_recursive(reference.geometry_set(), data_block_map);
      }
      else {
        /* Can only bake geometry instances currently. */
        reference = bke::InstanceReference();
      }
    }
  }
}

void GeometryBakeItem::prepare_geometry_for_bake(GeometrySet &main_geometry,
                                                 BakeDataBlockMap *data_block_map)
{
  main_geometry.ensure_owns_all_data();
  prepare_geometry_for_bake_recursive(main_geometry, data_block_map);
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
  *materials = MEM_calloc_arrayN<Material *>(materials_list->size(), __func__);
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

static void restore_data_blocks_recursive(GeometrySet &geometry, BakeDataBlockMap *data_block_map)
{
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
  if (GreasePencil *grease_pencil = geometry.get_grease_pencil_for_write()) {
    restore_materials(&grease_pencil->material_array,
                      &grease_pencil->material_array_num,
                      std::move(grease_pencil->runtime->bake_materials),
                      data_block_map);
  }
  if (PointCloud *pointcloud = geometry.get_pointcloud_for_write()) {
    restore_materials(&pointcloud->mat,
                      &pointcloud->totcol,
                      std::move(pointcloud->runtime->bake_materials),
                      data_block_map);
  }
  if (Volume *volume = geometry.get_volume_for_write()) {
    restore_materials(
        &volume->mat, &volume->totcol, std::move(volume->runtime->bake_materials), data_block_map);
  }
  if (bke::Instances *instances = geometry.get_instances_for_write()) {
    for (bke::InstanceReference &reference : instances->references_for_write()) {
      if (reference.type() == bke::InstanceReference::Type::GeometrySet) {
        restore_data_blocks_recursive(reference.geometry_set(), data_block_map);
      }
    }
  }
}

void GeometryBakeItem::try_restore_data_blocks(GeometrySet &main_geometry,
                                               BakeDataBlockMap *data_block_map)
{
  restore_data_blocks_recursive(main_geometry, data_block_map);
}

#ifdef WITH_OPENVDB
VolumeGridBakeItem::VolumeGridBakeItem(std::unique_ptr<GVolumeGrid> grid) : grid(std::move(grid))
{
}

VolumeGridBakeItem::~VolumeGridBakeItem() = default;

void VolumeGridBakeItem::count_memory(MemoryCounter &memory) const
{
  if (grid && *grid) {
    grid->get().count_memory(memory);
  }
}

#endif

ListBakeItem::ListBakeItem(nodes::ListPtr list) : value(std::move(list)) {}

ListBakeItem::ListBakeItem(Vector<BundleBakeItem> &&items) : value(std::move(items)) {}

ListBakeItem::~ListBakeItem() = default;

void ListBakeItem::count_memory(MemoryCounter & /*memory*/) const
{
  /* TODO this function seems unused atm. */
}

PrimitiveBakeItem::PrimitiveBakeItem(const CPPType &type, const void *value) : type_(type)
{
  value_ = MEM_mallocN_aligned(type.size, type.alignment, __func__);
  type.copy_construct(value, value_);
}

PrimitiveBakeItem::~PrimitiveBakeItem()
{
  type_.destruct(value_);
  MEM_freeN(value_);
}

StringBakeItem::StringBakeItem(std::string value) : value_(std::move(value)) {}

void StringBakeItem::count_memory(MemoryCounter &memory) const
{
  memory.add(value_.size());
}

BakeStateRef::BakeStateRef(const BakeState &bake_state)
{
  this->items_by_id.reserve(bake_state.items_by_id.size());
  for (auto item : bake_state.items_by_id.items()) {
    this->items_by_id.add_new(item.key, item.value.get());
  }
}

void BakeState::count_memory(MemoryCounter &memory) const
{
  for (const std::unique_ptr<BakeItem> &item : items_by_id.values()) {
    if (item) {
      item->count_memory(memory);
    }
  }
}

void BakeItem::count_memory(MemoryCounter & /*memory*/) const {}

}  // namespace blender::bke::bake
