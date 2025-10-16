/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_attribute_types.h"

#include "BKE_attribute.h"
#include "BKE_attribute.hh"
#include "BKE_attribute_storage.hh"

struct CustomData;
namespace blender::bke {
class CurvesGeometry;
}
struct PointCloud;
struct GreasePencil;
struct Mesh;

namespace blender::bke {

const CPPType *custom_data_type_to_cpp_type(eCustomDataType type);
eCustomDataType cpp_type_to_custom_data_type(const CPPType &type);

/**
 * Convert a custom data type to an attribute type. May return `std::nullopt` if the custom data
 * type isn't used at runtime, is not a generic type that can be stored as an attribute, or is only
 * used for #BMesh.
 */
std::optional<AttrType> custom_data_type_to_attr_type(eCustomDataType data_type);

/**
 * Convert an attribute type to a legacy custom data type.
 */
std::optional<eCustomDataType> attr_type_to_custom_data_type(AttrType attr_type);

/**
 * Move attributes from the #AttributeStorage to the mesh's #CustomData structs. Used for forward
 * compatibility: converting newer files written with #AttributeStorage while #CustomData is still
 * used at runtime.
 */
void mesh_convert_storage_to_customdata(Mesh &mesh);

/**
 * Move generic attributes from #CustomData to #AttributeStorage (not including non-generic layer
 * types). Use for versioning old files when the newer #AttributeStorage format is used at runtime.
 */
void mesh_convert_customdata_to_storage(Mesh &mesh);

/** See #mesh_convert_customdata_to_storage. */
void curves_convert_customdata_to_storage(CurvesGeometry &curves);

/** See #mesh_convert_customdata_to_storage. */
void pointcloud_convert_customdata_to_storage(PointCloud &pointcloud);

/** See #mesh_convert_customdata_to_storage. */
void grease_pencil_convert_customdata_to_storage(GreasePencil &grease_pencil);

}  // namespace blender::bke
