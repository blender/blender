/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <optional>

#include "scene/attribute.h"
#include "scene/pointcloud.h"
#include "scene/scene.h"

#include "blender/sync.h"
#include "blender/util.h"

#include "util/color.h"
#include "util/foreach.h"
#include "util/hash.h"

CCL_NAMESPACE_BEGIN

static void attr_create_motion(PointCloud *pointcloud,
                               BL::Attribute &b_attribute,
                               const float motion_scale)
{
  if (!(b_attribute.domain() == BL::Attribute::domain_POINT) &&
      (b_attribute.data_type() == BL::Attribute::data_type_FLOAT_VECTOR))
  {
    return;
  }

  BL::FloatVectorAttribute b_vector_attribute(b_attribute);
  const int num_points = pointcloud->get_points().size();

  /* Find or add attribute */
  float3 *P = &pointcloud->get_points()[0];
  Attribute *attr_mP = pointcloud->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);

  if (!attr_mP) {
    attr_mP = pointcloud->attributes.add(ATTR_STD_MOTION_VERTEX_POSITION);
  }

  /* Only export previous and next frame, we don't have any in between data. */
  float motion_times[2] = {-1.0f, 1.0f};
  for (int step = 0; step < 2; step++) {
    const float relative_time = motion_times[step] * 0.5f * motion_scale;
    float3 *mP = attr_mP->data_float3() + step * num_points;

    for (int i = 0; i < num_points; i++) {
      mP[i] = P[i] + get_float3(b_vector_attribute.data[i].vector()) * relative_time;
    }
  }
}

static void copy_attributes(PointCloud *pointcloud,
                            BL::PointCloud b_pointcloud,
                            const bool need_motion,
                            const float motion_scale)
{
  const int num_points = b_pointcloud.points.length();
  if (num_points == 0) {
    return;
  }

  AttributeSet &attributes = pointcloud->attributes;
  static const ustring u_velocity("velocity");
  for (BL::Attribute &b_attribute : b_pointcloud.attributes) {
    const ustring name{b_attribute.name().c_str()};

    if (need_motion && name == u_velocity) {
      attr_create_motion(pointcloud, b_attribute, motion_scale);
    }

    if (attributes.find(name)) {
      continue;
    }

    const AttributeElement element = ATTR_ELEMENT_VERTEX;
    const BL::Attribute::data_type_enum b_data_type = b_attribute.data_type();
    switch (b_data_type) {
      case BL::Attribute::data_type_FLOAT: {
        BL::FloatAttribute b_float_attribute{b_attribute};
        const float *src = static_cast<const float *>(b_float_attribute.data[0].ptr.data);
        Attribute *attr = attributes.add(name, TypeFloat, element);
        float *data = attr->data_float();
        std::copy(src, src + num_points, data);
        break;
      }
      case BL::Attribute::data_type_BOOLEAN: {
        BL::BoolAttribute b_bool_attribute{b_attribute};
        const bool *src = static_cast<const bool *>(b_bool_attribute.data[0].ptr.data);
        Attribute *attr = attributes.add(name, TypeFloat, element);
        float *data = attr->data_float();
        for (int i = 0; i < num_points; i++) {
          data[i] = float(src[i]);
        }
        break;
      }
      case BL::Attribute::data_type_INT: {
        BL::IntAttribute b_int_attribute{b_attribute};
        const int *src = static_cast<const int *>(b_int_attribute.data[0].ptr.data);
        Attribute *attr = attributes.add(name, TypeFloat, element);
        float *data = attr->data_float();
        for (int i = 0; i < num_points; i++) {
          data[i] = float(src[i]);
        }
        break;
      }
      case BL::Attribute::data_type_INT32_2D: {
        BL::Int2Attribute b_int2_attribute{b_attribute};
        const int2 *src = static_cast<const int2 *>(b_int2_attribute.data[0].ptr.data);
        Attribute *attr = attributes.add(name, TypeFloat2, element);
        float2 *data = attr->data_float2();
        for (int i = 0; i < num_points; i++) {
          data[i] = make_float2(float(src[i][0]), float(src[i][1]));
        }
        break;
      }
      case BL::Attribute::data_type_FLOAT_VECTOR: {
        BL::FloatVectorAttribute b_vector_attribute{b_attribute};
        const float(*src)[3] = static_cast<const float(*)[3]>(b_vector_attribute.data[0].ptr.data);
        Attribute *attr = attributes.add(name, TypeVector, element);
        float3 *data = attr->data_float3();
        for (int i = 0; i < num_points; i++) {
          data[i] = make_float3(src[i][0], src[i][1], src[i][2]);
        }
        break;
      }
      case BL::Attribute::data_type_BYTE_COLOR: {
        BL::ByteColorAttribute b_color_attribute{b_attribute};
        const uchar(*src)[4] = static_cast<const uchar(*)[4]>(b_color_attribute.data[0].ptr.data);
        Attribute *attr = attributes.add(name, TypeRGBA, element);
        float4 *data = attr->data_float4();
        for (int i = 0; i < num_points; i++) {
          data[i] = make_float4(color_srgb_to_linear(byte_to_float(src[i][0])),
                                color_srgb_to_linear(byte_to_float(src[i][1])),
                                color_srgb_to_linear(byte_to_float(src[i][2])),
                                color_srgb_to_linear(byte_to_float(src[i][3])));
        }
        break;
      }
      case BL::Attribute::data_type_FLOAT_COLOR: {
        BL::FloatColorAttribute b_color_attribute{b_attribute};
        const float(*src)[4] = static_cast<const float(*)[4]>(b_color_attribute.data[0].ptr.data);
        Attribute *attr = attributes.add(name, TypeRGBA, element);
        float4 *data = attr->data_float4();
        for (int i = 0; i < num_points; i++) {
          data[i] = make_float4(src[i][0], src[i][1], src[i][2], src[i][3]);
        }
        break;
      }
      case BL::Attribute::data_type_FLOAT2: {
        BL::Float2Attribute b_float2_attribute{b_attribute};
        const float(*src)[2] = static_cast<const float(*)[2]>(b_float2_attribute.data[0].ptr.data);
        Attribute *attr = attributes.add(name, TypeFloat2, element);
        float2 *data = attr->data_float2();
        for (int i = 0; i < num_points; i++) {
          data[i] = make_float2(src[i][0], src[i][1]);
        }
        break;
      }
      default:
        /* Not supported. */
        break;
    }
  }
}

