/**
 * $Id$
 * Copyright (C) 2001 NaN Technologies B.V.
 * The basic physics object.
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
// This warning tells us about truncation of __long__ stl-generated names.
// It can occasionally cause DevStudio to have internal compiler warnings.
#pragma warning( disable : 4786 )     
#endif

#include "SM_Object.h"
#include "SM_Scene.h"
#include "SM_FhObject.h"
#include "SM_Debug.h"

#include "MT_MinMax.h"


// Tweak parameters
static const MT_Scalar ImpulseThreshold = -10.;
static const MT_Scalar FixThreshold = 0.01;
static const MT_Scalar FixVelocity = 0.01;
SM_Object::SM_Object(
	DT_ShapeHandle shape, 
	const SM_MaterialProps *materialProps,
	const SM_ShapeProps *shapeProps,
	SM_Object *dynamicParent)  :
	
	m_dynamicParent(dynamicParent),
	m_client_object(0),
	
	m_shape(shape),
	m_materialProps(materialProps),
	m_materialPropsBackup(0),
	m_shapeProps(shapeProps),
	m_shapePropsBackup(0),
	m_object(DT_CreateObject(this, shape)),
	m_margin(0.0),
	m_scaling(1.0, 1.0, 1.0),
	m_reaction_impulse(0.0, 0.0, 0.0),
	m_reaction_force(0.0, 0.0, 0.0),
	m_kinematic(false),
	m_prev_kinematic(false),
	m_is_rigid_body(false),
	m_lin_mom(0.0, 0.0, 0.0),
	m_ang_mom(0.0, 0.0, 0.0),
	m_force(0.0, 0.0, 0.0),
	m_torque(0.0, 0.0, 0.0),
	m_error(0.0, 0.0, 0.0),
	m_combined_lin_vel (0.0, 0.0, 0.0),
	m_combined_ang_vel (0.0, 0.0, 0.0),
	m_fh_object(0),
	m_inv_mass(0.0),
	m_inv_inertia(0., 0., 0.)
{
	m_xform.setIdentity();
	m_xform.getValue(m_ogl_matrix);
	if (shapeProps)
	{ 
		if (shapeProps->m_do_fh || shapeProps->m_do_rot_fh) 
		{
			DT_Vector3 zero = {0., 0., 0.}, ray = {0.0, 0.0, -10.0};
			m_fh_object = new SM_FhObject(DT_NewLineSegment(zero, ray), MT_Vector3(ray), this);
			//printf("SM_Object:: WARNING! fh disabled.\n");
		}
		m_inv_mass = 1. / shapeProps->m_mass;
		m_inv_inertia = MT_Vector3(1./shapeProps->m_inertia[0], 1./shapeProps->m_inertia[1], 1./shapeProps->m_inertia[2]);
	}
	updateInvInertiaTensor();
	m_suspended = false;
}

	void 
SM_Object::
integrateForces(
	MT_Scalar timeStep
){
	if (!m_suspended) {
		m_prev_state = *this;
		m_prev_state.setLinearVelocity(actualLinVelocity());
		m_prev_state.setAngularVelocity(actualAngVelocity());
		if (isDynamic()) {
			// Integrate momentum (forward Euler)
			m_lin_mom += m_force * timeStep;
			m_ang_mom += m_torque * timeStep;
			// Drain momentum because of air/water resistance
			m_lin_mom *= pow(m_shapeProps->m_lin_drag, timeStep);
			m_ang_mom *= pow(m_shapeProps->m_ang_drag, timeStep);
			// Set velocities according momentum
			m_lin_vel = m_lin_mom * m_inv_mass;
			m_ang_vel = m_inv_inertia_tensor * m_ang_mom;
		}
	}	

};

	void 
SM_Object::
integrateMomentum(
	MT_Scalar timeStep
){
	// Integrate position and orientation

	// only do it for objects with linear and/or angular velocity
	// else clients with hierarchies may get into trouble
	if (!actualLinVelocity().fuzzyZero() || !actualAngVelocity().fuzzyZero()) 
	{

	// those MIDPOINT and BACKWARD integration methods are
	// in this form not ok with some testfiles ! 
	// For a release build please use forward euler unless completely tested

//#define MIDPOINT
//#define BACKWARD
#ifdef  MIDPOINT
// Midpoint rule
		m_pos += (m_prev_state.getLinearVelocity() + actualLinVelocity()) * (timeStep * 0.5);
		m_orn += (m_prev_state.getAngularVelocity() * m_prev_state.getOrientation() + actualAngVelocity() * m_orn) * (timeStep * 0.25);
#elif defined BACKWARD
// Backward Euler
		m_pos += actualLinVelocity() * timeStep;
		m_orn += actualAngVelocity() * m_orn * (timeStep * 0.5);
#else 
// Forward Euler

		m_pos += m_prev_state.getLinearVelocity() * timeStep;
		m_orn += m_prev_state.getAngularVelocity() * m_orn * (timeStep * 0.5);
#endif
		m_orn.normalize(); // I might not be necessary to do this every call

		calcXform();
		notifyClient();		

	}
}

/**
 * dynamicCollision computes the response to a collision.
 *
 * @param local2 the contact point in local coordinates.
 * @param normal the contact normal.
 * @param dist the penetration depth of the contact. (unused)
 * @param rel_vel the relative velocity of the objects
 * @param restitution the amount of momentum conserved in the collision. Range: 0.0 - 1.0
 * @param friction_factor the amount of friction between the two surfaces.
 * @param invMass the inverse mass of the collision objects (1.0 / mass)
 */
