/* SPDX-FileCopyrightText: 2024 NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_reader_utils.hh"

#include <pxr/usd/usd/attribute.h>

#include "CLG_log.h"
static CLG_LogRef LOG = {"io.usd"};

namespace {

template<typename VECT>
void set_array_prop(IDProperty *idgroup,
                    const char *prop_name,
                    const pxr::UsdAttribute &attr,
                    const pxr::UsdTimeCode motionSampleTime)
{
  if (!idgroup || !attr) {
    return;
  }

  VECT vec;
  if (!attr.Get<VECT>(&vec, motionSampleTime)) {
    return;
  }

  IDPropertyTemplate val = {0};
  val.array.len = static_cast<int>(vec.dimension);

  if (val.array.len <= 0) {
    CLOG_WARN(&LOG, "Invalid array length for prop %s", prop_name);
    return;
  }

  if (std::is_same<float, typename VECT::ScalarType>()) {
    val.array.type = IDP_FLOAT;
  }
  else if (std::is_same<pxr::GfHalf, typename VECT::ScalarType>()) {
    val.array.type = IDP_FLOAT;
  }
  else if (std::is_same<double, typename VECT::ScalarType>()) {
    val.array.type = IDP_DOUBLE;
  }
  else if (std::is_same<int, typename VECT::ScalarType>()) {
    val.array.type = IDP_INT;
  }
  else {
    CLOG_WARN(&LOG, "Couldn't determine array type for prop %s", prop_name);
    return;
  }

  IDProperty *prop = IDP_New(IDP_ARRAY, &val, prop_name);

  if (!prop) {
    CLOG_WARN(&LOG, "Couldn't create array prop %s", prop_name);
    return;
  }

  if (std::is_same<pxr::GfHalf, typename VECT::ScalarType>()) {
    float *prop_data = static_cast<float *>(prop->data.pointer);
    for (int i = 0; i < val.array.len; ++i) {
      prop_data[i] = vec[i];
    }
  }
  else {
    std::memcpy(prop->data.pointer, vec.data(), prop->len * sizeof(typename VECT::ScalarType));
  }

  IDP_AddToGroup(idgroup, prop);
}

bool equivalent(const pxr::SdfValueTypeName &type_name1, const pxr::SdfValueTypeName &type_name2)
{
  return type_name1.GetType().IsA(type_name2.GetType());
}

}  // anonymous namespace

namespace blender::io::usd {

/* TfToken objects are not cheap to construct, so we do it once. */
namespace usdtokens {
static const pxr::TfToken userProperties("userProperties", pxr::TfToken::Immortal);
}  // namespace usdtokens

static void set_string_prop(IDProperty *idgroup, const char *prop_name, const char *str_val)
{
  if (!idgroup) {
    return;
  }

  IDPropertyTemplate val = {0};
  val.string.str = str_val;
  /* Note length includes null terminator. */
  val.string.len = strlen(str_val) + 1;
  val.string.subtype = IDP_STRING_SUB_UTF8;

  IDProperty *prop = IDP_New(IDP_STRING, &val, prop_name);

  IDP_AddToGroup(idgroup, prop);
}

static void set_int_prop(IDProperty *idgroup, const char *prop_name, const int ival)
{
  if (!idgroup) {
    return;
  }

  IDPropertyTemplate val = {0};
  val.i = ival;
  IDProperty *prop = IDP_New(IDP_INT, &val, prop_name);

  IDP_AddToGroup(idgroup, prop);
}

static void set_bool_prop(IDProperty *idgroup, const char *prop_name, const bool bval)
{
  if (!idgroup) {
    return;
  }

  IDPropertyTemplate val = {0};
  val.i = bval;
  IDProperty *prop = IDP_New(IDP_BOOLEAN, &val, prop_name);

  IDP_AddToGroup(idgroup, prop);
}

static void set_float_prop(IDProperty *idgroup, const char *prop_name, const float fval)
{
  if (!idgroup) {
    return;
  }

  IDPropertyTemplate val = {0};
  val.f = fval;
  IDProperty *prop = IDP_New(IDP_FLOAT, &val, prop_name);

  IDP_AddToGroup(idgroup, prop);
}

static void set_double_prop(IDProperty *idgroup, const char *prop_name, const double dval)
{
  if (!idgroup) {
    return;
  }

  IDPropertyTemplate val = {0};
  val.d = dval;
  IDProperty *prop = IDP_New(IDP_DOUBLE, &val, prop_name);

  IDP_AddToGroup(idgroup, prop);
}

