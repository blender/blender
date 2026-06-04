/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "scene/pointcloud.h"
#include "scene/attribute.h"
#include "scene/scene.h"

#include "util/hash.h"

#include "blender/attribute_convert.h"
#include "blender/sync.h"
#include "blender/util.h"

#include "DNA_pointcloud_types.h"

#include "BKE_attribute.hh"
#include "BKE_attribute_math.hh"

CCL_NAMESPACE_BEGIN

static void attr_create_motion_from_velocity(PointCloud *pointcloud,
                                             const blender::Span<blender::float3> b_attribute,
                                             const float motion_scale)
{
  const int num_points = pointcloud->num_points();

  /* Override motion steps to fixed number. */
  pointcloud->set_motion_steps(3);

  /* Set motion steps on position and radius attributes. */
  Attribute *attr_P = pointcloud->attributes.find(ATTR_STD_POSITION);
  Attribute *attr_R = pointcloud->attributes.find(ATTR_STD_RADIUS);
  attr_P->add_motion(pointcloud);
  attr_R->add_motion(pointcloud);
  const packed_float3 *P = pointcloud->get_position();
  const float *radius = pointcloud->get_radius();

  /* Only export previous and next frame, we don't have any in between data. */
  const float motion_times[2] = {-1.0f, 1.0f};
  for (int step = 1; step <= 2; step++) {
    const float relative_time = motion_times[step - 1] * 0.5f * motion_scale;
    packed_float3 *mP = attr_P->data_for_write<packed_float3>(step);
    float *mR = attr_R->data_for_write<float>(step);

    for (int i = 0; i < num_points; i++) {
      mP[i] = float3(P[i]) +
              make_float3(b_attribute[i][0], b_attribute[i][1], b_attribute[i][2]) * relative_time;
      mR[i] = radius[i];
    }
  }
}

static void copy_attributes(PointCloud *pointcloud,
                            const blender::PointCloud &b_pointcloud,
                            const bool need_motion,
                            const float motion_scale)
{
  const blender::bke::AttributeAccessor b_attributes = b_pointcloud.attributes();
  if (b_attributes.domain_size(blender::bke::AttrDomain::Point) == 0) {
    return;
  }

  AttributeSet &attributes = pointcloud->attributes;
  static const ustring u_velocity("velocity");
  b_attributes.foreach_attribute([&](const blender::bke::AttributeIter &iter) {
    const ustring name{std::string_view(iter.name)};

    if (need_motion && name == u_velocity) {
      const blender::VArraySpan b_attr = *iter.get<blender::float3>();
      attr_create_motion_from_velocity(pointcloud, b_attr, motion_scale);
    }

    if (attributes.find(name)) {
      return;
    }

    const blender::bke::GAttributeReader b_attr = iter.get();
    blender::bke::attribute_math::to_static_type(b_attr.varray.type(), [&]<typename BlenderT>() {
      using Converter = typename ccl::AttributeConverter<BlenderT>;
      using CyclesT = typename Converter::CyclesT;
      if constexpr (!std::is_void_v<CyclesT>) {
        const blender::VArray<BlenderT> src_varray = b_attr.varray.typed<BlenderT>();
        const blender::CommonVArrayInfo info = b_attr.varray.common_info();

        if (info.type == blender::CommonVArrayInfo::Type::Single) {
          const auto &single_value = *static_cast<const BlenderT *>(info.data);
          Attribute *attr = attributes.add(name, Converter::type_desc, ATTR_ELEMENT_MESH);
          CyclesT *data = reinterpret_cast<CyclesT *>(attr->data_for_write());
          *data = Converter::convert(single_value);
          return;
        }

        if constexpr (Converter::layout_compatible) {
          if (info.type == blender::CommonVArrayInfo::Type::Span && b_attr.sharing_info) {
            attributes.add_shared(name,
                                  Converter::type_desc,
                                  ATTR_ELEMENT_VERTEX,
                                  info.data,
                                  src_varray.size(),
                                  b_attr.sharing_info);
            return;
          }
        }

        Attribute *attr = attributes.add(name, Converter::type_desc, ATTR_ELEMENT_VERTEX);
        CyclesT *data = reinterpret_cast<CyclesT *>(attr->data_for_write());

        const blender::VArraySpan src = src_varray;
        for (const int i : src.index_range()) {
          data[i] = Converter::convert(src[i]);
        }
      }
    });
  });
}

