/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "BLI_string_utf8.hh"

#include "NOD_rna_define.hh"
#include "RNA_access.hh"
#include "RNA_enum_types.hh"

#include "COM_node_operation.hh"

namespace blender {

namespace nodes::node_geo_warning_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  b.add_default_layout();

  b.add_input<decl::Bool>("Show"_ustr).default_value(true).hide_value();
  b.add_output<decl::Bool>("Show"_ustr).align_with_previous();
  b.add_input<decl::String>("Message"_ustr).optional_label();
}

class LazyFunctionForWarningNode : public LazyFunction {
  const bNode &node_;

 public:
  LazyFunctionForWarningNode(const bNode &node) : node_(node)
  {
    debug_name_ = "Warning";
    const CPPType &type = CPPType::get<SocketValueVariant>();
    inputs_.append_as("Show", type, lf::ValueUsage::Used);
    inputs_.append_as("Message", type);
    outputs_.append_as("Show", type);
  }

  void execute_impl(lf::Params &params, const lf::Context &context) const override
  {
    const SocketValueVariant show_variant = params.get_input<SocketValueVariant>(0);
    const bool show = show_variant.get<bool>();
    if (!show) {
      params.set_output(0, show_variant);
      return;
    }
    SocketValueVariant *message_variant =
        params.try_get_input_data_ptr_or_request<SocketValueVariant>(1);
    if (!message_variant) {
      /* Wait for the message to be computed. */
      return;
    }
    std::string message = message_variant->extract<std::string>();
    GeoNodesUserData &user_data = *static_cast<GeoNodesUserData *>(context.user_data);
    GeoNodesLocalUserData &local_user_data = *static_cast<GeoNodesLocalUserData *>(
        context.local_user_data);
    if (eval_log::NodeTreeLogger *tree_logger = local_user_data.try_get_tree_logger(user_data)) {
      tree_logger->node_warnings.append(
          *tree_logger->allocator,
          {node_.identifier, {NodeWarningType(node_.custom1), std::move(message)}});
    }
    /* Only set output in the end so that this node is not finished before the warning is set. */
    params.set_output(0, show_variant);
  }
};

using namespace blender::compositor;

class WarningOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const Result &input_show = this->get_input("Show");
    Result &output_show = this->get_result("Show");
    output_show.share_data(input_show);
    if (!input_show.get_single_value_default<bool>()) {
      return;
    }

    nodes::eval_log::NodesEvalLog *log = this->context().nodes_evaluation_log();
    if (!log) {
      return;
    }
    nodes::eval_log::NodeTreeLogger &tree_logger = log->get_local_tree_logger(
        this->get_compute_context());

    const std::string message = this->get_input("Message").get_single_value_default<std::string>();
    tree_logger.node_warnings.append(
        *tree_logger.allocator,
        {this->node().identifier, {NodeWarningType(this->node().custom1), std::move(message)}});
  }
};

static NodeOperation *get_compositor_operation(Context &context, const bNode &node)
{
  return new WarningOperation(context, node);
}

static void node_layout(ui::Layout &layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout.use_property_split_set(true);
  layout.use_property_decorate_set(false);
  layout.prop(ptr, "warning_type", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_rna(StructRNA *srna)
{
  RNA_def_node_enum(srna,
                    "warning_type",
                    "Warning Type",
                    "",
                    rna_enum_node_warning_type_items,
                    NOD_inline_enum_accessors(custom1));
}

static void node_label(const bNodeTree * /*ntree*/,
                       const bNode *node,
                       char *label,
                       int label_maxncpy)
{
  const char *name;
  bool enum_label = RNA_enum_name(rna_enum_node_warning_type_items, node->custom1, &name);
  if (!enum_label) {
    name = N_("Unknown");
  }
  BLI_strncpy_utf8(label, IFACE_(name), label_maxncpy);
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_cmp_node_type_base(&ntype, "GeometryNodeWarning"_ustr, GEO_NODE_WARNING);
  ntype.ui_name = "Warning";
  ntype.ui_description = "Create custom warnings in node groups";
  ntype.enum_name_legacy = "WARNING";
  ntype.nclass = NODE_CLASS_INTERFACE;
  ntype.declare = node_declare;
  ntype.labelfunc = node_label;
  ntype.draw_buttons = node_layout;
  ntype.get_compositor_operation = get_compositor_operation;
  bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace nodes::node_geo_warning_cc

namespace nodes {

std::unique_ptr<LazyFunction> get_warning_node_lazy_function(const bNode &node)
{
  using namespace node_geo_warning_cc;
  BLI_assert(node.type_legacy == GEO_NODE_WARNING);
  return std::make_unique<LazyFunctionForWarningNode>(node);
}

}  // namespace nodes
}  // namespace blender
