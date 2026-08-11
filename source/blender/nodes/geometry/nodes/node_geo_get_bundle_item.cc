/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "NOD_geo_bundle.hh"
#include "NOD_geometry_nodes_bundle.hh"
#include "NOD_rna_define.hh"

#include "RNA_enum_types.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "BKE_type_conversions.hh"

#include "COM_bundle_item.hh"
#include "COM_conversion_operation.hh"
#include "COM_node_operation.hh"

#include <fmt/format.h>

namespace blender::nodes::node_geo_get_bundle_item_cc {

NODE_STORAGE_FUNCS(NodeGetBundleItem)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_default_layout();
  const bNode *node = b.node_or_null();

  b.add_input<decl::Bundle>("Bundle"_ustr);
  b.add_output<decl::Bundle>("Bundle"_ustr).align_with_previous().propagate_all();
  if (node != nullptr) {
    const NodeGetBundleItem &storage = node_storage(*node);
    const eNodeSocketDatatype socket_type = storage.socket_type;
    auto &decl = b.add_output(socket_type, "Item"_ustr).propagate_all();
    if (storage.structure_type == NodeSocketInterfaceStructureType::Auto) {
      decl.structure_type(StructureType::Dynamic);
    }
    else {
      decl.structure_type(StructureType(storage.structure_type));
    }
  }
  b.add_output<decl::Bool>("Exists"_ustr);
  b.add_input<decl::String>("Path"_ustr).optional_label();
  b.add_input<decl::Bool>("Remove"_ustr);
}

