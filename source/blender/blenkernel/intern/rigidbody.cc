/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2013 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup bke
 * \brief Blender-side interface and methods for dealing with Rigid Body simulations
 */

#include "BLI_generic_virtual_array.hh"
#include "BLI_map.hh"
#include "BLI_math_rotation.h"

#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"

#include "BKE_geometry_set.hh"
#include "BKE_global.h"
#include "BKE_lib_id.h"
#include "BKE_object.h"
#include "BKE_pointcloud.h"
#include "BKE_rigidbody.h"
#include "BKE_rigidbody.hh"

#include "MOD_nodes.h"

#ifdef WITH_BULLET
#  include "RBI_api.h"
#endif

void RigidBodyMap::clear(RigidBodyWorld *rbw)
{
  rbDynamicsWorld *physics_world = (rbDynamicsWorld *)rbw->shared->physics_world;

  for (const RigidBodyMap::BodyPointer &body_ptr : body_map_.values()) {
    RB_dworld_remove_body(physics_world, body_ptr.body);
    RB_body_delete(body_ptr.body);
  }
  body_map_.clear();

  clear_shapes();
}

void RigidBodyMap::add_shape(ID *id_key, rbCollisionShape *shape)
{
  shape_list_.append(ShapePointer{id_key, shape, 0});
}

void RigidBodyMap::clear_shapes()
{
  for (const RigidBodyMap::ShapePointer &shape_ptr : shape_list_) {
    RB_shape_delete(shape_ptr.shape);
  }
  shape_list_.clear();
}


