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
 */

/** \file gameengine/Ketsji/KX_PyConstraintBinding.cpp
 *  \ingroup ketsji
 */

#include "KX_PyConstraintBinding.h"
#include "PHY_IPhysicsEnvironment.h"
#include "KX_ConstraintWrapper.h"
#include "KX_VehicleWrapper.h"
#include "KX_CharacterWrapper.h"
#include "PHY_IPhysicsController.h"
#include "PHY_IVehicle.h"
#include "PHY_DynamicTypes.h"
#include "MT_Matrix3x3.h"

#include "KX_GameObject.h" // ConvertPythonToGameObject()

#include "PyObjectPlus.h" 

#ifdef WITH_BULLET
#  include "LinearMath/btIDebugDraw.h"
#endif

#ifdef WITH_PYTHON

// macro copied from KX_PythonInit.cpp
#define KX_MACRO_addTypesToDict(dict, name, name2) PyDict_SetItemString(dict, #name, item=PyLong_FromLong(name2)); Py_DECREF(item)

// nasty glob variable to connect scripting language
// if there is a better way (without global), please do so!
static PHY_IPhysicsEnvironment* g_CurrentActivePhysicsEnvironment = NULL;

static char PhysicsConstraints_module_documentation[] =
"This is the Python API for the Physics Constraints";


static char gPySetGravity__doc__[] = "setGravity(float x,float y,float z)";
static char gPySetDebugMode__doc__[] = "setDebugMode(int mode)";

static char gPySetNumIterations__doc__[] = "setNumIterations(int numiter) This sets the number of iterations for an iterative constraint solver";
static char gPySetNumTimeSubSteps__doc__[] = "setNumTimeSubSteps(int numsubstep) This sets the number of substeps for each physics proceed. Tradeoff quality for performance.";


static char gPySetDeactivationTime__doc__[] = "setDeactivationTime(float time) This sets the time after which a resting rigidbody gets deactived";
static char gPySetDeactivationLinearTreshold__doc__[] = "setDeactivationLinearTreshold(float linearTreshold)";
static char gPySetDeactivationAngularTreshold__doc__[] = "setDeactivationAngularTreshold(float angularTreshold)";
static char gPySetContactBreakingTreshold__doc__[] = "setContactBreakingTreshold(float breakingTreshold) Reasonable default is 0.02 (if units are meters)";

static char gPySetCcdMode__doc__[] = "setCcdMode(int ccdMode) Very experimental, not recommended";
static char gPySetSorConstant__doc__[] = "setSorConstant(float sor) Very experimental, not recommended";
static char gPySetSolverTau__doc__[] = "setTau(float tau) Very experimental, not recommended";
static char gPySetSolverDamping__doc__[] = "setDamping(float damping) Very experimental, not recommended";
static char gPySetLinearAirDamping__doc__[] = "setLinearAirDamping(float damping) Very experimental, not recommended";
static char gPySetUseEpa__doc__[] = "setUseEpa(int epa) Very experimental, not recommended";
static char gPySetSolverType__doc__[] = "setSolverType(int solverType) Very experimental, not recommended";


static char gPyCreateConstraint__doc__[] = "createConstraint(ob1,ob2,float restLength,float restitution,float damping)";
static char gPyGetVehicleConstraint__doc__[] = "getVehicleConstraint(int constraintId)";
static char gPyGetCharacter__doc__[] = "getCharacter(KX_GameObject obj)";
static char gPyRemoveConstraint__doc__[] = "removeConstraint(int constraintId)";
static char gPyGetAppliedImpulse__doc__[] = "getAppliedImpulse(int constraintId)";






static PyObject *gPySetGravity(PyObject *self,
                               PyObject *args,
                               PyObject *kwds)
{
	float x,y,z;
	if (PyArg_ParseTuple(args,"fff",&x,&y,&z))
	{
		if (PHY_GetActiveEnvironment())
			PHY_GetActiveEnvironment()->SetGravity(x,y,z);
	}
	else {
		return NULL;
	}
	
	Py_RETURN_NONE;
}

