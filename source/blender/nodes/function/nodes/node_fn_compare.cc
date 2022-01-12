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

#include <cmath>

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_enum_types.h"

#include "node_function_util.hh"

#include "NOD_socket_search_link.hh"

namespace blender::nodes::node_fn_compare_cc {

NODE_STORAGE_FUNCS(NodeFunctionCompare)

static void fn_node_compare_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Float>(N_("A")).min(-10000.0f).max(10000.0f);
  b.add_input<decl::Float>(N_("B")).min(-10000.0f).max(10000.0f);

  b.add_input<decl::Int>(N_("A"), "A_INT");
  b.add_input<decl::Int>(N_("B"), "B_INT");

  b.add_input<decl::Vector>(N_("A"), "A_VEC3");
  b.add_input<decl::Vector>(N_("B"), "B_VEC3");

  b.add_input<decl::Color>(N_("A"), "A_COL");
  b.add_input<decl::Color>(N_("B"), "B_COL");

  b.add_input<decl::String>(N_("A"), "A_STR");
  b.add_input<decl::String>(N_("B"), "B_STR");

  b.add_input<decl::Float>(N_("C")).default_value(0.9f);
  b.add_input<decl::Float>(N_("Angle")).default_value(0.0872665f).subtype(PROP_ANGLE);
  b.add_input<decl::Float>(N_("Epsilon")).default_value(0.001).min(-10000.0f).max(10000.0f);

  b.add_output<decl::Bool>(N_("Result"));
}

static void geo_node_compare_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  const NodeFunctionCompare &data = node_storage(*static_cast<const bNode *>(ptr->data));
  uiItemR(layout, ptr, "data_type", 0, "", ICON_NONE);
  if (data.data_type == SOCK_VECTOR) {
    uiItemR(layout, ptr, "mode", 0, "", ICON_NONE);
  }
  uiItemR(layout, ptr, "operation", 0, "", ICON_NONE);
}

static void node_compare_update(bNodeTree *ntree, bNode *node)
{
  NodeFunctionCompare *data = (NodeFunctionCompare *)node->storage;

  bNodeSocket *sock_comp = (bNodeSocket *)BLI_findlink(&node->inputs, 10);
  bNodeSocket *sock_angle = (bNodeSocket *)BLI_findlink(&node->inputs, 11);
  bNodeSocket *sock_epsilon = (bNodeSocket *)BLI_findlink(&node->inputs, 12);

  LISTBASE_FOREACH (bNodeSocket *, socket, &node->inputs) {
    nodeSetSocketAvailability(ntree, socket, socket->type == (eNodeSocketDatatype)data->data_type);
  }

  nodeSetSocketAvailability(ntree,
                            sock_epsilon,
                            ELEM(data->operation, NODE_COMPARE_EQUAL, NODE_COMPARE_NOT_EQUAL) &&
                                !ELEM(data->data_type, SOCK_INT, SOCK_STRING));

  nodeSetSocketAvailability(ntree,
                            sock_comp,
                            ELEM(data->mode, NODE_COMPARE_MODE_DOT_PRODUCT) &&
                                data->data_type == SOCK_VECTOR);

  nodeSetSocketAvailability(ntree,
                            sock_angle,
                            ELEM(data->mode, NODE_COMPARE_MODE_DIRECTION) &&
                                data->data_type == SOCK_VECTOR);
}

static void node_compare_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeFunctionCompare *data = MEM_cnew<NodeFunctionCompare>(__func__);
  data->operation = NODE_COMPARE_GREATER_THAN;
  data->data_type = SOCK_FLOAT;
  data->mode = NODE_COMPARE_MODE_ELEMENT;
  node->storage = data;
}

class SocketSearchOp {
 public:
  std::string socket_name;
  eNodeSocketDatatype data_type;
  NodeCompareOperation operation;
  NodeCompareMode mode = NODE_COMPARE_MODE_ELEMENT;
  void operator()(LinkSearchOpParams &params)
  {
    bNode &node = params.add_node("FunctionNodeCompare");
    node_storage(node).data_type = data_type;
    node_storage(node).operation = operation;
    node_storage(node).mode = mode;
    params.update_and_connect_available_socket(node, socket_name);
  }
};

