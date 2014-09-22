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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/BPH_mass_spring.c
 *  \ingroup bph
 */

extern "C" {
#include "MEM_guardedalloc.h"

#include "DNA_cloth_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_force.h"
#include "DNA_object_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"

#include "BLI_math.h"
#include "BLI_linklist.h"
#include "BLI_utildefines.h"

#include "BKE_cloth.h"
#include "BKE_collision.h"
#include "BKE_effect.h"
}

#include "BPH_mass_spring.h"
#include "implicit.h"

/* Number of off-diagonal non-zero matrix blocks.
 * Basically there is one of these for each vertex-vertex interaction.
 */
static int cloth_count_nondiag_blocks(Cloth *cloth)
{
	LinkNode *link;
	int nondiag = 0;
	
	for (link = cloth->springs; link; link = link->next) {
		ClothSpring *spring = (ClothSpring *)link->link;
		switch (spring->type) {
			case CLOTH_SPRING_TYPE_BENDING_ANG:
				/* angular bending combines 3 vertices */
				nondiag += 3;
				break;
				
			default:
				/* all other springs depend on 2 vertices only */
				nondiag += 1;
				break;
		}
	}
	
	return nondiag;
}

int BPH_cloth_solver_init(Object *UNUSED(ob), ClothModifierData *clmd)
{
	Cloth *cloth = clmd->clothObject;
	ClothVertex *verts = cloth->verts;
	const float ZERO[3] = {0.0f, 0.0f, 0.0f};
	Implicit_Data *id;
	unsigned int i, nondiag;
	
	nondiag = cloth_count_nondiag_blocks(cloth);
	cloth->implicit = id = BPH_mass_spring_solver_create(cloth->numverts, nondiag);
	
	for (i = 0; i < cloth->numverts; i++) {
		BPH_mass_spring_set_vertex_mass(id, i, verts[i].mass);
	}
	
	// init springs 
	i = 0;
	LinkNode *link = cloth->springs;
	for (; link; link = link->next) {
		ClothSpring *spring = (ClothSpring *)link->link;
		
		spring->matrix_ij_kl = BPH_mass_spring_init_spring(id, i++, spring->ij, spring->kl);
		if (spring->type == CLOTH_SPRING_TYPE_BENDING_ANG) {
			spring->matrix_kl_mn = BPH_mass_spring_init_spring(id, i++, spring->kl, spring->mn);
			spring->matrix_ij_mn = BPH_mass_spring_init_spring(id, i++, spring->ij, spring->mn);
		}
	}
	
	for (i = 0; i < cloth->numverts; i++) {
		BPH_mass_spring_set_motion_state(id, i, verts[i].x, ZERO);
	}
	
	return 1;
}

void BPH_cloth_solver_free(ClothModifierData *clmd)
{
	Cloth *cloth = clmd->clothObject;
	
	if (cloth->implicit) {
		BPH_mass_spring_solver_free(cloth->implicit);
		cloth->implicit = NULL;
	}
}

void BKE_cloth_solver_set_positions(ClothModifierData *clmd)
{
	Cloth *cloth = clmd->clothObject;
	ClothVertex *verts = cloth->verts;
	unsigned int numverts = cloth->numverts, i;
	ClothHairRoot *cloth_roots = clmd->roots;
	Implicit_Data *id = cloth->implicit;
	const float ZERO[3] = {0.0f, 0.0f, 0.0f};
	
	for (i = 0; i < numverts; i++) {
		ClothHairRoot *root = &cloth_roots[i];
		
		BPH_mass_spring_set_rest_transform(id, i, root->rot);
		BPH_mass_spring_set_motion_state(id, i, verts[i].x, verts[i].v);
	}
}

