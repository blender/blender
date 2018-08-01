/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joshua Leung, Sergej Reich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file rigidbody.c
 *  \ingroup blenkernel
 *  \brief Blender-side interface and methods for dealing with Rigid Body simulations
 */

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <float.h>
#include <math.h>
#include <limits.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"

#ifdef WITH_BULLET
#  include "RBI_api.h"
#endif

#include "DNA_ID.h"
#include "DNA_group_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"

#include "BKE_collection.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_layer.h"
#include "BKE_library.h"
#include "BKE_library_query.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_object.h"
#include "BKE_pointcache.h"
#include "BKE_rigidbody.h"
#include "BKE_scene.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

/* ************************************** */
/* Memory Management */

/* Freeing Methods --------------------- */

#ifdef WITH_BULLET
static void rigidbody_update_ob_array(RigidBodyWorld *rbw);

#else
static void RB_dworld_remove_constraint(void *UNUSED(world), void *UNUSED(con)) {}
static void RB_dworld_remove_body(void *UNUSED(world), void *UNUSED(body)) {}
static void RB_dworld_delete(void *UNUSED(world)) {}
static void RB_body_delete(void *UNUSED(body)) {}
static void RB_shape_delete(void *UNUSED(shape)) {}
static void RB_constraint_delete(void *UNUSED(con)) {}

#endif

/* Free rigidbody world */
void BKE_rigidbody_free_world(Scene *scene)
{
	bool is_orig = (scene->id.tag & LIB_TAG_COPIED_ON_WRITE) == 0;
	RigidBodyWorld *rbw = scene->rigidbody_world;
	scene->rigidbody_world = NULL;

	/* sanity check */
	if (!rbw)
		return;

	if (is_orig && rbw->shared->physics_world) {
		/* free physics references, we assume that all physics objects in will have been added to the world */
		if (rbw->constraints) {
			FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN(rbw->constraints, object)
			{
				if (object->rigidbody_constraint) {
					RigidBodyCon *rbc = object->rigidbody_constraint;
					if (rbc->physics_constraint) {
						RB_dworld_remove_constraint(rbw->shared->physics_world, rbc->physics_constraint);
					}
				}
			}
			FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
		}

		if (rbw->group) {
			FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN(rbw->group, object)
			{
				BKE_rigidbody_free_object(object, rbw);
			}
			FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
		}
		/* free dynamics world */
		RB_dworld_delete(rbw->shared->physics_world);
	}
	if (rbw->objects)
		free(rbw->objects);

	if (is_orig) {
		/* free cache */
		BKE_ptcache_free_list(&(rbw->shared->ptcaches));
		rbw->shared->pointcache = NULL;

		MEM_freeN(rbw->shared);
	}

	/* free effector weights */
	if (rbw->effector_weights)
		MEM_freeN(rbw->effector_weights);

	/* free rigidbody world itself */
	MEM_freeN(rbw);
}

/* Free RigidBody settings and sim instances */
void BKE_rigidbody_free_object(Object *ob, RigidBodyWorld *rbw)
{
	bool is_orig = (ob->id.tag & LIB_TAG_COPIED_ON_WRITE) == 0;
	RigidBodyOb *rbo = (ob) ? ob->rigidbody_object : NULL;

	/* sanity check */
	if (rbo == NULL)
		return;

	/* free physics references */
	if (is_orig) {
		if (rbo->shared->physics_object) {
			BLI_assert(rbw);
			if (rbw) {
				/* We can only remove the body from the world if the world is known.
				 * The world is generally only unknown if it's an evaluated copy of
				 * an object that's being freed, in which case this code isn't run anyway. */
				RB_dworld_remove_body(rbw->shared->physics_world, rbo->shared->physics_object);
			}

			RB_body_delete(rbo->shared->physics_object);
			rbo->shared->physics_object = NULL;
		}

		if (rbo->shared->physics_shape) {
			RB_shape_delete(rbo->shared->physics_shape);
			rbo->shared->physics_shape = NULL;
		}

		MEM_freeN(rbo->shared);
	}

	/* free data itself */
	MEM_freeN(rbo);
	ob->rigidbody_object = NULL;
}

/* Free RigidBody constraint and sim instance */
void BKE_rigidbody_free_constraint(Object *ob)
{
	RigidBodyCon *rbc = (ob) ? ob->rigidbody_constraint : NULL;

	/* sanity check */
	if (rbc == NULL)
		return;

	/* free physics reference */
	if (rbc->physics_constraint) {
		RB_constraint_delete(rbc->physics_constraint);
		rbc->physics_constraint = NULL;
	}

	/* free data itself */
	MEM_freeN(rbc);
	ob->rigidbody_constraint = NULL;
}

#ifdef WITH_BULLET

/* Copying Methods --------------------- */

/* These just copy the data, clearing out references to physics objects.
 * Anything that uses them MUST verify that the copied object will
 * be added to relevant groups later...
 */

RigidBodyOb *BKE_rigidbody_copy_object(const Object *ob, const int flag)
{
	RigidBodyOb *rboN = NULL;

	if (ob->rigidbody_object) {
		/* just duplicate the whole struct first (to catch all the settings) */
		rboN = MEM_dupallocN(ob->rigidbody_object);

		if ((flag & LIB_ID_CREATE_NO_MAIN) == 0) {
			/* This is a regular copy, and not a CoW copy for depsgraph evaluation */
			rboN->shared = MEM_callocN(sizeof(*rboN->shared), "RigidBodyOb_Shared");
		}

		/* tag object as needing to be verified */
		rboN->flag |= RBO_FLAG_NEEDS_VALIDATE;
	}

	/* return new copy of settings */
	return rboN;
}

RigidBodyCon *BKE_rigidbody_copy_constraint(const Object *ob, const int UNUSED(flag))
{
	RigidBodyCon *rbcN = NULL;

	if (ob->rigidbody_constraint) {
		/* just duplicate the whole struct first (to catch all the settings) */
		rbcN = MEM_dupallocN(ob->rigidbody_constraint);

		/* tag object as needing to be verified */
		rbcN->flag |= RBC_FLAG_NEEDS_VALIDATE;

		/* clear out all the fields which need to be revalidated later */
		rbcN->physics_constraint = NULL;
	}

	/* return new copy of settings */
	return rbcN;
}

/* ************************************** */
/* Setup Utilities - Validate Sim Instances */

/* get the appropriate evaluated mesh based on rigid body mesh source */
static Mesh *rigidbody_get_mesh(Object *ob)
{
	switch (ob->rigidbody_object->mesh_source) {
		case RBO_MESH_DEFORM:
			return ob->runtime.mesh_deform_eval;
		case RBO_MESH_FINAL:
			return ob->runtime.mesh_eval;
		case RBO_MESH_BASE:
			/* This mesh may be used for computing looptris, which should be done
			 * on the original; otherwise every time the CoW is recreated it will
			 * have to be recomputed. */
			BLI_assert(ob->rigidbody_object->mesh_source == RBO_MESH_BASE);
			return ob->runtime.mesh_orig;
	}

	/* Just return something sensible so that at least Blender won't crash. */
	BLI_assert(!"Unknown mesh source");
	return ob->runtime.mesh_eval;
}

