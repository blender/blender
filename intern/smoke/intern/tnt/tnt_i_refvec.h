/** \file smoke/intern/tnt/tnt_i_refvec.h
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



#ifndef TNT_I_REFVEC_H
#define TNT_I_REFVEC_H

#include <cstdlib>
#include <iostream>

#ifdef TNT_BOUNDS_CHECK
#include <assert.h>
#endif

#ifndef NULL
#define NULL 0
#endif

namespace TNT
{
/*
	Internal representation of ref-counted array.  The TNT
	arrays all use this building block.

	<p>
	If an array block is created by TNT, then every time 
	an assignment is made, the left-hand-side reference 
	is decreased by one, and the right-hand-side refernce
	count is increased by one.  If the array block was
	external to TNT, the refernce count is a NULL pointer
	regardless of how many references are made, since the 
	memory is not freed by TNT.


	
*/
template <class T>
class i_refvec
{


  private:
    T* data_;                  
    int *ref_count_;


  public:

			 i_refvec();
	explicit i_refvec(int n);
	inline	 i_refvec(T* data);
	inline	 i_refvec(const i_refvec &v);
	inline   T*		 begin();
	inline const T* begin() const;
	inline  T& operator[](int i);
	inline const T& operator[](int i) const;
	inline  i_refvec<T> & operator=(const i_refvec<T> &V);
		    void copy_(T* p, const T* q, const T* e); 
		    void set_(T* p, const T* b, const T* e); 
	inline 	int	 ref_count() const;
	inline  int is_null() const;
	inline  void destroy();
			 ~i_refvec();
			
};

template <class T>
void i_refvec<T>::copy_(T* p, const T* q, const T* e)
{
	for (T* t=p; q<e; t++, q++)
		*t= *q;
}

template <class T>
i_refvec<T>::i_refvec() : data_(NULL), ref_count_(NULL) {}

/**
	In case n is 0 or negative, it does NOT call new. 
*/
template <class T>
i_refvec<T>::i_refvec(int n) : data_(NULL), ref_count_(NULL)
{
	if (n >= 1)
	{
#ifdef TNT_DEBUG
		std::cout  << "new data storage.\n";
#endif
		data_ = new T[n];
		ref_count_ = new int;
		*ref_count_ = 1;
	}
}

template <class T>
inline	 i_refvec<T>::i_refvec(const i_refvec<T> &V): data_(V.data_),
	ref_count_(V.ref_count_)
{
	if (V.ref_count_ != NULL)
	    (*(V.ref_count_))++;
}


template <class T>
i_refvec<T>::i_refvec(T* data) : data_(data), ref_count_(NULL) {}

template <class T>
inline T* i_refvec<T>::begin()
{
	return data_;
}

template <class T>
inline const T& i_refvec<T>::operator[](int i) const
{
	return data_[i];
}

template <class T>
inline T& i_refvec<T>::operator[](int i)
{
	return data_[i];
}


template <class T>
inline const T* i_refvec<T>::begin() const
{
	return data_;
}



template <class T>
i_refvec<T> & i_refvec<T>::operator=(const i_refvec<T> &V)
{
	if (this == &V)
		return *this;


	if (ref_count_ != NULL)
	{
		(*ref_count_) --;
		if ((*ref_count_) == 0)
			destroy();
	}

	data_ = V.data_;
	ref_count_ = V.ref_count_;

	if (V.ref_count_ != NULL)
	    (*(V.ref_count_))++;

	return *this;
}

template <class T>
void i_refvec<T>::destroy()
{
	if (ref_count_ != NULL)
	{
#ifdef TNT_DEBUG
		std::cout << "destorying data... \n";
#endif
		delete ref_count_;

#ifdef TNT_DEBUG
		std::cout << "deleted ref_count_ ...\n";
#endif
		if (data_ != NULL)
			delete []data_;
#ifdef TNT_DEBUG
		std::cout << "deleted data_[] ...\n";
#endif
		data_ = NULL;
	}
}

/*
* return 1 is vector is empty, 0 otherwise
*
* if is_null() is false and ref_count() is 0, then
* 
*/
template<class T>
int i_refvec<T>::is_null() const
{
	return (data_ == NULL ? 1 : 0);
}

/*
*  returns -1 if data is external, 
*  returns 0 if a is NULL array,
*  otherwise returns the positive number of vectors sharing
*  		this data space.
*/
template <class T>
int i_refvec<T>::ref_count() const
{
	if (data_ == NULL)
		return 0;
	else
		return (ref_count_ != NULL ? *ref_count_ : -1) ; 
}

template <class T>
i_refvec<T>::~i_refvec()
{
	if (ref_count_ != NULL)
	{
		(*ref_count_)--;

		if (*ref_count_ == 0)
		destroy();
	}
}


} /* namespace TNT */





#endif
/* TNT_I_REFVEC_H */

