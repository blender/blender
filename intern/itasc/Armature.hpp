/* $Id$
 * Armature.hpp
 *
 *  Created on: Feb 3, 2009
 *      Author: benoitbolsee
 */

#ifndef ARMATURE_HPP_
#define ARMATURE_HPP_

#include "ControlledObject.hpp"
#include "ConstraintSet.hpp"
#include "kdl/TreeJntToJacSolver.hpp"
#include "kdl/treefksolverpos_recursive.hpp"
#include <vector>

namespace iTaSC {

class Armature: public iTaSC::ControlledObject {
public:
    Armature();
    virtual ~Armature();

	bool addSegment(const std::string& segment_name, const std::string& hook_name, const Joint& joint, double q_rest=0.0, const Frame& f_tip=F_identity, const Inertia& M = Inertia::Zero());
	// general purpose constraint on joint
	int addConstraint(const std::string& segment_name, ConstraintCallback _function, void* _param=NULL, bool _freeParam=false, bool _substep=false);
	// specific limit constraint on joint
	int addLimitConstraint(const std::string& segment_name, double _min, double _max, double _threshold, double _maxWeight=1000.0, double _slope=1.0);
	double getJoint(unsigned int joint);
	double getMaxJointChange(double timestep);
	bool getSegment(const std::string& segment_name, const Joint* &p_joint, double &q_rest, double &q, const Frame* &p_tip);
	bool getRelativeFrame(Frame& result, const std::string& segment_name, const std::string& base_name=m_root);

	virtual void finalize();

	virtual int addEndEffector(const std::string& name);
	virtual const Frame& getPose(const unsigned int end_effector);
    virtual void updateKinematics(const Timestamp& timestamp);
    virtual void updateControlOutput(const Timestamp& timestamp);
	virtual bool setControlParameter(unsigned int constraintId, unsigned int valueId, ConstraintAction action, double value, double timestep);
	virtual void initCache(Cache *_cache);
	virtual double getMaxTimestep(double& timestep);

	struct Effector_struct {
		std::string name;
		Frame pose;
		Effector_struct(const std::string& _name) {name = _name; pose = F_identity;}
	};
	typedef std::vector<Effector_struct> EffectorList;

	enum ID  {
		ID_JOINT=1,
	};
	struct JointConstraint_struct {
		SegmentMap::const_iterator segment;
		ConstraintSingleValue value;
		ConstraintValues values;
		ConstraintCallback function;
		void* param;
		bool freeParam;
		bool substep;
		JointConstraint_struct(SegmentMap::const_iterator _segment, ID _id, ConstraintCallback _function, void* _param, bool _freeParam, bool _substep) :
			segment(_segment), value(), values(), function(_function), param(_param), freeParam(_freeParam), substep(_substep)
			{
				values.feedback = 20.0;
				value.id = _id;
				values.id = _id;
				values.tolerance = 1.0;
				value.yddot = 0.0;
				value.yd = 0.0;
				values.alpha = 0.0;
				values.number = 1;
				values.values = &value;
			}
		~JointConstraint_struct()
			{
				if (freeParam && param)
					free(param);
			}
	};
	typedef std::vector<JointConstraint_struct*> JointConstraintList;	

	typedef std::vector<double> JointList;
	
protected:
    virtual void updateJacobian();

private:
	static std::string m_root;
    Tree m_tree;
	unsigned int m_njoint;
	unsigned int m_nconstraint;
	unsigned int m_neffector;
	bool m_finalized;
	Cache* m_cache;
	double *m_buf;
	int m_qCCh;
	CacheTS m_qCTs;
	int m_yCCh;
	CacheTS m_yCTs;
    JntArray m_qKdl;
    JntArray m_qdotKdl;
    Jacobian* m_jac;
	KDL::TreeJntToJacSolver* m_jacsolver;
	KDL::TreeFkSolverPos_recursive* m_fksolver;
	EffectorList m_effectors;
	JointConstraintList m_constraints;
	JointList m_joints;

	void pushQ(CacheTS timestamp);
	bool popQ(CacheTS timestamp);
	void pushConstraints(CacheTS timestamp);
	bool popConstraints(CacheTS timestamp);

	struct LimitConstraintParam_struct {
		double min;
		double max;
		double threshold;
		double maxThreshold;
		double minThreshold;
		double maxWeight;
		double slope;
	};
	static bool JointLimitCallback(const Timestamp& timestamp, struct ConstraintValues* const _values, unsigned int _nvalues, void* _param);
};

}

#endif /* ARMATURE_HPP_ */
