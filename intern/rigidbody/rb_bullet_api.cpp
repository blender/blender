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

/** \file rb_bullet_api.cpp
 *  \ingroup RigidBody
 *  \brief Rigid Body API implementation for Bullet
 */

/*
Bullet Continuous Collision Detection and Physics Library
Copyright (c) 2003-2006 Erwin Coumans  http://continuousphysics.com/Bullet/

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, 
including commercial applications, and to alter it and redistribute it freely, 
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/
 
/* This file defines the "RigidBody interface" for the 
 * Bullet Physics Engine. This API is designed to be used
 * from C-code in Blender as part of the Rigid Body simulation
 * system.
 *
 * It is based on the Bullet C-API, but is heavily modified to 
 * give access to more data types and to offer a nicer interface.
 *
 * -- Joshua Leung, June 2010
 */

#include <stdio.h>
#include <errno.h>

#include "RBI_api.h"

#include "btBulletDynamicsCommon.h"

#include "LinearMath/btVector3.h"
#include "LinearMath/btScalar.h"	
#include "LinearMath/btMatrix3x3.h"
#include "LinearMath/btTransform.h"
#include "LinearMath/btConvexHullComputer.h"

#include "BulletCollision/Gimpact/btGImpactShape.h"
#include "BulletCollision/Gimpact/btGImpactCollisionAlgorithm.h"
#include "BulletCollision/CollisionShapes/btScaledBvhTriangleMeshShape.h"

struct rbDynamicsWorld {
	btDiscreteDynamicsWorld *dynamicsWorld;
	btDefaultCollisionConfiguration *collisionConfiguration;
	btDispatcher *dispatcher;
	btBroadphaseInterface *pairCache;
	btConstraintSolver *constraintSolver;
	btOverlapFilterCallback *filterCallback;
};
struct rbRigidBody {
	btRigidBody *body;
	int col_groups;
};

struct rbVert {
	float x, y, z;
};
struct rbTri {
	int v0, v1, v2;
};

struct rbMeshData {
	btTriangleIndexVertexArray *index_array;
	rbVert *vertices;
	rbTri *triangles;
	int num_vertices;
	int num_triangles;
};

struct rbCollisionShape {
	btCollisionShape *cshape;
	rbMeshData *mesh;
};

struct rbFilterCallback : public btOverlapFilterCallback
{
	virtual bool needBroadphaseCollision(btBroadphaseProxy *proxy0, btBroadphaseProxy *proxy1) const
	{
		rbRigidBody *rb0 = (rbRigidBody *)((btRigidBody *)proxy0->m_clientObject)->getUserPointer();
		rbRigidBody *rb1 = (rbRigidBody *)((btRigidBody *)proxy1->m_clientObject)->getUserPointer();
		
		bool collides;
		collides = (proxy0->m_collisionFilterGroup & proxy1->m_collisionFilterMask) != 0;
		collides = collides && (proxy1->m_collisionFilterGroup & proxy0->m_collisionFilterMask);
		collides = collides && (rb0->col_groups & rb1->col_groups);
		
		return collides;
	}
};

static inline void copy_v3_btvec3(float vec[3], const btVector3 &btvec)
{
	vec[0] = (float)btvec[0];
	vec[1] = (float)btvec[1];
	vec[2] = (float)btvec[2];
}
static inline void copy_quat_btquat(float quat[4], const btQuaternion &btquat)
{
	quat[0] = btquat.getW();
	quat[1] = btquat.getX();
	quat[2] = btquat.getY();
	quat[3] = btquat.getZ();
}

/* ********************************** */
/* Dynamics World Methods */

/* Setup ---------------------------- */

rbDynamicsWorld *RB_dworld_new(const float gravity[3])
{
	rbDynamicsWorld *world = new rbDynamicsWorld;
	
	/* collision detection/handling */
	world->collisionConfiguration = new btDefaultCollisionConfiguration();
	
	world->dispatcher = new btCollisionDispatcher(world->collisionConfiguration);
	btGImpactCollisionAlgorithm::registerAlgorithm((btCollisionDispatcher *)world->dispatcher);
	
	world->pairCache = new btDbvtBroadphase();
	
	world->filterCallback = new rbFilterCallback();
	world->pairCache->getOverlappingPairCache()->setOverlapFilterCallback(world->filterCallback);

	/* constraint solving */
	world->constraintSolver = new btSequentialImpulseConstraintSolver();

	/* world */
	world->dynamicsWorld = new btDiscreteDynamicsWorld(world->dispatcher,
	                                                   world->pairCache,
	                                                   world->constraintSolver,
	                                                   world->collisionConfiguration);

	RB_dworld_set_gravity(world, gravity);
	
	return world;
}

