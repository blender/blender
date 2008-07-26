/**
 * KX_CameraActuator.cpp
 *
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
 *
 */

#include "KX_CameraActuator.h"
#include <iostream>
#include <math.h>
#include "KX_GameObject.h"

STR_String KX_CameraActuator::X_AXIS_STRING = "x";
STR_String KX_CameraActuator::Y_AXIS_STRING = "y";

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */

KX_CameraActuator::KX_CameraActuator(
	SCA_IObject* gameobj, 
	SCA_IObject *obj,
	MT_Scalar hght,
	MT_Scalar minhght,
	MT_Scalar maxhght,
	bool  xytog,
	PyTypeObject* T
): 
	SCA_IActuator(gameobj, T),
	m_ob (obj),
	m_height (hght),
	m_minHeight (minhght),
	m_maxHeight (maxhght),
	m_x (xytog)
{
	if (m_ob)
		m_ob->RegisterActuator(this);
}

KX_CameraActuator::~KX_CameraActuator()
{
	if (m_ob)
		m_ob->UnregisterActuator(this);
}

	CValue* 
KX_CameraActuator::
GetReplica(
) {
	KX_CameraActuator* replica = new KX_CameraActuator(*this);
	replica->ProcessReplica();
	// this will copy properties and so on...
	CValue::AddDataToReplica(replica);
	return replica;
};

void KX_CameraActuator::ProcessReplica()
{
	if (m_ob)
		m_ob->RegisterActuator(this);
	SCA_IActuator::ProcessReplica();
}

bool KX_CameraActuator::UnlinkObject(SCA_IObject* clientobj)
{
	if (clientobj == m_ob)
	{
		// this object is being deleted, we cannot continue to track it.
		m_ob = NULL;
		return true;
	}
	return false;
}


void KX_CameraActuator::Relink(GEN_Map<GEN_HashedPtr, void*> *obj_map)
{
	void **h_obj = (*obj_map)[m_ob];
	if (h_obj) {
		if (m_ob)
			m_ob->UnregisterActuator(this);
		m_ob = (SCA_IObject*)(*h_obj);
		m_ob->RegisterActuator(this);
	}
}

/* three functions copied from blender arith... don't know if there's an equivalent */

static float Kx_Normalize(float *n)
{
	float d;
	
	d= n[0]*n[0]+n[1]*n[1]+n[2]*n[2];
	/* FLT_EPSILON is too large! A larger value causes normalize errors in a scaled down utah teapot */
	if(d>0.0000000000001) {
		d= sqrt(d);

		n[0]/=d; 
		n[1]/=d; 
		n[2]/=d;
	} else {
		n[0]=n[1]=n[2]= 0.0;
		d= 0.0;
	}
	return d;
}

static void Kx_Crossf(float *c, float *a, float *b)
{
	c[0] = a[1] * b[2] - a[2] * b[1];
	c[1] = a[2] * b[0] - a[0] * b[2];
	c[2] = a[0] * b[1] - a[1] * b[0];
}


static void Kx_VecUpMat3(float *vec, float mat[][3], short axis)
{

	// Construct a camera matrix s.t. the specified axis

	// maps to the given vector (*vec). Also defines the rotation

	// about this axis by mapping one of the other axis to the y-axis.


	float inp;
	short cox = 0, coy = 0, coz = 0;
	
	/* up varieeren heeft geen zin, is eigenlijk helemaal geen up!
	 * zie VecUpMat3old
	 */

	if(axis==0) {
		cox= 0; coy= 1; coz= 2;		/* Y up Z tr */
	}
	if(axis==1) {
		cox= 1; coy= 2; coz= 0;		/* Z up X tr */
	}
	if(axis==2) {
		cox= 2; coy= 0; coz= 1;		/* X up Y tr */
	}
	if(axis==3) {
		cox= 0; coy= 1; coz= 2;		/* Y op -Z tr */
		vec[0]= -vec[0];
		vec[1]= -vec[1];
		vec[2]= -vec[2];
	}
	if(axis==4) {
		cox= 1; coy= 0; coz= 2;		/*  */
	}
	if(axis==5) {
		cox= 2; coy= 1; coz= 0;		/* Y up X tr */
	}

	mat[coz][0]= vec[0];
	mat[coz][1]= vec[1];
	mat[coz][2]= vec[2];
	if (Kx_Normalize((float *)mat[coz]) == 0.f) {
		/* this is a very abnormal situation: the camera has reach the object center exactly
		   We will choose a completely arbitrary direction */
		mat[coz][0] = 1.0f;
		mat[coz][1] = 0.0f;
		mat[coz][2] = 0.0f;
	}
	
	inp= mat[coz][2];
	mat[coy][0]= - inp*mat[coz][0];
	mat[coy][1]= - inp*mat[coz][1];
	mat[coy][2]= 1.0 - inp*mat[coz][2];

	if (Kx_Normalize((float *)mat[coy]) == 0.f) {
		/* the camera is vertical, chose the y axis arbitrary */
		mat[coy][0] = 0.f;
		mat[coy][1] = 1.f;
		mat[coy][2] = 0.f;
	}
	
	Kx_Crossf(mat[cox], mat[coy], mat[coz]);
	
}

