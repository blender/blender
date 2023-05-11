/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2013 Blender Foundation */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_array.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"

#include "BLT_translation.h"

#include "DNA_defaults.h"
#include "DNA_movieclip_types.h"
#include "DNA_tracking_types.h"

#include "BKE_context.h"
#include "BKE_lib_id.h"
#include "BKE_movieclip.h"
#include "BKE_tracking.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "GPU_shader.h"
#include "GPU_texture.h"
#include "GPU_uniform_buffer.h"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_planetrackdeform_cc {

NODE_STORAGE_FUNCS(NodePlaneTrackDeformData)

static void cmp_node_planetrackdeform_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image")).compositor_skip_realization();
  b.add_output<decl::Color>(N_("Image"));
  b.add_output<decl::Float>(N_("Plane"));
}

static void init(const bContext *C, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;

  NodePlaneTrackDeformData *data = MEM_cnew<NodePlaneTrackDeformData>(__func__);
  data->motion_blur_samples = 16;
  data->motion_blur_shutter = 0.5f;
  node->storage = data;

  const Scene *scene = CTX_data_scene(C);
  if (scene->clip) {
    MovieClip *clip = scene->clip;
    MovieTracking *tracking = &clip->tracking;

    node->id = &clip->id;
    id_us_plus(&clip->id);

    const MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);
    STRNCPY(data->tracking_object, tracking_object->name);

    if (tracking_object->active_plane_track) {
      STRNCPY(data->plane_track_name, tracking_object->active_plane_track->name);
    }
  }
}

