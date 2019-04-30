/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __FREESTYLE_FUNCTIONS_1D_H__
#define __FREESTYLE_FUNCTIONS_1D_H__

/** \file
 * \ingroup freestyle
 * \brief Functions taking 1D input
 */

#include "Functions0D.h"
#include "Interface1D.h"
#include "ViewMap.h"

#include "../system/FreestyleConfig.h"
#include "../system/Precision.h"
#include "../system/TimeStamp.h"

#include "../python/Director.h"

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

//
// UnaryFunction1D (base class for functions in 1D)
//
///////////////////////////////////////////////////////////

/*! Base class for Unary Functions (functors) working on Interface1D.
 *  A unary function will be used by calling its operator() on an Interface1D.
 * \attention In the scripting language, there exists several prototypes depending on the returned
 * value type. For example, you would inherit from a UnaryFunction1DDouble if you wish to define a
 * function that returns a double. The different existing prototypes are:
 *    - UnaryFunction1DDouble
 *    - UnaryFunction1DEdgeNature
 *    - UnaryFunction1DFloat
 *    - UnaryFunction1DUnsigned
 *    - UnaryFunction1DVec2f
 *    - UnaryFunction1DVec3f
 *    - UnaryFunction1DVectorViewShape
 *    - UnaryFunction1DVoid
 */
template<class T> class UnaryFunction1D {
 public:
  T result;
  void *py_uf1D;

  /*! The type of the value returned by the functor. */
  typedef T ReturnedValueType;

  /*! Default constructor */
  UnaryFunction1D()
  {
    _integration = MEAN;
  }

  /*! Builds a UnaryFunction1D from an integration type.
   *  \param iType:
   *    In case the result for the Interface1D would be obtained by evaluating a 0D function over
   * the different Interface0D of the Interface1D, \a iType tells which integration method to use.
   *    The default integration method is the MEAN.
   */
  UnaryFunction1D(IntegrationType iType)
  {
    _integration = iType;
  }

  /*! destructor. */
  virtual ~UnaryFunction1D()
  {
  }

  /*! returns the string "UnaryFunction1D". */
  virtual string getName() const
  {
    return "UnaryFunction1D";
  }

  /*! The operator ().
   *  \param inter:
   *    The Interface1D on which we wish to evaluate the function.
   *  \return the result of the function of type T.
   */
  /* FIXME move the implementation to Functions1D.cpp */
  virtual int operator()(Interface1D &inter)
  {
    return Director_BPy_UnaryFunction1D___call__(this, py_uf1D, inter);
  }

  /*! Sets the integration method */
  void setIntegrationType(IntegrationType integration)
  {
    _integration = integration;
  }

  /*! Returns the integration method. */
  IntegrationType getIntegrationType() const
  {
    return _integration;
  }

 protected:
  IntegrationType _integration;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:UnaryFunction1D")
#endif
};

class UnaryFunction1D_void {
 public:
  void *py_uf1D;

  UnaryFunction1D_void()
  {
    _integration = MEAN;
  }

  UnaryFunction1D_void(IntegrationType iType)
  {
    _integration = iType;
  }

  virtual ~UnaryFunction1D_void()
  {
  }

  virtual string getName() const
  {
    return "UnaryFunction1D_void";
  }

  /* FIXME move the implementation to Functions1D.cpp */
  int operator()(Interface1D &inter)
  {
    return Director_BPy_UnaryFunction1D___call__(this, py_uf1D, inter);
  }

  void setIntegrationType(IntegrationType integration)
  {
    _integration = integration;
  }

  IntegrationType getIntegrationType() const
  {
    return _integration;
  }

 protected:
  IntegrationType _integration;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:UnaryFunction1D_void")
#endif
};

//
// Functions definitions
//
///////////////////////////////////////////////////////////

namespace Functions1D {

// GetXF1D
/*! Returns the X 3D coordinate of an Interface1D. */
class GetXF1D : public UnaryFunction1D<double> {
 private:
  Functions0D::GetXF0D _func;

 public:
  /*! Builds the functor.
   *  \param iType:
   *    The integration method used to compute a single value from a set of values.
   */
  GetXF1D(IntegrationType iType) : UnaryFunction1D<double>(iType)
  {
  }

