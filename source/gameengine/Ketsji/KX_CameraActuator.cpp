/**
 * KX_CameraActuator.cpp
 *
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
	const CValue *obj,
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
	// nothing to do
}

KX_CameraActuator::~KX_CameraActuator()
{
	//nothing to do
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




/* three functions copied from blender arith... don't know if there's an equivalent */

float Normalise(float *n)
{
	float d;
	
	d= n[0]*n[0]+n[1]*n[1]+n[2]*n[2];
	/* FLT_EPSILON is too large! A larger value causes normalise errors in a scaled down utah teapot */
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

void Crossf(float *c, float *a, float *b)
{
	c[0] = a[1] * b[2] - a[2] * b[1];
	c[1] = a[2] * b[0] - a[0] * b[2];
	c[2] = a[0] * b[1] - a[1] * b[0];
}


void VecUpMat3(float *vec, float mat[][3], short axis)
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
	Normalise((float *)mat[coz]);
	
	inp= mat[coz][2];
	mat[coy][0]= - inp*mat[coz][0];
	mat[coy][1]= - inp*mat[coz][1];
	mat[coy][2]= 1.0 - inp*mat[coz][2];

	Normalise((float *)mat[coy]);
	
	Crossf(mat[cox], mat[coy], mat[coz]);
	
}

bool KX_CameraActuator::Update(double curtime,double deltatime)
{
	bool result = true;

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
	
	/* wondering... is it really neccesary/desirable to suppress negative    */
	/* events here?                                                          */
	bool bNegativeEvent = IsNegativeEvent();
	RemoveAllEvents();

	if (bNegativeEvent) return false;

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
	VecUpMat3(rc, mat, 3);	/* y up Track -z */
	



	/* now set the camera position and rotation */
	
	obj->NodeSetLocalPosition(from);
	
	actormat[0][0]= mat[0][0]; actormat[0][1]= mat[1][0]; actormat[0][2]= mat[2][0];
	actormat[1][0]= mat[0][1]; actormat[1][1]= mat[1][1]; actormat[1][2]= mat[2][1];
	actormat[2][0]= mat[0][2]; actormat[2][1]= mat[1][2]; actormat[2][2]= mat[2][2];
	obj->NodeSetLocalOrientation(actormat);

	return result;
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
	{NULL,NULL,NULL,NULL} //Sentinel
};

PyObject* KX_CameraActuator::_getattr(const STR_String& attr) {
	_getattr_up(SCA_IActuator);
}


/* eof */
