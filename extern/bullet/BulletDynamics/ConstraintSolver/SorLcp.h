/*************************************************************************
 *                                                                       *
 * Open Dynamics Engine, Copyright (C) 2001,2002 Russell L. Smith.       *
 * All rights reserved.  Email: russ@q12.org   Web: www.q12.org          *
 *                                                                       *
 * This library is free software; you can redistribute it and/or         *
 * modify it under the terms of EITHER:                                  *
 *   (1) The GNU Lesser General Public License as published by the Free  *
 *       Software Foundation; either version 2.1 of the License, or (at  *
 *       your option) any later version. The text of the GNU Lesser      *
 *       General Public License is included with this library in the     *
 *       file LICENSE.TXT.                                               *
 *   (2) The BSD-style license that is included with this library in     *
 *       the file LICENSE-BSD.TXT.                                       *
 *                                                                       *
 * This library is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the files    *
 * LICENSE.TXT and LICENSE-BSD.TXT for more details.                     *
 *                                                                       *
 *************************************************************************/

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

