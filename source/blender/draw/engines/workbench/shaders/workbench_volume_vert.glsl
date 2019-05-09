
uniform mat4 ModelMatrix;

uniform vec3 OrcoTexCoFactors[2];
uniform float slicePosition;
uniform int sliceAxis; /* -1 is no slice, 0 is X, 1 is Y, 2 is Z. */

in vec3 pos;

#ifdef VOLUME_SLICE
in vec3 uvs;

out vec3 localPos;
#endif

void main()
{
#ifdef VOLUME_SLICE
  if (sliceAxis == 0) {
    localPos = vec3(slicePosition * 2.0 - 1.0, pos.xy);
  }
  else if (sliceAxis == 1) {
    localPos = vec3(pos.x, slicePosition * 2.0 - 1.0, pos.y);
  }
  else {
    localPos = vec3(pos.xy, slicePosition * 2.0 - 1.0);
  }
  vec3 final_pos = localPos;
#else
  vec3 final_pos = pos;
#endif
  final_pos = ((final_pos * 0.5 + 0.5) - OrcoTexCoFactors[0]) / OrcoTexCoFactors[1];
  gl_Position = point_object_to_ndc(final_pos);
}
