/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_shader.hh"
#include "GPU_texture.hh"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** SPLIT NODE ******************** */

namespace blender::nodes::node_composite_split_cc {

static void cmp_node_split_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image");
  b.add_input<decl::Color>("Image", "Image_001");
  b.add_output<decl::Color>("Image");
}

static void node_composit_init_split(bNodeTree * /*ntree*/, bNode *node)
{
  node->custom1 = 50; /* default 50% split */
}

static void node_composit_buts_split(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *row, *col;

  col = uiLayoutColumn(layout, false);
  row = uiLayoutRow(col, false);
  uiItemR(row, ptr, "axis", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
  uiItemR(col, ptr, "factor", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class SplitOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    if (this->context().use_gpu()) {
      this->execute_gpu();
    }
    else {
      this->execute_cpu();
    }
  }

  void execute_gpu()
  {
    GPUShader *shader = this->get_split_shader();
    GPU_shader_bind(shader);

    GPU_shader_uniform_1f(shader, "split_ratio", this->get_split_ratio());

    const Result &first_image = this->get_input("Image");
    first_image.bind_as_texture(shader, "first_image_tx");
    const Result &second_image = this->get_input("Image_001");
    second_image.bind_as_texture(shader, "second_image_tx");

    const Domain domain = this->compute_domain();
    Result &output_image = this->get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    first_image.unbind_as_texture();
    second_image.unbind_as_texture();
    output_image.unbind_as_image();
    GPU_shader_unbind();
  }

  GPUShader *get_split_shader()
  {
    if (this->get_split_axis() == CMP_NODE_SPLIT_HORIZONTAL) {
      return this->context().get_shader("compositor_split_horizontal");
    }

    return this->context().get_shader("compositor_split_vertical");
  }

  void execute_cpu()
  {
    const Result &first_image = this->get_input("Image");
    const Result &second_image = this->get_input("Image_001");

    const Domain domain = this->compute_domain();
    Result &output_image = this->get_result("Image");
    output_image.allocate_texture(domain);

    const float split_ratio = this->get_split_ratio();
    const bool is_horizontal = this->get_split_axis() == CMP_NODE_SPLIT_HORIZONTAL;
    const float split_pixel = (is_horizontal ? domain.size.x : domain.size.y) * split_ratio;

    if (is_horizontal) {
      parallel_for(domain.size, [&](const int2 texel) {
        output_image.store_pixel(texel,
                                 split_pixel <= texel.x ? first_image.load_pixel(texel) :
                                                          second_image.load_pixel(texel));
      });
    }
    else {
      parallel_for(domain.size, [&](const int2 texel) {
        output_image.store_pixel(texel,
                                 split_pixel <= texel.y ? first_image.load_pixel(texel) :
                                                          second_image.load_pixel(texel));
      });
    }
  }

  CMPNodeSplitAxis get_split_axis()
  {
    return static_cast<CMPNodeSplitAxis>(bnode().custom2);
  }

  float get_split_ratio()
  {
    return bnode().custom1 / 100.0f;
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new SplitOperation(context, node);
}

}  // namespace blender::nodes::node_composite_split_cc

void register_node_type_cmp_split()
{
  namespace file_ns = blender::nodes::node_composite_split_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_SPLIT, "Split", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::cmp_node_split_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_split;
  ntype.flag |= NODE_PREVIEW;
  ntype.initfunc = file_ns::node_composit_init_split;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  ntype.no_muting = true;

  blender::bke::node_register_type(&ntype);
}
