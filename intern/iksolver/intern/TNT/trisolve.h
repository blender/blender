/**
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

#endif // TRISLV_H

