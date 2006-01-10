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



// C compatible matrix: row-oriented, 0-based [i][j] and 1-based (i,j) indexing
//

#ifndef CMAT_H
#define CMAT_H

#include "subscript.h"
#include "vec.h"
#include <stdlib.h>
#include <assert.h>
#include <iostream>
#ifdef TNT_USE_REGIONS
#include "region2d.h"
#endif

namespace TNT
{

template <class T>
class Matrix 
{


  public:

    typedef Subscript   size_type;
    typedef         T   value_type;
    typedef         T   element_type;
    typedef         T*  pointer;
    typedef         T*  iterator;
    typedef         T&  reference;
    typedef const   T*  const_iterator;
    typedef const   T&  const_reference;

    Subscript lbound() const { return 1;}
 
  protected:
    Subscript m_;
    Subscript n_;
    Subscript mn_;      // total size
    T* v_;                  
    T** row_;           
    T* vm1_ ;       // these point to the same data, but are 1-based 
    T** rowm1_;

    // internal helper function to create the array
    // of row pointers

    void initialize(Subscript M, Subscript N)
    {
        mn_ = M*N;
        m_ = M;
        n_ = N;

        v_ = new T[mn_]; 
        row_ = new T*[M];
        rowm1_ = new T*[M];

        assert(v_  != NULL);
        assert(row_  != NULL);
        assert(rowm1_ != NULL);

        T* p = v_;              
        vm1_ = v_ - 1;
        for (Subscript i=0; i<M; i++)
        {
            row_[i] = p;
            rowm1_[i] = p-1;
            p += N ;
            
        }

        rowm1_ -- ;     // compensate for 1-based offset
    }
   
    void copy(const T*  v)
    {
        Subscript N = m_ * n_;
        Subscript i;

#ifdef TNT_UNROLL_LOOPS
        Subscript Nmod4 = N & 3;
        Subscript N4 = N - Nmod4;

        for (i=0; i<N4; i+=4)
        {
            v_[i] = v[i];
            v_[i+1] = v[i+1];
            v_[i+2] = v[i+2];
            v_[i+3] = v[i+3];
        }

        for (i=N4; i< N; i++)
            v_[i] = v[i];
#else

        for (i=0; i< N; i++)
            v_[i] = v[i];
#endif      
    }

    void set(const T& val)
    {
        Subscript N = m_ * n_;
        Subscript i;

#ifdef TNT_UNROLL_LOOPS
        Subscript Nmod4 = N & 3;
        Subscript N4 = N - Nmod4;

        for (i=0; i<N4; i+=4)
        {
            v_[i] = val;
            v_[i+1] = val;
            v_[i+2] = val;
            v_[i+3] = val; 
        }

        for (i=N4; i< N; i++)
            v_[i] = val;
#else

        for (i=0; i< N; i++)
            v_[i] = val;
        
#endif      
    }
    

    
    void destroy()
    {     
        /* do nothing, if no memory has been previously allocated */
        if (v_ == NULL) return ;

        /* if we are here, then matrix was previously allocated */
        if (v_ != NULL) delete [] (v_);     
        if (row_ != NULL) delete [] (row_);

        /* return rowm1_ back to original value */
        rowm1_ ++;
        if (rowm1_ != NULL ) delete [] (rowm1_);
    }


  public:

    operator T**(){ return  row_; }
    operator T**() const { return row_; }


    Subscript size() const { return mn_; }

    // constructors

    Matrix() : m_(0), n_(0), mn_(0), v_(0), row_(0), vm1_(0), rowm1_(0) {};

    Matrix(const Matrix<T> &A)
    {
        initialize(A.m_, A.n_);
        copy(A.v_);
    }

    Matrix(Subscript M, Subscript N, const T& value = T())
    {
        initialize(M,N);
        set(value);
    }

    Matrix(Subscript M, Subscript N, const T* v)
    {
        initialize(M,N);
        copy(v);
    }


    // destructor
    //
    ~Matrix()
    {
        destroy();
    }


    // reallocating
    //
    Matrix<T>& newsize(Subscript M, Subscript N)
    {
        if (num_rows() == M && num_cols() == N)
            return *this;

        destroy();
        initialize(M,N);
        
        return *this;
    }