void SM_Object::dynamicCollision(const MT_Point3 &local2, 
	const MT_Vector3 &normal, 
	MT_Scalar dist, 
	const MT_Vector3 &rel_vel,
	MT_Scalar restitution,
	MT_Scalar friction_factor,
	MT_Scalar invMass
)
{
	/**
	 * rel_vel_normal is the relative velocity in the contact normal direction.
	 */
	MT_Scalar  rel_vel_normal = normal.dot(rel_vel);
			
	/**
	 * if rel_vel_normal > 0, the objects are moving apart! 
	 */
	if (rel_vel_normal < 0.) {
		/**
		 * if rel_vel_normal < ImpulseThreshold, scale the restitution down.
		 * This should improve the simulation where the object is stacked.
		 */
		restitution *= MT_min(MT_Scalar(1.0), rel_vel_normal/ImpulseThreshold);
				
		MT_Scalar impulse = -(1.0 + restitution) * rel_vel_normal;
		
		if (isRigidBody())
		{
			MT_Vector3 temp = getInvInertiaTensor() * local2.cross(normal);
			impulse /= invMass + normal.dot(temp.cross(local2));
			
			/**
			 * Apply impulse at the collision point.
			 * Take rotational inertia into account.
			 */
			applyImpulse(local2 + m_pos, impulse * normal);
		} else {
			/**
			 * Apply impulse through object centre. (no rotation.)
			 */
			impulse /= invMass;
			applyCenterImpulse( impulse * normal ); 
		}
	   	
#if 0
		// test - only do friction on the physics part of the 
		// velocity.
		vel1  -= obj1->m_combined_lin_vel;
		vel2  -= obj2->m_combined_lin_vel;

		// This should look familiar....
		rel_vel        = vel2 - vel1;
		rel_vel_normal = normal.dot(rel_vel);
#endif
		/**
		 * The friction part starts here!!!!!!!!
                 *
		 * Compute the lateral component of the relative velocity
		 * lateral actually points in the opposite direction, i.e.,
		 * into the direction of the friction force.
		 */
		MT_Vector3 lateral =  rel_vel - normal * rel_vel_normal;
		if (m_shapeProps->m_do_anisotropic) {

			/**
			 * For anisotropic friction we scale the lateral component,
			 * rather than compute a direction-dependent fricition 
			 * factor. For this the lateral component is transformed to
			 * local coordinates.
			 */

			MT_Matrix3x3 lcs(m_orn);
			
			/**
			 * We cannot use m_xform.getBasis() for the matrix, since 
			 * it might contain a non-uniform scaling. 
			 * OPT: it's a bit daft to compute the matrix since the 
			 * quaternion itself can be used to do the transformation.
			 */
			MT_Vector3 loc_lateral = lateral * lcs;
			
			/**
			 * lcs is orthogonal so lcs.inversed() == lcs.transposed(),
			 * and lcs.transposed() * lateral == lateral * lcs.
			 */
			const MT_Vector3& friction_scaling = 
				m_shapeProps->m_friction_scaling; 

			// Scale the local lateral...
			loc_lateral.scale(friction_scaling[0], 
							friction_scaling[1], 
							friction_scaling[2]);
			// ... and transform it back to global coordinates
			lateral = lcs * loc_lateral;
		}
			
		/**
		 * A tiny Coulomb friction primer:
		 * The Coulomb friction law states that the magnitude of the
		 * maximum possible friction force depends linearly on the 
		 * magnitude of the normal force.
		 *
		 * \f[
		     F_max_friction = friction_factor * F_normal 
		   \f]
		 *
		 * (NB: independent of the contact area!!)
		 *
		 * The friction factor depends on the material. 
		 * We use impulses rather than forces but let us not be 
		 * bothered by this. 
		 */
		MT_Scalar  rel_vel_lateral = lateral.length();

		if (rel_vel_lateral > MT_EPSILON) {
			lateral /= rel_vel_lateral;

			// Compute the maximum friction impulse
			MT_Scalar max_friction = 
				friction_factor * MT_max(MT_Scalar(0.0), impulse);

			// I guess the GEN_max is not necessary, so let's check it

			assert(impulse >= 0.0);

			/**
			 * Here's the trick. We compute the impulse to make the
			 * lateral velocity zero. (Make the objects stick together
			 * at the contact point. If this impulse is larger than
			 * the maximum possible friction impulse, then shrink its
			 * magnitude to the maximum friction.
			 */

			if (isRigidBody()) {
					
				/**
				 * For rigid bodies we take the inertia into account, 
				 * since the friction impulse is going to change the
				 * angular momentum as well.
				 */
				MT_Vector3 temp = getInvInertiaTensor() * local2.cross(lateral);
				MT_Scalar impulse_lateral = rel_vel_lateral /
					(invMass + lateral.dot(temp.cross(local2)));

				MT_Scalar friction = MT_min(impulse_lateral, max_friction);
				applyImpulse(local2 + m_pos, -lateral * friction);
			}
			else {
				MT_Scalar impulse_lateral = rel_vel_lateral / invMass;

				MT_Scalar friction = MT_min(impulse_lateral, max_friction);
				applyCenterImpulse( -friction * lateral);
			}
				

		}	

		//calcXform();
		//notifyClient();

	}
}

