/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "DNA_collection_types.h"

#include "NOD_rna_define.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "BKE_collection.hh"
#include "BKE_instances.hh"

#include "DEG_depsgraph_query.hh"

#include "node_geometry_util.hh"

#include <algorithm>

namespace blender::nodes::node_geo_collection_info_cc {

NODE_STORAGE_FUNCS(NodeGeometryCollectionInfo)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Collection>("Collection").optional_label();
  b.add_input<decl::Bool>("Separate Children")
      .description(
          "Output each child of the collection as a separate instance, sorted alphabetically");
  b.add_input<decl::Bool>("Reset Children")
      .description(
          "Reset the transforms of every child instance in the output. Only used when Separate "
          "Children is enabled");
  b.add_output<decl::Geometry>("Instances")
      .description(
          "Instance of the collection or instances of all the children in the collection");
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "transform_space", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
}

static void node_node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryCollectionInfo *data = MEM_callocN<NodeGeometryCollectionInfo>(__func__);
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
  Collection *collection = params.extract_input<Collection *>("Collection");

  if (collection == nullptr) {
    params.set_default_remaining_outputs();
    return;
  }
  const Object *self_object = params.self_object();
  /* Compare by `orig_id` because objects may be copied into separate depsgraphs. */
  const bool is_recursive = BKE_collection_has_object_recursive_instanced_orig_id(
      collection, const_cast<Object *>(self_object));
  if (is_recursive) {
    params.error_message_add(NodeWarningType::Error, TIP_("Collection contains current object"));
    params.set_default_remaining_outputs();
    return;
  }
  if (!DEG_collection_geometry_is_evaluated(*collection)) {
    params.error_message_add(NodeWarningType::Error,
                             TIP_("Cannot access collections geometry because it's not evaluated "
                                  "yet. This can happen when there is a dependency cycle"));
    params.set_default_remaining_outputs();
    return;
  }

  const NodeGeometryCollectionInfo &storage = node_storage(params.node());
  const bool use_relative_transform = (storage.transform_space ==
                                       GEO_NODE_TRANSFORM_SPACE_RELATIVE);

  std::unique_ptr<bke::Instances> instances = std::make_unique<bke::Instances>();

  const bool separate_children = params.extract_input<bool>("Separate Children");
  if (separate_children) {
    const bool reset_children = params.extract_input<bool>("Reset Children");
    Vector<Collection *> children_collections;
    LISTBASE_FOREACH (CollectionChild *, collection_child, &collection->children) {
      children_collections.append(collection_child->collection);
    }
    Vector<Object *> children_objects;
    LISTBASE_FOREACH (CollectionObject *, collection_object, &collection->gobject) {
      children_objects.append(collection_object->ob);
    }

    Vector<InstanceListEntry> entries;
    entries.reserve(children_collections.size() + children_objects.size());

    for (Collection *child_collection : children_collections) {
      float4x4 transform = float4x4::identity();
      if (!reset_children) {
        transform.location() += float3(child_collection->instance_offset);
        if (use_relative_transform) {
          transform = self_object->world_to_object() * transform;
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
          transform = self_object->world_to_object();
        }
        else {
          transform.location() -= float3(collection->instance_offset);
        }
        transform *= child_object->object_to_world();
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
      transform = self_object->world_to_object() * transform;
    }

    const int handle = instances->add_reference(*collection);
    instances->add_instance(handle, transform);
  }
  GeometrySet geometry = GeometrySet::from_instances(instances.release());
  geometry.name = collection->id.name + 2;

  params.set_output("Instances", std::move(geometry));
}

static void node_rna(StructRNA *srna)
{
  static const EnumPropertyItem rna_node_geometry_collection_info_transform_space_items[] = {
      {GEO_NODE_TRANSFORM_SPACE_ORIGINAL,
       "ORIGINAL",
       0,
       "Original",
       "Output the geometry relative to the collection offset"},
      {GEO_NODE_TRANSFORM_SPACE_RELATIVE,
       "RELATIVE",
       0,
       "Relative",
       "Bring the input collection geometry into the modified object, maintaining the relative "
       "position between the objects in the scene"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  PropertyRNA *prop = RNA_def_node_enum(
      srna,
      "transform_space",
      "Transform Space",
      "The transformation of the instances output. Does not affect the internal geometry",
      rna_node_geometry_collection_info_transform_space_items,
      NOD_storage_enum_accessors(transform_space),
      GEO_NODE_TRANSFORM_SPACE_ORIGINAL);
  RNA_def_property_update_runtime(prop, rna_Node_update_relations);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeCollectionInfo", GEO_NODE_COLLECTION_INFO);
  ntype.ui_name = "Collection Info";
  ntype.ui_description = "Retrieve geometry instances from a collection";
  ntype.enum_name_legacy = "COLLECTION_INFO";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = node_declare;
  ntype.initfunc = node_node_init;
  blender::bke::node_type_storage(
      ntype, "NodeGeometryCollectionInfo", node_free_standard_storage, node_copy_standard_storage);
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  blender::bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_collection_info_cc
