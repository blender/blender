/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <optional>

#include "scene/attribute.h"
#include "scene/pointcloud.h"
#include "scene/scene.h"

#include "blender/attribute_convert.h"
#include "blender/sync.h"
#include "blender/util.h"

#include "util/color.h"
#include "util/foreach.h"
#include "util/hash.h"

#include "BKE_attribute.hh"
#include "BKE_attribute_math.hh"
#include "BKE_pointcloud.h"

CCL_NAMESPACE_BEGIN

static void attr_create_motion(PointCloud *pointcloud,
                               const blender::Span<blender::float3> b_attribute,
                               const float motion_scale)
{
  const int num_points = pointcloud->get_points().size();

  /* Find or add attribute */
  float3 *P = pointcloud->get_points().data();
  float *radius = pointcloud->get_radius().data();
  Attribute *attr_mP = pointcloud->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);

  if (!attr_mP) {
    attr_mP = pointcloud->attributes.add(ATTR_STD_MOTION_VERTEX_POSITION);
  }

  /* Only export previous and next frame, we don't have any in between data. */
  float motion_times[2] = {-1.0f, 1.0f};
  for (int step = 0; step < 2; step++) {
    const float relative_time = motion_times[step] * 0.5f * motion_scale;
    float4 *mP = attr_mP->data_float4() + step * num_points;

    for (int i = 0; i < num_points; i++) {
      float3 Pi = P[i] + make_float3(b_attribute[i][0], b_attribute[i][1], b_attribute[i][2]) *
                             relative_time;
      mP[i] = make_float4(Pi.x, Pi.y, Pi.z, radius[i]);
    }
  }
}

static void copy_attributes(PointCloud *pointcloud,
                            const ::PointCloud &b_pointcloud,
                            const bool need_motion,
                            const float motion_scale)
{
  const blender::bke::AttributeAccessor b_attributes = b_pointcloud.attributes();
  if (b_attributes.domain_size(ATTR_DOMAIN_POINT) == 0) {
    return;
  }

  AttributeSet &attributes = pointcloud->attributes;
  static const ustring u_velocity("velocity");
  b_attributes.for_all([&](const blender::bke::AttributeIDRef &id,
                           const blender::bke::AttributeMetaData /*meta_data*/) {
    const ustring name{std::string_view(id.name())};

    if (need_motion && name == u_velocity) {
      const blender::VArraySpan b_attr = *b_attributes.lookup<blender::float3>(id);
      attr_create_motion(pointcloud, b_attr, motion_scale);
    }

    if (attributes.find(name)) {
      return true;
    }

    const blender::bke::GAttributeReader b_attr = b_attributes.lookup(id);
    blender::bke::attribute_math::convert_to_static_type(b_attr.varray.type(), [&](auto dummy) {
      using BlenderT = decltype(dummy);
      using Converter = typename ccl::AttributeConverter<BlenderT>;
      using CyclesT = typename Converter::CyclesT;
      if constexpr (!std::is_void_v<CyclesT>) {
        Attribute *attr = attributes.add(name, Converter::type_desc, ATTR_ELEMENT_VERTEX);
        CyclesT *data = reinterpret_cast<CyclesT *>(attr->data());

        const blender::VArraySpan src = b_attr.varray.typed<BlenderT>();
        for (const int i : src.index_range()) {
          data[i] = Converter::convert(src[i]);
        }
      }
    });

    return true;
  });
}

static void export_pointcloud(Scene *scene,
                              PointCloud *pointcloud,
                              const ::PointCloud &b_pointcloud,
                              const bool need_motion,
                              const float motion_scale)
{
  const blender::Span<blender::float3> b_positions = b_pointcloud.positions();
  const blender::VArraySpan b_radius = *b_pointcloud.attributes().lookup<float>("radius",
                                                                                ATTR_DOMAIN_POINT);

  pointcloud->resize(b_positions.size());

  float3 *points = pointcloud->get_points().data();

  for (const int i : b_positions.index_range()) {
    points[i] = make_float3(b_positions[i][0], b_positions[i][1], b_positions[i][2]);
  }

  float *radius = pointcloud->get_radius().data();
  if (!b_radius.is_empty()) {
    std::copy(b_radius.data(), b_radius.data() + b_positions.size(), radius);
  }
  else {
    std::fill(radius, radius + b_positions.size(), 0.01f);
  }

  int *shader = pointcloud->get_shader().data();
  std::fill(shader, shader + b_positions.size(), 0);

  if (pointcloud->need_attribute(scene, ATTR_STD_POINT_RANDOM)) {
    Attribute *attr_random = pointcloud->attributes.add(ATTR_STD_POINT_RANDOM);
    float *data = attr_random->data_float();
    for (const int i : b_positions.index_range()) {
      data[i] = hash_uint2_to_float(i, 0);
    }
  }

  copy_attributes(pointcloud, b_pointcloud, need_motion, motion_scale);
}

static void export_pointcloud_motion(PointCloud *pointcloud,
                                     const ::PointCloud &b_pointcloud,
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

  const blender::Span<blender::float3> b_positions = b_pointcloud.positions();
  const blender::VArraySpan b_radius = *b_pointcloud.attributes().lookup<float>("radius",
                                                                                ATTR_DOMAIN_POINT);

  for (int i = 0; i < std::min<int>(num_points, b_positions.size()); i++) {
    const float3 P = make_float3(b_positions[i][0], b_positions[i][1], b_positions[i][2]);
    const float radius = b_radius.is_empty() ? 0.01f : b_radius[i];
    mP[i] = make_float4(P.x, P.y, P.z, radius);
    have_motion = have_motion || (P != pointcloud_points[i]);
  }

  /* In case of new attribute, we verify if there really was any motion. */
  if (new_attribute) {
    if (b_positions.size() != num_points || !have_motion) {
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
  export_pointcloud(scene,
                    &new_pointcloud,
                    *static_cast<const ::PointCloud *>(b_pointcloud.ptr.data),
                    need_motion,
                    motion_scale);

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
    export_pointcloud_motion(
        pointcloud, *static_cast<const ::PointCloud *>(b_pointcloud.ptr.data), motion_step);
  }
  else {
    /* No deformation on this frame, copy coordinates if other frames did have it. */
    pointcloud->copy_center_to_motion_step(motion_step);
  }
}

CCL_NAMESPACE_END
