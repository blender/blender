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

#include "UI_interface.h"
#include "UI_resources.h"

namespace blender::nodes {

static void geo_node_switch_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Bool>("Switch");

  b.add_input<decl::Float>("False");
  b.add_input<decl::Float>("True");
  b.add_input<decl::Int>("False", "False_001").min(-100000).max(100000);
  b.add_input<decl::Int>("True", "True_001").min(-100000).max(100000);
  b.add_input<decl::Bool>("False", "False_002");
  b.add_input<decl::Bool>("True", "True_002");
  b.add_input<decl::Vector>("False", "False_003");
  b.add_input<decl::Vector>("True", "True_003");
  b.add_input<decl::Color>("False", "False_004").default_value({0.8f, 0.8f, 0.8f, 1.0f});
  b.add_input<decl::Color>("True", "True_004").default_value({0.8f, 0.8f, 0.8f, 1.0f});
  b.add_input<decl::String>("False", "False_005");
  b.add_input<decl::String>("True", "True_005");
  b.add_input<decl::Geometry>("False", "False_006");
  b.add_input<decl::Geometry>("True", "True_006");
  b.add_input<decl::Object>("False", "False_007");
  b.add_input<decl::Object>("True", "True_007");
  b.add_input<decl::Collection>("False", "False_008");
  b.add_input<decl::Collection>("True", "True_008");
  b.add_input<decl::Texture>("False", "False_009");
  b.add_input<decl::Texture>("True", "True_009");
  b.add_input<decl::Material>("False", "False_010");
  b.add_input<decl::Material>("True", "True_010");

  b.add_output<decl::Float>("Output");
  b.add_output<decl::Int>("Output", "Output_001");
  b.add_output<decl::Bool>("Output", "Output_002");
  b.add_output<decl::Vector>("Output", "Output_003");
  b.add_output<decl::Color>("Output", "Output_004");
  b.add_output<decl::String>("Output", "Output_005");
  b.add_output<decl::Geometry>("Output", "Output_006");
  b.add_output<decl::Object>("Output", "Output_007");
  b.add_output<decl::Collection>("Output", "Output_008");
  b.add_output<decl::Texture>("Output", "Output_009");
  b.add_output<decl::Material>("Output", "Output_010");
}

static void geo_node_switch_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "input_type", 0, "", ICON_NONE);
}

static void geo_node_switch_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeSwitch *data = (NodeSwitch *)MEM_callocN(sizeof(NodeSwitch), __func__);
  data->input_type = SOCK_GEOMETRY;
  node->storage = data;
}

static void geo_node_switch_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeSwitch *node_storage = (NodeSwitch *)node->storage;
  int index = 0;
  LISTBASE_FOREACH (bNodeSocket *, socket, &node->inputs) {
    nodeSetSocketAvailability(
        socket, index == 0 || socket->type == (eNodeSocketDatatype)node_storage->input_type);
    index++;
  }
  LISTBASE_FOREACH (bNodeSocket *, socket, &node->outputs) {
    nodeSetSocketAvailability(socket,
                              socket->type == (eNodeSocketDatatype)node_storage->input_type);
  }
}

template<typename T>
static void output_input(GeoNodeExecParams &params,
                         const bool input,
                         const StringRef input_suffix,
                         const StringRef output_identifier)
{
  const std::string name_a = "False" + input_suffix;
  const std::string name_b = "True" + input_suffix;
  if (input) {
    params.set_input_unused(name_a);
    if (params.lazy_require_input(name_b)) {
      return;
    }
    params.set_output(output_identifier, params.extract_input<T>(name_b));
  }
  else {
    params.set_input_unused(name_b);
    if (params.lazy_require_input(name_a)) {
      return;
    }
    params.set_output(output_identifier, params.extract_input<T>(name_a));
  }
}

static void geo_node_switch_exec(GeoNodeExecParams params)
{
  if (params.lazy_require_input("Switch")) {
    return;
  }
  const NodeSwitch &storage = *(const NodeSwitch *)params.node().storage;
  const bool input = params.get_input<bool>("Switch");
  switch ((eNodeSocketDatatype)storage.input_type) {
    case SOCK_FLOAT: {
      output_input<float>(params, input, "", "Output");
      break;
    }
    case SOCK_INT: {
      output_input<int>(params, input, "_001", "Output_001");
      break;
    }
    case SOCK_BOOLEAN: {
      output_input<bool>(params, input, "_002", "Output_002");
      break;
    }
    case SOCK_VECTOR: {
      output_input<float3>(params, input, "_003", "Output_003");
      break;
    }
    case SOCK_RGBA: {
      output_input<ColorGeometry4f>(params, input, "_004", "Output_004");
      break;
    }
    case SOCK_STRING: {
      output_input<std::string>(params, input, "_005", "Output_005");
      break;
    }
    case SOCK_GEOMETRY: {
      output_input<GeometrySet>(params, input, "_006", "Output_006");
      break;
    }
    case SOCK_OBJECT: {
      output_input<Object *>(params, input, "_007", "Output_007");
      break;
    }
    case SOCK_COLLECTION: {
      output_input<Collection *>(params, input, "_008", "Output_008");
      break;
    }
    case SOCK_TEXTURE: {
      output_input<Tex *>(params, input, "_009", "Output_009");
      break;
    }
    case SOCK_MATERIAL: {
      output_input<Material *>(params, input, "_010", "Output_010");
      break;
    }
    default:
      BLI_assert_unreachable();
      break;
  }
}

}  // namespace blender::nodes

void register_node_type_geo_switch()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_SWITCH, "Switch", NODE_CLASS_CONVERTER, 0);
  ntype.declare = blender::nodes::geo_node_switch_declare;
  node_type_init(&ntype, blender::nodes::geo_node_switch_init);
  node_type_update(&ntype, blender::nodes::geo_node_switch_update);
  node_type_storage(&ntype, "NodeSwitch", node_free_standard_storage, node_copy_standard_storage);
  ntype.geometry_node_execute = blender::nodes::geo_node_switch_exec;
  ntype.geometry_node_execute_supports_laziness = true;
  ntype.draw_buttons = blender::nodes::geo_node_switch_layout;
  nodeRegisterType(&ntype);
}
