/** \file smoke/intern/tnt/tnt_fortran_array2d.h
 *  \ingroup smoke
 */
/*
*
* Template Numerical Toolkit (TNT): Two-dimensional Fortran numerical array
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
* and is in the public domain. NIST assumes no responsibility whatsoever for
* its use by other parties, and makes no guarantees, expressed or implied,
* about its quality, reliability, or any other characteristic.
*
*/



#ifndef TNT_FORTRAN_ARRAY2D_H
#define TNT_FORTRAN_ARRAY2D_H

#include <cstdlib>
#include <iostream>

#ifdef TNT_BOUNDS_CHECK
#include <assert.h>
#endif

#include "tnt_i_refvec.h"

namespace TNT
{

template <class T>
class Fortran_Array2D 
{


  private: 
  		i_refvec<T> v_;
		int m_;
		int n_;
		T* data_;


    	void initialize_(int n);
    	void copy_(T* p, const T*  q, int len);
    	void set_(T* begin,  T* end, const T& val);
 
  public:

    typedef         T   value_type;

	       Fortran_Array2D();
	       Fortran_Array2D(int m, int n);
	       Fortran_Array2D(int m, int n,  T *a);
	       Fortran_Array2D(int m, int n, const T &a);
    inline Fortran_Array2D(const Fortran_Array2D &A);
	inline Fortran_Array2D & operator=(const T &a);
	inline Fortran_Array2D & operator=(const Fortran_Array2D &A);
	inline Fortran_Array2D & ref(const Fortran_Array2D &A);
	       Fortran_Array2D copy() const;
		   Fortran_Array2D & inject(const Fortran_Array2D & A);
	inline T& operator()(int i, int j);
	inline const T& operator()(int i, int j) const ;
	inline int dim1() const;
	inline int dim2() const;
               ~Fortran_Array2D();

	/* extended interface */

	inline int ref_count() const;

};

template <class T>
Fortran_Array2D<T>::Fortran_Array2D() : v_(), m_(0), n_(0), data_(0) {}


template <class T>
Fortran_Array2D<T>::Fortran_Array2D(const Fortran_Array2D<T> &A) : v_(A.v_),
		m_(A.m_), n_(A.n_), data_(A.data_) {}



template <class T>
Fortran_Array2D<T>::Fortran_Array2D(int m, int n) : v_(m*n), m_(m), n_(n),
	data_(v_.begin()) {}

template <class T>
Fortran_Array2D<T>::Fortran_Array2D(int m, int n, const T &val) : 
	v_(m*n), m_(m), n_(n), data_(v_.begin())
{
	set_(data_, data_+m*n, val);
}


template <class T>
Fortran_Array2D<T>::Fortran_Array2D(int m, int n, T *a) : v_(a),
	m_(m), n_(n), data_(v_.begin()) {}




template <class T>
inline T& Fortran_Array2D<T>::operator()(int i, int j) 
{ 
#ifdef TNT_BOUNDS_CHECK
	assert(i >= 1);
	assert(i <= m_);
	assert(j >= 1);
	assert(j <= n_);
#endif

	return v_[ (j-1)*m_ + (i-1) ];

}

template <class T>
inline const T& Fortran_Array2D<T>::operator()(int i, int j) const
{ 
#ifdef TNT_BOUNDS_CHECK
	assert(i >= 1);
	assert(i <= m_);
	assert(j >= 1);
	assert(j <= n_);
#endif

	return v_[ (j-1)*m_ + (i-1) ];

}


template <class T>
Fortran_Array2D<T> & Fortran_Array2D<T>::operator=(const T &a)
{
 	set_(data_, data_+m_*n_, a);
	return *this;
}

template <class T>
Fortran_Array2D<T> Fortran_Array2D<T>::copy() const
{

	Fortran_Array2D B(m_,n_);
	
	B.inject(*this);
	return B;
}


template <class T>
Fortran_Array2D<T> & Fortran_Array2D<T>::inject(const Fortran_Array2D &A)
{
	if (m_ == A.m_ && n_ == A.n_)
		copy_(data_, A.data_, m_*n_);

	return *this;
}



template <class T>
Fortran_Array2D<T> & Fortran_Array2D<T>::ref(const Fortran_Array2D<T> &A)
{
	if (this != &A)
	{
		v_ = A.v_;
		m_ = A.m_;
		n_ = A.n_;
		data_ = A.data_;
	}
	return *this;
}

template <class T>
Fortran_Array2D<T> & Fortran_Array2D<T>::operator=(const Fortran_Array2D<T> &A)
{
	return ref(A);
}

template <class T>
inline int Fortran_Array2D<T>::dim1() const { return m_; }

template <class T>
inline int Fortran_Array2D<T>::dim2() const { return n_; }


template <class T>
Fortran_Array2D<T>::~Fortran_Array2D()
{
}

template <class T>
inline int Fortran_Array2D<T>::ref_count() const { return v_.ref_count(); }




template <class T>
void Fortran_Array2D<T>::set_(T* begin, T* end, const T& a)
{
	for (T* p=begin; p<end; p++)
		*p = a;

}

template <class T>
void Fortran_Array2D<T>::copy_(T* p, const T* q, int len) 
{
	T *end = p + len;
	while (p<end )
		*p++ = *q++;

}


} /* namespace TNT */

#endif
/* TNT_FORTRAN_ARRAY2D_H */

