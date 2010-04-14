/* 
 * $Id$
 *
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

#ifndef BKE_UTILDEFINES_H
#define BKE_UTILDEFINES_H

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

/* these values need to be hardcoded in structs, dna does not recognize defines */
/* also defined in DNA_space_types.h */
#ifndef FILE_MAXDIR
#define FILE_MAXDIR			160
#define FILE_MAXFILE		80
#define FILE_MAX			240
#endif

#define ELEM(a, b, c)           ( (a)==(b) || (a)==(c) )
#define ELEM3(a, b, c, d)       ( ELEM(a, b, c) || (a)==(d) )
#define ELEM4(a, b, c, d, e)    ( ELEM(a, b, c) || ELEM(a, d, e) )
#define ELEM5(a, b, c, d, e, f) ( ELEM(a, b, c) || ELEM3(a, d, e, f) )
#define ELEM6(a, b, c, d, e, f, g)      ( ELEM(a, b, c) || ELEM4(a, d, e, f, g) )
#define ELEM7(a, b, c, d, e, f, g, h)   ( ELEM3(a, b, c, d) || ELEM4(a, e, f, g, h) )
#define ELEM8(a, b, c, d, e, f, g, h, i)        ( ELEM4(a, b, c, d, e) || ELEM4(a, f, g, h, i) )
#define ELEM9(a, b, c, d, e, f, g, h, i, j)        ( ELEM4(a, b, c, d, e) || ELEM5(a, f, g, h, i, j) )
#define ELEM10(a, b, c, d, e, f, g, h, i, j, k)        ( ELEM4(a, b, c, d, e) || ELEM6(a, f, g, h, i, j, k) )
#define ELEM11(a, b, c, d, e, f, g, h, i, j, k, l)        ( ELEM4(a, b, c, d, e) || ELEM7(a, f, g, h, i, j, k, l) )

/* shift around elements */
#define SHIFT3(type, a, b, c) { type tmp; tmp = a; a = c; c = b; b = tmp; }
#define SHIFT4(type, a, b, c, d) { type tmp; tmp = a; a = d; d = c; c = b; b = tmp; }

/* min/max */
#define MIN2(x,y)               ( (x)<(y) ? (x) : (y) )
#define MIN3(x,y,z)             MIN2( MIN2((x),(y)) , (z) )
#define MIN4(x,y,z,a)           MIN2( MIN2((x),(y)) , MIN2((z),(a)) )

#define MAX2(x,y)               ( (x)>(y) ? (x) : (y) )
#define MAX3(x,y,z)             MAX2( MAX2((x),(y)) , (z) )
#define MAX4(x,y,z,a)           MAX2( MAX2((x),(y)) , MAX2((z),(a)) )

#define INIT_MINMAX(min, max) { (min)[0]= (min)[1]= (min)[2]= 1.0e30f; (max)[0]= (max)[1]= (max)[2]= -1.0e30f; }

#define INIT_MINMAX2(min, max) { (min)[0]= (min)[1]= 1.0e30f; (max)[0]= (max)[1]= -1.0e30f; }

#define DO_MIN(vec, min) { if( (min)[0]>(vec)[0] ) (min)[0]= (vec)[0];      \
							  if( (min)[1]>(vec)[1] ) (min)[1]= (vec)[1];   \
							  if( (min)[2]>(vec)[2] ) (min)[2]= (vec)[2]; } \

#define DO_MAX(vec, max) { if( (max)[0]<(vec)[0] ) (max)[0]= (vec)[0];		\
							  if( (max)[1]<(vec)[1] ) (max)[1]= (vec)[1];	\
							  if( (max)[2]<(vec)[2] ) (max)[2]= (vec)[2]; } \

#define DO_MINMAX(vec, min, max) { if( (min)[0]>(vec)[0] ) (min)[0]= (vec)[0]; \
							  if( (min)[1]>(vec)[1] ) (min)[1]= (vec)[1]; \
							  if( (min)[2]>(vec)[2] ) (min)[2]= (vec)[2]; \
							  if( (max)[0]<(vec)[0] ) (max)[0]= (vec)[0]; \
							  if( (max)[1]<(vec)[1] ) (max)[1]= (vec)[1]; \
							  if( (max)[2]<(vec)[2] ) (max)[2]= (vec)[2]; } \

#define DO_MINMAX2(vec, min, max) { if( (min)[0]>(vec)[0] ) (min)[0]= (vec)[0]; \
							  if( (min)[1]>(vec)[1] ) (min)[1]= (vec)[1]; \
							  if( (max)[0]<(vec)[0] ) (max)[0]= (vec)[0]; \
							  if( (max)[1]<(vec)[1] ) (max)[1]= (vec)[1]; }

/* some math and copy defines */

