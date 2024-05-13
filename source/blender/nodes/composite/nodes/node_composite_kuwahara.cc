/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_base.hh"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "COM_algorithm_summed_area_table.hh"
#include "COM_algorithm_symmetric_separable_blur.hh"

#include "node_composite_util.hh"

/* **************** Kuwahara ******************** */

namespace blender::nodes::node_composite_kuwahara_cc {

NODE_STORAGE_FUNCS(NodeKuwaharaData)

static void cmp_node_kuwahara_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_input<decl::Float>("Size").default_value(6.0f).compositor_domain_priority(1);
  b.add_output<decl::Color>("Image");
}

static void node_composit_init_kuwahara(bNodeTree * /*ntree*/, bNode *node)
{
  NodeKuwaharaData *data = MEM_cnew<NodeKuwaharaData>(__func__);
  node->storage = data;

  /* Set defaults. */
  data->uniformity = 4;
  data->eccentricity = 1.0;
  data->sharpness = 0.5;
}

static void node_composit_buts_kuwahara(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, false);

  uiItemR(col, ptr, "variation", UI_ITEM_NONE, nullptr, ICON_NONE);

  const int variation = RNA_enum_get(ptr, "variation");

  if (variation == CMP_NODE_KUWAHARA_CLASSIC) {
    uiItemR(col, ptr, "use_high_precision", UI_ITEM_NONE, nullptr, ICON_NONE);
  }
  else if (variation == CMP_NODE_KUWAHARA_ANISOTROPIC) {
    uiItemR(col, ptr, "uniformity", UI_ITEM_NONE, nullptr, ICON_NONE);
    uiItemR(col, ptr, "sharpness", UI_ITEM_NONE, nullptr, ICON_NONE);
    uiItemR(col, ptr, "eccentricity", UI_ITEM_NONE, nullptr, ICON_NONE);
  }
}

using namespace blender::realtime_compositor;

class ConvertKuwaharaOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    if (get_input("Image").is_single_value()) {
      get_input("Image").pass_through(get_result("Image"));
      return;
    }

    if (node_storage(bnode()).variation == CMP_NODE_KUWAHARA_ANISOTROPIC) {
      execute_anisotropic();
    }
    else {
      execute_classic();
    }
  }

  void execute_classic()
  {
    /* For high radii, we accelerate the filter using a summed area table, making the filter
     * execute in constant time as opposed to having quadratic complexity. Except if high precision
     * is enabled, since summed area tables are less precise. */
    Result &size_input = get_input("Size");
    if (!node_storage(bnode()).high_precision &&
        (size_input.is_texture() || size_input.get_float_value() > 5.0f))
    {
      execute_classic_summed_area_table();
      return;
    }

    GPUShader *shader = context().get_shader(get_classic_convolution_shader_name());
    GPU_shader_bind(shader);

    const Result &input_image = get_input("Image");
    input_image.bind_as_texture(shader, "input_tx");

    if (size_input.is_single_value()) {
      GPU_shader_uniform_1i(shader, "size", int(size_input.get_float_value()));
    }
    else {
      size_input.bind_as_texture(shader, "size_tx");
    }

    const Domain domain = compute_domain();
    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    input_image.unbind_as_texture();
    output_image.unbind_as_image();
    GPU_shader_unbind();
  }

  void execute_classic_summed_area_table()
  {
    Result table = context().create_temporary_result(ResultType::Color, ResultPrecision::Full);
    summed_area_table(context(), get_input("Image"), table);

    Result squared_table = context().create_temporary_result(ResultType::Color,
                                                             ResultPrecision::Full);
    summed_area_table(
        context(), get_input("Image"), squared_table, SummedAreaTableOperation::Square);

    GPUShader *shader = context().get_shader(get_classic_summed_area_table_shader_name());
    GPU_shader_bind(shader);

    Result &size_input = get_input("Size");
    if (size_input.is_single_value()) {
      GPU_shader_uniform_1i(shader, "size", int(size_input.get_float_value()));
    }
    else {
      size_input.bind_as_texture(shader, "size_tx");
    }

    table.bind_as_texture(shader, "table_tx");
    squared_table.bind_as_texture(shader, "squared_table_tx");

    const Domain domain = compute_domain();
    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    table.unbind_as_texture();
    squared_table.unbind_as_texture();
    output_image.unbind_as_image();
    GPU_shader_unbind();

    table.release();
    squared_table.release();
  }

  /* An implementation of the Anisotropic Kuwahara filter described in the paper:
   *
   *   Kyprianidis, Jan Eric, Henry Kang, and Jurgen Dollner. "Image and video abstraction by
   *   anisotropic Kuwahara filtering." 2009.
   */
  void execute_anisotropic()
  {
    Result structure_tensor = compute_structure_tensor();
    Result smoothed_structure_tensor = context().create_temporary_result(ResultType::Color);
    symmetric_separable_blur(context(),
                             structure_tensor,
                             smoothed_structure_tensor,
                             float2(node_storage(bnode()).uniformity));
    structure_tensor.release();

    GPUShader *shader = context().get_shader(get_anisotropic_shader_name());
    GPU_shader_bind(shader);

    GPU_shader_uniform_1f(shader, "eccentricity", get_eccentricity());
    GPU_shader_uniform_1f(shader, "sharpness", get_sharpness());

    Result &input = get_input("Image");
    input.bind_as_texture(shader, "input_tx");

    Result &size_input = get_input("Size");
    if (size_input.is_single_value()) {
      GPU_shader_uniform_1f(shader, "size", size_input.get_float_value());
    }
    else {
      size_input.bind_as_texture(shader, "size_tx");
    }

    smoothed_structure_tensor.bind_as_texture(shader, "structure_tensor_tx");

    const Domain domain = compute_domain();
    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    input.unbind_as_texture();
    smoothed_structure_tensor.unbind_as_texture();
    output_image.unbind_as_image();
    GPU_shader_unbind();

    smoothed_structure_tensor.release();
  }

  Result compute_structure_tensor()
  {
    GPUShader *shader = context().get_shader(
        "compositor_kuwahara_anisotropic_compute_structure_tensor");
    GPU_shader_bind(shader);

    Result &input = get_input("Image");
    input.bind_as_texture(shader, "input_tx");

    const Domain domain = compute_domain();
    Result structure_tensor = context().create_temporary_result(ResultType::Color);
    structure_tensor.allocate_texture(domain);
    structure_tensor.bind_as_image(shader, "structure_tensor_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    input.unbind_as_texture();
    structure_tensor.unbind_as_image();
    GPU_shader_unbind();

    return structure_tensor;
  }

  const char *get_classic_convolution_shader_name()
  {
    if (is_constant_size()) {
      return "compositor_kuwahara_classic_convolution_constant_size";
    }
    return "compositor_kuwahara_classic_convolution_variable_size";
  }

  const char *get_classic_summed_area_table_shader_name()
  {
    if (is_constant_size()) {
      return "compositor_kuwahara_classic_summed_area_table_constant_size";
    }
    return "compositor_kuwahara_classic_summed_area_table_variable_size";
  }

  const char *get_anisotropic_shader_name()
  {
    if (is_constant_size()) {
      return "compositor_kuwahara_anisotropic_constant_size";
    }
    return "compositor_kuwahara_anisotropic_variable_size";
  }

  bool is_constant_size()
  {
    return get_input("Size").is_single_value();
  }

  /* The sharpness controls the sharpness of the transitions between the kuwahara sectors, which
   * is controlled by the weighting function pow(standard_deviation, -sharpness) as can be seen
   * in the shader. The transition is completely smooth when the sharpness is zero and completely
   * sharp when it is infinity. But realistically, the sharpness doesn't change much beyond the
   * value of 16 due to its exponential nature, so we just assume a maximum sharpness of 16.
   *
   * The stored sharpness is in the range [0, 1], so we multiply by 16 to get it in the range
   * [0, 16], however, we also square it before multiplication to slow down the rate of change
   * near zero to counter its exponential nature for more intuitive user control. */
  float get_sharpness()
  {
    const float sharpness_factor = node_storage(bnode()).sharpness;
    return sharpness_factor * sharpness_factor * 16.0f;
  }

  /* The eccentricity controls how much the image anisotropy affects the eccentricity of the
   * kuwahara sectors, which is controlled by the following factor that gets multiplied to the
   * radius to get the ellipse width and divides the radius to get the ellipse height:
   *
   *   (eccentricity + anisotropy) / eccentricity
   *
   * Since the anisotropy is in the [0, 1] range, the factor tends to 1 as the eccentricity tends
   * to infinity and tends to infinity when the eccentricity tends to zero. The stored
   * eccentricity is in the range [0, 2], we map that to the range [infinity, 0.5] by taking the
   * reciprocal, satisfying the aforementioned limits. The upper limit doubles the computed
   * default eccentricity, which users can use to enhance the directionality of the filter.
   * Instead of actual infinity, we just use an eccentricity of 1 / 0.01 since the result is very
   * similar to that of infinity. */
  float get_eccentricity()
  {
    return 1.0f / math::max(0.01f, node_storage(bnode()).eccentricity);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new ConvertKuwaharaOperation(context, node);
}

}  // namespace blender::nodes::node_composite_kuwahara_cc

void register_node_type_cmp_kuwahara()
{
  namespace file_ns = blender::nodes::node_composite_kuwahara_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_KUWAHARA, "Kuwahara", NODE_CLASS_OP_FILTER);
  ntype.declare = file_ns::cmp_node_kuwahara_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_kuwahara;
  ntype.initfunc = file_ns::node_composit_init_kuwahara;
  blender::bke::node_type_storage(
      &ntype, "NodeKuwaharaData", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::nodeRegisterType(&ntype);
}
