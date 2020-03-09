
uniform vec4 gpDepthPlane;

flat in uint objectId;

/* using uint because 16bit uint can contain more ids than int. */
out uint outId;

vec3 ray_plane_intersection(vec3 ray_ori, vec3 ray_dir, vec4 plane)
{
  float d = dot(plane.xyz, ray_dir);
  vec3 plane_co = plane.xyz * (-plane.w / dot(plane.xyz, plane.xyz));
  vec3 h = ray_ori - plane_co;
  float lambda = -dot(plane.xyz, h) / ((abs(d) < 1e-8) ? 1e-8 : d);
  return ray_ori + ray_dir * lambda;
}

void main()
{
#ifdef USE_GPENCIL
  if (stroke_round_cap_mask(strokePt1, strokePt2, strokeAspect, strokeThickness, strokeHardeness) <
      0.001) {
    discard;
  }

  if (depth != -1.0) {
    /* Stroke order 2D. */
    bool is_persp = ProjectionMatrix[3][3] == 0.0;
    vec2 uvs = vec2(gl_FragCoord.xy) * sizeViewportInv;
    vec3 pos_ndc = vec3(uvs, gl_FragCoord.z) * 2.0 - 1.0;
    vec4 pos_world = ViewProjectionMatrixInverse * vec4(pos_ndc, 1.0);
    vec3 pos = pos_world.xyz / pos_world.w;

    vec3 ray_ori = pos;
    vec3 ray_dir = (is_persp) ? (ViewMatrixInverse[3].xyz - pos) : ViewMatrixInverse[2].xyz;
    vec3 isect = ray_plane_intersection(ray_ori, ray_dir, gpDepthPlane);
    vec4 ndc = point_world_to_ndc(isect);
    gl_FragDepth = (ndc.z / ndc.w) * 0.5 + 0.5;
  }
  else {
    gl_FragDepth = gl_FragCoord.z;
  }
#endif

  outId = uint(objectId);
}
