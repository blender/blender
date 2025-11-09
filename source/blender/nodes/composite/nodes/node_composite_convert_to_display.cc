/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "DNA_color_types.h"

#include "BKE_colortools.hh"

#include "NOD_socket_declarations.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "IMB_colormanagement.hh"

#include "GPU_shader.hh"

#include "COM_node_operation.hh"
#include "COM_ocio_color_space_conversion_shader.hh"
#include "COM_utilities.hh"

#include "RNA_access.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_convert_to_display_cc {

NODE_STORAGE_FUNCS(NodeConvertToDisplay)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .structure_type(StructureType::Dynamic);
  b.add_input<decl::Bool>("Invert").default_value(false).description(
      "Convert from display to scene linear instead. Not all view transforms can be inverted "
      "exactly, and the result may not match the original scene linear image");

  b.add_output<decl::Color>("Image").structure_type(StructureType::Dynamic);
}

static void node_init(bNodeTree * /*ntree*/, bNode *node)
{
  NodeConvertToDisplay *nctd = MEM_callocN<NodeConvertToDisplay>(__func__);
  BKE_color_managed_display_settings_init(&nctd->display_settings);
  BKE_color_managed_view_settings_init(&nctd->view_settings, &nctd->display_settings, nullptr);
  nctd->view_settings.flag |= COLORMANAGE_VIEW_ONLY_VIEW_LOOK;
  node->storage = nctd;
}

static void node_free(bNode *node)
{
  NodeConvertToDisplay *nctd = static_cast<NodeConvertToDisplay *>(node->storage);
  BKE_color_managed_view_settings_free(&nctd->view_settings);
  MEM_freeN(nctd);
}

static void node_copy(bNodeTree * /*dest_ntree*/, bNode *dest_node, const bNode *src_node)
{
  NodeConvertToDisplay *dest = MEM_callocN<NodeConvertToDisplay>(__func__);
  const NodeConvertToDisplay *src = static_cast<const NodeConvertToDisplay *>(src_node->storage);
  BKE_color_managed_view_settings_copy(&dest->view_settings, &src->view_settings);
  BKE_color_managed_display_settings_copy(&dest->display_settings, &src->display_settings);
  dest_node->storage = dest;
}

static void node_blend_write(const bNodeTree & /*tree*/, const bNode &node, BlendWriter &writer)
{
  const NodeConvertToDisplay *nctd = static_cast<const NodeConvertToDisplay *>(node.storage);
  BKE_color_managed_view_settings_blend_write(&writer, &nctd->view_settings);
}

static void node_blend_read(bNodeTree & /*tree*/, bNode &node, BlendDataReader &reader)
{
  NodeConvertToDisplay *nctd = static_cast<NodeConvertToDisplay *>(node.storage);
  BKE_color_managed_view_settings_blend_read_data(&reader, &nctd->view_settings);
}

static void node_draw_buttons(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
#ifndef WITH_OPENCOLORIO
  layout->label(RPT_("Disabled, built without OpenColorIO"), ICON_ERROR);
#endif

  PointerRNA display_ptr = RNA_pointer_get(ptr, "display_settings");
  PointerRNA view_ptr = RNA_pointer_get(ptr, "view_settings");

  layout->prop(&display_ptr, "display_device", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(&view_ptr, "view_transform", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(&view_ptr, "look", UI_ITEM_NONE, IFACE_("Look"), ICON_NONE);
}

using namespace blender::compositor;

class ConvertToDisplayOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  bool do_inverse()
  {
    return this->get_input("Invert").get_single_value_default(false);
  }

  void execute() override
  {
    const Result &input_image = this->get_input("Image");

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
    const NodeConvertToDisplay &nctd = node_storage(bnode());

    OCIOToDisplayShader &ocio_shader = context().cache_manager().ocio_to_display_shaders.get(
        context(), nctd.display_settings, nctd.view_settings, do_inverse());

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
    const NodeConvertToDisplay &nctd = node_storage(bnode());
    ColormanageProcessor *color_processor = IMB_colormanagement_display_processor_new(
        &nctd.view_settings, &nctd.display_settings, DISPLAY_SPACE_VIDEO_OUTPUT, do_inverse());

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
    const NodeConvertToDisplay &nctd = node_storage(bnode());
    ColormanageProcessor *color_processor = IMB_colormanagement_display_processor_new(
        &nctd.view_settings, &nctd.display_settings, DISPLAY_SPACE_VIDEO_OUTPUT, do_inverse());

    Result &input_image = get_input("Image");
    Color color = input_image.get_single_value<Color>();

    IMB_colormanagement_processor_apply_pixel(color_processor, color, 3);
    IMB_colormanagement_processor_free(color_processor);

    Result &output_image = get_result("Image");
    output_image.allocate_single_value();
    output_image.set_single_value(color);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new ConvertToDisplayOperation(context, node);
}

static void register_node_type_cmp_convert_to_display()
{
  namespace file_ns = blender::nodes::node_composite_convert_to_display_cc;
  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeConvertToDisplay", CMP_NODE_CONVERT_TO_DISPLAY);
  ntype.ui_name = "Convert to Display";
  ntype.ui_description =
      "Convert from scene linear to display color space, with a view transform and look for tone "
      "mapping";
  ntype.enum_name_legacy = "CONVERT_TO_DISPLAY";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.draw_buttons = node_draw_buttons;
  blender::bke::node_type_size_preset(ntype, blender::bke::eNodeSizePreset::Middle);
  ntype.initfunc = node_init;
  blender::bke::node_type_storage(ntype, "NodeConvertToDisplay", node_free, node_copy);
  ntype.blend_data_read_storage_content = node_blend_read;
  ntype.blend_write_storage_content = node_blend_write;
  ntype.get_compositor_operation = get_compositor_operation;
  blender::bke::node_type_size(ntype, 240, 150, NODE_DEFAULT_MAX_WIDTH);

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_convert_to_display)

}  // namespace blender::nodes::node_composite_convert_to_display_cc