void RB_dworld_delete(rbDynamicsWorld *world)
{
	/* bullet doesn't like if we free these in a different order */
	delete world->dynamicsWorld;
	delete world->constraintSolver;
	delete world->pairCache;
	delete world->dispatcher;
	delete world->collisionConfiguration;
	delete world->filterCallback;
	delete world;
}

/* Settings ------------------------- */

/* Gravity */
void RB_dworld_get_gravity(rbDynamicsWorld *world, float g_out[3])
{
	copy_v3_btvec3(g_out, world->dynamicsWorld->getGravity());
}

void RB_dworld_set_gravity(rbDynamicsWorld *world, const float g_in[3])
{
	world->dynamicsWorld->setGravity(btVector3(g_in[0], g_in[1], g_in[2]));
}

/* Constraint Solver */
void RB_dworld_set_solver_iterations(rbDynamicsWorld *world, int num_solver_iterations)
{
	btContactSolverInfo& info = world->dynamicsWorld->getSolverInfo();
	
	info.m_numIterations = num_solver_iterations;
}

/* Split Impulse */
void RB_dworld_set_split_impulse(rbDynamicsWorld *world, int split_impulse)
{
	btContactSolverInfo& info = world->dynamicsWorld->getSolverInfo();
	
	info.m_splitImpulse = split_impulse;
}

/* Simulation ----------------------- */

void RB_dworld_step_simulation(rbDynamicsWorld *world, float timeStep, int maxSubSteps, float timeSubStep)
{
	world->dynamicsWorld->stepSimulation(timeStep, maxSubSteps, timeSubStep);
}

/* Export -------------------------- */

/**
 * Exports entire dynamics world to Bullet's "*.bullet" binary format
 * which is similar to Blender's SDNA system.
 *
 * \param world Dynamics world to write to file
 * \param filename Assumed to be a valid filename, with .bullet extension
 */
void RB_dworld_export(rbDynamicsWorld *world, const char *filename)
{
	//create a large enough buffer. There is no method to pre-calculate the buffer size yet.
	int maxSerializeBufferSize = 1024 * 1024 * 5;
	
	btDefaultSerializer *serializer = new btDefaultSerializer(maxSerializeBufferSize);
	world->dynamicsWorld->serialize(serializer);
	
	FILE *file = fopen(filename, "wb");
	if (file) {
		fwrite(serializer->getBufferPointer(), serializer->getCurrentBufferSize(), 1, file);
		fclose(file);
	}
	else {
		 fprintf(stderr, "RB_dworld_export: %s\n", strerror(errno));
	}
}

/* ********************************** */
/* Rigid Body Methods */

/* Setup ---------------------------- */

void RB_dworld_add_body(rbDynamicsWorld *world, rbRigidBody *object, int col_groups)
{
	btRigidBody *body = object->body;
	object->col_groups = col_groups;
	
	world->dynamicsWorld->addRigidBody(body);
}

void RB_dworld_remove_body(rbDynamicsWorld *world, rbRigidBody *object)
{
	btRigidBody *body = object->body;
	
	world->dynamicsWorld->removeRigidBody(body);
}

/* Collision detection */

void RB_world_convex_sweep_test(
        rbDynamicsWorld *world, rbRigidBody *object,
        const float loc_start[3], const float loc_end[3],
        float v_location[3],  float v_hitpoint[3],  float v_normal[3], int *r_hit)
{
	btRigidBody *body = object->body;
	btCollisionShape *collisionShape = body->getCollisionShape();
	/* only convex shapes are supported, but user can specify a non convex shape */
	if (collisionShape->isConvex()) {
		btCollisionWorld::ClosestConvexResultCallback result(btVector3(loc_start[0], loc_start[1], loc_start[2]), btVector3(loc_end[0], loc_end[1], loc_end[2]));

		btQuaternion obRot = body->getWorldTransform().getRotation();
		
		btTransform rayFromTrans;
		rayFromTrans.setIdentity();
		rayFromTrans.setRotation(obRot);
		rayFromTrans.setOrigin(btVector3(loc_start[0], loc_start[1], loc_start[2]));

		btTransform rayToTrans;
		rayToTrans.setIdentity();
		rayToTrans.setRotation(obRot);
		rayToTrans.setOrigin(btVector3(loc_end[0], loc_end[1], loc_end[2]));
		
		world->dynamicsWorld->convexSweepTest((btConvexShape *)collisionShape, rayFromTrans, rayToTrans, result, 0);
		
		if (result.hasHit()) {
			*r_hit = 1;
			
			v_location[0] = result.m_convexFromWorld[0] + (result.m_convexToWorld[0] - result.m_convexFromWorld[0]) * result.m_closestHitFraction;
			v_location[1] = result.m_convexFromWorld[1] + (result.m_convexToWorld[1] - result.m_convexFromWorld[1]) * result.m_closestHitFraction;
			v_location[2] = result.m_convexFromWorld[2] + (result.m_convexToWorld[2] - result.m_convexFromWorld[2]) * result.m_closestHitFraction;
			
			v_hitpoint[0] = result.m_hitPointWorld[0];
			v_hitpoint[1] = result.m_hitPointWorld[1];
			v_hitpoint[2] = result.m_hitPointWorld[2];
			
			v_normal[0] = result.m_hitNormalWorld[0];
			v_normal[1] = result.m_hitNormalWorld[1];
			v_normal[2] = result.m_hitNormalWorld[2];
			
		}
		else {
			*r_hit = 0;
		}
	}
	else {
		/* we need to return a value if user passes non convex body, to report */
		*r_hit = -2;
	}
}

