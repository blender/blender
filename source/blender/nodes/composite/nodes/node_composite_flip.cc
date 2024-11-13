/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_assert.h"
#include "BLI_math_vector_types.hh"
#include "BLI_utildefines.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_shader.hh"
#include "GPU_texture.hh"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** Flip  ******************** */

namespace blender::nodes::node_composite_flip_cc {

static void cmp_node_flip_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_output<decl::Color>("Image");
}

static void node_composit_buts_flip(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "axis", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

using namespace blender::realtime_compositor;

class FlipOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    Result &input = get_input("Image");
    Result &result = get_result("Image");

    /* Can't flip a single value, pass it through to the output. */
    if (input.is_single_value()) {
      input.pass_through(result);
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
    GPUShader *shader = context().get_shader("compositor_flip");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1b(
        shader, "flip_x", ELEM(get_flip_mode(), CMP_NODE_FLIP_X, CMP_NODE_FLIP_X_Y));
    GPU_shader_uniform_1b(
        shader, "flip_y", ELEM(get_flip_mode(), CMP_NODE_FLIP_Y, CMP_NODE_FLIP_X_Y));

    Result &input = get_input("Image");
    input.bind_as_texture(shader, "input_tx");

    const Domain domain = compute_domain();
    Result &result = get_result("Image");
    result.allocate_texture(domain);
    result.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    input.unbind_as_texture();
    result.unbind_as_image();
    GPU_shader_unbind();
  }

  void execute_cpu()
  {
    const bool flip_x = ELEM(get_flip_mode(), CMP_NODE_FLIP_X, CMP_NODE_FLIP_X_Y);
    const bool flip_y = ELEM(get_flip_mode(), CMP_NODE_FLIP_Y, CMP_NODE_FLIP_X_Y);

    Result &input = get_input("Image");

    const Domain domain = compute_domain();
    Result &output = get_result("Image");
    output.allocate_texture(domain);

    const int2 size = domain.size;
    parallel_for(domain.size, [&](const int2 texel) {
      int2 flipped_texel = texel;
      if (flip_x) {
        flipped_texel.x = size.x - texel.x - 1;
      }
      if (flip_y) {
        flipped_texel.y = size.y - texel.y - 1;
      }
      output.store_pixel(texel, input.load_pixel(flipped_texel));
    });
  }

  CMPNodeFlipMode get_flip_mode()
  {
    return static_cast<CMPNodeFlipMode>(bnode().custom1);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new FlipOperation(context, node);
}

}  // namespace blender::nodes::node_composite_flip_cc

void register_node_type_cmp_flip()
{
  namespace file_ns = blender::nodes::node_composite_flip_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_FLIP, "Flip", NODE_CLASS_DISTORT);
  ntype.declare = file_ns::cmp_node_flip_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_flip;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(&ntype);
}
