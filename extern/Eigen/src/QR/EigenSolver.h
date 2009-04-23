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

#ifndef EIGEN_EIGENSOLVER_H
#define EIGEN_EIGENSOLVER_H

/** \ingroup QR_Module
  * \nonstableyet
  *
  * \class EigenSolver
  *
  * \brief Eigen values/vectors solver for non selfadjoint matrices
  *
  * \param MatrixType the type of the matrix of which we are computing the eigen decomposition
  *
  * Currently it only support real matrices.
  *
  * \note this code was adapted from JAMA (public domain)
  *
  * \sa MatrixBase::eigenvalues(), SelfAdjointEigenSolver
  */
template<typename _MatrixType> class EigenSolver
{
  public:

    typedef _MatrixType MatrixType;
    typedef typename MatrixType::Scalar Scalar;
    typedef typename NumTraits<Scalar>::Real RealScalar;
    typedef std::complex<RealScalar> Complex;
    typedef Matrix<Complex, MatrixType::ColsAtCompileTime, 1> EigenvalueType;
    typedef Matrix<Complex, MatrixType::RowsAtCompileTime, MatrixType::ColsAtCompileTime> EigenvectorType;
    typedef Matrix<RealScalar, MatrixType::ColsAtCompileTime, 1> RealVectorType;
    typedef Matrix<RealScalar, Dynamic, 1> RealVectorTypeX;

    EigenSolver(const MatrixType& matrix)
      : m_eivec(matrix.rows(), matrix.cols()),
        m_eivalues(matrix.cols())
    {
      compute(matrix);
    }


    EigenvectorType eigenvectors(void) const;

    /** \returns a real matrix V of pseudo eigenvectors.
      *
      * Let D be the block diagonal matrix with the real eigenvalues in 1x1 blocks,
      * and any complex values u+iv in 2x2 blocks [u v ; -v u]. Then, the matrices D
      * and V satisfy A*V = V*D.
      *
      * More precisely, if the diagonal matrix of the eigen values is:\n
      * \f$
      * \left[ \begin{array}{cccccc}
      * u+iv &      &      &      &   &   \\
      *      & u-iv &      &      &   &   \\
      *      &      & a+ib &      &   &   \\
      *      &      &      & a-ib &   &   \\
      *      &      &      &      & x &   \\
      *      &      &      &      &   & y \\
      * \end{array} \right]
      * \f$ \n
      * then, we have:\n
      * \f$
      * D =\left[ \begin{array}{cccccc}
      *  u & v &    &   &   &   \\
      * -v & u &    &   &   &   \\
      *    &   &  a & b &   &   \\
      *    &   & -b & a &   &   \\
      *    &   &    &   & x &   \\
      *    &   &    &   &   & y \\
      * \end{array} \right]
      * \f$
      *
      * \sa pseudoEigenvalueMatrix()
      */
    const MatrixType& pseudoEigenvectors() const { return m_eivec; }

    MatrixType pseudoEigenvalueMatrix() const;

    /** \returns the eigenvalues as a column vector */
    EigenvalueType eigenvalues() const { return m_eivalues; }

    void compute(const MatrixType& matrix);

  private:

    void orthes(MatrixType& matH, RealVectorType& ort);
    void hqr2(MatrixType& matH);

  protected:
    MatrixType m_eivec;
    EigenvalueType m_eivalues;
};

/** \returns the real block diagonal matrix D of the eigenvalues.
  *
  * See pseudoEigenvectors() for the details.
  */
template<typename MatrixType>
MatrixType EigenSolver<MatrixType>::pseudoEigenvalueMatrix() const
{
  int n = m_eivec.cols();
  MatrixType matD = MatrixType::Zero(n,n);
  for (int i=0; i<n; ++i)
  {
    if (ei_isMuchSmallerThan(ei_imag(m_eivalues.coeff(i)), ei_real(m_eivalues.coeff(i))))
      matD.coeffRef(i,i) = ei_real(m_eivalues.coeff(i));
    else
    {
      matD.template block<2,2>(i,i) <<  ei_real(m_eivalues.coeff(i)), ei_imag(m_eivalues.coeff(i)),
                                       -ei_imag(m_eivalues.coeff(i)), ei_real(m_eivalues.coeff(i));
      ++i;
    }
  }
  return matD;
}

