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

#ifndef _ODE_CONTACT_H_
#define _ODE_CONTACT_H_

#include <ode/common.h>

#ifdef __cplusplus
extern "C" {
#endif


enum {
  dContactMu2		= 0x001,
  dContactFDir1		= 0x002,
  dContactBounce	= 0x004,
  dContactSoftERP	= 0x008,
  dContactSoftCFM	= 0x010,
  dContactMotion1	= 0x020,
  dContactMotion2	= 0x040,
  dContactSlip1		= 0x080,
  dContactSlip2		= 0x100,

  dContactApprox0	= 0x0000,
  dContactApprox1_1	= 0x1000,
  dContactApprox1_2	= 0x2000,
  dContactApprox1	= 0x3000
};


typedef struct dSurfaceParameters {
  /* must always be defined */
  int mode;
  dReal mu;

  /* only defined if the corresponding flag is set in mode */
  dReal mu2;
  dReal bounce;
  dReal bounce_vel;
  dReal soft_erp;
  dReal soft_cfm;
  dReal motion1,motion2;
  dReal slip1,slip2;
} dSurfaceParameters;


/* contact info set by collision functions */

typedef struct dContactGeom {
  dVector3 pos;
  dVector3 normal;
  dReal depth;
  dGeomID g1,g2;
} dContactGeom;


/* contact info used by contact joint */

typedef struct dContact {
  dSurfaceParameters surface;
  dContactGeom geom;
  dVector3 fdir1;
} dContact;


#ifdef __cplusplus
}
#endif

#endif