static const float *find_radius_attribute(BL::PointCloud b_pointcloud)
{
  for (BL::Attribute &b_attribute : b_pointcloud.attributes) {
    if (b_attribute.name() != "radius") {
      continue;
    }
    if (b_attribute.data_type() != BL::Attribute::data_type_FLOAT) {
      continue;
    }
    BL::FloatAttribute b_float_attribute{b_attribute};
    if (b_float_attribute.data.length() == 0) {
      return nullptr;
    }
    return static_cast<const float *>(b_float_attribute.data[0].ptr.data);
  }
  return nullptr;
}

static const float (*find_position_attribute(BL::PointCloud b_pointcloud))[3]
{
  for (BL::Attribute &b_attribute : b_pointcloud.attributes) {
    if (b_attribute.name() != "position") {
      continue;
    }
    if (b_attribute.data_type() != BL::Attribute::data_type_FLOAT_VECTOR) {
      continue;
    }
    BL::FloatVectorAttribute b_float3_attribute{b_attribute};
    if (b_float3_attribute.data.length() == 0) {
      return nullptr;
    }
    return static_cast<const float(*)[3]>(b_float3_attribute.data[0].ptr.data);
  }
  /* The position attribute must exist. */
  assert(false);
  return nullptr;
}

static void export_pointcloud(Scene *scene,
                              PointCloud *pointcloud,
                              BL::PointCloud b_pointcloud,
                              const bool need_motion,
                              const float motion_scale)
{
  const int num_points = b_pointcloud.points.length();
  pointcloud->resize(num_points);

  const float(*b_attr_position)[3] = find_position_attribute(b_pointcloud);
  float3 *points = pointcloud->get_points().data();

  for (int i = 0; i < num_points; i++) {
    points[i] = make_float3(b_attr_position[i][0], b_attr_position[i][1], b_attr_position[i][2]);
  }

  const float *b_attr_radius = find_radius_attribute(b_pointcloud);
  float *radius = pointcloud->get_radius().data();
  if (b_attr_radius) {
    std::copy(b_attr_radius, b_attr_radius + num_points, radius);
  }
  else {
    std::fill(radius, radius + num_points, 0.01f);
  }

  int *shader = pointcloud->get_shader().data();
  std::fill(shader, shader + num_points, 0);

  if (pointcloud->need_attribute(scene, ATTR_STD_POINT_RANDOM)) {
    Attribute *attr_random = pointcloud->attributes.add(ATTR_STD_POINT_RANDOM);
    float *data = attr_random->data_float();
    for (int i = 0; i < num_points; i++) {
      data[i] = hash_uint2_to_float(i, 0);
    }
  }

  copy_attributes(pointcloud, b_pointcloud, need_motion, motion_scale);
}