/* ............ */

rbRigidBody *RB_body_new(rbCollisionShape *shape, const float loc[3], const float rot[4])
{
	rbRigidBody *object = new rbRigidBody;
	/* current transform */
	btTransform trans;
	trans.setOrigin(btVector3(loc[0], loc[1], loc[2]));
	trans.setRotation(btQuaternion(rot[1], rot[2], rot[3], rot[0]));
	
	/* create motionstate, which is necessary for interpolation (includes reverse playback) */
	btDefaultMotionState *motionState = new btDefaultMotionState(trans);
	
	/* make rigidbody */
	btRigidBody::btRigidBodyConstructionInfo rbInfo(1.0f, motionState, shape->cshape);
	
	object->body = new btRigidBody(rbInfo);
	
	object->body->setUserPointer(object);
	
	return object;
}

void RB_body_delete(rbRigidBody *object)
{
	btRigidBody *body = object->body;
	
	/* motion state */
	btMotionState *ms = body->getMotionState();
	if (ms)
		delete ms;
	
	/* collision shape is done elsewhere... */
	
	/* body itself */
	
	/* manually remove constraint refs of the rigid body, normally this happens when removing constraints from the world
	 * but since we delete everything when the world is rebult, we need to do it manually here */
	for (int i = body->getNumConstraintRefs() - 1; i >= 0; i--) {
		btTypedConstraint *con = body->getConstraintRef(i);
		body->removeConstraintRef(con);
	}
	
	delete body;
	delete object;
}

/* Settings ------------------------- */

void RB_body_set_collision_shape(rbRigidBody *object, rbCollisionShape *shape)
{
	btRigidBody *body = object->body;
	
	/* set new collision shape */
	body->setCollisionShape(shape->cshape);
	
	/* recalculate inertia, since that depends on the collision shape... */
	RB_body_set_mass(object, RB_body_get_mass(object));
}

/* ............ */

float RB_body_get_mass(rbRigidBody *object)
{
	btRigidBody *body = object->body;
	
	/* there isn't really a mass setting, but rather 'inverse mass'  
	 * which we convert back to mass by taking the reciprocal again 
	 */
	float value = (float)body->getInvMass();
	
	if (value)
		value = 1.0f / value;
		
	return value;
}

void RB_body_set_mass(rbRigidBody *object, float value)
{
	btRigidBody *body = object->body;
	btVector3 localInertia(0, 0, 0);
	
	/* calculate new inertia if non-zero mass */
	if (value) {
		btCollisionShape *shape = body->getCollisionShape();
		shape->calculateLocalInertia(value, localInertia);
	}
	
	body->setMassProps(value, localInertia);
	body->updateInertiaTensor();
}


float RB_body_get_friction(rbRigidBody *object)
{
	btRigidBody *body = object->body;
	return body->getFriction();
}

void RB_body_set_friction(rbRigidBody *object, float value)
{
	btRigidBody *body = object->body;
	body->setFriction(value);
}


float RB_body_get_restitution(rbRigidBody *object)
{
	btRigidBody *body = object->body;
	return body->getRestitution();
}

void RB_body_set_restitution(rbRigidBody *object, float value)
{
	btRigidBody *body = object->body;
	body->setRestitution(value);
}


float RB_body_get_linear_damping(rbRigidBody *object)
{
	btRigidBody *body = object->body;
	return body->getLinearDamping();
}

void RB_body_set_linear_damping(rbRigidBody *object, float value)
{
	RB_body_set_damping(object, value, RB_body_get_linear_damping(object));
}

float RB_body_get_angular_damping(rbRigidBody *object)
{
	btRigidBody *body = object->body;
	return body->getAngularDamping();
}

void RB_body_set_angular_damping(rbRigidBody *object, float value)
{
	RB_body_set_damping(object, RB_body_get_linear_damping(object), value);
}

