/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 * \brief Blender-side interface and methods for dealing with Rigid Body simulations
 */

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_mutex.hh"

#ifdef WITH_BULLET
#  include "RBI_api.h"
#endif

#include "DNA_ID.h"
#include "DNA_collection_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_force_types.h"
#include "DNA_object_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"

#include "BKE_collection.hh"
#include "BKE_effect.h"
#include "BKE_global.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_main.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_runtime.hh"
#include "BKE_object.hh"
#include "BKE_pointcache.h"
#include "BKE_report.hh"
#include "BKE_rigidbody.h"
#include "BKE_scene.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#ifdef WITH_BULLET
static CLG_LogRef LOG = {"physics.rigidbody"};
#endif

#ifndef WITH_BULLET
/* #RBI_api.h is not included, some types still need to be declared. */
struct rbCollisionShape;
struct rbConstraint;
struct rbDynamicsWorld;
struct rbRigidBody;
#endif /* !WITH_BULLET */

/* ************************************** */
/* Memory Management */

/* Freeing Methods --------------------- */

#ifdef WITH_BULLET
static void rigidbody_update_ob_array(RigidBodyWorld *rbw);
#else
static void RB_dworld_remove_constraint(void * /*world*/, void * /*con*/) {}
static void RB_dworld_remove_body(void * /*world*/, void * /*body*/) {}
static void RB_dworld_delete(void * /*world*/) {}
static void RB_body_delete(void * /*body*/) {}
static void RB_shape_delete(void * /*shape*/) {}
static void RB_constraint_delete(void * /*con*/) {}
#endif

struct RigidBodyWorld_Runtime {
  rbDynamicsWorld *physics_world = nullptr;
  blender::Mutex mutex;

  ~RigidBodyWorld_Runtime()
  {
    if (physics_world) {
      RB_dworld_delete(physics_world);
    }
  }
};

void BKE_rigidbody_world_init_runtime(RigidBodyWorld *rbw)
{
  if (rbw->shared) {
    rbw->shared->runtime = MEM_new<RigidBodyWorld_Runtime>(__func__);
  }
}

rbDynamicsWorld *BKE_rigidbody_world_physics(RigidBodyWorld *rbw)
{
  return (rbw->shared) ? rbw->shared->runtime->physics_world : nullptr;
}

void BKE_rigidbody_free_world(Scene *scene)
{
  bool is_orig = (scene->id.tag & ID_TAG_COPIED_ON_EVAL) == 0;
  RigidBodyWorld *rbw = scene->rigidbody_world;
  scene->rigidbody_world = nullptr;

  /* sanity check */
  if (!rbw) {
    return;
  }

  if (is_orig && rbw->shared->runtime->physics_world) {
    /* Free physics references,
     * we assume that all physics objects in will have been added to the world. */
    if (rbw->constraints) {
      FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (rbw->constraints, object) {
        if (object->rigidbody_constraint) {
          RigidBodyCon *rbc = object->rigidbody_constraint;
          if (rbc->physics_constraint) {
            RB_dworld_remove_constraint(rbw->shared->runtime->physics_world,
                                        static_cast<rbConstraint *>(rbc->physics_constraint));
          }
        }
      }
      FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
    }

    if (rbw->group) {
      FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (rbw->group, object) {
        BKE_rigidbody_free_object(object, rbw);
      }
      FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
    }
  }
  if (rbw->objects) {
    free(rbw->objects);
  }

  if (is_orig) {
    /* free cache */
    BKE_ptcache_free_list(&(rbw->shared->ptcaches));
    rbw->shared->pointcache = nullptr;

    MEM_delete(rbw->shared->runtime);
    MEM_freeN(rbw->shared);
  }

  /* free effector weights */
  if (rbw->effector_weights) {
    MEM_freeN(rbw->effector_weights);
  }

  /* free rigidbody world itself */
  MEM_freeN(rbw);
}

void BKE_rigidbody_free_object(Object *ob, RigidBodyWorld *rbw)
{
  bool is_orig = (ob->id.tag & ID_TAG_COPIED_ON_EVAL) == 0;
  RigidBodyOb *rbo = ob->rigidbody_object;

  /* sanity check */
  if (rbo == nullptr) {
    return;
  }

  /* free physics references */
  if (is_orig) {
    if (rbo->shared->physics_object) {
      if (rbw != nullptr && rbw->shared->runtime->physics_world != nullptr) {
        /* We can only remove the body from the world if the world is known.
         * The world is generally only unknown if it's an evaluated copy of
         * an object that's being freed, in which case this code isn't run anyway. */
        RB_dworld_remove_body(rbw->shared->runtime->physics_world,
                              static_cast<rbRigidBody *>(rbo->shared->physics_object));
      }
      else {
        /* We have no access to 'owner' RBW when deleting the object ID itself... No choice bu to
         * loop over all scenes then. */
        for (Scene *scene = static_cast<Scene *>(G_MAIN->scenes.first); scene != nullptr;
             scene = static_cast<Scene *>(scene->id.next))
        {
          RigidBodyWorld *scene_rbw = scene->rigidbody_world;
          if (scene_rbw != nullptr && scene_rbw->shared->runtime->physics_world != nullptr) {
            RB_dworld_remove_body(scene_rbw->shared->runtime->physics_world,
                                  static_cast<rbRigidBody *>(rbo->shared->physics_object));
          }
        }
      }

      RB_body_delete(static_cast<rbRigidBody *>(rbo->shared->physics_object));
      rbo->shared->physics_object = nullptr;
    }

    if (rbo->shared->physics_shape) {
      RB_shape_delete(static_cast<rbCollisionShape *>(rbo->shared->physics_shape));
      rbo->shared->physics_shape = nullptr;
    }

    MEM_freeN(rbo->shared);
  }

  /* free data itself */
  MEM_freeN(rbo);
  ob->rigidbody_object = nullptr;
}

void BKE_rigidbody_free_constraint(Object *ob)
{
  RigidBodyCon *rbc = (ob) ? ob->rigidbody_constraint : nullptr;

  /* sanity check */
  if (rbc == nullptr) {
    return;
  }

  /* free physics reference */
  if (rbc->physics_constraint) {
    RB_constraint_delete(static_cast<rbConstraint *>(rbc->physics_constraint));
    rbc->physics_constraint = nullptr;
  }

  /* free data itself */
  MEM_freeN(rbc);
  ob->rigidbody_constraint = nullptr;
}

bool BKE_rigidbody_is_affected_by_simulation(Object *ob)
{
  /* Check if the object will have its transform changed by the rigidbody simulation. */

  /* True if the shape of this object's parent is of type compound */
  bool obCompoundParent = (ob->parent != nullptr && ob->parent->rigidbody_object != nullptr &&
                           ob->parent->rigidbody_object->shape == RB_SHAPE_COMPOUND);

  RigidBodyOb *rbo = ob->rigidbody_object;
  if (rbo == nullptr || rbo->flag & RBO_FLAG_KINEMATIC || rbo->type == RBO_TYPE_PASSIVE ||
      obCompoundParent)
  {
    return false;
  }

  return true;
}

#ifdef WITH_BULLET

/* ************************************** */
/* Setup Utilities - Validate Sim Instances */

/* get the appropriate evaluated mesh based on rigid body mesh source */
static const Mesh *rigidbody_get_mesh(Object *ob)
{
  BLI_assert(ob->type == OB_MESH);

  switch (ob->rigidbody_object->mesh_source) {
    case RBO_MESH_DEFORM:
      return BKE_object_get_mesh_deform_eval(ob);
    case RBO_MESH_FINAL:
      return BKE_object_get_evaluated_mesh(ob);
    case RBO_MESH_BASE:
      /* This mesh may be used for computing corner_tris, which should be done
       * on the original; otherwise every time the evaluated copy is recreated it will
       * have to be recomputed. */
      BLI_assert(ob->rigidbody_object->mesh_source == RBO_MESH_BASE);
      return (Mesh *)ob->runtime->data_orig;
  }

  /* Just return something sensible so that at least Blender won't crash. */
  BLI_assert_msg(0, "Unknown mesh source");
  return BKE_object_get_evaluated_mesh(ob);
}

/* create collision shape of mesh - convex hull */
static rbCollisionShape *rigidbody_get_shape_convexhull_from_mesh(Object *ob,
                                                                  float margin,
                                                                  bool *can_embed)
{
  rbCollisionShape *shape = nullptr;
  const Mesh *mesh = nullptr;
  const float (*positions)[3] = nullptr;
  int totvert = 0;

  if (ob->type == OB_MESH && ob->data) {
    mesh = rigidbody_get_mesh(ob);
    positions = (mesh) ? reinterpret_cast<const float (*)[3]>(mesh->vert_positions().data()) :
                         nullptr;
    totvert = (mesh) ? mesh->verts_num : 0;
  }
  else {
    CLOG_ERROR(&LOG, "cannot make Convex Hull collision shape for non-Mesh object");
  }

  if (totvert) {
    shape = RB_shape_new_convex_hull(
        (float *)positions, sizeof(float[3]), totvert, margin, can_embed);
  }
  else {
    CLOG_ERROR(&LOG, "no vertices to define Convex Hull collision shape with");
  }

  return shape;
}

/* create collision shape of mesh - triangulated mesh
 * returns nullptr if creation fails.
 */
static rbCollisionShape *rigidbody_get_shape_trimesh_from_mesh(Object *ob)
{
  rbCollisionShape *shape = nullptr;

  if (ob->type == OB_MESH) {
    const Mesh *mesh = rigidbody_get_mesh(ob);
    if (mesh == nullptr) {
      return nullptr;
    }

    const blender::Span<blender::float3> positions = mesh->vert_positions();
    const int totvert = mesh->verts_num;
    const blender::Span<blender::int3> corner_tris = mesh->corner_tris();
    const int tottri = corner_tris.size();
    const blender::Span<int> corner_verts = mesh->corner_verts();

    /* sanity checking - potential case when no data will be present */
    if ((totvert == 0) || (tottri == 0)) {
      CLOG_WARN(
          &LOG, "no geometry data converted for Mesh Collision Shape (ob = %s)", ob->id.name + 2);
    }
    else {
      rbMeshData *mdata;
      int i;

      /* init mesh data for collision shape */
      mdata = RB_trimesh_data_new(tottri, totvert);

      RB_trimesh_add_vertices(mdata, (float *)positions.data(), totvert, sizeof(float[3]));

      /* loop over all faces, adding them as triangles to the collision shape
       * (so for some faces, more than triangle will get added)
       */
      if (positions.data()) {
        for (i = 0; i < tottri; i++) {
          /* add first triangle - verts 1,2,3 */
          const blender::int3 &tri = corner_tris[i];
          int vtri[3];

          vtri[0] = corner_verts[tri[0]];
          vtri[1] = corner_verts[tri[1]];
          vtri[2] = corner_verts[tri[2]];

          RB_trimesh_add_triangle_indices(mdata, i, UNPACK3(vtri));
        }
      }

      RB_trimesh_finish(mdata);

      /* construct collision shape
       *
       * These have been chosen to get better speed/accuracy tradeoffs with regards
       * to limitations of each:
       *    - BVH-Triangle Mesh: for passive objects only. Despite having greater
       *                         speed/accuracy, they cannot be used for moving objects.
       *    - GImpact Mesh:      for active objects. These are slower and less stable,
       *                         but are more flexible for general usage.
       */
      if (ob->rigidbody_object->type == RBO_TYPE_PASSIVE) {
        shape = RB_shape_new_trimesh(mdata);
      }
      else {
        shape = RB_shape_new_gimpact_mesh(mdata);
      }
    }
  }
  else {
    CLOG_ERROR(&LOG, "cannot make Triangular Mesh collision shape for non-Mesh object");
  }

  return shape;
}

/* Helper function to create physics collision shape for object.
 * Returns a new collision shape.
 */
