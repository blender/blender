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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * Game object wrapper
 */

/** \file gameengine/Ketsji/KX_GameObject.cpp
 *  \ingroup ketsji
 */

#ifdef _MSC_VER
  /* This warning tells us about truncation of __long__ stl-generated names.
   * It can occasionally cause DevStudio to have internal compiler warnings. */
#  pragma warning( disable:4786 )
#endif

#include "RAS_IPolygonMaterial.h"
#include "KX_BlenderMaterial.h"
#include "KX_GameObject.h"
#include "KX_Camera.h" // only for their ::Type
#include "KX_Light.h"  // only for their ::Type
#include "KX_FontObject.h"  // only for their ::Type
#include "RAS_MeshObject.h"
#include "KX_NavMeshObject.h"
#include "KX_MeshProxy.h"
#include "KX_PolyProxy.h"
#include <stdio.h> // printf
#include "SG_Controller.h"
#include "PHY_IGraphicController.h"
#include "SG_Node.h"
#include "KX_ClientObjectInfo.h"
#include "RAS_BucketManager.h"
#include "KX_RayCast.h"
#include "KX_PythonInit.h"
#include "KX_PyMath.h"
#include "SCA_IActuator.h"
#include "SCA_ISensor.h"
#include "SCA_IController.h"
#include "NG_NetworkScene.h" //Needed for sendMessage()
#include "KX_ObstacleSimulation.h"
#include "KX_Scene.h"

#include "BKE_object.h"

#include "BL_ActionManager.h"
#include "BL_Action.h"

#include "EXP_PyObjectPlus.h" /* python stuff */
#include "EXP_ListWrapper.h"
#include "BLI_utildefines.h"

#ifdef WITH_PYTHON
#  include "EXP_PythonCallBack.h"
#  include "python_utildefines.h"
#endif

// This file defines relationships between parents and children
// in the game engine.

#include "KX_SG_NodeRelationships.h"

#include "BLI_math.h"

static MT_Point3 dummy_point= MT_Point3(0.0f, 0.0f, 0.0f);
static MT_Vector3 dummy_scaling = MT_Vector3(1.0f, 1.0f, 1.0f);
static MT_Matrix3x3 dummy_orientation = MT_Matrix3x3(1.0f, 0.0f, 0.0f,
                                                     0.0f, 1.0f, 0.0f,
                                                     0.0f, 0.0f, 1.0f);

KX_GameObject::KX_GameObject(
        void* sgReplicationInfo,
        SG_Callbacks callbacks)
    : SCA_IObject(),
      m_bDyna(false),
      m_layer(0),
      m_currentLodLevel(0),
      m_previousLodLevel(0),
      m_pBlenderObject(NULL),
      m_pBlenderGroupObject(NULL),
      m_bUseObjectColor(false),
      m_bIsNegativeScaling(false),
      m_objectColor(1.0f, 1.0f, 1.0f, 1.0f),
      m_bVisible(true),
      m_bCulled(true),
      m_bOccluder(false),
      m_pPhysicsController(NULL),
      m_pGraphicController(NULL),
      m_pObstacleSimulation(NULL),
      m_pInstanceObjects(NULL),
      m_pDupliGroupObject(NULL),
      m_actionManager(NULL),
      m_bRecordAnimation(false),
      m_isDeformable(false)

#ifdef WITH_PYTHON
    , m_attr_dict(NULL),
    m_collisionCallbacks(NULL)
#endif
{
	m_ignore_activity_culling = false;
	m_pClient_info = new KX_ClientObjectInfo(this, KX_ClientObjectInfo::ACTOR);
	m_pSGNode = new SG_Node(this,sgReplicationInfo,callbacks);

	// define the relationship between this node and it's parent.
	
	KX_NormalParentRelation * parent_relation = 
		KX_NormalParentRelation::New();
	m_pSGNode->SetParentRelation(parent_relation);
};



KX_GameObject::~KX_GameObject()
{
#ifdef WITH_PYTHON
	if (m_attr_dict) {
		PyDict_Clear(m_attr_dict); /* in case of circular refs or other weird cases */
		/* Py_CLEAR: Py_DECREF's and NULL's */
		Py_CLEAR(m_attr_dict);
	}
	// Unregister collision callbacks
	// Do this before we start freeing physics information like m_pClient_info
	if (m_collisionCallbacks) {
		UnregisterCollisionCallbacks();
		Py_CLEAR(m_collisionCallbacks);
	}
#endif // WITH_PYTHON

	RemoveMeshes();

	// is this delete somewhere ?
	//if (m_sumoObj)
	//	delete m_sumoObj;
	delete m_pClient_info;
	//if (m_pSGNode)
	//	delete m_pSGNode;
	if (m_pSGNode)
	{
		// must go through controllers and make sure they will not use us anymore
		// This is important for KX_BulletPhysicsControllers that unregister themselves
		// from the object when they are deleted.
		SGControllerList::iterator contit;
		SGControllerList& controllers = m_pSGNode->GetSGControllerList();
		for (contit = controllers.begin();contit!=controllers.end();++contit)
		{
			(*contit)->ClearObject();
		}
		m_pSGNode->SetSGClientObject(NULL);

		/* m_pSGNode is freed in KX_Scene::RemoveNodeDestructObject */
	}
	if (m_pGraphicController)
	{
		delete m_pGraphicController;
	}

	if (m_pPhysicsController)
	{
		delete m_pPhysicsController;
	}

	if (m_pObstacleSimulation)
	{
		m_pObstacleSimulation->DestroyObstacleForObj(this);
	}

	if (m_actionManager)
	{
		delete m_actionManager;
	}

	if (m_pDupliGroupObject)
	{
		m_pDupliGroupObject->Release();
	}

	if (m_pInstanceObjects)
	{
		m_pInstanceObjects->Release();
	}
}

KX_GameObject* KX_GameObject::GetClientObject(KX_ClientObjectInfo *info)
{
	if (!info)
		return NULL;
	return info->m_gameobject;
}

CValue* KX_GameObject::	Calc(VALUE_OPERATOR op, CValue *val) 
{
	return NULL;
}



CValue* KX_GameObject::CalcFinal(VALUE_DATA_TYPE dtype, VALUE_OPERATOR op, CValue *val)
{
	return NULL;
}



const STR_String & KX_GameObject::GetText()
{
	return m_text;
}



double KX_GameObject::GetNumber()
{
	return 0;
}



STR_String& KX_GameObject::GetName()
{
	return m_name;
}


/* Set the name of the value */
void KX_GameObject::SetName(const char *name)
{
	m_name = name;
}

PHY_IPhysicsController* KX_GameObject::GetPhysicsController()
{
	return m_pPhysicsController;
}

KX_GameObject* KX_GameObject::GetDupliGroupObject()
{ 
	return m_pDupliGroupObject;
}

CListValue* KX_GameObject::GetInstanceObjects()
{ 
	return m_pInstanceObjects;
}

void KX_GameObject::AddInstanceObjects(KX_GameObject* obj)
{
	if (!m_pInstanceObjects)
		m_pInstanceObjects = new CListValue();

	obj->AddRef();
	m_pInstanceObjects->Add(obj);
}

void KX_GameObject::RemoveInstanceObject(KX_GameObject* obj)
{
	assert(m_pInstanceObjects);
	m_pInstanceObjects->RemoveValue(obj);
	obj->Release();
}

void KX_GameObject::RemoveDupliGroupObject()
{
	if (m_pDupliGroupObject) {
		m_pDupliGroupObject->Release();
		m_pDupliGroupObject = NULL;
	}
}

void KX_GameObject::SetDupliGroupObject(KX_GameObject* obj)
{
	obj->AddRef();
	m_pDupliGroupObject = obj;
}

void KX_GameObject::AddConstraint(bRigidBodyJointConstraint *cons)
{
	m_constraints.push_back(cons);
}

std::vector<bRigidBodyJointConstraint*> KX_GameObject::GetConstraints()
{
	return m_constraints;
}

void KX_GameObject::ClearConstraints()
{
	m_constraints.clear();
}

KX_GameObject* KX_GameObject::GetParent()
{
	KX_GameObject* result = NULL;
	SG_Node* node = m_pSGNode;
	
	while (node && !result)
	{
		node = node->GetSGParent();
		if (node)
			result = (KX_GameObject*)node->GetSGClientObject();
	}

	return result;
	
}

void KX_GameObject::SetParent(KX_Scene *scene, KX_GameObject* obj, bool addToCompound, bool ghost)
{
	// check on valid node in case a python controller holds a reference to a deleted object
	if (obj && 
		GetSGNode() &&			// object is not zombi
		obj->GetSGNode() &&		// object is not zombi
		GetSGNode()->GetSGParent() != obj->GetSGNode() &&	// not already parented to same object
		!GetSGNode()->IsAncessor(obj->GetSGNode()) && 		// no parenting loop
		this != obj)										// not the object itself
	{
		// Make sure the objects have some scale
		MT_Vector3 scale1 = NodeGetWorldScaling();
		MT_Vector3 scale2 = obj->NodeGetWorldScaling();
		if (fabs(scale2[0]) < (MT_Scalar)FLT_EPSILON ||
			fabs(scale2[1]) < (MT_Scalar)FLT_EPSILON ||
			fabs(scale2[2]) < (MT_Scalar)FLT_EPSILON ||
			fabs(scale1[0]) < (MT_Scalar)FLT_EPSILON ||
			fabs(scale1[1]) < (MT_Scalar)FLT_EPSILON ||
			fabs(scale1[2]) < (MT_Scalar)FLT_EPSILON) { return; }

		// Remove us from our old parent and set our new parent
		RemoveParent(scene);
		obj->GetSGNode()->AddChild(GetSGNode());

		if (m_pPhysicsController)
		{
			m_pPhysicsController->SuspendDynamics(ghost);
		}
		// Set us to our new scale, position, and orientation
		scale2[0] = 1.0f/scale2[0];
		scale2[1] = 1.0f/scale2[1];
		scale2[2] = 1.0f/scale2[2];
		scale1 = scale1 * scale2;
		MT_Matrix3x3 invori = obj->NodeGetWorldOrientation().inverse();
		MT_Vector3 newpos = invori*(NodeGetWorldPosition()-obj->NodeGetWorldPosition())*scale2;

		NodeSetLocalScale(scale1);
		NodeSetLocalPosition(MT_Point3(newpos[0],newpos[1],newpos[2]));
		NodeSetLocalOrientation(invori*NodeGetWorldOrientation());
		NodeUpdateGS(0.f);
		// object will now be a child, it must be removed from the parent list
		CListValue* rootlist = scene->GetRootParentList();
		if (rootlist->RemoveValue(this))
			// the object was in parent list, decrement ref count as it's now removed
			Release();
		// if the new parent is a compound object, add this object shape to the compound shape.
		// step 0: verify this object has physical controller
		if (m_pPhysicsController && addToCompound)
		{
			// step 1: find the top parent (not necessarily obj)
			KX_GameObject* rootobj = (KX_GameObject*)obj->GetSGNode()->GetRootSGParent()->GetSGClientObject();
			// step 2: verify it has a physical controller and compound shape
			if (rootobj != NULL && 
				rootobj->m_pPhysicsController != NULL &&
				rootobj->m_pPhysicsController->IsCompound())
			{
				rootobj->m_pPhysicsController->AddCompoundChild(m_pPhysicsController);
			}
		}
		// graphically, the object hasn't change place, no need to update m_pGraphicController
	}
}

void KX_GameObject::RemoveParent(KX_Scene *scene)
{
	// check on valid node in case a python controller holds a reference to a deleted object
	if (GetSGNode() && GetSGNode()->GetSGParent())
	{
		// get the root object to remove us from compound object if needed
		KX_GameObject* rootobj = (KX_GameObject*)GetSGNode()->GetRootSGParent()->GetSGClientObject();
		// Set us to the right spot 
		GetSGNode()->SetLocalScale(GetSGNode()->GetWorldScaling());
		GetSGNode()->SetLocalOrientation(GetSGNode()->GetWorldOrientation());
		GetSGNode()->SetLocalPosition(GetSGNode()->GetWorldPosition());

		// Remove us from our parent
		GetSGNode()->DisconnectFromParent();
		NodeUpdateGS(0.f);
		// the object is now a root object, add it to the parentlist
		CListValue* rootlist = scene->GetRootParentList();
		if (!rootlist->SearchValue(this))
			// object was not in root list, add it now and increment ref count
			rootlist->Add(AddRef());
		if (m_pPhysicsController)
		{
			// in case this controller was added as a child shape to the parent
			if (rootobj != NULL && 
				rootobj->m_pPhysicsController != NULL &&
				rootobj->m_pPhysicsController->IsCompound())
			{
				rootobj->m_pPhysicsController->RemoveCompoundChild(m_pPhysicsController);
			}
			m_pPhysicsController->RestoreDynamics();
			if (m_pPhysicsController->IsDynamic() && (rootobj != NULL && rootobj->m_pPhysicsController))
			{
				// dynamic object should remember the velocity they had while being parented
				MT_Point3 childPoint = GetSGNode()->GetWorldPosition();
				MT_Point3 rootPoint = rootobj->GetSGNode()->GetWorldPosition();
				MT_Point3 relPoint;
				relPoint = (childPoint-rootPoint);
				MT_Vector3 linVel = rootobj->m_pPhysicsController->GetVelocity(relPoint);
				MT_Vector3 angVel = rootobj->m_pPhysicsController->GetAngularVelocity();
				m_pPhysicsController->SetLinearVelocity(linVel, false);
				m_pPhysicsController->SetAngularVelocity(angVel, false);
			}
		}
		// graphically, the object hasn't change place, no need to update m_pGraphicController
	}
}

BL_ActionManager* KX_GameObject::GetActionManager()
{
	// We only want to create an action manager if we need it
	if (!m_actionManager)
	{
		GetScene()->AddAnimatedObject(this);
		m_actionManager = new BL_ActionManager(this);
	}
	return m_actionManager;
}

bool KX_GameObject::PlayAction(const char* name,
								float start,
								float end,
								short layer,
								short priority,
								float blendin,
								short play_mode,
								float layer_weight,
								short ipo_flags,
								float playback_speed,
								short blend_mode)
{
	return GetActionManager()->PlayAction(name, start, end, layer, priority, blendin, play_mode, layer_weight, ipo_flags, playback_speed, blend_mode);
}

void KX_GameObject::StopAction(short layer)
{
	GetActionManager()->StopAction(layer);
}

void KX_GameObject::RemoveTaggedActions()
{
	GetActionManager()->RemoveTaggedActions();
}

bool KX_GameObject::IsActionDone(short layer)
{
	return GetActionManager()->IsActionDone(layer);
}

void KX_GameObject::UpdateActionManager(float curtime)
{
	GetActionManager()->Update(curtime);
}

float KX_GameObject::GetActionFrame(short layer)
{
	return GetActionManager()->GetActionFrame(layer);
}

const char *KX_GameObject::GetActionName(short layer)
{
	return GetActionManager()->GetActionName(layer);
}

void KX_GameObject::SetActionFrame(short layer, float frame)
{
	GetActionManager()->SetActionFrame(layer, frame);
}

bAction *KX_GameObject::GetCurrentAction(short layer)
{
	return GetActionManager()->GetCurrentAction(layer);
}

void KX_GameObject::SetPlayMode(short layer, short mode)
{
	GetActionManager()->SetPlayMode(layer, mode);
}

void KX_GameObject::SetTimes(short layer, float start, float end)
{
	GetActionManager()->SetTimes(layer, start, end);
}

void KX_GameObject::ProcessReplica()
{
	SCA_IObject::ProcessReplica();

	m_pGraphicController = NULL;
	m_pPhysicsController = NULL;
	m_pSGNode = NULL;

	/* Dupli group and instance list are set later in replication.
	 * See KX_Scene::DupliGroupRecurse. */
	m_pDupliGroupObject = NULL;
	m_pInstanceObjects = NULL;
	m_pClient_info = new KX_ClientObjectInfo(*m_pClient_info);
	m_pClient_info->m_gameobject = this;
	m_actionManager = NULL;
	m_state = 0;

	KX_Scene* scene = KX_GetActiveScene();
	KX_ObstacleSimulation* obssimulation = scene->GetObstacleSimulation();
	struct Object* blenderobject = GetBlenderObject();
	if (obssimulation && (blenderobject->gameflag & OB_HASOBSTACLE))
	{
		obssimulation->AddObstacleForObj(this);
	}

#ifdef WITH_PYTHON
	if (m_attr_dict)
		m_attr_dict= PyDict_Copy(m_attr_dict);
#endif
		
}

static void setGraphicController_recursive(SG_Node* node)
{
	NodeList& children = node->GetSGChildren();

	for (NodeList::iterator childit = children.begin();!(childit==children.end());++childit)
	{
		SG_Node* childnode = (*childit);
		KX_GameObject *clientgameobj = static_cast<KX_GameObject*>( (*childit)->GetSGClientObject());
		if (clientgameobj != NULL) // This is a GameObject
			clientgameobj->ActivateGraphicController(false);
		
		// if the childobj is NULL then this may be an inverse parent link
		// so a non recursive search should still look down this node.
		setGraphicController_recursive(childnode);
	}
}


void KX_GameObject::ActivateGraphicController(bool recurse)
{
	if (m_pGraphicController)
	{
		m_pGraphicController->Activate(m_bVisible);
	}
	if (recurse)
	{
		setGraphicController_recursive(GetSGNode());
	}
}

void KX_GameObject::SetUserCollisionGroup(unsigned short group)
{
	m_userCollisionGroup = group;
	if (m_pPhysicsController)
		m_pPhysicsController->RefreshCollisions();
}
void KX_GameObject::SetUserCollisionMask(unsigned short mask)
{
	m_userCollisionMask = mask;
	if (m_pPhysicsController)
		m_pPhysicsController->RefreshCollisions();
}

unsigned short KX_GameObject::GetUserCollisionGroup()
{
	return m_userCollisionGroup;
}
unsigned short KX_GameObject::GetUserCollisionMask()
{
	return m_userCollisionMask;
}

bool KX_GameObject::CheckCollision(KX_GameObject* other)
{
	return this->m_userCollisionGroup & other->m_userCollisionMask;
}