DT_Bool SM_Object::boing(
	void *client_data,  
	void *object1,
	void *object2,
	const DT_CollData *coll_data
){
	//if (!coll_data)
	//	return DT_CONTINUE;

	SM_Scene  *scene = (SM_Scene *)client_data; 
	SM_Object *obj1  = (SM_Object *)object1;  
	SM_Object *obj2  = (SM_Object *)object2;  
	
	scene->addPair(obj1, obj2); // Record this collision for client callbacks
	
	// If one of the objects is a ghost then ignore it for the dynamics
	if (obj1->isGhost() || obj2->isGhost()) {
		return DT_CONTINUE;
	}

	// Objects do not collide with parent objects
	if (obj1->getDynamicParent() == obj2 || obj2->getDynamicParent() == obj1) {
		return DT_CONTINUE;
	}
	
	if (!obj2->isDynamic()) {
		std::swap(obj1, obj2);
	}

	if (!obj2->isDynamic()) {
		return DT_CONTINUE;
	}

	// Get collision data from SOLID
	DT_Vector3 p1, p2;
	if (!DT_GetPenDepth(obj1->getObjectHandle(), obj2->getObjectHandle(), p1, p2))
		return DT_CONTINUE;
	MT_Point3 local1(p1), local2(p2);
	MT_Vector3 normal(local2 - local1);
	MT_Scalar dist = normal.length();
	
	if (dist < MT_EPSILON)
		return DT_CONTINUE;
	
	local1 -= obj1->m_pos;
	local2 -= obj2->m_pos;
	
	// Calculate collision parameters
	MT_Vector3 rel_vel        = obj1->getVelocity(local1) - obj2->getVelocity(local2);
	
	MT_Scalar restitution = 
		MT_min(obj1->getMaterialProps()->m_restitution,
				obj2->getMaterialProps()->m_restitution);
	
	MT_Scalar friction_factor = 
		MT_min(obj1->getMaterialProps()->m_friction, 
				obj2->getMaterialProps()->m_friction);
				
	MT_Scalar invMass = obj1->getInvMass() + obj2->getInvMass();
	
	normal /= dist;
	
	// Calculate reactions
	if (obj1->isDynamic())
		obj1->dynamicCollision(local1, normal, dist, rel_vel, restitution, friction_factor, invMass);
		
	if (obj2->isDynamic())
		obj2->dynamicCollision(local2, -normal, dist, -rel_vel, restitution, friction_factor, invMass);
	
	return DT_CONTINUE;
}