static void export_pointcloud_motion(PointCloud *pointcloud,
                                     BL::PointCloud b_pointcloud,
                                     int motion_step)
{
  /* Find or add attribute. */
  Attribute *attr_mP = pointcloud->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
  bool new_attribute = false;

  if (!attr_mP) {
    attr_mP = pointcloud->attributes.add(ATTR_STD_MOTION_VERTEX_POSITION);
    new_attribute = true;
  }

  const int num_points = pointcloud->num_points();
  /* Point cloud attributes are stored as float4 with the radius in the w element.
   * This is explicit now as float3 is no longer interchangeable with float4 as it
   * is packed now. */
  float4 *mP = attr_mP->data_float4() + motion_step * num_points;
  bool have_motion = false;
  const array<float3> &pointcloud_points = pointcloud->get_points();

  const int b_points_num = b_pointcloud.points.length();
  const float(*b_attr_position)[3] = find_position_attribute(b_pointcloud);
  const float *b_attr_radius = find_radius_attribute(b_pointcloud);

  for (int i = 0; i < std::min(num_points, b_points_num); i++) {
    const float3 P = make_float3(
        b_attr_position[i][0], b_attr_position[i][1], b_attr_position[i][2]);
    const float radius = b_attr_radius ? b_attr_radius[i] : 0.01f;
    mP[i] = make_float4(P.x, P.y, P.z, radius);
    have_motion = have_motion || (P != pointcloud_points[i]);
  }

  /* In case of new attribute, we verify if there really was any motion. */
  if (new_attribute) {
    if (b_points_num != num_points || !have_motion) {
      pointcloud->attributes.remove(ATTR_STD_MOTION_VERTEX_POSITION);
    }
    else if (motion_step > 0) {
      /* Motion, fill up previous steps that we might have skipped because
       * they had no motion, but we need them anyway now. */
      for (int step = 0; step < motion_step; step++) {
        pointcloud->copy_center_to_motion_step(step);
      }
    }
  }

  /* Export attributes */
  copy_attributes(pointcloud, b_pointcloud, false, 0.0f);
}

void BlenderSync::sync_pointcloud(PointCloud *pointcloud, BObjectInfo &b_ob_info)
{
  size_t old_numpoints = pointcloud->num_points();

  array<Node *> used_shaders = pointcloud->get_used_shaders();

  PointCloud new_pointcloud;
  new_pointcloud.set_used_shaders(used_shaders);

  /* TODO: add option to filter out points in the view layer. */
  BL::PointCloud b_pointcloud(b_ob_info.object_data);
  /* Motion blur attribute is relative to seconds, we need it relative to frames. */
  const bool need_motion = object_need_motion_attribute(b_ob_info, scene);
  const float motion_scale = (need_motion) ?
                                 scene->motion_shutter_time() /
                                     (b_scene.render().fps() / b_scene.render().fps_base()) :
                                 0.0f;
  export_pointcloud(scene, &new_pointcloud, b_pointcloud, need_motion, motion_scale);

  /* Update original sockets. */
  for (const SocketType &socket : new_pointcloud.type->inputs) {
    /* Those sockets are updated in sync_object, so do not modify them. */
    if (socket.name == "use_motion_blur" || socket.name == "motion_steps" ||
        socket.name == "used_shaders")
    {
      continue;
    }
    pointcloud->set_value(socket, new_pointcloud, socket);
  }

  pointcloud->attributes.clear();
  foreach (Attribute &attr, new_pointcloud.attributes.attributes) {
    pointcloud->attributes.attributes.push_back(std::move(attr));
  }

  /* Tag update. */
  const bool rebuild = (pointcloud && old_numpoints != pointcloud->num_points());
  pointcloud->tag_update(scene, rebuild);
}

void BlenderSync::sync_pointcloud_motion(PointCloud *pointcloud,
                                         BObjectInfo &b_ob_info,
                                         int motion_step)
{
  /* Skip if nothing exported. */
  if (pointcloud->num_points() == 0) {
    return;
  }

  /* Export deformed coordinates. */
  if (ccl::BKE_object_is_deform_modified(b_ob_info, b_scene, preview)) {
    /* PointCloud object. */
    BL::PointCloud b_pointcloud(b_ob_info.object_data);
    export_pointcloud_motion(pointcloud, b_pointcloud, motion_step);
  }
  else {
    /* No deformation on this frame, copy coordinates if other frames did have it. */
    pointcloud->copy_center_to_motion_step(motion_step);
  }
}

CCL_NAMESPACE_END