static bool collision_response(ClothModifierData *clmd, CollisionModifierData *collmd, CollPair *collpair, float dt, float restitution, float r_impulse[3])
{
	Cloth *cloth = clmd->clothObject;
	int index = collpair->ap1;
	bool result = false;
	
	float v1[3], v2_old[3], v2_new[3], v_rel_old[3], v_rel_new[3];
	float epsilon2 = BLI_bvhtree_getepsilon(collmd->bvhtree);

	float margin_distance = collpair->distance - epsilon2;
	float mag_v_rel;
	
	zero_v3(r_impulse);
	
	if (margin_distance > 0.0f)
		return false; /* XXX tested before already? */
	
	/* only handle static collisions here */
	if ( collpair->flag & COLLISION_IN_FUTURE )
		return false;
	
	/* velocity */
	copy_v3_v3(v1, cloth->verts[index].v);
	collision_get_collider_velocity(v2_old, v2_new, collmd, collpair);
	/* relative velocity = velocity of the cloth point relative to the collider */
	sub_v3_v3v3(v_rel_old, v1, v2_old);
	sub_v3_v3v3(v_rel_new, v1, v2_new);
	/* normal component of the relative velocity */
	mag_v_rel = dot_v3v3(v_rel_old, collpair->normal);
	
	/* only valid when moving toward the collider */
	if (mag_v_rel < -ALMOST_ZERO) {
		float v_nor_old, v_nor_new;
		float v_tan_old[3], v_tan_new[3];
		float bounce, repulse;
		
		/* Collision response based on
		 * "Simulating Complex Hair with Robust Collision Handling" (Choe, Choi, Ko, ACM SIGGRAPH 2005)
		 * http://graphics.snu.ac.kr/publications/2005-choe-HairSim/Choe_2005_SCA.pdf
		 */
		
		v_nor_old = mag_v_rel;
		v_nor_new = dot_v3v3(v_rel_new, collpair->normal);
		
		madd_v3_v3v3fl(v_tan_old, v_rel_old, collpair->normal, -v_nor_old);
		madd_v3_v3v3fl(v_tan_new, v_rel_new, collpair->normal, -v_nor_new);
		
		bounce = -v_nor_old * restitution;
		
		repulse = -margin_distance / dt; /* base repulsion velocity in normal direction */
		/* XXX this clamping factor is quite arbitrary ...
		 * not sure if there is a more scientific approach, but seems to give good results
		 */
		CLAMP(repulse, 0.0f, 4.0f * bounce);
		
		if (margin_distance < -epsilon2) {
			mul_v3_v3fl(r_impulse, collpair->normal, max_ff(repulse, bounce) - v_nor_new);
		}
		else {
			bounce = 0.0f;
			mul_v3_v3fl(r_impulse, collpair->normal, repulse - v_nor_new);
		}
		
		result = true;
	}
	
	return result;
}

/* Init constraint matrix
 * This is part of the modified CG method suggested by Baraff/Witkin in
 * "Large Steps in Cloth Simulation" (Siggraph 1998)
 */
static void cloth_setup_constraints(ClothModifierData *clmd, ColliderContacts *contacts, int totcolliders, float dt)
{
	Cloth *cloth = clmd->clothObject;
	Implicit_Data *data = cloth->implicit;
	ClothVertex *verts = cloth->verts;
	int numverts = cloth->numverts;
	int i, j, v;
	
	const float ZERO[3] = {0.0f, 0.0f, 0.0f};
	
	BPH_mass_spring_clear_constraints(data);
	
	for (v = 0; v < numverts; v++) {
		if (verts[v].flags & CLOTH_VERT_FLAG_PINNED) {
			/* pinned vertex constraints */
			BPH_mass_spring_add_constraint_ndof0(data, v, ZERO); /* velocity is defined externally */
		}
		
		verts[v].impulse_count = 0;
	}

	for (i = 0; i < totcolliders; ++i) {
		ColliderContacts *ct = &contacts[i];
		for (j = 0; j < ct->totcollisions; ++j) {
			CollPair *collpair = &ct->collisions[j];
//			float restitution = (1.0f - clmd->coll_parms->damping) * (1.0f - ct->ob->pd->pdef_sbdamp);
			float restitution = 0.0f;
			int v = collpair->face1;
			float impulse[3];
			
			/* pinned verts handled separately */
			if (verts[v].flags & CLOTH_VERT_FLAG_PINNED)
				continue;
			
			/* XXX cheap way of avoiding instability from multiple collisions in the same step
			 * this should eventually be supported ...
			 */
			if (verts[v].impulse_count > 0)
				continue;
			
			/* calculate collision response */
			if (!collision_response(clmd, ct->collmd, collpair, dt, restitution, impulse))
				continue;
			
			BPH_mass_spring_add_constraint_ndof2(data, v, collpair->normal, impulse);
			++verts[v].impulse_count;
			
			BKE_sim_debug_data_add_dot(clmd->debug_data, collpair->pa, 0, 1, 0, "collision", hash_collpair(936, collpair));
//			BKE_sim_debug_data_add_dot(clmd->debug_data, collpair->pb, 1, 0, 0, "collision", hash_collpair(937, collpair));
//			BKE_sim_debug_data_add_line(clmd->debug_data, collpair->pa, collpair->pb, 0.7, 0.7, 0.7, "collision", hash_collpair(938, collpair));
			
			{ /* DEBUG */
				float nor[3];
				mul_v3_v3fl(nor, collpair->normal, -collpair->distance);
				BKE_sim_debug_data_add_vector(clmd->debug_data, collpair->pa, nor, 1, 1, 0, "collision", hash_collpair(939, collpair));
//				BKE_sim_debug_data_add_vector(clmd->debug_data, collpair->pb, impulse, 1, 1, 0, "collision", hash_collpair(940, collpair));
//				BKE_sim_debug_data_add_vector(clmd->debug_data, collpair->pb, collpair->normal, 1, 1, 0, "collision", hash_collpair(941, collpair));
			}
		}
	}
}

