// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2008 Gael Guennebaud <g.gael@free.fr>
// Copyright (C) 2011 Benoit Jacob <jacob.benoit.1@gmail.com>
//
// Eigen is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 3 of the License, or (at your option) any later version.
//
// Alternatively, you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of
// the License, or (at your option) any later version.
//
// Eigen is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License or the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License and a copy of the GNU General Public License along with
// Eigen. If not, see <http://www.gnu.org/licenses/>.

#ifndef EIGEN2_QR_H
#define EIGEN2_QR_H

template<typename MatrixType>
class QR : public HouseholderQR<MatrixType>
{
  public:

    typedef HouseholderQR<MatrixType> Base;
    typedef Block<const MatrixType, MatrixType::ColsAtCompileTime, MatrixType::ColsAtCompileTime> MatrixRBlockType;

    QR() : Base() {}

    template<typename T>
    explicit QR(const T& t) : Base(t) {}

    template<typename OtherDerived, typename ResultType>
    bool solve(const MatrixBase<OtherDerived>& b, ResultType *result) const
    {
      *result = static_cast<const Base*>(this)->solve(b);
      return true;
    }

    MatrixType matrixQ(void) const {
      MatrixType ret = MatrixType::Identity(this->rows(), this->cols());
      ret = this->householderQ() * ret;
      return ret;
    }

    bool isFullRank() const {
      return true;
    }
    
    const TriangularView<MatrixRBlockType, UpperTriangular>
    matrixR(void) const
    {
      int cols = this->cols();
      return MatrixRBlockType(this->matrixQR(), 0, 0, cols, cols).template triangularView<UpperTriangular>();
    }
};

/** \return the QR decomposition of \c *this.
  *
  * \sa class QR
  */
template<typename Derived>
const QR<typename MatrixBase<Derived>::PlainObject>
MatrixBase<Derived>::qr() const
{
  return QR<PlainObject>(eval());
}


#endif // EIGEN2_QR_H