void set_id_props_from_prim(ID *id,
                            const pxr::UsdPrim &prim,
                            const eUSDAttrImportMode attr_import_mode,
                            const pxr::UsdTimeCode time_code)
{
  pxr::UsdAttributeVector attribs = prim.GetAuthoredAttributes();
  if (attribs.empty()) {
    return;
  }

  bool all_custom_attrs = (attr_import_mode == USD_ATTR_IMPORT_ALL);

  for (const pxr::UsdAttribute &attr : attribs) {
    if (!attr.IsCustom()) {
      continue;
    }

    std::vector<std::string> attr_names = attr.SplitName();

    bool is_user_prop = attr_names[0] == "userProperties";

    if (attr_names.size() > 2 && is_user_prop && attr_names[1] == "blender") {
      continue;
    }

    if (!all_custom_attrs && !is_user_prop) {
      continue;
    }

    IDProperty *idgroup = IDP_EnsureProperties(id);

    /* When importing user properties, strip the namespace. */
    pxr::TfToken attr_name;
    if (is_user_prop) {
      /* We strip the userProperties namespace, but leave others in case
       * someone's custom attribute namespace is important in their pipeline. */
      const std::string token = "userProperties:";
      const std::string name = attr.GetName().GetString();
      attr_name = pxr::TfToken(name.substr(token.size(), name.size() - token.size()));
    }
    else {
      attr_name = attr.GetName();
    }

    pxr::SdfValueTypeName type_name = attr.GetTypeName();

    if (type_name == pxr::SdfValueTypeNames->Int) {
      int ival = 0;
      if (attr.Get<int>(&ival, time_code)) {
        set_int_prop(idgroup, attr_name.GetString().c_str(), ival);
      }
    }
    else if (type_name == pxr::SdfValueTypeNames->Float) {
      float fval = 0.0f;
      if (attr.Get<float>(&fval, time_code)) {
        set_float_prop(idgroup, attr_name.GetString().c_str(), fval);
      }
    }
    else if (type_name == pxr::SdfValueTypeNames->Double) {
      double dval = 0.0;
      if (attr.Get<double>(&dval, time_code)) {
        set_double_prop(idgroup, attr_name.GetString().c_str(), dval);
      }
    }
    else if (type_name == pxr::SdfValueTypeNames->Half) {
      pxr::GfHalf hval = 0.0f;
      if (attr.Get<pxr::GfHalf>(&hval, time_code)) {
        set_float_prop(idgroup, attr_name.GetString().c_str(), hval);
      }
    }
    else if (type_name == pxr::SdfValueTypeNames->String) {
      std::string sval;
      if (attr.Get<std::string>(&sval, time_code)) {
        set_string_prop(idgroup, attr_name.GetString().c_str(), sval.c_str());
      }
    }
    else if (type_name == pxr::SdfValueTypeNames->Token) {
      pxr::TfToken tval;
      if (attr.Get<pxr::TfToken>(&tval, time_code)) {
        set_string_prop(idgroup, attr_name.GetString().c_str(), tval.GetString().c_str());
      }
    }
    else if (type_name == pxr::SdfValueTypeNames->Asset) {
      pxr::SdfAssetPath aval;
      if (attr.Get<pxr::SdfAssetPath>(&aval, time_code)) {
        set_string_prop(idgroup, attr_name.GetString().c_str(), aval.GetAssetPath().c_str());
      }
    }
    else if (type_name == pxr::SdfValueTypeNames->Bool) {
      bool bval = false;
      if (attr.Get<bool>(&bval, time_code)) {
        set_bool_prop(idgroup, attr_name.GetString().c_str(), bval);
      }
    }
    else if (equivalent(type_name, pxr::SdfValueTypeNames->Float2)) {
      set_array_prop<pxr::GfVec2f>(idgroup, attr_name.GetString().c_str(), attr, time_code);
    }
    else if (equivalent(type_name, pxr::SdfValueTypeNames->Float3)) {
      set_array_prop<pxr::GfVec3f>(idgroup, attr_name.GetString().c_str(), attr, time_code);
    }
    else if (equivalent(type_name, pxr::SdfValueTypeNames->Float4)) {
      set_array_prop<pxr::GfVec4f>(idgroup, attr_name.GetString().c_str(), attr, time_code);
    }
    else if (equivalent(type_name, pxr::SdfValueTypeNames->Double2)) {
      set_array_prop<pxr::GfVec2d>(idgroup, attr_name.GetString().c_str(), attr, time_code);
    }
    else if (equivalent(type_name, pxr::SdfValueTypeNames->Double3)) {
      set_array_prop<pxr::GfVec3d>(idgroup, attr_name.GetString().c_str(), attr, time_code);
    }
    else if (equivalent(type_name, pxr::SdfValueTypeNames->Double4)) {
      set_array_prop<pxr::GfVec4d>(idgroup, attr_name.GetString().c_str(), attr, time_code);
    }
    else if (equivalent(type_name, pxr::SdfValueTypeNames->Int2)) {
      set_array_prop<pxr::GfVec2i>(idgroup, attr_name.GetString().c_str(), attr, time_code);
    }
    else if (equivalent(type_name, pxr::SdfValueTypeNames->Int3)) {
      set_array_prop<pxr::GfVec3i>(idgroup, attr_name.GetString().c_str(), attr, time_code);
    }
    else if (equivalent(type_name, pxr::SdfValueTypeNames->Int4)) {
      set_array_prop<pxr::GfVec4i>(idgroup, attr_name.GetString().c_str(), attr, time_code);
    }
    else if (equivalent(type_name, pxr::SdfValueTypeNames->Half2)) {
      set_array_prop<pxr::GfVec2h>(idgroup, attr_name.GetString().c_str(), attr, time_code);
    }
    else if (equivalent(type_name, pxr::SdfValueTypeNames->Half3)) {
      set_array_prop<pxr::GfVec3h>(idgroup, attr_name.GetString().c_str(), attr, time_code);
    }
    else if (equivalent(type_name, pxr::SdfValueTypeNames->Half4)) {
      set_array_prop<pxr::GfVec4h>(idgroup, attr_name.GetString().c_str(), attr, time_code);
    }
  }
}

}  // namespace blender::io::usd
