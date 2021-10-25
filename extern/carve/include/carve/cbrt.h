// N.B. only appropriate for IEEE doubles.
// Cube root implementation obtained from code with the following notice:

/* @(#)s_cbrt.c 1.3 95/01/18 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 *
 */

/* Sometimes it's necessary to define __LITTLE_ENDIAN explicitly
   but these catch some common cases. */

#if defined(i386) || defined(i486) || \
	defined(intel) || defined(x86) || defined(i86pc) || \
	defined(__alpha) || defined(__osf__)
#define __LITTLE_ENDIAN
#endif

#ifdef __LITTLE_ENDIAN
#define __HI(x) *(1+(int*)&x)
#define __LO(x) *(int*)&x
#define __HIp(x) *(1+(int*)x)
#define __LOp(x) *(int*)x
#else
#define __HI(x) *(int*)&x
#define __LO(x) *(1+(int*)&x)
#define __HIp(x) *(int*)x
#define __LOp(x) *(1+(int*)x)
#endif

/* cbrt(x)
 * Return cube root of x
 */

inline double cbrt(double x) {

  static const unsigned 
    B1 = 715094163, /* B1 = (682-0.03306235651)*2**20 */
    B2 = 696219795; /* B2 = (664-0.03306235651)*2**20 */
  static const double
    C =  5.42857142857142815906e-01, /* 19/35     = 0x3FE15F15, 0xF15F15F1 */
    D = -7.05306122448979611050e-01, /* -864/1225 = 0xBFE691DE, 0x2532C834 */
    E =  1.41428571428571436819e+00, /* 99/70     = 0x3FF6A0EA, 0x0EA0EA0F */
    F =  1.60714285714285720630e+00, /* 45/28     = 0x3FF9B6DB, 0x6DB6DB6E */
    G =  3.57142857142857150787e-01; /* 5/14      = 0x3FD6DB6D, 0xB6DB6DB7 */

  int	hx;
  double r,s,t=0.0,w;
  unsigned sign;

  hx = __HI(x);		/* high word of x */
  sign=hx&0x80000000; 		/* sign= sign(x) */
  hx  ^=sign;
  if(hx>=0x7ff00000) return(x+x); /* cbrt(NaN,INF) is itself */
  if((hx|__LO(x))==0) 
    return(x);		/* cbrt(0) is itself */

  __HI(x) = hx;	/* x <- |x| */
  /* rough cbrt to 5 bits */
  if(hx<0x00100000) 		/* subnormal number */
    {__HI(t)=0x43500000; 		/* set t= 2**54 */
    t*=x; __HI(t)=__HI(t)/3+B2;
    }
  else
    __HI(t)=hx/3+B1;	
  

  /* new cbrt to 23 bits, may be implemented in single precision */
  r=t*t/x;
  s=C+r*t;
  t*=G+F/(s+E+D/s);	

  /* chopped to 20 bits and make it larger than cbrt(x) */ 
  __LO(t)=0; __HI(t)+=0x00000001;

  /* one step newton iteration to 53 bits with error less than 0.667 ulps */
  s=t*t;		/* t*t is exact */
  r=x/s;
  w=t+t;
  r=(r-t)/(w+r);	/* r-s is exact */
  t=t+t*r;

  /* retore the sign bit */
  __HI(t) |= sign;
  return(t);
}
