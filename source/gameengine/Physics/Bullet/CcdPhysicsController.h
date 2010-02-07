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


#ifndef BULLET2_PHYSICSCONTROLLER_H
#define BULLET2_PHYSICSCONTROLLER_H

#include <vector>
#include <map>

#include "PHY_IPhysicsController.h"

///	PHY_IPhysicsController is the abstract simplified Interface to a physical object.
///	It contains the IMotionState and IDeformableMesh Interfaces.
#include "btBulletDynamicsCommon.h"
#include "LinearMath/btTransform.h"

#include "PHY_IMotionState.h"

extern float gDeactivationTime;
extern float gLinearSleepingTreshold;
extern float gAngularSleepingTreshold;
extern bool gDisableDeactivation;
class CcdPhysicsEnvironment;
class btMotionState;
class RAS_MeshObject;
struct DerivedMesh;
class btCollisionShape;


#define CCD_BSB_SHAPE_MATCHING	2
#define CCD_BSB_BENDING_CONSTRAINTS 8
#define CCD_BSB_AERO_VPOINT 16 /* aero model, Vertex normals are oriented toward velocity*/
#define CCD_BSB_AERO_VTWOSIDE 32 /* aero model, Vertex normals are flipped to match velocity */

/* BulletSoftBody.collisionflags */
#define CCD_BSB_COL_SDF_RS	2 /* SDF based rigid vs soft */
#define CCD_BSB_COL_CL_RS	4 /* Cluster based rigid vs soft */
#define CCD_BSB_COL_CL_SS	8 /* Cluster based soft vs soft */
#define CCD_BSB_COL_VF_SS	16 /* Vertex/Face based soft vs soft */


// Shape contructor
// It contains all the information needed to create a simple bullet shape at runtime
class CcdShapeConstructionInfo
{
public:
	struct UVco 
	{
		float uv[2];
	};
	
	static CcdShapeConstructionInfo* FindMesh(class RAS_MeshObject* mesh, struct DerivedMesh* dm, bool polytope);

	CcdShapeConstructionInfo() :
		m_shapeType(PHY_SHAPE_NONE),
		m_radius(1.0),
		m_height(1.0),
		m_halfExtend(0.f,0.f,0.f),
		m_childScale(1.0f,1.0f,1.0f),
		m_userData(NULL),
		m_refCount(1),
		m_meshObject(NULL),
		m_unscaledShape(NULL),
		m_forceReInstance(false),
		m_weldingThreshold1(0.f),
		m_shapeProxy(NULL)
	{
		m_childTrans.setIdentity();
	}

	~CcdShapeConstructionInfo();

	CcdShapeConstructionInfo* AddRef()
	{ 
		m_refCount++;
		return this;
	}

	int Release()
	{
		if (--m_refCount > 0)
			return m_refCount;
		delete this;
		return 0;
	}

	bool IsUnused(void)
	{
		return (m_meshObject==NULL && m_shapeArray.size() == 0 && m_shapeProxy == NULL);
	}

	void AddShape(CcdShapeConstructionInfo* shapeInfo);

	btTriangleMeshShape* GetMeshShape(void)
	{
		return (m_unscaledShape);
	}
	CcdShapeConstructionInfo* GetChildShape(int i)
	{
		if (i < 0 || i >= (int)m_shapeArray.size())
			return NULL;

		return m_shapeArray.at(i);
	}
	int FindChildShape(CcdShapeConstructionInfo* shapeInfo, void* userData)
	{
		if (shapeInfo == NULL)
			return -1;
		for (int i=0; i<(int)m_shapeArray.size(); i++)
		{
			CcdShapeConstructionInfo* childInfo = m_shapeArray.at(i);
			if ((userData == NULL || userData == childInfo->m_userData) &&
				(childInfo == shapeInfo ||
				 (childInfo->m_shapeType == PHY_SHAPE_PROXY && 
				  childInfo->m_shapeProxy == shapeInfo)))
				return i;
		}
		return -1;
	}

	bool RemoveChildShape(int i)
	{
		if (i < 0 || i >= (int)m_shapeArray.size())
			return false;
		m_shapeArray.at(i)->Release();
		if (i < (int)m_shapeArray.size()-1)
			m_shapeArray[i] = m_shapeArray.back();
		m_shapeArray.pop_back();
		return true;
	}

	bool SetMesh(class RAS_MeshObject* mesh, struct DerivedMesh* dm, bool polytope);
	RAS_MeshObject* GetMesh(void)
	{
		return m_meshObject;
	}

