
#ifndef MASS_PROPS_H
#define MASS_PROPS_H

#include <SimdVector3.h>

struct MassProps {
	MassProps(float mass,const SimdVector3& inertiaLocal):
	m_mass(mass),
		m_inertiaLocal(inertiaLocal)
	{
	}
	float   m_mass;
	SimdVector3	m_inertiaLocal;
};


#endif