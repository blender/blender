/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include <climits>

#include "DNA_camera_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_camera.h"

#include "RNA_access.hh"

#include "UI_interface.hh"
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
      .compositor_domain_priority(0);
  b.add_input<decl::Float>("Z").default_value(1.0f).min(0.0f).max(1.0f).compositor_domain_priority(
      1);
  b.add_output<decl::Color>("Image");
}

static void node_composit_init_defocus(bNodeTree * /*ntree*/, bNode *node)
{
  /* defocus node */
  NodeDefocus *nbd = MEM_cnew<NodeDefocus>(__func__);
  nbd->bktype = 0;
  nbd->rotation = 0.0f;
  nbd->preview = 1;
  nbd->gamco = 0;
  nbd->samples = 16;
  nbd->fstop = 128.0f;
  nbd->maxblur = 16;
  nbd->bthresh = 1.0f;
  nbd->scale = 1.0f;
  nbd->no_zbuf = 1;
  node->storage = nbd;
}

static void node_composit_buts_defocus(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  uiLayout *sub, *col;

  col = uiLayoutColumn(layout, false);
  uiItemL(col, IFACE_("Bokeh Type:"), ICON_NONE);
  uiItemR(col, ptr, "bokeh", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  uiItemR(col, ptr, "angle", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);

  uiItemR(layout, ptr, "use_gamma_correction", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);

  col = uiLayoutColumn(layout, false);
  uiLayoutSetActive(col, RNA_boolean_get(ptr, "use_zbuffer") == true);
  uiItemR(col, ptr, "f_stop", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);

  uiItemR(layout, ptr, "blur_max", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "threshold", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "use_preview", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);

  uiTemplateID(layout,
               C,
               ptr,
               "scene",
               nullptr,
               nullptr,
               nullptr,
               UI_TEMPLATE_ID_FILTER_ALL,
               false,
               nullptr);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "use_zbuffer", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  sub = uiLayoutColumn(col, false);
  uiLayoutSetActive(sub, RNA_boolean_get(ptr, "use_zbuffer") == false);
  uiItemR(sub, ptr, "z_scale", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class DefocusOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    Result &input = get_input("Image");
    Result &output = get_result("Image");
    if (input.is_single_value() || node_storage(bnode()).maxblur < 1.0f) {
      input.pass_through(output);
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
    const BokehKernel &bokeh_kernel = context().cache_manager().bokeh_kernels.get(
        context(), kernel_size, sides, rotation, roundness, 0.0f, 0.0f);

    GPUShader *shader = context().get_shader("compositor_defocus_blur");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1b(shader, "gamma_correct", node_storage(bnode()).gamco);
    GPU_shader_uniform_1i(shader, "search_radius", maximum_defocus_radius);

    input.bind_as_texture(shader, "input_tx");

    radius.bind_as_texture(shader, "radius_tx");

    GPU_texture_filter_mode(bokeh_kernel.texture(), true);
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

    radius.release();
  }

  Result compute_defocus_radius()
  {
    if (node_storage(bnode()).no_zbuf) {
      return compute_defocus_radius_from_scale();
    }
    else {
      return compute_defocus_radius_from_depth();
    }
  }

  Result compute_defocus_radius_from_scale()
  {
    GPUShader *shader = context().get_shader("compositor_defocus_radius_from_scale");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1f(shader, "scale", node_storage(bnode()).scale);
    GPU_shader_uniform_1f(shader, "max_radius", node_storage(bnode()).maxblur);

    Result &input_radius = get_input("Z");
    input_radius.bind_as_texture(shader, "radius_tx");

    Result output_radius = context().create_temporary_result(ResultType::Float);
    const Domain domain = input_radius.domain();
    output_radius.allocate_texture(domain);
    output_radius.bind_as_image(shader, "radius_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    input_radius.unbind_as_texture();
    output_radius.unbind_as_image();

    return output_radius;
  }

  Result compute_defocus_radius_from_depth()
  {
    GPUShader *shader = context().get_shader("compositor_defocus_radius_from_depth");
    GPU_shader_bind(shader);

    const float distance_to_image_of_focus = compute_distance_to_image_of_focus();
    GPU_shader_uniform_1f(shader, "f_stop", get_f_stop());
    GPU_shader_uniform_1f(shader, "focal_length", get_focal_length());
    GPU_shader_uniform_1f(shader, "max_radius", node_storage(bnode()).maxblur);
    GPU_shader_uniform_1f(shader, "pixels_per_meter", compute_pixels_per_meter());
    GPU_shader_uniform_1f(shader, "distance_to_image_of_focus", distance_to_image_of_focus);

    Result &input_depth = get_input("Z");
    input_depth.bind_as_texture(shader, "depth_tx");

    Result output_radius = context().create_temporary_result(ResultType::Float);
    const Domain domain = input_depth.domain();
    output_radius.allocate_texture(domain);
    output_radius.bind_as_image(shader, "radius_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    input_depth.unbind_as_texture();
    output_radius.unbind_as_image();

    /* We apply a dilate morphological operator on the radius computed from depth, the operator
     * radius is the maximum possible defocus radius. This is done such that objects in
     * focus---that is, objects whose defocus radius is small---are not affected by nearby out of
     * focus objects, hence the use of dilation. */
    const float morphological_radius = compute_maximum_defocus_radius();
    Result eroded_radius = context().create_temporary_result(ResultType::Float);
    morphological_blur(context(), output_radius, eroded_radius, float2(morphological_radius));
    output_radius.release();

    return eroded_radius;
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

  /* Returns the focal length in meters. Fallback to 50 mm in case of an invalid camera. Ensure a
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
   * over the sensor size, using the sensor fit axis. Fallback to DEFAULT_SENSOR_WIDTH in case of
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

  /* Returns the f-stop number. Fallback to 1e-3 for zero f-stop. */
  const float get_f_stop()
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

void register_node_type_cmp_defocus()
{
  namespace file_ns = blender::nodes::node_composite_defocus_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_DEFOCUS, "Defocus", NODE_CLASS_OP_FILTER);
  ntype.declare = file_ns::cmp_node_defocus_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_defocus;
  ntype.initfunc = file_ns::node_composit_init_defocus;
  blender::bke::node_type_storage(
      &ntype, "NodeDefocus", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::nodeRegisterType(&ntype);
}
