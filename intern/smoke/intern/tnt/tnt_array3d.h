/** \file smoke/intern/tnt/tnt_array3d.h
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



#ifndef TNT_ARRAY3D_H
#define TNT_ARRAY3D_H

#include <cstdlib>
#include <iostream>
#ifdef TNT_BOUNDS_CHECK
#include <assert.h>
#endif

#include "tnt_array1d.h"
#include "tnt_array2d.h"

namespace TNT
{

template <class T>
class Array3D 
{


  private:
  	Array1D<T> data_;
	Array2D<T*> v_;
	int m_;
    int n_;
	int g_;


  public:

    typedef         T   value_type;

	       Array3D();
	       Array3D(int m, int n, int g);
	       Array3D(int m, int n, int g,  T val);
	       Array3D(int m, int n, int g, T *a);

	inline operator T***();
	inline operator const T***();
    inline Array3D(const Array3D &A);
	inline Array3D & operator=(const T &a);
	inline Array3D & operator=(const Array3D &A);
	inline Array3D & ref(const Array3D &A);
	       Array3D copy() const;
		   Array3D & inject(const Array3D & A);

	inline T** operator[](int i);
	inline const T* const * operator[](int i) const;
	inline int dim1() const;
	inline int dim2() const;
	inline int dim3() const;
               ~Array3D();

	/* extended interface */

	inline int ref_count(){ return data_.ref_count(); }
   Array3D subarray(int i0, int i1, int j0, int j1, 
		   		int k0, int k1);
};

template <class T>
Array3D<T>::Array3D() : data_(), v_(), m_(0), n_(0) {}

template <class T>
Array3D<T>::Array3D(const Array3D<T> &A) : data_(A.data_), 
	v_(A.v_), m_(A.m_), n_(A.n_), g_(A.g_)
{
}



template <class T>
Array3D<T>::Array3D(int m, int n, int g) : data_(m*n*g), v_(m,n),
	m_(m), n_(n), g_(g)
{

  if (m>0 && n>0 && g>0)
  {
	T* p = & (data_[0]);
	int ng = n_*g_;

	for (int i=0; i<m_; i++)
	{	
		T* ping = p+ i*ng;
		for (int j=0; j<n; j++)
			v_[i][j] = ping + j*g_;
	}
  }
}



template <class T>
Array3D<T>::Array3D(int m, int n, int g, T val) : data_(m*n*g, val), 
	v_(m,n), m_(m), n_(n), g_(g)
{
  if (m>0 && n>0 && g>0)
  {

	T* p = & (data_[0]);
	int ng = n_*g_;

	for (int i=0; i<m_; i++)
	{	
		T* ping = p+ i*ng;
		for (int j=0; j<n; j++)
			v_[i][j] = ping + j*g_;
	}
  }
}



template <class T>
Array3D<T>::Array3D(int m, int n, int g, T* a) : 
		data_(m*n*g, a), v_(m,n), m_(m), n_(n), g_(g)
{

  if (m>0 && n>0 && g>0)
  {
	T* p = & (data_[0]);
	int ng = n_*g_;

	for (int i=0; i<m_; i++)
	{	
		T* ping = p+ i*ng;
		for (int j=0; j<n; j++)
			v_[i][j] = ping + j*g_;
	}
  }
}



template <class T>
inline T** Array3D<T>::operator[](int i) 
{ 
#ifdef TNT_BOUNDS_CHECK
	assert(i >= 0);
	assert(i < m_);
#endif

return v_[i]; 

}

template <class T>
inline const T* const * Array3D<T>::operator[](int i) const 
{ return v_[i]; }

template <class T>
Array3D<T> & Array3D<T>::operator=(const T &a)
{
	for (int i=0; i<m_; i++)
		for (int j=0; j<n_; j++)
			for (int k=0; k<g_; k++)
				v_[i][j][k] = a;

	return *this;
}

template <class T>
Array3D<T> Array3D<T>::copy() const
{
	Array3D A(m_, n_, g_);
	for (int i=0; i<m_; i++)
		for (int j=0; j<n_; j++)
			for (int k=0; k<g_; k++)
				A.v_[i][j][k] = v_[i][j][k];

	return A;
}


template <class T>
Array3D<T> & Array3D<T>::inject(const Array3D &A)
{
	if (A.m_ == m_ &&  A.n_ == n_ && A.g_ == g_)

	for (int i=0; i<m_; i++)
		for (int j=0; j<n_; j++)
			for (int k=0; k<g_; k++)
				v_[i][j][k] = A.v_[i][j][k];

	return *this;
}



template <class T>
Array3D<T> & Array3D<T>::ref(const Array3D<T> &A)
{
	if (this != &A)
	{
		m_ = A.m_;
		n_ = A.n_;
		g_ = A.g_;
		v_ = A.v_;
		data_ = A.data_;
	}
	return *this;
}

template <class T>
Array3D<T> & Array3D<T>::operator=(const Array3D<T> &A)
{
	return ref(A);
}


template <class T>
inline int Array3D<T>::dim1() const { return m_; }

template <class T>
inline int Array3D<T>::dim2() const { return n_; }

template <class T>
inline int Array3D<T>::dim3() const { return g_; }



template <class T>
Array3D<T>::~Array3D() {}

template <class T>
inline Array3D<T>::operator T***()
{
	return v_;
}


template <class T>
inline Array3D<T>::operator const T***()
{
	return v_;
}

/* extended interface */
template <class T>
Array3D<T> Array3D<T>::subarray(int i0, int i1, int j0,
	int j1, int k0, int k1)
{

	/* check that ranges are valid. */
	if (!( 0 <= i0 && i0 <= i1 && i1 < m_ &&
	      0 <= j0 && j0 <= j1 && j1 < n_ &&
	      0 <= k0 && k0 <= k1 && k1 < g_))
		return Array3D<T>();  /* null array */


	Array3D<T> A;
	A.data_ = data_;
	A.m_ = i1-i0+1;
	A.n_ = j1-j0+1;
	A.g_ = k1-k0+1;
	A.v_ = Array2D<T*>(A.m_,A.n_);
	T* p = &(data_[0]) + i0*n_*g_ + j0*g_ + k0; 

	for (int i=0; i<A.m_; i++)
	{
		T* ping = p + i*n_*g_;
		for (int j=0; j<A.n_; j++)
			A.v_[i][j] = ping + j*g_ ;
	}

	return A;
}
	


} /* namespace TNT */

#endif
/* TNT_ARRAY3D_H */

