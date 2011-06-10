// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2008 Gael Guennebaud <gael.guennebaud@inria.fr>
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

#ifndef EIGEN_TRANSLATION_H
#define EIGEN_TRANSLATION_H

/** \geometry_module \ingroup Geometry_Module
  *
  * \class Translation
  *
  * \brief Represents a translation transformation
  *
  * \param _Scalar the scalar type, i.e., the type of the coefficients.
  * \param _Dim the  dimension of the space, can be a compile time value or Dynamic
  *
  * \note This class is not aimed to be used to store a translation transformation,
  * but rather to make easier the constructions and updates of Transform objects.
  *
  * \sa class Scaling, class Transform
  */
template<typename _Scalar, int _Dim>
class Translation
{
public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW_IF_VECTORIZABLE_FIXED_SIZE(_Scalar,_Dim)
  /** dimension of the space */
  enum { Dim = _Dim };
  /** the scalar type of the coefficients */
  typedef _Scalar Scalar;
  /** corresponding vector type */
  typedef Matrix<Scalar,Dim,1> VectorType;
  /** corresponding linear transformation matrix type */
  typedef Matrix<Scalar,Dim,Dim> LinearMatrixType;
  /** corresponding affine transformation type */
  typedef Transform<Scalar,Dim,Affine> AffineTransformType;

protected:

  VectorType m_coeffs;

public:

  /** Default constructor without initialization. */
  Translation() {}
  /**  */
  inline Translation(const Scalar& sx, const Scalar& sy)
  {
    eigen_assert(Dim==2);
    m_coeffs.x() = sx;
    m_coeffs.y() = sy;
  }
  /**  */
  inline Translation(const Scalar& sx, const Scalar& sy, const Scalar& sz)
  {
    eigen_assert(Dim==3);
    m_coeffs.x() = sx;
    m_coeffs.y() = sy;
    m_coeffs.z() = sz;
  }
  /** Constructs and initialize the translation transformation from a vector of translation coefficients */
  explicit inline Translation(const VectorType& vector) : m_coeffs(vector) {}

  /** \brief Retruns the x-translation by value. **/
  inline Scalar x() const { return m_coeffs.x(); }
  /** \brief Retruns the y-translation by value. **/
  inline Scalar y() const { return m_coeffs.y(); }
  /** \brief Retruns the z-translation by value. **/
  inline Scalar z() const { return m_coeffs.z(); }

  /** \brief Retruns the x-translation as a reference. **/
  inline Scalar& x() { return m_coeffs.x(); }
  /** \brief Retruns the y-translation as a reference. **/
  inline Scalar& y() { return m_coeffs.y(); }
  /** \brief Retruns the z-translation as a reference. **/
  inline Scalar& z() { return m_coeffs.z(); }

  const VectorType& vector() const { return m_coeffs; }
  VectorType& vector() { return m_coeffs; }

  const VectorType& translation() const { return m_coeffs; }
  VectorType& translation() { return m_coeffs; }

  /** Concatenates two translation */
  inline Translation operator* (const Translation& other) const
  { return Translation(m_coeffs + other.m_coeffs); }

  /** Concatenates a translation and a uniform scaling */
  inline AffineTransformType operator* (const UniformScaling<Scalar>& other) const;

  /** Concatenates a translation and a linear transformation */
  template<typename OtherDerived>
  inline AffineTransformType operator* (const EigenBase<OtherDerived>& linear) const;

  /** Concatenates a translation and a rotation */
  template<typename Derived>
  inline AffineTransformType operator*(const RotationBase<Derived,Dim>& r) const
  { return *this * r.toRotationMatrix(); }

  /** \returns the concatenation of a linear transformation \a l with the translation \a t */
  // its a nightmare to define a templated friend function outside its declaration
  template<typename OtherDerived> friend
  inline AffineTransformType operator*(const EigenBase<OtherDerived>& linear, const Translation& t)
  {
    AffineTransformType res;
    res.matrix().setZero();
    res.linear() = linear.derived();
    res.translation() = linear.derived() * t.m_coeffs;
    res.matrix().row(Dim).setZero();
    res(Dim,Dim) = Scalar(1);
    return res;
  }

  /** Concatenates a translation and a transformation */
  template<int Mode, int Options>
  inline Transform<Scalar,Dim,Mode> operator* (const Transform<Scalar,Dim,Mode,Options>& t) const
  {
    Transform<Scalar,Dim,Mode> res = t;
    res.pretranslate(m_coeffs);
    return res;
  }

  /** Applies translation to vector */
  inline VectorType operator* (const VectorType& other) const
  { return m_coeffs + other; }

  /** \returns the inverse translation (opposite) */
  Translation inverse() const { return Translation(-m_coeffs); }

  Translation& operator=(const Translation& other)
  {
    m_coeffs = other.m_coeffs;
    return *this;
  }

  static const Translation Identity() { return Translation(VectorType::Zero()); }

  /** \returns \c *this with scalar type casted to \a NewScalarType
    *
    * Note that if \a NewScalarType is equal to the current scalar type of \c *this
    * then this function smartly returns a const reference to \c *this.
    */
  template<typename NewScalarType>
  inline typename internal::cast_return_type<Translation,Translation<NewScalarType,Dim> >::type cast() const
  { return typename internal::cast_return_type<Translation,Translation<NewScalarType,Dim> >::type(*this); }

  /** Copy constructor with scalar type conversion */
  template<typename OtherScalarType>
  inline explicit Translation(const Translation<OtherScalarType,Dim>& other)
  { m_coeffs = other.vector().template cast<Scalar>(); }

  /** \returns \c true if \c *this is approximately equal to \a other, within the precision
    * determined by \a prec.
    *
    * \sa MatrixBase::isApprox() */
  bool isApprox(const Translation& other, typename NumTraits<Scalar>::Real prec = NumTraits<Scalar>::dummy_precision()) const
  { return m_coeffs.isApprox(other.m_coeffs, prec); }

};

/** \addtogroup Geometry_Module */
//@{
typedef Translation<float, 2> Translation2f;
typedef Translation<double,2> Translation2d;
typedef Translation<float, 3> Translation3f;
typedef Translation<double,3> Translation3d;
//@}

template<typename Scalar, int Dim>
inline typename Translation<Scalar,Dim>::AffineTransformType
Translation<Scalar,Dim>::operator* (const UniformScaling<Scalar>& other) const
{
  AffineTransformType res;
  res.matrix().setZero();
  res.linear().diagonal().fill(other.factor());
  res.translation() = m_coeffs;
  res(Dim,Dim) = Scalar(1);
  return res;
}

template<typename Scalar, int Dim>
template<typename OtherDerived>
inline typename Translation<Scalar,Dim>::AffineTransformType
Translation<Scalar,Dim>::operator* (const EigenBase<OtherDerived>& linear) const
{
  AffineTransformType res;
  res.matrix().setZero();
  res.linear() = linear.derived();
  res.translation() = m_coeffs;
  res.matrix().row(Dim).setZero();
  res(Dim,Dim) = Scalar(1);
  return res;
}

#endif // EIGEN_TRANSLATION_H
