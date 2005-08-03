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
 */
#include "KX_PyConstraintBinding.h"
#include "PHY_IPhysicsEnvironment.h"
#include "KX_ConstraintWrapper.h"
#include "KX_PhysicsObjectWrapper.h"
#include "PHY_IPhysicsController.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

// nasty glob variable to connect scripting language
// if there is a better way (without global), please do so!
static PHY_IPhysicsEnvironment* g_CurrentActivePhysicsEnvironment = NULL;

static char PhysicsConstraints_module_documentation[] =
"This is the Python API for the Physics Constraints";


static char gPySetGravity__doc__[] = "setGravity(float x,float y,float z)";
static char gPySetDebugMode__doc__[] = "setDebugMode(int mode)";

static char gPySetNumIterations__doc__[] = "setNumIterations(int numiter) This sets the number of iterations for an iterative constraint solver";
static char gPySetDeactivationTime__doc__[] = "setDeactivationTime(float time) This sets the time after which a resting rigidbody gets deactived";
static char gPySetDeactivationLinearTreshold__doc__[] = "setDeactivationLinearTreshold(float linearTreshold)";
static char gPySetDeactivationAngularTreshold__doc__[] = "setDeactivationAngularTreshold(float angularTreshold)";
static char gPySetContactBreakingTreshold__doc__[] = "setContactBreakingTreshold(float breakingTreshold) Reasonable default is 0.02 (if units are meters)";
static char gPySetCcdMode__doc__[] = "setCcdMode(int ccdMode) Very experimental, not recommended";

static char gPySetSorConstant__doc__[] = "setSorConstant(float sor) Very experimental, not recommended";
static char gPySetTau__doc__[] = "setTau(float tau) Very experimental, not recommended";
static char gPySetDamping__doc__[] = "setDamping(float damping) Very experimental, not recommended";


static char gPyCreateConstraint__doc__[] = "createConstraint(ob1,ob2,float restLength,float restitution,float damping)";
static char gPyRemoveConstraint__doc__[] = "removeConstraint(constraint id)";





static PyObject* gPySetGravity(PyObject* self,
										 PyObject* args, 
										 PyObject* kwds)
{
	float x,y,z;
	int len = PyTuple_Size(args);
	if ((len == 3) && PyArg_ParseTuple(args,"fff",&x,&y,&z))
	{
		if (PHY_GetActiveEnvironment())
			PHY_GetActiveEnvironment()->setGravity(x,y,z);
	}
	Py_INCREF(Py_None); return Py_None;
}

static PyObject* gPySetDebugMode(PyObject* self,
										 PyObject* args, 
										 PyObject* kwds)
{
	int mode;
	if (PyArg_ParseTuple(args,"i",&mode))
	{
		if (PHY_GetActiveEnvironment())
		{
			PHY_GetActiveEnvironment()->setDebugMode(mode);
			
		}
		
	}
	Py_INCREF(Py_None); return Py_None;
}

static PyObject* gPySetNumIterations(PyObject* self,
										 PyObject* args, 
										 PyObject* kwds)
{
	int iter;
	if (PyArg_ParseTuple(args,"i",&iter))
	{
		if (PHY_GetActiveEnvironment())
		{
			PHY_GetActiveEnvironment()->setNumIterations(iter);
		}
	}
	Py_INCREF(Py_None); return Py_None;
}



static PyObject* gPySetDeactivationTime(PyObject* self,
										 PyObject* args, 
										 PyObject* kwds)
{
	float deactive_time;
	if (PyArg_ParseTuple(args,"f",&deactive_time))
	{
		if (PHY_GetActiveEnvironment())
		{
			PHY_GetActiveEnvironment()->setDeactivationTime(deactive_time);
		}
	}
	Py_INCREF(Py_None); return Py_None;
}


static PyObject* gPySetDeactivationLinearTreshold(PyObject* self,
										 PyObject* args, 
										 PyObject* kwds)
{
	float linearDeactivationTreshold;
	if (PyArg_ParseTuple(args,"f",&linearDeactivationTreshold))
	{
		if (PHY_GetActiveEnvironment())
		{
			PHY_GetActiveEnvironment()->setDeactivationLinearTreshold( linearDeactivationTreshold);
		}
	}
	Py_INCREF(Py_None); return Py_None;
}