static void node_layout(ui::Layout &layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout.use_property_split_set(true);
  layout.use_property_decorate_set(false);
  layout.prop(ptr, "socket_type", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_layout_ex(ui::Layout &layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout.prop(ptr, "socket_type", UI_ITEM_NONE, "", ICON_NONE);
  layout.use_property_split_set(true);
  layout.use_property_decorate_set(false);
  layout.prop(ptr, "structure_type", UI_ITEM_NONE, IFACE_("Shape"), ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  auto *storage = MEM_new<NodeGetBundleItem>(__func__);
  storage->socket_type = SOCK_FLOAT;
  node->storage = storage;
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const bNode &node = params.node();
  const NodeGetBundleItem &storage = node_storage(node);

  nodes::BundlePtr bundle = params.extract_input<nodes::BundlePtr>("Bundle"_ustr);
  if (!bundle) {
    params.set_default_remaining_outputs();
    return;
  }

  const std::string path = params.extract_input<std::string>("Path"_ustr);
  const bool remove = params.extract_input<bool>("Remove"_ustr);

  if (!Bundle::is_valid_path(path)) {
    if (!path.empty()) {
      params.error_message_add(NodeWarningType::Warning, "Invalid bundle path");
    }
    params.set_output("Bundle"_ustr, std::move(bundle));
    params.set_default_remaining_outputs();
    return;
  }

  const BundleItemValue *value = bundle->lookup_path(path);
  if (!value) {
    if (!params.output_is_required("Exists"_ustr)) {
      params.error_message_add(NodeWarningType::Warning, "Bundle path not found");
    }
    params.set_output("Bundle"_ustr, std::move(bundle));
    params.set_default_remaining_outputs();
    return;
  }
  const auto *socket_value = std::get_if<BundleItemSocketValue>(&value->value);
  if (!socket_value) {
    params.error_message_add(
        NodeWarningType::Error,
        fmt::format("{}: \"{}\"", TIP_("Cannot get internal value from bundle"), path));
    params.set_output("Bundle"_ustr, std::move(bundle));
    params.set_default_remaining_outputs();
    return;
  }

  const bke::bNodeSocketType *stype = bke::node_socket_type_find_static(storage.socket_type, 0);
  SocketValueVariant output_value = socket_value->value;
  if (socket_value->type->type != stype->type) {
    if (std::optional<SocketValueVariant> converted_value = implicitly_convert_socket_value(
            *socket_value->type, output_value, *stype))
    {
      output_value = std::move(*converted_value);
    }
    else {
      params.error_message_add(NodeWarningType::Error,
                               "Cannot implicitly convert item to the selected type");
      params.set_output("Bundle"_ustr, std::move(bundle));
      params.set_default_remaining_outputs();
      return;
    }
  }

  if (remove) {
    bundle.ensure_mutable_inplace().remove_path(path);
  }

  params.set_output("Bundle"_ustr, std::move(bundle));
  params.set_output("Item"_ustr, std::move(output_value));
  params.set_output("Exists"_ustr, true);
}

using namespace blender::compositor;

class GetBundleItemOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    Result &bundle_output = this->get_result("Bundle");
    const Result &bundle_input = this->get_input("Bundle");

    const std::string path = this->get_input("Path").get_single_value<std::string>();
    const std::optional<Vector<BundleKey>> split_path = Bundle::split_path(path);
    if (!split_path.has_value()) {
      if (!path.empty()) {
        this->add_warning(NodeWarningType::Warning, "Invalid bundle path");
      }
      if (bundle_output.should_compute()) {
        bundle_output.share_data(bundle_input);
      }
      this->allocate_default_remaining_outputs();
      return;
    }

    Result &item_output = this->get_result("Item");
    Result &exists_output = this->get_result("Exists");

    nodes::BundlePtr bundle = bundle_input.get_single_value<nodes::BundlePtr>();

    const BundleItemValue *item_value = bundle->lookup_path(split_path.value());
    if (!item_value) {
      if (bundle_output.should_compute()) {
        bundle_output.share_data(bundle_input);
      }
      if (exists_output.should_compute()) {
        exists_output.allocate_single_value();
        exists_output.set_single_value(false);
      }
      else {
        this->add_warning(NodeWarningType::Warning, "Bundle path not found");
      }
      this->allocate_default_remaining_outputs();
      return;
    }

    Result bundle_result = BundleItem::get_result(this->context(), *item_value);
    BLI_SCOPED_DEFER([&]() { bundle_result.release(); });

    if (item_output.should_compute()) {
      if (item_output.type() == bundle_result.type()) {
        item_output.share_data(bundle_result);
      }
      else {
        const bke::DataTypeConversions &conversions = bke::get_implicit_type_conversions();
        if (conversions.is_convertible(bundle_result.get_cpp_type(), item_output.get_cpp_type())) {
          ConversionOperation conversion_operation(
              this->context(), bundle_result.type(), item_output.type());
          Result conversion_input = this->context().create_result(bundle_result.type(),
                                                                  bundle_result.precision());
          conversion_input.share_data(bundle_result);
          conversion_operation.map_input_to_result(&conversion_input);
          conversion_operation.evaluate();
          item_output.share_data(conversion_operation.get_result());
          conversion_operation.get_result().release();
        }
        else {
          item_output.allocate_invalid();
          this->add_warning(NodeWarningType::Error,
                            "Cannot implicitly convert item to the selected type");
        }
      }
    }

    if (bundle_output.should_compute()) {
      if (this->get_input("Remove").get_single_value<bool>()) {
        bundle.ensure_mutable_inplace().remove_path(split_path.value());
      }
      bundle_output.allocate_single_value();
      bundle_output.set_single_value(std::move(bundle));
    }

    if (exists_output.should_compute()) {
      exists_output.allocate_single_value();
      exists_output.set_single_value(true);
    }
  }
};

static NodeOperation *get_compositor_operation(Context &context, const bNode &node)
{
  return new GetBundleItemOperation(context, node);
}

static void node_rna(StructRNA *srna)
{
  RNA_def_node_enum(srna,
                    "socket_type",
                    "Socket Type",
                    "Value may be implicitly converted if the type does not match",
                    rna_enum_node_socket_data_type_items,
                    NOD_storage_enum_accessors(socket_type),
                    SOCK_FLOAT,
                    [](bContext * /*C*/, PointerRNA *ptr, PropertyRNA * /*prop*/, bool *r_free) {
                      *r_free = true;
                      const bNodeTree &ntree = *id_cast<const bNodeTree *>(ptr->owner_id);
                      return enum_items_filter(rna_enum_node_socket_data_type_items,
                                               [&](const EnumPropertyItem &item) -> bool {
                                                 return socket_type_supported_in_bundle(
                                                     eNodeSocketDatatype(item.value), ntree.type);
                                               });
                    });
  RNA_def_node_enum(srna,
                    "structure_type",
                    "Structure Type",
                    "What kind of higher order types are expected to flow through this socket",
                    rna_enum_node_socket_structure_type_items,
                    NOD_storage_enum_accessors(structure_type));
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_cmp_node_type_base(&ntype, "NodeGetBundleItem"_ustr);
  ntype.ui_name = "Get Bundle Item";
  ntype.ui_description = "Retrieve a bundle item by path.";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.initfunc = node_init;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  ntype.draw_buttons_ex = node_layout_ex;
  ntype.get_compositor_operation = get_compositor_operation;
  bke::node_type_storage(
      ntype, "NodeGetBundleItem", node_free_standard_storage, node_copy_standard_storage);
  bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_get_bundle_item_cc
