/** \file gameengine/Physics/Bullet/CcdPhysicsController.cpp
 *  \ingroup physbullet
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

#ifndef WIN32
#include <stdint.h>
#endif

#include "CcdPhysicsController.h"
#include "btBulletDynamicsCommon.h"
#include "BulletCollision/CollisionDispatch/btGhostObject.h"
#include "BulletCollision/CollisionShapes/btScaledBvhTriangleMeshShape.h"
#include "BulletCollision/CollisionShapes/btTriangleIndexVertexArray.h"

#include "PHY_IMotionState.h"
#include "CcdPhysicsEnvironment.h"
#include "RAS_MeshObject.h"
#include "RAS_Polygon.h"
#include "RAS_Deformer.h"
#include "KX_GameObject.h"

#include "BulletSoftBody/btSoftBody.h"
#include "BulletSoftBody/btSoftBodyInternals.h"
#include "BulletSoftBody/btSoftBodyHelpers.h"
#include "LinearMath/btConvexHull.h"
#include "BulletCollision/Gimpact/btGImpactShape.h"


#include "BulletSoftBody/btSoftRigidDynamicsWorld.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

extern "C"{
	#include "BLI_utildefines.h"
	#include "BKE_cdderivedmesh.h"
}


class BP_Proxy;

///todo: fill all the empty CcdPhysicsController methods, hook them up to the btRigidBody class

//'temporarily' global variables
//float	gDeactivationTime = 2.f;
//bool	gDisableDeactivation = false;
extern float gDeactivationTime;
extern bool gDisableDeactivation;


float gLinearSleepingTreshold;
float gAngularSleepingTreshold;


BlenderBulletCharacterController::BlenderBulletCharacterController(btMotionState *motionState, btPairCachingGhostObject *ghost, btConvexShape* shape, float stepHeight)
	: btKinematicCharacterController(ghost,shape,stepHeight,2),
		m_motionState(motionState),
		m_jumps(0),
		m_maxJumps(1)
{
}

void BlenderBulletCharacterController::updateAction(btCollisionWorld *collisionWorld, btScalar dt)
{
	if (onGround())
		m_jumps = 0;

	btKinematicCharacterController::updateAction(collisionWorld,dt);
	m_motionState->setWorldTransform(getGhostObject()->getWorldTransform());
}

int BlenderBulletCharacterController::getMaxJumps() const
{
	return m_maxJumps;
}

void BlenderBulletCharacterController::setMaxJumps(int maxJumps)
{
	m_maxJumps = maxJumps;
}

int BlenderBulletCharacterController::getJumpCount() const
{
	return m_jumps;
}

bool BlenderBulletCharacterController::canJump() const
{
	return onGround() || m_jumps < m_maxJumps;
}

void BlenderBulletCharacterController::jump()
{
	if (!canJump())
		return;
		
	m_verticalVelocity = m_jumpSpeed;
	m_wasJumping = true;
	m_jumps++;
}

const btVector3& BlenderBulletCharacterController::getWalkDirection()
{
	return m_walkDirection;
}

CcdPhysicsController::CcdPhysicsController (const CcdConstructionInfo& ci)
:m_cci(ci)
{
	m_prototypeTransformInitialized = false;
	m_softbodyMappingDone = false;
	m_collisionDelay = 0;
	m_newClientInfo = 0;
	m_registerCount = 0;
	m_softBodyTransformInitialized = false;
	m_parentCtrl = 0;
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

	m_bulletChildShape = NULL;
	
	m_bulletMotionState = 0;
	m_characterController = 0;
	m_savedCollisionFlags = 0;
	m_savedCollisionFilterGroup = 0;
	m_savedCollisionFilterMask = 0;
	m_savedMass = 0.0;
	m_suspended = false;
	
	CreateRigidbody();
}

btTransform&	CcdPhysicsController::GetTransformFromMotionState(PHY_IMotionState* motionState)
{
	static btTransform trans;
	btVector3 tmp;
	motionState->GetWorldPosition(tmp.m_floats[0], tmp.m_floats[1], tmp.m_floats[2]);
	trans.setOrigin(tmp);

	float ori[12];
	motionState->GetWorldOrientation(ori);
	trans.getBasis().setFromOpenGLSubMatrix(ori);
	//btQuaternion orn;
	//motionState->getWorldOrientation(orn[0],orn[1],orn[2],orn[3]);
	//trans.setRotation(orn);
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

	void	getWorldTransform(btTransform& worldTrans ) const
	{
		btVector3 pos;
		float ori[12];

		m_blenderMotionState->GetWorldPosition(pos.m_floats[0],pos.m_floats[1],pos.m_floats[2]);
		m_blenderMotionState->GetWorldOrientation(ori);
		worldTrans.setOrigin(pos);
		worldTrans.getBasis().setFromOpenGLSubMatrix(ori);
	}

	void	setWorldTransform(const btTransform& worldTrans)
	{
		m_blenderMotionState->SetWorldPosition(worldTrans.getOrigin().getX(),worldTrans.getOrigin().getY(),worldTrans.getOrigin().getZ());
		btQuaternion rotQuat = worldTrans.getRotation();
		m_blenderMotionState->SetWorldOrientation(rotQuat[0],rotQuat[1],rotQuat[2],rotQuat[3]);
		m_blenderMotionState->CalculateWorldTransformations();
	}

};

btRigidBody* CcdPhysicsController::GetRigidBody()
{
	return btRigidBody::upcast(m_object);
}
const btRigidBody* CcdPhysicsController::GetRigidBody() const
{
	return btRigidBody::upcast(m_object);
}

btCollisionObject*	CcdPhysicsController::GetCollisionObject()
{
	return m_object;
}
btSoftBody* CcdPhysicsController::GetSoftBody()
{
	return btSoftBody::upcast(m_object);
}
btKinematicCharacterController* CcdPhysicsController::GetCharacterController()
{
	return m_characterController;
}

#include "BulletSoftBody/btSoftBodyHelpers.h"


bool CcdPhysicsController::CreateSoftbody()
{
	int shapeType = m_cci.m_collisionShape ? m_cci.m_collisionShape->getShapeType() : 0;

	//disable soft body until first sneak preview is ready
	if (!m_cci.m_bSoft || !m_cci.m_collisionShape ||
		((shapeType != CONVEX_HULL_SHAPE_PROXYTYPE)&&
		(shapeType != TRIANGLE_MESH_SHAPE_PROXYTYPE) &&
		(shapeType != SCALED_TRIANGLE_MESH_SHAPE_PROXYTYPE)))
	{
		return false;
	}

	btRigidBody::btRigidBodyConstructionInfo rbci(m_cci.m_mass,m_bulletMotionState,m_collisionShape,m_cci.m_localInertiaTensor * m_cci.m_inertiaFactor);
	rbci.m_linearDamping = m_cci.m_linearDamping;
	rbci.m_angularDamping = m_cci.m_angularDamping;
	rbci.m_friction = m_cci.m_friction;
	rbci.m_restitution = m_cci.m_restitution;
	
	btVector3 p(0,0,0);// = getOrigin();
	//btSoftBody*	psb=btSoftBodyHelpers::CreateRope(worldInfo,	btVector3(-10,0,i*0.25),btVector3(10,0,i*0.25),	16,1+2);
	btSoftBody* psb  = 0;
	btSoftBodyWorldInfo& worldInfo = m_cci.m_physicsEnv->GetDynamicsWorld()->getWorldInfo();

	if (m_cci.m_collisionShape->getShapeType() == CONVEX_HULL_SHAPE_PROXYTYPE) {
		btConvexHullShape* convexHull = (btConvexHullShape* )m_cci.m_collisionShape;
		{
			int nvertices = convexHull->getNumPoints();
			const btVector3* vertices = convexHull->getPoints();

			HullDesc		hdsc(QF_TRIANGLES,nvertices,vertices);
			HullResult		hres;
			HullLibrary		hlib;  /*??*/
			hdsc.mMaxVertices=nvertices;
			hlib.CreateConvexHull(hdsc,hres);
			
			psb = new btSoftBody(&worldInfo, (int)hres.mNumOutputVertices,
			                     &hres.m_OutputVertices[0], 0);
			for (int i = 0; i < (int)hres.mNumFaces; ++i) {
				const unsigned int idx[3] = {hres.m_Indices[i * 3 + 0],
				                             hres.m_Indices[i * 3 + 1],
				                             hres.m_Indices[i * 3 + 2]};
				if (idx[0] < idx[1]) psb->appendLink(idx[0], idx[1]);
				if (idx[1] < idx[2]) psb->appendLink(idx[1], idx[2]);
				if (idx[2] < idx[0]) psb->appendLink(idx[2], idx[0]);
				psb->appendFace(idx[0], idx[1], idx[2]);
			}
			hlib.ReleaseResult(hres);
		}
	}
	else {
		int numtris = 0;
		if (m_cci.m_collisionShape->getShapeType() ==SCALED_TRIANGLE_MESH_SHAPE_PROXYTYPE)
		{
			btScaledBvhTriangleMeshShape* scaledtrimeshshape = (btScaledBvhTriangleMeshShape*) m_cci.m_collisionShape;
			btBvhTriangleMeshShape* trimeshshape = scaledtrimeshshape->getChildShape();

			///only deal with meshes that have 1 sub part/component, for now
			if (trimeshshape->getMeshInterface()->getNumSubParts()==1)
			{
				unsigned char* vertexBase;
				btScalar* scaledVertexBase;
				btVector3 localScaling;
				PHY_ScalarType vertexType;
				int numverts;
				int vertexstride;
				unsigned char* indexbase;
				int indexstride;
				PHY_ScalarType indexType;
				trimeshshape->getMeshInterface()->getLockedVertexIndexBase(&vertexBase,numverts,vertexType,vertexstride,&indexbase,indexstride,numtris,indexType);
				localScaling = scaledtrimeshshape->getLocalScaling();
				scaledVertexBase = new btScalar[numverts*3];
				for (int i=0; i<numverts*3; i+=3)
				{
					scaledVertexBase[i] = ((const btScalar*)vertexBase)[i] * localScaling.getX();
					scaledVertexBase[i+1] = ((const btScalar*)vertexBase)[i+1] * localScaling.getY();
					scaledVertexBase[i+2] = ((const btScalar*)vertexBase)[i+2] * localScaling.getZ();
				}
				psb = btSoftBodyHelpers::CreateFromTriMesh(worldInfo,scaledVertexBase,(const int*)indexbase,numtris,false);
				delete [] scaledVertexBase;
			}
		} else
		{
			btTriangleMeshShape* trimeshshape = (btTriangleMeshShape*) m_cci.m_collisionShape;
			///only deal with meshes that have 1 sub part/component, for now
			if (trimeshshape->getMeshInterface()->getNumSubParts()==1)
			{
				unsigned char* vertexBase;
				PHY_ScalarType vertexType;
				int numverts;
				int vertexstride;
				unsigned char* indexbase;
				int indexstride;
				PHY_ScalarType indexType;
				trimeshshape->getMeshInterface()->getLockedVertexIndexBase(&vertexBase,numverts,vertexType,vertexstride,&indexbase,indexstride,numtris,indexType);
				
				psb = btSoftBodyHelpers::CreateFromTriMesh(worldInfo,(const btScalar*)vertexBase,(const int*)indexbase,numtris,false);
			}
		}
		// store face tag so that we can find our original face when doing ray casting
		btSoftBody::Face* ft;
		int i;
		for (i=0, ft=&psb->m_faces[0]; i<numtris; ++i, ++ft)
		{
			// Hack!! use m_tag to store the face number, normally it is a pointer
			// add 1 to make sure it is never 0
			ft->m_tag = (void*)((uintptr_t)(i+1));
		}
	}
	if (m_cci.m_margin > 0.f)
	{
		psb->getCollisionShape()->setMargin(m_cci.m_margin);
		psb->updateBounds();
	}
	m_object = psb;
	
	//btSoftBody::Material*	pm=psb->appendMaterial();
	btSoftBody::Material*	pm=psb->m_materials[0];
	pm->m_kLST				=	m_cci.m_soft_linStiff;
	pm->m_kAST				=	m_cci.m_soft_angStiff;
	pm->m_kVST				=	m_cci.m_soft_volume;
	psb->m_cfg.collisions = 0;

	if (m_cci.m_soft_collisionflags & CCD_BSB_COL_CL_RS)
	{
		psb->m_cfg.collisions	+=	btSoftBody::fCollision::CL_RS;
	} else
	{
		psb->m_cfg.collisions	+=	btSoftBody::fCollision::SDF_RS;
	}
	if (m_cci.m_soft_collisionflags & CCD_BSB_COL_CL_SS)
	{
		psb->m_cfg.collisions += btSoftBody::fCollision::CL_SS;
	} else
	{
		psb->m_cfg.collisions += btSoftBody::fCollision::VF_SS;
	}


	psb->m_cfg.kSRHR_CL = m_cci.m_soft_kSRHR_CL;		/* Soft vs rigid hardness [0,1] (cluster only) */
	psb->m_cfg.kSKHR_CL = m_cci.m_soft_kSKHR_CL;		/* Soft vs kinetic hardness [0,1] (cluster only) */
	psb->m_cfg.kSSHR_CL = m_cci.m_soft_kSSHR_CL;		/* Soft vs soft hardness [0,1] (cluster only) */
	psb->m_cfg.kSR_SPLT_CL = m_cci.m_soft_kSR_SPLT_CL;	/* Soft vs rigid impulse split [0,1] (cluster only) */

	psb->m_cfg.kSK_SPLT_CL = m_cci.m_soft_kSK_SPLT_CL;	/* Soft vs rigid impulse split [0,1] (cluster only) */
	psb->m_cfg.kSS_SPLT_CL = m_cci.m_soft_kSS_SPLT_CL;	/* Soft vs rigid impulse split [0,1] (cluster only) */
	psb->m_cfg.kVCF = m_cci.m_soft_kVCF;			/* Velocities correction factor (Baumgarte) */
	psb->m_cfg.kDP = m_cci.m_soft_kDP;			/* Damping coefficient [0,1] */

	psb->m_cfg.kDG = m_cci.m_soft_kDG;			/* Drag coefficient [0,+inf] */
	psb->m_cfg.kLF = m_cci.m_soft_kLF;			/* Lift coefficient [0,+inf] */
	psb->m_cfg.kPR = m_cci.m_soft_kPR;			/* Pressure coefficient [-inf,+inf] */
	psb->m_cfg.kVC = m_cci.m_soft_kVC;			/* Volume conversation coefficient [0,+inf] */

	psb->m_cfg.kDF = m_cci.m_soft_kDF;			/* Dynamic friction coefficient [0,1] */
	psb->m_cfg.kMT = m_cci.m_soft_kMT;			/* Pose matching coefficient [0,1] */
	psb->m_cfg.kCHR = m_cci.m_soft_kCHR;			/* Rigid contacts hardness [0,1] */
	psb->m_cfg.kKHR = m_cci.m_soft_kKHR;			/* Kinetic contacts hardness [0,1] */

	psb->m_cfg.kSHR = m_cci.m_soft_kSHR;			/* Soft contacts hardness [0,1] */
	psb->m_cfg.kAHR = m_cci.m_soft_kAHR;			/* Anchors hardness [0,1] */

	if (m_cci.m_gamesoftFlag & CCD_BSB_BENDING_CONSTRAINTS)//OB_SB_GOAL)
	{
		psb->generateBendingConstraints(2,pm);
	}

	psb->m_cfg.piterations = m_cci.m_soft_piterations;
	psb->m_cfg.viterations = m_cci.m_soft_viterations;
	psb->m_cfg.diterations = m_cci.m_soft_diterations;
	psb->m_cfg.citerations = m_cci.m_soft_citerations;

	if (m_cci.m_gamesoftFlag & CCD_BSB_SHAPE_MATCHING)//OB_SB_GOAL)
	{
		psb->setPose(false,true);//
	} else
	{
		psb->setPose(true,false);
	}
	
	psb->randomizeConstraints();

	if (m_cci.m_soft_collisionflags & (CCD_BSB_COL_CL_RS+CCD_BSB_COL_CL_SS))
	{
		psb->generateClusters(m_cci.m_soft_numclusteriterations);
	}

	psb->setTotalMass(m_cci.m_mass);
	
	psb->setCollisionFlags(0);

	///create a mapping between graphics mesh vertices and soft body vertices
	{
		RAS_MeshObject* rasMesh= GetShapeInfo()->GetMesh();

		if (rasMesh && !m_softbodyMappingDone)
		{
			//printf("apply\n");
			RAS_MeshSlot::iterator it;
			RAS_MeshMaterial *mmat;
			RAS_MeshSlot *slot;
			size_t i;

			//for each material
			for (int m=0;m<rasMesh->NumMaterials();m++)
			{
				mmat = rasMesh->GetMeshMaterial(m);

				slot = mmat->m_baseslot;
				for (slot->begin(it); !slot->end(it); slot->next(it))
				{
					int index = 0;
					for (i=it.startvertex; i<it.endvertex; i++,index++) 
					{
						RAS_TexVert* vertex = &it.vertex[i];
						//search closest index, and store it in vertex
						vertex->setSoftBodyIndex(0);
						btScalar maxDistSqr = 1e30;
						btSoftBody::tNodeArray&   nodes(psb->m_nodes);
						btVector3 xyz = btVector3(vertex->getXYZ()[0],vertex->getXYZ()[1],vertex->getXYZ()[2]);
						for (int n=0;n<nodes.size();n++)
						{
							btScalar distSqr = (nodes[n].m_x - xyz).length2();
							if (distSqr<maxDistSqr)
							{
								maxDistSqr = distSqr;
								
								vertex->setSoftBodyIndex(n);
							}
						}
					}
				}
			}
		}
	}
	m_softbodyMappingDone = true;

	btTransform startTrans;
	rbci.m_motionState->getWorldTransform(startTrans);

	m_MotionState->SetWorldPosition(startTrans.getOrigin().getX(),startTrans.getOrigin().getY(),startTrans.getOrigin().getZ());
	m_MotionState->SetWorldOrientation(0,0,0,1);

	if (!m_prototypeTransformInitialized)
	{
		m_prototypeTransformInitialized = true;
		m_softBodyTransformInitialized = true;
		psb->transform(startTrans);
	}
	m_object->setCollisionFlags(m_object->getCollisionFlags() | m_cci.m_collisionFlags);
	if (m_cci.m_do_anisotropic)
		m_object->setAnisotropicFriction(m_cci.m_anisotropicFriction);
	return true;
}

