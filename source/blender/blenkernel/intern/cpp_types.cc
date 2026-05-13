/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_cpp_type_make.hh"

#include "BKE_cpp_types.hh"
#include "BKE_geometry_nodes_reference_set.hh"
#include "BKE_geometry_set.hh"
#include "BKE_instances.hh"
#include "BKE_node_socket_value.hh"

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
  BLI_CPP_TYPE_REGISTER(bke::SocketValueVariant, CPPTypeFlags::Printable);
  BLI_CPP_TYPE_REGISTER(nodes::GeoNodesMultiInput<bke::SocketValueVariant>, CPPTypeFlags::None);
  BLI_CPP_TYPE_REGISTER(nodes::BundleItemValue, CPPTypeFlags::None);
}

}  // namespace blender