/* create collision shape of mesh - convex hull */
static rbCollisionShape *rigidbody_get_shape_convexhull_from_mesh(Object *ob, float margin, bool *can_embed)
{
	rbCollisionShape *shape = NULL;
	Mesh *mesh = NULL;
	MVert *mvert = NULL;
	int totvert = 0;

	if (ob->type == OB_MESH && ob->data) {
		mesh = rigidbody_get_mesh(ob);
		mvert   = (mesh) ? mesh->mvert : NULL;
		totvert = (mesh) ? mesh->totvert : 0;
	}
	else {
		printf("ERROR: cannot make Convex Hull collision shape for non-Mesh object\n");
	}

	if (totvert) {
		shape = RB_shape_new_convex_hull((float *)mvert, sizeof(MVert), totvert, margin, can_embed);
	}
	else {
		printf("ERROR: no vertices to define Convex Hull collision shape with\n");
	}

	return shape;
}

/* create collision shape of mesh - triangulated mesh
 * returns NULL if creation fails.
 */
static rbCollisionShape *rigidbody_get_shape_trimesh_from_mesh(Object *ob)
{
	rbCollisionShape *shape = NULL;

	if (ob->type == OB_MESH) {
		Mesh *mesh = NULL;
		MVert *mvert;
		const MLoopTri *looptri;
		int totvert;
		int tottri;
		const MLoop *mloop;

		mesh = rigidbody_get_mesh(ob);

		/* ensure mesh validity, then grab data */
		if (mesh == NULL)
			return NULL;

		mvert   = mesh->mvert;
		totvert = mesh->totvert;
		looptri = BKE_mesh_runtime_looptri_ensure(mesh);
		tottri = mesh->runtime.looptris.len;
		mloop = mesh->mloop;

		/* sanity checking - potential case when no data will be present */
		if ((totvert == 0) || (tottri == 0)) {
			printf("WARNING: no geometry data converted for Mesh Collision Shape (ob = %s)\n", ob->id.name + 2);
		}
		else {
			rbMeshData *mdata;
			int i;

			/* init mesh data for collision shape */
			mdata = RB_trimesh_data_new(tottri, totvert);

			RB_trimesh_add_vertices(mdata, (float *)mvert, totvert, sizeof(MVert));

			/* loop over all faces, adding them as triangles to the collision shape
			 * (so for some faces, more than triangle will get added)
			 */
			if (mvert && looptri) {
				for (i = 0; i < tottri; i++) {
					/* add first triangle - verts 1,2,3 */
					const MLoopTri *lt = &looptri[i];
					int vtri[3];

					vtri[0] = mloop[lt->tri[0]].v;
					vtri[1] = mloop[lt->tri[1]].v;
					vtri[2] = mloop[lt->tri[2]].v;

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
		printf("ERROR: cannot make Triangular Mesh collision shape for non-Mesh object\n");
	}

	return shape;
}

/* Create new physics sim collision shape for object and store it,
 * or remove the existing one first and replace...
 */
static void rigidbody_validate_sim_shape(Object *ob, bool rebuild)
{
	RigidBodyOb *rbo = ob->rigidbody_object;
	rbCollisionShape *new_shape = NULL;
	BoundBox *bb = NULL;
	float size[3] = {1.0f, 1.0f, 1.0f};
	float radius = 1.0f;
	float height = 1.0f;
	float capsule_height;
	float hull_margin = 0.0f;
	bool can_embed = true;
	bool has_volume;

	/* sanity check */
	if (rbo == NULL)
		return;

	/* don't create a new shape if we already have one and don't want to rebuild it */
	if (rbo->shared->physics_shape && !rebuild)
		return;

	/* if automatically determining dimensions, use the Object's boundbox
	 *	- assume that all quadrics are standing upright on local z-axis
	 *	- assume even distribution of mass around the Object's pivot
	 *	  (i.e. Object pivot is centralized in boundbox)
	 */
	// XXX: all dimensions are auto-determined now... later can add stored settings for this
	/* get object dimensions without scaling */
	bb = BKE_object_boundbox_get(ob);
	if (bb) {
		size[0] = (bb->vec[4][0] - bb->vec[0][0]);
		size[1] = (bb->vec[2][1] - bb->vec[0][1]);
		size[2] = (bb->vec[1][2] - bb->vec[0][2]);
	}
	mul_v3_fl(size, 0.5f);

	if (ELEM(rbo->shape, RB_SHAPE_CAPSULE, RB_SHAPE_CYLINDER, RB_SHAPE_CONE)) {
		/* take radius as largest x/y dimension, and height as z-dimension */
		radius = MAX2(size[0], size[1]);
		height = size[2];
	}
	else if (rbo->shape == RB_SHAPE_SPHERE) {
		/* take radius to the largest dimension to try and encompass everything */
		radius = MAX3(size[0], size[1], size[2]);
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
			/* try to emged collision margin */
			has_volume = (MIN3(size[0], size[1], size[2]) > 0.0f);

			if (!(rbo->flag & RBO_FLAG_USE_MARGIN) && has_volume)
				hull_margin = 0.04f;
			new_shape = rigidbody_get_shape_convexhull_from_mesh(ob, hull_margin, &can_embed);
			if (!(rbo->flag & RBO_FLAG_USE_MARGIN))
				rbo->margin = (can_embed && has_volume) ? 0.04f : 0.0f;  /* RB_TODO ideally we shouldn't directly change the margin here */
			break;
		case RB_SHAPE_TRIMESH:
			new_shape = rigidbody_get_shape_trimesh_from_mesh(ob);
			break;
	}
	/* use box shape if we can't fall back to old shape */
	if (new_shape == NULL && rbo->shared->physics_shape == NULL) {
		new_shape = RB_shape_new_box(size[0], size[1], size[2]);
	}
	/* assign new collision shape if creation was successful */
	if (new_shape) {
		if (rbo->shared->physics_shape) {
			RB_shape_delete(rbo->shared->physics_shape);
		}
		rbo->shared->physics_shape = new_shape;
		RB_shape_set_margin(rbo->shared->physics_shape, RBO_GET_MARGIN(rbo));
	}
}

/* --------------------- */

/* helper function to calculate volume of rigidbody object */
// TODO: allow a parameter to specify method used to calculate this?
void BKE_rigidbody_calc_volume(Object *ob, float *r_vol)
{
	RigidBodyOb *rbo = ob->rigidbody_object;

	float size[3]  = {1.0f, 1.0f, 1.0f};
	float radius = 1.0f;
	float height = 1.0f;

	float volume = 0.0f;

	/* if automatically determining dimensions, use the Object's boundbox
	 *	- assume that all quadrics are standing upright on local z-axis
	 *	- assume even distribution of mass around the Object's pivot
	 *	  (i.e. Object pivot is centralized in boundbox)
	 *	- boundbox gives full width
	 */
	// XXX: all dimensions are auto-determined now... later can add stored settings for this
	BKE_object_dimensions_get(ob, size);

	if (ELEM(rbo->shape, RB_SHAPE_CAPSULE, RB_SHAPE_CYLINDER, RB_SHAPE_CONE)) {
		/* take radius as largest x/y dimension, and height as z-dimension */
		radius = MAX2(size[0], size[1]) * 0.5f;
		height = size[2];
	}
	else if (rbo->shape == RB_SHAPE_SPHERE) {
		/* take radius to the largest dimension to try and encompass everything */
		radius = max_fff(size[0], size[1], size[2]) * 0.5f;
	}

	/* calculate volume as appropriate  */
	switch (rbo->shape) {
		case RB_SHAPE_BOX:
			volume = size[0] * size[1] * size[2];
			break;

		case RB_SHAPE_SPHERE:
			volume = 4.0f / 3.0f * (float)M_PI * radius * radius * radius;
			break;

		/* for now, assume that capsule is close enough to a cylinder... */
		case RB_SHAPE_CAPSULE:
		case RB_SHAPE_CYLINDER:
			volume = (float)M_PI * radius * radius * height;
			break;

		case RB_SHAPE_CONE:
			volume = (float)M_PI / 3.0f * radius * radius * height;
			break;

		case RB_SHAPE_CONVEXH:
		case RB_SHAPE_TRIMESH:
		{
			if (ob->type == OB_MESH) {
				Mesh *mesh = rigidbody_get_mesh(ob);
				MVert *mvert;
				const MLoopTri *lt = NULL;
				int totvert, tottri = 0;
				const MLoop *mloop = NULL;

				/* ensure mesh validity, then grab data */
				if (mesh == NULL)
					return;

				mvert   = mesh->mvert;
				totvert = mesh->totvert;
				lt = BKE_mesh_runtime_looptri_ensure(mesh);
				tottri = mesh->runtime.looptris.len;
				mloop = mesh->mloop;

				if (totvert > 0 && tottri > 0) {
					BKE_mesh_calc_volume(mvert, totvert, lt, tottri, mloop, &volume, NULL);
				}
			}
			else {
				/* rough estimate from boundbox as fallback */
				/* XXX could implement other types of geometry here (curves, etc.) */
				volume = size[0] * size[1] * size[2];
			}
			break;
		}

#if 0 // XXX: not defined yet
		case RB_SHAPE_COMPOUND:
			volume = 0.0f;
			break;
#endif
	}

	/* return the volume calculated */
	if (r_vol) *r_vol = volume;
}

void BKE_rigidbody_calc_center_of_mass(Object *ob, float r_center[3])
{
	RigidBodyOb *rbo = ob->rigidbody_object;

	float size[3]  = {1.0f, 1.0f, 1.0f};
	float height = 1.0f;

	zero_v3(r_center);

	/* if automatically determining dimensions, use the Object's boundbox
	 *	- assume that all quadrics are standing upright on local z-axis
	 *	- assume even distribution of mass around the Object's pivot
	 *	  (i.e. Object pivot is centralized in boundbox)
	 *	- boundbox gives full width
	 */
	// XXX: all dimensions are auto-determined now... later can add stored settings for this
	BKE_object_dimensions_get(ob, size);

	/* calculate volume as appropriate  */
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
		case RB_SHAPE_TRIMESH:
		{
			if (ob->type == OB_MESH) {
				Mesh *mesh = rigidbody_get_mesh(ob);
				MVert *mvert;
				const MLoopTri *looptri;
				int totvert, tottri;
				const MLoop *mloop;

				/* ensure mesh validity, then grab data */
				if (mesh == NULL)
					return;

				mvert   = mesh->mvert;
				totvert = mesh->totvert;
				looptri = BKE_mesh_runtime_looptri_ensure(mesh);
				tottri = mesh->runtime.looptris.len;
				mloop = mesh->mloop;

				if (totvert > 0 && tottri > 0) {
					BKE_mesh_calc_volume(mvert, totvert, looptri, tottri, mloop, NULL, r_center);
				}
			}
			break;
		}

#if 0 // XXX: not defined yet
		case RB_SHAPE_COMPOUND:
			volume = 0.0f;
			break;
#endif
	}
}

/* --------------------- */

/**
 * Create physics sim representation of object given RigidBody settings
 *
 * \param rebuild Even if an instance already exists, replace it
 */
static void rigidbody_validate_sim_object(RigidBodyWorld *rbw, Object *ob, bool rebuild)
{
	RigidBodyOb *rbo = (ob) ? ob->rigidbody_object : NULL;
	float loc[3];
	float rot[4];

	/* sanity checks:
	 *	- object doesn't have RigidBody info already: then why is it here?
	 */
	if (rbo == NULL)
		return;

	/* make sure collision shape exists */
	/* FIXME we shouldn't always have to rebuild collision shapes when rebuilding objects, but it's needed for constraints to update correctly */
	if (rbo->shared->physics_shape == NULL || rebuild)
		rigidbody_validate_sim_shape(ob, true);

	if (rbo->shared->physics_object) {
		RB_dworld_remove_body(rbw->shared->physics_world, rbo->shared->physics_object);
	}
	if (!rbo->shared->physics_object || rebuild) {
		/* remove rigid body if it already exists before creating a new one */
		if (rbo->shared->physics_object) {
			RB_body_delete(rbo->shared->physics_object);
		}

		mat4_to_loc_quat(loc, rot, ob->obmat);

		rbo->shared->physics_object = RB_body_new(rbo->shared->physics_shape, loc, rot);

		RB_body_set_friction(rbo->shared->physics_object, rbo->friction);
		RB_body_set_restitution(rbo->shared->physics_object, rbo->restitution);

		RB_body_set_damping(rbo->shared->physics_object, rbo->lin_damping, rbo->ang_damping);
		RB_body_set_sleep_thresh(rbo->shared->physics_object, rbo->lin_sleep_thresh, rbo->ang_sleep_thresh);
		RB_body_set_activation_state(rbo->shared->physics_object, rbo->flag & RBO_FLAG_USE_DEACTIVATION);

		if (rbo->type == RBO_TYPE_PASSIVE || rbo->flag & RBO_FLAG_START_DEACTIVATED)
			RB_body_deactivate(rbo->shared->physics_object);


		RB_body_set_linear_factor(rbo->shared->physics_object,
		                          (ob->protectflag & OB_LOCK_LOCX) == 0,
		                          (ob->protectflag & OB_LOCK_LOCY) == 0,
		                          (ob->protectflag & OB_LOCK_LOCZ) == 0);
		RB_body_set_angular_factor(rbo->shared->physics_object,
		                           (ob->protectflag & OB_LOCK_ROTX) == 0,
		                           (ob->protectflag & OB_LOCK_ROTY) == 0,
		                           (ob->protectflag & OB_LOCK_ROTZ) == 0);

		RB_body_set_mass(rbo->shared->physics_object, RBO_GET_MASS(rbo));
		RB_body_set_kinematic_state(rbo->shared->physics_object, rbo->flag & RBO_FLAG_KINEMATIC || rbo->flag & RBO_FLAG_DISABLED);
	}

	if (rbw && rbw->shared->physics_world)
		RB_dworld_add_body(rbw->shared->physics_world, rbo->shared->physics_object, rbo->col_groups);
}

/* --------------------- */

static void rigidbody_constraint_init_spring(
        RigidBodyCon *rbc, void (*set_spring)(rbConstraint *, int, int),
        void (*set_stiffness)(rbConstraint *, int, float), void (*set_damping)(rbConstraint *, int, float))
{
	set_spring(rbc->physics_constraint, RB_LIMIT_LIN_X, rbc->flag & RBC_FLAG_USE_SPRING_X);
	set_stiffness(rbc->physics_constraint, RB_LIMIT_LIN_X, rbc->spring_stiffness_x);
	set_damping(rbc->physics_constraint, RB_LIMIT_LIN_X, rbc->spring_damping_x);

	set_spring(rbc->physics_constraint, RB_LIMIT_LIN_Y, rbc->flag & RBC_FLAG_USE_SPRING_Y);
	set_stiffness(rbc->physics_constraint, RB_LIMIT_LIN_Y, rbc->spring_stiffness_y);
	set_damping(rbc->physics_constraint, RB_LIMIT_LIN_Y, rbc->spring_damping_y);

	set_spring(rbc->physics_constraint, RB_LIMIT_LIN_Z, rbc->flag & RBC_FLAG_USE_SPRING_Z);
	set_stiffness(rbc->physics_constraint, RB_LIMIT_LIN_Z, rbc->spring_stiffness_z);
	set_damping(rbc->physics_constraint, RB_LIMIT_LIN_Z, rbc->spring_damping_z);

	set_spring(rbc->physics_constraint, RB_LIMIT_ANG_X, rbc->flag & RBC_FLAG_USE_SPRING_ANG_X);
	set_stiffness(rbc->physics_constraint, RB_LIMIT_ANG_X, rbc->spring_stiffness_ang_x);
	set_damping(rbc->physics_constraint, RB_LIMIT_ANG_X, rbc->spring_damping_ang_x);

	set_spring(rbc->physics_constraint, RB_LIMIT_ANG_Y, rbc->flag & RBC_FLAG_USE_SPRING_ANG_Y);
	set_stiffness(rbc->physics_constraint, RB_LIMIT_ANG_Y, rbc->spring_stiffness_ang_y);
	set_damping(rbc->physics_constraint, RB_LIMIT_ANG_Y, rbc->spring_damping_ang_y);

	set_spring(rbc->physics_constraint, RB_LIMIT_ANG_Z, rbc->flag & RBC_FLAG_USE_SPRING_ANG_Z);
	set_stiffness(rbc->physics_constraint, RB_LIMIT_ANG_Z, rbc->spring_stiffness_ang_z);
	set_damping(rbc->physics_constraint, RB_LIMIT_ANG_Z, rbc->spring_damping_ang_z);
}

static void rigidbody_constraint_set_limits(
        RigidBodyCon *rbc, void (*set_limits)(rbConstraint *, int, float, float))
{
	if (rbc->flag & RBC_FLAG_USE_LIMIT_LIN_X)
		set_limits(rbc->physics_constraint, RB_LIMIT_LIN_X, rbc->limit_lin_x_lower, rbc->limit_lin_x_upper);
	else
		set_limits(rbc->physics_constraint, RB_LIMIT_LIN_X, 0.0f, -1.0f);

	if (rbc->flag & RBC_FLAG_USE_LIMIT_LIN_Y)
		set_limits(rbc->physics_constraint, RB_LIMIT_LIN_Y, rbc->limit_lin_y_lower, rbc->limit_lin_y_upper);
	else
		set_limits(rbc->physics_constraint, RB_LIMIT_LIN_Y, 0.0f, -1.0f);

	if (rbc->flag & RBC_FLAG_USE_LIMIT_LIN_Z)
		set_limits(rbc->physics_constraint, RB_LIMIT_LIN_Z, rbc->limit_lin_z_lower, rbc->limit_lin_z_upper);
	else
		set_limits(rbc->physics_constraint, RB_LIMIT_LIN_Z, 0.0f, -1.0f);

	if (rbc->flag & RBC_FLAG_USE_LIMIT_ANG_X)
		set_limits(rbc->physics_constraint, RB_LIMIT_ANG_X, rbc->limit_ang_x_lower, rbc->limit_ang_x_upper);
	else
		set_limits(rbc->physics_constraint, RB_LIMIT_ANG_X, 0.0f, -1.0f);

	if (rbc->flag & RBC_FLAG_USE_LIMIT_ANG_Y)
		set_limits(rbc->physics_constraint, RB_LIMIT_ANG_Y, rbc->limit_ang_y_lower, rbc->limit_ang_y_upper);
	else
		set_limits(rbc->physics_constraint, RB_LIMIT_ANG_Y, 0.0f, -1.0f);

	if (rbc->flag & RBC_FLAG_USE_LIMIT_ANG_Z)
		set_limits(rbc->physics_constraint, RB_LIMIT_ANG_Z, rbc->limit_ang_z_lower, rbc->limit_ang_z_upper);
	else
		set_limits(rbc->physics_constraint, RB_LIMIT_ANG_Z, 0.0f, -1.0f);
}

/**
 * Create physics sim representation of constraint given rigid body constraint settings
 *
 * \param rebuild Even if an instance already exists, replace it
 */
static void rigidbody_validate_sim_constraint(RigidBodyWorld *rbw, Object *ob, bool rebuild)
{
	RigidBodyCon *rbc = (ob) ? ob->rigidbody_constraint : NULL;
	float loc[3];
	float rot[4];
	float lin_lower;
	float lin_upper;
	float ang_lower;
	float ang_upper;

	/* sanity checks:
	 *	- object should have a rigid body constraint
	 *  - rigid body constraint should have at least one constrained object
	 */
	if (rbc == NULL) {
		return;
	}

	if (ELEM(NULL, rbc->ob1, rbc->ob1->rigidbody_object, rbc->ob2, rbc->ob2->rigidbody_object)) {
		if (rbc->physics_constraint) {
			RB_dworld_remove_constraint(rbw->shared->physics_world, rbc->physics_constraint);
			RB_constraint_delete(rbc->physics_constraint);
			rbc->physics_constraint = NULL;
		}
		return;
	}

	if (rbc->physics_constraint && rebuild == false) {
		RB_dworld_remove_constraint(rbw->shared->physics_world, rbc->physics_constraint);
	}
	if (rbc->physics_constraint == NULL || rebuild) {
		rbRigidBody *rb1 = rbc->ob1->rigidbody_object->shared->physics_object;
		rbRigidBody *rb2 = rbc->ob2->rigidbody_object->shared->physics_object;

		/* remove constraint if it already exists before creating a new one */
		if (rbc->physics_constraint) {
			RB_constraint_delete(rbc->physics_constraint);
			rbc->physics_constraint = NULL;
		}

		mat4_to_loc_quat(loc, rot, ob->obmat);

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
						RB_constraint_set_limits_hinge(rbc->physics_constraint, rbc->limit_ang_z_lower, rbc->limit_ang_z_upper);
					}
					else
						RB_constraint_set_limits_hinge(rbc->physics_constraint, 0.0f, -1.0f);
					break;
				case RBC_TYPE_SLIDER:
					rbc->physics_constraint = RB_constraint_new_slider(loc, rot, rb1, rb2);
					if (rbc->flag & RBC_FLAG_USE_LIMIT_LIN_X)
						RB_constraint_set_limits_slider(rbc->physics_constraint, rbc->limit_lin_x_lower, rbc->limit_lin_x_upper);
					else
						RB_constraint_set_limits_slider(rbc->physics_constraint, 0.0f, -1.0f);
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
					RB_constraint_set_limits_piston(rbc->physics_constraint, lin_lower, lin_upper, ang_lower, ang_upper);
					break;
				case RBC_TYPE_6DOF_SPRING:
					if (rbc->spring_type == RBC_SPRING_TYPE2) {
						rbc->physics_constraint = RB_constraint_new_6dof_spring2(loc, rot, rb1, rb2);

						rigidbody_constraint_init_spring(rbc, RB_constraint_set_spring_6dof_spring2, RB_constraint_set_stiffness_6dof_spring2, RB_constraint_set_damping_6dof_spring2);

						RB_constraint_set_equilibrium_6dof_spring2(rbc->physics_constraint);

						rigidbody_constraint_set_limits(rbc, RB_constraint_set_limits_6dof_spring2);
					}
					else {
						rbc->physics_constraint = RB_constraint_new_6dof_spring(loc, rot, rb1, rb2);

						rigidbody_constraint_init_spring(rbc, RB_constraint_set_spring_6dof_spring, RB_constraint_set_stiffness_6dof_spring, RB_constraint_set_damping_6dof_spring);

						RB_constraint_set_equilibrium_6dof_spring(rbc->physics_constraint);

						rigidbody_constraint_set_limits(rbc, RB_constraint_set_limits_6dof);
					}
					break;
				case RBC_TYPE_6DOF:
					rbc->physics_constraint = RB_constraint_new_6dof(loc, rot, rb1, rb2);

					rigidbody_constraint_set_limits(rbc, RB_constraint_set_limits_6dof);
					break;
				case RBC_TYPE_MOTOR:
					rbc->physics_constraint = RB_constraint_new_motor(loc, rot, rb1, rb2);

					RB_constraint_set_enable_motor(rbc->physics_constraint, rbc->flag & RBC_FLAG_USE_MOTOR_LIN, rbc->flag & RBC_FLAG_USE_MOTOR_ANG);
					RB_constraint_set_max_impulse_motor(rbc->physics_constraint, rbc->motor_lin_max_impulse, rbc->motor_ang_max_impulse);
					RB_constraint_set_target_velocity_motor(rbc->physics_constraint, rbc->motor_lin_target_velocity, rbc->motor_ang_target_velocity);
					break;
			}
		}
		else { /* can't create constraint without both rigid bodies */
			return;
		}

		RB_constraint_set_enabled(rbc->physics_constraint, rbc->flag & RBC_FLAG_ENABLED);

		if (rbc->flag & RBC_FLAG_USE_BREAKING)
			RB_constraint_set_breaking_threshold(rbc->physics_constraint, rbc->breaking_threshold);
		else
			RB_constraint_set_breaking_threshold(rbc->physics_constraint, FLT_MAX);

		if (rbc->flag & RBC_FLAG_OVERRIDE_SOLVER_ITERATIONS)
			RB_constraint_set_solver_iterations(rbc->physics_constraint, rbc->num_solver_iterations);
		else
			RB_constraint_set_solver_iterations(rbc->physics_constraint, -1);
	}

	if (rbw && rbw->shared->physics_world && rbc->physics_constraint) {
		RB_dworld_add_constraint(rbw->shared->physics_world, rbc->physics_constraint, rbc->flag & RBC_FLAG_DISABLE_COLLISIONS);
	}
}

