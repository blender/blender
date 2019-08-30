void separate_hsv(vec4 col, out float h, out float s, out float v)
{
  vec4 hsv;

  rgb_to_hsv(col, hsv);
  h = hsv[0];
  s = hsv[1];
  v = hsv[2];
}
