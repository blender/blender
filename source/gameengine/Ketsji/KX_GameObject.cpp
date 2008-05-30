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
#include <stdio.h> // printf
#include "SG_Controller.h"
#include "KX_IPhysicsController.h"
#include "SG_Node.h"
#include "SG_Controller.h"
#include "KX_ClientObjectInfo.h"
#include "RAS_BucketManager.h"
#include "KX_RayCast.h"
#include "KX_PythonInit.h"
#include "KX_PyMath.h"

// This file defines relationships between parents and children
// in the game engine.

#include "KX_SG_NodeRelationships.h"

KX_GameObject::KX_GameObject(
	void* sgReplicationInfo,
	SG_Callbacks callbacks,
	PyTypeObject* T
) : 
	SCA_IObject(T),
	m_bDyna(false),
	m_layer(0),
	m_bSuspendDynamics(false),
	m_bUseObjectColor(false),
	m_bIsNegativeScaling(false),
	m_bVisible(true),
	m_pPhysicsController1(NULL),
	m_pPhysicsEnvironment(NULL),
	m_isDeformable(false),
	m_pHitObject(NULL)
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



float KX_GameObject::GetNumber()
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



void KX_GameObject::ReplicaSetName(STR_String name)
{
}






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
	if (obj && GetSGNode()->GetSGParent() != obj->GetSGNode())
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

		// Set us to our new scale, position, and orientation
		scale1[0] = scale1[0]/scale2[0];
		scale1[1] = scale1[1]/scale2[1];
		scale1[2] = scale1[2]/scale2[2];
		MT_Matrix3x3 invori = obj->NodeGetWorldOrientation().inverse();
		MT_Vector3 newpos = invori*(NodeGetWorldPosition()-obj->NodeGetWorldPosition())*scale1;

		NodeSetLocalScale(scale1);
		NodeSetLocalPosition(MT_Point3(newpos[0],newpos[1],newpos[2]));
		NodeSetLocalOrientation(invori*NodeGetWorldOrientation());
		NodeUpdateGS(0.f,true);
		// object will now be a child, it must be removed from the parent list
		CListValue* rootlist = scene->GetRootParentList();
		if (rootlist->RemoveValue(this))
			// the object was in parent list, decrement ref count as it's now removed
			Release();
	}
}

void KX_GameObject::RemoveParent(KX_Scene *scene)
{
	if (GetSGNode()->GetSGParent())
	{
		// Set us to the right spot 
		GetSGNode()->SetLocalScale(GetSGNode()->GetWorldScaling());
		GetSGNode()->SetLocalOrientation(GetSGNode()->GetWorldOrientation());
		GetSGNode()->SetLocalPosition(GetSGNode()->GetWorldPosition());

		// Remove us from our parent
		GetSGNode()->DisconnectFromParent();
		NodeUpdateGS(0.f,true);
		// the object is now a root object, add it to the parentlist
		CListValue* rootlist = scene->GetRootParentList();
		if (!rootlist->SearchValue(this))
			// object was not in root list, add it now and increment ref count
			rootlist->Add(AddRef());
	}
}

void KX_GameObject::ProcessReplica(KX_GameObject* replica)
{
	replica->m_pPhysicsController1 = NULL;
	replica->m_pSGNode = NULL;
	replica->m_pClient_info = new KX_ClientObjectInfo(*m_pClient_info);
	replica->m_pClient_info->m_gameobject = replica;
}



CValue* KX_GameObject::GetReplica()
{
	KX_GameObject* replica = new KX_GameObject(*this);
	
	// this will copy properties and so on...
	CValue::AddDataToReplica(replica);
	ProcessReplica(replica);
	
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
	if (m_pPhysicsController1) // (IsDynamic())
	{
		m_pPhysicsController1->RelativeTranslate(dloc,local);
	}
	GetSGNode()->RelativeTranslate(dloc,GetSGNode()->GetSGParent(),local);
}



