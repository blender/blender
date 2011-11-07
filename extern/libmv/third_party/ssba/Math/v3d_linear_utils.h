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

#ifndef V3D_LINEAR_UTILS_H
#define V3D_LINEAR_UTILS_H

#include "Math/v3d_linear.h"

#include <iostream>

namespace V3D
{

   template <typename Elem, int Size>
   struct InlineVector : public InlineVectorBase<Elem, Size>
   {
   }; // end struct InlineVector

   template <typename Elem>
   struct Vector : public VectorBase<Elem>
   {
         Vector()
            : VectorBase<Elem>()
         { }

         Vector(unsigned int size)
            : VectorBase<Elem>(size)
         { }

         Vector(unsigned int size, Elem * values)
            : VectorBase<Elem>(size, values)
         { }

         Vector(Vector<Elem> const& a)
            : VectorBase<Elem>(a)
         { }

         Vector<Elem>& operator=(Vector<Elem> const& a)
         {
            (VectorBase<Elem>::operator=)(a);
            return *this;
         }

         Vector<Elem>& operator+=(Vector<Elem> const& rhs)
         {
            addVectorsIP(rhs, *this);
            return *this;
         }

         Vector<Elem>& operator*=(Elem scale)
         {
            scaleVectorsIP(scale, *this);
            return *this;
         }

         Vector<Elem> operator+(Vector<Elem> const& rhs) const
         {
            Vector<Elem> res(this->size());
            addVectors(*this, rhs, res);
            return res;
         }

         Vector<Elem> operator-(Vector<Elem> const& rhs) const
         {
            Vector<Elem> res(this->size());
            subtractVectors(*this, rhs, res);
            return res;
         }

         Elem operator*(Vector<Elem> const& rhs) const
         {
            return innerProduct(*this, rhs);
         }

   }; // end struct Vector

   template <typename Elem, int Rows, int Cols>
   struct InlineMatrix : public InlineMatrixBase<Elem, Rows, Cols>
   {
   }; // end struct InlineMatrix

   template <typename Elem>
   struct Matrix : public MatrixBase<Elem>
   {
         Matrix()
            : MatrixBase<Elem>()
         { }

         Matrix(unsigned int rows, unsigned int cols)
            : MatrixBase<Elem>(rows, cols)
         { }

         Matrix(unsigned int rows, unsigned int cols, Elem * values)
            : MatrixBase<Elem>(rows, cols, values)
         { }

         Matrix(Matrix<Elem> const& a)
            : MatrixBase<Elem>(a)
         { }

         Matrix<Elem>& operator=(Matrix<Elem> const& a)
         {
            (MatrixBase<Elem>::operator=)(a);
            return *this;
         }

         Matrix<Elem>& operator+=(Matrix<Elem> const& rhs)
         {
            addMatricesIP(rhs, *this);
            return *this;
         }

         Matrix<Elem>& operator*=(Elem scale)
         {
            scaleMatrixIP(scale, *this);
            return *this;
         }

         Matrix<Elem> operator+(Matrix<Elem> const& rhs) const
         {
            Matrix<Elem> res(this->num_rows(), this->num_cols());
            addMatrices(*this, rhs, res);
            return res;
         }

         Matrix<Elem> operator-(Matrix<Elem> const& rhs) const
         {
            Matrix<Elem> res(this->num_rows(), this->num_cols());
            subtractMatrices(*this, rhs, res);
            return res;
         }

   }; // end struct Matrix

//----------------------------------------------------------------------

   typedef InlineVector<float, 2>  Vector2f;
   typedef InlineVector<double, 2> Vector2d;
   typedef InlineVector<float, 3>  Vector3f;
   typedef InlineVector<double, 3> Vector3d;
   typedef InlineVector<float, 4>  Vector4f;
   typedef InlineVector<double, 4> Vector4d;

   typedef InlineMatrix<float, 2, 2>  Matrix2x2f;
   typedef InlineMatrix<double, 2, 2> Matrix2x2d;
   typedef InlineMatrix<float, 3, 3>  Matrix3x3f;
   typedef InlineMatrix<double, 3, 3> Matrix3x3d;
   typedef InlineMatrix<float, 4, 4>  Matrix4x4f;
   typedef InlineMatrix<double, 4, 4> Matrix4x4d;

   typedef InlineMatrix<float, 2, 3>  Matrix2x3f;
   typedef InlineMatrix<double, 2, 3> Matrix2x3d;
   typedef InlineMatrix<float, 3, 4>  Matrix3x4f;
   typedef InlineMatrix<double, 3, 4> Matrix3x4d;

   template <typename Elem>
   struct VectorArray
   {
         VectorArray(unsigned count, unsigned size)
            : _count(count), _size(size), _values(0), _vectors(0)
         {
            unsigned const nTotal = _count * _size;
            if (count > 0) _vectors = new Vector<Elem>[count];
            if (nTotal > 0) _values = new Elem[nTotal];
            for (unsigned i = 0; i < _count; ++i) new (&_vectors[i]) Vector<Elem>(_size, _values + i*_size);
         }

         VectorArray(unsigned count, unsigned size, Elem initVal)
            : _count(count), _size(size), _values(0), _vectors(0)
         {
            unsigned const nTotal = _count * _size;
            if (count > 0) _vectors = new Vector<Elem>[count];
            if (nTotal > 0) _values = new Elem[nTotal];
            for (unsigned i = 0; i < _count; ++i) new (&_vectors[i]) Vector<Elem>(_size, _values + i*_size);
            std::fill(_values, _values + nTotal, initVal);
         }

         ~VectorArray()
         {
            delete [] _values;
            delete [] _vectors;
         }

         unsigned count() const { return _count; }
         unsigned size()  const { return _size; }

