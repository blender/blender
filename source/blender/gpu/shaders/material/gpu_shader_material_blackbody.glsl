void node_blackbody(float temperature, sampler1DArray spectrummap, float layer, out vec4 color)
{
  if (temperature >= 12000.0) {
    color = vec4(0.826270103, 0.994478524, 1.56626022, 1.0);
  }
  else if (temperature < 965.0) {
    color = vec4(4.70366907, 0.0, 0.0, 1.0);
  }
  else {
    float t = (temperature - 965.0) / (12000.0 - 965.0);
    color = vec4(texture(spectrummap, vec2(t, layer)).rgb, 1.0);
  }
}
