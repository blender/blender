/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_assert.h"
#include "BLI_math_base.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_shader.h"
#include "GPU_texture.h"

#include "COM_algorithm_symmetric_separable_blur.hh"
#include "COM_node_operation.hh"
#include "COM_symmetric_blur_weights.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** BLUR ******************** */

namespace blender::nodes::node_composite_blur_cc {

NODE_STORAGE_FUNCS(NodeBlurData)

static void cmp_node_blur_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_input<decl::Float>("Size")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .compositor_domain_priority(1);
  b.add_output<decl::Color>("Image");
}

static void node_composit_init_blur(bNodeTree * /*ntree*/, bNode *node)
{
  NodeBlurData *data = MEM_cnew<NodeBlurData>(__func__);
  data->filtertype = R_FILTER_GAUSS;
  node->storage = data;
}

static void node_composit_buts_blur(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *col, *row;

  col = uiLayoutColumn(layout, false);
  const int filter = RNA_enum_get(ptr, "filter_type");
  const int reference = RNA_boolean_get(ptr, "use_variable_size");

  uiItemR(col, ptr, "filter_type", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  if (filter != R_FILTER_FAST_GAUSS) {
    uiItemR(col, ptr, "use_variable_size", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
    if (!reference) {
      uiItemR(col, ptr, "use_bokeh", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
    }
    uiItemR(col, ptr, "use_gamma_correction", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  }

  uiItemR(col, ptr, "use_relative", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);

  if (RNA_boolean_get(ptr, "use_relative")) {
    uiItemL(col, IFACE_("Aspect Correction"), ICON_NONE);
    row = uiLayoutRow(layout, true);
    uiItemR(row,
            ptr,
            "aspect_correction",
            UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_EXPAND,
            nullptr,
            ICON_NONE);

    col = uiLayoutColumn(layout, true);
    uiItemR(col, ptr, "factor_x", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("X"), ICON_NONE);
    uiItemR(col, ptr, "factor_y", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("Y"), ICON_NONE);
  }
  else {
    col = uiLayoutColumn(layout, true);
    uiItemR(col, ptr, "size_x", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("X"), ICON_NONE);
    uiItemR(col, ptr, "size_y", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("Y"), ICON_NONE);
  }
  uiItemR(col, ptr, "use_extended_bounds", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class BlurOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    if (is_identity()) {
      get_input("Image").pass_through(get_result("Image"));
      return;
    }

    if (use_variable_size()) {
      execute_variable_size();
    }
    else if (use_separable_filter()) {
      symmetric_separable_blur(context(),
                               get_input("Image"),
                               get_result("Image"),
                               compute_blur_radius(),
                               node_storage(bnode()).filtertype,
                               get_extend_bounds(),
                               node_storage(bnode()).gamma);
    }
    else {
      execute_constant_size();
    }
  }

  void execute_constant_size()
  {
    GPUShader *shader = context().get_shader("compositor_symmetric_blur");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1b(shader, "extend_bounds", get_extend_bounds());
    GPU_shader_uniform_1b(shader, "gamma_correct", node_storage(bnode()).gamma);

    const Result &input_image = get_input("Image");
    input_image.bind_as_texture(shader, "input_tx");

    const float2 blur_radius = compute_blur_radius();

    const SymmetricBlurWeights &weights = context().cache_manager().symmetric_blur_weights.get(
        context(), node_storage(bnode()).filtertype, blur_radius);
    weights.bind_as_texture(shader, "weights_tx");

    Domain domain = compute_domain();
    if (get_extend_bounds()) {
      /* Add a radius amount of pixels in both sides of the image, hence the multiply by 2. */
      domain.size += int2(math::ceil(blur_radius)) * 2;
    }

    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    output_image.unbind_as_image();
    input_image.unbind_as_texture();
    weights.unbind_as_texture();
  }

  void execute_variable_size()
  {
    GPUShader *shader = context().get_shader("compositor_symmetric_blur_variable_size");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1b(shader, "extend_bounds", get_extend_bounds());
    GPU_shader_uniform_1b(shader, "gamma_correct", node_storage(bnode()).gamma);

    const Result &input_image = get_input("Image");
    input_image.bind_as_texture(shader, "input_tx");

    const float2 blur_radius = compute_blur_radius();

    const SymmetricBlurWeights &weights = context().cache_manager().symmetric_blur_weights.get(
        context(), node_storage(bnode()).filtertype, blur_radius);
    weights.bind_as_texture(shader, "weights_tx");

    const Result &input_size = get_input("Size");
    input_size.bind_as_texture(shader, "size_tx");

    Domain domain = compute_domain();
    if (get_extend_bounds()) {
      /* Add a radius amount of pixels in both sides of the image, hence the multiply by 2. */
      domain.size += int2(math::ceil(blur_radius)) * 2;
    }

    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    output_image.unbind_as_image();
    input_image.unbind_as_texture();
    weights.unbind_as_texture();
    input_size.unbind_as_texture();
  }

  float2 compute_blur_radius()
  {
    const float size = math::clamp(get_input("Size").get_float_value_default(1.0f), 0.0f, 1.0f);

    if (!node_storage(bnode()).relative) {
      return float2(node_storage(bnode()).sizex, node_storage(bnode()).sizey) * size;
    }

    int2 image_size = get_input("Image").domain().size;
    switch (node_storage(bnode()).aspect) {
      case CMP_NODE_BLUR_ASPECT_Y:
        image_size.y = image_size.x;
        break;
      case CMP_NODE_BLUR_ASPECT_X:
        image_size.x = image_size.y;
        break;
      default:
        BLI_assert(node_storage(bnode()).aspect == CMP_NODE_BLUR_ASPECT_NONE);
        break;
    }

    return float2(image_size) * get_size_factor() * size;
  }

  /* Returns true if the operation does nothing and the input can be passed through. */
  bool is_identity()
  {
    const Result &input = get_input("Image");
    /* Single value inputs can't be blurred and are returned as is. */
    if (input.is_single_value()) {
      return true;
    }

    /* Zero blur radius. The operation does nothing and the input can be passed through. */
    if (compute_blur_radius() == float2(0.0)) {
      return true;
    }

    return false;
  }

  /* The blur node can operate with different filter types, evaluated on the normalized distance to
   * the center of the filter. Some of those filters are separable and can be computed as such. If
   * the bokeh member is disabled in the node, then the filter is always computed as separable even
   * if it is not in fact separable, in which case, the used filter is a cheaper approximation to
   * the actual filter. If the bokeh member is enabled, then the filter is computed as separable if
   * it is in fact separable and as a normal 2D filter otherwise. */
  bool use_separable_filter()
  {
    if (!node_storage(bnode()).bokeh) {
      return true;
    }

    /* Only Gaussian filters are separable. The rest is not. */
    switch (node_storage(bnode()).filtertype) {
      case R_FILTER_GAUSS:
      case R_FILTER_FAST_GAUSS:
        return true;
      default:
        return false;
    }
  }

  bool use_variable_size()
  {
    return get_variable_size() && !get_input("Size").is_single_value() &&
           node_storage(bnode()).filtertype != R_FILTER_FAST_GAUSS;
  }

  float2 get_size_factor()
  {
    return float2(node_storage(bnode()).percentx, node_storage(bnode()).percenty) / 100.0f;
  }

  bool get_extend_bounds()
  {
    return bnode().custom1 & CMP_NODEFLAG_BLUR_EXTEND_BOUNDS;
  }

  bool get_variable_size()
  {
    return bnode().custom1 & CMP_NODEFLAG_BLUR_VARIABLE_SIZE;
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new BlurOperation(context, node);
}

}  // namespace blender::nodes::node_composite_blur_cc

void register_node_type_cmp_blur()
{
  namespace file_ns = blender::nodes::node_composite_blur_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_BLUR, "Blur", NODE_CLASS_OP_FILTER);
  ntype.declare = file_ns::cmp_node_blur_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_blur;
  ntype.flag |= NODE_PREVIEW;
  ntype.initfunc = file_ns::node_composit_init_blur;
  node_type_storage(
      &ntype, "NodeBlurData", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}
