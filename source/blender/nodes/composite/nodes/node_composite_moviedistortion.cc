/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_vector_types.hh"
#include "BLI_string_utf8.h"

#include "DNA_movieclip_types.h"

#include "BKE_context.hh"
#include "BKE_lib_id.hh"
#include "BKE_tracking.h"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "GPU_shader.hh"
#include "GPU_texture.hh"

#include "COM_distortion_grid.hh"
#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_moviedistortion_cc {

static const EnumPropertyItem type_items[] = {
    {int(compositor::DistortionType::Distort), "UNDISTORT", 0, N_("Undistort"), ""},
    {int(compositor::DistortionType::Undistort), "DISTORT", 0, N_("Distort"), ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static void cmp_node_moviedistortion_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({0.8f, 0.8f, 0.8f, 1.0f})
      .structure_type(StructureType::Dynamic);
  b.add_input<decl::Menu>("Type")
      .default_value(compositor::DistortionType::Distort)
      .static_items(type_items)
      .optional_label();

  b.add_output<decl::Color>("Image").structure_type(StructureType::Dynamic);
}

static void init(const bContext *C, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;
  Scene *scene = CTX_data_scene(C);

  node->id = (ID *)scene->clip;
  id_us_plus(node->id);
}

static void storage_free(bNode *node)
{
  if (node->storage) {
    BKE_tracking_distortion_free((MovieDistortion *)node->storage);
  }

  node->storage = nullptr;
}

static void storage_copy(bNodeTree * /*dst_ntree*/, bNode *dest_node, const bNode *src_node)
{
  if (src_node->storage) {
    dest_node->storage = BKE_tracking_distortion_copy((MovieDistortion *)src_node->storage);
  }
}

static void node_composit_buts_moviedistortion(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  uiTemplateID(layout, C, ptr, "clip", nullptr, "CLIP_OT_open", nullptr);
}

using namespace blender::compositor;

class MovieDistortionOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const Result &input_image = this->get_input("Image");
    if (input_image.is_single_value() || !this->get_movie_clip()) {
      Result &output_image = this->get_result("Image");
      output_image.share_data(input_image);
      return;
    }

    const Domain domain = compute_domain();
    const Result &distortion_grid = context().cache_manager().distortion_grids.get(
        context(),
        get_movie_clip(),
        domain.size,
        get_distortion_type(),
        context().get_frame_number());

    if (this->context().use_gpu()) {
      this->execute_gpu(distortion_grid);
    }
    else {
      this->execute_cpu(distortion_grid);
    }
  }

  void execute_gpu(const Result &distortion_grid)
  {
    gpu::Shader *shader = context().get_shader("compositor_movie_distortion");
    GPU_shader_bind(shader);

    Result &input_image = get_input("Image");
    GPU_texture_extend_mode(input_image, GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER);
    GPU_texture_filter_mode(input_image, true);
    input_image.bind_as_texture(shader, "input_tx");

    distortion_grid.bind_as_texture(shader, "distortion_grid_tx");

    Result &output_image = get_result("Image");
    output_image.allocate_texture(distortion_grid.domain());
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, distortion_grid.domain().size);

    input_image.unbind_as_texture();
    distortion_grid.unbind_as_texture();
    output_image.unbind_as_image();
    GPU_shader_unbind();
  }

  void execute_cpu(const Result &distortion_grid)
  {
    Result &input = get_input("Image");

    Result &output = get_result("Image");
    output.allocate_texture(distortion_grid.domain());

    parallel_for(distortion_grid.domain().size, [&](const int2 texel) {
      output.store_pixel(
          texel, Color(input.sample_bilinear_zero(distortion_grid.load_pixel<float2>(texel))));
    });
  }

  DistortionType get_distortion_type()
  {
    const Result &input = this->get_input("Type");
    const MenuValue default_menu_value = MenuValue(DistortionType::Distort);
    const MenuValue menu_value = input.get_single_value_default(default_menu_value);
    return static_cast<DistortionType>(menu_value.value);
  }

  MovieClip *get_movie_clip()
  {
    return reinterpret_cast<MovieClip *>(bnode().id);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new MovieDistortionOperation(context, node);
}

}  // namespace blender::nodes::node_composite_moviedistortion_cc

static void register_node_type_cmp_moviedistortion()
{
  namespace file_ns = blender::nodes::node_composite_moviedistortion_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeMovieDistortion", CMP_NODE_MOVIEDISTORTION);
  ntype.ui_name = "Movie Distortion";
  ntype.ui_description =
      "Remove lens distortion from footage, using motion tracking camera lens settings";
  ntype.enum_name_legacy = "MOVIEDISTORTION";
  ntype.nclass = NODE_CLASS_DISTORT;
  ntype.declare = file_ns::cmp_node_moviedistortion_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_moviedistortion;
  ntype.initfunc_api = file_ns::init;
  blender::bke::node_type_storage(
      ntype, std::nullopt, file_ns::storage_free, file_ns::storage_copy);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_moviedistortion)
