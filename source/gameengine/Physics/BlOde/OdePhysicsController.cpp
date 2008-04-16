/**
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * The contents of this file may be used under the terms of either the GNU
 * General Public License Version 2 or later (the "GPL", see
 * http://www.gnu.org/licenses/gpl.html ), or the Blender License 1.0 or
 * later (the "BL", see http://www.blender.org/BL/ ) which has to be
 * bought from the Blender Foundation to become active, in which case the
 * above mentioned GPL option does not apply.
 *
 * The Original Code is Copyright (C) 2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
#define USE_ODE
#ifdef USE_ODE

#include "OdePhysicsController.h"
#include "PHY_IMotionState.h"

#include <ode/ode.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

///////////////////////////////////////////////////////////////////////////
//
// general to-do list for ODE physics. This is maintained in doxygen format.
//
/// \todo determine assignment time for bounding spheres.
///
/// it appears you have to select "sphere" for bounding volume AND "draw bounds"
/// in order for a bounding sphere to be generated. otherwise a box is generated.
/// determine exactly when and how the bounding volumes are generated and make
/// this consistent.
/// }
/// 
/// \todo bounding sphere size incorrect
///
/// it appears NOT to use the size of the shown bounding sphere (button "draw bounds").
/// it appears instead to use the size of the "size" dynamic parameter in the
/// gamebuttons but this "size" draws an incorrectly-sized circle on screen for the
/// bounding sphere (leftover skewed size calculation from sumo?) so figure out WHERE
/// its getting the radius from.
/// 
/// \todo ODE collisions must fire collision actuator
///
/// See OdePhysicsEnvironment::OdeNearCallback. If a sensor was created to check
/// for the presence of this collision, then in the NearCallback you need to
/// take appropriate action regarding the sensor - something like checking its
/// controller and if needed firing its actuator. Need to find similar code in
/// Fuzzics which fires collision controllers/actuators.
/// 
/// \todo Are ghost collisions possible?
///
/// How do ghost collisions work? Do they require collision detection through ODE
/// and NON-CREATION of contact-joint in OdeNearCallback? Currently OdeNearCallback
/// creates joints ALWAYS for collisions.
/// 
/// \todo Why is KX_GameObject::addLinearVelocity commented out?
///
/// Try putting this code back in.
/// 
/// \todo Too many non-dynamic actors bogs down ODE physics
///
/// Lots of "geoms" (ODE static geometry) probably slows down ode. Try a test file
/// with lots of static geometry - the game performance in Blender says it is
/// spending all its time in physics, and I bet all that time is in collision
/// detection. It's ode's non-hierarchical collision detection. 
/// try making a separate ode test program (not within blender) with 1000 geoms and
/// see how fast it is. if it is really slow, there is the culprit. 
/// isnt someone working on an improved ODE collision detector? check
/// ode mailing list.
/// 
/// 
/// \todo support collision of dynas with non-dynamic triangle meshes
///
/// ODE has trimesh-collision support but only for trimeshes without a transform
/// matrix. update ODE tricollider to support a transform matrix. this will allow
/// moving trimeshes non-dynamically (e.g. through Ipos). then collide trimeshes
/// with dynas. this allows dynamic primitives (spheres, boxes) to collide with
/// non-dynamic or kinematically controlled tri-meshes. full dynamic trimesh to
/// dynamic trimesh support is hard because it requires (a) collision and penetration
/// depth for trimesh to trimesh and (hard to compute) (b) an intertia tensor
/// (easy to compute).
/// 
/// a triangle mesh collision geometry should be created when the blender
/// bounding volume (F9, EDITBUTTONS) is set to "polyheder", since this is
/// currently the place where sphere/box selection is made
/// 
/// \todo specify ODE ERP+CFM in blender interface
///
/// when ODE physics selected, have to be able to set global cfm and erp.
/// per-joint erp/cfm could be handled in constraint window.
/// 
/// \todo moving infinite mass objects should impart extra impulse to objects they collide with
///
/// currently ODE's ERP pushes them apart but doesn't account for their motion.
/// you have to detect if one body in a collision is a non-dyna. This
/// requires adding a new accessor method to
/// KX_IPhysicsInterfaceController to access the hidden m_isDyna variable,
/// currently it can only be written, not read). If one of the bodies in a
/// collision is a non-dyna, then impart an extra impulse based on the
/// motion of the static object (using its last 2 frames as an approximation
/// of its linear and angular velocity). Linear velocity is easy to
/// approximate, but angular? you have orientation at this frame and
/// orientation at previous frame. The question is what is the angular
/// velocity which would have taken you from the previous frame's orientation
/// to this frame's orientation?
/// 
/// \todo allow tweaking bounding volume size
///
/// the scene converter currently uses the blender bounding volume of the selected
/// object as the geometry for ODE collision purposes. this is good and automatic
/// intuitive - lets you choose between cube, sphere, mesh. but you need to be able
/// to tweak this size for physics.
/// 
/// \todo off center meshes totally wrong for ode
/// 
/// ode uses x, y, z extents regradless of center. then places geom at center of object.
/// but visual geom is not necessarily at center. need to detect off-center situations.
/// then do what? treat it as an encapsulated off-center mass, or recenter it?
/// 
/// i.o.w. recalculate center, or recalculate mass distribution (using encapsulation)?
/// 
/// \todo allow off-center mass
/// 
/// using ode geometry encapsulators
/// 
/// \todo allow entering compound geoms for complex collision shapes specified as a union of simpler shapes
/// 
/// The collision shape for arbitrary triangle meshes can probably in general be
///well approximated by a compound ODE geometry object, which is merely a combination
///of many primitives (capsule, sphere, box). I eventually want to add the ability
///to associate compound geometry objects with Blender gameobjects. I think one
///way of doing this would be to add a new button in the GameButtons, "RigidBodyCompound".
///If the object is "Dynamic" + "RigidBody", then the object's bounding volume (sphere,
///box) is created. If an object is "Dynamic" + "RigidBodyCompound", then the object itself
///will merely create a "wrapper" compound object, with the actual geometry objects
///being created from the object's children in Blender. E.g. if I wanted to make a
///compound collision object consisting of a sphere and 2 boxes, I would create a
///parent gameobject with the actual triangle mesh, and set its GameButtons to
///"RigidBodyCompound". I would then create 3 children of this object, 1 sphere and
///2 boxes, and set the GameButtons for the children to be "RigidBody". Then at
///scene conversion time, the scene converter sees "RigidBodyCompound" for the
///top-level object, then appropriately traverses the children and creates the compound
///collision geometry consisting of 2 boxes and a sphere. In this way, arbitrary
///mesh-mesh collision becomes much less necessary - the artist can (or must,
///depending on your point of view!) approximate the collision shape for arbitrary
///meshes with a combination of one or more primitive shapes. I think using the
///parent/child relationship in Blender and a new button "RigidBodyCompound" for the
///parent object of a compound is a feasible way of doing this in Blender.
/// 
///See ODE demo test_boxstack and look at the code when you drop a compound object
///with the "X" key.
///
/// \todo add visual specification of constraints
/// 
/// extend the armature constraint system. by using empties and constraining one empty
/// to "copy location" of another, you can get a p2p constraint between the two empties.
/// by making the two empties each a parent of a blender object, you effectively have
/// a p2p constraint between 2 blender bodies. the scene converter can detect these
/// empties, detect the constraint, and generate an ODE constraint.
/// 
/// then add a new constraint type "hinge" and "slider" to correspond to ODE joints.
/// e.g. a slider would be a constraint which restricts the axis of its object to lie
/// along the same line as another axis of a different object. e.g. you constrain x-axis
/// of one empty to lie along the same line as the z-axis of another empty; this gives
/// a slider joint.
///
/// open questions: how to handle powered joints? to what extent should/must constraints
/// be enforced during modeling? use CCD-style algorithm in modeler to enforce constraints?
/// how about ODE powered constraints e.g. motors?
/// 
/// \todo enable suspension of bodies
/// ODE offers native support for suspending dynas. but what about suspending non-dynas
/// (e.g. geoms)? suspending geoms is also necessary to ease the load of ODE's (simple?)
/// collision detector. suspending dynas and geoms is important for the activity culling,
/// which apparently works at a simple level. perhaps suspension should actually
/// remove or insert geoms/dynas into the ODE space/world? is this operation (insertion/
/// removal) fast enough at run-time? test it. if fast enough, then suspension=remove from
/// ODE simulation, awakening=insertion into ODE simulation.
///
/// \todo python interface for tweaking constraints via python
///
/// \todo raytesting to support gameengine sensors that need it
///
/// \todo investigate compatibility issues with old Blender 2.25 physics engine (sumo/fuzzics)
/// is it possible to have compatibility? how hard is it? how important is it?


ODEPhysicsController::ODEPhysicsController(bool dyna, bool fullRigidBody,
                                           bool phantom, class PHY_IMotionState* motionstate, struct dxSpace* space,
                                           struct dxWorld* world, float mass,float friction,float restitution,
                                           bool implicitsphere,float center[3],float extents[3],float radius)
  :     
  m_OdeDyna(dyna),
  m_firstTime(true),
  m_bFullRigidBody(fullRigidBody),
  m_bPhantom(phantom),
  m_bKinematic(false),
  m_bPrevKinematic(false),
  m_MotionState(motionstate),
  m_OdeSuspendDynamics(false),
  m_space(space),
  m_world(world),
  m_mass(mass),
  m_friction(friction),
  m_restitution(restitution),
  m_bodyId(0),
  m_geomId(0),
  m_implicitsphere(implicitsphere),
  m_radius(radius)
{
  m_center[0] = center[0];
  m_center[1] = center[1];
  m_center[2] = center[2];
  m_extends[0] = extents[0];
  m_extends[1] = extents[1];
  m_extends[2] = extents[2];
};


ODEPhysicsController::~ODEPhysicsController()
{
  if (m_geomId)
    {
      dGeomDestroy (m_geomId);
    }
}

float ODEPhysicsController::getMass()
{
  dMass mass;
  dBodyGetMass(m_bodyId,&mass);
  return mass.mass;
}

//////////////////////////////////////////////////////////////////////
/// \todo Impart some extra impulse to dynamic objects when they collide with kinematically controlled "static" objects (ODE geoms), by using last 2 frames as 1st order approximation to the linear/angular velocity, and computing an appropriate impulse. Sumo (old physics engine) did this, see for details.
/// \todo handle scaling of static ODE geoms or fail with error message if Ipo tries to change scale of a static geom object

bool ODEPhysicsController::SynchronizeMotionStates(float time)
{
  /** 
      'Late binding' of the rigidbody, because the World Scaling is not available until the scenegraph is traversed
  */

        
  if (m_firstTime)
    {
      m_firstTime=false;

      m_MotionState->calculateWorldTransformations();
        
      dQuaternion worldquat;
      float worldpos[3];
                
#ifdef dDOUBLE
      m_MotionState->getWorldOrientation((float)worldquat[1],
	 (float)worldquat[2],(float)worldquat[3],(float)worldquat[0]);
#else
      m_MotionState->getWorldOrientation(worldquat[1],
	 worldquat[2],worldquat[3],worldquat[0]);
#endif
      m_MotionState->getWorldPosition(worldpos[0],worldpos[1],worldpos[2]);
        
      float scaling[3];
      m_MotionState->getWorldScaling(scaling[0],scaling[1],scaling[2]);
                
      if (!m_bPhantom)
        {
          if (m_implicitsphere)
            {
              m_geomId = dCreateSphere (m_space,m_radius*scaling[0]);
            } else
              {
                m_geomId = dCreateBox (m_space, m_extends[0]*scaling[0],m_extends[1]*scaling[1],m_extends[2]*scaling[2]);
              }
        } else
          {
            m_geomId=0;
          }

      if (m_geomId)
        dGeomSetData(m_geomId,this);

      if (!this->m_OdeDyna)
        {
          if (!m_bPhantom)
            {
              dGeomSetPosition (this->m_geomId,worldpos[0],worldpos[1],worldpos[2]);
              dMatrix3 R;
              dQtoR (worldquat, R);
              dGeomSetRotation (this->m_geomId,R);
            }
        } else
          {
            //it's dynamic, so create a 'model'
            m_bodyId = dBodyCreate(this->m_world);
            dBodySetPosition (m_bodyId,worldpos[0],worldpos[1],worldpos[2]);
            dBodySetQuaternion (this->m_bodyId,worldquat);
            //this contains both scalar mass and inertia tensor
            dMass m; 
            float length=1,width=1,height=1;
            dMassSetBox (&m,1,m_extends[0]*scaling[0],m_extends[1]*scaling[1],m_extends[2]*scaling[2]);
            dMassAdjust (&m,this->m_mass);
            dBodySetMass (m_bodyId,&m);

            if (!m_bPhantom)
              {
                dGeomSetBody (m_geomId,m_bodyId);
              }

                        
          } 
                
      if (this->m_OdeDyna && !m_bFullRigidBody)
        {
          // ?? huh? what to do here?
        }
    }



  if (m_OdeDyna)
    {
      if (this->m_OdeSuspendDynamics)
        {
          return false;
        }

      const float* worldPos = (float *)dBodyGetPosition(m_bodyId);
      m_MotionState->setWorldPosition(worldPos[0],worldPos[1],worldPos[2]);

      const float* worldquat = (float *)dBodyGetQuaternion(m_bodyId);
      m_MotionState->setWorldOrientation(worldquat[1],worldquat[2],worldquat[3],worldquat[0]);
    }
  else {
    // not a dyna, so dynamics (i.e. this controller) has not updated
    // anything. BUT! an Ipo or something else might have changed the
    // position/orientation of this geometry.
    // so update the static geom position

    /// \todo impart some extra impulse to colliding objects!  
      dQuaternion worldquat;
      float worldpos[3];

#ifdef dDOUBLE
      m_MotionState->getWorldOrientation((float)worldquat[1],
	(float)worldquat[2],(float)worldquat[3],(float)worldquat[0]);
#else
      m_MotionState->getWorldOrientation(worldquat[1],
	worldquat[2],worldquat[3],worldquat[0]);
#endif
      m_MotionState->getWorldPosition(worldpos[0],worldpos[1],worldpos[2]);
        
      float scaling[3];
      m_MotionState->getWorldScaling(scaling[0],scaling[1],scaling[2]);

      /// \todo handle scaling! what if Ipo changes scale of object?
      // Must propagate to geom... is scaling geoms possible with ODE? Also
      // what about scaling trimeshes, that is certainly difficult...
      dGeomSetPosition (this->m_geomId,worldpos[0],worldpos[1],worldpos[2]);
      dMatrix3 R;
      dQtoR (worldquat, R);
      dGeomSetRotation (this->m_geomId,R);
  }

  return false; //it update the worldpos
}
 



