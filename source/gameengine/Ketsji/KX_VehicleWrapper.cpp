

#include "PyObjectPlus.h"

#include "KX_VehicleWrapper.h"
#include "PHY_IPhysicsEnvironment.h"
#include "PHY_IVehicle.h"
#include "KX_PyMath.h"
#include "KX_GameObject.h"
#include "KX_MotionState.h"

KX_VehicleWrapper::KX_VehicleWrapper(
						PHY_IVehicle* vehicle,
						PHY_IPhysicsEnvironment* physenv) :
		PyObjectPlus(),
		m_vehicle(vehicle),
		m_physenv(physenv)
{
}

KX_VehicleWrapper::~KX_VehicleWrapper()
{
	int numMotion = m_motionStates.size();
	for (int i=0;i<numMotion;i++)
	{
		PHY_IMotionState* motionState = m_motionStates[i];
		delete motionState;
	}
	m_motionStates.clear();
}

#ifndef DISABLE_PYTHON

PyObject* KX_VehicleWrapper::PyAddWheel(PyObject* args)
{
	
	PyObject* pylistPos,*pylistDir,*pylistAxleDir;
	PyObject* wheelGameObject;
	float suspensionRestLength,wheelRadius;
	int hasSteering;

	
	if (PyArg_ParseTuple(args,"OOOOffi:addWheel",&wheelGameObject,&pylistPos,&pylistDir,&pylistAxleDir,&suspensionRestLength,&wheelRadius,&hasSteering))
	{
		KX_GameObject *gameOb;
		if (!ConvertPythonToGameObject(wheelGameObject, &gameOb, false, "vehicle.addWheel(...): KX_VehicleWrapper (first argument)"))
			return NULL;
		

		if (gameOb->GetSGNode())
		{
			PHY_IMotionState* motionState = new KX_MotionState(gameOb->GetSGNode());
			
			/* TODO - no error checking here! - bad juju */
			MT_Vector3 attachPos,attachDir,attachAxle;
			PyVecTo(pylistPos,attachPos);
			PyVecTo(pylistDir,attachDir);
			PyVecTo(pylistAxleDir,attachAxle);
			PHY__Vector3 aPos,aDir,aAxle;
			aPos[0] = attachPos[0];
			aPos[1] = attachPos[1];
			aPos[2] = attachPos[2];
			aDir[0] = attachDir[0];
			aDir[1] = attachDir[1];
			aDir[2] = attachDir[2];
			aAxle[0] = -attachAxle[0];//someone reverse some conventions inside Bullet (axle winding)
			aAxle[1] = -attachAxle[1];
			aAxle[2] = -attachAxle[2];
			
			printf("attempt for addWheel: suspensionRestLength%f wheelRadius %f, hasSteering:%d\n",suspensionRestLength,wheelRadius,hasSteering);
			m_vehicle->AddWheel(motionState,aPos,aDir,aAxle,suspensionRestLength,wheelRadius,hasSteering);
		}
		
	} else {
		return NULL;
	}
	Py_RETURN_NONE;
}




PyObject* KX_VehicleWrapper::PyGetWheelPosition(PyObject* args)
{
	
	int wheelIndex;

	if (PyArg_ParseTuple(args,"i:getWheelPosition",&wheelIndex))
	{
		float position[3];
		m_vehicle->GetWheelPosition(wheelIndex,position[0],position[1],position[2]);
		MT_Vector3 pos(position[0],position[1],position[2]);
		return PyObjectFrom(pos);
	}
	return NULL;
}

PyObject* KX_VehicleWrapper::PyGetWheelRotation(PyObject* args)
{
	int wheelIndex;
	if (PyArg_ParseTuple(args,"i:getWheelRotation",&wheelIndex))
	{
		return PyFloat_FromDouble(m_vehicle->GetWheelRotation(wheelIndex));
	}
	return NULL;
}

PyObject* KX_VehicleWrapper::PyGetWheelOrientationQuaternion(PyObject* args)
{
	int wheelIndex;
	if (PyArg_ParseTuple(args,"i:getWheelOrientationQuaternion",&wheelIndex))
	{
		float orn[4];
		m_vehicle->GetWheelOrientationQuaternion(wheelIndex,orn[0],orn[1],orn[2],orn[3]);
		MT_Quaternion	quatorn(orn[0],orn[1],orn[2],orn[3]);
		MT_Matrix3x3 ornmat(quatorn);
		return PyObjectFrom(ornmat);
	}
	return NULL;

}


PyObject* KX_VehicleWrapper::PyGetNumWheels(PyObject* args)
{
	return PyLong_FromSsize_t(m_vehicle->GetNumWheels());
}


PyObject* KX_VehicleWrapper::PyGetConstraintId(PyObject* args)
{
	return PyLong_FromSsize_t(m_vehicle->GetUserConstraintId());
}



PyObject* KX_VehicleWrapper::PyApplyEngineForce(PyObject* args)
{
	float force;
	int wheelIndex;

	if (PyArg_ParseTuple(args,"fi:applyEngineForce",&force,&wheelIndex))
	{
		force *= -1.f;//someone reverse some conventions inside Bullet (axle winding)
		m_vehicle->ApplyEngineForce(force,wheelIndex);
	}
	else {
		return NULL;
	}
	Py_RETURN_NONE;
}

PyObject* KX_VehicleWrapper::PySetTyreFriction(PyObject* args)
{
	float wheelFriction;
	int wheelIndex;

	if (PyArg_ParseTuple(args,"fi:setTyreFriction",&wheelFriction,&wheelIndex))
	{
		m_vehicle->SetWheelFriction(wheelFriction,wheelIndex);
	}
	else {
		return NULL;
	}
	Py_RETURN_NONE;
}

