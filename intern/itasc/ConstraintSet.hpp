/*
 * ConstraintSet.hpp
 *
 *  Created on: Jan 5, 2009
 *      Author: rubensmits
 */

#ifndef CONSTRAINTSET_HPP_
#define CONSTRAINTSET_HPP_

#include "kdl/frames.hpp"
#include "eigen_types.hpp"
#include "Cache.hpp"
#include <vector>

namespace iTaSC {

enum ConstraintAction {
	ACT_NONE=		0,
	ACT_VALUE=		1,
	ACT_VELOCITY=	2,
	ACT_TOLERANCE=	4,
	ACT_FEEDBACK=	8,
	ACT_ALPHA=		16
};

struct ConstraintSingleValue {
	unsigned int id;	// identifier of constraint value, depends on constraint
	unsigned int action;// action performed, compbination of ACT_..., set on return
	const double y;		// actual constraint value
	const double ydot;	// actual constraint velocity
	double yd;			// current desired constraint value, changed on return
	double yddot;		// current desired constraint velocity, changed on return
	ConstraintSingleValue(): id(0), action(0), y(0.0), ydot(0.0) {}
};

struct ConstraintValues {
	unsigned int id;	// identifier of group of constraint values, depend on constraint
	unsigned short number;		// number of constraints in list
	unsigned short action;		// action performed, ACT_..., set on return
	double alpha;		// constraint activation coefficient, should be [0..1]
	double tolerance;	// current desired tolerance on constraint, same unit than yd, changed on return
	double feedback;	// current desired feedback on error, in 1/sec, changed on return
	struct ConstraintSingleValue* values;
	ConstraintValues(): id(0), number(0), action(0), values(NULL) {}
};

class ConstraintSet;
typedef bool (*ConstraintCallback)(const Timestamp& timestamp, struct ConstraintValues* const _values, unsigned int _nvalues, void* _param);

class ConstraintSet {
protected:
    unsigned int m_nc;
	e_scalar m_maxDeltaChi;
	e_matrix m_Cf;
    e_vector m_Wy,m_y,m_ydot;
	e_vector6 m_chi,m_chidot,m_S,m_temp,m_tdelta;
    e_matrix6 m_Jf,m_U,m_V,m_B,m_Jf_inv;
	KDL::Frame m_internalPose,m_externalPose;
    ConstraintCallback m_constraintCallback;
    void* m_constraintParam;
	void* m_poseParam;
    bool m_toggle;
	bool m_substep;
    double m_threshold;
    unsigned int m_maxIter;

	friend class Scene;
	virtual void modelUpdate(KDL::Frame& _external_pose,const Timestamp& timestamp);
    virtual void updateKinematics(const Timestamp& timestamp)=0;
    virtual void pushCache(const Timestamp& timestamp)=0;
    virtual void updateJacobian()=0;
    virtual void updateControlOutput(const Timestamp& timestamp)=0;
	virtual void initCache(Cache *_cache) = 0;
	virtual bool initialise(KDL::Frame& init_pose);
	virtual void reset(unsigned int nc,double accuracy,unsigned int maximum_iterations);
    virtual bool closeLoop();
	virtual double getMaxTimestep(double& timestep);


public:
    ConstraintSet(unsigned int nc,double accuracy,unsigned int maximum_iterations);
    ConstraintSet();
    virtual ~ConstraintSet();

	EIGEN_MAKE_ALIGNED_OPERATOR_NEW

	virtual bool registerCallback(ConstraintCallback _function, void* _param)
	{
		m_constraintCallback = _function;
		m_constraintParam = _param;
		return true;
	}

    virtual const e_vector& getControlOutput()const{return m_ydot;};
	virtual const ConstraintValues* getControlParameters(unsigned int* _nvalues) = 0;
	virtual bool setControlParameters(ConstraintValues* _values, unsigned int _nvalues, double timestep=0.0) = 0;
	bool setControlParameter(int id, ConstraintAction action, double value, double timestep=0.0);

    virtual const e_matrix6& getJf() const{return m_Jf;};
	virtual const KDL::Frame& getPose() const{return m_internalPose;};
    virtual const e_matrix& getCf() const{return m_Cf;};

    virtual const e_vector& getWy() const {return m_Wy;};
    virtual void setWy(const e_vector& Wy_in){m_Wy = Wy_in;};
    virtual void setJointVelocity(const e_vector chidot_in){m_chidot = chidot_in;};

    virtual unsigned int getNrOfConstraints(){return m_nc;};
	void substep(bool _substep) {m_substep=_substep;}
	bool substep() {return m_substep;}
};

}

#endif /* CONSTRAINTSET_HPP_ */