static PyObject *gPySetDebugMode(PyObject *self,
                                 PyObject *args,
                                 PyObject *kwds)
{
	int mode;
	if (PyArg_ParseTuple(args,"i",&mode))
	{
		if (PHY_GetActiveEnvironment())
		{
			PHY_GetActiveEnvironment()->SetDebugMode(mode);
			
		}
		
	}
	else {
		return NULL;
	}
	
	Py_RETURN_NONE;
}



static PyObject *gPySetNumTimeSubSteps(PyObject *self,
                                       PyObject *args,
                                       PyObject *kwds)
{
	int substep;
	if (PyArg_ParseTuple(args,"i",&substep))
	{
		if (PHY_GetActiveEnvironment())
		{
			PHY_GetActiveEnvironment()->SetNumTimeSubSteps(substep);
		}
	}
	else {
		return NULL;
	}
	Py_RETURN_NONE;
}


static PyObject *gPySetNumIterations(PyObject *self,
                                     PyObject *args,
                                     PyObject *kwds)
{
	int iter;
	if (PyArg_ParseTuple(args,"i",&iter))
	{
		if (PHY_GetActiveEnvironment())
		{
			PHY_GetActiveEnvironment()->SetNumIterations(iter);
		}
	}
	else {
		return NULL;
	}
	Py_RETURN_NONE;
}


static PyObject *gPySetDeactivationTime(PyObject *self,
                                        PyObject *args,
                                        PyObject *kwds)
{
	float deactive_time;
	if (PyArg_ParseTuple(args,"f",&deactive_time))
	{
		if (PHY_GetActiveEnvironment())
		{
			PHY_GetActiveEnvironment()->SetDeactivationTime(deactive_time);
		}
	}
	else {
		return NULL;
	}
	Py_RETURN_NONE;
}


static PyObject *gPySetDeactivationLinearTreshold(PyObject *self,
                                                  PyObject *args,
                                                  PyObject *kwds)
{
	float linearDeactivationTreshold;
	if (PyArg_ParseTuple(args,"f",&linearDeactivationTreshold))
	{
		if (PHY_GetActiveEnvironment())
		{
			PHY_GetActiveEnvironment()->SetDeactivationLinearTreshold( linearDeactivationTreshold);
		}
	}
	else {
		return NULL;
	}
	Py_RETURN_NONE;
}


static PyObject *gPySetDeactivationAngularTreshold(PyObject *self,
                                                   PyObject *args,
                                                   PyObject *kwds)
{
	float angularDeactivationTreshold;
	if (PyArg_ParseTuple(args,"f",&angularDeactivationTreshold))
	{
		if (PHY_GetActiveEnvironment())
		{
			PHY_GetActiveEnvironment()->SetDeactivationAngularTreshold( angularDeactivationTreshold);
		}
	}
	else {
		return NULL;
	}
	Py_RETURN_NONE;
}

static PyObject *gPySetContactBreakingTreshold(PyObject *self,
                                               PyObject *args,
                                               PyObject *kwds)
{
	float contactBreakingTreshold;
	if (PyArg_ParseTuple(args,"f",&contactBreakingTreshold))
	{
		if (PHY_GetActiveEnvironment())
		{
			PHY_GetActiveEnvironment()->SetContactBreakingTreshold( contactBreakingTreshold);
		}
	}
	else {
		return NULL;
	}
	Py_RETURN_NONE;
}


static PyObject *gPySetCcdMode(PyObject *self,
                               PyObject *args,
                               PyObject *kwds)
{
	float ccdMode;
	if (PyArg_ParseTuple(args,"f",&ccdMode))
	{
		if (PHY_GetActiveEnvironment())
		{
			PHY_GetActiveEnvironment()->SetCcdMode( ccdMode);
		}
	}
	else {
		return NULL;
	}
	Py_RETURN_NONE;
}