static void node_compare_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const NodeDeclaration &declaration = *params.node_type().fixed_declaration;
  if (params.in_out() == SOCK_OUT) {
    search_link_ops_for_declarations(params, declaration.outputs());
    return;
  }

  const eNodeSocketDatatype type = static_cast<eNodeSocketDatatype>(params.other_socket().type);

  if (ELEM(type, SOCK_BOOLEAN, SOCK_FLOAT, SOCK_RGBA, SOCK_VECTOR, SOCK_INT, SOCK_STRING)) {
    const eNodeSocketDatatype mode_type = (type == SOCK_BOOLEAN) ? SOCK_FLOAT : type;
    const bool string_type = (type == SOCK_STRING);
    /* Add socket A compare operations. */
    for (const EnumPropertyItem *item = rna_enum_node_compare_operation_items;
         item->identifier != nullptr;
         item++) {
      if (item->name != nullptr && item->identifier[0] != '\0') {
        if (!string_type &&
            ELEM(item->value, NODE_COMPARE_COLOR_BRIGHTER, NODE_COMPARE_COLOR_DARKER)) {
          params.add_item(IFACE_(item->name),
                          SocketSearchOp{"A", SOCK_RGBA, (NodeCompareOperation)item->value});
        }
        else if ((!string_type) ||
                 (string_type && ELEM(item->value, NODE_COMPARE_EQUAL, NODE_COMPARE_NOT_EQUAL))) {
          params.add_item(IFACE_(item->name),
                          SocketSearchOp{"A", mode_type, (NodeCompareOperation)item->value});
        }
      }
    }
    /* Add Angle socket. */
    if (!string_type) {
      params.add_item(
          IFACE_("Angle"),
          SocketSearchOp{
              "Angle", SOCK_VECTOR, NODE_COMPARE_GREATER_THAN, NODE_COMPARE_MODE_DIRECTION});
    }
  }
}

static void node_compare_label(const bNodeTree *UNUSED(ntree),
                               const bNode *node,
                               char *label,
                               int maxlen)
{
  const NodeFunctionCompare *data = (NodeFunctionCompare *)node->storage;
  const char *name;
  bool enum_label = RNA_enum_name(rna_enum_node_compare_operation_items, data->operation, &name);
  if (!enum_label) {
    name = "Unknown";
  }
  BLI_strncpy(label, IFACE_(name), maxlen);
}

static float component_average(float3 a)
{
  return (a.x + a.y + a.z) / 3.0f;
}