static void export_pointcloud(Scene *scene,
                              PointCloud *pointcloud,
                              const blender::PointCloud &b_pointcloud,
                              const bool need_motion,
                              const float motion_scale)
{
  const blender::Span<blender::float3> b_positions = b_pointcloud.positions();
  const blender::bke::AttributeAccessor b_attributes = b_pointcloud.attributes();

  pointcloud->resize(b_positions.size());

  /* Sync positions, sharing with Blender when possible. */
  sync_attribute_from_blender(
      pointcloud->attributes,
      ATTR_STD_POSITION,
      b_attributes.lookup<blender::float3>("position", blender::bke::AttrDomain::Point),
      b_positions.size());
  pointcloud->tag_position_modified();

  /* Sync radius, sharing with Blender when possible, or filling default. */
  if (sync_attribute_from_blender(
          pointcloud->attributes,
          ATTR_STD_RADIUS,
          b_attributes.lookup<float>("radius", blender::bke::AttrDomain::Point),
          b_positions.size()))
  {
    pointcloud->tag_radius_modified();
  }
  else {
    float *radius = pointcloud->get_radius_for_write();
    std::fill(radius, radius + b_positions.size(), 0.01f);
  }

  int *shader = pointcloud->get_shader().data();
  std::fill(shader, shader + b_positions.size(), 0);

  if (pointcloud->need_attribute(scene, ATTR_STD_POINT_RANDOM)) {
    Attribute *attr_random = pointcloud->attributes.add(ATTR_STD_POINT_RANDOM);
    float *data = attr_random->data_for_write<float>();
    for (const int i : b_positions.index_range()) {
      data[i] = hash_uint2_to_float(i, 0);
    }
  }

  copy_attributes(pointcloud, b_pointcloud, need_motion, motion_scale);
}

