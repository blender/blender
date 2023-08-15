/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_base.h"
#include "BLI_math_vector_types.hh"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_shader.h"
#include "GPU_texture.h"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* Distortion can't be exactly -1.0 as it will cause infinite pincushion distortion. */
#define MINIMUM_DISTORTION -0.999f
/* Arbitrary scaling factor for the dispersion input in projector distortion mode. */
#define PROJECTOR_DISPERSION_SCALE 5.0f
/* Arbitrary scaling factor for the dispersion input in screen distortion mode. */
#define SCREEN_DISPERSION_SCALE 4.0f
/* Arbitrary scaling factor for the distortion input. */
#define DISTORTION_SCALE 4.0f

namespace blender::nodes::node_composite_lensdist_cc {

NODE_STORAGE_FUNCS(NodeLensDist)

static void cmp_node_lensdist_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_input<decl::Float>("Distortion")
      .default_value(0.0f)
      .min(MINIMUM_DISTORTION)
      .max(1.0f)
      .compositor_expects_single_value();
  b.add_input<decl::Float>("Dispersion")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .compositor_expects_single_value();
  b.add_output<decl::Color>("Image");
}

static void node_composit_init_lensdist(bNodeTree * /*ntree*/, bNode *node)
{
  NodeLensDist *nld = MEM_cnew<NodeLensDist>(__func__);
  nld->jit = nld->proj = nld->fit = 0;
  node->storage = nld;
}

static void node_composit_buts_lensdist(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "use_projector", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);

  col = uiLayoutColumn(col, false);
  uiLayoutSetActive(col, RNA_boolean_get(ptr, "use_projector") == false);
  uiItemR(col, ptr, "use_jitter", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(col, ptr, "use_fit", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class LensDistortionOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    if (is_identity()) {
      get_input("Image").pass_through(get_result("Image"));
      return;
    }

    if (get_is_projector()) {
      execute_projector_distortion();
    }
    else {
      execute_screen_distortion();
    }
  }

  void execute_projector_distortion()
  {
    GPUShader *shader = shader_manager().get("compositor_projector_lens_distortion");
    GPU_shader_bind(shader);

    const Result &input_image = get_input("Image");
    input_image.bind_as_texture(shader, "input_tx");

    GPU_texture_filter_mode(input_image.texture(), true);
    GPU_texture_extend_mode(input_image.texture(), GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER);

    const Domain domain = compute_domain();

    const float dispersion = (get_dispersion() * PROJECTOR_DISPERSION_SCALE) / domain.size.x;
    GPU_shader_uniform_1f(shader, "dispersion", dispersion);

    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    input_image.unbind_as_texture();
    output_image.unbind_as_image();
    GPU_shader_unbind();
  }

  void execute_screen_distortion()
  {
    GPUShader *shader = shader_manager().get(get_screen_distortion_shader());
    GPU_shader_bind(shader);

    const Result &input_image = get_input("Image");
    input_image.bind_as_texture(shader, "input_tx");

    GPU_texture_filter_mode(input_image.texture(), true);
    GPU_texture_extend_mode(input_image.texture(), GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER);

    const Domain domain = compute_domain();

    const float3 chromatic_distortion = compute_chromatic_distortion();
    GPU_shader_uniform_3fv(shader, "chromatic_distortion", chromatic_distortion);

    GPU_shader_uniform_1f(shader, "scale", compute_scale());

    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    input_image.unbind_as_texture();
    output_image.unbind_as_image();
    GPU_shader_unbind();
  }

  const char *get_screen_distortion_shader()
  {
    if (get_is_jitter()) {
      return "compositor_screen_lens_distortion_jitter";
    }
    return "compositor_screen_lens_distortion";
  }

  float get_distortion()
  {
    const Result &input = get_input("Distortion");
    return clamp_f(input.get_float_value_default(0.0f), MINIMUM_DISTORTION, 1.0f);
  }

  float get_dispersion()
  {
    const Result &input = get_input("Dispersion");
    return clamp_f(input.get_float_value_default(0.0f), 0.0f, 1.0f);
  }

  /* Get the distortion amount for each channel. The green channel has a distortion amount that
   * matches that specified in the node inputs, while the red and blue channels have higher and
   * lower distortion amounts respectively based on the dispersion value. */
  float3 compute_chromatic_distortion()
  {
    const float green_distortion = get_distortion();
    const float dispersion = get_dispersion() / SCREEN_DISPERSION_SCALE;
    const float red_distortion = clamp_f(green_distortion + dispersion, MINIMUM_DISTORTION, 1.0f);
    const float blue_distortion = clamp_f(green_distortion - dispersion, MINIMUM_DISTORTION, 1.0f);
    return float3(red_distortion, green_distortion, blue_distortion) * DISTORTION_SCALE;
  }

  /* The distortion model will distort the image in such a way that the result will no longer
   * fit the domain of the original image, so we scale the image to account for that. If get_is_fit
   * is false, then the scaling factor will be such that the furthest pixels horizontally and
   * vertically are at the boundary of the image. Otherwise, if get_is_fit is true, the scaling
   * factor will be such that the furthest pixels diagonally are at the corner of the image. */
  float compute_scale()
  {
    const float3 distortion = compute_chromatic_distortion() / DISTORTION_SCALE;
    const float maximum_distortion = max_fff(distortion[0], distortion[1], distortion[2]);

    if (get_is_fit() && (maximum_distortion > 0.0f)) {
      return 1.0f / (1.0f + 2.0f * maximum_distortion);
    }
    return 1.0f / (1.0f + maximum_distortion);
  }

  bool get_is_projector()
  {
    return node_storage(bnode()).proj;
  }

  bool get_is_jitter()
  {
    return node_storage(bnode()).jit;
  }

  bool get_is_fit()
  {
    return node_storage(bnode()).fit;
  }

  /* Returns true if the operation does nothing and the input can be passed through. */
  bool is_identity()
  {
    /* The input is a single value and the operation does nothing. */
    if (get_input("Image").is_single_value()) {
      return true;
    }

    /* Projector have zero dispersion and does nothing. */
    if (get_is_projector() && get_dispersion() == 0.0f) {
      return true;
    }

    /* Both distortion and dispersion are zero and the operation does nothing. */
    if (get_distortion() == 0.0f && get_dispersion() == 0.0f) {
      return true;
    }

    return false;
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new LensDistortionOperation(context, node);
}

}  // namespace blender::nodes::node_composite_lensdist_cc

void register_node_type_cmp_lensdist()
{
  namespace file_ns = blender::nodes::node_composite_lensdist_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_LENSDIST, "Lens Distortion", NODE_CLASS_DISTORT);
  ntype.declare = file_ns::cmp_node_lensdist_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_lensdist;
  ntype.initfunc = file_ns::node_composit_init_lensdist;
  node_type_storage(
      &ntype, "NodeLensDist", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}
