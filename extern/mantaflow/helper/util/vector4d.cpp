/******************************************************************************
 *
 * MantaFlow fluid solver framework
 * Copyright 2011 Tobias Pfaff, Nils Thuerey
 *
 * This program is free software, distributed under the terms of the
 * Apache License, Version 2.0
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Basic vector class
 *
 ******************************************************************************/

#include "vector4d.h"

using namespace std;

namespace Manta {

template<> const Vector4D<int> Vector4D<int>::Zero(0, 0, 0, 0);
template<> const Vector4D<float> Vector4D<float>::Zero(0.f, 0.f, 0.f, 0.f);
template<> const Vector4D<double> Vector4D<double>::Zero(0., 0., 0., 0.);
template<>
const Vector4D<float> Vector4D<float>::Invalid(numeric_limits<float>::quiet_NaN(),
                                               numeric_limits<float>::quiet_NaN(),
                                               numeric_limits<float>::quiet_NaN(),
                                               numeric_limits<float>::quiet_NaN());
template<>
const Vector4D<double> Vector4D<double>::Invalid(numeric_limits<double>::quiet_NaN(),
                                                 numeric_limits<double>::quiet_NaN(),
                                                 numeric_limits<double>::quiet_NaN(),
                                                 numeric_limits<double>::quiet_NaN());
template<> bool Vector4D<float>::isValid() const
{
  return !c_isnan(x) && !c_isnan(y) && !c_isnan(z) && !c_isnan(t);
}
template<> bool Vector4D<double>::isValid() const
{
  return !c_isnan(x) && !c_isnan(y) && !c_isnan(z) && !c_isnan(t);
}

//! Specialization for readable ints
template<> std::string Vector4D<int>::toString() const
{
  char buf[256];
  snprintf(buf, 256, "[%d,%d,%d,%d]", (*this)[0], (*this)[1], (*this)[2], (*this)[3]);
  return std::string(buf);
}

}  // namespace Manta
