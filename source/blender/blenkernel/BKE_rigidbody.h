/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 * \brief API for Blender-side Rigid Body stuff
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct RigidBodyOb;
struct RigidBodyWorld;

struct Collection;
struct Depsgraph;
struct Main;
struct Object;
struct ReportList;
struct Scene;

/* -------------------------------------------------------------------- */
/** \name Memory Management
 * \{ */

/**
 * Free rigid-body world.
 */
void BKE_rigidbody_free_world(struct Scene *scene);
/**
 * Free rigid-body settings and simulation instances.
 */
void BKE_rigidbody_free_object(struct Object *ob, struct RigidBodyWorld *rbw);
/**
 * Free rigid-body constraint and simulation instance.
 */
void BKE_rigidbody_free_constraint(struct Object *ob);

/* ...... */

void BKE_rigidbody_object_copy(struct Main *bmain,
                               struct Object *ob_dst,
                               const struct Object *ob_src,
                               int flag);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Iterator
 * \{ */

/**
 * Callback format for performing operations on ID-pointers for rigid-body world.
 */
typedef void (*RigidbodyWorldIDFunc)(struct RigidBodyWorld *rbw,
                                     struct ID **idpoin,
                                     void *userdata,
                                     int cb_flag);

void BKE_rigidbody_world_id_loop(struct RigidBodyWorld *rbw,
                                 RigidbodyWorldIDFunc func,
                                 void *userdata);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Setup
 * \{ */

/**
 * Set up RigidBody world.
 *
 * Create Blender-side settings data - physics objects not initialized yet.
 */
struct RigidBodyWorld *BKE_rigidbody_create_world(struct Scene *scene);
/**
 * Add rigid body settings to the specified object.
 */
struct RigidBodyOb *BKE_rigidbody_create_object(struct Scene *scene,
                                                struct Object *ob,
                                                short type);
/**
 * Add rigid body constraint to the specified object.
 */
struct RigidBodyCon *BKE_rigidbody_create_constraint(struct Scene *scene,
                                                     struct Object *ob,
                                                     short type);

/**
 * Ensure newly set collections' objects all have required data.
 */
void BKE_rigidbody_objects_collection_validate(struct Main *bmain,
                                               struct Scene *scene,
                                               struct RigidBodyWorld *rbw);
void BKE_rigidbody_constraints_collection_validate(struct Scene *scene,
                                                   struct RigidBodyWorld *rbw);

/**
 * Ensure object added to collection gets RB data if that collection is a RB one.
 */
void BKE_rigidbody_main_collection_object_add(struct Main *bmain,
                                              struct Collection *collection,
                                              struct Object *object);

/**
 * Copy.
 */
struct RigidBodyWorld *BKE_rigidbody_world_copy(struct RigidBodyWorld *rbw, int flag);
void BKE_rigidbody_world_groups_relink(struct RigidBodyWorld *rbw);

/**
 * 'validate' (i.e. make new or replace old) Physics-Engine objects.
 */
/**
 * Create physics sim world given RigidBody world settings
 *
 * \note this does NOT update object references that the scene uses,
 * in case those aren't ready yet!
 */
void BKE_rigidbody_validate_sim_world(struct Scene *scene,
                                      struct RigidBodyWorld *rbw,
                                      bool rebuild);

/**
 * Helper function to calculate volume of rigid-body object.
 *
 * TODO: allow a parameter to specify method used to calculate this?
 */
void BKE_rigidbody_calc_volume(struct Object *ob, float *r_vol);
void BKE_rigidbody_calc_center_of_mass(struct Object *ob, float r_center[3]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

/**
 * Get RigidBody world for the given scene, creating one if needed
 *
 * \param scene: Scene to find active Rigid Body world for.
 */
struct RigidBodyWorld *BKE_rigidbody_get_world(struct Scene *scene);
bool BKE_rigidbody_add_object(struct Main *bmain,
                              struct Scene *scene,
                              struct Object *ob,
                              int type,
                              struct ReportList *reports);
void BKE_rigidbody_ensure_local_object(struct Main *bmain, struct Object *ob);
void BKE_rigidbody_remove_object(struct Main *bmain,
                                 struct Scene *scene,
                                 struct Object *ob,
                                 bool free_us);
void BKE_rigidbody_remove_constraint(struct Main *bmain,
                                     struct Scene *scene,
                                     struct Object *ob,
                                     bool free_us);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utility Macros
 * \{ */

/**
 * Get mass of Rigid Body Object to supply to RigidBody simulators.
 */
#define RBO_GET_MASS(rbo) \
  (((rbo) && (((rbo)->type == RBO_TYPE_PASSIVE) || ((rbo)->flag & RBO_FLAG_KINEMATIC) || \
              ((rbo)->flag & RBO_FLAG_DISABLED))) ? \
       (0.0f) : \
       ((rbo)->mass))
/**
 * Get collision margin for Rigid Body Object, triangle mesh and cone shapes cannot embed margin,
 * convex hull always uses custom margin.
 */
#define RBO_GET_MARGIN(rbo) \
  (((rbo)->flag & RBO_FLAG_USE_MARGIN || (rbo)->shape == RB_SHAPE_CONVEXH || \
    (rbo)->shape == RB_SHAPE_TRIMESH || (rbo)->shape == RB_SHAPE_CONE) ? \
       ((rbo)->margin) : \
       (0.04f))

/** \} */

/* -------------------------------------------------------------------- */
/** \name Simulation
 * \{ */

/**
 * Used when canceling transforms - return rigidbody and object to initial states.
 */
void BKE_rigidbody_aftertrans_update(struct Object *ob,
                                     float loc[3],
                                     float rot[3],
                                     float quat[4],
                                     float rotAxis[3],
                                     float rotAngle);
/**
 * Sync rigid body and object transformations.
 */
void BKE_rigidbody_sync_transforms(struct RigidBodyWorld *rbw, struct Object *ob, float ctime);
bool BKE_rigidbody_check_sim_running(struct RigidBodyWorld *rbw, float ctime);
bool BKE_rigidbody_is_affected_by_simulation(struct Object *ob);
void BKE_rigidbody_cache_reset(struct RigidBodyWorld *rbw);
/**
 * Rebuild rigid body world.
 *
 * NOTE: this needs to be called before frame update to work correctly.
 */
void BKE_rigidbody_rebuild_world(struct Depsgraph *depsgraph, struct Scene *scene, float ctime);
/**
 * Run RigidBody simulation for the specified physics world.
 */
void BKE_rigidbody_do_simulation(struct Depsgraph *depsgraph, struct Scene *scene, float ctime);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Depsgraph evaluation
 * \{ */

void BKE_rigidbody_rebuild_sim(struct Depsgraph *depsgraph, struct Scene *scene);

void BKE_rigidbody_eval_simulation(struct Depsgraph *depsgraph, struct Scene *scene);

void BKE_rigidbody_object_sync_transforms(struct Depsgraph *depsgraph,
                                          struct Scene *scene,
                                          struct Object *ob);

/** \} */

#ifdef __cplusplus
}
#endif