static const fn::MultiFunction *get_multi_function(bNode &node)
{
  const NodeFunctionCompare *data = (NodeFunctionCompare *)node.storage;

  switch (data->data_type) {
    case SOCK_FLOAT:
      switch (data->operation) {
        case NODE_COMPARE_LESS_THAN: {
          static fn::CustomMF_SI_SI_SO<float, float, bool> fn{
              "Less Than", [](float a, float b) { return a < b; }};
          return &fn;
        }
        case NODE_COMPARE_LESS_EQUAL: {
          static fn::CustomMF_SI_SI_SO<float, float, bool> fn{
              "Less Equal", [](float a, float b) { return a <= b; }};
          return &fn;
        }
        case NODE_COMPARE_GREATER_THAN: {
          static fn::CustomMF_SI_SI_SO<float, float, bool> fn{
              "Greater Than", [](float a, float b) { return a > b; }};
          return &fn;
        }
        case NODE_COMPARE_GREATER_EQUAL: {
          static fn::CustomMF_SI_SI_SO<float, float, bool> fn{
              "Greater Equal", [](float a, float b) { return a >= b; }};
          return &fn;
        }
        case NODE_COMPARE_EQUAL: {
          static fn::CustomMF_SI_SI_SI_SO<float, float, float, bool> fn{
              "Equal", [](float a, float b, float epsilon) { return std::abs(a - b) <= epsilon; }};
          return &fn;
        }
        case NODE_COMPARE_NOT_EQUAL:
          static fn::CustomMF_SI_SI_SI_SO<float, float, float, bool> fn{
              "Not Equal",
              [](float a, float b, float epsilon) { return std::abs(a - b) > epsilon; }};
          return &fn;
      }
      break;
    case SOCK_INT:
      switch (data->operation) {
        case NODE_COMPARE_LESS_THAN: {
          static fn::CustomMF_SI_SI_SO<int, int, bool> fn{"Less Than",
                                                          [](int a, int b) { return a < b; }};
          return &fn;
        }
        case NODE_COMPARE_LESS_EQUAL: {
          static fn::CustomMF_SI_SI_SO<int, int, bool> fn{"Less Equal",
                                                          [](int a, int b) { return a <= b; }};
          return &fn;
        }
        case NODE_COMPARE_GREATER_THAN: {
          static fn::CustomMF_SI_SI_SO<int, int, bool> fn{"Greater Than",
                                                          [](int a, int b) { return a > b; }};
          return &fn;
        }
        case NODE_COMPARE_GREATER_EQUAL: {
          static fn::CustomMF_SI_SI_SO<int, int, bool> fn{"Greater Equal",
                                                          [](int a, int b) { return a >= b; }};
          return &fn;
        }
        case NODE_COMPARE_EQUAL: {
          static fn::CustomMF_SI_SI_SO<int, int, bool> fn{"Equal",
                                                          [](int a, int b) { return a == b; }};
          return &fn;
        }
        case NODE_COMPARE_NOT_EQUAL: {
          static fn::CustomMF_SI_SI_SO<int, int, bool> fn{"Not Equal",
                                                          [](int a, int b) { return a != b; }};
          return &fn;
        }
      }
      break;
    case SOCK_VECTOR:
      switch (data->operation) {
        case NODE_COMPARE_LESS_THAN:
          switch (data->mode) {
            case NODE_COMPARE_MODE_AVERAGE: {
              static fn::CustomMF_SI_SI_SO<float3, float3, bool> fn{
                  "Less Than - Average",
                  [](float3 a, float3 b) { return component_average(a) < component_average(b); }};
              return &fn;
            }
            case NODE_COMPARE_MODE_DOT_PRODUCT: {
              static fn::CustomMF_SI_SI_SI_SO<float3, float3, float, bool> fn{
                  "Less Than - Dot Product",
                  [](float3 a, float3 b, float comp) { return float3::dot(a, b) < comp; }};
              return &fn;
            }
            case NODE_COMPARE_MODE_DIRECTION: {
              static fn::CustomMF_SI_SI_SI_SO<float3, float3, float, bool> fn{
                  "Less Than - Direction",
                  [](float3 a, float3 b, float angle) { return angle_v3v3(a, b) < angle; }};
              return &fn;
            }
            case NODE_COMPARE_MODE_ELEMENT: {
              static fn::CustomMF_SI_SI_SO<float3, float3, bool> fn{
                  "Less Than - Element-wise",
                  [](float3 a, float3 b) { return a.x < b.x && a.y < b.y && a.z < b.z; }};
              return &fn;
            }
            case NODE_COMPARE_MODE_LENGTH: {
              static fn::CustomMF_SI_SI_SO<float3, float3, bool> fn{
                  "Less Than - Length",
                  [](float3 a, float3 b) { return a.length() < b.length(); }};
              return &fn;
            }
          }
          break;
        case NODE_COMPARE_LESS_EQUAL:
          switch (data->mode) {
            case NODE_COMPARE_MODE_AVERAGE: {
              static fn::CustomMF_SI_SI_SO<float3, float3, bool> fn{
                  "Less Equal - Average",
                  [](float3 a, float3 b) { return component_average(a) <= component_average(b); }};
              return &fn;
            }
            case NODE_COMPARE_MODE_DOT_PRODUCT: {
              static fn::CustomMF_SI_SI_SI_SO<float3, float3, float, bool> fn{
                  "Less Equal - Dot Product",
                  [](float3 a, float3 b, float comp) { return float3::dot(a, b) <= comp; }};
              return &fn;
            }
            case NODE_COMPARE_MODE_DIRECTION: {
              static fn::CustomMF_SI_SI_SI_SO<float3, float3, float, bool> fn{
                  "Less Equal - Direction",
                  [](float3 a, float3 b, float angle) { return angle_v3v3(a, b) <= angle; }};
              return &fn;
            }
            case NODE_COMPARE_MODE_ELEMENT: {
              static fn::CustomMF_SI_SI_SO<float3, float3, bool> fn{
                  "Less Equal - Element-wise",
                  [](float3 a, float3 b) { return a.x <= b.x && a.y <= b.y && a.z <= b.z; }};
              return &fn;
            }
            case NODE_COMPARE_MODE_LENGTH: {
              static fn::CustomMF_SI_SI_SO<float3, float3, bool> fn{
                  "Less Equal - Length",
                  [](float3 a, float3 b) { return a.length() <= b.length(); }};
              return &fn;
            }
          }
          break;
        case NODE_COMPARE_GREATER_THAN:
          switch (data->mode) {
            case NODE_COMPARE_MODE_AVERAGE: {
              static fn::CustomMF_SI_SI_SO<float3, float3, bool> fn{
                  "Greater Than - Average",
                  [](float3 a, float3 b) { return component_average(a) > component_average(b); }};
              return &fn;
            }
            case NODE_COMPARE_MODE_DOT_PRODUCT: {
              static fn::CustomMF_SI_SI_SI_SO<float3, float3, float, bool> fn{
                  "Greater Than - Dot Product",
                  [](float3 a, float3 b, float comp) { return float3::dot(a, b) > comp; }};
              return &fn;
            }
            case NODE_COMPARE_MODE_DIRECTION: {
              static fn::CustomMF_SI_SI_SI_SO<float3, float3, float, bool> fn{
                  "Greater Than - Direction",
                  [](float3 a, float3 b, float angle) { return angle_v3v3(a, b) > angle; }};
              return &fn;
            }
            case NODE_COMPARE_MODE_ELEMENT: {
              static fn::CustomMF_SI_SI_SO<float3, float3, bool> fn{
                  "Greater Than - Element-wise",
                  [](float3 a, float3 b) { return a.x > b.x && a.y > b.y && a.z > b.z; }};
              return &fn;
            }
            case NODE_COMPARE_MODE_LENGTH: {
              static fn::CustomMF_SI_SI_SO<float3, float3, bool> fn{
                  "Greater Than - Length",
                  [](float3 a, float3 b) { return a.length() > b.length(); }};
              return &fn;
            }
          }
          break;
        case NODE_COMPARE_GREATER_EQUAL:
          switch (data->mode) {
            case NODE_COMPARE_MODE_AVERAGE: {
              static fn::CustomMF_SI_SI_SO<float3, float3, bool> fn{
                  "Greater Equal - Average",
                  [](float3 a, float3 b) { return component_average(a) >= component_average(b); }};
              return &fn;
            }
            case NODE_COMPARE_MODE_DOT_PRODUCT: {
              static fn::CustomMF_SI_SI_SI_SO<float3, float3, float, bool> fn{
                  "Greater Equal - Dot Product",
                  [](float3 a, float3 b, float comp) { return float3::dot(a, b) >= comp; }};
              return &fn;
            }
            case NODE_COMPARE_MODE_DIRECTION: {
              static fn::CustomMF_SI_SI_SI_SO<float3, float3, float, bool> fn{
                  "Greater Equal - Direction",
                  [](float3 a, float3 b, float angle) { return angle_v3v3(a, b) >= angle; }};
              return &fn;
            }
            case NODE_COMPARE_MODE_ELEMENT: {
              static fn::CustomMF_SI_SI_SO<float3, float3, bool> fn{
                  "Greater Equal - Element-wise",
                  [](float3 a, float3 b) { return a.x >= b.x && a.y >= b.y && a.z >= b.z; }};
              return &fn;
            }
            case NODE_COMPARE_MODE_LENGTH: {
              static fn::CustomMF_SI_SI_SO<float3, float3, bool> fn{
                  "Greater Equal - Length",
                  [](float3 a, float3 b) { return a.length() >= b.length(); }};
              return &fn;
            }
          }
          break;
        case NODE_COMPARE_EQUAL:
          switch (data->mode) {
            case NODE_COMPARE_MODE_AVERAGE: {
              static fn::CustomMF_SI_SI_SI_SO<float3, float3, float, bool> fn{
                  "Equal - Average", [](float3 a, float3 b, float epsilon) {
                    return abs(component_average(a) - component_average(b)) <= epsilon;
                  }};
              return &fn;
            }
            case NODE_COMPARE_MODE_DOT_PRODUCT: {
              static fn::CustomMF_SI_SI_SI_SI_SO<float3, float3, float, float, bool> fn{
                  "Equal - Dot Product", [](float3 a, float3 b, float comp, float epsilon) {
                    return abs(float3::dot(a, b) - comp) <= epsilon;
                  }};
              return &fn;
            }
            case NODE_COMPARE_MODE_DIRECTION: {
              static fn::CustomMF_SI_SI_SI_SI_SO<float3, float3, float, float, bool> fn{
                  "Equal - Direction", [](float3 a, float3 b, float angle, float epsilon) {
                    return abs(angle_v3v3(a, b) - angle) <= epsilon;
                  }};
              return &fn;
            }
            case NODE_COMPARE_MODE_ELEMENT: {
              static fn::CustomMF_SI_SI_SI_SO<float3, float3, float, bool> fn{
                  "Equal - Element-wise", [](float3 a, float3 b, float epsilon) {
                    return abs(a.x - b.x) <= epsilon && abs(a.y - b.y) <= epsilon &&
                           abs(a.z - b.z) <= epsilon;
                  }};
              return &fn;
            }
            case NODE_COMPARE_MODE_LENGTH: {
              static fn::CustomMF_SI_SI_SI_SO<float3, float3, float, bool> fn{
                  "Equal - Length", [](float3 a, float3 b, float epsilon) {
                    return abs(a.length() - b.length()) <= epsilon;
                  }};
              return &fn;
            }
          }
          break;
        case NODE_COMPARE_NOT_EQUAL:
          switch (data->mode) {
            case NODE_COMPARE_MODE_AVERAGE: {
              static fn::CustomMF_SI_SI_SI_SO<float3, float3, float, bool> fn{
                  "Not Equal - Average", [](float3 a, float3 b, float epsilon) {
                    return abs(component_average(a) - component_average(b)) > epsilon;
                  }};
              return &fn;
            }
            case NODE_COMPARE_MODE_DOT_PRODUCT: {
              static fn::CustomMF_SI_SI_SI_SI_SO<float3, float3, float, float, bool> fn{
                  "Not Equal - Dot Product", [](float3 a, float3 b, float comp, float epsilon) {
                    return abs(float3::dot(a, b) - comp) >= epsilon;
                  }};
              return &fn;
            }
            case NODE_COMPARE_MODE_DIRECTION: {
              static fn::CustomMF_SI_SI_SI_SI_SO<float3, float3, float, float, bool> fn{
                  "Not Equal - Direction", [](float3 a, float3 b, float angle, float epsilon) {
                    return abs(angle_v3v3(a, b) - angle) > epsilon;
                  }};
              return &fn;
            }
            case NODE_COMPARE_MODE_ELEMENT: {
              static fn::CustomMF_SI_SI_SI_SO<float3, float3, float, bool> fn{
                  "Not Equal - Element-wise", [](float3 a, float3 b, float epsilon) {
                    return abs(a.x - b.x) > epsilon && abs(a.y - b.y) > epsilon &&
                           abs(a.z - b.z) > epsilon;
                  }};
              return &fn;
            }
            case NODE_COMPARE_MODE_LENGTH: {
              static fn::CustomMF_SI_SI_SI_SO<float3, float3, float, bool> fn{
                  "Not Equal - Length", [](float3 a, float3 b, float epsilon) {
                    return abs(a.length() - b.length()) > epsilon;
                  }};
              return &fn;
            }
          }
          break;
      }
      break;
    case SOCK_RGBA:
      switch (data->operation) {
        case NODE_COMPARE_EQUAL: {
          static fn::CustomMF_SI_SI_SI_SO<ColorGeometry4f, ColorGeometry4f, float, bool> fn{
              "Equal", [](ColorGeometry4f a, ColorGeometry4f b, float epsilon) {
                return abs(a.r - b.r) <= epsilon && abs(a.g - b.g) <= epsilon &&
                       abs(a.b - b.b) <= epsilon;
              }};
          return &fn;
        }
        case NODE_COMPARE_NOT_EQUAL: {
          static fn::CustomMF_SI_SI_SI_SO<ColorGeometry4f, ColorGeometry4f, float, bool> fn{
              "Not Equal", [](ColorGeometry4f a, ColorGeometry4f b, float epsilon) {
                return abs(a.r - b.r) > epsilon && abs(a.g - b.g) > epsilon &&
                       abs(a.b - b.b) > epsilon;
              }};
          return &fn;
        }
        case NODE_COMPARE_COLOR_BRIGHTER: {
          static fn::CustomMF_SI_SI_SO<ColorGeometry4f, ColorGeometry4f, bool> fn{
              "Brighter", [](ColorGeometry4f a, ColorGeometry4f b) {
                return rgb_to_grayscale(a) > rgb_to_grayscale(b);
              }};
          return &fn;
        }
        case NODE_COMPARE_COLOR_DARKER: {
          static fn::CustomMF_SI_SI_SO<ColorGeometry4f, ColorGeometry4f, bool> fn{
              "Darker", [](ColorGeometry4f a, ColorGeometry4f b) {
                return rgb_to_grayscale(a) < rgb_to_grayscale(b);
              }};
          return &fn;
        }
      }
      break;
    case SOCK_STRING:
      switch (data->operation) {
        case NODE_COMPARE_EQUAL: {
          static fn::CustomMF_SI_SI_SO<std::string, std::string, bool> fn{
              "Equal", [](std::string a, std::string b) { return a == b; }};
          return &fn;
        }
        case NODE_COMPARE_NOT_EQUAL: {
          static fn::CustomMF_SI_SI_SO<std::string, std::string, bool> fn{
              "Not Equal", [](std::string a, std::string b) { return a != b; }};
          return &fn;
        }
      }
      break;
  }
  return nullptr;
}

static void fn_node_compare_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const fn::MultiFunction *fn = get_multi_function(builder.node());
  builder.set_matching_fn(fn);
}

}  // namespace blender::nodes::node_fn_compare_cc

void register_node_type_fn_compare()
{
  namespace file_ns = blender::nodes::node_fn_compare_cc;

  static bNodeType ntype;
  fn_node_type_base(&ntype, FN_NODE_COMPARE, "Compare", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::fn_node_compare_declare;
  ntype.labelfunc = file_ns::node_compare_label;
  node_type_update(&ntype, file_ns::node_compare_update);
  node_type_init(&ntype, file_ns::node_compare_init);
  node_type_storage(
      &ntype, "NodeFunctionCompare", node_free_standard_storage, node_copy_standard_storage);
  ntype.build_multi_function = file_ns::fn_node_compare_build_multi_function;
  ntype.draw_buttons = file_ns::geo_node_compare_layout;
  ntype.gather_link_search_ops = file_ns::node_compare_gather_link_searches;
  nodeRegisterType(&ntype);
}
