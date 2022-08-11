/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#pragma once

#include "BLI_map.hh"
#include "BLI_math_vec_types.hh"

#include "DNA_node_types.h"
#include "obj_export_io.hh"

namespace blender {
template<> struct DefaultHash<io::obj::eMTLSyntaxElement> {
  uint64_t operator()(const io::obj::eMTLSyntaxElement value) const
  {
    return static_cast<uint64_t>(value);
  }
};

}  // namespace blender

namespace blender::io::obj {

/**
 * Generic container for texture node properties.
 */
struct tex_map_XX {
  tex_map_XX(StringRef to_socket_id) : dest_socket_id(to_socket_id){};
  bool is_valid() const
  {
    return !image_path.empty();
  }

  /* Target socket which this texture node connects to. */
  const std::string dest_socket_id;
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
  MTLMaterial()
  {
    texture_maps.add(eMTLSyntaxElement::map_Kd, tex_map_XX("Base Color"));
    texture_maps.add(eMTLSyntaxElement::map_Ks, tex_map_XX("Specular"));
    texture_maps.add(eMTLSyntaxElement::map_Ns, tex_map_XX("Roughness"));
    texture_maps.add(eMTLSyntaxElement::map_d, tex_map_XX("Alpha"));
    texture_maps.add(eMTLSyntaxElement::map_refl, tex_map_XX("Metallic"));
    texture_maps.add(eMTLSyntaxElement::map_Ke, tex_map_XX("Emission"));
    texture_maps.add(eMTLSyntaxElement::map_Bump, tex_map_XX("Normal"));
  }

  const tex_map_XX &tex_map_of_type(const eMTLSyntaxElement key) const
  {
    BLI_assert(texture_maps.contains(key));
    return texture_maps.lookup(key);
  }
  tex_map_XX &tex_map_of_type(const eMTLSyntaxElement key)
  {
    BLI_assert(texture_maps.contains(key));
    return texture_maps.lookup(key);
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
  Map<const eMTLSyntaxElement, tex_map_XX> texture_maps;
  /** Only used for Normal Map node: "map_Bump". */
  float map_Bump_strength{-1.0f};
};

MTLMaterial mtlmaterial_for_material(const Material *material);
}  // namespace blender::io::obj