#ifndef SWAP
#define SWAP(type, a, b)        { type sw_ap; sw_ap=(a); (a)=(b); (b)=sw_ap; }
#endif

#define ABS(a)					( (a)<0 ? (-(a)) : (a) )

#define AVG2(x, y)		( 0.5 * ((x) + (y)) )

#define FTOCHAR(val) ((val)<=0.0f)? 0 : (((val)>(1.0f-0.5f/255.0f))? 255 : (char)((255.0f*(val))+0.5f))

#define VECCOPY(v1,v2)          {*(v1)= *(v2); *(v1+1)= *(v2+1); *(v1+2)= *(v2+2);}
#define VECCOPY2D(v1,v2)          {*(v1)= *(v2); *(v1+1)= *(v2+1);}
#define QUATCOPY(v1,v2)         {*(v1)= *(v2); *(v1+1)= *(v2+1); *(v1+2)= *(v2+2); *(v1+3)= *(v2+3);}
#define LONGCOPY(a, b, c)	{int lcpc=c, *lcpa=(int *)a, *lcpb=(int *)b; while(lcpc-->0) *(lcpa++)= *(lcpb++);}


#define VECADD(v1,v2,v3) 	{*(v1)= *(v2) + *(v3); *(v1+1)= *(v2+1) + *(v3+1); *(v1+2)= *(v2+2) + *(v3+2);}
#define VECSUB(v1,v2,v3) 	{*(v1)= *(v2) - *(v3); *(v1+1)= *(v2+1) - *(v3+1); *(v1+2)= *(v2+2) - *(v3+2);}
#define VECSUB2D(v1,v2,v3) 	{*(v1)= *(v2) - *(v3); *(v1+1)= *(v2+1) - *(v3+1);}
#define VECADDFAC(v1,v2,v3,fac) {*(v1)= *(v2) + *(v3)*(fac); *(v1+1)= *(v2+1) + *(v3+1)*(fac); *(v1+2)= *(v2+2) + *(v3+2)*(fac);}
#define VECSUBFAC(v1,v2,v3,fac) {*(v1)= *(v2) - *(v3)*(fac); *(v1+1)= *(v2+1) - *(v3+1)*(fac); *(v1+2)= *(v2+2) - *(v3+2)*(fac);}
#define QUATADDFAC(v1,v2,v3,fac) {*(v1)= *(v2) + *(v3)*(fac); *(v1+1)= *(v2+1) + *(v3+1)*(fac); *(v1+2)= *(v2+2) + *(v3+2)*(fac); *(v1+3)= *(v2+3) + *(v3+3)*(fac);}

#define INPR(v1, v2)		( (v1)[0]*(v2)[0] + (v1)[1]*(v2)[1] + (v1)[2]*(v2)[2] )


/* some misc stuff.... */
#define CLAMP(a, b, c)		if((a)<(b)) (a)=(b); else if((a)>(c)) (a)=(c)
#define CLAMPIS(a, b, c) ((a)<(b) ? (b) : (a)>(c) ? (c) : (a))
#define CLAMPTEST(a, b, c)	if((b)<(c)) {CLAMP(a, b, c);} else {CLAMP(a, c, b);}

#define IS_EQ(a,b) ((fabs((double)(a)-(b)) >= (double) FLT_EPSILON) ? 0 : 1)

#define IS_EQT(a, b, c) ((a > b)? (((a-b) <= c)? 1:0) : ((((b-a) <= c)? 1:0)))
#define IN_RANGE(a, b, c) ((b < c)? ((b<a && a<c)? 1:0) : ((c<a && a<b)? 1:0))
#define IN_RANGE_INCL(a, b, c) ((b < c)? ((b<=a && a<=c)? 1:0) : ((c<=a && a<=b)? 1:0))

/* this weirdo pops up in two places ... */
#if !defined(WIN32)
#ifndef O_BINARY
#define O_BINARY 0
#endif
#endif

/* INTEGER CODES */
#if defined(__sgi) || defined (__sparc) || defined (__sparc__) || defined (__PPC__) || defined (__ppc__) || defined (__hppa__) || defined (__BIG_ENDIAN__)
	/* Big Endian */
#define MAKE_ID(a,b,c,d) ( (int)(a)<<24 | (int)(b)<<16 | (c)<<8 | (d) )
#else
	/* Little Endian */
#define MAKE_ID(a,b,c,d) ( (int)(d)<<24 | (int)(c)<<16 | (b)<<8 | (a) )
#endif

#define ID_NEW(a)		if( (a) && (a)->id.newid ) (a)= (void *)(a)->id.newid

#define FORM MAKE_ID('F','O','R','M')

#define BLEN MAKE_ID('B','L','E','N')
#define DER_ MAKE_ID('D','E','R','_')
#define V100 MAKE_ID('V','1','0','0')

