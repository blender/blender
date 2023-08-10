/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_bake_items.hh"
#include "BKE_bake_items_serialize.hh"
#include "BKE_curves.hh"
#include "BKE_instances.hh"
#include "BKE_lib_id.h"
#include "BKE_mesh.hh"
#include "BKE_pointcloud.h"

#include "BLI_endian_defines.h"
#include "BLI_endian_switch.h"
#include "BLI_math_matrix_types.hh"
#include "BLI_path_util.h"

#include "DNA_material_types.h"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"

namespace blender::bke {

using namespace io::serialize;
using DictionaryValuePtr = std::shared_ptr<DictionaryValue>;

GeometryBakeItem::GeometryBakeItem(GeometrySet geometry) : geometry(std::move(geometry)) {}

static void remove_materials(Material ***materials, short *materials_num)
{
  MEM_SAFE_FREE(*materials);
  *materials_num = 0;
}

void GeometryBakeItem::cleanup_geometry(GeometrySet &main_geometry)
{
  main_geometry.ensure_owns_all_data();
  main_geometry.modify_geometry_sets([&](GeometrySet &geometry) {
    if (Mesh *mesh = geometry.get_mesh_for_write()) {
      mesh->attributes_for_write().remove_anonymous();
      remove_materials(&mesh->mat, &mesh->totcol);
    }
    if (Curves *curves = geometry.get_curves_for_write()) {
      curves->geometry.wrap().attributes_for_write().remove_anonymous();
      remove_materials(&curves->mat, &curves->totcol);
    }
    if (PointCloud *pointcloud = geometry.get_pointcloud_for_write()) {
      pointcloud->attributes_for_write().remove_anonymous();
      remove_materials(&pointcloud->mat, &pointcloud->totcol);
    }
    if (bke::Instances *instances = geometry.get_instances_for_write()) {
      instances->attributes_for_write().remove_anonymous();
    }
    geometry.keep_only_during_modify({GeometryComponent::Type::Mesh,
                                      GeometryComponent::Type::Curve,
                                      GeometryComponent::Type::PointCloud,
                                      GeometryComponent::Type::Instance});
  });
}

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

}  // namespace blender::bke
