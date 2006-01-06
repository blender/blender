#ifndef __KX_MATERIALIPOCONTROLLER_H__
#define __KX_MATERIALIPOCONTROLLER_H__



#include "SG_Controller.h"
#include "SG_Spatial.h"
#include "KX_IInterpolator.h"

class KX_MaterialIpoController : public SG_Controller
{
public:
	MT_Vector4			m_rgba;
	MT_Vector3			m_specrgb;
	MT_Scalar			m_hard;
	MT_Scalar			m_spec;
	MT_Scalar			m_ref;
	MT_Scalar			m_emit;
	MT_Scalar			m_alpha;

private:
	T_InterpolatorList	m_interpolators;
	bool				m_modified;

	double		        m_ipotime;
public:
	KX_MaterialIpoController() : 
				m_modified(true),
				m_ipotime(0.0)
		{}
	virtual ~KX_MaterialIpoController();
	virtual	SG_Controller*	GetReplica(class SG_Node* destnode);
	virtual bool Update(double time);
	virtual void SetSimulatedTime(double time) {
		m_ipotime = time;
		m_modified = true;
	}
	
		void
	SetOption(
		int option,
		int value
	){
		// intentionally empty
	};


	void	AddInterpolator(KX_IInterpolator* interp);
};




#endif//__KX_MATERIALIPOCONTROLLER_H__