CValue* KX_GameObject::GetReplica()
{
	KX_GameObject* replica = new KX_GameObject(*this);

	// this will copy properties and so on...
	replica->ProcessReplica();

	return replica;
}

bool KX_GameObject::IsDynamicsSuspended() const
{
	if (m_pPhysicsController)
		return m_pPhysicsController->IsSuspended();
	return false;
}

float KX_GameObject::getLinearDamping() const
{
	if (m_pPhysicsController)
		return m_pPhysicsController->GetLinearDamping();
	return 0;
}

float KX_GameObject::getAngularDamping() const
{
	if (m_pPhysicsController)
		return m_pPhysicsController->GetAngularDamping();
	return 0;
}

void KX_GameObject::setLinearDamping(float damping)
{
	if (m_pPhysicsController)
		m_pPhysicsController->SetLinearDamping(damping);
}


void KX_GameObject::setAngularDamping(float damping)
{
	if (m_pPhysicsController)
		m_pPhysicsController->SetAngularDamping(damping);
}


void KX_GameObject::setDamping(float linear, float angular)
{
	if (m_pPhysicsController)
		m_pPhysicsController->SetDamping(linear, angular);
}


void KX_GameObject::ApplyForce(const MT_Vector3& force,bool local)
{
	if (m_pPhysicsController)
		m_pPhysicsController->ApplyForce(force,local);
}



void KX_GameObject::ApplyTorque(const MT_Vector3& torque,bool local)
{
	if (m_pPhysicsController)
		m_pPhysicsController->ApplyTorque(torque,local);
}



void KX_GameObject::ApplyMovement(const MT_Vector3& dloc,bool local)
{
	if (GetSGNode()) 
	{
		if (m_pPhysicsController) // (IsDynamic())
		{
			m_pPhysicsController->RelativeTranslate(dloc,local);
		}
		GetSGNode()->RelativeTranslate(dloc,GetSGNode()->GetSGParent(),local);
	}
}



void KX_GameObject::ApplyRotation(const MT_Vector3& drot,bool local)
{
	MT_Matrix3x3 rotmat(drot);
	
	if (GetSGNode()) {
		GetSGNode()->RelativeRotate(rotmat,local);

		if (m_pPhysicsController) { // (IsDynamic())
			m_pPhysicsController->RelativeRotate(rotmat,local);
		}
	}
}



/**
 * GetOpenGL Matrix, returns an OpenGL 'compatible' matrix
 */
float *KX_GameObject::GetOpenGLMatrix()
{
	// todo: optimize and only update if necessary
	float *fl = m_OpenGL_4x4Matrix.getPointer();
	if (GetSGNode()) {
		MT_Transform trans;
	
		trans.setOrigin(GetSGNode()->GetWorldPosition());
		trans.setBasis(GetSGNode()->GetWorldOrientation());
	
		MT_Vector3 scaling = GetSGNode()->GetWorldScaling();
		m_bIsNegativeScaling = ((scaling[0] < 0.0f) ^ (scaling[1] < 0.0f) ^ (scaling[2] < 0.0f)) ? true : false;
		trans.scale(scaling[0], scaling[1], scaling[2]);
		trans.getValue(fl);
		GetSGNode()->ClearDirty();
	}
	return fl;
}

void KX_GameObject::UpdateBlenderObjectMatrix(Object* blendobj)
{
	if (!blendobj)
		blendobj = m_pBlenderObject;
	if (blendobj) {
		const MT_Matrix3x3& rot = NodeGetWorldOrientation();
		const MT_Vector3& scale = NodeGetWorldScaling();
		const MT_Vector3& pos = NodeGetWorldPosition();
		rot.getValue(blendobj->obmat[0]);
		pos.getValue(blendobj->obmat[3]);
		mul_v3_fl(blendobj->obmat[0], scale[0]);
		mul_v3_fl(blendobj->obmat[1], scale[1]);
		mul_v3_fl(blendobj->obmat[2], scale[2]);
	}
}

void KX_GameObject::AddMeshUser()
{
	for (size_t i=0;i<m_meshes.size();i++)
	{
		m_meshes[i]->AddMeshUser(this, &m_meshSlots, GetDeformer());
	}
	// set the part of the mesh slot that never change
	float *fl = GetOpenGLMatrixPtr()->getPointer();

	SG_QList::iterator<RAS_MeshSlot> mit(m_meshSlots);
//	RAS_MeshSlot* ms;
	for (mit.begin(); !mit.end(); ++mit)
	{
		(*mit)->m_OpenGLMatrix = fl;
	}
	UpdateBuckets(false);
}

static void UpdateBuckets_recursive(SG_Node* node)
{
	NodeList& children = node->GetSGChildren();

	for (NodeList::iterator childit = children.begin();!(childit==children.end());++childit)
	{
		SG_Node* childnode = (*childit);
		KX_GameObject *clientgameobj = static_cast<KX_GameObject*>( (*childit)->GetSGClientObject());
		if (clientgameobj != NULL) // This is a GameObject
			clientgameobj->UpdateBuckets(0);
		
		// if the childobj is NULL then this may be an inverse parent link
		// so a non recursive search should still look down this node.
		UpdateBuckets_recursive(childnode);
	}
}

void KX_GameObject::UpdateBuckets( bool recursive )
{
	if (GetSGNode()) {
		RAS_MeshSlot *ms;

		if (GetSGNode()->IsDirty())
			GetOpenGLMatrix();

		SG_QList::iterator<RAS_MeshSlot> mit(m_meshSlots);
		for (mit.begin(); !mit.end(); ++mit)
		{
			ms = *mit;
			ms->m_bObjectColor = m_bUseObjectColor;
			ms->m_RGBAcolor = m_objectColor;
			ms->m_bVisible = m_bVisible;
			ms->m_bCulled = m_bCulled || !m_bVisible;
			if (!ms->m_bCulled) 
				ms->m_bucket->ActivateMesh(ms);
			
			/* split if necessary */
#ifdef USE_SPLIT
			ms->Split();
#endif
		}
	
		if (recursive) {
			UpdateBuckets_recursive(GetSGNode());
		}
	}
}

void KX_GameObject::RemoveMeshes()
{
	for (size_t i=0;i<m_meshes.size();i++)
		m_meshes[i]->RemoveFromBuckets(this);

	//note: meshes can be shared, and are deleted by KX_BlenderSceneConverter

	m_meshes.clear();
}

void KX_GameObject::AddLodMesh(RAS_MeshObject* mesh)
{
	m_lodmeshes.push_back(mesh);
}


static float calcHysteresis(KX_Scene *kxscene, LodLevel *lod)
{
	float hystvariance = 0.0f;

	if (!kxscene->IsActivedLodHysteresis())
		return hystvariance;
			
	short hysteresis = 0;
	// if exists, LoD level hysteresis will override scene hysteresis
	if (lod->next->flags & OB_LOD_USE_HYST)
		hysteresis = lod->next->obhysteresis;
	else
		hysteresis = kxscene->GetLodHysteresisValue();

	return hystvariance = MT_abs(lod->next->distance - lod->distance) * hysteresis / 100;
}

void KX_GameObject::UpdateLod(MT_Vector3 &cam_pos)
{
	// Handle dupligroups
	if (m_pInstanceObjects) {
		KX_GameObject *instob;
		int count = m_pInstanceObjects->GetCount();
		for (int i = 0; i < count; i++) {
			instob = (KX_GameObject*)m_pInstanceObjects->GetValue(i);
			instob->UpdateLod(cam_pos);
		}
	}

	if (m_lodmeshes.empty())
		return;

	MT_Vector3 delta = NodeGetWorldPosition() - cam_pos;
	float distance2 = delta.length2();

	int level = 0;
	float hystvariance = 0.0f;
	Object *bob = GetBlenderObject();
	LodLevel *lod = (LodLevel *)bob->lodlevels.first;
	KX_Scene *kxscene = GetScene();

	for (; lod; lod = lod->next, level++) {
		if (!lod->source || lod->source->type != OB_MESH)
			level--;

		if (!lod->next)
			break;

		if (level == m_previousLodLevel || level == (m_previousLodLevel + 1)) {
			hystvariance = calcHysteresis(kxscene, lod);
			float newdistance = lod->next->distance + hystvariance;
			if (newdistance * newdistance > distance2)
				break;
		}
		else if (level == (m_previousLodLevel - 1)) {
			hystvariance = calcHysteresis(kxscene, lod);
			float newdistance = lod->next->distance - hystvariance;
			if (newdistance * newdistance > distance2)
				break;
		}
	}

	RAS_MeshObject *mesh = m_lodmeshes[level];
	m_currentLodLevel = level;
	if (mesh != m_meshes[0]) {
		m_previousLodLevel = level;
		GetScene()->ReplaceMesh(this, mesh, true, false);
	}
}

void KX_GameObject::UpdateTransform()
{
	// HACK: saves function call for dynamic object, they are handled differently
	if (m_pPhysicsController && !m_pPhysicsController->IsDynamic())
		m_pPhysicsController->SetTransform();
	if (m_pGraphicController)
		// update the culling tree
		m_pGraphicController->SetGraphicTransform();

}

void KX_GameObject::UpdateTransformFunc(SG_IObject* node, void* gameobj, void* scene)
{
	((KX_GameObject*)gameobj)->UpdateTransform();
}

void KX_GameObject::SynchronizeTransform()
{
	// only used for sensor object, do full synchronization as bullet doesn't do it
	if (m_pPhysicsController)
		m_pPhysicsController->SetTransform();
	if (m_pGraphicController)
		m_pGraphicController->SetGraphicTransform();
}

void KX_GameObject::SynchronizeTransformFunc(SG_IObject* node, void* gameobj, void* scene)
{
	((KX_GameObject*)gameobj)->SynchronizeTransform();
}


void KX_GameObject::SetDebugColor(unsigned int bgra)
{
	for (size_t i=0;i<m_meshes.size();i++)
		m_meshes[i]->DebugColor(bgra);
}



void KX_GameObject::ResetDebugColor()
{
	SetDebugColor(0xff000000);
}

void KX_GameObject::InitIPO(bool ipo_as_force,
                            bool ipo_add,
                            bool ipo_local)
{
	SGControllerList::iterator it = GetSGNode()->GetSGControllerList().begin();

	while (it != GetSGNode()->GetSGControllerList().end()) {
		(*it)->SetOption(SG_Controller::SG_CONTR_IPO_RESET, true);
		(*it)->SetOption(SG_Controller::SG_CONTR_IPO_IPO_AS_FORCE, ipo_as_force);
		(*it)->SetOption(SG_Controller::SG_CONTR_IPO_IPO_ADD, ipo_add);
		(*it)->SetOption(SG_Controller::SG_CONTR_IPO_LOCAL, ipo_local);
		it++;
	}
} 

void KX_GameObject::UpdateIPO(float curframetime,
							  bool recurse) 
{
	/* This function shouldn't call BL_Action::Update, not even indirectly, 
	 * as it will cause deadlock due to the lock in BL_Action::Update. */

	// just the 'normal' update procedure.
	GetSGNode()->SetSimulatedTime(curframetime,recurse);
	GetSGNode()->UpdateWorldData(curframetime);
	UpdateTransform();
}

// IPO update
void 
KX_GameObject::UpdateMaterialData(
		dword matname_hash,
		MT_Vector4 rgba,
		MT_Vector3 specrgb,
		MT_Scalar hard,
		MT_Scalar spec,
		MT_Scalar ref,
		MT_Scalar emit,
		MT_Scalar alpha

	)
{
	int mesh = 0;
	if (((unsigned int)mesh < m_meshes.size()) && mesh >= 0) {
		list<RAS_MeshMaterial>::iterator mit = m_meshes[mesh]->GetFirstMaterial();

		for (; mit != m_meshes[mesh]->GetLastMaterial(); ++mit)
		{
			RAS_IPolyMaterial* poly = mit->m_bucket->GetPolyMaterial();

			if (poly->GetFlag() & RAS_BLENDERMAT )
			{
				KX_BlenderMaterial *m =  static_cast<KX_BlenderMaterial*>(poly);
				
				if (matname_hash == 0)
				{
					m->UpdateIPO(rgba, specrgb,hard,spec,ref,emit, alpha);
					// if mesh has only one material attached to it then use original hack with no need to edit vertices (better performance)
					SetObjectColor(rgba);
				}
				else
				{
					if (matname_hash == poly->GetMaterialNameHash())
					{
						m->UpdateIPO(rgba, specrgb,hard,spec,ref,emit, alpha);
						m_meshes[mesh]->SetVertexColor(poly,rgba);
						
						// no break here, because one blender material can be split into several game engine materials
						// (e.g. one uvsphere material is split into one material at poles with ras_mode TRIANGLE and one material for the body
						// if here was a break then would miss some vertices if material was split
					}
				}
			}
		}
	}
}
bool
KX_GameObject::GetVisible(
	void
	)
{
	return m_bVisible;
}

static void setVisible_recursive(SG_Node* node, bool v)
{
	NodeList& children = node->GetSGChildren();

	for (NodeList::iterator childit = children.begin();!(childit==children.end());++childit)
	{
		SG_Node* childnode = (*childit);
		KX_GameObject *clientgameobj = static_cast<KX_GameObject*>( (*childit)->GetSGClientObject());
		if (clientgameobj != NULL) // This is a GameObject
			clientgameobj->SetVisible(v, 0);
		
		// if the childobj is NULL then this may be an inverse parent link
		// so a non recursive search should still look down this node.
		setVisible_recursive(childnode, v);
	}
}


void
KX_GameObject::SetVisible(
	bool v,
	bool recursive
	)
{
	if (GetSGNode()) {
		m_bVisible = v;
		if (m_pGraphicController)
			m_pGraphicController->Activate(m_bVisible);
		if (recursive)
			setVisible_recursive(GetSGNode(), v);
	}
}

static void setOccluder_recursive(SG_Node* node, bool v)
{
	NodeList& children = node->GetSGChildren();

	for (NodeList::iterator childit = children.begin();!(childit==children.end());++childit)
	{
		SG_Node* childnode = (*childit);
		KX_GameObject *clientgameobj = static_cast<KX_GameObject*>( (*childit)->GetSGClientObject());
		if (clientgameobj != NULL) // This is a GameObject
			clientgameobj->SetOccluder(v, false);
		
		// if the childobj is NULL then this may be an inverse parent link
		// so a non recursive search should still look down this node.
		setOccluder_recursive(childnode, v);
	}
}

void
KX_GameObject::SetOccluder(
	bool v,
	bool recursive
	)
{
	if (GetSGNode()) {
		m_bOccluder = v;
		if (recursive)
			setOccluder_recursive(GetSGNode(), v);
	}
}

static void setDebug_recursive(SG_Node *node, bool debug)
{
	NodeList& children = node->GetSGChildren();
	KX_Scene *scene = KX_GetActiveScene();

	for (NodeList::iterator childit = children.begin();!(childit==children.end());++childit) {
		SG_Node *childnode = (*childit);
		KX_GameObject *clientgameobj = static_cast<KX_GameObject*>( (*childit)->GetSGClientObject());
		if (clientgameobj != NULL) {
			if (debug) {
				if (!scene->ObjectInDebugList(clientgameobj))
					scene->AddObjectDebugProperties(clientgameobj);
			}
			else
				scene->RemoveObjectDebugProperties(clientgameobj);
		}

		/* if the childobj is NULL then this may be an inverse parent link
		 * so a non recursive search should still look down this node. */
		setDebug_recursive(childnode, debug);
	}
}

void KX_GameObject::SetUseDebugProperties( bool debug, bool recursive )
{
	KX_Scene *scene = KX_GetActiveScene();

	if (debug) {
		if (!scene->ObjectInDebugList(this))
			scene->AddObjectDebugProperties(this);
	}
	else
		scene->RemoveObjectDebugProperties(this);

	if (recursive)
		setDebug_recursive(GetSGNode(), debug);
}

void
KX_GameObject::SetLayer(
	int l
	)
{
	m_layer = l;
}

int
KX_GameObject::GetLayer(
	void
	)
{
	return m_layer;
}

void KX_GameObject::addLinearVelocity(const MT_Vector3& lin_vel,bool local)
{
	if (m_pPhysicsController)
	{
		MT_Vector3 lv = local ? NodeGetWorldOrientation() * lin_vel : lin_vel;
		m_pPhysicsController->SetLinearVelocity(lv + m_pPhysicsController->GetLinearVelocity(), 0);
	}
}



void KX_GameObject::setLinearVelocity(const MT_Vector3& lin_vel,bool local)
{
	if (m_pPhysicsController)
		m_pPhysicsController->SetLinearVelocity(lin_vel,local);
}



void KX_GameObject::setAngularVelocity(const MT_Vector3& ang_vel,bool local)
{
	if (m_pPhysicsController)
		m_pPhysicsController->SetAngularVelocity(ang_vel,local);
}


void KX_GameObject::ResolveCombinedVelocities(
	const MT_Vector3 & lin_vel,
	const MT_Vector3 & ang_vel,
	bool lin_vel_local,
	bool ang_vel_local
) {
	if (m_pPhysicsController)
	{

		MT_Vector3 lv = lin_vel_local ? NodeGetWorldOrientation() * lin_vel : lin_vel;
		MT_Vector3 av = ang_vel_local ? NodeGetWorldOrientation() * ang_vel : ang_vel;
		m_pPhysicsController->ResolveCombinedVelocities(
			lv.x(),lv.y(),lv.z(),av.x(),av.y(),av.z());
	}
}


void KX_GameObject::SetObjectColor(const MT_Vector4& rgbavec)
{
	m_bUseObjectColor = true;
	m_objectColor = rgbavec;
}

const MT_Vector4& KX_GameObject::GetObjectColor()
{
	return m_objectColor;
}

