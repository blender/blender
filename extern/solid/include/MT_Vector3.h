/*
 * SOLID - Software Library for Interference Detection
 * 
 * Copyright (C) 2001-2003  Dtecta.  All rights reserved.
 *
 * This library may be distributed under the terms of the Q Public License
 * (QPL) as defined by Trolltech AS of Norway and appearing in the file
 * LICENSE.QPL included in the packaging of this file.
 *
 * This library may be distributed and/or modified under the terms of the
 * GNU General Public License (GPL) version 2 as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL included in the
 * packaging of this file.
 *
 * This library is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Commercial use or any other use of this library not covered by either 
 * the QPL or the GPL requires an additional license from Dtecta. 
 * Please contact info@dtecta.com for enquiries about the terms of commercial
 * use of this library.
 */

#ifndef MT_VECTOR3_H
#define MT_VECTOR3_H

#include "MT_Scalar.h"
#include <MT/Vector3.h>

typedef MT::Vector3<MT_Scalar> MT_Vector3;

#ifdef CPU_CMP

inline bool operator==(const MT_Vector3& p1, const MT_Vector3& p2) 
{
	const unsigned int *i1 = (const unsigned int *)&p1;
	const unsigned int *i2 = (const unsigned int *)&p2;
    return i1[0] == i2[0] && i1[1] == i2[1] && i1[2] == i2[2];
}

#else

inline bool operator==(const MT_Vector3& p1, const MT_Vector3& p2) 
{
	return p1[0] == p2[0] && p1[1] == p2[1] && p1[2] == p2[2];
}

#endif

#endif
