/**
 * $Id$
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef NAN_INCLUDED_LOD_Quadric_h
#define NAN_INCLUDED_LOD_Quadric_h

#include "MT_Vector3.h"
#include "MT_Matrix3x3.h"


class LOD_Quadric {

private:
    MT_Scalar a2, ab, ac, ad;
    MT_Scalar     b2, bc, bd;
    MT_Scalar         c2, cd;
    MT_Scalar             d2;

    void init(MT_Scalar a, MT_Scalar b, MT_Scalar c, MT_Scalar d);

public:

    LOD_Quadric(
	) {
		Clear();
	};

    LOD_Quadric(
		const MT_Vector3 & vec,
		const MT_Scalar & offset
	) {
		a2 = vec[0] *vec[0];
		b2 = vec[1] *vec[1];
		c2 = vec[2] *vec[2];

		ab = vec[0]*vec[1];
		ac = vec[0]*vec[2];
		bc = vec[1]*vec[2];

		MT_Vector3 temp = vec*offset;
		ad = temp[0];
		bd = temp[1];
		cd = temp[2];

		d2 = offset*offset;
	};

		MT_Matrix3x3 
	Tensor(
	) const {
		// return a symmetric matrix 

		return MT_Matrix3x3(
			a2,ab,ac,
			ab,b2,bc,
			ac,bc,c2
		);
	};


		MT_Vector3
	Vector(
	) const {
		return MT_Vector3(ad, bd, cd);
	};

		void 
	Clear(
		MT_Scalar val=0.0
	) {
		a2=ab=ac=ad=b2=bc=bd=c2=cd=d2=val;
	};

		LOD_Quadric & 
	operator=(
		const LOD_Quadric& Q
	) {

		a2 = Q.a2;  ab = Q.ab;  ac = Q.ac;  ad = Q.ad;
					b2 = Q.b2;  bc = Q.bc;  bd = Q.bd;
								c2 = Q.c2;  cd = Q.cd;  
											d2 = Q.d2;
		return *this;
	};
		
		LOD_Quadric& 
	operator+=(
		const LOD_Quadric& Q
	) {
		a2 += Q.a2; ab += Q.ab;  ac += Q.ac;  ad += Q.ad;
					b2 += Q.b2;  bc += Q.bc;  bd += Q.bd;
								 c2 += Q.c2;  cd += Q.cd;  
											  d2 += Q.d2;
		return *this;
	};

		LOD_Quadric& 
	operator*=(
		const MT_Scalar & s
	) {
		a2 *= s; ab *= s;  ac *= s;  ad *= s;
					b2 *= s;  bc *= s;  bd *= s;
								 c2 *= s;  cd *= s;  
											  d2 *= s;
		return *this;
	};


		MT_Scalar 
	Evaluate(
		const MT_Vector3 &v
	) const {
		// compute the LOD_Quadric error

		return v[0]*v[0]*a2 + 2*v[0]*v[1]*ab + 2*v[0]*v[2]*ac + 2*v[0]*ad
	          +v[1]*v[1]*b2 + 2*v[1]*v[2]*bc + 2*v[1]*bd
	          +v[2]*v[2]*c2   + 2*v[2]*cd
	          + d2;
	};
		
		bool 
	Optimize(
		MT_Vector3& v
	) const {
		
		MT_Scalar det = Tensor().determinant();
		if (MT_fuzzyZero(det)) {
			return false;
		}
		
		v = -((Tensor().inverse()) * Vector());
		return true;
	}; 

};

#endif

