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
    case CD_NUMTYPES:
    case CD_AUTO_FROM_NAME:
      /* These type is not used for actual #CustomData layers. */
      BLI_assert_unreachable();
      return std::nullopt;
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
      /* These types are only used for versioning old files. */
      return std::nullopt;
    /* These types are only used for #BMesh. */
    case CD_SHAPEKEY:
    case CD_SHAPE_KEYINDEX:
    case CD_BM_ELEM_PYPTR:
      return std::nullopt;
    case CD_MDEFORMVERT:
    case CD_MFACE:
    case CD_MCOL:
    case CD_ORIGINDEX:
    case CD_NORMAL:
    case CD_ORIGSPACE:
    case CD_ORCO:
    case CD_TANGENT:
    case CD_MDISPS:
    case CD_CLOTH_ORCO:
    case CD_ORIGSPACE_MLOOP:
    case CD_GRID_PAINT_MASK:
    case CD_MVERT_SKIN:
    case CD_FREESTYLE_EDGE:
    case CD_FREESTYLE_FACE:
    case CD_MLOOPTANGENT:
      /* These types are not generic. They will either be moved to some generic data type or
       * #AttributeStorage will be extended to be able to support a similar format. */
      return std::nullopt;
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
  const CustomData &data;
  int size;
};

static AttributeStorage attribute_legacy_convert_customdata_to_storage(
    const Map<AttrDomain, CustomDataAndSize> &domains)
{
  AttributeStorage storage{};
  struct AttributeToAdd {
    std::string name;
    AttrDomain domain;
    AttrType type;
    void *array_data;
    int array_size;
    const ImplicitSharingInfo *sharing_info;
  };
  Vector<AttributeToAdd> attributes_to_add;
  for (const auto &item : domains.items()) {
    const AttrDomain domain = item.key;
    const CustomData &custom_data = item.value.data;
    const int domain_size = item.value.size;
    for (const CustomDataLayer &layer : MutableSpan(custom_data.layers, custom_data.totlayer)) {
      const std::optional<AttrType> attr_type = custom_data_type_to_attr_type(
          eCustomDataType(layer.type));
      if (!attr_type) {
        continue;
      }
      attributes_to_add.append(
          {layer.name, domain, *attr_type, layer.data, domain_size, layer.sharing_info});
      layer.sharing_info->add_user();
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

  return storage;
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
}
AttributeStorage mesh_convert_customdata_to_storage(const Mesh &mesh)
{
  return bke::attribute_legacy_convert_customdata_to_storage(
      {{AttrDomain::Point, {mesh.vert_data, mesh.verts_num}},
       {AttrDomain::Edge, {mesh.edge_data, mesh.edges_num}},
       {AttrDomain::Face, {mesh.face_data, mesh.faces_num}},
       {AttrDomain::Corner, {mesh.corner_data, mesh.corners_num}}});
}

void curves_convert_storage_to_customdata(CurvesGeometry &curves)
{
  convert_storage_to_customdata(curves.attribute_storage.wrap(),
                                {{AttrDomain::Point, {curves.point_data, curves.points_num()}},
                                 {AttrDomain::Curve, {curves.curve_data, curves.curves_num()}}});
}
AttributeStorage curves_convert_customdata_to_storage(const CurvesGeometry &curves)
{
  return attribute_legacy_convert_customdata_to_storage(
      {{AttrDomain::Point, {curves.point_data, curves.points_num()}},
       {AttrDomain::Curve, {curves.curve_data, curves.curves_num()}}});
}

void pointcloud_convert_storage_to_customdata(PointCloud &pointcloud)
{
  convert_storage_to_customdata(pointcloud.attribute_storage.wrap(),
                                {{AttrDomain::Point, {pointcloud.pdata, pointcloud.totpoint}}});
}

AttributeStorage pointcloud_convert_customdata_to_storage(const PointCloud &pointcloud)
{
  return attribute_legacy_convert_customdata_to_storage(
      {{AttrDomain::Point, {pointcloud.pdata, pointcloud.totpoint}}});
}

void grease_pencil_convert_storage_to_customdata(GreasePencil &grease_pencil)
{
  convert_storage_to_customdata(
      grease_pencil.attribute_storage.wrap(),
      {{AttrDomain::Layer, {grease_pencil.layers_data, int(grease_pencil.layers().size())}}});
}
AttributeStorage grease_pencil_convert_customdata_to_storage(const GreasePencil &grease_pencil)
{
  return attribute_legacy_convert_customdata_to_storage(
      {{AttrDomain::Layer, {grease_pencil.layers_data, int(grease_pencil.layers().size())}}});
}

}  // namespace blender::bke