/* --------------------- */

/* Create physics sim world given RigidBody world settings */
// NOTE: this does NOT update object references that the scene uses, in case those aren't ready yet!
void BKE_rigidbody_validate_sim_world(Scene *scene, RigidBodyWorld *rbw, bool rebuild)
{
	/* sanity checks */
	if (rbw == NULL)
		return;

	/* create new sim world */
	if (rebuild || rbw->shared->physics_world == NULL) {
		if (rbw->shared->physics_world)
			RB_dworld_delete(rbw->shared->physics_world);
		rbw->shared->physics_world = RB_dworld_new(scene->physics_settings.gravity);
	}

	RB_dworld_set_solver_iterations(rbw->shared->physics_world, rbw->num_solver_iterations);
	RB_dworld_set_split_impulse(rbw->shared->physics_world, rbw->flag & RBW_FLAG_USE_SPLIT_IMPULSE);
}

/* ************************************** */
/* Setup Utilities - Create Settings Blocks */

/* Set up RigidBody world */
RigidBodyWorld *BKE_rigidbody_create_world(Scene *scene)
{
	/* try to get whatever RigidBody world that might be representing this already */
	RigidBodyWorld *rbw;

	/* sanity checks
	 *	- there must be a valid scene to add world to
	 *	- there mustn't be a sim world using this group already
	 */
	if (scene == NULL)
		return NULL;

	/* create a new sim world */
	rbw = MEM_callocN(sizeof(RigidBodyWorld), "RigidBodyWorld");
	rbw->shared = MEM_callocN(sizeof(*rbw->shared), "RigidBodyWorld_Shared");

	/* set default settings */
	rbw->effector_weights = BKE_add_effector_weights(NULL);

	rbw->ltime = PSFRA;

	rbw->time_scale = 1.0f;

	rbw->steps_per_second = 60; /* Bullet default (60 Hz) */
	rbw->num_solver_iterations = 10; /* 10 is bullet default */

	rbw->shared->pointcache = BKE_ptcache_add(&(rbw->shared->ptcaches));
	rbw->shared->pointcache->step = 1;

	/* return this sim world */
	return rbw;
}