	bool UpdateMesh(class KX_GameObject* gameobj, class RAS_MeshObject* mesh);


	bool SetProxy(CcdShapeConstructionInfo* shapeInfo);
	CcdShapeConstructionInfo* GetProxy(void)
	{
		return m_shapeProxy;
	}

	btCollisionShape* CreateBulletShape(btScalar margin, bool useGimpact=false, bool useBvh=true);

	// member variables
	PHY_ShapeType			m_shapeType;
	btScalar				m_radius;
	btScalar				m_height;
	btVector3				m_halfExtend;
	btTransform				m_childTrans;
	btVector3				m_childScale;
	void*					m_userData;	
	btAlignedObjectArray<btScalar>	m_vertexArray;	// Contains both vertex array for polytope shape and
											// triangle array for concave mesh shape. Each vertex is 3 consecutive values
											// In this case a triangle is made of 3 consecutive points
	std::vector<int>		m_polygonIndexArray;	// Contains the array of polygon index in the 
													// original mesh that correspond to shape triangles.
													// only set for concave mesh shape.
	
	std::vector<int>		m_triFaceArray;	// Contains an array of triplets of face indicies
											// quads turn into 2 tris

	std::vector<UVco>		m_triFaceUVcoArray;	// Contains an array of pair of UV coordinate for each vertex of faces
												// quads turn into 2 tris

	void	setVertexWeldingThreshold1(float threshold)
	{
		m_weldingThreshold1  = threshold*threshold;
	}
protected:
	static std::map<RAS_MeshObject*, CcdShapeConstructionInfo*> m_meshShapeMap;
	int						m_refCount;		// this class is shared between replicas
											// keep track of users so that we can release it 
	RAS_MeshObject*	m_meshObject;			// Keep a pointer to the original mesh 
	btBvhTriangleMeshShape* m_unscaledShape;// holds the shared unscale BVH mesh shape, 
											// the actual shape is of type btScaledBvhTriangleMeshShape
	std::vector<CcdShapeConstructionInfo*> m_shapeArray;	// for compound shapes
	bool	m_forceReInstance; //use gimpact for concave dynamic/moving collision detection
	float	m_weldingThreshold1;	//welding closeby vertices together can improve softbody stability etc.
	CcdShapeConstructionInfo* m_shapeProxy;	// only used for PHY_SHAPE_PROXY, pointer to actual shape info


#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new( unsigned int num_bytes) { return MEM_mallocN(num_bytes, "GE:CcdShapeConstructionInfo"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

struct CcdConstructionInfo
{

	///CollisionFilterGroups provides some optional usage of basic collision filtering
	///this is done during broadphase, so very early in the pipeline
	///more advanced collision filtering should be done in btCollisionDispatcher::NeedsCollision
	enum CollisionFilterGroups
	{
	        DefaultFilter = 1,
	        StaticFilter = 2,
	        KinematicFilter = 4,
	        DebrisFilter = 8,
			SensorFilter = 16,
	        AllFilter = DefaultFilter | StaticFilter | KinematicFilter | DebrisFilter | SensorFilter,
	};


	CcdConstructionInfo()
		: m_localInertiaTensor(1.f, 1.f, 1.f),
		m_gravity(0,0,0),
		m_scaling(1.f,1.f,1.f),
		m_mass(0.f),
		m_clamp_vel_min(-1.f), 
		m_clamp_vel_max(-1.f), 
		m_restitution(0.1f),
		m_friction(0.5f),
		m_linearDamping(0.1f),
		m_angularDamping(0.1f),
		m_margin(0.06f),
		m_gamesoftFlag(0),
		m_collisionFlags(0),
		m_bRigid(false),
		m_bSoft(false),
		m_bSensor(false),
		m_bGimpact(false),
		m_collisionFilterGroup(DefaultFilter),
		m_collisionFilterMask(AllFilter),
		m_collisionShape(0),
		m_MotionState(0),
		m_shapeInfo(0),
		m_physicsEnv(0),
		m_inertiaFactor(1.f),
		m_do_anisotropic(false),
		m_anisotropicFriction(1.f,1.f,1.f),
		m_contactProcessingThreshold(1e10)
	{
	}

	btVector3	m_localInertiaTensor;
	btVector3	m_gravity;
	btVector3	m_scaling;
	btScalar	m_mass;
	btScalar	m_clamp_vel_min;  
	btScalar	m_clamp_vel_max;  
	btScalar	m_restitution;
	btScalar	m_friction;
	btScalar	m_linearDamping;
	btScalar	m_angularDamping;
	btScalar	m_margin;

