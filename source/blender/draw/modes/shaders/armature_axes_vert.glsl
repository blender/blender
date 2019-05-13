
uniform mat4 ViewProjectionMatrix;
uniform vec3 screenVecs[3];

/* ---- Instantiated Attrs ---- */
in float axis; /* position on the axis. [0.0-1.0] is X axis, [1.0-2.0] is Y, etc... */
in vec2 screenPos;
in vec3 colorAxis;

/* ---- Per instance Attrs ---- */
in mat4 InstanceModelMatrix;
in vec4 color;

flat out vec4 finalColor;

void main()
{
  vec3 chosen_axis = InstanceModelMatrix[int(axis)].xyz;
  vec3 y_axis = InstanceModelMatrix[1].xyz;
  vec3 bone_loc = InstanceModelMatrix[3].xyz;
  vec3 wpos = bone_loc + y_axis + chosen_axis * fract(axis);
  vec3 spos = screenVecs[0].xyz * screenPos.x + screenVecs[1].xyz * screenPos.y;
  /* Scale uniformly by axis length */
  spos *= length(chosen_axis);

  vec4 pos_4d = vec4(wpos + spos, 1.0);
  gl_Position = ViewProjectionMatrix * pos_4d;

  finalColor.rgb = mix(colorAxis, color.rgb, color.a);
  finalColor.a = 1.0;

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(pos_4d.xyz);
#endif
}
