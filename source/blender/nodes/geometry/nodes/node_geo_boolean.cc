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

#include "DNA_mesh_types.h"

#include "BKE_mesh_boolean_convert.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes {

static void geo_node_boolean_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry 1");
  b.add_input<decl::Geometry>("Geometry 2").multi_input();
  b.add_input<decl::Bool>("Self Intersection");
  b.add_input<decl::Bool>("Hole Tolerant");
  b.add_output<decl::Geometry>("Geometry");
}

static void geo_node_boolean_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "operation", 0, "", ICON_NONE);
}

static void geo_node_boolean_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  GeometryNodeBooleanOperation operation = (GeometryNodeBooleanOperation)node->custom1;

  bNodeSocket *geometry_1_socket = (bNodeSocket *)node->inputs.first;
  bNodeSocket *geometry_2_socket = geometry_1_socket->next;

  switch (operation) {
    case GEO_NODE_BOOLEAN_INTERSECT:
    case GEO_NODE_BOOLEAN_UNION:
      nodeSetSocketAvailability(geometry_1_socket, false);
      nodeSetSocketAvailability(geometry_2_socket, true);
      node_sock_label(geometry_2_socket, N_("Geometry"));
      break;
    case GEO_NODE_BOOLEAN_DIFFERENCE:
      nodeSetSocketAvailability(geometry_1_socket, true);
      nodeSetSocketAvailability(geometry_2_socket, true);
      node_sock_label(geometry_2_socket, N_("Geometry 2"));
      break;
  }
}

static void geo_node_boolean_init(bNodeTree *UNUSED(tree), bNode *node)
{
  node->custom1 = GEO_NODE_BOOLEAN_DIFFERENCE;
}

static void geo_node_boolean_exec(GeoNodeExecParams params)
{
  GeometryNodeBooleanOperation operation = (GeometryNodeBooleanOperation)params.node().custom1;
  const bool use_self = params.get_input<bool>("Self Intersection");
  const bool hole_tolerant = params.get_input<bool>("Hole Tolerant");

#ifndef WITH_GMP
  params.error_message_add(NodeWarningType::Error,
                           TIP_("Disabled, Blender was compiled without GMP"));
#endif

  Vector<const Mesh *> meshes;
  Vector<const float4x4 *> transforms;

  GeometrySet set_a;
  if (operation == GEO_NODE_BOOLEAN_DIFFERENCE) {
    set_a = params.extract_input<GeometrySet>("Geometry 1");
    if (set_a.has_instances()) {
      params.error_message_add(
          NodeWarningType::Info,
          TIP_("Instances are not supported for the first geometry input, and will not be used"));
    }
    /* Note that it technically wouldn't be necessary to realize the instances for the first
     * geometry input, but the boolean code expects the first shape for the difference operation
     * to be a single mesh. */
    const Mesh *mesh_in_a = set_a.get_mesh_for_read();
    if (mesh_in_a != nullptr) {
      meshes.append(mesh_in_a);
      transforms.append(nullptr);
    }
  }

  /* The instance transform matrices are owned by the instance group, so we have to
   * keep all of them around for use during the boolean operation. */
  Vector<bke::GeometryInstanceGroup> set_groups;
  Vector<GeometrySet> geometry_sets = params.extract_multi_input<GeometrySet>("Geometry 2");
  for (const GeometrySet &geometry_set : geometry_sets) {
    bke::geometry_set_gather_instances(geometry_set, set_groups);
  }

  for (const bke::GeometryInstanceGroup &set_group : set_groups) {
    const Mesh *mesh_in = set_group.geometry_set.get_mesh_for_read();
    if (mesh_in != nullptr) {
      meshes.append_n_times(mesh_in, set_group.transforms.size());
      for (const int i : set_group.transforms.index_range()) {
        transforms.append(set_group.transforms.begin() + i);
      }
    }
  }

  Mesh *result = blender::meshintersect::direct_mesh_boolean(
      meshes, transforms, float4x4::identity(), {}, use_self, hole_tolerant, operation);

  params.set_output("Geometry", GeometrySet::create_with_mesh(result));
}

}  // namespace blender::nodes

void register_node_type_geo_boolean()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_MESH_BOOLEAN, "Mesh Boolean", NODE_CLASS_GEOMETRY, 0);
  ntype.declare = blender::nodes::geo_node_boolean_declare;
  ntype.draw_buttons = blender::nodes::geo_node_boolean_layout;
  ntype.updatefunc = blender::nodes::geo_node_boolean_update;
  node_type_init(&ntype, blender::nodes::geo_node_boolean_init);
  ntype.geometry_node_execute = blender::nodes::geo_node_boolean_exec;
  nodeRegisterType(&ntype);
}
