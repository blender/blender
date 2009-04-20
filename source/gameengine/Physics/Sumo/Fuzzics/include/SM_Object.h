/**
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef SM_OBJECT_H
#define SM_OBJECT_H

#include <vector>

#include <SOLID/SOLID.h>

#include "SM_Callback.h"
#include "SM_MotionState.h"
#include <stdio.h>

class SM_FhObject;

/** Properties of dynamic objects */
struct SM_ShapeProps {
	MT_Scalar  m_mass;                  ///< Total mass
	MT_Scalar  m_radius;                ///< Bound sphere size
	MT_Vector3 m_inertia;               ///< Inertia, should be a tensor some time 
	MT_Scalar  m_lin_drag;              ///< Linear drag (air, water) 0 = concrete, 1 = vacuum 
	MT_Scalar  m_ang_drag;              ///< Angular drag
	MT_Scalar  m_friction_scaling[3];   ///< Scaling for anisotropic friction. Component in range [0, 1]   
	bool       m_do_anisotropic;        ///< Should I do anisotropic friction? 
	bool       m_do_fh;                 ///< Should the object have a linear Fh spring?
	bool       m_do_rot_fh;             ///< Should the object have an angular Fh spring?
};


/** Properties of collidable objects (non-ghost objects) */
struct SM_MaterialProps {
	MT_Scalar m_restitution;           ///< restitution of energy after a collision 0 = inelastic, 1 = elastic
	MT_Scalar m_friction;              ///< Coulomb friction (= ratio between the normal en maximum friction force)
	MT_Scalar m_fh_spring;             ///< Spring constant (both linear and angular)
	MT_Scalar m_fh_damping;            ///< Damping factor (linear and angular) in range [0, 1]
	MT_Scalar m_fh_distance;           ///< The range above the surface where Fh is active.    
	bool      m_fh_normal;             ///< Should the object slide off slopes?
};

class SM_ClientObject
{
public:
	SM_ClientObject() {}
	virtual ~SM_ClientObject() {}
	
	virtual bool hasCollisionCallback() = 0;
};

/**
 * SM_Object is an internal part of the Sumo physics engine.
 *
 * It encapsulates an object in the physics scene, and is responsible
 * for calculating the collision response of objects.
 */
class SM_Object 
{
public:
	SM_Object() ;
	SM_Object(
		DT_ShapeHandle shape, 
		const SM_MaterialProps *materialProps,
		const SM_ShapeProps *shapeProps,
		SM_Object *dynamicParent
	);
	virtual ~SM_Object();

	bool isDynamic() const;  

	/* nzc experimental. There seem to be two places where kinematics
	 * are evaluated: proceedKinematic (called from SM_Scene) and
	 * proceed() in this object. I'll just try and bunge these out for
	 * now.  */

	void suspend(void);
	void resume(void);

	void suspendDynamics();
	
	void restoreDynamics();
	
	bool isGhost() const;

	void suspendMaterial();
	
	void restoreMaterial();
	
	SM_FhObject *getFhObject() const;
	
	void registerCallback(SM_Callback& callback);

	void calcXform();
	void notifyClient();
	void updateInvInertiaTensor();

    
	// Save the current state information for use in the 
	// velocity computation in the next frame.  

	void proceedKinematic(MT_Scalar timeStep);

	void saveReactionForce(MT_Scalar timeStep) ;
	
	void clearForce() ;

	void clearMomentum() ;
	
	void setMargin(MT_Scalar margin) ;
	
	MT_Scalar getMargin() const ;
	
	const SM_MaterialProps *getMaterialProps() const ;
	
	const SM_ShapeProps *getShapeProps() const ;
	
	void setPosition(const MT_Point3& pos);
	void setOrientation(const MT_Quaternion& orn);
	void setScaling(const MT_Vector3& scaling);
	
	/**
	 * set an external velocity. This velocity complements
	 * the physics velocity. So setting it does not override the
	 * physics velocity. It is your responsibility to clear 
	 * this external velocity. This velocity is not subject to 
	 * friction or damping.
	 */
	void setExternalLinearVelocity(const MT_Vector3& lin_vel) ;
	void addExternalLinearVelocity(const MT_Vector3& lin_vel) ;