static rbCollisionShape *rigidbody_validate_sim_shape_helper(RigidBodyWorld *rbw, Object *ob)
{
  RigidBodyOb *rbo = ob->rigidbody_object;
  rbCollisionShape *new_shape = nullptr;
  float size[3] = {1.0f, 1.0f, 1.0f};
  float radius = 1.0f;
  float height = 1.0f;
  float capsule_height;
  float hull_margin = 0.0f;
  bool can_embed = true;
  bool has_volume;

  /* sanity check */
  if (rbo == nullptr) {
    return nullptr;
  }

  /* if automatically determining dimensions, use the Object's boundbox
   * - assume that all quadrics are standing upright on local z-axis
   * - assume even distribution of mass around the Object's pivot
   *   (i.e. Object pivot is centralized in boundbox)
   */
  /* XXX: all dimensions are auto-determined now... later can add stored settings for this */
  /* get object dimensions without scaling */
  if (const std::optional<blender::Bounds<blender::float3>> bounds = BKE_object_boundbox_get(ob)) {
    copy_v3_v3(size, bounds->max - bounds->min);
  }
  mul_v3_fl(size, 0.5f);

  if (ELEM(rbo->shape, RB_SHAPE_CAPSULE, RB_SHAPE_CYLINDER, RB_SHAPE_CONE)) {
    /* take radius as largest x/y dimension, and height as z-dimension */
    radius = std::max(size[0], size[1]);
    height = size[2];
  }
  else if (rbo->shape == RB_SHAPE_SPHERE) {
    /* take radius to the largest dimension to try and encompass everything */
    radius = std::max({size[0], size[1], size[2]});
  }

  /* create new shape */
  switch (rbo->shape) {
    case RB_SHAPE_BOX:
      new_shape = RB_shape_new_box(size[0], size[1], size[2]);
      break;

    case RB_SHAPE_SPHERE:
      new_shape = RB_shape_new_sphere(radius);
      break;

    case RB_SHAPE_CAPSULE:
      capsule_height = (height - radius) * 2.0f;
      new_shape = RB_shape_new_capsule(radius, (capsule_height > 0.0f) ? capsule_height : 0.0f);
      break;
    case RB_SHAPE_CYLINDER:
      new_shape = RB_shape_new_cylinder(radius, height);
      break;
    case RB_SHAPE_CONE:
      new_shape = RB_shape_new_cone(radius, height * 2.0f);
      break;

    case RB_SHAPE_CONVEXH:
      /* try to embed collision margin */
      has_volume = (std::min({size[0], size[1], size[2]}) > 0.0f);

      if (!(rbo->flag & RBO_FLAG_USE_MARGIN) && has_volume) {
        hull_margin = 0.04f;
      }
      new_shape = rigidbody_get_shape_convexhull_from_mesh(ob, hull_margin, &can_embed);
      if (!(rbo->flag & RBO_FLAG_USE_MARGIN)) {
        rbo->margin = (can_embed && has_volume) ?
                          0.04f :
                          0.0f; /* RB_TODO ideally we shouldn't directly change the margin here */
      }
      break;
    case RB_SHAPE_TRIMESH:
      new_shape = rigidbody_get_shape_trimesh_from_mesh(ob);
      break;
    case RB_SHAPE_COMPOUND:
      new_shape = RB_shape_new_compound();
      rbCollisionShape *childShape = nullptr;
      float loc[3], rot[4];
      float mat[4][4];
      /* Add children to the compound shape */
      FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (rbw->group, childObject) {
        if (childObject->parent == ob) {
          childShape = rigidbody_validate_sim_shape_helper(rbw, childObject);
          if (childShape) {
            BKE_object_matrix_local_get(childObject, mat);
            mat4_to_loc_quat(loc, rot, mat);
            RB_compound_add_child_shape(new_shape, childShape, loc, rot);
          }
        }
      }
      FOREACH_COLLECTION_OBJECT_RECURSIVE_END;

      break;
  }
  /* use box shape if it failed to create new shape */
  if (new_shape == nullptr) {
    new_shape = RB_shape_new_box(size[0], size[1], size[2]);
  }
  if (new_shape) {
    RB_shape_set_margin(new_shape, RBO_GET_MARGIN(rbo));
  }

  return new_shape;
}

/* Create new physics sim collision shape for object and store it,
 * or remove the existing one first and replace...
 */
static void rigidbody_validate_sim_shape(RigidBodyWorld *rbw, Object *ob, bool rebuild)
{
  RigidBodyOb *rbo = ob->rigidbody_object;
  rbCollisionShape *new_shape = nullptr;

  /* sanity check */
  if (rbo == nullptr) {
    return;
  }

  /* don't create a new shape if we already have one and don't want to rebuild it */
  if (rbo->shared->physics_shape && !rebuild) {
    return;
  }

  /* Also don't create a shape if this object is parent of a compound shape */
  if (ob->parent != nullptr && ob->parent->rigidbody_object != nullptr &&
      ob->parent->rigidbody_object->shape == RB_SHAPE_COMPOUND)
  {
    return;
  }

  new_shape = rigidbody_validate_sim_shape_helper(rbw, ob);

  /* assign new collision shape if creation was successful */
  if (new_shape) {
    if (rbo->shared->physics_shape) {
      RB_shape_delete(static_cast<rbCollisionShape *>(rbo->shared->physics_shape));
    }
    rbo->shared->physics_shape = new_shape;
    if (rbo->shared->physics_object) {
      RB_body_set_collision_shape(static_cast<rbRigidBody *>(rbo->shared->physics_object),
                                  static_cast<rbCollisionShape *>(rbo->shared->physics_shape));
    }
  }
}

/* --------------------- */

void BKE_rigidbody_calc_volume(Object *ob, float *r_vol)
{
  RigidBodyOb *rbo = ob->rigidbody_object;

  float size[3] = {1.0f, 1.0f, 1.0f};
  float radius = 1.0f;
  float height = 1.0f;

  float volume = 0.0f;

  /* if automatically determining dimensions, use the Object's boundbox
   * - assume that all quadrics are standing upright on local z-axis
   * - assume even distribution of mass around the Object's pivot
   *   (i.e. Object pivot is centralized in boundbox)
   * - boundbox gives full width
   */
  /* XXX: all dimensions are auto-determined now... later can add stored settings for this */
  BKE_object_dimensions_get(ob, size);

  if (ELEM(rbo->shape, RB_SHAPE_CAPSULE, RB_SHAPE_CYLINDER, RB_SHAPE_CONE)) {
    /* take radius as largest x/y dimension, and height as z-dimension */
    radius = std::max(size[0], size[1]) * 0.5f;
    height = size[2];
  }
  else if (rbo->shape == RB_SHAPE_SPHERE) {
    /* take radius to the largest dimension to try and encompass everything */
    radius = max_fff(size[0], size[1], size[2]) * 0.5f;
  }

  /* Calculate volume as appropriate. */
  switch (rbo->shape) {
    case RB_SHAPE_BOX:
      volume = size[0] * size[1] * size[2];
      break;

    case RB_SHAPE_SPHERE:
      volume = 4.0f / 3.0f * float(M_PI) * radius * radius * radius;
      break;

    /* for now, assume that capsule is close enough to a cylinder... */
    case RB_SHAPE_CAPSULE:
    case RB_SHAPE_CYLINDER:
      volume = float(M_PI) * radius * radius * height;
      break;

    case RB_SHAPE_CONE:
      volume = float(M_PI) / 3.0f * radius * radius * height;
      break;

    case RB_SHAPE_CONVEXH:
    case RB_SHAPE_TRIMESH: {
      if (ob->type == OB_MESH) {
        const Mesh *mesh = rigidbody_get_mesh(ob);
        if (mesh == nullptr) {
          return;
        }

        const blender::Span<blender::float3> positions = mesh->vert_positions();
        const blender::Span<blender::int3> corner_tris = mesh->corner_tris();
        const blender::Span<int> corner_verts = mesh->corner_verts();

        if (!positions.is_empty() && !corner_tris.is_empty()) {
          BKE_mesh_calc_volume(reinterpret_cast<const float (*)[3]>(positions.data()),
                               positions.size(),
                               corner_tris.data(),
                               corner_tris.size(),
                               corner_verts.data(),
                               &volume,
                               nullptr);
          const float volume_scale = mat4_to_volume_scale(ob->object_to_world().ptr());
          volume *= fabsf(volume_scale);
        }
      }
      else {
        /* rough estimate from boundbox as a fallback */
        /* XXX could implement other types of geometry here (curves, etc.) */
        volume = size[0] * size[1] * size[2];
      }
      break;
    }
  }

  /* return the volume calculated */
  if (r_vol) {
    *r_vol = volume;
  }
}

void BKE_rigidbody_calc_center_of_mass(Object *ob, float r_center[3])
{
  RigidBodyOb *rbo = ob->rigidbody_object;

  float size[3] = {1.0f, 1.0f, 1.0f};
  float height = 1.0f;

  zero_v3(r_center);

  /* if automatically determining dimensions, use the Object's boundbox
   * - assume that all quadrics are standing upright on local z-axis
   * - assume even distribution of mass around the Object's pivot
   *   (i.e. Object pivot is centralized in boundbox)
   * - boundbox gives full width
   */
  /* XXX: all dimensions are auto-determined now... later can add stored settings for this. */
  BKE_object_dimensions_get(ob, size);

  /* Calculate volume as appropriate. */
  switch (rbo->shape) {
    case RB_SHAPE_BOX:
    case RB_SHAPE_SPHERE:
    case RB_SHAPE_CAPSULE:
    case RB_SHAPE_CYLINDER:
      break;

    case RB_SHAPE_CONE:
      /* take radius as largest x/y dimension, and height as z-dimension */
      height = size[2];
      /* cone is geometrically centered on the median,
       * center of mass is 1/4 up from the base
       */
      r_center[2] = -0.25f * height;
      break;

    case RB_SHAPE_CONVEXH:
    case RB_SHAPE_TRIMESH: {
      if (ob->type == OB_MESH) {
        const Mesh *mesh = rigidbody_get_mesh(ob);
        if (mesh == nullptr) {
          return;
        }

        const blender::Span<blender::float3> positions = mesh->vert_positions();
        const blender::Span<blender::int3> corner_tris = mesh->corner_tris();

        if (!positions.is_empty() && !corner_tris.is_empty()) {
          BKE_mesh_calc_volume(reinterpret_cast<const float (*)[3]>(positions.data()),
                               positions.size(),
                               corner_tris.data(),
                               corner_tris.size(),
                               mesh->corner_verts().data(),
                               nullptr,
                               r_center);
        }
      }
      break;
    }
  }
}

/* --------------------- */

/**
 * Create physics sim representation of object given RigidBody settings
 *
 * \param rebuild: Even if an instance already exists, replace it
 */
