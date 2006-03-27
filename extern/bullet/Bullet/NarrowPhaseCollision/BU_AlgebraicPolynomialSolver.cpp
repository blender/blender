/*
Bullet Continuous Collision Detection and Physics Library
Copyright (c) 2003-2006 Erwin Coumans  http://continuousphysics.com/Bullet/

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, 
including commercial applications, and to alter it and redistribute it freely, 
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/


#include "BU_AlgebraicPolynomialSolver.h"
#include <math.h>
#include <SimdMinMax.h>

int BU_AlgebraicPolynomialSolver::Solve2Quadratic(SimdScalar p, SimdScalar q) 
{ 

	SimdScalar basic_h_local;
	SimdScalar basic_h_local_delta;
	
	basic_h_local       = p * 0.5f; 
   basic_h_local_delta = basic_h_local * basic_h_local - q; 
   if (basic_h_local_delta > 0.0f)  { 
      basic_h_local_delta = SimdSqrt(basic_h_local_delta); 
      m_roots[0]  = - basic_h_local + basic_h_local_delta; 
      m_roots[1]  = - basic_h_local - basic_h_local_delta; 
      return 2; 
   } 
   else if (SimdGreaterEqual(basic_h_local_delta, SIMD_EPSILON)) { 
      m_roots[0]  = - basic_h_local; 
      return 1; 
   } 
   else { 
      return 0; 
   } 
 }


int BU_AlgebraicPolynomialSolver::Solve2QuadraticFull(SimdScalar a,SimdScalar b, SimdScalar c) 
{ 
    SimdScalar radical = b * b - 4.0f * a * c;
    if(radical >= 0.f)
    {
        SimdScalar sqrtRadical = SimdSqrt(radical); 
        SimdScalar idenom = 1.0f/(2.0f * a);
        m_roots[0]=(-b + sqrtRadical) * idenom;
        m_roots[1]=(-b - sqrtRadical) * idenom;
		return 2;
	}
	return 0;
}


#define cubic_rt(x) \
 ((x) > 0.0f ? SimdPow((SimdScalar)(x), 0.333333333333333333333333f) : \
  ((x) < 0.0f ? -SimdPow((SimdScalar)-(x), 0.333333333333333333333333f) : 0.0f))



/*                                                                           */
/* this function solves the following cubic equation:                        */
/*                                                                           */
/*              3          2                                                */
/*      lead * x   +  a * x   +  b * x  +  c  =  0.                          */
/*                                                                           */
/* it returns the number of different roots found, and stores the roots in   */
/* roots[0,2]. it returns -1 for a degenerate equation 0 = 0.                */
/*                                                                           */
int BU_AlgebraicPolynomialSolver::Solve3Cubic(SimdScalar lead, SimdScalar a, SimdScalar b, SimdScalar c)
{ 
   SimdScalar p, q, r;
   SimdScalar delta, u, phi;
   SimdScalar dummy;

   if (lead != 1.0) {
      /*                                                                     */
      /* transform into normal form: x^3 + a x^2 + b x + c = 0               */
      /*                                                                     */
      if (SimdEqual(lead, SIMD_EPSILON)) {
         /*                                                                  */
         /* we have  a x^2 + b x + c = 0                                     */
         /*                                                                  */
         if (SimdEqual(a, SIMD_EPSILON)) {
            /*                                                               */
            /* we have  b x + c = 0                                          */
            /*                                                               */
            if (SimdEqual(b, SIMD_EPSILON)) {
               if (SimdEqual(c, SIMD_EPSILON)) {
                  return -1;
               }
               else {
                  return 0;
               }
            }
            else {
               m_roots[0] = -c / b;
               return 1;
            }
         }
         else {
            p = c / a;
            q = b / a;
            return Solve2QuadraticFull(a,b,c);
         }
      }
      else {
         a = a / lead;
         b = b / lead;
         c = c / lead;
      }
   }
               
   /*                                                                        */
   /* we substitute  x = y - a / 3  in order to eliminate the quadric term.  */
   /* we get   x^3 + p x + q = 0                                             */
   /*                                                                        */
   a /= 3.0f;
   u  = a * a;
   p  = b / 3.0f - u;
   q  = a * (2.0f * u - b) + c;

   /*                                                                        */
   /* now use Cardano's formula                                              */
   /*                                                                        */
   if (SimdEqual(p, SIMD_EPSILON)) {
      if (SimdEqual(q, SIMD_EPSILON)) {
         /*                                                                  */
         /* one triple root                                                  */
         /*                                                                  */
         m_roots[0] = -a;
         return 1;
      }
      else {
         /*                                                                  */
         /* one real and two complex roots                                   */
         /*                                                                  */
         m_roots[0] = cubic_rt(-q) - a;
         return 1;
      }
   }

   q /= 2.0f;
   delta = p * p * p + q * q;
   if (delta > 0.0f) {
      /*                                                                     */
      /* one real and two complex roots. note that  v = -p / u.              */
      /*                                                                     */
      u = -q + SimdSqrt(delta);
      u = cubic_rt(u);
      m_roots[0] = u - p / u - a;
      return 1;
   }
   else if (delta < 0.0) {
      /*                                                                     */
      /* Casus irreducibilis: we have three real roots                       */
      /*                                                                     */
      r        = SimdSqrt(-p);
      p       *= -r;
      r       *= 2.0;
      phi      = SimdAcos(-q / p) / 3.0f;
      dummy    = SIMD_2_PI / 3.0f; 
      m_roots[0] = r * SimdCos(phi) - a;
      m_roots[1] = r * SimdCos(phi + dummy) - a;
      m_roots[2] = r * SimdCos(phi - dummy) - a;
      return 3;
   }
   else {
      /*                                                                     */
      /* one single and one SimdScalar root                                      */
      /*                                                                     */
      r = cubic_rt(-q);
      m_roots[0] = 2.0f * r - a;
      m_roots[1] = -r - a;
      return 2;
   }
}


