/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#pragma once

#include "BLI_math_vec_types.hh"

#include "DNA_node_types.h"
struct Material;

namespace blender::io::obj {

enum class MTLTexMapType { Kd = 0, Ks, Ns, d, refl, Ke, bump, Count };
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
    return texture_maps[(int)key];
  }
  MTLTexMap &tex_map_of_type(MTLTexMapType key)
  {
    return texture_maps[(int)key];
  }

  std::string name;
  /* Always check for negative values while importing or exporting. Use defaults if
   * any value is negative. */
  float Ns{-1.0f};
  float3 Ka{-1.0f};
  float3 Kd{-1.0f};
  float3 Ks{-1.0f};
  float3 Ke{-1.0f};
  float Ni{-1.0f};
  float d{-1.0f};
  int illum{-1};
  MTLTexMap texture_maps[(int)MTLTexMapType::Count];
  /** Only used for Normal Map node: "map_Bump". */
  float map_Bump_strength{-1.0f};
};

MTLMaterial mtlmaterial_for_material(const Material *material);
}  // namespace blender::io::obj