static void rigidbody_validate_sim_object(RigidBodyWorld *rbw, Object *ob, bool rebuild)
{
  RigidBodyOb *rbo = (ob) ? ob->rigidbody_object : nullptr;
  float loc[3];
  float rot[4];

  /* sanity checks:
   * - object doesn't have RigidBody info already: then why is it here?
   */
  if (rbo == nullptr) {
    return;
  }

  /* make sure collision shape exists */
  /* FIXME we shouldn't always have to rebuild collision shapes when rebuilding objects,
   * but it's needed for constraints to update correctly. */
  if (rbo->shared->physics_shape == nullptr || rebuild) {
    rigidbody_validate_sim_shape(rbw, ob, true);
  }

  if (rbo->shared->physics_object && !rebuild) {
    /* Don't remove body on rebuild as it has already been removed when deleting and rebuilding the
     * world. */
    RB_dworld_remove_body(rbw->shared->runtime->physics_world,
                          static_cast<rbRigidBody *>(rbo->shared->physics_object));
  }
  if (!rbo->shared->physics_object || rebuild) {
    /* remove rigid body if it already exists before creating a new one */
    if (rbo->shared->physics_object) {
      RB_body_delete(static_cast<rbRigidBody *>(rbo->shared->physics_object));
      rbo->shared->physics_object = nullptr;
    }
    /* Don't create rigid body object if the parent is a compound shape */
    if (ob->parent != nullptr && ob->parent->rigidbody_object != nullptr &&
        ob->parent->rigidbody_object->shape == RB_SHAPE_COMPOUND)
    {
      return;
    }

    mat4_to_loc_quat(loc, rot, ob->object_to_world().ptr());

    rbo->shared->physics_object = RB_body_new(
        static_cast<rbCollisionShape *>(rbo->shared->physics_shape), loc, rot);

    RB_body_set_friction(static_cast<rbRigidBody *>(rbo->shared->physics_object), rbo->friction);
    RB_body_set_restitution(static_cast<rbRigidBody *>(rbo->shared->physics_object),
                            rbo->restitution);

    RB_body_set_damping(static_cast<rbRigidBody *>(rbo->shared->physics_object),
                        rbo->lin_damping,
                        rbo->ang_damping);
    RB_body_set_sleep_thresh(static_cast<rbRigidBody *>(rbo->shared->physics_object),
                             rbo->lin_sleep_thresh,
                             rbo->ang_sleep_thresh);
    RB_body_set_activation_state(static_cast<rbRigidBody *>(rbo->shared->physics_object),
                                 rbo->flag & RBO_FLAG_USE_DEACTIVATION);

    if (rbo->type == RBO_TYPE_PASSIVE || rbo->flag & RBO_FLAG_START_DEACTIVATED) {
      RB_body_deactivate(static_cast<rbRigidBody *>(rbo->shared->physics_object));
    }

    RB_body_set_linear_factor(static_cast<rbRigidBody *>(rbo->shared->physics_object),
                              (ob->protectflag & OB_LOCK_LOCX) == 0,
                              (ob->protectflag & OB_LOCK_LOCY) == 0,
                              (ob->protectflag & OB_LOCK_LOCZ) == 0);
    RB_body_set_angular_factor(static_cast<rbRigidBody *>(rbo->shared->physics_object),
                               (ob->protectflag & OB_LOCK_ROTX) == 0,
                               (ob->protectflag & OB_LOCK_ROTY) == 0,
                               (ob->protectflag & OB_LOCK_ROTZ) == 0);

    RB_body_set_mass(static_cast<rbRigidBody *>(rbo->shared->physics_object), RBO_GET_MASS(rbo));
    RB_body_set_kinematic_state(static_cast<rbRigidBody *>(rbo->shared->physics_object),
                                rbo->flag & RBO_FLAG_KINEMATIC || rbo->flag & RBO_FLAG_DISABLED);
  }

  if (rbw && rbw->shared->runtime->physics_world && rbo->shared->physics_object) {
    RB_dworld_add_body(rbw->shared->runtime->physics_world,
                       static_cast<rbRigidBody *>(rbo->shared->physics_object),
                       rbo->col_groups);
  }
}

/* --------------------- */

static void rigidbody_constraint_init_spring(RigidBodyCon *rbc,
                                             void (*set_spring)(rbConstraint *, int, int),
                                             void (*set_stiffness)(rbConstraint *, int, float),
                                             void (*set_damping)(rbConstraint *, int, float))
{
  set_spring(static_cast<rbConstraint *>(rbc->physics_constraint),
             RB_LIMIT_LIN_X,
             rbc->flag & RBC_FLAG_USE_SPRING_X);
  set_stiffness(static_cast<rbConstraint *>(rbc->physics_constraint),
                RB_LIMIT_LIN_X,
                rbc->spring_stiffness_x);
  set_damping(
      static_cast<rbConstraint *>(rbc->physics_constraint), RB_LIMIT_LIN_X, rbc->spring_damping_x);

  set_spring(static_cast<rbConstraint *>(rbc->physics_constraint),
             RB_LIMIT_LIN_Y,
             rbc->flag & RBC_FLAG_USE_SPRING_Y);
  set_stiffness(static_cast<rbConstraint *>(rbc->physics_constraint),
                RB_LIMIT_LIN_Y,
                rbc->spring_stiffness_y);
  set_damping(
      static_cast<rbConstraint *>(rbc->physics_constraint), RB_LIMIT_LIN_Y, rbc->spring_damping_y);

  set_spring(static_cast<rbConstraint *>(rbc->physics_constraint),
             RB_LIMIT_LIN_Z,
             rbc->flag & RBC_FLAG_USE_SPRING_Z);
  set_stiffness(static_cast<rbConstraint *>(rbc->physics_constraint),
                RB_LIMIT_LIN_Z,
                rbc->spring_stiffness_z);
  set_damping(
      static_cast<rbConstraint *>(rbc->physics_constraint), RB_LIMIT_LIN_Z, rbc->spring_damping_z);

  set_spring(static_cast<rbConstraint *>(rbc->physics_constraint),
             RB_LIMIT_ANG_X,
             rbc->flag & RBC_FLAG_USE_SPRING_ANG_X);
  set_stiffness(static_cast<rbConstraint *>(rbc->physics_constraint),
                RB_LIMIT_ANG_X,
                rbc->spring_stiffness_ang_x);
  set_damping(static_cast<rbConstraint *>(rbc->physics_constraint),
              RB_LIMIT_ANG_X,
              rbc->spring_damping_ang_x);

  set_spring(static_cast<rbConstraint *>(rbc->physics_constraint),
             RB_LIMIT_ANG_Y,
             rbc->flag & RBC_FLAG_USE_SPRING_ANG_Y);
  set_stiffness(static_cast<rbConstraint *>(rbc->physics_constraint),
                RB_LIMIT_ANG_Y,
                rbc->spring_stiffness_ang_y);
  set_damping(static_cast<rbConstraint *>(rbc->physics_constraint),
              RB_LIMIT_ANG_Y,
              rbc->spring_damping_ang_y);

  set_spring(static_cast<rbConstraint *>(rbc->physics_constraint),
             RB_LIMIT_ANG_Z,
             rbc->flag & RBC_FLAG_USE_SPRING_ANG_Z);
  set_stiffness(static_cast<rbConstraint *>(rbc->physics_constraint),
                RB_LIMIT_ANG_Z,
                rbc->spring_stiffness_ang_z);
  set_damping(static_cast<rbConstraint *>(rbc->physics_constraint),
              RB_LIMIT_ANG_Z,
              rbc->spring_damping_ang_z);
}

static void rigidbody_constraint_set_limits(RigidBodyCon *rbc,
                                            void (*set_limits)(rbConstraint *, int, float, float))
{
  if (rbc->flag & RBC_FLAG_USE_LIMIT_LIN_X) {
    set_limits(static_cast<rbConstraint *>(rbc->physics_constraint),
               RB_LIMIT_LIN_X,
               rbc->limit_lin_x_lower,
               rbc->limit_lin_x_upper);
  }
  else {
    set_limits(static_cast<rbConstraint *>(rbc->physics_constraint), RB_LIMIT_LIN_X, 0.0f, -1.0f);
  }

  if (rbc->flag & RBC_FLAG_USE_LIMIT_LIN_Y) {
    set_limits(static_cast<rbConstraint *>(rbc->physics_constraint),
               RB_LIMIT_LIN_Y,
               rbc->limit_lin_y_lower,
               rbc->limit_lin_y_upper);
  }
  else {
    set_limits(static_cast<rbConstraint *>(rbc->physics_constraint), RB_LIMIT_LIN_Y, 0.0f, -1.0f);
  }

  if (rbc->flag & RBC_FLAG_USE_LIMIT_LIN_Z) {
    set_limits(static_cast<rbConstraint *>(rbc->physics_constraint),
               RB_LIMIT_LIN_Z,
               rbc->limit_lin_z_lower,
               rbc->limit_lin_z_upper);
  }
  else {
    set_limits(static_cast<rbConstraint *>(rbc->physics_constraint), RB_LIMIT_LIN_Z, 0.0f, -1.0f);
  }

  if (rbc->flag & RBC_FLAG_USE_LIMIT_ANG_X) {
    set_limits(static_cast<rbConstraint *>(rbc->physics_constraint),
               RB_LIMIT_ANG_X,
               rbc->limit_ang_x_lower,
               rbc->limit_ang_x_upper);
  }
  else {
    set_limits(static_cast<rbConstraint *>(rbc->physics_constraint), RB_LIMIT_ANG_X, 0.0f, -1.0f);
  }

  if (rbc->flag & RBC_FLAG_USE_LIMIT_ANG_Y) {
    set_limits(static_cast<rbConstraint *>(rbc->physics_constraint),
               RB_LIMIT_ANG_Y,
               rbc->limit_ang_y_lower,
               rbc->limit_ang_y_upper);
  }
  else {
    set_limits(static_cast<rbConstraint *>(rbc->physics_constraint), RB_LIMIT_ANG_Y, 0.0f, -1.0f);
  }

  if (rbc->flag & RBC_FLAG_USE_LIMIT_ANG_Z) {
    set_limits(static_cast<rbConstraint *>(rbc->physics_constraint),
               RB_LIMIT_ANG_Z,
               rbc->limit_ang_z_lower,
               rbc->limit_ang_z_upper);
  }
  else {
    set_limits(static_cast<rbConstraint *>(rbc->physics_constraint), RB_LIMIT_ANG_Z, 0.0f, -1.0f);
  }
}

/**
 * Create physics sim representation of constraint given rigid body constraint settings
 *
 * \param rebuild: Even if an instance already exists, replace it
 */