/* computes where the cloth would be if it were subject to perfectly stiff edges
 * (edge distance constraints) in a lagrangian solver.  then add forces to help
 * guide the implicit solver to that state.  this function is called after
 * collisions*/
static int UNUSED_FUNCTION(cloth_calc_helper_forces)(Object *UNUSED(ob), ClothModifierData *clmd, float (*initial_cos)[3], float UNUSED(step), float dt)
{
	Cloth *cloth= clmd->clothObject;
	float (*cos)[3] = (float (*)[3])MEM_callocN(sizeof(float)*3*cloth->numverts, "cos cloth_calc_helper_forces");
	float *masses = (float *)MEM_callocN(sizeof(float)*cloth->numverts, "cos cloth_calc_helper_forces");
	LinkNode *node;
	ClothSpring *spring;
	ClothVertex *cv;
	int i, steps;
	
	cv = cloth->verts;
	for (i=0; i<cloth->numverts; i++, cv++) {
		copy_v3_v3(cos[i], cv->tx);
		
		if (cv->goal == 1.0f || len_squared_v3v3(initial_cos[i], cv->tx) != 0.0f) {
			masses[i] = 1e+10;
		}
		else {
			masses[i] = cv->mass;
		}
	}
	
	steps = 55;
	for (i=0; i<steps; i++) {
		for (node=cloth->springs; node; node=node->next) {
			/* ClothVertex *cv1, *cv2; */ /* UNUSED */
			int v1, v2;
			float len, c, l, vec[3];
			
			spring = (ClothSpring *)node->link;
			if (spring->type != CLOTH_SPRING_TYPE_STRUCTURAL && spring->type != CLOTH_SPRING_TYPE_SHEAR) 
				continue;
			
			v1 = spring->ij; v2 = spring->kl;
			/* cv1 = cloth->verts + v1; */ /* UNUSED */
			/* cv2 = cloth->verts + v2; */ /* UNUSED */
			len = len_v3v3(cos[v1], cos[v2]);
			
			sub_v3_v3v3(vec, cos[v1], cos[v2]);
			normalize_v3(vec);
			
			c = (len - spring->restlen);
			if (c == 0.0f)
				continue;
			
			l = c / ((1.0f / masses[v1]) + (1.0f / masses[v2]));
			
			mul_v3_fl(vec, -(1.0f / masses[v1]) * l);
			add_v3_v3(cos[v1], vec);
	
			sub_v3_v3v3(vec, cos[v2], cos[v1]);
			normalize_v3(vec);
			
			mul_v3_fl(vec, -(1.0f / masses[v2]) * l);
			add_v3_v3(cos[v2], vec);
		}
	}
	
	cv = cloth->verts;
	for (i=0; i<cloth->numverts; i++, cv++) {
		float vec[3];
		
		/*compute forces*/
		sub_v3_v3v3(vec, cos[i], cv->tx);
		mul_v3_fl(vec, cv->mass*dt*20.0f);
		add_v3_v3(cv->tv, vec);
		//copy_v3_v3(cv->tx, cos[i]);
	}
	
	MEM_freeN(cos);
	MEM_freeN(masses);
	
	return 1;
}

