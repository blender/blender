/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_listbase.h"
#include "BLI_math_angle_types.hh"
#include "BLI_math_base.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_string.h"

#include "DNA_node_types.h"

#include "RNA_enum_types.hh"

#include "GPU_shader.hh"
#include "GPU_texture.hh"

#include "COM_domain.hh"
#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_scale_cc {

static const EnumPropertyItem type_items[] = {
    {CMP_NODE_SCALE_RELATIVE, "RELATIVE", 0, N_("Relative"), ""},
    {CMP_NODE_SCALE_ABSOLUTE, "ABSOLUTE", 0, N_("Absolute"), ""},
    {CMP_NODE_SCALE_RENDER_PERCENT, "SCENE_SIZE", 0, N_("Scene Size"), ""},
    {CMP_NODE_SCALE_RENDER_SIZE, "RENDER_SIZE", 0, N_("Render Size"), ""},
    {0, nullptr, 0, nullptr, nullptr},
};

/* Matches bgpic_camera_frame_items[]. */
static const EnumPropertyItem frame_type_items[] = {
    {CMP_NODE_SCALE_RENDER_SIZE_STRETCH, "STRETCH", 0, N_("Stretch"), ""},
    {CMP_NODE_SCALE_RENDER_SIZE_FIT, "FIT", 0, N_("Fit"), ""},
    {CMP_NODE_SCALE_RENDER_SIZE_CROP, "CROP", 0, N_("Crop"), ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static void cmp_node_scale_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .hide_value()
      .compositor_realization_mode(CompositorInputRealizationMode::None)
      .structure_type(StructureType::Dynamic);
  b.add_output<decl::Color>("Image").structure_type(StructureType::Dynamic).align_with_previous();

  b.add_input<decl::Menu>("Type").default_value(CMP_NODE_SCALE_RELATIVE).static_items(type_items);
  b.add_input<decl::Float>("X")
      .default_value(1.0f)
      .min(0.0001f)
      .max(CMP_SCALE_MAX)
      .structure_type(StructureType::Dynamic)
      .usage_by_menu("Type", {CMP_NODE_SCALE_RELATIVE, CMP_NODE_SCALE_ABSOLUTE});
  b.add_input<decl::Float>("Y")
      .default_value(1.0f)
      .min(0.0001f)
      .max(CMP_SCALE_MAX)
      .structure_type(StructureType::Dynamic)
      .usage_by_menu("Type", {CMP_NODE_SCALE_RELATIVE, CMP_NODE_SCALE_ABSOLUTE});
  b.add_input<decl::Menu>("Frame Type")
      .default_value(CMP_NODE_SCALE_RENDER_SIZE_STRETCH)
      .static_items(frame_type_items)
      .usage_by_menu("Type", CMP_NODE_SCALE_RENDER_SIZE)
      .optional_label()
      .description("How the image fits in the camera frame");

  PanelDeclarationBuilder &sampling_panel = b.add_panel("Sampling").default_closed(true);
  sampling_panel.add_input<decl::Menu>("Interpolation")
      .default_value(CMP_NODE_INTERPOLATION_BILINEAR)
      .static_items(rna_enum_node_compositor_interpolation_items)
      .optional_label()
      .description("Interpolation method");
  sampling_panel.add_input<decl::Menu>("Extension X")
      .default_value(CMP_NODE_EXTENSION_MODE_CLIP)
      .static_items(rna_enum_node_compositor_extension_items)
      .optional_label()
      .description("The extension mode applied to the X axis");
  sampling_panel.add_input<decl::Menu>("Extension Y")
      .default_value(CMP_NODE_EXTENSION_MODE_CLIP)
      .static_items(rna_enum_node_compositor_extension_items)
      .optional_label()
      .description("The extension mode applied to the Y axis");
}

static void node_composit_init_scale(bNodeTree * /*ntree*/, bNode *node)
{
  /* Unused, kept for forward compatibility. */
  NodeScaleData *data = MEM_callocN<NodeScaleData>(__func__);
  node->storage = data;
}

using namespace blender::compositor;

class ScaleOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    if (is_variable_size()) {
      execute_variable_size();
    }
    else {
      execute_constant_size();
    }
  }

  void execute_constant_size()
  {
    const float2 scale = this->get_scale();
    const float3x3 transformation = math::from_scale<float3x3>(scale);

    const Result &input = this->get_input("Image");
    Result &output = this->get_result("Image");
    output.share_data(input);
    output.transform(transformation);

    output.get_realization_options().interpolation = this->get_interpolation();
    output.get_realization_options().extension_x = this->get_extension_mode_x();
    output.get_realization_options().extension_y = this->get_extension_mode_y();
  }

  void execute_variable_size()
  {
    const Result &input = this->get_input("Image");
    if (input.is_single_value()) {
      Result &output = this->get_result("Image");
      output.share_data(input);
      return;
    }

    if (this->context().use_gpu()) {
      execute_variable_size_gpu();
    }
    else {
      execute_variable_size_cpu();
    }
  }

  void execute_variable_size_gpu()
  {
    gpu::Shader *shader = this->context().get_shader(this->get_shader_name());
    GPU_shader_bind(shader);

    Result &input = get_input("Image");
    /* The texture sampler should use bilinear interpolation for both the bilinear and bicubic
     * cases, as the logic used by the bicubic realization shader expects textures to use bilinear
     * interpolation. */
    const Interpolation interpolation = this->get_interpolation();
    const ExtensionMode extension_mode_x = this->get_extension_mode_x();
    const ExtensionMode extension_mode_y = this->get_extension_mode_y();

    /* For now the EWA sampling falls back to bicubic interpolation. */
    const bool use_bilinear = ELEM(interpolation, Interpolation::Bilinear, Interpolation::Bicubic);
    GPU_texture_filter_mode(input, use_bilinear);
    GPU_texture_extend_mode_x(input, map_extension_mode_to_extend_mode(extension_mode_x));
    GPU_texture_extend_mode_y(input, map_extension_mode_to_extend_mode(extension_mode_y));
    input.bind_as_texture(shader, "input_tx");

    Result &x_scale = get_input("X");
    x_scale.bind_as_texture(shader, "x_scale_tx");

    Result &y_scale = get_input("Y");
    y_scale.bind_as_texture(shader, "y_scale_tx");

    Result &output = get_result("Image");
    const Domain domain = compute_domain();
    output.allocate_texture(domain);
    output.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    input.unbind_as_texture();
    x_scale.unbind_as_texture();
    y_scale.unbind_as_texture();
    output.unbind_as_image();
    GPU_shader_unbind();
  }

  void execute_variable_size_cpu()
  {
    const Result &input = this->get_input("Image");
    const Result &x_scale = this->get_input("X");
    const Result &y_scale = this->get_input("Y");

    Result &output = this->get_result("Image");
    const Interpolation interpolation = this->get_interpolation();
    const ExtensionMode extension_mode_x = this->get_extension_mode_x();
    const ExtensionMode extension_mode_y = this->get_extension_mode_y();
    const Domain domain = compute_domain();
    const int2 size = domain.size;
    output.allocate_texture(domain);

    parallel_for(size, [&](const int2 texel) {
      float2 coordinates = (float2(texel) + float2(0.5f)) / float2(size);
      float2 center = float2(0.5f);

      float2 scale = float2(x_scale.load_pixel<float, true>(texel),
                            y_scale.load_pixel<float, true>(texel));
      float2 scaled_coordinates = center +
                                  (coordinates - center) / math::max(scale, float2(0.0001f));

      output.store_pixel(
          texel,
          input.sample<Color>(
              scaled_coordinates, interpolation, extension_mode_x, extension_mode_y));
    });
  }

  const char *get_shader_name() const
  {
    if (this->get_interpolation() == Interpolation::Bicubic) {
      return "compositor_scale_variable_bicubic";
    }
    return "compositor_scale_variable";
  }

  Interpolation get_interpolation() const
  {
    const Result &input = this->get_input("Interpolation");
    const MenuValue default_menu_value = MenuValue(CMP_NODE_INTERPOLATION_BILINEAR);
    const MenuValue menu_value = input.get_single_value_default(default_menu_value);
    const CMPNodeInterpolation interpolation = static_cast<CMPNodeInterpolation>(menu_value.value);
    switch (interpolation) {
      case CMP_NODE_INTERPOLATION_NEAREST:
        return Interpolation::Nearest;
      case CMP_NODE_INTERPOLATION_BILINEAR:
        return Interpolation::Bilinear;
      case CMP_NODE_INTERPOLATION_ANISOTROPIC:
      case CMP_NODE_INTERPOLATION_BICUBIC:
        return Interpolation::Bicubic;
    }

    return Interpolation::Nearest;
  }

  ExtensionMode get_extension_mode_x() const
  {
    const Result &input = this->get_input("Extension X");
    const MenuValue default_menu_value = MenuValue(CMP_NODE_EXTENSION_MODE_CLIP);
    const MenuValue menu_value = input.get_single_value_default(default_menu_value);
    const CMPExtensionMode extension_x = static_cast<CMPExtensionMode>(menu_value.value);
    switch (extension_x) {
      case CMP_NODE_EXTENSION_MODE_CLIP:
        return ExtensionMode::Clip;
      case CMP_NODE_EXTENSION_MODE_REPEAT:
        return ExtensionMode::Repeat;
      case CMP_NODE_EXTENSION_MODE_EXTEND:
        return ExtensionMode::Extend;
    }

    return ExtensionMode::Clip;
  }

  ExtensionMode get_extension_mode_y() const
  {
    const Result &input = this->get_input("Extension Y");
    const MenuValue default_menu_value = MenuValue(CMP_NODE_EXTENSION_MODE_CLIP);
    const MenuValue menu_value = input.get_single_value_default(default_menu_value);
    const CMPExtensionMode extension_y = static_cast<CMPExtensionMode>(menu_value.value);
    switch (extension_y) {
      case CMP_NODE_EXTENSION_MODE_CLIP:
        return ExtensionMode::Clip;
      case CMP_NODE_EXTENSION_MODE_REPEAT:
        return ExtensionMode::Repeat;
      case CMP_NODE_EXTENSION_MODE_EXTEND:
        return ExtensionMode::Extend;
    }

    return ExtensionMode::Clip;
  }

  float2 get_scale()
  {
    switch (get_type()) {
      case CMP_NODE_SCALE_RELATIVE:
        return get_scale_relative();
      case CMP_NODE_SCALE_ABSOLUTE:
        return get_scale_absolute();
      case CMP_NODE_SCALE_RENDER_PERCENT:
        return get_scale_render_percent();
      case CMP_NODE_SCALE_RENDER_SIZE:
        return get_scale_render_size();
    }

    return float2(1.0f);
  }

  /* Scale by the input factors. */
  float2 get_scale_relative()
  {
    return float2(get_input("X").get_single_value_default(1.0f),
                  get_input("Y").get_single_value_default(1.0f));
  }

  /* Scale such that the new size matches the input absolute size. */
  float2 get_scale_absolute()
  {
    const float2 input_size = float2(get_input("Image").domain().size);
    const float2 absolute_size = float2(get_input("X").get_single_value_default(1.0f),
                                        get_input("Y").get_single_value_default(1.0f));
    return absolute_size / input_size;
  }

  /* Scale by the render resolution percentage. */
  float2 get_scale_render_percent()
  {
    return float2(context().get_render_percentage());
  }

  float2 get_scale_render_size()
  {
    if (!context().is_valid_compositing_region()) {
      return float2(1.0f);
    }

    switch (get_frame_type()) {
      case CMP_NODE_SCALE_RENDER_SIZE_STRETCH:
        return get_scale_render_size_stretch();
      case CMP_NODE_SCALE_RENDER_SIZE_FIT:
        return get_scale_render_size_fit();
      case CMP_NODE_SCALE_RENDER_SIZE_CROP:
        return get_scale_render_size_crop();
    }

    return float2(1.0f);
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

  bool is_variable_size()
  {
    /* Only relative scaling can be variable. */
    if (get_type() != CMP_NODE_SCALE_RELATIVE) {
      return false;
    }

    return !get_input("X").is_single_value() || !get_input("Y").is_single_value();
  }

  CMPNodeScaleMethod get_type()
  {
    const Result &input = this->get_input("Type");
    const MenuValue default_menu_value = MenuValue(CMP_NODE_SCALE_RELATIVE);
    const MenuValue menu_value = input.get_single_value_default(default_menu_value);
    return static_cast<CMPNodeScaleMethod>(menu_value.value);
  }

  CMPNodeScaleRenderSizeMethod get_frame_type()
  {
    const Result &input = this->get_input("Frame Type");
    const MenuValue default_menu_value = MenuValue(CMP_NODE_SCALE_RENDER_SIZE_STRETCH);
    const MenuValue menu_value = input.get_single_value_default(default_menu_value);
    return static_cast<CMPNodeScaleRenderSizeMethod>(menu_value.value);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new ScaleOperation(context, node);
}

}  // namespace blender::nodes::node_composite_scale_cc

static void register_node_type_cmp_scale()
{
  namespace file_ns = blender::nodes::node_composite_scale_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeScale", CMP_NODE_SCALE);
  ntype.ui_name = "Scale";
  ntype.ui_description = "Change the size of the image";
  ntype.enum_name_legacy = "SCALE";
  ntype.nclass = NODE_CLASS_DISTORT;
  ntype.declare = file_ns::cmp_node_scale_declare;
  ntype.initfunc = file_ns::node_composit_init_scale;
  blender::bke::node_type_storage(
      ntype, "NodeScaleData", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_scale)