bool CcdPhysicsController::CreateCharacterController()
{
	if (!m_cci.m_bCharacter)
		return false;
 
	m_object = new btPairCachingGhostObject();
	m_object->setCollisionShape(m_collisionShape);
	m_object->setCollisionFlags(btCollisionObject::CF_CHARACTER_OBJECT);

	btTransform trans;
	m_bulletMotionState->getWorldTransform(trans);
	m_object->setWorldTransform(trans);

	m_characterController = new BlenderBulletCharacterController(m_bulletMotionState,(btPairCachingGhostObject*)m_object,(btConvexShape*)m_collisionShape,m_cci.m_stepHeight);

	m_characterController->setJumpSpeed(m_cci.m_jumpSpeed);
	m_characterController->setFallSpeed(m_cci.m_fallSpeed);

	return true;
}

void CcdPhysicsController::CreateRigidbody()
{

	//btTransform trans = GetTransformFromMotionState(m_MotionState);
	m_bulletMotionState = new BlenderBulletMotionState(m_MotionState);

	///either create a btCollisionObject, btRigidBody or btSoftBody
	if (CreateSoftbody() || CreateCharacterController())
		// soft body created, done
		return;

	//create a rgid collision object
	btRigidBody::btRigidBodyConstructionInfo rbci(m_cci.m_mass,m_bulletMotionState,m_collisionShape,m_cci.m_localInertiaTensor * m_cci.m_inertiaFactor);
	rbci.m_linearDamping = m_cci.m_linearDamping;
	rbci.m_angularDamping = m_cci.m_angularDamping;
	rbci.m_friction = m_cci.m_friction;
	rbci.m_restitution = m_cci.m_restitution;
	m_object = new btRigidBody(rbci);
	
	//
	// init the rigidbody properly
	//
	
	//setMassProps this also sets collisionFlags
	//convert collision flags!
	//special case: a near/radar sensor controller should not be defined static or it will
	//generate loads of static-static collision messages on the console
	if (m_cci.m_bSensor)
	{
		// reset the flags that have been set so far
		GetCollisionObject()->setCollisionFlags(0);
		// sensor must never go to sleep: they need to detect continously
		GetCollisionObject()->setActivationState(DISABLE_DEACTIVATION);
	}
	GetCollisionObject()->setCollisionFlags(m_object->getCollisionFlags() | m_cci.m_collisionFlags);
	btRigidBody* body = GetRigidBody();

	if (body)
	{
		body->setGravity( m_cci.m_gravity);
		body->setDamping(m_cci.m_linearDamping, m_cci.m_angularDamping);

		if (!m_cci.m_bRigid)
		{
			body->setAngularFactor(0.f);
		}
		// use bullet's default contact processing theshold, blender's old default of 1 is too small here.
		// if there's really a need to change this, it should be exposed in the ui first.
//		body->setContactProcessingThreshold(m_cci.m_contactProcessingThreshold);
		body->setSleepingThresholds(gLinearSleepingTreshold, gAngularSleepingTreshold);

	}
	if (m_object && m_cci.m_do_anisotropic)
	{
		m_object->setAnisotropicFriction(m_cci.m_anisotropicFriction);
	}
		
}

static void DeleteBulletShape(btCollisionShape* shape, bool free)
{
	if (shape->getShapeType() == TRIANGLE_MESH_SHAPE_PROXYTYPE)
	{
		// shapes based on meshes use an interface that contains the vertices.
		btTriangleMeshShape* meshShape = static_cast<btTriangleMeshShape*>(shape);
		btStridingMeshInterface* meshInterface = meshShape->getMeshInterface();
		if (meshInterface)
			delete meshInterface;
	}
	else if (shape->getShapeType() == GIMPACT_SHAPE_PROXYTYPE)
	{
		btGImpactMeshShape* meshShape = static_cast<btGImpactMeshShape*>(shape);
		btStridingMeshInterface* meshInterface = meshShape->getMeshInterface();
		if (meshInterface)
			delete meshInterface;
	}
	if (free) {
		delete shape;
	}
}

bool CcdPhysicsController::DeleteControllerShape( )
{
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
				DeleteBulletShape(childShape, true);
			}
		}
		DeleteBulletShape(m_collisionShape, true);

		return true;
	}

	return false;
}

