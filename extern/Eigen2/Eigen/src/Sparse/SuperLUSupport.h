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

#ifndef EIGEN_SUPERLUSUPPORT_H
#define EIGEN_SUPERLUSUPPORT_H

// declaration of gssvx taken from GMM++
#define DECL_GSSVX(NAMESPACE,FNAME,FLOATTYPE,KEYTYPE) \
    inline float SuperLU_gssvx(superlu_options_t *options, SuperMatrix *A,  \
         int *perm_c, int *perm_r, int *etree, char *equed,  \
         FLOATTYPE *R, FLOATTYPE *C, SuperMatrix *L,         \
         SuperMatrix *U, void *work, int lwork,              \
         SuperMatrix *B, SuperMatrix *X,                     \
         FLOATTYPE *recip_pivot_growth,                      \
         FLOATTYPE *rcond, FLOATTYPE *ferr, FLOATTYPE *berr, \
         SuperLUStat_t *stats, int *info, KEYTYPE) {         \
    NAMESPACE::mem_usage_t mem_usage;                                    \
    NAMESPACE::FNAME(options, A, perm_c, perm_r, etree, equed, R, C, L,  \
         U, work, lwork, B, X, recip_pivot_growth, rcond,    \
         ferr, berr, &mem_usage, stats, info);               \
    return mem_usage.for_lu; /* bytes used by the factor storage */     \
  }

DECL_GSSVX(SuperLU_S,sgssvx,float,float)
DECL_GSSVX(SuperLU_C,cgssvx,float,std::complex<float>)
DECL_GSSVX(SuperLU_D,dgssvx,double,double)
DECL_GSSVX(SuperLU_Z,zgssvx,double,std::complex<double>)

template<typename MatrixType>
struct SluMatrixMapHelper;

/** \internal
  *
  * A wrapper class for SuperLU matrices. It supports only compressed sparse matrices
  * and dense matrices. Supernodal and other fancy format are not supported by this wrapper.
  *
  * This wrapper class mainly aims to avoids the need of dynamic allocation of the storage structure.
  */
struct SluMatrix : SuperMatrix
{
  SluMatrix() {}

  SluMatrix(const SluMatrix& other)
    : SuperMatrix(other)
  {
    Store = &storage;
    storage = other.storage;
  }

  struct
  {
    union {int nnz;int lda;};
    void *values;
    int *innerInd;
    int *outerInd;
  } storage;

  void setStorageType(Stype_t t)
  {
    Stype = t;
    if (t==SLU_NC || t==SLU_NR || t==SLU_DN)
      Store = &storage;
    else
    {
      ei_assert(false && "storage type not supported");
      Store = 0;
    }
  }

  template<typename Scalar>
  void setScalarType()
  {
    if (ei_is_same_type<Scalar,float>::ret)
      Dtype = SLU_S;
    else if (ei_is_same_type<Scalar,double>::ret)
      Dtype = SLU_D;
    else if (ei_is_same_type<Scalar,std::complex<float> >::ret)
      Dtype = SLU_C;
    else if (ei_is_same_type<Scalar,std::complex<double> >::ret)
      Dtype = SLU_Z;
    else
    {
      ei_assert(false && "Scalar type not supported by SuperLU");
    }
  }
  
  template<typename Scalar, int Rows, int Cols, int Options, int MRows, int MCols>
  static SluMatrix Map(Matrix<Scalar,Rows,Cols,Options,MRows,MCols>& mat)
  {
    typedef Matrix<Scalar,Rows,Cols,Options,MRows,MCols> MatrixType;
    ei_assert( ((Options&RowMajor)!=RowMajor) && "row-major dense matrices is not supported by SuperLU");
    SluMatrix res;
    res.setStorageType(SLU_DN);
    res.setScalarType<Scalar>();
    res.Mtype     = SLU_GE;

    res.nrow      = mat.rows();
    res.ncol      = mat.cols();

    res.storage.lda       = mat.stride();
    res.storage.values    = mat.data();
    return res;
  }