		void
	diagonal(Vector<T> &diag)
	{
		int sz = diag.dim();
		newsize(sz,sz);
		set(0);

		Subscript i;
		for (i = 0; i < sz; i++) {
			row_[i][i] = diag[i];
		}
	}



    // assignments
    //
    Matrix<T>& operator=(const Matrix<T> &A)
    {
        if (v_ == A.v_)
            return *this;

        if (m_ == A.m_  && n_ == A.n_)      // no need to re-alloc
            copy(A.v_);

        else
        {
            destroy();
            initialize(A.m_, A.n_);
            copy(A.v_);
        }

        return *this;
    }
        
    Matrix<T>& operator=(const T& scalar)
    { 
        set(scalar); 
        return *this;
    }


    Subscript dim(Subscript d) const 
    {
#ifdef TNT_BOUNDS_CHECK
       assert( d >= 1);
        assert( d <= 2);
#endif
        return (d==1) ? m_ : ((d==2) ? n_ : 0); 
    }

    Subscript num_rows() const { return m_; }
    Subscript num_cols() const { return n_; }




    inline T* operator[](Subscript i)
    {
#ifdef TNT_BOUNDS_CHECK
        assert(0<=i);
        assert(i < m_) ;
#endif
        return row_[i];
    }

    inline const T* operator[](Subscript i) const
    {
#ifdef TNT_BOUNDS_CHECK
        assert(0<=i);
        assert(i < m_) ;
#endif
        return row_[i];
    }

    inline reference operator()(Subscript i)
    { 
#ifdef TNT_BOUNDS_CHECK
        assert(1<=i);
        assert(i <= mn_) ;
#endif
        return vm1_[i]; 
    }

    inline const_reference operator()(Subscript i) const
    { 
#ifdef TNT_BOUNDS_CHECK
        assert(1<=i);
        assert(i <= mn_) ;
#endif
        return vm1_[i]; 
    }



    inline reference operator()(Subscript i, Subscript j)
    { 
#ifdef TNT_BOUNDS_CHECK
        assert(1<=i);
        assert(i <= m_) ;
        assert(1<=j);
        assert(j <= n_);
#endif
        return  rowm1_[i][j]; 
    }


    
    inline const_reference operator() (Subscript i, Subscript j) const
    {
#ifdef TNT_BOUNDS_CHECK
        assert(1<=i);
        assert(i <= m_) ;
        assert(1<=j);
        assert(j <= n_);
#endif
        return rowm1_[i][j]; 
    }



#ifdef TNT_USE_REGIONS

    typedef Region2D<Matrix<T> > Region;
    

    Region operator()(const Index1D &I, const Index1D &J)
    {
        return Region(*this, I,J);
    }