static void rigidbody_validate_sim_constraint(RigidBodyWorld *rbw, Object *ob, bool rebuild)
{
  RigidBodyCon *rbc = (ob) ? ob->rigidbody_constraint : nullptr;
  float loc[3];
  float rot[4];
  float lin_lower;
  float lin_upper;
  float ang_lower;
  float ang_upper;

  /* sanity checks:
   * - object should have a rigid body constraint
   * - rigid body constraint should have at least one constrained object
   */
  if (rbc == nullptr) {
    return;
  }

  if (ELEM(nullptr, rbc->ob1, rbc->ob1->rigidbody_object, rbc->ob2, rbc->ob2->rigidbody_object)) {
    if (rbc->physics_constraint) {
      RB_dworld_remove_constraint(rbw->shared->runtime->physics_world,
                                  static_cast<rbConstraint *>(rbc->physics_constraint));
      RB_constraint_delete(static_cast<rbConstraint *>(rbc->physics_constraint));
      rbc->physics_constraint = nullptr;
    }
    return;
  }

  if (rbc->physics_constraint && rebuild == false) {
    RB_dworld_remove_constraint(rbw->shared->runtime->physics_world,
                                static_cast<rbConstraint *>(rbc->physics_constraint));
  }
  if (rbc->physics_constraint == nullptr || rebuild) {
    rbRigidBody *rb1 = static_cast<rbRigidBody *>(
        rbc->ob1->rigidbody_object->shared->physics_object);
    rbRigidBody *rb2 = static_cast<rbRigidBody *>(
        rbc->ob2->rigidbody_object->shared->physics_object);

    /* remove constraint if it already exists before creating a new one */
    if (rbc->physics_constraint) {
      RB_constraint_delete(static_cast<rbConstraint *>(rbc->physics_constraint));
      rbc->physics_constraint = nullptr;
    }

    mat4_to_loc_quat(loc, rot, ob->object_to_world().ptr());

    if (rb1 && rb2) {
      switch (rbc->type) {
        case RBC_TYPE_POINT:
          rbc->physics_constraint = RB_constraint_new_point(loc, rb1, rb2);
          break;
        case RBC_TYPE_FIXED:
          rbc->physics_constraint = RB_constraint_new_fixed(loc, rot, rb1, rb2);
          break;
        case RBC_TYPE_HINGE:
          rbc->physics_constraint = RB_constraint_new_hinge(loc, rot, rb1, rb2);
          if (rbc->flag & RBC_FLAG_USE_LIMIT_ANG_Z) {
            RB_constraint_set_limits_hinge(static_cast<rbConstraint *>(rbc->physics_constraint),
                                           rbc->limit_ang_z_lower,
                                           rbc->limit_ang_z_upper);
          }
          else {
            RB_constraint_set_limits_hinge(
                static_cast<rbConstraint *>(rbc->physics_constraint), 0.0f, -1.0f);
          }
          break;
        case RBC_TYPE_SLIDER:
          rbc->physics_constraint = RB_constraint_new_slider(loc, rot, rb1, rb2);
          if (rbc->flag & RBC_FLAG_USE_LIMIT_LIN_X) {
            RB_constraint_set_limits_slider(static_cast<rbConstraint *>(rbc->physics_constraint),
                                            rbc->limit_lin_x_lower,
                                            rbc->limit_lin_x_upper);
          }
          else {
            RB_constraint_set_limits_slider(
                static_cast<rbConstraint *>(rbc->physics_constraint), 0.0f, -1.0f);
          }
          break;
        case RBC_TYPE_PISTON:
          rbc->physics_constraint = RB_constraint_new_piston(loc, rot, rb1, rb2);
          if (rbc->flag & RBC_FLAG_USE_LIMIT_LIN_X) {
            lin_lower = rbc->limit_lin_x_lower;
            lin_upper = rbc->limit_lin_x_upper;
          }
          else {
            lin_lower = 0.0f;
            lin_upper = -1.0f;
          }
          if (rbc->flag & RBC_FLAG_USE_LIMIT_ANG_X) {
            ang_lower = rbc->limit_ang_x_lower;
            ang_upper = rbc->limit_ang_x_upper;
          }
          else {
            ang_lower = 0.0f;
            ang_upper = -1.0f;
          }
          RB_constraint_set_limits_piston(static_cast<rbConstraint *>(rbc->physics_constraint),
                                          lin_lower,
                                          lin_upper,
                                          ang_lower,
                                          ang_upper);
          break;
        case RBC_TYPE_6DOF_SPRING:
          if (rbc->spring_type == RBC_SPRING_TYPE2) {
            rbc->physics_constraint = RB_constraint_new_6dof_spring2(loc, rot, rb1, rb2);

            rigidbody_constraint_init_spring(rbc,
                                             RB_constraint_set_spring_6dof_spring2,
                                             RB_constraint_set_stiffness_6dof_spring2,
                                             RB_constraint_set_damping_6dof_spring2);

            RB_constraint_set_equilibrium_6dof_spring2(
                static_cast<rbConstraint *>(rbc->physics_constraint));

            rigidbody_constraint_set_limits(rbc, RB_constraint_set_limits_6dof_spring2);
          }
          else {
            rbc->physics_constraint = RB_constraint_new_6dof_spring(loc, rot, rb1, rb2);

            rigidbody_constraint_init_spring(rbc,
                                             RB_constraint_set_spring_6dof_spring,
                                             RB_constraint_set_stiffness_6dof_spring,
                                             RB_constraint_set_damping_6dof_spring);

            RB_constraint_set_equilibrium_6dof_spring(
                static_cast<rbConstraint *>(rbc->physics_constraint));

            rigidbody_constraint_set_limits(rbc, RB_constraint_set_limits_6dof);
          }
          break;
        case RBC_TYPE_6DOF:
          rbc->physics_constraint = RB_constraint_new_6dof(loc, rot, rb1, rb2);

          rigidbody_constraint_set_limits(rbc, RB_constraint_set_limits_6dof);
          break;
        case RBC_TYPE_MOTOR:
          rbc->physics_constraint = RB_constraint_new_motor(loc, rot, rb1, rb2);

          RB_constraint_set_enable_motor(static_cast<rbConstraint *>(rbc->physics_constraint),
                                         rbc->flag & RBC_FLAG_USE_MOTOR_LIN,
                                         rbc->flag & RBC_FLAG_USE_MOTOR_ANG);
          RB_constraint_set_max_impulse_motor(static_cast<rbConstraint *>(rbc->physics_constraint),
                                              rbc->motor_lin_max_impulse,
                                              rbc->motor_ang_max_impulse);
          RB_constraint_set_target_velocity_motor(
              static_cast<rbConstraint *>(rbc->physics_constraint),
              rbc->motor_lin_target_velocity,
              rbc->motor_ang_target_velocity);
          break;
      }
    }
    else { /* can't create constraint without both rigid bodies */
      return;
    }

    /* When 'rbc->type' is unknown. */
    if (rbc->physics_constraint == nullptr) {
      return;
    }

    RB_constraint_set_enabled(static_cast<rbConstraint *>(rbc->physics_constraint),
                              rbc->flag & RBC_FLAG_ENABLED);

    if (rbc->flag & RBC_FLAG_USE_BREAKING) {
      RB_constraint_set_breaking_threshold(static_cast<rbConstraint *>(rbc->physics_constraint),
                                           rbc->breaking_threshold);
    }
    else {
      RB_constraint_set_breaking_threshold(static_cast<rbConstraint *>(rbc->physics_constraint),
                                           FLT_MAX);
    }

    if (rbc->flag & RBC_FLAG_OVERRIDE_SOLVER_ITERATIONS) {
      RB_constraint_set_solver_iterations(static_cast<rbConstraint *>(rbc->physics_constraint),
                                          rbc->num_solver_iterations);
    }
    else {
      RB_constraint_set_solver_iterations(static_cast<rbConstraint *>(rbc->physics_constraint),
                                          -1);
    }
  }

  if (rbw && rbw->shared->runtime->physics_world && rbc->physics_constraint) {
    RB_dworld_add_constraint(rbw->shared->runtime->physics_world,
                             static_cast<rbConstraint *>(rbc->physics_constraint),
                             rbc->flag & RBC_FLAG_DISABLE_COLLISIONS);
  }
}

/* --------------------- */

void BKE_rigidbody_validate_sim_world(Scene *scene, RigidBodyWorld *rbw, bool rebuild)
{
  /* sanity checks */
  if (rbw == nullptr) {
    return;
  }

  /* create new sim world */
  if (rebuild || rbw->shared->runtime->physics_world == nullptr) {
    if (rbw->shared->runtime->physics_world) {
      RB_dworld_delete(rbw->shared->runtime->physics_world);
    }
    rbw->shared->runtime->physics_world = RB_dworld_new(scene->physics_settings.gravity);
  }

  RB_dworld_set_solver_iterations(rbw->shared->runtime->physics_world, rbw->num_solver_iterations);
  RB_dworld_set_split_impulse(rbw->shared->runtime->physics_world,
                              rbw->flag & RBW_FLAG_USE_SPLIT_IMPULSE);
}

/* ************************************** */
/* Setup Utilities - Create Settings Blocks */

RigidBodyWorld *BKE_rigidbody_create_world(Scene *scene)
{
  /* try to get whatever RigidBody world that might be representing this already */
  RigidBodyWorld *rbw;

  /* sanity checks
   * - there must be a valid scene to add world to
   * - there mustn't be a sim world using this group already
   */
  if (scene == nullptr) {
    return nullptr;
  }

  /* create a new sim world */
  rbw = MEM_callocN<RigidBodyWorld>("RigidBodyWorld");
  rbw->shared = MEM_callocN<RigidBodyWorld_Shared>("RigidBodyWorld_Shared");

  /* set default settings */
  rbw->effector_weights = BKE_effector_add_weights(nullptr);

  rbw->ltime = PSFRA;

  rbw->time_scale = 1.0f;

  /* Most high quality Bullet example files has an internal frame-rate of 240hz.
   * The blender default scene has a frame rate of 24, so take 10 sub-steps (24fps * 10). */
  rbw->substeps_per_frame = 10;
  rbw->num_solver_iterations = 10; /* 10 is bullet default */

  rbw->shared->pointcache = BKE_ptcache_add(&(rbw->shared->ptcaches));
  rbw->shared->pointcache->step = 1;

  BKE_rigidbody_world_init_runtime(rbw);

  /* return this sim world */
  return rbw;
}

RigidBodyWorld *BKE_rigidbody_world_copy(RigidBodyWorld *rbw, const int flag)
{
  RigidBodyWorld *rbw_copy = static_cast<RigidBodyWorld *>(MEM_dupallocN(rbw));

  if (rbw->effector_weights) {
    rbw_copy->effector_weights = static_cast<EffectorWeights *>(
        MEM_dupallocN(rbw->effector_weights));
    if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
      id_us_plus((ID *)rbw->effector_weights->group);
    }
  }
  if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
    id_us_plus((ID *)rbw_copy->group);
    id_us_plus((ID *)rbw_copy->constraints);
  }

  if ((flag & LIB_ID_COPY_SET_COPIED_ON_WRITE) == 0) {
    /* This is a regular copy, and not an evaluated copy for depsgraph evaluation. */
    rbw_copy->shared = MEM_callocN<RigidBodyWorld_Shared>("RigidBodyWorld_Shared");
    BKE_ptcache_copy_list(&rbw_copy->shared->ptcaches, &rbw->shared->ptcaches, LIB_ID_COPY_CACHES);
    rbw_copy->shared->pointcache = static_cast<PointCache *>(rbw_copy->shared->ptcaches.first);
    BKE_rigidbody_world_init_runtime(rbw_copy);
  }

  rbw_copy->objects = nullptr;
  rbw_copy->numbodies = 0;
  rigidbody_update_ob_array(rbw_copy);

  return rbw_copy;
}

void BKE_rigidbody_world_groups_relink(RigidBodyWorld *rbw)
{
  ID_NEW_REMAP(rbw->group);
  ID_NEW_REMAP(rbw->constraints);
  ID_NEW_REMAP(rbw->effector_weights->group);
}

RigidBodyOb *BKE_rigidbody_create_object(Scene *scene, Object *ob, short type)
{
  RigidBodyOb *rbo;
  RigidBodyWorld *rbw = scene->rigidbody_world;

  /* sanity checks
   * - rigidbody world must exist
   * - object must exist
   * - cannot add rigid body if it already exists
   */
  if (ob == nullptr) {
    return nullptr;
  }
  if (ob->rigidbody_object != nullptr) {
    return ob->rigidbody_object;
  }

  /* create new settings data, and link it up */
  rbo = MEM_callocN<RigidBodyOb>("RigidBodyOb");
  rbo->shared = MEM_callocN<RigidBodyOb_Shared>("RigidBodyOb_Shared");

  /* set default settings */
  rbo->type = type;

  rbo->mass = 1.0f;

  rbo->friction = 0.5f;    /* best when non-zero. 0.5 is Bullet default */
  rbo->restitution = 0.0f; /* best when zero. 0.0 is Bullet default */

  rbo->margin = 0.04f; /* 0.04 (in meters) is Bullet default */

  rbo->lin_sleep_thresh = 0.4f; /* 0.4 is half of Bullet default */
  rbo->ang_sleep_thresh = 0.5f; /* 0.5 is half of Bullet default */

  rbo->lin_damping = 0.04f;
  rbo->ang_damping = 0.1f;

  rbo->col_groups = 1;

  /* use triangle meshes for passive objects
   * use convex hulls for active objects since dynamic triangle meshes are very unstable
   */
  if (type == RBO_TYPE_ACTIVE) {
    rbo->shape = RB_SHAPE_CONVEXH;
  }
  else {
    rbo->shape = RB_SHAPE_TRIMESH;
  }

  rbo->mesh_source = RBO_MESH_DEFORM;

  /* set initial transform */
  mat4_to_loc_quat(rbo->pos, rbo->orn, ob->object_to_world().ptr());

  /* flag cache as outdated */
  BKE_rigidbody_cache_reset(rbw);
  rbo->flag |= (RBO_FLAG_NEEDS_VALIDATE | RBO_FLAG_NEEDS_RESHAPE);

  /* return this object */
  return rbo;
}

