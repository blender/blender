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

#ifndef __FREESTYLE_ADVANCED_FUNCTIONS_1D_H__
#define __FREESTYLE_ADVANCED_FUNCTIONS_1D_H__

/** \file
 * \ingroup freestyle
 * \brief Functions taking 1D input
 */

#include "AdvancedFunctions0D.h"

#include "../view_map/Functions1D.h"

//
// Functions definitions
//
///////////////////////////////////////////////////////////

namespace Freestyle {

namespace Functions1D {

// DensityF1D
/*! Returns the density evaluated for an Interface1D.
 *  The density is evaluated for a set of points along the Interface1D (using the DensityF0D
 * functor) with a user-defined sampling and then integrated into a single value using a
 * user-defined integration method.
 */
class DensityF1D : public UnaryFunction1D<double> {
 private:
  float _sampling;

 public:
  /*! Builds the functor.
   *  \param sigma:
   *    Thesigma used in DensityF0D and determining the window size used in each density query.
   *  \param iType:
   *    The integration method used to compute a single value from a set of values.
   *  \param sampling:
   *    The resolution used to sample the chain: the corresponding 0D function is evaluated at each
   * sample point and the result is obtained by combining the resulting values into a single one,
   * following the method specified by iType.
   */
  DensityF1D(double sigma = 2, IntegrationType iType = MEAN, float sampling = 2.0f)
      : UnaryFunction1D<double>(iType), _fun(sigma)
  {
    _sampling = sampling;
  }

  /*! Destructor */
  virtual ~DensityF1D()
  {
  }

  /*! Returns the string "DensityF1D"*/
  string getName() const
  {
    return "DensityF1D";
  }

  /*! the () operator.*/
  int operator()(Interface1D &inter)
  {
    result = integrate(
        _fun, inter.pointsBegin(_sampling), inter.pointsEnd(_sampling), _integration);
    return 0;
  }

 private:
  Functions0D::DensityF0D _fun;
};

// LocalAverageDepthF1D
/*! Returns the average depth evaluated for an Interface1D.
 *  The average depth is evaluated for a set of points along the Interface1D (using the
 * LocalAverageDepthF0D functor) with a user-defined sampling and then integrated into a single
 * value using a user-defined integration method.
 */
class LocalAverageDepthF1D : public UnaryFunction1D<double> {
 public:
  /*! Builds the functor.
   *  \param sigma:
   *    The sigma used in DensityF0D and determining the window size used in each density query.
   *  \param iType:
   *    The integration method used to compute a single value from a set of values.
   */
  LocalAverageDepthF1D(real sigma, IntegrationType iType = MEAN)
      : UnaryFunction1D<double>(iType), _fun(sigma)
  {
  }

  /*! Returns the string "LocalAverageDepthF1D" */
  string getName() const
  {
    return "LocalAverageDepthF1D";
  }

  /*! the () operator. */
  int operator()(Interface1D &inter)
  {
    result = integrate(_fun, inter.verticesBegin(), inter.verticesEnd(), _integration);
    return 0;
  }

 private:
  Functions0D::LocalAverageDepthF0D _fun;
};

// GetCompleteViewMapDensity
/*! Returns the density evaluated for an Interface1D in the complete viewmap image.
 *  The density is evaluated for a set of points along the Interface1D (using the
 * ReadCompleteViewMapPixelF0D functor) and then integrated into a single value using a
 * user-defined integration method.
 */
class GetCompleteViewMapDensityF1D : public UnaryFunction1D<double> {
 public:
  /*! Builds the functor.
   *  \param level:
   *    The level of the pyramid from which
   *    the pixel must be read.
   *  \param iType:
   *    The integration method used to compute
   *    a single value from a set of values.
   *  \param sampling:
   *    The resolution used to sample the chain: the corresponding 0D function
   *    is evaluated at each sample point and the result is obtained by
   *    combining the resulting values into a single one, following the
   *    method specified by iType.
   */
  GetCompleteViewMapDensityF1D(unsigned level, IntegrationType iType = MEAN, float sampling = 2.0f)
      : UnaryFunction1D<double>(iType), _fun(level)
  {
    _sampling = sampling;
  }

  /*! Returns the string "GetCompleteViewMapDensityF1D" */
  string getName() const
  {
    return "GetCompleteViewMapDensityF1D";
  }

  /*! the () operator. */
  int operator()(Interface1D &inter);

