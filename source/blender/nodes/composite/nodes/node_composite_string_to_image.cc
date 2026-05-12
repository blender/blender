/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <optional>

#include "DNA_vfont_types.h"

#include "BKE_vfont.hh"

#include "UI_resources.hh"

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_string_to_image_cc {

static const EnumPropertyItem rna_node_compositor_string_to_image_horizontal_alignment_items[] = {
    {CMP_NODE_STRING_TO_IMAGE_HORIZONTAL_ALIGNMENT_LEFT,
     "LEFT",
     ICON_ALIGN_LEFT,
     "Left",
     "Align text to the left"},
    {CMP_NODE_STRING_TO_IMAGE_HORIZONTAL_ALIGNMENT_CENTER,
     "CENTER",
     ICON_ALIGN_CENTER,
     "Center",
     "Align text to the center"},
    {CMP_NODE_STRING_TO_IMAGE_HORIZONTAL_ALIGNMENT_RIGHT,
     "RIGHT",
     ICON_ALIGN_RIGHT,
     "Right",
     "Align text to the right"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem rna_node_compositor_string_to_image_vertical_alignment_items[] = {
    {CMP_NODE_STRING_TO_IMAGE_VERTICAL_ALIGNMENT_TOP,
     "TOP",
     ICON_ALIGN_TOP,
     "Top",
     "Align text to the top"},
    {CMP_NODE_STRING_TO_IMAGE_VERTICAL_ALIGNMENT_TOP_BASELINE,
     "TOP_BASELINE",
     ICON_ALIGN_TOP,
     "Top Baseline",
     "Align text to the top line's baseline"},
    {CMP_NODE_STRING_TO_IMAGE_VERTICAL_ALIGNMENT_MIDDLE,
     "MIDDLE",
     ICON_ALIGN_MIDDLE,
     "Middle",
     "Align text to the middle"},
    {CMP_NODE_STRING_TO_IMAGE_VERTICAL_ALIGNMENT_BOTTOM_BASELINE,
     "BOTTOM_BASELINE",
     ICON_ALIGN_BOTTOM,
     "Bottom Baseline",
     "Align text to the bottom line's baseline"},
    {CMP_NODE_STRING_TO_IMAGE_VERTICAL_ALIGNMENT_BOTTOM,
     "BOTTOM",
     ICON_ALIGN_BOTTOM,
     "Bottom",
     "Align text to the bottom"},
    {0, nullptr, 0, nullptr, nullptr},
};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  b.add_output<decl::Float>("Image"_ustr)
      .structure_type(StructureType::Dynamic)
      .description("The image containing the paragraph of text");

  b.add_input<decl::String>("String"_ustr).optional_label();
  b.add_input<decl::Font>("Font"_ustr)
      .default_value_fn(
          [](const bNode & /*node*/) { return id_cast<ID *>(BKE_vfont_builtin_ensure()); })
      .optional_label();
  b.add_input<decl::Float>("Size"_ustr)
      .default_value(128.0f)
      .subtype(PROP_PIXEL)
      .min(0.0f)
      .description("The height of each line in pixels");

  {
    PanelDeclarationBuilder &panel = b.add_panel("Alignment"_ustr).default_closed(true);
    panel.add_input<decl::Menu>("Horizontal Alignment"_ustr)
        .static_items(rna_node_compositor_string_to_image_horizontal_alignment_items)
        .default_value(CMP_NODE_STRING_TO_IMAGE_HORIZONTAL_ALIGNMENT_CENTER)
        .optional_label();
    panel.add_input<decl::Menu>("Vertical Alignment"_ustr)
        .static_items(rna_node_compositor_string_to_image_vertical_alignment_items)
        .default_value(CMP_NODE_STRING_TO_IMAGE_VERTICAL_ALIGNMENT_MIDDLE)
        .optional_label();
  }

  {
    PanelDeclarationBuilder &panel = b.add_panel("Wrap"_ustr).default_closed(true);
    panel.add_input<decl::Bool>("Wrap"_ustr)
        .default_value(true)
        .panel_toggle()
        .description("Wrap text into new lines if it exceeds the specified width");
    panel.add_input<decl::Int>("Width"_ustr, "Wrap Width"_ustr)
        .default_value(1920)
        .min(0)
        .subtype(PROP_PIXEL)
        .description(
            "The maximum width of each line in pixels. Lines with larger widths will be wrapped "
            "into new lines");
  }
}

using namespace blender::compositor;

class StringToImageOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const std::string string = this->get_input("String").get_single_value_default<std::string>();
    const VFont *font = this->get_input("Font").get_single_value_default<VFont *>();
    const float size = this->get_input("Size").get_single_value_default<float>();

    const Result &string_image = this->context().cache_manager().string_images.get(
        this->context(),
        string,
        font,
        size,
        this->get_horizontal_alignment(),
        this->get_vertical_alignment(),
        this->get_wrap_width());
    if (!string_image.is_allocated()) {
      this->allocate_default_remaining_outputs();
      return;
    }

    Result &output = this->get_result("Image");
    output.share_data(string_image);
  }

  HorizontalAlignment get_horizontal_alignment()
  {
    const auto horizontal_alignment = CMPNodeStringToImageHorizontalAlignment(
        this->get_input("Horizontal Alignment").get_single_value_default<MenuValue>().value);
    switch (horizontal_alignment) {
      case CMP_NODE_STRING_TO_IMAGE_HORIZONTAL_ALIGNMENT_LEFT:
        return HorizontalAlignment::Left;
      case CMP_NODE_STRING_TO_IMAGE_HORIZONTAL_ALIGNMENT_CENTER:
        return HorizontalAlignment::Center;
      case CMP_NODE_STRING_TO_IMAGE_HORIZONTAL_ALIGNMENT_RIGHT:
        return HorizontalAlignment::Right;
    }

    BLI_assert_unreachable();
    return HorizontalAlignment::Left;
  }

  VerticalAlignment get_vertical_alignment()
  {
    const auto vertical_alignment = CMPNodeStringToImageVerticalAlignment(
        this->get_input("Vertical Alignment").get_single_value_default<MenuValue>().value);
    switch (vertical_alignment) {
      case CMP_NODE_STRING_TO_IMAGE_VERTICAL_ALIGNMENT_TOP:
        return VerticalAlignment::Top;
      case CMP_NODE_STRING_TO_IMAGE_VERTICAL_ALIGNMENT_TOP_BASELINE:
        return VerticalAlignment::TopBaseline;
      case CMP_NODE_STRING_TO_IMAGE_VERTICAL_ALIGNMENT_MIDDLE:
        return VerticalAlignment::Middle;
      case CMP_NODE_STRING_TO_IMAGE_VERTICAL_ALIGNMENT_BOTTOM_BASELINE:
        return VerticalAlignment::BottomBaseline;
      case CMP_NODE_STRING_TO_IMAGE_VERTICAL_ALIGNMENT_BOTTOM:
        return VerticalAlignment::Bottom;
    }

    BLI_assert_unreachable();
    return VerticalAlignment::Middle;
  }

  std::optional<int> get_wrap_width()
  {
    const bool use_wrap = this->get_input("Wrap").get_single_value_default<bool>();
    if (!use_wrap) {
      return std::nullopt;
    }

    return math::max(0, this->get_input("Wrap Width").get_single_value_default<int>());
  }
};

static NodeOperation *get_compositor_operation(Context &context, const bNode &node)
{
  return new StringToImageOperation(context, node);
}

static void node_register()
{
  static bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeStringToImage"_ustr);
  ntype.ui_name = "String To Image";
  ntype.ui_description = "Generates an image containing the given paragraph of text";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = node_declare;
  ntype.get_compositor_operation = get_compositor_operation;

  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_composite_string_to_image_cc
