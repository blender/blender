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

#ifndef FLOATPATCH_H
#define FLOATPATCH_H

/* floating point libs differ at systems... with these defines it comilies at all! */

#ifdef MIPS1
#else

#define fabsf(a)	fabs((double)(a))

#define facos		acosf
#define acosf(a)	acos((double)(a))

#define fasin		asinf
#define asinf(a)	asin((double)(a))

#define fatan		atanf
#define atanf(a)	atan((double)(a))

#define fatan2			atan2f
#define atan2f(a, b)	atan2((double)(a), (double)(b))

#define fmodf(a, b)		fmod((double)(a), (double)(b))

#define fcos		cosf
#define cosf(a)		cos((double)(a))

#define fsin		sinf
#define sinf(a)		sin((double)(a))

#define ftan		tanf
#define tanf(a)		tan((double)(a))

#define fexp		expf
#define expf(a)		exp((double)(a))

#define flog		logf
#define logf(a)		log((double)(a))

#define flog10		log10f
#define log10f(a)	log10((double)(a))

#define fsqrt		sqrtf
#define sqrtf(a)	sqrt((double)(a))

#define fceil		ceilf
#define ceilf(a)	ceil((double)(a))

#define ffloor		floorf
#define floorf(a)	floor((double)(a))

#define fpow		powf
#define powf(a, b)	pow((double)(a), (double)(b))

/* #endif  */

#endif

#endif /* FLOATPATCH_H */

