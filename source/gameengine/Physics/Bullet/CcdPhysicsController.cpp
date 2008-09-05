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

#include "CcdPhysicsController.h"
#include "btBulletDynamicsCommon.h"

#include "PHY_IMotionState.h"
#include "CcdPhysicsEnvironment.h"
#include "RAS_MeshObject.h"


class BP_Proxy;

///todo: fill all the empty CcdPhysicsController methods, hook them up to the btRigidBody class

//'temporarily' global variables
//float	gDeactivationTime = 2.f;
//bool	gDisableDeactivation = false;
extern float gDeactivationTime;
extern bool gDisableDeactivation;


float gLinearSleepingTreshold = 0.8f;
float gAngularSleepingTreshold = 1.0f;


btVector3 startVel(0,0,0);//-10000);

CcdPhysicsController::CcdPhysicsController (const CcdConstructionInfo& ci)
:m_cci(ci)
{
	m_collisionDelay = 0;
	m_newClientInfo = 0;
	m_registerCount = 0;
		
	// copy pointers locally to allow smart release
	m_MotionState = ci.m_MotionState;
	m_collisionShape = ci.m_collisionShape;
	// apply scaling before creating rigid body
	m_collisionShape->setLocalScaling(m_cci.m_scaling);
	if (m_cci.m_mass)
		m_collisionShape->calculateLocalInertia(m_cci.m_mass, m_cci.m_localInertiaTensor);
	// shape info is shared, increment ref count
	m_shapeInfo = ci.m_shapeInfo;
	if (m_shapeInfo)
		m_shapeInfo->AddRef();
	
	m_bulletMotionState = 0;
	
	
	CreateRigidbody();
	

	
	#ifdef WIN32
	if (m_body->getInvMass())
		m_body->setLinearVelocity(startVel);
	#endif

}

btTransform	CcdPhysicsController::GetTransformFromMotionState(PHY_IMotionState* motionState)
{
	btTransform trans;
	float tmp[3];
	motionState->getWorldPosition(tmp[0],tmp[1],tmp[2]);
	trans.setOrigin(btVector3(tmp[0],tmp[1],tmp[2]));

	btQuaternion orn;
	motionState->getWorldOrientation(orn[0],orn[1],orn[2],orn[3]);
	trans.setRotation(orn);
	return trans;

}

class	BlenderBulletMotionState : public btMotionState
{
	PHY_IMotionState*	m_blenderMotionState;

public:

	BlenderBulletMotionState(PHY_IMotionState* bms)
		:m_blenderMotionState(bms)
	{

	}

	virtual void	getWorldTransform(btTransform& worldTrans ) const
	{
		float pos[3];
		float quatOrn[4];

		m_blenderMotionState->getWorldPosition(pos[0],pos[1],pos[2]);
		m_blenderMotionState->getWorldOrientation(quatOrn[0],quatOrn[1],quatOrn[2],quatOrn[3]);
		worldTrans.setOrigin(btVector3(pos[0],pos[1],pos[2]));
		worldTrans.setBasis(btMatrix3x3(btQuaternion(quatOrn[0],quatOrn[1],quatOrn[2],quatOrn[3])));
	}

	virtual void	setWorldTransform(const btTransform& worldTrans)
	{
		m_blenderMotionState->setWorldPosition(worldTrans.getOrigin().getX(),worldTrans.getOrigin().getY(),worldTrans.getOrigin().getZ());
		btQuaternion rotQuat = worldTrans.getRotation();
		m_blenderMotionState->setWorldOrientation(rotQuat[0],rotQuat[1],rotQuat[2],rotQuat[3]);
		m_blenderMotionState->calculateWorldTransformations();
	}

};


void CcdPhysicsController::CreateRigidbody()
{

	btTransform trans = GetTransformFromMotionState(m_MotionState);

	m_bulletMotionState = new BlenderBulletMotionState(m_MotionState);

	m_body = new btRigidBody(m_cci.m_mass,
		m_bulletMotionState,
		m_collisionShape,
		m_cci.m_localInertiaTensor * m_cci.m_inertiaFactor,
		m_cci.m_linearDamping,m_cci.m_angularDamping,
		m_cci.m_friction,m_cci.m_restitution);

	//
	// init the rigidbody properly
	//
	
	//setMassProps this also sets collisionFlags
	//convert collision flags!
	//special case: a near/radar sensor controller should not be defined static or it will
	//generate loads of static-static collision messages on the console
	if ((m_cci.m_collisionFilterGroup & CcdConstructionInfo::SensorFilter) != 0)
	{
		// reset the flags that have been set so far
		m_body->setCollisionFlags(0);
	}
	m_body->setCollisionFlags(m_body->getCollisionFlags() | m_cci.m_collisionFlags);
	m_body->setGravity( m_cci.m_gravity);
	m_body->setDamping(m_cci.m_linearDamping, m_cci.m_angularDamping);

	if (!m_cci.m_bRigid)
	{
		m_body->setAngularFactor(0.f);
	}
}

