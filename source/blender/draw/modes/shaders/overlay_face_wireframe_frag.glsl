
uniform vec3 wireColor;
uniform vec3 rimColor;

flat in float edgeSharpness;
in float facing;

out vec4 fragColor;

void main()
{
  if (edgeSharpness < 0.0) {
    discard;
  }

  float facing_clamped = clamp(abs(facing), 0.0, 1.0);

  vec3 final_front_col = mix(rimColor, wireColor, 0.4);
  vec3 final_rim_col = mix(rimColor, wireColor, 0.1);

  fragColor.rgb = mix(final_rim_col, final_front_col, facing_clamped);
  fragColor.a = 1.0f;
}
