/**
 * $Id$
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

#ifndef NAN_INCLUDED_IK_LineMinimizer_h

#define NAN_INCLUDED_IK_LineMinimizer_h

/**
 * @author Laurence Bourn
 * @date 28/6/2001
 */

#include "MT_Scalar.h"
#include <vector>
#include "TNT/tntmath.h"
#include "MEM_NonCopyable.h"



#define GOLD 1.618034
#define GLIMIT 100.0
#define TINY 1.0e-20
#define ZEPS 1.0e-10

/** 
 * Routines for line - minization in n dimensions
 * these should be templated on the potenial function
 * p instead of using a virtual class. Please see 
 * numerical recipes in c for more details. www.nr.com
 */

class DifferentiablePotenialFunction1d {
public :
	virtual
		MT_Scalar 
	Evaluate(
		const MT_Scalar x
	) = 0;

	virtual
		MT_Scalar
	Derivative(
		const MT_Scalar x
	) = 0;
};


/**
 * TODO get rid of this class and make some
 * template functions in a seperate namespace
 */

class IK_LineMinimizer : public MEM_NonCopyable {

public :

	/**
	 * Before we proceed with line minimization 
	 * we need to construct an initial bracket
	 * of a minima of the PotenialFunction
	 */

	static
		void
	InitialBracket1d (	
		MT_Scalar &ax,
		MT_Scalar &bx,
		MT_Scalar &cx,
		MT_Scalar &fa,
		MT_Scalar &fb,
		MT_Scalar &fc,
		DifferentiablePotenialFunction1d &potenial
	) {
		MT_Scalar ulim,u,r,q,fu;

		fa = potenial.Evaluate(ax);
		fb = potenial.Evaluate(bx);

		if (fb > fa) {
			std::swap(ax,bx);
			std::swap(fa,fb);
		}
		cx = bx + GOLD*(bx-ax);
		fc = potenial.Evaluate(cx);
		
		while (fb > fc) {

			r = (bx - ax) * (fb - fc);
			q = (bx - cx) * (fb - fa);
			u = bx - ((bx - cx)*q - (bx - ax) *r)/ 
				(2 * TNT::sign(TNT::max(TNT::abs(q-r),TINY),q-r));
			ulim = bx + GLIMIT*(cx-bx);

			if ((bx-u)*(u-cx) > 0.0) {
				fu = potenial.Evaluate(u);
				if (fu < fc) {
					ax = bx;
					bx = u;
					fa = fb;
					fb = fu;
					return;
				} else if (fu > fb) {
					cx = u;
					fc = fu;
					return;
				}
				u = cx + GOLD*(cx-bx);
				fu = potenial.Evaluate(u);

			} else if ((cx - u)*(u - ulim) > 0.0) {
				fu = potenial.Evaluate(u);
 
				if (fu < fc) {
					bx = cx;
					cx = u;
					u = cx + GOLD*(cx - bx);
					fb = fc;
					fc = fu;
					fu = potenial.Evaluate(u);
				}
			} else if ((u-ulim)*(ulim-cx) >=0.0) {
				u = ulim;
				fu = potenial.Evaluate(u);
			} else {
				u = cx + GOLD*(cx-bx);
				fu = potenial.Evaluate(u);
			}
			ax = bx;
			bx = cx;
			cx = u;
			fa = fb;
			fb = fc;
			fc = fu;
		}
	};

	/**
	 * This is a 1 dimensional brent method for
	 * line-minization with derivatives
	 */


	static
		MT_Scalar
	DerivativeBrent1d(
		MT_Scalar ax,
		MT_Scalar bx,
		MT_Scalar cx,
		DifferentiablePotenialFunction1d &potenial,
		MT_Scalar &x_min,
		const MT_Scalar tol,
		int max_iter = 100
	) {
		int iter;
		bool ok1,ok2;
		MT_Scalar a,b,d,d1,d2,du,dv,dw,dx,e(0);
		MT_Scalar fu,fv,fw,fx,olde,tol1,tol2,u,u1,u2,v,w,x,xm;

		a = (ax < cx ? ax : cx);
		b = (ax > cx ? ax : cx);
		x = w = v = bx;
		fw = fv = fx = potenial.Evaluate(x);
		dw = dv = dx = potenial.Derivative(x);

		for (iter = 1; iter <= max_iter; iter++) {
			xm = 0.5*(a+b);
			tol1 = tol*fabs(x) + ZEPS;
			tol2 = 2 * tol1;
			if (fabs(x - xm) <= (tol2 - 0.5*(b-a))) {
				x_min = x;
				return fx;
			}

			if (fabs(e) > tol1) {
				d1 = 2*(b-a);
				d2 = d1;
				if (dw != dx) {
					d1 = (w-x)*dx/(dx-dw);
				}
				if (dv != dx) {
					d2 = (v-x)*dx/(dx-dv);
				}
					
				u1 = x+d1;
				u2 = x+d2;
				ok1 = ((a-u1)*(u1-b) > 0.0) && ((dx*d1) <= 0.0);
				ok2 = ((a-u2)*(u2-b) > 0.0) && ((dx*d2) <= 0.0);
				olde = e;
				e = d;

				if (ok1 || ok2) {
					if (ok1 && ok2) {
						d = fabs(d1) < fabs(d2) ? d1 : d2;
					} else if (ok1)  {
						d = d1;
					} else {
						d = d2;
					}
					if (fabs(d) <= fabs(0.5*olde)) {			
						u = x+d;
						if ((u-a < tol2) || (b-u < tol2)) {
							d = TNT::sign(tol1,xm-x);
						}
					} else {
						d = 0.5*(e = (dx >= 0.0 ? a-x : b-x));
					}
				} else {
					d = 0.5*(e = (dx >= 0.0 ? a-x : b-x));
				}
			} else {
				d = 0.5*(e = (dx >= 0.0 ? a-x : b-x));
			}

			if (fabs(d) >= tol1) {
				u = x+d;
				fu = potenial.Evaluate(u);
			} else {
				u  = x + TNT::sign(tol1,d);
				fu = potenial.Evaluate(u);
				if (fu > fx) {
					x_min = x;
					return fx;
				}
			}
			du = potenial.Derivative(u);
			if (fu <= fx) {
				if (u >= x) {
					a = x;
				} else {
					b = x;
				}
				v = w; fv = fw; dv = dw;
				w = x; fw = fx; dw = dx;
				x = u; fx = fu; dx = du;
			} else {
				if (u < x) {
					a = u;
				} else {
					b = u;
				}
				if (fu <= fw || w == x) {
					v = w; fv = fw; dv = dw;
					w = u; fw = fu; dw = du;
				} else if ( fu < fv || v == x || v == w) {
					v = u; fv = fu; dv = du;
				}
			}
		}
		// FIXME throw exception		
	
		assert(false);
		return MT_Scalar(0);
	};

private :

	/// This class just contains static helper methods so no instantiation

	IK_LineMinimizer();		

};

#undef GOLD
#undef GLIMIT
#undef TINY
#undef ZEPS


				
#endif
