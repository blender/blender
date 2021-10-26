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

#include "DNA_collection_types.h"

#include "BLI_hash.h"
#include "BLI_task.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes {

static void geo_node_point_instance_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry");
  b.add_input<decl::Object>("Object").hide_label();
  b.add_input<decl::Collection>("Collection").hide_label();
  b.add_input<decl::Geometry>("Instance Geometry");
  b.add_input<decl::Int>("Seed").min(-10000).max(10000);
  b.add_output<decl::Geometry>("Geometry");
}

static void geo_node_point_instance_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "instance_type", 0, "", ICON_NONE);
  if (RNA_enum_get(ptr, "instance_type") == GEO_NODE_POINT_INSTANCE_TYPE_COLLECTION) {
    uiItemR(layout, ptr, "use_whole_collection", 0, nullptr, ICON_NONE);
  }
}

static void geo_node_point_instance_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryPointInstance *data = (NodeGeometryPointInstance *)MEM_callocN(
      sizeof(NodeGeometryPointInstance), __func__);
  data->instance_type = GEO_NODE_POINT_INSTANCE_TYPE_OBJECT;
  data->flag |= GEO_NODE_POINT_INSTANCE_WHOLE_COLLECTION;
  node->storage = data;
}

static void geo_node_point_instance_update(bNodeTree *UNUSED(tree), bNode *node)
{
  bNodeSocket *object_socket = (bNodeSocket *)BLI_findlink(&node->inputs, 1);
  bNodeSocket *collection_socket = object_socket->next;
  bNodeSocket *instance_geometry_socket = collection_socket->next;
  bNodeSocket *seed_socket = instance_geometry_socket->next;

  NodeGeometryPointInstance *node_storage = (NodeGeometryPointInstance *)node->storage;
  GeometryNodePointInstanceType type = (GeometryNodePointInstanceType)node_storage->instance_type;
  const bool use_whole_collection = (node_storage->flag &
                                     GEO_NODE_POINT_INSTANCE_WHOLE_COLLECTION) != 0;

  nodeSetSocketAvailability(object_socket, type == GEO_NODE_POINT_INSTANCE_TYPE_OBJECT);
  nodeSetSocketAvailability(collection_socket, type == GEO_NODE_POINT_INSTANCE_TYPE_COLLECTION);
  nodeSetSocketAvailability(instance_geometry_socket,
                            type == GEO_NODE_POINT_INSTANCE_TYPE_GEOMETRY);
  nodeSetSocketAvailability(
      seed_socket, type == GEO_NODE_POINT_INSTANCE_TYPE_COLLECTION && !use_whole_collection);
}

static Vector<InstanceReference> get_instance_references__object(GeoNodeExecParams &params)
{
  Object *object = params.extract_input<Object *>("Object");
  if (object == params.self_object()) {
    return {};
  }
  if (object != nullptr) {
    return {*object};
  }
  return {};
}

static Vector<InstanceReference> get_instance_references__collection(GeoNodeExecParams &params)
{
  const bNode &node = params.node();
  NodeGeometryPointInstance *node_storage = (NodeGeometryPointInstance *)node.storage;

  Collection *collection = params.get_input<Collection *>("Collection");
  if (collection == nullptr) {
    return {};
  }

  if (BLI_listbase_is_empty(&collection->children) &&
      BLI_listbase_is_empty(&collection->gobject)) {
    params.error_message_add(NodeWarningType::Info, TIP_("Collection is empty"));
    return {};
  }

  if (node_storage->flag & GEO_NODE_POINT_INSTANCE_WHOLE_COLLECTION) {
    return {*collection};
  }

  Vector<InstanceReference> references;
  /* Direct child objects are instanced as objects. */
  LISTBASE_FOREACH (CollectionObject *, cob, &collection->gobject) {
    references.append(*cob->ob);
  }
  /* Direct child collections are instanced as collections. */
  LISTBASE_FOREACH (CollectionChild *, child, &collection->children) {
    references.append(*child->collection);
  }

  return references;
}

static Vector<InstanceReference> get_instance_references__geometry(GeoNodeExecParams &params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Instance Geometry");
  geometry_set.ensure_owns_direct_data();
  return {std::move(geometry_set)};
}

static Vector<InstanceReference> get_instance_references(GeoNodeExecParams &params)
{
  const bNode &node = params.node();
  NodeGeometryPointInstance *node_storage = (NodeGeometryPointInstance *)node.storage;
  const GeometryNodePointInstanceType type = (GeometryNodePointInstanceType)
                                                 node_storage->instance_type;

  switch (type) {
    case GEO_NODE_POINT_INSTANCE_TYPE_OBJECT: {
      return get_instance_references__object(params);
    }
    case GEO_NODE_POINT_INSTANCE_TYPE_COLLECTION: {
      return get_instance_references__collection(params);
    }
    case GEO_NODE_POINT_INSTANCE_TYPE_GEOMETRY: {
      return get_instance_references__geometry(params);
    }
  }
  return {};
}

