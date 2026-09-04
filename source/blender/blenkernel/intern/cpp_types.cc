/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BLI_cpp_type_make.hh"

#include "BKE_cpp_types.hh"
#include "BKE_geometry_nodes_reference_set.hh"
#include "BKE_geometry_set.hh"
#include "BKE_instances.hh"
#include "BKE_node_socket_value.hh"
#include "BKE_volume_grid.hh"

#include "NOD_geometry_nodes_bundle.hh"
#include "NOD_geometry_nodes_closure.hh"
#include "NOD_geometry_nodes_list.hh"
#include "NOD_geometry_nodes_values.hh"
#include "NOD_menu_value.hh"

#include "DNA_meshdata_types.h"

namespace blender {

struct Tex;
struct Image;
struct Material;

void BKE_cpp_types_init()
{
  register_cpp_types();

  BLI_CPP_TYPE_REGISTER(bke::GeometrySet,
                        CPPTypeFlags::Printable | CPPTypeFlags::EqualityComparable);
  BLI_CPP_TYPE_REGISTER(bke::InstanceReference, CPPTypeFlags::None);

  BLI_CPP_TYPE_REGISTER(Object *, CPPTypeFlags::BasicType);
  BLI_CPP_TYPE_REGISTER(Collection *, CPPTypeFlags::BasicType);
  BLI_CPP_TYPE_REGISTER(Tex *, CPPTypeFlags::BasicType);
  BLI_CPP_TYPE_REGISTER(Image *, CPPTypeFlags::BasicType);
  BLI_CPP_TYPE_REGISTER(Material *, CPPTypeFlags::BasicType);
  BLI_CPP_TYPE_REGISTER(VFont *, CPPTypeFlags::BasicType);
  BLI_CPP_TYPE_REGISTER(Scene *, CPPTypeFlags::BasicType);
  BLI_CPP_TYPE_REGISTER(Text *, CPPTypeFlags::BasicType);
  BLI_CPP_TYPE_REGISTER(Mask *, CPPTypeFlags::BasicType);
  BLI_CPP_TYPE_REGISTER(bSound *, CPPTypeFlags::BasicType);

  BLI_CPP_TYPE_REGISTER(MStringProperty, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(nodes::MenuValue,
                        CPPTypeFlags::Hashable | CPPTypeFlags::EqualityComparable);
  BLI_CPP_TYPE_REGISTER(nodes::BundlePtr, CPPTypeFlags::EqualityComparable);
  BLI_CPP_TYPE_REGISTER(nodes::ClosurePtr, CPPTypeFlags::EqualityComparable);
  BLI_CPP_TYPE_REGISTER(nodes::GListPtr, CPPTypeFlags::EqualityComparable);

  BLI_CPP_TYPE_REGISTER(bke::GeometryNodesReferenceSet, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(bke::SocketValueVariant, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(nodes::GeoNodesMultiInput<bke::SocketValueVariant>, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(nodes::BundleItemValue, CPPTypeFlags::None);

  BLI_CPP_TYPE_REGISTER(fn::GField, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(fn::Field<float>, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(fn::Field<float2>, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(fn::Field<float3>, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(fn::Field<float4>, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(fn::Field<int>, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(fn::Field<int2>, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(fn::Field<bool>, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(fn::Field<int8_t>, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(fn::Field<short2>, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(fn::Field<ColorGeometry4f>, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(fn::Field<ColorGeometry4b>, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(fn::Field<math::Quaternion>, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(fn::Field<float4x4>, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(fn::Field<nodes::MenuValue>, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(fn::Field<std::string>, CPPTypeFlags::None);

  BLI_CPP_TYPE_REGISTER(nodes::GListPtr, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(nodes::ListPtr<float>, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(nodes::ListPtr<float2>, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(nodes::ListPtr<float3>, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(nodes::ListPtr<float4>, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(nodes::ListPtr<int>, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(nodes::ListPtr<int2>, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(nodes::ListPtr<bool>, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(nodes::ListPtr<int8_t>, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(nodes::ListPtr<short2>, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(nodes::ListPtr<ColorGeometry4f>, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(nodes::ListPtr<ColorGeometry4b>, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(nodes::ListPtr<math::Quaternion>, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(nodes::ListPtr<float4x4>, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(nodes::ListPtr<std::string>, CPPTypeFlags::None);

#ifdef WITH_OPENVDB
  BLI_CPP_TYPE_REGISTER(bke::volume_grid::GVolumeGrid, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(bke::volume_grid::VolumeGrid<float>, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(bke::volume_grid::VolumeGrid<float3>, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(bke::volume_grid::VolumeGrid<bool>, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(bke::volume_grid::VolumeGrid<double>, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(bke::volume_grid::VolumeGrid<int>, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(bke::volume_grid::VolumeGrid<int64_t>, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(bke::volume_grid::VolumeGrid<int3>, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(bke::volume_grid::VolumeGrid<double3>, CPPTypeFlags::None);
#endif
}

}  // namespace blender