static void export_pointcloud_motion(PointCloud *pointcloud,
                                     const blender::PointCloud &b_pointcloud,
                                     const int motion_step)
{
  /* Set motion steps on position and radius attributes. */
  Attribute *attr_P = pointcloud->attributes.find(ATTR_STD_POSITION);
  Attribute *attr_R = pointcloud->attributes.find(ATTR_STD_RADIUS);
  bool new_attribute = false;

  if (!attr_P->has_motion()) {
    attr_P->add_motion(pointcloud);
    attr_R->add_motion(pointcloud);
    new_attribute = true;
  }

  const int num_points = pointcloud->num_points();
  const int attr_step = motion_step + 1;
  const blender::Span<blender::float3> b_positions = b_pointcloud.positions();
  const blender::bke::AttributeAccessor b_attributes = b_pointcloud.attributes();
  const bool size_matches = (b_positions.size() == num_points);

  bool have_motion = false;

  if (size_matches) {
    /* Fast path: point count unchanged, sync the whole step from Blender,
     * sharing the buffer when possible. */
    sync_attribute_motion_step_from_blender(
        *attr_P,
        attr_step,
        b_attributes.lookup<blender::float3>("position", blender::bke::AttrDomain::Point));
    if (!sync_attribute_motion_step_from_blender(
            *attr_R,
            attr_step,
            b_attributes.lookup<float>("radius", blender::bke::AttrDomain::Point)))
    {
      float *mR = attr_R->data_for_write<float>(attr_step);
      std::fill(mR, mR + num_points, 0.01f);
    }

    /* If the buffer is shared from Blender and unchanged across frames, the
     * pointer matches the center step's, so the memcmp is skipped. */
    const packed_float3 *motion_P = attr_P->data<packed_float3>(attr_step);
    const packed_float3 *center_P = pointcloud->get_position();
    have_motion = motion_P != center_P &&
                  std::memcmp(motion_P, center_P, num_points * sizeof(packed_float3)) != 0;
  }
  else {
    /* Slow path: point count differs, copy what overlaps. */
    const blender::VArraySpan b_radius = *b_attributes.lookup<float>(
        "radius", blender::bke::AttrDomain::Point);
    packed_float3 *mP = attr_P->data_for_write<packed_float3>(attr_step);
    float *mR = attr_R->data_for_write<float>(attr_step);
    for (int i = 0; i < std::min<int>(num_points, b_positions.size()); i++) {
      mP[i] = make_float3(b_positions[i][0], b_positions[i][1], b_positions[i][2]);
      mR[i] = b_radius.is_empty() ? 0.01f : b_radius[i];
    }
  }

  /* In case of new attribute, verify if there really was any motion. */
  if (new_attribute) {
    if (!size_matches || !have_motion) {
      attr_P->remove_motion();
      attr_R->remove_motion();
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
  const size_t old_numpoints = pointcloud->num_points();

  array<Node *> used_shaders = pointcloud->get_used_shaders();

  PointCloud new_pointcloud;
  new_pointcloud.set_used_shaders(used_shaders);

  /* TODO: add option to filter out points in the view layer. */
  const blender::PointCloud *b_pointcloud = blender::id_cast<blender::PointCloud *>(
      b_ob_info.object_data);
  /* Motion blur attribute is relative to seconds, we need it relative to frames. */
  const bool need_motion = object_need_motion_attribute(b_ob_info, scene);
  const float motion_scale = (need_motion) ? scene->motion_shutter_time() /
                                                 (b_scene->r.frs_sec / b_scene->r.frs_sec_base) :
                                             0.0f;
  export_pointcloud(scene, &new_pointcloud, *b_pointcloud, need_motion, motion_scale);

  if (scene->need_motion() == Scene::MOTION_PASS_INTERACTIVE &&
      pointcloud->num_points() == new_pointcloud.num_points())
  {
    new_pointcloud.set_motion_steps(2);

    Attribute *attr_P = pointcloud->attributes.find(ATTR_STD_POSITION);
    Attribute *new_attr_P = new_pointcloud.attributes.find(ATTR_STD_POSITION);
    if (attr_P->has_motion()) {
      new_attr_P->take_motion_from(*attr_P);
    }
    else {
      new_attr_P->add_motion(&new_pointcloud);
      new_pointcloud.copy_center_to_motion_step(0);
    }
  }

  /* Update original sockets. */

  pointcloud->clear_non_sockets();

  for (const SocketType &socket : new_pointcloud.type->inputs) {
    /* Those sockets are updated in sync_object, so do not modify them. */
    if (socket.name == "use_motion_blur" || socket.name == "used_shaders") {
      continue;
    }
    pointcloud->set_value(socket, new_pointcloud, socket);
  }

  pointcloud->attributes.update(std::move(new_pointcloud.attributes));

  /* Tag update. */
  const bool rebuild = (pointcloud && old_numpoints != pointcloud->num_points());
  pointcloud->tag_update(scene, rebuild);
}

void BlenderSync::sync_pointcloud_motion(PointCloud *pointcloud,
                                         BObjectInfo &b_ob_info,
                                         const int motion_step)
{
  /* Skip if nothing exported. */
  if (pointcloud->num_points() == 0) {
    return;
  }

  /* Export deformed coordinates. */
  if (ccl::BKE_object_is_deform_modified(b_ob_info, *b_scene, preview)) {
    /* PointCloud object. */
    const blender::PointCloud *b_pointcloud = blender::id_cast<blender::PointCloud *>(
        b_ob_info.object_data);
    export_pointcloud_motion(pointcloud, *b_pointcloud, motion_step);
  }
  else {
    /* No deformation on this frame, copy coordinates if other frames did have it. */
    pointcloud->copy_center_to_motion_step(motion_step);
  }
}

CCL_NAMESPACE_END
