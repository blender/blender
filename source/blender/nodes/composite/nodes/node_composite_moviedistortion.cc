/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_string_utf8.h"

#include "DNA_movieclip_types.h"

#include "BKE_context.h"
#include "BKE_lib_id.h"
#include "BKE_tracking.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_shader.h"
#include "GPU_texture.h"

#include "COM_distortion_grid.hh"
#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** Translate  ******************** */

namespace blender::nodes::node_composite_moviedistortion_cc {

static void cmp_node_moviedistortion_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({0.8f, 0.8f, 0.8f, 1.0f})
      .compositor_domain_priority(0);
  b.add_output<decl::Color>("Image");
}

static void label(const bNodeTree * /*ntree*/, const bNode *node, char *label, int label_maxncpy)
{
  if (node->custom1 == 0) {
    BLI_strncpy_utf8(label, IFACE_("Undistortion"), label_maxncpy);
  }
  else {
    BLI_strncpy_utf8(label, IFACE_("Distortion"), label_maxncpy);
  }
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
  bNode *node = (bNode *)ptr->data;

  uiTemplateID(layout,
               C,
               ptr,
               "clip",
               nullptr,
               "CLIP_OT_open",
               nullptr,
               UI_TEMPLATE_ID_FILTER_ALL,
               false,
               nullptr);

  if (!node->id) {
    return;
  }

  uiItemR(layout, ptr, "distortion_type", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

using namespace blender::realtime_compositor;

class MovieDistortionOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    Result &input_image = get_input("Image");
    Result &output_image = get_result("Image");
    if (input_image.is_single_value() || !get_movie_clip()) {
      input_image.pass_through(output_image);
      return;
    }

    const Domain domain = compute_domain();
    const DistortionGrid &distortion_grid = context().cache_manager().distortion_grids.get(
        get_movie_clip(), domain.size, get_distortion_type(), context().get_frame_number());

    GPUShader *shader = shader_manager().get("compositor_movie_distortion");
    GPU_shader_bind(shader);

    GPU_texture_extend_mode(input_image.texture(), GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER);
    GPU_texture_filter_mode(input_image.texture(), true);
    input_image.bind_as_texture(shader, "input_tx");

    distortion_grid.bind_as_texture(shader, "distortion_grid_tx");

    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    input_image.unbind_as_texture();
    distortion_grid.unbind_as_texture();
    output_image.unbind_as_image();
    GPU_shader_unbind();
  }

  DistortionType get_distortion_type()
  {
    return bnode().custom1 == 0 ? DistortionType::Distort : DistortionType::Undistort;
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

void register_node_type_cmp_moviedistortion()
{
  namespace file_ns = blender::nodes::node_composite_moviedistortion_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_MOVIEDISTORTION, "Movie Distortion", NODE_CLASS_DISTORT);
  ntype.declare = file_ns::cmp_node_moviedistortion_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_moviedistortion;
  ntype.labelfunc = file_ns::label;
  ntype.initfunc_api = file_ns::init;
  node_type_storage(&ntype, nullptr, file_ns::storage_free, file_ns::storage_copy);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}