BLI_INLINE void cloth_calc_spring_force(ClothModifierData *clmd, ClothSpring *s, float time)
{
	Cloth *cloth = clmd->clothObject;
	ClothSimSettings *parms = clmd->sim_parms;
	Implicit_Data *data = cloth->implicit;
	ClothVertex *verts = cloth->verts;
	
	bool no_compress = parms->flags & CLOTH_SIMSETTINGS_FLAG_NO_SPRING_COMPRESS;
	
	zero_v3(s->f);
	zero_m3(s->dfdx);
	zero_m3(s->dfdv);
	
	s->flags &= ~CLOTH_SPRING_FLAG_NEEDED;
	
	// calculate force of structural + shear springs
	if ((s->type & CLOTH_SPRING_TYPE_STRUCTURAL) || (s->type & CLOTH_SPRING_TYPE_SHEAR) || (s->type & CLOTH_SPRING_TYPE_SEWING) ) {
#ifdef CLOTH_FORCE_SPRING_STRUCTURAL
		float k, scaling;
		
		s->flags |= CLOTH_SPRING_FLAG_NEEDED;
		
		scaling = parms->structural + s->stiffness * fabsf(parms->max_struct - parms->structural);
		k = scaling / (parms->avg_spring_len + FLT_EPSILON);
		
		if (s->type & CLOTH_SPRING_TYPE_SEWING) {
			// TODO: verify, half verified (couldn't see error)
			// sewing springs usually have a large distance at first so clamp the force so we don't get tunnelling through colission objects
			BPH_mass_spring_force_spring_linear(data, s->ij, s->kl, s->matrix_ij_kl, s->restlen, k, parms->Cdis, no_compress, parms->max_sewing, s->f, s->dfdx, s->dfdv);
		}
		else {
			BPH_mass_spring_force_spring_linear(data, s->ij, s->kl, s->matrix_ij_kl, s->restlen, k, parms->Cdis, no_compress, 0.0f, s->f, s->dfdx, s->dfdv);
		}
#endif
	}
	else if (s->type & CLOTH_SPRING_TYPE_GOAL) {
#ifdef CLOTH_FORCE_SPRING_GOAL
		float goal_x[3], goal_v[3];
		float k, scaling;
		
		s->flags |= CLOTH_SPRING_FLAG_NEEDED;
		
		// current_position = xold + t * (newposition - xold)
		interp_v3_v3v3(goal_x, verts[s->ij].xold, verts[s->ij].xconst, time);
		sub_v3_v3v3(goal_v, verts[s->ij].xconst, verts[s->ij].xold); // distance covered over dt==1
		
		scaling = parms->goalspring + s->stiffness * fabsf(parms->max_struct - parms->goalspring);
		k = verts[s->ij].goal * scaling / (parms->avg_spring_len + FLT_EPSILON);
		
		BPH_mass_spring_force_spring_goal(data, s->ij, s->matrix_ij_kl, goal_x, goal_v, k, parms->goalfrict * 0.01f, s->f, s->dfdx, s->dfdv);
#endif
	}
	else if (s->type & CLOTH_SPRING_TYPE_BENDING) {  /* calculate force of bending springs */
#ifdef CLOTH_FORCE_SPRING_BEND
		float kb, cb, scaling;
		
		s->flags |= CLOTH_SPRING_FLAG_NEEDED;
		
		scaling = parms->bending + s->stiffness * fabsf(parms->max_bend - parms->bending);
		cb = kb = scaling / (20.0f * (parms->avg_spring_len + FLT_EPSILON));
		
		BPH_mass_spring_force_spring_bending(data, s->ij, s->kl, s->matrix_ij_kl, s->restlen, kb, cb, s->f, s->dfdx, s->dfdv);
#endif
	}
	else if (s->type & CLOTH_SPRING_TYPE_BENDING_ANG) {
#ifdef CLOTH_FORCE_SPRING_BEND
		float kb, cb, scaling;
		
		s->flags |= CLOTH_SPRING_FLAG_NEEDED;
		
		scaling = parms->bending + s->stiffness * fabsf(parms->max_bend - parms->bending);
		cb = kb = scaling / (20.0f * (parms->avg_spring_len + FLT_EPSILON));
		
		/* XXX assuming same restlen for ij and jk segments here, this can be done correctly for hair later */
		BPH_mass_spring_force_spring_bending_angular(data, s->ij, s->kl, s->mn, s->matrix_ij_kl, s->matrix_kl_mn, s->matrix_ij_mn, s->restlen, s->restlen, kb, cb);
#endif
	}
}