RigidBodyCon *BKE_rigidbody_create_constraint(Scene *scene, Object *ob, short type)
{
  RigidBodyCon *rbc;
  RigidBodyWorld *rbw = scene->rigidbody_world;

  /* sanity checks
   * - rigidbody world must exist
   * - object must exist
   * - cannot add constraint if it already exists
   */
  if (ob == nullptr || (ob->rigidbody_constraint != nullptr)) {
    return nullptr;
  }

  /* create new settings data, and link it up */
  rbc = MEM_callocN<RigidBodyCon>("RigidBodyCon");

  /* set default settings */
  rbc->type = type;

  rbc->ob1 = nullptr;
  rbc->ob2 = nullptr;

  rbc->flag |= RBC_FLAG_ENABLED;
  rbc->flag |= RBC_FLAG_DISABLE_COLLISIONS;
  rbc->flag |= RBC_FLAG_NEEDS_VALIDATE;

  rbc->spring_type = RBC_SPRING_TYPE2;

  rbc->breaking_threshold = 10.0f; /* no good default here, just use 10 for now */
  rbc->num_solver_iterations = 10; /* 10 is Bullet default */

  rbc->limit_lin_x_lower = -1.0f;
  rbc->limit_lin_x_upper = 1.0f;
  rbc->limit_lin_y_lower = -1.0f;
  rbc->limit_lin_y_upper = 1.0f;
  rbc->limit_lin_z_lower = -1.0f;
  rbc->limit_lin_z_upper = 1.0f;
  rbc->limit_ang_x_lower = -M_PI_4;
  rbc->limit_ang_x_upper = M_PI_4;
  rbc->limit_ang_y_lower = -M_PI_4;
  rbc->limit_ang_y_upper = M_PI_4;
  rbc->limit_ang_z_lower = -M_PI_4;
  rbc->limit_ang_z_upper = M_PI_4;

  rbc->spring_damping_x = 0.5f;
  rbc->spring_damping_y = 0.5f;
  rbc->spring_damping_z = 0.5f;
  rbc->spring_damping_ang_x = 0.5f;
  rbc->spring_damping_ang_y = 0.5f;
  rbc->spring_damping_ang_z = 0.5f;
  rbc->spring_stiffness_x = 10.0f;
  rbc->spring_stiffness_y = 10.0f;
  rbc->spring_stiffness_z = 10.0f;
  rbc->spring_stiffness_ang_x = 10.0f;
  rbc->spring_stiffness_ang_y = 10.0f;
  rbc->spring_stiffness_ang_z = 10.0f;

  rbc->motor_lin_max_impulse = 1.0f;
  rbc->motor_lin_target_velocity = 1.0f;
  rbc->motor_ang_max_impulse = 1.0f;
  rbc->motor_ang_target_velocity = 1.0f;

  /* flag cache as outdated */
  BKE_rigidbody_cache_reset(rbw);

  /* return this object */
  return rbc;
}

void BKE_rigidbody_objects_collection_validate(Main *bmain, Scene *scene, RigidBodyWorld *rbw)
{
  if (rbw->group != nullptr) {
    FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (rbw->group, object) {
      if (object->type != OB_MESH || object->rigidbody_object != nullptr) {
        continue;
      }
      object->rigidbody_object = BKE_rigidbody_create_object(scene, object, RBO_TYPE_ACTIVE);
      DEG_id_tag_update(&object->id, ID_RECALC_TRANSFORM);
    }
    FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
    DEG_relations_tag_update(bmain);
  }
}

void BKE_rigidbody_constraints_collection_validate(Scene *scene, RigidBodyWorld *rbw)
{
  if (rbw->constraints != nullptr) {
    FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (rbw->constraints, object) {
      if (object->rigidbody_constraint != nullptr) {
        continue;
      }
      object->rigidbody_constraint = BKE_rigidbody_create_constraint(
          scene, object, RBC_TYPE_FIXED);
    }
    FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
  }
}

void BKE_rigidbody_main_collection_object_add(Main *bmain, Collection *collection, Object *object)
{
  for (Scene *scene = static_cast<Scene *>(bmain->scenes.first); scene;
       scene = static_cast<Scene *>(scene->id.next))
  {
    RigidBodyWorld *rbw = scene->rigidbody_world;

    if (rbw == nullptr) {
      continue;
    }

    if (rbw->group == collection && object->type == OB_MESH && object->rigidbody_object == nullptr)
    {
      object->rigidbody_object = BKE_rigidbody_create_object(scene, object, RBO_TYPE_ACTIVE);
    }
    if (rbw->constraints == collection && object->rigidbody_constraint == nullptr) {
      object->rigidbody_constraint = BKE_rigidbody_create_constraint(
          scene, object, RBC_TYPE_FIXED);
    }
  }
}

/* ************************************** */
/* Utilities API */

RigidBodyWorld *BKE_rigidbody_get_world(Scene *scene)
{
  /* sanity check */
  if (scene == nullptr) {
    return nullptr;
  }

  return scene->rigidbody_world;
}

static bool rigidbody_add_object_to_scene(Main *bmain, Scene *scene, Object *ob)
{
  /* Add rigid body world and group if they don't exist for convenience */
  RigidBodyWorld *rbw = BKE_rigidbody_get_world(scene);
  if (rbw == nullptr) {
    rbw = BKE_rigidbody_create_world(scene);
    if (rbw == nullptr) {
      return false;
    }

    BKE_rigidbody_validate_sim_world(scene, rbw, false);
    scene->rigidbody_world = rbw;
  }

  if (rbw->group == nullptr) {
    rbw->group = BKE_collection_add(bmain, nullptr, "RigidBodyWorld");
    id_us_plus(&rbw->group->id);
  }

  /* Add object to rigid body group. */
  BKE_collection_object_add(bmain, rbw->group, ob);
  BKE_rigidbody_cache_reset(rbw);

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&rbw->group->id, ID_RECALC_SYNC_TO_EVAL);

  return true;
}

static bool rigidbody_add_constraint_to_scene(Main *bmain, Scene *scene, Object *ob)
{
  /* Add rigid body world and group if they don't exist for convenience */
  RigidBodyWorld *rbw = BKE_rigidbody_get_world(scene);
  if (rbw == nullptr) {
    rbw = BKE_rigidbody_create_world(scene);
    if (rbw == nullptr) {
      return false;
    }

    BKE_rigidbody_validate_sim_world(scene, rbw, false);
    scene->rigidbody_world = rbw;
  }

  if (rbw->constraints == nullptr) {
    rbw->constraints = BKE_collection_add(bmain, nullptr, "RigidBodyConstraints");
    id_us_plus(&rbw->constraints->id);
  }

  /* Add object to rigid body group. */
  BKE_collection_object_add(bmain, rbw->constraints, ob);
  BKE_rigidbody_cache_reset(rbw);

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&rbw->constraints->id, ID_RECALC_SYNC_TO_EVAL);

  return true;
}

void BKE_rigidbody_ensure_local_object(Main *bmain, Object *ob)
{
  if (ob->rigidbody_object != nullptr) {
    /* Add newly local object to scene. */
    for (Scene *scene = static_cast<Scene *>(bmain->scenes.first); scene;
         scene = static_cast<Scene *>(scene->id.next))
    {
      if (BKE_scene_object_find(scene, ob)) {
        rigidbody_add_object_to_scene(bmain, scene, ob);
      }
    }
  }
  if (ob->rigidbody_constraint != nullptr) {
    /* Add newly local object to scene. */
    for (Scene *scene = static_cast<Scene *>(bmain->scenes.first); scene;
         scene = static_cast<Scene *>(scene->id.next))
    {
      if (BKE_scene_object_find(scene, ob)) {
        rigidbody_add_constraint_to_scene(bmain, scene, ob);
      }
    }
  }
}

bool BKE_rigidbody_add_object(Main *bmain, Scene *scene, Object *ob, int type, ReportList *reports)
{
  if (ob->type != OB_MESH) {
    BKE_report(reports, RPT_ERROR, "Cannot add Rigid Body to non mesh object");
    return false;
  }

  /* Add object to rigid body world in scene. */
  if (!rigidbody_add_object_to_scene(bmain, scene, ob)) {
    BKE_report(reports, RPT_ERROR, "Cannot create Rigid Body world");
    return false;
  }

  /* make rigidbody object settings */
  if (ob->rigidbody_object == nullptr) {
    ob->rigidbody_object = BKE_rigidbody_create_object(scene, ob, type);
  }
  ob->rigidbody_object->type = type;
  ob->rigidbody_object->flag |= RBO_FLAG_NEEDS_VALIDATE;

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);

  return true;
}

void BKE_rigidbody_remove_object(Main *bmain, Scene *scene, Object *ob, const bool free_us)
{
  RigidBodyWorld *rbw = scene->rigidbody_world;
  RigidBodyCon *rbc;
  int i;

  if (rbw) {

    /* remove object from array */
    if (rbw->objects) {
      for (i = 0; i < rbw->numbodies; i++) {
        if (rbw->objects[i] == ob) {
          rbw->objects[i] = nullptr;
          break;
        }
      }
    }

    /* remove object from rigid body constraints */
    if (rbw->constraints) {
      FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (rbw->constraints, obt) {
        if (obt && obt->rigidbody_constraint) {
          rbc = obt->rigidbody_constraint;
          if (rbc->ob1 == ob) {
            rbc->ob1 = nullptr;
            DEG_id_tag_update(&obt->id, ID_RECALC_SYNC_TO_EVAL);
          }
          if (rbc->ob2 == ob) {
            rbc->ob2 = nullptr;
            DEG_id_tag_update(&obt->id, ID_RECALC_SYNC_TO_EVAL);
          }
        }
      }
      FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
    }

    /* Relying on user-count of the object should be OK, and it is much cheaper than looping in all
     * collections to check whether the object is already in another one... */
    if (ID_REAL_USERS(&ob->id) == 1) {
      /* Some users seems to find it funny to use a view-layer instancing collection
       * as RBW collection... Despite this being a bad (ab)use of the system, avoid losing objects
       * when we remove them from RB simulation. */
      BKE_collection_object_add(bmain, scene->master_collection, ob);
    }
    if (rbw->group) {
      BKE_collection_object_remove(bmain, rbw->group, ob, free_us);
      DEG_id_tag_update(&rbw->group->id, ID_RECALC_SYNC_TO_EVAL);
    }

    /* flag cache as outdated */
    BKE_rigidbody_cache_reset(rbw);
    /* Reset cache as the object order probably changed after freeing the object. */
    PTCacheID pid;
    BKE_ptcache_id_from_rigidbody(&pid, nullptr, rbw);
    BKE_ptcache_id_reset(scene, &pid, PTCACHE_RESET_OUTDATED);
  }

  /* remove object's settings */
  BKE_rigidbody_free_object(ob, rbw);

  /* Dependency graph update */
  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);
}

void BKE_rigidbody_remove_constraint(Main *bmain, Scene *scene, Object *ob, const bool free_us)
{
  RigidBodyWorld *rbw = scene->rigidbody_world;
  RigidBodyCon *rbc = ob->rigidbody_constraint;

  if (rbw != nullptr) {
    /* Remove from RBW constraints collection. */
    if (rbw->constraints != nullptr) {
      BKE_collection_object_remove(bmain, rbw->constraints, ob, free_us);
      DEG_id_tag_update(&rbw->constraints->id, ID_RECALC_SYNC_TO_EVAL);
    }

    /* remove from rigidbody world, free object won't do this */
    if (rbw->shared->runtime->physics_world && rbc->physics_constraint) {
      RB_dworld_remove_constraint(rbw->shared->runtime->physics_world,
                                  static_cast<rbConstraint *>(rbc->physics_constraint));
    }
  }

  /* remove object's settings */
  BKE_rigidbody_free_constraint(ob);

  /* flag cache as outdated */
  BKE_rigidbody_cache_reset(rbw);
}

/* ************************************** */
/* Simulation Interface - Bullet */