/** \returns the normalized complex eigenvectors as a matrix of column vectors.
  *
  * \sa eigenvalues(), pseudoEigenvectors()
  */
template<typename MatrixType>
typename EigenSolver<MatrixType>::EigenvectorType EigenSolver<MatrixType>::eigenvectors(void) const
{
  int n = m_eivec.cols();
  EigenvectorType matV(n,n);
  for (int j=0; j<n; ++j)
  {
    if (ei_isMuchSmallerThan(ei_abs(ei_imag(m_eivalues.coeff(j))), ei_abs(ei_real(m_eivalues.coeff(j)))))
    {
      // we have a real eigen value
      matV.col(j) = m_eivec.col(j).template cast<Complex>();
    }
    else
    {
      // we have a pair of complex eigen values
      for (int i=0; i<n; ++i)
      {
        matV.coeffRef(i,j)   = Complex(m_eivec.coeff(i,j),  m_eivec.coeff(i,j+1));
        matV.coeffRef(i,j+1) = Complex(m_eivec.coeff(i,j), -m_eivec.coeff(i,j+1));
      }
      matV.col(j).normalize();
      matV.col(j+1).normalize();
      ++j;
    }
  }
  return matV;
}

template<typename MatrixType>
void EigenSolver<MatrixType>::compute(const MatrixType& matrix)
{
  assert(matrix.cols() == matrix.rows());
  int n = matrix.cols();
  m_eivalues.resize(n,1);

  MatrixType matH = matrix;
  RealVectorType ort(n);

  // Reduce to Hessenberg form.
  orthes(matH, ort);

  // Reduce Hessenberg to real Schur form.
  hqr2(matH);
}

// Nonsymmetric reduction to Hessenberg form.
template<typename MatrixType>
void EigenSolver<MatrixType>::orthes(MatrixType& matH, RealVectorType& ort)
{
  //  This is derived from the Algol procedures orthes and ortran,
  //  by Martin and Wilkinson, Handbook for Auto. Comp.,
  //  Vol.ii-Linear Algebra, and the corresponding
  //  Fortran subroutines in EISPACK.

  int n = m_eivec.cols();
  int low = 0;
  int high = n-1;

  for (int m = low+1; m <= high-1; ++m)
  {
    // Scale column.
    RealScalar scale = matH.block(m, m-1, high-m+1, 1).cwise().abs().sum();
    if (scale != 0.0)
    {
      // Compute Householder transformation.
      RealScalar h = 0.0;
      // FIXME could be rewritten, but this one looks better wrt cache
      for (int i = high; i >= m; i--)
      {
        ort.coeffRef(i) = matH.coeff(i,m-1)/scale;
        h += ort.coeff(i) * ort.coeff(i);
      }
      RealScalar g = ei_sqrt(h);
      if (ort.coeff(m) > 0)
        g = -g;
      h = h - ort.coeff(m) * g;
      ort.coeffRef(m) = ort.coeff(m) - g;

      // Apply Householder similarity transformation
      // H = (I-u*u'/h)*H*(I-u*u')/h)
      int bSize = high-m+1;
      matH.block(m, m, bSize, n-m) -= ((ort.segment(m, bSize)/h)
        * (ort.segment(m, bSize).transpose() *  matH.block(m, m, bSize, n-m)).lazy()).lazy();

      matH.block(0, m, high+1, bSize) -= ((matH.block(0, m, high+1, bSize) * ort.segment(m, bSize)).lazy()
        * (ort.segment(m, bSize)/h).transpose()).lazy();

      ort.coeffRef(m) = scale*ort.coeff(m);
      matH.coeffRef(m,m-1) = scale*g;
    }
  }

  // Accumulate transformations (Algol's ortran).
  m_eivec.setIdentity();

  for (int m = high-1; m >= low+1; m--)
  {
    if (matH.coeff(m,m-1) != 0.0)
    {
      ort.segment(m+1, high-m) = matH.col(m-1).segment(m+1, high-m);

      int bSize = high-m+1;
      m_eivec.block(m, m, bSize, bSize) += ( (ort.segment(m, bSize) /  (matH.coeff(m,m-1) * ort.coeff(m) ) )
        * (ort.segment(m, bSize).transpose() * m_eivec.block(m, m, bSize, bSize)).lazy());
    }
  }
}