/**
 * Add the instance references to the component as a separate step from actually creating the
 * instances in order to avoid a map lookup for every transform. While this might add some
 * unnecessary references if they are not chosen while adding transforms, in the common cases
 * there are many more transforms than there are references, so that isn't likely.
 */
static Array<int> add_instance_references(InstancesComponent &instance_component,
                                          Span<InstanceReference> possible_references)
{
  Array<int> possible_handles(possible_references.size());
  for (const int i : possible_references.index_range()) {
    possible_handles[i] = instance_component.add_reference(possible_references[i]);
  }
  return possible_handles;
}

static void add_instances_from_component(InstancesComponent &instances,
                                         const GeometryComponent &src_geometry,
                                         Span<int> possible_handles,
                                         const GeoNodeExecParams &params)
{
  const AttributeDomain domain = ATTR_DOMAIN_POINT;

  const int domain_size = src_geometry.attribute_domain_size(domain);

  GVArray_Typed<float3> positions = src_geometry.attribute_get_for_read<float3>(
      "position", domain, {0, 0, 0});
  GVArray_Typed<float3> rotations = src_geometry.attribute_get_for_read<float3>(
      "rotation", domain, {0, 0, 0});
  GVArray_Typed<float3> scales = src_geometry.attribute_get_for_read<float3>(
      "scale", domain, {1, 1, 1});
  GVArray_Typed<int> id_attribute = src_geometry.attribute_get_for_read<int>("id", domain, -1);

  /* The initial size of the component might be non-zero if there are two component types. */
  const int start_len = instances.instances_amount();
  instances.resize(start_len + domain_size);
  MutableSpan<int> handles = instances.instance_reference_handles().slice(start_len, domain_size);
  MutableSpan<float4x4> transforms = instances.instance_transforms().slice(start_len, domain_size);
  MutableSpan<int> instance_ids = instances.instance_ids_ensure().slice(start_len, domain_size);

  /* Skip all of the randomness handling if there is only a single possible instance
   * (anything except for collection mode with "Whole Collection" turned off). */
  if (possible_handles.size() == 1) {
    const int handle = possible_handles.first();
    threading::parallel_for(IndexRange(domain_size), 1024, [&](IndexRange range) {
      for (const int i : range) {
        handles[i] = handle;
        transforms[i] = float4x4::from_loc_eul_scale(positions[i], rotations[i], scales[i]);
        instance_ids[i] = id_attribute[i];
      }
    });
  }
  else {
    const int seed = params.get_input<int>("Seed");
    Array<uint32_t> ids = get_geometry_element_ids_as_uints(src_geometry, ATTR_DOMAIN_POINT);
    threading::parallel_for(IndexRange(domain_size), 1024, [&](IndexRange range) {
      for (const int i : range) {
        const int index = BLI_hash_int_2d(ids[i], seed) % possible_handles.size();
        const int handle = possible_handles[index];
        handles[i] = handle;
        transforms[i] = float4x4::from_loc_eul_scale(positions[i], rotations[i], scales[i]);
        instance_ids[i] = id_attribute[i];
      }
    });
  }
}

static void geo_node_point_instance_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  GeometrySet geometry_set_out;

  /* TODO: This node should be able to instance on the input instances component
   * rather than making the entire input geometry set real. */
  geometry_set = geometry_set_realize_instances(geometry_set);

  const Vector<InstanceReference> possible_references = get_instance_references(params);
  if (possible_references.is_empty()) {
    params.set_output("Geometry", std::move(geometry_set_out));
    return;
  }

  InstancesComponent &instances = geometry_set_out.get_component_for_write<InstancesComponent>();
  Array<int> possible_handles = add_instance_references(instances, possible_references);

  if (geometry_set.has<MeshComponent>()) {
    add_instances_from_component(instances,
                                 *geometry_set.get_component_for_read<MeshComponent>(),
                                 possible_handles,
                                 params);
  }
  if (geometry_set.has<PointCloudComponent>()) {
    add_instances_from_component(instances,
                                 *geometry_set.get_component_for_read<PointCloudComponent>(),
                                 possible_handles,
                                 params);
  }
  if (geometry_set.has<CurveComponent>()) {
    add_instances_from_component(instances,
                                 *geometry_set.get_component_for_read<CurveComponent>(),
                                 possible_handles,
                                 params);
  }

  params.set_output("Geometry", std::move(geometry_set_out));
}

}  // namespace blender::nodes

void register_node_type_geo_point_instance()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_LEGACY_POINT_INSTANCE, "Point Instance", NODE_CLASS_GEOMETRY, 0);
  node_type_init(&ntype, blender::nodes::geo_node_point_instance_init);
  node_type_storage(
      &ntype, "NodeGeometryPointInstance", node_free_standard_storage, node_copy_standard_storage);
  ntype.declare = blender::nodes::geo_node_point_instance_declare;
  ntype.draw_buttons = blender::nodes::geo_node_point_instance_layout;
  node_type_update(&ntype, blender::nodes::geo_node_point_instance_update);
  ntype.geometry_node_execute = blender::nodes::geo_node_point_instance_exec;
  nodeRegisterType(&ntype);
}