bool KX_CameraActuator::Update(double curtime, bool frame)
{
	/* wondering... is it really neccesary/desirable to suppress negative    */
	/* events here?                                                          */
	bool bNegativeEvent = IsNegativeEvent();
	RemoveAllEvents();

	if (bNegativeEvent || !m_ob) 
		return false;
	
	KX_GameObject *obj = (KX_GameObject*) GetParent();
	MT_Point3 from = obj->NodeGetWorldPosition();
	MT_Matrix3x3 frommat = obj->NodeGetWorldOrientation();
	/* These casts are _very_ dangerous!!! */
	MT_Point3 lookat = ((KX_GameObject*)m_ob)->NodeGetWorldPosition();
	MT_Matrix3x3 actormat = ((KX_GameObject*)m_ob)->NodeGetWorldOrientation();

	float fp1[3], fp2[3], rc[3];
	float inp, fac; //, factor = 0.0; /* some factor...                                    */
	float mindistsq, maxdistsq, distsq;
	float mat[3][3];
	
	/* The rules:                                                            */
	/* CONSTRAINT 1: not implemented */
	/* CONSTRAINT 2: can camera see actor?              */
	/* CONSTRAINT 3: fixed height relative to floor below actor.             */
	/* CONSTRAINT 4: camera rotates behind actor                              */
	/* CONSTRAINT 5: minimum / maximum distance                             */
	/* CONSTRAINT 6: again: fixed height relative to floor below actor        */
	/* CONSTRAINT 7: track to floor below actor                               */
	/* CONSTRAINT 8: look a little bit left or right, depending on how the

	   character is looking (horizontal x)
 */

	/* ...and then set the camera position. Since we assume the parent of    */
	/* this actuator is always a camera, just set the parent position and    */
	/* rotation. We do not check whether we really have a camera as parent.  */
	/* It may be better to turn this into a general tracking actuator later  */
	/* on, since lots of plausible relations can be filled in here.          */

	/* ... set up some parameters ...                                        */
	/* missing here: the 'floorloc' of the actor's shadow */

 	mindistsq= m_minHeight*m_minHeight;
	maxdistsq= m_maxHeight*m_maxHeight;

	/* C1: not checked... is a future option                                 */

	/* C2: blender test_visibility function. Can this be a ray-test?         */

	/* C3: fixed height  */
	from[2] = (15.0*from[2] + lookat[2] + m_height)/16.0;


	/* C4: camera behind actor   */
	if (m_x) {
		fp1[0] = actormat[0][0];
		fp1[1] = actormat[1][0];
		fp1[2] = actormat[2][0];

		fp2[0] = frommat[0][0];
		fp2[1] = frommat[1][0];
		fp2[2] = frommat[2][0];
	} 
	else {
		fp1[0] = actormat[0][1];
		fp1[1] = actormat[1][1];
		fp1[2] = actormat[2][1];

		fp2[0] = frommat[0][1];
		fp2[1] = frommat[1][1];
		fp2[2] = frommat[2][1];
	}
	
	inp= fp1[0]*fp2[0] + fp1[1]*fp2[1] + fp1[2]*fp2[2];
	fac= (-1.0 + inp)/32.0;

	from[0]+= fac*fp1[0];
	from[1]+= fac*fp1[1];
	from[2]+= fac*fp1[2];
	
	/* alleen alstie ervoor ligt: cross testen en loodrechte bijtellen */
	if(inp<0.0) {
		if(fp1[0]*fp2[1] - fp1[1]*fp2[0] > 0.0) {
			from[0]-= fac*fp1[1];
			from[1]+= fac*fp1[0];
		}
		else {
			from[0]+= fac*fp1[1];
			from[1]-= fac*fp1[0];
		}
	}

	/* CONSTRAINT 5: minimum / maximum afstand */

	rc[0]= (lookat[0]-from[0]);
	rc[1]= (lookat[1]-from[1]);
	rc[2]= (lookat[2]-from[2]);
	distsq= rc[0]*rc[0] + rc[1]*rc[1] + rc[2]*rc[2];

	if(distsq > maxdistsq) {
		distsq = 0.15*(distsq-maxdistsq)/distsq;
		
		from[0] += distsq*rc[0];
		from[1] += distsq*rc[1];
		from[2] += distsq*rc[2];
	}
	else if(distsq < mindistsq) {
		distsq = 0.15*(mindistsq-distsq)/mindistsq;
		
		from[0] -= distsq*rc[0];
		from[1] -= distsq*rc[1];
		from[2] -= distsq*rc[2];
	}


	/* CONSTRAINT 7: track to schaduw */
	rc[0]= (lookat[0]-from[0]);
	rc[1]= (lookat[1]-from[1]);
	rc[2]= (lookat[2]-from[2]);
	Kx_VecUpMat3(rc, mat, 3);	/* y up Track -z */
	



	/* now set the camera position and rotation */
	
	obj->NodeSetLocalPosition(from);
	
	actormat[0][0]= mat[0][0]; actormat[0][1]= mat[1][0]; actormat[0][2]= mat[2][0];
	actormat[1][0]= mat[0][1]; actormat[1][1]= mat[1][1]; actormat[1][2]= mat[2][1];
	actormat[2][0]= mat[0][2]; actormat[2][1]= mat[1][2]; actormat[2][2]= mat[2][2];
	obj->NodeSetLocalOrientation(actormat);

	return true;
}

