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

static void geo_node_input_material_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Material>(N_("Material"));
}

static void geo_node_input_material_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "material", 0, "", ICON_NONE);
}

static void geo_node_input_material_exec(GeoNodeExecParams params)
{
  Material *material = (Material *)params.node().id;
  params.set_output("Material", material);
}

}  // namespace blender::nodes

void register_node_type_geo_input_material()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_INPUT_MATERIAL, "Material", NODE_CLASS_INPUT, 0);
  ntype.draw_buttons = blender::nodes::geo_node_input_material_layout;
  ntype.declare = blender::nodes::geo_node_input_material_declare;
  ntype.geometry_node_execute = blender::nodes::geo_node_input_material_exec;
  nodeRegisterType(&ntype);
}