/* Update object array and rigid body count so they're in sync with the rigid body group */
static void rigidbody_update_ob_array(RigidBodyWorld *rbw)
{
  if (rbw->group == nullptr) {
    rbw->numbodies = 0;
    rbw->objects = static_cast<Object **>(realloc(rbw->objects, 0));
    return;
  }

  int n = 0;
  FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (rbw->group, object) {
    (void)object;
    /* Ignore if this object is the direct child of an object with a compound shape */
    if (object->parent == nullptr || object->parent->rigidbody_object == nullptr ||
        object->parent->rigidbody_object->shape != RB_SHAPE_COMPOUND)
    {
      n++;
    }
  }
  FOREACH_COLLECTION_OBJECT_RECURSIVE_END;

  if (rbw->numbodies != n) {
    rbw->numbodies = n;
    rbw->objects = static_cast<Object **>(
        realloc(rbw->objects, sizeof(Object *) * rbw->numbodies));
  }

  int i = 0;
  FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (rbw->group, object) {
    /* Ignore if this object is the direct child of an object with a compound shape */
    if (object->parent == nullptr || object->parent->rigidbody_object == nullptr ||
        object->parent->rigidbody_object->shape != RB_SHAPE_COMPOUND)
    {
      rbw->objects[i] = object;
      i++;
    }
  }
  FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
}

static void rigidbody_update_sim_world(Scene *scene, RigidBodyWorld *rbw)
{
  float adj_gravity[3];

  /* adjust gravity to take effector weights into account */
  if (scene->physics_settings.flag & PHYS_GLOBAL_GRAVITY) {
    copy_v3_v3(adj_gravity, scene->physics_settings.gravity);
    mul_v3_fl(adj_gravity,
              rbw->effector_weights->global_gravity * rbw->effector_weights->weight[0]);
  }
  else {
    zero_v3(adj_gravity);
  }

  /* update gravity, since this RNA setting is not part of RigidBody settings */
  RB_dworld_set_gravity(rbw->shared->runtime->physics_world, adj_gravity);

  /* update object array in case there are changes */
  rigidbody_update_ob_array(rbw);
}

static void rigidbody_update_sim_ob(Depsgraph *depsgraph, Object *ob, RigidBodyOb *rbo)
{
  /* only update if rigid body exists */
  if (rbo->shared->physics_object == nullptr) {
    return;
  }

  const Scene *scene = DEG_get_input_scene(depsgraph);
  ViewLayer *view_layer = DEG_get_input_view_layer(depsgraph);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Base *base = BKE_view_layer_base_find(view_layer, ob);
  const bool is_selected = base ? (base->flag & BASE_SELECTED) != 0 : false;

  if (rbo->shape == RB_SHAPE_TRIMESH && rbo->flag & RBO_FLAG_USE_DEFORM) {
    const Mesh *mesh = BKE_object_get_mesh_deform_eval(ob);
    if (mesh) {
      const float (*positions)[3] = reinterpret_cast<const float (*)[3]>(
          mesh->vert_positions().data());
      int totvert = mesh->verts_num;
      const std::optional<blender::Bounds<blender::float3>> bounds = BKE_object_boundbox_get(ob);

      RB_shape_trimesh_update(static_cast<rbCollisionShape *>(rbo->shared->physics_shape),
                              (float *)positions,
                              totvert,
                              sizeof(float[3]),
                              bounds->min,
                              bounds->max);
    }
  }

  if (!(rbo->flag & RBO_FLAG_KINEMATIC)) {
    /* update scale for all non kinematic objects */
    float new_scale[3], old_scale[3];
    mat4_to_size(new_scale, ob->object_to_world().ptr());
    RB_body_get_scale(static_cast<rbRigidBody *>(rbo->shared->physics_object), old_scale);

    /* Avoid updating collision shape AABBs if scale didn't change. */
    if (!compare_size_v3v3(old_scale, new_scale, 0.001f)) {
      RB_body_set_scale(static_cast<rbRigidBody *>(rbo->shared->physics_object), new_scale);
      /* compensate for embedded convex hull collision margin */
      if (!(rbo->flag & RBO_FLAG_USE_MARGIN) && rbo->shape == RB_SHAPE_CONVEXH) {
        RB_shape_set_margin(static_cast<rbCollisionShape *>(rbo->shared->physics_shape),
                            RBO_GET_MARGIN(rbo) *
                                std::min({new_scale[0], new_scale[1], new_scale[2]}));
      }
    }
  }

  /* Make transformed objects temporarily kinematic
   * so that they can be moved by the user during simulation. */
  if (is_selected && (G.moving & G_TRANSFORM_OBJ)) {
    RB_body_set_kinematic_state(static_cast<rbRigidBody *>(rbo->shared->physics_object), true);
    RB_body_set_mass(static_cast<rbRigidBody *>(rbo->shared->physics_object), 0.0f);
  }

  /* NOTE: no other settings need to be explicitly updated here,
   * since RNA setters take care of the rest :)
   */
}

/**
 * Updates and validates world, bodies and shapes.
 *
 * \param rebuild: Rebuild entire simulation
 */
static void rigidbody_update_simulation(Depsgraph *depsgraph,
                                        Scene *scene,
                                        RigidBodyWorld *rbw,
                                        bool rebuild)
{
  /* update world */
  /* Note physics_world can get nullptr when undoing the deletion of the last object in it (see
   * #70667). */
  if (rebuild || rbw->shared->runtime->physics_world == nullptr) {
    BKE_rigidbody_validate_sim_world(scene, rbw, rebuild);
    /* We have rebuilt the world so we need to make sure the rest is rebuilt as well. */
    rebuild = true;
  }

  rigidbody_update_sim_world(scene, rbw);

  /* XXX TODO: For rebuild: remove all constraints first.
   * Otherwise we can end up deleting objects that are still
   * referenced by constraints, corrupting bullet's internal list.
   *
   * Memory management needs redesign here, this is just a dirty workaround.
   */
  if (rebuild && rbw->constraints) {
    FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (rbw->constraints, ob) {
      RigidBodyCon *rbc = ob->rigidbody_constraint;
      if (rbc && rbc->physics_constraint) {
        RB_dworld_remove_constraint(rbw->shared->runtime->physics_world,
                                    static_cast<rbConstraint *>(rbc->physics_constraint));
        RB_constraint_delete(static_cast<rbConstraint *>(rbc->physics_constraint));
        rbc->physics_constraint = nullptr;
      }
    }
    FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
  }

  /* update objects */
  FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (rbw->group, ob) {
    if (ob->type == OB_MESH) {
      /* validate that we've got valid object set up here... */
      RigidBodyOb *rbo = ob->rigidbody_object;

      /* TODO: remove this whole block once we are sure we never get nullptr rbo here anymore. */
      /* This cannot be done in copy-on-eval evaluation context anymore... */
      if (rbo == nullptr) {
        BLI_assert_msg(0,
                       "Evaluated object part of RBW object collection without RB object data, "
                       "should not happen.\n");
        /* Since this object is included in the sim group but doesn't have
         * rigid body settings (perhaps it was added manually), add!
         * - assume object to be active? That is the default for newly added settings...
         */
        ob->rigidbody_object = BKE_rigidbody_create_object(scene, ob, RBO_TYPE_ACTIVE);
        rigidbody_validate_sim_object(rbw, ob, true);

        rbo = ob->rigidbody_object;
      }
      else {
        /* perform simulation data updates as tagged */
        /* refresh object... */
        if (rebuild) {
          /* World has been rebuilt so rebuild object */
          /* TODO(Sybren): rigidbody_validate_sim_object() can call rigidbody_validate_sim_shape(),
           * but neither resets the RBO_FLAG_NEEDS_RESHAPE flag nor
           * calls RB_body_set_collision_shape().
           * This results in the collision shape being created twice, which is unnecessary. */
          rigidbody_validate_sim_object(rbw, ob, true);
        }
        else if (rbo->flag & RBO_FLAG_NEEDS_VALIDATE) {
          rigidbody_validate_sim_object(rbw, ob, false);
        }
        /* refresh shape... */
        if (rbo->flag & RBO_FLAG_NEEDS_RESHAPE) {
          /* mesh/shape data changed, so force shape refresh */
          rigidbody_validate_sim_shape(rbw, ob, true);
          /* now tell RB sim about it */
          /* XXX: we assume that this can only get applied for active/passive shapes
           * that will be included as rigid-bodies. */
          if (rbo->shared->physics_object != nullptr && rbo->shared->physics_shape != nullptr) {
            RB_body_set_collision_shape(
                static_cast<rbRigidBody *>(rbo->shared->physics_object),
                static_cast<rbCollisionShape *>(rbo->shared->physics_shape));
          }
        }
      }
      rbo->flag &= ~(RBO_FLAG_NEEDS_VALIDATE | RBO_FLAG_NEEDS_RESHAPE);

      /* update simulation object... */
      rigidbody_update_sim_ob(depsgraph, ob, rbo);
    }
  }
  FOREACH_COLLECTION_OBJECT_RECURSIVE_END;

  /* update constraints */
  if (rbw->constraints == nullptr) { /* no constraints, move on */
    return;
  }

  FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (rbw->constraints, ob) {
    /* validate that we've got valid object set up here... */
    RigidBodyCon *rbc = ob->rigidbody_constraint;

    /* TODO: remove this whole block once we are sure we never get nullptr rbo here anymore. */
    /* This cannot be done in copy-on-eval evaluation context anymore... */
    if (rbc == nullptr) {
      BLI_assert_msg(
          0,
          "Evaluated object part of RBW constraints collection without RB constraint data, "
          "should not happen.\n");
      /* Since this object is included in the group but doesn't have
       * constraint settings (perhaps it was added manually), add!
       */
      ob->rigidbody_constraint = BKE_rigidbody_create_constraint(scene, ob, RBC_TYPE_FIXED);
      rigidbody_validate_sim_constraint(rbw, ob, true);

      rbc = ob->rigidbody_constraint;
    }
    else {
      /* perform simulation data updates as tagged */
      if (rebuild) {
        /* World has been rebuilt so rebuild constraint */
        rigidbody_validate_sim_constraint(rbw, ob, true);
      }
      else if (rbc->flag & RBC_FLAG_NEEDS_VALIDATE) {
        rigidbody_validate_sim_constraint(rbw, ob, false);
      }
    }
    rbc->flag &= ~RBC_FLAG_NEEDS_VALIDATE;
  }
  FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
}

struct KinematicSubstepData {
  RigidBodyOb *rbo;
  float old_pos[3];
  float new_pos[3];
  float old_rot[4];
  float new_rot[4];
  bool scale_changed;
  float old_scale[3];
  float new_scale[3];
};

static ListBase rigidbody_create_substep_data(RigidBodyWorld *rbw)
{
  /* Objects that we want to update substep location/rotation for. */
  ListBase substep_targets = {nullptr, nullptr};

  FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (rbw->group, ob) {
    RigidBodyOb *rbo = ob->rigidbody_object;
    /* only update if rigid body exists */
    if (!rbo || rbo->shared->physics_object == nullptr) {
      continue;
    }

    if (rbo->flag & RBO_FLAG_KINEMATIC) {
      float loc[3], rot[4], scale[3];

      KinematicSubstepData *data = MEM_callocN<KinematicSubstepData>("RigidBody Substep data");

      data->rbo = rbo;

      RB_body_get_position(static_cast<rbRigidBody *>(rbo->shared->physics_object), loc);
      RB_body_get_orientation(static_cast<rbRigidBody *>(rbo->shared->physics_object), rot);
      RB_body_get_scale(static_cast<rbRigidBody *>(rbo->shared->physics_object), scale);

      copy_v3_v3(data->old_pos, loc);
      copy_v4_v4(data->old_rot, rot);
      copy_v3_v3(data->old_scale, scale);

      mat4_decompose(loc, rot, scale, ob->object_to_world().ptr());

      copy_v3_v3(data->new_pos, loc);
      copy_v4_v4(data->new_rot, rot);
      copy_v3_v3(data->new_scale, scale);

      data->scale_changed = !compare_size_v3v3(data->old_scale, data->new_scale, 0.001f);

      LinkData *ob_link = BLI_genericNodeN(data);
      BLI_addtail(&substep_targets, ob_link);
    }
  }
  FOREACH_COLLECTION_OBJECT_RECURSIVE_END;

  return substep_targets;
}