static void hair_get_boundbox(ClothModifierData *clmd, float gmin[3], float gmax[3])
{
	Cloth *cloth = clmd->clothObject;
	Implicit_Data *data = cloth->implicit;
	unsigned int numverts = cloth->numverts;
	int i;
	
	INIT_MINMAX(gmin, gmax);
	for (i = 0; i < numverts; i++) {
		float x[3];
		BPH_mass_spring_get_motion_state(data, i, x, NULL);
		DO_MINMAX(x, gmin, gmax);
	}
}

static void cloth_calc_volume_force(ClothModifierData *clmd)
{
	ClothSimSettings *parms = clmd->sim_parms;
	Cloth *cloth = clmd->clothObject;
	Implicit_Data *data = cloth->implicit;
	int numverts = cloth->numverts;
	
	/* 2.0f is an experimental value that seems to give good results */
	float smoothfac = 2.0f * parms->velocity_smooth;
	float collfac = 2.0f * parms->collider_friction;
	float pressfac = parms->pressure;
	float minpress = parms->pressure_threshold;
	float gmin[3], gmax[3];
	int i;
	
	hair_get_boundbox(clmd, gmin, gmax);
	
	/* gather velocities & density */
	if (smoothfac > 0.0f || pressfac > 0.0f) {
		HairVertexGrid *vertex_grid = BPH_hair_volume_create_vertex_grid(clmd->sim_parms->voxel_res, gmin, gmax);
		
		for (i = 0; i < numverts; i++) {
			float x[3], v[3];
			
			BPH_mass_spring_get_motion_state(data, i, x, v);
			BPH_hair_volume_add_vertex(vertex_grid, x, v);
		}
		BPH_hair_volume_normalize_vertex_grid(vertex_grid);
		
#if 0
		/* apply velocity filter */
		BPH_hair_volume_vertex_grid_filter_box(vertex_grid, clmd->sim_parms->voxel_filter_size);
#endif
		
		for (i = 0; i < numverts; i++) {
			float x[3], v[3], f[3], dfdx[3][3], dfdv[3][3];
			
			/* calculate volumetric forces */
			BPH_mass_spring_get_motion_state(data, i, x, v);
			BPH_hair_volume_vertex_grid_forces(vertex_grid, x, v, smoothfac, pressfac, minpress, f, dfdx, dfdv);
			/* apply on hair data */
			BPH_mass_spring_force_extern(data, i, f, dfdx, dfdv);
		}
		
		BPH_hair_volume_free_vertex_grid(vertex_grid);
	}
}

