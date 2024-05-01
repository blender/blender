/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_vector_types.hh"

#include "BKE_context.hh"
#include "BKE_lib_id.hh"
#include "BKE_movieclip.h"
#include "BKE_tracking.h"

#include "DNA_defaults.h"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_shader.hh"
#include "GPU_texture.hh"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_movieclip_cc {

static void cmp_node_movieclip_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Color>("Image");
  b.add_output<decl::Float>("Alpha");
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
}

static void node_composit_buts_movieclip_ex(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;
  PointerRNA clipptr;

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

  clipptr = RNA_pointer_get(ptr, "clip");

  uiTemplateColorspaceSettings(layout, &clipptr, "colorspace_settings");
}

using namespace blender::realtime_compositor;

class MovieClipOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    GPUTexture *movie_clip_texture = get_movie_clip_texture();

    compute_image(movie_clip_texture);
    compute_alpha(movie_clip_texture);
    compute_stabilization_data(movie_clip_texture);

    free_movie_clip_texture();
  }

  void compute_image(GPUTexture *movie_clip_texture)
  {
    if (!should_compute_output("Image")) {
      return;
    }

    Result &result = get_result("Image");

    /* The movie clip texture is invalid or missing, set an appropriate fallback value. */
    if (!movie_clip_texture) {
      result.allocate_invalid();
      return;
    }

    const int2 size = int2(GPU_texture_width(movie_clip_texture),
                           GPU_texture_height(movie_clip_texture));
    result.allocate_texture(Domain(size));

    GPUShader *shader = context().get_shader("compositor_read_input_color");
    GPU_shader_bind(shader);

    const int2 lower_bound = int2(0);
    GPU_shader_uniform_2iv(shader, "lower_bound", lower_bound);

    const int input_unit = GPU_shader_get_sampler_binding(shader, "input_tx");
    GPU_texture_bind(movie_clip_texture, input_unit);

    result.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, size);

    GPU_shader_unbind();
    GPU_texture_unbind(movie_clip_texture);
    result.unbind_as_image();
  }

  void compute_alpha(GPUTexture *movie_clip_texture)
  {
    if (!should_compute_output("Alpha")) {
      return;
    }

    Result &result = get_result("Alpha");

    /* The movie clip texture is invalid or missing, set an appropriate fallback value. */
    if (!movie_clip_texture) {
      result.allocate_single_value();
      result.set_float_value(1.0f);
      return;
    }

    const int2 size = int2(GPU_texture_width(movie_clip_texture),
                           GPU_texture_height(movie_clip_texture));
    result.allocate_texture(Domain(size));

    GPUShader *shader = context().get_shader("compositor_read_input_alpha");
    GPU_shader_bind(shader);

    const int2 lower_bound = int2(0);
    GPU_shader_uniform_2iv(shader, "lower_bound", lower_bound);

    const int input_unit = GPU_shader_get_sampler_binding(shader, "input_tx");
    GPU_texture_bind(movie_clip_texture, input_unit);

    result.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, size);

    GPU_shader_unbind();
    GPU_texture_unbind(movie_clip_texture);
    result.unbind_as_image();
  }

  void compute_stabilization_data(GPUTexture *movie_clip_texture)
  {
    /* The movie clip texture is invalid or missing, set appropriate fallback values. */
    if (!movie_clip_texture) {
      if (should_compute_output("Offset X")) {
        Result &result = get_result("Offset X");
        result.allocate_single_value();
        result.set_float_value(0.0f);
      }
      if (should_compute_output("Offset Y")) {
        Result &result = get_result("Offset Y");
        result.allocate_single_value();
        result.set_float_value(0.0f);
      }
      if (should_compute_output("Scale")) {
        Result &result = get_result("Scale");
        result.allocate_single_value();
        result.set_float_value(1.0f);
      }
      if (should_compute_output("Angle")) {
        Result &result = get_result("Angle");
        result.allocate_single_value();
        result.set_float_value(0.0f);
      }
      return;
    }

    MovieClip *movie_clip = get_movie_clip();
    const int frame_number = BKE_movieclip_remap_scene_to_clip_frame(movie_clip,
                                                                     context().get_frame_number());
    const int width = GPU_texture_width(movie_clip_texture);
    const int height = GPU_texture_height(movie_clip_texture);

    /* If the movie clip has no stabilization data, it will initialize the given values with
     * fallback values regardless, so no need to handle that case. */
    float2 offset;
    float scale, angle;
    BKE_tracking_stabilization_data_get(
        movie_clip, frame_number, width, height, offset, &scale, &angle);

    if (should_compute_output("Offset X")) {
      Result &result = get_result("Offset X");
      result.allocate_single_value();
      result.set_float_value(offset.x);
    }
    if (should_compute_output("Offset Y")) {
      Result &result = get_result("Offset Y");
      result.allocate_single_value();
      result.set_float_value(offset.y);
    }
    if (should_compute_output("Scale")) {
      Result &result = get_result("Scale");
      result.allocate_single_value();
      result.set_float_value(scale);
    }
    if (should_compute_output("Angle")) {
      Result &result = get_result("Angle");
      result.allocate_single_value();
      result.set_float_value(angle);
    }
  }

  GPUTexture *get_movie_clip_texture()
  {
    MovieClip *movie_clip = get_movie_clip();
    MovieClipUser *movie_clip_user = get_movie_clip_user();
    BKE_movieclip_user_set_frame(movie_clip_user, context().get_frame_number());
    return BKE_movieclip_get_gpu_texture(movie_clip, movie_clip_user);
  }

  void free_movie_clip_texture()
  {
    MovieClip *movie_clip = get_movie_clip();
    if (movie_clip) {
      BKE_movieclip_free_gputexture(movie_clip);
    }
  }

  MovieClip *get_movie_clip()
  {
    return (MovieClip *)bnode().id;
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

void register_node_type_cmp_movieclip()
{
  namespace file_ns = blender::nodes::node_composite_movieclip_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_MOVIECLIP, "Movie Clip", NODE_CLASS_INPUT);
  ntype.declare = file_ns::cmp_node_movieclip_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_movieclip;
  ntype.draw_buttons_ex = file_ns::node_composit_buts_movieclip_ex;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;
  ntype.initfunc_api = file_ns::init;
  ntype.flag |= NODE_PREVIEW;
  node_type_storage(
      &ntype, "MovieClipUser", node_free_standard_storage, node_copy_standard_storage);

  nodeRegisterType(&ntype);
}
