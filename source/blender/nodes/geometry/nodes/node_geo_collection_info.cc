/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_matrix.h"

#include "DNA_collection_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "BKE_collection.h"
#include "BKE_instances.hh"

#include "node_geometry_util.hh"

#include <algorithm>

namespace blender::nodes::node_geo_collection_info_cc {

NODE_STORAGE_FUNCS(NodeGeometryCollectionInfo)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Collection>("Collection").hide_label();
  b.add_input<decl::Bool>("Separate Children")
      .description(
          "Output each child of the collection as a separate instance, sorted alphabetically");
  b.add_input<decl::Bool>("Reset Children")
      .description(
          "Reset the transforms of every child instance in the output. Only used when Separate "
          "Children is enabled");
  b.add_output<decl::Geometry>("Instances");
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "transform_space", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
}

static void node_node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryCollectionInfo *data = MEM_cnew<NodeGeometryCollectionInfo>(__func__);
  data->transform_space = GEO_NODE_TRANSFORM_SPACE_ORIGINAL;
  node->storage = data;
}

struct InstanceListEntry {
  int handle;
  char *name;
  float4x4 transform;
};

static void node_geo_exec(GeoNodeExecParams params)
{
  Collection *collection = params.get_input<Collection *>("Collection");

  if (collection == nullptr) {
    params.set_default_remaining_outputs();
    return;
  }
  const Object *self_object = params.self_object();
  const bool is_recursive = BKE_collection_has_object_recursive_instanced(
      collection, const_cast<Object *>(self_object));
  if (is_recursive) {
    params.error_message_add(NodeWarningType::Error, TIP_("Collection contains current object"));
    params.set_default_remaining_outputs();
    return;
  }

  const NodeGeometryCollectionInfo &storage = node_storage(params.node());
  const bool use_relative_transform = (storage.transform_space ==
                                       GEO_NODE_TRANSFORM_SPACE_RELATIVE);

  std::unique_ptr<bke::Instances> instances = std::make_unique<bke::Instances>();

  const bool separate_children = params.get_input<bool>("Separate Children");
  if (separate_children) {
    const bool reset_children = params.get_input<bool>("Reset Children");
    Vector<Collection *> children_collections;
    LISTBASE_FOREACH (CollectionChild *, collection_child, &collection->children) {
      children_collections.append(collection_child->collection);
    }
    Vector<Object *> children_objects;
    LISTBASE_FOREACH (CollectionObject *, collection_object, &collection->gobject) {
      children_objects.append(collection_object->ob);
    }

    instances->reserve(children_collections.size() + children_objects.size());
    Vector<InstanceListEntry> entries;
    entries.reserve(children_collections.size() + children_objects.size());

    for (Collection *child_collection : children_collections) {
      float4x4 transform = float4x4::identity();
      if (!reset_children) {
        transform.location() += float3(child_collection->instance_offset);
        if (use_relative_transform) {
          transform = float4x4(self_object->world_to_object) * transform;
        }
        else {
          transform.location() -= float3(collection->instance_offset);
        }
      }
      const int handle = instances->add_reference(*child_collection);
      entries.append({handle, &(child_collection->id.name[2]), transform});
    }
    for (Object *child_object : children_objects) {
      const int handle = instances->add_reference(*child_object);
      float4x4 transform = float4x4::identity();
      if (!reset_children) {
        if (use_relative_transform) {
          transform = float4x4(self_object->world_to_object);
        }
        else {
          transform.location() -= float3(collection->instance_offset);
        }
        transform *= float4x4(child_object->object_to_world);
      }
      entries.append({handle, &(child_object->id.name[2]), transform});
    }

    std::sort(entries.begin(),
              entries.end(),
              [](const InstanceListEntry &a, const InstanceListEntry &b) {
                return BLI_strcasecmp_natural(a.name, b.name) < 0;
              });
    for (const InstanceListEntry &entry : entries) {
      instances->add_instance(entry.handle, entry.transform);
    }
  }
  else {
    float4x4 transform = float4x4::identity();
    if (use_relative_transform) {
      transform.location() = collection->instance_offset;
      transform = float4x4_view(self_object->world_to_object) * transform;
    }

    const int handle = instances->add_reference(*collection);
    instances->add_instance(handle, transform);
  }

  params.set_output("Instances", GeometrySet::from_instances(instances.release()));
}

}  // namespace blender::nodes::node_geo_collection_info_cc

void register_node_type_geo_collection_info()
{
  namespace file_ns = blender::nodes::node_geo_collection_info_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_COLLECTION_INFO, "Collection Info", NODE_CLASS_INPUT);
  ntype.declare = file_ns::node_declare;
  ntype.initfunc = file_ns::node_node_init;
  node_type_storage(&ntype,
                    "NodeGeometryCollectionInfo",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  nodeRegisterType(&ntype);
}
