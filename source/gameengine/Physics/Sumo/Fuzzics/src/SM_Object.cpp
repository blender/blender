/**
 * $Id$
 * Copyright (C) 2001 NaN Technologies B.V.
 * The basic physics object.
 */

#ifdef WIN32
// This warning tells us about truncation of __long__ stl-generated names.
// It can occasionally cause DevStudio to have internal compiler warnings.
#pragma warning( disable : 4786 )     
#endif

#include "SM_Object.h"
#include "SM_Scene.h"
#include "SM_FhObject.h"

#include "MT_MinMax.h"


// Tweak parameters
static MT_Scalar ImpulseThreshold = 0.5;

SM_Object::SM_Object(
	DT_ShapeHandle shape, 
	const SM_MaterialProps *materialProps,
	const SM_ShapeProps *shapeProps,
	SM_Object *dynamicParent
) :
	m_shape(shape),
	m_materialProps(materialProps),
	m_materialPropsBackup(0),
	m_shapeProps(shapeProps),
	m_shapePropsBackup(0),
	m_dynamicParent(dynamicParent),
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
	m_client_object(0),
	m_fh_object(0),

	m_combined_lin_vel (0.0, 0.0, 0.0),
	m_combined_ang_vel (0.0, 0.0, 0.0)
{
	m_xform.setIdentity();
	m_xform.getValue(m_ogl_matrix);
	if (shapeProps && 
		(shapeProps->m_do_fh || shapeProps->m_do_rot_fh)) {
		MT_Vector3 ray(0.0, 0.0, -10.0);
		m_fh_object = new SM_FhObject(ray, this);
	}
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
			m_lin_vel = m_lin_mom / m_shapeProps->m_mass;
			m_ang_vel = m_ang_mom / m_shapeProps->m_inertia;
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
[
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


void SM_Object::boing(
	void *client_data,  
	void *object1,
	void *object2,
	const DT_CollData *coll_data
){
	SM_Scene  *scene = (SM_Scene *)client_data; 
	SM_Object *obj1  = (SM_Object *)object1;  
	SM_Object *obj2  = (SM_Object *)object2;  
	
	scene->addPair(obj1, obj2); // Record this collision for client callbacks
	

	// If one of the objects is a ghost then ignore it for the dynamics
	if (obj1->isGhost() || obj2->isGhost()) {
		return;
	}

	
	if (!obj2->isDynamic()) {
		std::swap(obj1, obj2);
	}

	if (!obj2->isDynamic()) {
		return;
	}

	// obj1 points to a dynamic object


	// This distinction between dynamic and non-dynamic objects should not be 
	// necessary. Non-dynamic objects are assumed to have infinite mass.

	if (obj1->isDynamic()) {
		// normal to the contact plane 
		MT_Vector3 normal = obj2->m_pos - obj1->m_pos;
		MT_Scalar  dist = normal.length(); 
		
		if (MT_EPSILON < dist && dist <= obj1->getMargin() + obj2->getMargin())
		{
			normal /= dist;
			// relative velocity of the contact points
			MT_Vector3 rel_vel       = obj2->actualLinVelocity() - obj1->actualLinVelocity();
			// relative velocity projected onto the normal
			MT_Scalar rel_vel_normal = normal.dot(rel_vel);
			
			// if the objects are approaching eachother...
			if (rel_vel_normal < 0.0) {
				MT_Scalar restitution;
				// ...and the approaching velocity is large enough... 
				if (-rel_vel_normal > ImpulseThreshold) {
					// ...this must be a true collision.
					restitution = 
						MT_min(obj1->getMaterialProps()->m_restitution,
								obj2->getMaterialProps()->m_restitution);
				}
				else {
					// otherwise, it is a resting contact, and thus the 
					// relative velocity must be made zero
					restitution = 0.0;
					// We also need to interfere with the positions of the
					// objects, otherwise they might drift into eachother. 
					MT_Vector3 penalty = normal * 
						(0.5 * (obj1->getMargin() + obj2->getMargin() - dist));
					obj1->m_pos -= penalty;
					obj2->m_pos += penalty;
				}
				
				// Compute the impulse necessary to yield the desired relative velocity...
                MT_Scalar impulse = -(1.0 + restitution) * rel_vel_normal /
                    (obj1->getInvMass() + obj2->getInvMass());
               
				// ... and apply it.
				obj1->applyCenterImpulse(-impulse * normal);
				obj2->applyCenterImpulse( impulse * normal);
            }
		}
	}
	else {
		// Same again but now obj1 is non-dynamic

		// Compute the point on obj1 closest to obj2 (= sphere with radius = 0)
		MT_Point3 local1, local2;
		MT_Scalar dist = DT_GetClosestPair(obj1->m_object, obj2->m_object,
										   local1.getValue(), local2.getValue());
		
        // local1 is th point closest to obj2
        // local2 is the local origin of obj2 

		if (MT_EPSILON < dist && dist <= obj2->getMargin()) {
			MT_Point3 world1 = obj1->getWorldCoord(local1);
			MT_Point3 world2 = obj2->getWorldCoord(local2);
			MT_Vector3 vel1  = obj1->getVelocity(local1) + obj1->m_combined_lin_vel;
			MT_Vector3 vel2  = obj2->getVelocity(local2) + obj2->m_combined_lin_vel;
            
            // the normal to the contact plane
			MT_Vector3 normal = world2 - world1; 
			normal /= dist;

			// wr2 points from obj2's origin to the global contact point
			// wr2 is only needed for rigid bodies (objects for which the 
			// friction can change the angular momentum).
			// vel2 is adapted to denote the velocity of the contact point 
			MT_Vector3 wr2;
			if (obj2->isRigidBody()) {
				wr2 = -obj2->getMargin() * normal;
				vel2 += obj2->actualAngVelocity().cross(wr2);
			}


			// This should look familiar....
			MT_Vector3 rel_vel        = vel2 - vel1;
			MT_Scalar  rel_vel_normal = normal.dot(rel_vel);
			
			if (rel_vel_normal < 0.0) {
				MT_Scalar restitution;
				if (-rel_vel_normal > ImpulseThreshold) {
					restitution = 
						MT_min(obj1->getMaterialProps()->m_restitution,
								obj2->getMaterialProps()->m_restitution);
				}
				else {
					restitution = 0.0;
					// We fix drift by moving obj2 only, since obj1 was 
					// non-dynamic.
					obj2->m_pos += normal * (obj2->getMargin() - dist);

				}
				
				MT_Scalar impulse = -(1.0 + restitution) * rel_vel_normal /
					(obj1->getInvMass() + obj2->getInvMass());


				obj2->applyCenterImpulse( impulse * normal);
	   	
// The friction part starts here!!!!!!!!

				// Compute the lateral component of the relative velocity
				// lateral actually points in the opposite direction, i.e.,
				// into the direction of the friction force.

#if 1
				// test - only do friction on the physics part of the 
				// velocity.
				vel1  -= obj1->m_combined_lin_vel;
				vel2  -= obj2->m_combined_lin_vel;

				// This should look familiar....
				rel_vel        = vel2 - vel1;
				rel_vel_normal = normal.dot(rel_vel);
#endif
				
				MT_Vector3 lateral = normal * rel_vel_normal - rel_vel;
				
				const SM_ShapeProps *shapeProps = obj2->getShapeProps();
				
				if (shapeProps->m_do_anisotropic) {

					// For anisotropic friction we scale the lateral component,
					// rather than compute a direction-dependent fricition 
					// factor. For this the lateral component is transformed to
					// local coordinates.

					MT_Matrix3x3 lcs(obj2->m_orn);
					// We cannot use m_xform.getBasis() for the matrix, since 
					// it might contain a non-uniform scaling. 
					// OPT: it's a bit daft to compute the matrix since the 
					// quaternion itself can be used to do the transformation.

					MT_Vector3 loc_lateral = lateral * lcs;
					// lcs is orthogonal so lcs.inversed() == lcs.transposed(),
					// and lcs.transposed() * lateral == lateral * lcs.

					const MT_Vector3& friction_scaling = 
						shapeProps->m_friction_scaling; 
					
					// Scale the local lateral...
					loc_lateral.scale(friction_scaling[0], 
									  friction_scaling[1], 
									  friction_scaling[2]);
					// ... and transform it back to global coordinates
					lateral = lcs * loc_lateral;
				}
				
				// A tiny Coulomb friction primer:
				// The Coulomb friction law states that the magnitude of the
				// maximum possible friction force depends linearly on the 
				// magnitude of the normal force.
				//
				// F_max_friction = friction_factor * F_normal 
				//
				// (NB: independent of the contact area!!)
				//
				// The friction factor depends on the material. 
				// We use impulses rather than forces but let us not be 
				// bothered by this. 


				MT_Scalar  rel_vel_lateral = lateral.length();
				
				if (rel_vel_lateral > MT_EPSILON) {
					lateral /= rel_vel_lateral;
					
					MT_Scalar friction_factor = 
						MT_min(obj1->getMaterialProps()->m_friction, 
								obj2->getMaterialProps()->m_friction);
					
					// Compute the maximum friction impulse

					MT_Scalar max_friction = 
						friction_factor * MT_max(0.0, impulse);
					
					// I guess the GEN_max is not necessary, so let's check it

					assert(impulse >= 0.0);

					// Here's the trick. We compute the impulse to make the
					// lateral velocity zero. (Make the objects stick together
					// at the contact point. If this impulse is larger than
					// the maximum possible friction impulse, then shrink its
					// magnitude to the maximum friction.

					if (obj2->isRigidBody()) {
						
						// For rigid bodies we take the inertia into account, 
						// since the friciton impulse is going to change the
						// angular momentum as well.
						MT_Scalar impulse_lateral = rel_vel_lateral /
							(obj1->getInvMass() + obj2->getInvMass() +
							 ((obj2->getInvInertia() * wr2.cross(lateral)).cross(wr2)).dot(lateral));
						MT_Scalar friction = MT_min(impulse_lateral, max_friction);
						obj2->applyImpulse(world2 + wr2,  friction * lateral);
					}
					else {
						MT_Scalar impulse_lateral = rel_vel_lateral /
							(obj1->getInvMass() + obj2->getInvMass());
						
						MT_Scalar friction = MT_min(impulse_lateral, max_friction);
						obj2->applyCenterImpulse( friction * lateral);
					}
				}	
				
				obj2->calcXform();
				obj2->notifyClient();		
			}
		}
	}
}



SM_Object::SM_Object(
) {
	// warning no initialization of variables done by moto.
}

SM_Object::
~SM_Object() { 
	delete m_fh_object;
	DT_DeleteObject(m_object); 
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
	m_xform.setOrigin(m_pos);
	m_xform.setBasis(MT_Matrix3x3(m_orn, m_scaling));
	m_xform.getValue(m_ogl_matrix);
	DT_SetMatrixd(m_object, m_ogl_matrix);
	if (m_fh_object) {
		m_fh_object->setPosition(m_pos);
		m_fh_object->calcXform();
	}
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
	return m_shapeProps ? 1.0 / m_shapeProps->m_mass : 0.0;
	// OPT: cache the result of this division rather than compute it each call
}

	MT_Scalar 
SM_Object::
getInvInertia(
) const { 
	return m_shapeProps ? 1.0 / m_shapeProps->m_inertia : 0.0;
	// OPT: cache the result of this division rather than compute it each call
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
		m_lin_vel           = m_lin_mom / m_shapeProps->m_mass;

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
		m_ang_vel = m_ang_mom / m_shapeProps->m_inertia;
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

	return m_prev_kinematic && !isDynamic() ? 
		(m_xform(local) - m_prev_xform(local)) / m_timeStep :
		m_lin_vel +	m_ang_vel.cross(m_xform.getBasis() * local);

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

	void  
SM_Object::
setClientObject(
	void *clientobj
) {
	m_client_object = clientobj;
}

	void *
SM_Object::
getClientObject(
){ 
	return m_client_object;
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





