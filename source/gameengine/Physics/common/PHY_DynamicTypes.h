/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
#ifndef __PHY_DYNAMIC_TYPES
#define __PHY_DYNAMIC_TYPES

/// PHY_ScalarType enumerates possible scalar types.
/// See the PHY_IMeshInterface for its use
typedef enum PHY_ScalarType {
	PHY_FLOAT,
	PHY_DOUBLE,
	PHY_INTEGER,
	PHY_SHORT,
	PHY_FIXEDPOINT88
} PHY_ScalarType;

/// PHY_PhysicsType enumerates all possible Physics Entities.
/// It is mainly used to create/add Physics Objects

typedef enum PHY_PhysicsType {
	PHY_CONVEX_RIGIDBODY=16386,
	PHY_CONCAVE_RIGIDBODY=16399,
	PHY_CONVEX_FIXEDBODY=16388,//'collision object'
	PHY_CONCAVE_FIXEDBODY=16401,
	PHY_CONVEX_KINEMATICBODY=16387,// 
	PHY_CONCAVE_KINEMATICBODY=16400,
	PHY_CONVEX_PHANTOMBODY=16398,
	PHY_CONCAVE_PHANTOMBODY=16402
} PHY_PhysicsType;

/// PHY_ConstraintType enumerates all supported Constraint Types
typedef enum PHY_ConstraintType {
	PHY_POINT2POINT_CONSTRAINT=1,
	PHY_LINEHINGE_CONSTRAINT,

} PHY_ConstraintType;

typedef float	PHY_Vector3[3];

#endif //__PHY_DYNAMIC_TYPES

