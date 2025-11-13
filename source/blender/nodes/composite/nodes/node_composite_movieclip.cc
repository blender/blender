/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_array.hh"
#include "BLI_math_vector_types.hh"

#include "IMB_imbuf.hh"

#include "BKE_context.hh"
#include "BKE_lib_id.hh"
#include "BKE_movieclip.h"
#include "BKE_tracking.h"

#include "DNA_defaults.h"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"

#include "GPU_texture.hh"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_movieclip_cc {

static void cmp_node_movieclip_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Color>("Image").structure_type(StructureType::Dynamic);
  b.add_output<decl::Float>("Alpha").structure_type(StructureType::Dynamic);
  b.add_output<decl::Float>("Offset X");
  b.add_output<decl::Float>("Offset Y");
  b.add_output<decl::Float>("Scale");
  b.add_output<decl::Float>("Angle");
}

static void init(const bContext *C, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;
  Scene *scene = CTX_data_scene(C);
  MovieClipUser *user = DNA_struct_default_alloc(MovieClipUser);

  node->id = (ID *)scene->clip;
  id_us_plus(node->id);
  node->storage = user;
  user->framenr = 1;
}

static void node_composit_buts_movieclip(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  uiTemplateID(layout, C, ptr, "clip", nullptr, "CLIP_OT_open", nullptr);
}

static void node_composit_buts_movieclip_ex(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);
  uiTemplateMovieClip(layout, C, ptr, "clip", false);
}

using namespace blender::compositor;

class MovieClipOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    ImBuf *movie_clip_buffer = get_movie_clip_buffer();

    compute_image(movie_clip_buffer);
    compute_alpha(movie_clip_buffer);
    compute_stabilization_data(movie_clip_buffer);

    IMB_freeImBuf(movie_clip_buffer);
  }

  void compute_image(ImBuf *movie_clip_buffer)
  {
    if (!should_compute_output("Image")) {
      return;
    }

    Result &result = get_result("Image");
    if (!movie_clip_buffer) {
      result.allocate_invalid();
      return;
    }

    const int2 size = int2(movie_clip_buffer->x, movie_clip_buffer->y);
    result.allocate_texture(Domain(size));

    if (context().use_gpu()) {
      GPU_texture_update(result, GPU_DATA_FLOAT, movie_clip_buffer->float_buffer.data);
    }
    else {
      parallel_for(size, [&](const int2 texel) {
        int64_t pixel_index = (int64_t(texel.y) * size.x + texel.x) * 4;
        result.store_pixel(texel, Color(movie_clip_buffer->float_buffer.data + pixel_index));
      });
    }
  }

  void compute_alpha(ImBuf *movie_clip_buffer)
  {
    if (!should_compute_output("Alpha")) {
      return;
    }

    Result &result = get_result("Alpha");
    if (!movie_clip_buffer) {
      result.allocate_single_value();
      result.set_single_value(1.0f);
      return;
    }

    const int2 size = int2(movie_clip_buffer->x, movie_clip_buffer->y);
    result.allocate_texture(Domain(size));

    if (context().use_gpu()) {
      Array<float> alpha_values(size.x * size.y);
      parallel_for(size, [&](const int2 texel) {
        int64_t pixel_index = int64_t(texel.y) * size.x + texel.x;
        int64_t input_pixel_index = pixel_index * 4;
        alpha_values[pixel_index] = movie_clip_buffer->float_buffer.data[input_pixel_index + 3];
      });
      GPU_texture_update(result, GPU_DATA_FLOAT, alpha_values.data());
    }
    else {
      parallel_for(size, [&](const int2 texel) {
        int64_t pixel_index = (int64_t(texel.y) * size.x + texel.x) * 4;
        result.store_pixel(texel, movie_clip_buffer->float_buffer.data[pixel_index + 3]);
      });
    }
  }

  void compute_stabilization_data(ImBuf *movie_clip_buffer)
  {
    /* The movie clip buffer is invalid or missing, set appropriate fallback values. */
    if (!movie_clip_buffer) {
      if (should_compute_output("Offset X")) {
        Result &result = get_result("Offset X");
        result.allocate_single_value();
        result.set_single_value(0.0f);
      }
      if (should_compute_output("Offset Y")) {
        Result &result = get_result("Offset Y");
        result.allocate_single_value();
        result.set_single_value(0.0f);
      }
      if (should_compute_output("Scale")) {
        Result &result = get_result("Scale");
        result.allocate_single_value();
        result.set_single_value(1.0f);
      }
      if (should_compute_output("Angle")) {
        Result &result = get_result("Angle");
        result.allocate_single_value();
        result.set_single_value(0.0f);
      }
      return;
    }

    MovieClip *movie_clip = get_movie_clip();
    const int frame_number = BKE_movieclip_remap_scene_to_clip_frame(movie_clip,
                                                                     context().get_frame_number());
    const int width = movie_clip_buffer->x;
    const int height = movie_clip_buffer->y;

    /* If the movie clip has no stabilization data, it will initialize the given values with
     * fallback values regardless, so no need to handle that case. */
    float2 offset;
    float scale, angle;
    BKE_tracking_stabilization_data_get(
        movie_clip, frame_number, width, height, offset, &scale, &angle);

    if (should_compute_output("Offset X")) {
      Result &result = get_result("Offset X");
      result.allocate_single_value();
      result.set_single_value(offset.x);
    }
    if (should_compute_output("Offset Y")) {
      Result &result = get_result("Offset Y");
      result.allocate_single_value();
      result.set_single_value(offset.y);
    }
    if (should_compute_output("Scale")) {
      Result &result = get_result("Scale");
      result.allocate_single_value();
      result.set_single_value(scale);
    }
    if (should_compute_output("Angle")) {
      Result &result = get_result("Angle");
      result.allocate_single_value();
      result.set_single_value(angle);
    }
  }

  /* Get a float image buffer contacting the movie content at the current frame. If the movie clip
   * does not exist or is invalid, return nullptr. */
  ImBuf *get_movie_clip_buffer()
  {
    MovieClip *movie_clip = get_movie_clip();
    if (!movie_clip) {
      return nullptr;
    }

    MovieClipUser *movie_clip_user = get_movie_clip_user();
    BKE_movieclip_user_set_frame(movie_clip_user, context().get_frame_number());

    ImBuf *movie_clip_buffer = BKE_movieclip_get_ibuf(movie_clip, movie_clip_user);
    if (!movie_clip_buffer) {
      return nullptr;
    }

    if (movie_clip_buffer->float_buffer.data) {
      return movie_clip_buffer;
    }

    /* Create a float buffer from the byte buffer if it exists, if not, return nullptr. */
    IMB_float_from_byte(movie_clip_buffer);
    if (!movie_clip_buffer->float_buffer.data) {
      return nullptr;
    }

    return movie_clip_buffer;
  }

  MovieClip *get_movie_clip()
  {
    return reinterpret_cast<MovieClip *>(bnode().id);
  }

  MovieClipUser *get_movie_clip_user()
  {
    return static_cast<MovieClipUser *>(bnode().storage);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new MovieClipOperation(context, node);
}

}  // namespace blender::nodes::node_composite_movieclip_cc

static void register_node_type_cmp_movieclip()
{
  namespace file_ns = blender::nodes::node_composite_movieclip_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeMovieClip", CMP_NODE_MOVIECLIP);
  ntype.ui_name = "Movie Clip";
  ntype.ui_description =
      "Input image or movie from a movie clip data-block, typically used for motion tracking";
  ntype.enum_name_legacy = "MOVIECLIP";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = file_ns::cmp_node_movieclip_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_movieclip;
  ntype.draw_buttons_ex = file_ns::node_composit_buts_movieclip_ex;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;
  ntype.initfunc_api = file_ns::init;
  ntype.flag |= NODE_PREVIEW;
  blender::bke::node_type_storage(
      ntype, "MovieClipUser", node_free_standard_storage, node_copy_standard_storage);

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_movieclip)
