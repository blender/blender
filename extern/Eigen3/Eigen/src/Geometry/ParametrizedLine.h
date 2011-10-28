// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2008 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2008 Benoit Jacob <jacob.benoit.1@gmail.com>
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

#ifndef EIGEN_PARAMETRIZEDLINE_H
#define EIGEN_PARAMETRIZEDLINE_H

/** \geometry_module \ingroup Geometry_Module
  *
  * \class ParametrizedLine
  *
  * \brief A parametrized line
  *
  * A parametrized line is defined by an origin point \f$ \mathbf{o} \f$ and a unit
  * direction vector \f$ \mathbf{d} \f$ such that the line corresponds to
  * the set \f$ l(t) = \mathbf{o} + t \mathbf{d} \f$, \f$ t \in \mathbf{R} \f$.
  *
  * \param _Scalar the scalar type, i.e., the type of the coefficients
  * \param _AmbientDim the dimension of the ambient space, can be a compile time value or Dynamic.
  */
template <typename _Scalar, int _AmbientDim, int _Options>
class ParametrizedLine
{
public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW_IF_VECTORIZABLE_FIXED_SIZE(_Scalar,_AmbientDim)
  enum {
    AmbientDimAtCompileTime = _AmbientDim,
    Options = _Options
  };
  typedef _Scalar Scalar;
  typedef typename NumTraits<Scalar>::Real RealScalar;
  typedef DenseIndex Index;
  typedef Matrix<Scalar,AmbientDimAtCompileTime,1,Options> VectorType;

  /** Default constructor without initialization */
  inline explicit ParametrizedLine() {}
  
  template<int OtherOptions>
  ParametrizedLine(const ParametrizedLine<Scalar,AmbientDimAtCompileTime,OtherOptions>& other)
   : m_origin(other.origin()), m_direction(other.direction())
  {}

  /** Constructs a dynamic-size line with \a _dim the dimension
    * of the ambient space */
  inline explicit ParametrizedLine(Index _dim) : m_origin(_dim), m_direction(_dim) {}

  /** Initializes a parametrized line of direction \a direction and origin \a origin.
    * \warning the vector direction is assumed to be normalized.
    */
  ParametrizedLine(const VectorType& origin, const VectorType& direction)
    : m_origin(origin), m_direction(direction) {}

  template <int OtherOptions>
  explicit ParametrizedLine(const Hyperplane<_Scalar, _AmbientDim, OtherOptions>& hyperplane);

  /** Constructs a parametrized line going from \a p0 to \a p1. */
  static inline ParametrizedLine Through(const VectorType& p0, const VectorType& p1)
  { return ParametrizedLine(p0, (p1-p0).normalized()); }

  ~ParametrizedLine() {}

  /** \returns the dimension in which the line holds */
  inline Index dim() const { return m_direction.size(); }

  const VectorType& origin() const { return m_origin; }
  VectorType& origin() { return m_origin; }

  const VectorType& direction() const { return m_direction; }
  VectorType& direction() { return m_direction; }

  /** \returns the squared distance of a point \a p to its projection onto the line \c *this.
    * \sa distance()
    */
  RealScalar squaredDistance(const VectorType& p) const
  {
    VectorType diff = p - origin();
    return (diff - direction().dot(diff) * direction()).squaredNorm();
  }
  /** \returns the distance of a point \a p to its projection onto the line \c *this.
    * \sa squaredDistance()
    */
  RealScalar distance(const VectorType& p) const { return internal::sqrt(squaredDistance(p)); }

  /** \returns the projection of a point \a p onto the line \c *this. */
  VectorType projection(const VectorType& p) const
  { return origin() + direction().dot(p-origin()) * direction(); }

  template <int OtherOptions>
  Scalar intersection(const Hyperplane<_Scalar, _AmbientDim, OtherOptions>& hyperplane) const;

  /** \returns \c *this with scalar type casted to \a NewScalarType
    *
    * Note that if \a NewScalarType is equal to the current scalar type of \c *this
    * then this function smartly returns a const reference to \c *this.
    */
  template<typename NewScalarType>
  inline typename internal::cast_return_type<ParametrizedLine,
           ParametrizedLine<NewScalarType,AmbientDimAtCompileTime,Options> >::type cast() const
  {
    return typename internal::cast_return_type<ParametrizedLine,
                    ParametrizedLine<NewScalarType,AmbientDimAtCompileTime,Options> >::type(*this);
  }

  /** Copy constructor with scalar type conversion */
  template<typename OtherScalarType,int OtherOptions>
  inline explicit ParametrizedLine(const ParametrizedLine<OtherScalarType,AmbientDimAtCompileTime,OtherOptions>& other)
  {
    m_origin = other.origin().template cast<Scalar>();
    m_direction = other.direction().template cast<Scalar>();
  }

  /** \returns \c true if \c *this is approximately equal to \a other, within the precision
    * determined by \a prec.
    *
    * \sa MatrixBase::isApprox() */
  bool isApprox(const ParametrizedLine& other, typename NumTraits<Scalar>::Real prec = NumTraits<Scalar>::dummy_precision()) const
  { return m_origin.isApprox(other.m_origin, prec) && m_direction.isApprox(other.m_direction, prec); }

protected:

  VectorType m_origin, m_direction;
};

/** Constructs a parametrized line from a 2D hyperplane
  *
  * \warning the ambient space must have dimension 2 such that the hyperplane actually describes a line
  */
template <typename _Scalar, int _AmbientDim, int _Options>
template <int OtherOptions>
inline ParametrizedLine<_Scalar, _AmbientDim,_Options>::ParametrizedLine(const Hyperplane<_Scalar, _AmbientDim,OtherOptions>& hyperplane)
{
  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(VectorType, 2)
  direction() = hyperplane.normal().unitOrthogonal();
  origin() = -hyperplane.normal()*hyperplane.offset();
}

/** \returns the parameter value of the intersection between \c *this and the given hyperplane
  */
template <typename _Scalar, int _AmbientDim, int _Options>
template <int OtherOptions>
inline _Scalar ParametrizedLine<_Scalar, _AmbientDim,_Options>::intersection(const Hyperplane<_Scalar, _AmbientDim, OtherOptions>& hyperplane) const
{
  return -(hyperplane.offset()+hyperplane.normal().dot(origin()))
          / hyperplane.normal().dot(direction());
}

#endif // EIGEN_PARAMETRIZEDLINE_H
