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

#include "node_geometry_util.hh"

static bNodeSocketTemplate geo_node_collection_info_in[] = {
    {SOCK_COLLECTION, N_("Collection")},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_collection_info_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

static void geo_node_collection_info_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "transform_space", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
}

namespace blender::nodes {

static void geo_node_collection_info_node_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryCollectionInfo *data = (NodeGeometryCollectionInfo *)MEM_callocN(
      sizeof(NodeGeometryCollectionInfo), __func__);
  data->transform_space = GEO_NODE_TRANSFORM_SPACE_ORIGINAL;
  node->storage = data;
}

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
  const bool transform_space_relative = (node_storage->transform_space ==
                                         GEO_NODE_TRANSFORM_SPACE_RELATIVE);

  InstancesComponent &instances = geometry_set_out.get_component_for_write<InstancesComponent>();

  float transform_mat[4][4];
  unit_m4(transform_mat);
  const Object *self_object = params.self_object();

  if (transform_space_relative) {
    copy_v3_v3(transform_mat[3], collection->instance_offset);

    mul_m4_m4_pre(transform_mat, self_object->imat);
  }

  const int handle = instances.add_reference(*collection);
  instances.add_instance(handle, transform_mat, -1);

  params.set_output("Geometry", geometry_set_out);
}

}  // namespace blender::nodes

void register_node_type_geo_collection_info()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_COLLECTION_INFO, "Collection Info", NODE_CLASS_INPUT, 0);
  node_type_socket_templates(&ntype, geo_node_collection_info_in, geo_node_collection_info_out);
  node_type_init(&ntype, blender::nodes::geo_node_collection_info_node_init);
  node_type_storage(&ntype,
                    "NodeGeometryCollectionInfo",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  ntype.geometry_node_execute = blender::nodes::geo_node_collection_info_exec;
  ntype.draw_buttons = geo_node_collection_info_layout;
  nodeRegisterType(&ntype);
}
