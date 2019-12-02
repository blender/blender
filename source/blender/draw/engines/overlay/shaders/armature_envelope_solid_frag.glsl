
uniform float alpha = 0.6;
uniform bool isDistance;

flat in vec3 finalStateColor;
flat in vec3 finalBoneColor;
in vec3 normalView;

out vec4 fragColor;

void main()
{
  float n = normalize(normalView).z;
  if (isDistance) {
    n = 1.0 - clamp(-n, 0.0, 1.0);
    fragColor = vec4(1.0, 1.0, 1.0, 0.2) * n;
  }
  else {
    /* Smooth lighting factor. */
    const float s = 0.2; /* [0.0-0.5] range */
    float fac = clamp((n * (1.0 - s)) + s, 0.0, 1.0);
    fragColor.rgb = mix(finalStateColor, finalBoneColor, fac);
    fragColor.a = alpha;
  }
}
