/**
 * $Id$
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
 * Game object wrapper
 */

#ifdef WIN32
// This warning tells us about truncation of __long__ stl-generated names.
// It can occasionally cause DevStudio to have internal compiler warnings.
#pragma warning( disable : 4786 )     
#endif


#define KX_INERTIA_INFINITE 10000
#include "RAS_IPolygonMaterial.h"
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

// This file defines relationships between parents and children
// in the game engine.

#include "KX_SG_NodeRelationships.h"

KX_GameObject::KX_GameObject(
	void* sgReplicationInfo,
	SG_Callbacks callbacks,
	PyTypeObject* T
) : 
	SCA_IObject(T),
	m_bUseObjectColor(false),
	m_bDyna(false),
	m_bSuspendDynamics(false),
	m_pPhysicsController1(NULL),
	m_bVisible(true)
{
	m_ignore_activity_culling = false;
	m_pClient_info = new KX_ClientObjectInfo();
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



void KX_GameObject::ProcessReplica(KX_GameObject* replica)
{
	replica->m_pPhysicsController1 = NULL;
	replica->m_pSGNode = NULL;
	replica->m_pClient_info = new KX_ClientObjectInfo(*m_pClient_info);
	replica->m_pClient_info->m_clientobject = replica;
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
	if (this->IsDynamic()) 
	{
		m_pPhysicsController1->RelativeTranslate(dloc,local);
	}
	else
	{
		GetSGNode()->RelativeTranslate(dloc,GetSGNode()->GetSGParent(),local);
	}
}



void KX_GameObject::ApplyRotation(const MT_Vector3& drot,bool local)
{
	MT_Matrix3x3 rotmat(drot);
	
	if (this->IsDynamic()) //m_pPhysicsController)
		m_pPhysicsController1->RelativeRotate(rotmat.transposed(),local);
	else
		// in worldspace
		GetSGNode()->RelativeRotate(rotmat.transposed(),local);
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
	
	trans.scale(scaling[0], scaling[1], scaling[2]);
	trans.getValue(fl);

	return fl;
}



void KX_GameObject::Bucketize()
{
	double* fl = GetOpenGLMatrix();

	for (int i=0;i<m_meshes.size();i++)
		m_meshes[i]->Bucketize(fl, this, m_bUseObjectColor, m_objectColor);
}



void KX_GameObject::RemoveMeshes()
{
	double* fl = GetOpenGLMatrix();

	for (int i=0;i<m_meshes.size();i++)
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
	{
		m_pPhysicsController1->SetSumoTransform(false);
	}
}



void KX_GameObject::SetDebugColor(unsigned int bgra)
{
	for (int i=0;i<m_meshes.size();i++)
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


/*
void KX_GameObject::RegisterSumoObject(class SM_Scene* sumoScene,
									   DT_SceneHandle solidscene,
									   class SM_Object* sumoObj,
									   const char* matname,
									   bool isDynamic,
									   bool isActor)
{
	m_bDyna = isDynamic;

	// need easy access, not via 'node' etc.
	m_pPhysicsController = new KX_PhysicsController(sumoScene,solidscene,sumoObj,isDynamic);
				
	GetSGNode()->AddSGController(m_pPhysicsController);
	
	m_pClient_info->m_type = (isActor ? 1 : 0);
	m_pClient_info->m_clientobject = this;

	// store materialname in auxinfo, needed for touchsensors
	m_pClient_info->m_auxilary_info = (matname? (void*)(matname+2) : NULL);
	m_pPhysicsController->SetObject(this->GetSGNode());
}
*/

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
	
	for (int i=0;i<m_meshes.size();i++)
	{
		double* fl = GetOpenGLMatrix();
		m_meshes[i]->MarkVisible(fl,this,visible,m_bUseObjectColor,m_objectColor);
	}
}


// Always use the flag?
void 
KX_GameObject::MarkVisible(
	void
	)
{
	for (int i=0;i<m_meshes.size();i++)
	{
		double* fl = GetOpenGLMatrix();
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
//	if (m_pPhysicsController1)
//		m_pPhysicsController1->AddLinearVelocity(lin_vel,local);
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



void KX_GameObject::SetObjectColor(const MT_Vector4& rgbavec)
{
	m_bUseObjectColor = true;
	m_objectColor = rgbavec;
}



MT_Vector3 KX_GameObject::GetLinearVelocity()
{
	MT_Vector3 velocity(0.0,0.0,0.0);
	
	if (m_pPhysicsController1)
	{
		velocity = m_pPhysicsController1->GetLinearVelocity();
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

	GetSGNode()->SetLocalPosition(trans);
}



void KX_GameObject::NodeSetLocalOrientation(const MT_Matrix3x3& rot)
{
	if (m_pPhysicsController1)
	{
		m_pPhysicsController1->setOrientation(rot.getRotation());
	}
	
	GetSGNode()->SetLocalOrientation(rot);
}



void KX_GameObject::NodeSetLocalScale(const MT_Vector3& scale)
{
	if (m_pPhysicsController1)
	{
		m_pPhysicsController1->setScaling(scale);
	}
	
	GetSGNode()->SetLocalScale(scale);
}



void KX_GameObject::NodeSetRelativeScale(const MT_Vector3& scale)
{
	GetSGNode()->RelativeScale(scale);
}



void KX_GameObject::NodeUpdateGS(double time,bool bInitiator)
{
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
	{"getLinearVelocity", (PyCFunction) KX_GameObject::sPyGetLinearVelocity, METH_VARARGS},
	{"getVelocity", (PyCFunction) KX_GameObject::sPyGetVelocity, METH_VARARGS},
	{"getOrientation", (PyCFunction) KX_GameObject::sPyGetOrientation, METH_VARARGS},
	{"setOrientation", (PyCFunction) KX_GameObject::sPySetOrientation, METH_VARARGS},
	{"getMass", (PyCFunction) KX_GameObject::sPyGetMass, METH_VARARGS},
	{"getReactionForce", (PyCFunction) KX_GameObject::sPyGetReactionForce, METH_VARARGS},
	{"applyImpulse", (PyCFunction) KX_GameObject::sPyApplyImpulse, METH_VARARGS},
	{"suspendDynamics", (PyCFunction)KX_GameObject::sPySuspendDynamics,METH_VARARGS},
	{"restoreDynamics", (PyCFunction)KX_GameObject::sPyRestoreDynamics,METH_VARARGS},
	{"enableRigidBody", (PyCFunction)KX_GameObject::sPyEnableRigidBody,METH_VARARGS},
	{"disableRigidBody", (PyCFunction)KX_GameObject::sPyDisableRigidBody,METH_VARARGS},
	{"getParent", (PyCFunction)KX_GameObject::sPyGetParent,METH_VARARGS},
	{"getMesh", (PyCFunction)KX_GameObject::sPyGetMesh,METH_VARARGS},
	{"getPhysicsId", (PyCFunction)KX_GameObject::sPyGetPhysicsId,METH_VARARGS},
	
	
	{NULL,NULL} //Sentinel
};




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
	MT_Point3 pos = NodeGetWorldPosition();
	
	PyObject* resultlist = PyList_New(3);
	int index;
	for (index=0;index<3;index++)
	{
		PyList_SetItem(resultlist,index,PyFloat_FromDouble(pos[index]));
	}
	
	return resultlist;
	
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




PyObject* KX_GameObject::_getattr(char* attr)
{
	_getattr_up(SCA_IObject);
}



PyObject* KX_GameObject::PyGetLinearVelocity(PyObject* self, 
											 PyObject* args, 
											 PyObject* kwds)
{
	// only can get the velocity if we have a physics object connected to us...
	MT_Vector3 velocity = GetLinearVelocity();
	
	PyObject* resultlist = PyList_New(3);
	int index;
	for (index=0;index<3;index++)
	{
		PyList_SetItem(resultlist,index,PyFloat_FromDouble(velocity[index]));
	}

	return resultlist;
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
	
	
	MT_Point3 pos;
	PyObject* pylist;
	bool	error = false;
	
	int len = PyTuple_Size(args);
	
	if ((len > 0) && PyArg_ParseTuple(args,"O",&pylist))
	{
		if (pylist->ob_type == &CListValue::Type)
		{
			CListValue* listval = (CListValue*) pylist;
			if (listval->GetCount() == 3)
			{
				int index;
				for (index=0;index<3;index++)
				{
					pos[index] = listval->GetValue(index)->GetNumber();
				}
			}	else
			{
				error = true;
			}
			
		} else
		{
			
			// assert the list is long enough...
			int numitems = PyList_Size(pylist);
			if (numitems == 3)
			{
				int index;
				for (index=0;index<3;index++)
				{
					pos[index] = PyFloat_AsDouble(PyList_GetItem(pylist,index));
				}
			}
			else
			{
				error = true;
			}
		}
		
		if (!error)
			point = pos;	
	}
	
	
	if (m_pPhysicsController1)
	{
		velocity = m_pPhysicsController1->GetVelocity(point);
	}
	
	PyObject* resultlist = PyList_New(3);
	int index;
	for (index=0;index<3;index++)
	{
		PyList_SetItem(resultlist,index,PyFloat_FromDouble(velocity[index]));
	}

	return resultlist;
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
	
	MT_Vector3 reaction_force = GetPhysicsController()->getReactionForce();
	
	PyObject* resultlist = PyList_New(3);
	int index;
	for (index=0;index<3;index++)
	{
		PyList_SetItem(resultlist,index,
			PyFloat_FromDouble(reaction_force[index]));
	}

	return resultlist;
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
		return parent;
	Py_Return;
}



PyObject* KX_GameObject::PyGetMesh(PyObject* self, 
								   PyObject* args, 
								   PyObject* kwds)
{
	if (m_meshes.size() > 0)
	{
		KX_MeshProxy* meshproxy = new KX_MeshProxy(m_meshes[0]);
		return meshproxy;
	}
	
	Py_Return;
}



PyObject* KX_GameObject::PyApplyImpulse(PyObject* self, 
										PyObject* args, 
										PyObject* kwds)
{
	
	MT_Point3  attach(0, 1, 0);
	MT_Vector3 impulse(1, 0, 0);
	
	if (ConvertPythonVectorArgs(args,attach,impulse))
	{
		if (m_pPhysicsController1)
		{
			m_pPhysicsController1->applyImpulse(attach, impulse);
		}
		
	}
	
	Py_Return;
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
	// do the conversion of a C++ matrix to a python list
	
	PyObject* resultlist = PyList_New(3);
	
	
	int row,col;
	const MT_Matrix3x3& orient = NodeGetWorldOrientation();
	
	int index = 0;
	for (row=0;row<3;row++)
	{
		PyObject* veclist = PyList_New(3);
		
		for (col=0;col<3;col++)
		{
			const MT_Scalar fl = orient[row][col];
			PyList_SetItem(veclist,col,PyFloat_FromDouble(fl));
		}
		PyList_SetItem(resultlist,row,veclist);
		
	}
	return resultlist;
}



PyObject* KX_GameObject::PySetOrientation(PyObject* self, 
										  PyObject* args, 
										  PyObject* kwds)
{
	MT_Matrix3x3 matrix;
	
	PyObject* pylist;
	bool	error = false;
	int row,col;
	
	PyArg_ParseTuple(args,"O",&pylist);
	
	if (pylist->ob_type == &CListValue::Type)
	{
		CListValue* listval = (CListValue*) pylist;
		if (listval->GetCount() == 3)
		{
			for (row=0;row<3;row++) // each row has a 3-vector [x,y,z]
			{
				CListValue* vecval = (CListValue*)listval->GetValue(row);
				for (col=0;col<3;col++)
				{
					matrix[row][col] = vecval->GetValue(col)->GetNumber();
					
				}
			}
		}
		else
		{
			error = true;
		}
	}
	else
	{
		// assert the list is long enough...
		int numitems = PyList_Size(pylist);
		if (numitems == 3)
		{
			for (row=0;row<3;row++) // each row has a 3-vector [x,y,z]
			{
				
				PyObject* veclist = PyList_GetItem(pylist,row); // here we have a vector3 list
				for (col=0;col<3;col++)
				{
					matrix[row][col] =  PyFloat_AsDouble(PyList_GetItem(veclist,col));
					
				}
			}
		}
		else
		{
			error = true;
		}
	}
	
	if (!error)
	{
		if (m_pPhysicsController1)
		{
			m_pPhysicsController1->setOrientation(matrix.getRotation());
		}
		NodeSetLocalOrientation(matrix);
	}
	
	Py_INCREF(Py_None);
	return Py_None;
}



PyObject* KX_GameObject::PySetPosition(PyObject* self, 
									   PyObject* args, 
									   PyObject* kwds)
{
	// make a general function for this, it's needed many times
	
	MT_Point3 pos = ConvertPythonVectorArg(args);
	if (this->m_pPhysicsController1)
	{
		this->m_pPhysicsController1->setPosition(pos);
	}
	NodeSetLocalPosition(pos);
	
	Py_INCREF(Py_None);
	return Py_None;
}

PyObject* KX_GameObject::PyGetPhysicsId(PyObject* self,
											   PyObject* args,
											   PyObject* kwds)
{
	KX_IPhysicsController* ctrl = GetPhysicsController();
	int physid=0;
	if (ctrl)
	{
		physid= (int)ctrl->GetUserData();
	}
	return PyInt_FromLong(physid);
}

/* --------------------------------------------------------------------- 
 * Some stuff taken from the header
 * --------------------------------------------------------------------- */
void KX_GameObject::Relink(GEN_Map<GEN_HashedPtr, void*> *map_parameter)	
{
	/* intentionally empty ? */
}