static void DeleteBulletShape(btCollisionShape* shape)
{
	if (shape->getShapeType() == TRIANGLE_MESH_SHAPE_PROXYTYPE)
	{
		// shapes based on meshes use an interface that contains the vertices.
		btTriangleMeshShape* meshShape = static_cast<btTriangleMeshShape*>(shape);
		btStridingMeshInterface* meshInterface = meshShape->getMeshInterface();
		if (meshInterface)
			delete meshInterface;
	}
	delete shape;
}

CcdPhysicsController::~CcdPhysicsController()
{
	//will be reference counted, due to sharing
	if (m_cci.m_physicsEnv)
		m_cci.m_physicsEnv->removeCcdPhysicsController(this);

	if (m_MotionState)
		delete m_MotionState;
	if (m_bulletMotionState)
		delete m_bulletMotionState;
	delete m_body;

	if (m_collisionShape)
	{
		// collision shape is always unique to the controller, can delete it here
		if (m_collisionShape->isCompound())
		{
			// bullet does not delete the child shape, must do it here
			btCompoundShape* compoundShape = (btCompoundShape*)m_collisionShape;
			int numChild = compoundShape->getNumChildShapes();
			for (int i=numChild-1 ; i >= 0; i--)
			{
				btCollisionShape* childShape = compoundShape->getChildShape(i);
				DeleteBulletShape(childShape);
			}
		}
		DeleteBulletShape(m_collisionShape);
	}
	if (m_shapeInfo)
	{
		m_shapeInfo->Release();
	}
}


		/**
			SynchronizeMotionStates ynchronizes dynas, kinematic and deformable entities (and do 'late binding')
		*/
bool		CcdPhysicsController::SynchronizeMotionStates(float time)
{
	//sync non-static to motionstate, and static from motionstate (todo: add kinematic etc.)

	if (!m_body->isStaticObject())
	{
		const btVector3& worldPos = m_body->getCenterOfMassPosition();
		m_MotionState->setWorldPosition(worldPos[0],worldPos[1],worldPos[2]);
		
		const btQuaternion& worldquat = m_body->getOrientation();
		m_MotionState->setWorldOrientation(worldquat[0],worldquat[1],worldquat[2],worldquat[3]);

		m_MotionState->calculateWorldTransformations();

		float scale[3];
		m_MotionState->getWorldScaling(scale[0],scale[1],scale[2]);
		btVector3 scaling(scale[0],scale[1],scale[2]);
		GetCollisionShape()->setLocalScaling(scaling);
	} else
	{
		btVector3 worldPos;
		btQuaternion worldquat;

/*		m_MotionState->getWorldPosition(worldPos[0],worldPos[1],worldPos[2]);
		m_MotionState->getWorldOrientation(worldquat[0],worldquat[1],worldquat[2],worldquat[3]);
		btTransform oldTrans = m_body->getCenterOfMassTransform();
		btTransform newTrans(worldquat,worldPos);
				
		m_body->setCenterOfMassTransform(newTrans);
		//need to keep track of previous position for friction effects...
		
		m_MotionState->calculateWorldTransformations();
*/
		float scale[3];
		m_MotionState->getWorldScaling(scale[0],scale[1],scale[2]);
		btVector3 scaling(scale[0],scale[1],scale[2]);
		GetCollisionShape()->setLocalScaling(scaling);
	}
	return true;

}

		/**
			WriteMotionStateToDynamics synchronizes dynas, kinematic and deformable entities (and do 'late binding')
		*/
		
void		CcdPhysicsController::WriteMotionStateToDynamics(bool nondynaonly)
{

}
void		CcdPhysicsController::WriteDynamicsToMotionState()
{
}
		// controller replication
