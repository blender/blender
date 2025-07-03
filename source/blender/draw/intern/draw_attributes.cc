/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_customdata.hh"

#include "GPU_shader.hh"

#include "draw_attributes.hh"

namespace blender::draw {

void drw_attributes_merge(VectorSet<std::string> *dst, const VectorSet<std::string> *src)
{
  dst->add_multiple(src->as_span());
}

bool drw_attributes_overlap(const VectorSet<std::string> *a, const VectorSet<std::string> *b)
{
  for (const std::string &req : b->as_span()) {
    if (!a->contains(req)) {
      return false;
    }
  }

  return true;
}

void drw_attributes_add_request(VectorSet<std::string> *attrs, const StringRef name)
{
  if (attrs->size() >= GPU_MAX_ATTR) {
    return;
  }
  attrs->add_as(name);
}

bool drw_custom_data_match_attribute(const CustomData &custom_data,
                                     const StringRef name,
                                     int *r_layer_index,
                                     eCustomDataType *r_type)
{
  const eCustomDataType possible_attribute_types[11] = {
      CD_PROP_BOOL,
      CD_PROP_INT8,
      CD_PROP_INT16_2D,
      CD_PROP_INT32_2D,
      CD_PROP_INT32,
      CD_PROP_FLOAT,
      CD_PROP_FLOAT2,
      CD_PROP_FLOAT3,
      CD_PROP_COLOR,
      CD_PROP_QUATERNION,
      CD_PROP_BYTE_COLOR,
  };

  for (int i = 0; i < ARRAY_SIZE(possible_attribute_types); i++) {
    const eCustomDataType attr_type = possible_attribute_types[i];
    int layer_index = CustomData_get_named_layer(&custom_data, attr_type, name);
    if (layer_index == -1) {
      continue;
    }

    *r_layer_index = layer_index;
    *r_type = attr_type;
    return true;
  }

  return false;
}

}  // namespace blender::draw