void KX_GameObject::AlignAxisToVect(const MT_Vector3& dir, int axis, float fac)
{
	const MT_Scalar eps = 3.0f * MT_EPSILON;
	MT_Matrix3x3 orimat;
	MT_Vector3 vect,ori,z,x,y;
	MT_Scalar len;

	// check on valid node in case a python controller holds a reference to a deleted object
	if (!GetSGNode())
		return;

	vect = dir;
	len = vect.length();
	if (MT_fuzzyZero(len))
	{
		cout << "alignAxisToVect() Error: Null vector!\n";
		return;
	}
	
	if (fac <= 0.0f) {
		return;
	}
	
	// normalize
	vect /= len;
	orimat = GetSGNode()->GetWorldOrientation();
	switch (axis)
	{
		case 0: // align x axis of new coord system to vect
			ori.setValue(orimat[0][2], orimat[1][2], orimat[2][2]); // pivot axis
			if (1.0f - MT_abs(vect.dot(ori)) < eps)  { // vect parallel to pivot?
				ori.setValue(orimat[0][1], orimat[1][1], orimat[2][1]); // change the pivot!
			}

			if (fac == 1.0f) {
				x = vect;
			} else {
				x = (vect * fac) + ((orimat * MT_Vector3(1.0f, 0.0f, 0.0f)) * (1.0f - fac));
				len = x.length();
				if (MT_fuzzyZero(len)) x = vect;
				else x /= len;
			}
			y = ori.cross(x);
			z = x.cross(y);
			break;
		case 1: // y axis
			ori.setValue(orimat[0][0], orimat[1][0], orimat[2][0]);
			if (1.0f - MT_abs(vect.dot(ori)) < eps) {
				ori.setValue(orimat[0][2], orimat[1][2], orimat[2][2]);
			}

			if (fac == 1.0f) {
				y = vect;
			} else {
				y = (vect * fac) + ((orimat * MT_Vector3(0.0f, 1.0f, 0.0f)) * (1.0f - fac));
				len = y.length();
				if (MT_fuzzyZero(len)) y = vect;
				else y /= len;
			}
			z = ori.cross(y);
			x = y.cross(z);
			break;
		case 2: // z axis
			ori.setValue(orimat[0][1], orimat[1][1], orimat[2][1]);
			if (1.0f - MT_abs(vect.dot(ori)) < eps) {
				ori.setValue(orimat[0][0], orimat[1][0], orimat[2][0]);
			}

			if (fac == 1.0f) {
				z = vect;
			} else {
				z = (vect * fac) + ((orimat * MT_Vector3(0.0f, 0.0f, 1.0f)) * (1.0f - fac));
				len = z.length();
				if (MT_fuzzyZero(len)) z = vect;
				else z /= len;
			}
			x = ori.cross(z);
			y = z.cross(x);
			break;
		default: // invalid axis specified
			cout << "alignAxisToVect(): Invalid axis '" << axis <<"'\n";
			return;
	}
	x.normalize(); // normalize the new base vectors
	y.normalize();
	z.normalize();
	orimat.setValue(x[0], y[0], z[0],
	                x[1], y[1], z[1],
	                x[2], y[2], z[2]);

	if (GetSGNode()->GetSGParent() != NULL)
	{
		// the object is a child, adapt its local orientation so that 
		// the global orientation is aligned as we want (cancelling out the parent orientation)
		MT_Matrix3x3 invori = GetSGNode()->GetSGParent()->GetWorldOrientation().inverse();
		NodeSetLocalOrientation(invori*orimat);
	}
	else {
		NodeSetLocalOrientation(orimat);
	}
}

MT_Scalar KX_GameObject::GetMass()
{
	if (m_pPhysicsController)
	{
		return m_pPhysicsController->GetMass();
	}
	return 0.0f;
}

MT_Vector3 KX_GameObject::GetLocalInertia()
{
	MT_Vector3 local_inertia(0.0f,0.0f,0.0f);
	if (m_pPhysicsController)
	{
		local_inertia = m_pPhysicsController->GetLocalInertia();
	}
	return local_inertia;
}

MT_Vector3 KX_GameObject::GetLinearVelocity(bool local)
{
	MT_Vector3 velocity(0.0f,0.0f,0.0f), locvel;
	MT_Matrix3x3 ori;
	if (m_pPhysicsController)
	{
		velocity = m_pPhysicsController->GetLinearVelocity();
		
		if (local)
		{
			ori = GetSGNode()->GetWorldOrientation();
			
			locvel = velocity * ori;
			return locvel;
		}
	}
	return velocity;
}

MT_Vector3 KX_GameObject::GetAngularVelocity(bool local)
{
	MT_Vector3 velocity(0.0f,0.0f,0.0f), locvel;
	MT_Matrix3x3 ori;
	if (m_pPhysicsController)
	{
		velocity = m_pPhysicsController->GetAngularVelocity();
		
		if (local)
		{
			ori = GetSGNode()->GetWorldOrientation();
			
			locvel = velocity * ori;
			return locvel;
		}
	}
	return velocity;
}

MT_Vector3 KX_GameObject::GetVelocity(const MT_Point3& point)
{
	if (m_pPhysicsController)
	{
		return m_pPhysicsController->GetVelocity(point);
	}
	return MT_Vector3(0.0f,0.0f,0.0f);
}

// scenegraph node stuff

void KX_GameObject::NodeSetLocalPosition(const MT_Point3& trans)
{
	// check on valid node in case a python controller holds a reference to a deleted object
	if (!GetSGNode())
		return;

	if (m_pPhysicsController && !GetSGNode()->GetSGParent())
	{
		// don't update physic controller if the object is a child:
		// 1) the transformation will not be right
		// 2) in this case, the physic controller is necessarily a static object
		//    that is updated from the normal kinematic synchronization
		m_pPhysicsController->SetPosition(trans);
	}

	GetSGNode()->SetLocalPosition(trans);

}



void KX_GameObject::NodeSetLocalOrientation(const MT_Matrix3x3& rot)
{
	// check on valid node in case a python controller holds a reference to a deleted object
	if (!GetSGNode())
		return;

	if (m_pPhysicsController && !GetSGNode()->GetSGParent())
	{
		// see note above
		m_pPhysicsController->SetOrientation(rot);
	}
	GetSGNode()->SetLocalOrientation(rot);
}

void KX_GameObject::NodeSetGlobalOrientation(const MT_Matrix3x3& rot)
{
	// check on valid node in case a python controller holds a reference to a deleted object
	if (!GetSGNode())
		return;

	if (GetSGNode()->GetSGParent())
		GetSGNode()->SetLocalOrientation(GetSGNode()->GetSGParent()->GetWorldOrientation().inverse()*rot);
	else
		NodeSetLocalOrientation(rot);
}

void KX_GameObject::NodeSetLocalScale(const MT_Vector3& scale)
{
	// check on valid node in case a python controller holds a reference to a deleted object
	if (!GetSGNode())
		return;

	if (m_pPhysicsController && !GetSGNode()->GetSGParent())
	{
		// see note above
		m_pPhysicsController->SetScaling(scale);
	}
	GetSGNode()->SetLocalScale(scale);
}



void KX_GameObject::NodeSetRelativeScale(const MT_Vector3& scale)
{
	if (GetSGNode())
	{
		GetSGNode()->RelativeScale(scale);
		if (m_pPhysicsController && (!GetSGNode()->GetSGParent()))
		{
			// see note above
			// we can use the local scale: it's the same thing for a root object 
			// and the world scale is not yet updated
			MT_Vector3 newscale = GetSGNode()->GetLocalScale();
			m_pPhysicsController->SetScaling(newscale);
		}
	}
}

void KX_GameObject::NodeSetWorldScale(const MT_Vector3& scale)
{
	if (!GetSGNode())
		return;
	SG_Node* parent = GetSGNode()->GetSGParent();
	if (parent != NULL)
	{
		// Make sure the objects have some scale
		MT_Vector3 p_scale = parent->GetWorldScaling();
		if (fabs(p_scale[0]) < (MT_Scalar)FLT_EPSILON ||
			fabs(p_scale[1]) < (MT_Scalar)FLT_EPSILON ||
			fabs(p_scale[2]) < (MT_Scalar)FLT_EPSILON)
		{ 
			return; 
		}

		p_scale[0] = 1/p_scale[0];
		p_scale[1] = 1/p_scale[1];
		p_scale[2] = 1/p_scale[2];

		NodeSetLocalScale(scale * p_scale);
	}
	else
	{
		NodeSetLocalScale(scale);
	}
}

void KX_GameObject::NodeSetWorldPosition(const MT_Point3& trans)
{
	if (!GetSGNode())
		return;
	SG_Node* parent = GetSGNode()->GetSGParent();
	if (parent != NULL)
	{
		// Make sure the objects have some scale
		MT_Vector3 scale = parent->GetWorldScaling();
		if (fabs(scale[0]) < (MT_Scalar)FLT_EPSILON ||
			fabs(scale[1]) < (MT_Scalar)FLT_EPSILON ||
			fabs(scale[2]) < (MT_Scalar)FLT_EPSILON)
		{ 
			return; 
		}
		scale[0] = 1.0f/scale[0];
		scale[1] = 1.0f/scale[1];
		scale[2] = 1.0f/scale[2];
		MT_Matrix3x3 invori = parent->GetWorldOrientation().inverse();
		MT_Vector3 newpos = invori*(trans-parent->GetWorldPosition())*scale;
		NodeSetLocalPosition(MT_Point3(newpos[0],newpos[1],newpos[2]));
	}
	else
	{
		NodeSetLocalPosition(trans);
	}
}


void KX_GameObject::NodeUpdateGS(double time)
{
	if (GetSGNode())
		GetSGNode()->UpdateWorldData(time);
}



const MT_Matrix3x3& KX_GameObject::NodeGetWorldOrientation() const
{
	// check on valid node in case a python controller holds a reference to a deleted object
	if (!GetSGNode())
		return dummy_orientation;
	return GetSGNode()->GetWorldOrientation();
}

const MT_Matrix3x3& KX_GameObject::NodeGetLocalOrientation() const
{
	// check on valid node in case a python controller holds a reference to a deleted object
	if (!GetSGNode())
		return dummy_orientation;
	return GetSGNode()->GetLocalOrientation();
}

const MT_Vector3& KX_GameObject::NodeGetWorldScaling() const
{
	// check on valid node in case a python controller holds a reference to a deleted object
	if (!GetSGNode())
		return dummy_scaling;

	return GetSGNode()->GetWorldScaling();
}

const MT_Vector3& KX_GameObject::NodeGetLocalScaling() const
{
	// check on valid node in case a python controller holds a reference to a deleted object
	if (!GetSGNode())
		return dummy_scaling;

	return GetSGNode()->GetLocalScale();
}

const MT_Point3& KX_GameObject::NodeGetWorldPosition() const
{
	// check on valid node in case a python controller holds a reference to a deleted object
	if (GetSGNode())
		return GetSGNode()->GetWorldPosition();
	else
		return dummy_point;
}

const MT_Point3& KX_GameObject::NodeGetLocalPosition() const
{
	// check on valid node in case a python controller holds a reference to a deleted object
	if (GetSGNode())
		return GetSGNode()->GetLocalPosition();
	else
		return dummy_point;
}


void KX_GameObject::UnregisterCollisionCallbacks()
{
	if (!GetPhysicsController()) {
		printf("Warning, trying to unregister collision callbacks for object without collisions: %s!\n", GetName().ReadPtr());
		return;
	}

	// Unregister from callbacks
	KX_Scene* scene = GetScene();
	PHY_IPhysicsEnvironment* pe = scene->GetPhysicsEnvironment();
	PHY_IPhysicsController* spc = GetPhysicsController();
	// If we are the last to unregister on this physics controller
	if (pe->RemoveCollisionCallback(spc)) {
		// If we are a sensor object
		if (m_pClient_info->isSensor())
			// Remove sensor body from physics world
			pe->RemoveSensor(spc);
	}
}

void KX_GameObject::RegisterCollisionCallbacks()
{
	if (!GetPhysicsController()) {
		printf("Warning, trying to register collision callbacks for object without collisions: %s!\n", GetName().ReadPtr());
		return;
	}

	// Register from callbacks
	KX_Scene* scene = GetScene();
	PHY_IPhysicsEnvironment* pe = scene->GetPhysicsEnvironment();
	PHY_IPhysicsController* spc = GetPhysicsController();
	// If we are the first to register on this physics controller
	if (pe->RequestCollisionCallback(spc)) {
		// If we are a sensor object
		if (m_pClient_info->isSensor())
			// Add sensor body to physics world
			pe->AddSensor(spc);
	}
}
void KX_GameObject::RunCollisionCallbacks(KX_GameObject *collider, const MT_Vector3 &point, const MT_Vector3 &normal)
{
#ifdef WITH_PYTHON
	if (!m_collisionCallbacks || PyList_GET_SIZE(m_collisionCallbacks) == 0)
		return;

	PyObject *args[] = {collider->GetProxy(), PyObjectFrom(point), PyObjectFrom(normal)};
	RunPythonCallBackList(m_collisionCallbacks, args, 1, ARRAY_SIZE(args));

	for (unsigned int i = 0; i < ARRAY_SIZE(args); ++i) {
		Py_DECREF(args[i]);
	}
#endif
}

/* Suspend/ resume: for the dynamic behavior, there is a simple
 * method. For the residual motion, there is not. I wonder what the
 * correct solution is for Sumo. Remove from the motion-update tree?
 *
 * So far, only switch the physics and logic.
 * */

void KX_GameObject::Resume(void)
{
	if (m_suspended) {
		SCA_IObject::Resume();
		// Child objects must be static, so we block changing to dynamic
		if (GetPhysicsController() && !GetParent())
			GetPhysicsController()->RestoreDynamics();

		m_suspended = false;
	}
}

void KX_GameObject::Suspend()
{
	if ((!m_ignore_activity_culling) && (!m_suspended)) {
		SCA_IObject::Suspend();
		if (GetPhysicsController())
			GetPhysicsController()->SuspendDynamics();
		m_suspended = true;
	}
}

static void walk_children(SG_Node* node, CListValue* list, bool recursive)
{
	if (!node)
		return;
	NodeList& children = node->GetSGChildren();

	for (NodeList::iterator childit = children.begin();!(childit==children.end());++childit)
	{
		SG_Node* childnode = (*childit);
		CValue* childobj = (CValue*)childnode->GetSGClientObject();
		if (childobj != NULL) // This is a GameObject
		{
			// add to the list
			list->Add(childobj->AddRef());
		}
		
		// if the childobj is NULL then this may be an inverse parent link
		// so a non recursive search should still look down this node.
		if (recursive || childobj==NULL) {
			walk_children(childnode, list, recursive);
		}
	}
}

CListValue* KX_GameObject::GetChildren()
{
	CListValue* list = new CListValue();
	walk_children(GetSGNode(), list, 0); /* GetSGNode() is always valid or it would have raised an exception before this */
	return list;
}

CListValue* KX_GameObject::GetChildrenRecursive()
{
	CListValue* list = new CListValue();
	walk_children(GetSGNode(), list, 1);
	return list;
}

KX_Scene* KX_GameObject::GetScene()
{
	SG_Node* node = this->GetSGNode();
	if (node == NULL) {
		// this happens for object in non active layers, rely on static scene then
		return KX_GetActiveScene();
	}
	return static_cast<KX_Scene*>(node->GetSGClientInfo());
}

/* ---------------------------------------------------------------------
 * Some stuff taken from the header
 * --------------------------------------------------------------------- */
void KX_GameObject::Relink(CTR_Map<CTR_HashedPtr, void*> *map_parameter)
{
	// we will relink the sensors and actuators that use object references
	// if the object is part of the replicated hierarchy, use the new
	// object reference instead
	SCA_SensorList& sensorlist = GetSensors();
	SCA_SensorList::iterator sit;
	for (sit=sensorlist.begin(); sit != sensorlist.end(); sit++)
	{
		(*sit)->Relink(map_parameter);
	}
	SCA_ActuatorList& actuatorlist = GetActuators();
	SCA_ActuatorList::iterator ait;
	for (ait=actuatorlist.begin(); ait != actuatorlist.end(); ait++)
	{
		(*ait)->Relink(map_parameter);
	}
}

#ifdef USE_MATHUTILS

/* These require an SGNode */
#define MATHUTILS_VEC_CB_POS_LOCAL 1
#define MATHUTILS_VEC_CB_POS_GLOBAL 2
#define MATHUTILS_VEC_CB_SCALE_LOCAL 3
#define MATHUTILS_VEC_CB_SCALE_GLOBAL 4
#define MATHUTILS_VEC_CB_INERTIA_LOCAL 5
#define MATHUTILS_VEC_CB_OBJECT_COLOR 6
#define MATHUTILS_VEC_CB_LINVEL_LOCAL 7
#define MATHUTILS_VEC_CB_LINVEL_GLOBAL 8
#define MATHUTILS_VEC_CB_ANGVEL_LOCAL 9
#define MATHUTILS_VEC_CB_ANGVEL_GLOBAL 10

static unsigned char mathutils_kxgameob_vector_cb_index= -1; /* index for our callbacks */

static int mathutils_kxgameob_generic_check(BaseMathObject *bmo)
{
	KX_GameObject* self = static_cast<KX_GameObject*>BGE_PROXY_REF(bmo->cb_user);
	if (self == NULL)
		return -1;
	
	return 0;
}

static int mathutils_kxgameob_vector_get(BaseMathObject *bmo, int subtype)
{
	KX_GameObject* self = static_cast<KX_GameObject*>BGE_PROXY_REF(bmo->cb_user);
	if (self == NULL)
		return -1;

#define PHYS_ERR(attr) PyErr_SetString(PyExc_AttributeError, "KX_GameObject." attr ", is missing a physics controller")

	switch (subtype) {
		case MATHUTILS_VEC_CB_POS_LOCAL:
			self->NodeGetLocalPosition().getValue(bmo->data);
			break;
		case MATHUTILS_VEC_CB_POS_GLOBAL:
			self->NodeGetWorldPosition().getValue(bmo->data);
			break;
		case MATHUTILS_VEC_CB_SCALE_LOCAL:
			self->NodeGetLocalScaling().getValue(bmo->data);
			break;
		case MATHUTILS_VEC_CB_SCALE_GLOBAL:
			self->NodeGetWorldScaling().getValue(bmo->data);
			break;
		case MATHUTILS_VEC_CB_INERTIA_LOCAL:
			if (!self->GetPhysicsController()) return PHYS_ERR("localInertia"), -1;
			self->GetPhysicsController()->GetLocalInertia().getValue(bmo->data);
			break;
		case MATHUTILS_VEC_CB_OBJECT_COLOR:
			self->GetObjectColor().getValue(bmo->data);
			break;
		case MATHUTILS_VEC_CB_LINVEL_LOCAL:
			if (!self->GetPhysicsController()) return PHYS_ERR("localLinearVelocity"), -1;
			self->GetLinearVelocity(true).getValue(bmo->data);
			break;
		case MATHUTILS_VEC_CB_LINVEL_GLOBAL:
			if (!self->GetPhysicsController()) return PHYS_ERR("worldLinearVelocity"), -1;
			self->GetLinearVelocity(false).getValue(bmo->data);
			break;
		case MATHUTILS_VEC_CB_ANGVEL_LOCAL:
			if (!self->GetPhysicsController()) return PHYS_ERR("localLinearVelocity"), -1;
			self->GetAngularVelocity(true).getValue(bmo->data);
			break;
		case MATHUTILS_VEC_CB_ANGVEL_GLOBAL:
			if (!self->GetPhysicsController()) return PHYS_ERR("worldLinearVelocity"), -1;
			self->GetAngularVelocity(false).getValue(bmo->data);
			break;
			
	}
	
#undef PHYS_ERR
	
	return 0;
}

