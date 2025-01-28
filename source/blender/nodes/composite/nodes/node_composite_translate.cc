/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_assert.h"
#include "BLI_math_matrix.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** Translate ******************** */

namespace blender::nodes::node_composite_translate_cc {

NODE_STORAGE_FUNCS(NodeTranslateData)

static void cmp_node_translate_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0)
      .compositor_realization_options(CompositorInputRealizationOptions::None);
  b.add_input<decl::Float>("X")
      .default_value(0.0f)
      .min(-10000.0f)
      .max(10000.0f)
      .compositor_expects_single_value();
  b.add_input<decl::Float>("Y")
      .default_value(0.0f)
      .min(-10000.0f)
      .max(10000.0f)
      .compositor_expects_single_value();
  b.add_output<decl::Color>("Image");
}

static void node_composit_init_translate(bNodeTree * /*ntree*/, bNode *node)
{
  NodeTranslateData *data = MEM_cnew<NodeTranslateData>(__func__);
  node->storage = data;
}

static void node_composit_buts_translate(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "interpolation", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
  uiItemR(layout, ptr, "use_relative", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
  uiItemR(layout, ptr, "wrap_axis", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
}

using namespace blender::compositor;

class TranslateOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    Result &input = this->get_input("Image");
    Result &output = this->get_result("Image");

    if (input.is_single_value()) {
      input.pass_through(output);
      return;
    }

    float x = this->get_input("X").get_single_value_default(0.0f);
    float y = this->get_input("Y").get_single_value_default(0.0f);
    if (this->get_use_relative()) {
      x *= input.domain().size.x;
      y *= input.domain().size.y;
    }

    const float2 translation = float2(x, y);

    if (this->get_wrap_x() || this->get_wrap_y()) {
      this->execute_wrapped(translation);
    }
    else {
      input.pass_through(output);
      output.transform(math::from_location<float3x3>(translation));
      output.get_realization_options().interpolation = this->get_interpolation();
    }
  }

  void execute_wrapped(const float2 &translation)
  {
    BLI_assert(this->get_wrap_x() || this->get_wrap_y());

    /* Get the translation components that wrap. */
    const float2 wrapped_translation = float2(this->get_wrap_x() ? translation.x : 0.0f,
                                              this->get_wrap_y() ? translation.y : 0.0f);
    if (this->context().use_gpu()) {
      this->execute_wrapped_gpu(wrapped_translation);
    }
    else {
      this->execute_wrapped_cpu(wrapped_translation);
    }

    /* If we are wrapping on both sides, then there is nothing left to do. Otherwise, we will have
     * to transform the output by the translation components that do not wrap. While also setting
     * up the appropriate interpolation. */
    if (this->get_wrap_x() && this->get_wrap_y()) {
      return;
    }

    /* Get the translation components that do not wrap. */
    const float2 non_wrapped_translation = float2(this->get_wrap_x() ? 0.0f : translation.x,
                                                  this->get_wrap_y() ? 0.0f : translation.y);
    Result &output = this->get_result("Image");
    output.transform(math::from_location<float3x3>(non_wrapped_translation));
    output.get_realization_options().interpolation = this->get_interpolation();
  }

  void execute_wrapped_gpu(const float2 &translation)
  {
    const Result &input = this->get_input("Image");
    Result &output = this->get_result("Image");

    const Interpolation interpolation = this->get_interpolation();
    GPUShader *shader = this->context().get_shader(interpolation == Interpolation::Bicubic ?
                                                       "compositor_translate_wrapped_bicubic" :
                                                       "compositor_translate_wrapped");
    GPU_shader_bind(shader);

    GPU_shader_uniform_2fv(shader, "translation", translation);

    /* The texture sampler should use bilinear interpolation for both the bilinear and bicubic
     * cases, as the logic used by the bicubic realization shader expects textures to use bilinear
     * interpolation. */
    const bool use_bilinear = ELEM(interpolation, Interpolation::Bilinear, Interpolation::Bicubic);
    GPU_texture_filter_mode(input, use_bilinear);
    GPU_texture_extend_mode(input, GPU_SAMPLER_EXTEND_MODE_REPEAT);

    input.bind_as_texture(shader, "input_tx");

    output.allocate_texture(input.domain());
    output.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, input.domain().size);

    input.unbind_as_texture();
    output.unbind_as_image();
    GPU_shader_unbind();
  }

  void execute_wrapped_cpu(const float2 &translation)
  {
    const Result &input = this->get_input("Image");
    Result &output = this->get_result("Image");
    output.allocate_texture(input.domain());

    const bool wrap_x = this->get_wrap_x();
    const bool wrap_y = this->get_wrap_y();
    const Interpolation interpolation = this->get_interpolation();

    const int2 size = input.domain().size;
    parallel_for(size, [&](const int2 texel) {
      float2 translated_coordinates = (float2(texel) + float2(0.5f) - translation) / float2(size);

      float4 sample;
      switch (interpolation) {
        case Interpolation::Nearest:
          sample = input.sample_nearest_wrap(translated_coordinates, wrap_x, wrap_y);
          break;
        case Interpolation::Bilinear:
          sample = input.sample_bilinear_wrap(translated_coordinates, wrap_x, wrap_y);
          break;
        case Interpolation::Bicubic:
          sample = input.sample_cubic_wrap(translated_coordinates, wrap_x, wrap_y);
          break;
      }
      output.store_pixel(texel, sample);
    });
  }

  Interpolation get_interpolation()
  {
    switch (node_storage(bnode()).interpolation) {
      case CMP_NODE_INTERPOLATION_NEAREST:
        return Interpolation::Nearest;
      case CMP_NODE_INTERPOLATION_BILINEAR:
        return Interpolation::Bilinear;
      case CMP_NODE_INTERPOLATION_BICUBIC:
        return Interpolation::Bicubic;
    }

    BLI_assert_unreachable();
    return Interpolation::Nearest;
  }

  bool get_use_relative()
  {
    return node_storage(bnode()).relative;
  }

  bool get_wrap_x()
  {
    return ELEM(node_storage(bnode()).wrap_axis, CMP_NODE_WRAP_X, CMP_NODE_WRAP_XY);
  }

  bool get_wrap_y()
  {
    return ELEM(node_storage(bnode()).wrap_axis, CMP_NODE_WRAP_Y, CMP_NODE_WRAP_XY);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new TranslateOperation(context, node);
}

}  // namespace blender::nodes::node_composite_translate_cc

void register_node_type_cmp_translate()
{
  namespace file_ns = blender::nodes::node_composite_translate_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeTranslate", CMP_NODE_TRANSLATE);
  ntype.ui_name = "Translate";
  ntype.ui_description = "Offset an image";
  ntype.enum_name_legacy = "TRANSLATE";
  ntype.nclass = NODE_CLASS_DISTORT;
  ntype.declare = file_ns::cmp_node_translate_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_translate;
  ntype.initfunc = file_ns::node_composit_init_translate;
  blender::bke::node_type_storage(
      &ntype, "NodeTranslateData", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(&ntype);
}