static PyObject *gPySetSorConstant(PyObject *self,
                                   PyObject *args,
                                   PyObject *kwds)
{
	float sor;
	if (PyArg_ParseTuple(args,"f",&sor))
	{
		if (PHY_GetActiveEnvironment())
		{
			PHY_GetActiveEnvironment()->SetSolverSorConstant( sor);
		}
	}
	else {
		return NULL;
	}
	Py_RETURN_NONE;
}

static PyObject *gPySetSolverTau(PyObject *self,
                                 PyObject *args,
                                 PyObject *kwds)
{
	float tau;
	if (PyArg_ParseTuple(args,"f",&tau))
	{
		if (PHY_GetActiveEnvironment())
		{
			PHY_GetActiveEnvironment()->SetSolverTau( tau);
		}
	}
	else {
		return NULL;
	}
	Py_RETURN_NONE;
}


static PyObject *gPySetSolverDamping(PyObject *self,
                                     PyObject *args,
                                     PyObject *kwds)
{
	float damping;
	if (PyArg_ParseTuple(args,"f",&damping))
	{
		if (PHY_GetActiveEnvironment())
		{
			PHY_GetActiveEnvironment()->SetSolverDamping( damping);
		}
	}
	else {
		return NULL;
	}
	Py_RETURN_NONE;
}

static PyObject *gPySetLinearAirDamping(PyObject *self,
                                        PyObject *args,
                                        PyObject *kwds)
{
	float damping;
	if (PyArg_ParseTuple(args,"f",&damping))
	{
		if (PHY_GetActiveEnvironment())
		{
			PHY_GetActiveEnvironment()->SetLinearAirDamping( damping);
		}
	}
	else {
		return NULL;
	}
	Py_RETURN_NONE;
}


static PyObject *gPySetUseEpa(PyObject *self,
                              PyObject *args,
                              PyObject *kwds)
{
	int	epa;
	if (PyArg_ParseTuple(args,"i",&epa))
	{
		if (PHY_GetActiveEnvironment())
		{
			PHY_GetActiveEnvironment()->SetUseEpa(epa);
		}
	}
	else {
		return NULL;
	}
	Py_RETURN_NONE;
}
static PyObject *gPySetSolverType(PyObject *self,
                                  PyObject *args,
                                  PyObject *kwds)
{
	int	solverType;
	if (PyArg_ParseTuple(args,"i",&solverType))
	{
		if (PHY_GetActiveEnvironment())
		{
			PHY_GetActiveEnvironment()->SetSolverType(solverType);
		}
	}
	else {
		return NULL;
	}
	Py_RETURN_NONE;
}



static PyObject *gPyGetVehicleConstraint(PyObject *self,
                                         PyObject *args,
                                         PyObject *kwds)
{
#if defined(_WIN64)
	__int64 constraintid;
	if (PyArg_ParseTuple(args,"L",&constraintid))
#else
	long constraintid;
	if (PyArg_ParseTuple(args,"l",&constraintid))
#endif
	{
		if (PHY_GetActiveEnvironment())
		{
			
			PHY_IVehicle* vehicle = PHY_GetActiveEnvironment()->GetVehicleConstraint(constraintid);
			if (vehicle)
			{
				KX_VehicleWrapper* pyWrapper = new KX_VehicleWrapper(vehicle,PHY_GetActiveEnvironment());
				return pyWrapper->NewProxy(true);
			}

		}
	}
	else {
		return NULL;
	}

	Py_RETURN_NONE;
}

