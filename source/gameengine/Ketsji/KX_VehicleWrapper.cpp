

#include <Python.h>
#include "KX_VehicleWrapper.h"
#include "PHY_IPhysicsEnvironment.h"
#include "PHY_IVehicle.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

KX_VehicleWrapper::KX_VehicleWrapper(
						PHY_IVehicle* vehicle,
						PHY_IPhysicsEnvironment* physenv,PyTypeObject *T) :
		PyObjectPlus(T),
		m_vehicle(vehicle),
		m_physenv(physenv)
{
}

KX_VehicleWrapper::~KX_VehicleWrapper()
{
}


PyObject* KX_VehicleWrapper::PyAddWheel(PyObject* self, 
											PyObject* args, 
											PyObject* kwds)
{
	Py_INCREF(Py_None);
	return Py_None;
}



PyObject* KX_VehicleWrapper::PyGetWheelsTransform(PyObject* self, 
											PyObject* args, 
											PyObject* kwds)
{
	assert(0);
	return PyInt_FromLong(m_vehicle->GetNumWheels());
}


PyObject* KX_VehicleWrapper::PyGetNumWheels(PyObject* self, 
											PyObject* args, 
											PyObject* kwds)
{
	return PyInt_FromLong(m_vehicle->GetNumWheels());
}


PyObject* KX_VehicleWrapper::PyGetConstraintId(PyObject* self, 
											PyObject* args, 
											PyObject* kwds)
{
	return PyInt_FromLong(m_vehicle->GetUserConstraintId());
}

PyObject* KX_VehicleWrapper::PyGetConstraintType(PyObject* self, 
											PyObject* args, 
											PyObject* kwds)
{
	return PyInt_FromLong(m_vehicle->GetUserConstraintType());
}




//python specific stuff
PyTypeObject KX_VehicleWrapper::Type = {
	PyObject_HEAD_INIT(&PyType_Type)
		0,
		"KX_VehicleWrapper",
		sizeof(KX_VehicleWrapper),
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

PyParentObject KX_VehicleWrapper::Parents[] = {
	&KX_VehicleWrapper::Type,
	NULL
};

PyObject*	KX_VehicleWrapper::_getattr(const STR_String& attr)
{
	//here you can search for existing data members (like mass,friction etc.)
	_getattr_up(PyObjectPlus);
}

int	KX_VehicleWrapper::_setattr(const STR_String& attr,PyObject* pyobj)
{
	
	PyTypeObject* type = pyobj->ob_type;
	int result = 1;

	if (type == &PyList_Type)
	{
		result = 0;
	}
	if (type == &PyFloat_Type)
	{
		result = 0;

	}
	if (type == &PyInt_Type)
	{
		result = 0;
	}
	if (type == &PyString_Type)
	{
		result = 0;
	}
	if (result)
		result = PyObjectPlus::_setattr(attr,pyobj);
	return result;
};


PyMethodDef KX_VehicleWrapper::Methods[] = {
	{"addWheel",(PyCFunction) KX_VehicleWrapper::sPyAddWheel, METH_VARARGS},
	{"getNumWheels",(PyCFunction) KX_VehicleWrapper::sPyGetNumWheels, METH_VARARGS},
	{"getWheelsTransform",(PyCFunction) KX_VehicleWrapper::sPyGetWheelsTransform, METH_VARARGS},
	{"getConstraintId",(PyCFunction) KX_VehicleWrapper::sPyGetConstraintId, METH_VARARGS},
	{"getConstraintType",(PyCFunction) KX_VehicleWrapper::sPyGetConstraintType, METH_VARARGS},
	{NULL,NULL} //Sentinel
};