  /*! Returns the string "GetXF1D" */
  string getName() const
  {
    return "GetXF1D";
  }

  /*! the () operator. */
  int operator()(Interface1D &inter);
};

// GetYF1D
/*! Returns the Y 3D coordinate of an Interface1D. */
class GetYF1D : public UnaryFunction1D<double> {
 private:
  Functions0D::GetYF0D _func;

 public:
  /*! Builds the functor.
   *  \param iType:
   *    The integration method used to compute a single value from a set of values.
   */
  GetYF1D(IntegrationType iType = MEAN) : UnaryFunction1D<double>(iType)
  {
  }

  /*! Returns the string "GetYF1D" */
  string getName() const
  {
    return "GetYF1D";
  }

  /*! the () operator. */
  int operator()(Interface1D &inter);
};

// GetZF1D
/*! Returns the Z 3D coordinate of an Interface1D. */
class GetZF1D : public UnaryFunction1D<double> {
 private:
  Functions0D::GetZF0D _func;

 public:
  /*! Builds the functor.
   *  \param iType:
   *    The integration method used to compute a single value from a set of values.
   */
  GetZF1D(IntegrationType iType = MEAN) : UnaryFunction1D<double>(iType)
  {
  }

  /*! Returns the string "GetZF1D" */
  string getName() const
  {
    return "GetZF1D";
  }

  /*! the () operator. */
  int operator()(Interface1D &inter);
};

// GetProjectedXF1D
/*! Returns the projected X 3D coordinate of an Interface1D. */
class GetProjectedXF1D : public UnaryFunction1D<double> {
 private:
  Functions0D::GetProjectedXF0D _func;

 public:
  /*! Builds the functor.
   *  \param iType:
   *    The integration method used to compute a single value from a set of values.
   */
  GetProjectedXF1D(IntegrationType iType = MEAN) : UnaryFunction1D<double>(iType)
  {
  }

  /*! Returns the string "GetProjectedXF1D" */
  string getName() const
  {
    return "GetProjectedXF1D";
  }

  /*! the () operator. */
  int operator()(Interface1D &inter);
};

// GetProjectedYF1D
/*! Returns the projected Y 3D coordinate of an Interface1D. */
class GetProjectedYF1D : public UnaryFunction1D<double> {
 private:
  Functions0D::GetProjectedYF0D _func;

 public:
  /*! Builds the functor.
   *  \param iType:
   *    The integration method used to compute a single value from a set of values.
   */
  GetProjectedYF1D(IntegrationType iType = MEAN) : UnaryFunction1D<double>(iType)
  {
  }

  /*! Returns the string "GetProjectedYF1D" */
  string getName() const
  {
    return "GetProjectedYF1D";
  }

  /*! the () operator. */
  int operator()(Interface1D &inter);
};

// GetProjectedZF1D
/*! Returns the projected Z 3D coordinate of an Interface1D. */
class GetProjectedZF1D : public UnaryFunction1D<double> {
 private:
  Functions0D::GetProjectedZF0D _func;

 public:
  /*! Builds the functor.
   *  \param iType:
   *    The integration method used to compute a single value from a set of values.
   */
  GetProjectedZF1D(IntegrationType iType = MEAN) : UnaryFunction1D<double>(iType)
  {
  }

  /*! Returns the string "GetProjectedZF1D" */
  string getName() const
  {
    return "GetProjectedZF1D";
  }

  /*! the () operator. */
  int operator()(Interface1D &inter);
};

// Orientation2DF1D
/*! Returns the 2D orientation as a Vec2f*/
class Orientation2DF1D : public UnaryFunction1D<Vec2f> {
 private:
  Functions0D::VertexOrientation2DF0D _func;

 public:
  /*! Builds the functor.
   *  \param iType:
   *    The integration method used to compute a single value from a set of values.
   */
  Orientation2DF1D(IntegrationType iType = MEAN) : UnaryFunction1D<Vec2f>(iType)
  {
  }

  /*! Returns the string "Orientation2DF1D" */
  string getName() const
  {
    return "Orientation2DF1D";
  }