         //! Get the submatrix at position ix
         Vector<Elem> const& operator[](unsigned ix) const
         {
            return _vectors[ix];
         }

         //! Get the submatrix at position ix
         Vector<Elem>& operator[](unsigned ix)
         {
            return _vectors[ix];
         }

      protected:
         unsigned       _count, _size;
         Elem         * _values;
         Vector<Elem> * _vectors;

      private:
         VectorArray(VectorArray const&);
         void operator=(VectorArray const&);
   };

   template <typename Elem>
   struct MatrixArray
   {
         MatrixArray(unsigned count, unsigned nRows, unsigned nCols)
            : _count(count), _rows(nRows), _columns(nCols), _values(0), _matrices(0)
         {
            unsigned const nTotal = _count * _rows * _columns;
            if (count > 0) _matrices = new Matrix<Elem>[count];
            if (nTotal > 0) _values = new double[nTotal];
            for (unsigned i = 0; i < _count; ++i)
               new (&_matrices[i]) Matrix<Elem>(_rows, _columns, _values + i*(_rows*_columns));
         }

         ~MatrixArray()
         {
            delete [] _matrices;
            delete [] _values;
         }

         //! Get the submatrix at position ix
         Matrix<Elem> const& operator[](unsigned ix) const
         {
            return _matrices[ix];
         }

         //! Get the submatrix at position ix
         Matrix<Elem>& operator[](unsigned ix)
         {
            return _matrices[ix];
         }

         unsigned count()    const { return _count; }
         unsigned num_rows() const { return _rows; }
         unsigned num_cols() const { return _columns; }

      protected:
         unsigned       _count, _rows, _columns;
         double       * _values;
         Matrix<Elem> * _matrices;

      private:
         MatrixArray(MatrixArray const&);
         void operator=(MatrixArray const&);
   };

//----------------------------------------------------------------------

   template <typename Elem, int Size>
   inline InlineVector<Elem, Size>
   operator+(InlineVector<Elem, Size> const& v, InlineVector<Elem, Size> const& w)
   {
      InlineVector<Elem, Size> res;
      addVectors(v, w, res);
      return res;
   }

   template <typename Elem, int Size>
   inline InlineVector<Elem, Size>
   operator-(InlineVector<Elem, Size> const& v, InlineVector<Elem, Size> const& w)
   {
      InlineVector<Elem, Size> res;
      subtractVectors(v, w, res);
      return res;
   }

   template <typename Elem, int Size>
   inline InlineVector<Elem, Size>
   operator*(Elem scale, InlineVector<Elem, Size> const& v)
   {
      InlineVector<Elem, Size> res;
      scaleVector(scale, v, res);
      return res;
   }

   template <typename Elem, int Rows, int Cols>
   inline InlineVector<Elem, Rows>
   operator*(InlineMatrix<Elem, Rows, Cols> const& A, InlineVector<Elem, Cols> const& v)
   {
      InlineVector<Elem, Rows> res;
      multiply_A_v(A, v, res);
      return res;
   }

   template <typename Elem, int RowsA, int ColsA, int ColsB>
   inline InlineMatrix<Elem, RowsA, ColsB>
   operator*(InlineMatrix<Elem, RowsA, ColsA> const& A, InlineMatrix<Elem, ColsA, ColsB> const& B)
   {
      InlineMatrix<Elem, RowsA, ColsB> res;
      multiply_A_B(A, B, res);
      return res;
   }

   template <typename Elem, int Rows, int Cols>
   inline InlineMatrix<Elem, Cols, Rows>
   transposedMatrix(InlineMatrix<Elem, Rows, Cols> const& A)
   {
      InlineMatrix<Elem, Cols, Rows> At;
      makeTransposedMatrix(A, At);
      return At;
   }

   template <typename Elem>
   inline InlineMatrix<Elem, 3, 3>
   invertedMatrix(InlineMatrix<Elem, 3, 3> const& A)
   {
      Elem a = A[0][0], b = A[0][1], c = A[0][2];
      Elem d = A[1][0], e = A[1][1], f = A[1][2];
      Elem g = A[2][0], h = A[2][1], i = A[2][2];

      Elem const det = a*e*i + b*f*g + c*d*h - c*e*g - b*d*i - a*f*h;

      InlineMatrix<Elem, 3, 3> res;
      res[0][0] = e*i-f*h; res[0][1] = c*h-b*i; res[0][2] = b*f-c*e;
      res[1][0] = f*g-d*i; res[1][1] = a*i-c*g; res[1][2] = c*d-a*f;
      res[2][0] = d*h-e*g; res[2][1] = b*g-a*h; res[2][2] = a*e-b*d;

      scaleMatrixIP(1.0/det, res);
      return res;
   }

   template <typename Elem>
   inline InlineVector<Elem, 2>
   makeVector2(Elem a, Elem b)
   {
      InlineVector<Elem, 2> res;
      res[0] = a; res[1] = b;
      return res;
   }

   template <typename Elem>
   inline InlineVector<Elem, 3>
   makeVector3(Elem a, Elem b, Elem c)
   {
      InlineVector<Elem, 3> res;
      res[0] = a; res[1] = b; res[2] = c;
      return res;
   }

   template <typename Vec>
   inline void
   displayVector(Vec const& v)
   {
      using namespace std;

      for (int r = 0; r < v.size(); ++r)
         cout << v[r] << " ";
      cout << endl;
   }

   template <typename Mat>
   inline void
   displayMatrix(Mat const& A)
   {
      using namespace std;

      for (int r = 0; r < A.num_rows(); ++r)
      {
         for (int c = 0; c < A.num_cols(); ++c)
            cout << A[r][c] << " ";
         cout << endl;
      }
   }

} // end namespace V3D

#endif