  template<typename MatrixType>
  static SluMatrix Map(SparseMatrixBase<MatrixType>& mat)
  {
    SluMatrix res;
    if ((MatrixType::Flags&RowMajorBit)==RowMajorBit)
    {
      res.setStorageType(SLU_NR);
      res.nrow      = mat.cols();
      res.ncol      = mat.rows();
    }
    else
    {
      res.setStorageType(SLU_NC);
      res.nrow      = mat.rows();
      res.ncol      = mat.cols();
    }

    res.Mtype     = SLU_GE;

    res.storage.nnz       = mat.nonZeros();
    res.storage.values    = mat.derived()._valuePtr();
    res.storage.innerInd  = mat.derived()._innerIndexPtr();
    res.storage.outerInd  = mat.derived()._outerIndexPtr();

    res.setScalarType<typename MatrixType::Scalar>();

    // FIXME the following is not very accurate
    if (MatrixType::Flags & UpperTriangular)
      res.Mtype = SLU_TRU;
    if (MatrixType::Flags & LowerTriangular)
      res.Mtype = SLU_TRL;
    if (MatrixType::Flags & SelfAdjoint)
      ei_assert(false && "SelfAdjoint matrix shape not supported by SuperLU");
    return res;
  }
};

template<typename Scalar, int Rows, int Cols, int Options, int MRows, int MCols>
struct SluMatrixMapHelper<Matrix<Scalar,Rows,Cols,Options,MRows,MCols> >
{
  typedef Matrix<Scalar,Rows,Cols,Options,MRows,MCols> MatrixType;
  static void run(MatrixType& mat, SluMatrix& res)
  {
    ei_assert( ((Options&RowMajor)!=RowMajor) && "row-major dense matrices is not supported by SuperLU");
    res.setStorageType(SLU_DN);
    res.setScalarType<Scalar>();
    res.Mtype     = SLU_GE;

    res.nrow      = mat.rows();
    res.ncol      = mat.cols();

    res.storage.lda       = mat.stride();
    res.storage.values    = mat.data();
  }
};

template<typename Derived>
struct SluMatrixMapHelper<SparseMatrixBase<Derived> >
{
  typedef Derived MatrixType;
  static void run(MatrixType& mat, SluMatrix& res)
  {
    if ((MatrixType::Flags&RowMajorBit)==RowMajorBit)
    {
      res.setStorageType(SLU_NR);
      res.nrow      = mat.cols();
      res.ncol      = mat.rows();
    }
    else
    {
      res.setStorageType(SLU_NC);
      res.nrow      = mat.rows();
      res.ncol      = mat.cols();
    }

    res.Mtype     = SLU_GE;

    res.storage.nnz       = mat.nonZeros();
    res.storage.values    = mat._valuePtr();
    res.storage.innerInd  = mat._innerIndexPtr();
    res.storage.outerInd  = mat._outerIndexPtr();

    res.setScalarType<typename MatrixType::Scalar>();

    // FIXME the following is not very accurate
    if (MatrixType::Flags & UpperTriangular)
      res.Mtype = SLU_TRU;
    if (MatrixType::Flags & LowerTriangular)
      res.Mtype = SLU_TRL;
    if (MatrixType::Flags & SelfAdjoint)
      ei_assert(false && "SelfAdjoint matrix shape not supported by SuperLU");
  }
};

template<typename Derived>
SluMatrix SparseMatrixBase<Derived>::asSluMatrix()
{
  return SluMatrix::Map(derived());
}

template<typename Scalar, int Flags>
MappedSparseMatrix<Scalar,Flags>::MappedSparseMatrix(SluMatrix& sluMat)
{
  if ((Flags&RowMajorBit)==RowMajorBit)
  {
    assert(sluMat.Stype == SLU_NR);
    m_innerSize   = sluMat.ncol;
    m_outerSize   = sluMat.nrow;
  }
  else
  {
    assert(sluMat.Stype == SLU_NC);
    m_innerSize   = sluMat.nrow;
    m_outerSize   = sluMat.ncol;
  }
  m_outerIndex = sluMat.storage.outerInd;
  m_innerIndices = sluMat.storage.innerInd;
  m_values = reinterpret_cast<Scalar*>(sluMat.storage.values);
  m_nnz = sluMat.storage.outerInd[m_outerSize];
}