DT_Bool SM_Object::fix(
	void *client_data,
	void *object1,
	void *object2,
	const DT_CollData *coll_data
){
	SM_Scene  *scene = (SM_Scene *)client_data; 
	SM_Object *obj1  = (SM_Object *)object1;  
	SM_Object *obj2  = (SM_Object *)object2;  
	
	// If one of the objects is a ghost then ignore it for the dynamics
	if (obj1->isGhost() || obj2->isGhost()) {
		return DT_CONTINUE;
	}

	if (obj1->getDynamicParent() == obj2 || obj2->getDynamicParent() == obj1) {
		return DT_CONTINUE;
	}
	
	if (!obj2->isDynamic()) {
		std::swap(obj1, obj2);
	}

	if (!obj2->isDynamic()) {
		return DT_CONTINUE;
	}

	// obj1 points to a dynamic object
	DT_Vector3 p1, p2;
	if (!DT_GetPenDepth(obj1->getObjectHandle(), obj2->getObjectHandle(), p1, p2))
		return DT_CONTINUE;
	MT_Point3 local1(p1), local2(p2);
	// Get collision data from SOLID
	MT_Vector3 normal(local2 - local1);
	
	if (normal.dot(normal) < MT_EPSILON)
		return DT_CONTINUE;

	// This distinction between dynamic and non-dynamic objects should not be 
	// necessary. Non-dynamic objects are assumed to have infinite mass.
	if (obj1->isDynamic()) {
		MT_Vector3 error = normal * 0.5f;
		obj1->m_error += error;
		obj2->m_error -= error;
		// Remove the velocity component in the normal direction
		// Calculate collision parameters
		/*MT_Vector3 rel_vel = obj1->getLinearVelocity() - obj2->getLinearVelocity();
		if (normal.length() > FixThreshold && rel_vel.length() < FixVelocity) {
			normal.normalize();
			MT_Scalar  rel_vel_normal = 0.49*(normal.dot(rel_vel));

			obj1->addLinearVelocity(-rel_vel_normal*normal);
			obj2->addLinearVelocity(rel_vel_normal*normal);
		}*/
	}
	else {
		// Same again but now obj1 is non-dynamic
		obj2->m_error -= normal;
		/*MT_Vector3 rel_vel = obj2->getLinearVelocity();
		if (normal.length() > FixThreshold && rel_vel.length() < FixVelocity) {
			// Calculate collision parameters
			normal.normalize();
			MT_Scalar  rel_vel_normal = -0.99*(normal.dot(rel_vel));

			obj2->addLinearVelocity(rel_vel_normal*normal);
		}*/ 
	}
	
	return DT_CONTINUE;
}

