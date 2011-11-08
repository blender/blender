// -*- C++ -*-
/*
Copyright (c) 2008 University of North Carolina at Chapel Hill

This file is part of SSBA (Simple Sparse Bundle Adjustment).

SSBA is free software: you can redistribute it and/or modify it under the
terms of the GNU Lesser General Public License as published by the Free
Software Foundation, either version 3 of the License, or (at your option) any
later version.

SSBA is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
details.

You should have received a copy of the GNU Lesser General Public License along
with SSBA. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef V3D_LINEAR_H
#define V3D_LINEAR_H

#include <cassert>
#include <algorithm>
#include <vector>
#include <cmath>

namespace V3D
{
   using namespace std;

   //! Unboxed vector type
   template <typename Elem, int Size>
   struct InlineVectorBase
   {
         typedef Elem value_type;
         typedef Elem element_type;

         typedef Elem const * const_iterator;
         typedef Elem       * iterator;

         static unsigned int size() { return Size; }

         Elem& operator[](unsigned int i)       { return _vec[i]; }
         Elem  operator[](unsigned int i) const { return _vec[i]; }

         Elem& operator()(unsigned int i)       { return _vec[i-1]; }
         Elem  operator()(unsigned int i) const { return _vec[i-1]; }

         const_iterator begin() const { return _vec; }
         iterator       begin()       { return _vec; }
         const_iterator end() const { return _vec + Size; }
         iterator       end()       { return _vec + Size; }

         void newsize(unsigned int sz) const
         {
            assert(sz == Size);
         }

      protected:
         Elem _vec[Size];
   };

   //! Boxed (heap allocated) vector.
   template <typename Elem>
   struct VectorBase
   {
         typedef Elem value_type;
         typedef Elem element_type;

         typedef Elem const * const_iterator;
         typedef Elem       * iterator;

         VectorBase()
            : _size(0), _ownsVec(true), _vec(0)
         { }

         VectorBase(unsigned int size)
            : _size(size), _ownsVec(true), _vec(0)
         {
            if (size > 0) _vec = new Elem[size];
         }

         VectorBase(unsigned int size, Elem * values)
            : _size(size), _ownsVec(false), _vec(values)
         { }

         VectorBase(VectorBase<Elem> const& a)
            : _size(0), _ownsVec(true), _vec(0)
         {
            _size = a._size;
            if (_size == 0) return;
            _vec = new Elem[_size];
            std::copy(a._vec, a._vec + _size, _vec);
         }

         ~VectorBase() { if (_ownsVec && _vec != 0) delete [] _vec; }

         VectorBase& operator=(VectorBase<Elem> const& a)
         {
            if (this == &a) return *this;

            this->newsize(a._size);
            std::copy(a._vec, a._vec + _size, _vec);
            return *this;
         }

         unsigned int size() const { return _size; }

         VectorBase<Elem>& newsize(unsigned int sz)
         {
            if (sz == _size) return *this;
            assert(_ownsVec);

            __destroy();
            _size = sz;
            if (_size > 0) _vec = new Elem[_size];

            return *this;
         }


         Elem& operator[](unsigned int i)       { return _vec[i]; }
         Elem  operator[](unsigned int i) const { return _vec[i]; }

         Elem& operator()(unsigned int i)       { return _vec[i-1]; }
         Elem  operator()(unsigned int i) const { return _vec[i-1]; }

         const_iterator begin() const { return _vec; }
         iterator       begin()       { return _vec; }
         const_iterator end() const { return _vec + _size; }
         iterator       end()       { return _vec + _size; }

      protected:
         void __destroy()
         {
            assert(_ownsVec);

            if (_vec != 0) delete [] _vec;
            _size = 0;
            _vec  = 0;
         }

         unsigned int _size;
         bool         _ownsVec;
         Elem       * _vec;
   };

   template <typename Elem, int Rows, int Cols>
   struct InlineMatrixBase
   {
         typedef Elem         value_type;
         typedef Elem         element_type;

         typedef Elem       * iterator;
         typedef Elem const * const_iterator;

         static unsigned int num_rows() { return Rows; }
         static unsigned int num_cols() { return Cols; }

         Elem       * operator[](unsigned int row)        { return _m[row]; }
         Elem const * operator[](unsigned int row) const  { return _m[row]; }

         Elem& operator()(unsigned int row, unsigned int col)       { return _m[row-1][col-1]; }
         Elem  operator()(unsigned int row, unsigned int col) const { return _m[row-1][col-1]; }

         template <typename Vec>
         void getRowSlice(unsigned int row, unsigned int first, unsigned int last, Vec& dst) const
         {
            for (unsigned int c = first; c < last; ++c) dst[c-first] = _m[row][c];
         }

         template <typename Vec>
         void getColumnSlice(unsigned int first, unsigned int len, unsigned int col, Vec& dst) const
         {
            for (unsigned int r = 0; r < len; ++r) dst[r] = _m[r+first][col];
         }

         void newsize(unsigned int rows, unsigned int cols) const
         {
            assert(rows == Rows && cols == Cols);
         }

         const_iterator begin() const { return &_m[0][0]; }
         iterator       begin()       { return &_m[0][0]; }
         const_iterator end() const { return &_m[0][0] + Rows*Cols; }
         iterator       end()       { return &_m[0][0] + Rows*Cols; }

      protected:
         Elem _m[Rows][Cols];
   };

   template <typename Elem>
   struct MatrixBase
   {
         typedef Elem value_type;
         typedef Elem element_type;

         typedef Elem const * const_iterator;
         typedef Elem       * iterator;

         MatrixBase()
            : _rows(0), _cols(0), _ownsData(true), _m(0)
         { }

         MatrixBase(unsigned int rows, unsigned int cols)
            : _rows(rows), _cols(cols), _ownsData(true), _m(0)
         {
            if (_rows * _cols == 0) return;
            _m = new Elem[rows*cols];
         }

         MatrixBase(unsigned int rows, unsigned int cols, Elem * values)
            : _rows(rows), _cols(cols), _ownsData(false), _m(values)
         { }

         MatrixBase(MatrixBase<Elem> const& a)
            : _ownsData(true), _m(0)
         {
            _rows = a._rows; _cols = a._cols;
            if (_rows * _cols == 0) return;
            _m = new Elem[_rows*_cols];
            std::copy(a._m, a._m+_rows*_cols, _m);
         }

         ~MatrixBase()
         {
            if (_ownsData && _m != 0) delete [] _m;
         }

         MatrixBase& operator=(MatrixBase<Elem> const& a)
         {
            if (this == &a) return *this;

            this->newsize(a.num_rows(), a.num_cols());

            std::copy(a._m, a._m+_rows*_cols, _m);
            return *this;
         }

         void newsize(unsigned int rows, unsigned int cols)
         {
            if (rows == _rows && cols == _cols) return;

            assert(_ownsData);

            __destroy();

            _rows = rows;
            _cols = cols;
            if (_rows * _cols == 0) return;
            _m = new Elem[rows*cols];
         }

         unsigned int num_rows() const { return _rows; }
         unsigned int num_cols() const { return _cols; }

         Elem       * operator[](unsigned int row)        { return _m + row*_cols; }
         Elem const * operator[](unsigned int row) const  { return _m + row*_cols; }

         Elem& operator()(unsigned int row, unsigned int col)       { return _m[(row-1)*_cols + col-1]; }
         Elem  operator()(unsigned int row, unsigned int col) const { return _m[(row-1)*_cols + col-1]; }

         const_iterator begin() const { return _m; }
         iterator       begin()       { return _m; }
         const_iterator end() const { return _m + _rows*_cols; }
         iterator       end()       { return _m + _rows*_cols; }

         template <typename Vec>
         void getRowSlice(unsigned int row, unsigned int first, unsigned int last, Vec& dst) const
         {
            Elem const * v = (*this)[row];
            for (unsigned int c = first; c < last; ++c) dst[c-first] = v[c];
         }

         template <typename Vec>
         void getColumnSlice(unsigned int first, unsigned int len, unsigned int col, Vec& dst) const
         {
            for (unsigned int r = 0; r < len; ++r) dst[r] = _m[r+first][col];
         }

      protected:
         void __destroy()
         {
            assert(_ownsData);
            if (_m != 0) delete [] _m;
            _m = 0;
            _rows = _cols = 0;
         }

         unsigned int   _rows, _cols;
         bool           _ownsData;
         Elem         * _m;
   };

   template <typename T>
   struct CCS_Matrix
   {
         CCS_Matrix()
            : _rows(0), _cols(0)
         { }

         CCS_Matrix(int const rows, int const cols, vector<pair<int, int> > const& nonZeros)
            : _rows(rows), _cols(cols)
         {
            this->initialize(nonZeros);
         }

         CCS_Matrix(CCS_Matrix const& b)
            : _rows(b._rows), _cols(b._cols),
              _colStarts(b._colStarts), _rowIdxs(b._rowIdxs), _destIdxs(b._destIdxs), _values(b._values)
         { }

         CCS_Matrix& operator=(CCS_Matrix const& b)
         {
            if (this == &b) return *this;
            _rows       = b._rows;
            _cols       = b._cols;
            _colStarts  = b._colStarts;
            _rowIdxs    = b._rowIdxs;
            _destIdxs   = b._destIdxs;
            _values     = b._values;
            return *this;
         }

         void create(int const rows, int const cols, vector<pair<int, int> > const& nonZeros)
         {
            _rows = rows;
            _cols = cols;
            this->initialize(nonZeros);
         }

         unsigned int num_rows() const { return _rows; }
         unsigned int num_cols() const { return _cols; }

         int getNonzeroCount() const { return _values.size(); }

         T const * getValues() const { return &_values[0]; }
         T       * getValues()       { return &_values[0]; }

         int const * getDestIndices()  const { return &_destIdxs[0]; }
         int const * getColumnStarts() const { return &_colStarts[0]; }
         int const * getRowIndices()   const { return &_rowIdxs[0]; }

         void getRowRange(unsigned int col, unsigned int& firstRow, unsigned int& lastRow) const
         {
            firstRow = _rowIdxs[_colStarts[col]];
            lastRow  = _rowIdxs[_colStarts[col+1]-1]+1;
         }

         template <typename Vec>
         void getColumnSlice(unsigned int first, unsigned int len, unsigned int col, Vec& dst) const
         {
            unsigned int const last = first + len;

            for (int r = 0; r < len; ++r) dst[r] = 0; // Fill vector with zeros

            int const colStart = _colStarts[col];
            int const colEnd   = _colStarts[col+1];

            int i = colStart;
            int r;
            // Skip rows less than the given start row
            while (i < colEnd && (r = _rowIdxs[i]) < first) ++i;

            // Copy elements until the final row
            while (i < colEnd && (r = _rowIdxs[i]) < last)
            {
               dst[r-first] = _values[i];
               ++i;
            }
         } // end getColumnSlice()

         int getColumnNonzeroCount(unsigned int col) const
         {
            int const colStart = _colStarts[col];
            int const colEnd   = _colStarts[col+1];
            return colEnd - colStart;
         }

         template <typename VecA, typename VecB>
         void getSparseColumn(unsigned int col, VecA& rows, VecB& values) const
         {
            int const colStart = _colStarts[col];
            int const colEnd   = _colStarts[col+1];
            int const nnz      = colEnd - colStart;

            for (int i = 0; i < nnz; ++i)
            {
               rows[i] = _rowIdxs[colStart + i];
               values[i] = _values[colStart + i];
            }
         }

      protected:
         struct NonzeroInfo
         {
               int row, col, serial;

               // Sort wrt the column first
               bool operator<(NonzeroInfo const& rhs) const
               {
                  if (col < rhs.col) return true;
                  if (col > rhs.col) return false;
                  return row < rhs.row;
               }
         };

         void initialize(std::vector<std::pair<int, int> > const& nonZeros)
         {
            using namespace std;

            int const nnz = nonZeros.size();

            _colStarts.resize(_cols + 1);
            _rowIdxs.resize(nnz);

            vector<NonzeroInfo> nz(nnz);
            for (int k = 0; k < nnz; ++k)
            {
               nz[k].row    = nonZeros[k].first;
               nz[k].col    = nonZeros[k].second;
               nz[k].serial = k;
            }

            // Sort in column major order
            std::sort(nz.begin(), nz.end());

            for (size_t k = 0; k < nnz; ++k) _rowIdxs[k] = nz[k].row;

            int curCol = -1;
            for (int k = 0; k < nnz; ++k)
            {
               NonzeroInfo const& el = nz[k];
               if (el.col != curCol)
               {
                  // Update empty cols between
                  for (int c = curCol+1; c < el.col; ++c) _colStarts[c] = k;

                  curCol = el.col;
                  _colStarts[curCol] = k;
               } // end if
            } // end for (k)

            // Update remaining columns
            for (int c = curCol+1; c <= _cols; ++c) _colStarts[c] = nnz;

            _destIdxs.resize(nnz);
            for (int k = 0; k < nnz; ++k) _destIdxs[nz[k].serial] = k;

            _values.resize(nnz);
         } // end initialize()

         int              _rows, _cols;
         std::vector<int> _colStarts;
         std::vector<int> _rowIdxs;
         std::vector<int> _destIdxs;
         std::vector<T>   _values;
   }; // end struct CCS_Matrix

//----------------------------------------------------------------------

   template <typename Vec, typename Elem>
   inline void
   fillVector(Vec& v, Elem val)
   {
      // We do not use std::fill since we rely only on size() and operator[] member functions.
      for (unsigned int i = 0; i < v.size(); ++i) v[i] = val;
   }

   template <typename Vec>
   inline void
   makeZeroVector(Vec& v)
   {
      fillVector(v, 0);
   }

   template <typename VecA, typename VecB>
   inline void
   copyVector(VecA const& src, VecB& dst)
   {
      assert(src.size() == dst.size());
      // We do not use std::fill since we rely only on size() and operator[] member functions.
      for (unsigned int i = 0; i < src.size(); ++i) dst[i] = src[i];
   }

   template <typename VecA, typename VecB>
   inline void
   copyVectorSlice(VecA const& src, unsigned int srcStart, unsigned int srcLen,
                   VecB& dst, unsigned int dstStart)
   {
      unsigned int const end = std::min(srcStart + srcLen, src.size());
      unsigned int const sz = dst.size();
      unsigned int i0, i1;
      for (i0 = srcStart, i1 = dstStart; i0 < end && i1 < sz; ++i0, ++i1) dst[i1] = src[i0];
   }

   template <typename Vec>
   inline typename Vec::value_type
   norm_L1(Vec const& v)
   {
      typename Vec::value_type res(0);
      for (unsigned int i = 0; i < v.size(); ++i) res += fabs(v[i]);
      return res;
   }

   template <typename Vec>
   inline typename Vec::value_type
   norm_Linf(Vec const& v)
   {
      typename Vec::value_type res(0);
      for (unsigned int i = 0; i < v.size(); ++i) res = std::max(res, fabs(v[i]));
      return res;
   }

   template <typename Vec>
   inline typename Vec::value_type
   norm_L2(Vec const& v)
   {
      typename Vec::value_type res(0);
      for (unsigned int i = 0; i < v.size(); ++i) res += v[i]*v[i];
      return sqrt((double)res);
   }

   template <typename Vec>
   inline typename Vec::value_type
   sqrNorm_L2(Vec const& v)
   {
      typename Vec::value_type res(0);
      for (unsigned int i = 0; i < v.size(); ++i) res += v[i]*v[i];
      return res;
   }

   template <typename Vec>
   inline void
   normalizeVector(Vec& v)
   {
      typename Vec::value_type norm(norm_L2(v));
      for (unsigned int i = 0; i < v.size(); ++i) v[i] /= norm;
   }

   template<typename VecA, typename VecB>
   inline typename VecA::value_type
   innerProduct(VecA const& a, VecB const& b)
   {
      assert(a.size() == b.size());
      typename VecA::value_type res(0);
      for (unsigned int i = 0; i < a.size(); ++i) res += a[i] * b[i];
      return res;
   }

   template <typename Elem, typename VecA, typename VecB>
   inline void
   scaleVector(Elem s, VecA const& v, VecB& dst)
   {
      for (unsigned int i = 0; i < v.size(); ++i) dst[i] = s * v[i];
   }

   template <typename Elem, typename Vec>
   inline void
   scaleVectorIP(Elem s, Vec& v)
   {
      typedef typename Vec::value_type Elem2;
      for (unsigned int i = 0; i < v.size(); ++i)
         v[i] = (Elem2)(v[i] * s);
   }

   template <typename VecA, typename VecB, typename VecC>
   inline void
   makeCrossProductVector(VecA const& v, VecB const& w, VecC& dst)
   {
      assert(v.size() == 3);
      assert(w.size() == 3);
      assert(dst.size() == 3);
      dst[0] = v[1]*w[2] - v[2]*w[1];
      dst[1] = v[2]*w[0] - v[0]*w[2];
      dst[2] = v[0]*w[1] - v[1]*w[0];
   }

   template <typename VecA, typename VecB, typename VecC>
   inline void
   addVectors(VecA const& v, VecB const& w, VecC& dst)
   {
      assert(v.size() == w.size());
      assert(v.size() == dst.size());
      for (unsigned int i = 0; i < v.size(); ++i) dst[i] = v[i] + w[i];
   }

   template <typename VecA, typename VecB, typename VecC>
   inline void
   subtractVectors(VecA const& v, VecB const& w, VecC& dst)
   {
      assert(v.size() == w.size());
      assert(v.size() == dst.size());
      for (unsigned int i = 0; i < v.size(); ++i) dst[i] = v[i] - w[i];
   }

   template <typename MatA, typename MatB>
   inline void
   copyMatrix(MatA const& src, MatB& dst)
   {
      unsigned int const rows = src.num_rows();
      unsigned int const cols = src.num_cols();
      assert(dst.num_rows() == rows);
      assert(dst.num_cols() == cols);
      for (unsigned int c = 0; c < cols; ++c)
         for (unsigned int r = 0; r < rows; ++r) dst[r][c] = src[r][c];
   }

   template <typename MatA, typename MatB>
   inline void
   copyMatrixSlice(MatA const& src, unsigned int rowStart, unsigned int colStart, unsigned int rowLen, unsigned int colLen,
                   MatB& dst, unsigned int dstRow, unsigned int dstCol)
   {
      unsigned int const rows = dst.num_rows();
      unsigned int const cols = dst.num_cols();

      unsigned int const rowEnd = std::min(rowStart + rowLen, src.num_rows());
      unsigned int const colEnd = std::min(colStart + colLen, src.num_cols());

      unsigned int c0, c1, r0, r1;

      for (c0 = colStart, c1 = dstCol; c0 < colEnd && c1 < cols; ++c0, ++c1)
         for (r0 = rowStart, r1 = dstRow; r0 < rowEnd && r1 < rows; ++r0, ++r1)
            dst[r1][c1] = src[r0][c0];
   }

   template <typename MatA, typename MatB>
   inline void
   makeTransposedMatrix(MatA const& src, MatB& dst)
   {
      unsigned int const rows = src.num_rows();
      unsigned int const cols = src.num_cols();
      assert(dst.num_cols() == rows);
      assert(dst.num_rows() == cols);
      for (unsigned int c = 0; c < cols; ++c)
         for (unsigned int r = 0; r < rows; ++r) dst[c][r] = src[r][c];
   }

   template <typename Mat>
   inline void
   fillMatrix(Mat& m, typename Mat::value_type val)
   {
      unsigned int const rows = m.num_rows();
      unsigned int const cols = m.num_cols();
      for (unsigned int c = 0; c < cols; ++c)
         for (unsigned int r = 0; r < rows; ++r) m[r][c] = val;
   }

   template <typename Mat>
   inline void
   makeZeroMatrix(Mat& m)
   {
      fillMatrix(m, 0);
   }

   template <typename Mat>
   inline void
   makeIdentityMatrix(Mat& m)
   {
      makeZeroMatrix(m);
      unsigned int const rows = m.num_rows();
      unsigned int const cols = m.num_cols();
      unsigned int n = std::min(rows, cols);
      for (unsigned int i = 0; i < n; ++i)
         m[i][i] = 1;
   }

   template <typename Mat, typename Vec>
   inline void
   makeCrossProductMatrix(Vec const& v, Mat& m)
   {
      assert(v.size() == 3);
      assert(m.num_rows() == 3);
      assert(m.num_cols() == 3);
      m[0][0] = 0;     m[0][1] = -v[2]; m[0][2] = v[1];
      m[1][0] = v[2];  m[1][1] = 0;     m[1][2] = -v[0];
      m[2][0] = -v[1]; m[2][1] = v[0];  m[2][2] = 0;
   }

   template <typename Mat, typename Vec>
   inline void
   makeOuterProductMatrix(Vec const& v, Mat& m)
   {
      assert(m.num_cols() == m.num_rows());
      assert(v.size() == m.num_cols());
      unsigned const sz = v.size();
      for (unsigned r = 0; r < sz; ++r)
         for (unsigned c = 0; c < sz; ++c) m[r][c] = v[r]*v[c];
   }

   template <typename Mat, typename VecA, typename VecB>
   inline void
   makeOuterProductMatrix(VecA const& u, VecB const& v, Mat& m)
   {
      assert(m.num_cols() == m.num_rows());
      assert(u.size() == m.num_cols());
      assert(v.size() == m.num_cols());
      unsigned const sz = u.size();
      for (unsigned r = 0; r < sz; ++r)
         for (unsigned c = 0; c < sz; ++c) m[r][c] = u[r]*v[c];
   }

   template <typename MatA, typename MatB, typename MatC>
   void addMatrices(MatA const& a, MatB const& b, MatC& dst)
   {
      assert(a.num_cols() == b.num_cols());
      assert(a.num_rows() == b.num_rows());
      assert(dst.num_cols() == a.num_cols());
      assert(dst.num_rows() == a.num_rows());

      unsigned int const rows = a.num_rows();
      unsigned int const cols = a.num_cols();

      for (unsigned r = 0; r < rows; ++r)
         for (unsigned c = 0; c < cols; ++c) dst[r][c] = a[r][c] + b[r][c];
   }

   template <typename MatA, typename MatB>
   void addMatricesIP(MatA const& a, MatB& dst)
   {
      assert(dst.num_cols() == a.num_cols());
      assert(dst.num_rows() == a.num_rows());

      unsigned int const rows = a.num_rows();
      unsigned int const cols = a.num_cols();

      for (unsigned r = 0; r < rows; ++r)
         for (unsigned c = 0; c < cols; ++c) dst[r][c] += a[r][c];
   }

   template <typename MatA, typename MatB, typename MatC>
   void subtractMatrices(MatA const& a, MatB const& b, MatC& dst)
   {
      assert(a.num_cols() == b.num_cols());
      assert(a.num_rows() == b.num_rows());
      assert(dst.num_cols() == a.num_cols());
      assert(dst.num_rows() == a.num_rows());

      unsigned int const rows = a.num_rows();
      unsigned int const cols = a.num_cols();

      for (unsigned r = 0; r < rows; ++r)
         for (unsigned c = 0; c < cols; ++c) dst[r][c] = a[r][c] - b[r][c];
   }

   template <typename MatA, typename Elem, typename MatB>
   inline void
   makeScaledMatrix(MatA const& m, Elem scale, MatB& dst)
   {
      unsigned int const rows = m.num_rows();
      unsigned int const cols = m.num_cols();
      for (unsigned int c = 0; c < cols; ++c)
         for (unsigned int r = 0; r < rows; ++r) dst[r][c] = m[r][c] * scale;
   }

   template <typename Mat, typename Elem>
   inline void
   scaleMatrixIP(Elem scale, Mat& m)
   {
      unsigned int const rows = m.num_rows();
      unsigned int const cols = m.num_cols();
      for (unsigned int c = 0; c < cols; ++c)
         for (unsigned int r = 0; r < rows; ++r) m[r][c] *= scale;
   }

   template <typename Mat, typename VecA, typename VecB>
   inline void
   multiply_A_v(Mat const& m, VecA const& in, VecB& dst)
   {
      unsigned int const rows = m.num_rows();
      unsigned int const cols = m.num_cols();
      assert(in.size() == cols);
      assert(dst.size() == rows);

      makeZeroVector(dst);

      for (unsigned int r = 0; r < rows; ++r)
         for (unsigned int c = 0; c < cols; ++c) dst[r] += m[r][c] * in[c];
   }

   template <typename Mat, typename VecA, typename VecB>
   inline void
   multiply_A_v_projective(Mat const& m, VecA const& in, VecB& dst)
   {
      unsigned int const rows = m.num_rows();
      unsigned int const cols = m.num_cols();
      assert(in.size() == cols-1);
      assert(dst.size() == rows-1);

      typename VecB::value_type w = m(rows-1, cols-1);
      unsigned int r, i;
      for (i = 0; i < cols-1; ++i) w += m(rows-1, i) * in[i];
      for (r = 0; r < rows-1; ++r) dst[r] = m(r, cols-1);
      for (r = 0; r < rows-1; ++r)
         for (unsigned int c = 0; c < cols-1; ++c) dst[r] += m[r][c] * in[c];
      for (i = 0; i < rows-1; ++i) dst[i] /= w;
   }

   template <typename Mat, typename VecA, typename VecB>
   inline void
   multiply_A_v_affine(Mat const& m, VecA const& in, VecB& dst)
   {
      unsigned int const rows = m.num_rows();
      unsigned int const cols = m.num_cols();
      assert(in.size() == cols-1);
      assert(dst.size() == rows);

      unsigned int r;

      for (r = 0; r < rows; ++r) dst[r] = m(r, cols-1);
      for (r = 0; r < rows; ++r)
         for (unsigned int c = 0; c < cols-1; ++c) dst[r] += m[r][c] * in[c];
   }

   template <typename Mat, typename VecA, typename VecB>
   inline void
   multiply_At_v(Mat const& m, VecA const& in, VecB& dst)
   {
      unsigned int const rows = m.num_rows();
      unsigned int const cols = m.num_cols();
      assert(in.size() == rows);
      assert(dst.size() == cols);

      makeZeroVector(dst);
      for (unsigned int c = 0; c < cols; ++c)
         for (unsigned int r = 0; r < rows; ++r) dst[c] += m[r][c] * in[r];
   }

   template <typename MatA, typename MatB>
   inline void
   multiply_At_A(MatA const& a, MatB& dst)
   {
      assert(dst.num_rows() == a.num_cols());
      assert(dst.num_cols() == a.num_cols());

      typedef typename MatB::value_type Elem;

      Elem accum;
      for (unsigned int r = 0; r < a.num_cols(); ++r)
         for (unsigned int c = 0; c < a.num_cols(); ++c)
         {
            accum = 0;
            for (unsigned int k = 0; k < a.num_rows(); ++k) accum += a[k][r] * a[k][c];
            dst[r][c] = accum;
         }
   }

   template <typename MatA, typename MatB, typename MatC>
   inline void
   multiply_A_B(MatA const& a, MatB const& b, MatC& dst)
   {
      assert(a.num_cols() == b.num_rows());
      assert(dst.num_rows() == a.num_rows());
      assert(dst.num_cols() == b.num_cols());

      typedef typename MatC::value_type Elem;

      Elem accum;
      for (unsigned int r = 0; r < a.num_rows(); ++r)
         for (unsigned int c = 0; c < b.num_cols(); ++c)
         {
            accum = 0;
            for (unsigned int k = 0; k < a.num_cols(); ++k) accum += a[r][k] * b[k][c];
            dst[r][c] = accum;
         }
   }

   template <typename MatA, typename MatB, typename MatC>
   inline void
   multiply_At_B(MatA const& a, MatB const& b, MatC& dst)
   {
      assert(a.num_rows() == b.num_rows());
      assert(dst.num_rows() == a.num_cols());
      assert(dst.num_cols() == b.num_cols());

      typedef typename MatC::value_type Elem;

      Elem accum;
      for (unsigned int r = 0; r < a.num_cols(); ++r)
         for (unsigned int c = 0; c < b.num_cols(); ++c)
         {
            accum = 0;
            for (unsigned int k = 0; k < a.num_rows(); ++k) accum += a[k][r] * b[k][c];
            dst[r][c] = accum;
         }
   }

   template <typename MatA, typename MatB, typename MatC>
   inline void
   multiply_A_Bt(MatA const& a, MatB const& b, MatC& dst)
   {
      assert(a.num_cols() == b.num_cols());
      assert(dst.num_rows() == a.num_rows());
      assert(dst.num_cols() == b.num_rows());

      typedef typename MatC::value_type Elem;

      Elem accum;
      for (unsigned int r = 0; r < a.num_rows(); ++r)
         for (unsigned int c = 0; c < b.num_rows(); ++c)
         {
            accum = 0;
            for (unsigned int k = 0; k < a.num_cols(); ++k) accum += a[r][k] * b[c][k];
            dst[r][c] = accum;
         }
   }

   template <typename Mat>
   inline void
   transposeMatrixIP(Mat& a)
   {
      assert(a.num_rows() == a.num_cols());

      for (unsigned int r = 0; r < a.num_rows(); ++r)
         for (unsigned int c = 0; c < r; ++c)
            std::swap(a[r][c], a[c][r]);
   }

} // end namespace V3D

#endif
