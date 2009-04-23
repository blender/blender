// This file is part of Eigen, a lightweight C++ template library
// for linear algebra. Eigen itself is part of the KDE project.
//
// Copyright (C) 2008 Gael Guennebaud <g.gael@free.fr>
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

#ifndef EIGEN_UMFPACKSUPPORT_H
#define EIGEN_UMFPACKSUPPORT_H

/* TODO extract L, extract U, compute det, etc... */

// generic double/complex<double> wrapper functions:

inline void umfpack_free_numeric(void **Numeric, double)
{ umfpack_di_free_numeric(Numeric); }

inline void umfpack_free_numeric(void **Numeric, std::complex<double>)
{ umfpack_zi_free_numeric(Numeric); }

inline void umfpack_free_symbolic(void **Symbolic, double)
{ umfpack_di_free_symbolic(Symbolic); }

inline void umfpack_free_symbolic(void **Symbolic, std::complex<double>)
{ umfpack_zi_free_symbolic(Symbolic); }

inline int umfpack_symbolic(int n_row,int n_col,
                            const int Ap[], const int Ai[], const double Ax[], void **Symbolic,
                            const double Control [UMFPACK_CONTROL], double Info [UMFPACK_INFO])
{
  return umfpack_di_symbolic(n_row,n_col,Ap,Ai,Ax,Symbolic,Control,Info);
}

inline int umfpack_symbolic(int n_row,int n_col,
                            const int Ap[], const int Ai[], const std::complex<double> Ax[], void **Symbolic,
                            const double Control [UMFPACK_CONTROL], double Info [UMFPACK_INFO])
{
  return umfpack_zi_symbolic(n_row,n_col,Ap,Ai,&Ax[0].real(),0,Symbolic,Control,Info);
}

inline int umfpack_numeric( const int Ap[], const int Ai[], const double Ax[],
                            void *Symbolic, void **Numeric,
                            const double Control[UMFPACK_CONTROL],double Info [UMFPACK_INFO])
{
  return umfpack_di_numeric(Ap,Ai,Ax,Symbolic,Numeric,Control,Info);
}

inline int umfpack_numeric( const int Ap[], const int Ai[], const std::complex<double> Ax[],
                            void *Symbolic, void **Numeric,
                            const double Control[UMFPACK_CONTROL],double Info [UMFPACK_INFO])
{
  return umfpack_zi_numeric(Ap,Ai,&Ax[0].real(),0,Symbolic,Numeric,Control,Info);
}

inline int umfpack_solve( int sys, const int Ap[], const int Ai[], const double Ax[],
                          double X[], const double B[], void *Numeric,
                          const double Control[UMFPACK_CONTROL], double Info[UMFPACK_INFO])
{
  return umfpack_di_solve(sys,Ap,Ai,Ax,X,B,Numeric,Control,Info);
}

inline int umfpack_solve( int sys, const int Ap[], const int Ai[], const std::complex<double> Ax[],
                          std::complex<double> X[], const std::complex<double> B[], void *Numeric,
                          const double Control[UMFPACK_CONTROL], double Info[UMFPACK_INFO])
{
  return umfpack_zi_solve(sys,Ap,Ai,&Ax[0].real(),0,&X[0].real(),0,&B[0].real(),0,Numeric,Control,Info);
}

inline int umfpack_get_lunz(int *lnz, int *unz, int *n_row, int *n_col, int *nz_udiag, void *Numeric, double)
{
  return umfpack_di_get_lunz(lnz,unz,n_row,n_col,nz_udiag,Numeric);
}

inline int umfpack_get_lunz(int *lnz, int *unz, int *n_row, int *n_col, int *nz_udiag, void *Numeric, std::complex<double>)
{
  return umfpack_zi_get_lunz(lnz,unz,n_row,n_col,nz_udiag,Numeric);
}

inline int umfpack_get_numeric(int Lp[], int Lj[], double Lx[], int Up[], int Ui[], double Ux[],
                               int P[], int Q[], double Dx[], int *do_recip, double Rs[], void *Numeric)
{
  return umfpack_di_get_numeric(Lp,Lj,Lx,Up,Ui,Ux,P,Q,Dx,do_recip,Rs,Numeric);
}

inline int umfpack_get_numeric(int Lp[], int Lj[], std::complex<double> Lx[], int Up[], int Ui[], std::complex<double> Ux[],
                               int P[], int Q[], std::complex<double> Dx[], int *do_recip, double Rs[], void *Numeric)
{
  return umfpack_zi_get_numeric(Lp,Lj,Lx?&Lx[0].real():0,0,Up,Ui,Ux?&Ux[0].real():0,0,P,Q,
                               Dx?&Dx[0].real():0,0,do_recip,Rs,Numeric);
}

inline int umfpack_get_determinant(double *Mx, double *Ex, void *NumericHandle, double User_Info [UMFPACK_INFO])
{
  return umfpack_di_get_determinant(Mx,Ex,NumericHandle,User_Info);
}

inline int umfpack_get_determinant(std::complex<double> *Mx, double *Ex, void *NumericHandle, double User_Info [UMFPACK_INFO])
{
  return umfpack_zi_get_determinant(&Mx->real(),0,Ex,NumericHandle,User_Info);
}


