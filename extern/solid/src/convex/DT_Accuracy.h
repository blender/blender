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

#ifndef DT_ACCURACY_H
#define DT_ACCURACY_H

#include "MT_Scalar.h"

class DT_Accuracy {
public:
	static MT_Scalar rel_error2; // squared relative error in the computed distance
	static MT_Scalar depth_tolerance; // terminate EPA if upper_bound <= depth_tolerance * dist2
	static MT_Scalar tol_error; // error tolerance if the distance is almost zero
	
	static void setAccuracy(MT_Scalar rel_error) 
	{ 
		rel_error2 = rel_error * rel_error;
		depth_tolerance = MT_Scalar(1.0) + MT_Scalar(2.0) * rel_error;
	}	
   
	static void setTolerance(MT_Scalar epsilon) 
	{ 
		tol_error = epsilon;
	}
};

#endif