static void rigidbody_update_kinematic_obj_substep(ListBase *substep_targets, float interp_fac)
{
  LISTBASE_FOREACH (LinkData *, link, substep_targets) {
    KinematicSubstepData *data = static_cast<KinematicSubstepData *>(link->data);
    RigidBodyOb *rbo = data->rbo;

    float loc[3], rot[4];

    interp_v3_v3v3(loc, data->old_pos, data->new_pos, interp_fac);
    interp_qt_qtqt(rot, data->old_rot, data->new_rot, interp_fac);

    RB_body_activate(static_cast<rbRigidBody *>(rbo->shared->physics_object));
    RB_body_set_loc_rot(static_cast<rbRigidBody *>(rbo->shared->physics_object), loc, rot);

    if (!data->scale_changed) {
      /* Avoid having to rebuild the collision shape AABBs if scale didn't change. */
      continue;
    }

    float scale[3];

    interp_v3_v3v3(scale, data->old_scale, data->new_scale, interp_fac);

    RB_body_set_scale(static_cast<rbRigidBody *>(rbo->shared->physics_object), scale);

    /* compensate for embedded convex hull collision margin */
    if (!(rbo->flag & RBO_FLAG_USE_MARGIN) && rbo->shape == RB_SHAPE_CONVEXH) {
      RB_shape_set_margin(static_cast<rbCollisionShape *>(rbo->shared->physics_shape),
                          RBO_GET_MARGIN(rbo) * std::min({scale[0], scale[1], scale[2]}));
    }
  }
}

static void rigidbody_update_external_forces(Depsgraph *depsgraph,
                                             Scene *scene,
                                             RigidBodyWorld *rbw)
{
  FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (rbw->group, ob) {
    /* only update if rigid body exists */
    RigidBodyOb *rbo = ob->rigidbody_object;
    if (ob->type != OB_MESH || rbo->shared->physics_object == nullptr) {
      continue;
    }

    /* update influence of effectors - but don't do it on an effector */
    /* only dynamic bodies need effector update */
    if (rbo->type == RBO_TYPE_ACTIVE &&
        ((ob->pd == nullptr) || (ob->pd->forcefield == PFIELD_NULL)))
    {
      EffectorWeights *effector_weights = rbw->effector_weights;
      EffectedPoint epoint;
      ListBase *effectors;

      /* get effectors present in the group specified by effector_weights */
      effectors = BKE_effectors_create(depsgraph, ob, nullptr, effector_weights, false);
      if (effectors) {
        float eff_force[3] = {0.0f, 0.0f, 0.0f};
        float eff_loc[3], eff_vel[3];

        /* create dummy 'point' which represents last known position of object as result of sim
         */
        /* XXX: this can create some inaccuracies with sim position,
         * but is probably better than using un-simulated values? */
        RB_body_get_position(static_cast<rbRigidBody *>(rbo->shared->physics_object), eff_loc);
        RB_body_get_linear_velocity(static_cast<rbRigidBody *>(rbo->shared->physics_object),
                                    eff_vel);

        pd_point_from_loc(scene, eff_loc, eff_vel, 0, &epoint);

        /* Calculate net force of effectors, and apply to sim object:
         * - we use 'central force' since apply force requires a "relative position"
         *   which we don't have... */
        BKE_effectors_apply(
            effectors, nullptr, effector_weights, &epoint, eff_force, nullptr, nullptr);
        if (G.f & G_DEBUG) {
          printf("\tapplying force (%f,%f,%f) to '%s'\n",
                 eff_force[0],
                 eff_force[1],
                 eff_force[2],
                 ob->id.name + 2);
        }
        /* activate object in case it is deactivated */
        if (!is_zero_v3(eff_force)) {
          RB_body_activate(static_cast<rbRigidBody *>(rbo->shared->physics_object));
        }
        if ((rbo->flag & RBO_FLAG_DISABLED) == 0) {
          RB_body_apply_central_force(static_cast<rbRigidBody *>(rbo->shared->physics_object),
                                      eff_force);
        }
      }
      else if (G.f & G_DEBUG) {
        printf("\tno forces to apply to '%s'\n", ob->id.name + 2);
      }

      /* cleanup */
      BKE_effectors_free(effectors);
    }
    /* NOTE: passive objects don't need to be updated since they don't move */
  }
  FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
}

static void rigidbody_free_substep_data(ListBase *substep_targets)
{
  LISTBASE_FOREACH (LinkData *, link, substep_targets) {
    KinematicSubstepData *data = static_cast<KinematicSubstepData *>(link->data);
    MEM_freeN(data);
  }

  BLI_freelistN(substep_targets);
}
static void rigidbody_update_simulation_post_step(Depsgraph *depsgraph, RigidBodyWorld *rbw)
{
  const Scene *scene = DEG_get_input_scene(depsgraph);
  ViewLayer *view_layer = DEG_get_input_view_layer(depsgraph);
  BKE_view_layer_synced_ensure(scene, view_layer);

  FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (rbw->group, ob) {
    Base *base = BKE_view_layer_base_find(view_layer, ob);
    RigidBodyOb *rbo = ob->rigidbody_object;
    /* Reset kinematic state for transformed objects. */
    if (rbo && base && (base->flag & BASE_SELECTED) && (G.moving & G_TRANSFORM_OBJ) &&
        rbo->shared->physics_object)
    {
      RB_body_set_kinematic_state(static_cast<rbRigidBody *>(rbo->shared->physics_object),
                                  rbo->flag & RBO_FLAG_KINEMATIC || rbo->flag & RBO_FLAG_DISABLED);
      RB_body_set_mass(static_cast<rbRigidBody *>(rbo->shared->physics_object), RBO_GET_MASS(rbo));
      /* Deactivate passive objects so they don't interfere with deactivation of active objects. */
      if (rbo->type == RBO_TYPE_PASSIVE) {
        RB_body_deactivate(static_cast<rbRigidBody *>(rbo->shared->physics_object));
      }
    }
  }
  FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
}

bool BKE_rigidbody_check_sim_running(RigidBodyWorld *rbw, float ctime)
{
  return (rbw && (rbw->flag & RBW_FLAG_MUTED) == 0 && ctime > rbw->shared->pointcache->startframe);
}

void BKE_rigidbody_sync_transforms(RigidBodyWorld *rbw, Object *ob, float ctime)
{
  if (!BKE_rigidbody_is_affected_by_simulation(ob)) {
    /* Don't sync transforms for objects that are not affected/changed by the simulation. */
    return;
  }

  RigidBodyOb *rbo = ob->rigidbody_object;

  /* use rigid body transform after cache start frame if objects is not being transformed */
  if (BKE_rigidbody_check_sim_running(rbw, ctime) &&
      !(ob->base_flag & BASE_SELECTED && G.moving & G_TRANSFORM_OBJ))
  {
    float mat[4][4], size_mat[4][4], size[3];

    normalize_qt(rbo->orn); /* RB_TODO investigate why quaternion isn't normalized at this point */
    quat_to_mat4(mat, rbo->orn);
    copy_v3_v3(mat[3], rbo->pos);

    mat4_to_size(size, ob->object_to_world().ptr());
    size_to_mat4(size_mat, size);
    mul_m4_m4m4(mat, mat, size_mat);

    copy_m4_m4(ob->runtime->object_to_world.ptr(), mat);
  }
  /* otherwise set rigid body transform to current obmat */
  else {
    mat4_to_loc_quat(rbo->pos, rbo->orn, ob->object_to_world().ptr());
  }
}

void BKE_rigidbody_aftertrans_update(
    Object *ob, float loc[3], float rot[3], float quat[4], float rotAxis[3], float rotAngle)
{
  bool correct_delta = BKE_rigidbody_is_affected_by_simulation(ob);
  RigidBodyOb *rbo = ob->rigidbody_object;

  /* return rigid body and object to their initial states */
  copy_v3_v3(rbo->pos, ob->loc);
  copy_v3_v3(ob->loc, loc);

  if (correct_delta) {
    add_v3_v3(rbo->pos, ob->dloc);
  }

  if (ob->rotmode > 0) {
    float qt[4];
    eulO_to_quat(qt, ob->rot, ob->rotmode);

    if (correct_delta) {
      float dquat[4];
      eulO_to_quat(dquat, ob->drot, ob->rotmode);

      mul_qt_qtqt(rbo->orn, dquat, qt);
    }
    else {
      copy_qt_qt(rbo->orn, qt);
    }

    copy_v3_v3(ob->rot, rot);
  }
  else if (ob->rotmode == ROT_MODE_AXISANGLE) {
    float qt[4];
    axis_angle_to_quat(qt, ob->rotAxis, ob->rotAngle);

    if (correct_delta) {
      float dquat[4];
      axis_angle_to_quat(dquat, ob->drotAxis, ob->drotAngle);

      mul_qt_qtqt(rbo->orn, dquat, qt);
    }
    else {
      copy_qt_qt(rbo->orn, qt);
    }

    copy_v3_v3(ob->rotAxis, rotAxis);
    ob->rotAngle = rotAngle;
  }
  else {
    if (correct_delta) {
      mul_qt_qtqt(rbo->orn, ob->dquat, ob->quat);
    }
    else {
      copy_qt_qt(rbo->orn, ob->quat);
    }

    copy_qt_qt(ob->quat, quat);
  }

  if (rbo->shared->physics_object) {
    /* allow passive objects to return to original transform */
    if (rbo->type == RBO_TYPE_PASSIVE) {
      RB_body_set_kinematic_state(static_cast<rbRigidBody *>(rbo->shared->physics_object), true);
    }
    RB_body_set_loc_rot(
        static_cast<rbRigidBody *>(rbo->shared->physics_object), rbo->pos, rbo->orn);
  }
  /* RB_TODO update rigid body physics object's loc/rot for dynamic objects here as well
   * (needs to be done outside bullet's update loop). */
}

void BKE_rigidbody_cache_reset(RigidBodyWorld *rbw)
{
  if (rbw) {
    rbw->shared->pointcache->flag |= PTCACHE_OUTDATED;
  }
}

/* ------------------ */

void BKE_rigidbody_rebuild_world(Depsgraph *depsgraph, Scene *scene, float ctime)
{
  RigidBodyWorld *rbw = scene->rigidbody_world;
  PointCache *cache;
  PTCacheID pid;
  int startframe, endframe;

  /* Avoid multiple depsgraph evaluations accessing the same shared data. */
  std::unique_lock lock(rbw->shared->runtime->mutex);

  BKE_ptcache_id_from_rigidbody(&pid, nullptr, rbw);
  BKE_ptcache_id_time(&pid, scene, ctime, &startframe, &endframe, nullptr);
  cache = rbw->shared->pointcache;

  /* Flag cache as outdated if we don't have a world or number of objects
   * in the simulation has changed. */
  int n = 0;
  FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (rbw->group, object) {
    (void)object;
    /* Ignore if this object is the direct child of an object with a compound shape */
    if (object->parent == nullptr || object->parent->rigidbody_object == nullptr ||
        object->parent->rigidbody_object->shape != RB_SHAPE_COMPOUND)
    {
      n++;
    }
  }
  FOREACH_COLLECTION_OBJECT_RECURSIVE_END;

  if (rbw->shared->runtime->physics_world == nullptr || rbw->numbodies != n) {
    cache->flag |= PTCACHE_OUTDATED;
  }

  if (ctime == startframe + 1 && rbw->ltime == startframe) {
    if (cache->flag & PTCACHE_OUTDATED) {
      BKE_ptcache_id_reset(scene, &pid, PTCACHE_RESET_OUTDATED);
      rigidbody_update_simulation(depsgraph, scene, rbw, true);
      BKE_ptcache_validate(cache, int(ctime));
      cache->last_exact = 0;
      cache->flag &= ~PTCACHE_REDO_NEEDED;
    }
  }
}