// Complex scalar division.
template<typename Scalar>
std::complex<Scalar> cdiv(Scalar xr, Scalar xi, Scalar yr, Scalar yi)
{
  Scalar r,d;
  if (ei_abs(yr) > ei_abs(yi))
  {
      r = yi/yr;
      d = yr + r*yi;
      return std::complex<Scalar>((xr + r*xi)/d, (xi - r*xr)/d);
  }
  else
  {
      r = yr/yi;
      d = yi + r*yr;
      return std::complex<Scalar>((r*xr + xi)/d, (r*xi - xr)/d);
  }
}


// Nonsymmetric reduction from Hessenberg to real Schur form.
template<typename MatrixType>
void EigenSolver<MatrixType>::hqr2(MatrixType& matH)
{
  //  This is derived from the Algol procedure hqr2,
  //  by Martin and Wilkinson, Handbook for Auto. Comp.,
  //  Vol.ii-Linear Algebra, and the corresponding
  //  Fortran subroutine in EISPACK.

  // Initialize
  int nn = m_eivec.cols();
  int n = nn-1;
  int low = 0;
  int high = nn-1;
  Scalar eps = ei_pow(Scalar(2),ei_is_same_type<Scalar,float>::ret ? Scalar(-23) : Scalar(-52));
  Scalar exshift = 0.0;
  Scalar p=0,q=0,r=0,s=0,z=0,t,w,x,y;

  // Store roots isolated by balanc and compute matrix norm
  // FIXME to be efficient the following would requires a triangular reduxion code
  // Scalar norm = matH.upper().cwise().abs().sum() + matH.corner(BottomLeft,n,n).diagonal().cwise().abs().sum();
  Scalar norm = 0.0;
  for (int j = 0; j < nn; ++j)
  {
    // FIXME what's the purpose of the following since the condition is always false
    if ((j < low) || (j > high))
    {
      m_eivalues.coeffRef(j) = Complex(matH.coeff(j,j), 0.0);
    }
    norm += matH.row(j).segment(std::max(j-1,0), nn-std::max(j-1,0)).cwise().abs().sum();
  }

  // Outer loop over eigenvalue index
  int iter = 0;
  while (n >= low)
  {
    // Look for single small sub-diagonal element
    int l = n;
    while (l > low)
    {
      s = ei_abs(matH.coeff(l-1,l-1)) + ei_abs(matH.coeff(l,l));
      if (s == 0.0)
          s = norm;
      if (ei_abs(matH.coeff(l,l-1)) < eps * s)
        break;
      l--;
    }

    // Check for convergence
    // One root found
    if (l == n)
    {
      matH.coeffRef(n,n) = matH.coeff(n,n) + exshift;
      m_eivalues.coeffRef(n) = Complex(matH.coeff(n,n), 0.0);
      n--;
      iter = 0;
    }
    else if (l == n-1) // Two roots found
    {
      w = matH.coeff(n,n-1) * matH.coeff(n-1,n);
      p = (matH.coeff(n-1,n-1) - matH.coeff(n,n)) * Scalar(0.5);
      q = p * p + w;
      z = ei_sqrt(ei_abs(q));
      matH.coeffRef(n,n) = matH.coeff(n,n) + exshift;
      matH.coeffRef(n-1,n-1) = matH.coeff(n-1,n-1) + exshift;
      x = matH.coeff(n,n);

      // Scalar pair
      if (q >= 0)
      {
        if (p >= 0)
          z = p + z;
        else
          z = p - z;

        m_eivalues.coeffRef(n-1) = Complex(x + z, 0.0);
        m_eivalues.coeffRef(n) = Complex(z!=0.0 ? x - w / z : m_eivalues.coeff(n-1).real(), 0.0);

        x = matH.coeff(n,n-1);
        s = ei_abs(x) + ei_abs(z);
        p = x / s;
        q = z / s;
        r = ei_sqrt(p * p+q * q);
        p = p / r;
        q = q / r;

        // Row modification
        for (int j = n-1; j < nn; ++j)
        {
          z = matH.coeff(n-1,j);
          matH.coeffRef(n-1,j) = q * z + p * matH.coeff(n,j);
          matH.coeffRef(n,j) = q * matH.coeff(n,j) - p * z;
        }

        // Column modification
        for (int i = 0; i <= n; ++i)
        {
          z = matH.coeff(i,n-1);
          matH.coeffRef(i,n-1) = q * z + p * matH.coeff(i,n);
          matH.coeffRef(i,n) = q * matH.coeff(i,n) - p * z;
        }

        // Accumulate transformations
        for (int i = low; i <= high; ++i)
        {
          z = m_eivec.coeff(i,n-1);
          m_eivec.coeffRef(i,n-1) = q * z + p * m_eivec.coeff(i,n);
          m_eivec.coeffRef(i,n) = q * m_eivec.coeff(i,n) - p * z;
        }
      }
      else // Complex pair
      {
        m_eivalues.coeffRef(n-1) = Complex(x + p, z);
        m_eivalues.coeffRef(n)   = Complex(x + p, -z);
      }
      n = n - 2;
      iter = 0;
    }
    else // No convergence yet
    {
      // Form shift
      x = matH.coeff(n,n);
      y = 0.0;
      w = 0.0;
      if (l < n)
      {
          y = matH.coeff(n-1,n-1);
          w = matH.coeff(n,n-1) * matH.coeff(n-1,n);
      }

      // Wilkinson's original ad hoc shift
      if (iter == 10)
      {
        exshift += x;
        for (int i = low; i <= n; ++i)
          matH.coeffRef(i,i) -= x;
        s = ei_abs(matH.coeff(n,n-1)) + ei_abs(matH.coeff(n-1,n-2));
        x = y = Scalar(0.75) * s;
        w = Scalar(-0.4375) * s * s;
      }

      // MATLAB's new ad hoc shift
      if (iter == 30)
      {
        s = Scalar((y - x) / 2.0);
        s = s * s + w;
        if (s > 0)
        {
          s = ei_sqrt(s);
          if (y < x)
            s = -s;
          s = Scalar(x - w / ((y - x) / 2.0 + s));
          for (int i = low; i <= n; ++i)
            matH.coeffRef(i,i) -= s;
          exshift += s;
          x = y = w = Scalar(0.964);
        }
      }

      iter = iter + 1;   // (Could check iteration count here.)

      // Look for two consecutive small sub-diagonal elements
      int m = n-2;
      while (m >= l)
      {
        z = matH.coeff(m,m);
        r = x - z;
        s = y - z;
        p = (r * s - w) / matH.coeff(m+1,m) + matH.coeff(m,m+1);
        q = matH.coeff(m+1,m+1) - z - r - s;
        r = matH.coeff(m+2,m+1);
        s = ei_abs(p) + ei_abs(q) + ei_abs(r);
        p = p / s;
        q = q / s;
        r = r / s;
        if (m == l) {
          break;
        }
        if (ei_abs(matH.coeff(m,m-1)) * (ei_abs(q) + ei_abs(r)) <
          eps * (ei_abs(p) * (ei_abs(matH.coeff(m-1,m-1)) + ei_abs(z) +
          ei_abs(matH.coeff(m+1,m+1)))))
        {
          break;
        }
        m--;
      }

      for (int i = m+2; i <= n; ++i)
      {
        matH.coeffRef(i,i-2) = 0.0;
        if (i > m+2)
          matH.coeffRef(i,i-3) = 0.0;
      }

      // Double QR step involving rows l:n and columns m:n
      for (int k = m; k <= n-1; ++k)
      {
        int notlast = (k != n-1);
        if (k != m) {
          p = matH.coeff(k,k-1);
          q = matH.coeff(k+1,k-1);
          r = notlast ? matH.coeff(k+2,k-1) : Scalar(0);
          x = ei_abs(p) + ei_abs(q) + ei_abs(r);
          if (x != 0.0)
          {
            p = p / x;
            q = q / x;
            r = r / x;
          }
        }

        if (x == 0.0)
          break;

        s = ei_sqrt(p * p + q * q + r * r);

        if (p < 0)
          s = -s;

        if (s != 0)
        {
          if (k != m)
            matH.coeffRef(k,k-1) = -s * x;
          else if (l != m)
            matH.coeffRef(k,k-1) = -matH.coeff(k,k-1);

          p = p + s;
          x = p / s;
          y = q / s;
          z = r / s;
          q = q / p;
          r = r / p;

          // Row modification
          for (int j = k; j < nn; ++j)
          {
            p = matH.coeff(k,j) + q * matH.coeff(k+1,j);
            if (notlast)
            {
              p = p + r * matH.coeff(k+2,j);
              matH.coeffRef(k+2,j) = matH.coeff(k+2,j) - p * z;
            }
            matH.coeffRef(k,j) = matH.coeff(k,j) - p * x;
            matH.coeffRef(k+1,j) = matH.coeff(k+1,j) - p * y;
          }

          // Column modification
          for (int i = 0; i <= std::min(n,k+3); ++i)
          {
            p = x * matH.coeff(i,k) + y * matH.coeff(i,k+1);
            if (notlast)
            {
              p = p + z * matH.coeff(i,k+2);
              matH.coeffRef(i,k+2) = matH.coeff(i,k+2) - p * r;
            }
            matH.coeffRef(i,k) = matH.coeff(i,k) - p;
            matH.coeffRef(i,k+1) = matH.coeff(i,k+1) - p * q;
          }

          // Accumulate transformations
          for (int i = low; i <= high; ++i)
          {
            p = x * m_eivec.coeff(i,k) + y * m_eivec.coeff(i,k+1);
            if (notlast)
            {
              p = p + z * m_eivec.coeff(i,k+2);
              m_eivec.coeffRef(i,k+2) = m_eivec.coeff(i,k+2) - p * r;
            }
            m_eivec.coeffRef(i,k) = m_eivec.coeff(i,k) - p;
            m_eivec.coeffRef(i,k+1) = m_eivec.coeff(i,k+1) - p * q;
          }
        }  // (s != 0)
      }  // k loop
    }  // check convergence
  }  // while (n >= low)

  // Backsubstitute to find vectors of upper triangular form
  if (norm == 0.0)
  {
      return;
  }

  for (n = nn-1; n >= 0; n--)
  {
    p = m_eivalues.coeff(n).real();
    q = m_eivalues.coeff(n).imag();

    // Scalar vector
    if (q == 0)
    {
      int l = n;
      matH.coeffRef(n,n) = 1.0;
      for (int i = n-1; i >= 0; i--)
      {
        w = matH.coeff(i,i) - p;
        r = (matH.row(i).segment(l,n-l+1) * matH.col(n).segment(l, n-l+1))(0,0);

        if (m_eivalues.coeff(i).imag() < 0.0)
        {
          z = w;
          s = r;
        }
        else
        {
          l = i;
          if (m_eivalues.coeff(i).imag() == 0.0)
          {
            if (w != 0.0)
              matH.coeffRef(i,n) = -r / w;
            else
              matH.coeffRef(i,n) = -r / (eps * norm);
          }
          else // Solve real equations
          {
            x = matH.coeff(i,i+1);
            y = matH.coeff(i+1,i);
            q = (m_eivalues.coeff(i).real() - p) * (m_eivalues.coeff(i).real() - p) + m_eivalues.coeff(i).imag() * m_eivalues.coeff(i).imag();
            t = (x * s - z * r) / q;
            matH.coeffRef(i,n) = t;
            if (ei_abs(x) > ei_abs(z))
              matH.coeffRef(i+1,n) = (-r - w * t) / x;
            else
              matH.coeffRef(i+1,n) = (-s - y * t) / z;
          }

          // Overflow control
          t = ei_abs(matH.coeff(i,n));
          if ((eps * t) * t > 1)
            matH.col(n).end(nn-i) /= t;
        }
      }
    }
    else if (q < 0) // Complex vector
    {
      std::complex<Scalar> cc;
      int l = n-1;

      // Last vector component imaginary so matrix is triangular
      if (ei_abs(matH.coeff(n,n-1)) > ei_abs(matH.coeff(n-1,n)))
      {
        matH.coeffRef(n-1,n-1) = q / matH.coeff(n,n-1);
        matH.coeffRef(n-1,n) = -(matH.coeff(n,n) - p) / matH.coeff(n,n-1);
      }
      else
      {
        cc = cdiv<Scalar>(0.0,-matH.coeff(n-1,n),matH.coeff(n-1,n-1)-p,q);
        matH.coeffRef(n-1,n-1) = ei_real(cc);
        matH.coeffRef(n-1,n) = ei_imag(cc);
      }
      matH.coeffRef(n,n-1) = 0.0;
      matH.coeffRef(n,n) = 1.0;
      for (int i = n-2; i >= 0; i--)
      {
        Scalar ra,sa,vr,vi;
        ra = (matH.block(i,l, 1, n-l+1) * matH.block(l,n-1, n-l+1, 1)).lazy()(0,0);
        sa = (matH.block(i,l, 1, n-l+1) * matH.block(l,n, n-l+1, 1)).lazy()(0,0);
        w = matH.coeff(i,i) - p;

        if (m_eivalues.coeff(i).imag() < 0.0)
        {
          z = w;
          r = ra;
          s = sa;
        }
        else
        {
          l = i;
          if (m_eivalues.coeff(i).imag() == 0)
          {
            cc = cdiv(-ra,-sa,w,q);
            matH.coeffRef(i,n-1) = ei_real(cc);
            matH.coeffRef(i,n) = ei_imag(cc);
          }
          else
          {
            // Solve complex equations
            x = matH.coeff(i,i+1);
            y = matH.coeff(i+1,i);
            vr = (m_eivalues.coeff(i).real() - p) * (m_eivalues.coeff(i).real() - p) + m_eivalues.coeff(i).imag() * m_eivalues.coeff(i).imag() - q * q;
            vi = (m_eivalues.coeff(i).real() - p) * Scalar(2) * q;
            if ((vr == 0.0) && (vi == 0.0))
              vr = eps * norm * (ei_abs(w) + ei_abs(q) + ei_abs(x) + ei_abs(y) + ei_abs(z));

            cc= cdiv(x*r-z*ra+q*sa,x*s-z*sa-q*ra,vr,vi);
            matH.coeffRef(i,n-1) = ei_real(cc);
            matH.coeffRef(i,n) = ei_imag(cc);
            if (ei_abs(x) > (ei_abs(z) + ei_abs(q)))
            {
              matH.coeffRef(i+1,n-1) = (-ra - w * matH.coeff(i,n-1) + q * matH.coeff(i,n)) / x;
              matH.coeffRef(i+1,n) = (-sa - w * matH.coeff(i,n) - q * matH.coeff(i,n-1)) / x;
            }
            else
            {
              cc = cdiv(-r-y*matH.coeff(i,n-1),-s-y*matH.coeff(i,n),z,q);
              matH.coeffRef(i+1,n-1) = ei_real(cc);
              matH.coeffRef(i+1,n) = ei_imag(cc);
            }
          }

          // Overflow control
          t = std::max(ei_abs(matH.coeff(i,n-1)),ei_abs(matH.coeff(i,n)));
          if ((eps * t) * t > 1)
            matH.block(i, n-1, nn-i, 2) /= t;

        }
      }
    }
  }

  // Vectors of isolated roots
  for (int i = 0; i < nn; ++i)
  {
    // FIXME again what's the purpose of this test ?
    // in this algo low==0 and high==nn-1 !!
    if (i < low || i > high)
    {
      m_eivec.row(i).end(nn-i) = matH.row(i).end(nn-i);
    }
  }

  // Back transformation to get eigenvectors of original matrix
  int bRows = high-low+1;
  for (int j = nn-1; j >= low; j--)
  {
    int bSize = std::min(j,high)-low+1;
    m_eivec.col(j).segment(low, bRows) = (m_eivec.block(low, low, bRows, bSize) * matH.col(j).segment(low, bSize));
  }
}

#endif // EIGEN_EIGENSOLVER_H
