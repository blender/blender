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

#ifndef __FREESTYLE_PREDICATES_1D_H__
#define __FREESTYLE_PREDICATES_1D_H__

/** \file
 * \ingroup freestyle
 * \brief Class gathering stroke creation algorithms
 */

#include <string>

#include "AdvancedFunctions1D.h"

#include "../system/TimeStamp.h"

#include "../view_map/Interface1D.h"
#include "../view_map/Functions1D.h"

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

//
// UnaryPredicate1D (base class for predicates in 1D)
//
///////////////////////////////////////////////////////////

/*! Base class for Unary Predicates that work on Interface1D.
 *  A UnaryPredicate1D is a functor that evaluates a condition on a Interface1D and returns
 *  true or false depending on whether this condition is satisfied or not.
 *  The UnaryPredicate1D is used by calling its () operator.
 *  Any inherited class must overload the () operator.
 */
class UnaryPredicate1D {
 public:
  bool result;
  void *py_up1D;

  /*! Default constructor. */
  UnaryPredicate1D()
  {
    py_up1D = NULL;
  }

  /*! Destructor. */
  virtual ~UnaryPredicate1D()
  {
  }

  /*! Returns the string of the name of the UnaryPredicate1D. */
  virtual string getName() const
  {
    return "UnaryPredicate1D";
  }

  /*! The () operator. Must be overload by inherited classes.
   *  \param inter:
   *    The Interface1D on  which we wish to evaluate the predicate.
   *  \return true if the condition is satisfied, false otherwise.
   */
  virtual int operator()(Interface1D &inter);

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:UnaryPredicate1D")
#endif
};

//
// BinaryPredicate1D (base class for predicates in 1D)
//
///////////////////////////////////////////////////////////

/*! Base class for Binary Predicates working on Interface1D.
 *  A BinaryPredicate1D is typically an ordering relation between two Interface1D.
 *  It evaluates a relation between 2 Interface1D and returns true or false.
 *  It is used by calling the () operator.
 */
class BinaryPredicate1D {
 public:
  bool result;
  void *py_bp1D;

  /*! Default constructor. */
  BinaryPredicate1D()
  {
    py_bp1D = NULL;
  }

  /*! Destructor. */
  virtual ~BinaryPredicate1D()
  {
  }

  /*! Returns the string of the name of the binary predicate. */
  virtual string getName() const
  {
    return "BinaryPredicate1D";
  }

  /*! The () operator. Must be overload by inherited classes.
   *  It evaluates a relation between 2 Interface1D.
   *  \param inter1:
   *    The first Interface1D.
   *  \param inter2:
   *    The second Interface1D.
   *  \return true or false.
   */
  virtual int operator()(Interface1D &inter1, Interface1D &inter2);

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:BinaryPredicate1D")
#endif
};

//
// Predicates definitions
//
///////////////////////////////////////////////////////////

namespace Predicates1D {

// TrueUP1D
/*! Returns true */
class TrueUP1D : public UnaryPredicate1D {
 public:
  /*! Constructor */
  TrueUP1D()
  {
  }

  /*! Returns the string "TrueUP1D"*/
  string getName() const
  {
    return "TrueUP1D";
  }

  /*! the () operator */
  int operator()(Interface1D &)
  {
    result = true;
    return 0;
  }
};

// FalseUP1D
/*! Returns false */
class FalseUP1D : public UnaryPredicate1D {
 public:
  /*! Constructor */
  FalseUP1D()
  {
  }

  /*! Returns the string "FalseUP1D"*/
  string getName() const
  {
    return "FalseUP1D";
  }

  /*! the () operator */
  int operator()(Interface1D &)
  {
    result = false;
    return 0;
  }
};

// QuantitativeInvisibilityUP1D
/*! Returns true if the Quantitative Invisibility evaluated at an Interface1D, using the
 * QuantitativeInvisibilityF1D functor, equals a certain user-defined value.
 */
class QuantitativeInvisibilityUP1D : public UnaryPredicate1D {
 public:
  /*! Builds the Predicate.
   *  \param qi:
   *    The Quantitative Invisibility you want the Interface1D to have
   */
  QuantitativeInvisibilityUP1D(unsigned qi = 0) : _qi(qi)
  {
  }

  /*! Returns the string "QuantitativeInvisibilityUP1D" */
  string getName() const
  {
    return "QuantitativeInvisibilityUP1D";
  }

  /*! the () operator */
  int operator()(Interface1D &inter)
  {
    Functions1D::QuantitativeInvisibilityF1D func;
    if (func(inter) < 0)
      return -1;
    result = (func.result == _qi);
    return 0;
  }

 private:
  unsigned _qi;
};

// ContourUP1D
/*! Returns true if the Interface1D is a contour.
 *  An Interface1D is a contour if it is borded by a different shape on each of its sides.
 */
class ContourUP1D : public UnaryPredicate1D {
 private:
  Functions1D::CurveNatureF1D _getNature;

