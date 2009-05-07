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
 * Game object wrapper
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if defined(_WIN64)
typedef unsigned __int64 uint_ptr;
#else
typedef unsigned long uint_ptr;
#endif

#ifdef WIN32
// This warning tells us about truncation of __long__ stl-generated names.
// It can occasionally cause DevStudio to have internal compiler warnings.
#pragma warning( disable : 4786 )     
#endif


#define KX_INERTIA_INFINITE 10000
#include "RAS_IPolygonMaterial.h"
#include "KX_BlenderMaterial.h"
#include "KX_GameObject.h"
#include "RAS_MeshObject.h"
#include "KX_MeshProxy.h"
#include "KX_PolyProxy.h"
#include <stdio.h> // printf
#include "SG_Controller.h"
#include "KX_IPhysicsController.h"
#include "PHY_IGraphicController.h"
#include "SG_Node.h"
#include "SG_Controller.h"
#include "KX_ClientObjectInfo.h"
#include "RAS_BucketManager.h"
#include "KX_RayCast.h"
#include "KX_PythonInit.h"
#include "KX_PyMath.h"
#include "SCA_IActuator.h"
#include "SCA_ISensor.h"
#include "SCA_IController.h"
#include "NG_NetworkScene.h" //Needed for sendMessage()

#include "PyObjectPlus.h" /* python stuff */

// This file defines relationships between parents and children
// in the game engine.

#include "KX_SG_NodeRelationships.h"

static MT_Point3 dummy_point= MT_Point3(0.0, 0.0, 0.0);
static MT_Vector3 dummy_scaling = MT_Vector3(1.0, 1.0, 1.0);
static MT_Matrix3x3 dummy_orientation = MT_Matrix3x3(	1.0, 0.0, 0.0,
														0.0, 1.0, 0.0,
														0.0, 0.0, 1.0);

KX_GameObject::KX_GameObject(
	void* sgReplicationInfo,
	SG_Callbacks callbacks,
	PyTypeObject* T
) : 
	SCA_IObject(T),
	m_bDyna(false),
	m_layer(0),
	m_pBlenderObject(NULL),
	m_pBlenderGroupObject(NULL),
	m_bSuspendDynamics(false),
	m_bUseObjectColor(false),
	m_bIsNegativeScaling(false),
	m_bVisible(true),
	m_bCulled(true),
	m_bOccluder(false),
	m_pPhysicsController1(NULL),
	m_pGraphicController(NULL),
	m_pPhysicsEnvironment(NULL),
	m_xray(false),
	m_pHitObject(NULL),
	m_isDeformable(false),
	m_attr_dict(NULL)
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
	}
	if (m_pGraphicController)
	{
		delete m_pGraphicController;
	}
	
	if (m_attr_dict) {
		PyDict_Clear(m_attr_dict); /* incase of circular refs or other weired cases */
		Py_DECREF(m_attr_dict);
	}
}

KX_GameObject* KX_GameObject::GetClientObject(KX_ClientObjectInfo* info)
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



STR_String KX_GameObject::GetName()
{
	return m_name;
}



void KX_GameObject::SetName(STR_String name)
{
	m_name = name;
};								// Set the name of the value

KX_IPhysicsController* KX_GameObject::GetPhysicsController()
{
	return m_pPhysicsController1;
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
	
	if (result)
		result->AddRef();

	return result;
	
}

