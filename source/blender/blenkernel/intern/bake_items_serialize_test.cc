/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "testing/testing.h"

#include "BKE_bake_items_serialize.hh"
#include "BKE_geometry_set.hh"
#include "BKE_gtest_base.hh"
#include "BKE_node.hh"

#include "NOD_geometry_nodes_bundle.hh"
#include "NOD_geometry_nodes_list.hh"

#include <sstream>
#include <string>

namespace blender::bke::bake::tests {

class BakeItemsSerializeTest : public BlenderGTestBase {};

static std::optional<BakeValues> roundtrip_bake_values(const BakeValues &bake_values)
{
  MemoryBlobWriter blob_writer{"test"};
  BlobWriteSharing blob_write_sharing;
  std::ostringstream stream;
  serialize_bake(bake_values, blob_writer, blob_write_sharing, stream);

  Map<std::string, std::string> blobs;
  for (const auto &item : blob_writer.get_stream_by_name().items()) {
    blobs.add_new(item.key, item.value.stream->str());
  }

  MemoryBlobReader blob_reader;
  for (auto item : blobs.items()) {
    std::string &blob = item.value;
    blob_reader.add(item.key,
                    Span<std::byte>(reinterpret_cast<std::byte *>(blob.data()), blob.size()));
  }

  BlobReadSharing blob_read_sharing;
  std::istringstream read_stream{stream.str()};
  return deserialize_bake(read_stream, blob_reader, blob_read_sharing);
}

TEST_F(BakeItemsSerializeTest, nested_bundle_with_geometry_and_bundle_lists)
{
  nodes::BundlePtr bundle_ptr = nodes::Bundle::create();
  nodes::Bundle &bundle = bundle_ptr.ensure_mutable_inplace();

  Vector<GeometrySet> geometries;
  geometries.append(GeometrySet());
  geometries.append(GeometrySet());
  bundle.add(*nodes::BundleKey::from_str("geometries"),
             nodes::BundleItemSocketValue{
                 node_socket_type_find_static(SOCK_GEOMETRY),
                 SocketValueVariant::From(nodes::GList::from_container(std::move(geometries)))});

  Vector<std::string> strings;
  strings.append("cloth");
  strings.append("simulation");
  bundle.add(*nodes::BundleKey::from_str("strings"),
             nodes::BundleItemSocketValue{
                 node_socket_type_find_static(SOCK_STRING),
                 SocketValueVariant::From(nodes::GList::from_container(std::move(strings)))});

  Vector<nodes::BundlePtr> child_bundles;
  child_bundles.append(nodes::Bundle::create());
  child_bundles.append(nodes::Bundle::create());
  bundle.add(*nodes::BundleKey::from_str("bundles"),
             nodes::BundleItemSocketValue{node_socket_type_find_static(SOCK_BUNDLE),
                                          SocketValueVariant::From(nodes::GList::from_container(
                                              std::move(child_bundles)))});

  Map<int, BakeValues::Item> items;
  items.add_new(0, BakeValues::Item{SocketValueVariant::From(std::move(bundle_ptr))});

  const std::optional<BakeValues> bake_values = roundtrip_bake_values(
      BakeValues(std::move(items)));
  ASSERT_TRUE(bake_values);
  const BakeValues::Item *item = bake_values->values_by_id().lookup_ptr(0);
  ASSERT_NE(item, nullptr);

  nodes::BundlePtr restored_bundle = item->value.get<nodes::BundlePtr>();
  ASSERT_TRUE(restored_bundle);

  std::optional<nodes::GListPtr> restored_geometries = restored_bundle->lookup<nodes::GListPtr>(
      *nodes::BundleKey::from_str("geometries"));
  ASSERT_TRUE(restored_geometries);
  ASSERT_TRUE(*restored_geometries);
  EXPECT_TRUE((*restored_geometries)->cpp_type().is<GeometrySet>());
  EXPECT_EQ((*restored_geometries)->size(), 2);

  std::optional<nodes::GListPtr> restored_strings = restored_bundle->lookup<nodes::GListPtr>(
      *nodes::BundleKey::from_str("strings"));
  ASSERT_TRUE(restored_strings);
  ASSERT_TRUE(*restored_strings);
  EXPECT_TRUE((*restored_strings)->cpp_type().is<std::string>());
  EXPECT_EQ((*restored_strings)->size(), 2);

  std::optional<nodes::GListPtr> restored_bundles = restored_bundle->lookup<nodes::GListPtr>(
      *nodes::BundleKey::from_str("bundles"));
  ASSERT_TRUE(restored_bundles);
  ASSERT_TRUE(*restored_bundles);
  EXPECT_TRUE((*restored_bundles)->cpp_type().is<nodes::BundlePtr>());
  EXPECT_EQ((*restored_bundles)->size(), 2);
}

}  // namespace blender::bke::bake::tests