 public:
  /*! Returns the string "ContourUP1D"*/
  string getName() const
  {
    return "ContourUP1D";
  }

  /*! The () operator. */
  int operator()(Interface1D &inter)
  {
    if (_getNature(inter) < 0)
      return -1;
    if ((_getNature.result & Nature::SILHOUETTE) || (_getNature.result & Nature::BORDER)) {
      Interface0DIterator it = inter.verticesBegin();
      for (; !it.isEnd(); ++it) {
        if (Functions0D::getOccludeeF0D(it) != Functions0D::getShapeF0D(it)) {
          result = true;
          return 0;
        }
      }
    }
    result = false;
    return 0;
  }
};

// ExternalContourUP1D
/*! Returns true if the Interface1D is an external contour.
 *  An Interface1D is an external contour if it is borded by no shape on one of its sides.
 */
class ExternalContourUP1D : public UnaryPredicate1D {
 private:
  Functions1D::CurveNatureF1D _getNature;

 public:
  /*! Returns the string "ExternalContourUP1D" */
  string getName() const
  {
    return "ExternalContourUP1D";
  }

  /*! The () operator. */
  int operator()(Interface1D &inter)
  {
    if (_getNature(inter) < 0)
      return -1;
    if ((_getNature.result & Nature::SILHOUETTE) || (_getNature.result & Nature::BORDER)) {
      set<ViewShape *> occluded;
      Functions1D::getOccludeeF1D(inter, occluded);
      for (set<ViewShape *>::iterator os = occluded.begin(), osend = occluded.end(); os != osend;
           ++os) {
        if ((*os) == 0) {
          result = true;
          return 0;
        }
      }
    }
    result = false;
    return 0;
  }
};

// EqualToTimeStampUP1D
/*! Returns true if the Interface1D's time stamp is equal to a certain user-defined value. */
class EqualToTimeStampUP1D : public UnaryPredicate1D {
 protected:
  unsigned _timeStamp;

 public:
  EqualToTimeStampUP1D(unsigned ts) : UnaryPredicate1D()
  {
    _timeStamp = ts;
  }

  /*! Returns the string "EqualToTimeStampUP1D"*/
  string getName() const
  {
    return "EqualToTimeStampUP1D";
  }

  /*! The () operator. */
  int operator()(Interface1D &inter)
  {
    result = (inter.getTimeStamp() == _timeStamp);
    return 0;
  }
};

// EqualToChainingTimeStampUP1D
/*! Returns true if the Interface1D's time stamp is equal to a certain user-defined value. */
class EqualToChainingTimeStampUP1D : public UnaryPredicate1D {
 protected:
  unsigned _timeStamp;

 public:
  EqualToChainingTimeStampUP1D(unsigned ts) : UnaryPredicate1D()
  {
    _timeStamp = ts;
  }

  /*! Returns the string "EqualToChainingTimeStampUP1D"*/
  string getName() const
  {
    return "EqualToChainingTimeStampUP1D";
  }

  /*! The () operator. */
  int operator()(Interface1D &inter)
  {
    ViewEdge *edge = dynamic_cast<ViewEdge *>(&inter);
    if (!edge) {
      result = false;
      return 0;
    }
    result = (edge->getChainingTimeStamp() >= _timeStamp);
    return 0;
  }
};

// ShapeUP1D
/*! Returns true if the shape to which the Interface1D belongs to has the same Id as the one
 * specified by the user. */
class ShapeUP1D : public UnaryPredicate1D {
 private:
  Id _id;

 public:
  /*! Builds the Predicate.
   *  \param idFirst:
   *    The first Id component.
   *  \param idSecond:
   *    The second Id component.
   */
  ShapeUP1D(unsigned idFirst, unsigned idSecond = 0) : UnaryPredicate1D()
  {
    _id = Id(idFirst, idSecond);
  }

  /*! Returns the string "ShapeUP1D"*/
  string getName() const
  {
    return "ShapeUP1D";
  }

  /*! The () operator. */
  int operator()(Interface1D &inter)
  {
    set<ViewShape *> shapes;
    Functions1D::getShapeF1D(inter, shapes);
    for (set<ViewShape *>::iterator s = shapes.begin(), send = shapes.end(); s != send; ++s) {
      if ((*s)->getId() == _id) {
        result = true;
        return 0;
      }
    }
    result = false;
    return 0;
  }
};

// WithinImageBoundaryUP1D
/*! Returns true if the Interface1D is (partly) within the image boundary. */
class WithinImageBoundaryUP1D : public UnaryPredicate1D {
 private:
  real _xmin, _ymin, _xmax, _ymax;

 public:
  /*! Builds the Predicate.
   *  \param xmin:
   *    The X lower bound of the image boundary.
   *  \param ymin:
   *    The Y lower bound of the image boundary.
   *  \param xmax:
   *    The X upper bound of the image boundary.
   *  \param ymax:
   *    The Y upper bound of the image boundary.
   */
  WithinImageBoundaryUP1D(const real xmin, const real ymin, const real xmax, const real ymax)
      : _xmin(xmin), _ymin(ymin), _xmax(xmax), _ymax(ymax)
  {
  }

