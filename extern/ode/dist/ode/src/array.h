/*************************************************************************
 *                                                                       *
 * Open Dynamics Engine, Copyright (C) 2001,2002 Russell L. Smith.       *
 * All rights reserved.  Email: russ@q12.org   Web: www.q12.org          *
 *                                                                       *
 * This library is free software; you can redistribute it and/or         *
 * modify it under the terms of EITHER:                                  *
 *   (1) The GNU Lesser General Public License as published by the Free  *
 *       Software Foundation; either version 2.1 of the License, or (at  *
 *       your option) any later version. The text of the GNU Lesser      *
 *       General Public License is included with this library in the     *
 *       file LICENSE.TXT.                                               *
 *   (2) The BSD-style license that is included with this library in     *
 *       the file LICENSE-BSD.TXT.                                       *
 *                                                                       *
 * This library is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the files    *
 * LICENSE.TXT and LICENSE-BSD.TXT for more details.                     *
 *                                                                       *
 *************************************************************************/

/* this comes from the `reuse' library. copy any changes back to the source.
 *
 * Variable sized array template. The array is always stored in a contiguous
 * chunk. The array can be resized. A size increase will cause more memory
 * to be allocated, and may result in relocation of the array memory.
 * A size decrease has no effect on the memory allocation.
 *
 * Array elements with constructors or destructors are not supported!
 * But if you must have such elements, here's what to know/do:
 *   - Bitwise copy is used when copying whole arrays.
 *   - When copying individual items (via push(), insert() etc) the `='
 *     (equals) operator is used. Thus you should define this operator to do
 *     a bitwise copy. You should probably also define the copy constructor.
 */


#ifndef _ODE_ARRAY_H_
#define _ODE_ARRAY_H_

#include <ode/config.h>


// this base class has no constructors or destructor, for your convenience.

class dArrayBase {
protected:
  int _size;		// number of elements in `data'
  int _anum;		// allocated number of elements in `data'
  void *_data;		// array data

  void _freeAll (int sizeofT);
  void _setSize (int newsize, int sizeofT);
  // set the array size to `newsize', allocating more memory if necessary.
  // if newsize>_anum and is a power of two then this is guaranteed to
  // set _size and _anum to newsize.

public:
  // not: dArrayBase () { _size=0; _anum=0; _data=0; }

  int size() const { return _size; }
  int allocatedSize() const { return _anum; }
  void * operator new (size_t size);
  void operator delete (void *ptr, size_t size);

  void constructor() { _size=0; _anum=0; _data=0; }
  // if this structure is allocated with malloc() instead of new, you can
  // call this to set it up.

  void constructLocalArray (int __anum);
  // this helper function allows non-reallocating arrays to be constructed
  // on the stack (or in the heap if necessary). this is something of a
  // kludge and should be used with extreme care. this function acts like
  // a constructor - it is called on uninitialized memory that will hold the
  // Array structure and the data. __anum is the number of elements that
  // are allocated. the memory MUST be allocated with size:
  //   sizeof(ArrayBase) + __anum*sizeof(T)
  // arrays allocated this way will never try to reallocate or free the
  // memory - that's your job.
};


template <class T> class dArray : public dArrayBase {
public:
  void equals (const dArray<T> &x) {
    setSize (x.size());
    memcpy (_data,x._data,x._size * sizeof(T));
  }

  dArray () { constructor(); }
  dArray (const dArray<T> &x) { constructor(); equals (x); }
  ~dArray () { _freeAll(sizeof(T)); }
  void setSize (int newsize) { _setSize (newsize,sizeof(T)); }
  T *data() const { return (T*) _data; }
  T & operator[] (int i) const { return ((T*)_data)[i]; }
  void operator = (const dArray<T> &x) { equals (x); }

  void push (const T item) {
    if (_size < _anum) _size++; else _setSize (_size+1,sizeof(T));
    ((T*)_data)[_size-1] = item;
  }

  void swap (dArray<T> &x) {
    int tmp1;
    void *tmp2;
    tmp1=_size; _size=x._size; x._size=tmp1;
    tmp1=_anum; _anum=x._anum; x._anum=tmp1;
    tmp2=_data; _data=x._data; x._data=tmp2;
  }

  // insert the item at the position `i'. if i<0 then add the item to the
  // start, if i >= size then add the item to the end of the array.
  void insert (int i, const T item) {
    if (_size < _anum) _size++; else _setSize (_size+1,sizeof(T));
    if (i >= (_size-1)) i = _size-1;	// add to end
    else {
      if (i < 0) i=0;			// add to start
      int n = _size-1-i;
      if (n>0) memmove (((T*)_data) + i+1, ((T*)_data) + i, n*sizeof(T));
    }
    ((T*)_data)[i] = item;
  }

  void remove (int i) {
    if (i >= 0 && i < _size) {	// passing this test guarantees size>0
      int n = _size-1-i;
      if (n>0) memmove (((T*)_data) + i, ((T*)_data) + i+1, n*sizeof(T));
      _size--;
    }
  }
};


#endif
