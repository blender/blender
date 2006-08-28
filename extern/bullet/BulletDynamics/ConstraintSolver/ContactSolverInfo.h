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


struct ContactSolverInfo
{

	inline ContactSolverInfo()
	{
		m_tau = 0.6f;
		m_damping = 1.0f;
		m_friction = 0.3f;
		m_restitution = 0.f;
		m_maxErrorReduction = 20.f;
		m_numIterations = 10;
		m_erp = 0.4f;
		m_sor = 1.3f;
	}

	float	m_tau;
	float	m_damping;
	float	m_friction;
	float	m_timeStep;
	float	m_restitution;
	int		m_numIterations;
	float	m_maxErrorReduction;
	float	m_sor;
	float	m_erp;

};

#endif //CONTACT_SOLVER_INFO
