/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BKE_node.hh"
#include "BLI_math_base.h"
#include "BLI_math_vector_types.hh"

#include "DNA_node_types.h"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_shader.hh"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** Crop  ******************** */

namespace blender::nodes::node_composite_crop_cc {

NODE_STORAGE_FUNCS(NodeTwoXYs)

static void cmp_node_crop_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_output<decl::Color>("Image");
}

static void node_composit_init_crop(bNodeTree * /*ntree*/, bNode *node)
{
  NodeTwoXYs *nxy = MEM_callocN<NodeTwoXYs>(__func__);
  node->storage = nxy;
  nxy->x1 = 0;
  nxy->x2 = 0;
  nxy->y1 = 0;
  nxy->y2 = 0;
}

static void node_composit_buts_crop(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *col;

  uiItemR(layout, ptr, "use_crop_size", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
  uiItemR(layout, ptr, "relative", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);

  col = uiLayoutColumn(layout, true);
  if (RNA_boolean_get(ptr, "relative")) {
    uiItemR(col, ptr, "rel_min_x", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("Left"), ICON_NONE);
    uiItemR(col, ptr, "rel_max_x", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("Right"), ICON_NONE);
    uiItemR(col, ptr, "rel_min_y", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("Up"), ICON_NONE);
    uiItemR(col, ptr, "rel_max_y", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("Down"), ICON_NONE);
  }
  else {
    uiItemR(col, ptr, "min_x", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("Left"), ICON_NONE);
    uiItemR(col, ptr, "max_x", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("Right"), ICON_NONE);
    uiItemR(col, ptr, "min_y", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("Up"), ICON_NONE);
    uiItemR(col, ptr, "max_y", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("Down"), ICON_NONE);
  }
}

using namespace blender::compositor;

class CropOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    if (this->is_identity()) {
      const Result &input = get_input("Image");
      Result &output = get_result("Image");
      output.share_data(input);
      return;
    }

    if (get_is_image_crop()) {
      execute_image_crop();
    }
    else {
      execute_alpha_crop();
    }
  }

  /* Crop by replacing areas outside of the cropping bounds with zero alpha. The output have the
   * same domain as the input image. */
  void execute_alpha_crop()
  {
    if (this->context().use_gpu()) {
      this->execute_alpha_crop_gpu();
    }
    else {
      this->execute_alpha_crop_cpu();
    }
  }

  void execute_alpha_crop_gpu()
  {
    GPUShader *shader = context().get_shader("compositor_alpha_crop");
    GPU_shader_bind(shader);

    int2 lower_bound, upper_bound;
    compute_cropping_bounds(lower_bound, upper_bound);
    GPU_shader_uniform_2iv(shader, "lower_bound", lower_bound);
    GPU_shader_uniform_2iv(shader, "upper_bound", upper_bound);

    const Result &input_image = get_input("Image");
    input_image.bind_as_texture(shader, "input_tx");

    const Domain domain = compute_domain();

    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    input_image.unbind_as_texture();
    output_image.unbind_as_image();
    GPU_shader_unbind();
  }

  void execute_alpha_crop_cpu()
  {
    int2 lower_bound, upper_bound;
    compute_cropping_bounds(lower_bound, upper_bound);

    const Result &input = get_input("Image");

    const Domain domain = compute_domain();
    Result &output = get_result("Image");
    output.allocate_texture(domain);

    parallel_for(domain.size, [&](const int2 texel) {
      /* The lower bound is inclusive and upper bound is exclusive. */
      bool is_inside = texel.x >= lower_bound.x && texel.y >= lower_bound.y &&
                       texel.x < upper_bound.x && texel.y < upper_bound.y;
      /* Write the pixel color if it is inside the cropping region, otherwise, write zero. */
      float4 color = is_inside ? input.load_pixel<float4>(texel) : float4(0.0f);
      output.store_pixel(texel, color);
    });
  }

  /* Crop the image into a new size that matches the cropping bounds. */
  void execute_image_crop()
  {
    if (this->context().use_gpu()) {
      this->execute_image_crop_gpu();
    }
    else {
      this->execute_image_crop_cpu();
    }
  }

  void execute_image_crop_gpu()
  {
    int2 lower_bound, upper_bound;
    compute_cropping_bounds(lower_bound, upper_bound);

    /* The image is cropped into nothing, so just return a single zero value. */
    if (lower_bound.x == upper_bound.x || lower_bound.y == upper_bound.y) {
      Result &result = get_result("Image");
      result.allocate_invalid();
      return;
    }

    GPUShader *shader = context().get_shader("compositor_image_crop");
    GPU_shader_bind(shader);

    GPU_shader_uniform_2iv(shader, "lower_bound", lower_bound);

    const Result &input_image = get_input("Image");
    input_image.bind_as_texture(shader, "input_tx");

    const int2 size = upper_bound - lower_bound;

    Result &output_image = get_result("Image");
    output_image.allocate_texture(Domain(size, compute_domain().transformation));
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, size);

    input_image.unbind_as_texture();
    output_image.unbind_as_image();
    GPU_shader_unbind();
  }

  void execute_image_crop_cpu()
  {
    int2 lower_bound, upper_bound;
    compute_cropping_bounds(lower_bound, upper_bound);

    /* The image is cropped into nothing, so just return a single zero value. */
    if (lower_bound.x == upper_bound.x || lower_bound.y == upper_bound.y) {
      Result &result = get_result("Image");
      result.allocate_invalid();
      return;
    }

    const Result &input = get_input("Image");

    const int2 size = upper_bound - lower_bound;
    Result &output = get_result("Image");
    output.allocate_texture(Domain(size, compute_domain().transformation));

    parallel_for(size, [&](const int2 texel) {
      output.store_pixel(texel, input.load_pixel<float4>(texel + lower_bound));
    });
  }

  /* If true, the image should actually be cropped into a new size. Otherwise, if false, the region
   * outside of the cropping bounds will be set to a zero alpha value. */
  bool get_is_image_crop()
  {
    return bnode().custom1;
  }

  bool get_is_relative()
  {
    return bnode().custom2;
  }

  /* Returns true if the operation does nothing and the input can be passed through. */
  bool is_identity()
  {
    const Result &input = get_input("Image");
    /* Single value inputs can't be cropped and are returned as is. */
    if (input.is_single_value()) {
      return true;
    }

    int2 lower_bound, upper_bound;
    compute_cropping_bounds(lower_bound, upper_bound);
    const int2 input_size = input.domain().size;
    /* The cropping bounds cover the whole image, so no cropping happens. */
    if (lower_bound == int2(0) && upper_bound == input_size) {
      return true;
    }

    return false;
  }

  void compute_cropping_bounds(int2 &lower_bound, int2 &upper_bound)
  {
    const NodeTwoXYs &node_two_xys = node_storage(bnode());
    const int2 input_size = get_input("Image").domain().size;

    if (get_is_relative()) {
      /* The cropping bounds are relative to the image size. The factors are in the [0, 1] range,
       * so it is guaranteed that they won't go over the input image size. */
      lower_bound.x = input_size.x * node_two_xys.fac_x1;
      lower_bound.y = input_size.y * node_two_xys.fac_y2;
      upper_bound.x = input_size.x * node_two_xys.fac_x2;
      upper_bound.y = input_size.y * node_two_xys.fac_y1;
    }
    else {
      /* Make sure the bounds don't go over the input image size. */
      lower_bound.x = min_ii(node_two_xys.x1, input_size.x);
      lower_bound.y = min_ii(node_two_xys.y2, input_size.y);
      upper_bound.x = min_ii(node_two_xys.x2, input_size.x);
      upper_bound.y = min_ii(node_two_xys.y1, input_size.y);
    }

    /* Make sure upper bound is actually higher than the lower bound. */
    lower_bound.x = min_ii(lower_bound.x, upper_bound.x);
    lower_bound.y = min_ii(lower_bound.y, upper_bound.y);
    upper_bound.x = max_ii(lower_bound.x, upper_bound.x);
    upper_bound.y = max_ii(lower_bound.y, upper_bound.y);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new CropOperation(context, node);
}

}  // namespace blender::nodes::node_composite_crop_cc

void register_node_type_cmp_crop()
{
  namespace file_ns = blender::nodes::node_composite_crop_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeCrop", CMP_NODE_CROP);
  ntype.ui_name = "Crop";
  ntype.ui_description =
      "Crops image to a smaller region, either making the cropped area transparent or resizing "
      "the image";
  ntype.enum_name_legacy = "CROP";
  ntype.nclass = NODE_CLASS_DISTORT;
  ntype.declare = file_ns::cmp_node_crop_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_crop;
  ntype.initfunc = file_ns::node_composit_init_crop;
  blender::bke::node_type_storage(
      ntype, "NodeTwoXYs", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(ntype);
}
