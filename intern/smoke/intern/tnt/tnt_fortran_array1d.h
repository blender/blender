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



#ifndef TNT_FORTRAN_ARRAY1D_H
#define TNT_FORTRAN_ARRAY1D_H

#include <cstdlib>
#include <iostream>

#ifdef TNT_BOUNDS_CHECK
#include <assert.h>
#endif


#include "tnt_i_refvec.h"

namespace TNT
{

template <class T>
class Fortran_Array1D 
{

  private:

    i_refvec<T> v_;
    int n_;
    T* data_;				/* this normally points to v_.begin(), but
                             * could also point to a portion (subvector)
							 * of v_.
                            */

    void initialize_(int n);
    void copy_(T* p, const T*  q, int len) const;
    void set_(T* begin,  T* end, const T& val);
 

  public:

    typedef         T   value_type;


	         Fortran_Array1D();
	explicit Fortran_Array1D(int n);
	         Fortran_Array1D(int n, const T &a);
	         Fortran_Array1D(int n,  T *a);
    inline   Fortran_Array1D(const Fortran_Array1D &A);
	inline   Fortran_Array1D & operator=(const T &a);
	inline   Fortran_Array1D & operator=(const Fortran_Array1D &A);
	inline   Fortran_Array1D & ref(const Fortran_Array1D &A);
	         Fortran_Array1D copy() const;
		     Fortran_Array1D & inject(const Fortran_Array1D & A);
	inline   T& operator()(int i);
	inline   const T& operator()(int i) const;
	inline 	 int dim1() const;
	inline   int dim() const;
              ~Fortran_Array1D();


	/* ... extended interface ... */

	inline int ref_count() const;
	inline Fortran_Array1D<T> subarray(int i0, int i1);

};




template <class T>
Fortran_Array1D<T>::Fortran_Array1D() : v_(), n_(0), data_(0) {}

template <class T>
Fortran_Array1D<T>::Fortran_Array1D(const Fortran_Array1D<T> &A) : v_(A.v_),  n_(A.n_), 
		data_(A.data_)
{
#ifdef TNT_DEBUG
	std::cout << "Created Fortran_Array1D(const Fortran_Array1D<T> &A) \n";
#endif

}


template <class T>
Fortran_Array1D<T>::Fortran_Array1D(int n) : v_(n), n_(n), data_(v_.begin())
{
#ifdef TNT_DEBUG
	std::cout << "Created Fortran_Array1D(int n) \n";
#endif
}

template <class T>
Fortran_Array1D<T>::Fortran_Array1D(int n, const T &val) : v_(n), n_(n), data_(v_.begin()) 
{
#ifdef TNT_DEBUG
	std::cout << "Created Fortran_Array1D(int n, const T& val) \n";
#endif
	set_(data_, data_+ n, val);

}

template <class T>
Fortran_Array1D<T>::Fortran_Array1D(int n, T *a) : v_(a), n_(n) , data_(v_.begin())
{
#ifdef TNT_DEBUG
	std::cout << "Created Fortran_Array1D(int n, T* a) \n";
#endif
}

template <class T>
inline T& Fortran_Array1D<T>::operator()(int i) 
{ 
#ifdef TNT_BOUNDS_CHECK
	assert(i>= 1);
	assert(i <= n_);
#endif
	return data_[i-1]; 
}

template <class T>
inline const T& Fortran_Array1D<T>::operator()(int i) const 
{ 
#ifdef TNT_BOUNDS_CHECK
	assert(i>= 1);
	assert(i <= n_);
#endif
	return data_[i-1]; 
}


	

template <class T>
Fortran_Array1D<T> & Fortran_Array1D<T>::operator=(const T &a)
{
	set_(data_, data_+n_, a);
	return *this;
}

template <class T>
Fortran_Array1D<T> Fortran_Array1D<T>::copy() const
{
	Fortran_Array1D A( n_);
	copy_(A.data_, data_, n_);

	return A;
}


template <class T>
Fortran_Array1D<T> & Fortran_Array1D<T>::inject(const Fortran_Array1D &A)
{
	if (A.n_ == n_)
		copy_(data_, A.data_, n_);

	return *this;
}





template <class T>
Fortran_Array1D<T> & Fortran_Array1D<T>::ref(const Fortran_Array1D<T> &A)
{
	if (this != &A)
	{
		v_ = A.v_;		/* operator= handles the reference counting. */
		n_ = A.n_;
		data_ = A.data_; 
		
	}
	return *this;
}

template <class T>
Fortran_Array1D<T> & Fortran_Array1D<T>::operator=(const Fortran_Array1D<T> &A)
{
	return ref(A);
}

template <class T>
inline int Fortran_Array1D<T>::dim1() const { return n_; }

template <class T>
inline int Fortran_Array1D<T>::dim() const { return n_; }

template <class T>
Fortran_Array1D<T>::~Fortran_Array1D() {}


/* ............................ exented interface ......................*/

template <class T>
inline int Fortran_Array1D<T>::ref_count() const
{
	return v_.ref_count();
}

template <class T>
inline Fortran_Array1D<T> Fortran_Array1D<T>::subarray(int i0, int i1)
{
#ifdef TNT_DEBUG
		std::cout << "entered subarray. \n";
#endif
	if ((i0 > 0) && (i1 < n_) || (i0 <= i1))
	{
		Fortran_Array1D<T> X(*this);  /* create a new instance of this array. */
		X.n_ = i1-i0+1;
		X.data_ += i0;

		return X;
	}
	else
	{
#ifdef TNT_DEBUG
		std::cout << "subarray:  null return.\n";
#endif
		return Fortran_Array1D<T>();
	}
}


/* private internal functions */


template <class T>
void Fortran_Array1D<T>::set_(T* begin, T* end, const T& a)
{
	for (T* p=begin; p<end; p++)
		*p = a;

}

template <class T>
void Fortran_Array1D<T>::copy_(T* p, const T* q, int len) const
{
	T *end = p + len;
	while (p<end )
		*p++ = *q++;

}


} /* namespace TNT */

#endif
/* TNT_FORTRAN_ARRAY1D_H */