static void cloth_calc_force(ClothModifierData *clmd, float UNUSED(frame), ListBase *effectors, float time)
{
	/* Collect forces and derivatives:  F, dFdX, dFdV */
	Cloth *cloth = clmd->clothObject;
	Implicit_Data *data = cloth->implicit;
	unsigned int i	= 0;
	float 		drag 	= clmd->sim_parms->Cvi * 0.01f; /* viscosity of air scaled in percent */
	float 		gravity[3] = {0.0f, 0.0f, 0.0f};
	MFace 		*mfaces 	= cloth->mfaces;
	unsigned int numverts = cloth->numverts;
	
	/* initialize forces to zero */
	BPH_mass_spring_force_clear(data);
	
#ifdef CLOTH_FORCE_GRAVITY
	/* global acceleration (gravitation) */
	if (clmd->scene->physics_settings.flag & PHYS_GLOBAL_GRAVITY) {
		/* scale gravity force */
		mul_v3_v3fl(gravity, clmd->scene->physics_settings.gravity, 0.001f * clmd->sim_parms->effector_weights->global_gravity);
	}
	BPH_mass_spring_force_gravity(data, gravity);
#endif

	cloth_calc_volume_force(clmd);

#ifdef CLOTH_FORCE_DRAG
	BPH_mass_spring_force_drag(data, drag);
#endif
	
	/* handle external forces like wind */
	if (effectors) {
		/* cache per-vertex forces to avoid redundant calculation */
		float (*winvec)[3] = (float (*)[3])MEM_callocN(sizeof(float) * 3 * numverts, "effector forces");
		for (i = 0; i < cloth->numverts; i++) {
			float x[3], v[3];
			EffectedPoint epoint;
			
			BPH_mass_spring_get_motion_state(data, i, x, v);
			pd_point_from_loc(clmd->scene, x, v, i, &epoint);
			pdDoEffectors(effectors, NULL, clmd->sim_parms->effector_weights, &epoint, winvec[i], NULL);
		}
		
		for (i = 0; i < cloth->numfaces; i++) {
			MFace *mf = &mfaces[i];
			BPH_mass_spring_force_face_wind(data, mf->v1, mf->v2, mf->v3, mf->v4, winvec);
		}

		/* Hair has only edges */
		if (cloth->numfaces == 0) {
			for (LinkNode *link = cloth->springs; link; link = link->next) {
				ClothSpring *spring = (ClothSpring *)link->link;
				if (spring->type == CLOTH_SPRING_TYPE_STRUCTURAL)
					BPH_mass_spring_force_edge_wind(data, spring->ij, spring->kl, winvec);
			}
		}

		MEM_freeN(winvec);
	}
	
	// calculate spring forces
	for (LinkNode *link = cloth->springs; link; link = link->next) {
		ClothSpring *spring = (ClothSpring *)link->link;
		// only handle active springs
		if (!(spring->flags & CLOTH_SPRING_FLAG_DEACTIVATE))
			cloth_calc_spring_force(clmd, spring, time);
	}
}

static void cloth_clear_result(ClothModifierData *clmd)
{
	ClothSolverResult *sres = clmd->solver_result;
	
	sres->status = 0;
	sres->max_error = sres->min_error = sres->avg_error = 0.0f;
	sres->max_iterations = sres->min_iterations = 0;
	sres->avg_iterations = 0.0f;
}

static void cloth_record_result(ClothModifierData *clmd, ImplicitSolverResult *result, int steps)
{
	ClothSolverResult *sres = clmd->solver_result;
	
	if (sres->status) { /* already initialized ? */
		/* error only makes sense for successful iterations */
		if (result->status == BPH_SOLVER_SUCCESS) {
			sres->min_error = min_ff(sres->min_error, result->error);
			sres->max_error = max_ff(sres->max_error, result->error);
			sres->avg_error += result->error / (float)steps;
		}
		
		sres->min_iterations = min_ii(sres->min_iterations, result->iterations);
		sres->max_iterations = max_ii(sres->max_iterations, result->iterations);
		sres->avg_iterations += (float)result->iterations / (float)steps;
	}
	else {
		/* error only makes sense for successful iterations */
		if (result->status == BPH_SOLVER_SUCCESS) {
			sres->min_error = sres->max_error = result->error;
			sres->avg_error += result->error / (float)steps;
		}
		
		sres->min_iterations = sres->max_iterations  = result->iterations;
		sres->avg_iterations += (float)result->iterations / (float)steps;
	}
	
	sres->status |= result->status;
}