  /*! Returns the string "WithinImageBoundaryUP1D" */
  string getName() const
  {
    return "WithinImageBoundaryUP1D";
  }

  /*! The () operator. */
  int operator()(Interface1D &inter)
  {
    // 1st pass: check if a point is within the image boundary.
    Interface0DIterator it = inter.verticesBegin(), itend = inter.verticesEnd();
    for (; it != itend; ++it) {
      real x = (*it).getProjectedX();
      real y = (*it).getProjectedY();
      if (_xmin <= x && x <= _xmax && _ymin <= y && y <= _ymax) {
        result = true;
        return 0;
      }
    }
    // 2nd pass: check if a line segment intersects with the image boundary.
    it = inter.verticesBegin();
    if (it != itend) {
      Vec2r pmin(_xmin, _ymin);
      Vec2r pmax(_xmax, _ymax);
      Vec2r prev((*it).getPoint2D());
      ++it;
      for (; it != itend; ++it) {
        Vec2r p((*it).getPoint2D());
        if (GeomUtils::intersect2dSeg2dArea(pmin, pmax, prev, p)) {
          result = true;
          return 0;
        }
        prev = p;
      }
    }
    result = false;
    return 0;
  }
};

//
//   Binary Predicates definitions
//
///////////////////////////////////////////////////////////

// TrueBP1D
/*! Returns true. */
class TrueBP1D : public BinaryPredicate1D {
 public:
  /*! Returns the string "TrueBP1D" */
  string getName() const
  {
    return "TrueBP1D";
  }

  /*! The () operator. */
  int operator()(Interface1D & /*i1*/, Interface1D & /*i2*/)
  {
    result = true;
    return 0;
  }
};

// FalseBP1D
/*! Returns false. */
class FalseBP1D : public BinaryPredicate1D {
 public:
  /*! Returns the string "FalseBP1D" */
  string getName() const
  {
    return "FalseBP1D";
  }

  /*! The () operator. */
  int operator()(Interface1D & /*i1*/, Interface1D & /*i2*/)
  {
    result = false;
    return 0;
  }
};

// Length2DBP1D
/*! Returns true if the 2D length of the Interface1D i1 is less than the 2D length of the
 * Interface1D i2. */
class Length2DBP1D : public BinaryPredicate1D {
 public:
  /*! Returns the string "Length2DBP1D" */
  string getName() const
  {
    return "Length2DBP1D";
  }

  /*! The () operator. */
  int operator()(Interface1D &i1, Interface1D &i2)
  {
    result = (i1.getLength2D() > i2.getLength2D());
    return 0;
  }
};

// SameShapeIdBP1D
/*! Returns true if the Interface1D i1 and i2 belong to the same shape. */
class SameShapeIdBP1D : public BinaryPredicate1D {
 public:
  /*! Returns the string "SameShapeIdBP1D" */
  string getName() const
  {
    return "SameShapeIdBP1D";
  }

  /*! The () operator. */
  int operator()(Interface1D &i1, Interface1D &i2)
  {
    set<ViewShape *> shapes1;
    Functions1D::getShapeF1D(i1, shapes1);
    set<ViewShape *> shapes2;
    Functions1D::getShapeF1D(i2, shapes2);
    // FIXME:// n2 algo, can do better...
    for (set<ViewShape *>::iterator s = shapes1.begin(), send = shapes1.end(); s != send; ++s) {
      Id current = (*s)->getId();
      for (set<ViewShape *>::iterator s2 = shapes2.begin(), s2end = shapes2.end(); s2 != s2end;
           ++s2) {
        if ((*s2)->getId() == current) {
          result = true;
          return 0;
        }
      }
    }
    result = false;
    return 0;
  }
};

// ViewMapGradientNormBP1D
/*! Returns true if the evaluation of the Gradient norm Function is higher for Interface1D i1 than
 * for i2. */
class ViewMapGradientNormBP1D : public BinaryPredicate1D {
 private:
  Functions1D::GetViewMapGradientNormF1D _func;

 public:
  ViewMapGradientNormBP1D(int level, IntegrationType iType = MEAN, float sampling = 2.0)
      : BinaryPredicate1D(), _func(level, iType, sampling)
  {
  }

  /*! Returns the string "ViewMapGradientNormBP1D" */
  string getName() const
  {
    return "ViewMapGradientNormBP1D";
  }

  /*! The () operator. */
  int operator()(Interface1D &i1, Interface1D &i2)
  {
    if (_func(i1) < 0)
      return -1;
    real n1 = _func.result;
    if (_func(i2) < 0)
      return -1;
    real n2 = _func.result;
    result = (n1 > n2);
    return 0;
  }
};

}  // end of namespace Predicates1D

} /* namespace Freestyle */

#endif  // __FREESTYLE_PREDICATES_1D_H__