PyObject* KX_VehicleWrapper::PySetSuspensionStiffness(PyObject* args)
{
	float suspensionStiffness;
	int wheelIndex;

	if (PyArg_ParseTuple(args,"fi:setSuspensionStiffness",&suspensionStiffness,&wheelIndex))
	{
		m_vehicle->SetSuspensionStiffness(suspensionStiffness,wheelIndex);
	}
	else {
		return NULL;
	}
	Py_RETURN_NONE;
}

PyObject* KX_VehicleWrapper::PySetSuspensionDamping(PyObject* args)
{
	float suspensionDamping;
	int wheelIndex;

	if (PyArg_ParseTuple(args,"fi:setSuspensionDamping",&suspensionDamping,&wheelIndex))
	{
		m_vehicle->SetSuspensionDamping(suspensionDamping,wheelIndex);
	} else {
		return NULL;
	}
	Py_RETURN_NONE;
}

PyObject* KX_VehicleWrapper::PySetSuspensionCompression(PyObject* args)
{
	float suspensionCompression;
	int wheelIndex;

	if (PyArg_ParseTuple(args,"fi:setSuspensionCompression",&suspensionCompression,&wheelIndex))
	{
		m_vehicle->SetSuspensionCompression(suspensionCompression,wheelIndex);
	} else {
		return NULL;
	}
	Py_RETURN_NONE;
}

PyObject* KX_VehicleWrapper::PySetRollInfluence(PyObject* args)
{
	float rollInfluence;
	int wheelIndex;

	if (PyArg_ParseTuple(args,"fi:setRollInfluence",&rollInfluence,&wheelIndex))
	{
		m_vehicle->SetRollInfluence(rollInfluence,wheelIndex);
	}
	else {
		return NULL;
	}
	Py_RETURN_NONE;
}


PyObject* KX_VehicleWrapper::PyApplyBraking(PyObject* args)
{
	float braking;
	int wheelIndex;

	if (PyArg_ParseTuple(args,"fi:applyBraking",&braking,&wheelIndex))
	{
		m_vehicle->ApplyBraking(braking,wheelIndex);
	}
	else {
		return NULL;
	}
	Py_RETURN_NONE;
}




PyObject* KX_VehicleWrapper::PySetSteeringValue(PyObject* args)
{
	float steeringValue;
	int wheelIndex;

	if (PyArg_ParseTuple(args,"fi:setSteeringValue",&steeringValue,&wheelIndex))
	{
		m_vehicle->SetSteeringValue(steeringValue,wheelIndex);
	}
	else {
		return NULL;
	}
	Py_RETURN_NONE;
}


PyObject* KX_VehicleWrapper::PyGetConstraintType(PyObject* args)
{
	return PyLong_FromSsize_t(m_vehicle->GetUserConstraintType());
}





//python specific stuff
PyTypeObject KX_VehicleWrapper::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"KX_VehicleWrapper",
	sizeof(PyObjectPlus_Proxy),
	0,
	py_base_dealloc,
	0,
	0,
	0,
	0,
	py_base_repr,
	0,0,0,0,0,0,0,0,0,
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	0,0,0,0,0,0,0,
	Methods,
	0,
	0,
	&PyObjectPlus::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyMethodDef KX_VehicleWrapper::Methods[] = {
	{"addWheel",(PyCFunction) KX_VehicleWrapper::sPyAddWheel, METH_VARARGS},
	{"getNumWheels",(PyCFunction) KX_VehicleWrapper::sPyGetNumWheels, METH_VARARGS},
	{"getWheelOrientationQuaternion",(PyCFunction) KX_VehicleWrapper::sPyGetWheelOrientationQuaternion, METH_VARARGS},
	{"getWheelRotation",(PyCFunction) KX_VehicleWrapper::sPyGetWheelRotation, METH_VARARGS},
	{"getWheelPosition",(PyCFunction) KX_VehicleWrapper::sPyGetWheelPosition, METH_VARARGS},
	{"getConstraintId",(PyCFunction) KX_VehicleWrapper::sPyGetConstraintId, METH_VARARGS},
	{"getConstraintType",(PyCFunction) KX_VehicleWrapper::sPyGetConstraintType, METH_VARARGS},
	{"setSteeringValue",(PyCFunction) KX_VehicleWrapper::sPySetSteeringValue, METH_VARARGS},
	{"applyEngineForce",(PyCFunction) KX_VehicleWrapper::sPyApplyEngineForce, METH_VARARGS},
	{"applyBraking",(PyCFunction) KX_VehicleWrapper::sPyApplyBraking, METH_VARARGS},

	{"setTyreFriction",(PyCFunction) KX_VehicleWrapper::sPySetTyreFriction, METH_VARARGS},

	{"setSuspensionStiffness",(PyCFunction) KX_VehicleWrapper::sPySetSuspensionStiffness, METH_VARARGS},

	{"setSuspensionDamping",(PyCFunction) KX_VehicleWrapper::sPySetSuspensionDamping, METH_VARARGS},

	{"setSuspensionCompression",(PyCFunction) KX_VehicleWrapper::sPySetSuspensionCompression, METH_VARARGS},

	{"setRollInfluence",(PyCFunction) KX_VehicleWrapper::sPySetRollInfluence, METH_VARARGS},

	{NULL,NULL} //Sentinel
};

PyAttributeDef KX_VehicleWrapper::Attributes[] = {
	{ NULL }	//Sentinel
};

#endif // DISABLE_PYTHON
