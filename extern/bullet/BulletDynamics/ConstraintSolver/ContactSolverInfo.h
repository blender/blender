/*
 * Copyright (c) 2005 Erwin Coumans http://www.erwincoumans.com
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies.
 * Erwin Coumans makes no representations about the suitability 
 * of this software for any purpose.  
 * It is provided "as is" without express or implied warranty.
*/
#ifndef CONTACT_SOLVER_INFO
#define CONTACT_SOLVER_INFO


struct ContactSolverInfo
{

	inline ContactSolverInfo()
	{
		m_tau = 0.4f;
		m_damping = 0.9f;
		m_friction = 0.3f;
		m_restitution = 0.f;
		m_maxErrorReduction = 20.f;
		m_numIterations = 10;
	}

	float	m_tau;
	float	m_damping;
	float	m_friction;
	float	m_timeStep;
	float	m_restitution;
	int		m_numIterations;
	float	m_maxErrorReduction;


};

#endif //CONTACT_SOLVER_INFO