void KX_GameObject::ApplyRotation(const MT_Vector3& drot,bool local)
{
	MT_Matrix3x3 rotmat(drot);

	GetSGNode()->RelativeRotate(rotmat,local);

	if (m_pPhysicsController1) { // (IsDynamic())
		m_pPhysicsController1->RelativeRotate(rotmat,local); 
	}
}



/**
GetOpenGL Matrix, returns an OpenGL 'compatible' matrix
*/
double*	KX_GameObject::GetOpenGLMatrix()
{
	// todo: optimize and only update if necessary
	double* fl = m_OpenGL_4x4Matrix.getPointer();
	MT_Transform trans;
	
	trans.setOrigin(GetSGNode()->GetWorldPosition());
	trans.setBasis(GetSGNode()->GetWorldOrientation());
	
	MT_Vector3 scaling = GetSGNode()->GetWorldScaling();
	m_bIsNegativeScaling = ((scaling[0] < 0.0) ^ (scaling[1] < 0.0) ^ (scaling[2] < 0.0)) ? true : false;
	trans.scale(scaling[0], scaling[1], scaling[2]);
	trans.getValue(fl);

	return fl;
}



void KX_GameObject::Bucketize()
{
	double* fl = GetOpenGLMatrix();

	for (size_t i=0;i<m_meshes.size();i++)
		m_meshes[i]->Bucketize(fl, this, m_bUseObjectColor, m_objectColor);
}



void KX_GameObject::RemoveMeshes()
{
	double* fl = GetOpenGLMatrix();

	for (size_t i=0;i<m_meshes.size();i++)
		m_meshes[i]->RemoveFromBuckets(fl, this);

	//note: meshes can be shared, and are deleted by KX_BlenderSceneConverter

	m_meshes.clear();
}



void KX_GameObject::UpdateNonDynas()
{
	if (m_pPhysicsController1)
	{
		m_pPhysicsController1->SetSumoTransform(true);
	}
}



