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

#include "UI_resources.hh"

#include "node_composite_util.hh"

/* **************** Pixelate ******************** */

namespace blender::nodes::node_composite_pixelate_cc {

static void cmp_node_pixelate_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::Color>("Color").structure_type(StructureType::Dynamic).hide_value();
  b.add_output<decl::Color>("Color").structure_type(StructureType::Dynamic).align_with_previous();

  b.add_input<decl::Int>("Size").default_value(1).min(1).description(
      "The number of pixels that correspond to the same output pixel");
}

using namespace blender::compositor;

class PixelateOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const Result &input_image = this->get_input("Color");
    const int pixel_size = this->get_pixel_size();
    if (input_image.is_single_value() || pixel_size == 1) {
      Result &output_image = this->get_result("Color");
      output_image.share_data(input_image);
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
    gpu::Shader *shader = context().get_shader("compositor_pixelate");
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
          accumulated_color += float4(input.load_pixel<Color>(int2(x, y)));
        }
      }

      int2 size = end - start;
      int count = size.x * size.y;
      output.store_pixel(texel, Color(accumulated_color / count));
    });
  }

  float get_pixel_size()
  {
    return math::max(1, this->get_input("Size").get_single_value_default(1));
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new PixelateOperation(context, node);
}

}  // namespace blender::nodes::node_composite_pixelate_cc

static void register_node_type_cmp_pixelate()
{
  namespace file_ns = blender::nodes::node_composite_pixelate_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodePixelate", CMP_NODE_PIXELATE);
  ntype.ui_name = "Pixelate";
  ntype.ui_description =
      "Reduce detail in an image by making individual pixels more prominent, for a blocky or "
      "mosaic-like appearance";
  ntype.enum_name_legacy = "PIXELATE";
  ntype.nclass = NODE_CLASS_OP_FILTER;
  ntype.declare = file_ns::cmp_node_pixelate_declare;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_pixelate)