void RB_body_set_damping(rbRigidBody *object, float linear, float angular)
{
	btRigidBody *body = object->body;
	body->setDamping(linear, angular);
}


float RB_body_get_linear_sleep_thresh(rbRigidBody *object)
{
	btRigidBody *body = object->body;
	return body->getLinearSleepingThreshold();
}

void RB_body_set_linear_sleep_thresh(rbRigidBody *object, float value)
{
	RB_body_set_sleep_thresh(object, value, RB_body_get_angular_sleep_thresh(object));
}

float RB_body_get_angular_sleep_thresh(rbRigidBody *object)
{
	btRigidBody *body = object->body;
	return body->getAngularSleepingThreshold();
}

void RB_body_set_angular_sleep_thresh(rbRigidBody *object, float value)
{
	RB_body_set_sleep_thresh(object, RB_body_get_linear_sleep_thresh(object), value);
}

void RB_body_set_sleep_thresh(rbRigidBody *object, float linear, float angular)
{
	btRigidBody *body = object->body;
	body->setSleepingThresholds(linear, angular);
}

/* ............ */

void RB_body_get_linear_velocity(rbRigidBody *object, float v_out[3])
{
	btRigidBody *body = object->body;
	
	copy_v3_btvec3(v_out, body->getLinearVelocity());
}

void RB_body_set_linear_velocity(rbRigidBody *object, const float v_in[3])
{
	btRigidBody *body = object->body;
	
	body->setLinearVelocity(btVector3(v_in[0], v_in[1], v_in[2]));
}


void RB_body_get_angular_velocity(rbRigidBody *object, float v_out[3])
{
	btRigidBody *body = object->body;
	
	copy_v3_btvec3(v_out, body->getAngularVelocity());
}

void RB_body_set_angular_velocity(rbRigidBody *object, const float v_in[3])
{
	btRigidBody *body = object->body;
	
	body->setAngularVelocity(btVector3(v_in[0], v_in[1], v_in[2]));
}

void RB_body_set_linear_factor(rbRigidBody *object, float x, float y, float z)
{
	btRigidBody *body = object->body;
	body->setLinearFactor(btVector3(x, y, z));
}

void RB_body_set_angular_factor(rbRigidBody *object, float x, float y, float z)
{
	btRigidBody *body = object->body;
	body->setAngularFactor(btVector3(x, y, z));
}

/* ............ */

void RB_body_set_kinematic_state(rbRigidBody *object, int kinematic)
{
	btRigidBody *body = object->body;
	if (kinematic)
		body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
	else
		body->setCollisionFlags(body->getCollisionFlags() & ~btCollisionObject::CF_KINEMATIC_OBJECT);
}

/* ............ */

void RB_body_set_activation_state(rbRigidBody *object, int use_deactivation)
{
	btRigidBody *body = object->body;
	if (use_deactivation)
		body->forceActivationState(ACTIVE_TAG);
	else
		body->setActivationState(DISABLE_DEACTIVATION);
}
void RB_body_activate(rbRigidBody *object)
{
	btRigidBody *body = object->body;
	body->setActivationState(ACTIVE_TAG);
}
void RB_body_deactivate(rbRigidBody *object)
{
	btRigidBody *body = object->body;
	body->setActivationState(ISLAND_SLEEPING);
}

/* ............ */



/* Simulation ----------------------- */

/* The transform matrices Blender uses are OpenGL-style matrices, 
 * while Bullet uses the Right-Handed coordinate system style instead.
 */

void RB_body_get_transform_matrix(rbRigidBody *object, float m_out[4][4])
{
	btRigidBody *body = object->body;
	btMotionState *ms = body->getMotionState();
	
	btTransform trans;
	ms->getWorldTransform(trans);
	
	trans.getOpenGLMatrix((btScalar *)m_out);
}

void RB_body_set_loc_rot(rbRigidBody *object, const float loc[3], const float rot[4])
{
	btRigidBody *body = object->body;
	btMotionState *ms = body->getMotionState();
	
	/* set transform matrix */
	btTransform trans;
	trans.setOrigin(btVector3(loc[0], loc[1], loc[2]));
	trans.setRotation(btQuaternion(rot[1], rot[2], rot[3], rot[0]));
	
	ms->setWorldTransform(trans);
}

void RB_body_set_scale(rbRigidBody *object, const float scale[3])
{
	btRigidBody *body = object->body;
	
	/* apply scaling factor from matrix above to the collision shape */
	btCollisionShape *cshape = body->getCollisionShape();
	if (cshape) {
		cshape->setLocalScaling(btVector3(scale[0], scale[1], scale[2]));
		
		/* GIimpact shapes have to be updated to take scaling into account */
		if (cshape->getShapeType() == GIMPACT_SHAPE_PROXYTYPE)
			((btGImpactMeshShape *)cshape)->updateBound();
	}
}