static int mathutils_kxgameob_vector_set(BaseMathObject *bmo, int subtype)
{
	KX_GameObject* self = static_cast<KX_GameObject*>BGE_PROXY_REF(bmo->cb_user);
	if (self == NULL)
		return -1;
	
	switch (subtype) {
		case MATHUTILS_VEC_CB_POS_LOCAL:
			self->NodeSetLocalPosition(MT_Point3(bmo->data));
			self->NodeUpdateGS(0.f);
			break;
		case MATHUTILS_VEC_CB_POS_GLOBAL:
			self->NodeSetWorldPosition(MT_Point3(bmo->data));
			self->NodeUpdateGS(0.f);
			break;
		case MATHUTILS_VEC_CB_SCALE_LOCAL:
			self->NodeSetLocalScale(MT_Point3(bmo->data));
			self->NodeUpdateGS(0.f);
			break;
		case MATHUTILS_VEC_CB_SCALE_GLOBAL:
			PyErr_SetString(PyExc_AttributeError, "KX_GameObject.worldScale is read-only");
			return -1;
		case MATHUTILS_VEC_CB_INERTIA_LOCAL:
			/* read only */
			break;
		case MATHUTILS_VEC_CB_OBJECT_COLOR:
			self->SetObjectColor(MT_Vector4(bmo->data));
			break;
		case MATHUTILS_VEC_CB_LINVEL_LOCAL:
			self->setLinearVelocity(MT_Point3(bmo->data),true);
			break;
		case MATHUTILS_VEC_CB_LINVEL_GLOBAL:
			self->setLinearVelocity(MT_Point3(bmo->data),false);
			break;
		case MATHUTILS_VEC_CB_ANGVEL_LOCAL:
			self->setAngularVelocity(MT_Point3(bmo->data),true);
			break;
		case MATHUTILS_VEC_CB_ANGVEL_GLOBAL:
			self->setAngularVelocity(MT_Point3(bmo->data),false);
			break;
	}
	
	return 0;
}

static int mathutils_kxgameob_vector_get_index(BaseMathObject *bmo, int subtype, int index)
{
	/* lazy, avoid repeteing the case statement */
	if (mathutils_kxgameob_vector_get(bmo, subtype) == -1)
		return -1;
	return 0;
}

static int mathutils_kxgameob_vector_set_index(BaseMathObject *bmo, int subtype, int index)
{
	float f= bmo->data[index];
	
	/* lazy, avoid repeteing the case statement */
	if (mathutils_kxgameob_vector_get(bmo, subtype) == -1)
		return -1;
	
	bmo->data[index] = f;
	return mathutils_kxgameob_vector_set(bmo, subtype);
}

static Mathutils_Callback mathutils_kxgameob_vector_cb = {
	mathutils_kxgameob_generic_check,
	mathutils_kxgameob_vector_get,
	mathutils_kxgameob_vector_set,
	mathutils_kxgameob_vector_get_index,
	mathutils_kxgameob_vector_set_index
};

/* Matrix */
#define MATHUTILS_MAT_CB_ORI_LOCAL 1
#define MATHUTILS_MAT_CB_ORI_GLOBAL 2

static unsigned char mathutils_kxgameob_matrix_cb_index= -1; /* index for our callbacks */

static int mathutils_kxgameob_matrix_get(BaseMathObject *bmo, int subtype)
{
	KX_GameObject* self = static_cast<KX_GameObject*>BGE_PROXY_REF(bmo->cb_user);
	if (self == NULL)
		return -1;

	switch (subtype) {
		case MATHUTILS_MAT_CB_ORI_LOCAL:
			self->NodeGetLocalOrientation().getValue3x3(bmo->data);
			break;
		case MATHUTILS_MAT_CB_ORI_GLOBAL:
			self->NodeGetWorldOrientation().getValue3x3(bmo->data);
			break;
	}
	
	return 0;
}


static int mathutils_kxgameob_matrix_set(BaseMathObject *bmo, int subtype)
{
	KX_GameObject* self = static_cast<KX_GameObject*>BGE_PROXY_REF(bmo->cb_user);
	if (self == NULL)
		return -1;
	
	MT_Matrix3x3 mat3x3;
	switch (subtype) {
		case MATHUTILS_MAT_CB_ORI_LOCAL:
			mat3x3.setValue3x3(bmo->data);
			self->NodeSetLocalOrientation(mat3x3);
			self->NodeUpdateGS(0.f);
			break;
		case MATHUTILS_MAT_CB_ORI_GLOBAL:
			mat3x3.setValue3x3(bmo->data);
			self->NodeSetLocalOrientation(mat3x3);
			self->NodeUpdateGS(0.f);
			break;
	}
	
	return 0;
}

static Mathutils_Callback mathutils_kxgameob_matrix_cb = {
	mathutils_kxgameob_generic_check,
	mathutils_kxgameob_matrix_get,
	mathutils_kxgameob_matrix_set,
	NULL,
	NULL
};


void KX_GameObject_Mathutils_Callback_Init(void)
{
	// register mathutils callbacks, ok to run more than once.
	mathutils_kxgameob_vector_cb_index= Mathutils_RegisterCallback(&mathutils_kxgameob_vector_cb);
	mathutils_kxgameob_matrix_cb_index= Mathutils_RegisterCallback(&mathutils_kxgameob_matrix_cb);
}

#endif // USE_MATHUTILS

#ifdef WITH_PYTHON
/* ------- python stuff ---------------------------------------------------*/
PyMethodDef KX_GameObject::Methods[] = {
	{"applyForce", (PyCFunction)	KX_GameObject::sPyApplyForce, METH_VARARGS},
	{"applyTorque", (PyCFunction)	KX_GameObject::sPyApplyTorque, METH_VARARGS},
	{"applyRotation", (PyCFunction)	KX_GameObject::sPyApplyRotation, METH_VARARGS},
	{"applyMovement", (PyCFunction)	KX_GameObject::sPyApplyMovement, METH_VARARGS},
	{"getLinearVelocity", (PyCFunction) KX_GameObject::sPyGetLinearVelocity, METH_VARARGS},
	{"setLinearVelocity", (PyCFunction) KX_GameObject::sPySetLinearVelocity, METH_VARARGS},
	{"getAngularVelocity", (PyCFunction) KX_GameObject::sPyGetAngularVelocity, METH_VARARGS},
	{"setAngularVelocity", (PyCFunction) KX_GameObject::sPySetAngularVelocity, METH_VARARGS},
	{"getVelocity", (PyCFunction) KX_GameObject::sPyGetVelocity, METH_VARARGS},
	{"setDamping", (PyCFunction) KX_GameObject::sPySetDamping, METH_VARARGS},
	{"getReactionForce", (PyCFunction) KX_GameObject::sPyGetReactionForce, METH_NOARGS},
	{"alignAxisToVect",(PyCFunction) KX_GameObject::sPyAlignAxisToVect, METH_VARARGS},
	{"getAxisVect",(PyCFunction) KX_GameObject::sPyGetAxisVect, METH_O},
	{"suspendDynamics", (PyCFunction)KX_GameObject::sPySuspendDynamics, METH_VARARGS},
	{"restoreDynamics", (PyCFunction)KX_GameObject::sPyRestoreDynamics,METH_NOARGS},
	{"enableRigidBody", (PyCFunction)KX_GameObject::sPyEnableRigidBody,METH_NOARGS},
	{"disableRigidBody", (PyCFunction)KX_GameObject::sPyDisableRigidBody,METH_NOARGS},
	{"applyImpulse", (PyCFunction) KX_GameObject::sPyApplyImpulse, METH_VARARGS},
	{"setCollisionMargin", (PyCFunction) KX_GameObject::sPySetCollisionMargin, METH_O},
	{"setParent", (PyCFunction)KX_GameObject::sPySetParent,METH_VARARGS},
	{"setVisible",(PyCFunction) KX_GameObject::sPySetVisible, METH_VARARGS},
	{"setOcclusion",(PyCFunction) KX_GameObject::sPySetOcclusion, METH_VARARGS},
	{"removeParent", (PyCFunction)KX_GameObject::sPyRemoveParent,METH_NOARGS},


	{"getPhysicsId", (PyCFunction)KX_GameObject::sPyGetPhysicsId,METH_NOARGS},
	{"getPropertyNames", (PyCFunction)KX_GameObject::sPyGetPropertyNames,METH_NOARGS},
	{"replaceMesh",(PyCFunction) KX_GameObject::sPyReplaceMesh, METH_VARARGS},
	{"endObject",(PyCFunction) KX_GameObject::sPyEndObject, METH_NOARGS},
	{"reinstancePhysicsMesh", (PyCFunction)KX_GameObject::sPyReinstancePhysicsMesh,METH_VARARGS},
	
	KX_PYMETHODTABLE(KX_GameObject, rayCastTo),
	KX_PYMETHODTABLE(KX_GameObject, rayCast),
	KX_PYMETHODTABLE_O(KX_GameObject, getDistanceTo),
	KX_PYMETHODTABLE_O(KX_GameObject, getVectTo),
	KX_PYMETHODTABLE(KX_GameObject, sendMessage),
	KX_PYMETHODTABLE(KX_GameObject, addDebugProperty),

	KX_PYMETHODTABLE_KEYWORDS(KX_GameObject, playAction),
	KX_PYMETHODTABLE(KX_GameObject, stopAction),
	KX_PYMETHODTABLE(KX_GameObject, getActionFrame),
	KX_PYMETHODTABLE(KX_GameObject, getActionName),
	KX_PYMETHODTABLE(KX_GameObject, setActionFrame),
	KX_PYMETHODTABLE(KX_GameObject, isPlayingAction),
	
	// dict style access for props
	{"get",(PyCFunction) KX_GameObject::sPyget, METH_VARARGS},
	
	{NULL,NULL} //Sentinel
};

PyAttributeDef KX_GameObject::Attributes[] = {
	KX_PYATTRIBUTE_INT_RO("currentLodLevel", KX_GameObject, m_currentLodLevel),
	KX_PYATTRIBUTE_RO_FUNCTION("name",		KX_GameObject, pyattr_get_name),
	KX_PYATTRIBUTE_RO_FUNCTION("parent",	KX_GameObject, pyattr_get_parent),
	KX_PYATTRIBUTE_RO_FUNCTION("groupMembers",	KX_GameObject, pyattr_get_group_members),
	KX_PYATTRIBUTE_RO_FUNCTION("groupObject",	KX_GameObject, pyattr_get_group_object),
	KX_PYATTRIBUTE_RO_FUNCTION("scene",		KX_GameObject, pyattr_get_scene),
	KX_PYATTRIBUTE_RO_FUNCTION("life",		KX_GameObject, pyattr_get_life),
	KX_PYATTRIBUTE_RW_FUNCTION("mass",		KX_GameObject, pyattr_get_mass,		pyattr_set_mass),
	KX_PYATTRIBUTE_RO_FUNCTION("isSuspendDynamics",		KX_GameObject, pyattr_get_is_suspend_dynamics),
	KX_PYATTRIBUTE_RW_FUNCTION("linVelocityMin",		KX_GameObject, pyattr_get_lin_vel_min, pyattr_set_lin_vel_min),
	KX_PYATTRIBUTE_RW_FUNCTION("linVelocityMax",		KX_GameObject, pyattr_get_lin_vel_max, pyattr_set_lin_vel_max),
	KX_PYATTRIBUTE_RW_FUNCTION("angularVelocityMin", KX_GameObject, pyattr_get_ang_vel_min, pyattr_set_ang_vel_min),
	KX_PYATTRIBUTE_RW_FUNCTION("angularVelocityMax", KX_GameObject, pyattr_get_ang_vel_max, pyattr_set_ang_vel_max),
	KX_PYATTRIBUTE_RW_FUNCTION("visible",	KX_GameObject, pyattr_get_visible,	pyattr_set_visible),
	KX_PYATTRIBUTE_RW_FUNCTION("record_animation",	KX_GameObject, pyattr_get_record_animation,	pyattr_set_record_animation),
	KX_PYATTRIBUTE_BOOL_RW    ("occlusion", KX_GameObject, m_bOccluder),
	KX_PYATTRIBUTE_RW_FUNCTION("position",	KX_GameObject, pyattr_get_worldPosition,	pyattr_set_localPosition),
	KX_PYATTRIBUTE_RO_FUNCTION("localInertia",	KX_GameObject, pyattr_get_localInertia),
	KX_PYATTRIBUTE_RW_FUNCTION("orientation",KX_GameObject,pyattr_get_worldOrientation,pyattr_set_localOrientation),
	KX_PYATTRIBUTE_RW_FUNCTION("scaling",	KX_GameObject, pyattr_get_worldScaling,	pyattr_set_localScaling),
	KX_PYATTRIBUTE_RW_FUNCTION("timeOffset",KX_GameObject, pyattr_get_timeOffset,pyattr_set_timeOffset),
	KX_PYATTRIBUTE_RW_FUNCTION("collisionCallbacks",		KX_GameObject, pyattr_get_collisionCallbacks,	pyattr_set_collisionCallbacks),
	KX_PYATTRIBUTE_RW_FUNCTION("collisionGroup",			KX_GameObject, pyattr_get_collisionGroup, pyattr_set_collisionGroup),
	KX_PYATTRIBUTE_RW_FUNCTION("collisionMask",				KX_GameObject, pyattr_get_collisionMask, pyattr_set_collisionMask),
	KX_PYATTRIBUTE_RW_FUNCTION("state",		KX_GameObject, pyattr_get_state,	pyattr_set_state),
	KX_PYATTRIBUTE_RO_FUNCTION("meshes",	KX_GameObject, pyattr_get_meshes),
	KX_PYATTRIBUTE_RW_FUNCTION("localOrientation",KX_GameObject,pyattr_get_localOrientation,pyattr_set_localOrientation),
	KX_PYATTRIBUTE_RW_FUNCTION("worldOrientation",KX_GameObject,pyattr_get_worldOrientation,pyattr_set_worldOrientation),
	KX_PYATTRIBUTE_RW_FUNCTION("localPosition",	KX_GameObject, pyattr_get_localPosition,	pyattr_set_localPosition),
	KX_PYATTRIBUTE_RW_FUNCTION("worldPosition",	KX_GameObject, pyattr_get_worldPosition,    pyattr_set_worldPosition),
	KX_PYATTRIBUTE_RW_FUNCTION("localScale",	KX_GameObject, pyattr_get_localScaling,	pyattr_set_localScaling),
	KX_PYATTRIBUTE_RW_FUNCTION("worldScale",	KX_GameObject, pyattr_get_worldScaling, pyattr_set_worldScaling),
	KX_PYATTRIBUTE_RW_FUNCTION("localTransform",		KX_GameObject, pyattr_get_localTransform, pyattr_set_localTransform),
	KX_PYATTRIBUTE_RW_FUNCTION("worldTransform",		KX_GameObject, pyattr_get_worldTransform, pyattr_set_worldTransform),
	KX_PYATTRIBUTE_RW_FUNCTION("linearVelocity", KX_GameObject, pyattr_get_localLinearVelocity, pyattr_set_worldLinearVelocity),
	KX_PYATTRIBUTE_RW_FUNCTION("localLinearVelocity", KX_GameObject, pyattr_get_localLinearVelocity, pyattr_set_localLinearVelocity),
	KX_PYATTRIBUTE_RW_FUNCTION("worldLinearVelocity", KX_GameObject, pyattr_get_worldLinearVelocity, pyattr_set_worldLinearVelocity),
	KX_PYATTRIBUTE_RW_FUNCTION("angularVelocity", KX_GameObject, pyattr_get_localAngularVelocity, pyattr_set_worldAngularVelocity),
	KX_PYATTRIBUTE_RW_FUNCTION("localAngularVelocity", KX_GameObject, pyattr_get_localAngularVelocity, pyattr_set_localAngularVelocity),
	KX_PYATTRIBUTE_RW_FUNCTION("worldAngularVelocity", KX_GameObject, pyattr_get_worldAngularVelocity, pyattr_set_worldAngularVelocity),
	KX_PYATTRIBUTE_RW_FUNCTION("linearDamping", KX_GameObject, pyattr_get_linearDamping, pyattr_set_linearDamping),
	KX_PYATTRIBUTE_RW_FUNCTION("angularDamping", KX_GameObject, pyattr_get_angularDamping, pyattr_set_angularDamping),
	KX_PYATTRIBUTE_RO_FUNCTION("children",	KX_GameObject, pyattr_get_children),
	KX_PYATTRIBUTE_RO_FUNCTION("childrenRecursive",	KX_GameObject, pyattr_get_children_recursive),
	KX_PYATTRIBUTE_RO_FUNCTION("attrDict",	KX_GameObject, pyattr_get_attrDict),
	KX_PYATTRIBUTE_RW_FUNCTION("color", KX_GameObject, pyattr_get_obcolor, pyattr_set_obcolor),
	KX_PYATTRIBUTE_RW_FUNCTION("debug",	KX_GameObject, pyattr_get_debug, pyattr_set_debug),
	KX_PYATTRIBUTE_RW_FUNCTION("debugRecursive",	KX_GameObject, pyattr_get_debugRecursive, pyattr_set_debugRecursive),
	
	/* experimental, don't rely on these yet */
	KX_PYATTRIBUTE_RO_FUNCTION("sensors",		KX_GameObject, pyattr_get_sensors),
	KX_PYATTRIBUTE_RO_FUNCTION("controllers",	KX_GameObject, pyattr_get_controllers),
	KX_PYATTRIBUTE_RO_FUNCTION("actuators",		KX_GameObject, pyattr_get_actuators),
	{NULL} //Sentinel
};

