/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2013 Blender Foundation
 * All rights reserved.
 */

/** \file
 * \ingroup blenkernel
 * \brief API for Blender-side Rigid Body stuff
 */

#ifndef __BKE_RIGIDBODY_H__
#define __BKE_RIGIDBODY_H__

struct RigidBodyOb;
struct RigidBodyWorld;

struct Collection;
struct Depsgraph;
struct Main;
struct Object;
struct ReportList;
struct Scene;

/* -------------- */
/* Memory Management */

void BKE_rigidbody_free_world(struct Scene *scene);
void BKE_rigidbody_free_object(struct Object *ob, struct RigidBodyWorld *rbw);
void BKE_rigidbody_free_constraint(struct Object *ob);

/* ...... */

void BKE_rigidbody_object_copy(struct Main *bmain,
                               struct Object *ob_dst,
                               const struct Object *ob_src,
                               const int flag);

/* Callback format for performing operations on ID-pointers for rigidbody world. */
typedef void (*RigidbodyWorldIDFunc)(struct RigidBodyWorld *rbw,
                                     struct ID **idpoin,
                                     void *userdata,
                                     int cb_flag);

void BKE_rigidbody_world_id_loop(struct RigidBodyWorld *rbw,
                                 RigidbodyWorldIDFunc func,
                                 void *userdata);

/* -------------- */
/* Setup */

/* create Blender-side settings data - physics objects not initialized yet */
struct RigidBodyWorld *BKE_rigidbody_create_world(struct Scene *scene);
struct RigidBodyOb *BKE_rigidbody_create_object(struct Scene *scene,
                                                struct Object *ob,
                                                short type);
struct RigidBodyCon *BKE_rigidbody_create_constraint(struct Scene *scene,
                                                     struct Object *ob,
                                                     short type);

/* Ensure newly set collections' objects all have required data. */
void BKE_rigidbody_objects_collection_validate(struct Scene *scene, struct RigidBodyWorld *rbw);
void BKE_rigidbody_constraints_collection_validate(struct Scene *scene,
                                                   struct RigidBodyWorld *rbw);

/* Ensure object added to collection gets RB data if that collection is a RB one. */
void BKE_rigidbody_main_collection_object_add(struct Main *bmain,
                                              struct Collection *collection,
                                              struct Object *object);

/* copy */
struct RigidBodyWorld *BKE_rigidbody_world_copy(struct RigidBodyWorld *rbw, const int flag);
void BKE_rigidbody_world_groups_relink(struct RigidBodyWorld *rbw);

/* 'validate' (i.e. make new or replace old) Physics-Engine objects */
void BKE_rigidbody_validate_sim_world(struct Scene *scene,
                                      struct RigidBodyWorld *rbw,
                                      bool rebuild);

void BKE_rigidbody_calc_volume(struct Object *ob, float *r_vol);
void BKE_rigidbody_calc_center_of_mass(struct Object *ob, float r_center[3]);

/* -------------- */
/* Utilities */

struct RigidBodyWorld *BKE_rigidbody_get_world(struct Scene *scene);
bool BKE_rigidbody_add_object(struct Main *bmain,
                              struct Scene *scene,
                              struct Object *ob,
                              int type,
                              struct ReportList *reports);
void BKE_rigidbody_ensure_local_object(struct Main *bmain, struct Object *ob);
void BKE_rigidbody_remove_object(struct Main *bmain, struct Scene *scene, struct Object *ob);
void BKE_rigidbody_remove_constraint(struct Scene *scene, struct Object *ob);

/* -------------- */
/* Utility Macros */

/* get mass of Rigid Body Object to supply to RigidBody simulators */
#define RBO_GET_MASS(rbo) \
  ((rbo && ((rbo->type == RBO_TYPE_PASSIVE) || (rbo->flag & RBO_FLAG_KINEMATIC) || \
            (rbo->flag & RBO_FLAG_DISABLED))) ? \
       (0.0f) : \
       (rbo->mass))
/* Get collision margin for Rigid Body Object, triangle mesh and cone shapes cannot embed margin,
 * convex hull always uses custom margin. */
#define RBO_GET_MARGIN(rbo) \
  ((rbo->flag & RBO_FLAG_USE_MARGIN || rbo->shape == RB_SHAPE_CONVEXH || \
    rbo->shape == RB_SHAPE_TRIMESH || rbo->shape == RB_SHAPE_CONE) ? \
       (rbo->margin) : \
       (0.04f))

/* -------------- */
/* Simulation */

void BKE_rigidbody_aftertrans_update(struct Object *ob,
                                     float loc[3],
                                     float rot[3],
                                     float quat[4],
                                     float rotAxis[3],
                                     float rotAngle);
void BKE_rigidbody_sync_transforms(struct RigidBodyWorld *rbw, struct Object *ob, float ctime);
bool BKE_rigidbody_check_sim_running(struct RigidBodyWorld *rbw, float ctime);
void BKE_rigidbody_cache_reset(struct RigidBodyWorld *rbw);
void BKE_rigidbody_rebuild_world(struct Depsgraph *depsgraph, struct Scene *scene, float ctime);
void BKE_rigidbody_do_simulation(struct Depsgraph *depsgraph, struct Scene *scene, float ctime);

/* -------------------- */
/* Depsgraph evaluation */

void BKE_rigidbody_rebuild_sim(struct Depsgraph *depsgraph, struct Scene *scene);

void BKE_rigidbody_eval_simulation(struct Depsgraph *depsgraph, struct Scene *scene);

void BKE_rigidbody_object_sync_transforms(struct Depsgraph *depsgraph,
                                          struct Scene *scene,
                                          struct Object *ob);

#endif /* __BKE_RIGIDBODY_H__ */