/* ............ */
/* Read-only state info about status of simulation */

void RB_body_get_position(rbRigidBody *object, float v_out[3])
{
	btRigidBody *body = object->body;
	
	copy_v3_btvec3(v_out, body->getWorldTransform().getOrigin());
}

void RB_body_get_orientation(rbRigidBody *object, float v_out[4])
{
	btRigidBody *body = object->body;
	
	copy_quat_btquat(v_out, body->getWorldTransform().getRotation());
}

/* ............ */
/* Overrides for simulation */

void RB_body_apply_central_force(rbRigidBody *object, const float v_in[3])
{
	btRigidBody *body = object->body;
	
	body->applyCentralForce(btVector3(v_in[0], v_in[1], v_in[2]));
}

/* ********************************** */
/* Collision Shape Methods */

/* Setup (Standard Shapes) ----------- */

rbCollisionShape *RB_shape_new_box(float x, float y, float z)
{
	rbCollisionShape *shape = new rbCollisionShape;
	shape->cshape = new btBoxShape(btVector3(x, y, z));
	shape->mesh = NULL;
	return shape;
}

rbCollisionShape *RB_shape_new_sphere(float radius)
{
	rbCollisionShape *shape = new rbCollisionShape;
	shape->cshape = new btSphereShape(radius);
	shape->mesh = NULL;
	return shape;
}

rbCollisionShape *RB_shape_new_capsule(float radius, float height)
{
	rbCollisionShape *shape = new rbCollisionShape;
	shape->cshape = new btCapsuleShapeZ(radius, height);
	shape->mesh = NULL;
	return shape;
}

rbCollisionShape *RB_shape_new_cone(float radius, float height)
{
	rbCollisionShape *shape = new rbCollisionShape;
	shape->cshape = new btConeShapeZ(radius, height);
	shape->mesh = NULL;
	return shape;
}

rbCollisionShape *RB_shape_new_cylinder(float radius, float height)
{
	rbCollisionShape *shape = new rbCollisionShape;
	shape->cshape = new btCylinderShapeZ(btVector3(radius, radius, height));
	shape->mesh = NULL;
	return shape;
}

/* Setup (Convex Hull) ------------ */

rbCollisionShape *RB_shape_new_convex_hull(float *verts, int stride, int count, float margin, bool *can_embed)
{
	btConvexHullComputer hull_computer = btConvexHullComputer();
	
	// try to embed the margin, if that fails don't shrink the hull
	if (hull_computer.compute(verts, stride, count, margin, 0.0f) < 0.0f) {
		hull_computer.compute(verts, stride, count, 0.0f, 0.0f);
		*can_embed = false;
	}
	
	rbCollisionShape *shape = new rbCollisionShape;
	btConvexHullShape *hull_shape = new btConvexHullShape(&(hull_computer.vertices[0].getX()), hull_computer.vertices.size());
	
	shape->cshape = hull_shape;
	shape->mesh = NULL;
	return shape;
}

/* Setup (Triangle Mesh) ---------- */

/* Need to call RB_trimesh_finish() after creating triangle mesh and adding vertices and triangles */

rbMeshData *RB_trimesh_data_new(int num_tris, int num_verts)
{
	rbMeshData *mesh = new rbMeshData;
	mesh->vertices = new rbVert[num_verts];
	mesh->triangles = new rbTri[num_tris];
	mesh->num_vertices = num_verts;
	mesh->num_triangles = num_tris;
	
	return mesh;
}

static void RB_trimesh_data_delete(rbMeshData *mesh)
{
	delete mesh->index_array;
	delete[] mesh->vertices;
	delete[] mesh->triangles;
	delete mesh;
}
 
void RB_trimesh_add_vertices(rbMeshData *mesh, float *vertices, int num_verts, int vert_stride)
{
	for (int i = 0; i < num_verts; i++) {
		float *vert = (float*)(((char*)vertices + i * vert_stride));
		mesh->vertices[i].x = vert[0];
		mesh->vertices[i].y = vert[1];
		mesh->vertices[i].z = vert[2];
	}
}
void RB_trimesh_add_triangle_indices(rbMeshData *mesh, int num, int index0, int index1, int index2)
{
	mesh->triangles[num].v0 = index0;
	mesh->triangles[num].v1 = index1;
	mesh->triangles[num].v2 = index2;
}

void RB_trimesh_finish(rbMeshData *mesh)
{
	mesh->index_array = new btTriangleIndexVertexArray(mesh->num_triangles, (int*)mesh->triangles, sizeof(rbTri),
	                                                   mesh->num_vertices, (float*)mesh->vertices, sizeof(rbVert));
}
 