RigidBodyWorld *BKE_rigidbody_world_copy(RigidBodyWorld *rbw, const int flag)
{
	RigidBodyWorld *rbw_copy = MEM_dupallocN(rbw);

	if (rbw->effector_weights) {
		rbw_copy->effector_weights = MEM_dupallocN(rbw->effector_weights);
	}
	if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
		id_us_plus((ID *)rbw_copy->group);
		id_us_plus((ID *)rbw_copy->constraints);
	}

	if ((flag & LIB_ID_CREATE_NO_MAIN) == 0) {
		/* This is a regular copy, and not a CoW copy for depsgraph evaluation */
		rbw_copy->shared = MEM_callocN(sizeof(*rbw_copy->shared), "RigidBodyWorld_Shared");
		BKE_ptcache_copy_list(&rbw_copy->shared->ptcaches, &rbw->shared->ptcaches, LIB_ID_COPY_CACHES);
		rbw_copy->shared->pointcache = rbw_copy->shared->ptcaches.first;
	}

	rbw_copy->objects = NULL;
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

void BKE_rigidbody_world_id_loop(RigidBodyWorld *rbw, RigidbodyWorldIDFunc func, void *userdata)
{
	func(rbw, (ID **)&rbw->group, userdata, IDWALK_CB_NOP);
	func(rbw, (ID **)&rbw->constraints, userdata, IDWALK_CB_NOP);
	func(rbw, (ID **)&rbw->effector_weights->group, userdata, IDWALK_CB_NOP);

	if (rbw->objects) {
		int i;
		for (i = 0; i < rbw->numbodies; i++) {
			func(rbw, (ID **)&rbw->objects[i], userdata, IDWALK_CB_NOP);
		}
	}
}

