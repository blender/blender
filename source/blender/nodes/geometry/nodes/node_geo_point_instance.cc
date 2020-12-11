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

#include "BKE_mesh.h"
#include "BKE_persistent_data_handle.hh"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_pointcloud_types.h"

#include "node_geometry_util.hh"

static bNodeSocketTemplate geo_node_point_instance_in[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {SOCK_OBJECT, N_("Object")},
    {SOCK_COLLECTION, N_("Collection")},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_point_instance_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

namespace blender::nodes {

static void geo_node_point_instance_update(bNodeTree *UNUSED(tree), bNode *node)
{
  bNodeSocket *object_socket = (bNodeSocket *)BLI_findlink(&node->inputs, 1);
  bNodeSocket *collection_socket = object_socket->next;

  GeometryNodePointInstanceType type = (GeometryNodePointInstanceType)node->custom1;

  nodeSetSocketAvailability(object_socket, type == GEO_NODE_POINT_INSTANCE_TYPE_OBJECT);
  nodeSetSocketAvailability(collection_socket, type == GEO_NODE_POINT_INSTANCE_TYPE_COLLECTION);
}

static void add_instances_from_geometry_component(InstancesComponent &instances,
                                                  const GeometryComponent &src_geometry,
                                                  Object *object,
                                                  Collection *collection)
{
  Float3ReadAttribute positions = src_geometry.attribute_get_for_read<float3>(
      "position", ATTR_DOMAIN_POINT, {0, 0, 0});
  Float3ReadAttribute rotations = src_geometry.attribute_get_for_read<float3>(
      "rotation", ATTR_DOMAIN_POINT, {0, 0, 0});
  Float3ReadAttribute scales = src_geometry.attribute_get_for_read<float3>(
      "scale", ATTR_DOMAIN_POINT, {1, 1, 1});

  for (const int i : IndexRange(positions.size())) {
    if (object != nullptr) {
      instances.add_instance(object, positions[i], rotations[i], scales[i]);
    }
    if (collection != nullptr) {
      instances.add_instance(collection, positions[i], rotations[i], scales[i]);
    }
  }
}

static void geo_node_point_instance_exec(GeoNodeExecParams params)
{
  GeometryNodePointInstanceType type = (GeometryNodePointInstanceType)params.node().custom1;
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  GeometrySet geometry_set_out;

  Object *object = nullptr;
  Collection *collection = nullptr;

  if (type == GEO_NODE_POINT_INSTANCE_TYPE_OBJECT) {
    bke::PersistentObjectHandle object_handle = params.extract_input<bke::PersistentObjectHandle>(
        "Object");
    object = params.handle_map().lookup(object_handle);
    /* Avoid accidental recursion of instances. */
    if (object == params.self_object()) {
      object = nullptr;
    }
  }
  else if (type == GEO_NODE_POINT_INSTANCE_TYPE_COLLECTION) {
    bke::PersistentCollectionHandle collection_handle =
        params.extract_input<bke::PersistentCollectionHandle>("Collection");
    collection = params.handle_map().lookup(collection_handle);
  }

  InstancesComponent &instances = geometry_set_out.get_component_for_write<InstancesComponent>();
  if (geometry_set.has<MeshComponent>()) {
    add_instances_from_geometry_component(
        instances, *geometry_set.get_component_for_read<MeshComponent>(), object, collection);
  }
  if (geometry_set.has<PointCloudComponent>()) {
    add_instances_from_geometry_component(
        instances,
        *geometry_set.get_component_for_read<PointCloudComponent>(),
        object,
        collection);
  }

  params.set_output("Geometry", std::move(geometry_set_out));
}
}  // namespace blender::nodes

void register_node_type_geo_point_instance()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_POINT_INSTANCE, "Point Instance", NODE_CLASS_GEOMETRY, 0);
  node_type_socket_templates(&ntype, geo_node_point_instance_in, geo_node_point_instance_out);
  node_type_update(&ntype, blender::nodes::geo_node_point_instance_update);
  ntype.geometry_node_execute = blender::nodes::geo_node_point_instance_exec;
  nodeRegisterType(&ntype);
}