PyObject *KX_GameObject::PyReplaceMesh(PyObject *args)
{
	KX_Scene *scene = KX_GetActiveScene();
	SCA_LogicManager *logicmgr = GetScene()->GetLogicManager();

	PyObject *value;
	int use_gfx= 1, use_phys= 0;
	RAS_MeshObject *new_mesh;
	
	if (!PyArg_ParseTuple(args,"O|ii:replaceMesh", &value, &use_gfx, &use_phys))
		return NULL;
	
	if (!ConvertPythonToMesh(logicmgr, value, &new_mesh, false, "gameOb.replaceMesh(value): KX_GameObject"))
		return NULL;
	
	scene->ReplaceMesh(this, new_mesh, (bool)use_gfx, (bool)use_phys);
	Py_RETURN_NONE;
}

PyObject *KX_GameObject::PyEndObject()
{
	KX_Scene* scene = GetScene();
	
	scene->DelayedRemoveObject(this);
	
	Py_RETURN_NONE;

}

PyObject *KX_GameObject::PyReinstancePhysicsMesh(PyObject *args)
{
	KX_GameObject *gameobj= NULL;
	RAS_MeshObject *mesh= NULL;
	SCA_LogicManager *logicmgr = GetScene()->GetLogicManager();
	
	PyObject *gameobj_py= NULL;
	PyObject *mesh_py= NULL;

	if (	!PyArg_ParseTuple(args,"|OO:reinstancePhysicsMesh",&gameobj_py, &mesh_py) ||
			(gameobj_py && !ConvertPythonToGameObject(logicmgr, gameobj_py, &gameobj, true, "gameOb.reinstancePhysicsMesh(obj, mesh): KX_GameObject")) || 
			(mesh_py && !ConvertPythonToMesh(logicmgr, mesh_py, &mesh, true, "gameOb.reinstancePhysicsMesh(obj, mesh): KX_GameObject"))
		) {
		return NULL;
	}

	/* gameobj and mesh can be NULL */
	if (GetPhysicsController() && GetPhysicsController()->ReinstancePhysicsShape(gameobj, mesh))
		Py_RETURN_TRUE;

	Py_RETURN_FALSE;
}

static PyObject *Map_GetItem(PyObject *self_v, PyObject *item)
{
	KX_GameObject* self = static_cast<KX_GameObject*>BGE_PROXY_REF(self_v);
	const char *attr_str= _PyUnicode_AsString(item);
	CValue* resultattr;
	PyObject *pyconvert;
	
	if (self == NULL) {
		PyErr_SetString(PyExc_SystemError, "val = gameOb[key]: KX_GameObject, " BGE_PROXY_ERROR_MSG);
		return NULL;
	}
	
	/* first see if the attributes a string and try get the cvalue attribute */
	if (attr_str && (resultattr=self->GetProperty(attr_str))) {
		pyconvert = resultattr->ConvertValueToPython();
		return pyconvert ? pyconvert:resultattr->GetProxy();
	}
	/* no CValue attribute, try get the python only m_attr_dict attribute */
	else if (self->m_attr_dict && (pyconvert=PyDict_GetItem(self->m_attr_dict, item))) {
		
		if (attr_str)
			PyErr_Clear();
		Py_INCREF(pyconvert);
		return pyconvert;
	}
	else {
		if (attr_str)	PyErr_Format(PyExc_KeyError, "value = gameOb[key]: KX_GameObject, key \"%s\" does not exist", attr_str);
		else			PyErr_SetString(PyExc_KeyError, "value = gameOb[key]: KX_GameObject, key does not exist");
		return NULL;
	}
		
}


static int Map_SetItem(PyObject *self_v, PyObject *key, PyObject *val)
{
	KX_GameObject* self = static_cast<KX_GameObject*>BGE_PROXY_REF(self_v);
	const char *attr_str= _PyUnicode_AsString(key);
	if (attr_str==NULL)
		PyErr_Clear();
	
	if (self == NULL) {
		PyErr_SetString(PyExc_SystemError, "gameOb[key] = value: KX_GameObject, " BGE_PROXY_ERROR_MSG);
		return -1;
	}
	
	if (val==NULL) { /* del ob["key"] */
		int del= 0;
		
		/* try remove both just in case */
		if (attr_str)
			del |= (self->RemoveProperty(attr_str)==true) ? 1:0;
		
		if (self->m_attr_dict)
			del |= (PyDict_DelItem(self->m_attr_dict, key)==0) ? 1:0;
		
		if (del==0) {
			if (attr_str)	PyErr_Format(PyExc_KeyError, "gameOb[key] = value: KX_GameObject, key \"%s\" could not be set", attr_str);
			else			PyErr_SetString(PyExc_KeyError, "del gameOb[key]: KX_GameObject, key could not be deleted");
			return -1;
		}
		else if (self->m_attr_dict) {
			PyErr_Clear(); /* PyDict_DelItem sets an error when it fails */
		}
	}
	else { /* ob["key"] = value */
		bool set = false;
		
		/* as CValue */
		if (attr_str && PyObject_TypeCheck(val, &PyObjectPlus::Type)==0) /* don't allow GameObjects for eg to be assigned to CValue props */
		{
			CValue *vallie = self->ConvertPythonToValue(val, false, "gameOb[key] = value: ");
			
			if (vallie) {
				CValue* oldprop = self->GetProperty(attr_str);
				
				if (oldprop)
					oldprop->SetValue(vallie);
				else
					self->SetProperty(attr_str, vallie);
				
				vallie->Release();
				set = true;
				
				/* try remove dict value to avoid double ups */
				if (self->m_attr_dict) {
					if (PyDict_DelItem(self->m_attr_dict, key) != 0)
						PyErr_Clear();
				}
			}
			else if (PyErr_Occurred()) {
				return -1;
			}
		}
		
		if (set == false) {
			if (self->m_attr_dict==NULL) /* lazy init */
				self->m_attr_dict= PyDict_New();
			
			
			if (PyDict_SetItem(self->m_attr_dict, key, val)==0)
			{
				if (attr_str)
					self->RemoveProperty(attr_str); /* overwrite the CValue if it exists */
				set = true;
			}
			else {
				if (attr_str)	PyErr_Format(PyExc_KeyError, "gameOb[key] = value: KX_GameObject, key \"%s\" not be added to internal dictionary", attr_str);
				else			PyErr_SetString(PyExc_KeyError, "gameOb[key] = value: KX_GameObject, key not be added to internal dictionary");
			}
		}
		
		if (set == false) {
			return -1; /* pythons error value */
		}
		
	}
	
	return 0; /* success */
}

static int Seq_Contains(PyObject *self_v, PyObject *value)
{
	KX_GameObject* self = static_cast<KX_GameObject*>BGE_PROXY_REF(self_v);
	
	if (self == NULL) {
		PyErr_SetString(PyExc_SystemError, "val in gameOb: KX_GameObject, " BGE_PROXY_ERROR_MSG);
		return -1;
	}
	
	if (PyUnicode_Check(value) && self->GetProperty(_PyUnicode_AsString(value)))
		return 1;
	
	if (self->m_attr_dict && PyDict_GetItem(self->m_attr_dict, value))
		return 1;
	
	return 0;
}


PyMappingMethods KX_GameObject::Mapping = {
	(lenfunc)NULL					, 			/*inquiry mp_length */
	(binaryfunc)Map_GetItem,		/*binaryfunc mp_subscript */
	(objobjargproc)Map_SetItem,	/*objobjargproc mp_ass_subscript */
};

PySequenceMethods KX_GameObject::Sequence = {
	NULL,		/* Cant set the len otherwise it can evaluate as false */
	NULL,		/* sq_concat */
	NULL,		/* sq_repeat */
	NULL,		/* sq_item */
	NULL,		/* sq_slice */
	NULL,		/* sq_ass_item */
	NULL,		/* sq_ass_slice */
	(objobjproc)Seq_Contains,	/* sq_contains */
	(binaryfunc) NULL, /* sq_inplace_concat */
	(ssizeargfunc) NULL, /* sq_inplace_repeat */
};

PyTypeObject KX_GameObject::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"KX_GameObject",
	sizeof(PyObjectPlus_Proxy),
	0,
	py_base_dealloc,
	0,
	0,
	0,
	0,
	py_base_repr,
	0,
	&Sequence,
	&Mapping,
	0,0,0,
	NULL,
	NULL,
	0,
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	0,0,0,0,0,0,0,
	Methods,
	0,
	0,
	&SCA_IObject::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyObject *KX_GameObject::pyattr_get_name(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	return PyUnicode_From_STR_String(self->GetName());
}

PyObject *KX_GameObject::pyattr_get_parent(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	KX_GameObject* parent = self->GetParent();
	if (parent) {
		return parent->GetProxy();
	}
	Py_RETURN_NONE;
}

PyObject *KX_GameObject::pyattr_get_group_members(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	CListValue* instances = self->GetInstanceObjects();
	if (instances) {
		return instances->GetProxy();
	}
	Py_RETURN_NONE;
}

PyObject* KX_GameObject::pyattr_get_collisionCallbacks(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);

	// Only objects with a physics controller should have collision callbacks
	if (!self->GetPhysicsController()) {
		PyErr_SetString(PyExc_AttributeError, "KX_GameObject.collisionCallbacks: attribute only available for objects with collisions enabled");
		return NULL;
	}

	// Return the existing callbacks
	if (self->m_collisionCallbacks == NULL)
	{
		self->m_collisionCallbacks = PyList_New(0);
		// Subscribe to collision update from KX_TouchManager
		self->RegisterCollisionCallbacks();
	}
	Py_INCREF(self->m_collisionCallbacks);
	return self->m_collisionCallbacks;
}

int KX_GameObject::pyattr_set_collisionCallbacks(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);

	// Only objects with a physics controller should have collision callbacks
	if (!self->GetPhysicsController()) {
		PyErr_SetString(PyExc_AttributeError, "KX_GameObject.collisionCallbacks: attribute only available for objects with collisions enabled");
		return PY_SET_ATTR_FAIL;
	}

	if (!PyList_CheckExact(value))
	{
		PyErr_SetString(PyExc_ValueError, "Expected a list");
		return PY_SET_ATTR_FAIL;
	}

	if (self->m_collisionCallbacks == NULL) {
		self->RegisterCollisionCallbacks();
	} else {
		Py_DECREF(self->m_collisionCallbacks);
	}

	Py_INCREF(value);


	self->m_collisionCallbacks = value;

	return PY_SET_ATTR_SUCCESS;
}

PyObject *KX_GameObject::pyattr_get_collisionGroup(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	return PyLong_FromLong(self->GetUserCollisionGroup());
}

int KX_GameObject::pyattr_set_collisionGroup(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	int val = PyLong_AsLong(value);

	if (val == -1 && PyErr_Occurred()) {
		PyErr_SetString(PyExc_TypeError, "gameOb.collisionGroup = int: KX_GameObject, expected an int bit field");
		return PY_SET_ATTR_FAIL;
	}

	if (val == 0 || val & ~((1 << OB_MAX_COL_MASKS) - 1)) {
		PyErr_Format(PyExc_AttributeError, "gameOb.collisionGroup = int: KX_GameObject, expected a int bit field, 0 < group < %i", (1 << OB_MAX_COL_MASKS));
		return PY_SET_ATTR_FAIL;
	}

	self->SetUserCollisionGroup(val);
	return PY_SET_ATTR_SUCCESS;
}

PyObject *KX_GameObject::pyattr_get_collisionMask(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	return PyLong_FromLong(self->GetUserCollisionMask());
}

int KX_GameObject::pyattr_set_collisionMask(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	int val = PyLong_AsLong(value);

	if (val == -1 && PyErr_Occurred()) {
		PyErr_SetString(PyExc_TypeError, "gameOb.collisionMask = int: KX_GameObject, expected an int bit field");
		return PY_SET_ATTR_FAIL;
	}

	if (val == 0 || val & ~((1 << OB_MAX_COL_MASKS) - 1)) {
		PyErr_Format(PyExc_AttributeError, "gameOb.collisionMask = int: KX_GameObject, expected a int bit field, 0 < mask < %i", (1 << OB_MAX_COL_MASKS));
		return PY_SET_ATTR_FAIL;
	}

	self->SetUserCollisionMask(val);
	return PY_SET_ATTR_SUCCESS;
}

PyObject* KX_GameObject::pyattr_get_scene(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject *self = static_cast<KX_GameObject*>(self_v);
	KX_Scene *scene = self->GetScene();
	if (scene) {
		return scene->GetProxy();
	}
	Py_RETURN_NONE;
}

PyObject *KX_GameObject::pyattr_get_group_object(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	KX_GameObject* pivot = self->GetDupliGroupObject();
	if (pivot) {
		return pivot->GetProxy();
	}
	Py_RETURN_NONE;
}

PyObject *KX_GameObject::pyattr_get_life(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);

	CValue *life = self->GetProperty("::timebomb");
	if (life)
		// this convert the timebomb seconds to frames, hard coded 50.0f (assuming 50fps)
		// value hardcoded in KX_Scene::AddReplicaObject()
		return PyFloat_FromDouble(life->GetNumber() * 50.0);
	else
		Py_RETURN_NONE;
}

PyObject *KX_GameObject::pyattr_get_mass(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	PHY_IPhysicsController *spc = self->GetPhysicsController();
	return PyFloat_FromDouble(spc ? spc->GetMass() : 0.0f);
}

int KX_GameObject::pyattr_set_mass(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	PHY_IPhysicsController *spc = self->GetPhysicsController();
	MT_Scalar val = PyFloat_AsDouble(value);
	if (val < 0.0f) { /* also accounts for non float */
		PyErr_SetString(PyExc_AttributeError, "gameOb.mass = float: KX_GameObject, expected a float zero or above");
		return PY_SET_ATTR_FAIL;
	}

	if (spc)
		spc->SetMass(val);

	return PY_SET_ATTR_SUCCESS;
}

PyObject *KX_GameObject::pyattr_get_is_suspend_dynamics(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);

	// Only objects with a physics controller can be suspended
	if (!self->GetPhysicsController()) {
		PyErr_SetString(PyExc_AttributeError, "This object has not Physics Controller");
		return NULL;
	}

	return PyBool_FromLong(self->IsDynamicsSuspended());
}

PyObject *KX_GameObject::pyattr_get_lin_vel_min(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	PHY_IPhysicsController *spc = self->GetPhysicsController();
	return PyFloat_FromDouble(spc ? spc->GetLinVelocityMin() : 0.0f);
}

int KX_GameObject::pyattr_set_lin_vel_min(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	PHY_IPhysicsController *spc = self->GetPhysicsController();
	MT_Scalar val = PyFloat_AsDouble(value);
	if (val < 0.0f) { /* also accounts for non float */
		PyErr_SetString(PyExc_AttributeError, "gameOb.linVelocityMin = float: KX_GameObject, expected a float zero or above");
		return PY_SET_ATTR_FAIL;
	}

	if (spc)
		spc->SetLinVelocityMin(val);

	return PY_SET_ATTR_SUCCESS;
}

PyObject *KX_GameObject::pyattr_get_lin_vel_max(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	PHY_IPhysicsController *spc = self->GetPhysicsController();
	return PyFloat_FromDouble(spc ? spc->GetLinVelocityMax() : 0.0f);
}

int KX_GameObject::pyattr_set_lin_vel_max(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	PHY_IPhysicsController *spc = self->GetPhysicsController();
	MT_Scalar val = PyFloat_AsDouble(value);
	if (val < 0.0f) { /* also accounts for non float */
		PyErr_SetString(PyExc_AttributeError, "gameOb.linVelocityMax = float: KX_GameObject, expected a float zero or above");
		return PY_SET_ATTR_FAIL;
	}

	if (spc)
		spc->SetLinVelocityMax(val);

	return PY_SET_ATTR_SUCCESS;
}

PyObject *KX_GameObject::pyattr_get_ang_vel_min(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject *self = static_cast<KX_GameObject*>(self_v);
	PHY_IPhysicsController *spc = self->GetPhysicsController();
	return PyFloat_FromDouble(spc ? spc->GetAngularVelocityMin() : 0.0f);
}

int KX_GameObject::pyattr_set_ang_vel_min(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_GameObject *self = static_cast<KX_GameObject*>(self_v);
	PHY_IPhysicsController *spc = self->GetPhysicsController();
	MT_Scalar val = PyFloat_AsDouble(value);
	if (val < 0.0f) { /* also accounts for non float */
		PyErr_SetString(PyExc_AttributeError,
		                "gameOb.angularVelocityMin = float: KX_GameObject, expected a nonnegative float");
		return PY_SET_ATTR_FAIL;
	}

	if (spc)
		spc->SetAngularVelocityMin(val);

	return PY_SET_ATTR_SUCCESS;
}

PyObject *KX_GameObject::pyattr_get_ang_vel_max(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject *self = static_cast<KX_GameObject*>(self_v);
	PHY_IPhysicsController *spc = self->GetPhysicsController();
	return PyFloat_FromDouble(spc ? spc->GetAngularVelocityMax() : 0.0f);
}

int KX_GameObject::pyattr_set_ang_vel_max(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_GameObject *self = static_cast<KX_GameObject*>(self_v);
	PHY_IPhysicsController *spc = self->GetPhysicsController();
	MT_Scalar val = PyFloat_AsDouble(value);
	if (val < 0.0f) { /* also accounts for non float */
		PyErr_SetString(PyExc_AttributeError,
		                "gameOb.angularVelocityMax = float: KX_GameObject, expected a nonnegative float");
		return PY_SET_ATTR_FAIL;
	}

	if (spc)
		spc->SetAngularVelocityMax(val);

	return PY_SET_ATTR_SUCCESS;
}