// kinematic methods
void ODEPhysicsController::RelativeTranslate(float dlocX,float dlocY,float dlocZ,bool local)
{

}
void ODEPhysicsController::RelativeRotate(const float drot[9],bool local)
{
}
void ODEPhysicsController::setOrientation(float quatImag0,float quatImag1,float quatImag2,float quatReal)
{

  dQuaternion worldquat;
  worldquat[0] = quatReal;
  worldquat[1] = quatImag0;
  worldquat[2] = quatImag1;
  worldquat[3] = quatImag2;
        
  if (!this->m_OdeDyna)
    {
      dMatrix3 R;
      dQtoR (worldquat, R);
      dGeomSetRotation (this->m_geomId,R);
    } else
      {
        dBodySetQuaternion (m_bodyId,worldquat);
        this->m_MotionState->setWorldOrientation(quatImag0,quatImag1,quatImag2,quatReal);
      }

}

void ODEPhysicsController::getOrientation(float &quatImag0,float &quatImag1,float &quatImag2,float &quatReal)
{
  float q[4];
  this->m_MotionState->getWorldOrientation(q[0],q[1],q[2],q[3]);
  quatImag0=q[0];
  quatImag1=q[1];
  quatImag2=q[2];
  quatReal=q[3];
}

void 		ODEPhysicsController::getPosition(PHY__Vector3&	pos) const
{
	m_MotionState->getWorldPosition(pos[0],pos[1],pos[2]);

}
	
