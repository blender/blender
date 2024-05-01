/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include <cstdint>

#include "BLI_math_vector.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_compute.hh"
#include "GPU_shader.hh"
#include "GPU_state.hh"
#include "GPU_storage_buffer.hh"
#include "GPU_vertex_buffer.hh"

#include "COM_node_operation.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** VECTOR BLUR ******************** */

namespace blender::nodes::node_composite_vec_blur_cc {

NODE_STORAGE_FUNCS(NodeBlurData)

static void cmp_node_vec_blur_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_input<decl::Float>("Z").default_value(0.0f).min(0.0f).max(1.0f).compositor_domain_priority(
      2);
  b.add_input<decl::Vector>("Speed")
      .default_value({0.0f, 0.0f, 0.0f})
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_VELOCITY)
      .compositor_domain_priority(1);
  b.add_output<decl::Color>("Image");
}

/* custom1: iterations, custom2: max_speed (0 = no_limit). */
static void node_composit_init_vecblur(bNodeTree * /*ntree*/, bNode *node)
{
  NodeBlurData *nbd = MEM_cnew<NodeBlurData>(__func__);
  node->storage = nbd;
  nbd->samples = 32;
  nbd->fac = 0.25f;
}

static void node_composit_buts_vecblur(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "samples", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(col, ptr, "factor", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("Blur"), ICON_NONE);
}

using namespace blender::realtime_compositor;

class VectorBlurOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    Result &input = get_input("Image");
    Result &output = get_result("Image");
    if (input.is_single_value()) {
      input.pass_through(output);
      return;
    }

    Result max_tile_velocity = compute_max_tile_velocity();
    GPUStorageBuf *tile_indirection_buffer = dilate_max_velocity(max_tile_velocity);
    compute_motion_blur(max_tile_velocity, tile_indirection_buffer);
    max_tile_velocity.release();
    GPU_storagebuf_free(tile_indirection_buffer);
  }

  /* Reduces each 32x32 block of velocity pixels into a single velocity whose magnitude is largest.
   * Each of the previous and next velocities are reduces independently. */
  Result compute_max_tile_velocity()
  {
    GPUShader *shader = context().get_shader("compositor_max_velocity");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1b(shader, "is_initial_reduction", true);

    Result &input = get_input("Speed");
    input.bind_as_texture(shader, "input_tx");

    Result output = context().create_temporary_result(ResultType::Color);
    const int2 tiles_count = math::divide_ceil(input.domain().size, int2(32));
    output.allocate_texture(Domain(tiles_count));
    output.bind_as_image(shader, "output_img");

    GPU_compute_dispatch(shader, tiles_count.x, tiles_count.y, 1);

    GPU_shader_unbind();
    input.unbind_as_texture();
    output.unbind_as_image();

    return output;
  }

  /* The max tile velocity image computes the maximum within 32x32 blocks, while the velocity can
   * in fact extend beyond such a small block. So we dilate the max blocks by taking the maximum
   * along the path of each of the max velocity tiles. Since the shader uses custom max atomics,
   * the output will be an indirection buffer that points to a particular tile in the original max
   * tile velocity image. This is done as a form of performance optimization, see the shader for
   * more information. */
  GPUStorageBuf *dilate_max_velocity(Result &max_tile_velocity)
  {
    GPUShader *shader = context().get_shader("compositor_motion_blur_max_velocity_dilate");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1f(shader, "shutter_speed", node_storage(bnode()).fac);

    max_tile_velocity.bind_as_texture(shader, "input_tx");

    /* The shader assumes a maximum input size of 16k, and since the max tile velocity image is
     * composed of blocks of 32, we get 16k / 32 = 512. So the table is 512x512, but we store two
     * tables for the previous and next velocities, so we double that. */
    const int size = sizeof(uint32_t) * 512 * 512 * 2;
    GPUStorageBuf *tile_indirection_buffer = GPU_storagebuf_create_ex(
        size, nullptr, GPU_USAGE_DEVICE_ONLY, __func__);
    GPU_storagebuf_clear_to_zero(tile_indirection_buffer);
    const int slot = GPU_shader_get_ssbo_binding(shader, "tile_indirection_buf");
    GPU_storagebuf_bind(tile_indirection_buffer, slot);

    compute_dispatch_threads_at_least(shader, max_tile_velocity.domain().size);

    GPU_shader_unbind();
    max_tile_velocity.unbind_as_texture();
    GPU_storagebuf_unbind(tile_indirection_buffer);

    return tile_indirection_buffer;
  }

  void compute_motion_blur(Result &max_tile_velocity, GPUStorageBuf *tile_indirection_buffer)
  {
    GPUShader *shader = context().get_shader("compositor_motion_blur");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1i(shader, "samples_count", node_storage(bnode()).samples);
    GPU_shader_uniform_1f(shader, "shutter_speed", node_storage(bnode()).fac);

    Result &input = get_input("Image");
    input.bind_as_texture(shader, "input_tx");

    Result &depth = get_input("Z");
    depth.bind_as_texture(shader, "depth_tx");

    Result &velocity = get_input("Speed");
    velocity.bind_as_texture(shader, "velocity_tx");

    max_tile_velocity.bind_as_texture(shader, "max_velocity_tx");

    GPU_memory_barrier(GPU_BARRIER_SHADER_STORAGE);
    const int slot = GPU_shader_get_ssbo_binding(shader, "tile_indirection_buf");
    GPU_storagebuf_bind(tile_indirection_buffer, slot);

    Result &output = get_result("Image");
    const Domain domain = compute_domain();
    output.allocate_texture(domain);
    output.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, output.domain().size);

    GPU_shader_unbind();
    input.unbind_as_texture();
    depth.unbind_as_texture();
    velocity.unbind_as_texture();
    max_tile_velocity.unbind_as_texture();
    output.unbind_as_image();
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new VectorBlurOperation(context, node);
}

}  // namespace blender::nodes::node_composite_vec_blur_cc

void register_node_type_cmp_vecblur()
{
  namespace file_ns = blender::nodes::node_composite_vec_blur_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_VECBLUR, "Vector Blur", NODE_CLASS_OP_FILTER);
  ntype.declare = file_ns::cmp_node_vec_blur_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_vecblur;
  ntype.initfunc = file_ns::node_composit_init_vecblur;
  node_type_storage(
      &ntype, "NodeBlurData", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}
