void map_range(
    float value, float fromMin, float fromMax, float toMin, float toMax, out float result)
{
  if (fromMax != fromMin) {
    result = toMin + ((value - fromMin) / (fromMax - fromMin)) * (toMax - toMin);
  }
  else {
    result = 0.0;
  }
}
