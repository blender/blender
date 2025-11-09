/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#define DNA_DEPRECATED_ALLOW

#include <optional>

#include "DNA_grease_pencil_types.h"
#include "DNA_mesh_types.h"
#include "DNA_pointcloud_types.h"

#include "BKE_attribute.hh"
#include "BKE_curves.hh"
#include "BKE_customdata.hh"

#include "BKE_attribute_legacy_convert.hh"

namespace blender::bke {

std::optional<AttrType> custom_data_type_to_attr_type(const eCustomDataType data_type)
{
  switch (data_type) {
    /* These types are not used for actual #CustomData layers. */
    case CD_NUMTYPES:
    case CD_AUTO_FROM_NAME:
    case CD_TANGENT:
      BLI_assert_unreachable();
      return std::nullopt;

    /* These types are only used for versioning old files. */
    case CD_MVERT:
    case CD_MSTICKY:
    case CD_MEDGE:
    case CD_FACEMAP:
    case CD_MTEXPOLY:
    case CD_MLOOPUV:
    case CD_MPOLY:
    case CD_MLOOP:
    case CD_BWEIGHT:
    case CD_CREASE:
    case CD_PAINT_MASK:
    case CD_CUSTOMLOOPNORMAL:
    case CD_SCULPT_FACE_SETS:
    case CD_MTFACE:
    case CD_TESSLOOPNORMAL:
    case CD_FREESTYLE_EDGE:
    case CD_FREESTYLE_FACE:
      return std::nullopt;

    /* These types are only used for #BMesh. */
    case CD_SHAPEKEY:
    case CD_SHAPE_KEYINDEX:
    case CD_BM_ELEM_PYPTR:
      return std::nullopt;

    /* Only used for legacy #MFace data. */
    case CD_MFACE:
    case CD_ORIGSPACE:
    case CD_MCOL:
      return std::nullopt;

    /* Custom data on vertices. */
    case CD_MDEFORMVERT:
    case CD_MVERT_SKIN:
    case CD_ORCO:
    case CD_CLOTH_ORCO:
      return std::nullopt;

    /* Custom data on face corners. */
    case CD_NORMAL:
    case CD_MDISPS:
    case CD_ORIGSPACE_MLOOP:
    case CD_GRID_PAINT_MASK:
      return std::nullopt;

    /* Use for editing/selecting original data from evaluated mesh (vertices, edges, faces). */
    case CD_ORIGINDEX:
      return std::nullopt;

    /* Used as a cache of tangents for current RNA API (face corners). */
    case CD_MLOOPTANGENT:
      return std::nullopt;

    /* Attribute types. */
    case CD_PROP_FLOAT:
      return AttrType::Float;
    case CD_PROP_INT32:
      return AttrType::Int32;
    case CD_PROP_BYTE_COLOR:
      return AttrType::ColorByte;
    case CD_PROP_FLOAT4X4:
      return AttrType::Float4x4;
    case CD_PROP_INT16_2D:
      return AttrType::Int16_2D;
    case CD_PROP_INT8:
      return AttrType::Int8;
    case CD_PROP_INT32_2D:
      return AttrType::Int32_2D;
    case CD_PROP_COLOR:
      return AttrType::ColorFloat;
    case CD_PROP_FLOAT3:
      return AttrType::Float3;
    case CD_PROP_FLOAT2:
      return AttrType::Float2;
    case CD_PROP_BOOL:
      return AttrType::Bool;
    case CD_PROP_STRING:
      return AttrType::String;
    case CD_PROP_QUATERNION:
      return AttrType::Quaternion;
  }
  return std::nullopt;
}

struct CustomDataAndSize {
  CustomData &data;
  int size;
};

/**
 * Move generic attributes from #CustomData to #AttributeStorage. All other non-generic layers are
 * left in #CustomData.
 */
static void attribute_legacy_convert_customdata_to_storage(
    const Map<AttrDomain, CustomDataAndSize> &domains, AttributeStorage &storage)
{
  struct AttributeToAdd {
    StringRef name;
    AttrDomain domain;
    AttrType type;
    void *array_data;
    int array_size;
    const ImplicitSharingInfo *sharing_info;
  };
  Map<AttrDomain, Vector<CustomDataLayer>> layers_to_keep;
  Vector<AttributeToAdd> attributes_to_add;
  for (const auto &item : domains.items()) {
    const AttrDomain domain = item.key;
    const CustomData &custom_data = item.value.data;
    const int domain_size = item.value.size;
    for (const CustomDataLayer &layer : MutableSpan(custom_data.layers, custom_data.totlayer)) {
      if (const std::optional<AttrType> attr_type = custom_data_type_to_attr_type(
              eCustomDataType(layer.type)))
      {
        /* Skip adding a user. This #CustomDataLayer is just freed below. */
        attributes_to_add.append(
            {layer.name, domain, *attr_type, layer.data, domain_size, layer.sharing_info});
      }
      else {
        layers_to_keep.lookup_or_add_default(domain).append(layer);
      }
    }
  }

  for (AttributeToAdd &attribute : attributes_to_add) {
    bke::Attribute::ArrayData array_data;
    array_data.data = attribute.array_data;
    array_data.size = attribute.array_size;
    array_data.sharing_info = ImplicitSharingPtr<>(attribute.sharing_info);
    storage.add(storage.unique_name_calc(attribute.name),
                attribute.domain,
                attribute.type,
                std::move(array_data));
  }

  for (const auto &[domain, custom_data] : domains.items()) {
    Vector layers_vector = layers_to_keep.pop_default(domain, {});
    MEM_SAFE_FREE(custom_data.data.layers);
    custom_data.data.totlayer = 0;
    custom_data.data.maxlayer = 0;
    if (layers_vector.is_empty()) {
      CustomData_update_typemap(&custom_data.data);
      continue;
    }
    VectorData data = layers_vector.release();
    custom_data.data.layers = data.data;
    custom_data.data.totlayer = data.size;
    custom_data.data.maxlayer = data.capacity;
    CustomData_update_typemap(&custom_data.data);
  }
}

std::optional<eCustomDataType> attr_type_to_custom_data_type(const AttrType attr_type)
{
  switch (attr_type) {
    case AttrType::Bool:
      return CD_PROP_BOOL;
    case AttrType::Int8:
      return CD_PROP_INT8;
    case AttrType::Int16_2D:
      return CD_PROP_INT16_2D;
    case AttrType::Int32:
      return CD_PROP_INT32;
    case AttrType::Int32_2D:
      return CD_PROP_INT32_2D;
    case AttrType::Float:
      return CD_PROP_FLOAT;
    case AttrType::Float2:
      return CD_PROP_FLOAT2;
    case AttrType::Float3:
      return CD_PROP_FLOAT3;
    case AttrType::Float4x4:
      return CD_PROP_FLOAT4X4;
    case AttrType::ColorByte:
      return CD_PROP_BYTE_COLOR;
    case AttrType::ColorFloat:
      return CD_PROP_COLOR;
    case AttrType::Quaternion:
      return CD_PROP_QUATERNION;
    case AttrType::String:
      return CD_PROP_STRING;
  }
  return std::nullopt;
}

struct CustomDataAndSizeMutable {
  CustomData &data;
  int size;
};

static void convert_storage_to_customdata(
    AttributeStorage &storage,
    const Map<AttrDomain, CustomDataAndSizeMutable> &custom_data_domains)
{
  /* Name uniqueness is handled by the #CustomData API. */
  storage.foreach([&](const Attribute &attribute) {
    const std::optional<eCustomDataType> data_type = attr_type_to_custom_data_type(
        attribute.data_type());
    if (!data_type) {
      return;
    }
    CustomData &custom_data = custom_data_domains.lookup(attribute.domain()).data;
    const int domain_size = custom_data_domains.lookup(attribute.domain()).size;
    if (const auto *array_data = std::get_if<Attribute::ArrayData>(&attribute.data())) {
      BLI_assert(array_data->size == domain_size);
      CustomData_add_layer_named_with_data(&custom_data,
                                           *data_type,
                                           array_data->data,
                                           array_data->size,
                                           attribute.name(),
                                           array_data->sharing_info.get());
    }
    else if (const auto *single_data = std::get_if<Attribute::SingleData>(&attribute.data())) {
      const CPPType &cpp_type = *custom_data_type_to_cpp_type(*data_type);
      auto *value = new ImplicitSharedValue<GArray<>>(cpp_type, domain_size);
      cpp_type.fill_construct_n(single_data->value, value->data.data(), domain_size);
      CustomData_add_layer_named_with_data(
          &custom_data, *data_type, value->data.data(), domain_size, attribute.name(), value);
    }
  });
  storage = {};
}

void mesh_convert_storage_to_customdata(Mesh &mesh)
{
  convert_storage_to_customdata(mesh.attribute_storage.wrap(),
                                {{AttrDomain::Point, {mesh.vert_data, mesh.verts_num}},
                                 {AttrDomain::Edge, {mesh.edge_data, mesh.edges_num}},
                                 {AttrDomain::Face, {mesh.face_data, mesh.faces_num}},
                                 {AttrDomain::Corner, {mesh.corner_data, mesh.corners_num}}});
  if (const char *name = mesh.active_uv_map_attribute) {
    const int layer_n = CustomData_get_named_layer(&mesh.corner_data, CD_PROP_FLOAT2, name);
    if (layer_n != -1) {
      CustomData_set_layer_active(&mesh.corner_data, CD_PROP_FLOAT2, layer_n);
    }
    MEM_freeN(mesh.active_uv_map_attribute);
    mesh.active_uv_map_attribute = nullptr;
  }
  if (const char *name = mesh.default_uv_map_attribute) {
    const int layer_n = CustomData_get_named_layer(&mesh.corner_data, CD_PROP_FLOAT2, name);
    if (layer_n != -1) {
      CustomData_set_layer_render(&mesh.corner_data, CD_PROP_FLOAT2, layer_n);
    }
    MEM_freeN(mesh.default_uv_map_attribute);
    mesh.default_uv_map_attribute = nullptr;
  }
}
void mesh_convert_customdata_to_storage(Mesh &mesh)
{
  bke::attribute_legacy_convert_customdata_to_storage(
      {{AttrDomain::Point, {mesh.vert_data, mesh.verts_num}},
       {AttrDomain::Edge, {mesh.edge_data, mesh.edges_num}},
       {AttrDomain::Face, {mesh.face_data, mesh.faces_num}},
       {AttrDomain::Corner, {mesh.corner_data, mesh.corners_num}}},
      mesh.attribute_storage.wrap());
}

void curves_convert_customdata_to_storage(CurvesGeometry &curves)
{
  attribute_legacy_convert_customdata_to_storage(
      {{AttrDomain::Point, {curves.point_data, curves.points_num()}},
       {AttrDomain::Curve, {curves.curve_data_legacy, curves.curves_num()}}},
      curves.attribute_storage.wrap());
  CustomData_reset(&curves.curve_data_legacy);
  /* Update the curve type count again (the first time was done on file-read, where
   * #AttributeStorage data doesn't exist yet for older files). */
  curves.update_curve_types();
}

void pointcloud_convert_customdata_to_storage(PointCloud &pointcloud)
{
  attribute_legacy_convert_customdata_to_storage(
      {{AttrDomain::Point, {pointcloud.pdata_legacy, pointcloud.totpoint}}},
      pointcloud.attribute_storage.wrap());
  CustomData_reset(&pointcloud.pdata_legacy);
}

void grease_pencil_convert_customdata_to_storage(GreasePencil &grease_pencil)
{
  attribute_legacy_convert_customdata_to_storage(
      {{AttrDomain::Layer,
        {grease_pencil.layers_data_legacy, int(grease_pencil.layers().size())}}},
      grease_pencil.attribute_storage.wrap());
  CustomData_reset(&grease_pencil.layers_data_legacy);
}

}  // namespace blender::bke
