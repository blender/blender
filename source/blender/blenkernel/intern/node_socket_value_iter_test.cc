/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_geometry_set.hh"
#include "BKE_gtest_base.hh"

#include "BKE_instances.hh"
#include "BKE_mesh.h"
#include "BKE_node_socket_value_iter.hh"
#include "NOD_geometry_nodes_bundle.hh"
#include "NOD_geometry_nodes_list.hh"

namespace blender::bke::socket_value_visitor::tests {

class SocketValueVisitorTest : public BlenderGTestBase {};

TEST_F(SocketValueVisitorTest, simple_attribute_check)
{
  GeometrySet geometry = GeometrySet::from_mesh(BKE_mesh_new_nomain(0, 0, 0, 0));
  Set<std::string> names;
  auto check_attributes = [&](const AttributeAccessor &attributes) {
    attributes.foreach_attribute([&](const AttributeIter &iter) { names.add(iter.name); });
    return VisitParams::continue_check(true);
  };
  VisitParams params;
  params.check_AttributeAccessor = check_attributes;
  check_recursive(geometry, params);
  EXPECT_TRUE(names.contains("position"));
}

TEST_F(SocketValueVisitorTest, edit_in_nested_mesh)
{
  Mesh *mesh = BKE_mesh_new_nomain(0, 0, 0, 0);

  /* This deep data-structure is also build in a way that the mesh is never copied in the process
   * due to it having two users. */
  GeometrySet mesh_geometry = GeometrySet::from_mesh(mesh);
  Instances *instances = new Instances(1);
  instances->reference_handles_for_write().first() = instances->add_new_reference(
      std::move(mesh_geometry));
  GeometrySet instance_geometry = GeometrySet::from_instances(instances);
  Vector<GeometrySet> geometry_set_list;
  geometry_set_list.append(std::move(instance_geometry));
  nodes::GListPtr list_ptr = nodes::GList::from_container(std::move(geometry_set_list));
  nodes::BundlePtr bundle_ptr = nodes::Bundle::create();
  bundle_ptr.ensure_mutable_inplace().add(*nodes::BundleKey::from_str("test"),
                                          SocketValueVariant::From(std::move(list_ptr)));
  SocketValueVariant value = SocketValueVariant::From(std::move(bundle_ptr));

  auto attributes_need_edit = [&](const AttributeAccessor & /*attributes*/) { return true; };
  auto edit_attributes = [&](MutableAttributeAccessor &attributes) {
    attributes.add("my attribute", AttrDomain::Point, AttrType::Float, AttributeInitValue(1.0f));
    return;
  };
  VisitParams params;
  params.check_AttributeAccessor = attributes_need_edit;
  params.edit_AttributeAccessor = edit_attributes;
  edit_recursive(value, params);

  EXPECT_TRUE(mesh->attributes().contains("my attribute"));
}

TEST_F(SocketValueVisitorTest, empty_bundle_list_owns_direct_data)
{
  const CPPType &type = CPPType::get<nodes::BundlePtr>();
  nodes::GListPtr list = nodes::GList::create(
      type, nodes::GList::SingleData::ForDefaultValue(type), 0);
  SocketValueVariant value = SocketValueVariant::From(std::move(list));

  EXPECT_TRUE(value.owns_direct_data());
  value.ensure_owns_direct_data();
  EXPECT_TRUE(value.owns_direct_data());
}

}  // namespace blender::bke::socket_value_visitor::tests