void SM_Object::relax(void)
{ 
	if (m_error.fuzzyZero())
		return;
	//std::cout << "SM_Object::relax: { " << m_error << " }" << std::endl;
	
	m_pos += m_error; 
	m_error.setValue(0., 0., 0.); 
/*	m_pos.getValue(pos);
	DT_SetPosition(m_object, pos); 
	m_xform.setOrigin(m_pos); 
	m_xform.getValue(m_ogl_matrix); */
	calcXform();
	notifyClient();
}
	
SM_Object::SM_Object() :
	m_dynamicParent(0),
	m_client_object(0),
	
	m_shape(0),
	m_materialProps(0),
	m_materialPropsBackup(0),
	m_shapeProps(0),
	m_shapePropsBackup(0),
	m_object(0),
	m_margin(0.0),
	m_scaling(1.0, 1.0, 1.0),
	m_reaction_impulse(0.0, 0.0, 0.0),
	m_reaction_force(0.0, 0.0, 0.0),
	m_kinematic(false),
	m_prev_kinematic(false),
	m_is_rigid_body(false),
	m_lin_mom(0.0, 0.0, 0.0),
	m_ang_mom(0.0, 0.0, 0.0),
	m_force(0.0, 0.0, 0.0),
	m_torque(0.0, 0.0, 0.0),
	m_error(0.0, 0.0, 0.0),
	m_combined_lin_vel (0.0, 0.0, 0.0),
	m_combined_ang_vel (0.0, 0.0, 0.0),
	m_fh_object(0) 
{
	// warning no initialization of variables done by moto.
}

SM_Object::
~SM_Object() { 
	if (m_fh_object)
		delete m_fh_object;
	
	DT_DestroyObject(m_object);
	m_object = NULL;
}

	bool 
SM_Object::
isDynamic(
) const {
	return m_shapeProps != 0; 
} 

/* nzc experimental. There seem to be two places where kinematics
 * are evaluated: proceedKinematic (called from SM_Scene) and
 * proceed() in this object. I'll just try and bunge these out for
 * now.  */
	void 
SM_Object::
suspend(
){
	if (!m_suspended) {
		m_suspended = true;
		suspendDynamics();
	}
}

	void 
SM_Object::
resume(
) {
	if (m_suspended) {
		m_suspended = false;
		restoreDynamics();
	}
}

	void 
SM_Object::
suspendDynamics(
) {
	if (m_shapeProps) {
		m_shapePropsBackup = m_shapeProps;
		m_shapeProps = 0;
	}
}

	void 
SM_Object::
restoreDynamics(
) {
	if (m_shapePropsBackup) {
		m_shapeProps = m_shapePropsBackup;
		m_shapePropsBackup = 0;
	}
}

	bool 
SM_Object::
isGhost(
) const {
	return m_materialProps == 0;
} 

	void 
SM_Object::
suspendMaterial(
) {
	if (m_materialProps) {
		m_materialPropsBackup = m_materialProps;
		m_materialProps = 0;
	}
}

	void 
SM_Object::
restoreMaterial(
) {
	if (m_materialPropsBackup) {
		m_materialProps = m_materialPropsBackup;
		m_materialPropsBackup = 0;
	}
}

	SM_FhObject *
SM_Object::
getFhObject(
) const {
	return m_fh_object;
} 

	void 
SM_Object::
registerCallback(
	SM_Callback& callback
) {
	m_callbackList.push_back(&callback);
}

// Set the local coordinate system according to the current state 
	void 
