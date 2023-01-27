/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_base.hh"
#include "BLI_math_vector_types.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "GPU_texture.h"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** BLUR ******************** */

namespace blender::nodes::node_composite_bokehblur_cc {

static void cmp_node_bokehblur_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image"))
      .default_value({0.8f, 0.8f, 0.8f, 1.0f})
      .compositor_domain_priority(0);
  b.add_input<decl::Color>(N_("Bokeh"))
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_skip_realization();
  b.add_input<decl::Float>(N_("Size"))
      .default_value(1.0f)
      .min(0.0f)
      .max(10.0f)
      .compositor_domain_priority(1);
  b.add_input<decl::Float>(N_("Bounding box"))
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .compositor_domain_priority(2);
  b.add_output<decl::Color>(N_("Image"));
}

static void node_composit_init_bokehblur(bNodeTree * /*ntree*/, bNode *node)
{
  node->custom3 = 4.0f;
  node->custom4 = 16.0f;
}

static void node_composit_buts_bokehblur(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "use_variable_size", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  // uiItemR(layout, ptr, "f_stop", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE); /* UNUSED */
  uiItemR(layout, ptr, "blur_max", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "use_extended_bounds", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class BokehBlurOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    if (is_identity()) {
      get_input("Image").pass_through(get_result("Image"));
      return;
    }

    if (get_input("Size").is_single_value() || !get_variable_size()) {
      execute_constant_size();
    }
    else {
      execute_variable_size();
    }
  }

  void execute_constant_size()
  {
    GPUShader *shader = shader_manager().get("compositor_blur");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1i(shader, "radius", int(compute_blur_radius()));
    GPU_shader_uniform_1b(shader, "extend_bounds", get_extend_bounds());

    const Result &input_image = get_input("Image");
    input_image.bind_as_texture(shader, "input_tx");

    const Result &input_weights = get_input("Bokeh");
    input_weights.bind_as_texture(shader, "weights_tx");

    const Result &input_mask = get_input("Bounding box");
    input_mask.bind_as_texture(shader, "mask_tx");

    Domain domain = compute_domain();
    if (get_extend_bounds()) {
      /* Add a radius amount of pixels in both sides of the image, hence the multiply by 2. */
      domain.size += int2(int(compute_blur_radius()) * 2);
    }

    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    output_image.unbind_as_image();
    input_image.unbind_as_texture();
    input_weights.unbind_as_texture();
    input_mask.unbind_as_texture();
  }

  void execute_variable_size()
  {
    GPUShader *shader = shader_manager().get("compositor_blur_variable_size");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1f(shader, "base_size", compute_blur_radius());
    GPU_shader_uniform_1i(shader, "search_radius", get_max_size());

    const Result &input_image = get_input("Image");
    input_image.bind_as_texture(shader, "input_tx");

    const Result &input_weights = get_input("Bokeh");
    input_weights.bind_as_texture(shader, "weights_tx");

    const Result &input_size = get_input("Size");
    input_size.bind_as_texture(shader, "size_tx");

    const Result &input_mask = get_input("Bounding box");
    input_mask.bind_as_texture(shader, "mask_tx");

    const Domain domain = compute_domain();
    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    output_image.unbind_as_image();
    input_image.unbind_as_texture();
    input_weights.unbind_as_texture();
    input_size.unbind_as_texture();
    input_mask.unbind_as_texture();
  }

  float compute_blur_radius()
  {
    const int2 image_size = get_input("Image").domain().size;
    const int max_size = math::max(image_size.x, image_size.y);

    /* The [0, 10] range of the size is arbitrary and is merely in place to avoid very long
     * computations of the bokeh blur. */
    const float size = math::clamp(get_input("Size").get_float_value_default(1.0f), 0.0f, 10.0f);

    /* The 100 divisor is arbitrary and was chosen using visual judgment. */
    return size * (max_size / 100.0f);
  }

  bool is_identity()
  {
    const Result &input = get_input("Image");
    if (input.is_single_value()) {
      return true;
    }

    if (compute_blur_radius() == 0.0f) {
      return true;
    }

    /* This input is, in fact, a boolean mask. If it is zero, no blurring will take place.
     * Otherwise, the blurring will take place ignoring the value of the input entirely. */
    const Result &bounding_box = get_input("Bounding box");
    if (bounding_box.is_single_value() && bounding_box.get_float_value() == 0.0) {
      return true;
    }

    return false;
  }

  bool get_extend_bounds()
  {
    return bnode().custom1 & CMP_NODEFLAG_BLUR_EXTEND_BOUNDS;
  }

  bool get_variable_size()
  {
    return bnode().custom1 & CMP_NODEFLAG_BLUR_VARIABLE_SIZE;
  }

  int get_max_size()
  {
    return int(bnode().custom4);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new BokehBlurOperation(context, node);
}

}  // namespace blender::nodes::node_composite_bokehblur_cc

void register_node_type_cmp_bokehblur()
{
  namespace file_ns = blender::nodes::node_composite_bokehblur_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_BOKEHBLUR, "Bokeh Blur", NODE_CLASS_OP_FILTER);
  ntype.declare = file_ns::cmp_node_bokehblur_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_bokehblur;
  ntype.initfunc = file_ns::node_composit_init_bokehblur;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}
