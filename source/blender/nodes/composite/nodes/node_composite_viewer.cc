/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_bounds_types.hh"
#include "BLI_math_vector_types.hh"

#include "BKE_global.hh"
#include "BKE_image.hh"

#include "RNA_access.hh"

#include "UI_resources.hh"

#include "GPU_shader.hh"
#include "GPU_texture.hh"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** VIEWER ******************** */

namespace blender::nodes::node_composite_viewer_cc {

static void cmp_node_viewer_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({0.0f, 0.0f, 0.0f, 1.0f})
      .structure_type(StructureType::Dynamic);
}

static void node_composit_init_viewer(bNodeTree * /*ntree*/, bNode *node)
{
  ImageUser *iuser = MEM_callocN<ImageUser>(__func__);
  node->storage = iuser;
  iuser->sfra = 1;
  node->custom1 = NODE_VIEWER_SHORTCUT_NONE;
}

using namespace blender::compositor;

class ViewerOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    /* Viewers are treated as composite outputs that should be in the bounds of the compositing
     * region, so do nothing if the compositing region is invalid. */
    if (this->context().treat_viewer_as_compositor_output() &&
        !this->context().is_valid_compositing_region())
    {
      return;
    }

    const Result &image = this->get_input("Image");
    if (image.is_single_value()) {
      this->execute_clear();
    }
    else {
      this->execute_copy();
    }
  }

  void execute_clear()
  {
    const Result &image = this->get_input("Image");

    Color color = image.get_single_value<Color>();

    const Domain domain = this->compute_domain();
    Result output = this->context().get_viewer_output(
        domain, image.meta_data.is_non_color_data, image.precision());
    if (this->context().use_gpu()) {
      GPU_texture_clear(output, GPU_DATA_FLOAT, color);
    }
    else {
      parallel_for(domain.size, [&](const int2 texel) { output.store_pixel(texel, color); });
    }
  }

  void execute_copy()
  {
    if (this->context().use_gpu()) {
      this->execute_copy_gpu();
    }
    else {
      this->execute_copy_cpu();
    }
  }

  void execute_copy_gpu()
  {
    const Result &image = this->get_input("Image");
    const Domain domain = this->compute_domain();
    Result output = this->context().get_viewer_output(
        domain, image.meta_data.is_non_color_data, image.precision());

    gpu::Shader *shader = this->context().get_shader("compositor_write_output",
                                                     output.precision());
    GPU_shader_bind(shader);

    const Bounds<int2> bounds = this->get_output_bounds();
    GPU_shader_uniform_2iv(shader, "lower_bound", bounds.min);
    GPU_shader_uniform_2iv(shader, "upper_bound", bounds.max);

    image.bind_as_texture(shader, "input_tx");

    output.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    image.unbind_as_texture();
    output.unbind_as_image();
    GPU_shader_unbind();
  }

  void execute_copy_cpu()
  {
    const Domain domain = this->compute_domain();
    const Result &image = this->get_input("Image");
    Result output = this->context().get_viewer_output(
        domain, image.meta_data.is_non_color_data, image.precision());

    const Bounds<int2> bounds = this->get_output_bounds();
    parallel_for(domain.size, [&](const int2 texel) {
      const int2 output_texel = texel + bounds.min;
      if (output_texel.x > bounds.max.x || output_texel.y > bounds.max.y) {
        return;
      }
      output.store_pixel(texel + bounds.min, image.load_pixel<Color>(texel));
    });
  }

  /* Returns the bounds of the area of the viewer, which might be limited to a smaller region of
   * the output. */
  Bounds<int2> get_output_bounds()
  {
    /* Viewers are treated as composite outputs that should be in the bounds of the compositing
     * region. */
    if (this->context().treat_viewer_as_compositor_output() &&
        this->context().use_context_bounds_for_input_output())
    {
      return this->context().get_compositing_region();
    }

    /* Otherwise, use the bounds of the input as is. */
    return Bounds<int2>(int2(0), this->compute_domain().size);
  }

  Domain compute_domain() override
  {
    /* Viewers are treated as composite outputs that should be in the domain of the compositing
     * region. */
    if (this->context().treat_viewer_as_compositor_output() &&
        this->context().use_context_bounds_for_input_output())
    {
      return Domain(context().get_compositing_region_size());
    }

    /* Otherwise, use the domain of the input as is. */
    const Domain domain = NodeOperation::compute_domain();
    /* Fall back to the compositing region size in case of a single value domain. */
    return domain.size == int2(1) ? Domain(context().get_compositing_region_size()) : domain;
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new ViewerOperation(context, node);
}

}  // namespace blender::nodes::node_composite_viewer_cc

static void register_node_type_cmp_viewer()
{
  namespace file_ns = blender::nodes::node_composite_viewer_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeViewer", CMP_NODE_VIEWER);
  ntype.ui_name = "Viewer";
  ntype.ui_description =
      "Visualize data from inside a node graph, in the image editor or as a backdrop";
  ntype.enum_name_legacy = "VIEWER";
  ntype.nclass = NODE_CLASS_OUTPUT;
  ntype.declare = file_ns::cmp_node_viewer_declare;
  ntype.initfunc = file_ns::node_composit_init_viewer;
  blender::bke::node_type_storage(
      ntype, "ImageUser", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  ntype.no_muting = true;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_viewer)