static PyObject* gPyGetCharacter(PyObject* self,
                                 PyObject* args,
                                 PyObject* kwds)
{
	PyObject* pyob;
	KX_GameObject *ob;

	if (!PyArg_ParseTuple(args,"O", &pyob))
		return NULL;

	if (!ConvertPythonToGameObject(pyob, &ob, false, "bge.constraints.getCharacter(value)"))
		return NULL;

	if (PHY_GetActiveEnvironment())
	{
			
		PHY_ICharacter* character= PHY_GetActiveEnvironment()->GetCharacterController(ob);
		if (character)
		{
			KX_CharacterWrapper* pyWrapper = new KX_CharacterWrapper(character);
			return pyWrapper->NewProxy(true);
		}

	}

	Py_RETURN_NONE;
}

static PyObject *gPyCreateConstraint(PyObject *self,
                                     PyObject *args,
                                     PyObject *kwds)
{
	/* FIXME - physicsid is a long being cast to a pointer, should at least use PyCapsule */
#if defined(_WIN64)
	__int64 physicsid=0,physicsid2 = 0;
#else
	long physicsid=0,physicsid2 = 0;
#endif
	int constrainttype=0, extrainfo=0;
	int len = PyTuple_Size(args);
	int success = 1;
	int flag = 0;

	float pivotX=1,pivotY=1,pivotZ=1,axisX=0,axisY=0,axisZ=1;
	if (len == 3)
	{
#if defined(_WIN64)
		success = PyArg_ParseTuple(args,"LLi",&physicsid,&physicsid2,&constrainttype);
#else
		success = PyArg_ParseTuple(args,"lli",&physicsid,&physicsid2,&constrainttype);
#endif
	}
	else if (len == 6)
	{
#if defined(_WIN64)
		success = PyArg_ParseTuple(args,"LLifff",&physicsid,&physicsid2,&constrainttype,
		                           &pivotX,&pivotY,&pivotZ);
#else
		success = PyArg_ParseTuple(args,"llifff",&physicsid,&physicsid2,&constrainttype,
		                           &pivotX,&pivotY,&pivotZ);
#endif
	}
	else if (len == 9)
	{
#if defined(_WIN64)
		success = PyArg_ParseTuple(args,"LLiffffff",&physicsid,&physicsid2,&constrainttype,
		                           &pivotX,&pivotY,&pivotZ,&axisX,&axisY,&axisZ);
#else
		success = PyArg_ParseTuple(args,"lliffffff",&physicsid,&physicsid2,&constrainttype,
		                           &pivotX,&pivotY,&pivotZ,&axisX,&axisY,&axisZ);
#endif
	}
	else if (len == 10)
	{
#if defined(_WIN64)
		success = PyArg_ParseTuple(args,"LLiffffffi",&physicsid,&physicsid2,&constrainttype,
		                           &pivotX,&pivotY,&pivotZ,&axisX,&axisY,&axisZ,&flag);
#else
		success = PyArg_ParseTuple(args,"lliffffffi",&physicsid,&physicsid2,&constrainttype,
		                           &pivotX,&pivotY,&pivotZ,&axisX,&axisY,&axisZ,&flag);
#endif
	}

	/* XXX extrainfo seems to be nothing implemented. right now it works as a pivot with [X,0,0] */
	else if (len == 4)
	{
#if defined(_WIN64)
		success = PyArg_ParseTuple(args,"LLii",&physicsid,&physicsid2,&constrainttype,&extrainfo);
#else
		success = PyArg_ParseTuple(args,"llii",&physicsid,&physicsid2,&constrainttype,&extrainfo);
#endif
		pivotX=extrainfo;
	}

	if (success)
	{
		if (PHY_GetActiveEnvironment())
		{
			
			PHY_IPhysicsController* physctrl = (PHY_IPhysicsController*) physicsid;
			PHY_IPhysicsController* physctrl2 = (PHY_IPhysicsController*) physicsid2;
			if (physctrl) //TODO:check for existence of this pointer!
			{
				PHY_ConstraintType ct = (PHY_ConstraintType) constrainttype;
				int constraintid =0;

				if (ct == PHY_GENERIC_6DOF_CONSTRAINT)
				{
					//convert from euler angle into axis
					float radsPerDeg = 6.283185307179586232f / 360.f;

					//we need to pass a full constraint frame, not just axis
					//localConstraintFrameBasis
					MT_Matrix3x3 localCFrame(MT_Vector3(radsPerDeg*axisX,radsPerDeg*axisY,radsPerDeg*axisZ));
					MT_Vector3 axis0 = localCFrame.getColumn(0);
					MT_Vector3 axis1 = localCFrame.getColumn(1);
					MT_Vector3 axis2 = localCFrame.getColumn(2);

					constraintid = PHY_GetActiveEnvironment()->CreateConstraint(physctrl,physctrl2,(enum PHY_ConstraintType)constrainttype,
					                                                            pivotX,pivotY,pivotZ,
					                                                            (float)axis0.x(),(float)axis0.y(),(float)axis0.z(),
					                                                            (float)axis1.x(),(float)axis1.y(),(float)axis1.z(),
					                                                            (float)axis2.x(),(float)axis2.y(),(float)axis2.z(),flag);
				}
				else {
					constraintid = PHY_GetActiveEnvironment()->CreateConstraint(physctrl,physctrl2,(enum PHY_ConstraintType)constrainttype,pivotX,pivotY,pivotZ,axisX,axisY,axisZ,0);
				}
				
				KX_ConstraintWrapper* wrap = new KX_ConstraintWrapper((enum PHY_ConstraintType)constrainttype,constraintid,PHY_GetActiveEnvironment());

				return wrap->NewProxy(true);
			}
			
			
		}
	}
	else {
		return NULL;
	}

	Py_RETURN_NONE;
}




