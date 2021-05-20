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

static bNodeSocketTemplate geo_node_switch_in[] = {
    {SOCK_BOOLEAN, N_("Switch")},

    {SOCK_FLOAT, N_("A"), 0.0, 0.0, 0.0, 0.0, -FLT_MAX, FLT_MAX},
    {SOCK_FLOAT, N_("B"), 0.0, 0.0, 0.0, 0.0, -FLT_MAX, FLT_MAX},
    {SOCK_INT, N_("A"), 0, 0, 0, 0, -100000, 100000},
    {SOCK_INT, N_("B"), 0, 0, 0, 0, -100000, 100000},
    {SOCK_BOOLEAN, N_("A")},
    {SOCK_BOOLEAN, N_("B")},
    {SOCK_VECTOR, N_("A"), 0.0, 0.0, 0.0, 0.0, -FLT_MAX, FLT_MAX},
    {SOCK_VECTOR, N_("B"), 0.0, 0.0, 0.0, 0.0, -FLT_MAX, FLT_MAX},
    {SOCK_RGBA, N_("A"), 0.8, 0.8, 0.8, 1.0},
    {SOCK_RGBA, N_("B"), 0.8, 0.8, 0.8, 1.0},
    {SOCK_STRING, N_("A")},
    {SOCK_STRING, N_("B")},
    {SOCK_GEOMETRY, N_("A")},
    {SOCK_GEOMETRY, N_("B")},
    {SOCK_OBJECT, N_("A")},
    {SOCK_OBJECT, N_("B")},
    {SOCK_COLLECTION, N_("A")},
    {SOCK_COLLECTION, N_("B")},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_switch_out[] = {
    {SOCK_FLOAT, N_("Output")},
    {SOCK_INT, N_("Output")},
    {SOCK_BOOLEAN, N_("Output")},
    {SOCK_VECTOR, N_("Output")},
    {SOCK_RGBA, N_("Output")},
    {SOCK_STRING, N_("Output")},
    {SOCK_GEOMETRY, N_("Output")},
    {SOCK_OBJECT, N_("Output")},
    {SOCK_COLLECTION, N_("Output")},
    {-1, ""},
};

static void geo_node_switch_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "input_type", 0, "", ICON_NONE);
}

static void geo_node_switch_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeSwitch *data = (NodeSwitch *)MEM_callocN(sizeof(NodeSwitch), __func__);
  data->input_type = SOCK_FLOAT;
  node->storage = data;
}

namespace blender::nodes {

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
  const std::string name_a = "A" + input_suffix;
  const std::string name_b = "B" + input_suffix;
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
      output_input<Color4f>(params, input, "_004", "Output_004");
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
    default:
      BLI_assert_unreachable();
      break;
  }
}

}  // namespace blender::nodes

void register_node_type_geo_switch()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_SWITCH, "Switch", NODE_CLASS_GEOMETRY, 0);
  node_type_socket_templates(&ntype, geo_node_switch_in, geo_node_switch_out);
  node_type_init(&ntype, geo_node_switch_init);
  node_type_update(&ntype, blender::nodes::geo_node_switch_update);
  node_type_storage(&ntype, "NodeSwitch", node_free_standard_storage, node_copy_standard_storage);
  ntype.geometry_node_execute = blender::nodes::geo_node_switch_exec;
  ntype.geometry_node_execute_supports_lazyness = true;
  ntype.draw_buttons = geo_node_switch_layout;
  nodeRegisterType(&ntype);
}
