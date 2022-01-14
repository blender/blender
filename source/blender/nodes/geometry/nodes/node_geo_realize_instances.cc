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

#include "node_geometry_util.hh"

#include "GEO_realize_instances.hh"

#include "UI_interface.h"
#include "UI_resources.h"

namespace blender::nodes::node_geo_realize_instances_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"));
  b.add_output<decl::Geometry>(N_("Geometry"));
}

static void node_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "legacy_behavior", 0, nullptr, ICON_NONE);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const bool legacy_behavior = params.node().custom1 & GEO_NODE_REALIZE_INSTANCES_LEGACY_BEHAVIOR;

  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  geometry::RealizeInstancesOptions options;
  options.keep_original_ids = legacy_behavior;
  options.realize_instance_attributes = !legacy_behavior;
  geometry_set = geometry::realize_instances(geometry_set, options);
  params.set_output("Geometry", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_realize_instances_cc

void register_node_type_geo_realize_instances()
{
  namespace file_ns = blender::nodes::node_geo_realize_instances_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_REALIZE_INSTANCES, "Realize Instances", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons_ex = file_ns::node_layout;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