/*                                                                           */
/* this function solves the following quartic equation:                      */
/*                                                                           */
/*             4           3          2                                      */
/*     lead * x   +  a *  x   +  b * x   +  c * x  +  d = 0.                 */
/*                                                                           */
/* it returns the number of different roots found, and stores the roots in   */
/* roots[0,3]. it returns -1 for a degenerate equation 0 = 0.                */
/*                                                                           */
int BU_AlgebraicPolynomialSolver::Solve4Quartic(SimdScalar lead, SimdScalar a, SimdScalar b, SimdScalar c, SimdScalar d)
{ 
   SimdScalar p, q ,r;
   SimdScalar u, v, w;
   int i, num_roots, num_tmp;
   //SimdScalar tmp[2];

   if (lead != 1.0) {
      /*                                                                     */
      /* transform into normal form: x^4 + a x^3 + b x^2 + c x + d = 0       */
      /*                                                                     */
      if (SimdEqual(lead, SIMD_EPSILON)) {
         /*                                                                  */
         /* we have  a x^3 + b x^2 + c x + d = 0                             */
         /*                                                                  */
         if (SimdEqual(a, SIMD_EPSILON)) { 
            /*                                                               */
            /* we have  b x^2 + c x + d = 0                                  */
            /*                                                               */
            if (SimdEqual(b, SIMD_EPSILON)) {
               /*                                                            */
               /* we have  c x + d = 0                                       */
               /*                                                            */
               if (SimdEqual(c, SIMD_EPSILON)) {
                  if (SimdEqual(d, SIMD_EPSILON)) {
                     return -1;
                  }
                  else {
                     return 0;
                  }
               }
               else {
                  m_roots[0] = -d / c;
                  return 1;
               }
            }
            else {
               p = c / b;
               q = d / b;
               return Solve2QuadraticFull(b,c,d);
               
            }
         }
         else { 
            return Solve3Cubic(1.0, b / a, c / a, d / a);
         }
      }
      else {
         a = a / lead;
         b = b / lead;
         c = c / lead;
         d = d / lead;
      }
   }

   /*                                                                        */
   /* we substitute  x = y - a / 4  in order to eliminate the cubic term.    */
   /* we get:  y^4 + p y^2 + q y + r = 0.                                    */
   /*                                                                        */
   a /= 4.0f;
   p  = b - 6.0f * a * a;
   q  = a * (8.0f * a * a - 2.0f * b) + c;
   r  = a * (a * (b - 3.f * a * a) - c) + d;
   if (SimdEqual(q, SIMD_EPSILON)) {
      /*                                                                     */
      /* biquadratic equation:  y^4 + p y^2 + r = 0.                         */
      /*                                                                     */
      num_roots = Solve2Quadratic(p, r);
      if (num_roots > 0) {                 
         if (m_roots[0] > 0.0f) {
            if (num_roots > 1)  {
               if ((m_roots[1] > 0.0f)  &&  (m_roots[1] != m_roots[0])) {
                  u        = SimdSqrt(m_roots[1]);
                  m_roots[2] =  u - a;
                  m_roots[3] = -u - a;
                  u        = SimdSqrt(m_roots[0]);
                  m_roots[0] =  u - a;
                  m_roots[1] = -u - a;
                  return 4;
               }
               else {
                  u        = SimdSqrt(m_roots[0]);
                  m_roots[0] =  u - a;
                  m_roots[1] = -u - a;
                  return 2;
               }
            }
            else {
               u        = SimdSqrt(m_roots[0]);
               m_roots[0] =  u - a;
               m_roots[1] = -u - a;
               return 2;
            }
         }
      }
      return 0;
   }
   else if (SimdEqual(r, SIMD_EPSILON)) {
      /*                                                                     */
      /* no absolute term:  y (y^3 + p y + q) = 0.                           */
      /*                                                                     */
      num_roots = Solve3Cubic(1.0, 0.0, p, q);
      for (i = 0;  i < num_roots;  ++i)  m_roots[i] -= a;
      if (num_roots != -1) {
         m_roots[num_roots] = -a;
         ++num_roots;
      }
      else {
         m_roots[0]  = -a;
         num_roots = 1;;
      }
      return num_roots;
   }
   else {
      /*                                                                     */
      /* we solve the resolvent cubic equation                               */
      /*                                                                     */
      num_roots = Solve3Cubic(1.0f, -0.5f * p, -r, 0.5f * r * p - 0.125f * q * q);
      if (num_roots == -1) {
         num_roots = 1;
         m_roots[0]  = 0.0f;
      }

      /*                                                                     */
      /* build two quadric equations                                         */
      /*                                                                     */
      w = m_roots[0];
      u = w * w - r;
      v = 2.0f * w - p;

      if (SimdEqual(u, SIMD_EPSILON))
         u = 0.0;
      else if (u > 0.0f)
         u = SimdSqrt(u);
      else
         return 0;
      
      if (SimdEqual(v, SIMD_EPSILON))
         v = 0.0;
      else if (v > 0.0f)
         v = SimdSqrt(v);
      else
         return 0;

      if (q < 0.0f)  v = -v;
      w -= u;
      num_roots=Solve2Quadratic(v, w);
      for (i = 0;  i < num_roots;  ++i)  
	  {
		  m_roots[i] -= a;
	  }
      w += 2.0f *u;
	  SimdScalar tmp[2];
	  tmp[0] = m_roots[0];
	  tmp[1] = m_roots[1];

      num_tmp = Solve2Quadratic(-v, w);
      for (i = 0;  i < num_tmp;  ++i)
	  {
		 m_roots[i + num_roots] = tmp[i] - a;
		 m_roots[i]=tmp[i];
	  }

      return  (num_tmp + num_roots);
   }
}