	////////////////////
	int		m_gamesoftFlag;
	float	m_soft_linStiff;			/* linear stiffness 0..1 */
	float	m_soft_angStiff;		/* angular stiffness 0..1 */
	float	m_soft_volume;			/* volume preservation 0..1 */

	int		m_soft_viterations;		/* Velocities solver iterations */
	int		m_soft_piterations;		/* Positions solver iterations */
	int		m_soft_diterations;		/* Drift solver iterations */
	int		m_soft_citerations;		/* Cluster solver iterations */

	float	m_soft_kSRHR_CL;		/* Soft vs rigid hardness [0,1] (cluster only) */
	float	m_soft_kSKHR_CL;		/* Soft vs kinetic hardness [0,1] (cluster only) */
	float	m_soft_kSSHR_CL;		/* Soft vs soft hardness [0,1] (cluster only) */
	float	m_soft_kSR_SPLT_CL;	/* Soft vs rigid impulse split [0,1] (cluster only) */

	float	m_soft_kSK_SPLT_CL;	/* Soft vs rigid impulse split [0,1] (cluster only) */
	float	m_soft_kSS_SPLT_CL;	/* Soft vs rigid impulse split [0,1] (cluster only) */
	float	m_soft_kVCF;			/* Velocities correction factor (Baumgarte) */
	float	m_soft_kDP;			/* Damping coefficient [0,1] */

	float	m_soft_kDG;			/* Drag coefficient [0,+inf] */
	float	m_soft_kLF;			/* Lift coefficient [0,+inf] */
	float	m_soft_kPR;			/* Pressure coefficient [-inf,+inf] */
	float	m_soft_kVC;			/* Volume conversation coefficient [0,+inf] */

	float	m_soft_kDF;			/* Dynamic friction coefficient [0,1] */
	float	m_soft_kMT;			/* Pose matching coefficient [0,1] */
	float	m_soft_kCHR;			/* Rigid contacts hardness [0,1] */
	float	m_soft_kKHR;			/* Kinetic contacts hardness [0,1] */

	float	m_soft_kSHR;			/* Soft contacts hardness [0,1] */
	float	m_soft_kAHR;			/* Anchors hardness [0,1] */
	int		m_soft_collisionflags;	/* Vertex/Face or Signed Distance Field(SDF) or Clusters, Soft versus Soft or Rigid */
	int		m_soft_numclusteriterations;	/* number of iterations to refine collision clusters*/
///////////////////



	int			m_collisionFlags;
	bool		m_bRigid;
	bool		m_bSoft;
	bool		m_bSensor;
	bool		m_bGimpact;			// use Gimpact for mesh body

	///optional use of collision group/mask:
	///only collision with object goups that match the collision mask.
	///this is very basic early out. advanced collision filtering should be
	///done in the btCollisionDispatcher::NeedsCollision and NeedsResponse
	///both values default to 1
	short int	m_collisionFilterGroup;
	short int	m_collisionFilterMask;

	///these pointers are used as argument passing for the CcdPhysicsController constructor
	///and not anymore after that
	class btCollisionShape*	m_collisionShape;
	class PHY_IMotionState*	m_MotionState;
	class CcdShapeConstructionInfo* m_shapeInfo;
	
	CcdPhysicsEnvironment*	m_physicsEnv; //needed for self-replication
	float	m_inertiaFactor;//tweak the inertia (hooked up to Blender 'formfactor'
	bool	m_do_anisotropic;
	btVector3 m_anisotropicFriction;

	bool		m_do_fh;                 ///< Should the object have a linear Fh spring?
	bool		m_do_rot_fh;             ///< Should the object have an angular Fh spring?
	btScalar	m_fh_spring;             ///< Spring constant (both linear and angular)
	btScalar	m_fh_damping;            ///< Damping factor (linear and angular) in range [0, 1]
	btScalar	m_fh_distance;           ///< The range above the surface where Fh is active.    
	bool		m_fh_normal;             ///< Should the object slide off slopes?
	float		m_radius;//for fh backwards compatibility
	
	///m_contactProcessingThreshold allows to process contact points with positive distance
	///normally only contacts with negative distance (penetration) are solved
	///however, rigid body stacking is more stable when positive contacts are still passed into the constraint solver
	///this might sometimes lead to collisions with 'internal edges' such as a sliding character controller
	///so disable/set m_contactProcessingThreshold to zero for sliding characters etc.
	float		m_contactProcessingThreshold;///< Process contacts with positive distance in range [0..INF]

};


class btRigidBody;
class btCollisionObject;
class btSoftBody;

///CcdPhysicsController is a physics object that supports continuous collision detection and time of impact based physics resolution.
class CcdPhysicsController : public PHY_IPhysicsController	
{
protected:
	btCollisionObject* m_object;
	

