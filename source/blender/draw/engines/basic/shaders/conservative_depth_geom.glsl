
/* Adaptation of Conservative Rasterization
 * from GPU Gems 2
 * Using method 2.
 *
 * Actual final implementation does not do conservative rasterization and only
 * avoids triangles producing no fragments.
 */

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

RESOURCE_ID_VARYING

uniform vec2 sizeViewport;
uniform vec2 sizeViewportInv;

void main()
{
  /* Compute plane normal in ndc space. */
  vec3 pos0 = gl_in[0].gl_Position.xyz / gl_in[0].gl_Position.w;
  vec3 pos1 = gl_in[1].gl_Position.xyz / gl_in[1].gl_Position.w;
  vec3 pos2 = gl_in[2].gl_Position.xyz / gl_in[2].gl_Position.w;
  vec3 plane = normalize(cross(pos1 - pos0, pos2 - pos0));
  /* Compute NDC bound box. */
  vec4 bbox = vec4(min(min(pos0.xy, pos1.xy), pos2.xy), max(max(pos0.xy, pos1.xy), pos2.xy));
  /* Convert to pixel space. */
  bbox = (bbox * 0.5 + 0.5) * sizeViewport.xyxy;
  /* Detect failure cases where triangles would produce no fragments. */
  bvec2 is_subpixel = lessThan(bbox.zw - bbox.xy, vec2(1.0));
  /* View aligned triangle. */
  const float threshold = 0.00001;
  bool is_coplanar = abs(plane.z) < threshold;

  for (int i = 0; i < 3; i++) {
    gl_Position = gl_in[i].gl_Position;
    if (all(is_subpixel)) {
      vec2 ofs = (i == 0) ? vec2(-1.0) : ((i == 1) ? vec2(2.0, -1.0) : vec2(-1.0, 2.0));
      /* HACK: Fix cases where the triangle is too small make it cover at least one pixel. */
      gl_Position.xy += sizeViewportInv.xy * gl_Position.w * ofs;
    }
    /* Test if the triangle is almost parralele with the view to avoid precision issues. */
    else if (any(is_subpixel) || is_coplanar) {
      /* HACK: Fix cases where the triangle is Parallel to the view by deforming it slightly. */
      vec2 ofs = (i == 0) ? vec2(-1.0) : ((i == 1) ? vec2(1.0, -1.0) : vec2(1.0));
      gl_Position.xy += sizeViewportInv.xy * gl_Position.w * ofs;
    }
    else {
      /* Triangle expansion should happen here, but we decide to not implement it for
       * depth precision & performance reasons. */
    }

#ifdef USE_WORLD_CLIP_PLANES
    world_clip_planes_set_clip_distance(gl_in[i].gl_ClipDistance);
#endif
    EmitVertex();
  }
  EndPrimitive();
}
