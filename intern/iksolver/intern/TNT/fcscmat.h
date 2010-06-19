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



//  Templated compressed sparse column matrix (Fortran conventions).
//  uses 1-based offsets in storing row indices.
//  Used primarily to interface with Fortran sparse matrix libaries.
//  (CANNOT BE USED AS AN STL CONTAINER.)


#ifndef FCSCMAT_H
#define FCSCMAT_H

#include <iostream>
#include <cassert>
#include "tnt.h"
#include "vec.h"

using namespace std;

namespace TNT
{

template <class T>
class Fortran_Sparse_Col_Matrix 
{

   protected:

       Vector<T>           val_;       // data values (nz_ elements)
       Vector<Subscript>   rowind_;    // row_ind (nz_ elements)
       Vector<Subscript>   colptr_;    // col_ptr (n_+1 elements)

       int nz_;                   // number of nonzeros
       Subscript m_;              // global dimensions
       Subscript n_;
  
   public:


       Fortran_Sparse_Col_Matrix(void);
       Fortran_Sparse_Col_Matrix(const Fortran_Sparse_Col_Matrix<T> &S)
        : val_(S.val_), rowind_(S.rowind_), colptr_(S.colptr_), nz_(S.nz_),
            m_(S.m_), n_(S.n_) {};
       Fortran_Sparse_Col_Matrix(Subscript M, Subscript N, 
            Subscript nz, const T  *val,  const Subscript *r, 
            const Subscript *c) : val_(nz, val), rowind_(nz, r), 
            colptr_(N+1, c), nz_(nz), m_(M), n_(N) {};

       Fortran_Sparse_Col_Matrix(Subscript M, Subscript N, 
            Subscript nz, char *val,  char  *r, 
            char *c) : val_(nz, val), rowind_(nz, r), 
            colptr_(N+1, c), nz_(nz), m_(M), n_(N) {};

       Fortran_Sparse_Col_Matrix(Subscript M, Subscript N, 
            Subscript nz, const T  *val, Subscript *r, Subscript *c)
            : val_(nz, val), rowind_(nz, r), colptr_(N+1, c), nz_(nz), 
                    m_(M), n_(N) {};
    
      ~Fortran_Sparse_Col_Matrix() {};
        

       T &      val(Subscript i) { return val_(i); }
       const T &      val(Subscript i) const { return val_(i); }

       Subscript &   row_ind(Subscript i) { return rowind_(i); }
       const Subscript &   row_ind(Subscript i) const { return rowind_(i); }

       Subscript    col_ptr(Subscript i) { return colptr_(i);}
       const Subscript    col_ptr(Subscript i) const { return colptr_(i);}


       Subscript    num_cols() const { return m_;}
       Subscript    num_rows() const { return n_; }

       Subscript          dim(Subscript i) const 
       {
#ifdef TNT_BOUNDS_CHECK
            assert( 1 <= i );
            assert( i <= 2 );
#endif
            if (i==1) return m_;
            else if (i==2) return m_;
            else return 0;
        }

       Subscript          num_nonzeros() const {return nz_;};
       Subscript          lbound() const {return 1;}



       Fortran_Sparse_Col_Matrix& operator=(const 
            Fortran_Sparse_Col_Matrix &C)
        {
            val_ = C.val_;
            rowind_ = C.rowind_;
            colptr_ = C.colptr_;
            nz_ = C.nz_;
            m_ = C.m_;
            n_ = C.n_;

            return *this;
        }

       Fortran_Sparse_Col_Matrix& newsize(Subscript M, Subscript N, 
                Subscript nz)
        {
            val_.newsize(nz);
            rowind_.newsize(nz);
            colptr_.newsize(N+1);
            return *this;
        }
};

template <class T>
ostream& operator<<(ostream &s, const Fortran_Sparse_Col_Matrix<T> &A)
{
    Subscript M=A.num_rows();
    Subscript N=A.num_cols();

    s << M << " " << N << " " << A.num_nonzeros() <<  endl;


    for (Subscript k=1; k<=N; k++)
    {
        Subscript start = A.col_ptr(k);
        Subscript end = A.col_ptr(k+1);

        for (Subscript i= start; i<end; i++)
        {
            s << A.row_ind(i) << " " << k << " " << A.val(i) << endl;
        }
    }

    return s;
}


} // namespace TNT

#endif  /* FCSCMAT_H */