	class PHY_IMotionState*		m_MotionState;
	btMotionState* 	m_bulletMotionState;
	class btCollisionShape*	m_collisionShape;
	class CcdShapeConstructionInfo* m_shapeInfo;

	friend class CcdPhysicsEnvironment;	// needed when updating the controller

	//some book keeping for replication
	bool	m_softbodyMappingDone;
	bool	m_softBodyTransformInitialized;
	bool	m_prototypeTransformInitialized;
	btTransform	m_softbodyStartTrans;


	void*		m_newClientInfo;
	int			m_registerCount;	// needed when multiple sensors use the same controller
	CcdConstructionInfo	m_cci;//needed for replication

	CcdPhysicsController* m_parentCtrl;

	void GetWorldOrientation(btMatrix3x3& mat);

	void CreateRigidbody();
	bool CreateSoftbody();

	bool Register()	{ 
		return (m_registerCount++ == 0) ? true : false;
	}
	bool Unregister() {
		return (--m_registerCount == 0) ? true : false;
	}

	void setWorldOrientation(const btMatrix3x3& mat);
	void forceWorldTransform(const btMatrix3x3& mat, const btVector3& pos);

	public:
	
		int				m_collisionDelay;
	

		CcdPhysicsController (const CcdConstructionInfo& ci);

		bool DeleteControllerShape();
		bool ReplaceControllerShape(btCollisionShape *newShape);

		virtual ~CcdPhysicsController();

		CcdConstructionInfo& getConstructionInfo()
		{
			return m_cci;
		}
		const CcdConstructionInfo& getConstructionInfo() const
		{
			return m_cci;
		}


		btRigidBody* GetRigidBody();
		btCollisionObject*	GetCollisionObject();
		btSoftBody* GetSoftBody();

		CcdShapeConstructionInfo* GetShapeInfo() { return m_shapeInfo; }

		btCollisionShape*	GetCollisionShape() { 
			return m_object->getCollisionShape();
		}
		////////////////////////////////////
		// PHY_IPhysicsController interface
		////////////////////////////////////


		/**
			SynchronizeMotionStates ynchronizes dynas, kinematic and deformable entities (and do 'late binding')
		*/
		virtual bool		SynchronizeMotionStates(float time);
		/**
			WriteMotionStateToDynamics ynchronizes dynas, kinematic and deformable entities (and do 'late binding')
		*/
		
		virtual void		WriteMotionStateToDynamics(bool nondynaonly);
		virtual	void		WriteDynamicsToMotionState();

		// controller replication
		virtual	void		PostProcessReplica(class PHY_IMotionState* motionstate,class PHY_IPhysicsController* parentctrl);
		virtual void		SetPhysicsEnvironment(class PHY_IPhysicsEnvironment *env);

		// kinematic methods
		virtual void		RelativeTranslate(float dlocX,float dlocY,float dlocZ,bool local);
		virtual void		RelativeRotate(const float drot[9],bool local);
		virtual	void		getOrientation(float &quatImag0,float &quatImag1,float &quatImag2,float &quatReal);
		virtual	void		setOrientation(float quatImag0,float quatImag1,float quatImag2,float quatReal);
		virtual	void		setPosition(float posX,float posY,float posZ);
		virtual	void 		getPosition(PHY__Vector3&	pos) const;

		virtual	void		setScaling(float scaleX,float scaleY,float scaleZ);
		
		// physics methods
		virtual void		ApplyTorque(float torqueX,float torqueY,float torqueZ,bool local);
		virtual void		ApplyForce(float forceX,float forceY,float forceZ,bool local);
		virtual void		SetAngularVelocity(float ang_velX,float ang_velY,float ang_velZ,bool local);
		virtual void		SetLinearVelocity(float lin_velX,float lin_velY,float lin_velZ,bool local);
		virtual void		applyImpulse(float attachX,float attachY,float attachZ, float impulseX,float impulseY,float impulseZ);
		virtual void		SetActive(bool active);

		// reading out information from physics
		virtual void		GetLinearVelocity(float& linvX,float& linvY,float& linvZ);
		virtual void		GetAngularVelocity(float& angVelX,float& angVelY,float& angVelZ);
		virtual void		GetVelocity(const float posX,const float posY,const float posZ,float& linvX,float& linvY,float& linvZ); 
		virtual	void		getReactionForce(float& forceX,float& forceY,float& forceZ);

