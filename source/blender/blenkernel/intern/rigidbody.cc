/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2013 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup bke
 * \brief Blender-side interface and methods for dealing with Rigid Body simulations
 */

#include "BLI_generic_virtual_array.hh"
#include "BLI_map.hh"
#include "BLI_math_rotation.h"

#include "DNA_object_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"

#include "BKE_geometry_set.hh"
#include "BKE_object.h"
#include "BKE_pointcloud.h"
#include "BKE_rigidbody.h"
#include "BKE_rigidbody.hh"

#include "MOD_nodes.h"

#ifdef WITH_BULLET
#  include "RBI_api.h"
#endif

static const char *id_attribute_name = "id";
static const char *pos_attribute_name = "position";
static const char *rot_attribute_name = "rotation";

namespace blender {

static void update_simulation_nodes_component(struct RigidBodyWorld *rbw,
                                              Object *object,
                                              NodesModifierData *nmd,
                                              const GeometryComponent *component,
                                              int &num_used_bodies)
{
  static rbCollisionShape *generic_shape = RB_shape_new_sphere(0.05f);
  static int generic_collision_groups = 0xFFFFFFFF;

  rbDynamicsWorld *physics_world = (rbDynamicsWorld *)rbw->shared->physics_world;
  Object *orig_ob = (Object *)object->id.orig_id;
  BLI_assert(orig_ob->runtime.rigid_body_map);
  RigidBodyMap &rb_map = *orig_ob->runtime.rigid_body_map;

  if (component == nullptr) {
    return;
  }

  bke::ReadAttributeLookup id_attribute =
      component->attribute_try_get_for_read(id_attribute_name, CD_PROP_INT32);
  bke::ReadAttributeLookup pos_attribute = component->attribute_try_get_for_read(
      pos_attribute_name, CD_PROP_FLOAT3);
  bke::ReadAttributeLookup rot_attribute = component->attribute_try_get_for_read(
      rot_attribute_name, CD_PROP_FLOAT3);

  if (id_attribute && pos_attribute) {
    BLI_assert(id_attribute.size() == component->attribute_domain_num(id_attribute.domain));
    BLI_assert(pos_attribute.size() == component->attribute_domain_num(id_attribute.domain));
    // BLI_assert(rot_attribute.size() == component->attribute_domain_num(ATTR_DOMAIN_POINT));

    /* XXX should have some kind of point group feature to make bodies for relevant points only */
    VArray<int> id_data = id_attribute.varray.typed<int>();
    VArray<float3> pos_data = pos_attribute.varray.typed<float3>();
    VArray<float3> rot_data = rot_attribute.varray.typed<float3>();
    for (int i : id_data.index_range()) {
      int uid = id_data[i];
      const float3 &pos = pos_data[i];
      const float3 &rot_eul = rot_data ? rot_data[i] : float3(0, 0, 0);
      float rot_qt[4];
      eul_to_quat(rot_qt, rot_eul);

      /* Add new bodies */
      RigidBodyMap::BodyPointer &body_ptr = rb_map.map_.lookup_or_add_cb(
          uid, [physics_world, pos, rot_qt]() {
            /* Callback to add a new body */
            RigidBodyMap::BodyFlag flag = RigidBodyMap::Used;
            rbRigidBody *body = RB_body_new(generic_shape, pos, rot_qt);
            RB_dworld_add_body(physics_world, body, generic_collision_groups);
            return RigidBodyMap::BodyPointer{flag, body};
          });
      /* Flag existing bodies as used */
      body_ptr.flag |= RigidBodyMap::Used;
      ++num_used_bodies;
    }
  }
}

}  // namespace blender

