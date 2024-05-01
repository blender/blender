/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "subdiv_converter.hh"

#include "opensubdiv_converter_capi.hh"

namespace blender::bke::subdiv {

void converter_free(OpenSubdiv_Converter *converter)
{
  if (converter->freeUserData) {
    converter->freeUserData(converter);
  }
}

int converter_vtx_boundary_interpolation_from_settings(const Settings *settings)
{
  switch (settings->vtx_boundary_interpolation) {
    case SUBDIV_VTX_BOUNDARY_NONE:
      return OSD_VTX_BOUNDARY_NONE;
    case SUBDIV_VTX_BOUNDARY_EDGE_ONLY:
      return OSD_VTX_BOUNDARY_EDGE_ONLY;
    case SUBDIV_VTX_BOUNDARY_EDGE_AND_CORNER:
      return OSD_VTX_BOUNDARY_EDGE_AND_CORNER;
  }
  BLI_assert_msg(0, "Unknown vtx boundary interpolation");
  return OSD_VTX_BOUNDARY_EDGE_ONLY;
}

/*OpenSubdiv_FVarLinearInterpolation*/ int converter_fvar_linear_from_settings(
    const Settings *settings)
{
  switch (settings->fvar_linear_interpolation) {
    case SUBDIV_FVAR_LINEAR_INTERPOLATION_NONE:
      return OSD_FVAR_LINEAR_INTERPOLATION_NONE;
    case SUBDIV_FVAR_LINEAR_INTERPOLATION_CORNERS_ONLY:
      return OSD_FVAR_LINEAR_INTERPOLATION_CORNERS_ONLY;
    case SUBDIV_FVAR_LINEAR_INTERPOLATION_CORNERS_AND_JUNCTIONS:
      return OSD_FVAR_LINEAR_INTERPOLATION_CORNERS_PLUS1;
    case SUBDIV_FVAR_LINEAR_INTERPOLATION_CORNERS_JUNCTIONS_AND_CONCAVE:
      return OSD_FVAR_LINEAR_INTERPOLATION_CORNERS_PLUS2;
    case SUBDIV_FVAR_LINEAR_INTERPOLATION_BOUNDARIES:
      return OSD_FVAR_LINEAR_INTERPOLATION_BOUNDARIES;
    case SUBDIV_FVAR_LINEAR_INTERPOLATION_ALL:
      return OSD_FVAR_LINEAR_INTERPOLATION_ALL;
  }
  BLI_assert_msg(0, "Unknown fvar linear interpolation");
  return OSD_FVAR_LINEAR_INTERPOLATION_NONE;
}

}  // namespace blender::bke::subdiv
