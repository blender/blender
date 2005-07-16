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
#define USE_SOR_SOLVER
#ifdef USE_SOR_SOLVER

#ifndef SOR_LCP_H
#define SOR_LCP_H
class RigidBody;
class BU_Joint;
#include "SimdScalar.h"

struct ContactSolverInfo;

void SolveInternal1 (float global_cfm,
					 float global_erp,
					 RigidBody * const *body, int nb,
		     BU_Joint * const *_joint, int nj, const ContactSolverInfo& info);

int dRandInt2 (int n);


#endif //SOR_LCP_H

#endif //USE_SOR_SOLVER