		// dyna's that are rigidbody are free in orientation, dyna's with non-rigidbody are restricted 
		virtual	void		setRigidBody(bool rigid);

		
		virtual void		resolveCombinedVelocities(float linvelX,float linvelY,float linvelZ,float angVelX,float angVelY,float angVelZ);

		// clientinfo for raycasts for example
		virtual	void*				getNewClientInfo();
		virtual	void				setNewClientInfo(void* clientinfo);
		virtual PHY_IPhysicsController*	GetReplica();
		
		///There should be no 'SetCollisionFilterGroup' method, as changing this during run-time is will result in errors
		short int	GetCollisionFilterGroup() const
		{
			return m_cci.m_collisionFilterGroup;
		}
		///There should be no 'SetCollisionFilterGroup' method, as changing this during run-time is will result in errors
		short int	GetCollisionFilterMask() const
		{
			return m_cci.m_collisionFilterMask;
		}

		virtual void	calcXform() {} ;
		virtual void SetMargin(float margin) 
		{
			if (m_collisionShape)
				m_collisionShape->setMargin(btScalar(margin));
		}
		virtual float GetMargin() const 
		{
			return (m_collisionShape) ? m_collisionShape->getMargin() : 0.f;
		}
		virtual float GetRadius() const 
		{ 
			// this is not the actual shape radius, it's only used for Fh support
			return m_cci.m_radius;
		}
		virtual void  SetRadius(float margin) 
		{
			if (m_collisionShape && m_collisionShape->getShapeType() == SPHERE_SHAPE_PROXYTYPE)
			{
				btSphereShape* sphereShape = static_cast<btSphereShape*>(m_collisionShape);
				sphereShape->setUnscaledRadius(margin);
			}
			m_cci.m_radius = margin;
		}
		
		// velocity clamping
		virtual void SetLinVelocityMin(float val) 
		{
			m_cci.m_clamp_vel_min= val;
		}
		virtual float GetLinVelocityMin() const 
		{
			return m_cci.m_clamp_vel_min;
		}
		virtual void SetLinVelocityMax(float val) 
		{
			m_cci.m_clamp_vel_max= val;
		}
		virtual float GetLinVelocityMax() const 
		{
			return m_cci.m_clamp_vel_max;
		}

		bool	wantsSleeping();

		void	UpdateDeactivation(float timeStep);

		void	SetCenterOfMassTransform(btTransform& xform);

		static btTransform&	GetTransformFromMotionState(PHY_IMotionState* motionState);

		void	setAabb(const btVector3& aabbMin,const btVector3& aabbMax);


		class	PHY_IMotionState*			GetMotionState()
		{
			return m_MotionState;
		}

		const class	PHY_IMotionState*			GetMotionState() const
		{
			return m_MotionState;
		}

		class CcdPhysicsEnvironment* GetPhysicsEnvironment()
		{
			return m_cci.m_physicsEnv;
		}

		void	setParentCtrl(CcdPhysicsController* parentCtrl)
		{
			m_parentCtrl = parentCtrl;
		}

		CcdPhysicsController*	getParentCtrl()
		{
			return m_parentCtrl;
		}

		const CcdPhysicsController*	getParentCtrl() const
		{
			return m_parentCtrl;
		}


#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new( unsigned int num_bytes) { return MEM_mallocN(num_bytes, "GE:CcdPhysicsController"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};




///DefaultMotionState implements standard motionstate, using btTransform
class	DefaultMotionState : public PHY_IMotionState

{
	public:
		DefaultMotionState();

		virtual ~DefaultMotionState();

		virtual void	getWorldPosition(float& posX,float& posY,float& posZ);
		virtual void	getWorldScaling(float& scaleX,float& scaleY,float& scaleZ);
		virtual void	getWorldOrientation(float& quatIma0,float& quatIma1,float& quatIma2,float& quatReal);
		
		virtual void	setWorldPosition(float posX,float posY,float posZ);
		virtual	void	setWorldOrientation(float quatIma0,float quatIma1,float quatIma2,float quatReal);
		virtual void	getWorldOrientation(float* ori);
		virtual void	setWorldOrientation(const float* ori);
		
		virtual	void	calculateWorldTransformations();
		
		btTransform	m_worldTransform;
		btVector3		m_localScaling;
	
	
#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new( unsigned int num_bytes) { return MEM_mallocN(num_bytes, "GE:DefaultMotionState"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};


#endif //BULLET2_PHYSICSCONTROLLER_H