CValue *KX_CameraActuator::findObject(char *obName) 
{
	/* hook to object system */
	return NULL;
}

bool KX_CameraActuator::string2axischoice(const char *axisString) 
{
	bool res = true;

	res = !(axisString == Y_AXIS_STRING);

	return res;
}

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

/* Integration hooks ------------------------------------------------------- */
PyTypeObject KX_CameraActuator::Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,
	"KX_CameraActuator",
	sizeof(KX_CameraActuator),
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

PyParentObject KX_CameraActuator::Parents[] = {
	&KX_CameraActuator::Type,
	&SCA_IActuator::Type,
	&SCA_ILogicBrick::Type,
	&CValue::Type,
	NULL
};

PyMethodDef KX_CameraActuator::Methods[] = {
	{"setObject",(PyCFunction) KX_CameraActuator::sPySetObject, METH_VARARGS,	SetObject_doc},
	{"getObject",(PyCFunction) KX_CameraActuator::sPyGetObject, METH_NOARGS,	GetObject_doc},
	{"setMin"	,(PyCFunction) KX_CameraActuator::sPySetMin,	METH_VARARGS,	SetMin_doc},
	{"getMin"	,(PyCFunction) KX_CameraActuator::sPyGetMin,	METH_NOARGS,	GetMin_doc},
	{"setMax"	,(PyCFunction) KX_CameraActuator::sPySetMax,	METH_VARARGS,	SetMax_doc},
	{"getMax"	,(PyCFunction) KX_CameraActuator::sPyGetMax,	METH_NOARGS,	GetMax_doc},
	{"setHeight",(PyCFunction) KX_CameraActuator::sPySetHeight,	METH_VARARGS,	SetHeight_doc},
	{"getHeight",(PyCFunction) KX_CameraActuator::sPyGetHeight,	METH_NOARGS,	GetHeight_doc},
	{"setXY"	,(PyCFunction) KX_CameraActuator::sPySetXY,		METH_VARARGS,	SetXY_doc},
	{"getXY"	,(PyCFunction) KX_CameraActuator::sPyGetXY,	METH_VARARGS,	GetXY_doc},
	{NULL,NULL,NULL,NULL} //Sentinel
};