static PyObject* gPySetDeactivationAngularTreshold(PyObject* self,
										 PyObject* args, 
										 PyObject* kwds)
{
	float angularDeactivationTreshold;
	if (PyArg_ParseTuple(args,"f",&angularDeactivationTreshold))
	{
		if (PHY_GetActiveEnvironment())
		{
			PHY_GetActiveEnvironment()->setDeactivationAngularTreshold( angularDeactivationTreshold);
		}
	}
	Py_INCREF(Py_None); return Py_None;
}

static PyObject* gPySetContactBreakingTreshold(PyObject* self,
										 PyObject* args, 
										 PyObject* kwds)
{
	float contactBreakingTreshold;
	if (PyArg_ParseTuple(args,"f",&contactBreakingTreshold))
	{
		if (PHY_GetActiveEnvironment())
		{
			PHY_GetActiveEnvironment()->setContactBreakingTreshold( contactBreakingTreshold);
		}
	}
	Py_INCREF(Py_None); return Py_None;
}


static PyObject* gPySetCcdMode(PyObject* self,
										 PyObject* args, 
										 PyObject* kwds)
{
	float ccdMode;
	if (PyArg_ParseTuple(args,"f",&ccdMode))
	{
		if (PHY_GetActiveEnvironment())
		{
			PHY_GetActiveEnvironment()->setCcdMode( ccdMode);
		}
	}
	Py_INCREF(Py_None); return Py_None;
}

static PyObject* gPySetSorConstant(PyObject* self,
										 PyObject* args, 
										 PyObject* kwds)
{
	float sor;
	if (PyArg_ParseTuple(args,"f",&sor))
	{
		if (PHY_GetActiveEnvironment())
		{
			PHY_GetActiveEnvironment()->setSolverSorConstant( sor);
		}
	}
	Py_INCREF(Py_None); return Py_None;
}

static PyObject* gPySetTau(PyObject* self,
										 PyObject* args, 
										 PyObject* kwds)
{
	float tau;
	if (PyArg_ParseTuple(args,"f",&tau))
	{
		if (PHY_GetActiveEnvironment())
		{
			PHY_GetActiveEnvironment()->setTau( tau);
		}
	}
	Py_INCREF(Py_None); return Py_None;
}


static PyObject* gPySetDamping(PyObject* self,
										 PyObject* args, 
										 PyObject* kwds)
{
	float damping;
	if (PyArg_ParseTuple(args,"f",&damping))
	{
		if (PHY_GetActiveEnvironment())
		{
			PHY_GetActiveEnvironment()->setDamping( damping);
		}
	}
	Py_INCREF(Py_None); return Py_None;
}


static PyObject* gPyCreateConstraint(PyObject* self,
										 PyObject* args, 
										 PyObject* kwds)
{
	int physicsid=0,physicsid2 = 0,constrainttype=0,extrainfo=0;
	int len = PyTuple_Size(args);
	int success = 1;
	float pivotX=1,pivotY=1,pivotZ=1,axisX=0,axisY=0,axisZ=1;
	if (len == 3)
	{
		success = PyArg_ParseTuple(args,"iii",&physicsid,&physicsid2,&constrainttype);
	}
	else
	if (len ==6)
	{
		success = PyArg_ParseTuple(args,"iiifff",&physicsid,&physicsid2,&constrainttype,
			&pivotX,&pivotY,&pivotZ);
	}
	else if (len == 9)
	{
		success = PyArg_ParseTuple(args,"iiiffffff",&physicsid,&physicsid2,&constrainttype,
			&pivotX,&pivotY,&pivotZ,&axisX,&axisY,&axisZ);
	}
	else if (len==4)
	{
		success = PyArg_ParseTuple(args,"iiii",&physicsid,&physicsid2,&constrainttype,&extrainfo);
		pivotX=extrainfo;
	}
	
	if (success)
	{
		if (PHY_GetActiveEnvironment())
		{
			
			PHY_IPhysicsController* physctrl = (PHY_IPhysicsController*) physicsid;
			PHY_IPhysicsController* physctrl2 = (PHY_IPhysicsController*) physicsid2;
			if (physctrl) //TODO:check for existance of this pointer!
			{
				int constraintid = PHY_GetActiveEnvironment()->createConstraint(physctrl,physctrl2,(enum PHY_ConstraintType)constrainttype,pivotX,pivotY,pivotZ,axisX,axisY,axisZ);
				
				KX_ConstraintWrapper* wrap = new KX_ConstraintWrapper((enum PHY_ConstraintType)constrainttype,constraintid,PHY_GetActiveEnvironment());
				

				return wrap;
			}
			
			
		}
	}

	Py_INCREF(Py_None); return Py_None;
}


