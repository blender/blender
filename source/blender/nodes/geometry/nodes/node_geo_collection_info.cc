/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "BLI_math_matrix.h"

#include "DNA_collection_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "BKE_collection.h"

#include "node_geometry_util.hh"

#include <algorithm>

namespace blender::nodes {

static void geo_node_collection_info_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Collection>("Collection").hide_label();
  b.add_input<decl::Bool>("Separate Children")
      .description(
          "Output each child of the collection as a separate instance, sorted alphabetically");
  b.add_input<decl::Bool>("Reset Children")
      .description(
          "Reset the transforms of every child instance in the output. Only used when Separate "
          "Children is enabled");
  b.add_output<decl::Geometry>("Geometry");
}

static void geo_node_collection_info_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "transform_space", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
}

static void geo_node_collection_info_node_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryCollectionInfo *data = (NodeGeometryCollectionInfo *)MEM_callocN(
      sizeof(NodeGeometryCollectionInfo), __func__);
  data->transform_space = GEO_NODE_TRANSFORM_SPACE_ORIGINAL;
  node->storage = data;
}

struct InstanceListEntry {
  int handle;
  char *name;
  float4x4 transform;
};

static void geo_node_collection_info_exec(GeoNodeExecParams params)
{
  Collection *collection = params.get_input<Collection *>("Collection");

  GeometrySet geometry_set_out;

  if (collection == nullptr) {
    params.set_output("Geometry", geometry_set_out);
    return;
  }

  const bNode &bnode = params.node();
  NodeGeometryCollectionInfo *node_storage = (NodeGeometryCollectionInfo *)bnode.storage;
  const bool use_relative_transform = (node_storage->transform_space ==
                                       GEO_NODE_TRANSFORM_SPACE_RELATIVE);

  InstancesComponent &instances = geometry_set_out.get_component_for_write<InstancesComponent>();

  const Object *self_object = params.self_object();

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

    instances.reserve(children_collections.size() + children_objects.size());
    Vector<InstanceListEntry> entries;
    entries.reserve(children_collections.size() + children_objects.size());

    for (Collection *child_collection : children_collections) {
      float4x4 transform = float4x4::identity();
      if (!reset_children) {
        add_v3_v3(transform.values[3], child_collection->instance_offset);
        if (use_relative_transform) {
          mul_m4_m4_pre(transform.values, self_object->imat);
        }
        else {
          sub_v3_v3(transform.values[3], collection->instance_offset);
        }
      }
      const int handle = instances.add_reference(*child_collection);
      entries.append({handle, &(child_collection->id.name[2]), transform});
    }
    for (Object *child_object : children_objects) {
      const int handle = instances.add_reference(*child_object);
      float4x4 transform = float4x4::identity();
      if (!reset_children) {
        if (use_relative_transform) {
          transform = self_object->imat;
        }
        else {
          sub_v3_v3(transform.values[3], collection->instance_offset);
        }
        mul_m4_m4_post(transform.values, child_object->obmat);
      }
      entries.append({handle, &(child_object->id.name[2]), transform});
    }

    std::sort(entries.begin(),
              entries.end(),
              [](const InstanceListEntry &a, const InstanceListEntry &b) {
                return BLI_strcasecmp_natural(a.name, b.name) <= 0;
              });
    for (const InstanceListEntry &entry : entries) {
      instances.add_instance(entry.handle, entry.transform);
    }
  }
  else {
    float4x4 transform = float4x4::identity();
    if (use_relative_transform) {
      copy_v3_v3(transform.values[3], collection->instance_offset);
      mul_m4_m4_pre(transform.values, self_object->imat);
    }

    const int handle = instances.add_reference(*collection);
    instances.add_instance(handle, transform);
  }

  params.set_output("Geometry", geometry_set_out);
}

}  // namespace blender::nodes

void register_node_type_geo_collection_info()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_COLLECTION_INFO, "Collection Info", NODE_CLASS_INPUT, 0);
  ntype.declare = blender::nodes::geo_node_collection_info_declare;
  node_type_init(&ntype, blender::nodes::geo_node_collection_info_node_init);
  node_type_storage(&ntype,
                    "NodeGeometryCollectionInfo",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  ntype.geometry_node_execute = blender::nodes::geo_node_collection_info_exec;
  ntype.draw_buttons = blender::nodes::geo_node_collection_info_layout;
  nodeRegisterType(&ntype);
}
