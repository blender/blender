

uniform float dashWidth;

#ifdef SMOOTH_COLOR
noperspective in vec4 finalColor;
#else
flat in vec4 finalColor;
#endif

noperspective in vec2 stipple_pos;
flat in vec2 stipple_start;

out vec4 fragColor;

void main()
{
  fragColor = finalColor;

  /* Avoid passing viewport size */
  vec2 dd = fwidth(stipple_pos);

  float dist = distance(stipple_start, stipple_pos) / max(dd.x, dd.y);

  if (fract(dist / dashWidth) > 0.5) {
    fragColor.rgb = vec3(0.0);
  }

  fragColor = blender_srgb_to_framebuffer_space(fragColor);
}
