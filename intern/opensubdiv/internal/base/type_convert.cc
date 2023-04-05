// Copyright 2018 Blender Foundation
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

#ifdef _MSC_VER
#  include <iso646.h>
#endif

#include "internal/base/type_convert.h"

#include <cassert>
#include <opensubdiv/sdc/crease.h>

namespace blender {
namespace opensubdiv {

OpenSubdiv::Sdc::SchemeType getSchemeTypeFromCAPI(OpenSubdiv_SchemeType type)
{
  switch (type) {
    case OSD_SCHEME_BILINEAR:
      return OpenSubdiv::Sdc::SCHEME_BILINEAR;
    case OSD_SCHEME_CATMARK:
      return OpenSubdiv::Sdc::SCHEME_CATMARK;
    case OSD_SCHEME_LOOP:
      return OpenSubdiv::Sdc::SCHEME_LOOP;
  }
  assert(!"Unknown scheme type passed via C-API");
  return OpenSubdiv::Sdc::SCHEME_CATMARK;
}

OpenSubdiv::Sdc::Options::FVarLinearInterpolation getFVarLinearInterpolationFromCAPI(
    OpenSubdiv_FVarLinearInterpolation linear_interpolation)
{
  typedef OpenSubdiv::Sdc::Options Options;
  switch (linear_interpolation) {
    case OSD_FVAR_LINEAR_INTERPOLATION_NONE:
      return Options::FVAR_LINEAR_NONE;
    case OSD_FVAR_LINEAR_INTERPOLATION_CORNERS_ONLY:
      return Options::FVAR_LINEAR_CORNERS_ONLY;
    case OSD_FVAR_LINEAR_INTERPOLATION_CORNERS_PLUS1:
      return Options::FVAR_LINEAR_CORNERS_PLUS1;
    case OSD_FVAR_LINEAR_INTERPOLATION_CORNERS_PLUS2:
      return Options::FVAR_LINEAR_CORNERS_PLUS2;
    case OSD_FVAR_LINEAR_INTERPOLATION_BOUNDARIES:
      return Options::FVAR_LINEAR_BOUNDARIES;
    case OSD_FVAR_LINEAR_INTERPOLATION_ALL:
      return Options::FVAR_LINEAR_ALL;
  }
  assert(!"Unknown fvar linear interpolation passed via C-API");
  return Options::FVAR_LINEAR_NONE;
}

OpenSubdiv_FVarLinearInterpolation getCAPIFVarLinearInterpolationFromOSD(
    OpenSubdiv::Sdc::Options::FVarLinearInterpolation linear_interpolation)
{
  typedef OpenSubdiv::Sdc::Options Options;
  switch (linear_interpolation) {
    case Options::FVAR_LINEAR_NONE:
      return OSD_FVAR_LINEAR_INTERPOLATION_NONE;
    case Options::FVAR_LINEAR_CORNERS_ONLY:
      return OSD_FVAR_LINEAR_INTERPOLATION_CORNERS_ONLY;
    case Options::FVAR_LINEAR_CORNERS_PLUS1:
      return OSD_FVAR_LINEAR_INTERPOLATION_CORNERS_PLUS1;
    case Options::FVAR_LINEAR_CORNERS_PLUS2:
      return OSD_FVAR_LINEAR_INTERPOLATION_CORNERS_PLUS2;
    case Options::FVAR_LINEAR_BOUNDARIES:
      return OSD_FVAR_LINEAR_INTERPOLATION_BOUNDARIES;
    case Options::FVAR_LINEAR_ALL:
      return OSD_FVAR_LINEAR_INTERPOLATION_ALL;
  }
  assert(!"Unknown fvar linear interpolation passed via C-API");
  return OSD_FVAR_LINEAR_INTERPOLATION_NONE;
}

OpenSubdiv::Sdc::Options::VtxBoundaryInterpolation getVtxBoundaryInterpolationFromCAPI(
    OpenSubdiv_VtxBoundaryInterpolation boundary_interpolation)
{
  using OpenSubdiv::Sdc::Options;

  switch (boundary_interpolation) {
    case OSD_VTX_BOUNDARY_NONE:
      return Options::VTX_BOUNDARY_NONE;
    case OSD_VTX_BOUNDARY_EDGE_ONLY:
      return Options::VTX_BOUNDARY_EDGE_ONLY;
    case OSD_VTX_BOUNDARY_EDGE_AND_CORNER:
      return Options::VTX_BOUNDARY_EDGE_AND_CORNER;
  }
  assert(!"Unknown veretx boundary interpolation.");
  return Options::VTX_BOUNDARY_EDGE_ONLY;
}

}  // namespace opensubdiv
}  // namespace blender
