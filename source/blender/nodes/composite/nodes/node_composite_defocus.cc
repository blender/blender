/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_base.hh"
#include "BLI_math_vector_types.hh"

#include "DNA_camera_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_camera.h"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "COM_algorithm_morphological_blur.hh"
#include "COM_bokeh_kernel.hh"
#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* ************ Defocus Node ****************** */

namespace blender::nodes::node_composite_defocus_cc {

NODE_STORAGE_FUNCS(NodeDefocus)

static void cmp_node_defocus_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .structure_type(StructureType::Dynamic);
  b.add_input<decl::Float>("Z").default_value(1.0f).min(0.0f).max(1.0f).structure_type(
      StructureType::Dynamic);
  b.add_output<decl::Color>("Image").structure_type(StructureType::Dynamic);
}

static void node_composit_init_defocus(bNodeTree * /*ntree*/, bNode *node)
{
  /* defocus node */
  NodeDefocus *nbd = MEM_callocN<NodeDefocus>(__func__);
  nbd->bktype = 0;
  nbd->rotation = 0.0f;
  nbd->fstop = 128.0f;
  nbd->maxblur = 16;
  nbd->scale = 1.0f;
  nbd->no_zbuf = 1;
  node->storage = nbd;
}

static void node_composit_buts_defocus(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  uiLayout *sub, *col;

  col = &layout->column(false);
  col->label(IFACE_("Bokeh Type:"), ICON_NONE);
  col->prop(ptr, "bokeh", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  col->prop(ptr, "angle", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);

  col = &layout->column(false);
  col->active_set(RNA_boolean_get(ptr, "use_zbuffer") == true);
  col->prop(ptr, "f_stop", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);

  layout->prop(ptr, "blur_max", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);

  uiTemplateID(layout, C, ptr, "scene", nullptr, nullptr, nullptr);

  col = &layout->column(false);
  col->prop(ptr, "use_zbuffer", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
  sub = &col->column(false);
  sub->active_set(RNA_boolean_get(ptr, "use_zbuffer") == false);
  sub->prop(ptr, "z_scale", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
}

using namespace blender::compositor;

class DefocusOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const Result &input = this->get_input("Image");
    Result &output = this->get_result("Image");
    if (input.is_single_value() || node_storage(bnode()).maxblur < 1.0f) {
      output.share_data(input);
      return;
    }

    Result radius = compute_defocus_radius();

    const int maximum_defocus_radius = math::ceil(compute_maximum_defocus_radius());

    /* The special zero value indicate a circle, in which case, the roundness should be set to
     * 1, and the number of sides can be anything and is arbitrarily set to 3. */
    const bool is_circle = node_storage(bnode()).bktype == 0;
    const int2 kernel_size = int2(maximum_defocus_radius * 2 + 1);
    const int sides = is_circle ? 3 : node_storage(bnode()).bktype;
    const float rotation = node_storage(bnode()).rotation;
    const float roundness = is_circle ? 1.0f : 0.0f;
    const Result &bokeh_kernel = context().cache_manager().bokeh_kernels.get(
        context(), kernel_size, sides, rotation, roundness, 0.0f, 0.0f);

    if (this->context().use_gpu()) {
      this->execute_gpu(input, radius, bokeh_kernel, output, maximum_defocus_radius);
    }
    else {
      this->execute_cpu(input, radius, bokeh_kernel, output, maximum_defocus_radius);
    }

    radius.release();
  }

  void execute_gpu(const Result &input,
                   const Result &radius,
                   const Result &bokeh_kernel,
                   Result &output,
                   const int search_radius)
  {
    gpu::Shader *shader = context().get_shader("compositor_defocus_blur");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1i(shader, "search_radius", search_radius);

    input.bind_as_texture(shader, "input_tx");

    radius.bind_as_texture(shader, "radius_tx");

    GPU_texture_filter_mode(bokeh_kernel, true);
    bokeh_kernel.bind_as_texture(shader, "weights_tx");

    const Domain domain = compute_domain();
    output.allocate_texture(domain);
    output.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    input.unbind_as_texture();
    radius.unbind_as_texture();
    bokeh_kernel.unbind_as_texture();
    output.unbind_as_image();
  }

  void execute_cpu(const Result &input,
                   const Result &radius,
                   const Result &bokeh_kernel,
                   Result &output,
                   const int search_radius)
  {
    const Domain domain = compute_domain();
    output.allocate_texture(domain);

    /* Given the texel in the range [-radius, radius] in both axis, load the appropriate weight
     * from the weights image, where the given texel (0, 0) corresponds the center of weights
     * image. Note that we load the weights image inverted along both directions to maintain
     * the shape of the weights if it was not symmetrical. To understand why inversion makes sense,
     * consider a 1D weights image whose right half is all ones and whose left half is all zeros.
     * Further, consider that we are blurring a single white pixel on a black background. When
     * computing the value of a pixel that is to the right of the white pixel, the white pixel will
     * be in the left region of the search window, and consequently, without inversion, a zero will
     * be sampled from the left side of the weights image and result will be zero. However, what
     * we expect is that pixels to the right of the white pixel will be white, that is, they should
     * sample a weight of 1 from the right side of the weights image, hence the need for
     * inversion. */
    auto load_weight = [&](const int2 texel, const float radius) {
      /* Add the radius to transform the texel into the range [0, radius * 2], with an additional
       * 0.5 to sample at the center of the pixels, then divide by the upper bound plus one to
       * transform the texel into the normalized range [0, 1] needed to sample the weights sampler.
       * Finally, invert the textures coordinates by subtracting from 1 to maintain the shape of
       * the weights as mentioned in the function description. */
      return bokeh_kernel.sample_bilinear_extended(
          1.0f - ((float2(texel) + float2(radius + 0.5f)) / (radius * 2.0f + 1.0f)));
    };

    parallel_for(domain.size, [&](const int2 texel) {
      float center_radius = math::max(0.0f, radius.load_pixel<float, true>(texel));

      /* Go over the window of the given search radius and accumulate the colors multiplied by
       * their respective weights as well as the weights themselves, but only if both the radius of
       * the center pixel and the radius of the candidate pixel are less than both the x and y
       * distances of the candidate pixel. */
      float4 accumulated_color = float4(0.0);
      float4 accumulated_weight = float4(0.0);
      for (int y = -search_radius; y <= search_radius; y++) {
        for (int x = -search_radius; x <= search_radius; x++) {
          float candidate_radius = math::max(
              0.0f, radius.load_pixel_extended<float, true>(texel + int2(x, y)));

          /* Skip accumulation if either the x or y distances of the candidate pixel are larger
           * than either the center or candidate pixel radius. Note that the max and min functions
           * here denote "either" in the aforementioned description. */
          float radius = math::min(center_radius, candidate_radius);
          if (math::max(math::abs(x), math::abs(y)) > radius) {
            continue;
          }

          float4 weight = load_weight(int2(x, y), radius);
          float4 input_color = float4(input.load_pixel_extended<Color>(texel + int2(x, y)));

          accumulated_color += input_color * weight;
          accumulated_weight += weight;
        }
      }

      accumulated_color = math::safe_divide(accumulated_color, accumulated_weight);

      output.store_pixel(texel, Color(accumulated_color));
    });
  }

  Result compute_defocus_radius()
  {
    if (node_storage(bnode()).no_zbuf) {
      return compute_defocus_radius_from_scale();
    }
    return compute_defocus_radius_from_depth();
  }

  Result compute_defocus_radius_from_scale()
  {
    Result &input_depth = get_input("Z");
    if (this->context().use_gpu() && !input_depth.is_single_value()) {
      return compute_defocus_radius_from_scale_gpu();
    }
    return compute_defocus_radius_from_scale_cpu();
  }

  Result compute_defocus_radius_from_scale_gpu()
  {
    gpu::Shader *shader = context().get_shader("compositor_defocus_radius_from_scale");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1f(shader, "scale", node_storage(bnode()).scale);
    GPU_shader_uniform_1f(shader, "max_radius", node_storage(bnode()).maxblur);

    Result &input_depth = get_input("Z");
    input_depth.bind_as_texture(shader, "radius_tx");

    Result output_radius = context().create_result(ResultType::Float);
    const Domain domain = input_depth.domain();
    output_radius.allocate_texture(domain);
    output_radius.bind_as_image(shader, "radius_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    input_depth.unbind_as_texture();
    output_radius.unbind_as_image();

    return output_radius;
  }

  Result compute_defocus_radius_from_scale_cpu()
  {
    const float scale = node_storage(bnode()).scale;
    const float max_radius = node_storage(bnode()).maxblur;

    Result &input_depth = get_input("Z");

    Result output_radius = context().create_result(ResultType::Float);

    auto compute_radius = [&](const float depth) {
      return math::clamp(depth * scale, 0.0f, max_radius);
    };

    if (input_depth.is_single_value()) {
      output_radius.allocate_single_value();
      output_radius.set_single_value(compute_radius(input_depth.get_single_value<float>()));
      return output_radius;
    }

    const Domain domain = input_depth.domain();
    output_radius.allocate_texture(domain);

    parallel_for(domain.size, [&](const int2 texel) {
      float depth = input_depth.load_pixel<float>(texel);
      output_radius.store_pixel(texel, compute_radius(depth));
    });

    return output_radius;
  }

  Result compute_defocus_radius_from_depth()
  {
    Result &input_depth = get_input("Z");
    Result output_radius = context().create_result(ResultType::Float);
    if (this->context().use_gpu() && !input_depth.is_single_value()) {
      compute_defocus_radius_from_depth_gpu(output_radius);
    }
    else {
      compute_defocus_radius_from_depth_cpu(output_radius);
    }

    if (output_radius.is_single_value()) {
      return output_radius;
    }

    /* We apply a dilate morphological operator on the radius computed from depth, the operator
     * radius is the maximum possible defocus radius. This is done such that objects in
     * focus---that is, objects whose defocus radius is small---are not affected by nearby out of
     * focus objects, hence the use of erosion. */
    const float morphological_radius = compute_maximum_defocus_radius();
    Result eroded_radius = context().create_result(ResultType::Float);
    morphological_blur(context(), output_radius, eroded_radius, float2(morphological_radius));
    output_radius.release();

    return eroded_radius;
  }

  void compute_defocus_radius_from_depth_gpu(Result &output_radius)
  {
    gpu::Shader *shader = context().get_shader("compositor_defocus_radius_from_depth");
    GPU_shader_bind(shader);

    const float distance_to_image_of_focus = compute_distance_to_image_of_focus();
    GPU_shader_uniform_1f(shader, "f_stop", get_f_stop());
    GPU_shader_uniform_1f(shader, "focal_length", get_focal_length());
    GPU_shader_uniform_1f(shader, "max_radius", node_storage(bnode()).maxblur);
    GPU_shader_uniform_1f(shader, "pixels_per_meter", compute_pixels_per_meter());
    GPU_shader_uniform_1f(shader, "distance_to_image_of_focus", distance_to_image_of_focus);

    Result &input_depth = get_input("Z");
    input_depth.bind_as_texture(shader, "depth_tx");

    const Domain domain = input_depth.domain();
    output_radius.allocate_texture(domain);
    output_radius.bind_as_image(shader, "radius_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    input_depth.unbind_as_texture();
    output_radius.unbind_as_image();
  }

  void compute_defocus_radius_from_depth_cpu(Result &output_radius)
  {
    const float f_stop = this->get_f_stop();
    const float focal_length = this->get_focal_length();
    const float max_radius = node_storage(this->bnode()).maxblur;
    const float pixels_per_meter = this->compute_pixels_per_meter();
    const float distance_to_image_of_focus = this->compute_distance_to_image_of_focus();

    Result &input_depth = get_input("Z");

    /* Given a depth value, compute the radius of the circle of confusion in pixels based on
     * equation (8) of the paper:
     *
     *   Potmesil, Michael, and Indranil Chakravarty. "A lens and aperture camera model for
     * synthetic image generation." ACM SIGGRAPH Computer Graphics 15.3 (1981): 297-305. */
    auto compute_radius = [&](const float depth) {
      /* Compute `Vu` in equation (7). */
      const float distance_to_image_of_object = (focal_length * depth) / (depth - focal_length);

      /* Compute C in equation (8). Notice that the last multiplier was included in the absolute
       * since it is negative when the object distance is less than the focal length, as noted in
       * equation (7). */
      float diameter = math::abs((distance_to_image_of_object - distance_to_image_of_focus) *
                                 (focal_length / (f_stop * distance_to_image_of_object)));

      /* The diameter is in meters, so multiply by the pixels per meter. */
      float radius = (diameter / 2.0f) * pixels_per_meter;

      return math::min(max_radius, radius);
    };

    if (input_depth.is_single_value()) {
      output_radius.allocate_single_value();
      output_radius.set_single_value(compute_radius(input_depth.get_single_value<float>()));
      return;
    }

    const Domain domain = input_depth.domain();
    output_radius.allocate_texture(domain);

    parallel_for(domain.size, [&](const int2 texel) {
      float depth = input_depth.load_pixel<float>(texel);
      output_radius.store_pixel(texel, compute_radius(depth));
    });
  }

  /* Computes the maximum possible defocus radius in pixels. */
  float compute_maximum_defocus_radius()
  {
    if (node_storage(bnode()).no_zbuf) {
      return node_storage(bnode()).maxblur;
    }

    const float maximum_diameter = compute_maximum_diameter_of_circle_of_confusion();
    const float pixels_per_meter = compute_pixels_per_meter();
    const float radius = (maximum_diameter / 2.0f) * pixels_per_meter;
    return math::min(radius, node_storage(bnode()).maxblur);
  }

  /* Computes the diameter of the circle of confusion at infinity. This computes the limit in
   * figure (5) of the paper:
   *
   *   Potmesil, Michael, and Indranil Chakravarty. "A lens and aperture camera model for synthetic
   *   image generation." ACM SIGGRAPH Computer Graphics 15.3 (1981): 297-305.
   *
   * Notice that the diameter is asymmetric around the focus point, and we are computing the
   * limiting diameter at infinity, while another limiting diameter exist at zero distance from the
   * lens. This is a limitation of the implementation, as it assumes far defocusing only. */
  float compute_maximum_diameter_of_circle_of_confusion()
  {
    const float f_stop = get_f_stop();
    const float focal_length = get_focal_length();
    const float distance_to_image_of_focus = compute_distance_to_image_of_focus();
    return math::abs((distance_to_image_of_focus / (f_stop * focal_length)) -
                     (focal_length / f_stop));
  }

  /* Computes the distance in meters to the image of the focus point across a lens of the specified
   * focal length. This computes `Vp` in equation (7) of the paper:
   *
   *   Potmesil, Michael, and Indranil Chakravarty. "A lens and aperture camera model for synthetic
   *   image generation." ACM SIGGRAPH Computer Graphics 15.3 (1981): 297-305. */
  float compute_distance_to_image_of_focus()
  {
    const float focal_length = get_focal_length();
    const float focus_distance = compute_focus_distance();
    return (focal_length * focus_distance) / (focus_distance - focal_length);
  }

  /* Returns the focal length in meters. Fall back to 50 mm in case of an invalid camera. Ensure a
   * minimum of 1e-6. */
  float get_focal_length()
  {
    const Camera *camera = get_camera();
    return camera ? math::max(1e-6f, camera->lens / 1000.0f) : 50.0f / 1000.0f;
  }

  /* Computes the distance to the point that is completely in focus. Default to 10 meters for null
   * camera. */
  float compute_focus_distance()
  {
    const Object *camera_object = get_camera_object();
    if (!camera_object) {
      return 10.0f;
    }
    return BKE_camera_object_dof_distance(camera_object);
  }

  /* Computes the number of pixels per meter of the sensor size. This is essentially the resolution
   * over the sensor size, using the sensor fit axis. Fall back to DEFAULT_SENSOR_WIDTH in case of
   * an invalid camera. Note that the stored sensor size is in millimeter, so convert to meters. */
  float compute_pixels_per_meter()
  {
    const int2 size = compute_domain().size;
    const Camera *camera = get_camera();
    const float default_value = size.x / (DEFAULT_SENSOR_WIDTH / 1000.0f);
    if (!camera) {
      return default_value;
    }

    switch (camera->sensor_fit) {
      case CAMERA_SENSOR_FIT_HOR:
        return size.x / (camera->sensor_x / 1000.0f);
      case CAMERA_SENSOR_FIT_VERT:
        return size.y / (camera->sensor_y / 1000.0f);
      case CAMERA_SENSOR_FIT_AUTO: {
        return size.x > size.y ? size.x / (camera->sensor_x / 1000.0f) :
                                 size.y / (camera->sensor_y / 1000.0f);
      }
      default:
        break;
    }

    return default_value;
  }

  /* Returns the f-stop number. Fall back to 1e-3 for zero f-stop. */
  float get_f_stop()
  {
    return math::max(1e-3f, node_storage(bnode()).fstop);
  }

  const Camera *get_camera()
  {
    const Object *camera_object = get_camera_object();
    if (!camera_object || camera_object->type != OB_CAMERA) {
      return nullptr;
    }

    return reinterpret_cast<Camera *>(camera_object->data);
  }

  const Object *get_camera_object()
  {
    return get_scene()->camera;
  }

  const Scene *get_scene()
  {
    return bnode().id ? reinterpret_cast<Scene *>(bnode().id) : &context().get_scene();
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new DefocusOperation(context, node);
}

}  // namespace blender::nodes::node_composite_defocus_cc

static void register_node_type_cmp_defocus()
{
  namespace file_ns = blender::nodes::node_composite_defocus_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeDefocus", CMP_NODE_DEFOCUS);
  ntype.ui_name = "Defocus";
  ntype.ui_description = "Apply depth of field in 2D, using a Z depth map or mask";
  ntype.enum_name_legacy = "DEFOCUS";
  ntype.nclass = NODE_CLASS_OP_FILTER;
  ntype.declare = file_ns::cmp_node_defocus_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_defocus;
  ntype.initfunc = file_ns::node_composit_init_defocus;
  blender::bke::node_type_storage(
      ntype, "NodeDefocus", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_defocus)
