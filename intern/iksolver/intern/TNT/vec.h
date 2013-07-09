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


// Basic TNT  numerical vector (0-based [i] AND 1-based (i) indexing )
//

#ifndef VEC_H
#define VEC_H

#include "subscript.h"
#include <stdlib.h>
#include <assert.h>
#include <iostream>

namespace TNT
{

template <class T>
class Vector 
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
    T* v_;                  
    T* vm1_;        // pointer adjustment for optimzied 1-offset indexing
    Subscript n_;

    // internal helper function to create the array
    // of row pointers

    void initialize(Subscript N)
    {
        // adjust pointers so that they are 1-offset:
        // v_[] is the internal contiguous array, it is still 0-offset
        //
        assert(v_ == NULL);
        v_ = new T[N];
        assert(v_  != NULL);
        vm1_ = v_-1;
        n_ = N;
    }
   
    void copy(const T*  v)
    {
        Subscript N = n_;
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
        Subscript N = n_;
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
        delete [] (v_);     

        v_ = NULL;
        vm1_ = NULL;
    }


  public:

    // access

    iterator begin() { return v_;}
    iterator end()   { return v_ + n_; }
    const iterator begin() const { return v_;}
    const iterator end() const  { return v_ + n_; }

    // destructor

    ~Vector() 
    {
        destroy();
    }

    // constructors

    Vector() : v_(0), vm1_(0), n_(0)  {}

    Vector(const Vector<T> &A) : v_(0), vm1_(0), n_(0)
    {
        initialize(A.n_);
        copy(A.v_);
    }

    Vector(Subscript N, const T& value = T()) :  v_(0), vm1_(0), n_(0)
    {
        initialize(N);
        set(value);
    }

    Vector(Subscript N, const T* v) :  v_(0), vm1_(0), n_(0)
    {
        initialize(N);
        copy(v);
    }


    // methods
    // 
    Vector<T>& newsize(Subscript N)
    {
        if (n_ == N) return *this;

        destroy();
        initialize(N);

        return *this;
    }


    // assignments
    //
    Vector<T>& operator=(const Vector<T> &A)
    {
        if (v_ == A.v_)
            return *this;

        if (n_ == A.n_)         // no need to re-alloc
            copy(A.v_);

        else
        {
            destroy();
            initialize(A.n_);
            copy(A.v_);
        }

        return *this;
    }
        
    Vector<T>& operator=(const T& scalar)
    { 
        set(scalar);  
        return *this;
    }

    inline Subscript dim() const 
    {
        return  n_; 
    }

    inline Subscript size() const 
    {
        return  n_; 
    }


    inline reference operator()(Subscript i)
    { 
#ifdef TNT_BOUNDS_CHECK
        assert(1<=i);
        assert(i <= n_) ;
#endif
        return vm1_[i]; 
    }

    inline const_reference operator() (Subscript i) const
    {
#ifdef TNT_BOUNDS_CHECK
        assert(1<=i);
        assert(i <= n_) ;
#endif
        return vm1_[i]; 
    }

    inline reference operator[](Subscript i)
    { 
#ifdef TNT_BOUNDS_CHECK
        assert(0<=i);
        assert(i < n_) ;
#endif
        return v_[i]; 
    }

    inline const_reference operator[](Subscript i) const
    {
#ifdef TNT_BOUNDS_CHECK
        assert(0<=i) ;
        assert(i < n_) ;
#endif
        return v_[i]; 
    }



};


/* ***************************  I/O  ********************************/

template <class T>
std::ostream& operator<<(std::ostream &s, const Vector<T> &A)
{
    Subscript N=A.dim();

    s <<  N << std::endl;

    for (Subscript i=0; i<N; i++)
        s   << A[i] << " " << std::endl;
    s << std::endl;

    return s;
}

template <class T>
std::istream & operator>>(std::istream &s, Vector<T> &A)
{

    Subscript N;

    s >> N;

    if ( !(N == A.size() ))
    {
        A.newsize(N);
    }


    for (Subscript i=0; i<N; i++)
            s >>  A[i];


    return s;
}

// *******************[ basic matrix algorithms ]***************************


template <class T>
Vector<T> operator+(const Vector<T> &A, 
    const Vector<T> &B)
{
    Subscript N = A.dim();

    assert(N==B.dim());

    Vector<T> tmp(N);
    Subscript i;

    for (i=0; i<N; i++)
            tmp[i] = A[i] + B[i];

    return tmp;
}

template <class T>
Vector<T> operator-(const Vector<T> &A, 
    const Vector<T> &B)
{
    Subscript N = A.dim();

    assert(N==B.dim());

    Vector<T> tmp(N);
    Subscript i;

    for (i=0; i<N; i++)
            tmp[i] = A[i] - B[i];

    return tmp;
}

template <class T>
Vector<T> operator*(const Vector<T> &A, 
    const Vector<T> &B)
{
    Subscript N = A.dim();

    assert(N==B.dim());

    Vector<T> tmp(N);
    Subscript i;

    for (i=0; i<N; i++)
            tmp[i] = A[i] * B[i];

    return tmp;
}

template <class T>
Vector<T> operator*(const Vector<T> &A, 
    const T &B)
{
    Subscript N = A.dim();

    Vector<T> tmp(N);
    Subscript i;

    for (i=0; i<N; i++)
            tmp[i] = A[i] * B;

    return tmp;
}


template <class T>
T dot_prod(const Vector<T> &A, const Vector<T> &B)
{
    Subscript N = A.dim();
    assert(N == B.dim());

    Subscript i;
    T sum = 0;

    for (i=0; i<N; i++)
        sum += A[i] * B[i];

    return sum;
}

// inplace versions of the above template functions

// A = A + B

template <class T>
void vectoradd(
	Vector<T> &A, 
    const Vector<T> &B)
{
    const Subscript N = A.dim();
    assert(N==B.dim());
    Subscript i;

    for (i=0; i<N; i++)
            A[i] += B[i];
}

// same with separate output vector

template <class T>
void vectoradd(
	Vector<T> &C,
	const Vector<T> &A, 
    const Vector<T> &B)
{
    const Subscript N = A.dim();
    assert(N==B.dim());
    Subscript i;

    for (i=0; i<N; i++)
            C[i] = A[i] + B[i];
}

// A = A - B

template <class T>
void vectorsub(
	Vector<T> &A, 
    const Vector<T> &B)
{
    const Subscript N = A.dim();
    assert(N==B.dim());
    Subscript i;

    for (i=0; i<N; i++)
            A[i] -= B[i];
}

template <class T>
void vectorsub(
	Vector<T> &C,
	const Vector<T> &A, 
    const Vector<T> &B)
{
    const Subscript N = A.dim();
    assert(N==B.dim());
    Subscript i;

    for (i=0; i<N; i++)
            C[i] = A[i] - B[i];
}

template <class T>
void vectorscale(
	Vector<T> &C,
	const Vector<T> &A, 
    const T &B)
{
    const Subscript N = A.dim();
    Subscript i;

    for (i=0; i<N; i++)
            C[i] = A[i] * B;
}

template <class T>
void vectorscale(
	Vector<T> &A, 
    const T &B)
{
    const Subscript N = A.dim();
    Subscript i;

    for (i=0; i<N; i++)
            A[i] *= B;
}

}   /* namespace TNT */

#endif // VEC_H

