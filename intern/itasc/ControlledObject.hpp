/*
 * ControlledObject.hpp
 *
 *  Created on: Jan 5, 2009
 *      Author: rubensmits
 */

#ifndef CONTROLLEDOBJECT_HPP_
#define CONTROLLEDOBJECT_HPP_

#include "kdl/frames.hpp"
#include "eigen_types.hpp"

#include "Object.hpp"
#include "ConstraintSet.hpp"
#include <vector>

namespace iTaSC {

#define CONSTRAINT_ID_ALL	((unsigned int)-1)

class ControlledObject : public Object {
protected:
	e_scalar m_maxDeltaQ;
    unsigned int m_nq,m_nc,m_nee;
    e_matrix m_Wq,m_Cq;
    e_vector m_Wy,m_ydot,m_qdot;
	std::vector<e_matrix> m_JqArray;
public:
    ControlledObject();
    virtual ~ControlledObject();

	class JointLockCallback {
	public:
		JointLockCallback() {}
		virtual ~JointLockCallback() {}

		// lock a joint, no need to update output
		virtual void lockJoint(unsigned int q_nr, unsigned int ndof) = 0;
		// lock a joint and update output in view of reiteration
		virtual void lockJoint(unsigned int q_nr, unsigned int ndof, double* qdot) = 0;
	};

	virtual void initialize(unsigned int _nq,unsigned int _nc, unsigned int _nee);

	// returns true when a joint has been locked via the callback and the solver must run again
	virtual bool updateJoint(const Timestamp& timestamp, JointLockCallback& callback) = 0;
	virtual void updateControlOutput(const Timestamp& timestamp)=0;
    virtual void setJointVelocity(const e_vector qdot_in){m_qdot = qdot_in;};
	virtual double getMaxTimestep(double& timestep);
	virtual bool setControlParameter(unsigned int constraintId, unsigned int valueId, ConstraintAction action, e_scalar value, double timestep=0.0)=0;

    virtual const e_vector& getControlOutput() const{return m_ydot;}

	virtual const e_matrix& getJq(unsigned int ee) const;

    virtual const e_matrix& getCq() const{return m_Cq;};

    virtual e_matrix& getWq() {return m_Wq;};
    virtual void setWq(const e_matrix& Wq_in){m_Wq = Wq_in;};

    virtual const e_vector& getWy() const {return m_Wy;};

    virtual const unsigned int getNrOfCoordinates(){return m_nq;};
    virtual const unsigned int getNrOfConstraints(){return m_nc;};
};

}

#endif /* CONTROLLEDOBJECT_HPP_ */
