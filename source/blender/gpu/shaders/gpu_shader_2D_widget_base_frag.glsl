uniform vec3 checkerColorAndSize;

noperspective in vec4 finalColor;
noperspective in float butCo;
flat in float discardFac;

out vec4 fragColor;

vec4 do_checkerboard()
{
  float size = checkerColorAndSize.z;
  vec2 phase = mod(gl_FragCoord.xy, size * 2.0);

  if ((phase.x > size && phase.y < size) || (phase.x < size && phase.y > size)) {
    return vec4(checkerColorAndSize.xxx, 1.0);
  }
  else {
    return vec4(checkerColorAndSize.yyy, 1.0);
  }
}

void main()
{
  if (min(1.0, -butCo) > discardFac) {
    discard;
  }

  fragColor = finalColor;

  if (butCo > 0.5) {
    vec4 checker = do_checkerboard();
    fragColor = mix(checker, fragColor, fragColor.a);
  }

  if (butCo > 0.0) {
    fragColor.a = 1.0;
  }

  fragColor = blender_srgb_to_framebuffer_space(fragColor);
}