  /*! the () operator. */
  int operator()(Interface1D &inter);
};

// Orientation3DF1D
/*! Returns the 3D orientation as a Vec3f. */
class Orientation3DF1D : public UnaryFunction1D<Vec3f> {
 private:
  Functions0D::VertexOrientation3DF0D _func;

 public:
  /*! Builds the functor.
   *  \param iType:
   *    The integration method used to compute a single value from a set of values.
   */
  Orientation3DF1D(IntegrationType iType = MEAN) : UnaryFunction1D<Vec3f>(iType)
  {
  }

  /*! Returns the string "Orientation3DF1D" */
  string getName() const
  {
    return "Orientation3DF1D";
  }

  /*! the () operator. */
  int operator()(Interface1D &inter);
};

// ZDiscontinuityF1D
/*! Returns a real giving the distance between and Interface1D and the shape that lies behind
 * (occludee). This distance is evaluated in the camera space and normalized between 0 and 1.
 * Therefore, if no object is occluded by the shape to which the Interface1D belongs to, 1 is
 * returned.
 */
class ZDiscontinuityF1D : public UnaryFunction1D<double> {
 private:
  Functions0D::ZDiscontinuityF0D _func;

 public:
  /*! Builds the functor.
   *  \param iType:
   *    The integration method used to compute a single value from a set of values.
   */
  ZDiscontinuityF1D(IntegrationType iType = MEAN) : UnaryFunction1D<double>(iType)
  {
  }

  /*! Returns the string "ZDiscontinuityF1D" */
  string getName() const
  {
    return "ZDiscontinuityF1D";
  }

  /*! the () operator. */
  int operator()(Interface1D &inter);
};

// QuantitativeInvisibilityF1D
/*! Returns the Quantitative Invisibility of an Interface1D element.
 *  If the Interface1D is a ViewEdge, then there is no ambiguity concerning the result. But, if the
 * Interface1D results of a chaining (chain, stroke), then it might be made of several 1D elements
 * of different Quantitative Invisibilities.
 */
class QuantitativeInvisibilityF1D : public UnaryFunction1D<unsigned> {
 private:
  Functions0D::QuantitativeInvisibilityF0D _func;

 public:
  /*! Builds the functor.
   *  \param iType:
   *    The integration method used to compute a single value from a set of values.
   */
  QuantitativeInvisibilityF1D(IntegrationType iType = MEAN) : UnaryFunction1D<unsigned int>(iType)
  {
  }

  /*! Returns the string "QuantitativeInvisibilityF1D" */
  string getName() const
  {
    return "QuantitativeInvisibilityF1D";
  }

  /*! the () operator. */
  int operator()(Interface1D &inter);
};

// CurveNatureF1D
/*! Returns the nature of the Interface1D (silhouette, ridge, crease...).
 *  Except if the Interface1D is a ViewEdge, this result might be ambiguous.
 *  Indeed, the Interface1D might result from the gathering of several 1D elements, each one being
 * of a different nature. An integration method, such as the MEAN, might give, in this case,
 * irrelevant results.
 */
class CurveNatureF1D : public UnaryFunction1D<Nature::EdgeNature> {
 private:
  Functions0D::CurveNatureF0D _func;

 public:
  /*! Builds the functor.
   *  \param iType:
   *    The integration method used to compute a single value from a set of values.
   */
  CurveNatureF1D(IntegrationType iType = MEAN) : UnaryFunction1D<Nature::EdgeNature>(iType)
  {
  }

  /*! Returns the string "CurveNatureF1D" */
  string getName() const
  {
    return "CurveNatureF1D";
  }

  /*! the () operator. */
  int operator()(Interface1D &inter);
};

// TimeStampF1D
/*! Returns the time stamp of the Interface1D. */
class TimeStampF1D : public UnaryFunction1D_void {
 public:
  /*! Returns the string "TimeStampF1D" */
  string getName() const
  {
    return "TimeStampF1D";
  }

  /*! the () operator. */
  int operator()(Interface1D &inter);
};

// IncrementChainingTimeStampF1D
/*! Increments the chaining time stamp of the Interface1D. */
class IncrementChainingTimeStampF1D : public UnaryFunction1D_void {
 public:
  /*! Returns the string "IncrementChainingTimeStampF1D" */
  string getName() const
  {
    return "IncrementChainingTimeStampF1D";
  }