void KX_GameObject::SetParent(KX_Scene *scene, KX_GameObject* obj)
{
	// check on valid node in case a python controller holds a reference to a deleted object
	if (obj && GetSGNode() && obj->GetSGNode() && GetSGNode()->GetSGParent() != obj->GetSGNode())
	{
		// Make sure the objects have some scale
		MT_Vector3 scale1 = NodeGetWorldScaling();
		MT_Vector3 scale2 = obj->NodeGetWorldScaling();
		if (fabs(scale2[0]) < FLT_EPSILON || 
			fabs(scale2[1]) < FLT_EPSILON || 
			fabs(scale2[2]) < FLT_EPSILON || 
			fabs(scale1[0]) < FLT_EPSILON || 
			fabs(scale1[1]) < FLT_EPSILON || 
			fabs(scale1[2]) < FLT_EPSILON) { return; }

		// Remove us from our old parent and set our new parent
		RemoveParent(scene);
		obj->GetSGNode()->AddChild(GetSGNode());

		if (m_pPhysicsController1) 
		{
			m_pPhysicsController1->SuspendDynamics(true);
		}
		// Set us to our new scale, position, and orientation
		scale2[0] = 1.0/scale2[0];
		scale2[1] = 1.0/scale2[1];
		scale2[2] = 1.0/scale2[2];
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
		if (m_pPhysicsController1)
		{
			// step 1: find the top parent (not necessarily obj)
			KX_GameObject* rootobj = (KX_GameObject*)obj->GetSGNode()->GetRootSGParent()->GetSGClientObject();
			// step 2: verify it has a physical controller and compound shape
			if (rootobj != NULL && 
				rootobj->m_pPhysicsController1 != NULL &&
				rootobj->m_pPhysicsController1->IsCompound())
			{
				rootobj->m_pPhysicsController1->AddCompoundChild(m_pPhysicsController1);
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
		if (m_pPhysicsController1) 
		{
			// in case this controller was added as a child shape to the parent
			if (rootobj != NULL && 
				rootobj->m_pPhysicsController1 != NULL &&
				rootobj->m_pPhysicsController1->IsCompound())
			{
				rootobj->m_pPhysicsController1->RemoveCompoundChild(m_pPhysicsController1);
			}
			m_pPhysicsController1->RestoreDynamics();
		}
		// graphically, the object hasn't change place, no need to update m_pGraphicController
	}
}

void KX_GameObject::ProcessReplica()
{
	SCA_IObject::ProcessReplica();
	
	m_pPhysicsController1 = NULL;
	m_pGraphicController = NULL;
	m_pSGNode = NULL;
	m_pClient_info = new KX_ClientObjectInfo(*m_pClient_info);
	m_pClient_info->m_gameobject = this;
	m_state = 0;
	if(m_attr_dict)
		m_attr_dict= PyDict_Copy(m_attr_dict);
		
}

static void setGraphicController_recursive(SG_Node* node, bool v)
{
	NodeList& children = node->GetSGChildren();

	for (NodeList::iterator childit = children.begin();!(childit==children.end());++childit)
	{
		SG_Node* childnode = (*childit);
		KX_GameObject *clientgameobj = static_cast<KX_GameObject*>( (*childit)->GetSGClientObject());
		if (clientgameobj != NULL) // This is a GameObject
			clientgameobj->ActivateGraphicController(v, false);
		
		// if the childobj is NULL then this may be an inverse parent link
		// so a non recursive search should still look down this node.
		setGraphicController_recursive(childnode, v);
	}
}


void KX_GameObject::ActivateGraphicController(bool active, bool recurse)
{
	if (m_pGraphicController)
	{
		m_pGraphicController->Activate(active);
	}
	if (recurse)
	{
		setGraphicController_recursive(GetSGNode(), active);
	}
}


CValue* KX_GameObject::GetReplica()
{
	KX_GameObject* replica = new KX_GameObject(*this);

	// this will copy properties and so on...
	replica->ProcessReplica();

	return replica;
}



void KX_GameObject::ApplyForce(const MT_Vector3& force,bool local)
{
	if (m_pPhysicsController1)
		m_pPhysicsController1->ApplyForce(force,local);
}



void KX_GameObject::ApplyTorque(const MT_Vector3& torque,bool local)
{
	if (m_pPhysicsController1)
		m_pPhysicsController1->ApplyTorque(torque,local);
}



void KX_GameObject::ApplyMovement(const MT_Vector3& dloc,bool local)
{
	if (GetSGNode()) 
	{
		if (m_pPhysicsController1) // (IsDynamic())
		{
			m_pPhysicsController1->RelativeTranslate(dloc,local);
		}
		GetSGNode()->RelativeTranslate(dloc,GetSGNode()->GetSGParent(),local);
	}
}



void KX_GameObject::ApplyRotation(const MT_Vector3& drot,bool local)
{
	MT_Matrix3x3 rotmat(drot);
	
	if (GetSGNode()) {
		GetSGNode()->RelativeRotate(rotmat,local);

		if (m_pPhysicsController1) { // (IsDynamic())
			m_pPhysicsController1->RelativeRotate(rotmat,local); 
		}
	}
}



/**
GetOpenGL Matrix, returns an OpenGL 'compatible' matrix
*/
double*	KX_GameObject::GetOpenGLMatrix()
{
	// todo: optimize and only update if necessary
	double* fl = m_OpenGL_4x4Matrix.getPointer();
	if (GetSGNode()) {
		MT_Transform trans;
	
		trans.setOrigin(GetSGNode()->GetWorldPosition());
		trans.setBasis(GetSGNode()->GetWorldOrientation());
	
		MT_Vector3 scaling = GetSGNode()->GetWorldScaling();
		m_bIsNegativeScaling = ((scaling[0] < 0.0) ^ (scaling[1] < 0.0) ^ (scaling[2] < 0.0)) ? true : false;
		trans.scale(scaling[0], scaling[1], scaling[2]);
		trans.getValue(fl);
		GetSGNode()->ClearDirty();
	}
	return fl;
}

void KX_GameObject::AddMeshUser()
{
	for (size_t i=0;i<m_meshes.size();i++)
	{
		m_meshes[i]->AddMeshUser(this, &m_meshSlots);
	}
	// set the part of the mesh slot that never change
	double* fl = GetOpenGLMatrixPtr()->getPointer();
	RAS_Deformer *deformer = GetDeformer();

	//RAS_MeshSlot *ms;
	//for(ms =static_cast<RAS_MeshSlot*>(m_meshSlots.QPeek());
	//	ms!=static_cast<RAS_MeshSlot*>(m_meshSlots.Self());
	//	ms =static_cast<RAS_MeshSlot*>(ms->QPeek()))
	//{
	//	ms->m_OpenGLMatrix = fl;
	//	ms->SetDeformer(deformer);
	//}
	SG_QList::iterator<RAS_MeshSlot> mit(m_meshSlots);
	for(mit.begin(); !mit.end(); ++mit)
	{
		(*mit)->m_OpenGLMatrix = fl;
		(*mit)->SetDeformer(deformer);
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
		//for(ms =static_cast<RAS_MeshSlot*>(m_meshSlots.QPeek());
		//    ms!=static_cast<RAS_MeshSlot*>(m_meshSlots.Self());
		//    ms =static_cast<RAS_MeshSlot*>(ms->QPeek()))
		SG_QList::iterator<RAS_MeshSlot> mit(m_meshSlots);
		for(mit.begin(); !mit.end(); ++mit)
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

void KX_GameObject::UpdateTransform()
{
	// HACK: saves function call for dynamic object, they are handled differently
	if (m_pPhysicsController1 && !m_pPhysicsController1->IsDyna())
		// Note that for Bullet, this does not even update the transform of static object
		// but merely sets there collision flag to "kinematic" because the synchronization is 
		// done during physics simulation
		m_pPhysicsController1->SetSumoTransform(true);
	if (m_pGraphicController)
		// update the culling tree
		m_pGraphicController->SetGraphicTransform();

}

void KX_GameObject::UpdateTransformFunc(SG_IObject* node, void* gameobj, void* scene)
{
	((KX_GameObject*)gameobj)->UpdateTransform();
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

		for(; mit != m_meshes[mesh]->GetLastMaterial(); ++mit)
		{
			RAS_IPolyMaterial* poly = mit->m_bucket->GetPolyMaterial();

			if(poly->GetFlag() & RAS_BLENDERMAT )
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
	if (m_pPhysicsController1) 
	{
		MT_Vector3 lv = local ? NodeGetWorldOrientation() * lin_vel : lin_vel;
		m_pPhysicsController1->SetLinearVelocity(lv + m_pPhysicsController1->GetLinearVelocity(), 0);
	}
}



void KX_GameObject::setLinearVelocity(const MT_Vector3& lin_vel,bool local)
{
	if (m_pPhysicsController1)
		m_pPhysicsController1->SetLinearVelocity(lin_vel,local);
}



void KX_GameObject::setAngularVelocity(const MT_Vector3& ang_vel,bool local)
{
	if (m_pPhysicsController1)
		m_pPhysicsController1->SetAngularVelocity(ang_vel,local);
}


void KX_GameObject::ResolveCombinedVelocities(
	const MT_Vector3 & lin_vel,
	const MT_Vector3 & ang_vel,
	bool lin_vel_local,
	bool ang_vel_local
){
	if (m_pPhysicsController1)
	{

		MT_Vector3 lv = lin_vel_local ? NodeGetWorldOrientation() * lin_vel : lin_vel;
		MT_Vector3 av = ang_vel_local ? NodeGetWorldOrientation() * ang_vel : ang_vel;
		m_pPhysicsController1->resolveCombinedVelocities(
			lv.x(),lv.y(),lv.z(),av.x(),av.y(),av.z());
	}
}


void KX_GameObject::SetObjectColor(const MT_Vector4& rgbavec)
{
	m_bUseObjectColor = true;
	m_objectColor = rgbavec;
}

void KX_GameObject::AlignAxisToVect(const MT_Vector3& dir, int axis, float fac)
{
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
	
	if (fac<=0.0) {
		return;
	}
	
	// normalize
	vect /= len;
	orimat = GetSGNode()->GetWorldOrientation();
	switch (axis)
	{	
		case 0: //x axis
			ori.setValue(orimat[0][2], orimat[1][2], orimat[2][2]); //pivot axis
			if (MT_abs(vect.dot(ori)) > 1.0-3.0*MT_EPSILON) //is the vector paralell to the pivot?
				ori.setValue(orimat[0][1], orimat[1][1], orimat[2][1]); //change the pivot!
			if (fac == 1.0) {
				x = vect;
			} else {
				x = (vect * fac) + ((orimat * MT_Vector3(1.0, 0.0, 0.0)) * (1-fac));
				len = x.length();
				if (MT_fuzzyZero(len)) x = vect;
				else x /= len;
			}
			y = ori.cross(x);
			z = x.cross(y);
			break;
		case 1: //y axis
			ori.setValue(orimat[0][0], orimat[1][0], orimat[2][0]);
			if (MT_abs(vect.dot(ori)) > 1.0-3.0*MT_EPSILON)
				ori.setValue(orimat[0][2], orimat[1][2], orimat[2][2]);
			if (fac == 1.0) {
				y = vect;
			} else {
				y = (vect * fac) + ((orimat * MT_Vector3(0.0, 1.0, 0.0)) * (1-fac));
				len = y.length();
				if (MT_fuzzyZero(len)) y = vect;
				else y /= len;
			}
			z = ori.cross(y);
			x = y.cross(z);
			break;
		case 2: //z axis
			ori.setValue(orimat[0][1], orimat[1][1], orimat[2][1]);
			if (MT_abs(vect.dot(ori)) > 1.0-3.0*MT_EPSILON)
				ori.setValue(orimat[0][0], orimat[1][0], orimat[2][0]);
			if (fac == 1.0) {
				z = vect;
			} else {
				z = (vect * fac) + ((orimat * MT_Vector3(0.0, 0.0, 1.0)) * (1-fac));
				len = z.length();
				if (MT_fuzzyZero(len)) z = vect;
				else z /= len;
			}
			x = ori.cross(z);
			y = z.cross(x);
			break;
		default: //wrong input?
			cout << "alignAxisToVect(): Wrong axis '" << axis <<"'\n";
			return;
	}
	x.normalize(); //normalize the vectors
	y.normalize();
	z.normalize();
	orimat.setValue(	x[0],y[0],z[0],
						x[1],y[1],z[1],
						x[2],y[2],z[2]);
	if (GetSGNode()->GetSGParent() != NULL)
	{
		// the object is a child, adapt its local orientation so that 
		// the global orientation is aligned as we want.
		MT_Matrix3x3 invori = GetSGNode()->GetSGParent()->GetWorldOrientation().inverse();
		NodeSetLocalOrientation(invori*orimat);
	}
	else
		NodeSetLocalOrientation(orimat);
}

MT_Scalar KX_GameObject::GetMass()
{
	if (m_pPhysicsController1)
	{
		return m_pPhysicsController1->GetMass();
	}
	return 0.0;
}

MT_Vector3 KX_GameObject::GetLocalInertia()
{
	MT_Vector3 local_inertia(0.0,0.0,0.0);
	if (m_pPhysicsController1)
	{
		local_inertia = m_pPhysicsController1->GetLocalInertia();
	}
	return local_inertia;
}

MT_Vector3 KX_GameObject::GetLinearVelocity(bool local)
{
	MT_Vector3 velocity(0.0,0.0,0.0), locvel;
	MT_Matrix3x3 ori;
	if (m_pPhysicsController1)
	{
		velocity = m_pPhysicsController1->GetLinearVelocity();
		
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
	MT_Vector3 velocity(0.0,0.0,0.0), locvel;
	MT_Matrix3x3 ori;
	if (m_pPhysicsController1)
	{
		velocity = m_pPhysicsController1->GetAngularVelocity();
		
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
	if (m_pPhysicsController1)
	{
		return m_pPhysicsController1->GetVelocity(point);
	}
	return MT_Vector3(0.0,0.0,0.0);
}

// scenegraph node stuff

void KX_GameObject::NodeSetLocalPosition(const MT_Point3& trans)
{
	// check on valid node in case a python controller holds a reference to a deleted object
	if (!GetSGNode())
		return;

	if (m_pPhysicsController1 && !GetSGNode()->GetSGParent())
	{
		// don't update physic controller if the object is a child:
		// 1) the transformation will not be right
		// 2) in this case, the physic controller is necessarily a static object
		//    that is updated from the normal kinematic synchronization
		m_pPhysicsController1->setPosition(trans);
	}

	GetSGNode()->SetLocalPosition(trans);

}



void KX_GameObject::NodeSetLocalOrientation(const MT_Matrix3x3& rot)
{
	// check on valid node in case a python controller holds a reference to a deleted object
	if (!GetSGNode())
		return;

	if (m_pPhysicsController1 && !GetSGNode()->GetSGParent())
	{
		// see note above
		m_pPhysicsController1->setOrientation(rot);
	}
	GetSGNode()->SetLocalOrientation(rot);
}



void KX_GameObject::NodeSetLocalScale(const MT_Vector3& scale)
{
	// check on valid node in case a python controller holds a reference to a deleted object
	if (!GetSGNode())
		return;

	if (m_pPhysicsController1 && !GetSGNode()->GetSGParent())
	{
		// see note above
		m_pPhysicsController1->setScaling(scale);
	}
	GetSGNode()->SetLocalScale(scale);
}



void KX_GameObject::NodeSetRelativeScale(const MT_Vector3& scale)
{
	if (GetSGNode())
	{
		GetSGNode()->RelativeScale(scale);
		if (m_pPhysicsController1 && (!GetSGNode()->GetSGParent()))
		{
			// see note above
			// we can use the local scale: it's the same thing for a root object 
			// and the world scale is not yet updated
			MT_Vector3 newscale = GetSGNode()->GetLocalScale();
			m_pPhysicsController1->setScaling(newscale);
		}
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
		if (fabs(scale[0]) < FLT_EPSILON || 
			fabs(scale[1]) < FLT_EPSILON || 
			fabs(scale[2]) < FLT_EPSILON)
		{ 
			return; 
		}
		scale[0] = 1.0/scale[0];
		scale[1] = 1.0/scale[1];
		scale[2] = 1.0/scale[2];
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



const MT_Vector3& KX_GameObject::NodeGetWorldScaling() const
{
	// check on valid node in case a python controller holds a reference to a deleted object
	if (!GetSGNode())
		return dummy_scaling;

	return GetSGNode()->GetWorldScaling();
}



const MT_Point3& KX_GameObject::NodeGetWorldPosition() const
{
	// check on valid node in case a python controller holds a reference to a deleted object
	if (GetSGNode())
		return GetSGNode()->GetWorldPosition();
	else
		return dummy_point;
}

/* Suspend/ resume: for the dynamic behaviour, there is a simple
 * method. For the residual motion, there is not. I wonder what the
 * correct solution is for Sumo. Remove from the motion-update tree?
 *
 * So far, only switch the physics and logic.
 * */

void KX_GameObject::Resume(void)
{
	if (m_suspended) {
		SCA_IObject::Resume();
		if(GetPhysicsController())
			GetPhysicsController()->RestoreDynamics();

		m_suspended = false;
	}
}

void KX_GameObject::Suspend()
{
	if ((!m_ignore_activity_culling) 
		&& (!m_suspended))  {
		SCA_IObject::Suspend();
		if(GetPhysicsController())
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
	{"getReactionForce", (PyCFunction) KX_GameObject::sPyGetReactionForce, METH_NOARGS},
	{"alignAxisToVect",(PyCFunction) KX_GameObject::sPyAlignAxisToVect, METH_VARARGS},
	{"getAxisVect",(PyCFunction) KX_GameObject::sPyGetAxisVect, METH_O},
	{"suspendDynamics", (PyCFunction)KX_GameObject::sPySuspendDynamics,METH_NOARGS},
	{"restoreDynamics", (PyCFunction)KX_GameObject::sPyRestoreDynamics,METH_NOARGS},
	{"enableRigidBody", (PyCFunction)KX_GameObject::sPyEnableRigidBody,METH_NOARGS},
	{"disableRigidBody", (PyCFunction)KX_GameObject::sPyDisableRigidBody,METH_NOARGS},
	{"applyImpulse", (PyCFunction) KX_GameObject::sPyApplyImpulse, METH_VARARGS},
	{"setCollisionMargin", (PyCFunction) KX_GameObject::sPySetCollisionMargin, METH_O},
	{"setParent", (PyCFunction)KX_GameObject::sPySetParent,METH_O},
	{"setVisible",(PyCFunction) KX_GameObject::sPySetVisible, METH_VARARGS},
	{"setOcclusion",(PyCFunction) KX_GameObject::sPySetOcclusion, METH_VARARGS},
	{"removeParent", (PyCFunction)KX_GameObject::sPyRemoveParent,METH_NOARGS},
	{"getChildren", (PyCFunction)KX_GameObject::sPyGetChildren,METH_NOARGS},
	{"getChildrenRecursive", (PyCFunction)KX_GameObject::sPyGetChildrenRecursive,METH_NOARGS},
	{"getPhysicsId", (PyCFunction)KX_GameObject::sPyGetPhysicsId,METH_NOARGS},
	{"getPropertyNames", (PyCFunction)KX_GameObject::sPyGetPropertyNames,METH_NOARGS},
	{"replaceMesh",(PyCFunction) KX_GameObject::sPyReplaceMesh, METH_O},
	{"endObject",(PyCFunction) KX_GameObject::sPyEndObject, METH_NOARGS},
	
	KX_PYMETHODTABLE(KX_GameObject, rayCastTo),
	KX_PYMETHODTABLE(KX_GameObject, rayCast),
	KX_PYMETHODTABLE_O(KX_GameObject, getDistanceTo),
	KX_PYMETHODTABLE_O(KX_GameObject, getVectTo),
	KX_PYMETHODTABLE(KX_GameObject, sendMessage),

	// deprecated
	{"getPosition", (PyCFunction) KX_GameObject::sPyGetPosition, METH_NOARGS},
	{"setPosition", (PyCFunction) KX_GameObject::sPySetPosition, METH_O},
	{"setWorldPosition", (PyCFunction) KX_GameObject::sPySetWorldPosition, METH_O},
	{"getOrientation", (PyCFunction) KX_GameObject::sPyGetOrientation, METH_NOARGS},
	{"setOrientation", (PyCFunction) KX_GameObject::sPySetOrientation, METH_O},
	{"getState",(PyCFunction) KX_GameObject::sPyGetState, METH_NOARGS},
	{"setState",(PyCFunction) KX_GameObject::sPySetState, METH_O},
	{"getParent", (PyCFunction)KX_GameObject::sPyGetParent,METH_NOARGS},
	{"getVisible",(PyCFunction) KX_GameObject::sPyGetVisible, METH_NOARGS},
	{"getMass", (PyCFunction) KX_GameObject::sPyGetMass, METH_NOARGS},
	{"getMesh", (PyCFunction)KX_GameObject::sPyGetMesh,METH_VARARGS},
	{NULL,NULL} //Sentinel
};

PyAttributeDef KX_GameObject::Attributes[] = {
	KX_PYATTRIBUTE_RO_FUNCTION("name",		KX_GameObject, pyattr_get_name),
	KX_PYATTRIBUTE_RO_FUNCTION("parent",	KX_GameObject, pyattr_get_parent),
	KX_PYATTRIBUTE_RW_FUNCTION("mass",		KX_GameObject, pyattr_get_mass,		pyattr_set_mass),
	KX_PYATTRIBUTE_RW_FUNCTION("linVelocityMin",		KX_GameObject, pyattr_get_lin_vel_min, pyattr_set_lin_vel_min),
	KX_PYATTRIBUTE_RW_FUNCTION("linVelocityMax",		KX_GameObject, pyattr_get_lin_vel_max, pyattr_set_lin_vel_max),
	KX_PYATTRIBUTE_RW_FUNCTION("visible",	KX_GameObject, pyattr_get_visible,	pyattr_set_visible),
	KX_PYATTRIBUTE_BOOL_RW    ("occlusion", KX_GameObject, m_bOccluder),
	KX_PYATTRIBUTE_RW_FUNCTION("position",	KX_GameObject, pyattr_get_worldPosition,	pyattr_set_localPosition),
	KX_PYATTRIBUTE_RO_FUNCTION("localInertia",	KX_GameObject, pyattr_get_localInertia),
	KX_PYATTRIBUTE_RW_FUNCTION("orientation",KX_GameObject,pyattr_get_worldOrientation,pyattr_set_localOrientation),
	KX_PYATTRIBUTE_RW_FUNCTION("scaling",	KX_GameObject, pyattr_get_worldScaling,	pyattr_set_localScaling),
	KX_PYATTRIBUTE_RW_FUNCTION("timeOffset",KX_GameObject, pyattr_get_timeOffset,pyattr_set_timeOffset),
	KX_PYATTRIBUTE_RW_FUNCTION("state",		KX_GameObject, pyattr_get_state,	pyattr_set_state),
	KX_PYATTRIBUTE_RO_FUNCTION("meshes",	KX_GameObject, pyattr_get_meshes),
	KX_PYATTRIBUTE_RW_FUNCTION("localOrientation",KX_GameObject,pyattr_get_localOrientation,pyattr_set_localOrientation),
	KX_PYATTRIBUTE_RW_FUNCTION("worldOrientation",KX_GameObject,pyattr_get_worldOrientation,pyattr_set_worldOrientation),
	KX_PYATTRIBUTE_RW_FUNCTION("localPosition",	KX_GameObject, pyattr_get_localPosition,	pyattr_set_localPosition),
	KX_PYATTRIBUTE_RW_FUNCTION("worldPosition",	KX_GameObject, pyattr_get_worldPosition,    pyattr_set_worldPosition),
	KX_PYATTRIBUTE_RW_FUNCTION("localScaling",	KX_GameObject, pyattr_get_localScaling,	pyattr_set_localScaling),
	KX_PYATTRIBUTE_RO_FUNCTION("worldScaling",	KX_GameObject, pyattr_get_worldScaling),
	
	/* Experemental, dont rely on these yet */
	KX_PYATTRIBUTE_RO_FUNCTION("sensors",		KX_GameObject, pyattr_get_sensors),
	KX_PYATTRIBUTE_RO_FUNCTION("controllers",	KX_GameObject, pyattr_get_controllers),
	KX_PYATTRIBUTE_RO_FUNCTION("actuators",		KX_GameObject, pyattr_get_actuators),
	{NULL} //Sentinel
};


/*
bool KX_GameObject::ConvertPythonVectorArgs(PyObject* args,
											MT_Vector3& pos,
											MT_Vector3& pos2)
{
	PyObject* pylist;
	PyObject* pylist2;
	bool error = (PyArg_ParseTuple(args,"OO",&pylist,&pylist2)) != 0;

	pos = ConvertPythonPylist(pylist);
	pos2 = ConvertPythonPylist(pylist2);
		
	return error;
}
*/

PyObject* KX_GameObject::PyReplaceMesh(PyObject* value)
{
	KX_Scene *scene = KX_GetActiveScene();
	RAS_MeshObject* new_mesh;
	
	if (!ConvertPythonToMesh(value, &new_mesh, false, "gameOb.replaceMesh(value): KX_GameObject"))
		return NULL;
	
	scene->ReplaceMesh(this, new_mesh);
	Py_RETURN_NONE;
}

PyObject* KX_GameObject::PyEndObject()
{
	KX_Scene *scene = KX_GetActiveScene();
	
	scene->DelayedRemoveObject(this);
	
	Py_RETURN_NONE;

}


PyObject* KX_GameObject::PyGetPosition()
{
	ShowDeprecationWarning("getPosition()", "the position property");
	return PyObjectFrom(NodeGetWorldPosition());
}

PyObject *KX_GameObject::Map_GetItem(PyObject *self_v, PyObject *item)
{
	KX_GameObject* self= static_cast<KX_GameObject*>BGE_PROXY_REF(self_v);
	const char *attr_str= PyString_AsString(item);
	CValue* resultattr;
	PyObject* pyconvert;
	
	if (self==NULL) {
		PyErr_SetString(PyExc_SystemError, BGE_PROXY_ERROR_MSG);
		return NULL;
	}
	
	/* first see if the attributes a string and try get the cvalue attribute */
	if(attr_str && (resultattr=self->GetProperty(attr_str))) {
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
		if(attr_str)	PyErr_Format(PyExc_KeyError, "value = gameOb[key]: KX_GameObject, key \"%s\" does not exist", attr_str);
		else			PyErr_SetString(PyExc_KeyError, "value = gameOb[key]: KX_GameObject, key does not exist");
		return NULL;
	}
		
}


int KX_GameObject::Map_SetItem(PyObject *self_v, PyObject *key, PyObject *val)
{
	KX_GameObject* self= static_cast<KX_GameObject*>BGE_PROXY_REF(self_v);
	const char *attr_str= PyString_AsString(key);
	if(attr_str==NULL)
		PyErr_Clear();
	
	if (self==NULL) {
		PyErr_SetString(PyExc_SystemError, BGE_PROXY_ERROR_MSG);
		return -1;
	}
	
	if (val==NULL) { /* del ob["key"] */
		int del= 0;
		
		/* try remove both just incase */
		if(attr_str)
			del |= (self->RemoveProperty(attr_str)==true) ? 1:0;
		
		if(self->m_attr_dict)
			del |= (PyDict_DelItem(self->m_attr_dict, key)==0) ? 1:0;
		
		if (del==0) {
			if(attr_str)	PyErr_Format(PyExc_KeyError, "gameOb[key] = value: KX_GameObject, key \"%s\" could not be set", attr_str);
			else			PyErr_SetString(PyExc_KeyError, "gameOb[key] = value: KX_GameObject, key could not be set");
			return -1;
		}
		else if (self->m_attr_dict) {
			PyErr_Clear(); /* PyDict_DelItem sets an error when it fails */
		}
	}
	else { /* ob["key"] = value */
		int set= 0;
		
		/* as CValue */
		if(attr_str && BGE_PROXY_CHECK_TYPE(val)==0) /* dont allow GameObjects for eg to be assigned to CValue props */
		{
			CValue* vallie = self->ConvertPythonToValue(val, ""); /* error unused */
			
			if(vallie)
			{
				CValue* oldprop = self->GetProperty(attr_str);
				
				if (oldprop)
					oldprop->SetValue(vallie);
				else
					self->SetProperty(attr_str, vallie);
				
				vallie->Release();
				set= 1;
				
				/* try remove dict value to avoid double ups */
				if (self->m_attr_dict){
					if (PyDict_DelItem(self->m_attr_dict, key) != 0)
						PyErr_Clear();
				}
			}
			else {
				PyErr_Clear();
			}
		}
		
		if(set==0)
		{
			if (self->m_attr_dict==NULL) /* lazy init */
				self->m_attr_dict= PyDict_New();
			
			
			if(PyDict_SetItem(self->m_attr_dict, key, val)==0)
			{
				if(attr_str)
					self->RemoveProperty(attr_str); /* overwrite the CValue if it exists */
				set= 1;
			}
			else {
				if(attr_str)	PyErr_Format(PyExc_KeyError, "gameOb[key] = value: KX_GameObject, key \"%s\" not be added to internal dictionary", attr_str);
				else			PyErr_SetString(PyExc_KeyError, "gameOb[key] = value: KX_GameObject, key not be added to internal dictionary");
			}
		}
		
		if(set==0)
			return -1; /* pythons error value */
		
	}
	
	return 0; /* success */
}

/* Cant set the len otherwise it can evaluate as false */
PyMappingMethods KX_GameObject::Mapping = {
	(lenfunc)NULL					, 			/*inquiry mp_length */
	(binaryfunc)KX_GameObject::Map_GetItem,		/*binaryfunc mp_subscript */
	(objobjargproc)KX_GameObject::Map_SetItem,	/*objobjargproc mp_ass_subscript */
};

PyTypeObject KX_GameObject::Type = {
#if (PY_VERSION_HEX >= 0x02060000)
	PyVarObject_HEAD_INIT(NULL, 0)
#else
	/* python 2.5 and below */
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
#endif
		"KX_GameObject",
		sizeof(PyObjectPlus_Proxy),
		0,
		py_base_dealloc,
		0,
		0,
		0,
		0,
		py_base_repr,
		0,0,
		&Mapping,
		0,0,0,
		py_base_getattro,
		py_base_setattro,
		0,0,0,0,0,0,0,0,0,
		Methods
};






PyParentObject KX_GameObject::Parents[] = {
	&KX_GameObject::Type,
		&SCA_IObject::Type,
		&CValue::Type,
		NULL
};

PyObject* KX_GameObject::pyattr_get_name(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self= static_cast<KX_GameObject*>(self_v);
	return PyString_FromString(self->GetName().ReadPtr());
}

PyObject* KX_GameObject::pyattr_get_parent(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self= static_cast<KX_GameObject*>(self_v);
	KX_GameObject* parent = self->GetParent();
	if (parent)
		return parent->GetProxy();
	Py_RETURN_NONE;
}

PyObject* KX_GameObject::pyattr_get_mass(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self= static_cast<KX_GameObject*>(self_v);
	KX_IPhysicsController *spc = self->GetPhysicsController();
	return PyFloat_FromDouble(spc ? spc->GetMass() : 0.0f);
}

int KX_GameObject::pyattr_set_mass(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_GameObject* self= static_cast<KX_GameObject*>(self_v);
	KX_IPhysicsController *spc = self->GetPhysicsController();
	MT_Scalar val = PyFloat_AsDouble(value);
	if (val < 0.0f) { /* also accounts for non float */
		PyErr_SetString(PyExc_AttributeError, "gameOb.mass = float: KX_GameObject, expected a float zero or above");
		return 1;
	}

	if (spc)
		spc->SetMass(val);

	return 0;
}

PyObject* KX_GameObject::pyattr_get_lin_vel_min(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self= static_cast<KX_GameObject*>(self_v);
	KX_IPhysicsController *spc = self->GetPhysicsController();
	return PyFloat_FromDouble(spc ? spc->GetLinVelocityMax() : 0.0f);
}

int KX_GameObject::pyattr_set_lin_vel_min(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_GameObject* self= static_cast<KX_GameObject*>(self_v);
	KX_IPhysicsController *spc = self->GetPhysicsController();
	MT_Scalar val = PyFloat_AsDouble(value);
	if (val < 0.0f) { /* also accounts for non float */
		PyErr_SetString(PyExc_AttributeError, "gameOb.linVelocityMin = float: KX_GameObject, expected a float zero or above");
		return 1;
	}

	if (spc)
		spc->SetLinVelocityMin(val);

	return 0;
}

PyObject* KX_GameObject::pyattr_get_lin_vel_max(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self= static_cast<KX_GameObject*>(self_v);
	KX_IPhysicsController *spc = self->GetPhysicsController();
	return PyFloat_FromDouble(spc ? spc->GetLinVelocityMax() : 0.0f);
}

int KX_GameObject::pyattr_set_lin_vel_max(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_GameObject* self= static_cast<KX_GameObject*>(self_v);
	KX_IPhysicsController *spc = self->GetPhysicsController();
	MT_Scalar val = PyFloat_AsDouble(value);
	if (val < 0.0f) { /* also accounts for non float */
		PyErr_SetString(PyExc_AttributeError, "gameOb.linVelocityMax = float: KX_GameObject, expected a float zero or above");
		return 1;
	}

	if (spc)
		spc->SetLinVelocityMax(val);

	return 0;
}


PyObject* KX_GameObject::pyattr_get_visible(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self= static_cast<KX_GameObject*>(self_v);
	return PyBool_FromLong(self->GetVisible());
}

int KX_GameObject::pyattr_set_visible(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_GameObject* self= static_cast<KX_GameObject*>(self_v);
	int param = PyObject_IsTrue( value );
	if (param == -1) {
		PyErr_SetString(PyExc_AttributeError, "gameOb.visible = bool: KX_GameObject, expected True or False");
		return 1;
	}

	self->SetVisible(param, false);
	self->UpdateBuckets(false);
	return 0;
}

PyObject* KX_GameObject::pyattr_get_worldPosition(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self= static_cast<KX_GameObject*>(self_v);
	return PyObjectFrom(self->NodeGetWorldPosition());
}

int KX_GameObject::pyattr_set_worldPosition(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_GameObject* self= static_cast<KX_GameObject*>(self_v);
	MT_Point3 pos;
	if (!PyVecTo(value, pos))
		return 1;
	
	self->NodeSetWorldPosition(pos);
	self->NodeUpdateGS(0.f);
	return 0;
}

PyObject* KX_GameObject::pyattr_get_localPosition(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self= static_cast<KX_GameObject*>(self_v);
	if (self->GetSGNode())
		return PyObjectFrom(self->GetSGNode()->GetLocalPosition());
	else
		return PyObjectFrom(dummy_point);
}

int KX_GameObject::pyattr_set_localPosition(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_GameObject* self= static_cast<KX_GameObject*>(self_v);
	MT_Point3 pos;
	if (!PyVecTo(value, pos))
		return 1;
	
	self->NodeSetLocalPosition(pos);
	self->NodeUpdateGS(0.f);
	return 0;
}

PyObject* KX_GameObject::pyattr_get_localInertia(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self= static_cast<KX_GameObject*>(self_v);
	if (self->GetPhysicsController())
	{
		return PyObjectFrom(self->GetPhysicsController()->GetLocalInertia());
	}
	return Py_BuildValue("fff", 0.0f, 0.0f, 0.0f);
}

PyObject* KX_GameObject::pyattr_get_worldOrientation(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self= static_cast<KX_GameObject*>(self_v);
	return PyObjectFrom(self->NodeGetWorldOrientation());
}

int KX_GameObject::pyattr_set_worldOrientation(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_GameObject* self= static_cast<KX_GameObject*>(self_v);
	
	/* if value is not a sequence PyOrientationTo makes an error */
	MT_Matrix3x3 rot;
	if (!PyOrientationTo(value, rot, "gameOb.worldOrientation = sequence: KX_GameObject, "))
		return NULL;

	if (self->GetSGNode() && self->GetSGNode()->GetSGParent()) {
		self->NodeSetLocalOrientation(self->GetSGNode()->GetSGParent()->GetWorldOrientation().inverse()*rot);
	}
	else {
		self->NodeSetLocalOrientation(rot);
	}
	
	self->NodeUpdateGS(0.f);
	return 0;
}

PyObject* KX_GameObject::pyattr_get_localOrientation(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self= static_cast<KX_GameObject*>(self_v);
	if (self->GetSGNode())
		return PyObjectFrom(self->GetSGNode()->GetLocalOrientation());
	else
		return PyObjectFrom(dummy_orientation);
}

int KX_GameObject::pyattr_set_localOrientation(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_GameObject* self= static_cast<KX_GameObject*>(self_v);
	
	/* if value is not a sequence PyOrientationTo makes an error */
	MT_Matrix3x3 rot;
	if (!PyOrientationTo(value, rot, "gameOb.localOrientation = sequence: KX_GameObject, "))
		return NULL;

	self->NodeSetLocalOrientation(rot);
	self->NodeUpdateGS(0.f);
	return 0;
}

PyObject* KX_GameObject::pyattr_get_worldScaling(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self= static_cast<KX_GameObject*>(self_v);
	return PyObjectFrom(self->NodeGetWorldScaling());
}

PyObject* KX_GameObject::pyattr_get_localScaling(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self= static_cast<KX_GameObject*>(self_v);
	if (self->GetSGNode())
		return PyObjectFrom(self->GetSGNode()->GetLocalScale());
	else
		return PyObjectFrom(dummy_scaling);
}

int KX_GameObject::pyattr_set_localScaling(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_GameObject* self= static_cast<KX_GameObject*>(self_v);
	MT_Vector3 scale;
	if (!PyVecTo(value, scale))
		return 1;

	self->NodeSetLocalScale(scale);
	self->NodeUpdateGS(0.f);
	return 0;
}

PyObject* KX_GameObject::pyattr_get_timeOffset(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self= static_cast<KX_GameObject*>(self_v);
	SG_Node* sg_parent;
	if (self->GetSGNode() && (sg_parent = self->GetSGNode()->GetSGParent()) != NULL && sg_parent->IsSlowParent()) {
		return PyFloat_FromDouble(static_cast<KX_SlowParentRelation *>(sg_parent->GetParentRelation())->GetTimeOffset());
	} else {
		return PyFloat_FromDouble(0.0);
	}
}

int KX_GameObject::pyattr_set_timeOffset(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_GameObject* self= static_cast<KX_GameObject*>(self_v);
	if (self->GetSGNode()) {
		MT_Scalar val = PyFloat_AsDouble(value);
		SG_Node* sg_parent= self->GetSGNode()->GetSGParent();
		if (val < 0.0f) { /* also accounts for non float */
			PyErr_SetString(PyExc_AttributeError, "gameOb.timeOffset = float: KX_GameObject, expected a float zero or above");
			return 1;
		}
		if (sg_parent && sg_parent->IsSlowParent())
			static_cast<KX_SlowParentRelation *>(sg_parent->GetParentRelation())->SetTimeOffset(val);
	}
	return 0;
}

PyObject* KX_GameObject::pyattr_get_state(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self= static_cast<KX_GameObject*>(self_v);
	int state = 0;
	state |= self->GetState();
	return PyInt_FromLong(state);
}

int KX_GameObject::pyattr_set_state(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_GameObject* self= static_cast<KX_GameObject*>(self_v);
	int state_i = PyInt_AsLong(value);
	unsigned int state = 0;
	
	if (state_i == -1 && PyErr_Occurred()) {
		PyErr_SetString(PyExc_TypeError, "gameOb.state = int: KX_GameObject, expected an int bit field");
		return 1;
	}
	
	state |= state_i;
	if ((state & ((1<<30)-1)) == 0) {
		PyErr_SetString(PyExc_AttributeError, "gameOb.state = int: KX_GameObject, state bitfield was not between 0 and 30 (1<<0 and 1<<29)");
		return 1;
	}
	self->SetState(state);
	return 0;
}

PyObject* KX_GameObject::pyattr_get_meshes(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self= static_cast<KX_GameObject*>(self_v);
	PyObject *meshes= PyList_New(self->m_meshes.size());
	int i;
	
	for(i=0; i < (int)self->m_meshes.size(); i++)
	{
		KX_MeshProxy* meshproxy = new KX_MeshProxy(self->m_meshes[i]);
		PyList_SET_ITEM(meshes, i, meshproxy->GetProxy());
	}
	
	return meshes;
}

/* experemental! */
PyObject* KX_GameObject::pyattr_get_sensors(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self= static_cast<KX_GameObject*>(self_v);
	SCA_SensorList& sensors= self->GetSensors();
	PyObject* resultlist = PyList_New(sensors.size());
	
	for (unsigned int index=0;index<sensors.size();index++)
		PyList_SET_ITEM(resultlist, index, sensors[index]->GetProxy());
	
	return resultlist;
}

PyObject* KX_GameObject::pyattr_get_controllers(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self= static_cast<KX_GameObject*>(self_v);
	SCA_ControllerList& controllers= self->GetControllers();
	PyObject* resultlist = PyList_New(controllers.size());
	
	for (unsigned int index=0;index<controllers.size();index++)
		PyList_SET_ITEM(resultlist, index, controllers[index]->GetProxy());
	
	return resultlist;
}

PyObject* KX_GameObject::pyattr_get_actuators(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_GameObject* self= static_cast<KX_GameObject*>(self_v);
	SCA_ActuatorList& actuators= self->GetActuators();
	PyObject* resultlist = PyList_New(actuators.size());
	
	for (unsigned int index=0;index<actuators.size();index++)
		PyList_SET_ITEM(resultlist, index, actuators[index]->GetProxy());
	
	return resultlist;
}

/* We need these because the macros have a return in them */
PyObject* KX_GameObject::py_getattro__internal(PyObject *attr)
{
	py_getattro_up(SCA_IObject);
}

int KX_GameObject::py_setattro__internal(PyObject *attr, PyObject *value)	// py_setattro method
{
	py_setattro_up(SCA_IObject);
}


PyObject* KX_GameObject::py_getattro(PyObject *attr)
{
	PyObject *object= py_getattro__internal(attr);
	
	if (object==NULL && m_attr_dict)
	{
		/* backup the exception incase the attr doesnt exist in the dict either */
		PyObject *err_type, *err_value, *err_tb;
		PyErr_Fetch(&err_type, &err_value, &err_tb);
		
		object= PyDict_GetItem(m_attr_dict, attr);
		if (object) {
			Py_INCREF(object);
			
			PyErr_Clear();
			Py_XDECREF( err_type );
			Py_XDECREF( err_value );
			Py_XDECREF( err_tb );
		}
		else {
			PyErr_Restore(err_type, err_value, err_tb); /* use the error from the parent function */
		}
	}
	return object;
}

PyObject* KX_GameObject::py_getattro_dict() {
	//py_getattro_dict_up(SCA_IObject);
	PyObject *dict= py_getattr_dict(SCA_IObject::py_getattro_dict(), Type.tp_dict);
	if(dict==NULL)
		return NULL;
	
	/* normally just return this but KX_GameObject has some more items */

	
	/* Not super fast getting as a list then making into dict keys but its only for dir() */
	PyObject *list= ConvertKeysToPython();
	if(list)
	{
		int i;
		for(i=0; i<PyList_Size(list); i++)
			PyDict_SetItem(dict, PyList_GET_ITEM(list, i), Py_None);
	}
	else
		PyErr_Clear();
	
	Py_DECREF(list);
	
	/* Add m_attr_dict if we have it */
	if(m_attr_dict)
		PyDict_Update(dict, m_attr_dict);
	
	return dict;
}

int KX_GameObject::py_setattro(PyObject *attr, PyObject *value)	// py_setattro method
{
	int ret;
	
	ret= py_setattro__internal(attr, value);
	
	if (ret==PY_SET_ATTR_SUCCESS) {
		/* remove attribute in our own dict to avoid double ups */
		/* NOTE: Annoying that we also do this for setting builtin attributes like mass and visibility :/ */
		if (m_attr_dict) {
			if (PyDict_DelItem(m_attr_dict, attr) != 0)
				PyErr_Clear();
		}
	}
	
	if (ret==PY_SET_ATTR_COERCE_FAIL) {
		/* CValue attribute exists, remove CValue and add PyDict value */
		RemoveProperty(STR_String(PyString_AsString(attr)));
		ret= PY_SET_ATTR_MISSING;
	}
	
	if (ret==PY_SET_ATTR_MISSING) {
		/* Lazy initialization */
		if (m_attr_dict==NULL)
			m_attr_dict = PyDict_New();
		
		if (PyDict_SetItem(m_attr_dict, attr, value)==0) {
			PyErr_Clear();
			ret= PY_SET_ATTR_SUCCESS;
		}
		else {
			PyErr_Format(PyExc_AttributeError, "gameOb.myAttr = value: KX_GameObject, failed assigning value to internal dictionary");
			ret= PY_SET_ATTR_FAIL;
		}
	}
	
	return ret;	
}


int	KX_GameObject::py_delattro(PyObject *attr)
{
	char *attr_str= PyString_AsString(attr); 
	
	if (RemoveProperty(STR_String(attr_str))) // XXX - should call CValues instead but its only 2 lines here
		return 0;
	
	if (m_attr_dict && (PyDict_DelItem(m_attr_dict, attr) == 0))
		return 0;
	
	PyErr_Format(PyExc_AttributeError, "del gameOb.myAttr: KX_GameObject, attribute \"%s\" dosnt exist", attr_str);
	return 1;
}


PyObject* KX_GameObject::PyApplyForce(PyObject* args)
{
	int local = 0;
	PyObject* pyvect;

	if (PyArg_ParseTuple(args, "O|i:applyForce", &pyvect, &local)) {
		MT_Vector3 force;
		if (PyVecTo(pyvect, force)) {
			ApplyForce(force, (local!=0));
			Py_RETURN_NONE;
		}
	}
	return NULL;
}

PyObject* KX_GameObject::PyApplyTorque(PyObject* args)
{
	int local = 0;
	PyObject* pyvect;

	if (PyArg_ParseTuple(args, "O|i:applyTorque", &pyvect, &local)) {
		MT_Vector3 torque;
		if (PyVecTo(pyvect, torque)) {
			ApplyTorque(torque, (local!=0));
			Py_RETURN_NONE;
		}
	}
	return NULL;
}

PyObject* KX_GameObject::PyApplyRotation(PyObject* args)
{
	int local = 0;
	PyObject* pyvect;

	if (PyArg_ParseTuple(args, "O|i:applyRotation", &pyvect, &local)) {
		MT_Vector3 rotation;
		if (PyVecTo(pyvect, rotation)) {
			ApplyRotation(rotation, (local!=0));
			Py_RETURN_NONE;
		}
	}
	return NULL;
}

PyObject* KX_GameObject::PyApplyMovement(PyObject* args)
{
	int local = 0;
	PyObject* pyvect;

	if (PyArg_ParseTuple(args, "O|i:applyMovement", &pyvect, &local)) {
		MT_Vector3 movement;
		if (PyVecTo(pyvect, movement)) {
			ApplyMovement(movement, (local!=0));
			Py_RETURN_NONE;
		}
	}
	return NULL;
}

PyObject* KX_GameObject::PyGetLinearVelocity(PyObject* args)
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

PyObject* KX_GameObject::PySetLinearVelocity(PyObject* args)
{
	int local = 0;
	PyObject* pyvect;
	
	if (PyArg_ParseTuple(args,"O|i:setLinearVelocity",&pyvect,&local)) {
		MT_Vector3 velocity;
		if (PyVecTo(pyvect, velocity)) {
			setLinearVelocity(velocity, (local!=0));
			Py_RETURN_NONE;
		}
	}
	return NULL;
}

PyObject* KX_GameObject::PyGetAngularVelocity(PyObject* args)
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

PyObject* KX_GameObject::PySetAngularVelocity(PyObject* args)
{
	int local = 0;
	PyObject* pyvect;
	
	if (PyArg_ParseTuple(args,"O|i:setAngularVelocity",&pyvect,&local)) {
		MT_Vector3 velocity;
		if (PyVecTo(pyvect, velocity)) {
			setAngularVelocity(velocity, (local!=0));
			Py_RETURN_NONE;
		}
	}
	return NULL;
}

PyObject* KX_GameObject::PySetVisible(PyObject* args)
{
	int visible, recursive = 0;
	if (!PyArg_ParseTuple(args,"i|i:setVisible",&visible, &recursive))
		return NULL;
	
	SetVisible(visible ? true:false, recursive ? true:false);
	UpdateBuckets(recursive ? true:false);
	Py_RETURN_NONE;
	
}

PyObject* KX_GameObject::PySetOcclusion(PyObject* args)
{
	int occlusion, recursive = 0;
	if (!PyArg_ParseTuple(args,"i|i:setOcclusion",&occlusion, &recursive))
		return NULL;
	
	SetOccluder(occlusion ? true:false, recursive ? true:false);
	Py_RETURN_NONE;
}

PyObject* KX_GameObject::PyGetVisible()
{
	ShowDeprecationWarning("getVisible()", "the visible property");
	return PyInt_FromLong(m_bVisible);	
}

PyObject* KX_GameObject::PyGetState()
{
	ShowDeprecationWarning("getState()", "the state property");
	int state = 0;
	state |= GetState();
	return PyInt_FromLong(state);
}

PyObject* KX_GameObject::PySetState(PyObject* value)
{
	ShowDeprecationWarning("setState()", "the state property");
	int state_i = PyInt_AsLong(value);
	unsigned int state = 0;
	
	if (state_i == -1 && PyErr_Occurred()) {
		PyErr_SetString(PyExc_TypeError, "expected an int bit field");
		return NULL;
	}
	
	state |= state_i;
	if ((state & ((1<<30)-1)) == 0) {
		PyErr_SetString(PyExc_AttributeError, "The state bitfield was not between 0 and 30 (1<<0 and 1<<29)");
		return NULL;
	}
	SetState(state);
	
	Py_RETURN_NONE;
}

PyObject* KX_GameObject::PyGetVelocity(PyObject* args)
{
	// only can get the velocity if we have a physics object connected to us...
	MT_Point3 point(0.0,0.0,0.0);
	PyObject* pypos = NULL;
	
	if (PyArg_ParseTuple(args, "|O:getVelocity", &pypos))
	{
		if (pypos)
			PyVecTo(pypos, point);
	}
	else {
		return NULL;
	}
	
	if (m_pPhysicsController1)
	{
		return PyObjectFrom(m_pPhysicsController1->GetVelocity(point));
	}
	else {
		return PyObjectFrom(MT_Vector3(0.0,0.0,0.0));
	}
}



PyObject* KX_GameObject::PyGetMass()
{
	ShowDeprecationWarning("getMass()", "the mass property");
	return PyFloat_FromDouble((GetPhysicsController() != NULL) ? GetPhysicsController()->GetMass() : 0.0f);
}

PyObject* KX_GameObject::PyGetReactionForce()
{
	// only can get the velocity if we have a physics object connected to us...
	
	// XXX - Currently not working with bullet intergration, see KX_BulletPhysicsController.cpp's getReactionForce
	/*
	if (GetPhysicsController())
		return PyObjectFrom(GetPhysicsController()->getReactionForce());
	return PyObjectFrom(dummy_point);
	*/
	
	return Py_BuildValue("fff", 0.0f, 0.0f, 0.0f);
	
}



PyObject* KX_GameObject::PyEnableRigidBody()
{
	if(GetPhysicsController())
		GetPhysicsController()->setRigidBody(true);

	Py_RETURN_NONE;
}



PyObject* KX_GameObject::PyDisableRigidBody()
{
	if(GetPhysicsController())
		GetPhysicsController()->setRigidBody(false);

	Py_RETURN_NONE;
}



PyObject* KX_GameObject::PyGetParent()
{
	ShowDeprecationWarning("getParent()", "the parent property");
	KX_GameObject* parent = this->GetParent();
	if (parent)
		return parent->GetProxy();
	Py_RETURN_NONE;
}

PyObject* KX_GameObject::PySetParent(PyObject* value)
{
	KX_Scene *scene = KX_GetActiveScene();
	KX_GameObject *obj;
	
	if (!ConvertPythonToGameObject(value, &obj, false, "gameOb.setParent(value): KX_GameObject"))
		return NULL;
	
	this->SetParent(scene, obj);
	Py_RETURN_NONE;
}

PyObject* KX_GameObject::PyRemoveParent()
{
	KX_Scene *scene = KX_GetActiveScene();
	
	this->RemoveParent(scene);
	Py_RETURN_NONE;
}

PyObject* KX_GameObject::PyGetChildren()
{
	return GetChildren()->NewProxy(true);
}

PyObject* KX_GameObject::PyGetChildrenRecursive()
{
	return GetChildrenRecursive()->NewProxy(true);
}

PyObject* KX_GameObject::PyGetMesh(PyObject* args)
{
	ShowDeprecationWarning("getMesh()", "the meshes property");
	
	int mesh = 0;

	if (!PyArg_ParseTuple(args, "|i:getMesh", &mesh))
		return NULL; // python sets a simple error
	
	if (((unsigned int)mesh < m_meshes.size()) && mesh >= 0)
	{
		KX_MeshProxy* meshproxy = new KX_MeshProxy(m_meshes[mesh]);
		return meshproxy->NewProxy(true); // XXX Todo Python own.
	}
	
	Py_RETURN_NONE;
}





PyObject* KX_GameObject::PySetCollisionMargin(PyObject* value)
{
	float collisionMargin = PyFloat_AsDouble(value);
	
	if (collisionMargin==-1 && PyErr_Occurred()) {
		PyErr_SetString(PyExc_TypeError, "expected a float");
		return NULL;
	}
	
	if (m_pPhysicsController1)
	{
		m_pPhysicsController1->setMargin(collisionMargin);
		Py_RETURN_NONE;
	}
	PyErr_SetString(PyExc_RuntimeError, "This object has no physics controller");
	return NULL;
}



PyObject* KX_GameObject::PyApplyImpulse(PyObject* args)
{
	PyObject* pyattach;
	PyObject* pyimpulse;
	
	if (!m_pPhysicsController1)	{
		PyErr_SetString(PyExc_RuntimeError, "This object has no physics controller");
		return NULL;
	}
	
	if (PyArg_ParseTuple(args, "OO:applyImpulse", &pyattach, &pyimpulse))
	{
		MT_Point3  attach;
		MT_Vector3 impulse;
		if (PyVecTo(pyattach, attach) && PyVecTo(pyimpulse, impulse))
		{
			m_pPhysicsController1->applyImpulse(attach, impulse);
			Py_RETURN_NONE;
		}

	}
	
	return NULL;
}



PyObject* KX_GameObject::PySuspendDynamics()
{
	SuspendDynamics();
	Py_RETURN_NONE;
}



PyObject* KX_GameObject::PyRestoreDynamics()
{
	RestoreDynamics();
	Py_RETURN_NONE;
}



PyObject* KX_GameObject::PyGetOrientation() //keywords
{
	ShowDeprecationWarning("getOrientation()", "the orientation property");
	return PyObjectFrom(NodeGetWorldOrientation());
}



PyObject* KX_GameObject::PySetOrientation(PyObject* value)
{
	ShowDeprecationWarning("setOrientation()", "the orientation property");
	MT_Matrix3x3 rot;
	
	/* if value is not a sequence PyOrientationTo makes an error */
	if (!PyOrientationTo(value, rot, "gameOb.setOrientation(sequence): KX_GameObject, "))
		return NULL;

	NodeSetLocalOrientation(rot);
	NodeUpdateGS(0.f);
	Py_RETURN_NONE;
}

PyObject* KX_GameObject::PyAlignAxisToVect(PyObject* args)
{
	PyObject* pyvect;
	int axis = 2; //z axis is the default
	float fac = 1.0;
	
	if (PyArg_ParseTuple(args,"O|if:alignAxisToVect",&pyvect,&axis, &fac))
	{
		MT_Vector3 vect;
		if (PyVecTo(pyvect, vect))
		{
			if (fac<=0.0) Py_RETURN_NONE; // Nothing to do.
			if (fac> 1.0) fac= 1.0;
			
			AlignAxisToVect(vect,axis,fac);
			NodeUpdateGS(0.f);
			Py_RETURN_NONE;
		}
	}
	return NULL;
}

PyObject* KX_GameObject::PyGetAxisVect(PyObject* value)
{
	MT_Vector3 vect;
	if (PyVecTo(value, vect))
	{
		return PyObjectFrom(NodeGetWorldOrientation() * vect);
	}
	return NULL;
}

PyObject* KX_GameObject::PySetPosition(PyObject* value)
{
	ShowDeprecationWarning("setPosition()", "the position property");
	MT_Point3 pos;
	if (PyVecTo(value, pos))
	{
		NodeSetLocalPosition(pos);
		NodeUpdateGS(0.f);
		Py_RETURN_NONE;
	}

	return NULL;
}

PyObject* KX_GameObject::PySetWorldPosition(PyObject* value)
{
	ShowDeprecationWarning("setWorldPosition()", "the worldPosition property");
	MT_Point3 pos;
	if (PyVecTo(value, pos))
	{
		NodeSetWorldPosition(pos);
		NodeUpdateGS(0.f);
		Py_RETURN_NONE;
	}

	return NULL;
}

PyObject* KX_GameObject::PyGetPhysicsId()
{
	KX_IPhysicsController* ctrl = GetPhysicsController();
	uint_ptr physid=0;
	if (ctrl)
	{
		physid= (uint_ptr)ctrl->GetUserData();
	}
	return PyInt_FromLong((long)physid);
}

PyObject* KX_GameObject::PyGetPropertyNames()
{
	PyObject *list=  ConvertKeysToPython();
	
	if(m_attr_dict) {
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
	
	KX_GameObject *other;
	if (ConvertPythonToGameObject(value, &other, false, "gameOb.getDistanceTo(value): KX_GameObject"))
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

	PyObject *returnValue;

	if (!PyVecTo(value, toPoint))
	{
		PyErr_Clear();
		
		KX_GameObject *other;
		if (ConvertPythonToGameObject(value, &other, false, "")) /* error will be overwritten */
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
		locToDir = toDir = MT_Vector3(0.0,0.0,0.0);
		distance = 0.0;
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

bool KX_GameObject::RayHit(KX_ClientObjectInfo* client, KX_RayCast* result, void * const data)
{
	KX_GameObject* hitKXObj = client->m_gameobject;
	
	// if X-ray option is selected, the unwnted objects were not tested, so get here only with true hit
	// if not, all objects were tested and the front one may not be the correct one.
	if (m_xray || m_testPropName.Length() == 0 || hitKXObj->GetProperty(m_testPropName) != NULL)
	{
		m_pHitObject = hitKXObj;
		return true;
	}
	// return true to stop RayCast::RayTest from looping, the above test was decisive
	// We would want to loop only if we want to get more than one hit point
	return true;
}

/* this function is used to pre-filter the object before casting the ray on them.
   This is useful for "X-Ray" option when we want to see "through" unwanted object.
 */
bool KX_GameObject::NeedRayCast(KX_ClientObjectInfo* client)
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
	if (!m_xray || m_testPropName.Length() == 0 || hitKXObj->GetProperty(m_testPropName) != NULL)
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
	PyObject* pyarg;
	float dist = 0.0f;
	char *propName = NULL;

	if (!PyArg_ParseTuple(args,"O|fs:rayCastTo", &pyarg, &dist, &propName)) {
		return NULL; // python sets simple error
	}

	if (!PyVecTo(pyarg, toPoint))
	{
		KX_GameObject *other;
		PyErr_Clear();
		
		if (ConvertPythonToGameObject(pyarg, &other, false, "")) /* error will be overwritten */
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
	{
		MT_Vector3 toDir = toPoint-fromPoint;
		toDir.normalize();
		toPoint = fromPoint + (dist) * toDir;
	}

	PHY_IPhysicsEnvironment* pe = GetPhysicsEnvironment();
	KX_IPhysicsController *spc = GetPhysicsController();
	KX_GameObject *parent = GetParent();
	if (!spc && parent)
		spc = parent->GetPhysicsController();
	if (parent)
		parent->Release();
	
	m_pHitObject = NULL;
	if (propName)
		m_testPropName = propName;
	else
		m_testPropName.SetLength(0);
	KX_RayCast::Callback<KX_GameObject> callback(this,spc);
	KX_RayCast::RayTest(pe, fromPoint, toPoint, callback);

    if (m_pHitObject)
		return m_pHitObject->GetProxy();
	
	Py_RETURN_NONE;
}

KX_PYMETHODDEF_DOC(KX_GameObject, rayCast,
				   "rayCast(to,from,dist,prop,face,xray,poly): cast a ray and return 3-tuple (object,hit,normal) or 4-tuple (object,hit,normal,polygon) of contact point with object within dist that matches prop.\n"
				   " If no hit, return (None,None,None) or (None,None,None,None).\n"
" to   = 3-tuple or object reference for destination of ray (if object, use center of object)\n"
" from = 3-tuple or object reference for origin of ray (if object, use center of object)\n"
"        Can be None or omitted => start from self object center\n"
" dist = max distance to look (can be negative => look behind); 0 or omitted => detect up to to\n"
" prop = property name that object must have; can be omitted => detect any object\n"
" face = normal option: 1=>return face normal; 0 or omitted => normal is oriented towards origin\n"
" xray = X-ray option: 1=>skip objects that don't match prop; 0 or omitted => stop on first object\n"
" poly = polygon option: 1=>return value is a 4-tuple and the 4th element is a KX_PolyProxy object\n"
"                           which can be None if hit object has no mesh or if there is no hit\n"
"        If 0 or omitted, return value is a 3-tuple\n"
"Note: The object on which you call this method matters: the ray will ignore it.\n"
"      prop and xray option interact as follow:\n"
"        prop off, xray off: return closest hit or no hit if there is no object on the full extend of the ray\n"
"        prop off, xray on : idem\n"
"        prop on,  xray off: return closest hit if it matches prop, no hit otherwise\n"
"        prop on,  xray on : return closest hit matching prop or no hit if there is no object matching prop on the full extend of the ray\n")
{
	MT_Point3 toPoint;
	MT_Point3 fromPoint;
	PyObject* pyto;
	PyObject* pyfrom = NULL;
	float dist = 0.0f;
	char *propName = NULL;
	KX_GameObject *other;
	int face=0, xray=0, poly=0;

	if (!PyArg_ParseTuple(args,"O|Ofsiii:rayCast", &pyto, &pyfrom, &dist, &propName, &face, &xray, &poly)) {
		return NULL; // Python sets a simple error
	}

	if (!PyVecTo(pyto, toPoint))
	{
		PyErr_Clear();
		
		if (ConvertPythonToGameObject(pyto, &other, false, ""))  /* error will be overwritten */
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
		
		if (ConvertPythonToGameObject(pyfrom, &other, false, "")) /* error will be overwritten */
		{
			fromPoint = other->NodeGetWorldPosition();
		} else
		{
			PyErr_SetString(PyExc_TypeError, "gameOb.rayCast(to,from,dist,prop,face,xray,poly): KX_GameObject, the second optional argument to rayCast must be a vector or a KX_GameObject");
			return NULL;
		}
	}
	
	if (dist != 0.0f) {
		MT_Vector3 toDir = toPoint-fromPoint;
		if (MT_fuzzyZero(toDir.length2())) {
			return Py_BuildValue("OOO", Py_None, Py_None, Py_None);
		}
		toDir.normalize();
		toPoint = fromPoint + (dist) * toDir;
	} else if (MT_fuzzyZero((toPoint-fromPoint).length2())) {
		return Py_BuildValue("OOO", Py_None, Py_None, Py_None);
	}
	
	PHY_IPhysicsEnvironment* pe = GetPhysicsEnvironment();
	KX_IPhysicsController *spc = GetPhysicsController();
	KX_GameObject *parent = GetParent();
	if (!spc && parent)
		spc = parent->GetPhysicsController();
	if (parent)
		parent->Release();
	
	m_pHitObject = NULL;
	if (propName)
		m_testPropName = propName;
	else
		m_testPropName.SetLength(0);
	m_xray = xray;
	// to get the hit results
	KX_RayCast::Callback<KX_GameObject> callback(this,spc,NULL,face);
	KX_RayCast::RayTest(pe, fromPoint, toPoint, callback);

	if (m_pHitObject)
	{
		PyObject* returnValue = (poly) ? PyTuple_New(4) : PyTuple_New(3);
		if (returnValue) { // unlikely this would ever fail, if it does python sets an error
			PyTuple_SET_ITEM(returnValue, 0, m_pHitObject->GetProxy());
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
				}
				else
				{
					Py_INCREF(Py_None);
					PyTuple_SET_ITEM(returnValue, 3, Py_None);
				}
			}
		}
		return returnValue;
	}
	// no hit
	if (poly)
		return Py_BuildValue("OOOO", Py_None, Py_None, Py_None, Py_None);
	else
		return Py_BuildValue("OOO", Py_None, Py_None, Py_None);
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

	if (!PyArg_ParseTuple(args, "s|sss:sendMessage", &subject, &body, &to))
		return NULL;
	
	scene->GetNetworkScene()->SendMessage(to, from, subject, body);
	Py_RETURN_NONE;
}

/* --------------------------------------------------------------------- 
 * Some stuff taken from the header
 * --------------------------------------------------------------------- */
void KX_GameObject::Relink(GEN_Map<GEN_HashedPtr, void*> *map_parameter)	
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

bool ConvertPythonToGameObject(PyObject * value, KX_GameObject **object, bool py_none_ok, const char *error_prefix)
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
	
	if (PyString_Check(value)) {
		*object = (KX_GameObject*)SCA_ILogicBrick::m_sCurrentLogicManager->GetGameObjectByName(STR_String( PyString_AsString(value) ));
		
		if (*object) {
			return true;
		} else {
			PyErr_Format(PyExc_ValueError, "%s, requested name \"%s\" did not match any KX_GameObject in this scene", error_prefix, PyString_AsString(value));
			return false;
		}
	}
	
	if (PyObject_TypeCheck(value, &KX_GameObject::Type)) {
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