rbCollisionShape *RB_shape_new_trimesh(rbMeshData *mesh)
{
	rbCollisionShape *shape = new rbCollisionShape;
	
	/* triangle-mesh we create is a BVH wrapper for triangle mesh data (for faster lookups) */
	// RB_TODO perhaps we need to allow saving out this for performance when rebuilding?
	btBvhTriangleMeshShape *unscaledShape = new btBvhTriangleMeshShape(mesh->index_array, true, true);
	
	shape->cshape = new btScaledBvhTriangleMeshShape(unscaledShape, btVector3(1.0f, 1.0f, 1.0f));
	shape->mesh = mesh;
	return shape;
}

void RB_shape_trimesh_update(rbCollisionShape *shape, float *vertices, int num_verts, int vert_stride, float min[3], float max[3])
{
	if (shape->mesh == NULL || num_verts != shape->mesh->num_vertices)
		return;
	
	for (int i = 0; i < num_verts; i++) {
		float *vert = (float*)(((char*)vertices + i * vert_stride));
		shape->mesh->vertices[i].x = vert[0];
		shape->mesh->vertices[i].y = vert[1];
		shape->mesh->vertices[i].z = vert[2];
	}
	
	if (shape->cshape->getShapeType() == SCALED_TRIANGLE_MESH_SHAPE_PROXYTYPE) {
		btScaledBvhTriangleMeshShape *scaled_shape = (btScaledBvhTriangleMeshShape *)shape->cshape;
		btBvhTriangleMeshShape *mesh_shape = scaled_shape->getChildShape();
		mesh_shape->refitTree(btVector3(min[0], min[1], min[2]), btVector3(max[0], max[1], max[2]));
	}
	else if (shape->cshape->getShapeType() == GIMPACT_SHAPE_PROXYTYPE) {
		btGImpactMeshShape *mesh_shape = (btGImpactMeshShape*)shape->cshape;
		mesh_shape->updateBound();
	}
}

rbCollisionShape *RB_shape_new_gimpact_mesh(rbMeshData *mesh)
{
	rbCollisionShape *shape = new rbCollisionShape;
	
	btGImpactMeshShape *gimpactShape = new btGImpactMeshShape(mesh->index_array);
	gimpactShape->updateBound(); // TODO: add this to the update collision margin call?
	
	shape->cshape = gimpactShape;
	shape->mesh = mesh;
	return shape;
}

/* Cleanup --------------------------- */

void RB_shape_delete(rbCollisionShape *shape)
{
	if (shape->cshape->getShapeType() == SCALED_TRIANGLE_MESH_SHAPE_PROXYTYPE) {
		btBvhTriangleMeshShape *child_shape = ((btScaledBvhTriangleMeshShape *)shape->cshape)->getChildShape();
		if (child_shape)
			delete child_shape;
	}
	if (shape->mesh)
		RB_trimesh_data_delete(shape->mesh);
	delete shape->cshape;
	delete shape;
}

/* Settings --------------------------- */

float RB_shape_get_margin(rbCollisionShape *shape)
{
	return shape->cshape->getMargin();
}

void RB_shape_set_margin(rbCollisionShape *shape, float value)
{
	shape->cshape->setMargin(value);
}

/* ********************************** */
/* Constraints */

/* Setup ----------------------------- */

void RB_dworld_add_constraint(rbDynamicsWorld *world, rbConstraint *con, int disable_collisions)
{
	btTypedConstraint *constraint = reinterpret_cast<btTypedConstraint*>(con);
	
	world->dynamicsWorld->addConstraint(constraint, disable_collisions);
}

void RB_dworld_remove_constraint(rbDynamicsWorld *world, rbConstraint *con)
{
	btTypedConstraint *constraint = reinterpret_cast<btTypedConstraint*>(con);
	
	world->dynamicsWorld->removeConstraint(constraint);
}

/* ............ */

static void make_constraint_transforms(btTransform &transform1, btTransform &transform2, btRigidBody *body1, btRigidBody *body2, float pivot[3], float orn[4])
{
	btTransform pivot_transform = btTransform();
	pivot_transform.setOrigin(btVector3(pivot[0], pivot[1], pivot[2]));
	pivot_transform.setRotation(btQuaternion(orn[1], orn[2], orn[3], orn[0]));
	
	transform1 = body1->getWorldTransform().inverse() * pivot_transform;
	transform2 = body2->getWorldTransform().inverse() * pivot_transform;
}

rbConstraint *RB_constraint_new_point(float pivot[3], rbRigidBody *rb1, rbRigidBody *rb2)
{
	btRigidBody *body1 = rb1->body;
	btRigidBody *body2 = rb2->body;
	
	btVector3 pivot1 = body1->getWorldTransform().inverse() * btVector3(pivot[0], pivot[1], pivot[2]);
	btVector3 pivot2 = body2->getWorldTransform().inverse() * btVector3(pivot[0], pivot[1], pivot[2]);
	
	btTypedConstraint *con = new btPoint2PointConstraint(*body1, *body2, pivot1, pivot2);
	
	return (rbConstraint *)con;
}

