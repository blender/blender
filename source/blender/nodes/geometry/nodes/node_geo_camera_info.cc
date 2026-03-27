/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_camera_types.h"

#include "BKE_camera.h"

#include "DEG_depsgraph_query.hh"

#include "COM_node_operation.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_camera_info_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();

  b.add_output<decl::Matrix>("Projection Matrix").description("Camera projection matrix");
  b.add_output<decl::Float>("Focal Length").description("Perspective camera focal length");
  b.add_output<decl::Vector>("Sensor").dimensions(2).description("Size of the camera sensor");
  b.add_output<decl::Vector>("Shift").dimensions(2).description("Camera shift");
  b.add_output<decl::Float>("Clip Start").description("Camera near clipping distance");
  b.add_output<decl::Float>("Clip End").description("Camera far clipping distance");
  b.add_output<decl::Float>("Focus Distance")
      .description("Distance to the focus point for depth of field");
  b.add_output<decl::Bool>("Is Orthographic")
      .description("Whether the camera is using orthographic projection");
  b.add_output<decl::Float>("Orthographic Scale")
      .description("Orthographic camera scale (similar to zoom)");

  b.add_input<decl::Object>("Camera").optional_label();
}

static CameraParams get_camera_parameters(const Scene &scene, const Object &camera_object)
{
  CameraParams camera_params;
  BKE_camera_params_init(&camera_params);
  BKE_camera_params_from_object(&camera_params, &camera_object);
  BKE_camera_params_compute_viewplane(
      &camera_params, scene.r.xsch, scene.r.ysch, scene.r.xasp, scene.r.yasp);
  BKE_camera_params_compute_matrix(&camera_params);
  return camera_params;
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const Scene *scene = DEG_get_evaluated_scene(params.depsgraph());
  if (!scene) {
    params.set_default_remaining_outputs();
    return;
  }

  const Object *camera_obj = params.extract_input<Object *>("Camera"_ustr);

  if (!camera_obj || camera_obj->type != OB_CAMERA) {
    params.set_default_remaining_outputs();
    return;
  }

  Camera *camera = id_cast<Camera *>(camera_obj->data);
  if (!camera) {
    params.set_default_remaining_outputs();
    return;
  }

  const CameraParams camera_params = get_camera_parameters(*scene, *camera_obj);
  const float4x4 projection_matrix(camera_params.winmat);
  float focus_distance = BKE_camera_object_dof_distance(camera_obj);

  params.set_output("Projection Matrix"_ustr, projection_matrix);
  params.set_output("Focal Length"_ustr, camera_params.lens);
  params.set_output("Sensor"_ustr, float3{camera_params.sensor_x, camera_params.sensor_y, 0.0f});
  params.set_output("Shift"_ustr, float3{camera_params.shiftx, camera_params.shifty, 0.0f});
  params.set_output("Clip Start"_ustr, camera_params.clip_start);
  params.set_output("Clip End"_ustr, camera_params.clip_end);
  params.set_output("Focus Distance"_ustr, focus_distance);
  params.set_output("Is Orthographic"_ustr, camera_params.is_ortho);
  params.set_output("Orthographic Scale"_ustr, camera_params.ortho_scale);
}

using namespace blender::compositor;

class CameraInfoOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const Object *camera_object = this->get_input("Camera").get_single_value<Object *>();
    if (!camera_object || camera_object->type != OB_CAMERA) {
      this->allocate_default_remaining_outputs();
      return;
    }

    const Camera *camera = id_cast<Camera *>(camera_object->data);
    if (!camera) {
      this->allocate_default_remaining_outputs();
      return;
    }

    const Scene &scene = this->context().get_scene();
    const CameraParams camera_parameters = get_camera_parameters(scene, *camera_object);

    Result &projection_matrix_result = this->get_result("Projection Matrix");
    if (projection_matrix_result.should_compute()) {
      projection_matrix_result.allocate_single_value();
      projection_matrix_result.set_single_value(float4x4(camera_parameters.winmat));
    }

    Result &focal_length_result = this->get_result("Focal Length");
    if (focal_length_result.should_compute()) {
      focal_length_result.allocate_single_value();
      focal_length_result.set_single_value(camera_parameters.lens);
    }

    Result &sensor_result = this->get_result("Sensor");
    if (sensor_result.should_compute()) {
      sensor_result.allocate_single_value();
      sensor_result.set_single_value(
          float2(camera_parameters.sensor_x, camera_parameters.sensor_y));
    }

    Result &shift_result = this->get_result("Shift");
    if (shift_result.should_compute()) {
      shift_result.allocate_single_value();
      shift_result.set_single_value(float2(camera_parameters.shiftx, camera_parameters.shifty));
    }

    Result &clip_start_result = this->get_result("Clip Start");
    if (clip_start_result.should_compute()) {
      clip_start_result.allocate_single_value();
      clip_start_result.set_single_value(camera_parameters.clip_start);
    }

    Result &clip_end_result = this->get_result("Clip End");
    if (clip_end_result.should_compute()) {
      clip_end_result.allocate_single_value();
      clip_end_result.set_single_value(camera_parameters.clip_end);
    }

    Result &focus_distance_result = this->get_result("Focus Distance");
    if (focus_distance_result.should_compute()) {
      focus_distance_result.allocate_single_value();
      const float focus_distance = BKE_camera_object_dof_distance(camera_object);
      focus_distance_result.set_single_value(focus_distance);
    }

    Result &is_orthographic_result = this->get_result("Is Orthographic");
    if (is_orthographic_result.should_compute()) {
      is_orthographic_result.allocate_single_value();
      is_orthographic_result.set_single_value(camera_parameters.is_ortho);
    }

    Result &orthographic_scale_result = this->get_result("Orthographic Scale");
    if (orthographic_scale_result.should_compute()) {
      orthographic_scale_result.allocate_single_value();
      orthographic_scale_result.set_single_value(camera_parameters.ortho_scale);
    }
  }
};

static NodeOperation *get_compositor_operation(Context &context, const bNode &node)
{
  return new CameraInfoOperation(context, node);
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_cmp_node_type_base(&ntype, "GeometryNodeCameraInfo");
  ntype.ui_name = "Camera Info";
  ntype.ui_description = "Retrieve information from a camera object";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  ntype.get_compositor_operation = get_compositor_operation;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_camera_info_cc