    typedef const_Region2D< Matrix<T> > const_Region;
    const_Region operator()(const Index1D &I, const Index1D &J) const
    {
        return const_Region(*this, I,J);
    }

#endif


};


/* ***************************  I/O  ********************************/

template <class T>
std::ostream& operator<<(std::ostream &s, const Matrix<T> &A)
{
    Subscript M=A.num_rows();
    Subscript N=A.num_cols();

    s << M << " " << N << "\n";

    for (Subscript i=0; i<M; i++)
    {
        for (Subscript j=0; j<N; j++)
        {
            s << A[i][j] << " ";
        }
        s << "\n";
    }


    return s;
}

template <class T>
std::istream& operator>>(std::istream &s, Matrix<T> &A)
{

    Subscript M, N;

    s >> M >> N;

    if ( !(M == A.num_rows() && N == A.num_cols() ))
    {
        A.newsize(M,N);
    }


    for (Subscript i=0; i<M; i++)
        for (Subscript j=0; j<N; j++)
        {
            s >>  A[i][j];
        }


    return s;
}

// *******************[ basic matrix algorithms ]***************************

template <class T>
Matrix<T> operator+(const Matrix<T> &A, 
    const Matrix<T> &B)
{
    Subscript M = A.num_rows();
    Subscript N = A.num_cols();

    assert(M==B.num_rows());
    assert(N==B.num_cols());

    Matrix<T> tmp(M,N);
    Subscript i,j;

    for (i=0; i<M; i++)
        for (j=0; j<N; j++)
            tmp[i][j] = A[i][j] + B[i][j];

    return tmp;
}

template <class T>
Matrix<T> operator-(const Matrix<T> &A, 
    const Matrix<T> &B)
{
    Subscript M = A.num_rows();
    Subscript N = A.num_cols();

    assert(M==B.num_rows());
    assert(N==B.num_cols());

    Matrix<T> tmp(M,N);
    Subscript i,j;

    for (i=0; i<M; i++)
        for (j=0; j<N; j++)
            tmp[i][j] = A[i][j] - B[i][j];

    return tmp;
}

template <class T>
Matrix<T> mult_element(const Matrix<T> &A, 
    const Matrix<T> &B)
{
    Subscript M = A.num_rows();
    Subscript N = A.num_cols();

    assert(M==B.num_rows());
    assert(N==B.num_cols());

    Matrix<T> tmp(M,N);
    Subscript i,j;

    for (i=0; i<M; i++)
        for (j=0; j<N; j++)
            tmp[i][j] = A[i][j] * B[i][j];

    return tmp;
}

template <class T>
void transpose(const Matrix<T> &A, Matrix<T> &S)
{
    Subscript M = A.num_rows();
    Subscript N = A.num_cols();

    assert(M==S.num_cols());
    assert(N==S.num_rows());

    Subscript i, j;

    for (i=0; i<M; i++)
        for (j=0; j<N; j++)
            S[j][i] = A[i][j];

}


template <class T>
inline void matmult(Matrix<T>& C, const Matrix<T>  &A, 
    const Matrix<T> &B)
{

    assert(A.num_cols() == B.num_rows());

    Subscript M = A.num_rows();
    Subscript N = A.num_cols();
    Subscript K = B.num_cols();

    C.newsize(M,K);

    T sum;

    const T* row_i;
    const T* col_k;

    for (Subscript i=0; i<M; i++)
    for (Subscript k=0; k<K; k++)
    {
        row_i  = &(A[i][0]);
        col_k  = &(B[0][k]);
        sum = 0;
        for (Subscript j=0; j<N; j++)
        {
            sum  += *row_i * *col_k;
            row_i++;
            col_k += K;
        }
        C[i][k] = sum; 
    }

}

template <class T>
void matmult(Vector<T> &y, const Matrix<T>  &A, const Vector<T> &x)
{

#ifdef TNT_BOUNDS_CHECK
    assert(A.num_cols() == x.dim());
	assert(A.num_rows() == y.dim());
#endif

    Subscript M = A.num_rows();
    Subscript N = A.num_cols();

    T sum;

    for (Subscript i=0; i<M; i++)
    {
        sum = 0;
        const T* rowi = A[i];
        for (Subscript j=0; j<N; j++)
            sum = sum +  rowi[j] * x[j];

        y[i] = sum; 
    }
}

template <class T>
inline void matmultdiag(
	Matrix<T>& C, 
	const Matrix<T> &A, 
    const Vector<T> &diag
){
#ifdef TNT_BOUNDS_CHECK
    assert(A.num_cols() ==A.num_rows()== diag.dim());
#endif

    Subscript M = A.num_rows();
    Subscript K = diag.dim();

    C.newsize(M,K);

    const T* row_i;
    const T* col_k;

    for (Subscript i=0; i<M; i++) {
		for (Subscript k=0; k<K; k++)
		{
			C[i][k] = A[i][k] * diag[k];		
		}
	}
}
	

template <class T>
inline void matmultdiag(
	Matrix<T>& C, 
    const Vector<T> &diag,
	const Matrix<T> &A 
){
#ifdef TNT_BOUNDS_CHECK
    assert(A.num_cols() ==A.num_rows()== diag.dim());
#endif

    Subscript M = A.num_rows();
    Subscript K = diag.dim();

    C.newsize(M,K);

    for (Subscript i=0; i<M; i++) {
	
		const T diag_element = diag[i];

		for (Subscript k=0; k<K; k++)
		{
			C[i][k] = A[i][k] * diag_element;		
		}
	}
}

} // namespace TNT

#endif // CMAT_H

