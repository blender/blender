/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include <cmath>

#include "BLI_math_base.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_shader.hh"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** SCALAR MATH ******************** */

namespace blender::nodes::node_composite_ellipsemask_cc {

NODE_STORAGE_FUNCS(NodeEllipseMask)

static void cmp_node_ellipsemask_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Mask")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .compositor_domain_priority(0);
  b.add_input<decl::Float>("Value")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .compositor_domain_priority(1);
  b.add_output<decl::Float>("Mask");
}

static void node_composit_init_ellipsemask(bNodeTree * /*ntree*/, bNode *node)
{
  NodeEllipseMask *data = MEM_cnew<NodeEllipseMask>(__func__);
  data->x = 0.5;
  data->y = 0.5;
  data->width = 0.2;
  data->height = 0.1;
  data->rotation = 0.0;
  node->storage = data;
}

static void node_composit_buts_ellipsemask(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *row;
  row = uiLayoutRow(layout, true);
  uiItemR(row, ptr, "x", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
  uiItemR(row, ptr, "y", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
  row = uiLayoutRow(layout, true);
  uiItemR(row,
          ptr,
          "mask_width",
          UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER,
          std::nullopt,
          ICON_NONE);
  uiItemR(row,
          ptr,
          "mask_height",
          UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER,
          std::nullopt,
          ICON_NONE);

  uiItemR(layout, ptr, "rotation", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
  uiItemR(layout, ptr, "mask_type", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
}

using namespace blender::compositor;

template<CMPNodeMaskType MaskType>
static void ellipse_mask(const Result &base_mask,
                         const Result &value_mask,
                         Result &output_mask,
                         const int2 &texel,
                         const int2 &domain_size,
                         const float2 &location,
                         const float2 &radius,
                         const float cos_angle,
                         const float sin_angle)
{
  float2 uv = float2(texel) / float2(domain_size - int2(1));
  uv -= location;
  uv.y *= float(domain_size.y) / float(domain_size.x);
  uv = float2x2(float2(cos_angle, -sin_angle), float2(sin_angle, cos_angle)) * uv;
  bool is_inside = math::length(uv / radius) < 1.0f;

  float base_mask_value = base_mask.load_pixel<float, true>(texel);
  float value = value_mask.load_pixel<float, true>(texel);

  float output_mask_value = 0.0f;
  if constexpr (MaskType == CMP_NODE_MASKTYPE_ADD) {
    output_mask_value = is_inside ? math::max(base_mask_value, value) : base_mask_value;
  }
  else if constexpr (MaskType == CMP_NODE_MASKTYPE_SUBTRACT) {
    output_mask_value = is_inside ? math::clamp(base_mask_value - value, 0.0f, 1.0f) :
                                    base_mask_value;
  }
  else if constexpr (MaskType == CMP_NODE_MASKTYPE_MULTIPLY) {
    output_mask_value = is_inside ? base_mask_value * value : 0.0f;
  }
  else if constexpr (MaskType == CMP_NODE_MASKTYPE_NOT) {
    output_mask_value = is_inside ? (base_mask_value > 0.0f ? 0.0f : value) : base_mask_value;
  }

  output_mask.store_pixel(texel, output_mask_value);
}

class EllipseMaskOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const Result &input_mask = get_input("Mask");
    Result &output_mask = get_result("Mask");
    /* For single value masks, the output will assume the compositing region, so ensure it is valid
     * first. See the compute_domain method. */
    if (input_mask.is_single_value() && !context().is_valid_compositing_region()) {
      output_mask.allocate_invalid();
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
    GPUShader *shader = context().get_shader(get_shader_name());
    GPU_shader_bind(shader);

    const Domain domain = compute_domain();

    GPU_shader_uniform_2iv(shader, "domain_size", domain.size);

    GPU_shader_uniform_2fv(shader, "location", get_location());
    GPU_shader_uniform_2fv(shader, "radius", get_size() / 2.0f);
    GPU_shader_uniform_1f(shader, "cos_angle", std::cos(get_angle()));
    GPU_shader_uniform_1f(shader, "sin_angle", std::sin(get_angle()));

    const Result &input_mask = get_input("Mask");
    input_mask.bind_as_texture(shader, "base_mask_tx");

    const Result &value = get_input("Value");
    value.bind_as_texture(shader, "mask_value_tx");

    Result &output_mask = get_result("Mask");
    output_mask.allocate_texture(domain);
    output_mask.bind_as_image(shader, "output_mask_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    input_mask.unbind_as_texture();
    value.unbind_as_texture();
    output_mask.unbind_as_image();
    GPU_shader_unbind();
  }

  const char *get_shader_name()
  {
    switch (get_mask_type()) {
      default:
      case CMP_NODE_MASKTYPE_ADD:
        return "compositor_ellipse_mask_add";
      case CMP_NODE_MASKTYPE_SUBTRACT:
        return "compositor_ellipse_mask_subtract";
      case CMP_NODE_MASKTYPE_MULTIPLY:
        return "compositor_ellipse_mask_multiply";
      case CMP_NODE_MASKTYPE_NOT:
        return "compositor_ellipse_mask_not";
    }
  }

  void execute_cpu()
  {
    const Result &base_mask = this->get_input("Mask");
    const Result &value_mask = this->get_input("Value");

    Result &output_mask = get_result("Mask");
    const Domain domain = this->compute_domain();
    output_mask.allocate_texture(domain);

    const int2 domain_size = domain.size;
    const float2 location = this->get_location();
    const float2 radius = this->get_size() / 2.0f;
    const float cos_angle = math::cos(this->get_angle());
    const float sin_angle = math::sin(this->get_angle());

    switch (this->get_mask_type()) {
      case CMP_NODE_MASKTYPE_ADD:
        parallel_for(domain_size, [&](const int2 texel) {
          ellipse_mask<CMP_NODE_MASKTYPE_ADD>(base_mask,
                                              value_mask,
                                              output_mask,
                                              texel,
                                              domain_size,
                                              location,
                                              radius,
                                              cos_angle,
                                              sin_angle);
        });
        break;
      case CMP_NODE_MASKTYPE_SUBTRACT:
        parallel_for(domain_size, [&](const int2 texel) {
          ellipse_mask<CMP_NODE_MASKTYPE_SUBTRACT>(base_mask,
                                                   value_mask,
                                                   output_mask,
                                                   texel,
                                                   domain_size,
                                                   location,
                                                   radius,
                                                   cos_angle,
                                                   sin_angle);
        });
        break;
      case CMP_NODE_MASKTYPE_MULTIPLY:
        parallel_for(domain_size, [&](const int2 texel) {
          ellipse_mask<CMP_NODE_MASKTYPE_MULTIPLY>(base_mask,
                                                   value_mask,
                                                   output_mask,
                                                   texel,
                                                   domain_size,
                                                   location,
                                                   radius,
                                                   cos_angle,
                                                   sin_angle);
        });
        break;
      case CMP_NODE_MASKTYPE_NOT:
        parallel_for(domain_size, [&](const int2 texel) {
          ellipse_mask<CMP_NODE_MASKTYPE_NOT>(base_mask,
                                              value_mask,
                                              output_mask,
                                              texel,
                                              domain_size,
                                              location,
                                              radius,
                                              cos_angle,
                                              sin_angle);
        });
        break;
    }
  }

  Domain compute_domain() override
  {
    if (get_input("Mask").is_single_value()) {
      return Domain(context().get_compositing_region_size());
    }
    return get_input("Mask").domain();
  }

  CMPNodeMaskType get_mask_type()
  {
    return static_cast<CMPNodeMaskType>(bnode().custom1);
  }

  float2 get_location()
  {
    return float2(node_storage(bnode()).x, node_storage(bnode()).y);
  }

  float2 get_size()
  {
    return float2(node_storage(bnode()).width, node_storage(bnode()).height);
  }

  float get_angle()
  {
    return node_storage(bnode()).rotation;
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new EllipseMaskOperation(context, node);
}

}  // namespace blender::nodes::node_composite_ellipsemask_cc

void register_node_type_cmp_ellipsemask()
{
  namespace file_ns = blender::nodes::node_composite_ellipsemask_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeEllipseMask", CMP_NODE_MASK_ELLIPSE);
  ntype.ui_name = "Ellipse Mask";
  ntype.ui_description =
      "Create elliptical mask suitable for use as a simple matte or vignette mask";
  ntype.enum_name_legacy = "ELLIPSEMASK";
  ntype.nclass = NODE_CLASS_MATTE;
  ntype.declare = file_ns::cmp_node_ellipsemask_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_ellipsemask;
  blender::bke::node_type_size(&ntype, 260, 110, 320);
  ntype.initfunc = file_ns::node_composit_init_ellipsemask;
  blender::bke::node_type_storage(
      &ntype, "NodeEllipseMask", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(&ntype);
}
