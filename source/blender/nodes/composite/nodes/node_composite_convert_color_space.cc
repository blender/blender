/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation */

/** \file
 * \ingroup cmpnodes
 */

#include "BLT_translation.h"

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "IMB_colormanagement.h"

#include "GPU_shader.h"

#include "COM_node_operation.hh"
#include "COM_ocio_color_space_conversion_shader.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_convert_color_space_cc {

NODE_STORAGE_FUNCS(NodeConvertColorSpace)

static void CMP_NODE_CONVERT_COLOR_SPACE_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image"))
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_output<decl::Color>(N_("Image"));
}

static void node_composit_init_convert_colorspace(bNodeTree * /*ntree*/, bNode *node)
{
  NodeConvertColorSpace *ncs = static_cast<NodeConvertColorSpace *>(
      MEM_callocN(sizeof(NodeConvertColorSpace), "node colorspace"));
  const char *first_colorspace = IMB_colormanagement_role_colorspace_name_get(
      COLOR_ROLE_SCENE_LINEAR);
  if (first_colorspace && first_colorspace[0]) {
    STRNCPY(ncs->from_color_space, first_colorspace);
    STRNCPY(ncs->to_color_space, first_colorspace);
  }
  else {
    ncs->from_color_space[0] = 0;
    ncs->to_color_space[0] = 0;
  }
  node->storage = ncs;
}

static void node_composit_buts_convert_colorspace(uiLayout *layout,
                                                  bContext * /*C*/,
                                                  PointerRNA *ptr)
{
  uiItemR(layout, ptr, "from_color_space", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "to_color_space", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class ConvertColorSpaceOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    Result &input_image = get_input("Image");
    Result &output_image = get_result("Image");
    if (is_identity()) {
      input_image.pass_through(output_image);
      return;
    }

    if (input_image.is_single_value()) {
      execute_single();
      return;
    }

    const char *source = node_storage(bnode()).from_color_space;
    const char *target = node_storage(bnode()).to_color_space;

    OCIOColorSpaceConversionShader &ocio_shader =
        context().cache_manager().ocio_color_space_conversion_shaders.get(source, target);

    GPUShader *shader = ocio_shader.bind_shader_and_resources();

    /* A null shader indicates that the conversion shader is just a stub implementation since OCIO
     * is disabled at compile time, so pass the input through in that case. */
    if (!shader) {
      input_image.pass_through(output_image);
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

  void execute_single()
  {
    const char *source = node_storage(bnode()).from_color_space;
    const char *target = node_storage(bnode()).to_color_space;
    ColormanageProcessor *color_processor = IMB_colormanagement_colorspace_processor_new(source,
                                                                                         target);

    Result &input_image = get_input("Image");
    float4 color = input_image.get_color_value();

    IMB_colormanagement_processor_apply_pixel(color_processor, color, 3);
    IMB_colormanagement_processor_free(color_processor);

    Result &output_image = get_result("Image");
    output_image.allocate_single_value();
    output_image.set_color_value(color);
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

void register_node_type_cmp_convert_color_space(void)
{
  namespace file_ns = blender::nodes::node_composite_convert_color_space_cc;
  static bNodeType ntype;

  cmp_node_type_base(
      &ntype, CMP_NODE_CONVERT_COLOR_SPACE, "Convert Colorspace", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::CMP_NODE_CONVERT_COLOR_SPACE_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_convert_colorspace;
  node_type_size_preset(&ntype, NODE_SIZE_MIDDLE);
  ntype.initfunc = file_ns::node_composit_init_convert_colorspace;
  node_type_storage(
      &ntype, "NodeConvertColorSpace", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}
