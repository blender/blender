/*
 * CopyPose.h
 *
 *  Created on: Mar 17, 2009
 *      Author: benoit bolsee
 */

#ifndef COPYPOSE_H_
#define COPYPOSE_H_

#include "ConstraintSet.hpp"
namespace iTaSC{

using namespace KDL;

class CopyPose: public iTaSC::ConstraintSet
{
protected:
    virtual void updateKinematics(const Timestamp& timestamp);
    virtual void pushCache(const Timestamp& timestamp);
    virtual void updateJacobian();
    virtual bool initialise(Frame& init_pose);
    virtual void initCache(Cache *_cache);
    virtual void updateControlOutput(const Timestamp& timestamp);
	virtual void modelUpdate(Frame& _external_pose,const Timestamp& timestamp);
	virtual double getMaxTimestep(double& timestep);

public:
    enum ID {		// constraint ID in callback and setControlParameter
		ID_POSITION=0,
        ID_POSITIONX=1,
        ID_POSITIONY=2,
        ID_POSITIONZ=3,
		ID_ROTATION=4,
        ID_ROTATIONX=5,
        ID_ROTATIONY=6,
        ID_ROTATIONZ=7,
    };
	enum CTL {		// control ID in constructor to specify which output is constrainted
		CTL_NONE=0x00,
        CTL_POSITIONX=0x01,		// the bit order is important: it matches the y output order
        CTL_POSITIONY=0x02,
        CTL_POSITIONZ=0x04,
		CTL_POSITION=0x07,
        CTL_ROTATIONX=0x08,
        CTL_ROTATIONY=0x10,
        CTL_ROTATIONZ=0x20,
		CTL_ROTATION=0x38,
		CTL_ALL=0x3F,
	};

	// use a combination of CTL_.. in control_output to specify which 
    CopyPose(unsigned int control_output=CTL_ALL, unsigned int dynamic_output=CTL_NONE, double armlength=1.0, double accuracy=1e-6, unsigned int maximum_iterations=100);
    virtual ~CopyPose();

    virtual bool setControlParameters(struct ConstraintValues* _values, unsigned int _nvalues, double timestep);
    virtual const ConstraintValues* getControlParameters(unsigned int* _nvalues);

private:
    struct ConstraintSingleValue m_posData[3];	// index = controlled output in X,Y,Z order
    struct ConstraintSingleValue m_rotData[3];
    struct ConstraintValues m_values[2];		// index = group of controlled output, in position, rotation order
    Cache* m_cache;
    int m_poseCCh;
    CacheTS m_poseCTs;
	unsigned int m_poseCacheSize;
	unsigned int m_outputDynamic;	// combination of CTL_... determine which variables are dynamically controlled by the application
	unsigned int m_outputControl;	// combination of CTL_... determine which output are constrained
	unsigned int m_nvalues;		// number of elements used in m_values[]
	double m_maxerror;

	struct ControlState {
		int firsty;			// first y index
		int ny;				// number of y in output
		double alpha;
		double K;
		double tolerance;
		struct ControlValue {
			double yddot;
			double yd;
			double nextyd;
			double nextyddot;
		} output[3];		// inded numbex = same as m_rotData
	} m_rot, m_pos;

    void pushPose(CacheTS timestamp);
    bool popPose(CacheTS timestamp);
	int nBitsOn(unsigned int v)
		{ int n=0; while(v) { if (v&1) n++; v>>=1; } return n; }
	double* restoreValues(double* item, ConstraintValues* _values, ControlState* _state, unsigned int mask);
	double* pushValues(double* item, ControlState* _state, unsigned int mask);
	void updateState(ConstraintValues* _values, ControlState* _state, unsigned int mask, double timestep);
	void updateValues(Vector& vel, ConstraintValues* _values, ControlState* _state, unsigned int mask);
	void updateOutput(Vector& vel, ControlState* _state, unsigned int mask);
	void interpolateOutput(ControlState* _state, unsigned int mask, const Timestamp& timestamp);

};
}
#endif /* COPYROTATION_H_ */
