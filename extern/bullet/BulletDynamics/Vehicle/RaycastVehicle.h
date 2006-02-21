/*
 * Copyright (c) 2005 Erwin Coumans http://continuousphysics.com/Bullet/
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies.
 * Erwin Coumans makes no representations about the suitability 
 * of this software for any purpose.  
 * It is provided "as is" without express or implied warranty.
*/
#ifndef RAYCASTVEHICLE_H
#define RAYCASTVEHICLE_H

#include "Dynamics/RigidBody.h"
#include "ConstraintSolver/TypedConstraint.h"

struct MassProps;
#include "WheelInfo.h"

struct	VehicleRaycaster;
class VehicleTuning;

///Raycast vehicle, very special constraint that turn a rigidbody into a vehicle.
class RaycastVehicle : public TypedConstraint
{
public:
	class VehicleTuning
		{
			public:

			VehicleTuning()
				:m_suspensionStiffness(5.88f),
				m_suspensionCompression(0.83f),
				m_suspensionDamping(0.88f),
				m_maxSuspensionTravelCm(500.f),
				m_frictionSlip(10.5f)
			{
			}
			float	m_suspensionStiffness;
			float	m_suspensionCompression;
			float	m_suspensionDamping;
			float	m_maxSuspensionTravelCm;
			float	m_frictionSlip;

		};
private:

	SimdScalar	m_tau;
	SimdScalar	m_damping;
	VehicleRaycaster*	m_vehicleRaycaster;
	float		m_pitchControl;
	float	m_steeringValue; 
	float m_currentVehicleSpeedKmHour;

	RigidBody* m_chassisBody;

	int m_indexRightAxis;
	int m_indexUpAxis;
	int	m_indexForwardAxis;

	void DefaultInit(const VehicleTuning& tuning);

public:

	//constructor to create a car from an existing rigidbody
	RaycastVehicle(const VehicleTuning& tuning,RigidBody* chassis,	VehicleRaycaster* raycaster );

	virtual ~RaycastVehicle() ;

		



	SimdScalar Raycast(WheelInfo& wheel);

	virtual void UpdateVehicle(SimdScalar step);

	void ResetSuspension();

	SimdScalar	GetSteeringValue(int wheel) const;

	void	SetSteeringValue(SimdScalar steering,int wheel);


	void	ApplyEngineForce(SimdScalar force, int wheel);

	const SimdTransform&	GetWheelTransformWS( int wheelIndex ) const;

	void	UpdateWheelTransform( int wheelIndex );
	
	void	SetRaycastWheelInfo( int wheelIndex , bool isInContact, const SimdVector3& hitPoint, const SimdVector3& hitNormal,SimdScalar depth);

	WheelInfo&	AddWheel( const SimdVector3& connectionPointCS0, const SimdVector3& wheelDirectionCS0,const SimdVector3& wheelAxleCS,SimdScalar suspensionRestLength,SimdScalar wheelRadius,const VehicleTuning& tuning, bool isFrontWheel);

	inline int		GetNumWheels() const {
		return m_wheelInfo.size();
	}
	
	std::vector<WheelInfo>	m_wheelInfo;


	const WheelInfo&	GetWheelInfo(int index) const;

	WheelInfo&	GetWheelInfo(int index);

	void	UpdateWheelTransformsWS(WheelInfo& wheel );

	
	void SetBrake(float brake,int wheelIndex);

	void	SetPitchControl(float pitch)
	{
		m_pitchControl = pitch;
	}
	
	void	UpdateSuspension(SimdScalar deltaTime);

	void	UpdateFriction(SimdScalar	timeStep);



	inline RigidBody* GetRigidBody()
	{
		return m_chassisBody;
	}

	const RigidBody* GetRigidBody() const
	{
		return m_chassisBody;
	}

	inline int	GetRightAxis() const
	{
		return m_indexRightAxis;
	}
	inline int GetUpAxis() const
	{
		return m_indexUpAxis;
	}

	inline int GetForwardAxis() const
	{
		return m_indexForwardAxis;
	}

};

#endif //RAYCASTVEHICLE_H