void KX_GameObject::UpdateTransform()
{
	if (m_pPhysicsController1)
		m_pPhysicsController1->SetSumoTransform(false);
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



void KX_GameObject::UpdateIPO(float curframetime,
							  bool recurse,
							  bool ipo_as_force,
							  bool force_local) 
{

	// The ipo-actuator needs a sumo reference... this is retrieved (unfortunately)
	// by the iposgcontr itself...
//		ipocontr->SetSumoReference(gameobj->GetSumoScene(), 
//								   gameobj->GetSumoObject());


	// The ipo has to be treated as a force, and not a displacement!
	// For this case, we send some settings to the controller. This
	// may need some caching...
	if (ipo_as_force) {
		SGControllerList::iterator it = GetSGNode()->GetSGControllerList().begin();

		while (it != GetSGNode()->GetSGControllerList().end()) {
			(*it)->SetOption(SG_Controller::SG_CONTR_IPO_IPO_AS_FORCE, ipo_as_force);
			(*it)->SetOption(SG_Controller::SG_CONTR_IPO_FORCES_ACT_LOCAL, force_local);
			it++;
		}
	} 

	// The rest is the 'normal' update procedure.
	GetSGNode()->SetSimulatedTime(curframetime,recurse);
	GetSGNode()->UpdateWorldData(curframetime);
	UpdateTransform();
}

// IPO update
void 
KX_GameObject::UpdateMaterialData(
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
		RAS_MaterialBucket::Set::iterator mit = m_meshes[mesh]->GetFirstMaterial();
		for(; mit != m_meshes[mesh]->GetLastMaterial(); ++mit)
		{
			RAS_IPolyMaterial* poly = (*mit)->GetPolyMaterial();
			if(poly->GetFlag() & RAS_BLENDERMAT )
			{
				SetObjectColor(rgba);
				KX_BlenderMaterial *m =  static_cast<KX_BlenderMaterial*>(poly);
				m->UpdateIPO(rgba, specrgb,hard,spec,ref,emit, alpha);
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

void
KX_GameObject::SetVisible(
	bool v
	)
{
	m_bVisible = v;
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

// used by Python, and the actuatorshould _not_ be misused by the
// scene!
void 
KX_GameObject::MarkVisible(
	bool visible
	)
{
	/* If explicit visibility settings are used, this is
	 * determined on this level. Maybe change this to mesh level
	 * later on? */
	
	double* fl = GetOpenGLMatrixPtr()->getPointer();
	for (size_t i=0;i<m_meshes.size();i++)
	{
		m_meshes[i]->MarkVisible(fl,this,visible,m_bUseObjectColor,m_objectColor);
	}
}


// Always use the flag?
void 
KX_GameObject::MarkVisible(
	void
	)
{
	double* fl = GetOpenGLMatrixPtr()->getPointer();
	for (size_t i=0;i<m_meshes.size();i++)
	{
		m_meshes[i]->MarkVisible(fl,
					 this,
					 m_bVisible,
					 m_bUseObjectColor,
					 m_objectColor
			);
	}
}


void KX_GameObject::addLinearVelocity(const MT_Vector3& lin_vel,bool local)
{
	if (m_pPhysicsController1)
		m_pPhysicsController1->SetLinearVelocity(lin_vel + m_pPhysicsController1->GetLinearVelocity(),local);
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



MT_Vector3 KX_GameObject::GetLinearVelocity(bool local)
{
	MT_Vector3 velocity(0.0,0.0,0.0);
	MT_Matrix3x3 ori, locvel;
	int i, j; 
	if (m_pPhysicsController1)
	{
		velocity = m_pPhysicsController1->GetLinearVelocity();
		
		if (local)
		{
			ori = GetSGNode()->GetWorldOrientation();
			
			for(i=0; i < 3; i++)
				for(j=0; j < 3; j++)
					locvel[i][j]= velocity[i]*ori[i][j];
			for(i=0; i < 3; i++)
				velocity[i] = locvel[0][i] + locvel[1][i] + locvel[2][i];
		}
	}
	return velocity;	
}


// scenegraph node stuff

void KX_GameObject::NodeSetLocalPosition(const MT_Point3& trans)
{
	if (m_pPhysicsController1)
	{
		m_pPhysicsController1->setPosition(trans);
	}

	if (GetSGNode())
		GetSGNode()->SetLocalPosition(trans);
}



void KX_GameObject::NodeSetLocalOrientation(const MT_Matrix3x3& rot)
{
	if (m_pPhysicsController1)
	{
		m_pPhysicsController1->setOrientation(rot.getRotation());
	}
	if (GetSGNode())
		GetSGNode()->SetLocalOrientation(rot);
	else
	{
		int i;
		i=0;
	}
}



void KX_GameObject::NodeSetLocalScale(const MT_Vector3& scale)
{
	if (m_pPhysicsController1)
	{
		m_pPhysicsController1->setScaling(scale);
	}
	
	if (GetSGNode())
		GetSGNode()->SetLocalScale(scale);
}



void KX_GameObject::NodeSetRelativeScale(const MT_Vector3& scale)
{
	if (GetSGNode())
		GetSGNode()->RelativeScale(scale);
}



void KX_GameObject::NodeUpdateGS(double time,bool bInitiator)
{
	if (GetSGNode())
		GetSGNode()->UpdateWorldData(time);
}



const MT_Matrix3x3& KX_GameObject::NodeGetWorldOrientation() const
{
	return GetSGNode()->GetWorldOrientation();
}



const MT_Vector3& KX_GameObject::NodeGetWorldScaling() const
{
	return GetSGNode()->GetWorldScaling();
}



const MT_Point3& KX_GameObject::NodeGetWorldPosition() const
{
	return GetSGNode()->GetWorldPosition();
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
		GetPhysicsController()->RestoreDynamics();

		m_suspended = false;
	}
}

void KX_GameObject::Suspend(void)
{
	if ((!m_ignore_activity_culling) 
		&& (!m_suspended))  {
		SCA_IObject::Suspend();
		GetPhysicsController()->SuspendDynamics();
		m_suspended = true;
	}
}




/* ------- python stuff ---------------------------------------------------*/




PyMethodDef KX_GameObject::Methods[] = {
	{"setVisible",(PyCFunction) KX_GameObject::sPySetVisible, METH_VARARGS},  
	{"setPosition", (PyCFunction) KX_GameObject::sPySetPosition, METH_VARARGS},
	{"getPosition", (PyCFunction) KX_GameObject::sPyGetPosition, METH_VARARGS},
	{"getOrientation", (PyCFunction) KX_GameObject::sPyGetOrientation, METH_VARARGS},
	{"setOrientation", (PyCFunction) KX_GameObject::sPySetOrientation, METH_VARARGS},
	{"getLinearVelocity", (PyCFunction) KX_GameObject::sPyGetLinearVelocity, METH_VARARGS},
	{"getVelocity", (PyCFunction) KX_GameObject::sPyGetVelocity, METH_VARARGS},
	{"getMass", (PyCFunction) KX_GameObject::sPyGetMass, METH_VARARGS},
	{"getReactionForce", (PyCFunction) KX_GameObject::sPyGetReactionForce, METH_VARARGS},
	{"applyImpulse", (PyCFunction) KX_GameObject::sPyApplyImpulse, METH_VARARGS},
	{"setCollisionMargin", (PyCFunction) KX_GameObject::sPySetCollisionMargin, METH_VARARGS},
	{"suspendDynamics", (PyCFunction)KX_GameObject::sPySuspendDynamics,METH_VARARGS},
	{"restoreDynamics", (PyCFunction)KX_GameObject::sPyRestoreDynamics,METH_VARARGS},
	{"enableRigidBody", (PyCFunction)KX_GameObject::sPyEnableRigidBody,METH_VARARGS},
	{"disableRigidBody", (PyCFunction)KX_GameObject::sPyDisableRigidBody,METH_VARARGS},
	{"getParent", (PyCFunction)KX_GameObject::sPyGetParent,METH_VARARGS},
	{"setParent", (PyCFunction)KX_GameObject::sPySetParent,METH_VARARGS},
	{"removeParent", (PyCFunction)KX_GameObject::sPyRemoveParent,METH_VARARGS},
	{"getMesh", (PyCFunction)KX_GameObject::sPyGetMesh,METH_VARARGS},
	{"getPhysicsId", (PyCFunction)KX_GameObject::sPyGetPhysicsId,METH_VARARGS},
	KX_PYMETHODTABLE(KX_GameObject, getDistanceTo),
	KX_PYMETHODTABLE(KX_GameObject, rayCastTo),
	KX_PYMETHODTABLE(KX_GameObject, rayCast),
	{NULL,NULL} //Sentinel
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


PyObject* KX_GameObject::sPySetPosition(PyObject* self,
										PyObject* args,
										PyObject* kwds)
{
	return ((KX_GameObject*) self)->PySetPosition(self, args, kwds);
}
	


PyObject* KX_GameObject::PyGetPosition(PyObject* self,
									   PyObject* args, 
									   PyObject* kwds)
{
	return PyObjectFrom(NodeGetWorldPosition());
}



PyTypeObject KX_GameObject::Type = {
	PyObject_HEAD_INIT(&PyType_Type)
		0,
		"KX_GameObject",
		sizeof(KX_GameObject),
		0,
		PyDestructor,
		0,
		__getattr,
		__setattr,
		0, //&MyPyCompare,
		__repr,
		0, //&cvalue_as_number,
		0,
		0,
		0,
		0
};



PyParentObject KX_GameObject::Parents[] = {
	&KX_GameObject::Type,
		&SCA_IObject::Type,
		&CValue::Type,
		NULL
};




PyObject* KX_GameObject::_getattr(const STR_String& attr)
{
	if (m_pPhysicsController1)
	{
		if (attr == "mass")
			return PyFloat_FromDouble(GetPhysicsController()->GetMass());
	}

	if (attr == "parent")
	{	
		KX_GameObject* parent = GetParent();
		if (parent)
		{
			parent->AddRef();
			return parent;
		}
		Py_Return;
	}

	if (attr == "visible")
		return PyInt_FromLong(m_bVisible);
	
	if (attr == "position")
		return PyObjectFrom(NodeGetWorldPosition());
	
	if (attr == "orientation")
		return PyObjectFrom(NodeGetWorldOrientation());
	
	if (attr == "scaling")
		return PyObjectFrom(NodeGetWorldScaling());
		
	if (attr == "name")
		return PyString_FromString(m_name.ReadPtr());
	
	_getattr_up(SCA_IObject);
}

int KX_GameObject::_setattr(const STR_String& attr, PyObject *value)	// _setattr method
{
	if (attr == "mass")
		return 1;
	
	if (attr == "parent")
		return 1;
		
	if (PyInt_Check(value))
	{
		int val = PyInt_AsLong(value);
		if (attr == "visible")
		{
			SetVisible(val != 0);
			return 0;
		}
	}
	
	if (PySequence_Check(value))
	{
		if (attr == "orientation")
		{
			MT_Matrix3x3 rot;
			if (PyObject_IsMT_Matrix(value, 3))
			{
				if (PyMatTo(value, rot))
				{
					NodeSetLocalOrientation(rot);
					NodeUpdateGS(0.f,true);
					return 0;
				}
				return 1;
			}
			
			if (PySequence_Size(value) == 4)
			{
				MT_Quaternion qrot;
				if (PyVecTo(value, qrot))
				{
					rot.setRotation(qrot);
					NodeSetLocalOrientation(rot);
					NodeUpdateGS(0.f,true);
					return 0;
				}
				return 1;
			}
			
			if (PySequence_Size(value) == 3)
			{
				MT_Vector3 erot;
				if (PyVecTo(value, erot))
				{
					rot.setEuler(erot);
					NodeSetLocalOrientation(rot);
					NodeUpdateGS(0.f,true);
					return 0;
				}
				return 1;
			}
			
			return 1;
		}
		
		if (attr == "position")
		{
			MT_Point3 pos;
			if (PyVecTo(value, pos))
			{
				NodeSetLocalPosition(pos);
				NodeUpdateGS(0.f,true);
				return 0;
			}
			return 1;
		}
		
		if (attr == "scaling")
		{
			MT_Vector3 scale;
			if (PyVecTo(value, scale))
			{
				NodeSetLocalScale(scale);
				NodeUpdateGS(0.f,true);
				return 0;
			}
			return 1;
		}
	}
	
	if (PyString_Check(value))
	{
		if (attr == "name")
		{
			m_name = PyString_AsString(value);
			return 0;
		}
	}
	
	/* Need to have parent settable here too */
	
	return SCA_IObject::_setattr(attr, value);
}


PyObject* KX_GameObject::PyGetLinearVelocity(PyObject* self, 
											 PyObject* args, 
											 PyObject* kwds)
{
	// only can get the velocity if we have a physics object connected to us...
	int local = 0;
	if (PyArg_ParseTuple(args,"|i",&local))
	{
		return PyObjectFrom(GetLinearVelocity((local!=0)));
	}
	else
	{
		return NULL;
	}
}



PyObject* KX_GameObject::PySetVisible(PyObject* self,
									  PyObject* args,
									  PyObject* kwds)
{
	int visible = 1;
	
	if (PyArg_ParseTuple(args,"i",&visible))
	{
		MarkVisible(visible!=0);
		m_bVisible = (visible!=0);
	}
	else
	{
		return NULL;	     
	}
	Py_Return;
	
}



PyObject* KX_GameObject::PyGetVelocity(PyObject* self, 
									   PyObject* args, 
									   PyObject* kwds)
{
	// only can get the velocity if we have a physics object connected to us...
	MT_Vector3 velocity(0.0,0.0,0.0);
	MT_Point3 point(0.0,0.0,0.0);
	
	
	PyObject* pypos = NULL;
	if (PyArg_ParseTuple(args, "|O", &pypos))
	{
		if (pypos)
			PyVecTo(pypos, point);
	}
	
	if (m_pPhysicsController1)
	{
		velocity = m_pPhysicsController1->GetVelocity(point);
	}
	
	return PyObjectFrom(velocity);
}



PyObject* KX_GameObject::PyGetMass(PyObject* self, 
								   PyObject* args, 
								   PyObject* kwds)
{
	PyObject* pymass = NULL;
	
	float mass = GetPhysicsController()->GetMass();
	pymass = PyFloat_FromDouble(mass);

	if (pymass)
		return pymass;
	
	Py_Return;
}



PyObject* KX_GameObject::PyGetReactionForce(PyObject* self, 
											PyObject* args, 
											PyObject* kwds)
{
	// only can get the velocity if we have a physics object connected to us...
	return PyObjectFrom(GetPhysicsController()->getReactionForce());
}



PyObject* KX_GameObject::PyEnableRigidBody(PyObject* self, 
										   PyObject* args, 
										   PyObject* kwds)
{
	
	GetPhysicsController()->setRigidBody(true);

	Py_Return;
}



PyObject* KX_GameObject::PyDisableRigidBody(PyObject* self, 
											PyObject* args, 
											PyObject* kwds)
{
	GetPhysicsController()->setRigidBody(false);

	Py_Return;
}



PyObject* KX_GameObject::PyGetParent(PyObject* self, 
									 PyObject* args, 
									 PyObject* kwds)
{
	KX_GameObject* parent = this->GetParent();
	if (parent)
	{
		parent->AddRef();
		return parent;
	}
	Py_Return;
}

PyObject* KX_GameObject::PySetParent(PyObject* self, 
									 PyObject* args, 
									 PyObject* kwds)
{
	PyObject* gameobj;
	if (PyArg_ParseTuple(args, "O!", &KX_GameObject::Type, &gameobj))
	{
		// The object we want to set as parent
		CValue *m_ob = (CValue*)gameobj;
		KX_GameObject *obj = ((KX_GameObject*)m_ob);
		KX_Scene *scene = PHY_GetActiveScene();
		
		this->SetParent(scene, obj);
	}
	Py_Return;
}

PyObject* KX_GameObject::PyRemoveParent(PyObject* self, 
									 PyObject* args, 
									 PyObject* kwds)
{
	KX_Scene *scene = PHY_GetActiveScene();
	this->RemoveParent(scene);
	Py_Return;
}

PyObject* KX_GameObject::PyGetMesh(PyObject* self, 
								   PyObject* args, 
								   PyObject* kwds)
{
	int mesh = 0;

	if (PyArg_ParseTuple(args, "|i", &mesh))
	{
		if (((unsigned int)mesh < m_meshes.size()) && mesh >= 0)
		{
			KX_MeshProxy* meshproxy = new KX_MeshProxy(m_meshes[mesh]);
			return meshproxy;
		}
	}
	Py_Return;
}





PyObject* KX_GameObject::PySetCollisionMargin(PyObject* self, 
										PyObject* args, 
										PyObject* kwds)
{
	float collisionMargin;
	if (PyArg_ParseTuple(args, "f", &collisionMargin))
	{
		if (m_pPhysicsController1)
		{
			m_pPhysicsController1->setMargin(collisionMargin);
			Py_Return;
		}

	}
	
	return NULL;
}



PyObject* KX_GameObject::PyApplyImpulse(PyObject* self, 
										PyObject* args, 
										PyObject* kwds)
{
	PyObject* pyattach;
	PyObject* pyimpulse;
	if (PyArg_ParseTuple(args, "OO", &pyattach, &pyimpulse))
	{
		MT_Point3  attach;
		MT_Vector3 impulse;
		if (m_pPhysicsController1)
		{
			if (PyVecTo(pyattach, attach) && PyVecTo(pyimpulse, impulse))
			{
				m_pPhysicsController1->applyImpulse(attach, impulse);
				Py_Return;
			}
		}

	}
	
	return NULL;
}



PyObject* KX_GameObject::PySuspendDynamics(PyObject* self, 
										   PyObject* args, 
										   PyObject* kwds)
{
	if (m_bSuspendDynamics)
	{
		Py_Return;
	}
	
	if (m_pPhysicsController1)
	{
		m_pPhysicsController1->SuspendDynamics();
	}
	m_bSuspendDynamics = true;
	
	Py_Return;
}



PyObject* KX_GameObject::PyRestoreDynamics(PyObject* self, 
										   PyObject* args, 
										   PyObject* kwds)
{
	
	if (!m_bSuspendDynamics)
	{
		Py_Return;
	}
	
	if (m_pPhysicsController1)
	{
		m_pPhysicsController1->RestoreDynamics();
	}
	m_bSuspendDynamics = false;
	
	Py_Return;
}



PyObject* KX_GameObject::PyGetOrientation(PyObject* self,
										  PyObject* args,
										  PyObject* kwds) //keywords
{
	return PyObjectFrom(NodeGetWorldOrientation());
}



PyObject* KX_GameObject::PySetOrientation(PyObject* self, 
										  PyObject* args, 
										  PyObject* kwds)
{
	PyObject* pylist;
	
	if (PyArg_ParseTuple(args,"O",&pylist))
	{
		MT_Matrix3x3 matrix;
		if (PyObject_IsMT_Matrix(pylist, 3) && PyMatTo(pylist, matrix))
		{
			NodeSetLocalOrientation(matrix);
			NodeUpdateGS(0.f,true);
			Py_Return;
		}
	
		MT_Quaternion quat;
		if (PyVecTo(pylist, quat))
		{
			matrix.setRotation(quat);
			NodeSetLocalOrientation(matrix);
			NodeUpdateGS(0.f,true);
			Py_Return;
		}
	}
	return NULL;
}



PyObject* KX_GameObject::PySetPosition(PyObject* self, 
									   PyObject* args, 
									   PyObject* kwds)
{
	MT_Point3 pos;
	if (PyVecArgTo(args, pos))
	{
		NodeSetLocalPosition(pos);
		NodeUpdateGS(0.f,true);
		Py_Return;
	}
	
	return NULL;
}

PyObject* KX_GameObject::PyGetPhysicsId(PyObject* self,
											   PyObject* args,
											   PyObject* kwds)
{
	KX_IPhysicsController* ctrl = GetPhysicsController();
	uint_ptr physid=0;
	if (ctrl)
	{
		physid= (uint_ptr)ctrl->GetUserData();
	}
	return PyInt_FromLong((long)physid);
}

KX_PYMETHODDEF_DOC(KX_GameObject, getDistanceTo,
"getDistanceTo(other): get distance to another point/KX_GameObject")
{
	MT_Point3 b;
	if (PyVecArgTo(args, b))
	{
		return PyFloat_FromDouble(NodeGetWorldPosition().distance(b));
	}
	PyErr_Clear();
	
	PyObject *pyother;
	if (PyArg_ParseTuple(args, "O!", &KX_GameObject::Type, &pyother))
	{
		KX_GameObject *other = static_cast<KX_GameObject*>(pyother);
		return PyFloat_FromDouble(NodeGetWorldPosition().distance(other->NodeGetWorldPosition()));
	}
	
	return NULL;
}

bool KX_GameObject::RayHit(KX_ClientObjectInfo* client, MT_Point3& hit_point, MT_Vector3& hit_normal, void * const data)
{

	KX_GameObject* hitKXObj = client->m_gameobject;
	
	if (client->m_type > KX_ClientObjectInfo::ACTOR)
	{
		// false hit
		return false;
	}

	if (m_testPropName.Length() == 0 || hitKXObj->GetProperty(m_testPropName) != NULL)
	{
		m_pHitObject = hitKXObj;
		return true;
	}

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

	if (!PyArg_ParseTuple(args,"O|fs", &pyarg, &dist, &propName))
		return NULL;

	if (!PyVecTo(pyarg, toPoint))
	{
		KX_GameObject *other;
		PyErr_Clear();
		if (!PyType_IsSubtype(pyarg->ob_type, &KX_GameObject::Type))
			return NULL;
		other = static_cast<KX_GameObject*>(pyarg);
		toPoint = other->NodeGetWorldPosition();
	}
	MT_Point3 fromPoint = NodeGetWorldPosition();
	if (dist != 0.0f)
	{
		MT_Vector3 toDir = toPoint-fromPoint;
		toDir.normalize();
		toPoint = fromPoint + (dist) * toDir;
	}

	MT_Point3 resultPoint;
	MT_Vector3 resultNormal;
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
	KX_RayCast::RayTest(spc, pe, fromPoint, toPoint, resultPoint, resultNormal, KX_RayCast::Callback<KX_GameObject>(this));

    if (m_pHitObject)
	{
		m_pHitObject->AddRef();
		return m_pHitObject;
	}
	Py_Return;
}

KX_PYMETHODDEF_DOC(KX_GameObject, rayCast,
"rayCast(to,from,dist,prop): cast a ray and return tuple (object,hit,normal) of contact point with object within dist that matches prop or None if no hit\n"
" prop = property name that object must have; can be omitted => detect any object\n"
" dist = max distance to look (can be negative => look behind); 0 or omitted => detect up to to\n"
" from = 3-tuple or object reference for origin of ray (if object, use center of object)\n"
"        Can None or omitted => start from self object center\n"
" to = 3-tuple or object reference for destination of ray (if object, use center of object)\n"
"Note: the object on which you call this method matters: the ray will ignore it if it goes through it\n")
{
	MT_Point3 toPoint;
	MT_Point3 fromPoint;
	PyObject* pyto;
	PyObject* pyfrom = NULL;
	float dist = 0.0f;
	char *propName = NULL;
	KX_GameObject *other;

	if (!PyArg_ParseTuple(args,"O|Ofs", &pyto, &pyfrom, &dist, &propName))
		return NULL;

	if (!PyVecTo(pyto, toPoint))
	{
		PyErr_Clear();
		if (!PyType_IsSubtype(pyto->ob_type, &KX_GameObject::Type))
			return NULL;
		other = static_cast<KX_GameObject*>(pyto);
		toPoint = other->NodeGetWorldPosition();
	}
	if (!pyfrom || pyfrom == Py_None)
	{
		fromPoint = NodeGetWorldPosition();
	}
	else if (!PyVecTo(pyfrom, fromPoint))
	{
		PyErr_Clear();
		if (!PyType_IsSubtype(pyfrom->ob_type, &KX_GameObject::Type))
			return NULL;
		other = static_cast<KX_GameObject*>(pyfrom);
		fromPoint = other->NodeGetWorldPosition();
	}

	if (dist != 0.0f)
	{
		MT_Vector3 toDir = toPoint-fromPoint;
		toDir.normalize();
		toPoint = fromPoint + (dist) * toDir;
	}

	MT_Point3 resultPoint;
	MT_Vector3 resultNormal;
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
	KX_RayCast::RayTest(spc, pe, fromPoint, toPoint, resultPoint, resultNormal, KX_RayCast::Callback<KX_GameObject>(this));

    if (m_pHitObject)
	{
		PyObject* returnValue = PyTuple_New(3);
		if (!returnValue)
			return NULL;
		PyTuple_SET_ITEM(returnValue, 0, m_pHitObject->AddRef());
		PyTuple_SET_ITEM(returnValue, 1, PyObjectFrom(resultPoint));
		PyTuple_SET_ITEM(returnValue, 2, PyObjectFrom(resultNormal));
		return returnValue;
		//return Py_BuildValue("(O,(fff),(fff))", 
		//	m_pHitObject->AddRef(),		// trick: KX_GameObject are not true Python object, they use a difference reference count system
		//	resultPoint[0], resultPoint[1], resultPoint[2],
		//	resultNormal[0], resultNormal[1], resultNormal[2]);
	}
	return Py_BuildValue("OOO", Py_None, Py_None, Py_None);
	//Py_Return;
}

/* --------------------------------------------------------------------- 
 * Some stuff taken from the header
 * --------------------------------------------------------------------- */
void KX_GameObject::Relink(GEN_Map<GEN_HashedPtr, void*> *map_parameter)	
{
	/* intentionally empty ? */
}

