#ifndef __KX_MATERIALIPOCONTROLLER_H__
#define __KX_MATERIALIPOCONTROLLER_H__



#include "SG_Controller.h"
#include "SG_Spatial.h"
#include "KX_IInterpolator.h"

#include "STR_String.h" //typedef dword

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
	dword				m_matname_hash;
public:
	KX_MaterialIpoController(dword matname_hash) : 
				m_modified(true),
				m_ipotime(0.0),
				m_matname_hash(matname_hash)
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


#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new(size_t num_bytes) { return MEM_mallocN(num_bytes, "GE:KX_MaterialIpoController"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};




#endif//__KX_MATERIALIPOCONTROLLER_H__