/* Add rigid body settings to the specified object */
RigidBodyOb *BKE_rigidbody_create_object(Scene *scene, Object *ob, short type)
{
	RigidBodyOb *rbo;
	RigidBodyWorld *rbw = scene->rigidbody_world;

	/* sanity checks
	 *	- rigidbody world must exist
	 *	- object must exist
	 *	- cannot add rigid body if it already exists
	 */
	if (ob == NULL || (ob->rigidbody_object != NULL))
		return NULL;

	/* create new settings data, and link it up */
	rbo = MEM_callocN(sizeof(RigidBodyOb), "RigidBodyOb");
	rbo->shared = MEM_callocN(sizeof(*rbo->shared), "RigidBodyOb_Shared");

	/* set default settings */
	rbo->type = type;

	rbo->mass = 1.0f;

	rbo->friction = 0.5f; /* best when non-zero. 0.5 is Bullet default */
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
	if (type == RBO_TYPE_ACTIVE)
		rbo->shape = RB_SHAPE_CONVEXH;
	else
		rbo->shape = RB_SHAPE_TRIMESH;

	rbo->mesh_source = RBO_MESH_DEFORM;

	/* set initial transform */
	mat4_to_loc_quat(rbo->pos, rbo->orn, ob->obmat);

	/* flag cache as outdated */
	BKE_rigidbody_cache_reset(rbw);

	/* return this object */
	return rbo;
}

