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

/*

*
* Template Numerical Toolkit (TNT): Linear Algebra Module
*
* Mathematical and Computational Sciences Division
* National Institute of Technology,
* Gaithersburg, MD USA
*
*
* This software was developed at the National Institute of Standards and
* Technology (NIST) by employees of the Federal Government in the course
* of their official duties. Pursuant to title 17 Section 105 of the
* United States Code, this software is not subject to copyright protection
* and is in the public domain.  The Template Numerical Toolkit (TNT) is
* an experimental system.  NIST assumes no responsibility whatsoever for
* its use by other parties, and makes no guarantees, expressed or implied,
* about its quality, reliability, or any other characteristic.
*
* BETA VERSION INCOMPLETE AND SUBJECT TO CHANGE
* see http://math.nist.gov/tnt for latest updates.
*
*/



// Triangular Solves

#ifndef TRISLV_H
#define TRISLV_H


#include "triang.h"

namespace TNT
{

template <class MaTriX, class VecToR>
VecToR Lower_triangular_solve(/*const*/ MaTriX &A, /*const*/ VecToR &b)
{
    Subscript N = A.num_rows();

    // make sure matrix sizes agree; A must be square

    assert(A.num_cols() == N);
    assert(b.dim() == N);

    VecToR x(N);

    Subscript i;
    for (i=1; i<=N; i++)
    {
        typename MaTriX::element_type tmp=0;

        for (Subscript j=1; j<i; j++)
                tmp = tmp + A(i,j)*x(j);

        x(i) =  (b(i) - tmp)/ A(i,i);
    }

    return x;
}


template <class MaTriX, class VecToR>
VecToR Unit_lower_triangular_solve(/*const*/ MaTriX &A, /*const*/ VecToR &b)
{
    Subscript N = A.num_rows();

    // make sure matrix sizes agree; A must be square

    assert(A.num_cols() == N);
    assert(b.dim() == N);

    VecToR x(N);

    Subscript i;
    for (i=1; i<=N; i++)
    {

        typename MaTriX::element_type tmp=0;

        for (Subscript j=1; j<i; j++)
                tmp = tmp + A(i,j)*x(j);

        x(i) =  b(i) - tmp;
    }

    return x;
}


template <class MaTriX, class VecToR>
VecToR linear_solve(/*const*/ LowerTriangularView<MaTriX> &A, 
            /*const*/ VecToR &b)
{
    return Lower_triangular_solve(A, b);
}
    
template <class MaTriX, class VecToR>
VecToR linear_solve(/*const*/ UnitLowerTriangularView<MaTriX> &A, 
        /*const*/ VecToR &b)
{
    return Unit_lower_triangular_solve(A, b);
}
    


//********************** Upper triangular section ****************

template <class MaTriX, class VecToR>
VecToR Upper_triangular_solve(/*const*/ MaTriX &A, /*const*/ VecToR &b)
{
    Subscript N = A.num_rows();

    // make sure matrix sizes agree; A must be square

    assert(A.num_cols() == N);
    assert(b.dim() == N);

    VecToR x(N);

    Subscript i;
    for (i=N; i>=1; i--)
    {

        typename MaTriX::element_type tmp=0;

        for (Subscript j=i+1; j<=N; j++)
                tmp = tmp + A(i,j)*x(j);

        x(i) =  (b(i) - tmp)/ A(i,i);
    }

    return x;
}


template <class MaTriX, class VecToR>
VecToR Unit_upper_triangular_solve(/*const*/ MaTriX &A, /*const*/ VecToR &b)
{
    Subscript N = A.num_rows();

    // make sure matrix sizes agree; A must be square

    assert(A.num_cols() == N);
    assert(b.dim() == N);

    VecToR x(N);

    Subscript i;
    for (i=N; i>=1; i--)
    {

        typename MaTriX::element_type tmp=0;

        for (Subscript j=i+1; j<i; j++)
                tmp = tmp + A(i,j)*x(j);

        x(i) =  b(i) - tmp;
    }

    return x;
}


template <class MaTriX, class VecToR>
VecToR linear_solve(/*const*/ UpperTriangularView<MaTriX> &A, 
        /*const*/ VecToR &b)
{
    return Upper_triangular_solve(A, b);
}
    
template <class MaTriX, class VecToR>
VecToR linear_solve(/*const*/ UnitUpperTriangularView<MaTriX> &A, 
    /*const*/ VecToR &b)
{
    return Unit_upper_triangular_solve(A, b);
}


} // namespace TNT

#endif
// TRISLV_H
