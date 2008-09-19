/*
Bullet Continuous Collision Detection and Physics Library
Copyright (c) 2003-2006 Erwin Coumans  http://continuousphysics.com/Bullet/

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, 
including commercial applications, and to alter it and redistribute it freely, 
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

#ifndef CONTACT_SOLVER_INFO
#define CONTACT_SOLVER_INFO


struct btContactSolverInfo
{

	inline btContactSolverInfo()
	{
		m_tau = btScalar(0.6);
		m_damping = btScalar(1.0);
		m_friction = btScalar(0.3);
		m_restitution = btScalar(0.);
		m_maxErrorReduction = btScalar(20.);
		m_numIterations = 10;
		m_erp = btScalar(0.4);
		m_sor = btScalar(1.3);
	}

	btScalar	m_tau;
	btScalar	m_damping;
	btScalar	m_friction;
	btScalar	m_timeStep;
	btScalar	m_restitution;
	int		m_numIterations;
	btScalar	m_maxErrorReduction;
	btScalar	m_sor;
	btScalar	m_erp;

};

#endif //CONTACT_SOLVER_INFO
