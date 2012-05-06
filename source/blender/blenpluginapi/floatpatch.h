/* Copyright (c) 1999, Not a Number / NeoGeo b.v. 
 *
 * All rights reserved.
 * 
 * Contact:      info@blender.org   
 * Information:  http://www.blender.org
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef __FLOATPATCH_H__
#define __FLOATPATCH_H__

/** \file blender/blenpluginapi/floatpatch.h
 *  \ingroup blpluginapi
 */

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

#endif /* __FLOATPATCH_H__ */