	/** Override the physics velocity */
	void addLinearVelocity(const MT_Vector3& lin_vel);
	void setLinearVelocity(const MT_Vector3& lin_vel);

	/**
	 * Set an external angular velocity. This velocity complemetns
	 * the physics angular velocity so does not override it. It is
	 * your responsibility to clear this velocity. This velocity
	 * is not subject to friction or damping.
	 */
	void setExternalAngularVelocity(const MT_Vector3& ang_vel) ;
	void addExternalAngularVelocity(const MT_Vector3& ang_vel);

	/** Override the physics angular velocity */
	void addAngularVelocity(const MT_Vector3& ang_vel);
	void setAngularVelocity(const MT_Vector3& ang_vel);

	/** Clear the external velocities */
	void clearCombinedVelocities();

	/** 
	 * Tell the physics system to combine the external velocity
	 * with the physics velocity. 
	 */
	void resolveCombinedVelocities(
		const MT_Vector3 & lin_vel,
		const MT_Vector3 & ang_vel
	) ;



	MT_Scalar getInvMass() const;

	const MT_Vector3& getInvInertia() const ;
	
	const MT_Matrix3x3& getInvInertiaTensor() const;

	void applyForceField(const MT_Vector3& accel) ;
	
	void applyCenterForce(const MT_Vector3& force) ;
	
	void applyTorque(const MT_Vector3& torque) ;
	
	/**
	 * Apply an impulse to the object.  The impulse will be split into
	 * angular and linear components.
	 * @param attach point to apply the impulse to (in world coordinates)
	 */
	void applyImpulse(const MT_Point3& attach, const MT_Vector3& impulse) ;
	
	/**
	 * Applies an impulse through the center of this object. (ie the angular
	 * velocity will not change.
	 */
	void applyCenterImpulse(const MT_Vector3& impulse);
	/**
	 * Applies an angular impulse.
	 */
	void applyAngularImpulse(const MT_Vector3& impulse);
	
	MT_Point3 getWorldCoord(const MT_Point3& local) const;
	MT_Point3 getLocalCoord(const MT_Point3& world) const;
    
	MT_Vector3 getVelocity(const MT_Point3& local) const;


	const MT_Vector3& getReactionForce() const ;

	void getMatrix(double *m) const ;

	const double *getMatrix() const ;

	// Still need this???
	const MT_Transform&  getScaledTransform()  const; 

	DT_ObjectHandle getObjectHandle() const ;
	DT_ShapeHandle getShapeHandle() const ;

	SM_Object *getDynamicParent() ;

	void integrateForces(MT_Scalar timeStep);
	void integrateMomentum(MT_Scalar timeSteo);

	void setRigidBody(bool is_rigid_body) ;

	bool isRigidBody() const ;

	// This is the callback for handling collisions of dynamic objects
	static 
		DT_Bool 
	boing(
		void *client_data,  
		void *object1,
		void *object2,
		const DT_CollData *coll_data
	);

	static 
		DT_Bool 
	fix(
		void *client_data,  
		void *object1,
		void *object2,
		const DT_CollData *coll_data
	);
	
	
	SM_ClientObject *getClientObject() { return m_client_object; }
	void setClientObject(SM_ClientObject *client_object) { m_client_object = client_object; }
	void	setPhysicsClientObject(void* physicsClientObject)
	{
		m_physicsClientObject = physicsClientObject;
	}
	void*	getPhysicsClientObject() {
		return m_physicsClientObject;
	}
	void relax();
	
	SM_MotionState &getCurrentFrame();
	SM_MotionState &getPreviousFrame();
	SM_MotionState &getNextFrame();
	
	const SM_MotionState &getCurrentFrame()   const;
	const SM_MotionState &getPreviousFrame()  const;
	const SM_MotionState &getNextFrame()      const;

	// Motion state functions	
	const MT_Point3&     getPosition()        const;
	const MT_Quaternion& getOrientation()     const;
	const MT_Vector3&    getLinearVelocity()  const;
	const MT_Vector3&    getAngularVelocity() const;
	
	MT_Scalar            getTime()            const;
	
	void setTime(MT_Scalar time);
	
