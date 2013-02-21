
/** \file KX_VehicleWrapper.h
 *  \ingroup ketsji
 */

#ifndef __KX_VEHICLEWRAPPER_H__
#define __KX_VEHICLEWRAPPER_H__

#include "Value.h"
class PHY_IVehicle;
class PHY_IMotionState;

#include <vector>

///Python interface to physics vehicles (primarily 4-wheel cars and 2wheel bikes)
class	KX_VehicleWrapper : public PyObjectPlus
{
	Py_Header

	std::vector<PHY_IMotionState*> m_motionStates;

public:
	KX_VehicleWrapper(PHY_IVehicle* vehicle,class PHY_IPhysicsEnvironment* physenv);
	virtual ~KX_VehicleWrapper ();
	int			getConstraintId();
	
#ifdef WITH_PYTHON
	
	KX_PYMETHOD_VARARGS(KX_VehicleWrapper,AddWheel);
	KX_PYMETHOD_VARARGS(KX_VehicleWrapper,GetNumWheels);
	KX_PYMETHOD_VARARGS(KX_VehicleWrapper,GetWheelOrientationQuaternion);
	KX_PYMETHOD_VARARGS(KX_VehicleWrapper,GetWheelRotation);
	
	KX_PYMETHOD_VARARGS(KX_VehicleWrapper,GetWheelPosition);
	
	KX_PYMETHOD_VARARGS(KX_VehicleWrapper,GetConstraintId);
	KX_PYMETHOD_VARARGS(KX_VehicleWrapper,GetConstraintType);

	KX_PYMETHOD_VARARGS(KX_VehicleWrapper,SetSteeringValue);

	KX_PYMETHOD_VARARGS(KX_VehicleWrapper,ApplyEngineForce);

	KX_PYMETHOD_VARARGS(KX_VehicleWrapper,ApplyBraking);

	KX_PYMETHOD_VARARGS(KX_VehicleWrapper,SetTyreFriction);

	KX_PYMETHOD_VARARGS(KX_VehicleWrapper,SetSuspensionStiffness);
	
	KX_PYMETHOD_VARARGS(KX_VehicleWrapper,SetSuspensionDamping);
	
	KX_PYMETHOD_VARARGS(KX_VehicleWrapper,SetSuspensionCompression);
	
	KX_PYMETHOD_VARARGS(KX_VehicleWrapper,SetRollInfluence);
#endif  /* WITH_PYTHON */

private:
	PHY_IVehicle*			 m_vehicle;
	PHY_IPhysicsEnvironment* m_physenv;
};

#endif  /* __KX_VEHICLEWRAPPER_H__ */