/* Add rigid body constraint to the specified object */
RigidBodyCon *BKE_rigidbody_create_constraint(Scene *scene, Object *ob, short type)
{
	RigidBodyCon *rbc;
	RigidBodyWorld *rbw = scene->rigidbody_world;

	/* sanity checks
	 *	- rigidbody world must exist
	 *	- object must exist
	 *	- cannot add constraint if it already exists
	 */
	if (ob == NULL || (ob->rigidbody_constraint != NULL))
		return NULL;

	/* create new settings data, and link it up */
	rbc = MEM_callocN(sizeof(RigidBodyCon), "RigidBodyCon");

	/* set default settings */
	rbc->type = type;

	rbc->ob1 = NULL;
	rbc->ob2 = NULL;

	rbc->flag |= RBC_FLAG_ENABLED;
	rbc->flag |= RBC_FLAG_DISABLE_COLLISIONS;

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

/* ************************************** */
/* Utilities API */

/* Get RigidBody world for the given scene, creating one if needed
 *
 * \param scene Scene to find active Rigid Body world for
 */
RigidBodyWorld *BKE_rigidbody_get_world(Scene *scene)
{
	/* sanity check */
	if (scene == NULL)
		return NULL;

	return scene->rigidbody_world;
}

void BKE_rigidbody_remove_object(struct Main *bmain, Scene *scene, Object *ob)
{
	RigidBodyWorld *rbw = scene->rigidbody_world;
	RigidBodyCon *rbc;
	int i;

	if (rbw) {

		/* remove object from array */
		if (rbw && rbw->objects) {
			for (i = 0; i < rbw->numbodies; i++) {
				if (rbw->objects[i] == ob) {
					rbw->objects[i] = NULL;
					break;
				}
			}
		}

		/* remove object from rigid body constraints */
		if (rbw->constraints) {
			FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN(rbw->constraints, obt)
			{
				if (obt && obt->rigidbody_constraint) {
					rbc = obt->rigidbody_constraint;
					if (ELEM(ob, rbc->ob1, rbc->ob2)) {
						BKE_rigidbody_remove_constraint(scene, obt);
					}
				}
			}
			FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
		}
		BKE_collection_object_remove(bmain, rbw->group, ob, false);
	}

	/* remove object's settings */
	BKE_rigidbody_free_object(ob, rbw);

	/* flag cache as outdated */
	BKE_rigidbody_cache_reset(rbw);
}

void BKE_rigidbody_remove_constraint(Scene *scene, Object *ob)
{
	RigidBodyWorld *rbw = scene->rigidbody_world;
	RigidBodyCon *rbc = ob->rigidbody_constraint;

	/* remove from rigidbody world, free object won't do this */
	if (rbw && rbw->shared->physics_world && rbc->physics_constraint) {
		RB_dworld_remove_constraint(rbw->shared->physics_world, rbc->physics_constraint);
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
	if (rbw->group == NULL) {
		rbw->numbodies = 0;
		rbw->objects = realloc(rbw->objects, 0);
		return;
	}

	int n = 0;
	FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN(rbw->group, object)
	{
		(void)object;
		n++;
	}
	FOREACH_COLLECTION_OBJECT_RECURSIVE_END;

	if (rbw->numbodies != n) {
		rbw->numbodies = n;
		rbw->objects = realloc(rbw->objects, sizeof(Object *) * rbw->numbodies);
	}

	int i = 0;
	FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN(rbw->group, object)
	{
		rbw->objects[i] = object;
		i++;
	}
	FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
}

static void rigidbody_update_sim_world(Scene *scene, RigidBodyWorld *rbw)
{
	float adj_gravity[3];

	/* adjust gravity to take effector weights into account */
	if (scene->physics_settings.flag & PHYS_GLOBAL_GRAVITY) {
		copy_v3_v3(adj_gravity, scene->physics_settings.gravity);
		mul_v3_fl(adj_gravity, rbw->effector_weights->global_gravity * rbw->effector_weights->weight[0]);
	}
	else {
		zero_v3(adj_gravity);
	}

	/* update gravity, since this RNA setting is not part of RigidBody settings */
	RB_dworld_set_gravity(rbw->shared->physics_world, adj_gravity);

	/* update object array in case there are changes */
	rigidbody_update_ob_array(rbw);
}

static void rigidbody_update_sim_ob(Depsgraph *depsgraph, Scene *scene, RigidBodyWorld *rbw, Object *ob, RigidBodyOb *rbo)
{
	float loc[3];
	float rot[4];
	float scale[3];

	/* only update if rigid body exists */
	if (rbo->shared->physics_object == NULL)
		return;

	if (rbo->shape == RB_SHAPE_TRIMESH && rbo->flag & RBO_FLAG_USE_DEFORM) {
		Mesh *mesh = ob->runtime.mesh_deform_eval;
		if (mesh) {
			MVert *mvert = mesh->mvert;
			int totvert = mesh->totvert;
			BoundBox *bb = BKE_object_boundbox_get(ob);

			RB_shape_trimesh_update(rbo->shared->physics_shape, (float *)mvert, totvert, sizeof(MVert), bb->vec[0], bb->vec[6]);
		}
	}

	mat4_decompose(loc, rot, scale, ob->obmat);

	/* update scale for all objects */
	RB_body_set_scale(rbo->shared->physics_object, scale);
	/* compensate for embedded convex hull collision margin */
	if (!(rbo->flag & RBO_FLAG_USE_MARGIN) && rbo->shape == RB_SHAPE_CONVEXH)
		RB_shape_set_margin(rbo->shared->physics_shape, RBO_GET_MARGIN(rbo) * MIN3(scale[0], scale[1], scale[2]));

	/* make transformed objects temporarily kinmatic so that they can be moved by the user during simulation */
	if (ob->flag & SELECT && G.moving & G_TRANSFORM_OBJ) {
		RB_body_set_kinematic_state(rbo->shared->physics_object, true);
		RB_body_set_mass(rbo->shared->physics_object, 0.0f);
	}

	/* update rigid body location and rotation for kinematic bodies */
	if (rbo->flag & RBO_FLAG_KINEMATIC || (ob->flag & SELECT && G.moving & G_TRANSFORM_OBJ)) {
		RB_body_activate(rbo->shared->physics_object);
		RB_body_set_loc_rot(rbo->shared->physics_object, loc, rot);
	}
	/* update influence of effectors - but don't do it on an effector */
	/* only dynamic bodies need effector update */
	else if (rbo->type == RBO_TYPE_ACTIVE && ((ob->pd == NULL) || (ob->pd->forcefield == PFIELD_NULL))) {
		EffectorWeights *effector_weights = rbw->effector_weights;
		EffectedPoint epoint;
		ListBase *effectors;

		/* get effectors present in the group specified by effector_weights */
		effectors = BKE_effectors_create(depsgraph, ob, NULL, effector_weights);
		if (effectors) {
			float eff_force[3] = {0.0f, 0.0f, 0.0f};
			float eff_loc[3], eff_vel[3];

			/* create dummy 'point' which represents last known position of object as result of sim */
			// XXX: this can create some inaccuracies with sim position, but is probably better than using unsimulated vals?
			RB_body_get_position(rbo->shared->physics_object, eff_loc);
			RB_body_get_linear_velocity(rbo->shared->physics_object, eff_vel);

			pd_point_from_loc(scene, eff_loc, eff_vel, 0, &epoint);

			/* calculate net force of effectors, and apply to sim object
			 *	- we use 'central force' since apply force requires a "relative position" which we don't have...
			 */
			BKE_effectors_apply(effectors, NULL, effector_weights, &epoint, eff_force, NULL);
			if (G.f & G_DEBUG)
				printf("\tapplying force (%f,%f,%f) to '%s'\n", eff_force[0], eff_force[1], eff_force[2], ob->id.name + 2);
			/* activate object in case it is deactivated */
			if (!is_zero_v3(eff_force))
				RB_body_activate(rbo->shared->physics_object);
			RB_body_apply_central_force(rbo->shared->physics_object, eff_force);
		}
		else if (G.f & G_DEBUG)
			printf("\tno forces to apply to '%s'\n", ob->id.name + 2);

		/* cleanup */
		BKE_effectors_free(effectors);
	}
	/* NOTE: passive objects don't need to be updated since they don't move */

	/* NOTE: no other settings need to be explicitly updated here,
	 * since RNA setters take care of the rest :)
	 */
}

/**
 * Updates and validates world, bodies and shapes.
 *
 * \param rebuild Rebuild entire simulation
 */
static void rigidbody_update_simulation(Depsgraph *depsgraph, Scene *scene, RigidBodyWorld *rbw, bool rebuild)
{
	/* update world */
	if (rebuild)
		BKE_rigidbody_validate_sim_world(scene, rbw, true);
	rigidbody_update_sim_world(scene, rbw);

	/* XXX TODO For rebuild: remove all constraints first.
	 * Otherwise we can end up deleting objects that are still
	 * referenced by constraints, corrupting bullet's internal list.
	 *
	 * Memory management needs redesign here, this is just a dirty workaround.
	 */
	if (rebuild && rbw->constraints) {
		FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN(rbw->constraints, ob)
		{
			RigidBodyCon *rbc = ob->rigidbody_constraint;
			if (rbc && rbc->physics_constraint) {
				RB_dworld_remove_constraint(rbw->shared->physics_world, rbc->physics_constraint);
				RB_constraint_delete(rbc->physics_constraint);
				rbc->physics_constraint = NULL;
			}
		}
		FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
	}

	/* update objects */
	FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN(rbw->group, ob)
	{
		if (ob->type == OB_MESH) {
			/* validate that we've got valid object set up here... */
			RigidBodyOb *rbo = ob->rigidbody_object;
			/* update transformation matrix of the object so we don't get a frame of lag for simple animations */
			BKE_object_where_is_calc(depsgraph, scene, ob);

			if (rbo == NULL) {
				/* Since this object is included in the sim group but doesn't have
				 * rigid body settings (perhaps it was added manually), add!
				 *	- assume object to be active? That is the default for newly added settings...
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
					 * but neither resets the RBO_FLAG_NEEDS_RESHAPE flag nor calls RB_body_set_collision_shape().
					 * This results in the collision shape being created twice, which is unnecessary. */
					rigidbody_validate_sim_object(rbw, ob, true);
				}
				else if (rbo->flag & RBO_FLAG_NEEDS_VALIDATE) {
					rigidbody_validate_sim_object(rbw, ob, false);
				}
				/* refresh shape... */
				if (rbo->flag & RBO_FLAG_NEEDS_RESHAPE) {
					/* mesh/shape data changed, so force shape refresh */
					rigidbody_validate_sim_shape(ob, true);
					/* now tell RB sim about it */
					// XXX: we assume that this can only get applied for active/passive shapes that will be included as rigidbodies
					RB_body_set_collision_shape(rbo->shared->physics_object, rbo->shared->physics_shape);
				}
				rbo->flag &= ~(RBO_FLAG_NEEDS_VALIDATE | RBO_FLAG_NEEDS_RESHAPE);
			}

			/* update simulation object... */
			rigidbody_update_sim_ob(depsgraph, scene, rbw, ob, rbo);
		}
	}
	FOREACH_COLLECTION_OBJECT_RECURSIVE_END;

	/* update constraints */
	if (rbw->constraints == NULL) /* no constraints, move on */
		return;

	FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN(rbw->constraints, ob)
	{
		/* validate that we've got valid object set up here... */
		RigidBodyCon *rbc = ob->rigidbody_constraint;
		/* update transformation matrix of the object so we don't get a frame of lag for simple animations */
		BKE_object_where_is_calc(depsgraph, scene, ob);

		if (rbc == NULL) {
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
			rbc->flag &= ~RBC_FLAG_NEEDS_VALIDATE;
		}
	}
	FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
}

static void rigidbody_update_simulation_post_step(Depsgraph *depsgraph, RigidBodyWorld *rbw)
{
	ViewLayer *view_layer = DEG_get_input_view_layer(depsgraph);

	FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN(rbw->group, ob)
	{
		Base *base = BKE_view_layer_base_find(view_layer, ob);
		RigidBodyOb *rbo = ob->rigidbody_object;
		/* Reset kinematic state for transformed objects. */
		if (rbo && base && (base->flag & BASE_SELECTED) && (G.moving & G_TRANSFORM_OBJ)) {
			RB_body_set_kinematic_state(rbo->shared->physics_object, rbo->flag & RBO_FLAG_KINEMATIC || rbo->flag & RBO_FLAG_DISABLED);
			RB_body_set_mass(rbo->shared->physics_object, RBO_GET_MASS(rbo));
			/* Deactivate passive objects so they don't interfere with deactivation of active objects. */
			if (rbo->type == RBO_TYPE_PASSIVE)
				RB_body_deactivate(rbo->shared->physics_object);
		}
	}
	FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
}

bool BKE_rigidbody_check_sim_running(RigidBodyWorld *rbw, float ctime)
{
	return (rbw && (rbw->flag & RBW_FLAG_MUTED) == 0 && ctime > rbw->shared->pointcache->startframe);
}

/* Sync rigid body and object transformations */
void BKE_rigidbody_sync_transforms(RigidBodyWorld *rbw, Object *ob, float ctime)
{
	RigidBodyOb *rbo = ob->rigidbody_object;

	/* keep original transform for kinematic and passive objects */
	if (ELEM(NULL, rbw, rbo) || rbo->flag & RBO_FLAG_KINEMATIC || rbo->type == RBO_TYPE_PASSIVE)
		return;

	/* use rigid body transform after cache start frame if objects is not being transformed */
	if (BKE_rigidbody_check_sim_running(rbw, ctime) && !(ob->flag & SELECT && G.moving & G_TRANSFORM_OBJ)) {
		float mat[4][4], size_mat[4][4], size[3];

		normalize_qt(rbo->orn); // RB_TODO investigate why quaternion isn't normalized at this point
		quat_to_mat4(mat, rbo->orn);
		copy_v3_v3(mat[3], rbo->pos);

		mat4_to_size(size, ob->obmat);
		size_to_mat4(size_mat, size);
		mul_m4_m4m4(mat, mat, size_mat);

		copy_m4_m4(ob->obmat, mat);
	}
	/* otherwise set rigid body transform to current obmat */
	else {
		mat4_to_loc_quat(rbo->pos, rbo->orn, ob->obmat);
	}
}

/* Used when canceling transforms - return rigidbody and object to initial states */
void BKE_rigidbody_aftertrans_update(Object *ob, float loc[3], float rot[3], float quat[4], float rotAxis[3], float rotAngle)
{
	RigidBodyOb *rbo = ob->rigidbody_object;
	bool correct_delta = !(rbo->flag & RBO_FLAG_KINEMATIC || rbo->type == RBO_TYPE_PASSIVE);

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
		if (rbo->type == RBO_TYPE_PASSIVE)
			RB_body_set_kinematic_state(rbo->shared->physics_object, true);
		RB_body_set_loc_rot(rbo->shared->physics_object, rbo->pos, rbo->orn);
	}
	// RB_TODO update rigid body physics object's loc/rot for dynamic objects here as well (needs to be done outside bullet's update loop)
}

