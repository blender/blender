/* SPDX-FileCopyrightText: 2006 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_assert.h"
#include "BLI_math_base.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector_types.hh"

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

/* **************** Scale  ******************** */

namespace blender::nodes::node_composite_scale_cc {

static void cmp_node_scale_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_input<decl::Float>("X")
      .default_value(1.0f)
      .min(0.0001f)
      .max(CMP_SCALE_MAX)
      .compositor_expects_single_value();
  b.add_input<decl::Float>("Y")
      .default_value(1.0f)
      .min(0.0001f)
      .max(CMP_SCALE_MAX)
      .compositor_expects_single_value();
  b.add_output<decl::Color>("Image");
}

static void node_composite_update_scale(bNodeTree *ntree, bNode *node)
{
  bNodeSocket *sock;
  bool use_xy_scale = ELEM(node->custom1, CMP_NODE_SCALE_RELATIVE, CMP_NODE_SCALE_ABSOLUTE);

  /* Only show X/Y scale factor inputs for modes using them! */
  for (sock = (bNodeSocket *)node->inputs.first; sock; sock = sock->next) {
    if (STR_ELEM(sock->name, "X", "Y")) {
      bke::nodeSetSocketAvailability(ntree, sock, use_xy_scale);
    }
  }
}

static void node_composit_buts_scale(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "space", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);

  if (RNA_enum_get(ptr, "space") == CMP_NODE_SCALE_RENDER_SIZE) {
    uiLayout *row;
    uiItemR(layout,
            ptr,
            "frame_method",
            UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_EXPAND,
            nullptr,
            ICON_NONE);
    row = uiLayoutRow(layout, true);
    uiItemR(row, ptr, "offset_x", UI_ITEM_R_SPLIT_EMPTY_NAME, "X", ICON_NONE);
    uiItemR(row, ptr, "offset_y", UI_ITEM_R_SPLIT_EMPTY_NAME, "Y", ICON_NONE);
  }
}

using namespace blender::realtime_compositor;

class ScaleOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    Result &input = get_input("Image");
    Result &result = get_result("Image");
    input.pass_through(result);

    const float3x3 transformation = math::from_loc_rot_scale<float3x3>(
        get_translation(), math::AngleRadian(0.0f), get_scale());

    result.transform(transformation);
  }

  float2 get_scale()
  {
    switch (get_scale_method()) {
      case CMP_NODE_SCALE_RELATIVE:
        return get_scale_relative();
      case CMP_NODE_SCALE_ABSOLUTE:
        return get_scale_absolute();
      case CMP_NODE_SCALE_RENDER_PERCENT:
        return get_scale_render_percent();
      case CMP_NODE_SCALE_RENDER_SIZE:
        return get_scale_render_size();
      default:
        BLI_assert_unreachable();
        return float2(1.0f);
    }
  }

  /* Scale by the input factors. */
  float2 get_scale_relative()
  {
    return float2(get_input("X").get_float_value_default(1.0f),
                  get_input("Y").get_float_value_default(1.0f));
  }

  /* Scale such that the new size matches the input absolute size. */
  float2 get_scale_absolute()
  {
    const float2 input_size = float2(get_input("Image").domain().size);
    const float2 absolute_size = float2(get_input("X").get_float_value_default(1.0f),
                                        get_input("Y").get_float_value_default(1.0f));
    return absolute_size / input_size;
  }

  /* Scale by the render resolution percentage. */
  float2 get_scale_render_percent()
  {
    return float2(context().get_render_percentage());
  }

  float2 get_scale_render_size()
  {
    switch (get_scale_render_size_method()) {
      case CMP_NODE_SCALE_RENDER_SIZE_STRETCH:
        return get_scale_render_size_stretch();
      case CMP_NODE_SCALE_RENDER_SIZE_FIT:
        return get_scale_render_size_fit();
      case CMP_NODE_SCALE_RENDER_SIZE_CROP:
        return get_scale_render_size_crop();
      default:
        BLI_assert_unreachable();
        return float2(1.0f);
    }
  }

  /* Scale such that the new size matches the render size. Since the input is freely scaled, it is
   * potentially stretched, hence the name. */
  float2 get_scale_render_size_stretch()
  {
    const float2 input_size = float2(get_input("Image").domain().size);
    const float2 render_size = float2(context().get_compositing_region_size());
    return render_size / input_size;
  }

  /* Scale such that the dimension with the smaller scaling factor matches that of the render size
   * while maintaining the input's aspect ratio. Since the other dimension is guaranteed not to
   * exceed the render size region due to its larger scaling factor, the image is said to be fit
   * inside that region, hence the name. */
  float2 get_scale_render_size_fit()
  {
    const float2 input_size = float2(get_input("Image").domain().size);
    const float2 render_size = float2(context().get_compositing_region_size());
    const float2 scale = render_size / input_size;
    return float2(math::min(scale.x, scale.y));
  }

  /* Scale such that the dimension with the larger scaling factor matches that of the render size
   * while maintaining the input's aspect ratio. Since the other dimension is guaranteed to exceed
   * the render size region due to its lower scaling factor, the image will be cropped inside that
   * region, hence the name. */
  float2 get_scale_render_size_crop()
  {
    const float2 input_size = float2(get_input("Image").domain().size);
    const float2 render_size = float2(context().get_compositing_region_size());
    const float2 scale = render_size / input_size;
    return float2(math::max(scale.x, scale.y));
  }

  float2 get_translation()
  {
    /* Only the render size option supports offset translation. */
    if (get_scale_method() != CMP_NODE_SCALE_RENDER_SIZE) {
      return float2(0.0f);
    }

    /* Translate by the offset factor relative to the new size. */
    const float2 input_size = float2(get_input("Image").domain().size);
    return get_offset() * input_size * get_scale();
  }

  CMPNodeScaleMethod get_scale_method()
  {
    return (CMPNodeScaleMethod)bnode().custom1;
  }

  CMPNodeScaleRenderSizeMethod get_scale_render_size_method()
  {
    return (CMPNodeScaleRenderSizeMethod)bnode().custom2;
  }

  float2 get_offset()
  {
    return float2(bnode().custom3, bnode().custom4);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new ScaleOperation(context, node);
}

}  // namespace blender::nodes::node_composite_scale_cc

void register_node_type_cmp_scale()
{
  namespace file_ns = blender::nodes::node_composite_scale_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_SCALE, "Scale", NODE_CLASS_DISTORT);
  ntype.declare = file_ns::cmp_node_scale_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_scale;
  ntype.updatefunc = file_ns::node_composite_update_scale;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}