static PyObject* gPyRemoveConstraint(PyObject* self,
										 PyObject* args, 
										 PyObject* kwds)
{
	int constraintid;
	
	if (PyArg_ParseTuple(args,"i",&constraintid))
	{
		if (PHY_GetActiveEnvironment())
		{
			PHY_GetActiveEnvironment()->removeConstraint(constraintid);
		}
	}
	Py_INCREF(Py_None); return Py_None;
}


static struct PyMethodDef physicsconstraints_methods[] = {
  {"setGravity",(PyCFunction) gPySetGravity,
   METH_VARARGS, gPySetGravity__doc__},
  {"setDebugMode",(PyCFunction) gPySetDebugMode,
   METH_VARARGS, gPySetDebugMode__doc__},

   /// settings that influence quality of the rigidbody dynamics
  {"setNumIterations",(PyCFunction) gPySetNumIterations,
   METH_VARARGS, gPySetNumIterations__doc__},

  {"setDeactivationTime",(PyCFunction) gPySetDeactivationTime,
   METH_VARARGS, gPySetDeactivationTime__doc__},

  {"setDeactivationLinearTreshold",(PyCFunction) gPySetDeactivationLinearTreshold,
   METH_VARARGS, gPySetDeactivationLinearTreshold__doc__},
  {"setDeactivationAngularTreshold",(PyCFunction) gPySetDeactivationAngularTreshold,
   METH_VARARGS, gPySetDeactivationAngularTreshold__doc__},

   {"setContactBreakingTreshold",(PyCFunction) gPySetContactBreakingTreshold,
   METH_VARARGS, gPySetContactBreakingTreshold__doc__},
     {"setCcdMode",(PyCFunction) gPySetCcdMode,
   METH_VARARGS, gPySetCcdMode__doc__},
     {"setSorConstant",(PyCFunction) gPySetSorConstant,
   METH_VARARGS, gPySetSorConstant__doc__},
       {"setTau",(PyCFunction) gPySetTau,
   METH_VARARGS, gPySetTau__doc__},
        {"setDamping",(PyCFunction) gPySetDamping,
   METH_VARARGS, gPySetDamping__doc__},



  {"createConstraint",(PyCFunction) gPyCreateConstraint,
   METH_VARARGS, gPyCreateConstraint__doc__},
  {"removeConstraint",(PyCFunction) gPyRemoveConstraint,
   METH_VARARGS, gPyRemoveConstraint__doc__},

   //sentinel
  { NULL, (PyCFunction) NULL, 0, NULL }
};



PyObject*	initPythonConstraintBinding()
{

  PyObject* ErrorObject;
  PyObject* m;
  PyObject* d;


  m = Py_InitModule4("PhysicsConstraints", physicsconstraints_methods,
		     PhysicsConstraints_module_documentation,
		     (PyObject*)NULL,PYTHON_API_VERSION);

  // Add some symbolic constants to the module
  d = PyModule_GetDict(m);
  ErrorObject = PyString_FromString("PhysicsConstraints.error");
  PyDict_SetItemString(d, "error", ErrorObject);

  // XXXX Add constants here

  // Check for errors
  if (PyErr_Occurred())
    {
      Py_FatalError("can't initialize module PhysicsConstraints");
    }

  return d;
}


void	KX_RemovePythonConstraintBinding()
{
}

void	PHY_SetActiveEnvironment(class	PHY_IPhysicsEnvironment* env)
{
	g_CurrentActivePhysicsEnvironment = env;
}

PHY_IPhysicsEnvironment*	PHY_GetActiveEnvironment()
{
	return g_CurrentActivePhysicsEnvironment;
}

