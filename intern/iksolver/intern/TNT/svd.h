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

#ifndef SVD_H

#define SVD_H

// Compute the Single Value Decomposition of an arbitrary matrix A
// That is compute the 3 matrices U,W,V with U column orthogonal (m,n) 
// ,W a diagonal matrix and V an orthogonal square matrix s.t. 
// A = U.W.Vt. From this decomposition it is trivial to compute the 
// inverse of A as Ainv = V.Winv.tranpose(U).
// work_space is a temporary vector used by this class to compute
// intermediate values during the computation of the SVD. This should
// be of length a.num_cols. This is not checked

#include "tntmath.h"

namespace TNT
{


template <class MaTRiX, class VecToR >
void SVD(MaTRiX &a, VecToR &w,  MaTRiX &v, VecToR &work_space) {

		int n = a.num_cols();
	int m = a.num_rows();

	int flag,i,its,j,jj,k,l(0),nm(0);
	typename MaTRiX::value_type c,f,h,x,y,z;
	typename MaTRiX::value_type anorm(0),g(0),scale(0);
    typename MaTRiX::value_type s(0);

    work_space.newsize(n);

	for (i=1;i<=n;i++) {
		l=i+1;
		work_space(i)=scale*g;

		g = (typename MaTRiX::value_type)0;

		s = (typename  MaTRiX::value_type)0;
        scale = (typename  MaTRiX::value_type)0;

		if (i <= m) {
			for (k=i;k<=m;k++) scale += TNT::abs(a(k,i));
			if (scale > (typename  MaTRiX::value_type)0) {
				for (k=i;k<=m;k++) {
					a(k,i) /= scale;
					s += a(k,i)*a(k,i);
				}
				f=a(i,i);
				g = -TNT::sign(sqrt(s),f);
				h=f*g-s;
				a(i,i)=f-g;
				if (i != n) {
					for (j=l;j<=n;j++) {
                        s = (typename  MaTRiX::value_type)0;
						for (k=i;k<=m;k++) s += a(k,i)*a(k,j);
						f=s/h;
						for (k=i;k<=m;k++) a(k,j) += f*a(k,i);
					}
				}
				for (k=i;k<=m;k++) a(k,i) *= scale;
			}
		}
		w(i)=scale*g;
        g = (typename  MaTRiX::value_type)0;
        s = (typename  MaTRiX::value_type)0;
        scale = (typename  MaTRiX::value_type)0;
		if (i <= m && i != n) {
			for (k=l;k<=n;k++) scale += TNT::abs(a(i,k));
			if (scale > (typename  MaTRiX::value_type)0) {
				for (k=l;k<=n;k++) {
					a(i,k) /= scale;
					s += a(i,k)*a(i,k);
				}
				f=a(i,l);
				g = -TNT::sign(sqrt(s),f);
				h=f*g-s;
				a(i,l)=f-g;
				for (k=l;k<=n;k++) work_space(k)=a(i,k)/h;
				if (i != m) {
					for (j=l;j<=m;j++) {
                        s = (typename  MaTRiX::value_type)0;
						for (k=l;k<=n;k++) s += a(j,k)*a(i,k);
						for (k=l;k<=n;k++) a(j,k) += s*work_space(k);
					}
				}
				for (k=l;k<=n;k++) a(i,k) *= scale;
			}
		}
		anorm=TNT::max(anorm,(TNT::abs(w(i))+TNT::abs(work_space(i))));
	}
	for (i=n;i>=1;i--) {
		if (i < n) {
			if (g != (typename  MaTRiX::value_type)0) {
				for (j=l;j<=n;j++)
					v(j,i)=(a(i,j)/a(i,l))/g;
				for (j=l;j<=n;j++) {
                    s = (typename  MaTRiX::value_type)0;
					for (k=l;k<=n;k++) s += a(i,k)*v(k,j);
					for (k=l;k<=n;k++) v(k,j) += s*v(k,i);
				}
			}
			for (j=l;j<=n;j++) v(i,j)=v(j,i)= (typename  MaTRiX::value_type)0;
		}
		v(i,i)= (typename  MaTRiX::value_type)1;
		g=work_space(i);
		l=i;
	}
	for (i=n;i>=1;i--) {
		l=i+1;
		g=w(i);
		if (i < n) {
			for (j=l;j<=n;j++) a(i,j)= (typename  MaTRiX::value_type)0;
		}
		if (g !=  (typename  MaTRiX::value_type)0) {
			g= ((typename  MaTRiX::value_type)1)/g;
			if (i != n) {
				for (j=l;j<=n;j++) {
                    s =  (typename  MaTRiX::value_type)0;
					for (k=l;k<=m;k++) s += a(k,i)*a(k,j);
					f=(s/a(i,i))*g;
					for (k=i;k<=m;k++) a(k,j) += f*a(k,i);
				}
			}
			for (j=i;j<=m;j++) a(j,i) *= g;
		} else {
			for (j=i;j<=m;j++) a(j,i)= (typename  MaTRiX::value_type)0;
		}
		++a(i,i);
	}
	for (k=n;k>=1;k--) {
		for (its=1;its<=30;its++) {
			flag=1;
			for (l=k;l>=1;l--) {
				nm=l-1;
				if (TNT::abs(work_space(l))+anorm == anorm) {
					flag=0;
					break;
				}
				if (TNT::abs(w(nm))+anorm == anorm) break;
			}
			if (flag) {
				c= (typename  MaTRiX::value_type)0;
				s= (typename  MaTRiX::value_type)1;
				for (i=l;i<=k;i++) {
					f=s*work_space(i);
					if (TNT::abs(f)+anorm != anorm) {
						g=w(i);
						h= (typename  MaTRiX::value_type)TNT::pythag(float(f),float(g));
						w(i)=h;
						h= ((typename  MaTRiX::value_type)1)/h;
						c=g*h;
						s=(-f*h);
						for (j=1;j<=m;j++) {
							y=a(j,nm);
							z=a(j,i);
							a(j,nm)=y*c+z*s;
							a(j,i)=z*c-y*s;
						}
					}
				}
			}
			z=w(k);
			if (l == k) {
				if (z <  (typename  MaTRiX::value_type)0) {
					w(k) = -z;
					for (j=1;j<=n;j++) v(j,k)=(-v(j,k));
				}
				break;
			}


#if 1
			if (its == 30)
			{
                                TNTException an_exception;
                                an_exception.i = 0;
                                throw an_exception;

                                return ;
				assert(false);
			}
#endif
			x=w(l);
			nm=k-1;
			y=w(nm);
			g=work_space(nm);
			h=work_space(k);
			f=((y-z)*(y+z)+(g-h)*(g+h))/(((typename  MaTRiX::value_type)2)*h*y);
			g=(typename  MaTRiX::value_type)TNT::pythag(float(f), float(1));
			f=((x-z)*(x+z)+h*((y/(f+TNT::sign(g,f)))-h))/x;
                        c =  (typename  MaTRiX::value_type)1;
                        s =  (typename  MaTRiX::value_type)1;
			for (j=l;j<=nm;j++) {
				i=j+1;
				g=work_space(i);
				y=w(i);
				h=s*g;
				g=c*g;
				z=(typename  MaTRiX::value_type)TNT::pythag(float(f),float(h));
				work_space(j)=z;
				c=f/z;
				s=h/z;
				f=x*c+g*s;
				g=g*c-x*s;
				h=y*s;
				y=y*c;
				for (jj=1;jj<=n;jj++) {
					x=v(jj,j);
					z=v(jj,i);
					v(jj,j)=x*c+z*s;
					v(jj,i)=z*c-x*s;
				}
				z=(typename  MaTRiX::value_type)TNT::pythag(float(f),float(h));
				w(j)=z;
				if (z !=  (typename  MaTRiX::value_type)0) {
					z= ((typename  MaTRiX::value_type)1)/z;
					c=f*z;
					s=h*z;
				}
				f=(c*g)+(s*y);
				x=(c*y)-(s*g);
				for (jj=1;jj<=m;jj++) {
					y=a(jj,j);
					z=a(jj,i);
					a(jj,j)=y*c+z*s;
					a(jj,i)=z*c-y*s;
				}
			}
			work_space(l)= (typename  MaTRiX::value_type)0;
			work_space(k)=f;
			w(k)=x;
		}
	}
};



// A is replaced by the column orthogonal matrix U 


template <class MaTRiX, class VecToR >
void SVD_a( MaTRiX &a, VecToR &w,  MaTRiX &v) {

	int n = a.num_cols();
	int m = a.num_rows();

	int flag,i,its,j,jj,k,l,nm;
	typename MaTRiX::value_type anorm,c,f,g,h,s,scale,x,y,z;

	VecToR work_space;
	work_space.newsize(n);

	g = scale = anorm = 0.0;
	
	for (i=1;i <=n;i++) {
		l = i+1;
		work_space(i) = scale*g;
		g = s=scale=0.0;

		if (i <= m) {
			for(k=i; k<=m; k++) scale += abs(a(k,i));

			if (scale) {
				for (k = i; k <=m ; k++) {
					a(k,i) /= scale;
					s += a(k,i)*a(k,i);
				}
				f = a(i,i);
				g = -sign(sqrt(s),f);
				h = f*g -s;
				a(i,i) = f-g;
	
				for (j = l; j <=n; j++) {
					for (s = 0.0,k =i;k<=m;k++) s += a(k,i)*a(k,j);
					f = s/h;
					for (k = i; k <= m; k++) a(k,j) += f*a(k,i);
				}
				for (k = i; k <=m;k++) a(k,i) *= scale;
			}
		}

		w(i) = scale*g;
		g = s = scale = 0.0;

		if (i <=m && i != n) {
			for (k = l; k <=n;k++) scale += abs(a(i,k));
			if (scale) {
				for(k = l;k <=n;k++) {
					a(i,k) /= scale;
					s += a(i,k) * a(i,k);
				}

				f = a(i,l);
				g = -sign(sqrt(s),f);
				h= f*g -s;
				a(i,l) = f-g;
				for(k=l;k<=n;k++) work_space(k) = a(i,k)/h;
				for (j=l;j<=m;j++) {
					for(s = 0.0,k=l;k<=n;k++) s+= a(j,k)*a(i,k);
					for(k=l;k<=n;k++) a(j,k) += s*work_space(k);
				}
				for(k=l;k<=n;k++) a(i,k)*=scale;
			}
		}
		anorm = max(anorm,(abs(w(i)) + abs(work_space(i))));
	}
	for (i=n;i>=1;i--) {
		if (i <n) {
			if (g) {
				for(j=l;j<=n;j++) v(j,i) = (a(i,j)/a(i,l))/g;
				for(j=l;j<=n;j++) {
					for(s=0.0,k=l;k<=n;k++) s += a(i,k)*v(k,j);
					for(k=l; k<=n;k++) v(k,j) +=s*v(k,i);
				}
			}
			for(j=l;j <=n;j++) v(i,j) = v(j,i) = 0.0;
		}
		v(i,i) = 1.0;
		g = work_space(i);
		l = i;
	}

	for (i = min(m,n);i>=1;i--) {
		l = i+1;
		g = w(i);
		for (j=l;j <=n;j++) a(i,j) = 0.0;
		if (g) {
			g = 1.0/g;
			for (j=l;j<=n;j++) {
				for (s = 0.0,k=l;k<=m;k++) s += a(k,i)*a(k,j);
				f = (s/a(i,i))*g;
				for (k=i;k<=m;k++) a(k,j) += f*a(k,i);	
			}
			for (j=i;j<=m;j++) a(j,i)*=g;
		} else {
			for (j=i;j<=m;j++) a(j,i) = 0.0;
		}
		++a(i,i);
	}

	for (k=n;k>=1;k--) {
		for (its=1;its<=30;its++) {
			flag=1;
			for(l=k;l>=1;l--) {
				nm = l-1;
				if (abs(work_space(l)) + anorm == anorm) {
					flag = 0;
					break;
				}
				if (abs(w(nm)) + anorm == anorm) break;
			}
			if (flag) {
				c = 0.0;
				s = 1.0;
				for (i=l;i<=k;i++) {
					f = s*work_space(i);
					work_space(i) = c*work_space(i);
					if (abs(f) +anorm == anorm) break;
					g = w(i);
					h  = pythag(f,g);
					w(i) = h;
					h = 1.0/h;
					c = g*h;
					s = -f*h;
					for (j=1;j<=m;j++) {
						y= a(j,nm);
						z=a(j,i);
						a(j,nm) = y*c + z*s;
						a(j,i) = z*c - y*s;
					}
				}
			}
			z=w(k);
			if (l==k) {
				if (z <0.0) {
					w(k) = -z;
					for (j=1;j<=n;j++) v(j,k) = -v(j,k);
				}
				break;
			}

			if (its == 30) assert(false);

			x=w(l);
			nm=k-1;
			y=w(nm);
			g=work_space(nm);
			h=work_space(k);
			
			f= ((y-z)*(y+z) + (g-h)*(g+h))/(2.0*h*y);
			g = pythag(f,1.0);
			f= ((x-z)*(x+z) + h*((y/(f + sign(g,f)))-h))/x;
			c=s=1.0;

			for (j=l;j<=nm;j++) {
				i=j+1;
				g = work_space(i);
				y=w(i);
				h=s*g;
				g=c*g;
				z=pythag(f,h);
				work_space(j) = z;
				c=f/z;
				s=h/z;
				f=x*c + g*s;
				g= g*c - x*s;
				h=y*s;
				y*=c;
				for(jj=1;jj<=n;jj++) {
					x=v(jj,j);
					z=v(jj,i);
					v(jj,j) = x*c + z*s;
					v(jj,i) = z*c- x*s;
				}
				z=pythag(f,h);
				w(j)=z;
				if(z) {
					z = 1.0/z;
					c=f*z;
					s=h*z;
				}
				f=c*g + s*y;
				x= c*y-s*g;
			
				for(jj=1;jj<=m;jj++) {
					y=a(jj,j);
					z=a(jj,i);
					a(jj,j) = y*c+z*s;
					a(jj,i) = z*c - y*s;
				}
			}

			work_space(l) = 0.0;
			work_space(k) = f;
			w(k) = x;
		}
	}
}
}
#endif











				

	












					






	