PyObject *KX_GameObject::pyattr_get_visible(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	return PyBool_FromLong(self->GetVisible());
}

int KX_GameObject::pyattr_set_visible(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	int param = PyObject_IsTrue( value );
	if (param == -1) {
		PyErr_SetString(PyExc_AttributeError, "gameOb.visible = bool: KX_GameObject, expected True or False");
		return PY_SET_ATTR_FAIL;
	}

	self->SetVisible(param, false);
	self->UpdateBuckets(false);
	return PY_SET_ATTR_SUCCESS;
}

PyObject *KX_GameObject::pyattr_get_record_animation(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	return PyBool_FromLong(self->IsRecordAnimation());
}

int KX_GameObject::pyattr_set_record_animation(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	int param = PyObject_IsTrue(value);
	if (param == -1) {
		PyErr_SetString(PyExc_AttributeError, "gameOb.record_animation = bool: KX_GameObject, expected boolean");
		return PY_SET_ATTR_FAIL;
	}

	self->SetRecordAnimation(param);

	return PY_SET_ATTR_SUCCESS;
}



PyObject *KX_GameObject::pyattr_get_worldPosition(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
#ifdef USE_MATHUTILS
	return Vector_CreatePyObject_cb(
	        BGE_PROXY_FROM_REF_BORROW(self_v), 3,
	        mathutils_kxgameob_vector_cb_index, MATHUTILS_VEC_CB_POS_GLOBAL);
#else
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	return PyObjectFrom(self->NodeGetWorldPosition());
#endif
}

int KX_GameObject::pyattr_set_worldPosition(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	MT_Point3 pos;
	if (!PyVecTo(value, pos))
		return PY_SET_ATTR_FAIL;
	
	self->NodeSetWorldPosition(pos);
	self->NodeUpdateGS(0.f);
	return PY_SET_ATTR_SUCCESS;
}

PyObject *KX_GameObject::pyattr_get_localPosition(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
#ifdef USE_MATHUTILS
	return Vector_CreatePyObject_cb(
	        BGE_PROXY_FROM_REF_BORROW(self_v), 3,
	        mathutils_kxgameob_vector_cb_index, MATHUTILS_VEC_CB_POS_LOCAL);
#else
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	return PyObjectFrom(self->NodeGetLocalPosition());
#endif
}

int KX_GameObject::pyattr_set_localPosition(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	MT_Point3 pos;
	if (!PyVecTo(value, pos))
		return PY_SET_ATTR_FAIL;
	
	self->NodeSetLocalPosition(pos);
	self->NodeUpdateGS(0.f);
	return PY_SET_ATTR_SUCCESS;
}

PyObject *KX_GameObject::pyattr_get_localInertia(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
#ifdef USE_MATHUTILS
	return Vector_CreatePyObject_cb(
	        BGE_PROXY_FROM_REF_BORROW(self_v), 3,
	        mathutils_kxgameob_vector_cb_index, MATHUTILS_VEC_CB_INERTIA_LOCAL);
#else
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	if (self->GetPhysicsController1())
		return PyObjectFrom(self->GetPhysicsController1()->GetLocalInertia());
	return Py_BuildValue("fff", 0.0f, 0.0f, 0.0f);
#endif
}

PyObject *KX_GameObject::pyattr_get_worldOrientation(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
#ifdef USE_MATHUTILS
	return Matrix_CreatePyObject_cb(
	        BGE_PROXY_FROM_REF_BORROW(self_v), 3, 3,
	        mathutils_kxgameob_matrix_cb_index, MATHUTILS_MAT_CB_ORI_GLOBAL);
#else
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	return PyObjectFrom(self->NodeGetWorldOrientation());
#endif
}

int KX_GameObject::pyattr_set_worldOrientation(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	
	/* if value is not a sequence PyOrientationTo makes an error */
	MT_Matrix3x3 rot;
	if (!PyOrientationTo(value, rot, "gameOb.worldOrientation = sequence: KX_GameObject, "))
		return PY_SET_ATTR_FAIL;

	self->NodeSetGlobalOrientation(rot);
	
	self->NodeUpdateGS(0.f);
	return PY_SET_ATTR_SUCCESS;
}

PyObject *KX_GameObject::pyattr_get_localOrientation(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
#ifdef USE_MATHUTILS
	return Matrix_CreatePyObject_cb(
	        BGE_PROXY_FROM_REF_BORROW(self_v), 3, 3,
	        mathutils_kxgameob_matrix_cb_index, MATHUTILS_MAT_CB_ORI_LOCAL);
#else
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	return PyObjectFrom(self->NodeGetLocalOrientation());
#endif
}

int KX_GameObject::pyattr_set_localOrientation(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	
	/* if value is not a sequence PyOrientationTo makes an error */
	MT_Matrix3x3 rot;
	if (!PyOrientationTo(value, rot, "gameOb.localOrientation = sequence: KX_GameObject, "))
		return PY_SET_ATTR_FAIL;

	self->NodeSetLocalOrientation(rot);
	self->NodeUpdateGS(0.f);
	return PY_SET_ATTR_SUCCESS;
}

PyObject *KX_GameObject::pyattr_get_worldScaling(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
#ifdef USE_MATHUTILS
	return Vector_CreatePyObject_cb(
	        BGE_PROXY_FROM_REF_BORROW(self_v), 3,
	        mathutils_kxgameob_vector_cb_index, MATHUTILS_VEC_CB_SCALE_GLOBAL);
#else
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	return PyObjectFrom(self->NodeGetWorldScaling());
#endif
}

int KX_GameObject::pyattr_set_worldScaling(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	MT_Vector3 scale;
	if (!PyVecTo(value, scale))
		return PY_SET_ATTR_FAIL;

	self->NodeSetWorldScale(scale);
	self->NodeUpdateGS(0.f);
	return PY_SET_ATTR_SUCCESS;
}

PyObject *KX_GameObject::pyattr_get_localScaling(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
#ifdef USE_MATHUTILS
	return Vector_CreatePyObject_cb(
	        BGE_PROXY_FROM_REF_BORROW(self_v), 3,
	        mathutils_kxgameob_vector_cb_index, MATHUTILS_VEC_CB_SCALE_LOCAL);
#else
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	return PyObjectFrom(self->NodeGetLocalScaling());
#endif
}

int KX_GameObject::pyattr_set_localScaling(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	MT_Vector3 scale;
	if (!PyVecTo(value, scale))
		return PY_SET_ATTR_FAIL;

	self->NodeSetLocalScale(scale);
	self->NodeUpdateGS(0.f);
	return PY_SET_ATTR_SUCCESS;
}

PyObject *KX_GameObject::pyattr_get_localTransform(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);

	float mat[16];

	MT_Transform trans;
	
	trans.setOrigin(self->GetSGNode()->GetLocalPosition());
	trans.setBasis(self->GetSGNode()->GetLocalOrientation());
	
	MT_Vector3 scaling = self->GetSGNode()->GetLocalScale();
	trans.scale(scaling[0], scaling[1], scaling[2]);

	trans.getValue(mat);

	return PyObjectFrom(MT_Matrix4x4(mat));
}

int KX_GameObject::pyattr_set_localTransform(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	MT_Matrix4x4 temp;
	if (!PyMatTo(value, temp))
		return PY_SET_ATTR_FAIL;

	float transform[4][4];
	float loc[3], size[3];
	float rot[3][3];
	MT_Matrix3x3 orientation;

	temp.getValue(*transform);
	mat4_to_loc_rot_size(loc, rot, size, transform);

	self->NodeSetLocalPosition(MT_Point3(loc));

	//MT_Matrix3x3's constructor expects a 4x4 matrix
	orientation = MT_Matrix3x3();
	orientation.setValue3x3(*rot);
	self->NodeSetLocalOrientation(orientation);

	self->NodeSetLocalScale(MT_Vector3(size));

	return PY_SET_ATTR_SUCCESS;
}

PyObject *KX_GameObject::pyattr_get_worldTransform(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);

	return PyObjectFrom(MT_Matrix4x4(self->GetOpenGLMatrix()));
}

int KX_GameObject::pyattr_set_worldTransform(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	MT_Matrix4x4 temp;
	if (!PyMatTo(value, temp))
		return PY_SET_ATTR_FAIL;

	float transform[4][4];
	float loc[3], size[3];
	float rot[3][3];
	MT_Matrix3x3 orientation;

	temp.getValue(*transform);
	mat4_to_loc_rot_size(loc, rot, size, transform);

	self->NodeSetWorldPosition(MT_Point3(loc));

	//MT_Matrix3x3's constructor expects a 4x4 matrix
	orientation = MT_Matrix3x3();
	orientation.setValue3x3(*rot);
	self->NodeSetGlobalOrientation(orientation);

	self->NodeSetWorldScale(MT_Vector3(size));

	return PY_SET_ATTR_SUCCESS;
}

PyObject *KX_GameObject::pyattr_get_worldLinearVelocity(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
#ifdef USE_MATHUTILS
	return Vector_CreatePyObject_cb(
	        BGE_PROXY_FROM_REF_BORROW(self_v), 3,
	        mathutils_kxgameob_vector_cb_index, MATHUTILS_VEC_CB_LINVEL_GLOBAL);
#else
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	return PyObjectFrom(GetLinearVelocity(false));
#endif
}

int KX_GameObject::pyattr_set_worldLinearVelocity(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	MT_Vector3 velocity;
	if (!PyVecTo(value, velocity))
		return PY_SET_ATTR_FAIL;

	self->setLinearVelocity(velocity, false);

	return PY_SET_ATTR_SUCCESS;
}

PyObject *KX_GameObject::pyattr_get_localLinearVelocity(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
#ifdef USE_MATHUTILS
	return Vector_CreatePyObject_cb(
	        BGE_PROXY_FROM_REF_BORROW(self_v), 3,
	        mathutils_kxgameob_vector_cb_index, MATHUTILS_VEC_CB_LINVEL_LOCAL);
#else
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	return PyObjectFrom(GetLinearVelocity(true));
#endif
}

int KX_GameObject::pyattr_set_localLinearVelocity(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	MT_Vector3 velocity;
	if (!PyVecTo(value, velocity))
		return PY_SET_ATTR_FAIL;

	self->setLinearVelocity(velocity, true);

	return PY_SET_ATTR_SUCCESS;
}

PyObject *KX_GameObject::pyattr_get_worldAngularVelocity(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
#ifdef USE_MATHUTILS
	return Vector_CreatePyObject_cb(
	        BGE_PROXY_FROM_REF_BORROW(self_v), 3,
	        mathutils_kxgameob_vector_cb_index, MATHUTILS_VEC_CB_ANGVEL_GLOBAL);
#else
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	return PyObjectFrom(GetAngularVelocity(false));
#endif
}

int KX_GameObject::pyattr_set_worldAngularVelocity(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	MT_Vector3 velocity;
	if (!PyVecTo(value, velocity))
		return PY_SET_ATTR_FAIL;

	self->setAngularVelocity(velocity, false);

	return PY_SET_ATTR_SUCCESS;
}

PyObject *KX_GameObject::pyattr_get_localAngularVelocity(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
#ifdef USE_MATHUTILS
	return Vector_CreatePyObject_cb(
	        BGE_PROXY_FROM_REF_BORROW(self_v), 3,
	        mathutils_kxgameob_vector_cb_index, MATHUTILS_VEC_CB_ANGVEL_LOCAL);
#else
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	return PyObjectFrom(GetAngularVelocity(true));
#endif
}

int KX_GameObject::pyattr_set_localAngularVelocity(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	MT_Vector3 velocity;
	if (!PyVecTo(value, velocity))
		return PY_SET_ATTR_FAIL;

	self->setAngularVelocity(velocity, true);

	return PY_SET_ATTR_SUCCESS;
}

PyObject *KX_GameObject::pyattr_get_linearDamping(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	return PyFloat_FromDouble(self->getLinearDamping());
}

int KX_GameObject::pyattr_set_linearDamping(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	float val = PyFloat_AsDouble(value);
	self->setLinearDamping(val);
	return PY_SET_ATTR_SUCCESS;
}

PyObject *KX_GameObject::pyattr_get_angularDamping(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	return PyFloat_FromDouble(self->getAngularDamping());
}

int KX_GameObject::pyattr_set_angularDamping(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	float val = PyFloat_AsDouble(value);
	self->setAngularDamping(val);
	return PY_SET_ATTR_SUCCESS;
}


PyObject *KX_GameObject::pyattr_get_timeOffset(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	SG_Node* sg_parent;
	if (self->GetSGNode() && (sg_parent = self->GetSGNode()->GetSGParent()) != NULL && sg_parent->IsSlowParent()) {
		return PyFloat_FromDouble(static_cast<KX_SlowParentRelation *>(sg_parent->GetParentRelation())->GetTimeOffset());
	} else {
		return PyFloat_FromDouble(0.0f);
	}
}

int KX_GameObject::pyattr_set_timeOffset(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	if (self->GetSGNode()) {
		MT_Scalar val = PyFloat_AsDouble(value);
		SG_Node *sg_parent= self->GetSGNode()->GetSGParent();
		if (val < 0.0f) { /* also accounts for non float */
			PyErr_SetString(PyExc_AttributeError, "gameOb.timeOffset = float: KX_GameObject, expected a float zero or above");
			return PY_SET_ATTR_FAIL;
		}
		if (sg_parent && sg_parent->IsSlowParent())
			static_cast<KX_SlowParentRelation *>(sg_parent->GetParentRelation())->SetTimeOffset(val);
	}
	return PY_SET_ATTR_SUCCESS;
}

PyObject *KX_GameObject::pyattr_get_state(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	int state = 0;
	state |= self->GetState();
	return PyLong_FromLong(state);
}

int KX_GameObject::pyattr_set_state(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	int state_i = PyLong_AsLong(value);
	unsigned int state = 0;
	
	if (state_i == -1 && PyErr_Occurred()) {
		PyErr_SetString(PyExc_TypeError, "gameOb.state = int: KX_GameObject, expected an int bit field");
		return PY_SET_ATTR_FAIL;
	}
	
	state |= state_i;
	if ((state & ((1<<30)-1)) == 0) {
		PyErr_SetString(PyExc_AttributeError, "gameOb.state = int: KX_GameObject, state bitfield was not between 0 and 30 (1<<0 and 1<<29)");
		return PY_SET_ATTR_FAIL;
	}
	self->SetState(state);
	return PY_SET_ATTR_SUCCESS;
}

PyObject *KX_GameObject::pyattr_get_meshes(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	PyObject *meshes= PyList_New(self->m_meshes.size());
	int i;
	
	for (i=0; i < (int)self->m_meshes.size(); i++)
	{
		KX_MeshProxy* meshproxy = new KX_MeshProxy(self->m_meshes[i]);
		PyList_SET_ITEM(meshes, i, meshproxy->NewProxy(true));
	}
	
	return meshes;
}

PyObject *KX_GameObject::pyattr_get_obcolor(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
#ifdef USE_MATHUTILS
	return Vector_CreatePyObject_cb(
	        BGE_PROXY_FROM_REF_BORROW(self_v), 4,
	        mathutils_kxgameob_vector_cb_index, MATHUTILS_VEC_CB_OBJECT_COLOR);
#else
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	return PyObjectFrom(self->GetObjectColor());
#endif
}

int KX_GameObject::pyattr_set_obcolor(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	MT_Vector4 obcolor;
	if (!PyVecTo(value, obcolor))
		return PY_SET_ATTR_FAIL;

	self->SetObjectColor(obcolor);
	return PY_SET_ATTR_SUCCESS;
}

static int kx_game_object_get_sensors_size_cb(void *self_v)
{
	return ((KX_GameObject *)self_v)->GetSensors().size();
}

static PyObject *kx_game_object_get_sensors_item_cb(void *self_v, int index)
{
	return ((KX_GameObject *)self_v)->GetSensors()[index]->GetProxy();
}

static const char *kx_game_object_get_sensors_item_name_cb(void *self_v, int index)
{
	return ((KX_GameObject *)self_v)->GetSensors()[index]->GetName().ReadPtr();
}

/* These are experimental! */
PyObject *KX_GameObject::pyattr_get_sensors(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	return (new CListWrapper(self_v,
							 ((KX_GameObject *)self_v)->GetProxy(),
							 NULL,
							 kx_game_object_get_sensors_size_cb,
							 kx_game_object_get_sensors_item_cb,
							 kx_game_object_get_sensors_item_name_cb,
							 NULL))->NewProxy(true);
}

static int kx_game_object_get_controllers_size_cb(void *self_v)
{
	return ((KX_GameObject *)self_v)->GetControllers().size();
}

static PyObject *kx_game_object_get_controllers_item_cb(void *self_v, int index)
{
	return ((KX_GameObject *)self_v)->GetControllers()[index]->GetProxy();
}

static const char *kx_game_object_get_controllers_item_name_cb(void *self_v, int index)
{
	return ((KX_GameObject *)self_v)->GetControllers()[index]->GetName().ReadPtr();
}

PyObject *KX_GameObject::pyattr_get_controllers(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	return (new CListWrapper(self_v,
							 ((KX_GameObject *)self_v)->GetProxy(),
							 NULL,
							 kx_game_object_get_controllers_size_cb,
							 kx_game_object_get_controllers_item_cb,
							 kx_game_object_get_controllers_item_name_cb,
							 NULL))->NewProxy(true);
}

static int kx_game_object_get_actuators_size_cb(void *self_v)
{
	return ((KX_GameObject *)self_v)->GetActuators().size();
}

static PyObject *kx_game_object_get_actuators_item_cb(void *self_v, int index)
{
	return ((KX_GameObject *)self_v)->GetActuators()[index]->GetProxy();
}

static const char *kx_game_object_get_actuators_item_name_cb(void *self_v, int index)
{
	return ((KX_GameObject *)self_v)->GetActuators()[index]->GetName().ReadPtr();
}

PyObject *KX_GameObject::pyattr_get_actuators(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	return (new CListWrapper(self_v,
							 ((KX_GameObject *)self_v)->GetProxy(),
							 NULL,
							 kx_game_object_get_actuators_size_cb,
							 kx_game_object_get_actuators_item_cb,
							 kx_game_object_get_actuators_item_name_cb,
							 NULL))->NewProxy(true);
}
/* End experimental */

