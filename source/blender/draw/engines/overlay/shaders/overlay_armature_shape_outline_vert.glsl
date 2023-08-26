/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

/* project to screen space */
vec2 proj(vec4 pos)
{
  return (0.5 * (pos.xy / pos.w) + 0.5) * sizeViewport.xy;
}

void main()
{
  vec4 bone_color, state_color;
  mat4 model_mat = extract_matrix_packed_data(inst_obmat, state_color, bone_color);

  vec4 world_pos = model_mat * vec4(pos, 1.0);
  vec4 view_pos = drw_view.viewmat * world_pos;

  geom_in.vPos = view_pos.xyz;
  geom_in.pPos = drw_view.winmat * view_pos;

  geom_flat_in.inverted = int(dot(cross(model_mat[0].xyz, model_mat[1].xyz), model_mat[2].xyz) <
                              0.0);

  /* This is slow and run per vertex, but it's still faster than
   * doing it per instance on CPU and sending it on via instance attribute. */
  mat3 normal_mat = transpose(inverse(mat3(model_mat)));
  /* TODO: FIX: there is still a problem with this vector
   * when the bone is scaled or in perspective mode.
   * But it's barely visible at the outline corners. */
  geom_in.ssNor = normalize(normal_world_to_view(normal_mat * snor).xy);

  geom_in.ssPos = proj(geom_in.pPos);

  geom_in.vColSize = bone_color;

  view_clipping_distances(world_pos.xyz);
}
