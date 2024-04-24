/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_mesh_utils.hh"
#include "usd_hash_types.hh"

#include "BKE_attribute.hh"
#include "BKE_report.hh"

#include "BLI_color.hh"
#include "BLI_span.hh"

#include "DNA_mesh_types.h"

namespace blender::io::usd {

std::optional<eCustomDataType> convert_usd_type_to_blender(const pxr::SdfValueTypeName usd_type,
                                                           ReportList *reports)
{
  static const blender::Map<pxr::SdfValueTypeName, eCustomDataType> type_map = []() {
    blender::Map<pxr::SdfValueTypeName, eCustomDataType> map;
    map.add_new(pxr::SdfValueTypeNames->FloatArray, CD_PROP_FLOAT);
    map.add_new(pxr::SdfValueTypeNames->Double, CD_PROP_FLOAT);
    map.add_new(pxr::SdfValueTypeNames->IntArray, CD_PROP_INT32);
    map.add_new(pxr::SdfValueTypeNames->Float2Array, CD_PROP_FLOAT2);
    map.add_new(pxr::SdfValueTypeNames->TexCoord2dArray, CD_PROP_FLOAT2);
    map.add_new(pxr::SdfValueTypeNames->TexCoord2fArray, CD_PROP_FLOAT2);
    map.add_new(pxr::SdfValueTypeNames->TexCoord2hArray, CD_PROP_FLOAT2);
    map.add_new(pxr::SdfValueTypeNames->TexCoord3dArray, CD_PROP_FLOAT2);
    map.add_new(pxr::SdfValueTypeNames->TexCoord3fArray, CD_PROP_FLOAT2);
    map.add_new(pxr::SdfValueTypeNames->TexCoord3hArray, CD_PROP_FLOAT2);
    map.add_new(pxr::SdfValueTypeNames->Float3Array, CD_PROP_FLOAT3);
    map.add_new(pxr::SdfValueTypeNames->Point3fArray, CD_PROP_FLOAT3);
    map.add_new(pxr::SdfValueTypeNames->Point3dArray, CD_PROP_FLOAT3);
    map.add_new(pxr::SdfValueTypeNames->Point3hArray, CD_PROP_FLOAT3);
    map.add_new(pxr::SdfValueTypeNames->Normal3fArray, CD_PROP_FLOAT3);
    map.add_new(pxr::SdfValueTypeNames->Normal3dArray, CD_PROP_FLOAT3);
    map.add_new(pxr::SdfValueTypeNames->Normal3hArray, CD_PROP_FLOAT3);
    map.add_new(pxr::SdfValueTypeNames->Vector3fArray, CD_PROP_FLOAT3);
    map.add_new(pxr::SdfValueTypeNames->Vector3hArray, CD_PROP_FLOAT3);
    map.add_new(pxr::SdfValueTypeNames->Vector3dArray, CD_PROP_FLOAT3);
    map.add_new(pxr::SdfValueTypeNames->Color3fArray, CD_PROP_COLOR);
    map.add_new(pxr::SdfValueTypeNames->Color3hArray, CD_PROP_COLOR);
    map.add_new(pxr::SdfValueTypeNames->Color3dArray, CD_PROP_COLOR);
    map.add_new(pxr::SdfValueTypeNames->StringArray, CD_PROP_STRING);
    map.add_new(pxr::SdfValueTypeNames->BoolArray, CD_PROP_BOOL);
    map.add_new(pxr::SdfValueTypeNames->QuatfArray, CD_PROP_QUATERNION);
    map.add_new(pxr::SdfValueTypeNames->QuatdArray, CD_PROP_QUATERNION);
    map.add_new(pxr::SdfValueTypeNames->QuathArray, CD_PROP_QUATERNION);
    return map;
  }();

  const eCustomDataType *value = type_map.lookup_ptr(usd_type);
  if (value == nullptr) {
    BKE_reportf(reports,
                RPT_WARNING,
                "Unsupported type %s for mesh data",
                usd_type.GetAsToken().GetText());
    return std::nullopt;
  }

  return *value;
}

/* To avoid putting the templated method definition in the header file,
 * it is necessary to define each of the possible template instantiations
 * that we support.  Ugly here, but it keeps the header looking clean.
 */
template pxr::VtArray<pxr::GfVec2f> get_prim_attribute_array<pxr::GfVec2f>(
    const pxr::UsdGeomPrimvar &primvar, const double motionSampleTime, ReportList *reports);
template pxr::VtArray<pxr::GfVec3f> get_prim_attribute_array<pxr::GfVec3f>(
    const pxr::UsdGeomPrimvar &primvar, const double motionSampleTime, ReportList *reports);
template pxr::VtArray<bool> get_prim_attribute_array<bool>(const pxr::UsdGeomPrimvar &primvar,
                                                           const double motionSampleTime,
                                                           ReportList *reports);
template pxr::VtArray<int> get_prim_attribute_array<int>(const pxr::UsdGeomPrimvar &primvar,
                                                         const double motionSampleTime,
                                                         ReportList *reports);
template pxr::VtArray<float> get_prim_attribute_array<float>(const pxr::UsdGeomPrimvar &primvar,
                                                             const double motionSampleTime,
                                                             ReportList *reports);

template<typename T>
pxr::VtArray<T> get_prim_attribute_array(const pxr::UsdGeomPrimvar &primvar,
                                         const double motionSampleTime,
                                         ReportList *reports)
{
  pxr::VtArray<T> array;

  pxr::VtValue primvar_val;

  if (!primvar.ComputeFlattened(&primvar_val, motionSampleTime)) {
    BKE_reportf(reports,
                RPT_WARNING,
                "USD Import: unable to get array values for primvar '%s'",
                primvar.GetName().GetText());
    return array;
  }

  if (!primvar_val.CanCast<pxr::VtArray<T>>()) {
    BKE_reportf(reports,
                RPT_WARNING,
                "USD Import: can't cast attribute '%s' to array",
                primvar.GetName().GetText());
    return array;
  }

  array = primvar_val.Cast<pxr::VtArray<T>>().template UncheckedGet<pxr::VtArray<T>>();
  return array;
}

void read_color_data_primvar(Mesh *mesh,
                             const pxr::UsdGeomPrimvar &primvar,
                             double motion_sample_time,
                             ReportList *reports,
                             bool is_left_handed)
{
  if (!(mesh && primvar && primvar.HasValue())) {
    return;
  }

  pxr::VtArray<pxr::GfVec3f> usd_colors = get_prim_attribute_array<pxr::GfVec3f>(
      primvar, motion_sample_time, reports);

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