template<typename MatrixType>
class SparseLU<MatrixType,SuperLU> : public SparseLU<MatrixType>
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
      : Base(flags)
    {
    }

    SparseLU(const MatrixType& matrix, int flags = NaturalOrdering)
      : Base(flags)
    {
      compute(matrix);
    }

    ~SparseLU()
    {
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
    // cached data to reduce reallocation, etc.
    mutable LMatrixType m_l;
    mutable UMatrixType m_u;
    mutable IntColVectorType m_p;
    mutable IntRowVectorType m_q;

    mutable SparseMatrix<Scalar> m_matrix;
    mutable SluMatrix m_sluA;
    mutable SuperMatrix m_sluL, m_sluU;
    mutable SluMatrix m_sluB, m_sluX;
    mutable SuperLUStat_t m_sluStat;
    mutable superlu_options_t m_sluOptions;
    mutable std::vector<int> m_sluEtree;
    mutable std::vector<RealScalar> m_sluRscale, m_sluCscale;
    mutable std::vector<RealScalar> m_sluFerr, m_sluBerr;
    mutable char m_sluEqued;
    mutable bool m_extractedDataAreDirty;
};

template<typename MatrixType>
void SparseLU<MatrixType,SuperLU>::compute(const MatrixType& a)
{
  const int size = a.rows();
  m_matrix = a;

  set_default_options(&m_sluOptions);
  m_sluOptions.ColPerm = NATURAL;
  m_sluOptions.PrintStat = NO;
  m_sluOptions.ConditionNumber = NO;
  m_sluOptions.Trans = NOTRANS;
  // m_sluOptions.Equil = NO;

  switch (Base::orderingMethod())
  {
      case NaturalOrdering          : m_sluOptions.ColPerm = NATURAL; break;
      case MinimumDegree_AT_PLUS_A  : m_sluOptions.ColPerm = MMD_AT_PLUS_A; break;
      case MinimumDegree_ATA        : m_sluOptions.ColPerm = MMD_ATA; break;
      case ColApproxMinimumDegree   : m_sluOptions.ColPerm = COLAMD; break;
      default:
        std::cerr << "Eigen: ordering method \"" << Base::orderingMethod() << "\" not supported by the SuperLU backend\n";
        m_sluOptions.ColPerm = NATURAL;
  };

  m_sluA = m_matrix.asSluMatrix();
  memset(&m_sluL,0,sizeof m_sluL);
  memset(&m_sluU,0,sizeof m_sluU);
  m_sluEqued = 'B';
  int info = 0;

  m_p.resize(size);
  m_q.resize(size);
  m_sluRscale.resize(size);
  m_sluCscale.resize(size);
  m_sluEtree.resize(size);

  RealScalar recip_pivot_gross, rcond;
  RealScalar ferr, berr;

  // set empty B and X
  m_sluB.setStorageType(SLU_DN);
  m_sluB.setScalarType<Scalar>();
  m_sluB.Mtype = SLU_GE;
  m_sluB.storage.values = 0;
  m_sluB.nrow = m_sluB.ncol = 0;
  m_sluB.storage.lda = size;
  m_sluX = m_sluB;

  StatInit(&m_sluStat);
  SuperLU_gssvx(&m_sluOptions, &m_sluA, m_q.data(), m_p.data(), &m_sluEtree[0],
          &m_sluEqued, &m_sluRscale[0], &m_sluCscale[0],
          &m_sluL, &m_sluU,
          NULL, 0,
          &m_sluB, &m_sluX,
          &recip_pivot_gross, &rcond,
          &ferr, &berr,
          &m_sluStat, &info, Scalar());
  StatFree(&m_sluStat);

  m_extractedDataAreDirty = true;

  // FIXME how to better check for errors ???
  Base::m_succeeded = (info == 0);
}

template<typename MatrixType>
template<typename BDerived,typename XDerived>
bool SparseLU<MatrixType,SuperLU>::solve(const MatrixBase<BDerived> &b, MatrixBase<XDerived> *x) const
{
  const int size = m_matrix.rows();
  const int rhsCols = b.cols();
  ei_assert(size==b.rows());

  m_sluOptions.Fact = FACTORED;
  m_sluOptions.IterRefine = NOREFINE;

  m_sluFerr.resize(rhsCols);
  m_sluBerr.resize(rhsCols);
  m_sluB = SluMatrix::Map(b.const_cast_derived());
  m_sluX = SluMatrix::Map(x->derived());

  StatInit(&m_sluStat);
  int info = 0;
  RealScalar recip_pivot_gross, rcond;
  SuperLU_gssvx(
    &m_sluOptions, &m_sluA,
    m_q.data(), m_p.data(),
    &m_sluEtree[0], &m_sluEqued,
    &m_sluRscale[0], &m_sluCscale[0],
    &m_sluL, &m_sluU,
    NULL, 0,
    &m_sluB, &m_sluX,
    &recip_pivot_gross, &rcond,
    &m_sluFerr[0], &m_sluBerr[0],
    &m_sluStat, &info, Scalar());
  StatFree(&m_sluStat);

  return info==0;
}