 private:
  Functions0D::ReadCompleteViewMapPixelF0D _fun;
  float _sampling;
};

// GetDirectionalViewMapDensity
/*! Returns the density evaluated for an Interface1D in of the steerable viewmaps image.
 *  The direction telling which Directional map to choose is explicitly specified by the user.
 *  The density is evaluated for a set of points along the Interface1D
 *  (using the ReadSteerableViewMapPixelF0D functor)
 *  and then integrated into a single value using a user-defined integration method.
 */
class GetDirectionalViewMapDensityF1D : public UnaryFunction1D<double> {
 public:
  /*! Builds the functor.
   *  \param iOrientation:
   *    The number of the directional map we must work with.
   *  \param level:
   *    The level of the pyramid from which the pixel must be read.
   *  \param iType:
   *    The integration method used to compute a single value from a set of values.
   *  \param sampling:
   *    The resolution used to sample the chain: the corresponding 0D function is evaluated at
   *    each sample point and the result is obtained by combining the resulting values into a
   *    single one, following the method specified by iType.
   */
  GetDirectionalViewMapDensityF1D(unsigned iOrientation,
                                  unsigned level,
                                  IntegrationType iType = MEAN,
                                  float sampling = 2.0f)
      : UnaryFunction1D<double>(iType), _fun(iOrientation, level)
  {
    _sampling = sampling;
  }

  /*! Returns the string "GetDirectionalViewMapDensityF1D" */
  string getName() const
  {
    return "GetDirectionalViewMapDensityF1D";
  }

  /*! the () operator. */
  int operator()(Interface1D &inter);

 private:
  Functions0D::ReadSteerableViewMapPixelF0D _fun;
  float _sampling;
};

// GetSteerableViewMapDensityF1D
/*! Returns the density of the viewmap for a given Interface1D. The density of each FEdge is
 * evaluated in the proper steerable ViewMap depending on its oorientation.
 */
class GetSteerableViewMapDensityF1D : public UnaryFunction1D<double> {
 private:
  int _level;
  float _sampling;

 public:
  /*! Builds the functor from the level of the pyramid from which the pixel must be read.
   *  \param level:
   *    The level of the pyramid from which the pixel must be read.
   *  \param iType:
   *    The integration method used to compute a single value from a set of values.
   *  \param sampling:
   *    The resolution used to sample the chain: the corresponding 0D function is evaluated at each
   * sample point and the result is obtained by combining the resulting values into a single one,
   * following the method specified by iType.
   */
  GetSteerableViewMapDensityF1D(int level, IntegrationType iType = MEAN, float sampling = 2.0f)
      : UnaryFunction1D<double>(iType)
  {
    _level = level;
    _sampling = sampling;
  }

  /*! Destructor */
  virtual ~GetSteerableViewMapDensityF1D()
  {
  }

  /*! Returns the string "GetSteerableViewMapDensityF1D" */
  string getName() const
  {
    return "GetSteerableViewMapDensityF1D";
  }

  /*! the () operator. */
  int operator()(Interface1D &inter);
};

// GetViewMapGradientNormF1D
/*! Returns the density of the viewmap for a given Interface1D. The density of each FEdge is
 * evaluated in the proper steerable ViewMap depending on its oorientation.
 */
class GetViewMapGradientNormF1D : public UnaryFunction1D<double> {
 private:
  int _level;
  float _sampling;
  Functions0D::GetViewMapGradientNormF0D _func;

 public:
  /*! Builds the functor from the level of the pyramid from which the pixel must be read.
   *  \param level:
   *    The level of the pyramid from which the pixel must be read.
   *  \param iType:
   *    The integration method used to compute a single value from a set of values.
   *  \param sampling:
   *    The resolution used to sample the chain: the corresponding 0D function is evaluated at each
   *    sample point and the result is obtained by combining the resulting values into a single
   *    one, following the method specified by iType.
   */
  GetViewMapGradientNormF1D(int level, IntegrationType iType = MEAN, float sampling = 2.0f)
      : UnaryFunction1D<double>(iType), _func(level)
  {
    _level = level;
    _sampling = sampling;
  }

  /*! Returns the string "GetSteerableViewMapDensityF1D" */
  string getName() const
  {
    return "GetViewMapGradientNormF1D";
  }

  /*! the () operator. */
  int operator()(Interface1D &inter);
};

}  // end of namespace Functions1D

} /* namespace Freestyle */

#endif  // __FREESTYLE_ADVANCED_FUNCTIONS_1D_H__