void ODEPhysicsController::setPosition(float posX,float posY,float posZ)
{
  if (!m_bPhantom)
    {
      if (!this->m_OdeDyna)
        {
          dGeomSetPosition (m_geomId, posX, posY, posZ);
        } else
          {
            dBodySetPosition (m_bodyId, posX, posY, posZ);
          }
    }
}
void ODEPhysicsController::setScaling(float scaleX,float scaleY,float scaleZ)
{
}
        
// physics methods
void ODEPhysicsController::ApplyTorque(float torqueX,float torqueY,float torqueZ,bool local)
{
  if (m_OdeDyna) {
    if(local) {
      dBodyAddRelTorque(m_bodyId, torqueX, torqueY, torqueZ);
    } else {
      dBodyAddTorque (m_bodyId, torqueX, torqueY, torqueZ);
    }
  }
}

void ODEPhysicsController::ApplyForce(float forceX,float forceY,float forceZ,bool local)
{
  if (m_OdeDyna) {
    if(local) {
      dBodyAddRelForce(m_bodyId, forceX, forceY, forceZ);
    } else {
      dBodyAddForce (m_bodyId, forceX, forceY, forceZ);
    }
  }
}

void ODEPhysicsController::SetAngularVelocity(float ang_velX,float ang_velY,float ang_velZ,bool local)
{
  if (m_OdeDyna) {
    if(local) {
      // TODO: translate angular vel into local frame, then apply
    } else {
      dBodySetAngularVel  (m_bodyId, ang_velX,ang_velY,ang_velZ);
    }
  }
}
        
