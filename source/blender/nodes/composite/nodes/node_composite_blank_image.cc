/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_blank_image_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Color")
      .default_value({0.0f, 0.0f, 0.0f, 1.0f})
      .description("The color of all pixels in the image");
  b.add_input<decl::IntVector>("Size")
      .dimensions(2)
      .default_value(int2(1920, 1080))
      .subtype(PROP_UNSIGNED)
      .min(1)
      .description("The size of the image");

  b.add_output<decl::Color>("Image")
      .structure_type(StructureType::Dynamic)
      .description("An image of the given size and constant color");
}

using namespace blender::compositor;

class BlankImageOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const int2 size = this->get_input("Size").get_single_value_default<int2>();
    const int2 image_size = math::max(int2(1), size);

    Result &output = this->get_result("Image");
    output.allocate_texture(Domain(image_size));
    const Color color = this->get_input("Color").get_single_value_default<Color>();
    if (this->context().use_gpu()) {
      GPU_texture_clear(output, GPU_DATA_FLOAT, color);
    }
    else {
      parallel_for(output.domain().data_size,
                   [&](const int2 texel) { output.store_pixel(texel, color); });
    }
  }
};

static NodeOperation *get_compositor_operation(Context &context, const bNode &node)
{
  return new BlankImageOperation(context, node);
}

static void node_register()
{
  static bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeBlankImage");
  ntype.ui_name = "Blank Image";
  ntype.ui_description = "Returns an image with the given size and constant color";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = node_declare;
  ntype.get_compositor_operation = get_compositor_operation;

  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_composite_blank_image_cc