rbConstraint *RB_constraint_new_fixed(float pivot[3], float orn[4], rbRigidBody *rb1, rbRigidBody *rb2)
{
	btRigidBody *body1 = rb1->body;
	btRigidBody *body2 = rb2->body;
	btTransform transform1;
	btTransform transform2;
	
	make_constraint_transforms(transform1, transform2, body1, body2, pivot, orn);
	
	btFixedConstraint *con = new btFixedConstraint(*body1, *body2, transform1, transform2);
	
	return (rbConstraint *)con;
}

rbConstraint *RB_constraint_new_hinge(float pivot[3], float orn[4], rbRigidBody *rb1, rbRigidBody *rb2)
{
	btRigidBody *body1 = rb1->body;
	btRigidBody *body2 = rb2->body;
	btTransform transform1;
	btTransform transform2;
	
	make_constraint_transforms(transform1, transform2, body1, body2, pivot, orn);
	
	btHingeConstraint *con = new btHingeConstraint(*body1, *body2, transform1, transform2);
	
	return (rbConstraint *)con;
}

rbConstraint *RB_constraint_new_slider(float pivot[3], float orn[4], rbRigidBody *rb1, rbRigidBody *rb2)
{
	btRigidBody *body1 = rb1->body;
	btRigidBody *body2 = rb2->body;
	btTransform transform1;
	btTransform transform2;
	
	make_constraint_transforms(transform1, transform2, body1, body2, pivot, orn);
	
	btSliderConstraint *con = new btSliderConstraint(*body1, *body2, transform1, transform2, true);
	
	return (rbConstraint *)con;
}

rbConstraint *RB_constraint_new_piston(float pivot[3], float orn[4], rbRigidBody *rb1, rbRigidBody *rb2)
{
	btRigidBody *body1 = rb1->body;
	btRigidBody *body2 = rb2->body;
	btTransform transform1;
	btTransform transform2;
	
	make_constraint_transforms(transform1, transform2, body1, body2, pivot, orn);
	
	btSliderConstraint *con = new btSliderConstraint(*body1, *body2, transform1, transform2, true);
	con->setUpperAngLimit(-1.0f); // unlock rotation axis
	
	return (rbConstraint *)con;
}

rbConstraint *RB_constraint_new_6dof(float pivot[3], float orn[4], rbRigidBody *rb1, rbRigidBody *rb2)
{
	btRigidBody *body1 = rb1->body;
	btRigidBody *body2 = rb2->body;
	btTransform transform1;
	btTransform transform2;
	
	make_constraint_transforms(transform1, transform2, body1, body2, pivot, orn);
	
	btTypedConstraint *con = new btGeneric6DofConstraint(*body1, *body2, transform1, transform2, true);
	
	return (rbConstraint *)con;
}

rbConstraint *RB_constraint_new_6dof_spring(float pivot[3], float orn[4], rbRigidBody *rb1, rbRigidBody *rb2)
{
	btRigidBody *body1 = rb1->body;
	btRigidBody *body2 = rb2->body;
	btTransform transform1;
	btTransform transform2;
	
	make_constraint_transforms(transform1, transform2, body1, body2, pivot, orn);
	
	btTypedConstraint *con = new btGeneric6DofSpringConstraint(*body1, *body2, transform1, transform2, true);
	
	return (rbConstraint *)con;
}

rbConstraint *RB_constraint_new_motor(float pivot[3], float orn[4], rbRigidBody *rb1, rbRigidBody *rb2)
{
	btRigidBody *body1 = rb1->body;
	btRigidBody *body2 = rb2->body;
	btTransform transform1;
	btTransform transform2;
	
	make_constraint_transforms(transform1, transform2, body1, body2, pivot, orn);
	
	btGeneric6DofConstraint *con = new btGeneric6DofConstraint(*body1, *body2, transform1, transform2, true);
	
	/* unlock constraint axes */
	for (int i = 0; i < 6; i++) {
		con->setLimit(i, 0.0f, -1.0f);
	}
	/* unlock motor axes */
	con->getTranslationalLimitMotor()->m_upperLimit.setValue(-1.0f, -1.0f, -1.0f);
	
	return (rbConstraint *)con;
}

/* Cleanup ----------------------------- */

void RB_constraint_delete(rbConstraint *con)
{
	btTypedConstraint *constraint = reinterpret_cast<btTypedConstraint*>(con);
	delete constraint;
}

/* Settings ------------------------- */

