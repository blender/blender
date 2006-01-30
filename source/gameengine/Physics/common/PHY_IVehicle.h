#ifndef PHY_IVEHICLE_H
#define PHY_IVEHICLE_H

//PHY_IVehicle provides a generic interface for (raycast based) vehicles. Mostly targetting 4 wheel cars and 2 wheel motorbikes.

class PHY_IMotionState;
#include "PHY_DynamicTypes.h"

class PHY_IVehicle
{
public:

	virtual ~PHY_IVehicle();
	
	virtual void	AddWheel(
			PHY_Vector3	connectionPoint,
			PHY_Vector3	downDirection,
			PHY_Vector3	axleDirection,
			float	suspensionRestLength,
			float wheelRadius,
			bool hasSteering
		) = 0;


	virtual	int		GetNumWheels() const = 0;
	
	virtual const PHY_IMotionState*		GetWheelMotionState(int wheelIndex) const = 0;

	virtual int	GetUserConstraintId() const =0;
	virtual int	GetUserConstraintType() const =0;

	//some basic steering/braking/tuning/balancing (bikes)


};

#endif //PHY_IVEHICLE_H