static PyObject *gPyGetAppliedImpulse(PyObject *self,
                                      PyObject *args,
                                      PyObject *kwds)
{
	float	appliedImpulse = 0.f;

#if defined(_WIN64)
	__int64 constraintid;
	if (PyArg_ParseTuple(args,"L",&constraintid))
#else
	long constraintid;
	if (PyArg_ParseTuple(args,"l",&constraintid))
#endif
	{
		if (PHY_GetActiveEnvironment())
		{
			appliedImpulse = PHY_GetActiveEnvironment()->GetAppliedImpulse(constraintid);
		}
	}
	else {
		return NULL;
	}

	return PyFloat_FromDouble(appliedImpulse);
}


static PyObject *gPyRemoveConstraint(PyObject *self,
                                     PyObject *args,
                                     PyObject *kwds)
{
#if defined(_WIN64)
	__int64 constraintid;
	if (PyArg_ParseTuple(args,"L",&constraintid))
#else
	long constraintid;
	if (PyArg_ParseTuple(args,"l",&constraintid))
#endif
	{
		if (PHY_GetActiveEnvironment())
		{
			PHY_GetActiveEnvironment()->RemoveConstraint(constraintid);
		}
	}
	else {
		return NULL;
	}
	
	Py_RETURN_NONE;
}

static PyObject *gPyExportBulletFile(PyObject *, PyObject *args)
{
	char* filename;
	if (!PyArg_ParseTuple(args,"s:exportBulletFile",&filename))
		return NULL;

	if (PHY_GetActiveEnvironment())
	{
		PHY_GetActiveEnvironment()->ExportFile(filename);
	}
	Py_RETURN_NONE;
}

static struct PyMethodDef physicsconstraints_methods[] = {
	{"setGravity",(PyCFunction) gPySetGravity,
	 METH_VARARGS, (const char*)gPySetGravity__doc__},
	{"setDebugMode",(PyCFunction) gPySetDebugMode,
	 METH_VARARGS, (const char *)gPySetDebugMode__doc__},

	/// settings that influence quality of the rigidbody dynamics
	{"setNumIterations",(PyCFunction) gPySetNumIterations,
	 METH_VARARGS, (const char *)gPySetNumIterations__doc__},

	{"setNumTimeSubSteps",(PyCFunction) gPySetNumTimeSubSteps,
	 METH_VARARGS, (const char *)gPySetNumTimeSubSteps__doc__},