	void interpolate(MT_Scalar timeStep);
	void endFrame();
	
private:
	friend class Contact;
	// Tweak parameters
	static MT_Scalar ImpulseThreshold;

	// return the actual linear_velocity of this object this 
	// is the addition of m_combined_lin_vel and m_lin_vel.

	const 
		MT_Vector3
	actualLinVelocity(
	) const ;

	const 
		MT_Vector3
	actualAngVelocity(
	) const ;
	
	void dynamicCollision(const MT_Point3 &local2, 
		const MT_Vector3 &normal, 
		MT_Scalar dist, 
		const MT_Vector3 &rel_vel,
		MT_Scalar restitution,
		MT_Scalar friction_factor,
		MT_Scalar invMass
	);

	typedef std::vector<SM_Callback *> T_CallbackList;


	T_CallbackList          m_callbackList;    // Each object can have multiple callbacks from the client (=game engine)
	SM_Object              *m_dynamicParent;   // Collisions between parent and children are ignored

    // as the collision callback now has only information
	// on an SM_Object, there must be a way that the SM_Object client
	// can identify it's clientdata after a collision
	SM_ClientObject        *m_client_object;
	
	void*					m_physicsClientObject;

	DT_ShapeHandle          m_shape;                 // Shape for collision detection

	// Material and shape properties are not owned by this class.

	const SM_MaterialProps *m_materialProps;         
	const SM_MaterialProps *m_materialPropsBackup;   // Backup in case the object temporarily becomes a ghost.
	const SM_ShapeProps    *m_shapeProps;           
	const SM_ShapeProps    *m_shapePropsBackup;      // Backup in case the object's dynamics is temporarily suspended
	DT_ObjectHandle         m_object;                // A handle to the corresponding object in SOLID.
	MT_Scalar               m_margin;                // Offset for the object's shape (also for collision detection)
	MT_Vector3              m_scaling;               // Non-uniform scaling of the object's shape

	double                  m_ogl_matrix[16];        // An OpenGL-type 4x4 matrix      
	MT_Transform            m_xform;                 // The object's local coordinate system
	MT_Transform            m_prev_xform;            // The object's local coordinate system in the previous frame
	SM_MotionState          m_prev_state;            // The object's motion state in the previous frame
	MT_Scalar               m_timeStep;              // The duration of the last frame 

	MT_Vector3              m_reaction_impulse;      // The accumulated impulse resulting from collisions
	MT_Vector3              m_reaction_force;        // The reaction force derived from the reaction impulse   

	MT_Vector3              m_lin_mom;               // Linear momentum (linear velocity times mass)
	MT_Vector3              m_ang_mom;               // Angular momentum (angualr velocity times inertia)
	MT_Vector3              m_force;                 // Force on center of mass (afffects linear momentum)
	MT_Vector3              m_torque;                // Torque around center of mass (affects angular momentum)
	
	SM_MotionState          m_frames[3];             
	
	MT_Vector3              m_error;                 // Error in position:- amount object must be moved to prevent intersection with scene

	// Here are the values of externally set linear and angular
	// velocity. These are updated from the outside
	// (actuators and python) each frame and combined with the
	// physics values. At the end of each frame (at the end of a
	// call to proceed) they are set to zero. This allows the
	// outside world to contribute to the velocity of an object
	// but still have it react to physics. 

	MT_Vector3				m_combined_lin_vel;
	MT_Vector3				m_combined_ang_vel;

	// The force and torque are the accumulated forces and torques applied by the client (game logic, python).

	SM_FhObject            *m_fh_object;             // The ray object used for Fh
	bool                    m_suspended;             // Is this object frozen?
	
	// Mass properties
	MT_Scalar               m_inv_mass;              // 1/mass
	MT_Vector3              m_inv_inertia;           // [1/inertia_x, 1/inertia_y, 1/inertia_z]
	MT_Matrix3x3            m_inv_inertia_tensor;    // Inverse Inertia Tensor
	
	bool                    m_kinematic;             // Have I been displaced (translated, rotated, scaled) in this frame? 
	bool                    m_prev_kinematic;        // Have I been displaced (translated, rotated, scaled) in the previous frame? 
	bool                    m_is_rigid_body;         // Should friction give me a change in angular momentum?
	int                     m_static;                // temporarily static.

};

#endif