void RB_constraint_set_enabled(rbConstraint *con, int enabled)
{
	btTypedConstraint *constraint = reinterpret_cast<btTypedConstraint*>(con);
	
	constraint->setEnabled(enabled);
}

void RB_constraint_set_limits_hinge(rbConstraint *con, float lower, float upper)
{
	btHingeConstraint *constraint = reinterpret_cast<btHingeConstraint*>(con);
	
	// RB_TODO expose these
	float softness = 0.9f;
	float bias_factor = 0.3f;
	float relaxation_factor = 1.0f;
	
	constraint->setLimit(lower, upper, softness, bias_factor, relaxation_factor);
}

void RB_constraint_set_limits_slider(rbConstraint *con, float lower, float upper)
{
	btSliderConstraint *constraint = reinterpret_cast<btSliderConstraint*>(con);
	
	constraint->setLowerLinLimit(lower);
	constraint->setUpperLinLimit(upper);
}

void RB_constraint_set_limits_piston(rbConstraint *con, float lin_lower, float lin_upper, float ang_lower, float ang_upper)
{
	btSliderConstraint *constraint = reinterpret_cast<btSliderConstraint*>(con);
	
	constraint->setLowerLinLimit(lin_lower);
	constraint->setUpperLinLimit(lin_upper);
	constraint->setLowerAngLimit(ang_lower);
	constraint->setUpperAngLimit(ang_upper);
}

void RB_constraint_set_limits_6dof(rbConstraint *con, int axis, float lower, float upper)
{
	btGeneric6DofConstraint *constraint = reinterpret_cast<btGeneric6DofConstraint*>(con);
	
	constraint->setLimit(axis, lower, upper);
}

void RB_constraint_set_stiffness_6dof_spring(rbConstraint *con, int axis, float stiffness)
{
	btGeneric6DofSpringConstraint *constraint = reinterpret_cast<btGeneric6DofSpringConstraint*>(con);
	
	constraint->setStiffness(axis, stiffness);
}

void RB_constraint_set_damping_6dof_spring(rbConstraint *con, int axis, float damping)
{
	btGeneric6DofSpringConstraint *constraint = reinterpret_cast<btGeneric6DofSpringConstraint*>(con);
	
	// invert damping range so that 0 = no damping
	constraint->setDamping(axis, 1.0f - damping);
}

void RB_constraint_set_spring_6dof_spring(rbConstraint *con, int axis, int enable)
{
	btGeneric6DofSpringConstraint *constraint = reinterpret_cast<btGeneric6DofSpringConstraint*>(con);
	
	constraint->enableSpring(axis, enable);
}

void RB_constraint_set_equilibrium_6dof_spring(rbConstraint *con)
{
	btGeneric6DofSpringConstraint *constraint = reinterpret_cast<btGeneric6DofSpringConstraint*>(con);
	
	constraint->setEquilibriumPoint();
}

void RB_constraint_set_solver_iterations(rbConstraint *con, int num_solver_iterations)
{
	btTypedConstraint *constraint = reinterpret_cast<btTypedConstraint*>(con);
	
	constraint->setOverrideNumSolverIterations(num_solver_iterations);
}

void RB_constraint_set_breaking_threshold(rbConstraint *con, float threshold)
{
	btTypedConstraint *constraint = reinterpret_cast<btTypedConstraint*>(con);
	
	constraint->setBreakingImpulseThreshold(threshold);
}

void RB_constraint_set_enable_motor(rbConstraint *con, int enable_lin, int enable_ang)
{
	btGeneric6DofConstraint *constraint = reinterpret_cast<btGeneric6DofConstraint*>(con);
	
	constraint->getTranslationalLimitMotor()->m_enableMotor[0] = enable_lin;
	constraint->getRotationalLimitMotor(0)->m_enableMotor = enable_ang;
}

void RB_constraint_set_max_impulse_motor(rbConstraint *con, float max_impulse_lin, float max_impulse_ang)
{
	btGeneric6DofConstraint *constraint = reinterpret_cast<btGeneric6DofConstraint*>(con);
	
	constraint->getTranslationalLimitMotor()->m_maxMotorForce.setX(max_impulse_lin);
	constraint->getRotationalLimitMotor(0)->m_maxMotorForce = max_impulse_ang;
}

void RB_constraint_set_target_velocity_motor(rbConstraint *con, float velocity_lin, float velocity_ang)
{
	btGeneric6DofConstraint *constraint = reinterpret_cast<btGeneric6DofConstraint*>(con);
	
	constraint->getTranslationalLimitMotor()->m_targetVelocity.setX(velocity_lin);
	constraint->getRotationalLimitMotor(0)->m_targetVelocity = velocity_ang;
}

/* ********************************** */