	{"setDeactivationTime",(PyCFunction) gPySetDeactivationTime,
	 METH_VARARGS, (const char *)gPySetDeactivationTime__doc__},

	{"setDeactivationLinearTreshold",(PyCFunction) gPySetDeactivationLinearTreshold,
	 METH_VARARGS, (const char *)gPySetDeactivationLinearTreshold__doc__},
	{"setDeactivationAngularTreshold",(PyCFunction) gPySetDeactivationAngularTreshold,
	 METH_VARARGS, (const char *)gPySetDeactivationAngularTreshold__doc__},

	{"setContactBreakingTreshold",(PyCFunction) gPySetContactBreakingTreshold,
	 METH_VARARGS, (const char *)gPySetContactBreakingTreshold__doc__},
	{"setCcdMode",(PyCFunction) gPySetCcdMode,
	 METH_VARARGS, (const char *)gPySetCcdMode__doc__},
	{"setSorConstant",(PyCFunction) gPySetSorConstant,
	 METH_VARARGS, (const char *)gPySetSorConstant__doc__},
	{"setSolverTau",(PyCFunction) gPySetSolverTau,
	 METH_VARARGS, (const char *)gPySetSolverTau__doc__},
	{"setSolverDamping",(PyCFunction) gPySetSolverDamping,
	 METH_VARARGS, (const char *)gPySetSolverDamping__doc__},

	{"setLinearAirDamping",(PyCFunction) gPySetLinearAirDamping,
	 METH_VARARGS, (const char *)gPySetLinearAirDamping__doc__},

	{"setUseEpa",(PyCFunction) gPySetUseEpa,
	 METH_VARARGS, (const char *)gPySetUseEpa__doc__},
	{"setSolverType",(PyCFunction) gPySetSolverType,
	 METH_VARARGS, (const char *)gPySetSolverType__doc__},


	{"createConstraint",(PyCFunction) gPyCreateConstraint,
	 METH_VARARGS, (const char *)gPyCreateConstraint__doc__},
	{"getVehicleConstraint",(PyCFunction) gPyGetVehicleConstraint,
	 METH_VARARGS, (const char *)gPyGetVehicleConstraint__doc__},

	{"getCharacter",(PyCFunction) gPyGetCharacter,
	 METH_VARARGS, (const char *)gPyGetCharacter__doc__},

	{"removeConstraint",(PyCFunction) gPyRemoveConstraint,
	 METH_VARARGS, (const char *)gPyRemoveConstraint__doc__},
	{"getAppliedImpulse",(PyCFunction) gPyGetAppliedImpulse,
	 METH_VARARGS, (const char *)gPyGetAppliedImpulse__doc__},

	{"exportBulletFile",(PyCFunction)gPyExportBulletFile,
	 METH_VARARGS, "export a .bullet file"},

	//sentinel
	{ NULL, (PyCFunction) NULL, 0, NULL }
};

static struct PyModuleDef PhysicsConstraints_module_def = {
	{}, /* m_base */
	"PhysicsConstraints",  /* m_name */
	PhysicsConstraints_module_documentation,  /* m_doc */
	0,  /* m_size */
	physicsconstraints_methods,  /* m_methods */
	0,  /* m_reload */
	0,  /* m_traverse */
	0,  /* m_clear */
	0,  /* m_free */
};

