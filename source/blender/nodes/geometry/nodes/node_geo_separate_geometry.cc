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

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes {

static void geo_node_separate_geometry_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry");
  b.add_input<decl::Bool>("Selection")
      .default_value(true)
      .hide_value()
      .supports_field()
      .description("The parts of the geometry that go into the first output");
  b.add_output<decl::Geometry>("Selection")
      .description("The parts of the geometry in the selection");
  b.add_output<decl::Geometry>("Inverted")
      .description("The parts of the geometry not in the selection");
}

static void geo_node_separate_geometry_layout(uiLayout *layout,
                                              bContext *UNUSED(C),
                                              PointerRNA *ptr)
{
  uiItemR(layout, ptr, "domain", 0, "", ICON_NONE);
}

static void geo_node_separate_geometry_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometrySeparateGeometry *data = (NodeGeometrySeparateGeometry *)MEM_callocN(
      sizeof(NodeGeometrySeparateGeometry), __func__);
  data->domain = ATTR_DOMAIN_POINT;

  node->storage = data;
}

static void geo_node_separate_geometry_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  const Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");

  const bNode &node = params.node();
  const NodeGeometryDeleteGeometry &storage = *(const NodeGeometryDeleteGeometry *)node.storage;
  const AttributeDomain domain = static_cast<AttributeDomain>(storage.domain);

  bool all_is_error = false;
  GeometrySet second_set(geometry_set);
  if (params.output_is_required("Selection")) {
    geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
      bool this_is_error = false;
      separate_geometry(geometry_set,
                        domain,
                        GEO_NODE_DELETE_GEOMETRY_MODE_ALL,
                        selection_field,
                        false,
                        this_is_error);
      all_is_error &= this_is_error;
    });
    params.set_output("Selection", std::move(geometry_set));
  }
  if (params.output_is_required("Inverted")) {
    second_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
      bool this_is_error = false;
      separate_geometry(geometry_set,
                        domain,
                        GEO_NODE_DELETE_GEOMETRY_MODE_ALL,
                        selection_field,
                        true,
                        this_is_error);
      all_is_error &= this_is_error;
    });
    params.set_output("Inverted", std::move(second_set));
  }
  if (all_is_error) {
    /* Only show this if none of the instances/components actually changed. */
    params.error_message_add(NodeWarningType::Info, TIP_("No geometry with given domain"));
  }
}

}  // namespace blender::nodes

void register_node_type_geo_separate_geometry()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_SEPARATE_GEOMETRY, "Separate Geometry", NODE_CLASS_GEOMETRY, 0);

  node_type_storage(&ntype,
                    "NodeGeometrySeparateGeometry",
                    node_free_standard_storage,
                    node_copy_standard_storage);

  node_type_init(&ntype, blender::nodes::geo_node_separate_geometry_init);

  ntype.declare = blender::nodes::geo_node_separate_geometry_declare;
  ntype.geometry_node_execute = blender::nodes::geo_node_separate_geometry_exec;
  ntype.draw_buttons = blender::nodes::geo_node_separate_geometry_layout;
  nodeRegisterType(&ntype);
}