#define DATA MAKE_ID('D','A','T','A')
#define GLOB MAKE_ID('G','L','O','B')
#define IMAG MAKE_ID('I','M','A','G')

#define DNA1 MAKE_ID('D','N','A','1')
#define TEST MAKE_ID('T','E','S','T')
#define REND MAKE_ID('R','E','N','D')
#define USER MAKE_ID('U','S','E','R')

#define ENDB MAKE_ID('E','N','D','B')


/* This one rotates the bytes in an int64, int (32) and short (16) */
#define SWITCH_INT64(a) { \
	char s_i, *p_i; \
	p_i= (char *)&(a); \
	s_i=p_i[0]; p_i[0]=p_i[7]; p_i[7]=s_i; \
	s_i=p_i[1]; p_i[1]=p_i[6]; p_i[6]=s_i; \
	s_i=p_i[2]; p_i[2]=p_i[5]; p_i[5]=s_i; \
	s_i=p_i[3]; p_i[3]=p_i[4]; p_i[4]=s_i; }

#define SWITCH_INT(a) { \
	char s_i, *p_i; \
	p_i= (char *)&(a); \
	s_i=p_i[0]; p_i[0]=p_i[3]; p_i[3]=s_i; \
	s_i=p_i[1]; p_i[1]=p_i[2]; p_i[2]=s_i; }

#define SWITCH_SHORT(a)	{ \
	char s_i, *p_i; \
	p_i= (char *)&(a); \
	s_i=p_i[0]; p_i[0]=p_i[1]; p_i[1]=s_i; }


/* Bit operations */
#define BTST(a,b)	( ( (a) & 1<<(b) )!=0 )   
#define BNTST(a,b)	( ( (a) & 1<<(b) )==0 )
#define BTST2(a,b,c)	( BTST( (a), (b) ) || BTST( (a), (c) ) )
#define BSET(a,b)	( (a) | 1<<(b) )
#define BCLR(a,b)	( (a) & ~(1<<(b)) )
/* bit-row */
#define BROW(min, max)	(((max)>=31? 0xFFFFFFFF: (1<<(max+1))-1) - ((min)? ((1<<(min))-1):0) )


#ifdef GS
#undef GS
#endif
#define GS(a)	(*((short *)(a)))

/* Warning-free macros for storing ints in pointers. Use these _only_
 * for storing an int in a pointer, not a pointer in an int (64bit)! */
#define SET_INT_IN_POINTER(i) ((void*)(intptr_t)(i))
#define GET_INT_FROM_POINTER(i) ((int)(intptr_t)(i))

/*little array macro library.  example of usage:

int *arr = NULL;
V_DECLARE(arr);
int i;

for (i=0; i<10; i++) {
	V_GROW(arr);
	arr[i] = something;
}
V_FREE(arr);

arrays are buffered, using double-buffering (so on each reallocation,
the array size is doubled).  supposedly this should give good Big Oh
behaviour, though it may not be the best in practice.
*/

#define V_DECLARE(vec) int _##vec##_count=0; void *_##vec##_tmp

/*in the future, I plan on having V_DECLARE allocate stack memory it'll
  use at first, and switch over to heap when it needs more.  that'll mess
  up cases where you'd want to use this API to build a dynamic list for
  non-local use, so all such cases should use this macro.*/
#define V_DYNDECLARE(vec) V_DECLARE(vec)

/*this returns the entire size of the array, including any buffering.*/
#define V_SIZE(vec) ((signed int)((vec)==NULL ? 0 : MEM_allocN_len(vec) / sizeof(*vec)))

/*this returns the logical size of the array, not including buffering.*/
#define V_COUNT(vec) _##vec##_count

/*grow the array by one.  zeroes the new elements.*/
#define V_GROW(vec) \
	V_SIZE(vec) > _##vec##_count ? _##vec##_count++ : \
	((_##vec##_tmp = MEM_callocN(sizeof(*vec)*(_##vec##_count*2+2), #vec " " __FILE__ " ")),\
	(vec && memcpy(_##vec##_tmp, vec, sizeof(*vec) * _##vec##_count)),\
	(vec && (MEM_freeN(vec),1)),\
	(vec = _##vec##_tmp),\
	_##vec##_count++)

#define V_FREE(vec) if (vec) MEM_freeN(vec);

/*resets the logical size of an array to zero, but doesn't
  free the memory.*/
#define V_RESET(vec) _##vec##_count=0

/*set the count of the array*/
#define V_SETCOUNT(vec, count) _##vec##_count = (count)

/*little macro so inline keyword works*/
#if defined(_MSC_VER)
#define BM_INLINE static __forceinline
#else
#define BM_INLINE static inline __attribute((always_inline))
#endif

#define BMEMSET(mem, val, size) {unsigned int _i; char *_c = (char*) mem; for (_i=0; _i<size; _i++) *_c++ = val;}

#endif