SM_Object::
calcXform() {
#ifdef SM_DEBUG_XFORM
	printf("SM_Object::calcXform m_pos = { %-0.5f, %-0.5f, %-0.5f }\n",
		m_pos[0], m_pos[1], m_pos[2]);
	printf("                     m_orn = { %-0.5f, %-0.5f, %-0.5f, %-0.5f }\n",
		m_orn[0], m_orn[1], m_orn[2], m_orn[3]);
	printf("                 m_scaling = { %-0.5f, %-0.5f, %-0.5f }\n",
		m_scaling[0], m_scaling[1], m_scaling[2]);
#endif
	m_xform.setOrigin(m_pos);
	m_xform.setBasis(MT_Matrix3x3(m_orn, m_scaling));
	m_xform.getValue(m_ogl_matrix);
	
	/* Blender has been known to crash here.
	   This usually means SM_Object *this has been deleted more than once. */
	DT_SetMatrixd(m_object, m_ogl_matrix);
	if (m_fh_object) {
		m_fh_object->setPosition(m_pos);
		m_fh_object->calcXform();
	}
	updateInvInertiaTensor();
#ifdef SM_DEBUG_XFORM
	printf("\n               | %-0.5f %-0.5f %-0.5f %-0.5f |\n",
		m_ogl_matrix[0], m_ogl_matrix[4], m_ogl_matrix[ 8], m_ogl_matrix[12]);
	printf(  "               | %-0.5f %-0.5f %-0.5f %-0.5f |\n",
		m_ogl_matrix[1], m_ogl_matrix[5], m_ogl_matrix[ 9], m_ogl_matrix[13]);
	printf(  "m_ogl_matrix = | %-0.5f %-0.5f %-0.5f %-0.5f |\n",
		m_ogl_matrix[2], m_ogl_matrix[6], m_ogl_matrix[10], m_ogl_matrix[14]);
	printf(  "               | %-0.5f %-0.5f %-0.5f %-0.5f |\n\n",
		m_ogl_matrix[3], m_ogl_matrix[7], m_ogl_matrix[11], m_ogl_matrix[15]);
#endif
}

	void 
SM_Object::updateInvInertiaTensor() 
{
	m_inv_inertia_tensor = m_xform.getBasis().scaled(m_inv_inertia[0], m_inv_inertia[1], m_inv_inertia[2]) * m_xform.getBasis().transposed();
}

// Call callbacks to notify the client of a change of placement
	void 
SM_Object::
notifyClient() {
	T_CallbackList::iterator i;
	for (i = m_callbackList.begin(); i != m_callbackList.end(); ++i) {
		(*i)->do_me();
	}
}


// Save the current state information for use in the velocity computation in the next frame.  
	void 
SM_Object::
proceedKinematic(
	MT_Scalar timeStep
) {
	/* nzc: need to bunge this for the logic bubbling as well? */
	if (!m_suspended) {
		m_prev_kinematic = m_kinematic;              
		if (m_kinematic) {
			m_prev_xform = m_xform;
			m_timeStep = timeStep;
			calcXform();
			m_kinematic  = false;
		}
	}
}

	void 
SM_Object::
saveReactionForce(
	MT_Scalar timeStep
) {
	if (isDynamic()) {
		m_reaction_force   = m_reaction_impulse / timeStep;
		m_reaction_impulse.setValue(0.0, 0.0, 0.0);
	}
}

	void 
SM_Object::
clearForce(
) {
	m_force.setValue(0.0, 0.0, 0.0);
	m_torque.setValue(0.0, 0.0, 0.0);
}

	void 
SM_Object::
clearMomentum(
) {
	m_lin_mom.setValue(0.0, 0.0, 0.0);
	m_ang_mom.setValue(0.0, 0.0, 0.0);
}

	void 
SM_Object::
setMargin(
	MT_Scalar margin
) {
	m_margin = margin;
	DT_SetMargin(m_object, margin);
}

	MT_Scalar 
SM_Object::
getMargin(
) const {
    return m_margin;
}

const 
	SM_MaterialProps *
SM_Object::
getMaterialProps(
) const {
    return m_materialProps;
}

const 
	SM_ShapeProps *
SM_Object::
getShapeProps(
) const {
    return m_shapeProps;
}

	void 
SM_Object::
setPosition(
	const MT_Point3& pos
){
	m_kinematic = true;
	m_pos = pos;
}

	void 
SM_Object::
setOrientation(
	const MT_Quaternion& orn
){
	assert(!orn.fuzzyZero());
	m_kinematic = true;
	m_orn = orn;
}

	void 
SM_Object::
setScaling(
	const MT_Vector3& scaling
){
	m_kinematic = true;
	m_scaling = scaling;
}

