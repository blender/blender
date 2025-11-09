/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_array.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_string_utf8.h"

#include "DNA_defaults.h"
#include "DNA_movieclip_types.h"
#include "DNA_tracking_types.h"

#include "BKE_context.hh"
#include "BKE_lib_id.hh"
#include "BKE_movieclip.h"
#include "BKE_tracking.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "GPU_shader.hh"
#include "GPU_texture.hh"
#include "GPU_uniform_buffer.hh"

#include "COM_algorithm_smaa.hh"
#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_planetrackdeform_cc {

NODE_STORAGE_FUNCS(NodePlaneTrackDeformData)

static void cmp_node_planetrackdeform_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  b.add_input<decl::Color>("Image")
      .hide_value()
      .compositor_realization_mode(CompositorInputRealizationMode::Transforms)
      .structure_type(StructureType::Dynamic);
  b.add_output<decl::Color>("Image").structure_type(StructureType::Dynamic).align_with_previous();
  b.add_output<decl::Float>("Plane").structure_type(StructureType::Dynamic);

  b.add_layout([](uiLayout *layout, bContext *C, PointerRNA *ptr) {
    bNode *node = ptr->data_as<bNode>();

    uiTemplateID(layout, C, ptr, "clip", nullptr, "CLIP_OT_open", nullptr);

    if (node->id) {
      MovieClip *clip = reinterpret_cast<MovieClip *>(node->id);
      MovieTracking *tracking = &clip->tracking;
      MovieTrackingObject *tracking_object;
      PointerRNA tracking_ptr = RNA_pointer_create_discrete(
          &clip->id, &RNA_MovieTracking, tracking);

      uiLayout *col = &layout->column(false);
      col->prop_search(ptr, "tracking_object", &tracking_ptr, "objects", "", ICON_OBJECT_DATA);

      tracking_object = BKE_tracking_object_get_named(tracking,
                                                      node_storage(*node).tracking_object);
      if (tracking_object) {
        PointerRNA object_ptr = RNA_pointer_create_discrete(
            &clip->id, &RNA_MovieTrackingObject, tracking_object);

        col->prop_search(ptr, "plane_track_name", &object_ptr, "plane_tracks", "", ICON_ANIM_DATA);
      }
      else {
        layout->prop(ptr, "plane_track_name", UI_ITEM_NONE, "", ICON_ANIM_DATA);
      }
    }
  });

  PanelDeclarationBuilder &motion_blur_panel = b.add_panel("Motion Blur").default_closed(true);
  motion_blur_panel.add_input<decl::Bool>("Motion Blur")
      .default_value(false)
      .panel_toggle()
      .description("Use multi-sampled motion blur of the plane");
  motion_blur_panel.add_input<decl::Int>("Samples", "Motion Blur Samples")
      .default_value(16)
      .min(1)
      .max(64)
      .description("Number of motion blur samples");
  motion_blur_panel.add_input<decl::Float>("Shutter", "Motion Blur Shutter")
      .default_value(0.5f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(1.0f)
      .description("Exposure for motion blur as a factor of FPS");
}

static void init(const bContext *C, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;

  NodePlaneTrackDeformData *data = MEM_callocN<NodePlaneTrackDeformData>(__func__);
  node->storage = data;

  const Scene *scene = CTX_data_scene(C);
  if (scene->clip) {
    MovieClip *clip = scene->clip;
    MovieTracking *tracking = &clip->tracking;

    node->id = &clip->id;
    id_us_plus(&clip->id);

    const MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);
    STRNCPY_UTF8(data->tracking_object, tracking_object->name);

    if (tracking_object->active_plane_track) {
      STRNCPY_UTF8(data->plane_track_name, tracking_object->active_plane_track->name);
    }
  }
}

using namespace blender::compositor;

class PlaneTrackDeformOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    MovieTrackingPlaneTrack *plane_track = get_plane_track();

    const Result &input_image = get_input("Image");
    if (input_image.is_single_value() || !plane_track) {
      Result &output_image = get_result("Image");
      if (output_image.should_compute()) {
        output_image.share_data(input_image);
      }

      Result &output_mask = get_result("Plane");
      if (output_mask.should_compute()) {
        output_mask.allocate_single_value();
        output_mask.set_single_value(1.0f);
      }
      return;
    }

    const Array<float4x4> homography_matrices = compute_homography_matrices(plane_track);

    if (this->context().use_gpu()) {
      this->execute_gpu(homography_matrices);
    }
    else {
      this->execute_cpu(homography_matrices);
    }
  }

  void execute_gpu(const Array<float4x4> homography_matrices)
  {
    gpu::UniformBuf *homography_matrices_buffer = GPU_uniformbuf_create_ex(
        homography_matrices.size() * sizeof(float4x4),
        homography_matrices.data(),
        "Plane Track Deform Homography Matrices");

    Result plane_mask = this->compute_plane_mask_gpu(homography_matrices,
                                                     homography_matrices_buffer);
    Result anti_aliased_plane_mask = this->context().create_result(ResultType::Float);
    smaa(context(), plane_mask, anti_aliased_plane_mask);
    plane_mask.release();

    Result &output_image = get_result("Image");
    if (output_image.should_compute()) {
      this->compute_plane_gpu(
          homography_matrices, homography_matrices_buffer, anti_aliased_plane_mask);
    }

    GPU_uniformbuf_free(homography_matrices_buffer);

    Result &output_mask = get_result("Plane");
    if (output_mask.should_compute()) {
      output_mask.steal_data(anti_aliased_plane_mask);
    }
    else {
      anti_aliased_plane_mask.release();
    }
  }

  void compute_plane_gpu(const Array<float4x4> &homography_matrices,
                         gpu::UniformBuf *homography_matrices_buffer,
                         Result &plane_mask)
  {
    gpu::Shader *shader = context().get_shader("compositor_plane_deform_motion_blur");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1i(shader, "number_of_motion_blur_samples", homography_matrices.size());

    const int ubo_location = GPU_shader_get_ubo_binding(shader, "homography_matrices");
    GPU_uniformbuf_bind(homography_matrices_buffer, ubo_location);

    Result &input_image = get_input("Image");
    GPU_texture_mipmap_mode(input_image, true, true);
    GPU_texture_anisotropic_filter(input_image, true);
    /* We actually need zero boundary conditions, but we sampled using extended boundaries then
     * multiply by the anti-aliased plane mask to get better quality anti-aliased planes. */
    GPU_texture_extend_mode(input_image, GPU_SAMPLER_EXTEND_MODE_EXTEND);
    input_image.bind_as_texture(shader, "input_tx");

    plane_mask.bind_as_texture(shader, "mask_tx");

    const Domain domain = compute_domain();
    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    input_image.unbind_as_texture();
    plane_mask.unbind_as_texture();
    output_image.unbind_as_image();
    GPU_uniformbuf_unbind(homography_matrices_buffer);
    GPU_shader_unbind();
  }

  Result compute_plane_mask_gpu(const Array<float4x4> &homography_matrices,
                                gpu::UniformBuf *homography_matrices_buffer)
  {
    gpu::Shader *shader = context().get_shader("compositor_plane_deform_motion_blur_mask");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1i(shader, "number_of_motion_blur_samples", homography_matrices.size());

    const int ubo_location = GPU_shader_get_ubo_binding(shader, "homography_matrices");
    GPU_uniformbuf_bind(homography_matrices_buffer, ubo_location);

    const Domain domain = compute_domain();
    Result plane_mask = context().create_result(ResultType::Float);
    plane_mask.allocate_texture(domain);
    plane_mask.bind_as_image(shader, "mask_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    plane_mask.unbind_as_image();
    GPU_uniformbuf_unbind(homography_matrices_buffer);
    GPU_shader_unbind();

    return plane_mask;
  }

  void execute_cpu(const Array<float4x4> homography_matrices)
  {
    Result plane_mask = this->compute_plane_mask_cpu(homography_matrices);
    Result anti_aliased_plane_mask = this->context().create_result(ResultType::Float);
    smaa(context(), plane_mask, anti_aliased_plane_mask);
    plane_mask.release();

    Result &output_image = get_result("Image");
    if (output_image.should_compute()) {
      this->compute_plane_cpu(homography_matrices, anti_aliased_plane_mask);
    }

    Result &output_mask = get_result("Plane");
    if (output_mask.should_compute()) {
      output_mask.steal_data(anti_aliased_plane_mask);
    }
    else {
      anti_aliased_plane_mask.release();
    }
  }

  void compute_plane_cpu(const Array<float4x4> &homography_matrices, Result &plane_mask)
  {
    Result &input = get_input("Image");

    const Domain domain = compute_domain();
    Result &output = get_result("Image");
    output.allocate_texture(domain);

    const int2 size = domain.size;
    parallel_for(size, [&](const int2 texel) {
      float2 coordinates = (float2(texel) + float2(0.5f)) / float2(size);

      float4 accumulated_color = float4(0.0f);
      for (const float4x4 &homography_matrix : homography_matrices) {
        float3 transformed_coordinates = float3x3(homography_matrix) * float3(coordinates, 1.0f);
        /* Point is at infinity and will be zero when sampled, so early exit. */
        if (transformed_coordinates.z == 0.0f) {
          continue;
        }
        float2 projected_coordinates = transformed_coordinates.xy() / transformed_coordinates.z;

        /* The derivatives of the projected coordinates with respect to x and y are the first and
         * second columns respectively, divided by the z projection factor as can be shown by
         * differentiating the above matrix multiplication with respect to x and y. Divide by the
         * output size since sample_ewa assumes derivatives with respect to texel coordinates. */
        float2 x_gradient = (homography_matrix[0].xy() / transformed_coordinates.z) / size.x;
        float2 y_gradient = (homography_matrix[1].xy() / transformed_coordinates.z) / size.y;

        float4 sampled_color = input.sample_ewa_extended(
            projected_coordinates, x_gradient, y_gradient);
        accumulated_color += sampled_color;
      }

      accumulated_color /= homography_matrices.size();

      /* Premultiply the mask value as an alpha. */
      float4 plane_color = accumulated_color * plane_mask.load_pixel<float>(texel);

      output.store_pixel(texel, Color(plane_color));
    });
  }

  Result compute_plane_mask_cpu(const Array<float4x4> &homography_matrices)
  {
    const Domain domain = compute_domain();
    Result plane_mask = context().create_result(ResultType::Float);
    plane_mask.allocate_texture(domain);

    const int2 size = domain.size;
    parallel_for(size, [&](const int2 texel) {
      float2 coordinates = (float2(texel) + float2(0.5f)) / float2(size);

      float accumulated_mask = 0.0f;
      for (const float4x4 &homography_matrix : homography_matrices) {
        float3 transformed_coordinates = float3x3(homography_matrix) * float3(coordinates, 1.0f);
        /* Point is at infinity and will be zero when sampled, so early exit. */
        if (transformed_coordinates.z == 0.0f) {
          continue;
        }
        float2 projected_coordinates = transformed_coordinates.xy() / transformed_coordinates.z;

        bool is_inside_plane = projected_coordinates.x >= 0.0f &&
                               projected_coordinates.y >= 0.0f &&
                               projected_coordinates.x <= 1.0f && projected_coordinates.y <= 1.0f;
        accumulated_mask += is_inside_plane ? 1.0f : 0.0f;
      }

      accumulated_mask /= homography_matrices.size();

      plane_mask.store_pixel(texel, accumulated_mask);
    });

    return plane_mask;
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
    const int samples = this->get_motion_blur_samples();
    const float shutter = samples != 1 ? this->get_motion_blur_shutter() : 0.0f;
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

  int get_motion_blur_samples()
  {
    const int samples = math::clamp(
        this->get_input("Motion Blur Samples").get_single_value_default(16), 1, 64);
    return this->use_motion_blur() ? samples : 1;
  }

  float get_motion_blur_shutter()
  {
    return math::clamp(
        this->get_input("Motion Blur Shutter").get_single_value_default(0.5f), 0.0f, 1.0f);
  }

  bool use_motion_blur()
  {
    return this->get_input("Motion Blur").get_single_value_default(false);
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

static void register_node_type_cmp_planetrackdeform()
{
  namespace file_ns = blender::nodes::node_composite_planetrackdeform_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodePlaneTrackDeform", CMP_NODE_PLANETRACKDEFORM);
  ntype.ui_name = "Plane Track Deform";
  ntype.ui_description =
      "Replace flat planes in footage by another image, detected by plane tracks from motion "
      "tracking";
  ntype.enum_name_legacy = "PLANETRACKDEFORM";
  ntype.nclass = NODE_CLASS_DISTORT;
  ntype.declare = file_ns::cmp_node_planetrackdeform_declare;
  ntype.initfunc_api = file_ns::init;
  blender::bke::node_type_storage(
      ntype, "NodePlaneTrackDeformData", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_planetrackdeform)
