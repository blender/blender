#ifndef SM_MOTIONSTATE_H
#define SM_MOTIONSTATE_H

#include "MT_Transform.h"

class SM_MotionState {
public:
	SM_MotionState() :
		m_pos(0.0, 0.0, 0.0),
		m_orn(0.0, 0.0, 0.0, 1.0),
		m_lin_vel(0.0, 0.0, 0.0),
		m_ang_vel(0.0, 0.0, 0.0)
		{}

	void setPosition(const MT_Point3& pos)             { m_pos = pos; }
	void setOrientation(const MT_Quaternion& orn)      { m_orn = orn; }
	void setLinearVelocity(const MT_Vector3& lin_vel)  { m_lin_vel = lin_vel; }
	void setAngularVelocity(const MT_Vector3& ang_vel) { m_ang_vel = ang_vel; }
	
	const MT_Point3&     getPosition()        const { return m_pos; }
	const MT_Quaternion& getOrientation()     const { return m_orn; }
	const MT_Vector3&    getLinearVelocity()  const { return m_lin_vel; }
	const MT_Vector3&    getAngularVelocity() const { return m_ang_vel;	}
	
	virtual MT_Transform getTransform() const {
		return MT_Transform(m_pos, m_orn);
	}

protected:
	MT_Point3         m_pos;
	MT_Quaternion     m_orn;
	MT_Vector3        m_lin_vel;
	MT_Vector3        m_ang_vel;
};
	
#endif

