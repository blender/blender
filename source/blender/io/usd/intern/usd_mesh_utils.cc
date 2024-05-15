/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_mesh_utils.hh"
#include "usd_attribute_utils.hh"
#include "usd_hash_types.hh"

#include "BKE_attribute.hh"
#include "BKE_report.hh"

#include "BLI_color.hh"
#include "BLI_span.hh"

#include "DNA_mesh_types.h"

namespace blender::io::usd {

void read_color_data_primvar(Mesh *mesh,
                             const pxr::UsdGeomPrimvar &primvar,
                             double motion_sample_time,
                             ReportList *reports,
                             bool is_left_handed)
{
  if (!(mesh && primvar && primvar.HasValue())) {
    return;
  }

  pxr::VtArray<pxr::GfVec3f> usd_colors = get_primvar_array<pxr::GfVec3f>(primvar,
                                                                          motion_sample_time);

  if (usd_colors.empty()) {
    return;
  }

  pxr::TfToken interp = primvar.GetInterpolation();

  if ((interp == pxr::UsdGeomTokens->faceVarying && usd_colors.size() != mesh->corners_num) ||
      (interp == pxr::UsdGeomTokens->varying && usd_colors.size() != mesh->corners_num) ||
      (interp == pxr::UsdGeomTokens->vertex && usd_colors.size() != mesh->verts_num) ||
      (interp == pxr::UsdGeomTokens->constant && usd_colors.size() != 1) ||
      (interp == pxr::UsdGeomTokens->uniform && usd_colors.size() != mesh->faces_num))
  {
    BKE_reportf(
        reports,
        RPT_WARNING,
        "USD Import: color attribute value '%s' count inconsistent with interpolation type",
        primvar.GetName().GetText());
    return;
  }

  const StringRef primvar_name(primvar.GetBaseName().GetString());
  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();

  bke::AttrDomain color_domain = bke::AttrDomain::Point;

  if (ELEM(interp,
           pxr::UsdGeomTokens->varying,
           pxr::UsdGeomTokens->faceVarying,
           pxr::UsdGeomTokens->uniform))
  {
    color_domain = bke::AttrDomain::Corner;
  }

  bke::SpanAttributeWriter<ColorGeometry4f> color_data;
  color_data = attributes.lookup_or_add_for_write_only_span<ColorGeometry4f>(primvar_name,
                                                                             color_domain);
  if (!color_data) {
    BKE_reportf(reports,
                RPT_WARNING,
                "USD Import: couldn't add color attribute '%s'",
                primvar.GetBaseName().GetText());
    return;
  }

  if (ELEM(interp, pxr::UsdGeomTokens->constant)) {
    /* For situations where there's only a single item, flood fill the object. */
    color_data.span.fill(
        ColorGeometry4f(usd_colors[0][0], usd_colors[0][1], usd_colors[0][2], 1.0f));
  }
  /* Check for situations that allow for a straight-forward copy by index. */
  else if (interp == pxr::UsdGeomTokens->vertex ||
           (interp == pxr::UsdGeomTokens->faceVarying && !is_left_handed))
  {
    for (int i = 0; i < usd_colors.size(); i++) {
      ColorGeometry4f color = ColorGeometry4f(
          usd_colors[i][0], usd_colors[i][1], usd_colors[i][2], 1.0f);
      color_data.span[i] = color;
    }
  }
  else {
    /* Catch all for the remaining cases. */

    /* Special case: we will expand uniform color into corner color.
     * Uniforms in USD come through as single colors, face-varying. Since Blender does not
     * support this particular combination for paintable color attributes, we convert the type
     * here to make sure that the user gets the same visual result.
     */
    const OffsetIndices faces = mesh->faces();
    const Span<int> corner_verts = mesh->corner_verts();
    for (const int i : faces.index_range()) {
      const IndexRange face = faces[i];
      for (int j = 0; j < face.size(); ++j) {
        int loop_index = face[j];

        /* Default for constant interpolation. */
        int usd_index = 0;

        if (interp == pxr::UsdGeomTokens->vertex) {
          usd_index = corner_verts[loop_index];
        }
        else if (interp == pxr::UsdGeomTokens->faceVarying) {
          usd_index = face.start();
          if (is_left_handed) {
            usd_index += face.size() - 1 - j;
          }
          else {
            usd_index += j;
          }
        }
        else if (interp == pxr::UsdGeomTokens->uniform) {
          /* Uniform varying uses the face index. */
          usd_index = i;
        }

        if (usd_index >= usd_colors.size()) {
          continue;
        }

        ColorGeometry4f color = ColorGeometry4f(
            usd_colors[usd_index][0], usd_colors[usd_index][1], usd_colors[usd_index][2], 1.0f);
        color_data.span[loop_index] = color;
      }
    }
  }

  color_data.finish();
}

}  // namespace blender::io::usd
