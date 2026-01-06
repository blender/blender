/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_cpp_type_make.hh"
#include "BLI_cpp_types_make.hh"

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

BLI_CPP_TYPE_MAKE(bke::GeometrySet, CPPTypeFlags::Printable | CPPTypeFlags::EqualityComparable);
BLI_CPP_TYPE_MAKE(bke::InstanceReference, CPPTypeFlags::None)

BLI_VECTOR_CPP_TYPE_MAKE(bke::GeometrySet);

BLI_CPP_TYPE_MAKE(Object *, CPPTypeFlags::BasicType)
BLI_CPP_TYPE_MAKE(Collection *, CPPTypeFlags::BasicType)
BLI_CPP_TYPE_MAKE(Tex *, CPPTypeFlags::BasicType)
BLI_CPP_TYPE_MAKE(Image *, CPPTypeFlags::BasicType)
BLI_CPP_TYPE_MAKE(Material *, CPPTypeFlags::BasicType)
BLI_CPP_TYPE_MAKE(VFont *, CPPTypeFlags::BasicType)
BLI_CPP_TYPE_MAKE(Scene *, CPPTypeFlags::BasicType)
BLI_CPP_TYPE_MAKE(Text *, CPPTypeFlags::BasicType)
BLI_CPP_TYPE_MAKE(Mask *, CPPTypeFlags::BasicType)
BLI_CPP_TYPE_MAKE(bSound *, CPPTypeFlags::BasicType)

BLI_CPP_TYPE_MAKE(MStringProperty, CPPTypeFlags::None);
BLI_CPP_TYPE_MAKE(nodes::MenuValue, CPPTypeFlags::Hashable | CPPTypeFlags::EqualityComparable);
BLI_CPP_TYPE_MAKE(nodes::BundlePtr, CPPTypeFlags::EqualityComparable);
BLI_CPP_TYPE_MAKE(nodes::ClosurePtr, CPPTypeFlags::EqualityComparable);
BLI_CPP_TYPE_MAKE(nodes::ListPtr, CPPTypeFlags::EqualityComparable);

BLI_CPP_TYPE_MAKE(bke::GeometryNodesReferenceSet, CPPTypeFlags::None);
BLI_CPP_TYPE_MAKE(bke::SocketValueVariant, CPPTypeFlags::Printable);
BLI_VECTOR_CPP_TYPE_MAKE(bke::SocketValueVariant);
BLI_CPP_TYPE_MAKE(nodes::GeoNodesMultiInput<bke::SocketValueVariant>, CPPTypeFlags::None);
BLI_CPP_TYPE_MAKE(nodes::BundleItemValue, CPPTypeFlags::None);

void BKE_cpp_types_init()
{
  register_cpp_types();

  BLI_CPP_TYPE_REGISTER(bke::GeometrySet);
  BLI_CPP_TYPE_REGISTER(bke::InstanceReference);

  BLI_VECTOR_CPP_TYPE_REGISTER(bke::GeometrySet);

  BLI_CPP_TYPE_REGISTER(Object *);
  BLI_CPP_TYPE_REGISTER(Collection *);
  BLI_CPP_TYPE_REGISTER(Tex *);
  BLI_CPP_TYPE_REGISTER(Image *);
  BLI_CPP_TYPE_REGISTER(Material *);
  BLI_CPP_TYPE_REGISTER(VFont *);
  BLI_CPP_TYPE_REGISTER(Scene *);
  BLI_CPP_TYPE_REGISTER(Text *);
  BLI_CPP_TYPE_REGISTER(Mask *);
  BLI_CPP_TYPE_REGISTER(bSound *);

  BLI_CPP_TYPE_REGISTER(MStringProperty);
  BLI_CPP_TYPE_REGISTER(nodes::MenuValue);
  BLI_CPP_TYPE_REGISTER(nodes::BundlePtr);
  BLI_CPP_TYPE_REGISTER(nodes::ClosurePtr);
  BLI_CPP_TYPE_REGISTER(nodes::ListPtr);

  BLI_CPP_TYPE_REGISTER(bke::GeometryNodesReferenceSet);
  BLI_CPP_TYPE_REGISTER(bke::SocketValueVariant);
  BLI_VECTOR_CPP_TYPE_REGISTER(bke::SocketValueVariant);
  BLI_CPP_TYPE_REGISTER(nodes::GeoNodesMultiInput<bke::SocketValueVariant>);
  BLI_CPP_TYPE_REGISTER(nodes::BundleItemValue);
}

}  // namespace blender
