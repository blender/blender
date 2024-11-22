/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "GPU_shader.hh"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "node_composite_util.hh"

/* **************** Pixelate ******************** */

namespace blender::nodes::node_composite_pixelate_cc {

static void cmp_node_pixelate_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Color").compositor_domain_priority(0);
  b.add_output<decl::Color>("Color");
}

static void node_composit_init_pixelate(bNodeTree * /*ntree*/, bNode *node)
{
  node->custom1 = 1;
}

static void node_composit_buts_pixelate(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "pixel_size", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class PixelateOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    Result &input_image = get_input("Color");
    Result &output_image = get_result("Color");
    const int pixel_size = get_pixel_size();
    if (input_image.is_single_value() || pixel_size == 1) {
      input_image.pass_through(output_image);
      return;
    }

    if (this->context().use_gpu()) {
      this->execute_gpu();
    }
    else {
      this->execute_cpu();
    }
  }

  void execute_gpu()
  {
    GPUShader *shader = context().get_shader("compositor_pixelate");
    GPU_shader_bind(shader);

    const int pixel_size = get_pixel_size();
    GPU_shader_uniform_1i(shader, "pixel_size", pixel_size);

    Result &input_image = get_input("Color");
    input_image.bind_as_texture(shader, "input_tx");

    Result &output_image = get_result("Color");
    const Domain domain = compute_domain();
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    output_image.unbind_as_image();
    input_image.unbind_as_texture();
  }

  void execute_cpu()
  {
    Result &input = get_input("Color");

    Result &output = get_result("Color");
    const Domain domain = compute_domain();
    output.allocate_texture(domain);

    const int2 size = domain.size;
    const int pixel_size = get_pixel_size();
    parallel_for(size, [&](const int2 texel) {
      int2 start = (texel / int2(pixel_size)) * int2(pixel_size);
      int2 end = math::min(start + int2(pixel_size), size);

      float4 accumulated_color = float4(0.0f);
      for (int y = start.y; y < end.y; y++) {
        for (int x = start.x; x < end.x; x++) {
          accumulated_color += input.load_pixel(int2(x, y));
        }
      }

      int2 size = end - start;
      int count = size.x * size.y;
      output.store_pixel(texel, accumulated_color / count);
    });
  }

  float get_pixel_size()
  {
    return bnode().custom1;
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new PixelateOperation(context, node);
}

}  // namespace blender::nodes::node_composite_pixelate_cc

void register_node_type_cmp_pixelate()
{
  namespace file_ns = blender::nodes::node_composite_pixelate_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_PIXELATE, "Pixelate", NODE_CLASS_OP_FILTER);
  ntype.declare = file_ns::cmp_node_pixelate_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_pixelate;
  ntype.initfunc = file_ns::node_composit_init_pixelate;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(&ntype);
}
