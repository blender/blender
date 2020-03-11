
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_common_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_cavity_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_curvature_lib.glsl)

uniform sampler2D depthBuffer;
uniform sampler2D normalBuffer;
uniform usampler2D objectIdBuffer;

in vec4 uvcoordsvar;

out vec4 fragColor;

void main()
{
  float cavity = 0.0, edges = 0.0, curvature = 0.0;

#ifdef USE_CAVITY
  cavity_compute(uvcoordsvar.st, depthBuffer, normalBuffer, cavity, edges);
#endif

#ifdef USE_CURVATURE
  curvature_compute(uvcoordsvar.st, objectIdBuffer, normalBuffer, curvature);
#endif

  float final_cavity_factor = clamp((1.0 - cavity) * (1.0 + edges) * (1.0 + curvature), 0.0, 4.0);

  fragColor.rgb = vec3(final_cavity_factor);
  fragColor.a = 1.0;
}
