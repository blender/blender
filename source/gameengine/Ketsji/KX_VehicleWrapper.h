#ifndef KX_VEHICLE_WRAPPER
#define KX_VEHICLE_WRAPPER

#include "Value.h"
#include "PHY_DynamicTypes.h"
class PHY_IVehicle;

///Python interface to physics vehicles (primarily 4-wheel cars and 2wheel bikes)
class	KX_VehicleWrapper : public PyObjectPlus
{
	Py_Header;
	virtual PyObject*		_getattr(const STR_String& attr);
	virtual int 			_setattr(const STR_String& attr, PyObject *value);
public:
	KX_VehicleWrapper(PHY_IVehicle* vehicle,class PHY_IPhysicsEnvironment* physenv,PyTypeObject *T = &Type);
	virtual ~KX_VehicleWrapper ();
	int			getConstraintId();
	
	
	KX_PYMETHOD(KX_VehicleWrapper,AddWheel);
	KX_PYMETHOD(KX_VehicleWrapper,GetNumWheels);
	KX_PYMETHOD(KX_VehicleWrapper,GetWheelsTransform);
	
	KX_PYMETHOD(KX_VehicleWrapper,GetConstraintId);
	KX_PYMETHOD(KX_VehicleWrapper,GetConstraintType);


private:
	PHY_IVehicle*			 m_vehicle;
	PHY_IPhysicsEnvironment* m_physenv;
};

#endif //KX_VEHICLE_WRAPPER
