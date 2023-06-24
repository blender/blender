/* SPDX-FileCopyrightText: 2006 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_assert.h"
#include "BLI_math_base.hh"
#include "BLI_math_vector_types.hh"

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "GPU_shader.h"
#include "GPU_state.h"
#include "GPU_texture.h"

#include "COM_algorithm_morphological_distance.hh"
#include "COM_algorithm_morphological_distance_feather.hh"
#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** Dilate/Erode ******************** */

namespace blender::nodes::node_composite_dilate_cc {

NODE_STORAGE_FUNCS(NodeDilateErode)

static void cmp_node_dilate_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Mask").default_value(0.0f).min(0.0f).max(1.0f);
  b.add_output<decl::Float>("Mask");
}

static void node_composit_init_dilateerode(bNodeTree * /*ntree*/, bNode *node)
{
  NodeDilateErode *data = MEM_cnew<NodeDilateErode>(__func__);
  data->falloff = PROP_SMOOTH;
  node->storage = data;
}

static void node_composit_buts_dilateerode(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "distance", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  switch (RNA_enum_get(ptr, "mode")) {
    case CMP_NODE_DILATE_ERODE_DISTANCE_THRESHOLD:
      uiItemR(layout, ptr, "edge", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
      break;
    case CMP_NODE_DILATE_ERODE_DISTANCE_FEATHER:
      uiItemR(layout, ptr, "falloff", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
      break;
  }
}

using namespace blender::realtime_compositor;

class DilateErodeOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    if (is_identity()) {
      get_input("Mask").pass_through(get_result("Mask"));
      return;
    }

    switch (get_method()) {
      case CMP_NODE_DILATE_ERODE_STEP:
        execute_step();
        return;
      case CMP_NODE_DILATE_ERODE_DISTANCE:
        execute_distance();
        return;
      case CMP_NODE_DILATE_ERODE_DISTANCE_THRESHOLD:
        execute_distance_threshold();
        return;
      case CMP_NODE_DILATE_ERODE_DISTANCE_FEATHER:
        execute_distance_feather();
        return;
      default:
        BLI_assert_unreachable();
        return;
    }
  }

  /* ----------------------------
   * Step Morphological Operator.
   * ---------------------------- */

  void execute_step()
  {
    GPUTexture *horizontal_pass_result = execute_step_horizontal_pass();
    execute_step_vertical_pass(horizontal_pass_result);
  }

  GPUTexture *execute_step_horizontal_pass()
  {
    GPUShader *shader = shader_manager().get(get_morphological_step_shader_name());
    GPU_shader_bind(shader);

    /* Pass the absolute value of the distance. We have specialized shaders for each sign. */
    GPU_shader_uniform_1i(shader, "radius", math::abs(get_distance()));

    const Result &input_mask = get_input("Mask");
    input_mask.bind_as_texture(shader, "input_tx");

    /* We allocate an output image of a transposed size, that is, with a height equivalent to the
     * width of the input and vice versa. This is done as a performance optimization. The shader
     * will process the image horizontally and write it to the intermediate output transposed. Then
     * the vertical pass will execute the same horizontal pass shader, but since its input is
     * transposed, it will effectively do a vertical pass and write to the output transposed,
     * effectively undoing the transposition in the horizontal pass. This is done to improve
     * spatial cache locality in the shader and to avoid having two separate shaders for each of
     * the passes. */
    const Domain domain = compute_domain();
    const int2 transposed_domain = int2(domain.size.y, domain.size.x);

    GPUTexture *horizontal_pass_result = texture_pool().acquire_color(transposed_domain);
    const int image_unit = GPU_shader_get_sampler_binding(shader, "output_img");
    GPU_texture_image_bind(horizontal_pass_result, image_unit);

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    input_mask.unbind_as_texture();
    GPU_texture_image_unbind(horizontal_pass_result);

    return horizontal_pass_result;
  }

  void execute_step_vertical_pass(GPUTexture *horizontal_pass_result)
  {
    GPUShader *shader = shader_manager().get(get_morphological_step_shader_name());
    GPU_shader_bind(shader);

    /* Pass the absolute value of the distance. We have specialized shaders for each sign. */
    GPU_shader_uniform_1i(shader, "radius", math::abs(get_distance()));

    GPU_memory_barrier(GPU_BARRIER_TEXTURE_FETCH);
    const int texture_image_unit = GPU_shader_get_sampler_binding(shader, "input_tx");
    GPU_texture_bind(horizontal_pass_result, texture_image_unit);

    const Domain domain = compute_domain();
    Result &output_mask = get_result("Mask");
    output_mask.allocate_texture(domain);
    output_mask.bind_as_image(shader, "output_img");

    /* Notice that the domain is transposed, see the note on the horizontal pass method for more
     * information on the reasoning behind this. */
    compute_dispatch_threads_at_least(shader, int2(domain.size.y, domain.size.x));

    GPU_shader_unbind();
    output_mask.unbind_as_image();
    GPU_texture_unbind(horizontal_pass_result);
  }

  const char *get_morphological_step_shader_name()
  {
    if (get_distance() > 0) {
      return "compositor_morphological_step_dilate";
    }
    return "compositor_morphological_step_erode";
  }

  /* --------------------------------
   * Distance Morphological Operator.
   * -------------------------------- */

  void execute_distance()
  {
    morphological_distance(context(), get_input("Mask"), get_result("Mask"), get_distance());
  }

  /* ------------------------------------------
   * Distance Threshold Morphological Operator.
   * ------------------------------------------ */

  void execute_distance_threshold()
  {
    GPUShader *shader = shader_manager().get("compositor_morphological_distance_threshold");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1f(shader, "inset", get_inset());
    GPU_shader_uniform_1i(shader, "radius", get_morphological_distance_threshold_radius());
    GPU_shader_uniform_1i(shader, "distance", get_distance());

    const Result &input_mask = get_input("Mask");
    input_mask.bind_as_texture(shader, "input_tx");

    const Domain domain = compute_domain();
    Result &output_mask = get_result("Mask");
    output_mask.allocate_texture(domain);
    output_mask.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    output_mask.unbind_as_image();
    input_mask.unbind_as_texture();
  }

  /* See the discussion in the implementation for more information. */
  int get_morphological_distance_threshold_radius()
  {
    return int(math::ceil(get_inset())) + math::abs(get_distance());
  }

  /* ----------------------------------------
   * Distance Feather Morphological Operator.
   * ---------------------------------------- */

  void execute_distance_feather()
  {
    morphological_distance_feather(context(),
                                   get_input("Mask"),
                                   get_result("Mask"),
                                   get_distance(),
                                   node_storage(bnode()).falloff);
  }

  /* ---------------
   * Common Methods.
   * --------------- */

  bool is_identity()
  {
    const Result &input = get_input("Mask");
    if (input.is_single_value()) {
      return true;
    }

    if (get_method() == CMP_NODE_DILATE_ERODE_DISTANCE_THRESHOLD && get_inset() != 0.0f) {
      return false;
    }

    if (get_distance() == 0) {
      return true;
    }

    return false;
  }

  int get_distance()
  {
    return bnode().custom2;
  }

  float get_inset()
  {
    return bnode().custom3;
  }

  CMPNodeDilateErodeMethod get_method()
  {
    return (CMPNodeDilateErodeMethod)bnode().custom1;
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new DilateErodeOperation(context, node);
}

}  // namespace blender::nodes::node_composite_dilate_cc

void register_node_type_cmp_dilateerode()
{
  namespace file_ns = blender::nodes::node_composite_dilate_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_DILATEERODE, "Dilate/Erode", NODE_CLASS_OP_FILTER);
  ntype.draw_buttons = file_ns::node_composit_buts_dilateerode;
  ntype.declare = file_ns::cmp_node_dilate_declare;
  ntype.initfunc = file_ns::node_composit_init_dilateerode;
  node_type_storage(
      &ntype, "NodeDilateErode", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}
