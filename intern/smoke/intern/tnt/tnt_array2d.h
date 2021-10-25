/** \file smoke/intern/tnt/tnt_array2d.h
 *  \ingroup smoke
 */
/*
*
* Template Numerical Toolkit (TNT)
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



#ifndef TNT_ARRAY2D_H
#define TNT_ARRAY2D_H

#include <cstdlib>
#include <iostream>
#ifdef TNT_BOUNDS_CHECK
#include <assert.h>
#endif

#include "tnt_array1d.h"

namespace TNT
{

template <class T>
class Array2D 
{


  private:



  	Array1D<T> data_;
	Array1D<T*> v_;
	int m_;
    int n_;

  public:

    typedef         T   value_type;
	       Array2D();
	       Array2D(int m, int n);
	       Array2D(int m, int n,  T *a);
	       Array2D(int m, int n, const T &a);
    inline Array2D(const Array2D &A);
	inline operator T**();
	inline operator const T**();
	inline Array2D & operator=(const T &a);
	inline Array2D & operator=(const Array2D &A);
	inline Array2D & ref(const Array2D &A);
	       Array2D copy() const;
		   Array2D & inject(const Array2D & A);
	inline T* operator[](int i);
	inline const T* operator[](int i) const;
	inline int dim1() const;
	inline int dim2() const;
     ~Array2D();

	/* extended interface (not part of the standard) */


	inline int ref_count();
	inline int ref_count_data();
	inline int ref_count_dim1();
	Array2D subarray(int i0, int i1, int j0, int j1);

};


template <class T>
Array2D<T>::Array2D() : data_(), v_(), m_(0), n_(0) {} 

template <class T>
Array2D<T>::Array2D(const Array2D<T> &A) : data_(A.data_), v_(A.v_), 
	m_(A.m_), n_(A.n_) {}




template <class T>
Array2D<T>::Array2D(int m, int n) : data_(m*n), v_(m), m_(m), n_(n)
{
	if (m>0 && n>0)
	{
		T* p = &(data_[0]);
		for (int i=0; i<m; i++)
		{
			v_[i] = p;
			p += n;
		}
	}
}



template <class T>
Array2D<T>::Array2D(int m, int n, const T &val) : data_(m*n), v_(m), 
													m_(m), n_(n) 
{
  if (m>0 && n>0)
  {
	data_ = val;
	T* p  = &(data_[0]);
	for (int i=0; i<m; i++)
	{
			v_[i] = p;
			p += n;
	}
  }
}

template <class T>
Array2D<T>::Array2D(int m, int n, T *a) : data_(m*n, a), v_(m), m_(m), n_(n)
{
  if (m>0 && n>0)
  {
	T* p = &(data_[0]);
	
	for (int i=0; i<m; i++)
	{
			v_[i] = p;
			p += n;
	}
  }
}


template <class T>
inline T* Array2D<T>::operator[](int i) 
{ 
#ifdef TNT_BOUNDS_CHECK
	assert(i >= 0);
	assert(i < m_);
#endif

return v_[i]; 

}


template <class T>
inline const T* Array2D<T>::operator[](int i) const
{ 
#ifdef TNT_BOUNDS_CHECK
	assert(i >= 0);
	assert(i < m_);
#endif

return v_[i]; 

}

template <class T>
Array2D<T> & Array2D<T>::operator=(const T &a)
{
	/* non-optimzied, but will work with subarrays in future verions */

	for (int i=0; i<m_; i++)
		for (int j=0; j<n_; j++)
		v_[i][j] = a;
	return *this;
}




template <class T>
Array2D<T> Array2D<T>::copy() const
{
	Array2D A(m_, n_);

	for (int i=0; i<m_; i++)
		for (int j=0; j<n_; j++)
			A[i][j] = v_[i][j];


	return A;
}


template <class T>
Array2D<T> & Array2D<T>::inject(const Array2D &A)
{
	if (A.m_ == m_ &&  A.n_ == n_)
	{
		for (int i=0; i<m_; i++)
			for (int j=0; j<n_; j++)
				v_[i][j] = A[i][j];
	}
	return *this;
}




template <class T>
Array2D<T> & Array2D<T>::ref(const Array2D<T> &A)
{
	if (this != &A)
	{
		v_ = A.v_;
		data_ = A.data_;
		m_ = A.m_;
		n_ = A.n_;
		
	}
	return *this;
}



template <class T>
Array2D<T> & Array2D<T>::operator=(const Array2D<T> &A)
{
	return ref(A);
}

template <class T>
inline int Array2D<T>::dim1() const { return m_; }

template <class T>
inline int Array2D<T>::dim2() const { return n_; }


template <class T>
Array2D<T>::~Array2D() {}




template <class T>
inline Array2D<T>::operator T**()
{
	return &(v_[0]);
}
template <class T>
inline Array2D<T>::operator const T**()
{
	return &(v_[0]);
}

/* ............... extended interface ............... */
/**
	Create a new view to a subarray defined by the boundaries
	[i0][i0] and [i1][j1].  The size of the subarray is
	(i1-i0) by (j1-j0).  If either of these lengths are zero
	or negative, the subarray view is null.

*/
template <class T>
Array2D<T> Array2D<T>::subarray(int i0, int i1, int j0, int j1) 
{
	Array2D<T> A;
	int m = i1-i0+1;
	int n = j1-j0+1;

	/* if either length is zero or negative, this is an invalide
		subarray. return a null view.
	*/
	if (m<1 || n<1)
		return A;

	A.data_ = data_;
	A.m_ = m;
	A.n_ = n;
	A.v_ = Array1D<T*>(m);
	T* p = &(data_[0]) + i0 *  n_ + j0;
	for (int i=0; i<m; i++)
	{
		A.v_[i] = p + i*n_;

	}	
	return A;
}

template <class T>
inline int Array2D<T>::ref_count()
{
	return ref_count_data();
}



template <class T>
inline int Array2D<T>::ref_count_data()
{
	return data_.ref_count();
}

template <class T>
inline int Array2D<T>::ref_count_dim1()
{
	return v_.ref_count();
}




} /* namespace TNT */

#endif
/* TNT_ARRAY2D_H */

