/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#pragma once

#include "BLI_math_vector_types.hh"

#include "DNA_node_types.h"
struct Material;

namespace blender::io::obj {

enum class MTLTexMapType {
  Color = 0,
  Metallic,
  Specular,
  SpecularExponent,
  Roughness,
  Sheen,
  Reflection,
  Emission,
  Alpha,
  Normal,
  Count
};
extern const char *tex_map_type_to_socket_id[];

struct MTLTexMap {
  bool is_valid() const
  {
    return !image_path.empty();
  }

  /* Target socket which this texture node connects to. */
  float3 translation{0.0f};
  float3 scale{1.0f};
  /* Only Flat and Sphere projections are supported. */
  int projection_type = SHD_PROJ_FLAT;
  std::string image_path;
  std::string mtl_dir_path;
};

/**
 * Container suited for storing Material data for/from a .MTL file.
 */
struct MTLMaterial {
  const MTLTexMap &tex_map_of_type(MTLTexMapType key) const
  {
    return texture_maps[int(key)];
  }
  MTLTexMap &tex_map_of_type(MTLTexMapType key)
  {
    return texture_maps[int(key)];
  }

  std::string name;
  /* Always check for negative values while importing or exporting. Use defaults if
   * any value is negative. */
  float spec_exponent{-1.0f};   /* `Ns` */
  float3 ambient_color{-1.0f};  /* `Ka` */
  float3 color{-1.0f};          /* `Kd` */
  float3 spec_color{-1.0f};     /* `Ks` */
  float3 emission_color{-1.0f}; /* `Ke` */
  float ior{-1.0f};             /* `Ni` */
  float alpha{-1.0f};           /* `d` */
  float3 transmit_color{-1.0f}; /* `Kt` / `Tf` */
  float roughness{-1.0f};       /* `Pr` */
  float metallic{-1.0f};        /* `Pm` */
  float sheen{-1.0f};           /* `Ps` */
  float cc_thickness{-1.0f};    /* `Pc` */
  float cc_roughness{-1.0f};    /* `Pcr` */
  float aniso{-1.0f};           /* `aniso` */
  float aniso_rot{-1.0f};       /* `anisor` */

  int illum_mode{-1};
  MTLTexMap texture_maps[int(MTLTexMapType::Count)];
  /* Only used for Normal Map node: `map_Bump`. */
  float normal_strength{-1.0f};
};

MTLMaterial mtlmaterial_for_material(const Material *material);
}  // namespace blender::io::obj
