/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_bounds_types.hh"
#include "BLI_math_vector_types.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_shader.hh"
#include "GPU_texture.hh"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** COMPOSITE ******************** */

namespace blender::nodes::node_composite_composite_cc {

static void cmp_node_composite_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image").default_value({0.0f, 0.0f, 0.0f, 1.0f});
}

using namespace blender::compositor;

class CompositeOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    if (!this->context().is_valid_compositing_region()) {
      return;
    }

    const Result &image = this->get_input("Image");
    if (image.is_single_value()) {
      this->execute_clear();
    }
    else {
      this->execute_copy();
    }
  }

  void execute_clear()
  {
    const Result &image = this->get_input("Image");

    float4 color = image.get_single_value<float4>();

    const Domain domain = this->compute_domain();
    Result output = this->context().get_output_result();
    if (this->context().use_gpu()) {
      GPU_texture_clear(output, GPU_DATA_FLOAT, color);
    }
    else {
      parallel_for(domain.size, [&](const int2 texel) { output.store_pixel(texel, color); });
    }
  }

  void execute_copy()
  {
    if (this->context().use_gpu()) {
      this->execute_copy_gpu();
    }
    else {
      this->execute_copy_cpu();
    }
  }

  void execute_copy_gpu()
  {
    const Result &image = this->get_input("Image");
    const Domain domain = this->compute_domain();
    Result output = this->context().get_output_result();

    GPUShader *shader = this->context().get_shader("compositor_write_output", output.precision());
    GPU_shader_bind(shader);

    const Bounds<int2> bounds = this->get_output_bounds();
    GPU_shader_uniform_2iv(shader, "lower_bound", bounds.min);
    GPU_shader_uniform_2iv(shader, "upper_bound", bounds.max);

    image.bind_as_texture(shader, "input_tx");

    output.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    image.unbind_as_texture();
    output.unbind_as_image();
    GPU_shader_unbind();
  }

  void execute_copy_cpu()
  {
    const Domain domain = this->compute_domain();
    const Result &image = this->get_input("Image");
    Result output = this->context().get_output_result();

    const Bounds<int2> bounds = this->get_output_bounds();
    parallel_for(domain.size, [&](const int2 texel) {
      const int2 output_texel = texel + bounds.min;
      if (output_texel.x > bounds.max.x || output_texel.y > bounds.max.y) {
        return;
      }
      output.store_pixel(texel + bounds.min, image.load_pixel<float4>(texel));
    });
  }

  /* Returns the bounds of the area of the compositing region. Only write into the compositing
   * region, which might be limited to a smaller region of the output result. */
  Bounds<int2> get_output_bounds()
  {
    const rcti compositing_region = this->context().get_compositing_region();
    return Bounds<int2>(int2(compositing_region.xmin, compositing_region.ymin),
                        int2(compositing_region.xmax, compositing_region.ymax));
  }

  /* The operation domain has the same size as the compositing region without any transformations
   * applied. */
  Domain compute_domain() override
  {
    return Domain(this->context().get_compositing_region_size());
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new CompositeOperation(context, node);
}

}  // namespace blender::nodes::node_composite_composite_cc

void register_node_type_cmp_composite()
{
  namespace file_ns = blender::nodes::node_composite_composite_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeComposite", CMP_NODE_COMPOSITE);
  ntype.ui_name = "Composite";
  ntype.ui_description = "Final render output";
  ntype.enum_name_legacy = "COMPOSITE";
  ntype.nclass = NODE_CLASS_OUTPUT;
  ntype.declare = file_ns::cmp_node_composite_declare;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;
  ntype.no_muting = true;

  blender::bke::node_register_type(ntype);
}
