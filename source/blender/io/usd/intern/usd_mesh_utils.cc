/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_mesh_utils.hh"
#include "usd_attribute_utils.hh"
#include "usd_hash_types.hh"

#include "BKE_attribute.hh"

#include "DNA_mesh_types.h"

#include "CLG_log.h"
static CLG_LogRef LOG = {"io.usd"};

namespace blender::io::usd {

template<typename USDT>
static void read_face_display_color(Mesh *mesh,
                                    const pxr::UsdGeomPrimvar &primvar,
                                    const pxr::TfToken &pv_name,
                                    double motion_sample_time)
{
  const pxr::VtArray<USDT> usd_colors = get_primvar_array<USDT>(primvar, motion_sample_time);
  if (usd_colors.empty()) {
    return;
  }

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  const bke::AttrDomain color_domain = bke::AttrDomain::Corner;

  const StringRef attr_name(pv_name.GetString());
  bke::SpanAttributeWriter<ColorGeometry4f> color_data =
      attributes.lookup_or_add_for_write_only_span<ColorGeometry4f>(attr_name, color_domain);
  if (!color_data) {
    CLOG_WARN(&LOG, "Primvar '%s' could not be added to Blender", primvar.GetBaseName().GetText());
    return;
  }

  const OffsetIndices faces = mesh->faces();
  for (const int i : faces.index_range()) {
    if (i >= usd_colors.size()) {
      break;
    }

    /* Take the per-face USD color and place it on each face-corner. */
    const IndexRange face = faces[i];
    for (const int j : face.index_range()) {
      const int corner = face.start() + j;
      color_data.span[corner] = detail::convert_value<USDT, ColorGeometry4f>(usd_colors[i]);
    }
  }

  color_data.finish();
}

static std::optional<bke::AttrDomain> convert_usd_varying_to_blender(const pxr::TfToken usd_domain)
{
  static const blender::Map<pxr::TfToken, bke::AttrDomain> domain_map = []() {
    blender::Map<pxr::TfToken, bke::AttrDomain> map;
    map.add_new(pxr::UsdGeomTokens->faceVarying, bke::AttrDomain::Corner);
    map.add_new(pxr::UsdGeomTokens->vertex, bke::AttrDomain::Point);
    map.add_new(pxr::UsdGeomTokens->varying, bke::AttrDomain::Point);
    map.add_new(pxr::UsdGeomTokens->face, bke::AttrDomain::Face);
    /* As there's no "constant" type in Blender, for now we're
     * translating into a point Attribute. */
    map.add_new(pxr::UsdGeomTokens->constant, bke::AttrDomain::Point);
    map.add_new(pxr::UsdGeomTokens->uniform, bke::AttrDomain::Face);
    /* Notice: Edge types are not supported! */
    return map;
  }();

  const bke::AttrDomain *value = domain_map.lookup_ptr(usd_domain);

  if (value == nullptr) {
    return std::nullopt;
  }

  return *value;
}

void read_generic_mesh_primvar(Mesh *mesh,
                               const pxr::UsdGeomPrimvar &primvar,
                               const double motionSampleTime,
                               const bool is_left_handed)
{
  const pxr::SdfValueTypeName pv_type = primvar.GetTypeName();
  const pxr::TfToken pv_interp = primvar.GetInterpolation();
  const pxr::TfToken pv_name = pxr::UsdGeomPrimvar::StripPrimvarsName(primvar.GetPrimvarName());

  const std::optional<bke::AttrDomain> domain = convert_usd_varying_to_blender(pv_interp);
  const std::optional<eCustomDataType> type = convert_usd_type_to_blender(pv_type);

  if (!domain.has_value() || !type.has_value()) {
    CLOG_WARN(&LOG,
              "Primvar '%s' (interpolation %s, type %s) cannot be converted to Blender",
              pv_name.GetText(),
              pv_interp.GetText(),
              pv_type.GetAsToken().GetText());
    return;
  }

  /* Blender does not currently support displaying Face colors with the Viewport Shading
   * "Attribute" color type. Make a special case for "displayColor" primvars and put them on
   * the Corner domain instead. */
  if (pv_name == usdtokens::displayColor && domain == bke::AttrDomain::Face) {
    if (ELEM(pv_type,
             pxr::SdfValueTypeNames->Color3fArray,
             pxr::SdfValueTypeNames->Color3hArray,
             pxr::SdfValueTypeNames->Color3dArray))
    {
      read_face_display_color<pxr::GfVec3f>(mesh, primvar, pv_name, motionSampleTime);
    }
    else {
      read_face_display_color<pxr::GfVec4f>(mesh, primvar, pv_name, motionSampleTime);
    }

    return;
  }

  OffsetIndices<int> faces;
  if (is_left_handed) {
    faces = mesh->faces();
  }

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  copy_primvar_to_blender_attribute(primvar, motionSampleTime, *type, *domain, faces, attributes);
}

}  // namespace blender::io::usd