static void node_composit_buts_planetrackdeform(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;
  NodePlaneTrackDeformData *data = (NodePlaneTrackDeformData *)node->storage;

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

  if (node->id) {
    MovieClip *clip = (MovieClip *)node->id;
    MovieTracking *tracking = &clip->tracking;
    MovieTrackingObject *tracking_object;
    uiLayout *col;
    PointerRNA tracking_ptr;

    RNA_pointer_create(&clip->id, &RNA_MovieTracking, tracking, &tracking_ptr);

    col = uiLayoutColumn(layout, false);
    uiItemPointerR(col, ptr, "tracking_object", &tracking_ptr, "objects", "", ICON_OBJECT_DATA);

    tracking_object = BKE_tracking_object_get_named(tracking, data->tracking_object);
    if (tracking_object) {
      PointerRNA object_ptr;

      RNA_pointer_create(&clip->id, &RNA_MovieTrackingObject, tracking_object, &object_ptr);

      uiItemPointerR(
          col, ptr, "plane_track_name", &object_ptr, "plane_tracks", "", ICON_ANIM_DATA);
    }
    else {
      uiItemR(layout, ptr, "plane_track_name", 0, "", ICON_ANIM_DATA);
    }
  }

  uiItemR(layout, ptr, "use_motion_blur", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  if (data->flag & CMP_NODE_PLANE_TRACK_DEFORM_FLAG_MOTION_BLUR) {
    uiItemR(layout, ptr, "motion_blur_samples", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
    uiItemR(layout, ptr, "motion_blur_shutter", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  }
}

using namespace blender::realtime_compositor;

class PlaneTrackDeformOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    MovieTrackingPlaneTrack *plane_track = get_plane_track();

    Result &input_image = get_input("Image");
    Result &output_image = get_result("Image");
    Result &output_mask = get_result("Plane");
    if (input_image.is_single_value() || !plane_track) {
      if (output_image.should_compute()) {
        input_image.pass_through(output_image);
      }
      if (output_mask.should_compute()) {
        output_mask.allocate_single_value();
        output_mask.set_float_value(1.0f);
      }
      return;
    }

    const Array<float4x4> homography_matrices = compute_homography_matrices(plane_track);

    GPUShader *shader = shader_manager().get("compositor_plane_deform_motion_blur");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1i(shader, "number_of_motion_blur_samples", homography_matrices.size());

    GPUUniformBuf *matrices_buffer = GPU_uniformbuf_create_ex(
        homography_matrices.size() * sizeof(float4x4),
        homography_matrices.data(),
        "Plane Track Deform Homography Matrices");
    const int ubo_location = GPU_shader_get_ubo_binding(shader, "homography_matrices");
    GPU_uniformbuf_bind(matrices_buffer, ubo_location);

    GPU_texture_mipmap_mode(input_image.texture(), true, true);
    GPU_texture_anisotropic_filter(input_image.texture(), true);
    GPU_texture_extend_mode(input_image.texture(), GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER);
    input_image.bind_as_texture(shader, "input_tx");

    const Domain domain = compute_domain();
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    output_mask.allocate_texture(domain);
    output_mask.bind_as_image(shader, "mask_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    input_image.unbind_as_texture();
    output_image.unbind_as_image();
    output_mask.unbind_as_image();
    GPU_shader_unbind();

    GPU_uniformbuf_unbind(matrices_buffer);
    GPU_uniformbuf_free(matrices_buffer);
  }

  Domain compute_domain() override
  {
    MovieTrackingPlaneTrack *plane_track = get_plane_track();

    Result &input_image = get_input("Image");
    if (input_image.is_single_value() || !plane_track) {
      return input_image.domain();
    }

    return Domain(get_movie_clip_size());
  }

  Array<float4x4> compute_homography_matrices(MovieTrackingPlaneTrack *plane_track)
  {
    /* We evaluate at the frames in the range [frame - shutter, frame + shutter], if no motion blur
     * is enabled or the motion blur samples is set to 1, we just evaluate at the current frame. */
    const int samples = use_motion_blur() ? node_storage(bnode()).motion_blur_samples : 1;
    const float shutter = samples != 1 ? node_storage(bnode()).motion_blur_shutter : 0.0f;
    const float start_frame = context().get_frame_number() - shutter;
    const float frame_step = (shutter * 2.0f) / samples;

    Array<float4x4> matrices(samples);
    for (int i = 0; i < samples; i++) {
      const float frame = start_frame + frame_step * i;
      const float clip_frame = BKE_movieclip_remap_scene_to_clip_frame(get_movie_clip(), frame);

      float corners[4][2];
      BKE_tracking_plane_marker_get_subframe_corners(plane_track, clip_frame, corners);

      /* Compute a 2D projection matrix that projects from the corners of the image in normalized
       * coordinates into the corners of the tracking plane. */
      float3x3 homography_matrix;
      float identity_corners[4][2] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
      BKE_tracking_homography_between_two_quads(
          corners, identity_corners, homography_matrix.ptr());

      /* Store in a 4x4 matrix due to the alignment requirements of GPU uniform buffers. */
      matrices[i] = float4x4(homography_matrix);
    }

    return matrices;
  }

  MovieTrackingPlaneTrack *get_plane_track()
  {
    MovieClip *movie_clip = get_movie_clip();

    if (!movie_clip) {
      return nullptr;
    }

    MovieTrackingObject *tracking_object = BKE_tracking_object_get_named(
        &movie_clip->tracking, node_storage(bnode()).tracking_object);

    if (!tracking_object) {
      return nullptr;
    }

    return BKE_tracking_object_find_plane_track_with_name(tracking_object,
                                                          node_storage(bnode()).plane_track_name);
  }

  int2 get_movie_clip_size()
  {
    MovieClipUser user = *DNA_struct_default_get(MovieClipUser);
    BKE_movieclip_user_set_frame(&user, context().get_frame_number());

    int2 size;
    BKE_movieclip_get_size(get_movie_clip(), &user, &size.x, &size.y);
    return size;
  }

  bool use_motion_blur()
  {
    return get_flags() & CMP_NODE_PLANE_TRACK_DEFORM_FLAG_MOTION_BLUR;
  }

  CMPNodePlaneTrackDeformFlags get_flags()
  {
    return static_cast<CMPNodePlaneTrackDeformFlags>(node_storage(bnode()).flag);
  }

  MovieClip *get_movie_clip()
  {
    return reinterpret_cast<MovieClip *>(bnode().id);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new PlaneTrackDeformOperation(context, node);
}

}  // namespace blender::nodes::node_composite_planetrackdeform_cc

void register_node_type_cmp_planetrackdeform()
{
  namespace file_ns = blender::nodes::node_composite_planetrackdeform_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_PLANETRACKDEFORM, "Plane Track Deform", NODE_CLASS_DISTORT);
  ntype.declare = file_ns::cmp_node_planetrackdeform_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_planetrackdeform;
  ntype.initfunc_api = file_ns::init;
  node_type_storage(
      &ntype, "NodePlaneTrackDeformData", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}