void BKE_rigidbody_do_simulation(Depsgraph *depsgraph, Scene *scene, float ctime)
{
  RigidBodyWorld *rbw = scene->rigidbody_world;
  PointCache *cache;
  PTCacheID pid;
  int startframe, endframe;

  BKE_ptcache_id_from_rigidbody(&pid, nullptr, rbw);
  BKE_ptcache_id_time(&pid, scene, ctime, &startframe, &endframe, nullptr);
  cache = rbw->shared->pointcache;

  if (ctime <= startframe) {
    rbw->ltime = startframe;
    return;
  }
  /* make sure we don't go out of cache frame range */
  ctime = std::min<float>(ctime, endframe);

  /* don't try to run the simulation if we don't have a world yet but allow reading baked cache */
  if (rbw->shared->runtime->physics_world == nullptr && !(cache->flag & PTCACHE_BAKED)) {
    return;
  }
  if (rbw->objects == nullptr) {
    rigidbody_update_ob_array(rbw);
  }

  /* try to read from cache */
  /* RB_TODO deal with interpolated, old and baked results */
  bool can_simulate = (ctime == rbw->ltime + 1) && !(cache->flag & PTCACHE_BAKED);

  if (BKE_ptcache_read(&pid, ctime, can_simulate) == PTCACHE_READ_EXACT) {
    BKE_ptcache_validate(cache, int(ctime));
    rbw->ltime = ctime;
    return;
  }

  if (!DEG_is_active(depsgraph)) {
    /* When the depsgraph is inactive we should neither write to the cache
     * nor run the simulation. */
    return;
  }

  /* advance simulation, we can only step one frame forward */
  if (compare_ff_relative(ctime, rbw->ltime + 1, FLT_EPSILON, 64)) {
    /* write cache for first frame when on second frame */
    if (rbw->ltime == startframe && (cache->flag & PTCACHE_OUTDATED || cache->last_exact == 0)) {
      BKE_ptcache_write(&pid, startframe);
    }

    const float frame_diff = ctime - rbw->ltime;
    /* calculate how much time elapsed since last step in seconds */
    const float timestep = 1.0f / float(scene->frames_per_second()) * frame_diff * rbw->time_scale;

    const float substep = timestep / rbw->substeps_per_frame;

    ListBase kinematic_substep_targets = rigidbody_create_substep_data(rbw);

    const float interp_step = 1.0f / rbw->substeps_per_frame;
    float cur_interp_val = interp_step;

    /* update and validate simulation */
    rigidbody_update_simulation(depsgraph, scene, rbw, false);

    for (int i = 0; i < rbw->substeps_per_frame; i++) {
      rigidbody_update_external_forces(depsgraph, scene, rbw);
      rigidbody_update_kinematic_obj_substep(&kinematic_substep_targets, cur_interp_val);
      RB_dworld_step_simulation(rbw->shared->runtime->physics_world, substep, 0, substep);
      cur_interp_val += interp_step;
    }
    rigidbody_free_substep_data(&kinematic_substep_targets);

    rigidbody_update_simulation_post_step(depsgraph, rbw);

    /* write cache for current frame */
    BKE_ptcache_validate(cache, int(ctime));
    BKE_ptcache_write(&pid, uint(ctime));

    rbw->ltime = ctime;
  }
}
/* ************************************** */

#else /* WITH_BULLET */

/* stubs */
#  if defined(__GNUC__) || defined(__clang__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wunused-parameter"
#  elif defined(_MSC_VER)
/* Suppress unreferenced formal parameter warning. */
#    pragma warning(disable : 4100)
#  endif

void BKE_rigidbody_validate_sim_world(Scene *scene, RigidBodyWorld *rbw, bool rebuild) {}

void BKE_rigidbody_calc_volume(Object *ob, float *r_vol)
{
  if (r_vol) {
    *r_vol = 0.0f;
  }
}
void BKE_rigidbody_calc_center_of_mass(Object *ob, float r_center[3])
{
  zero_v3(r_center);
}
RigidBodyWorld *BKE_rigidbody_create_world(Scene *scene)
{
  return nullptr;
}
RigidBodyWorld *BKE_rigidbody_world_copy(RigidBodyWorld *rbw, const int flag)
{
  return nullptr;
}
void BKE_rigidbody_world_groups_relink(RigidBodyWorld *rbw) {}
RigidBodyOb *BKE_rigidbody_create_object(Scene *scene, Object *ob, short type)
{
  return nullptr;
}
RigidBodyCon *BKE_rigidbody_create_constraint(Scene *scene, Object *ob, short type)
{
  return nullptr;
}
RigidBodyWorld *BKE_rigidbody_get_world(Scene *scene)
{
  return nullptr;
}

void BKE_rigidbody_ensure_local_object(Main *bmain, Object *ob) {}

bool BKE_rigidbody_add_object(Main *bmain, Scene *scene, Object *ob, int type, ReportList *reports)
{
  BKE_report(reports, RPT_ERROR, "Compiled without Bullet physics engine");
  return false;
}

void BKE_rigidbody_remove_object(Main *bmain, Scene *scene, Object *ob, const bool free_us) {}
void BKE_rigidbody_remove_constraint(Main *bmain, Scene *scene, Object *ob, const bool free_us) {}
void BKE_rigidbody_sync_transforms(RigidBodyWorld *rbw, Object *ob, float ctime) {}
void BKE_rigidbody_aftertrans_update(
    Object *ob, float loc[3], float rot[3], float quat[4], float rotAxis[3], float rotAngle)
{
}
bool BKE_rigidbody_check_sim_running(RigidBodyWorld *rbw, float ctime)
{
  return false;
}
void BKE_rigidbody_cache_reset(RigidBodyWorld *rbw) {}
void BKE_rigidbody_rebuild_world(Depsgraph *depsgraph, Scene *scene, float ctime) {}
void BKE_rigidbody_do_simulation(Depsgraph *depsgraph, Scene *scene, float ctime) {}
void BKE_rigidbody_objects_collection_validate(Main *bmain, Scene *scene, RigidBodyWorld *rbw) {}
void BKE_rigidbody_constraints_collection_validate(Scene *scene, RigidBodyWorld *rbw) {}
void BKE_rigidbody_main_collection_object_add(Main *bmain, Collection *collection, Object *object)
{
}

#  if defined(__GNUC__) || defined(__clang__)
#    pragma GCC diagnostic pop
#  endif

#endif /* WITH_BULLET */

/* -------------------- */
/* Depsgraph evaluation */

void BKE_rigidbody_rebuild_sim(Depsgraph *depsgraph, Scene *scene)
{
  float ctime = DEG_get_ctime(depsgraph);
  DEG_debug_print_eval_time(depsgraph, __func__, scene->id.name, scene, ctime);
  /* rebuild sim data (i.e. after resetting to start of timeline) */
  if (BKE_scene_check_rigidbody_active(scene)) {
    BKE_rigidbody_rebuild_world(depsgraph, scene, ctime);
  }
}

void BKE_rigidbody_eval_simulation(Depsgraph *depsgraph, Scene *scene)
{
  float ctime = DEG_get_ctime(depsgraph);
  DEG_debug_print_eval_time(depsgraph, __func__, scene->id.name, scene, ctime);

  /* evaluate rigidbody sim */
  if (!BKE_scene_check_rigidbody_active(scene)) {
    return;
  }
  BKE_rigidbody_do_simulation(depsgraph, scene, ctime);
}

void BKE_rigidbody_object_sync_transforms(Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  RigidBodyWorld *rbw = scene->rigidbody_world;
  float ctime = DEG_get_ctime(depsgraph);
  DEG_debug_print_eval_time(depsgraph, __func__, ob->id.name, ob, ctime);
  /* read values pushed into RBO from sim/cache... */
  BKE_rigidbody_sync_transforms(rbw, ob, ctime);
}

void BKE_rigidbody_world_id_loop(RigidBodyWorld *rbw, RigidbodyWorldIDFunc func, void *userdata)
{
  func(rbw, (ID **)&rbw->group, userdata, IDWALK_CB_USER);
  func(rbw, (ID **)&rbw->constraints, userdata, IDWALK_CB_USER);
  func(rbw, (ID **)&rbw->effector_weights->group, userdata, IDWALK_CB_USER);

  if (rbw->objects) {
    int i;
    for (i = 0; i < rbw->numbodies; i++) {
      func(rbw, (ID **)&rbw->objects[i], userdata, IDWALK_CB_NOP);
    }
  }
}

/* Copying Methods --------------------- */

/* These just copy the data, clearing out references to physics objects.
 * Anything that uses them MUST verify that the copied object will
 * be added to relevant groups later...
 */

static RigidBodyOb *rigidbody_copy_object(const Object *ob, const int flag)
{
  RigidBodyOb *rboN = nullptr;

  if (ob->rigidbody_object) {
    const bool is_orig = (flag & LIB_ID_COPY_SET_COPIED_ON_WRITE) == 0;

    /* just duplicate the whole struct first (to catch all the settings) */
    rboN = static_cast<RigidBodyOb *>(MEM_dupallocN(ob->rigidbody_object));

    if (is_orig) {
      /* This is a regular copy, and not an evaluated copy for depsgraph evaluation */
      rboN->shared = MEM_callocN<RigidBodyOb_Shared>("RigidBodyOb_Shared");
    }

    /* tag object as needing to be verified */
    rboN->flag |= RBO_FLAG_NEEDS_VALIDATE;
  }

  /* return new copy of settings */
  return rboN;
}

static RigidBodyCon *rigidbody_copy_constraint(const Object *ob, const int /*flag*/)
{
  RigidBodyCon *rbcN = nullptr;

  if (ob->rigidbody_constraint) {
    /* Just duplicate the whole struct first (to catch all the settings). */
    rbcN = static_cast<RigidBodyCon *>(MEM_dupallocN(ob->rigidbody_constraint));

    /* Tag object as needing to be verified. */
    rbcN->flag |= RBC_FLAG_NEEDS_VALIDATE;

    /* Clear out all the fields which need to be re-validated later. */
    rbcN->physics_constraint = nullptr;
  }

  /* return new copy of settings */
  return rbcN;
}

void BKE_rigidbody_object_copy(Main *bmain, Object *ob_dst, const Object *ob_src, const int flag)
{
  ob_dst->rigidbody_object = rigidbody_copy_object(ob_src, flag);
  ob_dst->rigidbody_constraint = rigidbody_copy_constraint(ob_src, flag);

  if ((flag & (LIB_ID_CREATE_NO_MAIN | LIB_ID_COPY_RIGID_BODY_NO_COLLECTION_HANDLING)) != 0) {
    return;
  }

  /* We have to ensure that duplicated object ends up in relevant rigidbody collections...
   * Otherwise duplicating the RB data itself is meaningless. */
  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    RigidBodyWorld *rigidbody_world = scene->rigidbody_world;

    if (rigidbody_world != nullptr) {
      bool need_objects_update = false;
      bool need_constraints_update = false;

      if (ob_dst->rigidbody_object) {
        if (BKE_collection_has_object(rigidbody_world->group, ob_src)) {
          BKE_collection_object_add(bmain, rigidbody_world->group, ob_dst);
          need_objects_update = true;
        }
      }
      if (ob_dst->rigidbody_constraint) {
        if (BKE_collection_has_object(rigidbody_world->constraints, ob_src)) {
          BKE_collection_object_add(bmain, rigidbody_world->constraints, ob_dst);
          need_constraints_update = true;
        }
      }

      if ((flag & LIB_ID_CREATE_NO_DEG_TAG) == 0 &&
          (need_objects_update || need_constraints_update))
      {
        BKE_rigidbody_cache_reset(rigidbody_world);

        DEG_relations_tag_update(bmain);
        if (need_objects_update) {
          DEG_id_tag_update(&rigidbody_world->group->id, ID_RECALC_SYNC_TO_EVAL);
        }
        if (need_constraints_update) {
          DEG_id_tag_update(&rigidbody_world->constraints->id, ID_RECALC_SYNC_TO_EVAL);
        }
        DEG_id_tag_update(&ob_dst->id, ID_RECALC_TRANSFORM);
      }
    }
  }
}
