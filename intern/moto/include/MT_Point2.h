/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

/** \file moto/include/MT_Point2.h
 *  \ingroup moto
 */


/*

 * Copyright (c) 2000 Gino van den Bergen <gino@acm.org>
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Gino van den Bergen makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 */

#ifndef MT_POINT2_H
#define MT_POINT2_H

#include "MT_Vector2.h"

class MT_Point2 : public MT_Vector2 {
public:
    MT_Point2() {}
    MT_Point2(const float *v2) : MT_Vector2(v2) {} 
    MT_Point2(const double *v2) : MT_Vector2(v2) {}
    MT_Point2(MT_Scalar x2, MT_Scalar y2) : MT_Vector2(x2, y2) {}

    MT_Point2& operator+=(const MT_Vector2& v);
    MT_Point2& operator-=(const MT_Vector2& v);
    MT_Point2& operator=(const MT_Vector2& v);

    MT_Scalar  distance(const MT_Point2& p) const;
    MT_Scalar  distance2(const MT_Point2& p) const;

    MT_Point2  lerp(const MT_Point2& p, MT_Scalar t) const;
};

MT_Point2  operator+(const MT_Point2& p, const MT_Vector2& v);
MT_Point2  operator-(const MT_Point2& p, const MT_Vector2& v);
MT_Vector2 operator-(const MT_Point2& p1, const MT_Point2& p2);

MT_Scalar MT_distance(const MT_Point2& p1, const MT_Point2& p2);
MT_Scalar MT_distance2(const MT_Point2& p1, const MT_Point2& p2);

MT_Point2 MT_lerp(const MT_Point2& p1, const MT_Point2& p2, MT_Scalar t);

#ifdef GEN_INLINED
#include "MT_Point2.inl"
#endif

#endif