void ODEPhysicsController::SetLinearVelocity(float lin_velX,float lin_velY,float lin_velZ,bool local)
{
  if (m_OdeDyna)
    {
      dVector3 vel = {lin_velX,lin_velY,lin_velZ, 1.0};
      if (local)
        {
          dMatrix3 worldmat;
          dVector3 localvel;
          dQuaternion worldquat;

#ifdef dDOUBLE
          m_MotionState->getWorldOrientation((float)worldquat[1],
		(float)worldquat[2], (float)worldquat[3],(float)worldquat[0]);
#else
          m_MotionState->getWorldOrientation(worldquat[1],worldquat[2],
		worldquat[3],worldquat[0]);
#endif
          dQtoR (worldquat, worldmat);
                        
          dMULTIPLY0_331 (localvel,worldmat,vel);
          dBodySetLinearVel  (m_bodyId, localvel[0],localvel[1],localvel[2]);

        } else
          {
            dBodySetLinearVel  (m_bodyId, lin_velX,lin_velY,lin_velZ);
          }
    }
}

void ODEPhysicsController::applyImpulse(float attachX,float attachY,float attachZ, float impulseX,float impulseY,float impulseZ)
{
  if (m_OdeDyna)
    {
      //apply linear and angular effect
      const dReal* linvel = dBodyGetLinearVel(m_bodyId);
      float mass =      getMass();
      if (mass >= 0.00001f)
        {
          float massinv = 1.f/mass;
          float newvel[3];
          newvel[0]=linvel[0]+impulseX*massinv;
          newvel[1]=linvel[1]+impulseY*massinv;
          newvel[2]=linvel[2]+impulseZ*massinv;
          dBodySetLinearVel(m_bodyId,newvel[0],newvel[1],newvel[2]);

          const float* worldPos = (float *)dBodyGetPosition(m_bodyId);

          const float* angvelc = (float *)dBodyGetAngularVel(m_bodyId);
          float angvel[3];
          angvel[0]=angvelc[0];
          angvel[1]=angvelc[1];
          angvel[2]=angvelc[2];

          dVector3 impulse;
          impulse[0]=impulseX;
          impulse[1]=impulseY;
          impulse[2]=impulseZ;

          dVector3 ap;
          ap[0]=attachX-worldPos[0];
          ap[1]=attachY-worldPos[1];
          ap[2]=attachZ-worldPos[2];

          dCROSS(angvel,+=,ap,impulse);
          dBodySetAngularVel(m_bodyId,angvel[0],angvel[1],angvel[2]);

        }
                
    }

}