bool CcdPhysicsController::ReplaceControllerShape(btCollisionShape *newShape)
{
	
	/* Note, deleting the previous collision shape must be done already */
	/* if (m_collisionShape) DeleteControllerShape(); */

	m_object->setCollisionShape(newShape);
	m_collisionShape= newShape;
	m_cci.m_collisionShape= newShape;
	
	if (GetSoftBody()) {
		// soft body must be recreated
		m_cci.m_physicsEnv->RemoveCcdPhysicsController(this);
		delete m_object;
		m_object = NULL;
		// force complete reinitialization
		m_softbodyMappingDone = false;
		m_prototypeTransformInitialized = false;
		m_softBodyTransformInitialized = false;
		CreateSoftbody();
		assert(m_object);
		// reinsert the new body
		m_cci.m_physicsEnv->AddCcdPhysicsController(this);
	}
	
	/* Copied from CcdPhysicsEnvironment::addCcdPhysicsController() */
	
	/* without this, an object can rest on the old physics mesh
	 * and not move to account for the physics mesh, even with 'nosleep' */ 
	btSoftRigidDynamicsWorld* dw= GetPhysicsEnvironment()->GetDynamicsWorld();
	btCollisionObjectArray &obarr= dw->getCollisionObjectArray();
	btCollisionObject *ob;
	btBroadphaseProxy* proxy;

	for (int i= 0; i < obarr.size(); i++) {
		ob= obarr[i];
		if (ob->getCollisionShape() == newShape) {
			proxy = ob->getBroadphaseHandle();
			
			if (proxy)
				dw->getPairCache()->cleanProxyFromPairs(proxy,dw->getDispatcher());
		}
	}
	
	return true;
}

CcdPhysicsController::~CcdPhysicsController()
{
	//will be reference counted, due to sharing
	if (m_cci.m_physicsEnv)
		m_cci.m_physicsEnv->RemoveCcdPhysicsController(this);

	if (m_MotionState)
		delete m_MotionState;
	if (m_bulletMotionState)
		delete m_bulletMotionState;
	if (m_characterController)
		delete m_characterController;
	delete m_object;

	DeleteControllerShape();

	if (m_shapeInfo)
	{
		m_shapeInfo->Release();
	}
}


/**
 * SynchronizeMotionStates ynchronizes dynas, kinematic and deformable entities (and do 'late binding')
 */
bool		CcdPhysicsController::SynchronizeMotionStates(float time)
{
	//sync non-static to motionstate, and static from motionstate (todo: add kinematic etc.)

	btSoftBody* sb = GetSoftBody();
	if (sb)
	{
		if (sb->m_pose.m_bframe) 
		{
			btVector3 worldPos = sb->m_pose.m_com;
			btQuaternion worldquat;
			btMatrix3x3	trs = sb->m_pose.m_rot*sb->m_pose.m_scl;
			trs.getRotation(worldquat);
			m_MotionState->SetWorldPosition(worldPos[0],worldPos[1],worldPos[2]);
			m_MotionState->SetWorldOrientation(worldquat[0],worldquat[1],worldquat[2],worldquat[3]);
		}
		else 
		{
			btVector3 aabbMin,aabbMax;
			sb->getAabb(aabbMin,aabbMax);
			btVector3 worldPos  = (aabbMax+aabbMin)*0.5f;
			m_MotionState->SetWorldPosition(worldPos[0],worldPos[1],worldPos[2]);
		}
		m_MotionState->CalculateWorldTransformations();
		return true;
	}

	btRigidBody* body = GetRigidBody();

	if (body && !body->isStaticObject())
	{
		
		if ((m_cci.m_clamp_vel_max>0.0) || (m_cci.m_clamp_vel_min>0.0))
		{
			const btVector3& linvel = body->getLinearVelocity();
			float len= linvel.length();
			
			if ((m_cci.m_clamp_vel_max>0.0) && (len > m_cci.m_clamp_vel_max))
					body->setLinearVelocity(linvel * (m_cci.m_clamp_vel_max / len));
			
			else if ((m_cci.m_clamp_vel_min>0.0) && btFuzzyZero(len)==0 && (len < m_cci.m_clamp_vel_min))
				body->setLinearVelocity(linvel * (m_cci.m_clamp_vel_min / len));
		}
		
		const btTransform& xform = body->getCenterOfMassTransform();
		const btMatrix3x3& worldOri = xform.getBasis();
		const btVector3& worldPos = xform.getOrigin();
		float ori[12];
		worldOri.getOpenGLSubMatrix(ori);
		m_MotionState->SetWorldOrientation(ori);
		m_MotionState->SetWorldPosition(worldPos[0],worldPos[1],worldPos[2]);
		m_MotionState->CalculateWorldTransformations();

		float scale[3];
		m_MotionState->GetWorldScaling(scale[0],scale[1],scale[2]);
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
				
		SetCenterOfMassTransform(newTrans);
		//need to keep track of previous position for friction effects...
		
		m_MotionState->calculateWorldTransformations();
*/
		float scale[3];
		m_MotionState->GetWorldScaling(scale[0],scale[1],scale[2]);
		btVector3 scaling(scale[0],scale[1],scale[2]);
		GetCollisionShape()->setLocalScaling(scaling);
	}
	return true;

}

		/**
		 * WriteMotionStateToDynamics synchronizes dynas, kinematic and deformable entities (and do 'late binding')
		 */
		
void		CcdPhysicsController::WriteMotionStateToDynamics(bool nondynaonly)
{
	btTransform& xform = CcdPhysicsController::GetTransformFromMotionState(m_MotionState);
	SetCenterOfMassTransform(xform);
}

void		CcdPhysicsController::WriteDynamicsToMotionState()
{
}
		// controller replication
