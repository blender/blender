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

#ifndef GEN_MINMAX_H
#define GEN_MINMAX_H

template <class T>
inline const T& GEN_min(const T& a, const T& b) 
{
  return b < a ? b : a;
}

template <class T>
inline const T& GEN_max(const T& a, const T& b) 
{
  return  a < b ? b : a;
}

template <class T>
inline const T& GEN_clamped(const T& a, const T& lb, const T& ub) 
{
	return a < lb ? lb : (ub < a ? ub : a); 
}

template <class T>
inline void GEN_set_min(T& a, const T& b) 
{
    if (b < a) 
	{
		a = b;
	}
}

template <class T>
inline void GEN_set_max(T& a, const T& b) 
{
    if (a < b) 
	{
		a = b;
	}
}

template <class T>
inline void GEN_clamp(T& a, const T& lb, const T& ub) 
{
	if (a < lb) 
	{
		a = lb; 
	}
	else if (ub < a) 
	{
		a = ub;
	}
}

#endif
