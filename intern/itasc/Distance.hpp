/* $Id: Distance.hpp 19905 2009-04-23 13:29:54Z ben2610 $
 * Distance.hpp
 *
 *  Created on: Jan 30, 2009
 *      Author: rsmits
 */

#ifndef DISTANCE_HPP_
#define DISTANCE_HPP_

#include "ConstraintSet.hpp"
#include "kdl/chain.hpp"
#include "kdl/chainfksolverpos_recursive.hpp"
#include "kdl/chainjnttojacsolver.hpp"

namespace iTaSC
{

class Distance: public iTaSC::ConstraintSet
{
protected:
    virtual void updateKinematics(const Timestamp& timestamp);
    virtual void pushCache(const Timestamp& timestamp);
    virtual void updateJacobian();
    virtual bool initialise(Frame& init_pose);
	virtual void initCache(Cache *_cache);
    virtual void updateControlOutput(const Timestamp& timestamp);
	virtual bool closeLoop();

public:
	enum ID {
		ID_DISTANCE=1,
	};
    Distance(double armlength=1.0, double accuracy=1e-6, unsigned int maximum_iterations=100);
    virtual ~Distance();

	virtual bool setControlParameters(struct ConstraintValues* _values, unsigned int _nvalues, double timestep);
	virtual const ConstraintValues* getControlParameters(unsigned int* _nvalues);

private:
	bool computeChi(Frame& pose);
    KDL::Chain m_chain;
    KDL::ChainFkSolverPos_recursive* m_fksolver;
    KDL::ChainJntToJacSolver* m_jacsolver;
    KDL::JntArray m_chiKdl;
    KDL::Jacobian m_jac;
	struct ConstraintSingleValue m_data;
	struct ConstraintValues m_values;
	Cache* m_cache;
	int m_distCCh;
	CacheTS m_distCTs;
	double m_maxerror;

	void pushDist(CacheTS timestamp);
	bool popDist(CacheTS timestamp);

    double m_alpha,m_yddot,m_yd,m_nextyd,m_nextyddot,m_K,m_tolerance;
};

}

#endif /* DISTANCE_HPP_ */