/**
 * Functions to handle linear velocity
 */

	void 
SM_Object::
setExternalLinearVelocity(
	const MT_Vector3& lin_vel
) {
	m_combined_lin_vel=lin_vel;
}

	void 
SM_Object::
addExternalLinearVelocity(
	const MT_Vector3& lin_vel
) {
	m_combined_lin_vel+=lin_vel;
}

	void 
SM_Object::
addLinearVelocity(
	const MT_Vector3& lin_vel
){
	m_lin_vel += lin_vel;
	if (m_shapeProps) {
		m_lin_mom = m_lin_vel * m_shapeProps->m_mass;
	}
}

	void 
SM_Object::
setLinearVelocity(
	const MT_Vector3& lin_vel
){
	m_lin_vel = lin_vel;
	if (m_shapeProps) {
		m_lin_mom = m_lin_vel * m_shapeProps->m_mass;
	}
}

/**
 * Functions to handle angular velocity
 */

	void 
SM_Object::
setExternalAngularVelocity(
	const MT_Vector3& ang_vel
) {
	m_combined_ang_vel = ang_vel;
}

	void
SM_Object::
addExternalAngularVelocity(
	const MT_Vector3& ang_vel
) {
	m_combined_ang_vel += ang_vel;
}

	void 
SM_Object::
setAngularVelocity(
	const MT_Vector3& ang_vel
) {
	m_ang_vel = ang_vel;
	if (m_shapeProps) {
		m_ang_mom = m_ang_vel * m_shapeProps->m_inertia;
	}
}

	void
SM_Object::
addAngularVelocity(
	const MT_Vector3& ang_vel
) {
	m_ang_vel += ang_vel;
	if (m_shapeProps) {
		m_ang_mom = m_ang_vel * m_shapeProps->m_inertia;
	}
}


	void 
SM_Object::
clearCombinedVelocities(
) {
	m_combined_lin_vel = MT_Vector3(0,0,0);
	m_combined_ang_vel = MT_Vector3(0,0,0);
}

	void 
SM_Object::
resolveCombinedVelocities(
	const MT_Vector3 & lin_vel,
	const MT_Vector3 & ang_vel
) {

	// Different behaviours for dynamic and non-dynamic 
	// objects. For non-dynamic we just set the velocity to 
	// zero. For dynmic the physics velocity has to be 
	// taken into account. We must make an arbitrary decision
	// on how to resolve the 2 velocities. Choices are
	// Add the physics velocity to the linear velocity. Objects
	// will just keep on moving in the direction they were
	// last set in - untill external forces affect them.
	// Set the combinbed linear and physics velocity to zero.
	// Set the physics velocity in the direction of the set velocity
	// zero.
	if (isDynamic()) {		

#if 1
		m_lin_vel += lin_vel;
		m_ang_vel += ang_vel;
#else

		//compute the component of the physics velocity in the 
		// direction of the set velocity and set it to zero.
		MT_Vector3 lin_vel_norm = lin_vel.normalized();

		m_lin_vel -= (m_lin_vel.dot(lin_vel_norm) * lin_vel_norm);
#endif
		m_lin_mom = m_lin_vel * m_shapeProps->m_mass;
		m_ang_mom = m_ang_vel * m_shapeProps->m_inertia;
		clearCombinedVelocities();

	}

}		


	MT_Scalar 
SM_Object::
getInvMass(
) const { 
	return m_inv_mass;
	// OPT: cache the result of this division rather than compute it each call
}

	const MT_Vector3&
SM_Object::
getInvInertia(
) const { 
	return m_inv_inertia;
	// OPT: cache the result of this division rather than compute it each call
}

	const MT_Matrix3x3&
SM_Object::
getInvInertiaTensor(
) const { 
	return m_inv_inertia_tensor; 
}

	void 
SM_Object::
applyForceField(
	const MT_Vector3& accel
) {
	if (m_shapeProps) {
		m_force += m_shapeProps->m_mass * accel;  // F = m * a
	}
}

	void 
