/**
 * Simple down-sample shader. Takes the average of the 4 texels of lower mip.
 */

#pragma BLENDER_REQUIRE(common_math_lib.glsl)

uniform samplerCube source;
uniform float texelSize;

flat in int fFace;

out vec4 FragColor;

const vec3 maj_axes[6] = vec3[6](vec3(1.0, 0.0, 0.0),
                                 vec3(-1.0, 0.0, 0.0),
                                 vec3(0.0, 1.0, 0.0),
                                 vec3(0.0, -1.0, 0.0),
                                 vec3(0.0, 0.0, 1.0),
                                 vec3(0.0, 0.0, -1.0));
const vec3 x_axis[6] = vec3[6](vec3(0.0, 0.0, -1.0),
                               vec3(0.0, 0.0, 1.0),
                               vec3(1.0, 0.0, 0.0),
                               vec3(1.0, 0.0, 0.0),
                               vec3(1.0, 0.0, 0.0),
                               vec3(-1.0, 0.0, 0.0));
const vec3 y_axis[6] = vec3[6](vec3(0.0, -1.0, 0.0),
                               vec3(0.0, -1.0, 0.0),
                               vec3(0.0, 0.0, 1.0),
                               vec3(0.0, 0.0, -1.0),
                               vec3(0.0, -1.0, 0.0),
                               vec3(0.0, -1.0, 0.0));

void main()
{
  vec2 uvs = gl_FragCoord.xy * texelSize;

  uvs = 2.0 * uvs - 1.0;

  vec3 cubevec = x_axis[fFace] * uvs.x + y_axis[fFace] * uvs.y + maj_axes[fFace];

  FragColor = textureLod(source, cubevec, 0.0);
}