namespace blender::particles {

static rbCollisionShape *get_collision_shape_from_object(Object *object,
                                                         ParticleNodeShapeType shape_type)
{
  rbCollisionShape *new_shape = NULL;
  float size[3] = {1.0f, 1.0f, 1.0f};
  float radius = 1.0f;
  float height = 1.0f;
  float capsule_height;
  float hull_margin = 0.0f;
  float margin = 0.0f;
  bool can_embed = true;
  bool has_volume;

  const bool use_margin = false;

  /* if automatically determining dimensions, use the Object's boundbox
   * - assume that all quadrics are standing upright on local z-axis
   * - assume even distribution of mass around the Object's pivot
   *   (i.e. Object pivot is centralized in boundbox)
   */
  /* XXX: all dimensions are auto-determined now... later can add stored settings for this */
  /* get object dimensions without scaling */
  const BoundBox *bb = BKE_object_boundbox_get(object);
  if (bb) {
    size[0] = (bb->vec[4][0] - bb->vec[0][0]);
    size[1] = (bb->vec[2][1] - bb->vec[0][1]);
    size[2] = (bb->vec[1][2] - bb->vec[0][2]);
  }
  mul_v3_fl(size, 0.5f);

  if (ELEM(shape_type, PARTICLE_SHAPE_CAPSULE, PARTICLE_SHAPE_CYLINDER, PARTICLE_SHAPE_CONE)) {
    /* take radius as largest x/y dimension, and height as z-dimension */
    radius = MAX2(size[0], size[1]);
    height = size[2];
  }
  else if (shape_type == PARTICLE_SHAPE_SPHERE) {
    /* take radius to the largest dimension to try and encompass everything */
    radius = MAX3(size[0], size[1], size[2]);
  }

  /* create new shape */
  switch (shape_type) {
    case PARTICLE_SHAPE_BOX:
      new_shape = RB_shape_new_box(size[0], size[1], size[2]);
      break;

    case PARTICLE_SHAPE_SPHERE:
      new_shape = RB_shape_new_sphere(radius);
      break;

    case PARTICLE_SHAPE_CAPSULE:
      capsule_height = (height - radius) * 2.0f;
      new_shape = RB_shape_new_capsule(radius, (capsule_height > 0.0f) ? capsule_height : 0.0f);
      break;
    case PARTICLE_SHAPE_CYLINDER:
      new_shape = RB_shape_new_cylinder(radius, height);
      break;
    case PARTICLE_SHAPE_CONE:
      new_shape = RB_shape_new_cone(radius, height * 2.0f);
      break;

    case PARTICLE_SHAPE_CONVEX_HULL:
      /* try to embed collision margin */
      has_volume = (MIN3(size[0], size[1], size[2]) > 0.0f);

      if (!use_margin && has_volume) {
        hull_margin = 0.04f;
      }
      new_shape = rigidbody_get_shape_convexhull_from_mesh(object, hull_margin, &can_embed);
      if (!use_margin) {
        margin = (can_embed && has_volume) ? 0.04f : 0.0f;
      }
      break;
    case PARTICLE_SHAPE_TRIMESH:
      new_shape = rigidbody_get_shape_trimesh_from_mesh(object);
      break;
    //case PARTICLE_SHAPE_COMPOUND:
    //  new_shape = RB_shape_new_compound();
    //  rbCollisionShape *childShape = NULL;
    //  float loc[3], rot[4];
    //  float mat[4][4];
    //  /* Add children to the compound shape */
    //  FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (rbw->group, childObject) {
    //    if (childObject->parent == ob) {
    //      childShape = rigidbody_validate_sim_shape_helper(rbw, childObject);
    //      if (childShape) {
    //        BKE_object_matrix_local_get(childObject, mat);
    //        mat4_to_loc_quat(loc, rot, mat);
    //        RB_compound_add_child_shape(new_shape, childShape, loc, rot);
    //      }
    //    }
    //  }
    //  FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
    //  break;
  }
  /* use box shape if it failed to create new shape */
  if (new_shape == NULL) {
    new_shape = RB_shape_new_box(size[0], size[1], size[2]);
  }
  if (new_shape) {
    RB_shape_set_margin(new_shape, margin);
  }

  return new_shape;
}

static void update_simulation_nodes_component(struct RigidBodyWorld *rbw,
                                              Object *object,
                                              NodesModifierData *nmd,
                                              const GeometryComponent *component,
                                              int &num_used_bodies)
{
  static rbCollisionShape *generic_shape = RB_shape_new_sphere(0.05f);
  static int generic_collision_groups = 0xFFFFFFFF;
  static RigidBodyMap::ShapePointer fallback_shape_ptr{nullptr, generic_shape, 0};

  rbDynamicsWorld *physics_world = (rbDynamicsWorld *)rbw->shared->physics_world;
  Object *orig_ob = (Object *)object->id.orig_id;
  BLI_assert(orig_ob->runtime.rigid_body_map);
  RigidBodyMap &rb_map = *orig_ob->runtime.rigid_body_map;

  if (component == nullptr) {
    return;
  }

  bke::ReadAttributeLookup id_attribute =
      component->attribute_try_get_for_read(id_attribute_name, CD_PROP_INT32);
  bke::ReadAttributeLookup shape_index_attribute = component->attribute_try_get_for_read(
      shape_index_attribute_name, CD_PROP_INT32);
  bke::ReadAttributeLookup pos_attribute = component->attribute_try_get_for_read(
      pos_attribute_name, CD_PROP_FLOAT3);
  bke::ReadAttributeLookup rot_attribute = component->attribute_try_get_for_read(
      rot_attribute_name, CD_PROP_FLOAT3);

  if (id_attribute && pos_attribute) {
    /* XXX should have some kind of point group feature to make bodies for relevant points only */
    VArray<int> id_data = id_attribute.varray.typed<int>();
    VArray<int> shape_index_data = shape_index_attribute.varray.typed<int>();
    VArray<float3> pos_data = pos_attribute.varray.typed<float3>();
    VArray<float3> rot_data = rot_attribute.varray.typed<float3>();
    BLI_assert(id_data.size() == component->attribute_domain_num(id_attribute.domain));
    BLI_assert(pos_data.size() == component->attribute_domain_num(id_attribute.domain));
    for (int i : id_data.index_range()) {
      int uid = id_data[i];
      int shape_index = shape_index_data ? shape_index_data[i] : -1;
      RigidBodyMap::ShapePointer &shape_ptr = rb_map.shape_list_.index_range().contains(
                                                  shape_index) ?
                                                  rb_map.shape_list_[shape_index] :
                                                  fallback_shape_ptr;
      ++shape_ptr.users;

      /* Add new bodies */
      RigidBodyMap::BodyPointer &body_ptr = rb_map.body_map_.lookup_or_add_cb(
          uid, [=]() {
            /* Callback to add a new body */
            const float3 &pos = pos_data[i];
            const float3 &rot_eul = rot_data ? rot_data[i] : float3(0, 0, 0);
            float rot_qt[4];
            eul_to_quat(rot_qt, rot_eul);
            RigidBodyMap::BodyFlag flag = RigidBodyMap::Used;
            rbRigidBody *body = RB_body_new(shape_ptr.shape, pos, rot_qt);
            /* This also computes local moment of inertia, which is needed for rotations! */
            RB_body_set_friction(body, 0.5f);
            RB_body_set_restitution(body, 0.05f);
            RB_body_set_mass(body, 1.0f);
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

void BKE_rigidbody_update_simulation_nodes(RigidBodyWorld *rbw,
                                           Object *object,
                                           NodesModifierData *nmd)
{
  using namespace blender;

  rbDynamicsWorld *physics_world = (rbDynamicsWorld *)rbw->shared->physics_world;

  BKE_object_runtime_ensure_rigid_body_map(
      rbw, object, MOD_nodes_needs_rigid_body_sim(object, nmd));
  Object *orig_ob = (Object *)object->id.orig_id;
  RigidBodyMap &rb_map = *orig_ob->runtime.rigid_body_map;

  /* XXX Placeholder */
  if (rb_map.shape_list_.size() < 1) {
    if (Object *suzanne = (Object *)BKE_libblock_find_name(G_MAIN, ID_OB, "Suzanne")) {
      if (rbCollisionShape *shape = particles::get_collision_shape_from_object(
              suzanne, PARTICLE_SHAPE_CONVEX_HULL)) {
        rb_map.add_shape(&suzanne->id, shape);
      }
    }
  }

  /* Update flags for used bodies */
  const int num_existing_bodies = rb_map.body_map_.size();
  for (RigidBodyMap::BodyPointer &body_ptr : rb_map.body_map_.values()) {
    body_ptr.flag &= ~RigidBodyMap::BodyFlag::Used;
  }
  for (RigidBodyMap::ShapePointer &shape_ptr : rb_map.shape_list_) {
    shape_ptr.users = 0;
  }

  int num_used_bodies = 0;
  if (GeometrySet *geometry_set = object->runtime.geometry_set_eval) {
    for (const GeometryComponent *component : geometry_set->get_components_for_read()) {
      particles::update_simulation_nodes_component(rbw, object, nmd, component, num_used_bodies);
    }
  }

  /* Remove unused bodies */
  Vector<RigidBodyMap::UID> bodies_to_remove;
  bodies_to_remove.reserve(num_existing_bodies - num_used_bodies);
  for (const auto &item : rb_map.body_map_.items()) {
    if (!(item.value.flag & RigidBodyMap::BodyFlag::Used)) {
      RB_dworld_remove_body(physics_world, item.value.body);
      RB_body_delete(item.value.body);
      bodies_to_remove.append(item.key);
    }
  }
  for (RigidBodyMap::UID uid : bodies_to_remove) {
    rb_map.body_map_.remove(uid);
  }
}

namespace blender::particles {

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

    VArray<int> id_data = id_attribute.varray.typed<int>();
    Array<float3> pos_data(num_points);
    Array<float3> rot_data(num_points);
    BLI_assert(id_data.size() == num_points);

    for (int i : id_data.index_range()) {
      int uid = id_data[i];

      const RigidBodyMap::BodyPointer *body_ptr = rb_map.body_map_.lookup_ptr(uid);
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
    particles::update_simulation_nodes_component_post_step(
        rbw, object, nmd, &geometry_set->get_component_for_write<MeshComponent>());
    particles::update_simulation_nodes_component_post_step(
        rbw, object, nmd, &geometry_set->get_component_for_write<PointCloudComponent>());
    particles::update_simulation_nodes_component_post_step(
        rbw, object, nmd, &geometry_set->get_component_for_write<InstancesComponent>());
  }
}
