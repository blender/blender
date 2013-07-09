/** \file smoke/intern/tnt/tnt_fortran_array3d.h
 *  \ingroup smoke
 */
/*
*
* Template Numerical Toolkit (TNT): Three-dimensional Fortran numerical array
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



#ifndef TNT_FORTRAN_ARRAY3D_H
#define TNT_FORTRAN_ARRAY3D_H

#include <cstdlib>
#include <iostream>
#ifdef TNT_BOUNDS_CHECK
#include <assert.h>
#endif
#include "tnt_i_refvec.h"

namespace TNT
{

template <class T>
class Fortran_Array3D 
{


  private: 


		i_refvec<T> v_;
		int m_;
		int n_;
		int k_;
		T* data_;

  public:

    typedef         T   value_type;

	       Fortran_Array3D();
	       Fortran_Array3D(int m, int n, int k);
	       Fortran_Array3D(int m, int n, int k,  T *a);
	       Fortran_Array3D(int m, int n, int k, const T &a);
    inline Fortran_Array3D(const Fortran_Array3D &A);
	inline Fortran_Array3D & operator=(const T &a);
	inline Fortran_Array3D & operator=(const Fortran_Array3D &A);
	inline Fortran_Array3D & ref(const Fortran_Array3D &A);
	       Fortran_Array3D copy() const;
		   Fortran_Array3D & inject(const Fortran_Array3D & A);
	inline T& operator()(int i, int j, int k);
	inline const T& operator()(int i, int j, int k) const ;
	inline int dim1() const;
	inline int dim2() const;
	inline int dim3() const;
	inline int ref_count() const;
               ~Fortran_Array3D();


};

template <class T>
Fortran_Array3D<T>::Fortran_Array3D() :  v_(), m_(0), n_(0), k_(0), data_(0) {}


template <class T>
Fortran_Array3D<T>::Fortran_Array3D(const Fortran_Array3D<T> &A) : 
	v_(A.v_), m_(A.m_), n_(A.n_), k_(A.k_), data_(A.data_) {}



template <class T>
Fortran_Array3D<T>::Fortran_Array3D(int m, int n, int k) : 
	v_(m*n*k), m_(m), n_(n), k_(k), data_(v_.begin()) {}



template <class T>
Fortran_Array3D<T>::Fortran_Array3D(int m, int n, int k, const T &val) : 
	v_(m*n*k), m_(m), n_(n), k_(k), data_(v_.begin())
{
	for (T* p = data_; p < data_ + m*n*k; p++)
		*p = val;
}

template <class T>
Fortran_Array3D<T>::Fortran_Array3D(int m, int n, int k, T *a) : 
	v_(a), m_(m), n_(n), k_(k), data_(v_.begin()) {}




template <class T>
inline T& Fortran_Array3D<T>::operator()(int i, int j, int k) 
{ 
#ifdef TNT_BOUNDS_CHECK
	assert(i >= 1);
	assert(i <= m_);
	assert(j >= 1);
	assert(j <= n_);
	assert(k >= 1);
	assert(k <= k_);
#endif

	return data_[(k-1)*m_*n_ + (j-1) * m_ + i-1];

}

template <class T>
inline const T& Fortran_Array3D<T>::operator()(int i, int j, int k)  const
{ 
#ifdef TNT_BOUNDS_CHECK
	assert(i >= 1);
	assert(i <= m_);
	assert(j >= 1);
	assert(j <= n_);
	assert(k >= 1);
	assert(k <= k_);
#endif

	return data_[(k-1)*m_*n_ + (j-1) * m_ + i-1];
}


template <class T>
Fortran_Array3D<T> & Fortran_Array3D<T>::operator=(const T &a)
{

	T *end = data_ + m_*n_*k_;

	for (T *p=data_; p != end; *p++ = a);

	return *this;
}

template <class T>
Fortran_Array3D<T> Fortran_Array3D<T>::copy() const
{

	Fortran_Array3D B(m_, n_, k_);
	B.inject(*this);
	return B;
	
}


template <class T>
Fortran_Array3D<T> & Fortran_Array3D<T>::inject(const Fortran_Array3D &A)
{

	if (m_ == A.m_ && n_ == A.n_ && k_ == A.k_)
	{
		T *p = data_;
		T *end = data_ + m_*n_*k_;
		const T* q = A.data_;
		for (; p < end; *p++ =  *q++);
	}
	return *this;
}




template <class T>
Fortran_Array3D<T> & Fortran_Array3D<T>::ref(const Fortran_Array3D<T> &A)
{

	if (this != &A)
	{
		v_ = A.v_;
		m_ = A.m_;
		n_ = A.n_;
		k_ = A.k_;
		data_ = A.data_;
	}
	return *this;
}

template <class T>
Fortran_Array3D<T> & Fortran_Array3D<T>::operator=(const Fortran_Array3D<T> &A)
{
	return ref(A);
}

template <class T>
inline int Fortran_Array3D<T>::dim1() const { return m_; }

template <class T>
inline int Fortran_Array3D<T>::dim2() const { return n_; }

template <class T>
inline int Fortran_Array3D<T>::dim3() const { return k_; }


template <class T>
inline int Fortran_Array3D<T>::ref_count() const 
{ 
	return v_.ref_count(); 
}

template <class T>
Fortran_Array3D<T>::~Fortran_Array3D()
{
}


} /* namespace TNT */

#endif
/* TNT_FORTRAN_ARRAY3D_H */

