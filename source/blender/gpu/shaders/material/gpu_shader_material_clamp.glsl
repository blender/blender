void clamp_value(float value, float min, float max, out float result)
{
  result = clamp(value, min, max);
}

void clamp_range(float value, float min, float max, out float result)
{
  result = (max > min) ? clamp(value, min, max) : clamp(value, max, min);
}
