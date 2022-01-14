/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "scene/pointcloud.h"
#include "scene/attribute.h"
#include "scene/scene.h"

#include "blender/sync.h"
#include "blender/util.h"

#include "util/foreach.h"
#include "util/hash.h"

CCL_NAMESPACE_BEGIN

template<typename TypeInCycles, typename GetValueAtIndex>
static void fill_generic_attribute(BL::PointCloud &b_pointcloud,
                                   TypeInCycles *data,
                                   const GetValueAtIndex &get_value_at_index)
{
  const int num_points = b_pointcloud.points.length();
  for (int i = 0; i < num_points; i++) {
    data[i] = get_value_at_index(i);
  }
}

static void attr_create_motion(PointCloud *pointcloud,
                               BL::Attribute &b_attribute,
                               const float motion_scale)
{
  if (!(b_attribute.domain() == BL::Attribute::domain_POINT) &&
      (b_attribute.data_type() == BL::Attribute::data_type_FLOAT_VECTOR)) {
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
        Attribute *attr = attributes.add(name, TypeFloat, element);
        float *data = attr->data_float();
        fill_generic_attribute(
            b_pointcloud, data, [&](int i) { return b_float_attribute.data[i].value(); });
        break;
      }
      case BL::Attribute::data_type_BOOLEAN: {
        BL::BoolAttribute b_bool_attribute{b_attribute};
        Attribute *attr = attributes.add(name, TypeFloat, element);
        float *data = attr->data_float();
        fill_generic_attribute(
            b_pointcloud, data, [&](int i) { return (float)b_bool_attribute.data[i].value(); });
        break;
      }
      case BL::Attribute::data_type_INT: {
        BL::IntAttribute b_int_attribute{b_attribute};
        Attribute *attr = attributes.add(name, TypeFloat, element);
        float *data = attr->data_float();
        fill_generic_attribute(
            b_pointcloud, data, [&](int i) { return (float)b_int_attribute.data[i].value(); });
        break;
      }
      case BL::Attribute::data_type_FLOAT_VECTOR: {
        BL::FloatVectorAttribute b_vector_attribute{b_attribute};
        Attribute *attr = attributes.add(name, TypeVector, element);
        float3 *data = attr->data_float3();
        fill_generic_attribute(b_pointcloud, data, [&](int i) {
          BL::Array<float, 3> v = b_vector_attribute.data[i].vector();
          return make_float3(v[0], v[1], v[2]);
        });
        break;
      }
      case BL::Attribute::data_type_FLOAT_COLOR: {
        BL::FloatColorAttribute b_color_attribute{b_attribute};
        Attribute *attr = attributes.add(name, TypeRGBA, element);
        float4 *data = attr->data_float4();
        fill_generic_attribute(b_pointcloud, data, [&](int i) {
          BL::Array<float, 4> v = b_color_attribute.data[i].color();
          return make_float4(v[0], v[1], v[2], v[3]);
        });
        break;
      }
      case BL::Attribute::data_type_FLOAT2: {
        BL::Float2Attribute b_float2_attribute{b_attribute};
        Attribute *attr = attributes.add(name, TypeFloat2, element);
        float2 *data = attr->data_float2();
        fill_generic_attribute(b_pointcloud, data, [&](int i) {
          BL::Array<float, 2> v = b_float2_attribute.data[i].vector();
          return make_float2(v[0], v[1]);
        });
        break;
      }
      default:
        /* Not supported. */
        break;
    }
  }
}

static void export_pointcloud(Scene *scene,
                              PointCloud *pointcloud,
                              BL::PointCloud b_pointcloud,
                              const bool need_motion,
                              const float motion_scale)
{
  /* TODO: optimize so we can straight memcpy arrays from Blender? */

  /* Add requested attributes. */
  Attribute *attr_random = NULL;
  if (pointcloud->need_attribute(scene, ATTR_STD_POINT_RANDOM)) {
    attr_random = pointcloud->attributes.add(ATTR_STD_POINT_RANDOM);
  }

  /* Reserve memory. */
  const int num_points = b_pointcloud.points.length();
  pointcloud->reserve(num_points);

  /* Export points. */
  BL::PointCloud::points_iterator b_point_iter;
  for (b_pointcloud.points.begin(b_point_iter); b_point_iter != b_pointcloud.points.end();
       ++b_point_iter) {
    BL::Point b_point = *b_point_iter;
    const float3 co = get_float3(b_point.co());
    const float radius = b_point.radius();
    pointcloud->add_point(co, radius);

    /* Random number per point. */
    if (attr_random != NULL) {
      attr_random->add(hash_uint2_to_float(b_point.index(), 0));
    }
  }

  /* Export attributes */
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

  /* Export motion points. */
  const int num_points = pointcloud->num_points();
  float3 *mP = attr_mP->data_float3() + motion_step * num_points;
  bool have_motion = false;
  int num_motion_points = 0;
  const array<float3> &pointcloud_points = pointcloud->get_points();

  BL::PointCloud::points_iterator b_point_iter;
  for (b_pointcloud.points.begin(b_point_iter); b_point_iter != b_pointcloud.points.end();
       ++b_point_iter) {
    BL::Point b_point = *b_point_iter;

    if (num_motion_points < num_points) {
      float3 P = get_float3(b_point.co());
      P.w = b_point.radius();
      mP[num_motion_points] = P;
      have_motion = have_motion || (P != pointcloud_points[num_motion_points]);
      num_motion_points++;
    }
  }

  /* In case of new attribute, we verify if there really was any motion. */
  if (new_attribute) {
    if (num_motion_points != num_points || !have_motion) {
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

  /* update original sockets */
  for (const SocketType &socket : new_pointcloud.type->inputs) {
    /* Those sockets are updated in sync_object, so do not modify them. */
    if (socket.name == "use_motion_blur" || socket.name == "motion_steps" ||
        socket.name == "used_shaders") {
      continue;
    }
    pointcloud->set_value(socket, new_pointcloud, socket);
  }

  pointcloud->attributes.clear();
  foreach (Attribute &attr, new_pointcloud.attributes.attributes) {
    pointcloud->attributes.attributes.push_back(std::move(attr));
  }

  /* tag update */
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
