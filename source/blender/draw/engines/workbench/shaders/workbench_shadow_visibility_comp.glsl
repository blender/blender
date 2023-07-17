
#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(common_intersect_lib.glsl)

shared uint shared_result;

#ifdef DYNAMIC_PASS_SELECTION
void set_visibility(bool pass, bool fail)
{
  if (!pass) {
    atomicAnd(pass_visibility_buf[gl_WorkGroupID.x], ~(1u << gl_LocalInvocationID.x));
  }
  if (!fail) {
    atomicAnd(fail_visibility_buf[gl_WorkGroupID.x], ~(1u << gl_LocalInvocationID.x));
  }
}
#else
void set_visibility(bool visibility)
{
  if (!visibility) {
    atomicAnd(visibility_buf[gl_WorkGroupID.x], ~(1u << gl_LocalInvocationID.x));
  }
}
#endif

bool is_visible(IsectBox box)
{
  for (int i_plane = 0; i_plane < extruded_frustum.planes_count; i_plane++) {
    vec4 plane = extruded_frustum.planes[i_plane];
    bool separating_axis = true;
    for (int i_corner = 0; i_corner < 8; i_corner++) {
      float signed_distance = dot(box.corners[i_corner], plane.xyz) - plane.w;
      if (signed_distance <= 0) {
        separating_axis = false;
        break;
      }
    }
    if (separating_axis) {
      return false;
    }
  }
  return true;
}

bool intersects_near_plane(IsectBox box)
{
  vec4 near_plane = drw_view_culling.frustum_planes.planes[4];
  bool on_positive_side = false;
  bool on_negative_side = false;

  for (int i_corner = 0; i_corner < 8; i_corner++) {
    for (int i_displace = 0; i_displace < 2; i_displace++) {
      vec3 corner = box.corners[i_corner] + (shadow_direction * 1e5f * i_displace);
      float signed_distance = dot(corner, -near_plane.xyz) - near_plane.w;
      if (signed_distance <= 0) {
        on_negative_side = true;
      }
      else {
        on_positive_side = true;
      }
      if (on_negative_side && on_positive_side) {
        return true;
      }
    }
  }

  return false;
}

void main()
{
  if (int(gl_GlobalInvocationID.x) >= resource_len) {
    return;
  }

  ObjectBounds bounds = bounds_buf[gl_GlobalInvocationID.x];
  IsectBox box = isect_data_setup(bounds.bounding_corners[0].xyz,
                                  bounds.bounding_corners[1].xyz,
                                  bounds.bounding_corners[2].xyz,
                                  bounds.bounding_corners[3].xyz);

#ifdef DYNAMIC_PASS_SELECTION
  if (is_visible(box)) {
    bool use_fail_pass = force_fail_method || intersects_near_plane(box);
    set_visibility(!use_fail_pass, use_fail_pass);
  }
  else {
    set_visibility(false, false);
  }
#else
  set_visibility(is_visible(box));
#endif
}
