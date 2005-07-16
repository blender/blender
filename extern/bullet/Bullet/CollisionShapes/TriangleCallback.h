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
#ifndef TRIANGLE_CALLBACK_H
#define TRIANGLE_CALLBACK_H

#include "SimdVector3.h"


class TriangleCallback
{
public:

	virtual ~TriangleCallback();
	virtual void ProcessTriangle(SimdVector3* triangle) = 0;
};

#endif //TRIANGLE_CALLBACK_H
