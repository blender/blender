
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(surface_lib.glsl)

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

RESOURCE_ID_VARYING

/* Only used to compute barycentric coordinates. */

void main()
{
#ifdef DO_BARYCENTRIC_DISTANCES
  dataAttrOut.barycentricDist = calc_barycentric_distances(
      dataIn[0].worldPosition, dataIn[1].worldPosition, dataIn[2].worldPosition);
#endif

  PASS_RESOURCE_ID

#ifdef USE_ATTR
  pass_attr(0);
#endif
  PASS_SURFACE_INTERFACE(0);
  gl_Position = gl_in[0].gl_Position;
  gl_ClipDistance[0] = gl_in[0].gl_ClipDistance[0];
  EmitVertex();

#ifdef USE_ATTR
  pass_attr(1);
#endif
  PASS_SURFACE_INTERFACE(1);
  gl_Position = gl_in[1].gl_Position;
  gl_ClipDistance[0] = gl_in[1].gl_ClipDistance[0];
  EmitVertex();

#ifdef USE_ATTR
  pass_attr(2);
#endif
  PASS_SURFACE_INTERFACE(2);
  gl_Position = gl_in[2].gl_Position;
  gl_ClipDistance[0] = gl_in[2].gl_ClipDistance[0];
  EmitVertex();

  EndPrimitive();
}