int BPH_cloth_solve(Object *ob, float frame, ClothModifierData *clmd, ListBase *effectors)
{
	unsigned int i=0;
	float step=0.0f, tf=clmd->sim_parms->timescale;
	Cloth *cloth = clmd->clothObject;
	ClothVertex *verts = cloth->verts/*, *cv*/;
	unsigned int numverts = cloth->numverts;
	float dt = clmd->sim_parms->timescale / clmd->sim_parms->stepsPerFrame;
	Implicit_Data *id = cloth->implicit;
	ColliderContacts *contacts = NULL;
	int totcolliders = 0;
	
	BPH_mass_spring_solver_debug_data(id, clmd->debug_data);
	
	BKE_sim_debug_data_clear_category(clmd->debug_data, "collision");
	
	if (!clmd->solver_result)
		clmd->solver_result = (ClothSolverResult *)MEM_callocN(sizeof(ClothSolverResult), "cloth solver result");
	cloth_clear_result(clmd);
	
	if (clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_GOAL) { /* do goal stuff */
		for (i = 0; i < numverts; i++) {
			// update velocities with constrained velocities from pinned verts
			if (verts [i].flags & CLOTH_VERT_FLAG_PINNED) {
				float v[3];
				sub_v3_v3v3(v, verts[i].xconst, verts[i].xold);
				// mul_v3_fl(v, clmd->sim_parms->stepsPerFrame);
				BPH_mass_spring_set_velocity(id, i, v);
			}
		}
	}
	
	if (clmd->debug_data) {
		for (i = 0; i < numverts; i++) {
			BKE_sim_debug_data_add_dot(clmd->debug_data, verts[i].x, 1.0f, 0.1f, 1.0f, "points", hash_vertex(583, i));
		}
	}
	
	while (step < tf) {
		ImplicitSolverResult result;
		
		/* copy velocities for collision */
		for (i = 0; i < numverts; i++) {
			BPH_mass_spring_get_motion_state(id, i, NULL, verts[i].tv);
			copy_v3_v3(verts[i].v, verts[i].tv);
		}
		
		/* determine contact points */
		if (clmd->coll_parms->flags & CLOTH_COLLSETTINGS_FLAG_ENABLED) {
			if (clmd->coll_parms->flags & CLOTH_COLLSETTINGS_FLAG_POINTS) {
				cloth_find_point_contacts(ob, clmd, 0.0f, tf, &contacts, &totcolliders);
			}
		}
		
		/* setup vertex constraints for pinned vertices and contacts */
		cloth_setup_constraints(clmd, contacts, totcolliders, dt);
		
		// damping velocity for artistic reasons
		// this is a bad way to do it, should be removed imo - lukas_t
		if (clmd->sim_parms->vel_damping != 1.0f) {
			for (i = 0; i < numverts; i++) {
				float v[3];
				BPH_mass_spring_get_motion_state(id, i, NULL, v);
				mul_v3_fl(v, clmd->sim_parms->vel_damping);
				BPH_mass_spring_set_velocity(id, i, v);
			}
		}
		
		// calculate forces
		cloth_calc_force(clmd, frame, effectors, step);
		
		// calculate new velocity and position
		BPH_mass_spring_solve(id, dt, &result);
		cloth_record_result(clmd, &result, clmd->sim_parms->stepsPerFrame);
		
		BPH_mass_spring_apply_result(id);
		
		/* move pinned verts to correct position */
		for (i = 0; i < numverts; i++) {
			if (clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_GOAL) {
				if (verts[i].flags & CLOTH_VERT_FLAG_PINNED) {
					float x[3];
					interp_v3_v3v3(x, verts[i].xold, verts[i].xconst, step + dt);
					BPH_mass_spring_set_position(id, i, x);
				}
			}
			
			BPH_mass_spring_get_motion_state(id, i, verts[i].txold, NULL);
			
//			if (!(verts[i].flags & CLOTH_VERT_FLAG_PINNED) && i > 0) {
//				BKE_sim_debug_data_add_line(clmd->debug_data, id->X[i], id->X[i-1], 0.6, 0.3, 0.3, "hair", hash_vertex(4892, i));
//				BKE_sim_debug_data_add_line(clmd->debug_data, id->Xnew[i], id->Xnew[i-1], 1, 0.5, 0.5, "hair", hash_vertex(4893, i));
//			}
//			BKE_sim_debug_data_add_vector(clmd->debug_data, id->X[i], id->V[i], 0, 0, 1, "velocity", hash_vertex(3158, i));
		}
		
		/* free contact points */
		if (contacts) {
			cloth_free_contacts(contacts, totcolliders);
		}
		
		step += dt;
	}
	
	/* copy results back to cloth data */
	for (i = 0; i < numverts; i++) {
		BPH_mass_spring_get_motion_state(id, i, verts[i].x, verts[i].v);
		copy_v3_v3(verts[i].txold, verts[i].x);
	}
	
	BPH_mass_spring_solver_debug_data(id, NULL);
	
	return 1;
}
