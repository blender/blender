/**
 * $Id$
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



// Header file for Fortran Lapack

#ifndef LAPACK_H
#define LAPACK_H

// This file incomplete and included here to only demonstrate the
// basic framework for linking with the Fortran Lapack routines.

#include "fortran.h"
#include "vec.h"
#include "fmat.h"


#define F77_DGESV   dgesv_
#define F77_DGELS   dgels_
#define F77_DSYEV   dsyev_
#define F77_DGEEV   dgeev_

extern "C"
{

    // linear equations (general) using LU factorizaiton
    //
    void F77_DGESV(cfi_ N, cfi_ nrhs, fda_ A, cfi_ lda,
        fia_ ipiv, fda_ b, cfi_ ldb, fi_ info);

    // solve linear least squares using QR or LU factorization
    //
    void F77_DGELS(cfch_ trans, cfi_ M, 
        cfi_ N, cfi_ nrhs, fda_ A, cfi_ lda, fda_ B, cfi_ ldb, fda_ work, 
            cfi_ lwork, fi_ info);

    // solve symmetric eigenvalues
    //
    void F77_DSYEV( cfch_ jobz, cfch_ uplo, cfi_ N, fda_  A, cfi_ lda, 
        fda_ W, fda_ work, cfi_ lwork, fi_ info);

    // solve unsymmetric eigenvalues
    //
    void F77_DGEEV(cfch_ jobvl, cfch_ jobvr, cfi_ N, fda_ A, cfi_ lda,
        fda_ wr, fda_ wi, fda_ vl, cfi_ ldvl, fda_ vr, 
        cfi_ ldvr, fda_ work, cfi_ lwork, fi_ info);

}

// solve linear equations using LU factorization

using namespace TNT;

Vector<double> Lapack_LU_linear_solve(const Fortran_Matrix<double> &A,
    const Vector<double> &b)
{
    const Fortran_integer one=1;
    Subscript M=A.num_rows();
    Subscript N=A.num_cols();

    Fortran_Matrix<double> Tmp(A);
    Vector<double> x(b);
    Vector<Fortran_integer> index(M);
    Fortran_integer info = 0;

    F77_DGESV(&N, &one, &Tmp(1,1), &M, &index(1), &x(1), &M, &info);    

    if (info != 0) return Vector<double>(0);
    else
        return x;
}

// solve linear least squares problem using QR factorization
//
Vector<double> Lapack_LLS_QR_linear_solve(const Fortran_Matrix<double> &A,
    const Vector<double> &b)
{
    const Fortran_integer one=1;
    Subscript M=A.num_rows();
    Subscript N=A.num_cols();

    Fortran_Matrix<double> Tmp(A);
    Vector<double> x(b);
    Fortran_integer info = 0;

    char transp = 'N';
    Fortran_integer lwork = 5 * (M+N);      // temporary work space
    Vector<double> work(lwork);

    F77_DGELS(&transp, &M, &N, &one, &Tmp(1,1), &M, &x(1), &M,  &work(1),
        &lwork, &info); 

    if (info != 0) return Vector<double>(0);
    else
        return x;
}

// *********************** Eigenvalue problems *******************

// solve symmetric eigenvalue problem (eigenvalues only)
//
Vector<double> Upper_symmetric_eigenvalue_solve(const Fortran_Matrix<double> &A)
{
    char jobz = 'N';
    char uplo = 'U';
    Subscript N = A.num_rows();

    assert(N == A.num_cols());

    Vector<double> eigvals(N);
    Fortran_integer worksize = 3*N;
    Fortran_integer info = 0;
    Vector<double> work(worksize);
    Fortran_Matrix<double> Tmp = A;

    F77_DSYEV(&jobz, &uplo, &N, &Tmp(1,1), &N, eigvals.begin(), work.begin(),
        &worksize, &info);

    if (info != 0) return Vector<double>();
    else
        return eigvals;
}


// solve unsymmetric eigenvalue problems 
//
int eigenvalue_solve(const Fortran_Matrix<double> &A, 
        Vector<double> &wr, Vector<double> &wi)
{
    char jobvl = 'N';
    char jobvr = 'N';

    Fortran_integer N = A.num_rows();


    assert(N == A.num_cols());
    
    if (N<1) return 1;

    Fortran_Matrix<double> vl(1,N);  /* should be NxN ? **** */
    Fortran_Matrix<double> vr(1,N);  
    Fortran_integer one = 1;

    Fortran_integer worksize = 5*N;
    Fortran_integer info = 0;
    Vector<double> work(worksize, 0.0);
    Fortran_Matrix<double> Tmp = A;

    wr.newsize(N);
    wi.newsize(N);

//  void F77_DGEEV(cfch_ jobvl, cfch_ jobvr, cfi_ N, fda_ A, cfi_ lda,
//      fda_ wr, fda_ wi, fda_ vl, cfi_ ldvl, fda_ vr, 
//      cfi_ ldvr, fda_ work, cfi_ lwork, fi_ info);

    F77_DGEEV(&jobvl, &jobvr, &N, &Tmp(1,1), &N, &(wr(1)),
        &(wi(1)), &(vl(1,1)), &one, &(vr(1,1)), &one,
        &(work(1)), &worksize, &info);

    return (info==0 ? 0: 1);
}

#endif // LAPACK_H

