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

#ifndef MT_PLANE3
#define MT_PLANE3

#include "MT_Tuple4.h"
#include "MT_Point3.h"

/**
 * A simple 3d plane class.
 *
 * This class represents a plane in 3d. The internal parameterization used
 * is n.x + d =0 where n is a unit vector and d is a scalar.
 *
 * It inherits data from MT_Tuple4 please see this class for low level
 * access to the internal representation.
 * 
 */

class MT_Plane3 : public MT_Tuple4
{
public :
	/**
	 * Constructor from 3 points
	 */

	MT_Plane3(
		const MT_Vector3 &a,
		const MT_Vector3 &b,
		const MT_Vector3 &c
	);
	/**
	 * Construction from vector and a point.
	 */

	MT_Plane3(
		const MT_Vector3 &n,
		const MT_Vector3 &p
	);

	/**
	 * Default constructor
	 */
	MT_Plane3(
	);

	/**
	 * Default constructor
	 */

	MT_Plane3(
		const MT_Plane3 & p
	):
		MT_Tuple4(p)
	{
	}

	/**
	 * Return plane normal
	 */
	
		MT_Vector3
	Normal(
	) const;

	/**
	 * Return plane scalar i.e the d from n.x + d = 0
	 */

		MT_Scalar
	Scalar(
	) const ; 

	/**
	 * Invert the plane - just swaps direction of normal.
	 */
		void
	Invert(
	);
	
	/**
	 * Assignment operator
	 */

		MT_Plane3 &
	operator = (
		const MT_Plane3 & rhs
	);

	/**
	 * Return the signed perpendicular distance from a point to the plane
	 */

		MT_Scalar
	signedDistance(
		const MT_Vector3 &
	) const;
		
	
};

#ifdef GEN_INLINED
#include "MT_Plane3.inl"
#endif

#endif