void		CcdPhysicsController::PostProcessReplica(class PHY_IMotionState* motionstate,class PHY_IPhysicsController* parentctrl)
{
	SetParentCtrl((CcdPhysicsController*)parentctrl);
	m_softBodyTransformInitialized=false;
	m_MotionState = motionstate;
	m_registerCount = 0;
	m_collisionShape = NULL;

	// always create a new shape to avoid scaling bug
	if (m_shapeInfo)
	{
		m_shapeInfo->AddRef();
		m_collisionShape = m_shapeInfo->CreateBulletShape(m_cci.m_margin, m_cci.m_bGimpact, !m_cci.m_bSoft);

		if (m_collisionShape)
		{
			// new shape has no scaling, apply initial scaling
			//m_collisionShape->setMargin(m_cci.m_margin);
			m_collisionShape->setLocalScaling(m_cci.m_scaling);
			
			if (m_cci.m_mass)
				m_collisionShape->calculateLocalInertia(m_cci.m_mass, m_cci.m_localInertiaTensor);
		}
	}

	// load some characterists that are not 
	btRigidBody* oldbody = GetRigidBody();
	m_object = 0;
	CreateRigidbody();
	btRigidBody* body = GetRigidBody();
	if (body)
	{
		if (m_cci.m_mass)
		{
			body->setMassProps(m_cci.m_mass, m_cci.m_localInertiaTensor * m_cci.m_inertiaFactor);
		}

		if (oldbody)
		{
			body->setLinearFactor(oldbody->getLinearFactor());
			body->setAngularFactor(oldbody->getAngularFactor());
			if (oldbody->getActivationState() == DISABLE_DEACTIVATION)
				body->setActivationState(DISABLE_DEACTIVATION);
		}
	}
	// sensor object are added when needed
	if (!m_cci.m_bSensor)
		m_cci.m_physicsEnv->AddCcdPhysicsController(this);


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

void	CcdPhysicsController::SetPhysicsEnvironment(class PHY_IPhysicsEnvironment *env)
{
	// can safely assume CCD environment
	CcdPhysicsEnvironment *physicsEnv = static_cast<CcdPhysicsEnvironment*>(env);

	if (m_cci.m_physicsEnv != physicsEnv) 
	{
		// since the environment is changing, we must also move the controler to the
		// new environment. Note that we don't handle sensor explicitly: this
		// function can be called on sensor but only when they are not registered
		if (m_cci.m_physicsEnv->RemoveCcdPhysicsController(this))
		{
			physicsEnv->AddCcdPhysicsController(this);

			// Set the object to be active so it can at least by evaluated once.
			// This fixes issues with static objects not having their physics meshes
			// in the right spot when lib loading.
			this->GetCollisionObject()->setActivationState(ACTIVE_TAG);
		}
		m_cci.m_physicsEnv = physicsEnv;
	}
}

void	CcdPhysicsController::SetCenterOfMassTransform(btTransform& xform)
{
	btRigidBody* body = GetRigidBody();
	if (body)
	{
		body->setCenterOfMassTransform(xform);
	} else
	{
		//either collision object or soft body?
		if (GetSoftBody())
		{

		} else
		{

			if (m_object->isStaticOrKinematicObject())
			{
				m_object->setInterpolationWorldTransform(m_object->getWorldTransform());
			} else
			{
				m_object->setInterpolationWorldTransform(xform);
			}
			if (body)
			{
				body->setInterpolationLinearVelocity(body->getLinearVelocity());
				body->setInterpolationAngularVelocity(body->getAngularVelocity());
				body->updateInertiaTensor();
			}
			m_object->setWorldTransform(xform);
		}
	}
}

		// kinematic methods
void		CcdPhysicsController::RelativeTranslate(const MT_Vector3& dlocin,bool local)
{
	if (m_object)
	{
		m_object->activate(true);
		if (m_object->isStaticObject())
		{
			if (!m_cci.m_bSensor)
				m_object->setCollisionFlags(m_object->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
			// kinematic object should not set the transform, it disturbs the velocity interpolation
			return;
		}

		btVector3 dloc(dlocin.x(), dlocin.y(), dlocin.z());
		btTransform xform = m_object->getWorldTransform();
	
		if (local)
			dloc = xform.getBasis()*dloc;

		xform.setOrigin(xform.getOrigin() + dloc);
		SetCenterOfMassTransform(xform);
	}

}

void		CcdPhysicsController::RelativeRotate(const MT_Matrix3x3& rotval,bool local)
{
	if (m_object)
	{
		m_object->activate(true);
		if (m_object->isStaticObject())
		{
			if (!m_cci.m_bSensor)
				m_object->setCollisionFlags(m_object->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
			// kinematic object should not set the transform, it disturbs the velocity interpolation
			return;
		}

		btMatrix3x3 drotmat(rotval[0].x(), rotval[0].y(), rotval[0].z(),
		        rotval[1].x(), rotval[1].y(), rotval[1].z(),
		        rotval[2].x(), rotval[2].y(), rotval[2].z());


		btMatrix3x3 currentOrn;
		GetWorldOrientation(currentOrn);

		btTransform xform = m_object->getWorldTransform();
		
		xform.setBasis(xform.getBasis()*(local ? 
		drotmat : (currentOrn.inverse() * drotmat * currentOrn)));

		SetCenterOfMassTransform(xform);
	}
}


void CcdPhysicsController::GetWorldOrientation(btMatrix3x3& mat)
{
	float ori[12];
	m_MotionState->GetWorldOrientation(ori);
	mat.setFromOpenGLSubMatrix(ori);
}

MT_Matrix3x3 CcdPhysicsController::GetOrientation()
{
	btMatrix3x3 orn = m_object->getWorldTransform().getBasis();
	return MT_Matrix3x3(orn[0][0], orn[0][1], orn[0][2], orn[1][0], orn[1][1], orn[1][2], orn[2][0], orn[2][1], orn[2][2]);
}

void		CcdPhysicsController::SetOrientation(const MT_Matrix3x3& orn)
{
	btMatrix3x3 btmat(orn[0][0], orn[0][1], orn[0][2], orn[1][0], orn[1][1], orn[1][2], orn[2][0], orn[2][1], orn[2][2]);
	SetWorldOrientation(btmat);
}

void CcdPhysicsController::SetWorldOrientation(const btMatrix3x3& orn)
{
	if (m_object)
	{
		m_object->activate(true);
		if (m_object->isStaticObject() && !m_cci.m_bSensor)
		{
			m_object->setCollisionFlags(m_object->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
		}
		// not required
		//m_MotionState->setWorldOrientation(quatImag0,quatImag1,quatImag2,quatReal);
		btTransform xform  = m_object->getWorldTransform();
		xform.setBasis(orn);
		SetCenterOfMassTransform(xform);
		// not required
		//m_bulletMotionState->setWorldTransform(xform);
		//only once!
		if (!m_softBodyTransformInitialized && GetSoftBody())
		{
			m_softbodyStartTrans.setBasis(orn);
			xform.setOrigin(m_softbodyStartTrans.getOrigin());
			GetSoftBody()->transform(xform);
			m_softBodyTransformInitialized = true;
		}

	}

}

void		CcdPhysicsController::SetPosition(const MT_Vector3& pos)
{
	if (m_object)
	{
		m_object->activate(true);
		if (m_object->isStaticObject())
		{
			if (!m_cci.m_bSensor)
				m_object->setCollisionFlags(m_object->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
			// kinematic object should not set the transform, it disturbs the velocity interpolation
			return;
		}
		// not required, this function is only used to update the physic controller
		//m_MotionState->setWorldPosition(posX,posY,posZ);
		btTransform xform  = m_object->getWorldTransform();
		xform.setOrigin(btVector3(pos.x(), pos.y(), pos.z()));
		SetCenterOfMassTransform(xform);
		if (!m_softBodyTransformInitialized)
			m_softbodyStartTrans.setOrigin(xform.getOrigin());
		// not required
		//m_bulletMotionState->setWorldTransform(xform);
	}
}

void CcdPhysicsController::ForceWorldTransform(const btMatrix3x3& mat, const btVector3& pos)
{
	if (m_object)
	{
		btTransform& xform = m_object->getWorldTransform();
		xform.setBasis(mat);
		xform.setOrigin(pos);
	}
}


void		CcdPhysicsController::ResolveCombinedVelocities(float linvelX,float linvelY,float linvelZ,float angVelX,float angVelY,float angVelZ)
{
}

void	CcdPhysicsController::SuspendDynamics(bool ghost)
{
	btRigidBody *body = GetRigidBody();
	if (body && !m_suspended && !GetConstructionInfo().m_bSensor)
	{
		btBroadphaseProxy* handle = body->getBroadphaseHandle();

		m_savedCollisionFlags = body->getCollisionFlags();
		m_savedMass = GetMass();
		m_savedCollisionFilterGroup = handle->m_collisionFilterGroup;
		m_savedCollisionFilterMask = handle->m_collisionFilterMask;
		m_suspended = true;
		GetPhysicsEnvironment()->UpdateCcdPhysicsController(this,
			0.0,
			btCollisionObject::CF_STATIC_OBJECT|((ghost)?btCollisionObject::CF_NO_CONTACT_RESPONSE:(m_savedCollisionFlags&btCollisionObject::CF_NO_CONTACT_RESPONSE)),
			btBroadphaseProxy::StaticFilter,
			btBroadphaseProxy::AllFilter ^ btBroadphaseProxy::StaticFilter);
	}
}

void	CcdPhysicsController::RestoreDynamics()
{
	btRigidBody *body = GetRigidBody();
	if (body && m_suspended)
	{
		// before make sure any position change that was done in this logic frame are accounted for
		SetTransform();
		GetPhysicsEnvironment()->UpdateCcdPhysicsController(this,
			m_savedMass,
			m_savedCollisionFlags,
			m_savedCollisionFilterGroup,
			m_savedCollisionFilterMask);
		body->activate();
		m_suspended = false;
	}
}

void 		CcdPhysicsController::GetPosition(MT_Vector3&	pos) const
{
	const btTransform& xform = m_object->getWorldTransform();
	pos[0] = xform.getOrigin().x();
	pos[1] = xform.getOrigin().y();
	pos[2] = xform.getOrigin().z();
}

void		CcdPhysicsController::SetScaling(const MT_Vector3& scale)
{
	if (!btFuzzyZero(m_cci.m_scaling.x()-scale.x()) ||
	    !btFuzzyZero(m_cci.m_scaling.y()-scale.y()) ||
	    !btFuzzyZero(m_cci.m_scaling.z()-scale.z()))
	{
		m_cci.m_scaling = btVector3(scale.x(),scale.y(),scale.z());

		if (m_object && m_object->getCollisionShape())
		{
			m_object->activate(true); // without this, sleeping objects scale wont be applied in bullet if python changes the scale - Campbell.
			m_object->getCollisionShape()->setLocalScaling(m_cci.m_scaling);
			
			//printf("no inertia recalc for fixed objects with mass=0\n");
			btRigidBody* body = GetRigidBody();
			if (body && m_cci.m_mass)
			{
				body->getCollisionShape()->calculateLocalInertia(m_cci.m_mass, m_cci.m_localInertiaTensor);
				body->setMassProps(m_cci.m_mass, m_cci.m_localInertiaTensor * m_cci.m_inertiaFactor);
			} 
			
		}
	}
}

void CcdPhysicsController::SetTransform()
{
	btVector3 pos;
	btVector3 scale;
	float ori[12];
	m_MotionState->GetWorldPosition(pos.m_floats[0],pos.m_floats[1],pos.m_floats[2]);
	m_MotionState->GetWorldScaling(scale.m_floats[0],scale.m_floats[1],scale.m_floats[2]);
	m_MotionState->GetWorldOrientation(ori);
	btMatrix3x3 rot(ori[0], ori[4], ori[8],
					ori[1], ori[5], ori[9],
					ori[2], ori[6], ori[10]);
	ForceWorldTransform(rot, pos);

	if (!IsDynamic() && !GetConstructionInfo().m_bSensor && !GetCharacterController())
	{
		btCollisionObject* object = GetRigidBody();
		object->setActivationState(ACTIVE_TAG);
		object->setCollisionFlags(object->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
	}
}

MT_Scalar	CcdPhysicsController::GetMass()
{
	if (GetSoftBody())
		return GetSoftBody()->getTotalMass();

	MT_Scalar invmass = 0.f;
	if (GetRigidBody())
		invmass = GetRigidBody()->getInvMass();
	if (invmass)
		return 1.f/invmass;
	return 0.f;

}

void CcdPhysicsController::SetMass(MT_Scalar newmass)
{
	btRigidBody *body = GetRigidBody();
	if (body && !m_suspended && newmass>MT_EPSILON && GetMass()>MT_EPSILON)
	{
		btVector3 grav = body->getGravity();
		btVector3 accel = grav / GetMass();

		btBroadphaseProxy* handle = body->getBroadphaseHandle();
		GetPhysicsEnvironment()->UpdateCcdPhysicsController(this,
			newmass,
			body->getCollisionFlags(),
			handle->m_collisionFilterGroup,
			handle->m_collisionFilterMask);
		body->setGravity(accel);
	}
}
		
		// physics methods
void		CcdPhysicsController::ApplyTorque(const MT_Vector3&  torquein,bool local)
{
	btVector3 torque(torquein.x(),torquein.y(),torquein.z());
	btTransform xform = m_object->getWorldTransform();
	

	if (m_object && torque.length2() > (SIMD_EPSILON*SIMD_EPSILON))
	{
		btRigidBody* body = GetRigidBody();
		m_object->activate();
		if (m_object->isStaticObject())
		{
			if (!m_cci.m_bSensor)
				m_object->setCollisionFlags(m_object->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
			return;
		}
		if (local)
		{
			torque	= xform.getBasis()*torque;
		}
		if (body)
		{
			if 	(m_cci.m_bRigid)
			{
				body->applyTorque(torque);
			}
			else
			{
				//workaround for incompatibility between 'DYNAMIC' game object, and angular factor
				//a DYNAMIC object has some inconsistency: it has no angular effect due to collisions, but still has torque
				const btVector3 angFac = body->getAngularFactor();
				btVector3 tmpFac(1,1,1);
				body->setAngularFactor(tmpFac);
				body->applyTorque(torque);
				body->setAngularFactor(angFac);
			} 
		} 
	}
}

void		CcdPhysicsController::ApplyForce(const MT_Vector3& forcein,bool local)
{
	btVector3 force(forcein.x(),forcein.y(),forcein.z());
	

	if (m_object && force.length2() > (SIMD_EPSILON*SIMD_EPSILON))
	{
		m_object->activate();
		if (m_object->isStaticObject())
		{
			if (!m_cci.m_bSensor)
				m_object->setCollisionFlags(m_object->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
			return;
		}
		btTransform xform = m_object->getWorldTransform();
		
		if (local)
		{
			force	= xform.getBasis()*force;
		}
		btRigidBody* body = GetRigidBody();
		if (body)
			body->applyCentralForce(force);
		btSoftBody* soft = GetSoftBody();
		if (soft)
		{
			// the force is applied on each node, must reduce it in the same extend
			if (soft->m_nodes.size() > 0)
				force /= soft->m_nodes.size();
			soft->addForce(force);
		}
	}
}
void		CcdPhysicsController::SetAngularVelocity(const MT_Vector3& ang_vel,bool local)
{
	btVector3 angvel(ang_vel.x(),ang_vel.y(),ang_vel.z());
	if (m_object && angvel.length2() > (SIMD_EPSILON*SIMD_EPSILON))
	{
		m_object->activate(true);
		if (m_object->isStaticObject())
		{
			if (!m_cci.m_bSensor)
				m_object->setCollisionFlags(m_object->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
			return;
		}
		btTransform xform = m_object->getWorldTransform();
		if (local)
		{
			angvel	= xform.getBasis()*angvel;
		}
		btRigidBody* body = GetRigidBody();
		if (body)
			body->setAngularVelocity(angvel);
	}

}
void		CcdPhysicsController::SetLinearVelocity(const MT_Vector3& lin_vel,bool local)
{

	btVector3 linVel(lin_vel.x(),lin_vel.y(),lin_vel.z());
	if (m_object/* && linVel.length2() > (SIMD_EPSILON*SIMD_EPSILON)*/)
	{
		m_object->activate(true);
		if (m_object->isStaticObject())
		{
			if (!m_cci.m_bSensor)
				m_object->setCollisionFlags(m_object->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
			return;
		}
		
		btSoftBody* soft = GetSoftBody();
		if (soft)
		{
			if (local)
			{
				linVel	= m_softbodyStartTrans.getBasis()*linVel;
			}
			soft->setVelocity(linVel);
		} else
		{
			btTransform xform = m_object->getWorldTransform();
			if (local)
			{
				linVel	= xform.getBasis()*linVel;
			}
			btRigidBody* body = GetRigidBody();
			if (body)
				body->setLinearVelocity(linVel);
		}
	}
}
void		CcdPhysicsController::ApplyImpulse(const MT_Point3& attach, const MT_Vector3& impulsein, bool local)
{
	btVector3 pos;
	btVector3 impulse(impulsein.x(), impulsein.y(), impulsein.z());

	if (m_object && impulse.length2() > (SIMD_EPSILON*SIMD_EPSILON))
	{
		m_object->activate();
		if (m_object->isStaticObject())
		{
			if (!m_cci.m_bSensor)
				m_object->setCollisionFlags(m_object->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
			return;
		}
		
		btTransform xform = m_object->getWorldTransform();

		if (local)
		{
			pos = btVector3(attach.x(), attach.y(), attach.z());
			impulse = xform.getBasis() * impulse;
		}
		else {
			/* If the point of impulse application is not equal to the object position
			 * then an angular momentum is generated in the object*/
			pos = btVector3(attach.x()-xform.getOrigin().x(), attach.y()-xform.getOrigin().y(), attach.z()-xform.getOrigin().z());
		}

		btRigidBody* body = GetRigidBody();
		if (body)
			body->applyImpulse(impulse,pos);
			
	}

}

void		CcdPhysicsController::Jump()
{
	if (m_object && m_characterController)
		m_characterController->jump();
}

void		CcdPhysicsController::SetActive(bool active)
{
}

float		CcdPhysicsController::GetLinearDamping() const
{
	const btRigidBody* body = GetRigidBody();
	if (body)
		return body->getLinearDamping();
	return 0;
}

float		CcdPhysicsController::GetAngularDamping() const
{
	const	btRigidBody* body = GetRigidBody();
	if (body)
		return body->getAngularDamping();
	return 0;
}

void		CcdPhysicsController::SetLinearDamping(float damping)
{
	SetDamping(damping, GetAngularDamping());
}

void		CcdPhysicsController::SetAngularDamping(float damping)
{
	SetDamping(GetLinearDamping(), damping);
}

void		CcdPhysicsController::SetDamping(float linear, float angular)
{
	btRigidBody* body = GetRigidBody();
	if (!body) return;

	body->setDamping(linear, angular);
}


		// reading out information from physics
MT_Vector3		CcdPhysicsController::GetLinearVelocity()
{
	btRigidBody* body = GetRigidBody();
	if (body)
	{
		const btVector3& linvel = body->getLinearVelocity();
		return MT_Vector3(linvel.x(), linvel.y(), linvel.z());
	}

	return MT_Vector3(0.f, 0.f, 0.f);
}

MT_Vector3		CcdPhysicsController::GetAngularVelocity()
{
	btRigidBody* body = GetRigidBody();
	if (body)
	{
		const btVector3& angvel= body->getAngularVelocity();
		return MT_Vector3(angvel.x(), angvel.y(), angvel.z());
	}

	return MT_Vector3(0.f, 0.f, 0.f);
}

MT_Vector3		CcdPhysicsController::GetVelocity(const MT_Point3 &posin)
{
	btVector3 pos(posin.x(), posin.y(), posin.z());
	btRigidBody* body = GetRigidBody();
	if (body)
	{
		btVector3 linvel = body->getVelocityInLocalPoint(pos);
		return MT_Vector3(linvel.x(), linvel.y(), linvel.z());
	}

	return MT_Vector3(0.f, 0.f, 0.f);
}

MT_Vector3 CcdPhysicsController::GetLocalInertia()
{
	MT_Vector3 inertia(0.f, 0.f, 0.f);
	btVector3 inv_inertia;
	if (GetRigidBody()) {
		inv_inertia = GetRigidBody()->getInvInertiaDiagLocal();
		if (!btFuzzyZero(inv_inertia.getX()) &&
		        !btFuzzyZero(inv_inertia.getY()) &&
		        !btFuzzyZero(inv_inertia.getZ()))
			inertia = MT_Vector3(1.f/inv_inertia.getX(), 1.f/inv_inertia.getY(), 1.f/inv_inertia.getZ());
	}
	return inertia;
}

		// dyna's that are rigidbody are free in orientation, dyna's with non-rigidbody are restricted 
void		CcdPhysicsController::SetRigidBody(bool rigid)
{
	btRigidBody* body = GetRigidBody();
	if (body)
	{
		m_cci.m_bRigid = rigid;
		if (!rigid) {
			body->setAngularFactor(0.f);
			body->setAngularVelocity(btVector3(0.f, 0.f, 0.f));
		}
		else
			body->setAngularFactor(m_cci.m_angularFactor);
	}
}

		// clientinfo for raycasts for example
void*		CcdPhysicsController::GetNewClientInfo()
{
	return m_newClientInfo;
}
void		CcdPhysicsController::SetNewClientInfo(void* clientinfo)
{
	m_newClientInfo = clientinfo;

	if (m_cci.m_bSensor)
	{
		// use a different callback function for sensor object,
		// bullet will not synchronize, we must do it explicitly
		SG_Callbacks& callbacks = KX_GameObject::GetClientObject((KX_ClientObjectInfo*)clientinfo)->GetSGNode()->GetCallBackFunctions();
		callbacks.m_updatefunc = KX_GameObject::SynchronizeTransformFunc;
	}
}


void	CcdPhysicsController::UpdateDeactivation(float timeStep)
{
	btRigidBody* body = GetRigidBody();
	if (body)
	{
		body->updateDeactivation( timeStep);
	}
}

bool CcdPhysicsController::WantsSleeping()
{
	btRigidBody* body = GetRigidBody();
	if (body)
	{
		return body->wantsSleeping();
	}
	//check it out
	return true;
}
/* This function dynamically adds the collision shape of another controller to
 * the current controller shape provided it is a compound shape.
 * The idea is that dynamic parenting on a compound object will dynamically extend the shape
 */
void    CcdPhysicsController::AddCompoundChild(PHY_IPhysicsController* child)
{
	if (child == NULL || !IsCompound())
		return;
	// other controller must be a bullet controller too
	// verify that body and shape exist and match
	CcdPhysicsController* childCtrl = dynamic_cast<CcdPhysicsController*>(child);
	btRigidBody* rootBody = GetRigidBody();
	btRigidBody* childBody = childCtrl->GetRigidBody();
	if (!rootBody || !childBody)
		return;
	const btCollisionShape* rootShape = rootBody->getCollisionShape();
	const btCollisionShape* childShape = childBody->getCollisionShape();
	if (!rootShape ||
		!childShape ||
		rootShape->getShapeType() != COMPOUND_SHAPE_PROXYTYPE ||
		childShape->getShapeType() == COMPOUND_SHAPE_PROXYTYPE)
		return;
	btCompoundShape* compoundShape = (btCompoundShape*)rootShape;
	// compute relative transformation between parent and child
	btTransform rootTrans;
	btTransform childTrans;
	rootBody->getMotionState()->getWorldTransform(rootTrans);
	childBody->getMotionState()->getWorldTransform(childTrans);
	btVector3 rootScale = rootShape->getLocalScaling();
	rootScale[0] = 1.0/rootScale[0];
	rootScale[1] = 1.0/rootScale[1];
	rootScale[2] = 1.0/rootScale[2];
	// relative scale = child_scale/parent_scale
	btVector3 relativeScale = childShape->getLocalScaling()*rootScale;
	btMatrix3x3 rootRotInverse = rootTrans.getBasis().transpose();
	// relative pos = parent_rot^-1 * ((parent_pos-child_pos)/parent_scale)
	btVector3 relativePos = rootRotInverse*((childTrans.getOrigin()-rootTrans.getOrigin())*rootScale);
	// relative rot = parent_rot^-1 * child_rot
	btMatrix3x3 relativeRot = rootRotInverse*childTrans.getBasis();
	// create a proxy shape info to store the transformation
	CcdShapeConstructionInfo* proxyShapeInfo = new CcdShapeConstructionInfo();
	// store the transformation to this object shapeinfo
	proxyShapeInfo->m_childTrans.setOrigin(relativePos);
	proxyShapeInfo->m_childTrans.setBasis(relativeRot);
	proxyShapeInfo->m_childScale.setValue(relativeScale[0], relativeScale[1], relativeScale[2]);
	// we will need this to make sure that we remove the right proxy later when unparenting
	proxyShapeInfo->m_userData = childCtrl;
	proxyShapeInfo->SetProxy(childCtrl->GetShapeInfo()->AddRef());
	// add to parent compound shapeinfo (increments ref count)
	GetShapeInfo()->AddShape(proxyShapeInfo);
	// create new bullet collision shape from the object shapeinfo and set scaling
	btCollisionShape* newChildShape = proxyShapeInfo->CreateBulletShape(childCtrl->GetMargin(), childCtrl->GetConstructionInfo().m_bGimpact, true);
	newChildShape->setLocalScaling(relativeScale);
	// add bullet collision shape to parent compound collision shape
	compoundShape->addChildShape(proxyShapeInfo->m_childTrans,newChildShape);
	// proxyShapeInfo is not needed anymore, release it
	proxyShapeInfo->Release();
	// remember we created this shape
	childCtrl->m_bulletChildShape = newChildShape;
	// recompute inertia of parent
	if (!rootBody->isStaticOrKinematicObject())
	{
		btVector3 localInertia;
		float mass = 1.f/rootBody->getInvMass();
		compoundShape->calculateLocalInertia(mass,localInertia);
		rootBody->setMassProps(mass,localInertia);
	}
	// must update the broadphase cache,
	GetPhysicsEnvironment()->RefreshCcdPhysicsController(this);
	// remove the children
	GetPhysicsEnvironment()->DisableCcdPhysicsController(childCtrl);
}

/* Reverse function of the above, it will remove a shape from a compound shape
 * provided that the former was added to the later using  AddCompoundChild()
 */
void    CcdPhysicsController::RemoveCompoundChild(PHY_IPhysicsController* child)
{
	if (child == NULL || !IsCompound())
		return;
	// other controller must be a bullet controller too
	// verify that body and shape exist and match
	CcdPhysicsController* childCtrl = dynamic_cast<CcdPhysicsController*>(child);
	btRigidBody* rootBody = GetRigidBody();
	btRigidBody* childBody = childCtrl->GetRigidBody();
	if (!rootBody || !childBody)
		return;
	const btCollisionShape* rootShape = rootBody->getCollisionShape();
	if (!rootShape ||
		rootShape->getShapeType() != COMPOUND_SHAPE_PROXYTYPE)
		return;
	btCompoundShape* compoundShape = (btCompoundShape*)rootShape;
	// retrieve the shapeInfo
	CcdShapeConstructionInfo* childShapeInfo = childCtrl->GetShapeInfo();
	CcdShapeConstructionInfo* rootShapeInfo = GetShapeInfo();
	// and verify that the child is part of the parent
	int i = rootShapeInfo->FindChildShape(childShapeInfo, childCtrl);
	if (i < 0)
		return;
	rootShapeInfo->RemoveChildShape(i);
	if (childCtrl->m_bulletChildShape)
	{
		int numChildren = compoundShape->getNumChildShapes();
		for (i=0; i<numChildren; i++)
		{
			if (compoundShape->getChildShape(i) == childCtrl->m_bulletChildShape)
			{
				compoundShape->removeChildShapeByIndex(i);
				compoundShape->recalculateLocalAabb();
				break;
			}
		}
		delete childCtrl->m_bulletChildShape;
		childCtrl->m_bulletChildShape = NULL;
	}
	// recompute inertia of parent
	if (!rootBody->isStaticOrKinematicObject())
	{
		btVector3 localInertia;
		float mass = 1.f/rootBody->getInvMass();
		compoundShape->calculateLocalInertia(mass,localInertia);
		rootBody->setMassProps(mass,localInertia);
	}
	// must update the broadphase cache,
	GetPhysicsEnvironment()->RefreshCcdPhysicsController(this);
	// reactivate the children
	GetPhysicsEnvironment()->EnableCcdPhysicsController(childCtrl);
}

PHY_IPhysicsController* CcdPhysicsController::GetReplica()
{
	CcdPhysicsController* replica = new CcdPhysicsController(*this);
	return replica;
}

// Keeping this separate for now, maybe we can combine it with GetReplica()...
PHY_IPhysicsController*	CcdPhysicsController::GetReplicaForSensors()
{
	// This is used only to replicate Near and Radar sensor controllers
	// The replication of object physics controller is done in KX_BulletPhysicsController::GetReplica()
	CcdConstructionInfo cinfo = m_cci;
	if (m_shapeInfo)
	{
		// This situation does not normally happen
		cinfo.m_collisionShape = m_shapeInfo->CreateBulletShape(m_cci.m_margin, m_cci.m_bGimpact, !m_cci.m_bSoft);
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

/* Refresh the physics object from either an object or a mesh.
 * from_gameobj and from_meshobj can be NULL
 *
 * when setting the mesh, the following vars get priority
 * 1) from_meshobj - creates the phys mesh from RAS_MeshObject
 * 2) from_gameobj - creates the phys mesh from the DerivedMesh where possible, else the RAS_MeshObject
 * 3) this - update the phys mesh from DerivedMesh or RAS_MeshObject
 *
 * Most of the logic behind this is in shapeInfo->UpdateMesh(...)
 */
bool CcdPhysicsController::ReinstancePhysicsShape(KX_GameObject *from_gameobj, RAS_MeshObject *from_meshobj)
{
	CcdShapeConstructionInfo	*shapeInfo;

	shapeInfo = this->GetShapeInfo();

	if (shapeInfo->m_shapeType != PHY_SHAPE_MESH/* || spc->GetSoftBody()*/)
		return false;

	this->DeleteControllerShape();

	if (from_gameobj==NULL && from_meshobj==NULL)
		from_gameobj = KX_GameObject::GetClientObject((KX_ClientObjectInfo*)this->GetNewClientInfo());

	/* updates the arrays used for making the new bullet mesh */
	shapeInfo->UpdateMesh(from_gameobj, from_meshobj);

	/* create the new bullet mesh */
	CcdConstructionInfo& cci = this->GetConstructionInfo();
	btCollisionShape* bm= shapeInfo->CreateBulletShape(cci.m_margin, cci.m_bGimpact, !cci.m_bSoft);

	this->ReplaceControllerShape(bm);
	return true;
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

void	DefaultMotionState::GetWorldPosition(float& posX,float& posY,float& posZ)
{
	posX = m_worldTransform.getOrigin().x();
	posY = m_worldTransform.getOrigin().y();
	posZ = m_worldTransform.getOrigin().z();
}

void	DefaultMotionState::GetWorldScaling(float& scaleX,float& scaleY,float& scaleZ)
{
	scaleX = m_localScaling.getX();
	scaleY = m_localScaling.getY();
	scaleZ = m_localScaling.getZ();
}

void	DefaultMotionState::GetWorldOrientation(float& quatIma0,float& quatIma1,float& quatIma2,float& quatReal)
{
	btQuaternion quat = m_worldTransform.getRotation();
	quatIma0 = quat.x();
	quatIma1 = quat.y();
	quatIma2 = quat.z();
	quatReal = quat[3];
}
		
void	DefaultMotionState::GetWorldOrientation(float* ori)
{
	m_worldTransform.getBasis().getOpenGLSubMatrix(ori);
}

void	DefaultMotionState::SetWorldOrientation(const float* ori)
{
	m_worldTransform.getBasis().setFromOpenGLSubMatrix(ori);
}
void	DefaultMotionState::SetWorldPosition(float posX,float posY,float posZ)
{
	btVector3 pos(posX,posY,posZ);
	m_worldTransform.setOrigin( pos );
}

void	DefaultMotionState::SetWorldOrientation(float quatIma0,float quatIma1,float quatIma2,float quatReal)
{
	btQuaternion orn(quatIma0,quatIma1,quatIma2,quatReal);
	m_worldTransform.setRotation( orn );
}
		
void	DefaultMotionState::CalculateWorldTransformations()
{

}

// Shape constructor
std::map<RAS_MeshObject*, CcdShapeConstructionInfo*> CcdShapeConstructionInfo::m_meshShapeMap;

CcdShapeConstructionInfo* CcdShapeConstructionInfo::FindMesh(RAS_MeshObject* mesh, struct DerivedMesh* dm, bool polytope)
{
	if (polytope || dm)
		// not yet supported
		return NULL;

	std::map<RAS_MeshObject*,CcdShapeConstructionInfo*>::const_iterator mit = m_meshShapeMap.find(mesh);
	if (mit != m_meshShapeMap.end())
		return mit->second;
	return NULL;
}

bool CcdShapeConstructionInfo::SetMesh(RAS_MeshObject* meshobj, DerivedMesh* dm, bool polytope)
{
	int numpolys, numverts;

	// assume no shape information
	// no support for dynamic change of shape yet
	assert(IsUnused());
	m_shapeType = PHY_SHAPE_NONE;
	m_meshObject = NULL;
	bool free_dm = false;

	// No mesh object or mesh has no polys
	if (!meshobj || meshobj->HasColliderPolygon()==false) {
		m_vertexArray.clear();
		m_polygonIndexArray.clear();
		m_triFaceArray.clear();
		m_triFaceUVcoArray.clear();
		return false;
	}

	if (!dm) {
		free_dm = true;
		dm = CDDM_from_mesh(meshobj->GetMesh());
		DM_ensure_tessface(dm);
	}

	MVert *mvert = dm->getVertArray(dm);
	MFace *mface = dm->getTessFaceArray(dm);
	numpolys = dm->getNumTessFaces(dm);
	numverts = dm->getNumVerts(dm);
	MTFace *tface = (MTFace *)dm->getTessFaceDataArray(dm, CD_MTFACE);

	/* double lookup */
	const int *index_mf_to_mpoly = (const int *)dm->getTessFaceDataArray(dm, CD_ORIGINDEX);
	const int *index_mp_to_orig  = (const int *)dm->getPolyDataArray(dm, CD_ORIGINDEX);
	if (index_mf_to_mpoly == NULL) {
		index_mp_to_orig = NULL;
	}

	m_shapeType = (polytope) ? PHY_SHAPE_POLYTOPE : PHY_SHAPE_MESH;

	/* Convert blender geometry into bullet mesh, need these vars for mapping */
	vector<bool> vert_tag_array(numverts, false);
	unsigned int tot_bt_verts= 0;

	if (polytope)
	{
		// Tag verts we're using
		for (int p2=0; p2<numpolys; p2++)
		{
			MFace* mf = &mface[p2];
			const int origi = index_mf_to_mpoly ? DM_origindex_mface_mpoly(index_mf_to_mpoly, index_mp_to_orig, p2) : p2;
			RAS_Polygon* poly = meshobj->GetPolygon(origi);

			// only add polygons that have the collision flag set
			if (poly->IsCollider())
			{
				if (vert_tag_array[mf->v1] == false) {vert_tag_array[mf->v1] = true; tot_bt_verts++;}
				if (vert_tag_array[mf->v2] == false) {vert_tag_array[mf->v2] = true; tot_bt_verts++;}
				if (vert_tag_array[mf->v3] == false) {vert_tag_array[mf->v3] = true; tot_bt_verts++;}
				if (mf->v4 && vert_tag_array[mf->v4] == false) {vert_tag_array[mf->v4] = true; tot_bt_verts++;}
			}
		}
		
		/* Can happen with ngons */
		if (!tot_bt_verts) {
			goto cleanup_empty_mesh;
		}

		m_vertexArray.resize(tot_bt_verts*3);

		btScalar *bt= &m_vertexArray[0];

		for (int p2=0; p2<numpolys; p2++)
		{
			MFace* mf = &mface[p2];
			const int origi = index_mf_to_mpoly ? DM_origindex_mface_mpoly(index_mf_to_mpoly, index_mp_to_orig, p2) : p2;
			RAS_Polygon* poly= meshobj->GetPolygon(origi);

			// only add polygons that have the collisionflag set
			if (poly->IsCollider())
			{
				if (vert_tag_array[mf->v1]==true)
				{
					const float* vtx = mvert[mf->v1].co;
					vert_tag_array[mf->v1] = false;
					*bt++ = vtx[0];
					*bt++ = vtx[1];
					*bt++ = vtx[2];
				}
				if (vert_tag_array[mf->v2]==true)
				{
					const float* vtx = mvert[mf->v2].co;
					vert_tag_array[mf->v2] = false;
					*bt++ = vtx[0];
					*bt++ = vtx[1];
					*bt++ = vtx[2];
				}
				if (vert_tag_array[mf->v3]==true)
				{
					const float* vtx = mvert[mf->v3].co;
					vert_tag_array[mf->v3] = false;
					*bt++ = vtx[0];
					*bt++ = vtx[1];
					*bt++ = vtx[2];
				}
				if (mf->v4 && vert_tag_array[mf->v4]==true)
				{
					const float* vtx = mvert[mf->v4].co;
					vert_tag_array[mf->v4] = false;
					*bt++ = vtx[0];
					*bt++ = vtx[1];
					*bt++ = vtx[2];
				}
			}
		}
	}
	else {
		unsigned int tot_bt_tris= 0;
		vector<int> vert_remap_array(numverts, 0);
		
		// Tag verts we're using
		for (int p2=0; p2<numpolys; p2++)
		{
			MFace* mf = &mface[p2];
			const int origi = index_mf_to_mpoly ? DM_origindex_mface_mpoly(index_mf_to_mpoly, index_mp_to_orig, p2) : p2;
			RAS_Polygon* poly= meshobj->GetPolygon(origi);

			// only add polygons that have the collision flag set
			if (poly->IsCollider())
			{
				if (vert_tag_array[mf->v1]==false)
					{vert_tag_array[mf->v1] = true;vert_remap_array[mf->v1] = tot_bt_verts;tot_bt_verts++;}
				if (vert_tag_array[mf->v2]==false)
					{vert_tag_array[mf->v2] = true;vert_remap_array[mf->v2] = tot_bt_verts;tot_bt_verts++;}
				if (vert_tag_array[mf->v3]==false)
					{vert_tag_array[mf->v3] = true;vert_remap_array[mf->v3] = tot_bt_verts;tot_bt_verts++;}
				if (mf->v4 && vert_tag_array[mf->v4]==false)
					{vert_tag_array[mf->v4] = true;vert_remap_array[mf->v4] = tot_bt_verts;tot_bt_verts++;}
				tot_bt_tris += (mf->v4 ? 2:1); /* a quad or a tri */
			}
		}

		/* Can happen with ngons */
		if (!tot_bt_verts) {
			goto cleanup_empty_mesh;
		}

		m_vertexArray.resize(tot_bt_verts*3);
		m_polygonIndexArray.resize(tot_bt_tris);
		m_triFaceArray.resize(tot_bt_tris*3);
		btScalar *bt= &m_vertexArray[0];
		int *poly_index_pt= &m_polygonIndexArray[0];
		int *tri_pt= &m_triFaceArray[0];

		UVco *uv_pt = NULL;
		if (tface)
		{
			m_triFaceUVcoArray.resize(tot_bt_tris*3);
			uv_pt = &m_triFaceUVcoArray[0];
		} 
		else 
			m_triFaceUVcoArray.clear();

		for (int p2=0; p2<numpolys; p2++)
		{
			MFace* mf = &mface[p2];
			MTFace* tf = (tface) ? &tface[p2] : NULL;
			const int origi = index_mf_to_mpoly ? DM_origindex_mface_mpoly(index_mf_to_mpoly, index_mp_to_orig, p2) : p2;
			RAS_Polygon* poly= meshobj->GetPolygon(origi);

			// only add polygons that have the collisionflag set
			if (poly->IsCollider())
			{
				MVert *v1= &mvert[mf->v1];
				MVert *v2= &mvert[mf->v2];
				MVert *v3= &mvert[mf->v3];

				// the face indices
				tri_pt[0] = vert_remap_array[mf->v1];
				tri_pt[1] = vert_remap_array[mf->v2];
				tri_pt[2] = vert_remap_array[mf->v3];
				tri_pt= tri_pt+3;
				if (tf)
				{
					uv_pt[0].uv[0] = tf->uv[0][0];
					uv_pt[0].uv[1] = tf->uv[0][1];
					uv_pt[1].uv[0] = tf->uv[1][0];
					uv_pt[1].uv[1] = tf->uv[1][1];
					uv_pt[2].uv[0] = tf->uv[2][0];
					uv_pt[2].uv[1] = tf->uv[2][1];
					uv_pt += 3;
				}

				// m_polygonIndexArray
				*poly_index_pt = origi;
				poly_index_pt++;

				// the vertex location
				if (vert_tag_array[mf->v1]==true) { /* *** v1 *** */
					vert_tag_array[mf->v1] = false;
					*bt++ = v1->co[0];
					*bt++ = v1->co[1];
					*bt++ = v1->co[2];
				}
				if (vert_tag_array[mf->v2]==true) { /* *** v2 *** */
					vert_tag_array[mf->v2] = false;
					*bt++ = v2->co[0];
					*bt++ = v2->co[1];
					*bt++ = v2->co[2];
				}
				if (vert_tag_array[mf->v3]==true) { /* *** v3 *** */
					vert_tag_array[mf->v3] = false;
					*bt++ = v3->co[0];
					*bt++ = v3->co[1];
					*bt++ = v3->co[2];
				}

				if (mf->v4)
				{
					MVert *v4= &mvert[mf->v4];

					tri_pt[0] = vert_remap_array[mf->v1];
					tri_pt[1] = vert_remap_array[mf->v3];
					tri_pt[2] = vert_remap_array[mf->v4];
					tri_pt= tri_pt+3;
					if (tf)
					{
						uv_pt[0].uv[0] = tf->uv[0][0];
						uv_pt[0].uv[1] = tf->uv[0][1];
						uv_pt[1].uv[0] = tf->uv[2][0];
						uv_pt[1].uv[1] = tf->uv[2][1];
						uv_pt[2].uv[0] = tf->uv[3][0];
						uv_pt[2].uv[1] = tf->uv[3][1];
						uv_pt += 3;
					}

					// m_polygonIndexArray
					*poly_index_pt = origi;
					poly_index_pt++;

					// the vertex location
					if (vert_tag_array[mf->v4] == true) { /* *** v4 *** */
						vert_tag_array[mf->v4] = false;
						*bt++ = v4->co[0];
						*bt++ = v4->co[1];
						*bt++ = v4->co[2];
					}
				}
			}
		}


		/* If this ever gets confusing, print out an OBJ file for debugging */
#if 0
		printf("# vert count %d\n", m_vertexArray.size());
		for (i=0; i<m_vertexArray.size(); i+=1) {
			printf("v %.6f %.6f %.6f\n", m_vertexArray[i].x(), m_vertexArray[i].y(), m_vertexArray[i].z());
		}

		printf("# face count %d\n", m_triFaceArray.size());
		for (i=0; i<m_triFaceArray.size(); i+=3) {
			printf("f %d %d %d\n", m_triFaceArray[i]+1, m_triFaceArray[i+1]+1, m_triFaceArray[i+2]+1);
		}
#endif

	}

#if 0
	if (validpolys==false)
	{
		// should not happen
		m_shapeType = PHY_SHAPE_NONE;
		return false;
	}
#endif
	
	m_meshObject = meshobj;
	if (free_dm) {
		dm->release(dm);
		dm = NULL;
	}

	// sharing only on static mesh at present, if you change that, you must also change in FindMesh
	if (!polytope && !dm)
	{
		// triangle shape can be shared, store the mesh object in the map
		m_meshShapeMap.insert(std::pair<RAS_MeshObject*,CcdShapeConstructionInfo*>(meshobj,this));
	}
	return true;


cleanup_empty_mesh:
	m_shapeType = PHY_SHAPE_NONE;
	m_meshObject = NULL;
	m_vertexArray.clear();
	m_polygonIndexArray.clear();
	m_triFaceArray.clear();
	m_triFaceUVcoArray.clear();
	if (free_dm) {
		dm->release(dm);
	}
	return false;
}

#include <cstdio>

/* Updates the arrays used by CreateBulletShape(),
 * take care that recalcLocalAabb() runs after CreateBulletShape is called.
 * */
bool CcdShapeConstructionInfo::UpdateMesh(class KX_GameObject* gameobj, class RAS_MeshObject* meshobj)
{
	int numpolys;
	int numverts;

	unsigned int tot_bt_tris= 0;
	unsigned int tot_bt_verts= 0;

	int i, j;
	int v_orig;

	/* Use for looping over verts in a face as a try or 2 tris */
	const int quad_verts[7]=	{0,1,2,		 0,2,3,		-1};
	const int tri_verts[4]=	{0,1,2,		-1};
	const int *fv_pt;

	if (gameobj==NULL && meshobj==NULL)
		return false;
	
	if (m_shapeType != PHY_SHAPE_MESH)
		return false;

	RAS_Deformer *deformer= gameobj ? gameobj->GetDeformer():NULL;
	DerivedMesh* dm = NULL;

	if (deformer)
		dm = deformer->GetPhysicsMesh();
	
	/* get the mesh from the object if not defined */
	if (meshobj==NULL) {
		
		/* modifier mesh */
		if (dm)
			meshobj= deformer->GetRasMesh();
		
		/* game object first mesh */
		if (meshobj==NULL) {
			if (gameobj->GetMeshCount() > 0) {
				meshobj= gameobj->GetMesh(0);
			}
		}
	}
	
	if (dm && deformer->GetRasMesh() == meshobj)
	{	/*
		 * Derived Mesh Update
		 *
		 * */

		MVert *mvert = dm->getVertArray(dm);
		MFace *mface = dm->getTessFaceArray(dm);
		numpolys = dm->getNumTessFaces(dm);
		numverts = dm->getNumVerts(dm);

		/* double lookup */
		const int *index_mf_to_mpoly = (const int *)dm->getTessFaceDataArray(dm, CD_ORIGINDEX);
		const int *index_mp_to_orig  = (const int *)dm->getPolyDataArray(dm, CD_ORIGINDEX);
		if (index_mf_to_mpoly == NULL) {
			index_mp_to_orig = NULL;
		}

		MFace *mf;
		MVert *mv;

		int flen;

		if (CustomData_has_layer(&dm->faceData, CD_MTFACE))
		{
			MTFace *tface = (MTFace *)dm->getTessFaceDataArray(dm, CD_MTFACE);
			MTFace *tf;

			vector<bool> vert_tag_array(numverts, false);
			vector<int> vert_remap_array(numverts, 0);

			for (mf= mface, tf= tface, i=0; i < numpolys; mf++, tf++, i++) {
				if (tf->mode & TF_DYNAMIC)
				{
					if (mf->v4) {
						tot_bt_tris+= 2;
						flen= 4;
					} else {
						tot_bt_tris++;
						flen= 3;
					}

					for (j=0; j<flen; j++)
					{
						v_orig = (*(&mf->v1 + j));

						if (vert_tag_array[v_orig]==false)
						{
							vert_tag_array[v_orig] = true;
							vert_remap_array[v_orig] = tot_bt_verts;
							tot_bt_verts++;
						}
					}
				}
			}

			m_vertexArray.resize(tot_bt_verts*3);
			btScalar *bt= &m_vertexArray[0];

			m_triFaceArray.resize(tot_bt_tris*3);
			int *tri_pt= &m_triFaceArray[0];

			m_triFaceUVcoArray.resize(tot_bt_tris*3);
			UVco *uv_pt= &m_triFaceUVcoArray[0];

			m_polygonIndexArray.resize(tot_bt_tris);
			int *poly_index_pt= &m_polygonIndexArray[0];

			for (mf= mface, tf= tface, i=0; i < numpolys; mf++, tf++, i++)
			{
				if (tf->mode & TF_DYNAMIC)
				{
					int origi = index_mf_to_mpoly ? DM_origindex_mface_mpoly(index_mf_to_mpoly, index_mp_to_orig, i) : i;

					if (mf->v4) {
						fv_pt= quad_verts;
						*poly_index_pt++ = origi;
						*poly_index_pt++ = origi;
						flen= 4;
					} else {
						fv_pt= tri_verts;
						*poly_index_pt++ = origi;
						flen= 3;
					}

					for (; *fv_pt > -1; fv_pt++)
					{
						v_orig = (*(&mf->v1 + (*fv_pt)));

						if (vert_tag_array[v_orig])
						{
							mv= mvert + v_orig;
							*bt++ = mv->co[0];
							*bt++ = mv->co[1];
							*bt++ = mv->co[2];

							vert_tag_array[v_orig] = false;
						}
						*tri_pt++ = vert_remap_array[v_orig];
						uv_pt->uv[0] = tf->uv[*fv_pt][0];
						uv_pt->uv[1] = tf->uv[*fv_pt][1];
						uv_pt++;
					}
				}
			}
		}
		else {
			/* no need for a vertex mapping. simple/fast */

			tot_bt_verts= numverts;

			for (mf= mface, i=0; i < numpolys; mf++, i++) {
				tot_bt_tris += (mf->v4 ? 2:1);
			}

			m_vertexArray.resize(tot_bt_verts*3);
			btScalar *bt= &m_vertexArray[0];

			m_triFaceArray.resize(tot_bt_tris*3);
			int *tri_pt= &m_triFaceArray[0];

			m_polygonIndexArray.resize(tot_bt_tris);
			int *poly_index_pt= &m_polygonIndexArray[0];

			m_triFaceUVcoArray.clear();

			for (mv= mvert, i=0; i < numverts; mv++, i++) {
				*bt++ = mv->co[0]; *bt++ = mv->co[1]; *bt++ = mv->co[2];
			}

			for (mf= mface, i=0; i < numpolys; mf++, i++) {
				int origi = index_mf_to_mpoly ? DM_origindex_mface_mpoly(index_mf_to_mpoly, index_mp_to_orig, i) : i;

				if (mf->v4) {
					fv_pt= quad_verts;
					*poly_index_pt++ = origi;
					*poly_index_pt++ = origi;
				}
				else {
					fv_pt= tri_verts;
					*poly_index_pt++ = origi;
				}

				for (; *fv_pt > -1; fv_pt++)
					*tri_pt++ = (*(&mf->v1 + (*fv_pt)));
			}
		}
	}
	else {	/*
			 * RAS Mesh Update
			 *
			 * */
		
		/* Note!, gameobj can be NULL here */

		/* transverts are only used for deformed RAS_Meshes, the RAS_TexVert data
		 * is too hard to get at, see below for details */
		float (*transverts)[3] = NULL;
		int transverts_tot= 0; /* with deformed meshes - should always be greater than the max orginal index, or we get crashes */

		if (deformer) {
			/* map locations from the deformed array
			 *
			 * Could call deformer->Update(); but rely on redraw updating.
			 * */
			transverts= deformer->GetTransVerts(&transverts_tot);
		}

		// Tag verts we're using
		numpolys= meshobj->NumPolygons();
		numverts= meshobj->m_sharedvertex_map.size();
		const float *xyz;


		vector<bool> vert_tag_array(numverts, false);
		vector<int> vert_remap_array(numverts, 0);

		for (int p=0; p<numpolys; p++)
		{
			RAS_Polygon* poly= meshobj->GetPolygon(p);
			if (poly->IsCollider())
			{
				for (i=0; i < poly->VertexCount(); i++)
				{
					v_orig= poly->GetVertex(i)->getOrigIndex();
					if (vert_tag_array[v_orig]==false)
					{
						vert_tag_array[v_orig] = true;
						vert_remap_array[v_orig] = tot_bt_verts;
						tot_bt_verts++;
					}
				}
				tot_bt_tris += (poly->VertexCount()==4 ? 2:1);
			}
		}

		// This case happens when none of the polys are colliders
		if (tot_bt_tris == 0 || tot_bt_verts == 0)
			return false;

		m_vertexArray.resize(tot_bt_verts*3);
		btScalar *bt= &m_vertexArray[0];

		m_triFaceArray.resize(tot_bt_tris*3);
		int *tri_pt= &m_triFaceArray[0];

		/* cant be used for anything useful in this case, since we don't rely on the original mesh
		 * will just be an array like pythons range(tot_bt_tris) */
		m_polygonIndexArray.resize(tot_bt_tris);


		for (int p=0; p<numpolys; p++)
		{
			RAS_Polygon* poly= meshobj->GetPolygon(p);

			if (poly->IsCollider())
			{
				/* quad or tri loop */
				fv_pt= (poly->VertexCount()==3 ? tri_verts:quad_verts);

				for (; *fv_pt > -1; fv_pt++)
				{
					v_orig= poly->GetVertex(*fv_pt)->getOrigIndex();

					if (vert_tag_array[v_orig])
					{
						if (transverts) {
							/* deformed mesh, using RAS_TexVert locations would be too troublesome
							 * because they are use the gameob as a hash in the material slot */
							*bt++ = transverts[v_orig][0];
							*bt++ = transverts[v_orig][1];
							*bt++ = transverts[v_orig][2];
						}
						else {
							/* static mesh python may have modified */
							xyz= meshobj->GetVertexLocation( v_orig );
							*bt++ = xyz[0];
							*bt++ = xyz[1];
							*bt++ = xyz[2];
						}

						vert_tag_array[v_orig] = false;
					}

					*tri_pt++ = vert_remap_array[v_orig];
				}
			}

			m_polygonIndexArray[p] = p; /* dumb counting */
		}
	}
	
#if 0
	/* needs #include <cstdio> */
	printf("# vert count %d\n", m_vertexArray.size());
	for (int i=0; i<m_vertexArray.size(); i+=3) {
		printf("v %.6f %.6f %.6f\n", m_vertexArray[i], m_vertexArray[i+1], m_vertexArray[i+2]);
	}

	printf("# face count %d\n", m_triFaceArray.size());
	for (int i=0; i<m_triFaceArray.size(); i+=3) {
		printf("f %d %d %d\n", m_triFaceArray[i]+1, m_triFaceArray[i+1]+1, m_triFaceArray[i+2]+1);
	}
#endif

	/* force recreation of the m_unscaledShape.
	 * If this has multiple users we cant delete */
	if (m_unscaledShape) {
		// don't free now so it can re-allocate under the same location and not break pointers.
		// DeleteBulletShape(m_unscaledShape); 
		m_forceReInstance= true;
	}

	// Make sure to also replace the mesh in the shape map! Otherwise we leave dangling references when we free.
	// Note, this whole business could cause issues with shared meshes. If we update one mesh, do we replace
	// them all?
	std::map<RAS_MeshObject*,CcdShapeConstructionInfo*>::iterator mit = m_meshShapeMap.find(m_meshObject);
	if (mit != m_meshShapeMap.end()) {
		m_meshShapeMap.erase(mit);
		m_meshShapeMap[meshobj] = this;
	}

	m_meshObject= meshobj;
	
	if (dm) {
		dm->needsFree = 1;
		dm->release(dm);
	}
	return true;
}



bool CcdShapeConstructionInfo::SetProxy(CcdShapeConstructionInfo* shapeInfo)
{
	if (shapeInfo == NULL)
		return false;
	// no support for dynamic change
	assert(IsUnused());
	m_shapeType = PHY_SHAPE_PROXY;
	m_shapeProxy = shapeInfo;
	return true;
}

btCollisionShape* CcdShapeConstructionInfo::CreateBulletShape(btScalar margin, bool useGimpact, bool useBvh)
{
	btCollisionShape* collisionShape = 0;
	btCompoundShape* compoundShape = 0;

	if (m_shapeType == PHY_SHAPE_PROXY && m_shapeProxy != NULL)
		return m_shapeProxy->CreateBulletShape(margin, useGimpact, useBvh);

	switch (m_shapeType) 
	{
	default:
		break;

	case PHY_SHAPE_BOX:
		collisionShape = new btBoxShape(m_halfExtend);
		collisionShape->setMargin(margin);
		break;

	case PHY_SHAPE_SPHERE:
		collisionShape = new btSphereShape(m_radius);
		collisionShape->setMargin(margin);
		break;

	case PHY_SHAPE_CYLINDER:
		collisionShape = new btCylinderShapeZ(m_halfExtend);
		collisionShape->setMargin(margin);
		break;

	case PHY_SHAPE_CONE:
		collisionShape = new btConeShapeZ(m_radius, m_height);
		collisionShape->setMargin(margin);
		break;

	case PHY_SHAPE_POLYTOPE:
		collisionShape = new btConvexHullShape(&m_vertexArray[0], m_vertexArray.size()/3, 3*sizeof(btScalar));
		collisionShape->setMargin(margin);
		break;

	case PHY_SHAPE_CAPSULE:
		collisionShape = new btCapsuleShapeZ(m_radius, m_height);
		collisionShape->setMargin(margin);
		break;

	case PHY_SHAPE_MESH:
		// Let's use the latest btScaledBvhTriangleMeshShape: it allows true sharing of 
		// triangle mesh information between duplicates => drastic performance increase when 
		// duplicating complex mesh objects. 
		// BUT it causes a small performance decrease when sharing is not required: 
		// 9 multiplications/additions and one function call for each triangle that passes the mid phase filtering
		// One possible optimization is to use directly the btBvhTriangleMeshShape when the scale is 1,1,1
		// and btScaledBvhTriangleMeshShape otherwise.
		if (useGimpact)
		{
				btTriangleIndexVertexArray* indexVertexArrays = new btTriangleIndexVertexArray(
						m_polygonIndexArray.size(),
						&m_triFaceArray[0],
						3*sizeof(int),
						m_vertexArray.size()/3,
						&m_vertexArray[0],
						3*sizeof(btScalar)
				);
				btGImpactMeshShape* gimpactShape =  new btGImpactMeshShape(indexVertexArrays);
				gimpactShape->setMargin(margin);
				gimpactShape->updateBound();
				collisionShape = gimpactShape;
				

		} else
		{
			if (!m_unscaledShape || m_forceReInstance)
			{
			
				btTriangleIndexVertexArray* indexVertexArrays = 0;

				///enable welding, only for the objects that need it (such as soft bodies)
				if (0.f != m_weldingThreshold1)
				{
					btTriangleMesh* collisionMeshData = new btTriangleMesh(true,false);
					collisionMeshData->m_weldingThreshold = m_weldingThreshold1;
					bool removeDuplicateVertices=true;
					// m_vertexArray not in multiple of 3 anymore, use m_triFaceArray
					for (unsigned int i=0; i<m_triFaceArray.size(); i+=3) {
						btScalar *bt = &m_vertexArray[3*m_triFaceArray[i]];
						btVector3 v1(bt[0], bt[1], bt[2]);
						bt = &m_vertexArray[3*m_triFaceArray[i+1]];
						btVector3 v2(bt[0], bt[1], bt[2]);
						bt = &m_vertexArray[3*m_triFaceArray[i+2]];
						btVector3 v3(bt[0], bt[1], bt[2]);
						collisionMeshData->addTriangle(v1, v2, v3, removeDuplicateVertices);
					}
					indexVertexArrays = collisionMeshData;

				} else
				{
					indexVertexArrays = new btTriangleIndexVertexArray(
							m_polygonIndexArray.size(),
							&m_triFaceArray[0],
							3*sizeof(int),
							m_vertexArray.size()/3,
							&m_vertexArray[0],
							3*sizeof(btScalar));
				}
				
				// this shape will be shared and not deleted until shapeInfo is deleted
				
				// for UpdateMesh, reuse the last memory location so instancing wont crash.
				if (m_unscaledShape) {
					DeleteBulletShape(m_unscaledShape, false);
					m_unscaledShape->~btBvhTriangleMeshShape();
					m_unscaledShape = new(m_unscaledShape) btBvhTriangleMeshShape( indexVertexArrays, true, useBvh );
				} else {
					m_unscaledShape = new btBvhTriangleMeshShape( indexVertexArrays, true, useBvh );
				}
				m_forceReInstance= false;
			} else if (useBvh && m_unscaledShape->getOptimizedBvh() == NULL) {
				// the existing unscaledShape was not build with Bvh, do it now
				m_unscaledShape->buildOptimizedBvh();
			}
			collisionShape = new btScaledBvhTriangleMeshShape(m_unscaledShape, btVector3(1.0f,1.0f,1.0f));
			collisionShape->setMargin(margin);
		}
		break;

	case PHY_SHAPE_COMPOUND:
		if (m_shapeArray.size() > 0)
		{
			compoundShape = new btCompoundShape();
			for (std::vector<CcdShapeConstructionInfo*>::iterator sit = m_shapeArray.begin();
			     sit != m_shapeArray.end();
			     sit++)
			{
				collisionShape = (*sit)->CreateBulletShape(margin, useGimpact, useBvh);
				if (collisionShape)
				{
					collisionShape->setLocalScaling((*sit)->m_childScale);
					compoundShape->addChildShape((*sit)->m_childTrans, collisionShape);
				}
			}
			collisionShape = compoundShape;
		}
		break;
	}
	return collisionShape;
}

void CcdShapeConstructionInfo::AddShape(CcdShapeConstructionInfo* shapeInfo)
{
	m_shapeArray.push_back(shapeInfo);
	shapeInfo->AddRef();
}

CcdShapeConstructionInfo::~CcdShapeConstructionInfo()
{
	for (std::vector<CcdShapeConstructionInfo*>::iterator sit = m_shapeArray.begin();
	     sit != m_shapeArray.end();
	     sit++)
	{
		(*sit)->Release();
	}
	m_shapeArray.clear();
	if (m_unscaledShape)
	{
		DeleteBulletShape(m_unscaledShape, true);
	}
	m_vertexArray.clear();
	if (m_shapeType == PHY_SHAPE_MESH && m_meshObject != NULL) 
	{
		std::map<RAS_MeshObject*,CcdShapeConstructionInfo*>::iterator mit = m_meshShapeMap.find(m_meshObject);
		if (mit != m_meshShapeMap.end() && mit->second == this)
		{
			m_meshShapeMap.erase(mit);
		}
	}
	if (m_shapeType == PHY_SHAPE_PROXY && m_shapeProxy != NULL)
	{
		m_shapeProxy->Release();
	}
}