void		CcdPhysicsController::PostProcessReplica(class PHY_IMotionState* motionstate,class PHY_IPhysicsController* parentctrl)
{
	m_MotionState = motionstate;
	m_registerCount = 0;
	m_collisionShape = NULL;

	// always create a new shape to avoid scaling bug
	if (m_shapeInfo)
	{
		m_shapeInfo->AddRef();
		m_collisionShape = m_shapeInfo->CreateBulletShape();

		if (m_collisionShape)
		{
			// new shape has no scaling, apply initial scaling
			m_collisionShape->setLocalScaling(m_cci.m_scaling);
			if (m_cci.m_mass)
				m_collisionShape->calculateLocalInertia(m_cci.m_mass, m_cci.m_localInertiaTensor);
		}
	}

	m_body = 0;
	CreateRigidbody();

	if (m_body)
	{
		if (m_cci.m_mass)
		{
			m_body->setMassProps(m_cci.m_mass, m_cci.m_localInertiaTensor * m_cci.m_inertiaFactor);
		} 
	}			
	m_cci.m_physicsEnv->addCcdPhysicsController(this);


/*	SM_Object* dynaparent=0;
	SumoPhysicsController* sumoparentctrl = (SumoPhysicsController* )parentctrl;
	
	if (sumoparentctrl)
	{
		dynaparent = sumoparentctrl->GetSumoObject();
	}
	
	SM_Object* orgsumoobject = m_sumoObj;
	
	
	m_sumoObj	=	new SM_Object(
		orgsumoobject->getShapeHandle(), 
		orgsumoobject->getMaterialProps(),			
		orgsumoobject->getShapeProps(),
		dynaparent);
	
	m_sumoObj->setRigidBody(orgsumoobject->isRigidBody());
	
	m_sumoObj->setMargin(orgsumoobject->getMargin());
	m_sumoObj->setPosition(orgsumoobject->getPosition());
	m_sumoObj->setOrientation(orgsumoobject->getOrientation());
	//if it is a dyna, register for a callback
	m_sumoObj->registerCallback(*this);
	
	m_sumoScene->add(* (m_sumoObj));
	*/



}

		// kinematic methods
