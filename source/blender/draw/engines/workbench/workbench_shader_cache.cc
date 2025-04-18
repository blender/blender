/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "workbench_private.hh"

namespace blender::workbench {

ShaderCache::ShaderCache()
{
  const std::string geometries[] = {"_mesh", "_curves", "_ptcloud"};
  const std::string pipelines[] = {"_opaque", "_transparent"};
  const std::string lightings[] = {"_flat", "_studio", "_matcap"};
  const std::string shaders[] = {"_material", "_texture"};
  const std::string clip[] = {"_no_clip", "_clip"};
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

  const std::string cavity[] = {"_no_cavity", "_cavity"};
  const std::string curvature[] = {"_no_curvature", "_curvature"};
  const std::string shadow[] = {"_no_shadow", "_shadow"};

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

  const std::string pass[] = {"_fail", "_pass"};
  const std::string manifold[] = {"_no_manifold", "_manifold"};
  const std::string caps[] = {"_no_caps", "_caps"};

  for (auto p : IndexRange(2) /*pass*/) {
    for (auto m : IndexRange(2) /*manifold*/) {
      for (auto c : IndexRange(2) /*caps*/) {
        shadow_[p][m][c] = {"workbench_shadow" + pass[p] + manifold[m] + caps[c] +
                            (DEBUG_SHADOW_VOLUME ? "_debug" : "")};
      }
    }
  }

  const std::string smoke[] = {"_object", "_smoke"};
  const std::string interpolation[] = {"_linear", "_cubic", "_closest"};
  const std::string coba[] = {"_no_coba", "_coba"};
  const std::string slice[] = {"_no_slice", "_slice"};

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