void BKE_rigidbody_cache_reset(RigidBodyWorld *rbw)
{
	if (rbw) {
		rbw->shared->pointcache->flag |= PTCACHE_OUTDATED;
	}
}

/* ------------------ */

/* Rebuild rigid body world */
/* NOTE: this needs to be called before frame update to work correctly */
void BKE_rigidbody_rebuild_world(Depsgraph *depsgraph, Scene *scene, float ctime)
{
	RigidBodyWorld *rbw = scene->rigidbody_world;
	PointCache *cache;
	PTCacheID pid;
	int startframe, endframe;

	BKE_ptcache_id_from_rigidbody(&pid, NULL, rbw);
	BKE_ptcache_id_time(&pid, scene, ctime, &startframe, &endframe, NULL);
	cache = rbw->shared->pointcache;

	/* flag cache as outdated if we don't have a world or number of objects in the simulation has changed */
	int n = 0;
	FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN(rbw->group, object)
	{
		(void)object;
		n++;
	}
	FOREACH_COLLECTION_OBJECT_RECURSIVE_END;

	if (rbw->shared->physics_world == NULL || rbw->numbodies != n) {
		cache->flag |= PTCACHE_OUTDATED;
	}

	if (ctime == startframe + 1 && rbw->ltime == startframe) {
		if (cache->flag & PTCACHE_OUTDATED) {
			BKE_ptcache_id_reset(scene, &pid, PTCACHE_RESET_OUTDATED);
			rigidbody_update_simulation(depsgraph, scene, rbw, true);
			BKE_ptcache_validate(cache, (int)ctime);
			cache->last_exact = 0;
			cache->flag &= ~PTCACHE_REDO_NEEDED;
		}
	}
}