void BKE_rigidbody_update_simulation_nodes(struct RigidBodyWorld *rbw,
                                           Object *object,
                                           NodesModifierData *nmd)
{
  using namespace blender;

  rbDynamicsWorld *physics_world = (rbDynamicsWorld *)rbw->shared->physics_world;

  BKE_object_runtime_ensure_rigid_body_map(object, MOD_nodes_needs_rigid_body_sim(object, nmd));
  Object *orig_ob = (Object *)object->id.orig_id;
  RigidBodyMap &rb_map = *orig_ob->runtime.rigid_body_map;

  /* Update flags for used bodies */
  const int num_existing_bodies = rb_map.map_.size();
  for (RigidBodyMap::BodyPointer &body_ptr : rb_map.map_.values()) {
    body_ptr.flag &= ~RigidBodyMap::BodyFlag::Used;
  }

  int num_used_bodies = 0;
  if (GeometrySet *geometry_set = object->runtime.geometry_set_eval) {
    for (const GeometryComponent *component : geometry_set->get_components_for_read()) {
      update_simulation_nodes_component(rbw, object, nmd, component, num_used_bodies);
    }
  }

  /* Remove unused bodies */
  Vector<RigidBodyMap::UID> bodies_to_remove;
  bodies_to_remove.reserve(num_existing_bodies - num_used_bodies);
  for (const auto &item : rb_map.map_.items()) {
    if (!(item.value.flag & RigidBodyMap::BodyFlag::Used)) {
      RB_dworld_remove_body(physics_world, item.value.body);
      RB_body_delete(item.value.body);
      bodies_to_remove.append(item.key);
    }
  }
  for (RigidBodyMap::UID uid : bodies_to_remove) {
    rb_map.map_.remove(uid);
  }
}

namespace blender {

static void update_simulation_nodes_component_post_step(RigidBodyWorld *rbw,
                                                        Object *object,
                                                        NodesModifierData *nmd,
                                                        GeometryComponent *component)
{
  if (component == nullptr) {
    return;
  }

  Object *orig_ob = (Object *)object->id.orig_id;
  if (orig_ob->runtime.rigid_body_map == nullptr) {
    return;
  }
  const RigidBodyMap &rb_map = *orig_ob->runtime.rigid_body_map;

  bke::ReadAttributeLookup id_attribute = component->attribute_try_get_for_read(id_attribute_name,
                                                                                CD_PROP_INT32);
  bke::WriteAttributeLookup pos_attribute = component->attribute_try_get_for_write(
      pos_attribute_name);
  bke::WriteAttributeLookup rot_attribute = component->attribute_try_get_for_write(
      rot_attribute_name);

  if (id_attribute) {
    const int num_points = component->attribute_domain_num(id_attribute.domain);
    BLI_assert(id_attribute.size() == num_points);

    VArray<int> id_data = id_attribute.varray.typed<int>();
    Array<float3> pos_data(num_points);
    Array<float3> rot_data(num_points);

    for (int i : id_data.index_range()) {
      int uid = id_data[i];

      const RigidBodyMap::BodyPointer *body_ptr = rb_map.map_.lookup_ptr(uid);
      if (body_ptr) {
        RB_body_get_position(body_ptr->body, pos_data[i]);
        float rot_qt[4];
        RB_body_get_orientation(body_ptr->body, rot_qt);
        quat_to_eul(rot_data[i], rot_qt);
      }
    }

    if (pos_attribute) {
      pos_attribute.varray.set_all(pos_data.data());
    }
    if (rot_attribute) {
      rot_attribute.varray.set_all(rot_data.data());
    }
  }
}

}  // namespace blender

void BKE_rigidbody_update_simulation_nodes_post_step(RigidBodyWorld *rbw,
                                                     Object *object,
                                                     NodesModifierData *nmd)
{
  using namespace blender;

  if (GeometrySet *geometry_set = object->runtime.geometry_set_eval) {
    update_simulation_nodes_component_post_step(
        rbw, object, nmd, &geometry_set->get_component_for_write<MeshComponent>());
    update_simulation_nodes_component_post_step(
        rbw, object, nmd, &geometry_set->get_component_for_write<PointCloudComponent>());
    update_simulation_nodes_component_post_step(
        rbw, object, nmd, &geometry_set->get_component_for_write<InstancesComponent>());
  }
}
