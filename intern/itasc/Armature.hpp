/* 
 * Armature.hpp
 *
 *  Created on: Feb 3, 2009
 *      Author: benoitbolsee
 */

#ifndef ARMATURE_HPP_
#define ARMATURE_HPP_

#include "ControlledObject.hpp"
#include "ConstraintSet.hpp"
#include "kdl/treejnttojacsolver.hpp"
#include "kdl/treefksolverpos_recursive.hpp"
#include <vector>

namespace iTaSC {

class Armature: public iTaSC::ControlledObject {
public:
    Armature();
    virtual ~Armature();

	bool addSegment(const std::string& segment_name, const std::string& hook_name, const Joint& joint, const double& q_rest, const Frame& f_tip=F_identity, const Inertia& M = Inertia::Zero());
	// general purpose constraint on joint
	int addConstraint(const std::string& segment_name, ConstraintCallback _function, void* _param=NULL, bool _freeParam=false, bool _substep=false);
	// specific limit constraint on joint
	int addLimitConstraint(const std::string& segment_name, unsigned int dof, double _min, double _max);
	double getMaxJointChange();
	double getMaxEndEffectorChange();
	bool getSegment(const std::string& segment_name, const unsigned int q_size, const Joint* &p_joint, double &q_rest, double &q, const Frame* &p_tip);
	bool getRelativeFrame(Frame& result, const std::string& segment_name, const std::string& base_name=m_root);

	virtual bool finalize();

	virtual int addEndEffector(const std::string& name);
	virtual const Frame& getPose(const unsigned int end_effector);
	virtual bool updateJoint(const Timestamp& timestamp, JointLockCallback& callback);
    virtual void updateKinematics(const Timestamp& timestamp);
    virtual void pushCache(const Timestamp& timestamp);
    virtual void updateControlOutput(const Timestamp& timestamp);
	virtual bool setControlParameter(unsigned int constraintId, unsigned int valueId, ConstraintAction action, double value, double timestep=0.0);
	virtual void initCache(Cache *_cache);
	virtual bool setJointArray(const KDL::JntArray& joints);
	virtual const KDL::JntArray& getJointArray();

	virtual double getArmLength()
	{
		return m_armlength;
	}

	struct Effector_struct {
		std::string name;
		Frame oldpose;
		Frame pose;
		Effector_struct(const std::string& _name) {name = _name; oldpose = pose = F_identity;}
	};
	typedef std::vector<Effector_struct> EffectorList;

	enum ID  {
		ID_JOINT=1,
		ID_JOINT_RX=2,
		ID_JOINT_RY=3,
		ID_JOINT_RZ=4,
		ID_JOINT_TX=2,
		ID_JOINT_TY=3,
		ID_JOINT_TZ=4,
	};
	struct JointConstraint_struct {
		SegmentMap::const_iterator segment;
		ConstraintSingleValue value[3];
		ConstraintValues values[3];
		ConstraintCallback function;
		unsigned int v_nr;
		unsigned int y_nr;	// first coordinate of constraint in Y vector
		void* param;
		bool freeParam;
		bool substep;
		JointConstraint_struct(SegmentMap::const_iterator _segment, unsigned int _y_nr, ConstraintCallback _function, void* _param, bool _freeParam, bool _substep);
		~JointConstraint_struct();
	};
	typedef std::vector<JointConstraint_struct*> JointConstraintList;	

	struct Joint_struct {
		KDL::Joint::JointType	type;
		unsigned short			ndof;
		bool					useLimit;
		bool					locked;
		double					rest;
		double					min;
		double					max;

		Joint_struct(KDL::Joint::JointType _type, unsigned int _ndof, double _rest) :
			type(_type), ndof(_ndof), rest(_rest)  { useLimit=locked=false; min=0.0; max=0.0; }
	};
	typedef std::vector<Joint_struct> JointList;
	
protected:
    virtual void updateJacobian();

private:
	static std::string m_root;
    Tree m_tree;
	unsigned int m_njoint;
	unsigned int m_nconstraint;
	unsigned int m_noutput;
	unsigned int m_neffector;
	bool m_finalized;
	Cache* m_cache;
	double *m_buf;
	int m_qCCh;
	CacheTS m_qCTs;
	int m_yCCh;
#if 0
	CacheTS m_yCTs;
#endif
    JntArray m_qKdl;
    JntArray m_oldqKdl;
    JntArray m_newqKdl;
    JntArray m_qdotKdl;
    Jacobian* m_jac;
	double m_armlength;

	KDL::TreeJntToJacSolver* m_jacsolver;
	KDL::TreeFkSolverPos_recursive* m_fksolver;
	EffectorList m_effectors;
	JointConstraintList m_constraints;
	JointList m_joints;

	void pushQ(CacheTS timestamp);
	bool popQ(CacheTS timestamp);
	//void pushConstraints(CacheTS timestamp);
	//bool popConstraints(CacheTS timestamp);

};

}

#endif /* ARMATURE_HPP_ */