void ODEPhysicsController::SuspendDynamics()
{

}

void ODEPhysicsController::RestoreDynamics()
{

}


/**  
     reading out information from physics
*/
void ODEPhysicsController::GetLinearVelocity(float& linvX,float& linvY,float& linvZ)
{
  if (m_OdeDyna)
    {   
      const float* vel = (float *)dBodyGetLinearVel(m_bodyId);
      linvX = vel[0];
      linvY = vel[1];
      linvZ = vel[2];
    } else
      {
        linvX = 0.f;
        linvY = 0.f;
        linvZ = 0.f;

      }
}
/** 
    GetVelocity parameters are in geometric coordinates (Origin is not center of mass!).
*/
void ODEPhysicsController::GetVelocity(const float posX,const float posY,const float posZ,float& linvX,float& linvY,float& linvZ)
{

}


void ODEPhysicsController::getReactionForce(float& forceX,float& forceY,float& forceZ)
{

}
void ODEPhysicsController::setRigidBody(bool rigid)
{

}
                
        
void ODEPhysicsController::PostProcessReplica(class PHY_IMotionState* motionstate,class PHY_IPhysicsController* parentctrl)
{
  m_MotionState = motionstate;
  m_bKinematic = false;
  m_bPrevKinematic = false;
  m_firstTime = true;
}
        

void ODEPhysicsController::SetSimulatedTime(float time)
{
}
        
        
void ODEPhysicsController::WriteMotionStateToDynamics(bool nondynaonly)
{

}
#endif