template<typename MatrixType>
class SparseLU<MatrixType,UmfPack> : public SparseLU<MatrixType>
{
  protected:
    typedef SparseLU<MatrixType> Base;
    typedef typename Base::Scalar Scalar;
    typedef typename Base::RealScalar RealScalar;
    typedef Matrix<Scalar,Dynamic,1> Vector;
    typedef Matrix<int, 1, MatrixType::ColsAtCompileTime> IntRowVectorType;
    typedef Matrix<int, MatrixType::RowsAtCompileTime, 1> IntColVectorType;
    typedef SparseMatrix<Scalar,LowerTriangular|UnitDiagBit> LMatrixType;
    typedef SparseMatrix<Scalar,UpperTriangular> UMatrixType;
    using Base::m_flags;
    using Base::m_status;

  public:

    SparseLU(int flags = NaturalOrdering)
      : Base(flags), m_numeric(0)
    {
    }

    SparseLU(const MatrixType& matrix, int flags = NaturalOrdering)
      : Base(flags), m_numeric(0)
    {
      compute(matrix);
    }

    ~SparseLU()
    {
      if (m_numeric)
        umfpack_free_numeric(&m_numeric,Scalar());
    }

    inline const LMatrixType& matrixL() const
    {
      if (m_extractedDataAreDirty) extractData();
      return m_l;
    }

    inline const UMatrixType& matrixU() const
    {
      if (m_extractedDataAreDirty) extractData();
      return m_u;
    }

    inline const IntColVectorType& permutationP() const
    {
      if (m_extractedDataAreDirty) extractData();
      return m_p;
    }

    inline const IntRowVectorType& permutationQ() const
    {
      if (m_extractedDataAreDirty) extractData();
      return m_q;
    }

    Scalar determinant() const;

    template<typename BDerived, typename XDerived>
    bool solve(const MatrixBase<BDerived> &b, MatrixBase<XDerived>* x) const;

    void compute(const MatrixType& matrix);

  protected:

    void extractData() const;
  
  protected:
    // cached data:
    void* m_numeric;
    const MatrixType* m_matrixRef;
    mutable LMatrixType m_l;
    mutable UMatrixType m_u;
    mutable IntColVectorType m_p;
    mutable IntRowVectorType m_q;
    mutable bool m_extractedDataAreDirty;
};

template<typename MatrixType>
void SparseLU<MatrixType,UmfPack>::compute(const MatrixType& a)
{
  const int rows = a.rows();
  const int cols = a.cols();
  ei_assert((MatrixType::Flags&RowMajorBit)==0 && "Row major matrices are not supported yet");

  m_matrixRef = &a;

  if (m_numeric)
    umfpack_free_numeric(&m_numeric,Scalar());

  void* symbolic;
  int errorCode = 0;
  errorCode = umfpack_symbolic(rows, cols, a._outerIndexPtr(), a._innerIndexPtr(), a._valuePtr(),
                                  &symbolic, 0, 0);
  if (errorCode==0)
    errorCode = umfpack_numeric(a._outerIndexPtr(), a._innerIndexPtr(), a._valuePtr(),
                                   symbolic, &m_numeric, 0, 0);

  umfpack_free_symbolic(&symbolic,Scalar());

  m_extractedDataAreDirty = true;

  Base::m_succeeded = (errorCode==0);
}

template<typename MatrixType>
void SparseLU<MatrixType,UmfPack>::extractData() const
{
  if (m_extractedDataAreDirty)
  {
    // get size of the data
    int lnz, unz, rows, cols, nz_udiag;
    umfpack_get_lunz(&lnz, &unz, &rows, &cols, &nz_udiag, m_numeric, Scalar());

    // allocate data
    m_l.resize(rows,std::min(rows,cols));
    m_l.resizeNonZeros(lnz);
    
    m_u.resize(std::min(rows,cols),cols);
    m_u.resizeNonZeros(unz);

    m_p.resize(rows);
    m_q.resize(cols);

    // extract
    umfpack_get_numeric(m_l._outerIndexPtr(), m_l._innerIndexPtr(), m_l._valuePtr(),
                        m_u._outerIndexPtr(), m_u._innerIndexPtr(), m_u._valuePtr(),
                        m_p.data(), m_q.data(), 0, 0, 0, m_numeric);
    
    m_extractedDataAreDirty = false;
  }
}

template<typename MatrixType>
typename SparseLU<MatrixType,UmfPack>::Scalar SparseLU<MatrixType,UmfPack>::determinant() const
{
  Scalar det;
  umfpack_get_determinant(&det, 0, m_numeric, 0);
  return det;
}

template<typename MatrixType>
template<typename BDerived,typename XDerived>
bool SparseLU<MatrixType,UmfPack>::solve(const MatrixBase<BDerived> &b, MatrixBase<XDerived> *x) const
{
  //const int size = m_matrix.rows();
  const int rhsCols = b.cols();
//   ei_assert(size==b.rows());
  ei_assert((BDerived::Flags&RowMajorBit)==0 && "UmfPack backend does not support non col-major rhs yet");
  ei_assert((XDerived::Flags&RowMajorBit)==0 && "UmfPack backend does not support non col-major result yet");

  int errorCode;
  for (int j=0; j<rhsCols; ++j)
  {
    errorCode = umfpack_solve(UMFPACK_A,
        m_matrixRef->_outerIndexPtr(), m_matrixRef->_innerIndexPtr(), m_matrixRef->_valuePtr(),
        &x->col(j).coeffRef(0), &b.const_cast_derived().col(j).coeffRef(0), m_numeric, 0, 0);
    if (errorCode!=0)
      return false;
  }
//   errorCode = umfpack_di_solve(UMFPACK_A,
//       m_matrixRef._outerIndexPtr(), m_matrixRef._innerIndexPtr(), m_matrixRef._valuePtr(),
//       x->derived().data(), b.derived().data(), m_numeric, 0, 0);

  return true;
}

#endif // EIGEN_UMFPACKSUPPORT_H