void		CcdPhysicsController::RelativeTranslate(float dlocX,float dlocY,float dlocZ,bool local)
{
	if (m_body)
	{
		m_body->activate(true);
		if (m_body->isStaticObject())
		{
			m_body->setCollisionFlags(m_body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
		}


		btVector3 dloc(dlocX,dlocY,dlocZ);
		btTransform xform = m_body->getCenterOfMassTransform();

		if (local)
		{
			dloc = xform.getBasis()*dloc;
		}

		xform.setOrigin(xform.getOrigin() + dloc);
		m_body->setCenterOfMassTransform(xform);
	}

}

void		CcdPhysicsController::RelativeRotate(const float rotval[9],bool local)
{
	if (m_body)
	{
		m_body->activate(true);
		if (m_body->isStaticObject())
		{
			m_body->setCollisionFlags(m_body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
		}

		btMatrix3x3 drotmat(	rotval[0],rotval[4],rotval[8],
								rotval[1],rotval[5],rotval[9],
								rotval[2],rotval[6],rotval[10]);


		btMatrix3x3 currentOrn;
		GetWorldOrientation(currentOrn);

		btTransform xform = m_body->getCenterOfMassTransform();

		xform.setBasis(xform.getBasis()*(local ? 
		drotmat : (currentOrn.inverse() * drotmat * currentOrn)));

		m_body->setCenterOfMassTransform(xform);
	}

}

void CcdPhysicsController::GetWorldOrientation(btMatrix3x3& mat)
{
	float orn[4];
	m_MotionState->getWorldOrientation(orn[0],orn[1],orn[2],orn[3]);
	btQuaternion quat(orn[0],orn[1],orn[2],orn[3]);
	mat.setRotation(quat);
}

void		CcdPhysicsController::getOrientation(float &quatImag0,float &quatImag1,float &quatImag2,float &quatReal)
{
	btQuaternion q = m_body->getCenterOfMassTransform().getRotation();
	quatImag0 = q[0];
	quatImag1 = q[1];
	quatImag2 = q[2];
	quatReal = q[3];
}
void		CcdPhysicsController::setOrientation(float quatImag0,float quatImag1,float quatImag2,float quatReal)
{
	if (m_body)
	{
		m_body->activate(true);
		if (m_body->isStaticObject())
		{
			m_body->setCollisionFlags(m_body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
		}
		// not required
		//m_MotionState->setWorldOrientation(quatImag0,quatImag1,quatImag2,quatReal);
		btTransform xform  = m_body->getCenterOfMassTransform();
		xform.setRotation(btQuaternion(quatImag0,quatImag1,quatImag2,quatReal));
		m_body->setCenterOfMassTransform(xform);
		// not required
		//m_bulletMotionState->setWorldTransform(xform);
	}

}

void CcdPhysicsController::setWorldOrientation(const btMatrix3x3& orn)
{
	if (m_body)
	{
		m_body->activate(true);
		if (m_body->isStaticObject())
		{
			m_body->setCollisionFlags(m_body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
		}
		// not required
		//m_MotionState->setWorldOrientation(quatImag0,quatImag1,quatImag2,quatReal);
		btTransform xform  = m_body->getCenterOfMassTransform();
		xform.setBasis(orn);
		m_body->setCenterOfMassTransform(xform);
		// not required
		//m_bulletMotionState->setWorldTransform(xform);
	}

}

void		CcdPhysicsController::setPosition(float posX,float posY,float posZ)
{
	if (m_body)
	{
		m_body->activate(true);
		if (m_body->isStaticObject())
		{
			m_body->setCollisionFlags(m_body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
		}
		// not required, this function is only used to update the physic controller
		//m_MotionState->setWorldPosition(posX,posY,posZ);
		btTransform xform  = m_body->getCenterOfMassTransform();
		xform.setOrigin(btVector3(posX,posY,posZ));
		m_body->setCenterOfMassTransform(xform);
		// not required
		//m_bulletMotionState->setWorldTransform(xform);
	}


}
void		CcdPhysicsController::resolveCombinedVelocities(float linvelX,float linvelY,float linvelZ,float angVelX,float angVelY,float angVelZ)
{
}

void 		CcdPhysicsController::getPosition(PHY__Vector3&	pos) const
{
	const btTransform& xform = m_body->getCenterOfMassTransform();
	pos[0] = xform.getOrigin().x();
	pos[1] = xform.getOrigin().y();
	pos[2] = xform.getOrigin().z();
}

void		CcdPhysicsController::setScaling(float scaleX,float scaleY,float scaleZ)
{
	if (!btFuzzyZero(m_cci.m_scaling.x()-scaleX) ||
		!btFuzzyZero(m_cci.m_scaling.y()-scaleY) ||
		!btFuzzyZero(m_cci.m_scaling.z()-scaleZ))
	{
		m_cci.m_scaling = btVector3(scaleX,scaleY,scaleZ);

		if (m_body && m_body->getCollisionShape())
		{
			m_body->getCollisionShape()->setLocalScaling(m_cci.m_scaling);
			
			//printf("no inertia recalc for fixed objects with mass=0\n");
			if (m_cci.m_mass)
			{
				m_body->getCollisionShape()->calculateLocalInertia(m_cci.m_mass, m_cci.m_localInertiaTensor);
				m_body->setMassProps(m_cci.m_mass, m_cci.m_localInertiaTensor * m_cci.m_inertiaFactor);
			} 
			
		}
	}
}
		
		// physics methods
void		CcdPhysicsController::ApplyTorque(float torqueX,float torqueY,float torqueZ,bool local)
{
	btVector3 torque(torqueX,torqueY,torqueZ);
	btTransform xform = m_body->getCenterOfMassTransform();
	if (m_body && torque.length2() > (SIMD_EPSILON*SIMD_EPSILON))
	{
		m_body->activate();
		if (m_body->isStaticObject())
		{
			m_body->setCollisionFlags(m_body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
		}
		if (local)
		{
			torque	= xform.getBasis()*torque;
		}
		m_body->applyTorque(torque);
	}
}

void		CcdPhysicsController::ApplyForce(float forceX,float forceY,float forceZ,bool local)
{
	btVector3 force(forceX,forceY,forceZ);
	
	if (m_body && force.length2() > (SIMD_EPSILON*SIMD_EPSILON))
	{
		m_body->activate();
		if (m_body->isStaticObject())
		{
			m_body->setCollisionFlags(m_body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
		}
		{
			btTransform xform = m_body->getCenterOfMassTransform();
			if (local)
			{	
				force	= xform.getBasis()*force;
			}
		}
		m_body->applyCentralForce(force);
	}
}
void		CcdPhysicsController::SetAngularVelocity(float ang_velX,float ang_velY,float ang_velZ,bool local)
{
	btVector3 angvel(ang_velX,ang_velY,ang_velZ);
	if (m_body && angvel.length2() > (SIMD_EPSILON*SIMD_EPSILON))
	{
		m_body->activate(true);
		if (m_body->isStaticObject())
		{
			m_body->setCollisionFlags(m_body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
		}
		{
			btTransform xform = m_body->getCenterOfMassTransform();
			if (local)
			{
				angvel	= xform.getBasis()*angvel;
			}
		}
		m_body->setAngularVelocity(angvel);
	}

}
void		CcdPhysicsController::SetLinearVelocity(float lin_velX,float lin_velY,float lin_velZ,bool local)
{

	btVector3 linVel(lin_velX,lin_velY,lin_velZ);
	if (m_body && linVel.length2() > (SIMD_EPSILON*SIMD_EPSILON))
	{
		m_body->activate(true);
		if (m_body->isStaticObject())
		{
			m_body->setCollisionFlags(m_body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
		}

		{
			btTransform xform = m_body->getCenterOfMassTransform();
			if (local)
			{
				linVel	= xform.getBasis()*linVel;
			}
		}
		m_body->setLinearVelocity(linVel);
	}
}
void		CcdPhysicsController::applyImpulse(float attachX,float attachY,float attachZ, float impulseX,float impulseY,float impulseZ)
{
	btVector3 impulse(impulseX,impulseY,impulseZ);

	if (m_body && impulse.length2() > (SIMD_EPSILON*SIMD_EPSILON))
	{
		m_body->activate();
		if (m_body->isStaticObject())
		{
			m_body->setCollisionFlags(m_body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
		}
		
		btVector3 pos(attachX,attachY,attachZ);

		m_body->applyImpulse(impulse,pos);
	}

}
void		CcdPhysicsController::SetActive(bool active)
{
}
		// reading out information from physics
void		CcdPhysicsController::GetLinearVelocity(float& linvX,float& linvY,float& linvZ)
{
	const btVector3& linvel = this->m_body->getLinearVelocity();
	linvX = linvel.x();
	linvY = linvel.y();
	linvZ = linvel.z();

}

void		CcdPhysicsController::GetAngularVelocity(float& angVelX,float& angVelY,float& angVelZ)
{
	const btVector3& angvel= m_body->getAngularVelocity();
	angVelX = angvel.x();
	angVelY = angvel.y();
	angVelZ = angvel.z();
}

void		CcdPhysicsController::GetVelocity(const float posX,const float posY,const float posZ,float& linvX,float& linvY,float& linvZ)
{
	btVector3 pos(posX,posY,posZ);
	btVector3 rel_pos = pos-m_body->getCenterOfMassPosition();
	btVector3 linvel = m_body->getVelocityInLocalPoint(rel_pos);
	linvX = linvel.x();
	linvY = linvel.y();
	linvZ = linvel.z();
}
void		CcdPhysicsController::getReactionForce(float& forceX,float& forceY,float& forceZ)
{
}

		// dyna's that are rigidbody are free in orientation, dyna's with non-rigidbody are restricted 
void		CcdPhysicsController::setRigidBody(bool rigid)
{
	if (!rigid)
	{
		//fake it for now
		btVector3 inertia = m_body->getInvInertiaDiagLocal();
		inertia[1] = 0.f;
		m_body->setInvInertiaDiagLocal(inertia);
		m_body->updateInertiaTensor();
	}
}

		// clientinfo for raycasts for example
void*		CcdPhysicsController::getNewClientInfo()
{
	return m_newClientInfo;
}
void		CcdPhysicsController::setNewClientInfo(void* clientinfo)
{
	m_newClientInfo = clientinfo;
}


void	CcdPhysicsController::UpdateDeactivation(float timeStep)
{
	m_body->updateDeactivation( timeStep);
}

bool CcdPhysicsController::wantsSleeping()
{

	return m_body->wantsSleeping();
}

PHY_IPhysicsController*	CcdPhysicsController::GetReplica()
{
	// This is used only to replicate Near and Radar sensor controllers
	// The replication of object physics controller is done in KX_BulletPhysicsController::GetReplica()
	CcdConstructionInfo cinfo = m_cci;
	if (m_shapeInfo)
	{
		// This situation does not normally happen
		cinfo.m_collisionShape = m_shapeInfo->CreateBulletShape();
	} 
	else if (m_collisionShape)
	{
		switch (m_collisionShape->getShapeType())
		{
		case SPHERE_SHAPE_PROXYTYPE:
			{
				btSphereShape* orgShape = (btSphereShape*)m_collisionShape;
				cinfo.m_collisionShape = new btSphereShape(*orgShape);
				break;
			}

		case CONE_SHAPE_PROXYTYPE:
			{
				btConeShape* orgShape = (btConeShape*)m_collisionShape;
				cinfo.m_collisionShape = new btConeShape(*orgShape);
				break;
			}

		default:
			{
				return 0;
			}
		}
	}

	cinfo.m_MotionState = new DefaultMotionState();
	cinfo.m_shapeInfo = m_shapeInfo;

	CcdPhysicsController* replica = new CcdPhysicsController(cinfo);
	return replica;
}

///////////////////////////////////////////////////////////
///A small utility class, DefaultMotionState
///
///////////////////////////////////////////////////////////

DefaultMotionState::DefaultMotionState()
{
	m_worldTransform.setIdentity();
	m_localScaling.setValue(1.f,1.f,1.f);
}


DefaultMotionState::~DefaultMotionState()
{

}

void	DefaultMotionState::getWorldPosition(float& posX,float& posY,float& posZ)
{
	posX = m_worldTransform.getOrigin().x();
	posY = m_worldTransform.getOrigin().y();
	posZ = m_worldTransform.getOrigin().z();
}

void	DefaultMotionState::getWorldScaling(float& scaleX,float& scaleY,float& scaleZ)
{
	scaleX = m_localScaling.getX();
	scaleY = m_localScaling.getY();
	scaleZ = m_localScaling.getZ();
}

void	DefaultMotionState::getWorldOrientation(float& quatIma0,float& quatIma1,float& quatIma2,float& quatReal)
{
	quatIma0 = m_worldTransform.getRotation().x();
	quatIma1 = m_worldTransform.getRotation().y();
	quatIma2 = m_worldTransform.getRotation().z();
	quatReal = m_worldTransform.getRotation()[3];
}
		
void	DefaultMotionState::setWorldPosition(float posX,float posY,float posZ)
{
	btPoint3 pos(posX,posY,posZ);
	m_worldTransform.setOrigin( pos );
}

void	DefaultMotionState::setWorldOrientation(float quatIma0,float quatIma1,float quatIma2,float quatReal)
{
	btQuaternion orn(quatIma0,quatIma1,quatIma2,quatReal);
	m_worldTransform.setRotation( orn );
}
		
void	DefaultMotionState::calculateWorldTransformations()
{

}

// Shape constructor
bool CcdShapeConstructionInfo::SetMesh(RAS_MeshObject* meshobj, bool polytope)
{
	// assume no shape information
	m_shapeType = PHY_SHAPE_NONE;
	m_vertexArray.clear();
	m_polygonIndexArray.clear();
	m_meshObject = NULL;

	if (!meshobj)
		return false;

	// Mesh has no polygons!
	int numpolys = meshobj->NumPolygons();
	if (!numpolys)
	{
		return false;
	}

	// check that we have at least one colliding polygon
	int numvalidpolys = 0;

	for (int p=0; p<numpolys; p++)
	{
		RAS_Polygon* poly = meshobj->GetPolygon(p);

		// only add polygons that have the collisionflag set
		if (poly->IsCollider())
		{
			numvalidpolys++;
			break;
		}
	}

	// No collision polygons
	if (numvalidpolys < 1)
		return false;

	m_shapeType = (polytope) ? PHY_SHAPE_POLYTOPE : PHY_SHAPE_MESH;

	numvalidpolys = 0;

	for (int p2=0; p2<numpolys; p2++)
	{
		RAS_Polygon* poly = meshobj->GetPolygon(p2);

		// only add polygons that have the collisionflag set
		if (poly->IsCollider())
		{   
			//Bullet can raycast any shape, so
			if (polytope)
			{
				for (int i=0;i<poly->VertexCount();i++)
				{
					const float* vtx = poly->GetVertex(i)->getXYZ();
					btPoint3 point(vtx[0],vtx[1],vtx[2]);
					m_vertexArray.push_back(point);
					numvalidpolys++;
				}
			} else
			{
				{
					const float* vtx = poly->GetVertex(2)->getXYZ();
					btPoint3 vertex0(vtx[0],vtx[1],vtx[2]);

					vtx = poly->GetVertex(1)->getXYZ();
					btPoint3 vertex1(vtx[0],vtx[1],vtx[2]);

					vtx = poly->GetVertex(0)->getXYZ();
					btPoint3 vertex2(vtx[0],vtx[1],vtx[2]);

					m_vertexArray.push_back(vertex0);
					m_vertexArray.push_back(vertex1);
					m_vertexArray.push_back(vertex2);
					m_polygonIndexArray.push_back(p2);
					numvalidpolys++;
				}
				if (poly->VertexCount() == 4)
				{
					const float* vtx = poly->GetVertex(3)->getXYZ();
					btPoint3 vertex0(vtx[0],vtx[1],vtx[2]);

					vtx = poly->GetVertex(2)->getXYZ();
					btPoint3 vertex1(vtx[0],vtx[1],vtx[2]);

					vtx = poly->GetVertex(0)->getXYZ();
					btPoint3 vertex2(vtx[0],vtx[1],vtx[2]);

					m_vertexArray.push_back(vertex0);
					m_vertexArray.push_back(vertex1);
					m_vertexArray.push_back(vertex2);
					m_polygonIndexArray.push_back(p2);
					numvalidpolys++;
				}
			}		
		}
	}

	if (!numvalidpolys)
	{
		// should not happen
		m_shapeType = PHY_SHAPE_NONE;
		return false;
	}
	m_meshObject = meshobj;
	return true;
}

btCollisionShape* CcdShapeConstructionInfo::CreateBulletShape()
{
	btCollisionShape* collisionShape = 0;
	btTriangleMeshShape* concaveShape = 0;
	btTriangleMesh* collisionMeshData = 0;
	btCompoundShape* compoundShape = 0;
	CcdShapeConstructionInfo* nextShapeInfo;

	switch (m_shapeType) 
	{
	case PHY_SHAPE_NONE:
		break;

	case PHY_SHAPE_BOX:
		collisionShape = new btBoxShape(m_halfExtend);
		break;

	case PHY_SHAPE_SPHERE:
		collisionShape = new btSphereShape(m_radius);
		break;

	case PHY_SHAPE_CYLINDER:
		collisionShape = new btCylinderShapeZ(m_halfExtend);
		break;

	case PHY_SHAPE_CONE:
		collisionShape = new btConeShapeZ(m_radius, m_height);
		break;

	case PHY_SHAPE_POLYTOPE:
		collisionShape = new btConvexHullShape(&m_vertexArray.begin()->getX(), m_vertexArray.size());
		break;

	case PHY_SHAPE_MESH:
		collisionMeshData = new btTriangleMesh();
		// m_vertexArray is necessarily a multiple of 3
		for (std::vector<btPoint3>::iterator it=m_vertexArray.begin(); it != m_vertexArray.end(); )
		{
            collisionMeshData->addTriangle(*it++,*it++,*it++);
		}
		concaveShape = new btBvhTriangleMeshShape( collisionMeshData, true );
		concaveShape->recalcLocalAabb();
		collisionShape = concaveShape;
		break;

	case PHY_SHAPE_COMPOUND:
		if (m_nextShape)
		{
			compoundShape = new btCompoundShape();
			for (nextShapeInfo=m_nextShape; nextShapeInfo; nextShapeInfo = nextShapeInfo->m_nextShape)
			{
				collisionShape = nextShapeInfo->CreateBulletShape();
				if (collisionShape)
				{
					compoundShape->addChildShape(nextShapeInfo->m_childTrans, collisionShape);
				}
			}
			collisionShape = compoundShape;
		}
	}
	return collisionShape;
}

void CcdShapeConstructionInfo::AddShape(CcdShapeConstructionInfo* shapeInfo)
{
	CcdShapeConstructionInfo* nextShape = this;
	while (nextShape->m_nextShape != NULL)
		nextShape = nextShape->m_nextShape;
	nextShape->m_nextShape = shapeInfo;
}

CcdShapeConstructionInfo::~CcdShapeConstructionInfo()
{
	CcdShapeConstructionInfo* childShape = m_nextShape;

	while (childShape)
	{
		CcdShapeConstructionInfo* nextShape = childShape->m_nextShape;
		childShape->m_nextShape = NULL;
		childShape->Release();
		childShape = nextShape;
	}
	
	m_vertexArray.clear();
}