PyObject *initPythonConstraintBinding()
{

	PyObject *ErrorObject;
	PyObject *m;
	PyObject *d;
	PyObject *item;

	/* Use existing module where possible
	 * be careful not to init any runtime vars after this */
	m = PyImport_ImportModule( "PhysicsConstraints" );
	if (m) {
		Py_DECREF(m);
		return m;
	}
	else {
		PyErr_Clear();
		
		m = PyModule_Create(&PhysicsConstraints_module_def);
		PyDict_SetItemString(PySys_GetObject("modules"), PhysicsConstraints_module_def.m_name, m);
	}

	// Add some symbolic constants to the module
	d = PyModule_GetDict(m);
	ErrorObject = PyUnicode_FromString("PhysicsConstraints.error");
	PyDict_SetItemString(d, "error", ErrorObject);
	Py_DECREF(ErrorObject);

#ifdef WITH_BULLET
	//Debug Modes constants to be used with setDebugMode() python function
	KX_MACRO_addTypesToDict(d, DBG_NODEBUG, btIDebugDraw::DBG_NoDebug);
	KX_MACRO_addTypesToDict(d, DBG_DRAWWIREFRAME, btIDebugDraw::DBG_DrawWireframe);
	KX_MACRO_addTypesToDict(d, DBG_DRAWAABB, btIDebugDraw::DBG_DrawAabb);
	KX_MACRO_addTypesToDict(d, DBG_DRAWFREATURESTEXT, btIDebugDraw::DBG_DrawFeaturesText);
	KX_MACRO_addTypesToDict(d, DBG_DRAWCONTACTPOINTS, btIDebugDraw::DBG_DrawContactPoints);
	KX_MACRO_addTypesToDict(d, DBG_NOHELPTEXT, btIDebugDraw::DBG_NoHelpText);
	KX_MACRO_addTypesToDict(d, DBG_DRAWTEXT, btIDebugDraw::DBG_DrawText);
	KX_MACRO_addTypesToDict(d, DBG_PROFILETIMINGS, btIDebugDraw::DBG_ProfileTimings);
	KX_MACRO_addTypesToDict(d, DBG_ENABLESATCOMPARISION, btIDebugDraw::DBG_EnableSatComparison);
	KX_MACRO_addTypesToDict(d, DBG_DISABLEBULLETLCP, btIDebugDraw::DBG_DisableBulletLCP);
	KX_MACRO_addTypesToDict(d, DBG_ENABLECCD, btIDebugDraw::DBG_EnableCCD);
	KX_MACRO_addTypesToDict(d, DBG_DRAWCONSTRAINTS, btIDebugDraw::DBG_DrawConstraints);
	KX_MACRO_addTypesToDict(d, DBG_DRAWCONSTRAINTLIMITS, btIDebugDraw::DBG_DrawConstraintLimits);
	KX_MACRO_addTypesToDict(d, DBG_FASTWIREFRAME, btIDebugDraw::DBG_FastWireframe);
#endif // WITH_BULLET

	//Constraint types to be used with createConstraint() python function
	KX_MACRO_addTypesToDict(d, POINTTOPOINT_CONSTRAINT, PHY_POINT2POINT_CONSTRAINT);
	KX_MACRO_addTypesToDict(d, LINEHINGE_CONSTRAINT, PHY_LINEHINGE_CONSTRAINT);
	KX_MACRO_addTypesToDict(d, ANGULAR_CONSTRAINT, PHY_ANGULAR_CONSTRAINT);
	KX_MACRO_addTypesToDict(d, CONETWIST_CONSTRAINT, PHY_CONE_TWIST_CONSTRAINT);
	KX_MACRO_addTypesToDict(d, VEHICLE_CONSTRAINT, PHY_VEHICLE_CONSTRAINT);
	KX_MACRO_addTypesToDict(d, GENERIC_6DOF_CONSTRAINT, PHY_GENERIC_6DOF_CONSTRAINT);

	// Check for errors
	if (PyErr_Occurred()) {
		Py_FatalError("can't initialize module PhysicsConstraints");
	}

	return d;
}

#if 0
static void KX_RemovePythonConstraintBinding()
{
}
#endif

void	PHY_SetActiveEnvironment(class	PHY_IPhysicsEnvironment* env)
{
	g_CurrentActivePhysicsEnvironment = env;
}

PHY_IPhysicsEnvironment*	PHY_GetActiveEnvironment()
{
	return g_CurrentActivePhysicsEnvironment;
}

#endif // WITH_PYTHON
