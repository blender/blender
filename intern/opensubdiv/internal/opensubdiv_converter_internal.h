// Copyright 2018 Blender Foundation. All rights reserved.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
// Author: Sergey Sharybin

#ifndef OPENSUBDIV_CONVERTER_INTERNAL_H_
#define OPENSUBDIV_CONVERTER_INTERNAL_H_

#ifdef _MSC_VER
#  include <iso646.h>
#endif

#include <opensubdiv/sdc/options.h>
#include <opensubdiv/sdc/types.h>

#include "opensubdiv_converter_capi.h"

struct OpenSubdiv_Converter;

namespace opensubdiv_capi {

// Convert scheme type from C-API enum to an OpenSubdiv native enum.
OpenSubdiv::Sdc::SchemeType getSchemeTypeFromCAPI(OpenSubdiv_SchemeType type);

// Convert face-varying interpolation type from C-API to an OpenSubdiv
// native enum.
OpenSubdiv::Sdc::Options::FVarLinearInterpolation
getFVarLinearInterpolationFromCAPI(
    OpenSubdiv_FVarLinearInterpolation linear_interpolation);

// Similar to above, just other way around.
OpenSubdiv_FVarLinearInterpolation
getCAPIFVarLinearInterpolationFromOSD(
     OpenSubdiv::Sdc::Options::FVarLinearInterpolation linear_interpolation);

}  // namespace opensubdiv_capi

#endif  // OPENSUBDIV_CONVERTER_INTERNAL_H_
