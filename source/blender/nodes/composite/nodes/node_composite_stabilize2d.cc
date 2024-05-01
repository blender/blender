/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_assert.h"
#include "BLI_math_angle_types.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "DNA_movieclip_types.h"
#include "DNA_node_types.h"

#include "BKE_context.hh"
#include "BKE_lib_id.hh"
#include "BKE_movieclip.h"
#include "BKE_tracking.h"

#include "COM_algorithm_transform.hh"
#include "COM_node_operation.hh"

#include "node_composite_util.hh"

/* **************** Stabilize 2D ******************** */

namespace blender::nodes::node_composite_stabilize2d_cc {

static void cmp_node_stabilize2d_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({0.8f, 0.8f, 0.8f, 1.0f})
      .compositor_domain_priority(0);
  b.add_output<decl::Color>("Image");
}

static void init(const bContext *C, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;
  Scene *scene = CTX_data_scene(C);

  node->id = (ID *)scene->clip;
  id_us_plus(node->id);

  /* Default to bi-linear, see node_sampler_type_items in `rna_nodetree.cc`. */
  node->custom1 = 1;
}

static void node_composit_buts_stabilize2d(uiLayout *layout, bContext *C, PointerRNA *ptr)
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

  uiItemR(layout, ptr, "filter_type", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  uiItemR(layout, ptr, "invert", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class Stabilize2DOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    Result &input = get_input("Image");
    Result &output = get_result("Image");

    MovieClip *movie_clip = get_movie_clip();
    if (input.is_single_value() || !movie_clip) {
      input.pass_through(output);
      return;
    }

    const int width = input.domain().size.x;
    const int height = input.domain().size.y;
    const int frame_number = BKE_movieclip_remap_scene_to_clip_frame(movie_clip,
                                                                     context().get_frame_number());

    float2 translation;
    float scale, rotation;
    BKE_tracking_stabilization_data_get(
        movie_clip, frame_number, width, height, translation, &scale, &rotation);

    float3x3 transformation = math::from_loc_rot_scale<float3x3>(
        translation, math::AngleRadian(rotation), float2(scale));
    if (do_inverse_stabilization()) {
      transformation = math::invert(transformation);
    }

    RealizationOptions realization_options = input.get_realization_options();
    realization_options.interpolation = get_interpolation();

    transform(context(), input, output, transformation, realization_options);
  }

  Interpolation get_interpolation()
  {
    switch (static_cast<CMPNodeInterpolation>(bnode().custom1)) {
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

  bool do_inverse_stabilization()
  {
    return bnode().custom2 & CMP_NODE_STABILIZE_FLAG_INVERSE;
  }

  MovieClip *get_movie_clip()
  {
    return (MovieClip *)bnode().id;
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new Stabilize2DOperation(context, node);
}

}  // namespace blender::nodes::node_composite_stabilize2d_cc

void register_node_type_cmp_stabilize2d()
{
  namespace file_ns = blender::nodes::node_composite_stabilize2d_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_STABILIZE2D, "Stabilize 2D", NODE_CLASS_DISTORT);
  ntype.declare = file_ns::cmp_node_stabilize2d_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_stabilize2d;
  ntype.initfunc_api = file_ns::init;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}
