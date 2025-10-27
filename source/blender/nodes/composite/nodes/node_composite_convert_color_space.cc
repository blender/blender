/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_string_utf8.h"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "IMB_colormanagement.hh"

#include "GPU_shader.hh"

#include "COM_node_operation.hh"
#include "COM_ocio_color_space_conversion_shader.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_convert_color_space_cc {

NODE_STORAGE_FUNCS(NodeConvertColorSpace)

static void CMP_NODE_CONVERT_COLOR_SPACE_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .structure_type(StructureType::Dynamic);

  b.add_output<decl::Color>("Image").structure_type(StructureType::Dynamic);
}

static void node_composit_init_convert_colorspace(bNodeTree * /*ntree*/, bNode *node)
{
  NodeConvertColorSpace *ncs = MEM_callocN<NodeConvertColorSpace>("node colorspace");
  STRNCPY_UTF8(ncs->from_color_space, "scene_linear");
  STRNCPY_UTF8(ncs->to_color_space, "scene_linear");
  node->storage = ncs;
}

static void node_composit_buts_convert_colorspace(uiLayout *layout,
                                                  bContext * /*C*/,
                                                  PointerRNA *ptr)
{
#ifndef WITH_OPENCOLORIO
  layout->label(RPT_("Disabled, built without OpenColorIO"), ICON_ERROR);
#endif

  layout->prop(ptr, "from_color_space", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
  layout->prop(ptr, "to_color_space", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
}

using namespace blender::compositor;

class ConvertColorSpaceOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const Result &input_image = this->get_input("Image");
    if (this->is_identity()) {
      Result &output_image = this->get_result("Image");
      output_image.share_data(input_image);
      return;
    }

    if (input_image.is_single_value()) {
      execute_single();
      return;
    }
    if (this->context().use_gpu()) {
      execute_gpu();
    }
    else {
      execute_cpu();
    }
  }

  void execute_gpu()
  {
    const char *source = node_storage(bnode()).from_color_space;
    const char *target = node_storage(bnode()).to_color_space;

    OCIOColorSpaceConversionShader &ocio_shader =
        context().cache_manager().ocio_color_space_conversion_shaders.get(
            context(), source, target);

    gpu::Shader *shader = ocio_shader.bind_shader_and_resources();

    /* A null shader indicates that the conversion shader is just a stub implementation since OCIO
     * is disabled at compile time, so pass the input through in that case. */
    const Result &input_image = this->get_input("Image");
    Result &output_image = this->get_result("Image");
    if (!shader) {
      output_image.share_data(input_image);
      return;
    }

    input_image.bind_as_texture(shader, ocio_shader.input_sampler_name());

    const Domain domain = compute_domain();
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, ocio_shader.output_image_name());

    compute_dispatch_threads_at_least(shader, domain.size);

    input_image.unbind_as_texture();
    output_image.unbind_as_image();
    ocio_shader.unbind_shader_and_resources();
  }

  void execute_cpu()
  {
    const char *source = node_storage(bnode()).from_color_space;
    const char *target = node_storage(bnode()).to_color_space;
    ColormanageProcessor *color_processor = IMB_colormanagement_colorspace_processor_new(source,
                                                                                         target);

    Result &input_image = get_input("Image");

    const Domain domain = compute_domain();
    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);

    parallel_for(domain.size, [&](const int2 texel) {
      output_image.store_pixel(texel, input_image.load_pixel<Color>(texel));
    });

    IMB_colormanagement_processor_apply(color_processor,
                                        static_cast<float *>(output_image.cpu_data().data()),
                                        domain.size.x,
                                        domain.size.y,
                                        input_image.channels_count(),
                                        false);
    IMB_colormanagement_processor_free(color_processor);
  }

  void execute_single()
  {
    const char *source = node_storage(bnode()).from_color_space;
    const char *target = node_storage(bnode()).to_color_space;
    ColormanageProcessor *color_processor = IMB_colormanagement_colorspace_processor_new(source,
                                                                                         target);

    Result &input_image = get_input("Image");
    Color color = input_image.get_single_value<Color>();

    IMB_colormanagement_processor_apply_pixel(color_processor, color, 3);
    IMB_colormanagement_processor_free(color_processor);

    Result &output_image = get_result("Image");
    output_image.allocate_single_value();
    output_image.set_single_value(color);
  }

  bool is_identity()
  {
    const char *source = node_storage(bnode()).from_color_space;
    const char *target = node_storage(bnode()).to_color_space;

    if (STREQ(source, target)) {
      return true;
    }

    /* Data color spaces ignore any color transformation that gets applied to them. */
    if (IMB_colormanagement_space_name_is_data(source)) {
      return true;
    }

    return false;
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new ConvertColorSpaceOperation(context, node);
}

}  // namespace blender::nodes::node_composite_convert_color_space_cc

static void register_node_type_cmp_convert_color_space()
{
  namespace file_ns = blender::nodes::node_composite_convert_color_space_cc;
  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeConvertColorSpace", CMP_NODE_CONVERT_COLOR_SPACE);
  ntype.ui_name = "Convert Colorspace";
  ntype.ui_description = "Convert between color spaces";
  ntype.enum_name_legacy = "CONVERT_COLORSPACE";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = file_ns::CMP_NODE_CONVERT_COLOR_SPACE_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_convert_colorspace;
  blender::bke::node_type_size_preset(ntype, blender::bke::eNodeSizePreset::Middle);
  ntype.initfunc = file_ns::node_composit_init_convert_colorspace;
  blender::bke::node_type_storage(
      ntype, "NodeConvertColorSpace", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;
  blender::bke::node_type_size(ntype, 160, 150, NODE_DEFAULT_MAX_WIDTH);

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_convert_color_space)