  /*! the () operator. */
  int operator()(Interface1D &inter);
};

// ChainingTimeStampF1D
/*! Sets the chaining time stamp of the Interface1D. */
class ChainingTimeStampF1D : public UnaryFunction1D_void {
 public:
  /*! Returns the string "ChainingTimeStampF1D" */
  string getName() const
  {
    return "ChainingTimeStampF1D";
  }

  /*! the () operator. */
  int operator()(Interface1D &inter);
};

// Curvature2DAngleF1D
/*! Returns the 2D curvature as an angle for an Interface1D. */
class Curvature2DAngleF1D : public UnaryFunction1D<double> {
 public:
  /*! Builds the functor.
   *  \param iType:
   *    The integration method used to compute a single value from a set of values.
   */
  Curvature2DAngleF1D(IntegrationType iType = MEAN) : UnaryFunction1D<double>(iType)
  {
  }

  /*! Returns the string "Curvature2DAngleF1D" */
  string getName() const
  {
    return "Curvature2DAngleF1D";
  }

  /*! the () operator.*/
  int operator()(Interface1D &inter)
  {
    result = integrate(_fun, inter.verticesBegin(), inter.verticesEnd(), _integration);
    return 0;
  }

 private:
  Functions0D::Curvature2DAngleF0D _fun;
};

// Normal2DF1D
/*! Returns the 2D normal for an interface 1D. */
class Normal2DF1D : public UnaryFunction1D<Vec2f> {
 public:
  /*! Builds the functor.
   *  \param iType:
   *    The integration method used to compute a single value from a set of values.
   */
  Normal2DF1D(IntegrationType iType = MEAN) : UnaryFunction1D<Vec2f>(iType)
  {
  }

  /*! Returns the string "Normal2DF1D" */
  string getName() const
  {
    return "Normal2DF1D";
  }

  /*! the () operator.*/
  int operator()(Interface1D &inter)
  {
    result = integrate(_fun, inter.verticesBegin(), inter.verticesEnd(), _integration);
    return 0;
  }

 private:
  Functions0D::Normal2DF0D _fun;
};

// GetShapeF1D
/*! Returns list of shapes covered by this Interface1D. */
class GetShapeF1D : public UnaryFunction1D<std::vector<ViewShape *>> {
 public:
  /*! Builds the functor. */
  GetShapeF1D() : UnaryFunction1D<std::vector<ViewShape *>>()
  {
  }

  /*! Returns the string "GetShapeF1D" */
  string getName() const
  {
    return "GetShapeF1D";
  }

  /*! the () operator. */
  int operator()(Interface1D &inter);
};

// GetOccludersF1D
/*! Returns list of occluding shapes covered by this Interface1D. */
class GetOccludersF1D : public UnaryFunction1D<std::vector<ViewShape *>> {
 public:
  /*! Builds the functor. */
  GetOccludersF1D() : UnaryFunction1D<std::vector<ViewShape *>>()
  {
  }

  /*! Returns the string "GetOccludersF1D" */
  string getName() const
  {
    return "GetOccludersF1D";
  }

  /*! the () operator. */
  int operator()(Interface1D &inter);
};

// GetOccludeeF1D
/*! Returns list of occluded shapes covered by this Interface1D. */
class GetOccludeeF1D : public UnaryFunction1D<std::vector<ViewShape *>> {
 public:
  /*! Builds the functor. */
  GetOccludeeF1D() : UnaryFunction1D<std::vector<ViewShape *>>()
  {
  }

  /*! Returns the string "GetOccludeeF1D" */
  string getName() const
  {
    return "GetOccludeeF1D";
  }

  /*! the () operator. */
  int operator()(Interface1D &inter);
};

// internal
////////////

// getOccludeeF1D
void getOccludeeF1D(Interface1D &inter, set<ViewShape *> &oShapes);

// getOccludersF1D
void getOccludersF1D(Interface1D &inter, set<ViewShape *> &oShapes);

// getShapeF1D
void getShapeF1D(Interface1D &inter, set<ViewShape *> &oShapes);

}  // end of namespace Functions1D

} /* namespace Freestyle */

#endif  // __FREESTYLE_FUNCTIONS_1D_H__