PyObject *KX_GameObject::pyattr_get_children(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	return self->GetChildren()->NewProxy(true);
}

PyObject *KX_GameObject::pyattr_get_children_recursive(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	return self->GetChildrenRecursive()->NewProxy(true);
}

PyObject *KX_GameObject::pyattr_get_attrDict(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self = static_cast<KX_GameObject*>(self_v);
	
	if (self->m_attr_dict==NULL)
		self->m_attr_dict= PyDict_New();
	
	Py_INCREF(self->m_attr_dict);
	return self->m_attr_dict;
}

PyObject *KX_GameObject::pyattr_get_debug(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_Scene *scene = KX_GetActiveScene();
	KX_GameObject *self = static_cast<KX_GameObject*>(self_v);

	return PyBool_FromLong(scene->ObjectInDebugList(self));
}

int KX_GameObject::pyattr_set_debug(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_GameObject *self = static_cast<KX_GameObject*>(self_v);
	int param = PyObject_IsTrue(value);

	if (param == -1) {
		PyErr_SetString(PyExc_AttributeError, "gameOb.debug = bool: KX_GameObject, expected True or False");
		return PY_SET_ATTR_FAIL;
	}

	self->SetUseDebugProperties(param, false);

	return PY_SET_ATTR_SUCCESS;
}

PyObject *KX_GameObject::pyattr_get_debugRecursive(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_Scene *scene = KX_GetActiveScene();
	KX_GameObject *self = static_cast<KX_GameObject*>(self_v);

	return PyBool_FromLong(scene->ObjectInDebugList(self));
}

int KX_GameObject::pyattr_set_debugRecursive(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_GameObject *self = static_cast<KX_GameObject*>(self_v);
	int param = PyObject_IsTrue(value);

	if (param == -1) {
		PyErr_SetString(PyExc_AttributeError, "gameOb.debugRecursive = bool: KX_GameObject, expected True or False");
		return PY_SET_ATTR_FAIL;
	}

	self->SetUseDebugProperties(param, true);

	return PY_SET_ATTR_SUCCESS;
}

PyObject *KX_GameObject::PyApplyForce(PyObject *args)
{
	int local = 0;
	PyObject *pyvect;

	if (PyArg_ParseTuple(args, "O|i:applyForce", &pyvect, &local)) {
		MT_Vector3 force;
		if (PyVecTo(pyvect, force)) {
			ApplyForce(force, (local!=0));
			Py_RETURN_NONE;
		}
	}
	return NULL;
}

PyObject *KX_GameObject::PyApplyTorque(PyObject *args)
{
	int local = 0;
	PyObject *pyvect;

	if (PyArg_ParseTuple(args, "O|i:applyTorque", &pyvect, &local)) {
		MT_Vector3 torque;
		if (PyVecTo(pyvect, torque)) {
			ApplyTorque(torque, (local!=0));
			Py_RETURN_NONE;
		}
	}
	return NULL;
}

PyObject *KX_GameObject::PyApplyRotation(PyObject *args)
{
	int local = 0;
	PyObject *pyvect;

	if (PyArg_ParseTuple(args, "O|i:applyRotation", &pyvect, &local)) {
		MT_Vector3 rotation;
		if (PyVecTo(pyvect, rotation)) {
			ApplyRotation(rotation, (local!=0));
			Py_RETURN_NONE;
		}
	}
	return NULL;
}

PyObject *KX_GameObject::PyApplyMovement(PyObject *args)
{
	int local = 0;
	PyObject *pyvect;

	if (PyArg_ParseTuple(args, "O|i:applyMovement", &pyvect, &local)) {
		MT_Vector3 movement;
		if (PyVecTo(pyvect, movement)) {
			ApplyMovement(movement, (local!=0));
			Py_RETURN_NONE;
		}
	}
	return NULL;
}

PyObject *KX_GameObject::PyGetLinearVelocity(PyObject *args)
{
	// only can get the velocity if we have a physics object connected to us...
	int local = 0;
	if (PyArg_ParseTuple(args,"|i:getLinearVelocity",&local))
	{
		return PyObjectFrom(GetLinearVelocity((local!=0)));
	}
	else
	{
		return NULL;
	}
}

PyObject *KX_GameObject::PySetLinearVelocity(PyObject *args)
{
	int local = 0;
	PyObject *pyvect;
	
	if (PyArg_ParseTuple(args,"O|i:setLinearVelocity",&pyvect,&local)) {
		MT_Vector3 velocity;
		if (PyVecTo(pyvect, velocity)) {
			setLinearVelocity(velocity, (local!=0));
			Py_RETURN_NONE;
		}
	}
	return NULL;
}

PyObject *KX_GameObject::PyGetAngularVelocity(PyObject *args)
{
	// only can get the velocity if we have a physics object connected to us...
	int local = 0;
	if (PyArg_ParseTuple(args,"|i:getAngularVelocity",&local))
	{
		return PyObjectFrom(GetAngularVelocity((local!=0)));
	}
	else
	{
		return NULL;
	}
}

PyObject *KX_GameObject::PySetAngularVelocity(PyObject *args)
{
	int local = 0;
	PyObject *pyvect;
	
	if (PyArg_ParseTuple(args,"O|i:setAngularVelocity",&pyvect,&local)) {
		MT_Vector3 velocity;
		if (PyVecTo(pyvect, velocity)) {
			setAngularVelocity(velocity, (local!=0));
			Py_RETURN_NONE;
		}
	}
	return NULL;
}

PyObject *KX_GameObject::PySetDamping(PyObject *args)
{
	float linear;
	float angular;

	if (!PyArg_ParseTuple(args,"ff|i:setDamping", &linear, &angular))
		return NULL;

	setDamping(linear, angular);
	Py_RETURN_NONE;
}

PyObject *KX_GameObject::PySetVisible(PyObject *args)
{
	int visible, recursive = 0;
	if (!PyArg_ParseTuple(args,"i|i:setVisible",&visible, &recursive))
		return NULL;
	
	SetVisible(visible ? true:false, recursive ? true:false);
	UpdateBuckets(recursive ? true:false);
	Py_RETURN_NONE;
	
}

PyObject *KX_GameObject::PySetOcclusion(PyObject *args)
{
	int occlusion, recursive = 0;
	if (!PyArg_ParseTuple(args,"i|i:setOcclusion",&occlusion, &recursive))
		return NULL;
	
	SetOccluder(occlusion ? true:false, recursive ? true:false);
	Py_RETURN_NONE;
}

PyObject *KX_GameObject::PyGetVelocity(PyObject *args)
{
	// only can get the velocity if we have a physics object connected to us...
	MT_Point3 point(0.0f,0.0f,0.0f);
	PyObject *pypos = NULL;
	
	if (!PyArg_ParseTuple(args, "|O:getVelocity", &pypos) || (pypos && !PyVecTo(pypos, point)))
		return NULL;

	return PyObjectFrom(GetVelocity(point));
}

PyObject *KX_GameObject::PyGetReactionForce()
{
	// only can get the velocity if we have a physics object connected to us...
	
	// XXX - Currently not working with bullet intergration, see KX_BulletPhysicsController.cpp's getReactionForce
#if 0
	if (GetPhysicsController1())
		return PyObjectFrom(GetPhysicsController1()->getReactionForce());
	return PyObjectFrom(dummy_point);
#endif
	
	return Py_BuildValue("fff", 0.0f, 0.0f, 0.0f);
	
}



PyObject *KX_GameObject::PyEnableRigidBody()
{
	if (GetPhysicsController())
		GetPhysicsController()->SetRigidBody(true);

	Py_RETURN_NONE;
}



PyObject *KX_GameObject::PyDisableRigidBody()
{
	if (GetPhysicsController())
		GetPhysicsController()->SetRigidBody(false);

	Py_RETURN_NONE;
}


PyObject *KX_GameObject::PySetParent(PyObject *args)
{
	KX_Scene *scene = KX_GetActiveScene();
	SCA_LogicManager *logicmgr = GetScene()->GetLogicManager();
	PyObject *pyobj;
	KX_GameObject *obj;
	int addToCompound=1, ghost=1;
	
	if (!PyArg_ParseTuple(args,"O|ii:setParent", &pyobj, &addToCompound, &ghost)) {
		return NULL; // Python sets a simple error
	}
	if (!ConvertPythonToGameObject(logicmgr, pyobj, &obj, true, "gameOb.setParent(obj): KX_GameObject"))
		return NULL;
	if (obj)
		this->SetParent(scene, obj, addToCompound, ghost);
	Py_RETURN_NONE;
}

PyObject *KX_GameObject::PyRemoveParent()
{
	KX_Scene *scene = KX_GetActiveScene();
	
	this->RemoveParent(scene);
	Py_RETURN_NONE;
}


PyObject *KX_GameObject::PySetCollisionMargin(PyObject *value)
{
	float collisionMargin = PyFloat_AsDouble(value);
	
	if (collisionMargin==-1 && PyErr_Occurred()) {
		PyErr_SetString(PyExc_TypeError, "expected a float");
		return NULL;
	}
	
	if (m_pPhysicsController)
	{
		m_pPhysicsController->SetMargin(collisionMargin);
		Py_RETURN_NONE;
	}
	PyErr_SetString(PyExc_RuntimeError, "This object has no physics controller");
	return NULL;
}



PyObject *KX_GameObject::PyApplyImpulse(PyObject *args)
{
	PyObject *pyattach;
	PyObject *pyimpulse;
	int local = 0;
	
	if (!m_pPhysicsController)	{
		PyErr_SetString(PyExc_RuntimeError, "This object has no physics controller");
		return NULL;
	}
	
	if (PyArg_ParseTuple(args, "OO|i:applyImpulse", &pyattach, &pyimpulse, &local))
	{
		MT_Point3  attach;
		MT_Vector3 impulse;
		if (PyVecTo(pyattach, attach) && PyVecTo(pyimpulse, impulse))
		{
			m_pPhysicsController->ApplyImpulse(attach, impulse, (local!=0));
			Py_RETURN_NONE;
		}

	}
	
	return NULL;
}



PyObject *KX_GameObject::PySuspendDynamics(PyObject *args)
{
	bool ghost = false;

	if (!PyArg_ParseTuple(args, "|b", &ghost))
		return NULL;

	if (GetPhysicsController())
		GetPhysicsController()->SuspendDynamics(ghost);

	Py_RETURN_NONE;
}



PyObject *KX_GameObject::PyRestoreDynamics()
{
	// Child objects must be static, so we block changing to dynamic
	if (GetPhysicsController() && !GetParent())
		GetPhysicsController()->RestoreDynamics();
	Py_RETURN_NONE;
}


PyObject *KX_GameObject::PyAlignAxisToVect(PyObject *args)
{
	PyObject *pyvect;
	int axis = 2; //z axis is the default
	float fac = 1.0f;
	
	if (PyArg_ParseTuple(args,"O|if:alignAxisToVect",&pyvect,&axis, &fac))
	{
		MT_Vector3 vect;
		if (PyVecTo(pyvect, vect)) {
			if (fac > 0.0f) {
				if (fac> 1.0f) fac = 1.0f;

				AlignAxisToVect(vect, axis, fac);
				NodeUpdateGS(0.f);
			}
			Py_RETURN_NONE;
		}
	}
	return NULL;
}

PyObject *KX_GameObject::PyGetAxisVect(PyObject *value)
{
	MT_Vector3 vect;
	if (PyVecTo(value, vect))
	{
		return PyObjectFrom(NodeGetWorldOrientation() * vect);
	}
	return NULL;
}


PyObject *KX_GameObject::PyGetPhysicsId()
{
	PHY_IPhysicsController* ctrl = GetPhysicsController();
	unsigned long long physid = 0;
	if (ctrl)
	{
		physid = (unsigned long long)ctrl;
	}
	return PyLong_FromUnsignedLongLong(physid);
}

PyObject *KX_GameObject::PyGetPropertyNames()
{
	PyObject *list= ConvertKeysToPython();
	
	if (m_attr_dict) {
		PyObject *key, *value;
		Py_ssize_t pos = 0;

		while (PyDict_Next(m_attr_dict, &pos, &key, &value)) {
			PyList_Append(list, key);
		}
	}
	return list;
}

KX_PYMETHODDEF_DOC_O(KX_GameObject, getDistanceTo,
"getDistanceTo(other): get distance to another point/KX_GameObject")
{
	MT_Point3 b;
	if (PyVecTo(value, b))
	{
		return PyFloat_FromDouble(NodeGetWorldPosition().distance(b));
	}
	PyErr_Clear();

	SCA_LogicManager *logicmgr = GetScene()->GetLogicManager();
	KX_GameObject *other;
	if (ConvertPythonToGameObject(logicmgr, value, &other, false, "gameOb.getDistanceTo(value): KX_GameObject"))
	{
		return PyFloat_FromDouble(NodeGetWorldPosition().distance(other->NodeGetWorldPosition()));
	}
	
	return NULL;
}

KX_PYMETHODDEF_DOC_O(KX_GameObject, getVectTo,
"getVectTo(other): get vector and the distance to another point/KX_GameObject\n"
"Returns a 3-tuple with (distance,worldVector,localVector)\n")
{
	MT_Point3 toPoint, fromPoint;
	MT_Vector3 toDir, locToDir;
	MT_Scalar distance;

	SCA_LogicManager *logicmgr = GetScene()->GetLogicManager();
	PyObject *returnValue;

	if (!PyVecTo(value, toPoint))
	{
		PyErr_Clear();
		
		KX_GameObject *other;
		if (ConvertPythonToGameObject(logicmgr, value, &other, false, "")) /* error will be overwritten */
		{
			toPoint = other->NodeGetWorldPosition();
		} else
		{
			PyErr_SetString(PyExc_TypeError, "gameOb.getVectTo(other): KX_GameObject, expected a 3D Vector or KX_GameObject type");
			return NULL;
		}
	}

	fromPoint = NodeGetWorldPosition();
	toDir = toPoint-fromPoint;
	distance = toDir.length();

	if (MT_fuzzyZero(distance))
	{
		//cout << "getVectTo() Error: Null vector!\n";
		locToDir = toDir = MT_Vector3(0.0f,0.0f,0.0f);
		distance = 0.0f;
	} else {
		toDir.normalize();
		locToDir = toDir * NodeGetWorldOrientation();
	}
	
	returnValue = PyTuple_New(3);
	if (returnValue) { // very unlikely to fail, python sets a memory error here.
		PyTuple_SET_ITEM(returnValue, 0, PyFloat_FromDouble(distance));
		PyTuple_SET_ITEM(returnValue, 1, PyObjectFrom(toDir));
		PyTuple_SET_ITEM(returnValue, 2, PyObjectFrom(locToDir));
	}
	return returnValue;
}

struct KX_GameObject::RayCastData
{
	RayCastData(STR_String prop, bool xray, short mask)
		:m_prop(prop),
		m_xray(xray),
		m_mask(mask),
		m_hitObject(NULL)
	{
	}

	STR_String m_prop;
	bool m_xray;
	unsigned short m_mask;
	KX_GameObject *m_hitObject;
};

bool KX_GameObject::RayHit(KX_ClientObjectInfo *client, KX_RayCast *result, RayCastData *rayData)
{
	KX_GameObject* hitKXObj = client->m_gameobject;

	// if X-ray option is selected, the unwnted objects were not tested, so get here only with true hit
	// if not, all objects were tested and the front one may not be the correct one.
	if ((rayData->m_xray || rayData->m_prop.Length() == 0 || hitKXObj->GetProperty(rayData->m_prop) != NULL) && 
		hitKXObj->GetUserCollisionGroup() & rayData->m_mask)
	{
		rayData->m_hitObject = hitKXObj;
		return true;
	}
	// return true to stop RayCast::RayTest from looping, the above test was decisive
	// We would want to loop only if we want to get more than one hit point
	return true;
}

/* this function is used to pre-filter the object before casting the ray on them.
 * This is useful for "X-Ray" option when we want to see "through" unwanted object.
 */
bool KX_GameObject::NeedRayCast(KX_ClientObjectInfo *client, RayCastData *rayData)
{
	KX_GameObject* hitKXObj = client->m_gameobject;

	if (client->m_type > KX_ClientObjectInfo::ACTOR)
	{
		// Unknown type of object, skip it.
		// Should not occur as the sensor objects are filtered in RayTest()
		printf("Invalid client type %d found in ray casting\n", client->m_type);
		return false;
	}
	
	// if X-Ray option is selected, skip object that don't match the criteria as we see through them
	// if not, test all objects because we don't know yet which one will be on front
	if ((!rayData->m_xray || rayData->m_prop.Length() == 0 || hitKXObj->GetProperty(rayData->m_prop) != NULL) && 
		hitKXObj->GetUserCollisionGroup() & rayData->m_mask)
	{
		return true;
	}
	// skip the object
	return false;
}

KX_PYMETHODDEF_DOC(KX_GameObject, rayCastTo,
"rayCastTo(other,dist,prop): look towards another point/KX_GameObject and return first object hit within dist that matches prop\n"
" prop = property name that object must have; can be omitted => detect any object\n"
" dist = max distance to look (can be negative => look behind); 0 or omitted => detect up to other\n"
" other = 3-tuple or object reference")
{
	MT_Point3 toPoint;
	PyObject *pyarg;
	float dist = 0.0f;
	char *propName = NULL;
	SCA_LogicManager *logicmgr = GetScene()->GetLogicManager();

	if (!PyArg_ParseTuple(args,"O|fs:rayCastTo", &pyarg, &dist, &propName)) {
		return NULL; // python sets simple error
	}

	if (!PyVecTo(pyarg, toPoint))
	{
		KX_GameObject *other;
		PyErr_Clear();
		
		if (ConvertPythonToGameObject(logicmgr, pyarg, &other, false, "")) /* error will be overwritten */
		{
			toPoint = other->NodeGetWorldPosition();
		} else
		{
			PyErr_SetString(PyExc_TypeError, "gameOb.rayCastTo(other,dist,prop): KX_GameObject, the first argument to rayCastTo must be a vector or a KX_GameObject");
			return NULL;
		}
	}
	MT_Point3 fromPoint = NodeGetWorldPosition();
	
	if (dist != 0.0f)
		toPoint = fromPoint + dist * (toPoint-fromPoint).safe_normalized();
	
	PHY_IPhysicsEnvironment* pe = GetScene()->GetPhysicsEnvironment();
	PHY_IPhysicsController *spc = GetPhysicsController();
	KX_GameObject *parent = GetParent();
	if (!spc && parent)
		spc = parent->GetPhysicsController();

	RayCastData rayData(propName, false, (1u << OB_MAX_COL_MASKS) - 1);
	KX_RayCast::Callback<KX_GameObject, RayCastData> callback(this, spc, &rayData);
	if (KX_RayCast::RayTest(pe, fromPoint, toPoint, callback) && rayData.m_hitObject) {
		return rayData.m_hitObject->GetProxy();
	}
	
	Py_RETURN_NONE;
}