SM_Object::
applyCenterForce(
	const MT_Vector3& force
) {
	m_force += force;
}

	void 
SM_Object::
applyTorque(
	const MT_Vector3& torque
) {
	m_torque += torque;
}

	void 
SM_Object::
applyImpulse(
	const MT_Point3& attach, const MT_Vector3& impulse
) {
	applyCenterImpulse(impulse);                          // Change in linear momentum
	applyAngularImpulse((attach - m_pos).cross(impulse)); // Change in angular momentump
}

	void 
SM_Object::
applyCenterImpulse(
	const MT_Vector3& impulse
) {
	if (m_shapeProps) {
		m_lin_mom          += impulse;
		m_reaction_impulse += impulse;
		m_lin_vel           = m_lin_mom * m_inv_mass;

		// The linear velocity is immedialtely updated since otherwise
		// simultaneous collisions will get a double impulse. 
	}
}

	void 
SM_Object::
applyAngularImpulse(
	const MT_Vector3& impulse
) {
	if (m_shapeProps) {
		m_ang_mom += impulse;
		m_ang_vel = m_inv_inertia_tensor * m_ang_mom;
	}
}

	MT_Point3 
SM_Object::
getWorldCoord(
	const MT_Point3& local
) const {
    return m_xform(local);
}

	MT_Vector3 
SM_Object::
getVelocity(
	const MT_Point3& local
) const {
	// For displaced objects the velocity is faked using the previous state. 
	// Dynamic objects get their own velocity, not the faked velocity.
	// (Dynamic objects shouldn't be displaced in the first place!!)
/* FIXME: -KM- Valgrind report:
==17624== Use of uninitialised value of size 8
==17624==    at 0x831F925: MT_Vector3::dot(MT_Vector3 const&) const (MT_Tuple3.h:60)
==17624==    by 0x82E4574: SM_Object::getVelocity(MT_Point3 const&) const (MT_Matrix3x3.h:81)
==17624==    by 0x82E324D: SM_Object::boing(void*, void*, void*, DT_CollData const*) (SM_Object.cpp:319)
==17624==    by 0x83E7308: DT_Encounter::exactTest(DT_RespTable const*, int&) const (in /home/kester/blender-src/DEBUG/blender)
*/
	return m_prev_kinematic && !isDynamic() ? 
		(m_xform(local) - m_prev_xform(local)) / m_timeStep :
		actualLinVelocity() +	actualAngVelocity().cross(local);

		//m_lin_vel +	m_ang_vel.cross(m_xform.getBasis() * local);
	// NB: m_xform.getBasis() * local == m_xform(local) - m_xform.getOrigin()

}


const 
	MT_Vector3& 
SM_Object::
getReactionForce(
) const {
	return m_reaction_force;
}

	void 
SM_Object::
getMatrix(
	double *m
) const {
    std::copy(&m_ogl_matrix[0], &m_ogl_matrix[16], &m[0]);
}

const 
	double *
SM_Object::
getMatrix(
) const {
	return m_ogl_matrix;
}

// Still need this???
const 
	MT_Transform&  
SM_Object::
getScaledTransform(
) const {
	return m_xform;
}

	DT_ObjectHandle 
SM_Object::
getObjectHandle(
) const {
	return m_object;
}

	DT_ShapeHandle 
SM_Object::
getShapeHandle(
) const { 
	return m_shape;
}

	SM_Object *
SM_Object::
getDynamicParent(
) {
	return m_dynamicParent;
}

	void 
SM_Object::
setRigidBody(
	bool is_rigid_body
) { 
	m_is_rigid_body = is_rigid_body;
} 

	bool 
SM_Object::
isRigidBody(
) const {
	return m_is_rigid_body;
}

const 
	MT_Vector3
SM_Object::
actualLinVelocity(
) const {
	return m_combined_lin_vel + m_lin_vel;
};

const 
	MT_Vector3
SM_Object::
actualAngVelocity(
) const {
	return m_combined_ang_vel + m_ang_vel;
};





