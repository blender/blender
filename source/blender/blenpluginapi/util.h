/* Copyright (c) 1999, Not a Number / NeoGeo b.v. 
 * $Id$
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

#ifndef UTIL_H
#define UTIL_H

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include "externdef.h"

#ifndef	NULL
#define NULL			0
#endif

#ifndef	FALSE
#define FALSE			0
#endif

#ifndef	TRUE
#define TRUE			1
#endif

#ifndef ulong
#define ulong unsigned long
#endif

#ifndef ushort
#define ushort unsigned short
#endif

#ifndef uchar
#define uchar unsigned char
#endif

#ifndef uint
#define uint unsigned int
#endif

#define MIN2(x,y)		( (x)<(y) ? (x) : (y) )
#define MIN3(x,y,z)		MIN2( MIN2((x),(y)) , (z) )
#define MIN4(x,y,z,a)		MIN2( MIN2((x),(y)) , MIN2((z),(a)) )

#define MAX2(x,y)		( (x)>(y) ? (x) : (y) )
#define MAX3(x,y,z)		MAX2( MAX2((x),(y)) , (z) )
#define MAX4(x,y,z,a)		MAX2( MAX2((x),(y)) , MAX2((z),(a)) )

#define SWAP(type, a, b)	{ type sw_ap; sw_ap=(a); (a)=(b); (b)=sw_ap; }

#define ABS(x)	((x) < 0 ? -(x) : (x))
#define FLOOR(x) ((int)(x) - ((x) < 0 && (x) != (int)(x)))
#define CEIL(x) ((int)(x) + ((x) > 0 && (x) != (int)(x)))
#define STEP(a,b)	( (a)>(b) ? (1) : (0) )
#define CLAMP(val, low, high) ((val>high)?high:((val<low)?low:val))
#define LERP(t,x0,x1) ((x0) + (t)*((x1)-(x0)))
#define PULSE(a,b,x) (STEP((a),(x)) - STEP((b),(x)))
#define BOXSTEP(a,b,x) CLAMP(((x)-(a))/((b)-(a)),0,1)

#define PRINT(d, var1)	printf(# var1 ":%" # d "\n", var1)
#define PRINT2(d, e, var1, var2)	printf(# var1 ":%" # d " " # var2 ":%" # e "\n", var1, var2)
#define PRINT3(d, e, f, var1, var2, var3)	printf(# var1 ":%" # d " " # var2 ":%" # e " " # var3 ":%" # f "\n", var1, var2, var3)
#define PRINT4(d, e, f, g, var1, var2, var3, var4)	printf(# var1 ":%" # d " " # var2 ":%" # e " " # var3 ":%" # f " " # var4 ":%" # g "\n", var1, var2, var3, var4)

LIBEXPORT          void *mallocN(int len, char *str);
LIBEXPORT          void *callocN(int len, char *str);
LIBEXPORT          short freeN(void *vmemh);   

LIBEXPORT          void *mallocT(int len, char *str);
LIBEXPORT          void *callocT(int len, char *str);
LIBEXPORT          void freeT(void *vmemh);   

#endif /* UTIL_H */