/* faster then Py_BuildValue since some scripts call raycast a lot */
static PyObject *none_tuple_3()
{
	PyObject *ret= PyTuple_New(3);
	PyTuple_SET_ITEM(ret, 0, Py_None);
	PyTuple_SET_ITEM(ret, 1, Py_None);
	PyTuple_SET_ITEM(ret, 2, Py_None);
	
	Py_INCREF(Py_None);
	Py_INCREF(Py_None);
	Py_INCREF(Py_None);
	return ret;
}
static PyObject *none_tuple_4()
{
	PyObject *ret= PyTuple_New(4);
	PyTuple_SET_ITEM(ret, 0, Py_None);
	PyTuple_SET_ITEM(ret, 1, Py_None);
	PyTuple_SET_ITEM(ret, 2, Py_None);
	PyTuple_SET_ITEM(ret, 3, Py_None);
	
	Py_INCREF(Py_None);
	Py_INCREF(Py_None);
	Py_INCREF(Py_None);
	Py_INCREF(Py_None);
	return ret;
}

static PyObject *none_tuple_5()
{
	PyObject *ret= PyTuple_New(5);
	PyTuple_SET_ITEM(ret, 0, Py_None);
	PyTuple_SET_ITEM(ret, 1, Py_None);
	PyTuple_SET_ITEM(ret, 2, Py_None);
	PyTuple_SET_ITEM(ret, 3, Py_None);
	PyTuple_SET_ITEM(ret, 4, Py_None);
	
	Py_INCREF(Py_None);
	Py_INCREF(Py_None);
	Py_INCREF(Py_None);
	Py_INCREF(Py_None);
	Py_INCREF(Py_None);
	return ret;
}

KX_PYMETHODDEF_DOC(KX_GameObject, rayCast,
				   "rayCast(to,from,dist,prop,face,xray,poly,mask): cast a ray and return 3-tuple (object,hit,normal) or 4-tuple (object,hit,normal,polygon) or 4-tuple (object,hit,normal,polygon,hituv) of contact point with object within dist that matches prop.\n"
				   " If no hit, return (None,None,None) or (None,None,None,None) or (None,None,None,None,None).\n"
" to   = 3-tuple or object reference for destination of ray (if object, use center of object)\n"
" from = 3-tuple or object reference for origin of ray (if object, use center of object)\n"
"        Can be None or omitted => start from self object center\n"
" dist = max distance to look (can be negative => look behind); 0 or omitted => detect up to to\n"
" prop = property name that object must have; can be omitted => detect any object\n"
" face = normal option: 1=>return face normal; 0 or omitted => normal is oriented towards origin\n"
" xray = X-ray option: 1=>skip objects that don't match prop; 0 or omitted => stop on first object\n"
" poly = polygon option: 1=>return value is a 4-tuple and the 4th element is a KX_PolyProxy object\n"
"                           which can be None if hit object has no mesh or if there is no hit\n"
"                        2=>return value is a 5-tuple, the 4th element is the KX_PolyProxy object\n"
"                           and the 5th element is the vector of UV coordinates at the hit point of the None if there is no UV mapping\n"
"        If 0 or omitted, return value is a 3-tuple\n"
" mask = collision mask: the collision mask that ray can hit, 0 < mask < 65536\n"
"Note: The object on which you call this method matters: the ray will ignore it.\n"
"      prop and xray option interact as follow:\n"
"        prop off, xray off: return closest hit or no hit if there is no object on the full extend of the ray\n"
"        prop off, xray on : idem\n"
"        prop on,  xray off: return closest hit if it matches prop, no hit otherwise\n"
"        prop on,  xray on : return closest hit matching prop or no hit if there is no object matching prop on the full extend of the ray\n")
{
	MT_Point3 toPoint;
	MT_Point3 fromPoint;
	PyObject *pyto;
	PyObject *pyfrom = NULL;
	float dist = 0.0f;
	char *propName = NULL;
	KX_GameObject *other;
	int face=0, xray=0, poly=0;
	int mask = (1 << OB_MAX_COL_MASKS) - 1;
	SCA_LogicManager *logicmgr = GetScene()->GetLogicManager();

	if (!PyArg_ParseTuple(args,"O|Ofsiiii:rayCast", &pyto, &pyfrom, &dist, &propName, &face, &xray, &poly, &mask)) {
		return NULL; // Python sets a simple error
	}

	if (!PyVecTo(pyto, toPoint))
	{
		PyErr_Clear();
		
		if (ConvertPythonToGameObject(logicmgr, pyto, &other, false, ""))  /* error will be overwritten */
		{
			toPoint = other->NodeGetWorldPosition();
		} else
		{
			PyErr_SetString(PyExc_TypeError, "the first argument to rayCast must be a vector or a KX_GameObject");
			return NULL;
		}
	}
	if (!pyfrom || pyfrom == Py_None)
	{
		fromPoint = NodeGetWorldPosition();
	}
	else if (!PyVecTo(pyfrom, fromPoint))
	{
		PyErr_Clear();
		
		if (ConvertPythonToGameObject(logicmgr, pyfrom, &other, false, "")) /* error will be overwritten */
		{
			fromPoint = other->NodeGetWorldPosition();
		} else
		{
			PyErr_SetString(PyExc_TypeError, "gameOb.rayCast(to,from,dist,prop,face,xray,poly,mask): KX_GameObject, the second optional argument to rayCast must be a vector or a KX_GameObject");
			return NULL;
		}
	}

	if (mask == 0 || mask & ~((1 << OB_MAX_COL_MASKS) - 1)) {
		PyErr_Format(PyExc_TypeError, "gameOb.rayCast(to,from,dist,prop,face,xray,poly,mask): KX_GameObject, mask argument to rayCast must be a int bitfield, 0 < mask < %i", (1 << OB_MAX_COL_MASKS));
		return NULL;
	}

	if (dist != 0.0f) {
		MT_Vector3 toDir = toPoint-fromPoint;
		if (MT_fuzzyZero(toDir.length2())) {
			//return Py_BuildValue("OOO", Py_None, Py_None, Py_None);
			return none_tuple_3();
		}
		toDir.normalize();
		toPoint = fromPoint + (dist) * toDir;
	} else if (MT_fuzzyZero((toPoint-fromPoint).length2())) {
		//return Py_BuildValue("OOO", Py_None, Py_None, Py_None);
		return none_tuple_3();
	}
	
	PHY_IPhysicsEnvironment* pe = GetScene()->GetPhysicsEnvironment();
	PHY_IPhysicsController *spc = GetPhysicsController();
	KX_GameObject *parent = GetParent();
	if (!spc && parent)
		spc = parent->GetPhysicsController();

	// to get the hit results
	RayCastData rayData(propName, xray, mask);
	KX_RayCast::Callback<KX_GameObject, RayCastData> callback(this, spc, &rayData, face, (poly == 2));

	if (KX_RayCast::RayTest(pe, fromPoint, toPoint, callback) && rayData.m_hitObject) {
		PyObject *returnValue = (poly == 2) ? PyTuple_New(5) : (poly) ? PyTuple_New(4) : PyTuple_New(3);
		if (returnValue) { // unlikely this would ever fail, if it does python sets an error
			PyTuple_SET_ITEM(returnValue, 0, rayData.m_hitObject->GetProxy());
			PyTuple_SET_ITEM(returnValue, 1, PyObjectFrom(callback.m_hitPoint));
			PyTuple_SET_ITEM(returnValue, 2, PyObjectFrom(callback.m_hitNormal));
			if (poly)
			{
				if (callback.m_hitMesh)
				{
					// if this field is set, then we can trust that m_hitPolygon is a valid polygon
					RAS_Polygon* polygon = callback.m_hitMesh->GetPolygon(callback.m_hitPolygon);
					KX_PolyProxy* polyproxy = new KX_PolyProxy(callback.m_hitMesh, polygon);
					PyTuple_SET_ITEM(returnValue, 3, polyproxy->NewProxy(true));
					if (poly == 2)
					{
						if (callback.m_hitUVOK)
							PyTuple_SET_ITEM(returnValue, 4, PyObjectFrom(callback.m_hitUV));
						else {
							Py_INCREF(Py_None);
							PyTuple_SET_ITEM(returnValue, 4, Py_None);
						}
					}
				}
				else
				{
					Py_INCREF(Py_None);
					PyTuple_SET_ITEM(returnValue, 3, Py_None);
					if (poly==2)
					{
						Py_INCREF(Py_None);
						PyTuple_SET_ITEM(returnValue, 4, Py_None);
					}
				}
			}
		}
		return returnValue;
	}
	// no hit
	if (poly == 2)
		return none_tuple_5();
	else if (poly)
		return none_tuple_4();
	else
		return none_tuple_3();
}

KX_PYMETHODDEF_DOC_VARARGS(KX_GameObject, sendMessage, 
						   "sendMessage(subject, [body, to])\n"
"sends a message in same manner as a message actuator"
"subject = Subject of the message (string)"
"body = Message body (string)"
"to = Name of object to send the message to")
{
	KX_Scene *scene = KX_GetActiveScene();
	char* subject;
	char* body = (char *)"";
	char* to = (char *)"";
	const STR_String& from = GetName();

	if (!PyArg_ParseTuple(args, "s|ss:sendMessage", &subject, &body, &to))
		return NULL;
	
	scene->GetNetworkScene()->SendMessage(to, from, subject, body);
	Py_RETURN_NONE;
}

static void layer_check(short &layer, const char *method_name)
{
	if (layer < 0 || layer >= MAX_ACTION_LAYERS)
	{
		printf("KX_GameObject.%s(): given layer (%d) is out of range (0 - %d), setting to 0.\n", method_name, layer, MAX_ACTION_LAYERS-1);
		layer = 0;
	}
}

KX_PYMETHODDEF_DOC(KX_GameObject, playAction,
	"playAction(name, start_frame, end_frame, layer=0, priority=0 blendin=0, play_mode=ACT_MODE_PLAY, layer_weight=0.0, ipo_flags=0, speed=1.0)\n"
	"Plays an action\n")
{
	const char* name;
	float start, end, blendin=0.f, speed=1.f, layer_weight=0.f;
	short layer=0, priority=0;
	short ipo_flags=0;
	short play_mode=0;
	short blend_mode=0;

	static const char *kwlist[] = {"name", "start_frame", "end_frame", "layer", "priority", "blendin", "play_mode", "layer_weight", "ipo_flags", "speed", "blend_mode", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "sff|hhfhfhfh:playAction", const_cast<char**>(kwlist),
									&name, &start, &end, &layer, &priority, &blendin, &play_mode, &layer_weight, &ipo_flags, &speed, &blend_mode))
		return NULL;

	layer_check(layer, "playAction");

	if (play_mode < 0 || play_mode > BL_Action::ACT_MODE_MAX)
	{
		printf("KX_GameObject.playAction(): given play_mode (%d) is out of range (0 - %d), setting to ACT_MODE_PLAY", play_mode, BL_Action::ACT_MODE_MAX-1);
		play_mode = BL_Action::ACT_MODE_PLAY;
	}

	if (blend_mode < 0 || blend_mode > BL_Action::ACT_BLEND_MAX)
	{
		printf("KX_GameObject.playAction(): given blend_mode (%d) is out of range (0 - %d), setting to ACT_BLEND_BLEND", blend_mode, BL_Action::ACT_BLEND_MAX-1);
		blend_mode = BL_Action::ACT_BLEND_BLEND;
	}

	if (layer_weight < 0.f || layer_weight > 1.f)
	{
		printf("KX_GameObject.playAction(): given layer_weight (%f) is out of range (0.0 - 1.0), setting to 0.0", layer_weight);
		layer_weight = 0.f;
	}

	PlayAction(name, start, end, layer, priority, blendin, play_mode, layer_weight, ipo_flags, speed, blend_mode);

	Py_RETURN_NONE;
}

KX_PYMETHODDEF_DOC(KX_GameObject, stopAction,
	"stopAction(layer=0)\n"
	"Stop playing the action on the given layer\n")
{
	short layer = 0;

	if (!PyArg_ParseTuple(args, "|h:stopAction", &layer))
		return NULL;

	layer_check(layer, "stopAction");

	StopAction(layer);

	Py_RETURN_NONE;
}

KX_PYMETHODDEF_DOC(KX_GameObject, getActionFrame,
	"getActionFrame(layer=0)\n"
	"Gets the current frame of the action playing in the supplied layer\n")
{
	short layer = 0;

	if (!PyArg_ParseTuple(args, "|h:getActionFrame", &layer))
		return NULL;

	layer_check(layer, "getActionFrame");

	return PyFloat_FromDouble(GetActionFrame(layer));
}

KX_PYMETHODDEF_DOC(KX_GameObject, getActionName,
	"getActionName(layer=0)\n"
	"Gets the name of the current action playing in the supplied layer\n")
{
	short layer = 0;

	if (!PyArg_ParseTuple(args, "|h:getActionName", &layer))
		return NULL;

	layer_check(layer, "getActionName");

	return PyUnicode_FromString(GetActionName(layer));
}

KX_PYMETHODDEF_DOC(KX_GameObject, setActionFrame,
	"setActionFrame(frame, layer=0)\n"
	"Set the current frame of the action playing in the supplied layer\n")
{
	short layer = 0;
	float frame;

	if (!PyArg_ParseTuple(args, "f|h:setActionFrame", &frame, &layer))
		return NULL;

	layer_check(layer, "setActionFrame");

	SetActionFrame(layer, frame);

	Py_RETURN_NONE;
}

KX_PYMETHODDEF_DOC(KX_GameObject, isPlayingAction,
	"isPlayingAction(layer=0)\n"
	"Checks to see if there is an action playing in the given layer\n")
{
	short layer = 0;

	if (!PyArg_ParseTuple(args, "|h:isPlayingAction", &layer))
		return NULL;

	layer_check(layer, "isPlayingAction");

	return PyBool_FromLong(!IsActionDone(layer));
}


KX_PYMETHODDEF_DOC(KX_GameObject, addDebugProperty,
"addDebugProperty(name, visible=1)\n"
"Added or remove a debug property to the debug list.\n")
{
	KX_Scene *scene = KX_GetActiveScene();
	char *name;
	int visible = 1;

	if (!PyArg_ParseTuple(args,"s|i:debugProperty", &name , &visible))
		return NULL;

	if (visible) {
		if (!scene->PropertyInDebugList(this, name))
			scene->AddDebugProperty(this, name);
	}
	else {
		scene->RemoveDebugProperty(this, name);
	}

	Py_RETURN_NONE;
}


/* dict style access */


/* Matches python dict.get(key, [default]) */
PyObject *KX_GameObject::Pyget(PyObject *args)
{
	PyObject *key;
	PyObject *def = Py_None;
	PyObject *ret;

	if (!PyArg_ParseTuple(args, "O|O:get", &key, &def))
		return NULL;
	
	
	if (PyUnicode_Check(key)) {
		CValue *item = GetProperty(_PyUnicode_AsString(key));
		if (item) {
			ret = item->ConvertValueToPython();
			if (ret)
				return ret;
			else
				return item->GetProxy();
		}
	}
	
	if (m_attr_dict && (ret=PyDict_GetItem(m_attr_dict, key))) {
		Py_INCREF(ret);
		return ret;
	}
	
	Py_INCREF(def);
	return def;
}

bool ConvertPythonToGameObject(SCA_LogicManager *manager, PyObject *value, KX_GameObject **object, bool py_none_ok, const char *error_prefix)
{
	if (value==NULL) {
		PyErr_Format(PyExc_TypeError, "%s, python pointer NULL, should never happen", error_prefix);
		*object = NULL;
		return false;
	}
		
	if (value==Py_None) {
		*object = NULL;
		
		if (py_none_ok) {
			return true;
		} else {
			PyErr_Format(PyExc_TypeError, "%s, expected KX_GameObject or a KX_GameObject name, None is invalid", error_prefix);
			return false;
		}
	}
	
	if (PyUnicode_Check(value)) {
		*object = (KX_GameObject*)manager->GetGameObjectByName(STR_String( _PyUnicode_AsString(value) ));
		
		if (*object) {
			return true;
		} else {
			PyErr_Format(PyExc_ValueError, "%s, requested name \"%s\" did not match any KX_GameObject in this scene", error_prefix, _PyUnicode_AsString(value));
			return false;
		}
	}
	
	if (PyObject_TypeCheck(value, &KX_GameObject::Type)	||
	    PyObject_TypeCheck(value, &KX_LightObject::Type)	||
	    PyObject_TypeCheck(value, &KX_Camera::Type)			||
	    PyObject_TypeCheck(value, &KX_FontObject::Type) ||
	    PyObject_TypeCheck(value, &KX_NavMeshObject::Type))
	{
		*object = static_cast<KX_GameObject*>BGE_PROXY_REF(value);
		
		/* sets the error */
		if (*object==NULL) {
			PyErr_Format(PyExc_SystemError, "%s, " BGE_PROXY_ERROR_MSG, error_prefix);
			return false;
		}
		
		return true;
	}
	
	*object = NULL;
	
	if (py_none_ok) {
		PyErr_Format(PyExc_TypeError, "%s, expect a KX_GameObject, a string or None", error_prefix);
	} else {
		PyErr_Format(PyExc_TypeError, "%s, expect a KX_GameObject or a string", error_prefix);
	}
	
	return false;
}
#endif // WITH_PYTHON
