/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

bool test_occlusion()
{
  vec3 ndc = (gl_Position.xyz / gl_Position.w) * 0.5 + 0.5;
  return (ndc.z - 0.00035) > texture(depthTex, ndc.xy).r;
}

void main()
{
  GPU_INTEL_VERTEX_SHADER_WORKAROUND

  /* Avoid undefined behavior after return. */
  finalColor = vec4(0.0);
  gl_Position = vec4(0.0);

  vec3 nor;
  /* Select the right normal by checking if the generic attribute is used. */
  if (!all(equal(lnor.xyz, vec3(0)))) {
    if (lnor.w < 0.0) {
      return;
    }
    nor = lnor.xyz;
    finalColor = colorLNormal;
  }
  else if (!all(equal(vnor.xyz, vec3(0)))) {
    if (vnor.w < 0.0) {
      return;
    }
    nor = vnor.xyz;
    finalColor = colorVNormal;
  }
  else {
    nor = norAndFlag.xyz;
    if (all(equal(nor, vec3(0)))) {
      return;
    }
    finalColor = colorNormal;
  }

  vec3 n = normalize(normal_object_to_world(nor));
  vec3 world_pos = point_object_to_world(pos);

  if (gl_VertexID == 0) {
    if (isConstantScreenSizeNormals) {
      bool is_persp = (drw_view.winmat[3][3] == 0.0);
      if (is_persp) {
        float dist_fac = length(cameraPos - world_pos);
        float cos_fac = dot(cameraForward, cameraVec(world_pos));
        world_pos += n * normalScreenSize * dist_fac * cos_fac * pixelFac * sizePixel;
      }
      else {
        float frustrum_fac = mul_project_m4_v3_zfac(n) * sizePixel;
        world_pos += n * normalScreenSize * frustrum_fac;
      }
    }
    else {
      world_pos += n * normalSize;
    }
  }

  gl_Position = point_world_to_ndc(world_pos);

  finalColor.a *= (test_occlusion()) ? alpha : 1.0;

  view_clipping_distances(world_pos);
}
