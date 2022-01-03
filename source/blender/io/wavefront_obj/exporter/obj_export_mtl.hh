/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup obj
 */

#pragma once

#include "BLI_float3.hh"
#include "BLI_map.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

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
class OBJMesh;

/**
 * Generic container for texture node properties.
 */
struct tex_map_XX {
  tex_map_XX(StringRef to_socket_id) : dest_socket_id(to_socket_id){};

  /** Target socket which this texture node connects to. */
  const std::string dest_socket_id;
  float3 translation{0.0f};
  float3 scale{1.0f};
  /* Only Flat and Smooth projections are supported. */
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

  /**
   * Caller must ensure that the given lookup key exists in the Map.
   * \return Texture map corresponding to the given ID.
   */
  tex_map_XX &tex_map_of_type(const eMTLSyntaxElement key)
  {
    {
      BLI_assert(texture_maps.contains_as(key));
      return texture_maps.lookup_as(key);
    }
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
