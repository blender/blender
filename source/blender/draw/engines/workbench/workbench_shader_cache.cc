/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "workbench_private.hh"

namespace blender::workbench {

ShaderCache *ShaderCache::static_cache = nullptr;

ShaderCache &ShaderCache::get()
{
  if (!ShaderCache::static_cache) {
    ShaderCache::static_cache = new ShaderCache();
  }
  return *ShaderCache::static_cache;
}

void ShaderCache::release()
{
  if (ShaderCache::static_cache) {
    delete ShaderCache::static_cache;
    ShaderCache::static_cache = nullptr;
  }
}

ShaderCache::ShaderCache()
{
  std::string geometries[] = {"_mesh", "_curves", "_ptcloud"};
  std::string pipelines[] = {"_opaque", "_transparent"};
  std::string lightings[] = {"_flat", "_studio", "_matcap"};
  std::string shaders[] = {"_material", "_texture"};
  std::string clip[] = {"_no_clip", "_clip"};
  static_assert(std::size(geometries) == geometry_type_len);
  static_assert(std::size(pipelines) == pipeline_type_len);
  static_assert(std::size(lightings) == lighting_type_len);
  static_assert(std::size(shaders) == shader_type_len);

  for (auto g : IndexRange(geometry_type_len)) {
    for (auto p : IndexRange(pipeline_type_len)) {
      for (auto l : IndexRange(lighting_type_len)) {
        for (auto s : IndexRange(shader_type_len)) {
          for (auto c : IndexRange(2) /*clip*/) {
            prepass_[g][p][l][s][c] = {"workbench_prepass" + geometries[g] + pipelines[p] +
                                       lightings[l] + shaders[s] + clip[c]};
          }
        }
      }
    }
  }

  std::string cavity[] = {"_no_cavity", "_cavity"};
  std::string curvature[] = {"_no_curvature", "_curvature"};
  std::string shadow[] = {"_no_shadow", "_shadow"};

  for (auto l : IndexRange(lighting_type_len)) {
    for (auto ca : IndexRange(2) /*cavity*/) {
      for (auto cu : IndexRange(2) /*curvature*/) {
        for (auto s : IndexRange(2) /*shadow*/) {
          resolve_[l][ca][cu][s] = {"workbench_resolve_opaque" + lightings[l] + cavity[ca] +
                                    curvature[cu] + shadow[s]};
        }
      }
    }
  }

  std::string pass[] = {"_fail", "_pass"};
  std::string manifold[] = {"_no_manifold", "_manifold"};
  std::string caps[] = {"_no_caps", "_caps"};

  for (auto p : IndexRange(2) /*pass*/) {
    for (auto m : IndexRange(2) /*manifold*/) {
      for (auto c : IndexRange(2) /*caps*/) {
        shadow_[p][m][c] = {"workbench_shadow" + pass[p] + manifold[m] + caps[c] +
                            (DEBUG_SHADOW_VOLUME ? "_debug" : "")};
      }
    }
  }

  std::string smoke[] = {"_smoke", "_object"};
  std::string interpolation[] = {"_linear", "_cubic", "_closest"};
  std::string coba[] = {"_no_coba", "_coba"};
  std::string slice[] = {"_no_slice", "_slice"};

  for (auto sm : IndexRange(2) /*smoke*/) {
    for (auto i : IndexRange(3) /*interpolation*/) {
      for (auto c : IndexRange(2) /*coba*/) {
        for (auto sl : IndexRange(2) /*slice*/) {
          volume_[sm][i][c][sl] = {"workbench_volume" + smoke[sm] + interpolation[i] + coba[c] +
                                   slice[sl]};
        }
      }
    }
  }
}

}  // namespace blender::workbench