PyObject* KX_CameraActuator::_getattr(const STR_String& attr) {
	_getattr_up(SCA_IActuator);
}
/* get obj  ---------------------------------------------------------- */
char KX_CameraActuator::GetObject_doc[] = 
"getObject\n"
"\tReturns the object this sensor reacts to.\n";
PyObject* KX_CameraActuator::PyGetObject(PyObject* self, 
										PyObject* args, 
										PyObject* kwds)
{
	return PyString_FromString(m_ob->GetName());
}
/* set obj  ---------------------------------------------------------- */
char KX_CameraActuator::SetObject_doc[] = 
"setObject\n"
"\tSets the object this sensor reacts to.\n";
PyObject* KX_CameraActuator::PySetObject(PyObject* self, 
										PyObject* args, 
										PyObject* kwds)
{
    
	PyObject* gameobj;
	if (PyArg_ParseTuple(args, "O!", &KX_GameObject::Type, &gameobj))
	{
		if (m_ob)
			m_ob->UnregisterActuator(this);
		m_ob = (SCA_IObject*)gameobj;
		if (m_ob)
			m_ob->RegisterActuator(this);
		Py_Return;
	}
	PyErr_Clear();
	
	char* objectname;
	if (PyArg_ParseTuple(args, "s", &objectname))
	{
		SCA_IObject *object = (SCA_IObject*)SCA_ILogicBrick::m_sCurrentLogicManager->GetGameObjectByName(STR_String(objectname));
		if(object)
		{
			if (m_ob != NULL)
				m_ob->UnregisterActuator(this);
			m_ob = object;
			m_ob->RegisterActuator(this);
			Py_Return;
		}
	}
	
	return NULL;
}

/* get min  ---------------------------------------------------------- */
char KX_CameraActuator::GetMin_doc[] = 
"getMin\n"
"\tReturns the minimum value set in the Min: field.\n";
PyObject* KX_CameraActuator::PyGetMin(PyObject* self, 
										PyObject* args, 
										PyObject* kwds)
{
	return PyFloat_FromDouble(m_minHeight);
}
/* set min  ---------------------------------------------------------- */
char KX_CameraActuator::SetMin_doc[] = 
"setMin\n"
"\tSets the minimum value.\n";
PyObject* KX_CameraActuator::PySetMin(PyObject* self, 
										PyObject* args, 
										PyObject* kwds)
{
	float min;
	if(PyArg_ParseTuple(args,"f", &min))
	{
		m_minHeight = min;
		Py_Return;
	}
	return NULL;
}
/* get min  ---------------------------------------------------------- */
char KX_CameraActuator::GetMax_doc[] = 
"getMax\n"
"\tReturns the maximum value set in the Max: field.\n";
PyObject* KX_CameraActuator::PyGetMax(PyObject* self, 
										PyObject* args, 
										PyObject* kwds)
{
	return PyFloat_FromDouble(m_maxHeight);
}
/* set min  ---------------------------------------------------------- */
char KX_CameraActuator::SetMax_doc[] = 
"setMax\n"
"\tSets the maximum value.\n";
PyObject* KX_CameraActuator::PySetMax(PyObject* self, 
										PyObject* args, 
										PyObject* kwds)
{
	float max;
	if(PyArg_ParseTuple(args,"f", &max))
	{
		m_maxHeight = max;
		Py_Return;
	}
	return NULL;
}
/* get height  ---------------------------------------------------------- */
char KX_CameraActuator::GetHeight_doc[] = 
"getHeight\n"
"\tReturns the height value set in the height: field.\n";
PyObject* KX_CameraActuator::PyGetHeight(PyObject* self, 
										PyObject* args, 
										PyObject* kwds)
{
	return PyFloat_FromDouble(m_height);
}
/* set height  ---------------------------------------------------------- */
char KX_CameraActuator::SetHeight_doc[] = 
"setHeight\n"
"\tSets the height value.\n";
PyObject* KX_CameraActuator::PySetHeight(PyObject* self, 
										PyObject* args, 
										PyObject* kwds)
{
	float height;
	if(PyArg_ParseTuple(args,"f", &height))
	{
		m_height = height;
		Py_Return;
	}
	return NULL;
}
/* set XY  ---------------------------------------------------------- */
char KX_CameraActuator::SetXY_doc[] = 
"setXY\n"
"\tSets axis the camera tries to get behind.\n"
"\t1=x, 0=y\n";
PyObject* KX_CameraActuator::PySetXY(PyObject* self, 
										PyObject* args, 
										PyObject* kwds)
{
	int value;
	if(PyArg_ParseTuple(args,"i", &value))
	{
		m_x = value != 0;
		Py_Return;
	}
	return NULL;
}

/* get XY -------------------------------------------------------------*/
char KX_CameraActuator::GetXY_doc[] =
"getXY\n"
"\tGets the axis the camera tries to get behind.\n"
"\tTrue = X, False = Y\n";
PyObject* KX_CameraActuator::PyGetXY(PyObject* self,
										PyObject* args, 
										PyObject* kwds)
{
	return PyInt_FromLong(m_x);
}

/* eof */
