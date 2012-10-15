// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2008 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2006-2009 Benoit Jacob <jacob.benoit.1@gmail.com>
// Copyright (C) 2010 Hauke Heibel <hauke.heibel@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef EIGEN_MATRIXSTORAGE_H
#define EIGEN_MATRIXSTORAGE_H

#ifdef EIGEN_DENSE_STORAGE_CTOR_PLUGIN
  #define EIGEN_INTERNAL_DENSE_STORAGE_CTOR_PLUGIN EIGEN_DENSE_STORAGE_CTOR_PLUGIN;
#else
  #define EIGEN_INTERNAL_DENSE_STORAGE_CTOR_PLUGIN
#endif

namespace Eigen {

namespace internal {

struct constructor_without_unaligned_array_assert {};

/** \internal
  * Static array. If the MatrixOrArrayOptions require auto-alignment, the array will be automatically aligned:
  * to 16 bytes boundary if the total size is a multiple of 16 bytes.
  */
template <typename T, int Size, int MatrixOrArrayOptions,
          int Alignment = (MatrixOrArrayOptions&DontAlign) ? 0
                        : (((Size*sizeof(T))%16)==0) ? 16
                        : 0 >
struct plain_array
{
  T array[Size];
  plain_array() {}
  plain_array(constructor_without_unaligned_array_assert) {}
};

#ifdef EIGEN_DISABLE_UNALIGNED_ARRAY_ASSERT
  #define EIGEN_MAKE_UNALIGNED_ARRAY_ASSERT(sizemask)
#else
  #define EIGEN_MAKE_UNALIGNED_ARRAY_ASSERT(sizemask) \
    eigen_assert((reinterpret_cast<size_t>(array) & sizemask) == 0 \
              && "this assertion is explained here: " \
              "http://eigen.tuxfamily.org/dox-devel/TopicUnalignedArrayAssert.html" \
              " **** READ THIS WEB PAGE !!! ****");
#endif

template <typename T, int Size, int MatrixOrArrayOptions>
struct plain_array<T, Size, MatrixOrArrayOptions, 16>
{
  EIGEN_USER_ALIGN16 T array[Size];
  plain_array() { EIGEN_MAKE_UNALIGNED_ARRAY_ASSERT(0xf) }
  plain_array(constructor_without_unaligned_array_assert) {}
};

template <typename T, int MatrixOrArrayOptions, int Alignment>
struct plain_array<T, 0, MatrixOrArrayOptions, Alignment>
{
  EIGEN_USER_ALIGN16 T array[1];
  plain_array() {}
  plain_array(constructor_without_unaligned_array_assert) {}
};

} // end namespace internal

/** \internal
  *
  * \class DenseStorage
  * \ingroup Core_Module
  *
  * \brief Stores the data of a matrix
  *
  * This class stores the data of fixed-size, dynamic-size or mixed matrices
  * in a way as compact as possible.
  *
  * \sa Matrix
  */
template<typename T, int Size, int _Rows, int _Cols, int _Options> class DenseStorage;

// purely fixed-size matrix
template<typename T, int Size, int _Rows, int _Cols, int _Options> class DenseStorage
{
    internal::plain_array<T,Size,_Options> m_data;
  public:
    inline explicit DenseStorage() {}
    inline DenseStorage(internal::constructor_without_unaligned_array_assert)
      : m_data(internal::constructor_without_unaligned_array_assert()) {}
    inline DenseStorage(DenseIndex,DenseIndex,DenseIndex) {}
    inline void swap(DenseStorage& other) { std::swap(m_data,other.m_data); }
    static inline DenseIndex rows(void) {return _Rows;}
    static inline DenseIndex cols(void) {return _Cols;}
    inline void conservativeResize(DenseIndex,DenseIndex,DenseIndex) {}
    inline void resize(DenseIndex,DenseIndex,DenseIndex) {}
    inline const T *data() const { return m_data.array; }
    inline T *data() { return m_data.array; }
};

// null matrix
template<typename T, int _Rows, int _Cols, int _Options> class DenseStorage<T, 0, _Rows, _Cols, _Options>
{
  public:
    inline explicit DenseStorage() {}
    inline DenseStorage(internal::constructor_without_unaligned_array_assert) {}
    inline DenseStorage(DenseIndex,DenseIndex,DenseIndex) {}
    inline void swap(DenseStorage& ) {}
    static inline DenseIndex rows(void) {return _Rows;}
    static inline DenseIndex cols(void) {return _Cols;}
    inline void conservativeResize(DenseIndex,DenseIndex,DenseIndex) {}
    inline void resize(DenseIndex,DenseIndex,DenseIndex) {}
    inline const T *data() const { return 0; }
    inline T *data() { return 0; }
};

// more specializations for null matrices; these are necessary to resolve ambiguities
template<typename T, int _Options> class DenseStorage<T, 0, Dynamic, Dynamic, _Options>
: public DenseStorage<T, 0, 0, 0, _Options> { };

template<typename T, int _Rows, int _Options> class DenseStorage<T, 0, _Rows, Dynamic, _Options>
: public DenseStorage<T, 0, 0, 0, _Options> { };

template<typename T, int _Cols, int _Options> class DenseStorage<T, 0, Dynamic, _Cols, _Options>
: public DenseStorage<T, 0, 0, 0, _Options> { };

// dynamic-size matrix with fixed-size storage
template<typename T, int Size, int _Options> class DenseStorage<T, Size, Dynamic, Dynamic, _Options>
{
    internal::plain_array<T,Size,_Options> m_data;
    DenseIndex m_rows;
    DenseIndex m_cols;
  public:
    inline explicit DenseStorage() : m_rows(0), m_cols(0) {}
    inline DenseStorage(internal::constructor_without_unaligned_array_assert)
      : m_data(internal::constructor_without_unaligned_array_assert()), m_rows(0), m_cols(0) {}
    inline DenseStorage(DenseIndex, DenseIndex rows, DenseIndex cols) : m_rows(rows), m_cols(cols) {}
    inline void swap(DenseStorage& other)
    { std::swap(m_data,other.m_data); std::swap(m_rows,other.m_rows); std::swap(m_cols,other.m_cols); }
    inline DenseIndex rows(void) const {return m_rows;}
    inline DenseIndex cols(void) const {return m_cols;}
    inline void conservativeResize(DenseIndex, DenseIndex rows, DenseIndex cols) { m_rows = rows; m_cols = cols; }
    inline void resize(DenseIndex, DenseIndex rows, DenseIndex cols) { m_rows = rows; m_cols = cols; }
    inline const T *data() const { return m_data.array; }
    inline T *data() { return m_data.array; }
};

// dynamic-size matrix with fixed-size storage and fixed width
template<typename T, int Size, int _Cols, int _Options> class DenseStorage<T, Size, Dynamic, _Cols, _Options>
{
    internal::plain_array<T,Size,_Options> m_data;
    DenseIndex m_rows;
  public:
    inline explicit DenseStorage() : m_rows(0) {}
    inline DenseStorage(internal::constructor_without_unaligned_array_assert)
      : m_data(internal::constructor_without_unaligned_array_assert()), m_rows(0) {}
    inline DenseStorage(DenseIndex, DenseIndex rows, DenseIndex) : m_rows(rows) {}
    inline void swap(DenseStorage& other) { std::swap(m_data,other.m_data); std::swap(m_rows,other.m_rows); }
    inline DenseIndex rows(void) const {return m_rows;}
    inline DenseIndex cols(void) const {return _Cols;}
    inline void conservativeResize(DenseIndex, DenseIndex rows, DenseIndex) { m_rows = rows; }
    inline void resize(DenseIndex, DenseIndex rows, DenseIndex) { m_rows = rows; }
    inline const T *data() const { return m_data.array; }
    inline T *data() { return m_data.array; }
};

// dynamic-size matrix with fixed-size storage and fixed height
template<typename T, int Size, int _Rows, int _Options> class DenseStorage<T, Size, _Rows, Dynamic, _Options>
{
    internal::plain_array<T,Size,_Options> m_data;
    DenseIndex m_cols;
  public:
    inline explicit DenseStorage() : m_cols(0) {}
    inline DenseStorage(internal::constructor_without_unaligned_array_assert)
      : m_data(internal::constructor_without_unaligned_array_assert()), m_cols(0) {}
    inline DenseStorage(DenseIndex, DenseIndex, DenseIndex cols) : m_cols(cols) {}
    inline void swap(DenseStorage& other) { std::swap(m_data,other.m_data); std::swap(m_cols,other.m_cols); }
    inline DenseIndex rows(void) const {return _Rows;}
    inline DenseIndex cols(void) const {return m_cols;}
    inline void conservativeResize(DenseIndex, DenseIndex, DenseIndex cols) { m_cols = cols; }
    inline void resize(DenseIndex, DenseIndex, DenseIndex cols) { m_cols = cols; }
    inline const T *data() const { return m_data.array; }
    inline T *data() { return m_data.array; }
};

// purely dynamic matrix.
template<typename T, int _Options> class DenseStorage<T, Dynamic, Dynamic, Dynamic, _Options>
{
    T *m_data;
    DenseIndex m_rows;
    DenseIndex m_cols;
  public:
    inline explicit DenseStorage() : m_data(0), m_rows(0), m_cols(0) {}
    inline DenseStorage(internal::constructor_without_unaligned_array_assert)
       : m_data(0), m_rows(0), m_cols(0) {}
    inline DenseStorage(DenseIndex size, DenseIndex rows, DenseIndex cols)
      : m_data(internal::conditional_aligned_new_auto<T,(_Options&DontAlign)==0>(size)), m_rows(rows), m_cols(cols) 
    { EIGEN_INTERNAL_DENSE_STORAGE_CTOR_PLUGIN }
    inline ~DenseStorage() { internal::conditional_aligned_delete_auto<T,(_Options&DontAlign)==0>(m_data, m_rows*m_cols); }
    inline void swap(DenseStorage& other)
    { std::swap(m_data,other.m_data); std::swap(m_rows,other.m_rows); std::swap(m_cols,other.m_cols); }
    inline DenseIndex rows(void) const {return m_rows;}
    inline DenseIndex cols(void) const {return m_cols;}
    inline void conservativeResize(DenseIndex size, DenseIndex rows, DenseIndex cols)
    {
      m_data = internal::conditional_aligned_realloc_new_auto<T,(_Options&DontAlign)==0>(m_data, size, m_rows*m_cols);
      m_rows = rows;
      m_cols = cols;
    }
    void resize(DenseIndex size, DenseIndex rows, DenseIndex cols)
    {
      if(size != m_rows*m_cols)
      {
        internal::conditional_aligned_delete_auto<T,(_Options&DontAlign)==0>(m_data, m_rows*m_cols);
        if (size)
          m_data = internal::conditional_aligned_new_auto<T,(_Options&DontAlign)==0>(size);
        else
          m_data = 0;
        EIGEN_INTERNAL_DENSE_STORAGE_CTOR_PLUGIN
      }
      m_rows = rows;
      m_cols = cols;
    }
    inline const T *data() const { return m_data; }
    inline T *data() { return m_data; }
};

// matrix with dynamic width and fixed height (so that matrix has dynamic size).
template<typename T, int _Rows, int _Options> class DenseStorage<T, Dynamic, _Rows, Dynamic, _Options>
{
    T *m_data;
    DenseIndex m_cols;
  public:
    inline explicit DenseStorage() : m_data(0), m_cols(0) {}
    inline DenseStorage(internal::constructor_without_unaligned_array_assert) : m_data(0), m_cols(0) {}
    inline DenseStorage(DenseIndex size, DenseIndex, DenseIndex cols) : m_data(internal::conditional_aligned_new_auto<T,(_Options&DontAlign)==0>(size)), m_cols(cols)
    { EIGEN_INTERNAL_DENSE_STORAGE_CTOR_PLUGIN }
    inline ~DenseStorage() { internal::conditional_aligned_delete_auto<T,(_Options&DontAlign)==0>(m_data, _Rows*m_cols); }
    inline void swap(DenseStorage& other) { std::swap(m_data,other.m_data); std::swap(m_cols,other.m_cols); }
    static inline DenseIndex rows(void) {return _Rows;}
    inline DenseIndex cols(void) const {return m_cols;}
    inline void conservativeResize(DenseIndex size, DenseIndex, DenseIndex cols)
    {
      m_data = internal::conditional_aligned_realloc_new_auto<T,(_Options&DontAlign)==0>(m_data, size, _Rows*m_cols);
      m_cols = cols;
    }
    EIGEN_STRONG_INLINE void resize(DenseIndex size, DenseIndex, DenseIndex cols)
    {
      if(size != _Rows*m_cols)
      {
        internal::conditional_aligned_delete_auto<T,(_Options&DontAlign)==0>(m_data, _Rows*m_cols);
        if (size)
          m_data = internal::conditional_aligned_new_auto<T,(_Options&DontAlign)==0>(size);
        else
          m_data = 0;
        EIGEN_INTERNAL_DENSE_STORAGE_CTOR_PLUGIN
      }
      m_cols = cols;
    }
    inline const T *data() const { return m_data; }
    inline T *data() { return m_data; }
};

// matrix with dynamic height and fixed width (so that matrix has dynamic size).
template<typename T, int _Cols, int _Options> class DenseStorage<T, Dynamic, Dynamic, _Cols, _Options>
{
    T *m_data;
    DenseIndex m_rows;
  public:
    inline explicit DenseStorage() : m_data(0), m_rows(0) {}
    inline DenseStorage(internal::constructor_without_unaligned_array_assert) : m_data(0), m_rows(0) {}
    inline DenseStorage(DenseIndex size, DenseIndex rows, DenseIndex) : m_data(internal::conditional_aligned_new_auto<T,(_Options&DontAlign)==0>(size)), m_rows(rows)
    { EIGEN_INTERNAL_DENSE_STORAGE_CTOR_PLUGIN }
    inline ~DenseStorage() { internal::conditional_aligned_delete_auto<T,(_Options&DontAlign)==0>(m_data, _Cols*m_rows); }
    inline void swap(DenseStorage& other) { std::swap(m_data,other.m_data); std::swap(m_rows,other.m_rows); }
    inline DenseIndex rows(void) const {return m_rows;}
    static inline DenseIndex cols(void) {return _Cols;}
    inline void conservativeResize(DenseIndex size, DenseIndex rows, DenseIndex)
    {
      m_data = internal::conditional_aligned_realloc_new_auto<T,(_Options&DontAlign)==0>(m_data, size, m_rows*_Cols);
      m_rows = rows;
    }
    EIGEN_STRONG_INLINE void resize(DenseIndex size, DenseIndex rows, DenseIndex)
    {
      if(size != m_rows*_Cols)
      {
        internal::conditional_aligned_delete_auto<T,(_Options&DontAlign)==0>(m_data, _Cols*m_rows);
        if (size)
          m_data = internal::conditional_aligned_new_auto<T,(_Options&DontAlign)==0>(size);
        else
          m_data = 0;
        EIGEN_INTERNAL_DENSE_STORAGE_CTOR_PLUGIN
      }
      m_rows = rows;
    }
    inline const T *data() const { return m_data; }
    inline T *data() { return m_data; }
};

} // end namespace Eigen

#endif // EIGEN_MATRIX_H