/* Run RigidBody simulation for the specified physics world */
void BKE_rigidbody_do_simulation(Depsgraph *depsgraph, Scene *scene, float ctime)
{
	float timestep;
	RigidBodyWorld *rbw = scene->rigidbody_world;
	PointCache *cache;
	PTCacheID pid;
	int startframe, endframe;

	BKE_ptcache_id_from_rigidbody(&pid, NULL, rbw);
	BKE_ptcache_id_time(&pid, scene, ctime, &startframe, &endframe, NULL);
	cache = rbw->shared->pointcache;

	if (ctime <= startframe) {
		rbw->ltime = startframe;
		return;
	}
	/* make sure we don't go out of cache frame range */
	else if (ctime > endframe) {
		ctime = endframe;
	}

	/* don't try to run the simulation if we don't have a world yet but allow reading baked cache */
	if (rbw->shared->physics_world == NULL && !(cache->flag & PTCACHE_BAKED))
		return;
	else if (rbw->objects == NULL)
		rigidbody_update_ob_array(rbw);

	/* try to read from cache */
	// RB_TODO deal with interpolated, old and baked results
	bool can_simulate = (ctime == rbw->ltime + 1) && !(cache->flag & PTCACHE_BAKED);

	if (BKE_ptcache_read(&pid, ctime, can_simulate) == PTCACHE_READ_EXACT) {
		BKE_ptcache_validate(cache, (int)ctime);
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

		/* update and validate simulation */
		rigidbody_update_simulation(depsgraph, scene, rbw, false);

		/* calculate how much time elapsed since last step in seconds */
		timestep = 1.0f / (float)FPS * (ctime - rbw->ltime) * rbw->time_scale;
		/* step simulation by the requested timestep, steps per second are adjusted to take time scale into account */
		RB_dworld_step_simulation(rbw->shared->physics_world, timestep, INT_MAX, 1.0f / (float)rbw->steps_per_second * min_ff(rbw->time_scale, 1.0f));

		rigidbody_update_simulation_post_step(depsgraph, rbw);

		/* write cache for current frame */
		BKE_ptcache_validate(cache, (int)ctime);
		BKE_ptcache_write(&pid, (unsigned int)ctime);

		rbw->ltime = ctime;
	}
}
/* ************************************** */

#else  /* WITH_BULLET */

/* stubs */
#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

struct RigidBodyOb *BKE_rigidbody_copy_object(const Object *ob, const int flag) { return NULL; }
struct RigidBodyCon *BKE_rigidbody_copy_constraint(const Object *ob, const int flag) { return NULL; }
void BKE_rigidbody_validate_sim_world(Scene *scene, RigidBodyWorld *rbw, bool rebuild) {}
void BKE_rigidbody_calc_volume(Object *ob, float *r_vol) { if (r_vol) *r_vol = 0.0f; }
void BKE_rigidbody_calc_center_of_mass(Object *ob, float r_center[3]) { zero_v3(r_center); }
struct RigidBodyWorld *BKE_rigidbody_create_world(Scene *scene) { return NULL; }
struct RigidBodyWorld *BKE_rigidbody_world_copy(RigidBodyWorld *rbw, const int flag) { return NULL; }
void BKE_rigidbody_world_groups_relink(struct RigidBodyWorld *rbw) {}
void BKE_rigidbody_world_id_loop(struct RigidBodyWorld *rbw, RigidbodyWorldIDFunc func, void *userdata) {}
struct RigidBodyOb *BKE_rigidbody_create_object(Scene *scene, Object *ob, short type) { return NULL; }
struct RigidBodyCon *BKE_rigidbody_create_constraint(Scene *scene, Object *ob, short type) { return NULL; }
struct RigidBodyWorld *BKE_rigidbody_get_world(Scene *scene) { return NULL; }
void BKE_rigidbody_remove_object(struct Main *bmain, Scene *scene, Object *ob) {}
void BKE_rigidbody_remove_constraint(Scene *scene, Object *ob) {}
void BKE_rigidbody_sync_transforms(RigidBodyWorld *rbw, Object *ob, float ctime) {}
void BKE_rigidbody_aftertrans_update(Object *ob, float loc[3], float rot[3], float quat[4], float rotAxis[3], float rotAngle) {}
bool BKE_rigidbody_check_sim_running(RigidBodyWorld *rbw, float ctime) { return false; }
void BKE_rigidbody_cache_reset(RigidBodyWorld *rbw) {}
void BKE_rigidbody_rebuild_world(Depsgraph *depsgraph, Scene *scene, float ctime) {}
void BKE_rigidbody_do_simulation(Depsgraph *depsgraph, Scene *scene, float ctime) {}

#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic pop
#endif

#endif  /* WITH_BULLET */



/* -------------------- */
/* Depsgraph evaluation */

void BKE_rigidbody_rebuild_sim(Depsgraph *depsgraph,
                               Scene *scene)
{
	float ctime = DEG_get_ctime(depsgraph);
	DEG_debug_print_eval_time(depsgraph, __func__, scene->id.name, scene, ctime);
	/* rebuild sim data (i.e. after resetting to start of timeline) */
	if (BKE_scene_check_rigidbody_active(scene)) {
		BKE_rigidbody_rebuild_world(depsgraph, scene, ctime);
	}
}

void BKE_rigidbody_eval_simulation(Depsgraph *depsgraph,
                                   Scene *scene)
{
	float ctime = DEG_get_ctime(depsgraph);
	DEG_debug_print_eval_time(depsgraph, __func__, scene->id.name, scene, ctime);

	/* evaluate rigidbody sim */
	if (!BKE_scene_check_rigidbody_active(scene)) {
		return;
	}
	BKE_rigidbody_do_simulation(depsgraph, scene, ctime);
}

void BKE_rigidbody_object_sync_transforms(Depsgraph *depsgraph,
                                          Scene *scene,
                                          Object *ob)
{
	RigidBodyWorld *rbw = scene->rigidbody_world;
	float ctime = DEG_get_ctime(depsgraph);
	DEG_debug_print_eval_time(depsgraph, __func__, ob->id.name, ob, ctime);
	/* read values pushed into RBO from sim/cache... */
	BKE_rigidbody_sync_transforms(rbw, ob, ctime);
}