//
// the code of this extractData() function has been adapted from the SuperLU's Matlab support code,
//
//  Copyright (c) 1994 by Xerox Corporation.  All rights reserved.
//
//  THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY
//  EXPRESSED OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
//
template<typename MatrixType>
void SparseLU<MatrixType,SuperLU>::extractData() const
{
  if (m_extractedDataAreDirty)
  {
    int         upper;
    int         fsupc, istart, nsupr;
    int         lastl = 0, lastu = 0;
    SCformat    *Lstore = static_cast<SCformat*>(m_sluL.Store);
    NCformat    *Ustore = static_cast<NCformat*>(m_sluU.Store);
    Scalar      *SNptr;

    const int size = m_matrix.rows();
    m_l.resize(size,size);
    m_l.resizeNonZeros(Lstore->nnz);
    m_u.resize(size,size);
    m_u.resizeNonZeros(Ustore->nnz);

    int* Lcol = m_l._outerIndexPtr();
    int* Lrow = m_l._innerIndexPtr();
    Scalar* Lval = m_l._valuePtr();

    int* Ucol = m_u._outerIndexPtr();
    int* Urow = m_u._innerIndexPtr();
    Scalar* Uval = m_u._valuePtr();

    Ucol[0] = 0;
    Ucol[0] = 0;

    /* for each supernode */
    for (int k = 0; k <= Lstore->nsuper; ++k)
    {
      fsupc   = L_FST_SUPC(k);
      istart  = L_SUB_START(fsupc);
      nsupr   = L_SUB_START(fsupc+1) - istart;
      upper   = 1;

      /* for each column in the supernode */
      for (int j = fsupc; j < L_FST_SUPC(k+1); ++j)
      {
        SNptr = &((Scalar*)Lstore->nzval)[L_NZ_START(j)];

        /* Extract U */
        for (int i = U_NZ_START(j); i < U_NZ_START(j+1); ++i)
        {
          Uval[lastu] = ((Scalar*)Ustore->nzval)[i];
          /* Matlab doesn't like explicit zero. */
          if (Uval[lastu] != 0.0)
            Urow[lastu++] = U_SUB(i);
        }
        for (int i = 0; i < upper; ++i)
        {
          /* upper triangle in the supernode */
          Uval[lastu] = SNptr[i];
          /* Matlab doesn't like explicit zero. */
          if (Uval[lastu] != 0.0)
            Urow[lastu++] = L_SUB(istart+i);
        }
        Ucol[j+1] = lastu;

        /* Extract L */
        Lval[lastl] = 1.0; /* unit diagonal */
        Lrow[lastl++] = L_SUB(istart + upper - 1);
        for (int i = upper; i < nsupr; ++i)
        {
          Lval[lastl] = SNptr[i];
          /* Matlab doesn't like explicit zero. */
          if (Lval[lastl] != 0.0)
            Lrow[lastl++] = L_SUB(istart+i);
        }
        Lcol[j+1] = lastl;

        ++upper;
      } /* for j ... */

    } /* for k ... */

    // squeeze the matrices :
    m_l.resizeNonZeros(lastl);
    m_u.resizeNonZeros(lastu);

    m_extractedDataAreDirty = false;
  }
}

template<typename MatrixType>
typename SparseLU<MatrixType,SuperLU>::Scalar SparseLU<MatrixType,SuperLU>::determinant() const
{
  if (m_extractedDataAreDirty)
    extractData();

  // TODO this code coule be moved to the default/base backend
  // FIXME perhaps we have to take into account the scale factors m_sluRscale and m_sluCscale ???
  Scalar det = Scalar(1);
  for (int j=0; j<m_u.cols(); ++j)
  {
    if (m_u._outerIndexPtr()[j+1]-m_u._outerIndexPtr()[j] > 0)
    {
      int lastId = m_u._outerIndexPtr()[j+1]-1;
      ei_assert(m_u._innerIndexPtr()[lastId]<=j);
      if (m_u._innerIndexPtr()[lastId]==j)
      {
        det *= m_u._valuePtr()[lastId];
      }
    }
    // std::cout << m_sluRscale[j] << " " << m_sluCscale[j] << "   ";
  }
  return det;
}

#endif // EIGEN_SUPERLUSUPPORT_H
