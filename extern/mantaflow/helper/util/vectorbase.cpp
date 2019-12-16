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

#include "vectorbase.h"

using namespace std;

namespace Manta {

template<> const Vector3D<int> Vector3D<int>::Zero(0, 0, 0);
template<> const Vector3D<float> Vector3D<float>::Zero(0.f, 0.f, 0.f);
template<> const Vector3D<double> Vector3D<double>::Zero(0., 0., 0.);
template<>
const Vector3D<float> Vector3D<float>::Invalid(numeric_limits<float>::quiet_NaN(),
                                               numeric_limits<float>::quiet_NaN(),
                                               numeric_limits<float>::quiet_NaN());
template<>
const Vector3D<double> Vector3D<double>::Invalid(numeric_limits<double>::quiet_NaN(),
                                                 numeric_limits<double>::quiet_NaN(),
                                                 numeric_limits<double>::quiet_NaN());

template<> bool Vector3D<float>::isValid() const
{
  return !c_isnan(x) && !c_isnan(y) && !c_isnan(z);
}
template<> bool Vector3D<double>::isValid() const
{
  return !c_isnan(x) && !c_isnan(y) && !c_isnan(z);
}

//! Specialization for readable ints
template<> std::string Vector3D<int>::toString() const
{
  char buf[256];
  snprintf(buf, 256, "[%d,%d,%d]", (*this)[0], (*this)[1], (*this)[2]);
  return std::string(buf);
}

}  // namespace Manta
