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

#ifndef _ODE_MASS_H_
#define _ODE_MASS_H_

#include <ode/common.h>

#ifdef __cplusplus
extern "C" {
#endif

struct dMass;
typedef struct dMass dMass;


void dMassSetZero (dMass *);

void dMassSetParameters (dMass *, dReal themass,
			 dReal cgx, dReal cgy, dReal cgz,
			 dReal I11, dReal I22, dReal I33,
			 dReal I12, dReal I13, dReal I23);

void dMassSetSphere (dMass *, dReal density, dReal radius);

void dMassSetCappedCylinder (dMass *, dReal density, int direction,
			     dReal a, dReal b);

void dMassSetBox (dMass *, dReal density,
		  dReal lx, dReal ly, dReal lz);

void dMassAdjust (dMass *, dReal newmass);

void dMassTranslate (dMass *, dReal x, dReal y, dReal z);

void dMassRotate (dMass *, const dMatrix3 R);

void dMassAdd (dMass *a, const dMass *b);



struct dMass {
  dReal mass;
  dVector4 c;
  dMatrix3 I;

#ifdef __cplusplus
  dMass()
    { dMassSetZero (this); }
  void setZero()
    { dMassSetZero (this); }
  void setParameters (dReal themass, dReal cgx, dReal cgy, dReal cgz,
		      dReal I11, dReal I22, dReal I33,
		      dReal I12, dReal I13, dReal I23)
    { dMassSetParameters (this,themass,cgx,cgy,cgz,I11,I22,I33,I12,I13,I23); }
  void setSphere (dReal density, dReal radius)
    { dMassSetSphere (this,density,radius); }
  void setCappedCylinder (dReal density, int direction, dReal a, dReal b)
    { dMassSetCappedCylinder (this,density,direction,a,b); }
  void setBox (dReal density, dReal lx, dReal ly, dReal lz)
    { dMassSetBox (this,density,lx,ly,lz); }
  void adjust (dReal newmass)
    { dMassAdjust (this,newmass); }
  void translate (dReal x, dReal y, dReal z)
    { dMassTranslate (this,x,y,z); }
  void rotate (const dMatrix3 R)
    { dMassRotate (this,R); }
  void add (const dMass *b)
    { dMassAdd (this,b); }
#endif
};


#ifdef __cplusplus
}
#endif

#endif
